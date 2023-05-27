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

void TUdoIpComm::WifiConnected(bool awifi_connected)
{
  if (awifi_connected)
  {
    if (!wifi_connected)
    {
      wudp.begin(WiFi.localIP(), port);

      TRACE("UDO-IP is ready for requests at %s\r\n", WiFi.localIP().toString().c_str());
    }
  }

  wifi_connected = awifi_connected;
}

bool TUdoIpComm::UdpInit()
{
  return true;
}

int TUdoIpComm::UdpRecv()
{
  if (!wifi_connected)
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

      miprq.srcip = client_addr;
      miprq.srcport = client_port;
      miprq.datalen = len;
      miprq.dataptr = rqbuf;

      return len;
    }
  }

  return 0;
}

int TUdoIpComm::UdpRespond(void * srcbuf, unsigned buflen)
{
  if (!wifi_connected)
  {
    return 0;
  }

  wudp.beginPacket(client_addr, client_port);
  unsigned r = wudp.write((uint8_t *)srcbuf, buflen);
  wudp.endPacket();

  return r;
}
