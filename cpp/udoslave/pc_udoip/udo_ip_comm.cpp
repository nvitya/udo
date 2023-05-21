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
    miprq.srcip = *(uint32_t *)&client_addr.sin_addr;
    miprq.srcport = ntohs(client_addr.sin_port);
    miprq.datalen = r;
    miprq.dataptr = rqbuf;
  }
  return r;
}

int TUdoIpComm::UdpRespond(void * srcbuf, unsigned buflen)
{
  // dst address and port is already set
  int  r = sendto(fdsocket, srcbuf, buflen, 0, (struct sockaddr*)&client_addr, client_struct_length);
  return r;
}
