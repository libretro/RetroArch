#!/bin/sh

. ./functions.sh

echo "basic cp read/write test with valgrind and socket errors"

echo "Testing cp to root of share"
rm testfile2 2>/dev/null
echo "HappyPenguins!" > testfile
NUM_CALLS=`libtool --mode=execute strace ../utils/smb2-cp testfile "${TESTURL}/testfile" 2>&1 >/dev/null | grep readv |wc -l`

for IDX in `seq 1 $NUM_CALLS`; do
    echo -n "Testing cp to root of share with socket failure at #${IDX} ..."
    READV_CLOSE=${IDX} LD_PRELOAD=./ld_sockerr.so libtool --mode=execute valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=77 ../utils/smb2-cp testfile "${TESTURL}/testfile" >/dev/null 2>valgrind.out
    expr $? "==" "77" >/dev/null && failure
    success
done

echo "Testing cp from root of share"
NUM_CALLS=`libtool --mode=execute strace ../utils/smb2-cp "${TESTURL}/testfile" testfile 2>&1 >/dev/null | grep readv |wc -l`

for IDX in `seq 1 $NUM_CALLS`; do
    echo -n "Testing cp to root of share with socket failure at #${IDX} ..."
    READV_CLOSE=${IDX} LD_PRELOAD=./ld_sockerr.so libtool --mode=execute valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=77 ../utils/smb2-cp "${TESTURL}/testfile" testfile >/dev/null 2>valgrind.out
    expr $? "==" "77" >/dev/null && failure
    success
done

exit 0
