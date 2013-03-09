#!/bin/sh

ffmpeg -i "$1" -f s16le - | ./test-sinc-highest 44100 48000 $3 | ffmpeg -y -ar 48000 -f s16le -ac 2 -i - "$2"
