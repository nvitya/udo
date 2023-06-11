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

using namespace std;

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
};

extern TCommHandlerUdoSl  udosl_commh;

#endif /* COMMH_UDOSL_H_ */
