(*-----------------------------------------------------------------------------
  This file is a part of the PASUTILS project: https://github.com/nvitya/pasutils
  Copyright (c) 2022 Viktor Nagy, nvitya

  This software is provided 'as-is', without any express or implied warranty.
  In no event will the authors be held liable for any damages arising from
  the use of this software. Permission is granted to anyone to use this
  software for any purpose, including commercial applications, and to alter
  it and redistribute it freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software in
     a product, an acknowledgment in the product documentation would be
     appreciated but is not required.

  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.

  3. This notice may not be removed or altered from any source distribution.
  --------------------------------------------------------------------------- */
   file:     serial_comm.pas
   brief:    simple multi-platform serial communication class
   date:     2022-03-30
   authors:  nvitya
*)

unit serial_comm;

{$ifdef FPC}
 {$mode delphi}{$H+}
{$endif}

interface

uses
  Classes, SysUtils,
  {$ifdef WINDOWS}
  windows
  {$else}
  BaseUnix, termio, unix
  {$endif}
  ;

const
  COMM_BUFFER_SIZE = 4096;

type

  { TSerialComm }

  TSerialComm = class
  public
    {$ifdef WINDOWS}
      comhandle : HANDLE;
    {$else}
      comfd   : THandle;
    {$endif}

    comport   : string;
    baudrate  : integer;
    databits  : byte;
    stopbits  : byte;
    parity    : boolean;
    oddparity : boolean;

    constructor Create;
    destructor Destroy; override;

    function Open(acomport : string) : boolean;
    procedure Close;

    function Read(out dst; dstlen : integer) : integer;
    function Write(const src; len : integer) : integer;
    procedure FlushInput;
    procedure FlushOutput;
    function Opened : boolean;
  end;

implementation

procedure OutVarInit(out v);  // dummy proc for avoid false FPC hints
begin
  //
end;

{ TSerialComm }

constructor TSerialComm.Create;
begin
  {$ifdef WINDOWS}
    comhandle := INVALID_HANDLE_VALUE;
  {$else}
    comfd := -1;
  {$endif}
  baudrate := 115200;
  databits := 8;
  stopbits := 1;
  parity    := false;
  oddparity := false;
end;

destructor TSerialComm.Destroy;
begin
  Close;
  inherited Destroy;
end;

{$ifdef WINDOWS}

function TSerialComm.Open(acomport : string) : boolean;
var
  portname : string;
  dcb : TDCB;
  timeouts : TCOMMTIMEOUTS;
begin
  result := false;
  comport := acomport;
  portname := '\\.\' + comport;
  comhandle := CreateFile( PChar(portname),
		  GENERIC_READ or GENERIC_WRITE,
		  0,
		  nil,
		  OPEN_EXISTING,
		  0, //FILE_FLAG_OVERLAPPED,
		  0);

  if comhandle = INVALID_HANDLE_VALUE then EXIT(false);

  //printf("Port \"%s\" opened.\n", &comport[0]);

  dcb.BaudRate := 0; // to avoid FPC hint
  FillChar(dcb, sizeof(dcb), 0);

  GetCommState(comhandle, dcb);

  dcb.BaudRate := baudrate;
  //dcb.BaudRate = 921600;
  dcb.ByteSize := 8;  // oh windows..., it was 7 by default!
  dcb.Parity := NOPARITY;
  dcb.StopBits := ONESTOPBIT;
  dcb.flags := bm_DCB_fBinary;
  // available flags:
  //   bm_DCB_Parity
  //   bm_DCB_fDtrControl
  //   bm_DCB_fRtsControl

  SetCommState(comhandle, dcb);

  timeouts.ReadIntervalTimeout := MAXDWORD;
  timeouts.ReadTotalTimeoutMultiplier := 0;
  timeouts.ReadTotalTimeoutConstant := 0;
  timeouts.WriteTotalTimeoutMultiplier := 0;
  timeouts.WriteTotalTimeoutConstant := 0;

  SetCommTimeouts(comhandle, timeouts);

  SetupComm(comhandle, COMM_BUFFER_SIZE, COMM_BUFFER_SIZE);

  // kill all characters in the RX bufer
  PurgeComm(comhandle, $0F);

  result := true;
end;

procedure TSerialComm.Close;
begin
  if Opened then
  begin
    CloseHandle(comhandle);
    comhandle := INVALID_HANDLE_VALUE;
  end;
end;

function TSerialComm.Read(out dst; dstlen : integer) : integer;
var
  readcount : DWORD;
