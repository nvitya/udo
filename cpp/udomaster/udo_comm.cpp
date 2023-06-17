/*
 * udo_comm.cpp
 *
 *  Created on: Jun 11, 2023
 *      Author: vitya
 */

#include "string.h"
#include "stdarg.h"
#include "udo_comm.h"

TUdoComm         udocomm;
TUdoCommHandler  commh_none;

EUdoAbort::EUdoAbort(uint16_t acode, const char * afmt, ...)
{
	va_list arglist;
	char  fmtbuf[512]; // local format buffer on the stack

	ecode = acode;

	va_start(arglist, afmt);
	vsnprintf(&fmtbuf[0], sizeof(fmtbuf), afmt, arglist);
	va_end(arglist);

	emsg = string(&fmtbuf[0]);
}

//-----------------------------------------------------------------------------
// TUdoCommHandler
//-----------------------------------------------------------------------------

TUdoCommHandler::TUdoCommHandler()
{
}

TUdoCommHandler::~TUdoCommHandler()
{
}

void TUdoCommHandler::Open() // virtual, must be overridden
{
	throw EUdoAbort(UDOERR_APPLICATION, "Open: Invalid comm. handler");
}

void TUdoCommHandler::Close() // virtual, must be overridden
{
	// does nothing
}

bool TUdoCommHandler::Opened() // virtual, must be overridden
{
	return false;
}

string TUdoCommHandler::ConnString() // virtual, must be overridden
{
	return string("NONE");
}

int TUdoCommHandler::UdoRead(uint16_t index, uint32_t offset, void * dataptr, uint32_t maxdatalen)
{
	throw EUdoAbort(UDOERR_APPLICATION, "Open: Invalid comm. handler");
	return 0; // never reached.
}

void TUdoCommHandler::UdoWrite(uint16_t index, uint32_t offset, void * dataptr, uint32_t datalen)
{
	throw EUdoAbort(UDOERR_APPLICATION, "Open: Invalid comm. handler");
}

//-----------------------------------------------------------------------------
// TUdoComm
//-----------------------------------------------------------------------------

TUdoComm::TUdoComm()
{
  commh = &commh_none;
  max_payload_size = 64;  // start with the smallest
}

TUdoComm::~TUdoComm()
{
	//
}

void TUdoComm::SetHandler(TUdoCommHandler * acommh)
{
	if (acommh)
	{
	  commh = acommh;
	}
	else
	{
		commh = &commh_none;
	}
}

void TUdoComm::Open()
{
	int       r;
	uint32_t  d32;

	if (!commh->Opened())
	{
		commh->Open();
	}

	r = commh->UdoRead(0x0000, 0, &d32, 4);
	if ((r != 4) or (d32 != 0x66CCAA55))
	{
    commh->Close();
    throw EUdoAbort(UDOERR_CONNECTION, "Invalid Obj-0000 response: %.8X", d32);
	}

  r = commh->UdoRead(0x0001, 0, &d32, 4);  // get the maximal payload length
  if ((d32 < 64) or (d32 > UDO_MAX_PAYLOAD_LEN))
  {
    commh->Close();
    throw EUdoAbort(UDOERR_CONNECTION, "Invalid maximal payload size: %d", d32);
  }

  max_payload_size = d32;
}

void TUdoComm::Close()
{
	commh->Close();
}

bool TUdoComm::Opened()
{
	return commh->Opened();
}

int TUdoComm::UdoRead(uint16_t index, uint32_t offset, void * dataptr, uint32_t maxdatalen)
{
	int result = commh->UdoRead(index, offset, dataptr, maxdatalen);
  if ((result <= 8) and (result < maxdatalen))
  {
    uint8_t * pdata = (uint8_t *)dataptr;
    memset(pdata + result, 0, maxdatalen - result); // pad smaller responses, todo: sign extension
  }
  return result;
}

void TUdoComm::UdoWrite(uint16_t index, uint32_t offset, void * dataptr, uint32_t datalen)
{
	commh->UdoWrite(index, offset, dataptr, datalen);
}

int TUdoComm::ReadBlob(uint16_t index, uint32_t offset, void *  dataptr, uint32_t maxdatalen)
{
  int r;
  int result = 0;
  int remaining = maxdatalen;
  uint8_t * pdata = (uint8_t *)dataptr;
  uint32_t offs = offset;

  while (remaining > 0)
  {
    int chunksize = max_payload_size;
    if (chunksize > remaining)  chunksize = remaining;
    r = commh->UdoRead(index, offs, pdata, chunksize);
    if (r <= 0)
    {
      break;
    }

    result += r;
    pdata  += r;
    offs   += r;
    remaining -= r;

    if (r < chunksize)
    {
      break;
    }
  }

  return result;
}

void TUdoComm::WriteBlob(uint16_t index, uint32_t offset, void *  dataptr, uint32_t datalen)
{
  int r;
  int remaining = datalen;
  uint8_t * pdata = (uint8_t *)dataptr;
  uint32_t offs = offset;

  while (remaining > 0)
  {
    int chunksize = max_payload_size;
    if (chunksize > remaining)  chunksize = remaining;
    commh->UdoWrite(index, offs, pdata, chunksize);

    pdata  += chunksize;
    offs   += chunksize;
    remaining -= chunksize;
  }
}


int32_t TUdoComm::ReadI32(uint16_t index, uint32_t offset)
{
	int32_t i32 = 0;
	int r = UdoRead(index, offset, &i32, sizeof(i32));
	if (2 == r) // sign extension required
	{
		return *(int16_t *)&i32;
	}
	else
	{
		return i32;
	}
}

int16_t TUdoComm::ReadI16(uint16_t index, uint32_t offset)
{
	int16_t rvalue = 0;
	int r = UdoRead(index, offset, &rvalue, sizeof(rvalue));
	return rvalue;
}

uint32_t TUdoComm::ReadU32(uint16_t index, uint32_t offset)
{
	uint32_t rvalue = 0;
	int r = UdoRead(index, offset, &rvalue, sizeof(rvalue));
	return rvalue;
}

uint16_t TUdoComm::ReadU16(uint16_t index, uint32_t offset)
{
	uint16_t rvalue = 0;
	int r = UdoRead(index, offset, &rvalue, sizeof(rvalue));
	return rvalue;
}

uint8_t TUdoComm::ReadU8(uint16_t index, uint32_t offset)
{
	uint8_t rvalue = 0;
	int r = UdoRead(index, offset, &rvalue, sizeof(rvalue));
	return rvalue;
}

void TUdoComm::WriteI32(uint16_t index, uint32_t offset, int32_t avalue)
{
	UdoWrite(index, offset, &avalue, sizeof(avalue));
}

void TUdoComm::WriteI16(uint16_t index, uint32_t offset, int16_t avalue)
{
	UdoWrite(index, offset, &avalue, sizeof(avalue));
}

void TUdoComm::WriteU32(uint16_t index, uint32_t offset, uint32_t avalue)
{
	UdoWrite(index, offset, &avalue, sizeof(avalue));
}

void TUdoComm::WriteU16(uint16_t index, uint32_t offset, uint16_t avalue)
{
	UdoWrite(index, offset, &avalue, sizeof(avalue));
}

void TUdoComm::WriteU8(uint16_t index, uint32_t offset, uint8_t avalue)
{
	UdoWrite(index, offset, &avalue, sizeof(avalue));
}
