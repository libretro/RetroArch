#!/bin/sh
# Compiles the QNX input driver and joypad driver without the QNX SDK.
#
# RetroArch's QNX port is the BlackBerry 10 one (pkg/qnx: QCC, the BPS
# and screen libraries). Its SDK is not something a CI machine can
# fetch, so nothing built these files, and they broke unseen: the input
# driver asked for a name it never declared and did not compile.
#
# This compiles them - syntax and declarations only, nothing is linked
# or run - with the defines pkg/qnx builds with, against stand-ins for
# the SDK's headers (tools/qnx_standin): the types as opaque pointers,
# the constants as numbers, the calls without their arguments. So it
# catches what does not depend on the SDK being real - an undeclared
# name, a call to a function that is not there, a driver table that
# does not fit - and nothing that does.
#
# A type, constant or call of the SDK's that the drivers start to use
# is added to tools/qnx_standin/qnx_standin.h.
#
# Covered: input/drivers/qnx_input.c and
# input/drivers_joypad/qnx_joypad.c, in the order the port's one
# translation unit has them (griffin), with HAVE_BB10. Not covered: the
# variant without HAVE_BB10, which calls a function only that define
# provides; and qnx_ctx.c, platform_qnx.c and alsa_qsa.c, which use the
# SDK's structures by their fields.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
CC=${CC:-gcc}
tu=$(mktemp --suffix=.c)
log=$(mktemp)
trap 'rm -f "$tu" "$log"' EXIT

# The first lines stand for what the unity build has included by the
# time it reaches these two files.
cat > "$tu" <<TU
#include <errno.h>
#include <string.h>
#include "$root/input/input_keymaps.h"
#include "$root/verbosity.h"
#include "$root/input/drivers/qnx_input.c"
#include "$root/input/drivers_joypad/qnx_joypad.c"
TU

defs="-D__QNX__ -DHAVE_BB10 -DRARCH_INTERNAL -DRARCH_MOBILE -DHAVE_GRIFFIN
 -DHAVE_THREADS -DHAVE_MENU -DHAVE_RGUI -DHAVE_XMB -DHAVE_MATERIALUI
 -DHAVE_OVERLAY -DHAVE_NETWORKING -DHAVE_ZLIB -DHAVE_DYNAMIC -DHAVE_DYLIB
 -DHAVE_CONFIGFILE -DHAVE_PATCH -DHAVE_REWIND -DHAVE_CHEATS
 -DHAVE_SCREENSHOTS -DHAVE_LIBRETRODB -DHAVE_LANGEXTRA
 -DHAVE_VIDEO_FILTER -DHAVE_DSP_FILTER -D__LIBRETRO__"

if ! $CC -fsyntax-only -std=gnu99 \
      -Werror=implicit-function-declaration \
      -Werror=incompatible-pointer-types \
      $defs -isystem tools/qnx_standin \
      -I. -Ilibretro-common/include -Ideps \
      "$tu" > "$log" 2>&1
then
   grep -E 'error|note: ' "$log" | sed "s#$root/##g" | head -40
   echo "FAIL qnx_standin_check: the QNX input drivers do not compile"
   exit 1
fi
echo "PASS qnx_standin_check (qnx_input.c, qnx_joypad.c; HAVE_BB10; against stand-ins for the SDK)"
