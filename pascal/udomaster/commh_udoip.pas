unit commh_udoip;

{$mode ObjFPC}{$H+}

interface

uses
  Classes, SysUtils, Sockets,
{$ifdef WINDOWS}
  Windows, WinSock,
{$else}
  BaseUnix,
{$endif}
  udo_comm, fpcfixes;

const
  UDOIP_MAX_DATALEN = UDO_MAX_PAYLOAD_LEN;
  UDOIP_MAX_RQ_SIZE = UDOIP_MAX_DATALEN + 16; // 1024 byte payload + 16 byte header

  UDOIP_DEFAULT_PORT = 1221;

type
  TUdoIpRqHeader = packed record
  	rqid         : uint32;  // sequence id to detect repeated requests
  	len_cmd      : uint16;  // LEN, MLEN, RW
  	index        : uint16;
  	offset       : uint32;
  	metadata     : uint32;
  end;

  PUdoIpRqHeader = ^TUdoIpRqHeader;

type

  { TCommHandlerUdoIp }

  TCommHandlerUdoIp = class(TUdoCommHandler)
  public
    ipaddrstr : string;

    max_tries : integer;

    timeout_ms : integer;

    cursqnum : uint16;

    constructor Create; override;
    destructor Destroy; override;

    procedure Open; override;
    procedure Close; override;
    function  Opened : boolean; override;
    function  ConnString : string; override;

    function  UdoRead(address : uint16; offset : uint32; out dataptr; maxdatalen : uint32) : integer; override;
    procedure UdoWrite(address : uint16; offset : uint32; const dataptr; datalen : uint32); override;

  protected
    fdsocket      : integer;
    server_addr   : TSockAddr;
    response_addr : TSockAddr;
    rxpoll_fds    : Tfdset;

    rqbuf  : array[0..UDOIP_MAX_RQ_SIZE-1] of byte;
    ansbuf : array[0..UDOIP_MAX_RQ_SIZE-1] of byte;

    iswrite   : boolean;
    mindex  : uint16;
    moffset   : uint32;
    mdataptr  : PByte;
    mrqlen  : integer;

    opstring : string;

    ans_index    : uint16;
    ans_offset   : uint32;
    ans_metadata : uint32;

    ans_datalen : integer;

    procedure DoUdoReadWrite;

  end;

var
  udoip_commh : TCommHandlerUdoIp;

implementation

{ TCommHandlerUdoIp }

constructor TCommHandlerUdoIp.Create;
begin
  inherited;
  protocol := ucpIP;
  ipaddrstr := '127.0.0.1';

  cursqnum := 0; // always start at zero, and increment, the port number will be at every connection different
  fdsocket := -1;
  timeout_ms := 500;  // ESP responds with 100 ms delay
  max_tries := 3;
end;

destructor TCommHandlerUdoIp.Destroy;
begin
  Close;
  inherited Destroy;
end;

procedure TCommHandlerUdoIp.Open;
var
  sarr : array of string;
begin
  if fdsocket >= 0 then
  begin
    closesocket(fdsocket);
  end;

  // Create UDP socket:
  fdsocket := fpSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  sarr := ipaddrstr.split(':');

  // Create UDP socket:
	server_addr.sin_family := AF_INET;
  server_addr.sin_addr.s_addr := htonl(StrToHostAddr(sarr[0]).s_addr);
  if length(sarr) > 1 then
  begin
  	server_addr.sin_port := htons(StrToInt(sarr[1]));
  end
  else
  begin
  	server_addr.sin_port := htons(UDOIP_DEFAULT_PORT);
  end;

{$ifdef WINDOWS}
  FD_ZERO(rxpoll_fds);
  FD_SET(fdsocket, rxpoll_fds);
{$else}
  fpFD_ZERO(rxpoll_fds);
  fpFD_SET(fdsocket, rxpoll_fds);
{$endif}

  cursqnum := 0;
end;

procedure TCommHandlerUdoIp.Close;
begin
  if fdsocket >= 0 then
  begin
    closesocket(fdsocket);
    fdsocket := -1;
  end;
end;

function TCommHandlerUdoIp.Opened : boolean;
begin
  Result := (fdsocket >= 0);
end;

function TCommHandlerUdoIp.ConnString : string;
begin
  Result := format('UDO-IP %s', [ipaddrstr]);
end;

function TCommHandlerUdoIp.UdoRead(address : uint16; offset : uint32; out dataptr; maxdatalen : uint32) : integer;
begin
  iswrite  := false;
  mindex := address;
  moffset  := offset;
  mdataptr := PByte(@dataptr);
  mrqlen := maxdatalen;

  DoUdoReadWrite;

  result := ans_datalen;
end;

