unit udo_comm;

{$mode ObjFPC}{$H+}

interface

uses
  Classes, SysUtils;

const
  UDO_MAX_PAYLOAD_LEN = 1024;

const
  UDOERR_CONNECTION             = $1001;  // not connected, send / receive error
  UDOERR_CRC                    = $1002;
  UDOERR_TIMEOUT                = $1003;
  UDOERR_DATA_TOO_BIG           = $1004;

  UDOERR_WRONG_INDEX            = $2000;  // index / object does not exists
  UDOERR_WRONG_OFFSET           = $2001;  // like the offset must be divisible by the 4
  UDOERR_WRONG_ACCESS           = $2002;
  UDOERR_READ_ONLY              = $2010;
  UDOERR_WRITE_ONLY             = $2011;
  UDOERR_WRITE_BOUNDS           = $2012;  // write is out ouf bounds
  UDOERR_WRITE_VALUE            = $2020;  // invalid value
  UDOERR_RUN_MODE               = $2030;  // config mode required
  UDOERR_UNITSEL                = $2040;  // the referenced unit is not existing
  UDOERR_BUSY                   = $2050;

  UDOERR_NOT_IMPLEMENTED        = $9001;
  UDOERR_INTERNAL               = $9002;  // internal implementation error
  UDOERR_APPLICATION            = $9003;  // internal implementation error

type

  { EUdoAbort }

  EUdoAbort = class(Exception)
  public
    ecode : uint16;
    constructor Create(acode : uint16; amsg : string; const args : array of const);
  end;

  TUdoCommProtocol = (ucpNone, ucpSerial, ucpIP);

  { TUdoCommHandler }

  TUdoCommHandler = class
  public
    default_timeout : single;
    timeout : single;
    protocol : TUdoCommProtocol;

    constructor Create; virtual;

  public // virtual functions
    procedure Open; virtual;
    procedure Close; virtual;
    function  Opened : boolean; virtual;
    function  ConnString : string; virtual;

    function  UdoRead(index : uint16; offset : uint32; out dataptr; maxdatalen : uint32) : integer; virtual;
    procedure UdoWrite(index : uint16; offset : uint32; const dataptr; datalen : uint32); virtual;
  end;

  { TUdoComm }

  TUdoComm = class
  public
    commh : TUdoCommHandler;

    max_payload_size : uint16;

    constructor Create;

    procedure SetHandler(acommh : TUdoCommHandler);
    procedure Open;
    procedure Close;
    function  Opened : boolean;

    function  UdoRead(index : uint16; offset : uint32; out dataptr; maxdatalen : uint32) : integer;
    procedure UdoWrite(index : uint16; offset : uint32; const dataptr; datalen : uint32);

  public // some helper functions
    function  ReadBlob(index : uint16; offset : uint32; out dataptr; maxdatalen : uint32) : integer;
    procedure WriteBlob(index : uint16; offset : uint32; const dataptr; datalen : uint32);

    function  ReadI32(index : uint16; offset : uint32) : int32;
    function  ReadI16(index : uint16; offset : uint32) : int16;
    function  ReadU32(index : uint16; offset : uint32) : uint32;
    function  ReadU16(index : uint16; offset : uint32) : uint16;
    function  ReadU8(index : uint16; offset : uint32) : uint8;

    procedure WriteI32(index : uint16; offset : uint32; avalue : int32);
    procedure WriteI16(index : uint16; offset : uint32; avalue : int16);
    procedure WriteU32(index : uint16; offset : uint32; avalue : uint32);
    procedure WriteU16(index : uint16; offset : uint32; avalue : uint16);
    procedure WriteU8(index : uint16; offset : uint32; avalue : uint8);

  end;

function UdoAbortText(abortcode : word) : string;

function udo_calc_crc(acrc : byte; adata : byte) : byte;

var
  commh_none : TUdoCommHandler = nil;

var
  udocomm : TUdoComm;

implementation

function UdoAbortText(abortcode : word) : string;
begin
  if      abortcode = UDOERR_CONNECTION       then result := 'connection error'
  else if abortcode = UDOERR_CRC              then result := 'CRC error'
  else if abortcode = UDOERR_TIMEOUT          then result := 'Timeout'
  else if abortcode = UDOERR_DATA_TOO_BIG     then result := 'Data does not fit'
  else if abortcode = UDOERR_WRONG_INDEX      then result := 'Object does not exists'
  else if abortcode = UDOERR_WRONG_OFFSET     then result := 'Invalid offset'
  else if abortcode = UDOERR_WRONG_ACCESS     then result := 'Invalid Access'
  else if abortcode = UDOERR_READ_ONLY        then result := 'Writing read-only object'
  else if abortcode = UDOERR_WRITE_ONLY       then result := 'Reading write-only object'
  else if abortcode = UDOERR_WRITE_BOUNDS     then result := 'Write out of bounds'
  else if abortcode = UDOERR_WRITE_VALUE      then result := 'invalid value'
  else if abortcode = UDOERR_RUN_MODE         then result := 'config mode required'
  else if abortcode = UDOERR_UNITSEL          then result := 'Unit not existing'
  else if abortcode = UDOERR_BUSY             then result := 'Busy'
  else if abortcode = UDOERR_NOT_IMPLEMENTED  then result := 'Not Implemented'
  else if abortcode = UDOERR_INTERNAL         then result := 'Internal error'
  else if abortcode = UDOERR_APPLICATION      then result := 'Application error'
  else
      result := format('Error 0x%04X', [abortcode]);
