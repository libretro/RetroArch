#!/bin/sh
# Asserts that retro_atomic_thread_fence_seq_cst lowers to a locked or on
# x86, not an mfence, with every GCC-family compiler on the host: GCC
# picks the locked or for a seq_cst fence by itself, Clang emits mfence,
# which costs several times as much on most x86 cores.
#
# Skips cleanly on a host that is not x86, or with no compiler to try.

HERE=$(dirname "$0")
OBJ=$(mktemp /tmp/retro_atomic_x86_fence.XXXXXX.o)
rc=0
tried=0

case "$(uname -m)" in
   x86_64|amd64|i386|i486|i586|i686) ;;
   *) echo "skip  x86 fence check: not an x86 host"; rm -f "$OBJ"; exit 0 ;;
esac

for cc in gcc clang; do
   command -v "$cc" >/dev/null 2>&1 || continue
   for std in gnu99 c11; do
      if ! "$cc" -std=$std -O2 -I"$HERE/../../../include" -c "$HERE/retro_atomic_test.c" -o "$OBJ" 2>/dev/null; then
         echo "FAIL  x86 fence check: $cc -std=$std did not build"
         rc=1
         continue
      fi
      tried=1
      dis=$(objdump -d --no-show-raw-insn "$OBJ" --disassemble=seq_cst_fence_shape 2>/dev/null)
      if echo "$dis" | grep -q "mfence"; then
         echo "FAIL  seq_cst_fence_shape ($cc -std=$std): mfence"
         rc=1
      elif ! echo "$dis" | grep -qE "lock.*or|lock$"; then
         echo "FAIL  seq_cst_fence_shape ($cc -std=$std): no locked or"
         rc=1
      else
         echo "ok    seq_cst_fence_shape ($cc -std=$std): a locked or"
      fi
   done
done

[ $tried -eq 1 ] || echo "skip  x86 fence check: no compiler found"
rm -f "$OBJ"
exit $rc
