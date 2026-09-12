#!/bin/sh
# Asserts that the seqlock in retro_atomic_test.c lowers to real
# barriers on a weakly-ordered target.
#
# A host run cannot check this.  On x86_64 the hardware supplies the
# ordering the code forgot to ask for, and qemu-aarch64 does not model
# reordering either: building this file with the release fence deleted
# still passes under both.  What does change is the emitted code, so
# that is what this checks.
#
# Expected, with the seqlock written as it is:
#
#   seqlock_writer  dmb ish    the release fence between the odd stamp
#                              and the field stores
#                   stlr       the two stamps (plus writer_done)
#                   str        the fields, relaxed, no barrier
#   seqlock_reader  dmb ishld  the acquire fence after the field loads
#                   ldar       the two stamp reads
#                   ldr        the fields, relaxed, no barrier
#
# Skips cleanly when no aarch64 cross compiler is installed, so it can
# sit in a workflow that also runs on machines without one.

CROSS=${CROSS:-aarch64-linux-gnu}
CC_X=$CROSS-gcc
OBJDUMP=$CROSS-objdump
HERE=$(dirname "$0")
OBJ=$(mktemp /tmp/retro_atomic_barriers.XXXXXX.o)
rc=0

if ! command -v "$CC_X" >/dev/null 2>&1 || ! command -v "$OBJDUMP" >/dev/null 2>&1; then
   echo "skip  barrier check: no $CROSS toolchain"
   rm -f "$OBJ"
   exit 0
fi

if ! "$CC_X" -std=gnu99 -O2 -DHAVE_THREADS \
      -I"$HERE/../../../include" -c "$HERE/retro_atomic_test.c" -o "$OBJ"; then
   echo "FAIL  barrier check: cross compile failed"
   rm -f "$OBJ"
   exit 1
fi

want() { # function, instruction, human name
   n=$("$OBJDUMP" -d "$OBJ" --disassemble="$1" 2>/dev/null \
         | grep -cE "[[:space:]]$2([[:space:]]|$)")
   if [ "$n" -lt 1 ]; then
      echo "FAIL  $1: no $3 ($2) in the emitted code"
      rc=1
   else
      echo "ok    $1: $3"
   fi
}

want seqlock_writer "dmb"  "release fence"
want seqlock_writer "stlr" "release stores for the stamps"
want seqlock_writer "str"  "plain stores for the relaxed fields"
want seqlock_reader "dmb"  "acquire fence"
want seqlock_reader "ldar" "acquire loads for the stamps"
want seqlock_reader "ldr"  "plain loads for the relaxed fields"

rm -f "$OBJ"
exit $rc
