#!/bin/sh

interrupt_count=0

trap 'if [ $interrupt_count -eq 5 ]; then exit 0; else interrupt_count=$(($interrupt_count + 1)); fi' INT

echo ===== START: `date` =====
while true; do
  netcat -p 4405 -l
  if [ $? -ne 0 ]; then
    break
  fi
done
echo ===== END: `date` =====
