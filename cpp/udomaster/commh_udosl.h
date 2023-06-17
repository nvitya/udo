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
private:
  typedef TUdoCommHandler super;

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
  int        rxreadpos = 0;
  int        rxcnt = 0;
  int        rxstate = 0;
  uint8_t    crc = 0;
  nstime_t   lastrecvtime = 0;

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

  int        rwbuf_ansdatapos = 0;  // the start of the answer data in the rwbuf (answer phase)

  uint32_t   rwbuflen = 0;
  uint8_t    rwbuf[UDOSL_MAX_RQ_SIZE - 1];

  void       SendRequest();
  void       RecvResponse();

  int        AddTx(void * asrc, int len);
  int        TxAvailable();


};

extern TCommHandlerUdoSl  udosl_commh;

#endif /* COMMH_UDOSL_H_ */
