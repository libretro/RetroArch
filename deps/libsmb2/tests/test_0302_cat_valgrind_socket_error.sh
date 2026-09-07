#!/bin/sh

. ./functions.sh

echo "basic cat test with valgrind and session errors"

../utils/smb2-cp ./prog_cat.c "${TESTURL}/CAT"

NUM_CALLS=`libtool --mode=execute strace ./prog_cat "${TESTURL}/CAT" 2>&1 >/dev/null | grep readv |wc -l`

for IDX in `seq 1 $NUM_CALLS`; do
    echo -n "Testing prog_cat on root of share with socket failure at #${IDX} ..."
    READV_CLOSE=${IDX} LD_PRELOAD=./ld_sockerr.so libtool --mode=execute valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./prog_cat "${TESTURL}/CAT" >/dev/null 2>valgrind.out || failure

    success
done

exit 0
