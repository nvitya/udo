// sercomm.cpp

using namespace std;

#include <string>
#include "string.h"
#include "sercomm.h"

#define TRACECOMM 0

// Platform dependent functions

#ifdef WIN32

bool TSerComm::Open(string acomport)
{
	comport = acomport;

  string portname = string("\\\\.\\") + comport;

	comhandle = CreateFile( &portname[0],
									GENERIC_READ | GENERIC_WRITE,
									0,
									0,
									OPEN_EXISTING,
									0, //FILE_FLAG_OVERLAPPED,
									0);

	if (comhandle == INVALID_HANDLE_VALUE)
	{
		printf("Error opening \"%s\" port!\n", &comport[0]);
		return false;
	}

	//printf("Port \"%s\" opened.\n", &comport[0]);

	DCB dcb;
	memset(&dcb, 0, sizeof(dcb));

	GetCommState(comhandle, &dcb);

	dcb.BaudRate = 115200;
	//dcb.BaudRate = 921600;
	dcb.ByteSize = 8;  // oh windows..., it was 7 by default!
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fBinary = TRUE;
	dcb.fParity = FALSE;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;

	SetCommState(comhandle, &dcb);

	COMMTIMEOUTS timeouts;

	timeouts.ReadIntervalTimeout = MAXDWORD;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 0;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 0;

	SetCommTimeouts(comhandle, &timeouts);

  SetupComm(comhandle, 16384, 16384);

  // kill all characters in the RX bufer
  PurgeComm(comhandle, 0xF);

	return true;
}

void TSerComm::Close()
{
	CloseHandle(comhandle);
	comhandle = INVALID_HANDLE_VALUE;
}

int TSerComm::Read(void * dst, unsigned len)
{
	unsigned readcount = 0;
	ReadFile(comhandle, dst, len, (PDWORD)&readcount, (LPOVERLAPPED)0);
	if (readcount > 0)
	{
		return readcount;
	}

	int err = GetLastError();
	if (err)
	{
		return -err;
	}
	else
	{
		return -EAGAIN;
	}
}

int TSerComm::Write(void * src, unsigned len)
{
	unsigned count = 0;
	WriteFile(comhandle, src, len, (PDWORD)&count, (LPOVERLAPPED)0);
	if (count > 0)
	{
		return count;
	}

	int err = GetLastError();
	if (err)
	{
		return -err;
	}
	else
	{
		return -EAGAIN;
	}
}

void TSerComm::Flush()
{
  PurgeComm(comhandle, 0xF);
}

bool TSerComm::Opened()
{
	return (comhandle != INVALID_HANDLE_VALUE);
}

#else // linux

bool TSerComm::Open(string acomport) // full filename on linux
{
	comport = acomport;

	comfd = open(&comport[0], O_RDWR | O_NOCTTY | O_NONBLOCK);

	if (comfd < 0)
	{
		printf("Error opening comm port \"%s\"!\n", &comport[0]);
		return false;
	}

	//printf("Port \"%s\" opened.\n", &comport[0]);

	struct termios tty;	/* Create the structure                          */
	memset(&tty, 0, sizeof(tty));

  int brcode;
  switch (baudrate)
  {
    case    9600:  brcode = B9600;  break;
    case   19200:  brcode = B19200;  break;
    case   38400:  brcode = B38400;  break;
    case   57600:  brcode = B57600;  break;
    case  115200:  brcode = B115200;  break;
    case  230400:  brcode = B230400;  break;
    case  460800:  brcode = B460800;  break;
    case  500000:  brcode = B500000;  break;
    case  921600:  brcode = B921600;  break;
    case 1000000:  brcode = B1000000;  break;
    case 1152000:  brcode = B1152000;  break;
    case 1500000:  brcode = B1500000;  break;
    case 2000000:  brcode = B2000000;  break;
    case 2500000:  brcode = B2500000;  break;
    case 3000000:  brcode = B3000000;  break;
    case 3500000:  brcode = B3500000;  break;
    case 4000000:  brcode = B4000000;  break;

    default:
    	printf("SerComm unhandled baudrate: %i\n", baudrate);
    	return false;
  }

	/* Setting the Baud rate */
	cfsetispeed(&tty, brcode);
	cfsetospeed(&tty, brcode);

	tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
	tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
	tty.c_cflag |= CS8; // 8 bits per byte (most common)
	tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
	tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

	tty.c_lflag &= ~ICANON;
	tty.c_lflag &= ~ECHO; // Disable echo
	tty.c_lflag &= ~ECHOE; // Disable erasure
	tty.c_lflag &= ~ECHONL; // Disable new-line echo
	tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
	tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

	tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
	tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
	// tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
	// tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)

	/* Setting Time outs */
	tty.c_cc[VMIN] = 0; /* Read at least 10 characters */
	tty.c_cc[VTIME] = 10; /* Wait indefinetly   */


	if ( tcsetattr(comfd, TCSANOW, &tty) != 0) /* Set the attributes to the termios structure*/
	{
		printf("ERROR setting serial line attributes");
		return false;
	}

				/*------------------------------- Read data from serial port -----------------------------*/

	tcflush(comfd, TCIFLUSH);   /* Discards old data in the rx buffer            */

	return true;
}

void TSerComm::Close()
{
	if (comfd >= 0)
	{
		close(comfd);
		comfd = -1;
	}
}

int TSerComm::Read(void * dst, unsigned len)
{
	int r = read(comfd, dst, len);
	if (r < 0)
	{
		return -errno;
	}
	else
	{
		return r;
	}
}

int TSerComm::Write(void * src, unsigned len)
{
	int r = write(comfd, src, len);
	if (r < 0)
	{
		return -errno;
	}
	else
	{
		return r;
	}
}

void TSerComm::FlushInput()
{
	tcflush(comfd, TCIFLUSH);   // Discards old data in the rx buffer
}

void TSerComm::FlushOutput()
{
	tcflush(comfd, TCOFLUSH);   // Discards old data in the rx buffer
}

bool TSerComm::Opened()
{
	return (comfd >= 0);
}

#endif
