#!/bin/sh

. ./functions.sh

echo "DCE/RPC coder tests"

./smb2-dcerpc-coder-test || failure
success

exit 0
