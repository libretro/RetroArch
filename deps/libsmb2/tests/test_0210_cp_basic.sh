#!/bin/sh

. ./functions.sh

echo "basic cp read/write test"

echo -n "Testing cp to root of share ... "
rm testfile2 2>/dev/null
echo "HappyPenguins!" > testfile
../utils/smb2-cp testfile "${TESTURL}/testfile" > /dev/null || failure
success

echo -n "Testing cp from root of share ... "
../utils/smb2-cp "${TESTURL}/testfile"  testfile2 > /dev/null || failure
success

echo -n "Verify file content match ... "
cmp testfile testfile2 || failure
success

echo -n "Testing cp from a file that does not exist ... "
../utils/smb2-cp "${TESTURL}/testfile-not-exist"  testfile2 2>/dev/null && failure
success

exit 0
