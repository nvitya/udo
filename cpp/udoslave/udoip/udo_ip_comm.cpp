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
 *  file:     udoipslave.cpp
 *  brief:    UDO IP (UDP) Slave implementation
 *  created:  2023-05-01
 *  authors:  nvitya
*/

#include "string.h"
#include "udo_ip_comm.h"
#include <fcntl.h>
#include "traces.h"

#include "udoslaveapp.h"

TUdoIpComm g_udoip_comm;

uint8_t udoip_rq_buffer[UDOIP_MAX_RQ_SIZE];
uint8_t udoip_ans_cache_buffer[UDOIP_ANSCACHE_NUM * UDOIP_MAX_RQ_SIZE]; // this might be relative big!

TUdoIpComm::TUdoIpComm()
{


}

TUdoIpComm::~TUdoIpComm()
{

}

bool TUdoIpComm::Init()
{
  initialized = false;

  rqbuf = &udoip_rq_buffer[0];
  rqbufsize = sizeof(udoip_rq_buffer);

  // initialize the ans_cache

  uint32_t offs = 0;
  for (unsigned n = 0; n < UDOIP_ANSCACHE_NUM; ++n)
  {
    memset(&ans_cache[n], 0, sizeof(ans_cache[n]));
    ans_cache[n].dataptr = &udoip_ans_cache_buffer[offs];
    ans_cache[n].idx = n;
    offs += UDOIP_MAX_RQ_SIZE;
    ans_cache_lru_idx[n] = n;
  }

  if (!UdpInit())
  {
    return false;
  }

  // ok.

  initialized = true;
  return true;
}

void TUdoIpComm::Run()
{
  // Receive client's message:
  int r = UdpRecv(); // fills the mcurq on success
  if (r > 0)
  {
    // something was received, mcurq is prepared

    ProcessUdpRequest(&mucrq);

    last_request_mstime = mscounter();
  }
}

void TUdoIpComm::ProcessUdpRequest(TUdoIpRequest * ucrq)
{
	int r;

	TUdoIpRqHeader * prqh = (TUdoIpRqHeader *)ucrq->dataptr;

#if 0
	TRACETS("UDP RQ: srcip=%s, port=%i, datalen=%i\n",
				 inet_ntoa(*(struct in_addr *)&ucrq->srcip), ucrq->srcport, ucrq->datalen);

	TRACE("  sqid=%u, len_cmd=%u, address=%04X, offset=%u\n", prqh->rqid, prqh->len_cmd, prqh->index, prqh->offset);
#endif

	// prepare the response

	// Check against the answer cache
	TUdoIpSlaveCacheRec * pansc;

	pansc = FindAnsCache(ucrq, prqh);
	if (pansc)
	{
		//TRACE(" (sending cached answer)\n");
		// send back the cached answer, avoid double execution
    r = UdpRespond(pansc->dataptr, pansc->datalen);
    if (r <= 0)
    {
      TRACE("UdoIpSlave: error sending back the answer!\r\n");
    }
		return;
	}

	// allocate an answer cache record
	pansc = AllocateAnsCache(ucrq, prqh);

	TUdoIpRqHeader * pansh = (TUdoIpRqHeader *)pansc->dataptr;
	*pansh = *prqh;  // initialize the answer header with the request header
	uint8_t * pansdata = (uint8_t *)(pansh + 1); // the data comes right after the header

	// execute the UDO request

	memset(&mudorq, 0, sizeof(mudorq));
	mudorq.index  = prqh->index;
	mudorq.offset = prqh->offset;
	mudorq.rqlen = (prqh->len_cmd & 0x7FF);
	mudorq.iswrite = ((prqh->len_cmd >> 15) & 1);
	mudorq.metalen = ((0x8420 >> ((prqh->len_cmd >> 13) & 3) * 4) & 0xF);
	mudorq.metadata = prqh->metadata;


	if (mudorq.iswrite)
	{
		// write
		mudorq.dataptr = (uint8_t *)(prqh + 1); // the data comes after the header
		mudorq.rqlen = ucrq->datalen - sizeof(TUdoIpRqHeader);  // override the datalen from the header

    g_slaveapp.UdoReadWrite(&mudorq);
	}
	else
	{
		// read
		mudorq.maxanslen = UDOIP_MAX_RQ_SIZE - sizeof(TUdoIpRqHeader);
		mudorq.dataptr = pansdata;

		g_slaveapp.UdoReadWrite(&mudorq);  // should set the mudorq.anslen
	}

	if (mudorq.result)
	{
		mudorq.anslen = 2;
		pansh->len_cmd |= 0x7FF; // abort response
		*(uint16_t *)pansdata = mudorq.result;
	}

	pansc->datalen = mudorq.anslen + sizeof(TUdoIpRqHeader);

	// send the response
  r = UdpRespond(pansc->dataptr, pansc->datalen);
	if (r <= 0)
	{
		TRACE("UdoIpSlave: error sending back the answer!\n");
	}
}