begin
  readcount := 0;
  OutVarInit(dst);
  OutVarInit(readcount);
  if ReadFile(comhandle, dst, dstlen, readcount, nil) then
  begin
    result := integer(readcount);
  end
  else
  begin
    result := -GetLastError();
  end;
end;

function TSerialComm.Write(const src; len : integer) : integer;
var
  count : DWORD;
begin
  count := 0;
  if WriteFile(comhandle, src, len, count, nil) then
  begin
    result := integer(count);
  end
  else
  begin
    result := -GetLastError();
  end;
end;

procedure TSerialComm.FlushInput;
begin
  if not Opened then EXIT;
  PurgeComm(comhandle, PURGE_RXCLEAR);
end;

procedure TSerialComm.FlushOutput;
begin
  if not Opened then EXIT;
  PurgeComm(comhandle, PURGE_TXCLEAR); // Discards old data in the tx buffer
end;

function TSerialComm.Opened: boolean;
begin
  result := (comhandle <> INVALID_HANDLE_VALUE);
end;

{$else}

function TSerialComm.Open(acomport : string) : boolean;
var
  tty : termios;
  brcode : integer;
begin
  result := false;

  comport := acomport;

	comfd := FileOpen(comport, O_RDWR or O_NOCTTY or O_NONBLOCK);
	if comfd < 0 then
  begin
		//writeln(format('Error opening comm port "%s"!', [comport]));
		exit;
  end;

  //writeln(format('Port %s" opened.', [comport]));

  tty.c_cflag := 0; // to avoid FPC not initialized warning
	FillByte(tty, sizeof(tty), 0);

  case baudrate
  of
       9600:  brcode := B9600;
      19200:  brcode := B19200;
      38400:  brcode := B38400;
      57600:  brcode := B57600;
     115200:  brcode := B115200;
     230400:  brcode := B230400;
     460800:  brcode := B460800;
     500000:  brcode := B500000;
     921600:  brcode := B921600;
    1000000:  brcode := B1000000;
    1152000:  brcode := B1152000;
    1500000:  brcode := B1500000;
    2000000:  brcode := B2000000;
    2500000:  brcode := B2500000;
    3000000:  brcode := B3000000;
    3500000:  brcode := B3500000;
    4000000:  brcode := B4000000;
  else
    Close;
    //raise Exception.CreateFmt('SerComm unhandled baudrate: %d', [baudrate]);
    exit;
  end; // case

	// Setting the Baud rate
	cfsetispeed(tty, brcode);
	cfsetospeed(tty, brcode);

  case databits of
    5:   tty.c_cflag := tty.c_cflag or CS5;
    6:   tty.c_cflag := tty.c_cflag or CS6;
    7:   tty.c_cflag := tty.c_cflag or CS7;
    else tty.c_cflag := tty.c_cflag or CS8;
  end;

  if parity then
  begin
    tty.c_cflag := tty.c_cflag or PARENB;
    if oddparity then tty.c_cflag := tty.c_cflag or PARODD;
  end;

  if stopbits = 2 then tty.c_cflag := tty.c_cflag or CSTOPB;

(* no flow control support yet
  if rts_cts_flow_control then
  begin
  	tty.c_cflag := tty.c_cflag or CRTSCTS;
  end;
*)

	tty.c_cflag := tty.c_cflag or (CREAD or CLOCAL); // Turn on READ & ignore ctrl lines (CLOCAL = 1)

(* no other special flags:

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
*)

	// Setting Time-outs
	tty.c_cc[VMIN] := 0; // return 1 char immediately
	tty.c_cc[VTIME] := 10; // wait for 10 ms

  // Set the attributes to the termios structure
	if tcsetattr(comfd, TCSANOW, tty) <> 0 then
	begin
    Close;
		//raise Exception.Create('ERROR setting serial line attributes!');
		exit;
  end;

	tcflush(comfd, TCIFLUSH);   // Discards old data in the rx buffer

  result := true;
end;

procedure TSerialComm.Close;
begin
	if comfd >= 0 then
  begin
		FileClose(comfd);
		comfd := -1;
	end;
end;

function TSerialComm.Read(out dst; dstlen : integer) : integer;
begin
  result := FileRead(comfd, dst, dstlen);
end;

function TSerialComm.Write(const src; len : integer) : integer;
begin
  result := FileWrite(comfd, src, len);
end;

procedure TSerialComm.FlushInput;
begin
	tcflush(comfd, TCIFLUSH);   // Discards old data in the rx buffer
end;

procedure TSerialComm.FlushOutput;
begin
	tcflush(comfd, TCOFLUSH);   // Discards old data in the tx buffer
end;

function TSerialComm.Opened: boolean;
begin
  result := (comfd >= 0);
end;

{$endif}

end.

