/*
 *  file:     mscounter.cpp
 *  brief:    simple 32-bit millisecond counter
 *  created:  2023-05-01
 *  authors:  nvitya
 *  license:  public domain
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mscounter.h"

#ifdef WINDOWS

  #include "windows.h"

#else
  // Linux
  #include "time.h"
#endif

uint32_t mscounter(void)
{
  #ifdef WINDOWS

	  return GetTickCount();

	#else // linux

    // high resolution timer on linux
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ns = (uint64_t)ts.tv_sec * (uint64_t)1000000000 + (uint64_t)ts.tv_nsec;
    return ns / 1000000;

  #endif
}
