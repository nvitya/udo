/*
 * commh_udosl.cpp
 *
 *  Created on: Jun 11, 2023
 *      Author: vitya
 */

#include "commh_udosl.h"
#include "general.h"

TCommHandlerUdoSl  udosl_commh;

TCommHandlerUdoSl::TCommHandlerUdoSl()
{
	devstr = string("");
  comm.baudrate = 1000000;
}

TCommHandlerUdoSl::~TCommHandlerUdoSl()
{
	// TODO Auto-generated destructor stub
}

void TCommHandlerUdoSl::Open()
{
	comm.Close();
	if (!comm.Open(devstr.c_str()))
	{
		throw EUdoAbort(UDOERR_CONNECTION, "UDO-SL: error opening device \"%s\"", devstr.c_str());
	}
}

void TCommHandlerUdoSl::Close()
{
	comm.Close();
}

bool TCommHandlerUdoSl::Opened()
{
  return comm.Opened();
}

string TCommHandlerUdoSl::ConnString()
{
	return StringFormat("UDO-SL %s", devstr.c_str());
}

int TCommHandlerUdoSl::UdoRead(uint16_t index, uint32_t offset, void * dataptr, uint32_t maxdatalen)
{
  return 0;
}

void TCommHandlerUdoSl::UdoWrite(uint16_t index, uint32_t offset, void * dataptr, uint32_t datalen)
{

}
