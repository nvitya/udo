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
 *  file:     udo.h
 *  brief:    UDO (Universal Device Objects) common definitions
 *  created:  2023-05-01
 *  authors:  nvitya
*/

#ifndef __UDO_H__
#define __UDO_H__

#include "stdint.h"

#ifndef UDO_MAX_DATALEN
  #define UDO_MAX_DATALEN    1024
#endif

#define UDOERR_CONNECTION       0x1001  // not connected, send / receive error
#define UDOERR_CRC              0x1002
#define UDOERR_TIMEOUT          0x1003
#define UDOERR_DATA_TOO_BIG     0x1004

#define UDOERR_INDEX            0x2000  // index / object not existing
#define UDOERR_WRONG_OFFSET     0x2001  // like the offset must be divisible by the 4
#define UDOERR_WRONG_ACCESS     0x2002
#define UDOERR_READ_ONLY        0x2010
#define UDOERR_WRITE_ONLY       0x2011
#define UDOERR_WRITE_BOUNDS     0x2012  // write is out ouf bounds
#define UDOERR_WRITE_VALUE      0x2020  // invalid value
#define UDOERR_RUN_MODE         0x2030  // config mode required
#define UDOERR_UNITSEL          0x2040  // the referenced unit is not existing
#define UDOERR_BUSY             0x2050

#define UDOERR_NOT_IMPLEMENTED  0x9001
#define UDOERR_INTERNAL         0x9002  // internal implementation error
#define UDOERR_APPLICATION      0x9003  // application (interfaceing) error

typedef struct TUdoRequest
{
  uint8_t      iswrite;      // 0 = read, 1 = write
  uint8_t      metalen;      // length of the metadata
  uint16_t     index;        // object index
  uint32_t     offset;
  uint32_t     metadata;

  uint16_t     rqlen;        // requested read/write length
  uint16_t     anslen;       // response data length, default = 0
  uint16_t     maxanslen;    // maximal read buffer length
  uint16_t     result;       // 0 if no error

  uint8_t *    dataptr;
//
} TUdoRequest;

uint8_t udo_calc_crc(uint8_t acrc, uint8_t adata);  // used for serial communication

#endif
