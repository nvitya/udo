/*
 *  file:     nstime.h
 *  brief:    simple 64-bit nanosecond timer for Linux and Windows
 *  created:  2023-05-13
 *  authors:  nvitya
 *  license:  public domain
*/

#ifndef __NSTIME_H_
#define __NSTIME_H_

#ifdef __cplusplus
extern "C"
{
#endif

typedef long long  nstime_t;

extern nstime_t  nstime(void);
extern void      waitns(nstime_t wns);
extern void      ns_sleep_until(nstime_t wakeuptime);

#ifdef __cplusplus
}
#endif
#endif // __NSTIME_H_
