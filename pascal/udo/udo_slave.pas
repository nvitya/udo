unit udo_slave;

{$mode ObjFPC}{$H+}

interface

uses
  Classes, SysUtils, udo_common, nano_sockets;

const
  udo_ip_default_port = 1221;

type
  PUdoRequest = ^TUdoRequest;
  TUdoRequest = record
    iswrite   : boolean;  // false: read
    metalen   : byte;     // length of the metadata
    index     : uint16;   // object index
    offset    : uint32;
    metadata  : uint32;

    rqlen     : uint16;   // requested read/write length
    anslen    : uint16;   // response data length, default = 0
    maxanslen : uint16;   // maximal read buffer length
    result    : uint16;   // 0 if no error

    dataptr   : PByte;
  end;

  TUdoIpRqHeader = packed record
    rqid     : uint32;   // request id to detect repeated requests
    len_cmd  : uint16;   // LEN, MLEN, RW
    index    : uint16;   // object index
    offset   : uint32;
    metadata : uint32;
  end;
  PUdoIpRqHeader = ^TUdoIpRqHeader;

  { TUdoDatagram }

  TUdoDatagram = class(TNanoDatagram)
  public
    constructor Create(aserver : TNanoUdpServer); override;
    procedure ProcessInData(); override;
  end;

  { TUdoIpSlaveBase }

  TUdoIpSlaveBase = class(TNanoUdpServer)
  public
    dgans     : TUdoDatagram;

    constructor Create(alisten_port : uint16 = udo_ip_default_port); reintroduce;
    destructor Destroy; override;

    function ParamReadWrite(var udorq : TUdoRequest) : boolean; virtual;
  end;

function udo_response_error(var udorq : TUdoRequest; aresult : uint16) : boolean;
function udo_response_ok(var udorq : TUdoRequest) : boolean;
function udo_ro_int(var udorq : TUdoRequest; avalue : integer; len : uint32) : boolean;
function udo_ro_uint(var udorq : TUdoRequest; avalue : uint32; len : uint32) : boolean;
function udo_ro_f32(var udorq : TUdoRequest; avalue : single) : boolean;
function udo_ro_f64(var udorq : TUdoRequest; avalue : double) : boolean;

function udo_rw_data(var udorq : TUdoRequest; dataptr : Pointer; datalen : uint32) : boolean;
function udo_rw_data_zp(var udorq : TUdoRequest; dataptr: Pointer; datalen : uint32) : boolean;
function udo_ro_data(var udorq : TUdoRequest; dataptr: Pointer; datalen : uint32) : boolean;
function udo_wo_data(var udorq : TUdoRequest; dataptr: Pointer; datalen : uint32) : boolean;
function udo_response_string(var udorq : TUdoRequest; astr : ansistring) : boolean;

function udorq_intvalue(var udorq : TUdoRequest) : int32;
function udorq_uintvalue(var udorq : TUdoRequest) : uint32;
function udorq_f32value(var udorq : TUdoRequest) : single;
function udorq_f64value(var udorq : TUdoRequest) : double;

function udoslave_handle_base_objects(var udorq : TUdoRequest) : boolean;

// application-specific callback (must be defined by the user)
//function udoslave_app_read_write(udorq : PUdoRequest) : boolean;

implementation

function udo_response_error(var udorq : TUdoRequest; aresult : uint16) : boolean;
begin
  udorq.result := aresult;
  result := (aresult = 0);
end;

function udo_response_ok(var udorq : TUdoRequest) : boolean;
begin
  udorq.result := 0;
  result := true;
end;

function udo_ro_int(var udorq : TUdoRequest; avalue : integer; len : uint32) : boolean;
begin
	if udorq.iswrite then
  begin
		udorq.result := UDOERR_READ_ONLY;
		EXIT(false);
	end;

  PInt32(udorq.dataptr)^ := avalue;
  udorq.anslen := len;
  udorq.result := 0;

  result := true;
end;

function udo_ro_uint(var udorq : TUdoRequest; avalue : uint32; len : uint32) : boolean;
begin
	if udorq.iswrite then
  begin
		udorq.result := UDOERR_READ_ONLY;
		EXIT(false);
	end;

  PUInt32(udorq.dataptr)^ := avalue;
  udorq.anslen := len;
  udorq.result := 0;

  result := true;
end;

function udo_ro_f32(var udorq : TUdoRequest; avalue : single) : boolean;
begin
	if udorq.iswrite then
  begin
		udorq.result := UDOERR_READ_ONLY;
		EXIT(false);
	end;

  PSingle(udorq.dataptr)^ := avalue;
  udorq.anslen := 4;
  udorq.result := 0;

  result := true;
end;

