(*-----------------------------------------------------------------------------
  This file is a part of the NANONET project: https://github.com/nvitya/nanonet
  Copyright (c) 2025 Viktor Nagy, nvitya

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
  ---------------------------------------------------------------------------
   file:     nano_sockets.pas
   brief:    Basic client and server socket handling
   date:     2025-08-08
   authors:  nvitya
*)

unit nano_sockets;

{$mode ObjFPC}{$H+}

interface

uses
  Classes, SysUtils, Sockets, fgl
  {$ifdef WINDOWS} , WinSock {$else} , BaseUnix, Linux {$endif}
  ;

type

  ENanoNet = class(Exception);

  TNbSocketType = (stTcp4, stUdp4);

  TSocketWatcher = class;

  //TNanoSocket = class;

  TInEventHandler  = procedure(aobj : TObject) of object;
  TOutEventHandler = procedure(aobj : TObject) of object;

  { TNanoSocket }

  TNanoSocket = class
  protected
    fsocket : integer;

  public
    stype : TNbSocketType;
    //host : string;
    //port : word;

    sendbuf_size : integer;
    recvbuf_size : integer;

    local_addr  : TSockAddr;
    remote_addr : TSockAddr;

    ConnectTimeout : integer;    // [ms]
    ResponseTimeout : integer;   // [ms]

    obj_link : TObject;
    in_event_handler : TInEventHandler;
    out_event_handler : TOutEventHandler;

    constructor Create(atype : TNbSocketType);
    destructor Destroy; override;
    procedure Disconnect;
    function Connected : boolean;

    procedure InitListener(aport : uint16);
    function AcceptConnection(asvrsocket : THandle) : boolean;

    function Send(const srcdata; srclen : cardinal) : integer;
    function Recv(out dstdata; dstmaxlen : cardinal) : integer;

    function Connect(aaddr : ansistring; aport : uint16) : boolean;

    procedure SetNonBlocking;
    procedure SetTcpNoDelay(anodelay : boolean);
    procedure SetSockBufSizes;

    procedure SetSockParams;


  public
    property Socket : integer read FSocket;
  end;

  { TSocketWatcher }

  {$ifdef WINDOWS}

    // use SELECT on windows

  {$else}  // linux then

    // use EPOLL on linux

    TEpollEvent = epoll_event;
    TEpollData  = epoll_data;

  {$endif}


  TSocketWatcher = class  // for timed wait for read/write events
  protected
    {$ifdef WINDOWS}

    {$endif}

    {$ifdef LINUX}
      FFdEpoll : THandle;
      FREvents : array of TEpollEvent;
    {$endif}

  public
    maxfds    : integer;
    maxevents : integer;

    constructor Create(amaxfds : integer);
    destructor Destroy; override;

    procedure AddSocket(asock : TNanoSocket; aobj_link : TObject; ainev : TInEventHandler; aoutev : TOutEventHandler);
    procedure ModifySocket(asock : TNanoSocket);
    procedure RemoveSocket(asock : TNanoSocket);
    procedure SetOutHandler(asock : TNanoSocket; aoutev : TOutEventHandler);
    procedure SetInHandler(asock : TNanoSocket; ainev : TOutEventHandler);

    function WaitForEvents(atimeout_ms : integer) : boolean;
  end;

  //-----------------------------------------------------------------
  // Nano-Server
  //-----------------------------------------------------------------

  TNanoServer = class;

  { TSConnection }

  TSConnection = class  // Servers connection to client
  public
    sock : TNanoSocket;
    server : TNanoServer;

    constructor Create(aserver : TNanoServer); virtual;
    destructor Destroy; override;

    procedure Close; // schedules the connection object for safe removal

    function AcceptConnection(svrsock : TNanoSocket) : boolean; virtual;

    procedure HandleInput(aobj : TObject); virtual;
    procedure HandleOutput(aobj : TObject); virtual;

    procedure SetOutHandler(aoutev : TOutEventHandler);
  end;

  TSConnectionClass = class of TSConnection;

  TSConnList = specialize TFPGObjectList<TSConnection>;  // automatically frees the objects

  { TNanoServer }

  TNanoServer = class
  public
    listen_port  : uint16;
    sock_listen  : TNanoSocket;

    connections  : TSConnList;
    closed_conns : TSConnList;
    swatcher     : TSocketWatcher;

    sconn_class : TSConnectionClass;

    constructor Create(asconn_class : TSConnectionClass; alisten_port : uint16); virtual;
    destructor Destroy; override;

    procedure InitListener;

    function WaitForEvents(timeout_ms : integer) : boolean;

    procedure HandleListener(aobj : TObject);

    function CreateSConnection : TSConnection;
    procedure CloseConnection(aconn : TSConnection);
  end;

  //-----------------------------------------------------------------
  // Multi-Client Connections
  //-----------------------------------------------------------------

  TMultiClientMgr = class;

  { TCConnection }

  TCConnection = class  // Client connection to server
  public
    server_port : uint16;
    server_addr : ansistring;

    sock    : TNanoSocket;
    manager : TMultiClientMgr;

    constructor Create(amanager : TMultiClientMgr); virtual;
    destructor Destroy; override;

    procedure Connect; virtual;

    procedure HandleOutputForConnect(aobj : TObject); virtual;

    procedure HandleInput(aobj : TObject); virtual;
    procedure HandleOutput(aobj : TObject); virtual;

    procedure HandleConnectError(aerror : integer); virtual;

    procedure SetOutHandler(aoutev : TOutEventHandler);
  end;

  TCConnList = specialize TFPGObjectList<TCConnection>;  // automatically frees the objects

  { TMultiClientMgr }

  TMultiClientMgr = class
  public
    connections  : TCConnList;
    swatcher     : TSocketWatcher;

    constructor Create; virtual;
    destructor Destroy; override;

    procedure AddConnection(aconn : TCConnection);
    procedure CloseConnection(aconn : TCConnection);

    function WaitForEvents(timeout_ms : integer) : boolean;
  end;

  //-----------------------------------------------------------------
  // UDP / Datagram Handling
  //-----------------------------------------------------------------

