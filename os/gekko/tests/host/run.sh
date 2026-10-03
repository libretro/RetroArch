#!/bin/sh
# fs/fat.c against images made and checked by dosfstools and mtools,
# and disk/sdspi.c and disk/usbmsc.c against a model SD card and USB
# drive, kernel/exec_image.c's program layouts, rvl/ir.c's pointer and
# rvl/ave_gamma.c's curves.
# Needs a C compiler, mkfs.fat, fsck.fat, mtools and python3.
set -e
cd "$(dirname "$0")"
work=${TMPDIR:-/tmp}/gekko-fat-test
rm -rf "$work"
mkdir -p "$work"
export MTOOLS_SKIP_CHECK=1 LC_ALL=C.UTF-8
${CC:-cc} -O1 -g -Wall -Wextra -fsanitize=address,undefined \
   -I../../include -o "$work/fat_test" fat_test.c ../../fs/fat.c
${CC:-cc} -O1 -g -Wall -Wextra -fsanitize=address,undefined \
   -I../../include -o "$work/sdspi_test" sdspi_test.c ../../disk/sdspi.c \
   ../../fs/fat.c
${CC:-cc} -O1 -g -Wall -Wextra -fsanitize=address,undefined \
   -I../../include -o "$work/usbmsc_test" usbmsc_test.c ../../disk/usbmsc.c \
   ../../fs/fat.c
${CC:-cc} -O1 -g -Wall -Wextra -fsanitize=address,undefined \
   -I../../include -o "$work/exec_image_test" exec_image_test.c \
   ../../kernel/exec_image.c
${CC:-cc} -O1 -g -Wall -Wextra -fsanitize=address,undefined \
   -o "$work/ir_test" ir_test.c ../../rvl/ir.c -lm
${CC:-cc} -O1 -g -Wall -Wextra -fsanitize=address,undefined \
   -o "$work/ave_gamma_test" ave_gamma_test.c ../../rvl/ave_gamma.c -lm
"$work/sdspi_test"
"$work/usbmsc_test"
"$work/exec_image_test"
"$work/ir_test"
"$work/ave_gamma_test"

pattern() { # seed size file
   python3 -c "import sys
s,n=int(sys.argv[1]),int(sys.argv[2])
sys.stdout.buffer.write(bytes((((s+i)*2654435761)&0xffffffff)>>24 for i in range(n)))" "$1" "$2" > "$3"
}

fsck() { # image offset
   if [ "$2" = 0 ]; then part=$1; else
      part=$work/part.img
      dd if="$1" of="$part" bs=512 skip="$2" status=none
   fi
   fsck.fat -n "$part" > "$work/fsck.log" 2>&1 || {
      cat "$work/fsck.log"; echo "FAIL fsck $1"; exit 1; }
}

expect() { # image@@off path seed size [suffix]
   pattern "$3" "$4" "$work/want"
   [ -z "$5" ] || printf '%s' "$5" >> "$work/want"
   mtype -i "$1" "::$2" > "$work/got"
   cmp "$work/want" "$work/got" || { echo "FAIL content $2"; exit 1; }
}

populate() { # image@@off
   i=$1
   printf 'hello\n' > "$work/hello.txt"
   printf 'long\n'  > "$work/long"
   printf 'deep\n'  > "$work/deep"
   printf 'utf\n'   > "$work/utf"
   pattern 7 1000003 "$work/big.bin"
   mcopy -i "$i" "$work/hello.txt" ::/hello.txt
   mcopy -i "$i" "$work/long" "::/A Long File Name.data"
   mcopy -i "$i" "$work/utf" "::/café €.txt"
   mcopy -i "$i" "$work/big.bin" ::/big.bin
   mmd -i "$i" ::/dir ::/dir/sub ::/dir/many
   mcopy -i "$i" "$work/deep" "::/dir/sub/deep name.txt"
   n=0
   while [ $n -lt 300 ]; do
      mcopy -i "$i" "$work/deep" "::/dir/many/file with long name $n.txt"
      n=$((n + 1))
   done
}

