#!/bin/env python3
import os
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
        return os.read(self.f, len)

    def readAll(self):
        buf = b''
        for _ in range(1024):
            buf += os.read(self.f, 1)
            if buf[-1:] == b";":
                return buf

    def binbuf(self, data):
        print("Uploading binbuf")
        self.writeCmd(bytes("bin:{};".format(len(data)).encode("utf-8")))
        self.writeCmd(buf)

s = Serial(DEV)

buf = b''
for i in range(256):
    buf += struct.pack("<L", i)

s.binbuf(buf)
s.writeCmd(b'update_lut;')

buf = b'\x00' * (144 * 8)
s.binbuf(buf)
s.writeCmd(b'blit_lut;')

while True:
    print(s.readAll())
