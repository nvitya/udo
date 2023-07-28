
UCP_NONE   = 0
UCP_SERIAL = 1
UCP_IP     = 2

UDO_MAX_PAYLOAD_LEN = 1024

UDOERR_CONNECTION             = 0x1001  # not connected, send / receive error
UDOERR_CRC                    = 0x1002
UDOERR_TIMEOUT                = 0x1003
UDOERR_DATA_TOO_BIG           = 0x1004
UDOERR_WRONG_INDEX            = 0x2000  # index / object does not exists
UDOERR_WRONG_OFFSET           = 0x2001  # like the offset must be divisible by the 4
UDOERR_WRONG_ACCESS           = 0x2002
UDOERR_READ_ONLY              = 0x2010
UDOERR_WRITE_ONLY             = 0x2011
UDOERR_WRITE_BOUNDS           = 0x2012  # write is out ouf bounds
UDOERR_WRITE_VALUE            = 0x2020  # invalid value
UDOERR_RUN_MODE               = 0x2030  # config mode required
UDOERR_UNITSEL                = 0x2040  # the referenced unit is not existing
UDOERR_BUSY                   = 0x2050
UDOERR_NOT_IMPLEMENTED        = 0x9001
UDOERR_INTERNAL               = 0x9002  # internal implementation error
UDOERR_APPLICATION            = 0x9003  # internal implementation error


class EUdoAbort(Exception):
  def __init__(self, aerrorcode, amessage):
    self.errorcode = aerrorcode
    self.message = amessage
      

class TUdoCommHandler:
    def __init__(self):
        self.protocol = UCP_NONE
        self.default_timeout : float = 0.5
        self.timeout : float = self.default_timeout

    def Open(self):
        raise EUdoAbort(UDOERR_APPLICATION, "Open: Invalid comm. handler")

    def Close(self):
        pass

    def Opened(self) -> bool:
        return False

    def UdoRead(self, index : int, offset : int, maxdatalen : int) -> bytearray:
        raise EUdoAbort(UDOERR_APPLICATION, "Open: Invalid comm. handler")

    def UdoWrite(self, index : int, offset : int, avalue : bytearray):
        raise EUdoAbort(UDOERR_APPLICATION, "Open: Invalid comm. handler")

class TUdoComm:
    def __init__(self):
        self.commh = commh_none
        self.max_payload_size = 64

    def SetHandler(self, acommh : TUdoCommHandler):
        if acommh:
            self.commh = acommh
        else:
            self.commh = commh_none

    def Close(self):
        self.commh.Close()

    def Opened(self) -> bool:
        return self.commh.Opened()

    def UdoRead(self, index : int, offset : int, maxdatalen : int) -> bytearray:
        return self.commh.UdoRead(index, offset, maxdatalen)

    def UdoWrite(self, index : int, offset : int, value : bytearray):
        self.commh.UdoWrite(index, offset, value)

    def ReadI32(self, index : int, offset : int) -> int:
        r = self.commh.UdoRead(index, offset, 4)
        return self.ByteArrayToInt(r)

    def ReadI16(self, index : int, offset : int) -> int:
        r = self.commh.UdoRead(index, offset, 2)
        return self.ByteArrayToInt(r)

    def ReadU32(self, index : int, offset : int) -> int:
        r = self.commh.UdoRead(index, offset, 4)
        return self.ByteArrayToUint(r)

    def ReadU16(self, index : int, offset : int) -> int:
        r = self.commh.UdoRead(index, offset, 2)
        return self.ByteArrayToUint(r)

    def ReadU8(self, index : int, offset : int) -> int:
        r = self.commh.UdoRead(index, offset, 1)
        return self.ByteArrayToUint(r)

    def WriteU32(self, index : int, offset : int, value : int):
        self.commh.UdoWrite(index, offset, self.IntToByteArray(value, 4))

    def WriteI32(self, index : int, offset : int, value : int):
        self.commh.UdoWrite(index, offset, self.IntToByteArray(value, 4))

    def WriteU16(self, index : int, offset : int, value : int):
        self.commh.UdoWrite(index, offset, self.IntToByteArray(value, 2))

    def WriteI16(self, index : int, offset : int, value : int):
        self.commh.UdoWrite(index, offset, self.IntToByteArray(value, 2))

    def WriteU8(self, index : int, offset : int, value : int):
        self.commh.UdoWrite(index, offset, self.IntToByteArray(value, 1))

    def ReadBlob(self, index : int, offset : int, maxdatalen : int) -> bytearray:
        result = bytearray()
        remaining = maxdatalen
        offs = offset
        while remaining > 0:
             chunksize = self.max_payload_size
             if chunksize > remaining:  chunksize = remaining
             r = self.commh.UdoRead(index, offs, chunksize)
             if len(r) <= 0:
                 break

             result.extend(r)
             offs += len(r)
             remaining -= len(r)
             if len(r) < chunksize:
                 break
        return result

    def WriteBlob(self, index : int, offset : int, value : bytearray):
        data = bytearray(value)
        remaining = len(data)
        dataoffs = 0
        while remaining > 0:
            chunksize = self.max_payload_size
            if chunksize > remaining:  chunksize = remaining
            self.commh.UdoWrite(index, offset + dataoffs, data[dataoffs : dataoffs+chunksize])
            dataoffs += chunksize
            remaining -= chunksize

    def Open(self):
        if not self.commh.Opened():
            self.commh.Open()

        r = self.ReadU32(0x0000, 0)
        if r != 0x66CCAA55:
            self.Close()
            raise EUdoAbort(UDOERR_CONNECTION, f"Invalid Obj-0000 response: {r:.8X}")

        r = self.ReadU32(0x0001, 0) # get the maximal payload length
        if (r < 64) or (r > UDO_MAX_PAYLOAD_LEN):
            self.Close()
            raise EUdoAbort(UDOERR_CONNECTION, f"Invalid maximal payload size: {r}")

        self. max_payload_size = r

    @staticmethod
    def ByteArrayToInt(data : bytearray) -> int:
        """ uses only up to the first 4 bytes """
        v : int = 0
        bs = 0
        dl = len(data)
        if dl > 4:  dl = 4
        for b in data[:dl]:
            v |= (b << bs)
            bs += 8
        # convert to signed:
        if (dl == 2) and ((v & 0x8000) != 0):  v = -(0xFFFF - v + 1)
        elif (dl >= 4) and (v & 0x80000000) != 0:  v = -(0xFFFFFFFF - v + 1)
        return v

    @staticmethod
    def ByteArrayToUint(data : bytearray) -> int:
        """ uses only up to the first 4 bytes """
        v : int = 0
        bs = 0
        dl = len(data)
        if dl > 4:  dl = 4
        for b in data[:dl]:
            v |= (b << bs)
            bs += 8
        return v

    @staticmethod
    def IntToByteArray(avalue : int, alen = 4) -> bytearray:
        wdata = bytearray()
        i : int = 0
        while i < alen:
            wdata.append((avalue >> (8 * i)) & 0xFF)
            i += 1
        return wdata

commh_none = TUdoCommHandler()
udocomm = TUdoComm()