/*
 *  file:     traces.h
 *  brief:    TRACES() PC implementation
 *  created:  2023-05-13
 *  authors:  nvitya
 *  license:  public domain
*/

#ifndef __Traces__h
#define __Traces__h

#include <stdarg.h>

extern void send_trace_message_ts(const char * fmt, ...);
extern void send_trace_message(const char * fmt, ...);
extern void send_trace_message_va(const char * fmt, va_list * arglist);

extern void send_trace_message_buffered(const char * fmt, ...);
extern void trace_send_buffer(void);
extern void send_trace_char(char ch);
extern void buffer_trace_char(char ch);

#define TRACES

#ifdef TRACES
  // add trace calls
  #define TRACETS(...) 		     send_trace_message_ts( __VA_ARGS__ )
  #define TRACE(...)  		     send_trace_message( __VA_ARGS__ )
  #define TRACE_VA(fmt, varg)  send_trace_message_va(fmt, varg)
  #define TRACEBUF(...)  	     send_trace_message_buffered( __VA_ARGS__ )
#else
  // ignore traces
  #define TRACE(...)
  #define TRACETS(...)
	#define TRACEBUF(...)
#endif

#define TRACE_FLUSH(...)

#ifdef LTRACES
 #define LTRACE(...)    TRACE( __VA_ARGS__ )
 #define LTRACETS(...)  TRACETS( __VA_ARGS__ )
#else
 #define LTRACE(...)
 #define LTRACETS(...)
#endif

#undef LTRACES

#endif //!defined(Traces__h)

//--- End Of file --------------------------------------------------------------