one() { # name mkfs-args size-MiB [partition offset] [nofill]
   name=$1; args=$2; mib=$3; off=${4:-0}
   img=$work/$name.img
   rm -f "$img"
   truncate -s "${mib}M" "$img"
   if [ "$off" = 0 ]; then
      mkfs.fat $args "$img" > /dev/null
      mi=$img
   else
      python3 -c "import struct,sys
off,n=int(sys.argv[2]),int(sys.argv[3])
f=open(sys.argv[1],'r+b'); f.seek(446)
f.write(struct.pack('<B3sB3sII',0,b'\\0'*3,0x0c,b'\\0'*3,off,n-off)+bytes(48)+b'\\x55\\xaa')" \
         "$img" "$off" $((mib * 2048))
      mkfs.fat $args --offset "$off" "$img" $(( (mib * 2048 - off) / 2 )) > /dev/null
      mi="$img@@$((off * 512))"
   fi
   populate "$mi"
   "$work/fat_test" "$img" check
   "$work/fat_test" "$img" write
   fsck "$img" "$off"
   expect "$mi" "/new/short.txt" 1 10 appended
   expect "$mi" "/new/Mixed Case Directory/moved.txt" 2 512
   expect "$mi" "/new/Mixed Case Directory/A file with a long name.bin" 3 70000
   expect "$mi" "/new/.dotfile" 4 3
   expect "$mi" "/new/café.txt" 5 4
   expect "$mi" "/new/replaced" 12 300
   expect "$mi" "/new/trunc" 13 1000
   [ "$(mtype -i "$mi" "::/new/Mixed Case Directory/was open")" = "first second" ] ||
      { echo "FAIL was open"; exit 1; }
   expect "$mi" "/new/collision number 11.txt" 111 111
   expect "$mi" "/new/Mixed Case Directory/grown/reused" 9 9
   expect "$mi" "/new/Mixed Case Directory/grown/entry 199 with a name long enough" 199 3
   mdir -i "$mi" -b "::/new/Mixed Case Directory/grown" > "$work/list"
   [ "$(wc -l < "$work/list")" = 134 ] || { echo "FAIL listing"; exit 1; }
   mdir -i "$mi" ::/hello.txt > /dev/null 2>&1 && { echo "FAIL unlink"; exit 1; }
   "$work/fat_test" "$img" stress
   fsck "$img" "$off"
   if [ -z "$5" ]; then
      "$work/fat_test" "$img" fill
      fsck "$img" "$off"
   fi
   echo "$name: ok"
}

truncate -s 64M "$work/spi.img"
mkfs.fat -F 32 -s 1 "$work/spi.img" > /dev/null
printf 'hello\n' > "$work/hello.txt"
mcopy -i "$work/spi.img" "$work/hello.txt" ::/hello.txt
"$work/sdspi_test" "$work/spi.img"
fsck "$work/spi.img" 0
[ "$(mtype -i "$work/spi.img" ::/spi.txt)" = "over spi" ] ||
   { echo "FAIL spi.txt"; exit 1; }
cp "$work/spi.img" "$work/usb.img"
"$work/usbmsc_test" "$work/usb.img"
fsck "$work/usb.img" 0
[ "$(mtype -i "$work/usb.img" ::/usb.txt)" = "over usb" ] ||
   { echo "FAIL usb.txt"; exit 1; }

one fat12 "-F 12" 4
one fat16 "-F 16" 64
one fat32 "-F 32 -s 1" 128
one fat32-part "-F 32 -s 8" 600 2048
one fat16-part "-F 16 -s 4" 64 63
one fat32-64k "-F 32 -s 64" 4200 0 nofill
cp "$work/fat16.img" "$work/unplug.img"
"$work/fat_test" "$work/unplug.img" unplug
echo "all ok"
