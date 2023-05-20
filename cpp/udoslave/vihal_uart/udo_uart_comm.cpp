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
 *  file:     udo_uart_comm.cpp
 *  brief:    UDO-SL Slave Implementation for VIHAL
 *  created:  2023-05-18
 *  authors:  nvitya
*/
#include "platform.h"

#include "common.h"
#include "udo.h"
#include "udo_uart_comm.h"
#include "traces.h"

#include "udoslaveapp.h"

TUdoUartComm g_uartcomm;

bool TUdoUartComm::Init()
{
  initialized = false;

  if (!uart.initialized)  // the .uart must be initialized before !
  {
    return false;
  }

  uart.DmaAssign(true,  &dma_tx);
  uart.DmaAssign(false, &dma_rx);

  // start the DMA receive with circular DMA buffer
  rxdmapos = 0;
  dmaxfer_rx.bytewidth = 1;
  dmaxfer_rx.count = sizeof(rxdmabuf);
  dmaxfer_rx.dstaddr = &rxdmabuf[0];
  dmaxfer_rx.flags = DMATR_CIRCULAR;
  uart.DmaStartRecv(&dmaxfer_rx);

  rxmsglen = 0;
  txlen = 0;

  rx_timeout_clocks = UARTCOMM_RX_TIMEOUT_US * (SystemCoreClock / 1000000);

  initialized = true;
  return true;
}

unsigned TUdoUartComm::AddTx(void * asrc, unsigned len) // returns the amount actually written
{
  unsigned available = TxAvailable();
  if (0 == available)
  {
    return 0;
  }

  if (len > available)  len = available;

  uint8_t * srcp = (uint8_t *)asrc;
  uint8_t * dstp = &txbuf[txbufwr][txlen];
  uint8_t * endp = dstp + len;
  while (dstp < endp)
  {
    uint8_t b = *srcp++;
    *dstp++ = b;
    txcrc = udo_calc_crc(txcrc, b);
  }

  txlen += len;

  return len;
}


void TUdoUartComm::StartSendTxBuffer()
{
  if (txlen && !dma_tx.Active())
  {
    // setup the TX DMA and flip the buffer

    dmaxfer_tx.flags = 0;
    dmaxfer_tx.bytewidth = 1;
    dmaxfer_tx.count = txlen;
    dmaxfer_tx.srcaddr = &txbuf[txbufwr][0];

    uart.DmaStartSend(&dmaxfer_tx);

    // change the buffer
    txbufwr ^= 1;
    txlen = 0;
  }
}

void TUdoUartComm::Run()
{
  // RX processing
  uint16_t newrxdmapos = sizeof(rxdmabuf) - dma_rx.Remaining();
  if (newrxdmapos >= sizeof(rxdmabuf))
  {
    // fix for Remaining() == 0 (sometimes the linked list operation is processed only later)
    newrxdmapos = 0;
  }

  unsigned t0 = CLOCKCNT;

  while (rxdmapos != newrxdmapos)
  {
    uint8_t b = rxdmabuf[rxdmapos];
    lastrecvtime = t0;

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
      }
      else
      {
        // execute the request, prepare the answer
        rq.maxanslen = sizeof(rwdatabuf);
        rq.anslen = 0;
        rq.result = 0;

        g_slaveapp.UdoReadWrite(&rq);

        SendAnswer(); // the answer is prepared in the rq
      }

      rxstate = 0; // go to the next request
    }

    ++rxdmapos;
    if (rxdmapos >= sizeof(rxdmabuf))  rxdmapos = 0;
  }

  if ((rxstate != 0) && (t0 - lastrecvtime > rx_timeout_clocks))
  {
    TRACE("UDO-RQ timeout\r\n");
    ++error_count_timeout;
    rxstate = 0;
  }

  // Sending buffered tx messages
  StartSendTxBuffer();
}

void TUdoUartComm::SendAnswer()
{
  uint8_t   b;
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
  }
  else if (rq.anslen)
  {
    AddTx(rq.dataptr, rq.anslen);
  }

  b = txcrc;
  AddTx(&b, 1); // then send the crc
}