const
  nano_datagram_max_length = 1472;  // maximal UDP payload size

type
  TNanoUdpServer = class;

  { TNanoDatagram }

  TNanoDatagram = class
  public
    rawdata         : array[0..nano_datagram_max_length] of byte;
    rawdata_len     : integer;

    remote_addr     : TSockAddr;
    remote_addr_len : TSocklen;

    server          : TNanoUdpServer;
    keep_in_memory  : boolean;          // false: will be released after processing

    constructor Create(aserver : TNanoUdpServer); virtual;
    destructor Destroy; override;

    function RecvRawData() : integer;   // remote_addr will be filled
    function SendRawData() : integer;   // uses rawdata, rawdata_len, remote_addr

    procedure ProcessInData(); virtual;  // should be overridden

    procedure SetRemoteAddr(aaddr : ansistring; aport : uint16);
    function GetRemoteAddrStr() : ansistring;
  end;

  TNanoDatagramClass = class of TNanoDatagram;

  { TNanoUdpServer }

  TNanoUdpServer = class
  public
    listen_port  : uint16;
    sock_listen  : TNanoSocket;
    swatcher     : TSocketWatcher;
    dg_class     : TNanoDatagramClass;

    constructor Create(adg_class : TNanoDatagramClass; alisten_port : uint16); virtual;
    destructor Destroy; override;

    procedure InitListener;

    function WaitForEvents(timeout_ms : integer) : boolean;

    procedure HandleListener(aobj : TObject);
    function CreateDatagram : TNanoDatagram;
  end;


implementation


{$ifdef LINUX} // add some missing definitions

