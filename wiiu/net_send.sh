#!/bin/sh

if [ -z $1 ] ; then
   echo
   echo "usage: $0 <WiiU-ip> <elf>"
   echo
   exit 0
fi

export WIILOAD=tcp:$1
rm $2.stripped -rf
powerpc-eabi-strip $2 -o $2.stripped
wiiload $2.stripped
