#!/bin/sh

. ./functions.sh

echo "basic ls test with valgrind and [c|m]alloc errors"

echo libtool --mode=execute ltrace -l \* ./prog_ls "${TESTURL}/"
NUM_CALLS=`libtool --mode=execute ltrace -l \* ./prog_ls "${TESTURL}/" 2>&1 >/dev/null | grep "[c|m]alloc" |wc -l`
echo "Num" $NUM_CALLS

for IDX in `seq 1 $NUM_CALLS`; do
    # check for failing due to a signal (segv/abrt/...)
    echo -n "Testing prog_ls for crashes at #${IDX} ..."
    ALLOC_FAIL=${IDX} ./prog_ls "${TESTURL}/" 2>&1 >/dev/null 2>valgrind.out
    expr $? ">=" "128" >/dev/null && failure
    success

    # check for memory leaks in error paths
    echo -n "Testing prog_ls for memory leaks at #${IDX} ..."
    ALLOC_FAIL=${IDX} libtool --mode=execute valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=77 ./prog_ls "${TESTURL}/" >/dev/null 2>valgrind.out
    expr $? "==" "77" >/dev/null && failure
    success
done

exit 0
