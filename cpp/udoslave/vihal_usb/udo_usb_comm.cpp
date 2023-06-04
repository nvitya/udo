/* -----------------------------------------------------------------------------
 * This file is a part of the UDO project: https://github.com/nvitya/udo
 * Copyright (c) 2023 Viktor Nagy, nvitya
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software. Permission is granted to anyone to use this
 * software for any purpose, including commercial applications, and to alter
 * it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software in
 *    a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 * --------------------------------------------------------------------------- */
/*
 *  file:     udo_usb_comm.cpp
 *  brief:    USB (CDC) UDO-SL Slave Implementation for VIHAL
 *  created:  2023-06-04
 *  authors:  nvitya
*/

#include "string.h"
#include "udo_usb_comm.h"
#include "traces.h"
#include "hwpins.h"
#include "usbdevice.h"
#include "hwusbctrl.h"

bool TUdoCdcControl::InitInterface()
{
	intfdesc.interface_class = 2; // CDC
	intfdesc.interface_subclass = 2; // Abstract Control Model
	intfdesc.interface_protocol = 0; // 0 = no class specitic control

	interface_name = "UDO Control";

	desc_cdc_callmgmt.data_interface = index + 1;  // the following interface must be the data interface
	desc_cdc_union.master_interface = index;
	desc_cdc_union.slave_interface = index + 1;

	// some other descriptors are required

	AddConfigDesc((void *)&cdc_desc_header_func[0],     true);
  AddConfigDesc((void *)&desc_cdc_callmgmt,           true);
	AddConfigDesc((void *)&cdc_desc_call_acm_func[0],   true);
  AddConfigDesc((void *)&desc_cdc_union,              true);

	// endpoints

	ep_manage.Init(HWUSB_EP_TYPE_INTERRUPT, false, 8);
	ep_manage.interval = 10; // polling interval
	AddEndpoint(&ep_manage);

	return true;
}

void TUdoCdcControl::OnConfigured()
{
	//TRACE("SPEC Device Configured.\r\n");

	//ep_manage.EnableRecv();
}

bool TUdoCdcControl::HandleTransferEvent(TUsbEndpoint * aep, bool htod)
{
	return false;
}

bool TUdoCdcControl::HandleSetupRequest(TUsbSetupRequest * psrq)
{
	if (0x20 == psrq->request) // set line coding, data stage follows !
	{
		//TRACE("UIO Ctrl Set line coding (SETUP)\r\n");
		device->StartSetupData();  // start receiving the data part, which will be handled at the HandleSetupData()
		return true;
	}
	else if (0x21 == psrq->request) // Get line coding
	{
		//TRACE("VCP Get line coding\r\n");
		device->StartSendControlData(&linecoding, sizeof(linecoding));
		return true;
	}
	else if (0x22 == psrq->request) // Set Control Line State
	{
		//TRACE("VCP Set Control Line State: %04X\r\n", psrq->value);
		device->SendControlStatus(true);
		return true;
	}

	return false;
}

// the setup request's data part comes in a separate phase and so it has a separate callback:
bool TUdoCdcControl::HandleSetupData(TUsbSetupRequest * psrq, void * adata, unsigned adatalen)
{
	if (0x20 == psrq->request) // set line coding
	{
		memcpy(&linecoding, adata, sizeof(linecoding));

	  //TRACE("%u ", CLOCKCNT / (SystemCoreClock / 1000));
		//TRACE("UIO Ctrl Line Coding data:\r\n  baud=%i, format=%i, parity=%i, bits=%i\r\n",
		//		linecoding.baudrate, linecoding.charformat, linecoding.paritytype, linecoding.databits
		//);

		device->SendControlStatus(true);
		return true;
	}

	return false;
}

//------------------------------------------------

bool TUdoCdcData::InitInterface()
{
	intfdesc.interface_class = 0x0A; // CDC Data
	intfdesc.interface_subclass = 0;
	intfdesc.interface_protocol = 0; // no specific protocol

	interface_name = "UDO Data";

	// endpoints

	ep_input.Init(HWUSB_EP_TYPE_BULK, true, 64);
	AddEndpoint(&ep_input);
	ep_output.Init(HWUSB_EP_TYPE_BULK, false, 64);
	AddEndpoint(&ep_output);

  Reset();

	return true;
}

void TUdoCdcData::OnConfigured()
{
	TRACE("UDO Data Interface ready.\r\n");

  Reset();

	ep_input.EnableRecv();
}

