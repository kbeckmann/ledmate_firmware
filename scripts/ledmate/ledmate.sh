#!/bin/bash

ACM_DEVICE=${ACM_DEVICE:-/dev/ttyACM0}

LUT_SIZE=$(( 256 * 4 ))
FB_SIZE=$(( 144 * 8 ))

echo "LUT_SIZE: $LUT_SIZE"
echo "FB_SIZE: $FB_SIZE"

stty -F "$ACM_DEVICE" 115200 raw -clocal -echo icrnl

echo -n "cmd:bin:$LUT_SIZE;" > "$ACM_DEVICE";
cat palette.bin > "$ACM_DEVICE";
echo -n "cmd:update_lut;" > "$ACM_DEVICE";

while true; do
    echo -n "cmd:bin:$FB_SIZE;" > "$ACM_DEVICE";
    cat frame1.bin > "$ACM_DEVICE";
    echo -n "cmd:blit_lut;" > "$ACM_DEVICE";

    # Wait for OK.. Guess I should write this in python
    sleep 0.01
done;