end;

// CRC8 table with the standard polynom of 0x07:
const udo_crc_table : array[0..255] of byte =
(
  $00, $07, $0e, $09, $1c, $1b, $12, $15, $38, $3f, $36, $31, $24, $23, $2a, $2d,
  $70, $77, $7e, $79, $6c, $6b, $62, $65, $48, $4f, $46, $41, $54, $53, $5a, $5d,
  $e0, $e7, $ee, $e9, $fc, $fb, $f2, $f5, $d8, $df, $d6, $d1, $c4, $c3, $ca, $cd,
  $90, $97, $9e, $99, $8c, $8b, $82, $85, $a8, $af, $a6, $a1, $b4, $b3, $ba, $bd,
  $c7, $c0, $c9, $ce, $db, $dc, $d5, $d2, $ff, $f8, $f1, $f6, $e3, $e4, $ed, $ea,
  $b7, $b0, $b9, $be, $ab, $ac, $a5, $a2, $8f, $88, $81, $86, $93, $94, $9d, $9a,
  $27, $20, $29, $2e, $3b, $3c, $35, $32, $1f, $18, $11, $16, $03, $04, $0d, $0a,
  $57, $50, $59, $5e, $4b, $4c, $45, $42, $6f, $68, $61, $66, $73, $74, $7d, $7a,
  $89, $8e, $87, $80, $95, $92, $9b, $9c, $b1, $b6, $bf, $b8, $ad, $aa, $a3, $a4,
  $f9, $fe, $f7, $f0, $e5, $e2, $eb, $ec, $c1, $c6, $cf, $c8, $dd, $da, $d3, $d4,
  $69, $6e, $67, $60, $75, $72, $7b, $7c, $51, $56, $5f, $58, $4d, $4a, $43, $44,
  $19, $1e, $17, $10, $05, $02, $0b, $0c, $21, $26, $2f, $28, $3d, $3a, $33, $34,
  $4e, $49, $40, $47, $52, $55, $5c, $5b, $76, $71, $78, $7f, $6a, $6d, $64, $63,
  $3e, $39, $30, $37, $22, $25, $2c, $2b, $06, $01, $08, $0f, $1a, $1d, $14, $13,
  $ae, $a9, $a0, $a7, $b2, $b5, $bc, $bb, $96, $91, $98, $9f, $8a, $8d, $84, $83,
  $de, $d9, $d0, $d7, $c2, $c5, $cc, $cb, $e6, $e1, $e8, $ef, $fa, $fd, $f4, $f3
);

function udo_calc_crc(acrc : byte; adata : byte) : byte;
var
  idx : byte;
begin
  idx := (acrc xor adata);
  result := udo_crc_table[idx];
end;

{ EUdoAbort }

constructor EUdoAbort.Create(acode : uint16; amsg : string; const args : array of const);
begin
  inherited CreateFmt(amsg, args);
  ecode := acode;
end;

{ TUdoCommHandler }

constructor TUdoCommHandler.Create;
begin
  default_timeout := 1;
  timeout := default_timeout;
  protocol := ucpNone;
end;

procedure TUdoCommHandler.Open;
begin
  raise EUdoAbort.Create(UDOERR_APPLICATION, 'Open: Invalid commhandler', []);
end;

procedure TUdoCommHandler.Close;
begin
  // does nothing
end;

function TUdoCommHandler.Opened : boolean;
begin
  result := false;
end;

function TUdoCommHandler.ConnString : string;
begin
  result := 'NONE';
end;

function TUdoCommHandler.UdoRead(index : uint16; offset : uint32; out dataptr; maxdatalen : uint32) : integer;
begin
  result := 0;
  raise EUdoAbort.Create(UDOERR_APPLICATION, 'UdoRead: Invalid commhandler', []);
end;

procedure TUdoCommHandler.UdoWrite(index : uint16; offset : uint32; const dataptr; datalen : uint32);
begin
  raise EUdoAbort.Create(UDOERR_APPLICATION, 'UdoWrite: Invalid commhandler', []);
end;

{ TUdoComm }

constructor TUdoComm.Create;
begin
  commh := commh_none;
  max_payload_size := 64;  // start with the smallest
end;

procedure TUdoComm.SetHandler(acommh : TUdoCommHandler);
begin
  if acommh <> nil then commh := acommh
                   else commh := commh_none;
end;

procedure TUdoComm.Open;
var
  d32 : uint32;
  r : integer;
