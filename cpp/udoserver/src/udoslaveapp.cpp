/*
 *  file:     udoslaveapp.cpp
 *  brief:    UDO Slave Application Implementation (udo request handling)
 *  created:  2024-09-18
 *  authors:  nvitya
*/

#include <udoslaveapp.h>
#include "udo_comm.h"

// the udoslave_app_read_write() is called from the communication system (Serial or IP) to
// handle the actual UDO requests

bool udoslave_app_read_write(TUdoRequest * udorq)
{
#if 0
  if (param_read_write(udorq)) // try the parameter table first
  {
    return true;
  }

#endif

  if (!udocomm.Opened())
  {
  	return udoslave_handle_base_objects(udorq);
  }

  // forward all requests
  try
  {
  	if (udorq->iswrite)
  	{
  		udocomm.UdoWrite(udorq->index,  udorq->offset, udorq->dataptr, udorq->rqlen);
  		return udo_response_ok(udorq);
  	}
  	else
  	{
  		int r = udocomm.UdoRead(udorq->index,  udorq->offset, udorq->dataptr, udorq->maxanslen);
  		udorq->anslen = r;
  		return udo_response_ok(udorq);
  	}
  }
  catch (EUdoAbort &e)
	{
  	return udo_response_error(udorq, e.ecode);
	}
}