const
  EPOLL_MAX_EVENTS = 32;

  EPOLL_CTL_ADD = 1;
  EPOLL_CTL_DEL = 2;
  EPOLL_CTL_MOD = 3;

  EPOLLIN  = $01; { The associated file is available for read(2) operations. }
  EPOLLPRI = $02; { There is urgent data available for read(2) operations. }
  EPOLLOUT = $04; { The associated file is available for write(2) operations. }
  EPOLLERR = $08; { Error condition happened on the associated file descriptor. }
  EPOLLHUP = $10; { Hang up happened on the associated file descriptor. }
  EPOLLONESHOT = 1 shl 30;
  EPOLLET  = 1 shl 31; { Sets  the  Edge  Triggered  behaviour  for  the  associated file descriptor. }

{$endif}


{ TNanoSocket }

constructor TNanoSocket.Create(atype : TNbSocketType);
begin
  stype := atype;
  fsocket := -1;

  ConnectTimeout := 1000;
  ResponseTimeout := 1000;

  recvbuf_size := 16384;
  sendbuf_size := 16384;

  obj_link := nil;
  in_event_handler := nil;
  out_event_handler := nil;
end;

function TNanoSocket.Connect(aaddr : ansistring; aport : uint16) : boolean;
var
  sarr : array of string;
  r : integer;
begin
  result := false;

  Disconnect;

  // parse the IP address and port
  sarr := aaddr.Split('.');
  if length(sarr) <> 4 then raise Exception.Create('Invalid Host IP address: '+aaddr);

	remote_addr.sin_family := AF_INET;
  remote_addr.sin_addr.s_addr := htonl(StrToHostAddr(aaddr).s_addr);
  remote_addr.sin_port := htons(aport);

  FSocket := fpSocket(AF_INET, SOCK_STREAM, 0);
  if FSocket < 0 then
  begin
    FSocket := -1;
    exit;
  end;

  SetSockParams;

  r := fpConnect(FSocket, @remote_addr, sizeof(remote_addr));
  if r < 0 then
  begin
    if (socketerror <> EsockEWOULDBLOCK) and (socketerror <> ESysEINPROGRESS) then
    begin
      Disconnect;
      exit;
    end;
  end;

  result := true;
end;

procedure TNanoSocket.SetNonBlocking;
var
  i : integer;
begin
  {$ifdef WINDOWS}
    i := 1;
    ioctlsocket(FSocket, longint(FIONBIO), i);  // set to non-blocking
  {$else}
    i := FpFcntl(FSocket , F_GETFL);
    i := i or O_NONBLOCK;
    FpFcntl(FSocket, F_SETFL, i);
  {$endif}
end;

procedure TNanoSocket.SetTcpNoDelay(anodelay : boolean);
var
  i : integer;
begin
  // set no-delay for fast reacting possibility
  if anodelay then i := 1
              else i := 0;

  fpsetsockopt(FSocket, IPPROTO_TCP, TCP_NODELAY, PAnsiChar(@i), sizeof(i));
end;

procedure TNanoSocket.SetSockBufSizes;
begin
  // setting this can make some communication very slow, like big file transfers
{
  fpsetsockopt(FSocket, SOL_SOCKET, SO_SNDBUF, PAnsiChar(@sendbuf_size), sizeof(sendbuf_size));
  fpsetsockopt(FSocket, SOL_SOCKET, SO_RCVBUF, PAnsiChar(@recvbuf_size), sizeof(recvbuf_size));
}
end;

procedure TNanoSocket.SetSockParams;
begin
  SetNonBlocking;
  if stype <> stUdp4 then SetTcpNoDelay(true);
  SetSockBufSizes;
end;

function TNanoSocket.Connected : boolean;
begin
  result := (FSocket >= 0);
end;

destructor TNanoSocket.Destroy;
begin
  Disconnect;
  inherited Destroy;
end;

procedure TNanoSocket.InitListener(aport : uint16);
var
  i, arg : integer;
