#!/bin/sh
# The size and stack frame of the frontend's hottest functions, against
# a checked-in budget.
#
# Code size is I-cache footprint on the path that runs every frame, and
# the stack frame is what the prologue touches before any work happens.
# Neither is visible to a test: a change that doubles either passes
# every lane in the suite and shows up only as a slower frame on
# someone's machine. This is the check that notices.
#
# The numbers come from the release objects, so run it after a plain
# `make`. The budget is deliberately loose - a quarter over what the
# tree measures today - because compilers differ and the point is to
# catch a step change, not to freeze a byte count. When a change means
# to grow one of these, the reference moves with it in the same commit,
# which is exactly the moment to say why in the message.
#
# The functions, and why each is here:
#   runloop_iterate            the frame's orchestration
#   video_driver_frame         the frontend's hottest function by its
#                              own comment; also the largest frame
#   video_thread_frame         the handover, on the paced path
#   audio_driver_sample_batch  called once per core frame, more with
#                              per-sample cores
set -eu

cd "$(dirname "$0")/../../.."
ref=".github/workflows/scripts/hot-function-budget.ref"
obj="obj-unix/release"
fail=0

if [ ! -d "$obj" ]; then
   echo "no release objects in $obj; run make first" >&2
   exit 1
fi

measure() {
   o="$1"; fn="$2"
   sz=$(nm -S "$o" 2>/dev/null | grep -w "$fn" | head -1 | awk '{print $2}')
   [ -n "$sz" ] || { echo "SKIP  $fn (not in $o)"; return; }
   size=$(printf '%d' "0x$sz")
   # mawk has no strtonum(); the hex comes back as text and the shell
   # converts it.
   frame_hex=$(objdump -d --no-show-raw-insn "$o" 2>/dev/null | awk -v f="$fn" '
      $0 ~ "<"f">:" { in_fn = 1; next }
      in_fn && /^$/ { exit }
      in_fn && /sub +\$0x[0-9a-f]+,%rsp/ {
         match($0, /0x[0-9a-f]+/); print substr($0, RSTART + 2, RLENGTH - 2); exit }')
   if [ -n "$frame_hex" ]; then
      frame=$(printf '%d' "0x$frame_hex")
   else
      frame=0
   fi

   want_size=$(awk -v f="$fn" '$1 == f { print $2 }' "$ref")
   want_frame=$(awk -v f="$fn" '$1 == f { print $3 }' "$ref")
   [ -n "$want_size" ] || { echo "SKIP  $fn (no reference)"; return; }

   max_size=$(( want_size + want_size / 4 ))
   max_frame=$(( want_frame + want_frame / 4 + 64 ))
   status="ok   "
   if [ "$size" -gt "$max_size" ] || [ "$frame" -gt "$max_frame" ]; then
      status="FAIL "
      fail=1
   fi
   printf '%s %-28s size %6d (budget %6d)  frame %5d (budget %5d)\n' \
      "$status" "$fn" "$size" "$max_size" "$frame" "$max_frame"
}

echo "== hot function budget =="
measure "$obj/runloop.o"                      runloop_iterate
measure "$obj/gfx/video_driver.o"             video_driver_frame
measure "$obj/gfx/video_thread_wrapper.o"     video_thread_frame
measure "$obj/audio/audio_driver.o"           audio_driver_sample_batch

if [ "$fail" = 1 ]; then
   echo
   echo "A hot function grew past its budget. If that is deliberate, move"
   echo "the number in $ref in the same commit and say why."
   exit 1
fi
