#!/bin/env python3
import os
import time
import subprocess
import struct

DEV = "/dev/ttyACM0"

class Serial:
    def __init__(self, path):
        subprocess.call("stty -F {} 115200 raw -clocal -echo icrnl".format(DEV), shell=True)
        self.f = os.open(path, os.O_RDWR)

    def write(self, data):
        os.write(self.f, data)

    def writeCmd(self, data):
        os.write(self.f, data)
        print(s.readAll())

    def read(self, len):
        ret = os.read(self.f, len)
        if ret == b'':
            exit(1)
        return ret

    def readAll(self):
        buf = b''
        for _ in range(1024):
            buf += self.read(1)
            if buf[-1:] == b";":
                return buf

    def binbuf(self, data):
        print("Uploading binbuf")
        self.writeCmd(bytes("bin:{};".format(len(data)).encode("utf-8")))
        self.writeCmd(data)

    def blitImageFromFile(self, path):
        data = open(path, "rb").read()
        buf = b''
        if len(data) == 2304:
            # Weird 16 bit gimp shit
            for y in range(8):
                for x in range(144):
                    if y % 2 == 0:
                        buf += struct.pack("<B", data[2 * (y * 144 + x)])
                    else:
                        buf += struct.pack("<B", data[2 * (y * 144 + 144 - x - 1)])
            self.binbuf(buf)
            self.writeCmd(b'copy_and_blit;')
            print("Done")


s = Serial(DEV)

buf = b''
for i in range(256):
    # value = (i // 16) * 256 + i % 16
    value = (i // 16)
    buf += struct.pack("<L", value)

s.binbuf(buf)
s.writeCmd(b'update_lut;')

buf = b''
for y in range(8):
    for x in range(144):
        value = x
        buf += struct.pack("<B", value)

print(len(buf))
s.binbuf(buf)
s.writeCmd(b'copy_and_blit;')


s.blitImageFromFile("/home/konrad/test.data")



# offset = 0
# while True:
#     buf = b''
#     for i in range(256):
#         n = i + offset
#         value = (n // 16) * 256 + n % 16
#         buf += struct.pack("<L", value)
#     offset += 1
#     s.binbuf(buf)
#     s.writeCmd(b'update_lut;')
#     s.writeCmd(b'blit;')

#     time.sleep(0.1)


while True:
    print(s.readAll())
