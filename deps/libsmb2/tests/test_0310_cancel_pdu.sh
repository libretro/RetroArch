#!/bin/sh

. ./functions.sh

echo "basic test that we can cancel a pdu in flight"

../utils/smb2-cp ./prog_cat.c "${TESTURL}/CAT"

echo -n "Testing prog_cat_cancel on root of share/CAT ... "
./prog_cat_cancel "${TESTURL}/CAT" > /dev/null || failure
success

exit 0
