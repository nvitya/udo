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
 *  brief:    UDO IP (UDP) Slave implementation
 *  created:  2023-05-01
 *  authors:  nvitya
*/

#ifndef UDO_IP_SLAVE_H_
#define UDO_IP_SLAVE_H_

#include "platform.h"

#if defined(ARDUINO)

  #include "Arduino.h"
  #include <WiFi.h>
  #include <WiFiUdp.h>
  #include "board_pins.h"

#elif defined(CPU_ARMM)  // vihal
  #include "board_pins.h"

  #include "netadapter.h"
  #include "net_ip4.h"
#else // linux
  #include "unistd.h"
  #include <string.h>
  #include <sys/socket.h>
  #include <arpa/inet.h>
#endif

#include "mscounter.h"

#include "udo.h"
#include "udoslave.h"

typedef struct
{
	uint32_t    srcip;     // source IP (IPv4)
	uint16_t    srcport;   // LSB format

	uint16_t    datalen;
	uint8_t *   dataptr;
//
} TUdoIpRequest;

typedef struct
{
	uint32_t    rqid;       // request id to detect repeated requests
	uint16_t    len_cmd;    // LEN, MLEN, RW
	uint16_t    index;      // object index
	uint32_t    offset;
	uint32_t    metadata;
//
} TUdoIpRqHeader; // 16 bytes

#define UDOIP_DEFAULT_PORT  1221
#define UDOIP_MAX_RQ_SIZE  (1024 + 16)  // 1024 byte payload + 16 byte header

#ifndef UDOIP_ANSCACHE_NUM
  #define UDOIP_ANSCACHE_NUM  4  // this is also the parallel clients supported
                                 // requires 6kByte RAM (= 4 * 1.5 k)
#endif

typedef struct
{
	uint8_t         idx;
	uint8_t         _reserved;
	uint16_t        srcport;

	uint32_t        srcip;

	TUdoIpRqHeader  rqh;

  uint16_t        datalen;  // data length with header
  uint8_t *       dataptr;
//
} TUdoIpSlaveCacheRec;

class TUdoIpComm
{
public:
	uint16_t              port = UDOIP_DEFAULT_PORT;

public:
	bool                  initialized = false;

	TUdoIpRequest         miprq;
	TUdoRequest           mudorq;

	uint8_t *             rqbuf = nullptr;
	uint32_t              rqbufsize = 0;

	uint8_t               ans_cache_lru_idx[UDOIP_ANSCACHE_NUM];
	TUdoIpSlaveCacheRec   ans_cache[UDOIP_ANSCACHE_NUM];

	uint32_t              last_request_mstime = 0;

	TUdoIpComm();
	virtual ~TUdoIpComm();

	bool Init();
	void Run(); // must be called regularly

public: // platform specific

  bool         UdpInit();
  int          UdpRecv(); // into mcurq.
  int          UdpRespond(void * srcbuf, unsigned buflen);

#if defined(ARDUINO)

  bool         prev_wifi_connected = false;

  IPAddress    client_addr;
  uint16_t     client_port;

  WiFiUDP      wudp;

  bool         WifiConnected();

#elif defined(CPU_ARMM) // vihal

  TUdp4Socket  udps; // VIHAL UDP Socket

#else
  int   fdsocket = -1;
  struct sockaddr_in   server_addr;
  struct sockaddr_in   client_addr;
  socklen_t client_struct_length = sizeof(client_addr);
  socklen_t server_struct_length = sizeof(server_addr);
#endif

public:
	TUdoIpSlaveCacheRec *  FindAnsCache(TUdoIpRequest * iprq, TUdoIpRqHeader * prqh);
	TUdoIpSlaveCacheRec *  AllocateAnsCache(TUdoIpRequest * iprq, TUdoIpRqHeader * prqh);

  void         ProcessUdpRequest(TUdoIpRequest * ucrq);
};

extern TUdoIpComm g_udoip_comm;

#endif /* UDO_IP_SLAVE_H_ */
