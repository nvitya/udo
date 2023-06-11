/*
 * commh_udosl.h
 *
 *  Created on: Jun 11, 2023
 *      Author: vitya
 */

#ifndef COMMH_UDOSL_H_
#define COMMH_UDOSL_H_

#include <string>
#include "udo_comm.h"
#include "sercomm.h"
#include "nstime.h"

using namespace std;

#define UDOSL_MAX_RQ_SIZE  (UDO_MAX_PAYLOAD_LEN + 32) // 1024 byte payload + variable size header


class TCommHandlerUdoSl : public TUdoCommHandler
{
public:
	string     devstr;
	TSerComm   comm;

	TCommHandlerUdoSl();
	virtual ~TCommHandlerUdoSl();

public:
	virtual void       Open();
	virtual void       Close();
	virtual bool       Opened();
	virtual string     ConnString();

	virtual int        UdoRead(uint16_t index, uint32_t offset, void * dataptr, uint32_t maxdatalen);
	virtual void       UdoWrite(uint16_t index, uint32_t offset, void * dataptr, uint32_t datalen);

protected:
  int        rxreadpos;
  int        rxcnt;
  int        rxstate;
  uint8_t    crc;
  nstime_t   lastrecvtime;

  string     opstring;
  bool       iswrite;
  uint16_t   mindex;
  uint32_t   moffset;
  uint32_t   mmetadata;
  uint32_t   mrqlen;
  uint8_t *  mdataptr;

  uint32_t   ans_index;
  uint32_t   ans_offset;
  uint32_t   ans_metadata;
  int        ans_datalen;

  int        rwbuf_ansdatapos;  // the start of the answer data in the rwbuf (answer phase)

  uint32_t   rwbuflen;
  uint8_t    rwbuf[UDOSL_MAX_RQ_SIZE - 1];

  void       SendRequest();
  void       RecvResponse();

  int        AddTx(void * asrc, int len);
  int        TxAvailable();


};

extern TCommHandlerUdoSl  udosl_commh;

#endif /* COMMH_UDOSL_H_ */
