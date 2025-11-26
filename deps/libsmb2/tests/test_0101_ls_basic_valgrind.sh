#!/bin/sh

. ./functions.sh

echo "basic ls test with valgrind"

echo -n "Testing prog_ls on root of share ... "
libtool --mode=execute valgrind --leak-check=full --error-exitcode=77 ./prog_ls "${TESTURL}/" >/dev/null 2>&1 || failure
success

echo -n "Testing prog_ls on a directory that does not exist ... "
./prog_rmdir "${TESTURL}/testdir" > /dev/null
libtool --mode=execute valgrind --leak-check=full --error-exitcode=77 ./prog_ls "${TESTURL}/testdir" >/dev/null 2>&1 
expr $? "==" "77" >/dev/null && failure
success

echo -n "Testing prog_ls on a directory that does exist ... "
./prog_mkdir "${TESTURL}/testdir" > /dev/null
libtool --mode=execute valgrind --leak-check=full --error-exitcode=77 ./prog_ls "${TESTURL}/testdir" >/dev/null 2>&1 || failure
./prog_rmdir "${TESTURL}/testdir" > /dev/null
success

exit 0
