import socket
import struct
from udo_comm import *

class TUdoIpRqHeader:
    def __init__(self):
        self.rqid     : int = 0  # u32
        self.len_cmd  : int = 0  # u16: LEN, RW
        self.index    : int = 0  # u16
        self.offset   : int = 0  # u32
        self.metadata : int = 0  # u32

        self.bindata = b''
        self.bin_record_format = '=LHHLL'

    def LoadBin(self, bindata : bytes):
        self.bindata = bytes(bindata)
        (self.rqid,
         self.len_cmd,
         self.index,
         self.offset,
         self.metadata
         ) = struct.unpack(self.bin_record_format, self.bindata)

    def AsBin(self) -> bytes:
        self.bindata = struct.pack(self.bin_record_format,
            self.rqid,
            self.len_cmd,
            self.index,
            self.offset,
            self.metadata
        )
        return self.bindata

    def Size(self):
        return 16


class TCommHandlerUdoIp(TUdoCommHandler):
    def __init__(self):
        super().__init__()
        self.protocol = UCP_IP
        self.ipaddrstr = '127.0.0.1'
        self.sock : socket.socket | None = None
        self.cursqnum : int = 0
        self.max_tries : int = 3

        self.svr_ip : str = ''
        self.svr_port : int = 1221

        self.rqhead = TUdoIpRqHeader()
        self.anshead = TUdoIpRqHeader()

        self.iswrite   : bool = False
        self.mindex    : int = 0
        self.moffset   : int = 0
        self.rq_data   = bytearray()
        self.mrqlen    : int = 0
        self.opstring  : str = ''
        self.ans_index : int = 0
        self.ans_offset   : int = 0
        self.ans_metadata : int = 0
        self.ans_datalen  : int = 0
        self.ans_data  = bytearray()

    def Open(self):
        sarr = self.ipaddrstr.split(':')
        self.svr_ip = sarr[0]
        if len(sarr) > 1:
            self.svr_port = int(sarr[1])

        self.sock = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
        self.cursqnum = 0

    def Close(self):
        if self.sock:
            self.sock.close()
            self.sock = None

    def Opened(self) -> bool:
        return self.sock is not None

    def UdoRead(self, index : int, offset : int, maxdatalen : int) -> bytearray:
        self.iswrite = False
        self.mindex = index
        self.moffset = offset
        self.mrqlen = maxdatalen
        self.rq_data = bytearray()  # empty
        self.__DoUdoReadWrite()
        return self.ans_data

    def UdoWrite(self, index : int, offset : int, avalue : bytearray):
        self.iswrite = True
        self.mindex  = index
        self.moffset = offset
        self.rq_data = bytearray(avalue)
        self.mrqlen = len(self.rq_data)
        self.__DoUdoReadWrite()

    def __DoUdoReadWrite(self):
        self.sock.settimeout(self.timeout)
        self.cursqnum += 1

        self.rqhead.rqid = self.cursqnum
        self.rqhead.index = self.mindex
        self.rqhead.offset = self.moffset
        self.rqhead.metadata = 0  # no metadata support so far

        if self.iswrite:
            self.rqhead.len_cmd = self.mrqlen | (1 << 15)
            self.opstring = 'UdoWrite(%04X, %d)[%d]' % (self.mindex, self.moffset, self.mrqlen)
        else:
            self.rqhead.len_cmd = self.mrqlen # bit15 = 0: read
            self.opstring = 'UdoRead(%04X, %d)' % (self.mindex, self.moffset)

        rqbuf = bytearray(self.rqhead.AsBin())
        rqbuf.extend(self.rq_data)

        ansbuf = bytearray()

        trynum = 0
        while True:
            trynum += 1

            try:
                self.sock.sendto(rqbuf, (self.svr_ip, self.svr_port))
                (ansbuf, ans_addr) = self.sock.recvfrom(1536)
                bok = True
            except TimeoutError as e:
                if trynum >= self.max_tries:
                    raise EUdoAbort(UDOERR_TIMEOUT, '%s timeout' % (self.opstring))
                else:
                    continue
            except Exception as e:
                raise EUdoAbort(UDOERR_CONNECTION, '%s Rend/Recv error: %s' % (self.opstring, str(e)))

            # process the response
            # print('processing the response:', ansbuf)

            if len(ansbuf) < self.anshead.Size():
                if trynum >= self.max_tries:
                    raise EUdoAbort(UDOERR_CONNECTION, '%s invalid response length: %d' % (self.opstring, len(ansbuf)))
                else:
                    continue

            self.anshead.LoadBin(ansbuf[:self.anshead.Size()])
            #print('anshead.cmdlen=', self.anshead.len_cmd)
            self.ans_data = ansbuf[self.anshead.Size():]

            if (self.anshead.rqid != self.cursqnum) or (self.anshead.index != self.mindex)  or (self.anshead.offset != self.moffset):
                if trynum >= self.max_tries:
                    raise EUdoAbort(UDOERR_CONNECTION, '%s unexpected response' % (self.opstring))
                else:
                    continue

            if (self.anshead.len_cmd & 0x7FF) == 0x7FF:  # error response
                if len(self.ans_data) < 2:
                    raise EUdoAbort(UDOERR_CONNECTION, '%s error response length: %d' % (self.opstring, len(self.ans_data)))
                ecode = struct.unpack("=H", self.ans_data)[0]
                raise EUdoAbort(ecode, '%s result: %04X' % (self.opstring, ecode))

            if not self.iswrite:  # return the data
                if self.mrqlen < len(self.ans_data):
                    raise EUdoAbort(UDOERR_DATA_TOO_BIG, '%s result data is too big: %d' % (self.opstring, len(self.ans_data)))

            return # everything was ok, return with the data


udoip_commh = TCommHandlerUdoIp()