procedure TCommHandlerUdoIp.UdoWrite(address : uint16; offset : uint32; const dataptr; datalen : uint32);
begin
  iswrite := true;
  mindex := address;
  moffset  := offset;
  mdataptr := PByte(@dataptr);
  mrqlen := datalen;

  DoUdoReadWrite;
end;

procedure TCommHandlerUdoIp.DoUdoReadWrite;
var
  rqhead, anshead : PUdoIpRqHeader;
  headsize : integer;
  rsp_addr_len : TSocklen;
  r : integer;
  trynum : integer;
  ecode : uint16;
  tv : TTimeVal;
label
  repeat_send, repeat_recv;
begin
  Inc(cursqnum); // increment sequence number

  rqhead  := PUdoIpRqHeader(@rqbuf[0]);
  anshead := PUdoIpRqHeader(@ansbuf[0]);
  headsize := sizeof(TUdoIpRqHeader);

  rqhead^.rqid   := cursqnum;
  rqhead^.index := mindex;
  rqhead^.offset := moffset;
  rqhead^.metadata := 0;  // no metadata support so far

  if iswrite then
  begin
    rqhead^.len_cmd := mrqlen or (1 shl 15);
    move(mdataptr^, rqbuf[headsize], mrqlen);
    opstring := format('UdoWrite(%.4X, %d)[%d]', [mindex, moffset, mrqlen]);
  end
  else  // read
  begin
    rqhead^.len_cmd := mrqlen;  // bit15 = 0: read
    opstring := format('UdoRead(%.4X, %d)', [mindex, moffset]);
  end;

  trynum := 1;

repeat_send:

  r := fpsendto(fdsocket, @rqbuf[0], headsize + mrqlen, 0, @server_addr, sizeof(server_addr));
  if r <= 0
  then
      raise EUdoAbort.Create(UDOERR_CONNECTION, '%s request error: %d', [opstring, r]);

repeat_recv:

  // timed wait for the response, letting the OS do something other
  tv.tv_sec := timeout_ms div 1000;
  tv.tv_usec := (timeout_ms mod 1000) * 1000;

{$ifdef WINDOWS}
  FD_ZERO(rxpoll_fds);
  FD_SET(fdsocket, rxpoll_fds);
  r := select(fdsocket + 1, @rxpoll_fds, nil, nil, @tv);
{$else}
  fpFD_ZERO(rxpoll_fds);
  fpFD_SET(fdsocket, rxpoll_fds);
  r := fpselect(fdsocket + 1, @rxpoll_fds, nil, nil, @tv);
{$endif}
  if r <= 0 then
  begin
    if trynum < max_tries then
    begin
      Inc(trynum);       // the only place where the trynum is incremented !
      goto repeat_send;  // re-send on timeout
    end;
    raise EUdoAbort.Create(UDOERR_TIMEOUT, '%s timeout', [opstring]);
  end;

  rsp_addr_len := sizeof(response_addr);
  r := fprecvfrom(fdsocket, @ansbuf[0], sizeof(ansbuf), 0, @response_addr, @rsp_addr_len);
  if r <= 0
  then
    raise EUdoAbort.Create(UDOERR_TIMEOUT, '%s response read error: %d', [opstring, r]);

  ans_datalen := r - headsize; // data length
  if ans_datalen < 0 then
  begin
    // something invalid received
    if trynum < max_tries
    then
        goto repeat_recv;

    raise EUdoAbort.Create(UDOERR_CONNECTION, '%s invalid response length: %d', [opstring, ans_datalen]);
  end;

  if (anshead^.rqid <> cursqnum) or (anshead^.index <> mindex) or (anshead^.offset <> moffset) then
  begin
    if trynum < max_tries
    then
        goto repeat_recv;

    raise EUdoAbort.Create(UDOERR_CONNECTION, '%s unexpected response', [opstring]);
  end;

  if (anshead^.len_cmd and $7FF) = $7FF then // error response?
  begin
    if r < sizeof(TUdoIpRqHeader) + 2
    then
        raise EUdoAbort.Create(UDOERR_CONNECTION, '%s error response length: %d', [opstring, r]);

    ecode := PUint16(@ansbuf[headsize])^;
    raise EUdoAbort.Create(ecode, '%s result: %.4X', [opstring, ecode]);
  end;

  if not iswrite then
  begin
    if ans_datalen > 0 then
    begin
      if ans_datalen > mrqlen then
      begin
        raise EUdoAbort.Create(UDOERR_DATA_TOO_BIG, '%s result data is too big: %d', [opstring, ans_datalen]);
      end;

      move(ansbuf[headsize], mdataptr^, ans_datalen);
    end;
  end;
end;

initialization
begin
  udoip_commh := TCommHandlerUdoIp.Create;
end;

end.

