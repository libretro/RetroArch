#!/bin/sh
# Every stack frame in the graphics objects, against the tree's limit.
#
# CODING-GUIDELINES puts a ceiling on a function's stack frame - two
# kilobytes inside libretro-common, four outside it - because the
# threads this code runs on do not all get a desktop's stack, and a
# frame that overruns one corrupts whatever is under it rather than
# failing. Nothing checked it: the frames this sweep was written for
# were twelve kilobytes, in four font paths and an overlay batch, and
# they had been there for years.
#
# A large local array is invisible in review and to every other lane -
# it compiles, it runs on a machine with eight megabytes of stack, and
# it is only a problem on the platform with the smallest one. This is
# the check that notices.
#
# Reads the release objects, so run it after a plain `make`. Functions
# allowed to exceed the ceiling are listed in stack-frame-sweep.ref
# with the size they measure; a change that grows one moves its line in
# the same commit, which is the moment to say why in the message.
set -e
cd "$(dirname "$0")/../../.."

OBJDIR=${OBJDIR:-obj-unix/release}
ref="$(dirname "$0")/stack-frame-sweep.ref"
limit=${LIMIT:-4096}
fail=0

if [ ! -d "$OBJDIR/gfx" ]; then
   echo "skip: no objects in $OBJDIR (run make first)"
   exit 0
fi

# A large frame is built by repeated probes of a page at a time, so the
# allocation is the sum of the subtractions a function makes, not the
# largest one.
for o in $(find "$OBJDIR/gfx" -name '*.o' | sort); do
   objdump -d --no-show-raw-insn "$o" 2>/dev/null | awk -v obj="$o" -v lim="$limit" '
      /^[0-9a-f]+ <.*>:/ {
         if (fn != "" && total > lim)
            printf "%s %s %d\n", obj, fn, total
         fn = $2; sub(/^</, "", fn); sub(/>:$/, "", fn)
         total = 0
         next
      }
      /sub +\$0x[0-9a-f]+,%rsp/ {
         # The hex is not at a fixed field: the address column shifts
         # them, so take it out of the line rather than by index.
         line = $0
         sub(/^.*sub +\$0x/, "", line)
         sub(/,%rsp.*$/, "", line)
         hex = line
         # mawk has no strtonum(); fold the hex by hand, and ignore the
         # sign-extended constants an add-by-subtract produces.
         n = 0; ok = 1
         for (i = 1; i <= length(hex); i++) {
            c = substr(hex, i, 1)
            p = index("0123456789abcdef", c)
            if (p == 0) { ok = 0; break }
            n = n * 16 + (p - 1)
         }
         if (ok && n < 4194304) total += n
         next
      }
      END {
         if (fn != "" && total > lim)
            printf "%s %s %d\n", obj, fn, total
      }
   '
done > /tmp/stack-frame-sweep.$$ || true

echo "== stack frames over $limit bytes =="
while read -r obj fn size; do
   [ -n "$fn" ] || continue
   want=$(awk -v f="$fn" '$1 == f { print $2 }' "$ref" 2>/dev/null)
   if [ -z "$want" ]; then
      echo "FAIL  $fn ($size bytes) in $(basename "$obj")"
      fail=1
   elif [ "$size" -gt "$want" ]; then
      echo "FAIL  $fn grew to $size, recorded $want"
      fail=1
   else
      echo "ok    $fn ($size bytes, recorded $want)"
   fi
done < /tmp/stack-frame-sweep.$$
rm -f /tmp/stack-frame-sweep.$$

if [ "$fail" = 1 ]; then
   echo
   echo "A function's stack frame is over the ceiling in CODING-GUIDELINES."
   echo "Move the array off the frame, or record it in $ref with why."
   exit 1
fi
echo "ok    nothing else over $limit bytes"
exit 0
