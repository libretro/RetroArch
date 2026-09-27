#!/bin/sh
# Cross-check the WaveRT declarations in audio/drivers/wdmks.c against a
# Windows SDK ksmedia.h.
#
# Everything else in that file is checked against the mingw headers by
# samples/audio/wdmks_abi, which runs in CI. The WaveRT types cannot be:
# the mingw ksmedia.h stops at KSPROPERTY_RTAUDIO_GETPOSITIONFUNCTION and
# declares none of the KSRTAUDIO_* structures, and PortAudio does not
# carry them either - it takes them from the SDK. So this takes the SDK
# header as an argument instead of shipping one, and is run by hand when
# one is to hand:
#
#   tools/wdmks_wavert_check.sh /path/to/ksmedia.h
#
# Every size, every field offset that is read, and every property
# ordinal. Nothing runs; the assertions are the test.
set -eu
[ $# -eq 1 ] || { echo "usage: $0 <path to a Windows SDK ksmedia.h>" >&2; exit 2; }
SDK="$1"
CC="${CC:-x86_64-w64-mingw32-gcc}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# The SDK headers are shipped with CRLF; mingw minds in some places.
sed 's/\r$//' "$SDK" > "$TMP/sdk_ksmedia.h"
sed "s|@SDK@|$TMP/sdk_ksmedia.h|" "$HERE/tools/wdmks_wavert_check.c.in" > "$TMP/check.c"

$CC -std=gnu99 -w -Wno-unused-const-variable \
    -DRARCH_INTERNAL -DHAVE_THREADS \
    -I"$HERE" -I"$HERE/libretro-common/include" -c -o /dev/null "$TMP/check.c"
echo "WaveRT: every size, offset and ordinal matches $SDK"
