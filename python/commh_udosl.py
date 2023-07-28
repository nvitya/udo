import serial
import struct
from udo_comm import *

# CRC8 table with the standard polynom of 0x07:
udo_crc_table = bytearray([
    0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d,
    0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65, 0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
    0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5, 0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
    0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85, 0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
    0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2, 0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
    0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2, 0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
    0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32, 0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
    0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42, 0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
    0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c, 0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
    0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec, 0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
    0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c, 0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
    0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c, 0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
    0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b, 0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63,
    0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b, 0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
    0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb, 0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83,
    0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb, 0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3
])

UDOSL_DEFAULT_SPEED = 115200

def calculate_crc(abarr : bytearray) -> int:
    crc : int = 0
    for b in abarr:
        idx = (crc ^ b)
        crc = udo_crc_table[idx]
    return crc

class TCommHandlerUdoSl(TUdoCommHandler):
    def __init__(self):
        super().__init__()
        self.protocol = UCP_SERIAL
        self.devstr : str = ''
        self.baudrate = UDOSL_DEFAULT_SPEED
        self.com : serial.Serial | None = None

        self.txdata = bytearray()
        self.rxdata = bytearray()

        self.opstring : str = ''

        self.iswrite  : bool = False
        self.mindex   : int = 0
        self.moffset  : int = 0
        self.mmetadata : int = 0
        self.mrqlen    : int = 0
        self.rqdata = bytearray()

        self.ans_index    : int = 0
        self.ans_offset   : int = 0
        self.ans_metadata : int = 0
        self.ans_datalen  : int = 0
        self.ans_data = bytearray()

    def Open(self):
        sarr = self.devstr.split(':')
        if len(sarr) > 1:
            try:
                self.baudrate = int(sarr[1])
            except:
                pass

        self.com = serial.Serial(self.devstr, self.baudrate, timeout=self.timeout,
                                 bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE)

    def Close(self):
        if self.com:
            self.com.close()
            self.com = None

    def Opened(self) -> bool:
        if self.com:
            return True
        else:
            return False

    def UdoRead(self, index : int, offset : int, maxdatalen : int) -> bytearray:
        self.iswrite = False
        self.mindex  = index
        self.moffset = offset
        self.mrqlen = maxdatalen

        self.opstring = "UdoRead(%.4X, %d)" % (self.mindex, self.moffset)

        self.SendRequest()
        self.RecvResponse()

        if self.ans_datalen > maxdatalen:
            raise EUdoAbort(UDOERR_DATA_TOO_BIG, "%s result data is too big: %d" % (self.opstring, self.ans_datalen))

        return self.ans_data

    def UdoWrite(self, index : int, offset : int, avalue : bytearray):
        self.iswrite = False
        self.mindex  = index
        self.moffset = offset
        self.rqdata = bytearray(avalue)
        self.mrqlen = len(self.rqdata)

        self.opstring = "UdoWrite(%.4X, %d)[%d]" % (self.mindex, self.moffset, self.mrqlen)

        self.SendRequest()
        self.RecvResponse()

    def SendRequest(self):
        self.com.flushInput()
        self.com.flushOutput()

        if   self.moffset == 0:      offslen = 0
        elif self.moffset > 0xFFFF:  offslen = 4
        elif self.moffset >   0xFF:  offslen = 2
        else:                        offslen = 1

        if   self.mmetadata ==     0:  metalen = 0
        elif self.mmetadata > 0xFFFF:  metalen = 4
        elif self.mmetadata >   0xFF:  metalen = 2
        else:                          metalen = 1

        self.txdata = bytearray()

        # 1. the sync byte
        self.txdata.append(0x55)

        # 2. command byte
        if self.iswrite:  b = 0x80
        else:             b = 0

        if 4 == offslen:   b |= 3
        else:              b |= offslen

        if 4 == metalen:   b |= 0x0C
        else:              b |= (metalen << 2)

        extlen = 0
        if    3  > self.mrqlen:    b |= (self.mrqlen << 4)
        elif  4 == self.mrqlen:    b |= (3 << 4)
        elif  8 == self.mrqlen:    b |= (4 << 4)
        elif 16 == self.mrqlen:    b |= (5 << 4)
        else:
            b |= (7 << 4)
            extlen = self.mrqlen

        self.txdata.append(b)  # add the command byte

        # 3. extlen
        if extlen > 0:
            self.txdata.extend([extlen & 0xFF, (extlen >> 8) & 0xFF])

        # 4. index
        self.txdata.extend([self.mindex & 0xFF, (self.mindex >> 8) & 0xFF])

        # 5. offset
        if offslen > 0:
            self.txdata.append(self.moffset & 0xFF)
            if offslen > 1:  self.txdata.append((self.moffset >>  8) & 0xFF)
            if offslen > 2:  self.txdata.append((self.moffset >> 16) & 0xFF)
            if offslen > 3:  self.txdata.append((self.moffset >> 24) & 0xFF)

        # 6. metadata
        if metalen > 0:
            self.txdata.append(self.mmetadata & 0xFF)
            if metalen > 1:  self.txdata.append((self.mmetadata >>  8) & 0xFF)
            if metalen > 2:  self.txdata.append((self.mmetadata >> 16) & 0xFF)
            if metalen > 3:  self.txdata.append((self.mmetadata >> 24) & 0xFF)

        # 7. write data
        if self.iswrite and (self.mrqlen > 0):
            self.txdata.extend(self.rqdata)

        # 8. crc
        self.txdata.append(calculate_crc(self.txdata))

        # send the request
        self.com.write(self.txdata)

    def RecvResponse(self):
        self.ans_data = bytearray()
        self.rxdata = bytearray()
        self.ans_datalen = 0
        self.ans_offset = 0
        self.ans_metadata = 0
        self.ans_index = 0
        offslen = 0
        metalen = 0
        iserror = False

        rxstate = 0
        rxcnt = 0
        rxpos = 0
        while True:
            rxch = self.com.read(1)
            # print('received: 0x%02X' % rxch[0])
            if len(rxch) == 0:
                raise EUdoAbort(UDOERR_CONNECTION, "Error receiving response")
            self.rxdata.extend(bytearray(rxch))
            while rxpos < len(self.rxdata):
                b = self.rxdata[rxpos]
                if 0 == rxstate:
                    rxpos = 0
                    rxcnt = 0
                    if 0x55 == self.rxdata[0]:
                        rxstate = 1
                    else:
                        self.rxdata.pop(0) # remove the first byte and continue

                elif 1 == rxstate: # command and lengths
                    if ((b & 0x80) != 0) != self.iswrite:  # does the response R/W differ from the request ?
                        rxstate = 0
                    else:
                        # decode the length fields
                        offslen = (0x4210 >> ((b & 3) << 2)) & 0xF
                        metalen = (0x4210 >> (b & 0xC)) & 0xF  # its already multiple of 4
                        rxcnt = 0
                        rxstate = 3  # index follows normally
                        lencode = ((b >> 4) & 7)
                        if   lencode < 5:   self.ans_datalen = ((0x84210 >> (lencode << 2)) & 0xF) # in-line demultiplexing
                        elif 5 == lencode:  self.ans_datalen = 16
                        elif 7 == lencode:  rxstate = 2     # extended length follows
                        else:  # 6 == error code
                            self.ans_datalen = 2
                            iserror = True
                            
                elif 2 == rxstate: # extended length
                    if 0 == rxcnt:
                        self.ans_datalen = b # low byte
                        rxcnt = 1
                    else:
                        self.ans_datalen |= (b << 8) # high byte
                        rxcnt = 0
                        rxstate = 3 # index follows
				
                elif 3 == rxstate: # index
                    if 0 == rxcnt:
                        self.ans_index = b  # index low
                        rxcnt = 1
                    else:
                        self.ans_index |= (b << 8)  # index high
                        rxcnt = 0
                        if offslen > 0:
                            rxstate = 4  # offset follows
                        elif metalen > 0:
                            rxstate = 5  # meta follows
                        elif self.ans_datalen > 0:
                            rxstate = 6  # read data or error code
                        else:
                            rxstate = 10  # then crc check

                elif 4 == rxstate:  # offset
                    self.ans_offset |= (b << (rxcnt << 3))
                    rxcnt += 1
                    if (rxcnt >= offslen):
                        rxcnt = 0
                        if metalen > 0:
                            rxstate = 5  # meta follows
                        elif self.ans_datalen > 0:
                            rxstate = 6  # read data or error code
                        else:
                            rxstate = 10  # then crc check

                elif 5 == rxstate:  # metadata
                    self.ans_metadata |= (b << (rxcnt << 3))
                    rxcnt += 1
                    if rxcnt >= metalen:
                        rxcnt = 0
                        if self.ans_datalen > 0:
                            rxstate = 6  # read data or error code
                        else:
                            rxstate = 10  # then crc check

                elif 6 == rxstate:  # read data (or error code)
                    self.ans_data.append(b)
                    rxcnt += 1
                    if rxcnt >= self.ans_datalen:
                        rxstate = 10  # crc check

                elif 10 == rxstate:  # crc check
                    crc = calculate_crc(self.rxdata[0:rxpos])
                    if b != crc:
                        raise EUdoAbort(UDOERR_CRC, "%s CRC error" % self.opstring)

                    if iserror:
                        if len(self.ans_data) < 2:
                            raise EUdoAbort(UDOERR_CONNECTION, '%s error response length: %d' % (self.opstring, len(self.ans_data)))
                        ecode = struct.unpack("=H", self.ans_data)[0]
                        raise EUdoAbort(ecode, '%s result: %04X' % (self.opstring, ecode))

                    return  # everything was ok, return with the data in self.ans_data

                rxpos += 1
            #/  while
        #/ while


udosl_commh = TCommHandlerUdoSl()