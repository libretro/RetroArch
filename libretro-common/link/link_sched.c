/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (link_sched.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <libco.h>
#include <link/link_sched.h>

#define LINK_SCHED_SEED 2463534242UL /* xorshift32 for a seed of 0 */
#define LINK_SCHED_ASK  (~(uint32_t)0) /* any value but WAKE_NONE asks */

enum link_sched_state
{
   LINK_SCHED_DONE = 0,  /* finished this frame, held, or added since */
   LINK_SCHED_UNSTARTED,
   LINK_SCHED_RUNNING,
   LINK_SCHED_PARKED,    /* in an advance not grantable when it was made */
   LINK_SCHED_STALLED    /* parked for making no progress */
};

struct link_sched_mark   /* one entry of a console's previous advance */
{
   uint64_t local, grant;
};

struct link_sched_console
{
   cothread_t thread;
   link_sched_run_t run;
   void *user;
   struct retro_link_wait *waits; /* a parked advance's entries, on its own stack */
   struct link_sched_mark *marks; /* its previous advance, for the no-progress count */
   uint64_t activity;             /* link_bus_activity after that advance */
   uint64_t changes;              /* link_bus_changes when it was last tested */
   unsigned count;                /* entries in waits */
   unsigned marks_count, marks_cap;
   unsigned spins;                /* advances in a row without progress */
   bool marked;                   /* marks holds an advance of this frame */
   bool ready;                    /* parked, and grantable when last tested */
   enum link_sched_state state;
};

struct link_sched
{
   link_bus_t *bus;
   struct link_sched_console *consoles;
   struct link_sched_stuck *stuck; /* one per console, filled at a release */
   cothread_t main;                /* where run_frame switches from */
   struct link_sched_stats stats;
   unsigned count, stuck_count;
   int running, entered;
   uint32_t rng;
   enum link_sched_pick pick;
   bool framing;                   /* inside link_sched_run_frame */
};

/* The scheduler a console is running or entered in, read by the link
 * calls, which carry no context pointer: the running context, as
 * co_active is. */
static link_sched_t *link_sched_active;

/* The scheduler and console a link call comes from; false for none */
static bool link_sched_caller(link_sched_t **s, unsigned *console)
{
   int c = link_sched_console(link_sched_active);
   if (c < 0)
      return false;
   *s       = link_sched_active;
   *console = (unsigned)c;
   return true;
}

static void link_sched_switch_out(link_sched_t *s, unsigned c, enum link_sched_state state)
{
   s->consoles[c].state = state;
   co_switch(s->main);
}

/* Copies an advance's local ticks and grants, the marks growing only when
 * a console lists more ports than before */
static bool link_sched_mark_all(struct link_sched_console *k,
      const struct retro_link_wait *w, unsigned n)
{
   unsigned i;
   if (n > k->marks_cap)
   {
      struct link_sched_mark *m = (struct link_sched_mark*)realloc(k->marks,
            n * sizeof(*m)); /* smaller than the core's own entries */
      if (!m)
         return false;
      k->marks     = m;
      k->marks_cap = n;
   }
   for (i = 0; i < n; i++)
   {
      k->marks[i].local = w[i].local_tick;
      k->marks[i].grant = w[i].grant;
   }
   k->marks_count = n;
   return true;
}

/* After a granted advance: one with the previous one's local ticks and
 * grants and no send or recv between counts as no progress, and at the
 * limit the console parks until its buses change. Its result is already
 * written, so the park moves nothing. */
static void link_sched_progress(link_sched_t *s, unsigned c,
      struct retro_link_wait *w, unsigned n)
{
   struct link_sched_console *k = &s->consoles[c];
   uint64_t activity            = link_bus_activity(s->bus, c);
   bool same                    = k->marked && k->marks_count == n && k->activity == activity;
   unsigned i;
   for (i = 0; same && i < n; i++)
      same = k->marks[i].local == w[i].local_tick && k->marks[i].grant == w[i].grant;
   if (!same)
   {
      k->spins    = 0;
      k->activity = activity;
      k->marked   = link_sched_mark_all(k, w, n);
      return;
   }
   if (++k->spins < LINK_SCHED_SPIN_LIMIT || link_bus_released(s->bus))
      return;
   k->spins   = 0;
   k->waits   = w;
   k->count   = n;
   k->changes = link_bus_changes(s->bus, c);
   s->stats.spin_parks++;
   link_sched_switch_out(s, c, LINK_SCHED_STALLED);
   s->consoles[c].waits = NULL;
   s->consoles[c].count = 0;
}

/* publish; if grantable, grant; else park on the console's cothread until
 * it is, or until a release, then grant */
static void link_sched_advance(link_sched_t *s, unsigned c,
      struct retro_link_wait *w, unsigned n)
{
   bool own = s->running == (int)c; /* on its cothread, so it may park */
   link_bus_publish(s->bus, c, w, n);
   if (own && !link_bus_grantable(s->bus, c, w, n))
   {
      struct link_sched_console *k = &s->consoles[c];
      k->waits   = w;
      k->count   = n;
      k->changes = link_bus_changes(s->bus, c);
      s->stats.parks++;
      link_sched_switch_out(s, c, LINK_SCHED_PARKED);
      s->consoles[c].waits = NULL;
      s->consoles[c].count = 0;
   }
   link_bus_grant(s->bus, c, w, n);
   if (own)
      link_sched_progress(s, c, w, n);
}

static retro_link_port_t *RETRO_CALLCONV link_sched_attach(unsigned port,
      const char *protocol_id, uint64_t clock_rate)
{
   link_sched_t *s;
   unsigned c;
   if (!link_sched_caller(&s, &c))
      return NULL;
   return link_bus_attach(s->bus, c, port, protocol_id, clock_rate);
}

static void RETRO_CALLCONV link_sched_detach(retro_link_port_t *handle)
{
   link_sched_t *s;
   unsigned c;
   if (link_sched_caller(&s, &c))
      link_bus_detach(s->bus, c, handle);
}

static int RETRO_CALLCONV link_sched_peers(retro_link_port_t *handle, unsigned *count)
{
   link_sched_t *s;
   unsigned c;
   if (link_sched_caller(&s, &c))
      return link_bus_peers(s->bus, c, handle, count);
   if (count)
      *count = 0;
   return -1;
}

static bool RETRO_CALLCONV link_sched_send(retro_link_port_t *handle, uint64_t tick,
      unsigned to, const void *buf, size_t len)
{
   link_sched_t *s;
   unsigned c;
   if (!link_sched_caller(&s, &c))
      return false;
   return link_bus_send(s->bus, c, handle, tick, to, buf, len);
}

static bool RETRO_CALLCONV link_sched_recv(retro_link_port_t *handle, uint64_t *tick,
      unsigned *from, void *buf, size_t *len)
{
   link_sched_t *s;
   unsigned c;
   if (link_sched_caller(&s, &c))
      return link_bus_recv(s->bus, c, handle, tick, from, buf, len);
   if (len)
      *len = 0;
   return false;
}

static const void *RETRO_CALLCONV link_sched_peek(retro_link_port_t *handle,
      uint64_t *tick, unsigned *from, size_t *len)
{
   link_sched_t *s;
   unsigned c;
   if (link_sched_caller(&s, &c))
      return link_bus_peek(s->bus, c, handle, tick, from, len);
   if (len)
      *len = 0;
   return NULL;
}

static void RETRO_CALLCONV link_sched_advance_multi(struct retro_link_wait *waits,
      unsigned count)
{
   link_sched_t *s;
   unsigned c, i;
   if (link_sched_caller(&s, &c))
   {
      link_sched_advance(s, c, waits, count);
      return;
   }
   for (i = 0; i < count; i++)
   {
      waits[i].grant      = RETRO_LINK_UNBOUNDED;
      waits[i].wake_flags = waits[i].wake_flags != RETRO_LINK_WAKE_NONE
         ? RETRO_LINK_WAKE_DETACHED : RETRO_LINK_WAKE_NONE;
   }
}

static uint64_t RETRO_CALLCONV link_sched_advance_one(retro_link_port_t *handle,
      uint64_t local_tick, uint64_t safe_tick, uint64_t request_tick,
      uint32_t *wake_flags)
{
   struct retro_link_wait w;
   w.handle       = handle;
   w.local_tick   = local_tick;
   w.safe_tick    = safe_tick;
   w.request_tick = request_tick;
   w.grant        = 0;
   w.wake_flags   = wake_flags ? LINK_SCHED_ASK : RETRO_LINK_WAKE_NONE;
   link_sched_advance_multi(&w, 1);
   if (wake_flags)
      *wake_flags = w.wake_flags;
   return w.grant;
}

/* Each console's cothread, for the scheduler's life */
static void link_sched_entry(void)
{
   link_sched_t *s = link_sched_active;
   unsigned c      = (unsigned)s->running;
   for (;;)
   {
      s->consoles[c].run(s->consoles[c].user);
      link_sched_switch_out(s, c, LINK_SCHED_DONE);
   }
}

static void link_sched_resume(link_sched_t *s, unsigned c)
{
   s->consoles[c].state = LINK_SCHED_RUNNING;
   s->consoles[c].ready = false;
   s->running           = (int)c;
   s->stats.switches++;
   co_switch(s->consoles[c].thread);
   s->running = -1;
}

static bool link_sched_waiting(const struct link_sched_console *k)
{
   return k->state == LINK_SCHED_PARKED || k->state == LINK_SCHED_STALLED;
}

/* Tests a parked console again once its buses have changed */
static void link_sched_retest(link_sched_t *s, unsigned c)
{
   struct link_sched_console *k = &s->consoles[c];
   uint64_t changes;
   if (!link_sched_waiting(k) || k->ready)
      return;
   changes = link_bus_changes(s->bus, c);
   if (changes == k->changes)
      return;
   k->changes = changes;
   k->ready   = k->state == LINK_SCHED_STALLED
      || link_bus_grantable(s->bus, c, k->waits, k->count);
}

static bool link_sched_any_waiting(const link_sched_t *s)
{
   unsigned c;
   for (c = 0; c < s->count; c++)
      if (link_sched_waiting(&s->consoles[c]))
         return true;
   return false;
}

static bool link_sched_can_run(const struct link_sched_console *k, bool released)
{
   return k->state == LINK_SCHED_UNSTARTED
      || (link_sched_waiting(k) && (k->ready || released));
}

static int link_sched_pick_next(link_sched_t *s)
{
   unsigned c, n = 0;
   int lowest    = -1, highest = -1;
   bool released = link_bus_released(s->bus);
   for (c = 0; c < s->count; c++)
   {
      link_sched_retest(s, c);
      if (!link_sched_can_run(&s->consoles[c], released))
         continue;
      if (lowest < 0)
         lowest = (int)c;
      highest = (int)c;
      n++;
   }
   /* After a release every parked console runs in console order */
   if (!n || released || s->pick == LINK_SCHED_PICK_LOWEST)
      return lowest;
   if (s->pick == LINK_SCHED_PICK_REVERSED)
      return highest;
   s->rng ^= s->rng << 13;
   s->rng ^= s->rng >> 17;
   s->rng ^= s->rng << 5;
   n       = (unsigned)(s->rng % n);
   for (c = 0; ; c++)
      if (link_sched_can_run(&s->consoles[c], released) && !n--)
         return (int)c;
}

static void link_sched_record(link_sched_t *s, unsigned c, struct link_sched_stuck *out)
{
   const struct link_sched_console *k = &s->consoles[c];
   struct link_bus_blocker by;
   out->console = c;
   if (link_bus_blocked_by(s->bus, c, k->waits, k->count, &by))
   {
      out->port          = by.port;
      out->local_tick    = k->waits[by.entry].local_tick;
      out->request_tick  = by.request;
      out->bound         = by.bound;
      out->bound_console = by.peer_console;
      out->bound_port    = by.peer_port;
      out->edge_unknown  = by.edge_unknown;
      return;
   }
   /* Parked for no progress: nothing but itself holds it */
   out->port          = UINT_MAX;
   out->local_tick    = k->count ? k->waits[0].local_tick : 0;
   out->request_tick  = k->count ? k->waits[0].request_tick : 0;
   out->bound         = k->count ? k->waits[0].grant : 0;
   out->bound_console = c;
   out->bound_port    = UINT_MAX;
   out->edge_unknown  = false;
}

/* Nothing can run while a console is parked: record each parked console,
 * then release the bus so every advance returns at once */
static void link_sched_release(link_sched_t *s)
{
   unsigned c;
   s->stuck_count = 0;
   for (c = 0; c < s->count; c++)
      if (link_sched_waiting(&s->consoles[c]))
         link_sched_record(s, c, &s->stuck[s->stuck_count++]);
   link_bus_release(s->bus);
}

link_sched_t *link_sched_new(link_bus_t *bus)
{
   link_sched_t *s;
   if (!bus || !(s = (link_sched_t*)calloc(1, sizeof(*s))))
      return NULL;
   s->bus     = bus;
   s->running = -1;
   s->entered = -1;
   s->pick    = LINK_SCHED_PICK_LOWEST;
   s->rng     = (uint32_t)LINK_SCHED_SEED;
   return s;
}

void link_sched_free(link_sched_t *s)
{
   unsigned c;
   if (!s)
      return;
   for (c = 0; c < s->count; c++)
   {
      co_delete(s->consoles[c].thread);
      free(s->consoles[c].marks);
   }
   free(s->consoles);
   free(s->stuck);
   if (link_sched_active == s)
      link_sched_active = NULL;
   free(s);
}

int link_sched_add(link_sched_t *s, link_sched_run_t run, void *user, size_t stack_size)
{
   struct link_sched_console *consoles, *k;
   struct link_sched_stuck *stuck;
   if (!s || !run || s->framing || s->count >= INT_MAX)
      return -1;
   if (!stack_size)
      stack_size = LINK_SCHED_DEFAULT_STACK;
   if (stack_size > UINT_MAX)
      return -1;
   if (!(consoles = (struct link_sched_console*)realloc(s->consoles,
               (s->count + 1) * sizeof(*consoles))))
      return -1;
   s->consoles = consoles;
   if (!(stuck = (struct link_sched_stuck*)realloc(s->stuck,
               (s->count + 1) * sizeof(*stuck))))
      return -1;
   s->stuck = stuck;
   k        = &consoles[s->count];
   memset(k, 0, sizeof(*k));
   if (!(k->thread = co_create((unsigned)stack_size, link_sched_entry)))
      return -1;
   k->run   = run;
   k->user  = user;
   k->state = LINK_SCHED_DONE;
   return (int)s->count++;
}

/* murmur3's fmix32: xorshift32's first outputs from a small seed are
 * multiples of it, so the first pick would hardly vary */
static uint32_t link_sched_mix(uint32_t x)
{
   x ^= x >> 16;
   x  = (uint32_t)(x * 0x85EBCA6BUL);
   x ^= x >> 13;
   x  = (uint32_t)(x * 0xC2B2AE35UL);
   x ^= x >> 16;
   return x;
}

void link_sched_set_pick(link_sched_t *s, enum link_sched_pick pick, uint32_t seed)
{
   if (!s)
      return;
   s->pick = pick;
   s->rng  = seed ? link_sched_mix(seed) : (uint32_t)LINK_SCHED_SEED;
}

bool link_sched_environment(link_sched_t *s, void *data)
{
   struct retro_link_interface *link = (struct retro_link_interface*)data;
   int c                             = link_sched_console(s);
   if (!link || !link->interface_version || c < 0)
      return false;
   if (!link_bus_set_ports(s->bus, (unsigned)c, link->ports))
      return false;
   if (link->interface_version > RETRO_LINK_INTERFACE_VERSION)
      link->interface_version = RETRO_LINK_INTERFACE_VERSION;
   link->attach        = link_sched_attach;
   link->detach        = link_sched_detach;
   link->peers         = link_sched_peers;
   link->send_msg      = link_sched_send;
   link->recv_msg      = link_sched_recv;
   link->advance       = link_sched_advance_one;
   link->advance_multi = link_sched_advance_multi;
   link->peek          = link_sched_peek;
   return true;
}

void link_sched_enter(link_sched_t *s, unsigned console)
{
   if (!s || console >= s->count)
      return;
   s->entered        = (int)console;
   link_sched_active = s;
}

void link_sched_leave(link_sched_t *s)
{
   if (!s)
      return;
   s->entered = -1;
   if (!s->framing && link_sched_active == s)
      link_sched_active = NULL;
}

int link_sched_console(const link_sched_t *s)
{
   if (!s)
      return -1;
   return s->running >= 0 ? s->running : s->entered;
}

enum link_sched_frame link_sched_run_frame(link_sched_t *s)
{
   unsigned c;
   int next;
   bool released;
   link_sched_t *was;
   if (!s || s->framing)
      return LINK_SCHED_FRAME_DONE;
   was               = link_sched_active;
   released          = false;
   s->framing        = true;
   s->main           = co_active();
   link_sched_active = s;
   for (c = 0; c < s->count; c++)
   {
      struct link_sched_console *k = &s->consoles[c];
      k->state = LINK_SCHED_DONE;
      if (link_bus_is_held(s->bus, c))
         continue;
      link_bus_frame_start(s->bus, c);
      k->state  = LINK_SCHED_UNSTARTED;
      k->spins  = 0;
      k->marked = false;
   }
   for (;;)
   {
      if ((next = link_sched_pick_next(s)) >= 0)
      {
         link_sched_resume(s, (unsigned)next);
         if (s->consoles[next].state == LINK_SCHED_DONE)
            link_bus_frame_end(s->bus, (unsigned)next);
         continue;
      }
      if (!link_sched_any_waiting(s))
         break;
      link_sched_release(s);
      released = true;
   }
   link_bus_edge(s->bus);
   s->stats.frames++;
   s->framing        = false;
   link_sched_active = was;
   return released ? LINK_SCHED_FRAME_RELEASED : LINK_SCHED_FRAME_DONE;
}

unsigned link_sched_stuck_report(const link_sched_t *s, struct link_sched_stuck *out, unsigned cap)
{
   unsigned i;
   if (!s)
      return 0;
   for (i = 0; out && i < cap && i < s->stuck_count; i++)
      out[i] = s->stuck[i];
   return s->stuck_count;
}

void link_sched_get_stats(const link_sched_t *s, struct link_sched_stats *out)
{
   if (!out)
      return;
   if (s)
      *out = s->stats;
   else
      memset(out, 0, sizeof(*out));
}
