#!/bin/sh
# usage: sd-image.sh make|check IMAGE [MiB]
# Makes the SD card image sd_test reads, and checks what it wrote.
# Needs mkfs.fat, fsck.fat, mtools and python3.
set -e
export MTOOLS_SKIP_CHECK=1 LC_ALL=C.UTF-8
img=$2
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
case $1 in
make)
   rm -f "$img"
   truncate -s "${3:-128}M" "$img"
   mkfs.fat -F 32 -s 1 "$img" > /dev/null
   printf 'hello\n' > "$tmp/hello"
   printf 'long\n' > "$tmp/long"
   printf 'deep\n' > "$tmp/deep"
   mcopy -i "$img" "$tmp/hello" ::/hello.txt
   mcopy -i "$img" "$tmp/long" "::/A Long File Name.data"
   mmd -i "$img" ::/dir ::/dir/sub ::/dir/many
   mcopy -i "$img" "$tmp/deep" "::/dir/sub/deep name.txt"
   n=0
   while [ $n -lt 100 ]; do
      mcopy -i "$img" "$tmp/deep" "::/dir/many/file with long name $n.txt"
      n=$((n + 1))
   done
   ;;
check)
   fsck.fat -n "$img" > "$tmp/log" 2>&1 || { cat "$tmp/log"; exit 1; }
   python3 -c "import sys
sys.stdout.buffer.write(bytes((((5+i)*2654435761)&0xffffffff)>>24 for i in range(4<<20)))" > "$tmp/want"
   mtype -i "$img" ::/out/big.bin > "$tmp/got"
   cmp "$tmp/want" "$tmp/got"
   [ "$(mtype -i "$img" "::/out/Written by the Wii.txt")" = written ]
   [ "$(mtype -i "$img" "::/out/final name.txt")" = renamed ]
   ! mdir -i "$img" ::/hello.txt > /dev/null 2>&1
   echo "sd image: ok"
   ;;
esac
