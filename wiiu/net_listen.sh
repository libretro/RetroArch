#!/bin/sh

if [ -z $1 ] ; then
   echo
   echo "usage: $0 <WiiU-ip>"
   echo
   exit 0
fi

interrupt_count=0

trap 'if [ $interrupt_count -eq 20 ]; then exit 0; else interrupt_count=$(($interrupt_count + 1)); fi' INT

while true; do echo; echo ========= `date` =========; echo; netcat -p 4405 -l $1; done
