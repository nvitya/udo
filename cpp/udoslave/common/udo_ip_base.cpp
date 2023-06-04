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
#include "udo_ip_base.h"
#include <fcntl.h>
#include "udoslave_traces.h"

uint8_t udoip_rq_buffer[UDOIP_MAX_RQ_SIZE];
uint8_t udoip_ans_cache_buffer[UDOIP_ANSCACHE_NUM * UDOIP_MAX_RQ_SIZE]; // this might be relative big!

TUdoIpCommBase::TUdoIpCommBase()
{


}

TUdoIpCommBase::~TUdoIpCommBase()
{

}

bool TUdoIpCommBase::Init()
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

void TUdoIpCommBase::Run()
{
  // Receive client's message:
  int r = UdpRecv(); // fills the mcurq on success
  if (r > 0)
  {
    // something was received, mcurq is prepared

    ProcessUdpRequest(&miprq);

    last_request_mstime = mscounter();
  }
}

void TUdoIpCommBase::ProcessUdpRequest(TUdoIpRequest * ucrq)
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
	}
	else
	{
		// read
		mudorq.maxanslen = UDOIP_MAX_RQ_SIZE - sizeof(TUdoIpRqHeader);
		mudorq.dataptr = pansdata;
	}

	udoslave_app_read_write(&mudorq);

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

TUdoIpSlaveCacheRec * TUdoIpCommBase::FindAnsCache(TUdoIpRequest * iprq, TUdoIpRqHeader * prqh)
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

TUdoIpSlaveCacheRec * TUdoIpCommBase::AllocateAnsCache(TUdoIpRequest * iprq, TUdoIpRqHeader * prqh)
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

