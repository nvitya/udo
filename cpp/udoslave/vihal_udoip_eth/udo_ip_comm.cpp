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

TUdoIpComm g_udoip_comm;

bool TUdoIpComm::UdpInit()
{
  udps.Init(&ip4_handler, port);

  return true;
}

int TUdoIpComm::UdpRecv()
{
  int r = udps.Receive(rqbuf, rqbufsize); // sets udps.srcaddr, udps.srcport
  if (r > 0)
  {
    miprq.srcip = udps.srcaddr.u32;
    miprq.srcport = udps.srcport;
    miprq.datalen = r;
    miprq.dataptr = rqbuf;

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
