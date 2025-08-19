(*-----------------------------------------------------------------------------
  This file is a part of the UDO project: https://github.com/nvitya/udo
  Copyright (c) 2023 Viktor Nagy, nvitya

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
   file:     udo_comm.pas
   brief:    UDO Master communication objects
   date:     2023-09-27
   authors:  nvitya
*)
unit udo_comm;

{$mode ObjFPC}{$H+}

interface

uses
  Classes, SysUtils, udo_common;

type

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
    function  ReadF32(index : uint16; offset : uint32) : single;

    procedure WriteI32(index : uint16; offset : uint32; avalue : int32);
    procedure WriteI16(index : uint16; offset : uint32; avalue : int16);
    procedure WriteU32(index : uint16; offset : uint32; avalue : uint32);
    procedure WriteU16(index : uint16; offset : uint32; avalue : uint16);
    procedure WriteU8(index : uint16; offset : uint32; avalue : uint8);
    procedure WriteF32(index : uint16; offset : uint32; avalue : single);

  end;

var
  commh_none : TUdoCommHandler = nil;

var
  udocomm : TUdoComm;

implementation

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

function TUdoComm.ReadF32(index : uint16; offset : uint32) : single;
var
  rvalue : single = 0;
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

procedure TUdoComm.WriteF32(index : uint16; offset : uint32; avalue : single);
var
  lvalue : single;
begin
  lvalue := avalue;
  commh.UdoWrite(index, offset, lvalue, 4);
end;

initialization
begin
  commh_none := TUdoCommHandler.Create;
  udocomm := TUdoComm.Create;
end;

end.

