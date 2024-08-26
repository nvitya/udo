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

	if (ans_datalen > int(maxdatalen))
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
	if ((r <= 0) or (r != int(rwbuflen)))
	{
    throw EUdoAbort(UDOERR_CONNECTION, "%s: send error", opstring);
	}
}

void TCommHandlerUdoSl::RecvResponse()
{
	int       r;
	uint8_t   lencode;
	uint32_t  offslen;
	uint32_t  metalen;
	uint16_t  ecode;
	bool      iserror = false;

	// receive the response

	rwbuflen = 0;
	rwbuf_ansdatapos = 0;

  rxreadpos = 0;
  rxstate = 0;
  rxcnt = 0;
  crc = 0;
  ans_datalen = 0;

  ans_offset   = 0;
  ans_metadata = 0;
  ans_index    = 0;

	lastrecvtime = nstime();

  while (true)
  {
  	r = comm.Read(&rwbuf[rwbuflen], sizeof(rwbuf) - rwbuflen);
  	if (r <= 0)
  	{
  		if (r == -EAGAIN)
  		{
  			if (nstime() - lastrecvtime > timeout * 1000000000)
  			{
  				throw EUdoAbort(UDOERR_TIMEOUT, "%s timeout", opstring);
  			}
  			continue;
  		}
  		throw EUdoAbort(UDOERR_TIMEOUT, "%s response read error: %d", opstring, r);
  	}

  	lastrecvtime = nstime();

#if TRACE_COMM
  	printf("<< ");
  	for (unsigned bi = 0; bi < r; ++bi)  printf(" %02X", rwbuf[rwbuflen + bi]);
  	printf("\n");
#endif

  	rwbuflen += r;

  	while (rxreadpos < int(rwbuflen))
  	{
			uint8_t b = rwbuf[rxreadpos];

			if ((rxstate > 0) && (rxstate < 10))
			{
				crc = udo_calc_crc(crc, b);
			}

			if (0 == rxstate)  // waiting for the sync byte
			{
				if (0x55 == b)
				{
					crc = udo_calc_crc(0, b); // start the CRC from zero
					rxstate = 1;
				}
			}
			else if (1 == rxstate) // command and lengths
			{
        if (((b & 0x80) != 0) != iswrite)  // does the response R/W differ from the request ?
				{
          rxstate = 0;
				}
        else
        {
          // decode the length fields
          offslen = (0x4210 >> ((b & 3) << 2)) & 0xF;
          metalen = (0x4210 >> (b & 0xC)) & 0xF;  // its already multiple by 4

          rxcnt = 0;
          rxstate = 3;  // index follows normally

          lencode = ((b >> 4) & 7);
          if      (lencode < 5)   ans_datalen = ((0x84210 >> (lencode << 2)) & 0xF); // in-line demultiplexing
          else if (5 == lencode)  ans_datalen = 16;
          else if (7 == lencode)  rxstate = 2;     // extended length follows
          else  // 6 == error code
          {
            ans_datalen = 2;
            iserror = true;
          }
        }
			}
			else if (2 == rxstate) // extended length
			{
				if (0 == rxcnt)
				{
					ans_datalen = b; // low byte
					rxcnt = 1;
				}
				else
				{
					ans_datalen |= (b << 8); // high byte
					rxcnt = 0;
					rxstate = 3; // index follows
				}
			}
			else if (3 == rxstate) // index
			{
				if (0 == rxcnt)
				{
					ans_index = b;  // index low
					rxcnt = 1;
				}
				else
				{
					ans_index |= (b << 8);  // index high
					rxcnt = 0;
  				if (offslen > 0)
  				{
  					rxstate = 4;  // offset follows
  				}
  				else if (metalen > 0)
  				{
  					rxstate = 5;  // meta follows
  				}
  				else if (ans_datalen > 0)
  				{
            rxstate = 6;  // read data or error code
  				}
          else
          {
            rxstate = 10;  // then crc check
          }
				}
			}
  		else if (4 == rxstate) // offset
  		{
        ans_offset |= (b << (rxcnt << 3));
  			++rxcnt;
  			if (rxcnt >= offslen)
  			{
          rxcnt = 0;
  				if (metalen > 0)
  				{
  					rxstate = 5;  // meta follows
  				}
  				else if (ans_datalen > 0)
  				{
            rxstate = 6;  // read data or error code
  				}
          else
          {
            rxstate = 10;  // then crc check
          }
  			}
  		}
  		else if (5 == rxstate) // metadata
  		{
        ans_metadata |= (b << (rxcnt << 3));
  			++rxcnt;
  			if (rxcnt >= metalen)
  			{
          rxcnt = 0;
  				if (ans_datalen > 0)
  				{
            rxstate = 6;  // read data or error code
  				}
          else
          {
            rxstate = 10;  // then crc check
          }
  			}
  		}
  		else if (6 == rxstate) // read data (or error code)
  		{
        if (0 == rxcnt)  // save the answer data start position
        {
          rwbuf_ansdatapos = rxreadpos;
        }

        // just count the bytes, the rwbuf_ansdatapos points to its start
  			++rxcnt;
  			if (rxcnt >= ans_datalen)
  			{
  				rxstate = 10;
  			}
  		}
  		else if (10 == rxstate) // crc check
  		{
  			if (b != crc)
  			{
          throw EUdoAbort(UDOERR_CRC, "%s CRC error", opstring);
  			}
  			else  // CRC OK
  			{
  				if (iserror)
  				{
            ecode = *(uint16_t *)&rwbuf[rwbuf_ansdatapos];
            throw EUdoAbort(ecode, "%s result: %.4X", opstring, ecode);
  				}
          else
          {
    				return;  // --> everything is ok, return to the caller
          }
  			}
  		}

			++rxreadpos;
  	}
  }
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