begin
  if stype = stUdp4 then FSocket := fpSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
                    else FSocket := fpSocket(AF_INET, SOCK_STREAM, 0);
  if FSocket <= 0
  then
      raise ENanoNet.Create('InitListener: Error creating the listener socket');

  SetSockParams;

  // re-use the listener port
  i := SO_REUSEADDR;
  {$ifdef WIN32} // I expect 64 has it oddly, so screw them for now
  if (Win32Platform = 2) and (Win32MajorVersion >= 5) then
    i := Integer(not i);
  {$endif}
  arg := 1;
  fpsetsockopt(FSocket, SOL_SOCKET, i, @Arg, Sizeof(Arg)); // ignore errors for now

	local_addr.sin_family := AF_INET;
  local_addr.sin_addr.s_addr := $00000000;
  local_addr.sin_port := htons(aport);

  if fpBind(FSocket, @local_addr, sizeof(local_addr)) <> 0
  then
      raise ENanoNet.Create('InitListener: Error binding to port '+IntToStr(aport));

  if stype <> stUdp4 then
  begin
    if fpListen(FSocket, 64) <> 0
    then
        raise ENanoNet.Create('InitListener: Listen error');
  end;
end;

function TNanoSocket.AcceptConnection(asvrsocket : THandle) : boolean;
var
  sl : TSockLen;
begin
  sl := sizeof(remote_addr);
  fSocket := fpAccept(asvrsocket, @remote_addr, @sl);

  SetSockParams;

  result := (fsocket >= 0);
end;

procedure TNanoSocket.Disconnect;
begin
  if Connected then
  begin
    closesocket(FSocket);
    FSocket := -1;
  end;
end;

function TNanoSocket.Recv(out dstdata; dstmaxlen : cardinal) : integer;
begin
  result := fprecv(FSocket, @dstdata, dstmaxlen, 0);
  if result < 0 then result := -socketerror;
end;

function TNanoSocket.Send(const srcdata; srclen : cardinal) : integer;
begin
  result := fpsend(FSocket, @srcdata, srclen, MSG_NOSIGNAL);
  if result < 0 then result := -socketerror;
end;

{$ifdef LINUX}

{ TSocketWatcher }

constructor TSocketWatcher.Create(amaxfds : integer);
begin
  maxfds := amaxfds;
  maxevents := 32; // ok for smaller systems too
  FFdEpoll := epoll_create(amaxfds);  // the size argument is ignored since linux 2.6.8
  SetLength(FREvents, maxevents);
end;

destructor TSocketWatcher.Destroy;
begin
  FileClose(FFdEpoll);
  SetLength(FREvents, 0);
  inherited Destroy;
end;

procedure TSocketWatcher.AddSocket(asock : TNanoSocket; aobj_link : TObject;
  ainev : TInEventHandler; aoutev : TOutEventHandler);
var
  ev : TEpollEvent;
  r : integer;
begin
  if asock.Socket < 0
  then
      raise ENanoNet.Create('SocketWatcher.AddSocket: invalid socket handle.');

  asock.obj_link := aobj_link;
  asock.in_event_handler := ainev;
  asock.out_event_handler := aoutev;

  ev.Events := 0;
  if asock.in_event_handler <> nil  then ev.Events := ev.Events or EPOLLIN;
  if asock.out_event_handler <> nil then ev.Events := ev.Events or EPOLLOUT;
  ev.data.ptr := pointer(asock);

  r := epoll_ctl(FFdEpoll, EPOLL_CTL_ADD, asock.Socket, @ev);
  if r < 0 then
  begin
    if errno = ESysEEXIST then
    begin
      r := epoll_ctl(FFdEpoll, EPOLL_CTL_MOD, asock.Socket, @ev);
    end;
    if r < 0
    then
        raise ENanoNet.Create('SocketWatcher.AddSocket: Error adding socket to epoll.');
  end;
end;

procedure TSocketWatcher.ModifySocket(asock : TNanoSocket);
var
  ev : TEpollEvent;
  r : integer;