function udo_ro_f64(var udorq : TUdoRequest; avalue : double) : boolean;
begin
	if udorq.iswrite then
  begin
		udorq.result := UDOERR_READ_ONLY;
		EXIT(false);
	end;

  PDouble(udorq.dataptr)^ := avalue;
  udorq.anslen := 8;
  udorq.result := 0;

  result := true;
end;

function udo_rw_data(var udorq : TUdoRequest; dataptr : Pointer; datalen : uint32) : boolean;
var
  cp : PByte;
  chunksize, remaining : uint32;
begin
	if udorq.iswrite then
  begin
		// write, handling segmented write too

		//TRACE("udo_write(%04X, %u), len=%u\r\n", udorq->index, udorq->offset, udorq->datalen);

		if datalen < udorq.offset + udorq.rqlen then
		begin
			udorq.result := UDOERR_WRITE_BOUNDS;
			EXIT(false);
    end;

		cp := PByte(dataptr);
		cp += udorq.offset;

		chunksize := datalen - udorq.offset;  // the maximal possible chunksize
		if chunksize > udorq.rqlen then  chunksize := udorq.rqlen;

		move(udorq.dataptr^, cp^, chunksize);
		udorq.result := 0;
		result := true;
	end
	else // read, handling segmented read too
	begin
		if datalen <= udorq.offset then
		begin
			udorq.anslen := 0; // empty read: no more data
			EXIT(true);
    end;

		cp := PByte(dataptr);
		cp += udorq.offset;

		remaining := datalen - udorq.offset;  // already checked for negative case
		if remaining > udorq.maxanslen then
		begin
			udorq.anslen := udorq.maxanslen;
    end
		else
		begin
			udorq.anslen := remaining;
    end;

		move(udorq.dataptr^, cp^, udorq.anslen);
		udorq.result := 0;
		result := true;
  end;
end;

function udo_rw_data_zp(var udorq : TUdoRequest; dataptr : Pointer; datalen : uint32) : boolean;
begin
  if udorq.iswrite then
  begin
    FillChar(dataptr^, datalen, 0);
  end;
  result := udo_rw_data(udorq, dataptr, datalen);
end;

function udo_ro_data(var udorq : TUdoRequest; dataptr : Pointer; datalen : uint32) : boolean;
begin
	if not udorq.iswrite then
	begin
		EXIT( udo_rw_data(udorq, dataptr, datalen) );
  end;

	udorq.result := UDOERR_READ_ONLY;
	result := false;
end;

function udo_wo_data(var udorq : TUdoRequest; dataptr : Pointer; datalen : uint32) : boolean;
begin
	if udorq.iswrite then
	begin
		EXIT( udo_rw_data(udorq, dataptr, datalen) );
  end;

	udorq.result := UDOERR_WRITE_ONLY;
	result := false;
end;

function udo_response_string(var udorq : TUdoRequest; astr : ansistring) : boolean;
begin
	result := udo_ro_data(udorq, @astr[1], length(astr));
end;

function udorq_intvalue(var udorq : TUdoRequest) : int32;
begin
	if      4 <= udorq.rqlen      then result := PInt32(udorq.dataptr)^
	else if 2  = udorq.rqlen      then result := PInt16(udorq.dataptr)^
	else if 3  = udorq.rqlen      then result := (PInt32(udorq.dataptr)^ and $00FFFFFF)
	else if 0  = udorq.rqlen      then result := 0
	else
    EXIT( PInt8(udorq.dataptr)^ )
  ;
end;

function udorq_uintvalue(var udorq : TUdoRequest) : uint32;
begin
	if      4 <= udorq.rqlen      then result := PUInt32(udorq.dataptr)^
	else if 2  = udorq.rqlen      then result := PUInt16(udorq.dataptr)^
	else if 3  = udorq.rqlen      then result := (PUInt32(udorq.dataptr)^ and $00FFFFFF)
	else if 0  = udorq.rqlen      then result := 0
	else
    result := PUInt8(udorq.dataptr)^
  ;
end;

function udorq_f32value(var udorq : TUdoRequest) : single;
begin
	if udorq.rqlen >= sizeof(single) then result := PSingle(udorq.dataptr)^
                                   else result := 0;
end;

function udorq_f64value(var udorq : TUdoRequest) : double;
begin
	if udorq.rqlen >= sizeof(double) then result := PDouble(udorq.dataptr)^
                                   else result := 0;
end;

function udoslave_handle_blobtest(var udorq : TUdoRequest) : boolean; // object 0002
var
  datalen : uint32 = 262144;
  v, remaining : uint32;
  sp, dp, endp : PUint32;
