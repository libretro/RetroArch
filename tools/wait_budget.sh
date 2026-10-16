#!/bin/sh
# Calls that wait on the video thread, against a checked-in count.
#
# A main-thread wait for the video thread is a stall on every path it
# sits on, and a deadlock waiting for the next change that makes the
# video thread wait back. They go one by one, handing work over
# instead; this keeps a removed one from returning and a new one from
# arriving unnoticed. A file's count may only go down: when a change
# removes a wait, the reference goes down with it in the same commit.
# Files not listed may have none.
set -eu

cd "$(dirname "$0")/.."
ref="tools/wait_budget.ref"
fail=0
seen=""

for f in $(grep -rlE 'video_thread_(wait_idle|run_blocking)[[:space:]]*\(' \
      --include='*.c' --include='*.cpp' --include='*.m' --include='*.h' \
      . | sed 's|^\./||' | grep -vE '^(samples|tools|libretro-common)/' \
      | sort); do
   case "$f" in
      gfx/video_thread_wrapper.[ch]) continue ;;
   esac
   n=$(grep -cE 'video_thread_(wait_idle|run_blocking)[[:space:]]*\([^)]*\)[[:space:]]*;' "$f" || true)
   [ "$n" -eq 0 ] && continue
   allowed=$(awk -v f="$f" '$1 == f { print $2 }' "$ref")
   [ -z "$allowed" ] && allowed=0
   seen="$seen $f"
   if [ "$n" -gt "$allowed" ]; then
      echo "FAIL $f: $n waits on the video thread, $allowed allowed"
      fail=1
   elif [ "$n" -lt "$allowed" ]; then
      echo "FAIL $f: $n waits on the video thread, down from $allowed: lower $ref"
      fail=1
   else
      echo "ok   $f: $n"
   fi
done

# A listed file that has none left
for f in $(awk '!/^#/ && NF { print $1 }' "$ref"); do
   case " $seen " in
      *" $f "*) ;;
      *) echo "FAIL $f: no waits left, drop it from $ref"; fail=1 ;;
   esac
done

exit $fail
