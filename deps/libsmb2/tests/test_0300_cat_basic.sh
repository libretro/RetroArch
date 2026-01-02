#!/bin/sh

. ./functions.sh

echo "basic cat test"

../utils/smb2-cp ./prog_cat.c "${TESTURL}/CAT"

echo -n "Testing prog_cat on root of share/CAT ... "
./prog_cat "${TESTURL}/CAT" > /dev/null || failure
success

exit 0