begin
  if asock.Socket < 0
  then
      raise ENanoNet.Create('SocketWatcher.AddSocket: invalid socket handle.');

  ev.Events := 0;
  if asock.in_event_handler <> nil  then ev.Events := ev.Events or EPOLLIN;
  if asock.out_event_handler <> nil then ev.Events := ev.Events or EPOLLOUT;
  ev.data.ptr := pointer(asock);

  r := epoll_ctl(FFdEpoll, EPOLL_CTL_MOD, asock.Socket, @ev);
  if r < 0
  then
      raise ENanoNet.Create('SocketWatcher.ModifySocket: Error changing socket in epoll.');
end;

procedure TSocketWatcher.RemoveSocket(asock : TNanoSocket);
var
  ev : TEpollEvent;
begin
  if asock.Socket >= 0 then
  begin
    epoll_ctl(FFdEpoll, EPOLL_CTL_DEL, asock.Socket, @ev);
  end;
end;

procedure TSocketWatcher.SetOutHandler(asock : TNanoSocket; aoutev : TOutEventHandler);
begin
  asock.out_event_handler := aoutev;
  ModifySocket(asock);
end;

procedure TSocketWatcher.SetInHandler(asock : TNanoSocket; ainev : TOutEventHandler);
begin
  asock.in_event_handler := ainev;
  ModifySocket(asock);
end;

function TSocketWatcher.WaitForEvents(atimeout_ms : integer) : boolean;
var
  evcnt : integer;
  i     : integer;
  pev   : ^TEpollEvent;
  sock  : TNanoSocket;
begin
  result := false;

  evcnt := epoll_wait(FFdEpoll, @FREvents[0], length(FREvents), atimeout_ms);
  if evcnt < 0 then
  begin
    // some error happened.
    EXIT;
  end;

  // process the reported events
  for i := 0 to evcnt - 1 do
  begin
    pev := @FREvents[i];
    sock := TNanoSocket(pev^.Data.ptr);
    if (pev^.Events and EPOLLOUT) <> 0 then
    begin
      if sock.out_event_handler = nil
      then
          raise ENanoNet.Create('Socket has no output event handler!');

      sock.out_event_handler(sock.obj_link);
      result := true;
    end;
    if (pev^.Events and EPOLLIN) <> 0 then
    begin
      if sock.in_event_handler = nil
      then
          raise ENanoNet.Create('Socket has no input event handler!');

      sock.in_event_handler(sock.obj_link);
      result := true;
    end;
  end;
end;

{ TSConnection }

constructor TSConnection.Create(aserver : TNanoServer);
begin
  server := aserver;
  sock := TNanoSocket.Create(stTcp4);
end;

destructor TSConnection.Destroy;
begin
  sock.Free;
  inherited Destroy;
end;

procedure TSConnection.Close;
begin
  server.CloseConnection(self);
end;

function TSConnection.AcceptConnection(svrsock : TNanoSocket) : boolean;
begin
  result := sock.AcceptConnection(svrsock.socket);
end;

procedure TSConnection.HandleInput(aobj : TObject);
var
  sbuf : ansistring = '';
  r : integer;
begin
  writeln('Error: TSConnection.HandleInput is not overriden!');

  // just read the socket to handle the input event
  SetLength(sbuf, 16 * 1024);
  r := sock.Recv(sbuf[1], length(sbuf));
  if r > 0 then ;
  sbuf := '';
end;

procedure TSConnection.HandleOutput(aobj : TObject);
begin
  writeln('Error: TSConnection.HandleOutput is not overriden!');
end;

procedure TSConnection.SetOutHandler(aoutev : TOutEventHandler);
begin
  server.swatcher.SetOutHandler(sock, aoutev);
end;

{ TNanoServer }

constructor TNanoServer.Create(asconn_class : TSConnectionClass; alisten_port : uint16);
begin
  listen_port := alisten_port;
  sconn_class := asconn_class;
  sock_listen := TNanoSocket.Create(stTcp4);
  swatcher := TSocketWatcher.Create(64);

  connections := TSConnList.Create;
  connections.FreeObjects := False;
  closed_conns := TSConnList.Create;
  closed_conns.FreeObjects := True;   // free object only at closed connections
end;

