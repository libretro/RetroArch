#!/usr/bin/env python3
"""Test that a ring drain on a thread holding the one GL context ends."""
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
hw = (root / "gfx/video_thread_hw.c").read_text()
wrapper = (root / "gfx/video_thread_wrapper.c").read_text()


def function(source, name, start=0):
    match = re.search(r"^(?:static )?(?:VIDEO_NOINLINE )?(?:void|bool|hw_ring_t \*)[ \t]*"
                      + name + r"\(thread_video_t \*thr(?:, bool video_thread)?\)\n\{",
                      source[start:], re.M)
    assert match, name
    begin = start + match.start()
    end = source.index("{", begin) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[begin:end]


fixture = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int bool;
#define true 1
#define false 0
#define VIDEO_NOINLINE
#define HW_CTX_FREE 0
#define HW_CTX_MAIN 1
#define HW_CTX_VIDEO 2
#define VIDEO_THREAD_RING_PENDING_OF(st) ((st) & 3)
#define VIDEO_THREAD_RING_BUSY_OF(st) (((st) >> 2) & 1)
typedef struct { int handoff, ctx_owner, ctx_free, ctx_turns, turns_at_release; } hw_ring_t;
typedef struct { struct { hw_ring_t *hw_ring; int hw_handoff, state, ring; } frame;
   int self; } thread_video_t;
static thread_video_t thr;
static hw_ring_t ring;
static int current, caller, waits;
#define FAIL(msg) do { fprintf(stderr, "FAIL %s\n", msg); exit(1); } while (0)
static int retro_atomic_load_acquire_int(int *v) { return *v; }
static void retro_atomic_store_release_int(int *v, int n) { *v = n; }
static int retro_atomic_cas_int(int *v, int old, int n)
{ if (*v != old) return 0; *v = n; return 1; }
static void retro_atomic_fetch_add_int(int *v, int n) { *v += n; }
static void retro_eventcount_notify(int *v) { (void)v; }
static int retro_eventcount_prepare_wait(int *v) { (void)v; return 0; }
static void retro_eventcount_cancel_wait(int *v) { (void)v; }
static void video_context_driver_release_current(void)
{ if (current != caller) FAIL("release on a thread not holding it"); current = 0; }
static void video_context_driver_bind_hw_render(int core)
{ if (current || core != (caller == HW_CTX_MAIN)) FAIL("bad bind"); current = caller; }
static bool video_thread_is_self(thread_video_t *t) { return t->self; }
static int video_thread_ring_load(thread_video_t *t) { return t->frame.state; }
static void video_thread_main_pump(void) { }
/* The video thread's pass: claim, take the context, complete, release.
 * Runs as far as it can; false when it can make no progress. */
static int stage;
static bool video_step(void)
{
   bool moved = false;
   int saved  = caller;
   caller     = HW_CTX_VIDEO;
   if (stage == 0 && (thr.frame.state & 3))
   {
      thr.frame.state = (thr.frame.state - 1) | 4;
      stage = 1; moved = true;
   }
   if (stage == 1 && ring.ctx_owner == HW_CTX_FREE)
   {
      ring.ctx_owner = HW_CTX_VIDEO; current = HW_CTX_VIDEO;
      stage = 2; moved = true;
   }
   if (stage == 2)
   {
      thr.frame.state &= ~4;
      current = 0; ring.ctx_turns++; ring.ctx_owner = HW_CTX_FREE;
      stage = 0; moved = true;
   }
   caller = saved;
   return moved;
}
static void retro_eventcount_commit_wait(int *v, int key)
{
   (void)v; (void)key;
   if (++waits > 16 || !video_step())
      FAIL("drain waits on a video thread blocked on the GL context");
}
static void video_thread_ring_sleep(thread_video_t *t, int st, long us)
{ (void)st; (void)us; retro_eventcount_commit_wait(&t->frame.ring, 0); }
FUNCTIONS
static void reset(int owner, int pending, int handoff)
{
   memset(&thr, 0, sizeof(thr)); memset(&ring, 0, sizeof(ring));
   thr.frame.hw_ring = &ring; thr.frame.hw_handoff = handoff; ring.handoff = 1;
   ring.ctx_owner = owner; current = owner; caller = HW_CTX_MAIN;
   thr.frame.state = pending; stage = 0; waits = 0;
}
int main(void)
{
   /* Apple-only in the drain; the old drain never asks. */
   (void)video_thread_main_pump; (void)video_thread_is_self;
   /* A dupe pushed while the core runs, with the menu up. */
   reset(HW_CTX_MAIN, 1, 1);
   video_thread_ring_drain(&thr);
   if (thr.frame.state || ring.ctx_owner != HW_CTX_MAIN || current != HW_CTX_MAIN)
      FAIL("held context not taken back after the drain");
   /* Not held: nothing taken. */
   reset(HW_CTX_FREE, 1, 1);
   video_thread_ring_drain(&thr);
   if (thr.frame.state || ring.ctx_owner == HW_CTX_MAIN)
      FAIL("drain took a context it did not hold");
   /* Two contexts: no handoff. */
   reset(HW_CTX_FREE, 2, 0);
   video_thread_ring_drain(&thr);
   if (thr.frame.state)
      FAIL("drain without handoff");
   /* Idle ring while held: no wait, still held. */
   reset(HW_CTX_MAIN, 0, 1);
   video_thread_ring_drain(&thr);
   if (waits || ring.ctx_owner != HW_CTX_MAIN || current != HW_CTX_MAIN)
      FAIL("idle drain");
   puts("PASS ring drain gives up and retakes the one GL context");
   return 0;
}
'''
handoff = hw.index("static hw_ring_t *hw_handoff_ring(thread_video_t *thr)\n{")
parts = [function(hw, "hw_handoff_ring", handoff),
         function(hw, "video_thread_hw_ctx_take"),
         function(hw, "video_thread_hw_ctx_held"),
         function(hw, "video_thread_hw_ctx_release"),
         function(wrapper, "video_thread_ring_drain")]
fixture = fixture.replace("FUNCTIONS", "\n".join(parts))
with tempfile.TemporaryDirectory(prefix="gl-context-drain-") as temp:
    unit = Path(temp) / "test.c"
    compiler = shlex.split(os.environ.get("CC", "cc"))
    suffix = ".exe" if os.name == "nt" or "mingw" in compiler[0] else ""
    exe = Path(temp) / ("test" + suffix)
    unit.write_text(fixture)
    subprocess.run(compiler + ["-std=c89", "-pedantic", "-Wall", "-Wextra",
                   "-Werror", "-Wframe-larger-than=2048"]
                   + shlex.split(os.environ.get("CFLAGS", ""))
                   + [str(unit), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
