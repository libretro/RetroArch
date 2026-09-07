#!/bin/bash

. ./functions.sh

echo "test credits with SMB2 v2.0.2"

echo -n "Discovering file on root of share ... "
FILENAME=$(./prog_ls "${TESTURL}" | head -2 | tail -1 | cut -f1 -d" ")
echo "${FILENAME}"

echo -n "Testing metastat on root of share ... "
pids=()
for jj in $(seq 10)
do
        for ii in $(seq 2000)
        do
                echo "$FILENAME"
        done | xargs ./metastat-0202-censored "${TESTURL}" > /dev/null &
        pids[$jj]=$!
done
for pid in ${pids[*]}
do
        wait $pid || { wait; failure; }
done
success
