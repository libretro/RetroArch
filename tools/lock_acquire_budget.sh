#!/bin/sh
# Lock acquisitions on the paths that must not take one, against a
# checked-in count.
#
# A mutex on a path that runs every frame or every audio pass costs more
# than the critical section: the acquisition itself is a write, so every
# pass pulls the lock's cache line away from whichever thread had it,
# and a reader that only wants to look at a value pays the same as a
# writer. Several of these paths carry the value in atomics for exactly
# that reason - the statistics the overlay reads, the viewport an input
# driver asks for on every poll, the audio thread's two loop conditions
# - and nothing stops the next change from putting an slock_lock back.
# A reinstated acquisition breaks no test: the suite passes, and it
# shows up as a slower frame on someone's machine. This is the check
# that notices.
#
# The count comes from the release objects, so run it after a plain
# `make`. It is read off the relocations inside each function's own
# block, which means a lock taken through a helper the compiler inlined
# is counted too - and one taken through a call the compiler kept is
# not. So a clean run means "no acquisition this can see", not "none
# exists": a floor, like tools/video_thread_state_check.py, and worth
# having because every count it reports is a real one.
#
# Some of these are not zero, and are not meant to be. viewport_info
# takes the lock when the viewport it reports has changed - a resize -
# and not on the polls in between; the audio loop takes it on the pass
# that parks or leaves; the gather takes it once it has something to
# retire. What matters is that none of them grows. When a change means
# to move one of these counts, the reference moves with it in the same
# commit, which is the moment to say why in the message.
set -eu

cd "$(dirname "$0")/.."
ref="tools/lock_acquire_budget.ref"
obj="obj-unix/release"
fail=0

if [ ! -d "$obj" ]; then
   echo "no release objects in $obj; run make first" >&2
   exit 1
fi

# The counting reads x86-64 relocation names. On another architecture it
# would find none of them and report zero for everything, which is a
# pass that proves nothing - so say so instead.
if ! objdump -dr "$obj/runloop.o" 2>/dev/null | grep -q 'R_X86_64_'; then
   echo "this reads x86-64 relocations; nothing to count in $obj" >&2
   exit 77
fi

# Acquisitions inside FN's own block in OBJ, by the relocation that
# names each call's target: the objects are not linked, so the
# disassembly's call targets are offsets and the symbol is only on the
# relocation line.
count_in() {
   objdump -dr --no-show-raw-insn "$1" 2>/dev/null | awk -v f="$2" '
      $0 ~ "<"f">:"                          { in_fn = 1; next }
      in_fn && /^$/                          { exit }
      in_fn && /R_X86_64_[A-Z0-9]*[ \t]+slock_lock-/     { c++ }
      in_fn && /R_X86_64_[A-Z0-9]*[ \t]+slock_try_lock-/ { c++ }
      END { print c + 0 }'
}

measure() {
   o="$obj/$1"; fn="$2"
   if [ ! -f "$o" ]; then
      echo "SKIP  $fn (no $o)"
      return
   fi
   if ! objdump -d "$o" 2>/dev/null | grep -q "<$fn>:"; then
      echo "SKIP  $fn (not in $1)"
      return
   fi
   have=$(count_in "$o" "$fn")
   want=$(awk -v f="$fn" '$1 == f { print $2 }' "$ref")
   if [ -z "$want" ]; then
      echo "SKIP  $fn (no reference)"
      return
   fi
   status="ok   "
   if [ "$have" -gt "$want" ]; then
      status="FAIL "
      fail=1
   fi
   printf '%s %-30s %d acquisition(s) (allowed %d)\n' \
      "$status" "$fn" "$have" "$want"
}

echo "== lock acquisitions on the lock-free paths =="

# The wrapper's readers. Every one of these is called from another
# thread while the video thread holds thr->lock for the frame handoff
# and the ring, and every one of them reads a published snapshot.
measure gfx/video_thread_wrapper.o  video_thread_presenter_stats
measure gfx/video_thread_wrapper.o  video_thread_latency_stats
measure gfx/video_thread_wrapper.o  video_thread_pacing_stats
measure gfx/video_thread_wrapper.o  video_thread_swap_count
measure gfx/video_thread_wrapper.o  thread_get_refresh_rate
measure gfx/video_thread_wrapper.o  video_thread_viewport_info
# The handoff itself: one region, and the ring is what it is for.
measure gfx/video_thread_wrapper.o  video_thread_frame

# The audio thread's loop: the lock belongs to the pass that parks or
# leaves, not to the passes that play audio.
measure audio/audio_thread_wrapper.o  audio_thread_loop

# Runs every frame and returns on two atomic loads while there is
# nothing in flight.
measure libretro-common/queues/task_queue.o  retro_task_threaded_gather

# The frame and the batch. Neither takes a lock today, and the reason
# to record that is how much runs under them.
measure gfx/video_driver.o    video_driver_frame
measure audio/audio_driver.o  audio_driver_sample_batch
measure runloop.o             runloop_iterate

if [ "$fail" = 1 ]; then
   echo
   echo "A path that is meant to be lock-free took a lock. If that is"
   echo "deliberate, move the count in $ref in the same commit and say"
   echo "why."
   exit 1
fi
