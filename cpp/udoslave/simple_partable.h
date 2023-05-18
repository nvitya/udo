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
 *  file:     simple_partable.h
 *  brief:    Simple parameter table definitions
 *  created:  2023-05-13
 *  authors:  nvitya
*/

#ifndef SIMPLE_PARTABLE_H_
#define SIMPLE_PARTABLE_H_

#include "parameterdef.h"
#include "tclass.h"

struct TParamRangeDef;

typedef bool (TClass:: * PParRangeMethod)(TUdoRequest * udorq, TParamRangeDef * prdef);
typedef bool            (* PParRangeFunc)(TUdoRequest * udorq, TParamRangeDef * prdef);

struct TParamRangeDef
{
	uint16_t               firstindex;
	uint16_t               lastindex;
	const TParameterDef *  partable;
	void *                 obj_func_ptr;
	PParRangeMethod        method_ptr;
//
};

extern const TParamRangeDef  param_range_table[];  // this must be provided

TParameterDef * pdef_get(uint16_t aindex);

int pdef_varsize(TParameterDef * pdef);

bool pdef_empty(TParameterDef * pdef);

bool param_handle_pdef(TUdoRequest * udorq, TParameterDef * pdef);
bool param_handle_pdef_var(TUdoRequest * udorq, TParameterDef * pdef, void * varptr);

bool param_read_write(TUdoRequest * udorq);

#endif /* SIMPLE_PARTABLE_H_ */