begin
	// provide 16384 incrementing 32-bit integers
	if (udorq.offset and 3) <> 0 then
	begin
		EXIT( udo_response_error(udorq, UDOERR_WRONG_OFFSET) );
	end;

	udorq.rqlen := ((udorq.rqlen + 3) and $FFFC); // force rqlen 4 divisible

	v := (udorq.offset shr 2); // starting number

	if udorq.iswrite then
	begin
		if datalen < udorq.offset + udorq.rqlen then
		begin
  		EXIT( udo_response_error(udorq, UDOERR_WRITE_BOUNDS) );
		end;

		sp := PUint32(udorq.dataptr);
		endp := sp + (udorq.rqlen shr 2);
		while sp < endp do
		begin
			if sp^ <> v then  EXIT( udo_response_error(udorq, UDOERR_WRITE_VALUE) );
      Inc(sp);
      Inc(v);
		end;
	end
	else  // read
	begin
		if datalen <= udorq.offset then
    begin
			udorq.anslen := 0; // empty read: no more data
			EXIT(true);
		end;

		remaining := datalen - udorq.offset;
		if remaining > udorq.maxanslen then	udorq.anslen := udorq.maxanslen
 		                               else udorq.anslen := remaining;

		dp := PUint32(udorq.dataptr);
		endp := dp + (udorq.anslen shr 2);
		while dp < endp do
    begin
			dp^ := v;
      Inc(dp);
      Inc(v);
		end;
	end;

	udorq.result := 0;
	result := true;
end;


function udoslave_handle_base_objects(var udorq : TUdoRequest) : boolean;
begin
  if $0000 = udorq.index then // communication test
  begin
    result := udo_ro_uint(udorq, $66CCAA55, 4);
  end
  else if $0001 = udorq.index then // maximal payload length
  begin
    result := udo_ro_uint(udorq, UDO_MAX_PAYLOAD_LEN, 4);
  end
  else if $0002 = udorq.index then // blob test object
  begin
    result := udoslave_handle_blobtest(udorq);
  end
  else
    result := udo_response_error(udorq, UDOERR_INDEX);
end;

{ TUdoDatagram }

constructor TUdoDatagram.Create(aserver : TNanoUdpServer);
begin
  inherited Create(aserver);
end;

procedure TUdoDatagram.ProcessInData();
var
  udoslave : TUdoIpSlaveBase;

  dgans : TUdoDatagram;

  prqh, pansh        : PUdoIpRqHeader;
  pansdata, prqdata  : PByte;

  mudorq : TUdoRequest;
begin
  udoslave := TUdoIpSlaveBase(server);

  //TODO: add answer cacheing
  dgans := TUdoIpSlaveBase(server).dgans;

  prqh  := PUdoIpRqHeader(@rawdata[0]);
  pansh := PUdoIpRqHeader(@dgans.rawdata[0]);
  move(prqh^, pansh^, sizeof(TUdoIpRqHeader));  // initialize the answer header with the request header

	prqdata  := @rawdata[sizeof(TUdoIpRqHeader)]; // the data comes right after the header
	pansdata := @dgans.rawdata[sizeof(TUdoIpRqHeader)]; // the data comes right after the header

	// execute the UDO request

  mudorq.result := 0;
	FillChar(mudorq, sizeof(mudorq), 0);
	mudorq.index  := prqh^.index;
	mudorq.offset := prqh^.offset;
	mudorq.rqlen := (prqh^.len_cmd and $7FF);
	mudorq.maxanslen := mudorq.rqlen;
	mudorq.iswrite := ((prqh^.len_cmd and $8000) <> 0);
	mudorq.metalen := (($8420 shr ((prqh^.len_cmd shr 13) and 3) * 4) and $0F);
	mudorq.metadata := prqh^.metadata;

	if mudorq.iswrite then
	begin
		// write
		mudorq.dataptr := prqdata;
		mudorq.rqlen := rawdata_len - sizeof(TUdoIpRqHeader);  // override the datalen from the header
  end
	else // read
	begin
		mudorq.dataptr := pansdata;
	end;

	udoslave.ParamReadWrite(mudorq);

	if mudorq.result <> 0 then
	begin
		mudorq.anslen := 2;
		pansh^.len_cmd := (pansh^.len_cmd or $7FF); // abort response
		PUint16(pansdata)^ := mudorq.result;
	end;

  dgans.rawdata_len := mudorq.anslen + sizeof(TUdoIpRqHeader);

	// send the response

  dgans.remote_addr := remote_addr;
  dgans.SendRawData();
end;

{ TUdoIpSlaveBase }

constructor TUdoIpSlaveBase.Create(alisten_port : uint16);
begin
  inherited Create(TUdoDatagram, alisten_port);

  dgans := TUdoDatagram(CreateDatagram);
end;

destructor TUdoIpSlaveBase.Destroy;
begin
  dgans.Free;
  inherited Destroy;
end;

function TUdoIpSlaveBase.ParamReadWrite(var udorq : TUdoRequest) : boolean;
begin
  if udorq.index < $1010 then
  begin
    result := udoslave_handle_base_objects(udorq);
  end
  else
  begin
    result := udo_response_error(udorq, UDOERR_INDEX);
  end;
end;

end.