bool TUdoCdcData::HandleTransferEvent(TUsbEndpoint * aep, bool htod)
{
	int r;
	if (htod)
	{
	  // data chunk arrived
	  rxenabled = false;
		r = ep_input.ReadRecvData(&usb_rxbuf[0], sizeof(usb_rxbuf));
		//TRACE("%i byte VCP data arrived\r\n", r);
		if (r > 0)
		{
		  usb_rxlen = r;
		  usb_rpos = 0;
		  lastrecvtime = CLOCKCNT;

      #if 0
        TRACE("USB RxDATA: ");
        for (r = 0; r < usb_rxlen; ++r)  TRACE(" %02X", usb_rxbuf[r]);
        TRACE("\r\n");
      #endif

		  HandleRx();
		}
		else
		{
		  rxenabled = true;
      ep_input.EnableRecv();
		}
	}
	else
	{
	  usb_ready_to_send = true;
	  HandleTx();
	}

	return true;
}

void TUdoCdcData::Reset()
{
  usb_txlen = 0;
  rxenabled = true;
  tx_remaining = 0;
  txstate = 0;
}

void TUdoCdcData::Run()
{
  HandleTx();
  HandleRx();
}

void TUdoCdcData::HandleRx()
{
  while ((rxstate != 50) && (usb_rpos < usb_rxlen))
  {
    uint8_t b = usb_rxbuf[usb_rpos];

    //TRACE("< %02X, rxstate=%u\r\n", b, rxstate);

    if ((rxstate > 0) && (rxstate < 10))
    {
      rxcrc = udo_calc_crc(rxcrc, b);
    }

    if (0 == rxstate)  // waiting for the sync byte
    {
      if (0x55 == b)
      {
        rxcrc = udo_calc_crc(0, b); // start the CRC from zero
        rxstate = 1;
      }
    }
    else if (1 == rxstate) // command and length
    {
      rqcmd = b;  // store the cmd for the response

      if (b & 0x80) // bit7: 0 = read, 1 = write
      {
        rq.iswrite = 1;
      }
      else
      {
        rq.iswrite = 0;
      }

      // decode the length fields
      offslen    = ((0x4210 >> ((b & 3) << 2)) & 0xF);
      rq.metalen = ((0x4210 >> (b & 0xC)) & 0xF);

      // initialize optional members
      rq.offset = 0;
      rq.metadata = 0;
      rq.dataptr = &rwdatabuf[0];

      rxcnt = 0;
      rxstate = 3;  // index follows normally

      unsigned lencode = ((b >> 4) & 7);
      if      (lencode < 5)   { rq.rqlen = ((0x84210 >> (lencode << 2)) & 0xF); }  // in-line demultiplexing
      else if (5 == lencode)  { rq.rqlen = 16; }
      else if (7 == lencode)  { rxstate = 2; }  // extended length follows
      else  // invalid (6)
      {
        rxstate = 0;
        ++error_count_crc;
      }
    }
    else if (2 == rxstate) // extended length
    {
      if (0 == rxcnt)
      {
        rq.rqlen = b; // low byte
        rxcnt = 1;
      }
      else
      {
        rq.rqlen |= (b << 8); // high byte
        rxcnt = 0;
        rxstate = 3; // index follows
      }
    }
    else if (3 == rxstate) // index
    {
      if (0 == rxcnt)
      {
        rq.index = b;  // index low
        rxcnt = 1;
      }
      else
      {
        rq.index |= (b << 8);  // index high
        rxcnt = 0;
        if (offslen)
        {
          rxstate = 4;  // offset follows
        }
        else if (rq.metalen)
        {
          rxstate = 5;  // meta follows
        }
        else
        {
          if (rq.iswrite)
          {
            rxstate = 6;  // data follows when write
          }
          else
          {
            rxstate = 10;  // then crc check
          }
        }
      }
    }
    else if (4 == rxstate) // offset
    {
      rq.offset |= (b << (rxcnt << 3));
      ++rxcnt;
      if (rxcnt >= offslen)
      {
        rxcnt = 0;
        if (rq.metalen)
        {
          rxstate = 5;  // meta follows
        }
        else
        {
          if (rq.iswrite)
          {
            rxstate = 6;  // data follows when write
          }
          else
          {
            rxstate = 10;  // then crc check
          }
        }
      }
    }
    else if (5 == rxstate) // metadata
    {
      rq.metadata |= (b << (rxcnt << 3));
      ++rxcnt;
      if (rxcnt >= rq.metalen)
      {
        if (rq.iswrite)
        {
          rxcnt = 0;
          rxstate = 5; // write data follows
        }
        else
        {
          rxstate = 10;  // crc check
        }
      }
    }
    else if (6 == rxstate) // write data
    {
      rwdatabuf[rxcnt] = b;
      ++rxcnt;
      if ((rxcnt >= rq.rqlen) || (rxcnt >= UDO_MAX_DATALEN))
      {
        rxstate = 10;
      }
    }
    else if (10 == rxstate) // crc check
    {
      if (b != rxcrc)
      {
        TRACE("UDO-RQ CRC error: expected: %02X\r\n", rxcrc);
        // crc error, no answer
        ++error_count_crc;

        rxstate = 0; // go to the next request
      }
      else
      {
        // execute the request, prepare the answer
        rq.maxanslen = sizeof(rwdatabuf);
        rq.anslen = 0;
        rq.result = 0;

        udoslave_app_read_write(&rq);  // call the application specific handler

        // the answer is prepared in the rq, send it

        txstate = 1; // start the answering
        rxstate = 50; // wait until the response is sent, the HandleTX() might change this

        HandleTx();
      }
    }

    ++usb_rpos;
  }

  if (0 == txstate)  // if answering is finished
  {
    if (50 == rxstate)
    {
      rxstate = 0;  // start the receiving
    }

    if (!rxenabled)
    {
      rxenabled = true;
      ep_input.EnableRecv();
    }
  }
}

