/*
 * commh_udosl.h
 *
 *  Created on: Jun 11, 2023
 *      Author: vitya
 */

#ifndef COMMH_UDOIP_H_
#define COMMH_UDOIP_H_

#include <string>
#include "stdint.h"
#include "udo_comm.h"
#include "nstime.h"

#ifdef WINDOWS
  #include <winsock.h>

  typedef int  socklen_t;

#else
  #include <sys/socket.h>
  #include <arpa/inet.h>
#endif

using namespace std;

#define UDOIP_MAX_DATALEN   UDO_MAX_PAYLOAD_LEN
#define UDOIP_MAX_RQ_SIZE   (UDOIP_MAX_DATALEN + 16) // 1024 byte payload + 16 byte header

#define UDOIP_DEFAULT_PORT  1221

typedef struct
{
	uint32_t    rqid;       // request id to detect repeated requests
	uint16_t    len_cmd;    // LEN, MLEN, RW
	uint16_t    index;      // object index
	uint32_t    offset;
	uint32_t    metadata;
//
} TUdoIpRqHeader; // 16 bytes

class TCommHandlerUdoIp : public TUdoCommHandler
{
private:
  typedef TUdoCommHandler super;

public:
	string               ipaddrstr;

  int                  fdsocket = -1;
  struct sockaddr_in   server_addr;
  struct sockaddr_in   client_addr;

	TCommHandlerUdoIp();
	virtual ~TCommHandlerUdoIp();

public:
	virtual void       Open();
	virtual void       Close();
	virtual bool       Opened();
	virtual string     ConnString();

	virtual int        UdoRead(uint16_t index, uint32_t offset, void * dataptr, uint32_t maxdatalen);
	virtual void       UdoWrite(uint16_t index, uint32_t offset, void * dataptr, uint32_t datalen);

protected:

  int        max_tries = 3;
  uint16_t   cursqnum = 0;

  uint8_t    rqbuf[UDOIP_MAX_RQ_SIZE];
  uint8_t    ansbuf[UDOIP_MAX_RQ_SIZE];

  struct sockaddr_in   response_addr;
  socklen_t            rsp_addr_len = 0;
  fd_set     rxpoll_fds;

  string     opstring;

  bool       iswrite = false;
  uint16_t   mindex = 0;
  uint32_t   moffset = 0;
  uint32_t   mmetadata = 0;
  uint32_t   mrqlen = 0;
  uint8_t *  mdataptr = nullptr;

  uint32_t   ans_index = 0;
  uint32_t   ans_offset = 0;
  uint32_t   ans_metadata = 0;
  int        ans_datalen = 0;

  void       DoUdoReadWrite();

};

extern TCommHandlerUdoIp  udoip_commh;

#endif /* COMMH_UDOIP_H_ */
