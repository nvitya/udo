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
 *  file:     udoslave.h
 *  brief:    Generic UDO Slave Implementations
 *  created:  2023-05-01
 *  authors:  nvitya
*/

#ifndef SRC_UDOSLAVE_H_
#define SRC_UDOSLAVE_H_

#include "udo.h"

class TUdoSlave
{
public:
	virtual       ~TUdoSlave() { }

	virtual bool  UdoReadWrite(TUdoRequest * udorq); // must be overridden

protected:
	bool          UdoBlobTest(TUdoRequest * udorq);
};

bool      udo_response_error(TUdoRequest * udorq, uint16_t aresult);
bool      udo_response_ok(TUdoRequest * udorq);
bool      udo_ro_int(TUdoRequest * udorq, int avalue, unsigned len);
bool      udo_ro_uint(TUdoRequest * udorq, unsigned avalue, unsigned len);

bool      udo_rw_data(TUdoRequest * udorq, void * dataptr, unsigned datalen);
bool      udo_ro_data(TUdoRequest * udorq, void * dataptr, unsigned datalen);
bool      udo_wo_data(TUdoRequest * udorq, void * dataptr, unsigned datalen);
bool      udo_response_cstring(TUdoRequest * udorq, char * astr);
int32_t   udorq_intvalue(TUdoRequest * udorq);
uint32_t  udorq_uintvalue(TUdoRequest * udorq);

#endif /* SRC_UDOSLAVE_H_ */