void TUdoCdcData::HandleTx()
{
  if (0 == txstate)
  {
    return;
  }

  uint8_t   b;

  if (1 == txstate)  // prepare the head
  {
    usb_txlen = 0;

    uint16_t  extlen = 0;

    txcrc = 0;
    b = 0x55; // sync
    AddTx(&b, 1);

    b = (rqcmd & 0x8F); // use the request command except data length

    if (rq.result)  // prepare error response
    {
      b |= (6 << 4);  // invalid length signalizes error response
    }
    else
    {
      // normal response
      if (rq.iswrite)
      {
        rq.anslen = 0;
      }
      else  // rq.anslen is already set
      {
        if      ( 3  > rq.anslen)  { b |= (rq.anslen << 4);  }
        else if ( 4 == rq.anslen)  { b |= (3 << 4); }
        else if ( 8 == rq.anslen)  { b |= (4 << 4); }
        else if (16 == rq.anslen)  { b |= (5 << 4); }
        else
        {
          b |= (7 << 4);
          extlen = rq.anslen;
        }
      }
    }

    AddTx(&b, 1);  // command / length info
    if (extlen)
    {
      AddTx(&extlen, 2);
    }
    AddTx(&rq.index, 2); // echo the address back
    if (offslen)
    {
      AddTx(&rq.offset, offslen);
    }
    if (rq.metalen)
    {
      AddTx(&rq.metadata, rq.metalen);
    }

    if (rq.result)
    {
      AddTx(&rq.result, 2); // send the result
      txstate = 10; // send CRC
    }
    else if (rq.anslen)
    {
      txstate = 5; // send the data body
      tx_dataidx = 0;
      //AddTx(rq.dataptr, rq.anslen);
    }
    else
    {
      txstate = 10; // send the crc
    }
  }

  if (5 == txstate) // sending data body, might be chunked
  {
    int chunksize = sizeof(usb_txbuf) - usb_txlen;
    if (chunksize > rq.anslen - tx_dataidx)
    {
      chunksize = rq.anslen - tx_dataidx;
    }

    if (AddTx(&rq.dataptr[tx_dataidx], chunksize))
    {
      tx_dataidx += chunksize;
      if (tx_dataidx >= rq.anslen)
      {
        txstate = 10; // go to CRC
      }
    }
  }

  if (10 == txstate) // sending crc
  {
    if (AddTx(&txcrc, 1))
    {
      txstate = 50;
    }
  }

  // sending buffered data
  if ((usb_txlen > 0) && usb_ready_to_send)
  {
    #if 0
      TRACE("USB Tx: ");
      for (unsigned n = 0; n < usb_txlen; ++n)  TRACE(" %02X", usb_txbuf[n]);
      TRACE("\r\n");
    #endif

    ep_output.StartSendData(&usb_txbuf[0], usb_txlen);
    usb_ready_to_send = false;
    usb_txlen = 0;
  }

  if (50 == txstate) // wait for last chunk send
  {
    if (0 == usb_txlen)
    {
      txstate = 0;  // go back to idle
    }
  }
}

bool TUdoCdcData::AddTx(void * asrc, unsigned len)
{
  if (usb_txlen + len > sizeof(usb_txbuf))
  {
    return false;
  }

  uint8_t * srcp = (uint8_t *)asrc;
  uint8_t * dstp = &usb_txbuf[usb_txlen];
  uint8_t * endp = dstp + len;
  while (dstp < endp)
  {
    uint8_t b = *srcp++;
    *dstp++ = b;
    txcrc = udo_calc_crc(txcrc, b);
  }

  usb_txlen += len;
  return true;
}

//------------------------------------------------

bool TUsbFuncUdo::InitFunction()
{
  funcdesc.function_class = 2;
  funcdesc.function_sub_class = 2;
  funcdesc.function_protocol = 1;

  func_name = "UDO Control";

  AddInterface(&uif_control);
  AddInterface(&uif_data);

  return true;
}

void TUsbFuncUdo::Run()
{
  uif_data.Run();
}
