/*
 *  file:     nstime.cpp
 *  brief:    simple 64-bit nanosecond timer for Linux and Windows
 *  created:  2023-05-13
 *  authors:  nvitya
 *  license:  public domain
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "nstime.h"

#ifdef WIN32
  #include "windows.h"
  double qpcmsscale;

#else
  // Linux
  #include "time.h"

	#ifndef LINUX
		#define LINUX
	#endif

#endif

nstime_t nstime(void)
{
  #ifdef WIN32

    long long qpc;
    QueryPerformanceCounter((LARGE_INTEGER *)&qpc);
    return qpcmsscale * (double)qpc;

	#else // linux

    // high resolution timer on linux
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (nstime_t)ts.tv_sec * (nstime_t)1000000000 + (nstime_t)ts.tv_nsec;

  #endif
}

void ns_sleep_until(nstime_t wakeuptime)
{
  #ifndef LINUX
    // no such possibility on windows, we do busy waiting.
    while (nstime() < wakeuptime)
    {
      // wait...
    }
  #else
    // on linux we use clock_nanosleep

		struct timespec ts;
		struct timespec tr;
		ts.tv_sec = long(wakeuptime / 1000000000);
		ts.tv_nsec = long(wakeuptime % 1000000000);

		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, &tr);

  #endif
}

void waitns(nstime_t wns)
{
	nstime_t st = nstime();
	while (nstime() - st < wns)
	{
		// wait;
	}
}
