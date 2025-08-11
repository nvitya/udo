unit commh_udosl;

{$mode ObjFPC}{$H+}

interface

uses
  Classes, SysUtils, udo_comm, serial_comm, util_nstime;

const
  UDOSL_MAX_RQ_SIZE = UDO_MAX_PAYLOAD_LEN + 32; // 1024 byte payload + variable size header

  UDOSL_DEFAULT_SPEED = 115200;

type

  { TCommHandlerUdoSl }

  TCommHandlerUdoSl = class(TUdoCommHandler)
  public
    devstr     : string;
    comm       : TSerialComm;

    constructor Create; override;
    destructor Destroy; override;

    procedure Open; override;
    procedure Close; override;
    function  Opened : boolean; override;
    function  ConnString : string; override;

    function  UdoRead(address : uint16; offset : uint32; out dataptr; maxdatalen : uint32) : integer; override;
    procedure UdoWrite(address : uint16; offset : uint32; const dataptr; datalen : uint32); override;

  protected
    rxreadpos  : integer;
    rxcnt      : integer;
    rxstate    : integer;
    crc        : byte;
    lastrecvtime : int64;

    opstring   : string;
    iswrite    : boolean;
    mindex     : uint16;
    moffset    : uint32;
    mmetadata  : uint32;
    mrqlen     : uint32;
    mdataptr   : PByte;

    ans_index    : uint16;
    ans_offset   : uint32;
    ans_metadata : uint32;
    ans_datalen  : integer;

    rwbuf_ansdatapos : integer;  // the start of the answer data in the rwbuf (answer phase)

    rwbuflen     : uint32;
    rwbuf         : array[0..UDOSL_MAX_RQ_SIZE - 1] of byte;

  protected

    procedure SendRequest();
    procedure RecvResponse();

    function  AddTx(const asrc; len : integer) : integer;
    function  TxAvailable() : integer;
  end;

var
  udosl_commh : TCommHandlerUdoSl;

implementation

{ TCommHandlerUdoSl }

constructor TCommHandlerUdoSl.Create;
begin
  inherited;
  comm := TSerialComm.Create;
  comm.baudrate := UDOSL_DEFAULT_SPEED;

  default_timeout := 0.2;
  timeout := default_timeout;

  protocol := ucpSerial;
  devstr := '';

  rxstate := 0;
end;

destructor TCommHandlerUdoSl.Destroy;
begin
  comm.Free;
  inherited Destroy;
end;

procedure TCommHandlerUdoSl.Open;
var
  sarr : array of string;
begin
  if Opened then Close;

  sarr := devstr.split(':');
  if length(sarr) > 1 then
  begin
    comm.baudrate := StrToIntDef(sarr[1], comm.baudrate);
  end;

  if not comm.Open(sarr[0])
  then
      raise EUdoAbort.Create(UDOERR_CONNECTION, 'UDO-SL: error opening device "%s"', [devstr]);
end;

procedure TCommHandlerUdoSl.Close;
begin
  comm.Close();
end;

function TCommHandlerUdoSl.Opened : boolean;
begin
  result := comm.Opened;
end;

function TCommHandlerUdoSl.ConnString : string;
begin
  Result := format('UDO-SL %s', [devstr]);
end;

function TCommHandlerUdoSl.UdoRead(address : uint16; offset : uint32; out dataptr; maxdatalen : uint32) : integer;
begin
  iswrite := false;
	mindex  := address;
  moffset := offset;
  mdataptr := PByte(@dataptr);
  mrqlen := maxdatalen;

  opstring := format('UdoRead(%.4X, %d)', [mindex, moffset]);

	SendRequest();
  RecvResponse();

	if ans_datalen > maxdatalen
  then
      raise EUdoAbort.Create(UDOERR_DATA_TOO_BIG, '%s result data is too big: %d', [opstring, ans_datalen]);

  // copy the response to the user buffer
  if ans_datalen > 0 then
  begin
	  move(rwbuf[rwbuf_ansdatapos], mdataptr^, ans_datalen);
  end;

	result := ans_datalen;
end;

procedure TCommHandlerUdoSl.UdoWrite(address : uint16; offset : uint32; const dataptr; datalen : uint32);
begin
  iswrite := true;
  mindex := address;
  moffset  := offset;
  mdataptr := PByte(@dataptr);
  mrqlen := datalen;

  opstring := format('UdoWrite(%.4X, %d)[%d]', [mindex, moffset, mrqlen]);

  SendRequest();
  RecvResponse();
