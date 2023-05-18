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
 *  file:     udoslave.cpp
 *  brief:    Generic UDO Slave Implementations
 *  created:  2023-05-01
 *  authors:  nvitya
*/

#include "udoslave.h"

#include "udo.h"
#include "string.h"

bool udo_response_error(TUdoRequest * udorq, uint16_t aresult)
{
  udorq->result = aresult;
  return false;
}

bool udo_response_ok(TUdoRequest * udorq)
{
  udorq->result = 0;
  return true;
}

bool udo_ro_int(TUdoRequest * udorq, int avalue, unsigned len)
{
	if (udorq->iswrite)
	{
		udorq->result = UDOERR_READ_ONLY;
		return false;
	}

  *(int *)(udorq->dataptr) = avalue;
  udorq->anslen = len;
  udorq->result = 0;

  return true;
}

bool udo_ro_uint(TUdoRequest * udorq, unsigned avalue, unsigned len)
{
	if (udorq->iswrite)
	{
		udorq->result = UDOERR_READ_ONLY;
		return false;
	}

  *(unsigned *)(udorq->dataptr) = avalue;
  udorq->anslen = len;
  udorq->result = 0;

  return true;
}

bool udo_rw_data(TUdoRequest * udorq, void * dataptr, unsigned datalen)
{
	if (udorq->iswrite)
	{
		// write, handling segmented write too

		//TRACE("udo_write(%04X, %u), len=%u\r\n", udorq->index, udorq->offset, udorq->datalen);

		if (datalen < udorq->offset + udorq->rqlen)
		{
			udorq->result = UDOERR_WRITE_BOUNDS;
			return false;
		}

		uint8_t * cp = (uint8_t *)dataptr;
		cp += udorq->offset;

		uint32_t chunksize = datalen - udorq->offset;  // the maximal possible chunksize
		if (chunksize > udorq->rqlen)  chunksize = udorq->rqlen;

		memcpy(cp, udorq->dataptr, chunksize);
		udorq->result = 0;
		return true;
	}
	else // read, handling segmented read too
	{
		if (datalen <= udorq->offset)
		{
			udorq->anslen = 0; // empty read: no more data
			return true;
		}

		uint8_t * cp = (uint8_t *)dataptr;
		cp += udorq->offset;

		unsigned remaining = datalen - udorq->offset;  // already checked for negative case
		if (remaining > udorq->maxanslen)
		{
			udorq->anslen = udorq->maxanslen;
		}
		else
		{
			udorq->anslen = remaining;
		}
		memcpy(udorq->dataptr, cp, udorq->anslen);
		udorq->result = 0;
		return true;
	}
}

bool udo_ro_data(TUdoRequest * udorq, void * dataptr, unsigned datalen)
{
	if (!udorq->iswrite)
	{
		return udo_rw_data(udorq, dataptr, datalen);
	}

	udorq->result = UDOERR_READ_ONLY;
	return false;
}

bool udo_wo_data(TUdoRequest * udorq, void * dataptr, unsigned datalen)
{
	if (udorq->iswrite)
	{
		return udo_rw_data(udorq, dataptr, datalen);
	}

	udorq->result = UDOERR_WRITE_ONLY;
	return false;
}

bool udo_response_cstring(TUdoRequest * udorq, char * astr)
{
	unsigned len = strlen(astr);
	return udo_ro_data(udorq, astr, len);
}

int32_t udorq_intvalue(TUdoRequest * udorq) // get signed integer value from the udo request
{
	if (udorq->rqlen >= 4)
	{
		return *(int32_t *)udorq->dataptr;
	}
	else if (udorq->rqlen == 2)
	{
		return *(int16_t *)udorq->dataptr;
	}
	else if (udorq->rqlen == 3)
	{
		return (*(int *)udorq->dataptr) & 0x00FFFFFF;
	}
	else if (udorq->rqlen == 0)
	{
		return 0;
	}
	else
	{
		return *(int8_t *)udorq->dataptr;
	}
}

uint32_t udorq_uintvalue(TUdoRequest * udorq) // get unsigned integer value from the udo request
{
	if (udorq->rqlen >= 4)
	{
		return *(uint32_t *)udorq->dataptr;
	}
	else if (udorq->rqlen == 2)
	{
		return *(uint16_t *)udorq->dataptr;
	}
	else if (udorq->rqlen == 3)
	{
		return (*(unsigned *)udorq->dataptr) & 0x00FFFFFF;
	}
	else if (udorq->rqlen == 0)
	{
		return 0;
	}
	else
	{
		return *(uint8_t *)udorq->dataptr;
	}
}

//-----------------------------------------------------------------------------

bool TUdoSlave::UdoReadWrite(TUdoRequest * udorq)
{
	if (0x0000 == udorq->index) // communication test
	{
		return udo_ro_uint(udorq, 0x66CCAA55, 4);
	}
	else if (0x0001 == udorq->index) // maximal payload length
	{
		return udo_ro_uint(udorq, UDO_MAX_DATALEN, 4);
	}
	else if (0x0002 == udorq->index) // blob test object
	{
		return UdoBlobTest(udorq);
	}
	else
	{
		return udo_response_error(udorq, UDOERR_INDEX);
	}
}

bool TUdoSlave::UdoBlobTest(TUdoRequest * udorq)
{
	// provide 16384 incrementing 32-bit integers

	uint32_t datalen = 262144;

	if (udorq->offset & 3)
	{
		return udo_response_error(udorq, UDOERR_WRONG_OFFSET);
	}

	udorq->rqlen = (udorq->rqlen + 3) & 0xFFFC; // force rqlen 4 divisible

	uint32_t v = (udorq->offset >> 2); // starting number

	if (udorq->iswrite)
	{
		if (datalen < udorq->offset + udorq->rqlen)
		{
			udorq->result = UDOERR_WRITE_BOUNDS;
			return false;
		}

		uint32_t * sp = (uint32_t *)udorq->dataptr;
		uint32_t * endp = sp + (udorq->rqlen >> 2);
		while (sp < endp)
		{
			if (*sp++ != v++)
			{
				udorq->result = UDOERR_WRITE_VALUE;
				return false;
			}
		}
	}
	else
	{
		if (datalen <= udorq->offset)
		{
			udorq->anslen = 0; // empty read: no more data
			return true;
		}

		unsigned remaining = datalen - udorq->offset;
		if (remaining > udorq->maxanslen)
		{
			udorq->anslen = udorq->maxanslen;
		}
		else
		{
			udorq->anslen = remaining;
		}

		uint32_t * dp = (uint32_t *)(udorq->dataptr);
		uint32_t * endp = dp + (udorq->anslen >> 2);
		while (dp < endp)
		{
			*dp++ = v++;
		}
	}

	udorq->result = 0;
	return true;
}
