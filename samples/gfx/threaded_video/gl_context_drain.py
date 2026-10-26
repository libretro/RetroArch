#!/usr/bin/env python3
"""Check the one-GL-context handoff over every interleaving of two threads.

The shipping take/release/held/hand functions, the ring drain and the
command round trip are compiled with every atomic and eventcount step
made a scheduling point, and run as two coroutines - the core's thread
and a video thread modelled on video_thread_loop() - under a scheduler
that explores every schedule within a preemption bound. A run fails on
a deadlock, on a step budget overrun, on a context bound while it is
current on the other thread, on GL work by a thread not holding it, on
a thread taking the context from the free pool while the other has
asked for it, and on a context not back in the free pool at the end.
"""
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
    match = re.search(r"^(?:static )?(?:VIDEO_NOINLINE )?[a-z_]+ \*?[ \t]*"
                      + name + r"\([^;{]*\)\n\{", source[start:], re.M)
    assert match, name
    begin = start + match.start()
    end = source.index("{", begin) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[begin:end]


def order(text, *needles):
    at = [text.index(n) for n in needles]
    assert at == sorted(at), needles


# The model's two threads stand in for the shipping code around these
# functions; the order they assume is checked against that code.
loop = function(wrapper, "video_thread_loop")
order(loop, "retro_atomic_cas_int(&thr->frame.state", "video_thread_hw_ctx_take(thr, true);",
      "video_thread_handle_packet(thr, &pkt)", "video_thread_hw_ctx_release(thr, true);")
assert loop.rstrip().endswith("video_thread_hw_ctx_release(thr, true);\n   }\n}")
publish = function(hw, "video_thread_hw_publish")
gl = publish[publish.index("if (ring->api == HW_API_GL)"):]
order(gl, "hw_ring_capture", "hw_ctx_hand(ring, HW_CTX_MAIN, true);")
push = function(wrapper, "video_thread_frame")
order(push, "ctx_held = video_thread_hw_ctx_held(thr);",
      "video_thread_hw_publish(thr, &taken_back);",
      "video_thread_ring_publish(thr);", "video_thread_hw_ctx_take(thr, false);")
free_cmd = wrapper[wrapper.index("      case CMD_FREE:"):]
assert "video_thread_hw_free(thr);" in free_cmd[:free_cmd.index("      case ", 10)]
hw_free = function(hw, "video_thread_hw_free")
assert "retro_atomic_store_release_int(&thr->frame.hw_handoff, 0);" in hw_free