destructor TNanoServer.Destroy;
begin
  while connections.Count > 0 do
  begin
    closed_conns.Add(connections[0]);
    connections.Delete(0);
  end;

  closed_conns.Free; // this also frees the objects
  connections.Free;
  swatcher.Free;
  sock_listen.Free;
  inherited Destroy;
end;

procedure TNanoServer.InitListener;
begin
  sock_listen.InitListener(listen_port);

  swatcher.AddSocket(sock_listen, sock_listen, @HandleListener, nil);
end;

function TNanoServer.WaitForEvents(timeout_ms : integer) : boolean;
begin
  result := swatcher.WaitForEvents(timeout_ms);

  while closed_conns.count > 0 do
  begin
    //writeln('freeing a connection object.');
    closed_conns.Delete(0); // also frees the objects
  end;
end;

procedure TNanoServer.HandleListener(aobj : TObject);
var
  conn : TSConnection;
begin
  //writeln('Handling listener object event!');

  conn := CreateSConnection();
  if not conn.AcceptConnection(sock_listen) then
  begin
    //writeln('Accept error!');
    conn.Free;
    EXIT;
  end;

  // add to the connection list
  connections.Add(conn);
  swatcher.AddSocket(conn.sock, conn, @conn.HandleInput, nil);
end;

function TNanoServer.CreateSConnection : TSConnection;
begin
  result := sconn_class.Create(self);
end;

procedure TNanoServer.CloseConnection(aconn : TSConnection);
begin
  // this is usually called from the HandleInput event handler.
  // Freeing the connection object is not safe now, so we just schedule
  // the connection for the safe free later putting it into the closed_conn list
  swatcher.RemoveSocket(aconn.sock);
  aconn.sock.Disconnect;

  connections.Remove(aconn);
  closed_conns.Add(aconn);
end;

{ TNanoUdpServer }

constructor TNanoUdpServer.Create(adg_class : TNanoDatagramClass; alisten_port : uint16);
begin
  dg_class := adg_class;
  listen_port := alisten_port;

  sock_listen := TNanoSocket.Create(stUdp4);
  swatcher := TSocketWatcher.Create(64);
end;

destructor TNanoUdpServer.Destroy;
begin
  swatcher.Free;
  sock_listen.Free;

  inherited Destroy;
end;

procedure TNanoUdpServer.InitListener;
begin
  sock_listen.InitListener(listen_port);

  swatcher.AddSocket(sock_listen, sock_listen, @HandleListener, nil);
end;

function TNanoUdpServer.WaitForEvents(timeout_ms : integer) : boolean;
begin
  result := swatcher.WaitForEvents(timeout_ms);
end;

procedure TNanoUdpServer.HandleListener(aobj : TObject);
var
  dg : TNanoDatagram;
begin
  dg := CreateDatagram();
  if dg.RecvRawData() > 0 then
  begin
    dg.ProcessInData();
  end;
  if not dg.keep_in_memory then
  begin
    dg.Free;
  end;
end;

function TNanoUdpServer.CreateDatagram : TNanoDatagram;
begin
  result := dg_class.Create(self);
end;

{ TNanoDatagram }

constructor TNanoDatagram.Create(aserver : TNanoUdpServer);
begin
  server := aserver;
  rawdata_len := 0;
  fillchar(remote_addr, sizeof(remote_addr), 0);
  remote_addr_len := sizeof(remote_addr);
  keep_in_memory := false;
end;

destructor TNanoDatagram.Destroy;
begin
  inherited Destroy;
end;

function TNanoDatagram.RecvRawData() : integer;
begin
  remote_addr_len := sizeof(remote_addr);
  result := fprecvfrom(server.sock_listen.Socket, @rawdata[0], length(rawdata), 0, @remote_addr, @remote_addr_len);
  if result > 0 then
  begin
    rawdata_len := result;
  end
  else
  begin
    rawdata_len := 0;
  end;
end;

