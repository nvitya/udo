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
 *  file:     udo_ip_comm.h (PC_UDOIP)
 *  brief:    UDO IP (UDP) Slave implementation for PC (Linux / Windows)
 *  created:  2023-05-01
 *  authors:  nvitya
*/

#ifndef UDO_IP_COMM_H_
#define UDO_IP_COMM_H_

#include "platform.h"

#include "mscounter.h"

#include "udo.h"
#include "udo_ip_base.h"

#include <string.h>

#ifdef WINDOWS
#include "windows.h"
#include "winsock.h"
#else
#include "unistd.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

class TUdoIpComm : public TUdoIpCommBase
{
public: // platform specific

  virtual bool  UdpInit();
  virtual int   UdpRecv(); // into mcurq.
  virtual int   UdpRespond(void * srcbuf, unsigned buflen);

  int   fdsocket = -1;
  struct sockaddr_in   server_addr;
  struct sockaddr_in   client_addr;
  int  client_struct_length = sizeof(client_addr);
  int  server_struct_length = sizeof(server_addr);
};

extern TUdoIpComm g_udoip_comm;

#endif /* UDO_IP_COMM_H_ */
