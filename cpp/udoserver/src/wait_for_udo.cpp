/*
 * wait_for_udo.cpp
 *
 *  Created on: Aug 25, 2024
 *      Author: vitya
 */

#include "wait_for_udo.h"

#ifdef WINDOWS

#include "windows.h"

fd_set udoip_poll;

void prepare_udoip_wait(int afd)
{
  FD_ZERO(&udoip_poll);
  FD_SET(afd, &udoip_poll);
  //udoip_poll.fd = afd;
  //udoip_poll.events = POLLIN;
  //udoip_poll.revents = 0;
}

bool wait_for_udoip_ms(int atimeout_ms)
{
  TIMEVAL tv;
  tv.tv_sec = 0;
  tv.tv_usec = atimeout_ms * 1000;
  int r = select(0, &udoip_poll, nullptr, nullptr, &tv);
  if (r > 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}


#else

#include "poll.h"
struct pollfd  udoip_poll;

void prepare_udoip_wait(int afd)
{
  udoip_poll.fd = afd;
  udoip_poll.events = POLLIN;
  udoip_poll.revents = 0;
}

bool wait_for_udoip_ms(int atimeout_ms)
{
  int r = poll(&udoip_poll, 1, atimeout_ms);
  if (r > 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}

#endif