function TNanoDatagram.SendRawData() : integer;
begin
  result := fpsendto(server.sock_listen.Socket, @rawdata[0], rawdata_len, 0, @remote_addr, remote_addr_len);
end;

procedure TNanoDatagram.ProcessInData();
begin
  // does nothing, should be overridden
end;

procedure TNanoDatagram.SetRemoteAddr(aaddr : ansistring; aport : uint16);
var
  sarr : array of ansistring;
begin
  sarr := aaddr.Split('.');
  if length(sarr) <> 4 then raise Exception.Create('Invalid Host IP address: '+aaddr);

	remote_addr.sin_family := AF_INET;
  remote_addr.sin_addr.s_addr := htonl(StrToHostAddr(aaddr).s_addr);
  remote_addr.sin_port := htons(aport);
end;

function TNanoDatagram.GetRemoteAddrStr() : ansistring;
begin
  result := NetAddrToStr(remote_addr.sin_addr) + ':' + IntToStr(ntohs(remote_addr.sin_port));
end;

{ TMultiClientMgr }

constructor TMultiClientMgr.Create;
begin
  swatcher := TSocketWatcher.Create(64);
  connections := TCConnList.Create;
  connections.FreeObjects := True;
end;

destructor TMultiClientMgr.Destroy;
begin
  connections.Free;  // this also frees the objects
  swatcher.Free;
  inherited Destroy;
end;

procedure TMultiClientMgr.AddConnection(aconn : TCConnection);
begin
  connections.Add(aconn);
end;

procedure TMultiClientMgr.CloseConnection(aconn : TCConnection);
begin
  swatcher.RemoveSocket(aconn.sock);
  aconn.sock.Disconnect;
end;

function TMultiClientMgr.WaitForEvents(timeout_ms : integer) : boolean;
begin
  result := swatcher.WaitForEvents(timeout_ms);
end;

{ TCConnection }

constructor TCConnection.Create(amanager : TMultiClientMgr);
begin
  server_port := 80;
  server_addr := '127.0.0.1';
  sock := TNanoSocket.Create(stTcp4);

  manager := amanager;
  manager.AddConnection(self);
end;

destructor TCConnection.Destroy;
begin
  sock.Free;
  inherited Destroy;
end;

procedure TCConnection.Connect;
begin
  if not sock.Connect(server_addr, server_port)
  then
      raise ENanoNet.Create('TCConnection.Connect: error initiating a connection');

  //manager.swatcher.AddSocket(sock, self, @HandleOutputForConnect, nil);
  manager.swatcher.AddSocket(sock, self, nil, @HandleOutputForConnect);
end;

procedure TCConnection.HandleOutputForConnect(aobj : TObject);  // called when the connect finished
var
  err : cint;
  len : TSockLen;
begin
  //writeln('HandleOutputForConnect');

  len := sizeof(err);
  if fpgetsockopt(sock.Socket, SOL_SOCKET, SO_ERROR, @err, @len) <> 0 then
  begin
    err := ESysEBADF;
  end;
  if err <> 0 then
  begin
    HandleConnectError(err);
  end
  else
  begin
    // Switch to output ready
    sock.in_event_handler := @HandleInput;
    sock.out_event_handler := @HandleOutput;
    manager.swatcher.ModifySocket(sock);

    HandleOutput(sock.obj_link);
  end;
end;

procedure TCConnection.HandleInput(aobj : TObject);
begin

end;

procedure TCConnection.HandleOutput(aobj : TObject);
begin

end;

procedure TCConnection.HandleConnectError(aerror : integer);
begin
  writeln('connect error: ', aerror);
  manager.CloseConnection(self);
end;

procedure TCConnection.SetOutHandler(aoutev : TOutEventHandler);
begin
  manager.swatcher.SetOutHandler(sock, aoutev);
end;


{$endif}

{$ifdef WINDOWS}
var
  wsaData : TWSAData;
{$endif}

initialization
begin
{$ifdef WINDOWS}
  wsaData.iMaxSockets := 0;
  WSAStartup($0202, wsaData);
{$endif}
end;

end.