begin
  if not commh.Opened then
  begin
    commh.Open();
  end;

  r := commh.UdoRead($0000, 0, d32, 4);  // check the fix data register first
  if (r <> 4) or (d32 <> $66CCAA55) then
  begin
    commh.Close();
    raise EUdoAbort.Create(UDOERR_CONNECTION, 'Invalid Obj-0000 response: %.8X', [d32]);
  end;

  r := commh.UdoRead($0001, 0, d32, 4);  // get the maximal payload length
  if (d32 < 64) or (d32 > UDO_MAX_PAYLOAD_LEN) then
  begin
    commh.Close();
    raise EUdoAbort.Create(UDOERR_CONNECTION, 'Invalid maximal payload size: %d', [d32]);
  end;

  max_payload_size := d32;
end;

procedure TUdoComm.Close;
begin
  commh.Close();
end;

function TUdoComm.Opened : boolean;
begin
  result := commh.Opened();
end;

function TUdoComm.UdoRead(index : uint16; offset : uint32; out dataptr; maxdatalen : uint32) : integer;
var
  pdata : PByte;
begin
  result := commh.UdoRead(index, offset, dataptr, maxdatalen);
  if (result <= 8) and (result < maxdatalen) then
  begin
    pdata := PByte(@dataptr);
    FillChar(PByte(pdata + result)^, maxdatalen - result, 0); // pad smaller responses, todo: sign extension
  end;
end;

procedure TUdoComm.UdoWrite(index : uint16; offset : uint32; const dataptr; datalen : uint32);
begin
  commh.UdoWrite(index, offset, dataptr, datalen);
end;

function TUdoComm.ReadBlob(index : uint16; offset : uint32; out dataptr; maxdatalen : uint32) : integer;
var
  chunksize : integer;
  remaining : integer;
  offs  : uint32;
  pdata : PByte;
  r : integer;
begin
  result := 0;
  remaining := maxdatalen;
  pdata := PByte(@dataptr);
  offs  := offset;

  while remaining > 0 do
  begin
    chunksize := max_payload_size;
    if chunksize > remaining then chunksize := remaining;
    r := commh.UdoRead(index, offs, pdata^, chunksize);
    if r <= 0
    then
        break;

    result += r;
    pdata  += r;
    offs   += r;
    remaining -= r;

    if r < chunksize
    then
        break;
  end;
end;

procedure TUdoComm.WriteBlob(index : uint16; offset : uint32; const dataptr; datalen : uint32);
var
  chunksize : integer;
  remaining : integer;
  offs  : uint32;
  pdata : PByte;
begin
  remaining := datalen;
  pdata := PByte(@dataptr);
  offs  := offset;

  while remaining > 0 do
  begin
    chunksize := max_payload_size;
    if chunksize > remaining then chunksize := remaining;
    commh.UdoWrite(index, offs, pdata^, chunksize);

    pdata  += chunksize;
    offs   += chunksize;
    remaining -= chunksize;
  end;
end;

function TUdoComm.ReadI32(index : uint16; offset : uint32) : int32;
var
  rvalue : int32 = 0;
begin
  commh.UdoRead(index, offset, rvalue, sizeof(rvalue));
  result := rvalue;
end;

function TUdoComm.ReadI16(index : uint16; offset : uint32) : int16;
var
  rvalue : int16 = 0;
begin
  commh.UdoRead(index, offset, rvalue, sizeof(rvalue));
  result := rvalue;
end;

function TUdoComm.ReadU32(index : uint16; offset : uint32) : uint32;
var
  rvalue : uint32 = 0;
begin
  commh.UdoRead(index, offset, rvalue, sizeof(rvalue));
  result := rvalue;
end;

function TUdoComm.ReadU16(index : uint16; offset : uint32) : uint16;
var
  rvalue : uint16 = 0;
begin
  commh.UdoRead(index, offset, rvalue, sizeof(rvalue));
  result := rvalue;
end;

function TUdoComm.ReadU8(index : uint16; offset : uint32) : uint8;
var
  rvalue : uint16 = 0;
begin
  commh.UdoRead(index, offset, rvalue, sizeof(rvalue));
  result := rvalue;
end;

procedure TUdoComm.WriteI32(index : uint16; offset : uint32; avalue : int32);
var
  lvalue : int32;
begin
  lvalue := avalue;
  commh.UdoWrite(index, offset, lvalue, 4);
end;

procedure TUdoComm.WriteI16(index : uint16; offset : uint32; avalue : int16);
var
  lvalue : int16;
begin
  lvalue := avalue;
  commh.UdoWrite(index, offset, lvalue, 2);
end;

procedure TUdoComm.WriteU32(index : uint16; offset : uint32; avalue : uint32);
var
  lvalue : uint32;
begin
  lvalue := avalue;
  commh.UdoWrite(index, offset, lvalue, 4);
end;

procedure TUdoComm.WriteU16(index : uint16; offset : uint32; avalue : uint16);
var
  lvalue : uint16;
begin
  lvalue := avalue;
  commh.UdoWrite(index, offset, lvalue, 2);
end;

procedure TUdoComm.WriteU8(index : uint16; offset : uint32; avalue : uint8);
var
  lvalue : int8;
begin
  lvalue := avalue;
  commh.UdoWrite(index, offset, lvalue, 1);
end;

initialization
begin
  commh_none := TUdoCommHandler.Create;
  udocomm := TUdoComm.Create;
end;

end.

