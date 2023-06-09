/*
 * udo_comm.h
 *
 *  Created on: Jun 11, 2023
 *      Author: vitya
 */

#ifndef UDO_COMM_H_
#define UDO_COMM_H_

#include "stdint.h"
#include "udo.h"
#include <exception>
#include <string>

using namespace std;

#define  UDO_MAX_PAYLOAD_LEN  1024

enum TUdoCommProtocol
{
	UCP_NONE = 0,
	UCP_SERIAL = 1,
	UCP_IP = 2
};

class EUdoAbort : public exception
{
public:
	uint16_t  ecode;
	string    emsg;

	EUdoAbort(uint16_t acode, const char * afmt, ...);

  const char * what () const throw ()
  {
    return emsg.c_str();
  }
};

class TUdoCommHandler
{
public:
	float             default_timeout = 1.0;
	float             timeout = 1.0;
	TUdoCommProtocol  protocol = UCP_NONE;

	/* constructor */ TUdoCommHandler();
	virtual           ~TUdoCommHandler();

public:
	virtual void       Open();
	virtual void       Close();
	virtual bool       Opened();
	virtual string     ConnString();

	virtual int        UdoRead(uint16_t index, uint32_t offset, void * dataptr, uint32_t maxdatalen);
	virtual void       UdoWrite(uint16_t index, uint32_t offset, void * dataptr, uint32_t datalen);
};

class TUdoComm
{
public:
	TUdoCommHandler *  commh;  // defaults to commh_none
	uint16_t           max_payload_size;

	TUdoComm();
	virtual            ~TUdoComm();

	void               SetHandler(TUdoCommHandler * acommh);

	void               Open();
	void               Close();
	bool               Opened();

	int                UdoRead(uint16_t index, uint32_t offset, void * dataptr, uint32_t maxdatalen);
	void               UdoWrite(uint16_t index, uint32_t offset, void * dataptr, uint32_t datalen);

public: // utility functions
	int                ReadBlob(uint16_t index, uint32_t offset, void *  dataptr, uint32_t maxdatalen);
	void               WriteBlob(uint16_t index, uint32_t offset, void *  dataptr, uint32_t datalen);

	int32_t            ReadI32(uint16_t index, uint32_t offset);
	int16_t            ReadI16(uint16_t index, uint32_t offset);
	uint32_t           ReadU32(uint16_t index, uint32_t offset);
	uint16_t           ReadU16(uint16_t index, uint32_t offset);
	uint8_t            ReadU8(uint16_t index, uint32_t offset);
	void               WriteI32(uint16_t index, uint32_t offset, int32_t avalue);
	void               WriteI16(uint16_t index, uint32_t offset, int16_t avalue);
	void               WriteU32(uint16_t index, uint32_t offset, uint32_t avalue);
	void               WriteU16(uint16_t index, uint32_t offset, uint16_t avalue);
	void               WriteU8(uint16_t index, uint32_t offset, uint8_t avalue);
};

extern TUdoCommHandler  commh_none;
extern TUdoComm         udocomm;

#endif /* UDO_COMM_H_ */
