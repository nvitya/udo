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
 *  file:     simple_partable.cpp
 *  brief:    Simple parameter table helping functions
 *  created:  2023-05-13
 *  authors:  nvitya
*/

#include "simple_partable.h"
#include "udoslave.h"

bool param_read_write(TUdoRequest * udorq)
{
	uint16_t index = udorq->index;
	TParamRangeDef * prtab = (TParamRangeDef *)&param_range_table[0];

	while (prtab->lastindex)
	{
		if (index < prtab->firstindex)
		{
			return udo_response_error(udorq, UDOERR_INDEX);
		}

		if ((prtab->firstindex <= index) && (index <= prtab->lastindex))
		{
			// use this entry
			if (prtab->partable)
			{
				uint16_t idx = (index - prtab->firstindex);
				TParameterDef * pdef = (TParameterDef *)&(prtab->partable[idx]);
				return param_handle_pdef(udorq, pdef);
			}
			else // handled by a function
			{
				if (!prtab->method_ptr)
				{
					if (!prtab->obj_func_ptr) // error
					{
						return udo_response_error(udorq, UDOERR_APPLICATION);
					}

					// call the handler
					PParRangeFunc func = PParRangeFunc(prtab->obj_func_ptr);
					return (*func)(udorq, prtab);
				}
				else // method call
				{
					if (!prtab->obj_func_ptr)
					{
						// no object provided !
						return udo_response_error(udorq, UDOERR_APPLICATION);
					}

					PParRangeMethod method = PParRangeMethod(prtab->method_ptr);
					uint8_t * objptr = (uint8_t *)prtab->obj_func_ptr;
					TClass * obj = (TClass *)(objptr);
					return (obj->*method)(udorq, prtab);
				}
			}
		}
		++prtab;
	}

	return udo_response_error(udorq, UDOERR_INDEX);
}

TParameterDef * pdef_get(uint16_t aindex)
{
	const TParamRangeDef * prtab = &param_range_table[0];

	while (prtab->firstindex)
	{
		if (aindex < prtab->firstindex)
		{
			return nullptr;
		}

		if ((prtab->firstindex <= aindex) && (aindex <= prtab->lastindex))
		{
			// use this entry
			if (prtab->partable)
			{
				uint16_t idx = (aindex - prtab->firstindex);
				return (TParameterDef *)&(prtab->partable[idx]);
			}
			else // not scopeable if it is handled by a function
			{
				return nullptr;
			}
		}
		++prtab;
	}

	return nullptr;
}

int pdef_varsize(TParameterDef * pdef)
{
	uint8_t sizecode = (pdef->flags & PARF_SIZE_MASK);
	switch (sizecode)
	{
	case PARF_SIZE_32:  return 4;
	case PARF_SIZE_8:   return 1;
	case PARF_SIZE_16:  return 2;
	case PARF_SIZE_64:  return 8;
	}
	return 1;
}

bool pdef_empty(TParameterDef * pdef)
{
	if (!pdef->var_ptr && !pdef->obj_func_ptr && !pdef->method_ptr)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool param_handle_pdef(TUdoRequest * udorq, TParameterDef * pdef)
{
	uint16_t   f = pdef->flags;
  uint16_t   rw = (f & PARF_RW_MASK);

	if (rw == PARF_ROCONST)  // special case: constants
	{
		if (udorq->iswrite)
		{
			return udo_response_error(udorq, UDOERR_READ_ONLY);
		}

		return udo_ro_int(udorq, intptr_t(pdef->var_ptr), pdef_varsize(pdef));
	}

	if ((rw == PARF_READONLY) && udorq->iswrite)
	{
		return udo_response_error(udorq, UDOERR_READ_ONLY);
	}
	else if ((rw == PARF_WRITEONLY) && !udorq->iswrite)
	{
		return udo_response_error(udorq, UDOERR_WRITE_ONLY);
	}

	uint8_t *  varptr = (uint8_t *)pdef->var_ptr;
	//uint16_t   ptype = (f & PARF_TYPE_MASK);

	if (pdef->obj_func_ptr || pdef->method_ptr)  // Call the handler function if provided
	{
		if (!pdef->method_ptr)
		{
			if (!pdef->obj_func_ptr) // error
			{
				return udo_response_error(udorq, UDOERR_APPLICATION);
			}

			// call the handler
			PUdoParFunc func = PUdoParFunc(pdef->obj_func_ptr);
			return (*func)(udorq, pdef, varptr);
		}
		else // method call
		{
			if (!pdef->obj_func_ptr)
			{
				// no object provided !
				return udo_response_error(udorq, UDOERR_APPLICATION);
			}

			PUdoParMethod method = PUdoParMethod(pdef->method_ptr);
			uint8_t * objptr = (uint8_t *)pdef->obj_func_ptr;
			TClass * obj = (TClass *)(objptr);
			return (obj->*method)(udorq, pdef, varptr);
		}
	}

	return param_handle_pdef_var(udorq, pdef, varptr);
}

bool param_handle_pdef_var(TUdoRequest * udorq, TParameterDef * pdef, void * varptr)
{
	if (!varptr)
	{
		return udo_response_error(udorq, UDOERR_APPLICATION);
	}

	if (pdef->flags & PARF_PP)
	{
		varptr = *((void * *)varptr); // resolve double pointer
		if (!varptr)
		{
			return udo_response_error(udorq, UDOERR_APPLICATION);
		}
	}

	//int varsize = pdef_varsize(pdef);

	uint16_t sizecode = (pdef->flags & PARF_SIZE_MASK);
	switch (sizecode)
	{
	case PARF_SIZE_32:
		if (udorq->iswrite)
		{
			*(uint32_t *)varptr = *(uint32_t *)udorq->dataptr;
		}
		else
		{
			*(uint32_t *)udorq->dataptr = *(uint32_t *)varptr;
			udorq->anslen = 4;
		}
		return true;

	case PARF_SIZE_16:
		if (udorq->iswrite)
		{
			*(uint16_t *)varptr = *(uint16_t *)udorq->dataptr;
		}
		else
		{
			*(uint16_t *)udorq->dataptr = *(uint16_t *)varptr;
			udorq->anslen = 2;
		}
		return true;

	case PARF_SIZE_8:
		if (udorq->iswrite)
		{
			*(uint8_t *)varptr = *(uint8_t *)udorq->dataptr;
		}
		else
		{
			*(uint8_t *)udorq->dataptr = *(uint8_t *)varptr;
			udorq->anslen = 1;
		}
		return true;

	case PARF_SIZE_64:
		if (udorq->iswrite)
		{
			// to avoid unaligned errors do in two parts
			*(uint32_t *)varptr = *(uint32_t *)udorq->dataptr;
			*((uint32_t *)varptr + 1) = *((uint32_t *)udorq->dataptr + 1);
		}
		else
		{
			// to avoid unaligned errors do in two parts
			*(uint32_t *)udorq->dataptr = *(uint32_t *)varptr;
			*((uint32_t *)udorq->dataptr + 1) = *((uint32_t *)varptr + 1);
			udorq->anslen = 8;
		}
		return true;

	}

	// unknown size, can not be handled without function, wrong table definition !
	return udo_response_error(udorq, UDOERR_APPLICATION);
}