fixture = r'''
#if defined(_WIN32)
#include <stddef.h>
__declspec(dllimport) void * __stdcall ConvertThreadToFiber(void *p);
__declspec(dllimport) void * __stdcall CreateFiber(size_t s,
      void (__stdcall *fn)(void *), void *p);
__declspec(dllimport) void __stdcall SwitchToFiber(void *f);
__declspec(dllimport) void __stdcall DeleteFiber(void *f);
#else
#define _XOPEN_SOURCE 600
#include <ucontext.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int bool;
#define true 1
#define false 0
#define VIDEO_NOINLINE
#define GFX_INSTR_INC(x) ((void)0)
#define HW_CTX_FREE      0
#define HW_CTX_MAIN      1
#define HW_CTX_VIDEO     2
#define HW_CTX_HOLDER    3
#define HW_CTX_GRANT     4
#define HW_CTX_WANT(who) (4 << (who))
#define VIDEO_THREAD_RING_PENDING_OF(st) ((st) & 3)
#define VIDEO_THREAD_RING_BUSY_OF(st) (((st) >> 2) & 1)
#define CMD_NONE 0
#define CMD_OTHER 1
#define CMD_FREE 2
typedef struct { int type; } thread_packet_t;
typedef struct { int handoff, ctx_owner, ctx_free; } hw_ring_t;
typedef struct
{
   struct { hw_ring_t *hw_ring; int hw_handoff, state, ring; } frame;
   thread_packet_t *inline_reply;
   int cmd, reply, reply_ec, work, stop;
} thread_video_t;

/* --- the scheduler ---------------------------------------------------- */
#define NT          2
#define MAX_STEPS   4000
#define MAX_DEPTH   4096
enum { READY, BLOCKED, DONE };
static struct co
{
   int state, *ec, key, timed;
   void (*script)(void);
#if defined(_WIN32)
   void *fiber;
#else
   ucontext_t uc;
   char *stack;
#endif
} co[NT];
#if defined(_WIN32)
static void *sched_fiber;
#else
static ucontext_t sched_uc;
#endif
static int cur, steps, depth, plen, used, budget;
static unsigned char prefix[MAX_DEPTH], taken[MAX_DEPTH], cost[MAX_DEPTH];
static const char *scenario;
static unsigned long runs;

static void fail(const char *msg)
{
   int i;
   fprintf(stderr, "FAIL [%s] %s (thread %d, step %d)\n  schedule:",
         scenario, msg, cur, steps);
   for (i = 0; i < depth; i++)
      fprintf(stderr, " %d", taken[i]);
   fputc('\n', stderr);
   exit(1);
}

static void to_sched(void)
{
#if defined(_WIN32)
   SwitchToFiber(sched_fiber);
#else
   swapcontext(&co[cur].uc, &sched_uc);
#endif
}

/* Every shared step goes through here: the scheduler picks who goes on. */
static void step(void) { to_sched(); }

static void co_body(void)
{
   co[cur].script();
   co[cur].state = DONE;
   for (;;)
      to_sched();
}
#if defined(_WIN32)
static void __stdcall co_entry(void *p) { (void)p; co_body(); }
#endif

/* Next thread: whoever is ready; a switch away from a thread that could
 * go on is a decision, and costs a preemption. */
static int pick(int last)
{
   int other = !last, d;
   if (co[last].state != READY && co[other].state != READY)
   {
      int i;
      /* A timed wait ends only when nothing else can move. */
      for (i = 0; i < NT; i++)
         if (co[i].state == BLOCKED && co[i].timed)
         {
            co[i].state = READY;
            return i;
         }
      fail("deadlock: both threads wait");
   }
   if (co[last].state != READY)
      return other;
   if (co[other].state != READY)
      return last;
   if (depth >= MAX_DEPTH)
      fail("decision depth");
   d          = depth++;
   cost[d]    = (unsigned char)used;
   taken[d]   = (d < plen) ? prefix[d] : 0;
   if (taken[d])
      used++;
   return taken[d] ? other : last;
}

static void run(void (*main_script)(void), void (*video_script)(void))
{
   int i, last = 0;
   steps = depth = used = 0;
   co[0].script = main_script;
   co[1].script = video_script;
   for (i = 0; i < NT; i++)
   {
      co[i].state = READY;
      co[i].timed = 0;
      co[i].ec    = NULL;
#if defined(_WIN32)
      co[i].fiber = CreateFiber(65536, co_entry, NULL);
#else
      getcontext(&co[i].uc);
      co[i].uc.uc_stack.ss_sp   = co[i].stack;
      co[i].uc.uc_stack.ss_size = 65536;
      co[i].uc.uc_link          = &sched_uc;
      makecontext(&co[i].uc, co_body, 0);
#endif
   }
   while (co[0].state != DONE || co[1].state != DONE)
   {
      if (++steps > MAX_STEPS)
         fail("no end within the step budget");
      cur = last = pick(last);
#if defined(_WIN32)
      SwitchToFiber(co[cur].fiber);
#else
      swapcontext(&sched_uc, &co[cur].uc);
#endif
   }
#if defined(_WIN32)
   for (i = 0; i < NT; i++)
      DeleteFiber(co[i].fiber);
#endif
   runs++;
}

/* Every schedule of the scenario with at most `budget` preemptions. */
static void explore(const char *name, void (*init)(void), void (*check)(void),
      void (*main_script)(void), void (*video_script)(void))
{
   unsigned long before = runs;
   scenario = name;
   plen     = 0;
   for (;;)
   {
      int d;
      init();
      run(main_script, video_script);
      check();
      for (d = depth - 1; d >= 0; d--)
         if (!taken[d] && cost[d] < budget)
            break;
      if (d < 0)
         break;
      memcpy(prefix, taken, (size_t)d);
      prefix[d] = 1;
      plen      = d + 1;
   }
   printf("  %-26s %lu schedules\n", name, runs - before);
}

/* --- the shared state and its primitives ------------------------------ */
static thread_video_t thr;
static hw_ring_t ring;
static int bound;             /* the thread the context is current on */
static int binds[NT + 1], asked[NT + 1];

static int me(void) { return cur ? HW_CTX_VIDEO : HW_CTX_MAIN; }

static int retro_atomic_load_acquire_int(int *v) { step(); return *v; }
static void retro_atomic_store_release_int(int *v, int n) { step(); *v = n; }
static int retro_atomic_cas_int(int *v, int old, int n)
{
   step();
   if (*v != old)
      return 0;
   if (v == &ring.ctx_owner)
   {
      int other = HW_CTX_MAIN + HW_CTX_VIDEO - me();
      if ((n & HW_CTX_WANT(me())) && !(old & HW_CTX_WANT(me())))
         asked[me()] = 1;
      if (     (old & HW_CTX_HOLDER) == HW_CTX_FREE
            && (n & HW_CTX_HOLDER) == me() && asked[other])
         fail("context taken from the free pool while the other thread asked for it");
   }
   *v = n;
   return 1;
}
static int retro_eventcount_prepare_wait(int *ec) { step(); return *ec; }
static void retro_eventcount_cancel_wait(int *ec) { (void)ec; step(); }
static void wait_on(int *ec, int key, int timed)
{
   step();
   if (*ec != key)
      return;
   if (ec == &ring.ctx_free)
      asked[me()] = 1;
   co[cur].state = BLOCKED;
   co[cur].ec    = ec;
   co[cur].key   = key;
   co[cur].timed = timed;
   to_sched();
   co[cur].timed = 0;
}
static void retro_eventcount_commit_wait(int *ec, int key) { wait_on(ec, key, 0); }
static void retro_eventcount_notify(int *ec)
{
   int i;
   step();
   (*ec)++;
   for (i = 0; i < NT; i++)
      if (co[i].state == BLOCKED && co[i].ec == ec)
         co[i].state = READY;
}

static void video_context_driver_release_current(void)
{
   step();
   if (bound != me())
      fail("context released on a thread it is not current on");
   bound = 0;
}
static void video_context_driver_bind_hw_render(bool core)
{
   step();
   if (bound)
      fail("context bound while current on the other thread");
   if (core != (me() == HW_CTX_MAIN))
      fail("bound the wrong surface");
   bound = me();
   binds[me()]++;
}
/* GL work: only by the thread the context is current on. */
static void gl(void)
{
   step();
   if (bound != me())
      fail("GL work without the context");
}

static bool video_thread_is_self(thread_video_t *t) { (void)t; return cur == 1; }
static int video_thread_ring_load(thread_video_t *t)
{ return retro_atomic_load_acquire_int(&t->frame.state); }
static void video_thread_main_pump(void) { }
static void video_thread_ring_sleep(thread_video_t *t, int state, long us)
{
   int key = retro_eventcount_prepare_wait(&t->frame.ring);
   (void)us;
   if (retro_atomic_load_acquire_int(&t->frame.state) != state)
   {
      retro_eventcount_cancel_wait(&t->frame.ring);
      return;
   }
   retro_eventcount_commit_wait(&t->frame.ring, key);
}
static bool video_thread_handle_packet(thread_video_t *t, thread_packet_t *p)
{ (void)t; (void)p; fail("inline command"); return false; }
static void video_thread_user_acquire(thread_video_t *t) { (void)t; step(); }
static void video_thread_user_release(thread_video_t *t) { (void)t; step(); }
static void video_thread_send_packet(thread_video_t *t, thread_packet_t *p)
{
   retro_atomic_store_release_int(&t->reply, 0);
   retro_atomic_store_release_int(&t->cmd, p->type);
   retro_eventcount_notify(&t->work);
}
static void video_thread_wait_reply(thread_video_t *t, thread_packet_t *p)
{
   (void)p;
   for (;;)
   {
      int key;
      if (retro_atomic_load_acquire_int(&t->reply))
         return;
      key = retro_eventcount_prepare_wait(&t->reply_ec);
      if (retro_atomic_load_acquire_int(&t->reply))
      {
         retro_eventcount_cancel_wait(&t->reply_ec);
         return;
      }
      retro_eventcount_commit_wait(&t->reply_ec, key);
   }
}

FUNCTIONS

/* The shipping take, with the ask cleared once it is answered. */
void video_thread_hw_ctx_take(thread_video_t *t, bool video_thread)
{
   shipping_ctx_take(t, video_thread);
   asked[video_thread ? HW_CTX_VIDEO : HW_CTX_MAIN] = 0;
}

/* --- the two threads --------------------------------------------------- */
static int busywork, presented, video_held_at_start, free_held;

static void queue_frame(void)
{
   for (;;)
   {
      int st = retro_atomic_load_acquire_int(&thr.frame.state);
      if (retro_atomic_cas_int(&thr.frame.state, st, st + 1))
         break;
   }
   retro_eventcount_notify(&thr.work);
}

static bool work_due(void)
{
   return VIDEO_THREAD_RING_PENDING_OF(retro_atomic_load_acquire_int(&thr.frame.state))
      || retro_atomic_load_acquire_int(&thr.cmd) != CMD_NONE
      || busywork > 0 || retro_atomic_load_acquire_int(&thr.stop);
}

/* video_thread_loop(): park, claim, take, command, draw, release. */
static void video_thread(void)
{
   if (video_held_at_start)
      video_thread_hw_ctx_release(&thr, true);
   for (;;)
   {
      bool claimed = false;
      int st, cmd;
      for (;;)
      {
         int key;
         if (work_due())
            break;
         key = retro_eventcount_prepare_wait(&thr.work);
         if (work_due())
         {
            retro_eventcount_cancel_wait(&thr.work);
            break;
         }
         retro_eventcount_commit_wait(&thr.work, key);
      }
      st = retro_atomic_load_acquire_int(&thr.frame.state);
      if (     retro_atomic_load_acquire_int(&thr.stop)
            && !VIDEO_THREAD_RING_PENDING_OF(st) && busywork <= 0
            && retro_atomic_load_acquire_int(&thr.cmd) == CMD_NONE)
         return;
      while (VIDEO_THREAD_RING_PENDING_OF(st))
      {
         if (retro_atomic_cas_int(&thr.frame.state, st, (st - 1) | 4))
         {
            claimed = true;
            retro_eventcount_notify(&thr.frame.ring);
            break;
         }
         st = retro_atomic_load_acquire_int(&thr.frame.state);
      }
      video_thread_hw_ctx_take(&thr, true);
      cmd = retro_atomic_load_acquire_int(&thr.cmd);
      if (cmd != CMD_NONE)
      {
         gl();
         if (cmd == CMD_FREE)
            retro_atomic_store_release_int(&thr.frame.hw_handoff, 0);
         retro_atomic_store_release_int(&thr.cmd, CMD_NONE);
         retro_atomic_store_release_int(&thr.reply, 1);
         retro_eventcount_notify(&thr.reply_ec);
         /* CMD_FREE: the loop ends with the context still current here,
          * where the driver is freed with it. */
         if (cmd == CMD_FREE)
            return;
      }
      if (claimed)
      {
         gl();
         gl();
         presented++;
         for (;;)
         {
            st = retro_atomic_load_acquire_int(&thr.frame.state);
            if (retro_atomic_cas_int(&thr.frame.state, st, st & ~4))
               break;
         }
         retro_eventcount_notify(&thr.frame.ring);
      }
      else if (busywork > 0)
      {
         gl();
         busywork--;
      }
      video_thread_hw_ctx_release(&thr, true);
   }
}

static void stop_video(void)
{
   retro_atomic_store_release_int(&thr.stop, 1);
   retro_eventcount_notify(&thr.work);
}

/* runloop: core_context_take, retro_run with its push, core_context_release */
static void core_frame(void)
{
   bool held;
   video_thread_hw_ctx_take(&thr, false);
   gl();
   held = video_thread_hw_ctx_held(&thr);
   gl();                                   /* hw_ring_capture */
   if (held)
      hw_ctx_hand(&ring, HW_CTX_MAIN, true);
   video_thread_hw_ctx_release(&thr, false);
   queue_frame();
   if (held)
      video_thread_hw_ctx_take(&thr, false);
   gl();                                   /* back in retro_run */
   video_thread_hw_ctx_release(&thr, false);
}

static void main_push(void) { core_frame(); core_frame(); stop_video(); }

/* A dupe pushed from retro_run with the menu up: nothing handed, a drain. */
static void main_drain(void)
{
   video_thread_hw_ctx_take(&thr, false);
   gl();
   queue_frame();
   video_thread_ring_drain(&thr);
   if (     VIDEO_THREAD_RING_PENDING_OF(thr.frame.state)
         || VIDEO_THREAD_RING_BUSY_OF(thr.frame.state))
      fail("drain returned with a frame in flight");
   gl();
   video_thread_hw_ctx_release(&thr, false);
   stop_video();
}

static void main_command(void)
{
   thread_packet_t pkt;
   video_thread_hw_ctx_take(&thr, false);
   gl();
   pkt.type = CMD_OTHER;
   video_thread_send_and_wait_user_to_thread(&thr, &pkt);
   gl();
   video_thread_hw_ctx_release(&thr, false);
   core_frame();
   stop_video();
}

/* The presenter ends: CMD_FREE, sent holding the context or not. */
static void main_free(void)
{
   thread_packet_t pkt;
   int before;
   video_thread_hw_ctx_take(&thr, false);
   gl();
   if (!free_held)
      video_thread_hw_ctx_release(&thr, false);
   pkt.type = CMD_FREE;
   video_thread_send_and_wait_user_to_thread(&thr, &pkt);
   before = binds[HW_CTX_MAIN];
   video_thread_hw_ctx_take(&thr, false);
   if (video_thread_hw_ctx_release(&thr, false) || video_thread_hw_ctx_held(&thr))
      fail("handoff still live after the free");
   if (binds[HW_CTX_MAIN] != before || bound == HW_CTX_MAIN)
      fail("context bound on the core's thread after the free");
}

/* A video thread with a pass of work after every pass. */
static void main_busy(void)
{
   video_thread_hw_ctx_take(&thr, false);
   gl();
   video_thread_hw_ctx_release(&thr, false);
   video_thread_hw_ctx_take(&thr, false);
   gl();
   video_thread_hw_ctx_release(&thr, false);
   core_frame();
   stop_video();
}

/* --- scenarios --------------------------------------------------------- */
static void reset(void)
{
   memset(&thr, 0, sizeof(thr));
   memset(&ring, 0, sizeof(ring));
   thr.frame.hw_ring    = &ring;
   thr.frame.hw_handoff = 1;
   ring.handoff         = 1;
   ring.ctx_owner       = HW_CTX_FREE;
   bound = busywork = presented = video_held_at_start = free_held = 0;
   memset(binds, 0, sizeof(binds));
   memset(asked, 0, sizeof(asked));
}
static void init_free_word(void) { reset(); }
/* As video_thread_hw_prepare_context() leaves it: the video thread
 * holds the context in the pass that installed the ring. */
static void init_video_holds(void)
{
   reset();
   ring.ctx_owner      = HW_CTX_VIDEO;
   bound               = HW_CTX_VIDEO;
   video_held_at_start = 1;
}
static void init_busy(void) { reset(); busywork = 3; }
static void init_free_held(void) { reset(); free_held = 1; }

static void check_settled(void)
{
   if (ring.ctx_owner != HW_CTX_FREE || bound)
      fail("context not back in the free pool at the end");
}
static void check_push(void)
{
   check_settled();
   if (presented != 2)
      fail("pushed frames not presented");
}
static void check_drain(void)
{
   check_settled();
   if (presented != 1)
      fail("drained frame not presented");
}
static void check_free(void)
{
   if (thr.frame.hw_handoff || bound != HW_CTX_VIDEO)
      fail("free did not end on the video thread");
}

int main(int argc, char **argv)
{
   int i;
   (void)video_thread_main_pump;
   budget = argc > 1 ? atoi(argv[1]) : 3;
#if defined(_WIN32)
   sched_fiber = ConvertThreadToFiber(NULL);
#else
   for (i = 0; i < NT; i++)
      co[i].stack = (char*)malloc(65536);
#endif
   printf("preemption bound %d\n", budget);
   explore("push", init_free_word, check_push, main_push, video_thread);
   explore("push, video holds first", init_video_holds, check_push, main_push, video_thread);
   explore("drain", init_free_word, check_drain, main_drain, video_thread);
   explore("command", init_free_word, check_settled, main_command, video_thread);
   explore("free, released", init_free_word, check_free, main_free, video_thread);
   explore("free, held", init_free_held, check_free, main_free, video_thread);
   explore("busy video", init_busy, check_settled, main_busy, video_thread);
#if !defined(_WIN32)
   for (i = 0; i < NT; i++)
      free(co[i].stack);
#else
   (void)i;
#endif
   printf("PASS one-context handoff over %lu schedules\n", runs);
   return 0;
}
'''
handoff = hw.index("static hw_ring_t *hw_handoff_ring(thread_video_t *thr)\n{")
take = function(hw, "video_thread_hw_ctx_take").replace(
    "void video_thread_hw_ctx_take(", "static void shipping_ctx_take(", 1)
parts = ["void video_thread_hw_ctx_take(thread_video_t *t, bool video_thread);",
         function(hw, "hw_ctx_hand"),
         function(hw, "hw_handoff_ring", handoff),
         take,
         function(hw, "video_thread_hw_ctx_held"),
         function(hw, "video_thread_hw_ctx_release"),
         function(wrapper, "video_thread_ring_drain"),
         function(wrapper, "video_thread_send_and_wait_user_to_thread")]
fixture = fixture.replace("FUNCTIONS", "\n".join(parts))
with tempfile.TemporaryDirectory(prefix="gl-context-handoff-") as temp:
    unit = Path(temp) / "test.c"
    compiler = shlex.split(os.environ.get("CC", "cc"))
    suffix = ".exe" if os.name == "nt" or "mingw" in compiler[0] else ""
    exe = Path(temp) / ("test" + suffix)
    unit.write_text(fixture)
    subprocess.run(compiler + ["-std=c89", "-pedantic", "-Wall", "-Wextra",
                   "-Werror", "-Wframe-larger-than=2048"]
                   + shlex.split(os.environ.get("CFLAGS", ""))
                   + [str(unit), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)]
                   + shlex.split(os.environ.get("BOUND", "")), check=True)
