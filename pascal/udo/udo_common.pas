unit udo_common;

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

  UDOERR_INDEX                  = $2000;  // index / object does not exists
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

function UdoAbortText(abortcode : word) : string;

function udo_calc_crc(acrc : byte; adata : byte) : byte;

implementation

function UdoAbortText(abortcode : word) : string;
begin
  if      abortcode = UDOERR_CONNECTION       then result := 'connection error'
  else if abortcode = UDOERR_CRC              then result := 'CRC error'
  else if abortcode = UDOERR_TIMEOUT          then result := 'Timeout'
  else if abortcode = UDOERR_DATA_TOO_BIG     then result := 'Data does not fit'
  else if abortcode = UDOERR_INDEX            then result := 'Object does not exists'
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

end.

