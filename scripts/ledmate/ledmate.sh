#!/bin/bash

ACM_DEVICE=${ACM_DEVICE:-/dev/ttyACM0}

LUT_SIZE=$(( 128 * 4 ))
FB_SIZE=$(( 144 * 8 ))

echo $FB_SIZE
stty -F "$ACM_DEVICE" 115200 raw -clocal -echo icrnl

while true; do
    echo -n "cmd:bin:$LUT_SIZE;" > "$ACM_DEVICE";
    cat palette.bin > "$ACM_DEVICE";
    echo -n "cmd:update_lut;" > "$ACM_DEVICE";

    echo -n "cmd:bin:$FB_SIZE;" > "$ACM_DEVICE";
    cat frame1.bin > "$ACM_DEVICE";
    echo -n "cmd:blit_lut;" > "$ACM_DEVICE";

    sleep 10
done;
