/*
 * commh_udosl.cpp
 *
 *  Created on: Jun 11, 2023
 *      Author: vitya
 */

#include "string.h"
#include "stdlib.h"
#include "udo.h"
#include "udo_comm.h"
#include "commh_udoip.h"
#include <fcntl.h>
#include <unistd.h>
#include "general.h"

TCommHandlerUdoIp  udoip_commh;

#ifdef WIN32

bool     winsock_initialized = false;
WSAData  winsock_wsaData;

void InitWinsock()
{
	if (!winsock_initialized)
	{
		// Start Winsock up
		WSAStartup(MAKEWORD(1, 1), &winsock_wsaData);
	}
}

#endif


TCommHandlerUdoIp::TCommHandlerUdoIp()
{
	protocol = UCP_IP;
	ipaddrstr = string("127.0.0.1:1221");

	default_timeout = 0.5;  // lower the default timeout
	timeout = default_timeout;
}

TCommHandlerUdoIp::~TCommHandlerUdoIp()
{
	// TODO Auto-generated destructor stub
}

void TCommHandlerUdoIp::Open()
{

	#ifdef WIN32
		InitWinsock();
  #endif

  // Create UDP socket:
  fdsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fdsocket < 0)
  {
  	throw EUdoAbort(UDOERR_CONNECTION, "UDO-IP: error creating socket");
  }

  // prapare the master IP+Port

  string svr_ip_str = ipaddrstr;
  string svr_port_str = "1221";

  int i = ipaddrstr.find(':');
  if (i > 0)
  {
  	svr_ip_str = ipaddrstr.substr(0, i);
  	svr_port_str = ipaddrstr.substr(i+1, 9999);
  }

  // Set port and IP:
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(svr_port_str.c_str()));
  server_addr.sin_addr.s_addr = inet_addr(svr_ip_str.c_str());  // bind to all interfaces

#if 0
  // set the socket non-blocking
  #ifdef WIN32
    ioctlsocket(fdsocket, FIONBIO, &noBlock);
  #else
    int flags = fcntl(fdsocket, F_GETFL, 0);
    fcntl(fdsocket, F_SETFL, flags | O_NONBLOCK);
  #endif
#endif

  cursqnum = 0;  // always start at zero, and increment, the port number will be at every connection different
}

void TCommHandlerUdoIp::Close()
{
	if (fdsocket >= 0)
	{
		close(fdsocket);
		fdsocket = -1;
	}
}

bool TCommHandlerUdoIp::Opened()
{
  return (fdsocket != -1);
}

string TCommHandlerUdoIp::ConnString()
{
	return StringFormat("UDO-IP %s", ipaddrstr.c_str());
}

int TCommHandlerUdoIp::UdoRead(uint16_t index, uint32_t offset, void * dataptr, uint32_t maxdatalen)
{
  iswrite = false;
	mindex  = index;
  moffset = offset;
  mdataptr = (uint8_t *)dataptr;
  mrqlen = maxdatalen;

  opstring = StringFormat("UdoRead(%.4X, %d)", mindex, moffset);

  DoUdoReadWrite();

	return ans_datalen;
}

void TCommHandlerUdoIp::UdoWrite(uint16_t index, uint32_t offset, void * dataptr, uint32_t datalen)
{
  iswrite = true;
  mindex = index;
  moffset  = offset;
  mdataptr = (uint8_t *)dataptr;
  mrqlen = datalen;

  if (mrqlen > UDOIP_MAX_DATALEN)
  {
  	throw EUdoAbort(UDOERR_DATA_TOO_BIG, "%s write data is too big: %d", opstring, mrqlen);
  }

  opstring = StringFormat("UdoWrite(%.4X, %d)[%d]", mindex, moffset, mrqlen);

  DoUdoReadWrite();
}

void TCommHandlerUdoIp::DoUdoReadWrite()
{
  socklen_t rsp_addr_len;
  int r;
  int trynum;
  uint16_t ecode;
  struct timeval tv;

  ++cursqnum; // increment the sequence number at every new request

  TUdoIpRqHeader * rqhead = (TUdoIpRqHeader *)&rqbuf[0];
  TUdoIpRqHeader * anshead = (TUdoIpRqHeader *)&ansbuf[0];
  int headsize = sizeof(TUdoIpRqHeader);

  rqhead->rqid     = cursqnum;
  rqhead->index    = mindex;
  rqhead->offset   = moffset;
  rqhead->metadata = mmetadata;

  if (iswrite)
  {
    rqhead->len_cmd = (mrqlen | (1 << 15));
    memcpy(&rqbuf[headsize], mdataptr, mrqlen);
  }
  else  // read
  {
    rqhead->len_cmd = mrqlen;  // bit15 = 0: read
  };


#ifdef WINDOWS
  int timeout_ms = timeout * 1000;
  setsockopt(fdsocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
#else
  tv.tv_sec = timeout;
  tv.tv_usec = int(timeout * 1000000) % 1000000;
  setsockopt(fdsocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(struct timeval));
#endif

  trynum = 0;
  while (true)
  {
  	++trynum;

		r = sendto(fdsocket, (char *)&rqbuf[0], headsize + mrqlen, 0, (sockaddr *)&server_addr, sizeof(server_addr));
		//printf("sendto result: %i, errno=%i\n", r, errno);
    if (r < 0)
    {
    	throw EUdoAbort(UDOERR_CONNECTION, "%s: send error: %d", opstring.c_str(), errno);
    }

    rsp_addr_len = sizeof(response_addr);
		r = recvfrom(fdsocket, (char *)&ansbuf[0], sizeof(ansbuf), 0, (sockaddr *)&response_addr, &rsp_addr_len);
		//printf("recvfrom result: %i, errno = %i\n", r, errno);
		if (r <= 0)
		{
			if (errno == EAGAIN)
			{
				if (trynum < max_tries)
				{
					continue;  // re-send on timeout
				}

				throw EUdoAbort(UDOERR_TIMEOUT, "%s: timeout", opstring.c_str());
			}
			else
			{
				throw EUdoAbort(UDOERR_CONNECTION, "%s: receive error: %i", opstring.c_str(), errno);
			}
		}

		ans_datalen = r - headsize; // data length
		if (ans_datalen < 0)
		{
			// something invalid received
			if (trynum < max_tries)
			{
				continue;
			}

			throw EUdoAbort(UDOERR_CONNECTION, "%s invalid response length: %d", opstring.c_str(), ans_datalen);
		}

		if ((anshead->rqid != cursqnum) || (anshead->index != mindex) || (anshead->offset != moffset))
		{
			if (trynum < max_tries)
			{
				continue;
			}

			throw EUdoAbort(UDOERR_CONNECTION, "%s unexpected response", opstring.c_str());
		}

		if ((anshead->len_cmd & 0x7FF) == 0x7FF) // error response ?
		{
			if (r < int(sizeof(TUdoIpRqHeader) + 2))
			{
				throw EUdoAbort(UDOERR_CONNECTION, "%s error response length: %d", opstring.c_str(), r);
			}

			ecode = *(uint16_t *)&ansbuf[headsize];
			throw EUdoAbort(ecode, "%s result: %.4X", opstring.c_str(), ecode);
		}

		if (!iswrite)
		{
			if (ans_datalen > 0)
			{
				if (ans_datalen > int(mrqlen))
				{
					throw EUdoAbort(UDOERR_DATA_TOO_BIG, "%s result data is too big: %d", opstring.c_str(), ans_datalen);
				}

				memcpy(mdataptr, &ansbuf[headsize], ans_datalen);
			}
		}

		break; // everything was ok.

	} // while
}
