/*
 *  file:     traces.cpp
 *  brief:    TRACES() PC implementation
 *  created:  2023-05-13
 *  authors:  nvitya
 *  license:  public domain
*/

#include <stdio.h>
#include <stdarg.h>
#include "time.h"

#define MAXDEBUGMSG 256  // maximal length of the formatted debug message

char tracefmtbuf[MAXDEBUGMSG];

char tracebuf[1024];
unsigned trace_read_idx = 0;
unsigned trace_write_idx = 0;

void send_trace_char(char ch)
{
	if (ch != '\r')
	{
		putchar(ch);
	}
}

void buffer_trace_char(char ch)
{
	tracebuf[trace_write_idx] = ch;
	trace_write_idx = (trace_write_idx + 1) & 0x3FF;
}

void trace_send_buffer(void)
{
	if (trace_read_idx != trace_write_idx)
	{
		putchar(tracebuf[trace_read_idx]);
		trace_read_idx = (trace_read_idx + 1) & 0x3FF;
	}
}

void send_trace_message_va(const char * fmt, va_list * arglist)
{
  char * pch = &tracefmtbuf[0];

  *pch = 0;

  vsnprintf(pch, MAXDEBUGMSG, fmt, *arglist);

  while (*pch != 0)
  {
  	send_trace_char(*pch);
    ++pch;
  }
}

void send_trace_message(const char * fmt, ...)
{
  va_list arglist;
  va_start(arglist, fmt);

  send_trace_message_va(fmt, &arglist);

  va_end(arglist);
}

void send_trace_message_ts(const char * fmt, ...)
{
	// format timestamp
	time_t rawtime;
	time(&rawtime);
	struct tm * tminfo = localtime(&rawtime);
  char * pch = &tracefmtbuf[0];
  *pch = 0;
	strftime(pch, sizeof(tracefmtbuf), "%y-%m-%d %H:%M:%S  ", tminfo);

  while (*pch != 0)
  {
  	send_trace_char(*pch);
    ++pch;
  }

  va_list arglist;
  va_start(arglist, fmt);

  send_trace_message_va(fmt, &arglist);

  va_end(arglist);
}

void send_trace_message_buffered(const char * fmt, ...)
{
  va_list arglist;
  va_start(arglist, fmt);
  char * pch;

  pch = &tracefmtbuf[0];

  *pch = 0;

  vsnprintf(pch, MAXDEBUGMSG, fmt, arglist);

  while (*pch != 0)
  {
  	buffer_trace_char(*pch);
    ++pch;
  }

  trace_send_buffer();

  va_end(arglist);
}


//--- End Of file --------------------------------------------------------------
