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
 *  file:     udoipslave.h
 *  brief:    UDO IP (UDP) Slave implementation for ESP32 WiFi
 *  created:  2023-05-27
 *  authors:  nvitya
*/

#ifndef UDO_IP_SLAVE_H_
#define UDO_IP_SLAVE_H_

#include "Arduino.h"

#include <WiFi.h>
#include <WiFiUdp.h>

#include "mscounter.h"

#include "udo.h"
#include "udo.h"
#include "udo_ip_base.h"

class TUdoIpComm : public TUdoIpCommBase
{
public: // platform specific

  virtual bool  UdpInit();
  virtual int   UdpRecv(); // into mcurq.
  virtual int   UdpRespond(void * srcbuf, unsigned buflen);

  bool          wifi_connected = false;

  IPAddress     client_addr;
  uint16_t      client_port;

  WiFiUDP       wudp;

  void          WifiConnected(bool awifi_connected);
};

extern TUdoIpComm g_udoip_comm;

#endif /* UDO_IP_SLAVE_H_ */
