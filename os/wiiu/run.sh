#!/bin/sh

if [ -z $1 ] ; then
   echo
   echo "usage: $0 <rpx>"
   echo
   exit 0
fi

wiiload $1

echo ===== START: `date` =====
  netcat -p 4405 -l
echo ===== END: `date` =====
