/*
 * commh_udosl.cpp
 *
 *  Created on: Jun 11, 2023
 *      Author: vitya
 */

#include "string.h"
#include "commh_udosl.h"
#include "general.h"

TCommHandlerUdoSl  udosl_commh;

TCommHandlerUdoSl::TCommHandlerUdoSl()
{
	devstr = string("");
  comm.baudrate = 1000000;
}

TCommHandlerUdoSl::~TCommHandlerUdoSl()
{
	// TODO Auto-generated destructor stub
}

void TCommHandlerUdoSl::Open()
{
	comm.Close();
	if (!comm.Open(devstr.c_str()))
	{
		throw EUdoAbort(UDOERR_CONNECTION, "UDO-SL: error opening device \"%s\"", devstr.c_str());
	}
}

void TCommHandlerUdoSl::Close()
{
	comm.Close();
}

bool TCommHandlerUdoSl::Opened()
{
  return comm.Opened();
}

string TCommHandlerUdoSl::ConnString()
{
	return StringFormat("UDO-SL %s", devstr.c_str());
}

int TCommHandlerUdoSl::UdoRead(uint16_t index, uint32_t offset, void * dataptr, uint32_t maxdatalen)
{
  iswrite = false;
	mindex  = index;
  moffset = offset;
  mdataptr = (uint8_t *)dataptr;
  mrqlen = maxdatalen;

  opstring = StringFormat("UdoRead(%.4X, %d)", mindex, moffset);

	SendRequest();
  RecvResponse();

	if (ans_datalen > maxdatalen)
  {
		throw EUdoAbort(UDOERR_DATA_TOO_BIG, "%s result data is too big: %d", opstring, ans_datalen);
  }

  // copy the response to the user buffer
  if (ans_datalen > 0)
  {
	  memcpy(mdataptr, &rwbuf[rwbuf_ansdatapos], ans_datalen);
  };

	return ans_datalen;
}

void TCommHandlerUdoSl::UdoWrite(uint16_t index, uint32_t offset, void * dataptr, uint32_t datalen)
{
  iswrite = true;
  mindex = index;
  moffset  = offset;
  mdataptr = (uint8_t *)dataptr;
  mrqlen = datalen;

  opstring = StringFormat("UdoWrite(%.4X, %d)[%d]", mindex, moffset, mrqlen);

  SendRequest();
  RecvResponse();
}

void TCommHandlerUdoSl::SendRequest()
{
	int r;
	uint8_t  b;

	comm.FlushInput();
	comm.FlushOutput();

	uint8_t offslen;
  if      (moffset ==     0)  offslen = 0;
  else if (moffset > 0xFFFF)  offslen = 4;
  else if (moffset >   0xFF)  offslen = 2;
  else                        offslen = 1;

	uint8_t metalen;
  if      (mmetadata ==     0)  metalen = 0;
  else if (mmetadata > 0xFFFF)  metalen = 4;
  else if (mmetadata >   0xFF)  metalen = 2;
  else                          metalen = 1;

	crc = 0;
  rwbuflen = 0;

  // 1. the sync byte
  b = 0x55; // sync
  AddTx(&b, 1);

  // 2. command byte
  if (iswrite)  b = 0x80;
  else          b = 0;

  if (4 == offslen)  b |= 3;
  else               b |= offslen;

  if (4 == metalen)  b |= 0x0C;
  else               b |= (metalen << 2);

  unsigned extlen = 0;
  if      ( 3  > mrqlen)  b |= (mrqlen << 4);
  else if ( 4 == mrqlen)  b |= (3 << 4);
  else if ( 8 == mrqlen)  b |= (4 << 4);
  else if (16 == mrqlen)  b |= (5 << 4);
  else
  {
    b |= (7 << 4);
    extlen = mrqlen;
  }

	AddTx(&b, 1);  // add the command byte

  // 3. extlen
  if (extlen > 0)  AddTx(&extlen, 2);

  // 4. index
	AddTx(&mindex, 2);

  // 5. offset
  if (offslen > 0)  AddTx(&moffset, offslen);

  // 6. metadata
	if (metalen > 0)  AddTx(&mmetadata, metalen);

  // 7. write data
	if (iswrite and (mrqlen > 0))
	{
		AddTx(mdataptr, mrqlen);
	};

  // 8. crc
	AddTx(&crc, 1);

	// send the request

	r = comm.Write(&rwbuf[0], rwbuflen);
	if ((r <= 0) or (r != rwbuflen))
	{
    throw EUdoAbort(UDOERR_CONNECTION, "%s: send error", opstring);
	}
}

void TCommHandlerUdoSl::RecvResponse()
{

}

int TCommHandlerUdoSl::AddTx(void * asrc, int len)
{
  unsigned available = TxAvailable();
  if (0 == available)
  {
    return 0;
  }

  if (len > available)  len = available;

  uint8_t * srcp = (uint8_t *)asrc;
  uint8_t * dstp = &rwbuf[rwbuflen];
  uint8_t * endp = dstp + len;
  while (dstp < endp)
  {
    uint8_t b = *srcp++;
    *dstp++ = b;
    crc = udo_calc_crc(crc, b);
  }

  rwbuflen += len;
  return len;
}

int TCommHandlerUdoSl::TxAvailable()
{
	return (sizeof(rwbuf) - rwbuflen);
}
