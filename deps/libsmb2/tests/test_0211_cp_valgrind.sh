#!/bin/sh

. ./functions.sh

echo "basic cp read/write test with valgrind"

echo -n "Testing cp to root of share ... "
rm testfile2 2>/dev/null
echo "HappyPenguins!" > testfile
libtool --mode=execute valgrind --leak-check=full --error-exitcode=77 ../utils/smb2-cp testfile "${TESTURL}/testfile" >/dev/null 2>&1 || failure
success

echo -n "Testing cp from root of share ... "
libtool --mode=execute valgrind --leak-check=full --error-exitcode=77 ../utils/smb2-cp "${TESTURL}/testfile"  testfile2 >/dev/null 2>&1 || failure
success

echo -n "Verify file content match ... "
cmp testfile testfile2 || failure
success

echo -n "Testing cp from a file that does not exist ... "
libtool --mode=execute valgrind --leak-check=full --error-exitcode=77 ../utils/smb2-cp "${TESTURL}/testfile-not-exist"  testfile2  >/dev/null 2>valgrind.out
expr $? "==" "77" >/dev/null && failure
success

exit 0