TUdoIpSlaveCacheRec * TUdoIpComm::FindAnsCache(TUdoIpRequest * iprq, TUdoIpRqHeader * prqh)
{
	TUdoIpSlaveCacheRec * pac = &ans_cache[0];
	TUdoIpSlaveCacheRec * last_pac = pac + UDOIP_ANSCACHE_NUM;
	while (pac < last_pac)
	{
		if ( (pac->srcip == iprq->srcip) and (pac->srcport == iprq->srcport)
				 and (pac->rqh.rqid == prqh->rqid) and (pac->rqh.len_cmd == prqh->len_cmd)
				 and (pac->rqh.index == prqh->index) and (pac->rqh.offset == prqh->offset)
				 and (pac->datalen == iprq->datalen)
			 )
		{
			// found the previous request, the response was probably lost
			return pac;
		}
		++pac;
	}

	return nullptr;
}

TUdoIpSlaveCacheRec * TUdoIpComm::AllocateAnsCache(TUdoIpRequest * iprq, TUdoIpRqHeader * prqh)
{
	// re-use the oldest entry
	uint8_t idx = ans_cache_lru_idx[0]; // the oldest entry
	memcpy(&ans_cache_lru_idx[0], &ans_cache_lru_idx[1], UDOIP_ANSCACHE_NUM - 1); // rotate the array
	ans_cache_lru_idx[UDOIP_ANSCACHE_NUM-1] = idx; // append to the end

	TUdoIpSlaveCacheRec * pac = &ans_cache[idx];

	pac->srcip   = iprq->srcip;
	pac->srcport = iprq->srcport;
	pac->rqh     = *prqh;

	return pac;
}

#if defined(ARDUINO)

bool TUdoIpComm::WifiConnected()
{
  if (wifi_connected)
  {
    if (!prev_wifi_connected)
    {
      wudp.begin(WiFi.localIP(), UDPCAN_SLAVE_PORT);

      TRACE("UDPCAN is ready for requests at %s\r\n", WiFi.localIP().toString().c_str());
    }
  }

  prev_wifi_connected = wifi_connected;

  return wifi_connected;
}

bool TUdoIpComm::UdpInit()
{
  return true;
}

int TUdoIpComm::UdpRecv()
{
  if (!WifiConnected())
  {
    return 0;
  }

  int packetsize = wudp.parsePacket();
  if (packetsize)
  {
    int len = wudp.read((char *)rqbuf, rqbufsize);
    if (len > 0)
    {
      client_addr = wudp.remoteIP();
      client_port = wudp.remotePort();

      mucrq.srcip = client_addr;
      mucrq.srcport = client_port;
      mucrq.datalen = len;
      mucrq.dataptr = rqbuf;

      return len;
    }
  }

  return 0;
}

int TUdoIpComm::UdpRespond(void * srcbuf, unsigned buflen)
{
  if (!WifiConnected())
  {
    return 0;
  }

  wudp.beginPacket(client_addr, client_port);
  unsigned r = wudp.write((uint8_t *)srcbuf, buflen);
  wudp.endPacket();

  return r;
}

#elif defined(CPU_ARMM)

bool TUdoIpComm::UdpInit()
{
  udps.Init(&ip4_handler, UDOIP_DEFAULT_PORT);

  return true;
}

int TUdoIpComm::UdpRecv()
{
  int r = udps.Receive(rqbuf, rqbufsize); // sets udps.srcaddr, udps.srcport
  if (r > 0)
  {
    mucrq.srcip = udps.srcaddr.u32;
    mucrq.srcport = udps.srcport;
    mucrq.datalen = r;
    mucrq.dataptr = rqbuf;

    // prepare the response addresses
    udps.destaddr = udps.srcaddr;
    udps.destport = udps.srcport;
  }
  return r;
}

int TUdoIpComm::UdpRespond(void * srcbuf, unsigned buflen)
{
  // dst address and port is already set
  int r = udps.Send(srcbuf, buflen);
  return r;
}

#else

bool TUdoIpComm::UdpInit()
{
  // Create UDP socket:
  fdsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fdsocket < 0)
  {
    TRACE("UdoIpComm: Error creating socket\n");
    return false;
  }

  // Set port and IP:
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr("0.0.0.0");  // bind to all interfaces

  // set the socket non-blocking
  #ifdef WIN32
    ioctlsocket(fdsocket, FIONBIO, &noBlock);
  #else
    int flags = fcntl(fdsocket, F_GETFL, 0);
    fcntl(fdsocket, F_SETFL, flags | O_NONBLOCK);
  #endif

  // Bind to the set port and IP:
  if (bind(fdsocket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
  {
    TRACE("UdoIpComm: bind error (is another slave already running?)\n");
    return false;
  }

  return true;
}

int TUdoIpComm::UdpRecv()
{
  int r = recvfrom(fdsocket, rqbuf, rqbufsize, 0, (struct sockaddr*)&client_addr, &client_struct_length);
  if (r > 0)
  {
    mucrq.srcip = *(uint32_t *)&client_addr.sin_addr;
    mucrq.srcport = ntohs(client_addr.sin_port);
    mucrq.datalen = r;
    mucrq.dataptr = rqbuf;
  }
  return r;
}

int TUdoIpComm::UdpRespond(void * srcbuf, unsigned buflen)
{
  // dst address and port is already set
  int  r = sendto(fdsocket, srcbuf, buflen, 0, (struct sockaddr*)&client_addr, client_struct_length);
  return r;
}

#endif
