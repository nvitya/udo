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
 *  file:     parameterdef.h
 *  brief:    Parameter definitition for simple parameter tables
 *  created:  2023-05-13
 *  authors:  nvitya
*/

#ifndef UDO_PARAMETERDEF_H_
#define UDO_PARAMETERDEF_H_

#include "udo.h"
#include "tclass.h"

#define PARF_SIZE_32         0x00000000  // default
#define PARF_SIZE_8          0x00000001
#define PARF_SIZE_16         0x00000002
#define PARF_SIZE_64         0x00000003
// reserved value            0x00000004
// reserved value            0x00000005
// reserved value            0x00000006
#define PARF_SIZE_UNK        0x00000007  // for BLOB data
#define PARF_SIZE_MASK       0x00000007

// reserved bit              0x00000008

#define PARF_TYPE_INT        0x00000000  // (default) signed integer
#define PARF_TYPE_UINT       0x00000010  // unsigned integer
#define PARF_TYPE_FLOAT      0x00000020  // floating point
#define PARF_TYPE_STRING     0x00000030
// reserved value            0x00000040
// reserved value            0x00000050
// reserved value            0x00000060
#define PARF_TYPE_BLOB       0x00000070  // Any binary data
#define PARF_TYPE_MASK       0x00000070

#define PARF_PP              0x00000080  // pointer to pointer: the variable pointer points to a pointer which points to the real data

#define PARF_RW              0x00000000  // default
#define PARF_READONLY        0x00000100
#define PARF_WRITEONLY       0x00000200
#define PARF_ROCONST         0x00000300  // returns a fix value
#define PARF_RW_MASK         0x00000300

// reserved bit              0x00000400
// reserved bit              0x00000800
// reserved bit              0x00001000
// reserved bit              0x00002000
// reserved bit              0x00004000
// reserved bit              0x00008000

// reserved bits (16)        0xFFFF0000

//----------------------------------------

#define PARF_INT8            0x00000001
#define PARF_INT16           0x00000002
#define PARF_INT32           0x00000000
#define PARF_UINT8           0x00000011
#define PARF_UINT16          0x00000012
#define PARF_UINT32          0x00000010
#define PARF_FLOAT           0x00000020
#define PARF_STRING          (PARF_SIZE_UNK | PARF_TYPE_STRING)
#define PARF_BLOB            (PARF_SIZE_UNK | PARF_TYPE_BLOB)

#define PAR_INT8_CONST    (PARF_INT8   | PARF_ROCONST)
#define PAR_INT16_CONST   (PARF_INT16  | PARF_ROCONST)
#define PAR_INT32_CONST   (PARF_INT32  | PARF_ROCONST)
#define PAR_UINT8_CONST   (PARF_UINT8  | PARF_ROCONST)
#define PAR_UINT32_CONST  (PARF_UINT32 | PARF_ROCONST)
#define PAR_UINT16_CONST  (PARF_UINT16 | PARF_ROCONST)

#define PAR_INT8_RW       (PARF_INT8   | PARF_RW)
#define PAR_INT16_RW      (PARF_INT16  | PARF_RW)
#define PAR_INT32_RW      (PARF_INT32  | PARF_RW)
#define PAR_UINT8_RW      (PARF_UINT8  | PARF_RW)
#define PAR_UINT16_RW     (PARF_UINT16 | PARF_RW)
#define PAR_UINT32_RW     (PARF_UINT32 | PARF_RW)
#define PAR_FLOAT_RW      (PARF_FLOAT  | PARF_RW)

#define PAR_INT8_RO       (PARF_INT8   | PARF_READONLY)
#define PAR_INT16_RO      (PARF_INT16  | PARF_READONLY)
#define PAR_INT32_RO      (PARF_INT32  | PARF_READONLY)
#define PAR_UINT8_RO      (PARF_UINT8  | PARF_READONLY)
#define PAR_UINT16_RO     (PARF_UINT16 | PARF_READONLY)
#define PAR_UINT32_RO     (PARF_UINT32 | PARF_READONLY)
#define PAR_FLOAT_RO      (PARF_FLOAT  | PARF_READONLY)

#define PAR_INT8_WO       (PARF_INT8   | PARF_WRITEONLY)
#define PAR_INT16_WO      (PARF_INT16  | PARF_WRITEONLY)
#define PAR_INT32_WO      (PARF_INT32  | PARF_WRITEONLY)
#define PAR_UINT8_WO      (PARF_UINT8  | PARF_WRITEONLY)
#define PAR_UINT16_WO     (PARF_UINT16 | PARF_WRITEONLY)
#define PAR_UINT32_WO     (PARF_UINT32 | PARF_WRITEONLY)
#define PAR_FLOAT_WO      (PARF_FLOAT  | PARF_WRITEONLY)


struct TParameterDef;

typedef bool (TClass:: * PUdoParMethod)(TUdoRequest * udorq, TParameterDef * adef, void * varptr);
typedef bool            (* PUdoParFunc)(TUdoRequest * udorq, TParameterDef * adef, void * varptr);

struct TParameterDef
{
	uint32_t        flags;
  void *          var_ptr; // the constants will be stored here too
	void *          obj_func_ptr;
	PUdoParMethod   method_ptr;
//
}; // 20 bytes

#endif /* UDO_PARAMETERDEF_H_ */
