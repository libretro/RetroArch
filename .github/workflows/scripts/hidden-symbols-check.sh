#!/bin/sh
# Definitions that share their names with system libraries must stay
# hidden. Exported, they win the lookup: libssl's MD5_Init lands in
# libretro-common's copy, whose MD5_CTX is a different size, and so do
# the MD5 calls of a core bundling LibreSSL. Whether the executable
# exports them depends on what it links, so this reads the objects,
# where the visibility is set no matter what the link pulls in.
#
# Reads the release objects, so run it after a plain `make`.
set -e
cd "$(dirname "$0")/../../.."

OBJDIR=${OBJDIR:-obj-unix/release}
LRC="$OBJDIR/libretro-common"
fail=0

objs="$LRC/utils/md5.o"
if grep -q '^HAVE_RETROSMB = 1' config.mk 2>/dev/null; then
   objs="$objs $LRC/net/net_krb5.o"
fi

for o in $objs; do
   if [ ! -f "$o" ]; then
      echo "FAIL: $o not built (run make first)"
      fail=1
      continue
   fi
   # Num Value Size Type Bind Vis Ndx Name
   exported=$(readelf -sW "$o" | awk '$4 == "FUNC" && $5 == "GLOBAL" && $6 != "HIDDEN" && $7 != "UND" { print $8 }')
   if [ -n "$exported" ]; then
      echo "FAIL: $o exports:" $exported
      fail=1
   else
      echo "ok: $o"
   fi
done

exit $fail