end;

procedure TCommHandlerUdoSl.SendRequest;
var
  r : integer;
  b : byte;
  offslen : uint32;
  metalen : uint32;
  extlen  : uint32;
begin
	comm.FlushInput();
	comm.FlushOutput();

  if      moffset =     0  then offslen := 0
  else if moffset > $FFFF  then offslen := 4
  else if moffset >   $FF  then offslen := 2
  else                          offslen := 1;

  if      mmetadata =     0  then metalen := 0
  else if mmetadata > $FFFF  then metalen := 4
  else if mmetadata >   $FF  then metalen := 2
  else                            metalen := 1;

	// prepare the request
	crc := 0;
  rwbuflen := 0;

  // 1. the sync byte
	b := $55;
	AddTx(b, 1);

  // 2. command byte
  if iswrite then b := $80 else b := 0;
  if 4 = offslen then b := b or 3
                 else b := b or offslen;

  if 4 = metalen then b := b or $0C
                 else b := b or (metalen shl 2);

  extlen := 0;
  if       3 > mrqlen then b := b or (mrqlen shl 4)
  else if  4 = mrqlen then b := b or (3 shl 4)
  else if  8 = mrqlen then b := b or (4 shl 4)
  else if 16 = mrqlen then b := b or (5 shl 4)
  else
  begin
    b := b or (7 shl 4);
    extlen := mrqlen;
  end;

	AddTx(b, 1);  // add the command byte

  // 3. extlen
  if extlen > 0 then
  begin
    AddTx(extlen, 2);
  end;

  // 4. index
	AddTx(mindex, 2);

  // 5. offset
  if offslen > 0 then AddTx(moffset, offslen);

  // 6. metadata
	if metalen > 0 then AddTx(mmetadata, metalen);

  // 7. write data
	if iswrite and (mrqlen > 0) then
	begin
		AddTx(mdataptr^, mrqlen);
	end;

  // 8. crc
	AddTx(crc, 1);

	// send the request

	r := comm.Write(rwbuf[0], rwbuflen);
	if (r <= 0) or (r <> rwbuflen)
  then
      raise EUdoAbort.Create(UDOERR_CONNECTION, '%s: send error', [opstring]);

end;

procedure TCommHandlerUdoSl.RecvResponse;
var
  r : integer;
  b : byte;

  iserror : boolean;
  ecode   : uint16;

  lencode  : byte;
  offslen  : uint32;
  metalen  : uint32;

