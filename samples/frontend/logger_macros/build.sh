#!/bin/sh
# Logger-macro capture harness: compiles verbosity.h under the four
# HAVE_LOGGER configurations (ORBIS x IS_SALAMANDER) against
# capturing sinks and runs each. The ORBIS builds use the stubbed
# debugnet header beside this script.
set -eu
cd "$(dirname "$0")"
CC="${CC:-gcc}"
INC="-I../../../libretro-common/include -I../../.."
mkdir -p dbgnet_stub
cat > dbgnet_stub/debugnet.h <<'HDR'
#ifndef DEBUGNET_H
#define DEBUGNET_H
#define DEBUGNET_NONE 0
#define DEBUGNET_INFO 1
#define DEBUGNET_ERROR 2
#define DEBUGNET_DEBUG 3
void debugNetPrintf(int level, const char *fmt, ...);
#endif
HDR
for CFG in "" "-DIS_SALAMANDER" "-DORBIS -Idbgnet_stub" \
           "-DIS_SALAMANDER -DORBIS -Idbgnet_stub"; do
   $CC -std=gnu99 -Wall -Werror -g $INC $CFG \
       -o logger_macros_test logger_macros_test.c
   ./logger_macros_test >/dev/null
   echo "ok: ${CFG:-plain HAVE_LOGGER}"
done
echo "built samples/frontend/logger_macros (all four configurations green)"
