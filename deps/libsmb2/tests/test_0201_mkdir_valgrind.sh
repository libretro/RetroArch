#!/bin/sh

. ./functions.sh

echo "mkdir test with valgrind"

echo -n "Testing mkdir on root of share ... "
libtool --mode=execute valgrind --leak-check=full --error-exitcode=1 ./prog_mkdir "${TESTURL}/testdir" >/dev/null 2>&1 || failure
success

echo -n "Testing rmdir on root of share ... "
libtool --mode=execute valgrind --leak-check=full --error-exitcode=1 ./prog_rmdir "${TESTURL}/testdir" >/dev/null 2>&1 || failure
success

exit 0