begin
  // receive the response

  iserror := false;
  lastrecvtime := nstime();

  rwbuflen := 0;
  rwbuf_ansdatapos := 0;

  rxreadpos := 0;
  rxstate := 0;
  rxcnt := 0;
  crc := 0;
  ans_datalen := 0;

  ans_offset   := 0;
  ans_metadata := 0;
  ans_index    := 0;

  while true do
  begin
    r := comm.Read(rwbuf[rwbuflen], sizeof(rwbuf) - rwbuflen);
    if r <= 0 then
    begin
    	if (r = 0) or (r = -11) then  // 11 = EAGAIN
    	begin
    		if nstime() - lastrecvtime > timeout * 1000000000 then
        begin
          raise EUdoAbort.Create(UDOERR_TIMEOUT, '%s timeout', [opstring]);
        end;

    		continue;
    	end
      else
      begin
        raise EUdoAbort.Create(UDOERR_TIMEOUT, '%s response read error: %d', [opstring, r]);
      end;
    end;

    lastrecvtime := nstime();

    Inc(rwbuflen, r);

    while rxreadpos < rwbuflen do
    begin
  		b := rwbuf[rxreadpos];

  		if (rxstate > 0) and (rxstate < 10) then
  		begin
  			crc := udo_calc_crc(crc, b);
  		end;

  		if 0 = rxstate then  // waiting for the sync byte
  		begin
  			if $55 = b then
  			begin
  				crc := udo_calc_crc(0, b); // start the CRC from zero
  				rxstate := 1;
  			end;
  		end
  		else if 1 = rxstate then // command and lengths
  		begin
        if ((b and $80) <> 0) <> iswrite then  // response R/W differs from the request ?
        begin
          rxstate := 0;
        end
        else
        begin
          // decode the length fields
          offslen := ($4210 shr ((b and 3) shl 2)) and $F;
          metalen := ($4210 shr (b and $C)) and $F;  // its already multiple by 4

          rxcnt := 0;
          rxstate := 3;  // index follows normally

          lencode := ((b shr 4) and 7);
          if      lencode < 5 then ans_datalen := (($84210 shr (lencode shl 2)) and $F) // in-line demultiplexing
          else if 5 = lencode then ans_datalen := 16
          else if 7 = lencode then rxstate := 2     // extended length follows
          else  // 6 == error code
          begin
            ans_datalen := 2;
            iserror := true;
          end;
        end;
      end
  		else if 2 = rxstate then // extended length
  		begin
  			if 0 = rxcnt then
  			begin
  				ans_datalen := b; // low byte
  				rxcnt := 1;
  			end
  			else
  			begin
  				ans_datalen := ans_datalen or (b shl 8); // high byte
  				rxcnt := 0;
  				rxstate := 3; // index follows
  			end;
  		end
  		else if 3 = rxstate then // index
  		begin
  			if 0 = rxcnt then
  			begin
  				ans_index := b;  // index low
  				rxcnt := 1;
  			end
  			else
  			begin
  				ans_index := ans_index or (b shl 8);  // index high
  				rxcnt := 0;
  				if offslen > 0 then
  				begin
  					rxstate := 4;  // offset follows
  				end
  				else if metalen > 0 then
  				begin
  					rxstate := 5;  // meta follows
  				end
  				else if ans_datalen > 0 then
          begin
            rxstate := 6;  // read data or error code
          end
          else
          begin
            rxstate := 10;  // then crc check
          end;
  			end;
  		end
  		else if 4 = rxstate then // offset
  		begin
        ans_offset := ans_offset or (b shl (rxcnt shl 3));
  			Inc(rxcnt);
  			if rxcnt >= offslen then
  			begin
          rxcnt := 0;
  				if metalen > 0 then
  				begin
  					rxstate := 5;  // meta follows
  				end
  				else if ans_datalen > 0 then
          begin
            rxstate := 6;  // read data or error code
          end
          else
          begin
            rxstate := 10;  // then crc check
          end;
  			end;
  		end
  		else if 5 = rxstate then // metadata
  		begin
        ans_metadata := ans_metadata or (b shl (rxcnt shl 3));
  			Inc(rxcnt);
  			if rxcnt >= metalen then
  			begin
          rxcnt := 0;
  				if ans_datalen > 0 then
          begin
            rxstate := 6;  // read data or error code
          end
          else
          begin
            rxstate := 10;  // then crc check
          end;
  			end;
  		end
  		else if 6 = rxstate then // read data (or error code)
  		begin
        if 0 = rxcnt then  // save the answer data start position
        begin
          rwbuf_ansdatapos := rxreadpos;
        end;

        // just count the bytes, the rwbuf_ansdatapos points to its start
  			Inc(rxcnt);
  			if rxcnt >= ans_datalen then
  			begin
  				rxstate := 10;
  			end;
  		end
  		else if 10 = rxstate then // crc check
  		begin
  			if b <> crc then
  			begin
          raise EUdoAbort.Create(UDOERR_CRC, '%s CRC error', [opstring]);
  			end
  			else  // CRC OK
  			begin
  				if iserror then
          begin
            ecode := PUint16(@rwbuf[rwbuf_ansdatapos])^;
            raise EUdoAbort.Create(ecode, '%s result: %.4X', [opstring, ecode]);
          end
          else
          begin
    				EXIT;  // everything is ok, return to the caller
          end;
        end;
  		end;

  		Inc(rxreadpos);
    end;
  end;
end;

function TCommHandlerUdoSl.AddTx(const asrc; len : integer) : integer;
var
  available : integer;
  srcp, dstp, endp : PByte;
  b : byte;
begin
  available := TxAvailable();
  if 0 = available then exit(0);

  if len > available then len := available;

  srcp := PByte(@asrc);
  dstp := @rwbuf[rwbuflen];
  endp := dstp + len;
  while dstp < endp do
  begin
    b := srcp^;
    dstp^ := b;
    crc := udo_calc_crc(crc, b);
    Inc(srcp);
    Inc(dstp);
  end;

  rwbuflen += len;

  result := len;
end;

function TCommHandlerUdoSl.TxAvailable : integer;
begin
  result := sizeof(rwbuf) - rwbuflen;
end;

initialization
begin
  udosl_commh := TCommHandlerUdoSl.Create;
end;

end.

