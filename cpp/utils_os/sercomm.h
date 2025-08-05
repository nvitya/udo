// sercomm.h

#ifndef SERCOMM_H_
#define SERCOMM_H_

using namespace std;

#include "stdio.h"
#include "stdint.h"
#include <string>

#ifdef WIN32
  #include "Windows.h"
#else
  #include <fcntl.h>   /* File Control Definitions           */
  #include <termios.h> /* POSIX Terminal Control Definitions */
  #include <unistd.h>  /* UNIX Standard Definitions 	   */
  #include <errno.h>   /* ERROR Number Definitions           */
#endif

#define COMM_BUFFER_SIZE 2048

class TSerComm
{
public:
#ifdef WIN32
	HANDLE comhandle = INVALID_HANDLE_VALUE;
#else
	int    comfd = -1;
#endif

	int     baudrate = 115200;

	string  comport;

	uint8_t txbuf[COMM_BUFFER_SIZE];
	uint8_t rxbuf[COMM_BUFFER_SIZE];

public: // platform dependent functions:
	bool  Open(string acomport);
	void  Close();
	int   Read(void * dst, unsigned len);
	int   Write(void * src, unsigned len);
	void  FlushInput();
	void  FlushOutput();
	bool  Opened();

};


#endif /* SERCOMM_H_ */
