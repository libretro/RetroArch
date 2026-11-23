/* link/link_sched.c over the bus, with fake consoles: plain run functions
 * that make the link calls a core makes, get the interface inside
 * link_sched_enter and keep what they saw for the test to read. Ticks at
 * 61440, so a frame is 1024 ticks, unless a test says otherwise. */

#include <stdio.h>
#include <string.h>

#include <libco.h>
#include <link/link_sched.h>

#ifndef LINK_SCHED_TEST_STACK
#define LINK_SCHED_TEST_STACK 0
#endif

static int failures;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

#define FX_MAX     4
#define FX_RATE    61440
#define FX_ASK     (~(uint32_t)0)
#define FX_ORDERS  18 /* lowest, reversed, shuffled with seeds 1 to 16 */
#define FX_FNV     ((((uint64_t)0x100UL) << 32) | 0x1B3UL)

struct fx_console
{
   struct retro_link_interface link; /* as the environment call gave it */
   retro_link_port_t *h;             /* port 0 */
   retro_link_port_t *h1;            /* port 1, where a test attaches one */
   unsigned index;
   uint64_t span;                    /* ticks a frame */
   uint64_t tick;                    /* at the edge the session stands at */
   unsigned runs;                    /* retro_run calls begun */
   unsigned finished;                /* and returned */
   uint64_t sum;                     /* of the messages received */
   unsigned received;
   unsigned steps, ahead;            /* fx_span_run: steps a frame, promise steps past a request */
   uint64_t passed;                  /* the furthest tick granted and run to */
   unsigned late;                    /* messages received below passed */
};

static link_bus_t *fx_bus;
static link_sched_t *fx_sched;
static struct fx_console fx_c[FX_MAX];
static struct retro_link_port_info fx_list[FX_MAX][2];
static const struct retro_link_port_info *fx_ptrs[FX_MAX][3];

static bool fx_linked(const struct retro_link_interface *link)
{
   return link->attach && link->detach && link->peers && link->send_msg
      && link->recv_msg && link->advance && link->advance_multi && link->peek;
}

static void fx_new(unsigned n, link_sched_run_t run)
{
   unsigned c;
   memset(fx_c, 0, sizeof(fx_c));
   memset(fx_list, 0, sizeof(fx_list));
   memset(fx_ptrs, 0, sizeof(fx_ptrs));
   fx_bus   = link_bus_new(n);
   fx_sched = link_sched_new(fx_bus);
   CHECK(fx_bus != NULL && fx_sched != NULL);
   for (c = 0; c < n; c++)
   {
      fx_c[c].index = c;
      fx_c[c].span  = FX_RATE / 60;
      CHECK(link_sched_add(fx_sched, run, &fx_c[c], LINK_SCHED_TEST_STACK) == (int)c);
   }
}

/* Console c attaches port 0 of its list inside link_sched_enter */
static void fx_attach_now(unsigned c)
{
   struct fx_console *k = &fx_c[c];
   link_sched_enter(fx_sched, c);
   if (fx_linked(&k->link))
      k->h = k->link.attach(0, fx_list[c][0].protocol_id, fx_list[c][0].clock_rate);
   CHECK(k->h != NULL);
   link_sched_leave(fx_sched);
}

/* Console c gets the interface inside link_sched_enter, giving a port
 * list of port 0, and attaches it there if attach is set */
static void fx_join_port(unsigned c, const char *protocol, uint64_t rate,
      unsigned max_ports, unsigned flags, bool attach)
{
   struct fx_console *k           = &fx_c[c];
   struct retro_link_port_info *e = &fx_list[c][0];
   e->port        = 0;
   e->protocol_id = protocol;
   e->desc        = "Link";
   e->clock_rate  = rate;
   e->max_ports   = max_ports;
   e->flags       = flags;
   k->span        = rate / 60;
   fx_ptrs[c][0]  = e;
   link_sched_enter(fx_sched, c);
   memset(&k->link, 0, sizeof(k->link));
   k->link.interface_version = RETRO_LINK_INTERFACE_VERSION;
   k->link.ports             = fx_ptrs[c];
   CHECK(link_sched_environment(fx_sched, &k->link));
   link_sched_leave(fx_sched);
   if (attach)
      fx_attach_now(c);
}

static void fx_join(unsigned c, uint64_t rate, unsigned max_ports)
{
   fx_join_port(c, "test-cable", rate, max_ports, 0, true);
}

/* n consoles at 61440 on one cable bus */
static void fx_session(unsigned n, link_sched_run_t run)
{
   unsigned c;
   fx_new(n, run);
   for (c = 0; c < n; c++)
      fx_join(c, FX_RATE, n);
   for (c = 1; c < n; c++)
      CHECK(link_bus_cable(fx_bus, c - 1, 0, c, 0));
}

static void fx_free(void)
{
   link_sched_free(fx_sched);
   link_bus_free(fx_bus);
   fx_sched = NULL;
   fx_bus   = NULL;
}

static void fx_set_pick(unsigned order)
{
   if (order == 0)
      link_sched_set_pick(fx_sched, LINK_SCHED_PICK_LOWEST, 0);
   else if (order == 1)
      link_sched_set_pick(fx_sched, LINK_SCHED_PICK_REVERSED, 0);
   else
      link_sched_set_pick(fx_sched, LINK_SCHED_PICK_SHUFFLED, order - 1);
}

static struct link_sched_stats fx_stats(void)
{
   struct link_sched_stats st;
   memset(&st, 0, sizeof(st));
   link_sched_get_stats(fx_sched, &st);
   return st;
}

static uint64_t fx_adv(struct fx_console *k, uint64_t local, uint64_t safe,
      uint64_t request, uint32_t *flags)
{
   return k->link.advance(k->h, local, safe, request, flags);
}

/* The frame-end publish, promising ahead ticks past the frame end; the
 * console then stands at the next edge */
static void fx_end(struct fx_console *k, uint64_t ahead)
{
   uint64_t end = k->tick + k->span;
   fx_adv(k, end, end + ahead, end, NULL);
   k->tick = end;
}

static void fx_idle_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   k->runs++;
   k->finished++;
}

static void fx_plain_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   k->runs++;
   fx_adv(k, k->tick, k->tick + 64, k->tick, NULL);
   fx_end(k, 64);
   k->finished++;
}

static void env_call_versions(void)
{
   struct retro_link_interface link;
   fx_new(1, fx_idle_run);
   link_sched_enter(fx_sched, 0);
   CHECK(!link_sched_environment(fx_sched, NULL));
   memset(&link, 0, sizeof(link));
   CHECK(!link_sched_environment(fx_sched, &link));
   CHECK(!link.attach);
   link.interface_version = 1;
   CHECK(link_sched_environment(fx_sched, &link));
   CHECK(link.interface_version == 1);
   CHECK(fx_linked(&link));
   memset(&link, 0, sizeof(link));
   link.interface_version = 7;
   CHECK(link_sched_environment(fx_sched, &link));
   CHECK(link.interface_version == 1);
   CHECK(fx_linked(&link));
   /* The port list reaches the bus */
   fx_list[0][0].port        = 0;
   fx_list[0][0].protocol_id = "test-cable";
   fx_list[0][0].clock_rate  = FX_RATE;
   fx_ptrs[0][0]             = &fx_list[0][0];
   link.ports                = fx_ptrs[0];
   CHECK(link_sched_environment(fx_sched, &link));
   if (fx_linked(&link))
   {
      CHECK(!link.attach(0, "test-cable", 60000));
      CHECK(link.attach(0, "test-cable", FX_RATE) != NULL);
   }
   link_sched_leave(fx_sched);
   fx_free();
}

static void env_needs_a_console(void)
{
   struct retro_link_interface link;
   fx_new(1, fx_idle_run);
   memset(&link, 0, sizeof(link));
   link.interface_version = RETRO_LINK_INTERFACE_VERSION;
   CHECK(!link_sched_environment(fx_sched, &link));
   CHECK(!link.attach);
   link_sched_enter(fx_sched, 0);
   CHECK(link_sched_environment(fx_sched, &link));
   link_sched_leave(fx_sched);
   fx_free();
}

static void enter_leave_attach(void)
{
   struct retro_link_interface link;
   retro_link_port_t *h = NULL;
   unsigned count       = 0;
   fx_new(2, fx_idle_run);
   fx_join(0, FX_RATE, 2);
   link_sched_enter(fx_sched, 1);
   CHECK(link_sched_console(fx_sched) == 1);
   memset(&link, 0, sizeof(link));
   link.interface_version = RETRO_LINK_INTERFACE_VERSION;
   CHECK(link_sched_environment(fx_sched, &link));
   if (fx_linked(&link))
      h = link.attach(0, "test-cable", FX_RATE);
   CHECK(h != NULL);
   link_sched_leave(fx_sched);
   CHECK(link_sched_console(fx_sched) == -1);
   /* Attached on console 1 at once: cabled now, it is on a bus */
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_peers(fx_bus, 1, h, &count) == 1 && count == 2);
   fx_free();
}

/* Every call answered as for a port not attached, nothing reaching the bus */
static void fx_outside(void)
{
   struct fx_console *k = &fx_c[0];
   struct retro_link_wait w;
   struct retro_link_wait *list = &w;
   unsigned char buf[16];
   uint64_t hash  = link_bus_hash(fx_bus);
   uint64_t tick  = 7;
   unsigned from  = 7, count = 7;
   size_t len     = sizeof(buf);
   uint32_t flags = 0;
   CHECK(link_sched_console(fx_sched) == -1);
   if (!fx_linked(&k->link))
   {
      CHECK(fx_linked(&k->link));
      return;
   }
   CHECK(!k->link.attach(1, "test-cable", FX_RATE));
   CHECK(k->link.peers(k->h, &count) == -1 && count == 0);
   CHECK(!k->link.send_msg(k->h, k->tick + 64, RETRO_LINK_BROADCAST, "x", 1));
   CHECK(!k->link.recv_msg(k->h, &tick, &from, buf, &len) && len == 0);
   len = sizeof(buf);
   CHECK(!k->link.peek(k->h, &tick, &from, &len) && len == 0);
   CHECK(fx_adv(k, k->tick, k->tick + 64, k->tick + 64, &flags) == RETRO_LINK_UNBOUNDED);
   CHECK(flags == RETRO_LINK_WAKE_DETACHED);
   CHECK(fx_adv(k, k->tick, k->tick + 64, k->tick + 64, NULL) == RETRO_LINK_UNBOUNDED);
   w.handle       = k->h;
   w.local_tick   = k->tick;
   w.safe_tick    = k->tick + 64;
   w.request_tick = k->tick + 64;
   w.grant        = 0;
   w.wake_flags   = FX_ASK;
   k->link.advance_multi(&list, 1);
   CHECK(w.grant == RETRO_LINK_UNBOUNDED && w.wake_flags == RETRO_LINK_WAKE_DETACHED);
   w.grant      = 0;
   w.wake_flags = RETRO_LINK_WAKE_NONE;
   k->link.advance_multi(&list, 1);
   CHECK(w.grant == RETRO_LINK_UNBOUNDED && w.wake_flags == RETRO_LINK_WAKE_NONE);
   k->link.detach(k->h);
   CHECK(link_bus_peers(fx_bus, 0, k->h, &count) == 0 && count == 2);
   CHECK(link_bus_hash(fx_bus) == hash);
}

static void calls_outside_entry_points(void)
{
   struct link_sched_stats st;
   fx_session(2, fx_plain_run);
   fx_outside();
   st = fx_stats();
   CHECK(st.switches == 0 && st.parks == 0 && st.spin_parks == 0);
   CHECK(link_sched_run_frame(fx_sched) == LINK_SCHED_FRAME_DONE);
   CHECK(fx_c[0].finished == 1 && fx_c[1].finished == 1);
   fx_outside();
   st = fx_stats();
   CHECK(st.parks == 0 && st.spin_parks == 0);
   fx_free();
}

static uint64_t single_grant[3];
static uint32_t single_flags[2];

static void single_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   uint64_t t           = k->tick;
   k->runs++;
   if (k->index == 0)
   {
      struct retro_link_wait w;
      struct retro_link_wait *list = &w;
      single_grant[0] = fx_adv(k, t, t + 100, t + 500, NULL);
      w.handle        = k->h;
      w.local_tick    = t;
      w.safe_tick     = t + 100;
      w.request_tick  = t + 500;
      w.grant         = 0;
      w.wake_flags    = RETRO_LINK_WAKE_NONE;
      k->link.advance_multi(&list, 1);
      single_grant[1] = w.grant;
      single_flags[0] = w.wake_flags;
      single_grant[2] = fx_adv(k, t, t + 100, t + 500, &single_flags[1]);
      fx_end(k, 64);
   }
   else
   {
      fx_adv(k, t, t + 2048, t, NULL);
      fx_end(k, 1024);
   }
   k->finished++;
}

static void single_advance_is_one_entry_multi(void)
{
   memset(single_grant, 0, sizeof(single_grant));
   memset(single_flags, 0, sizeof(single_flags));
   fx_session(2, single_run);
   fx_c[0].tick = fx_c[1].tick = 4096;
   CHECK(link_sched_run_frame(fx_sched) == LINK_SCHED_FRAME_DONE);
   CHECK(single_grant[0] == 4196);              /* the request granted as the promise */
   CHECK(single_grant[1] == single_grant[0]);
   CHECK(single_flags[0] == RETRO_LINK_WAKE_NONE);
   CHECK(single_grant[2] == single_grant[0]);
   CHECK(single_flags[1] == RETRO_LINK_WAKE_TOPOLOGY);
   CHECK(fx_c[0].finished == 1 && fx_c[1].finished == 1);
   fx_free();
}

#define PARK_FRAMES 10
static uint64_t park_grant[PARK_FRAMES];

static void park_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   uint64_t t           = k->tick;
   k->runs++;
   CHECK(link_sched_console(fx_sched) == (int)k->index);
   if (k->index == 0)
   {
      uint64_t g;
      fx_adv(k, t, t + 1088, t, NULL);
      g = fx_adv(k, t, t + 1088, t + 1024, NULL);
      if (k->runs <= PARK_FRAMES)
         park_grant[k->runs - 1] = g;
   }
   else
   {
      unsigned i;
      fx_adv(k, t, t + 64, t, NULL);
      for (i = 1; i <= 16; i++)
         fx_adv(k, t + 64 * (i - 1), t + 64 * i, t + 64 * i, NULL);
   }
   fx_end(k, 64);
   k->finished++;
}

static void park_and_resume(void)
{
   unsigned f;
   struct link_sched_stats st;
   memset(park_grant, 0, sizeof(park_grant));
   fx_session(2, park_run);
   for (f = 0; f < PARK_FRAMES; f++)
      CHECK(link_sched_run_frame(fx_sched) == LINK_SCHED_FRAME_DONE);
   for (f = 0; f < PARK_FRAMES; f++)
      CHECK(park_grant[f] == (uint64_t)1024 * f + 1024);
   CHECK(fx_c[0].finished == PARK_FRAMES && fx_c[1].finished == PARK_FRAMES);
   st = fx_stats();
   CHECK(st.parks >= 1);
   CHECK(st.switches >= 2);
   CHECK(st.frames == PARK_FRAMES);
   fx_free();
}

#define PICK_FRAMES 50
#define PICK_STEPS  16
#define PICK_AHEAD  2
#define PICK_CONSOLES 3

static uint64_t pick_fold(uint64_t sum, uint64_t v)
{
   return (sum ^ v) * FX_FNV;
}

static uint64_t pick_tick(const struct fx_console *k, unsigned m)
{
   return k->tick + (uint64_t)m * k->span / PICK_STEPS;
}

/* Every message that can be handed out, into the console's checksum */
static void pick_take(struct fx_console *k)
{
   unsigned char buf[64];
   uint64_t tick = 0;
   unsigned from = 0;
   size_t len    = sizeof(buf);
   while (k->link.recv_msg(k->h, &tick, &from, buf, &len))
   {
      size_t i;
      if (tick < k->passed)
         k->late++;
      k->sum = pick_fold(k->sum, tick);
      k->sum = pick_fold(k->sum, from);
      for (i = 0; i < len; i++)
         k->sum = pick_fold(k->sum, buf[i]);
      k->received++;
      len = sizeof(buf);
   }
}

#define PICK_RUNS 256
static unsigned char pick_ran[FX_ORDERS][PICK_RUNS]; /* frame 0: consoles in the order they ran */
static unsigned pick_ran_len, pick_order;

static void pick_note(const struct fx_console *k)
{
   unsigned char *ran = pick_ran[pick_order];
   if (     k->runs == 1 && pick_ran_len < PICK_RUNS
         && (!pick_ran_len || ran[pick_ran_len - 1] != k->index))
      ran[pick_ran_len++] = (unsigned char)k->index;
}

/* Each step: advance, receive, then broadcast at the promise a payload
 * made from what it has received */
static void pick_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   unsigned m;
   k->runs++;
   pick_note(k);
   fx_adv(k, pick_tick(k, 0), pick_tick(k, PICK_AHEAD), pick_tick(k, 0), NULL);
   pick_note(k);
   pick_take(k);
   for (m = 0; m < PICK_STEPS; m++)
   {
      unsigned char msg[16];
      uint64_t stamp = pick_tick(k, m + PICK_AHEAD);
      unsigned len   = 8 + (unsigned)(k->sum % 9);
      unsigned i;
      fx_adv(k, pick_tick(k, m), stamp, pick_tick(k, m + 1), NULL);
      pick_note(k);
      pick_take(k);
      for (i = 0; i < len; i++)
         msg[i] = (unsigned char)((k->sum >> (8 * (i % 8))) ^ k->index ^ m);
      CHECK(k->link.send_msg(k->h, stamp, RETRO_LINK_BROADCAST, msg, len));
   }
   fx_adv(k, pick_tick(k, PICK_STEPS), pick_tick(k, PICK_STEPS + PICK_AHEAD),
         pick_tick(k, PICK_STEPS), NULL);
   pick_take(k);
   k->tick = pick_tick(k, PICK_STEPS);
   k->finished++;
}

static uint64_t pick_hash[FX_ORDERS][PICK_FRAMES];
static uint64_t pick_sum[FX_ORDERS][PICK_FRAMES][FX_MAX];

static void pick_orders_same_result(void)
{
   unsigned o, f, c;
   bool shuffled = false, started = false;
   memset(pick_ran, 0, sizeof(pick_ran));
   for (o = 0; o < FX_ORDERS; o++)
   {
      pick_order   = o;
      pick_ran_len = 0;
      fx_new(3, pick_run);
      fx_join(0, 61440, 3);
      fx_join(1, 60000, 3);
      fx_join(2, 61440, 3);
      CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
      CHECK(link_bus_cable(fx_bus, 1, 0, 2, 0));
      fx_set_pick(o);
      for (f = 0; f < PICK_FRAMES; f++)
      {
         CHECK(link_sched_run_frame(fx_sched) == LINK_SCHED_FRAME_DONE);
         pick_hash[o][f] = link_bus_hash(fx_bus);
         CHECK(pick_hash[o][f] != 0);
         for (c = 0; c < FX_MAX; c++)
            pick_sum[o][f][c] = fx_c[c].sum;
      }
      for (c = 0; c < PICK_CONSOLES; c++)
         CHECK(fx_c[c].received > 0 && fx_c[c].finished == PICK_FRAMES);
      /* Each order runs the consoles in an order of its own */
      if (o == 0)
         CHECK(fx_stats().parks > 0 && pick_ran[0][0] == 0);
      else if (o == 1)
         CHECK(pick_ran[1][0] == 2);
      else
      {
         if (memcmp(pick_ran[o], pick_ran[0], PICK_RUNS))
            shuffled = true;
         if (pick_ran[o][0] != 0)
            started = true;
      }
      fx_free();
      for (f = 0; f < PICK_FRAMES; f++)
         if (     pick_hash[o][f] != pick_hash[0][f]
               || memcmp(pick_sum[o][f], pick_sum[0][f], sizeof(pick_sum[0][f])))
         {
            fprintf(stderr, "order %u differs from lowest first at frame %u:%s%s\n", o, f,
                  pick_hash[o][f] != pick_hash[0][f] ? " bus hash" : "",
                  memcmp(pick_sum[o][f], pick_sum[0][f], sizeof(pick_sum[0][f])) ? " messages" : "");
            failures++;
            break;
         }
   }
   CHECK(shuffled);
   CHECK(started);
}

static uint64_t stuck_grant;
static uint32_t stuck_flags;
static bool stuck_unbounded;

/* Console 1 attached and never advances; console 0 asks past the edge */
static void stuck_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   uint64_t t           = k->tick;
   k->runs++;
   if (k->index == 0 && k->runs == 1)
   {
      fx_adv(k, t, t + 64, t, NULL);
      stuck_grant = fx_adv(k, t, t + 64, t + 64, &stuck_flags);
      fx_end(k, 64);
   }
   else if (k->index == 0)
   {
      stuck_unbounded =  fx_adv(k, t, t + 64, t, NULL) == RETRO_LINK_UNBOUNDED
                      && fx_adv(k, t, t + 64, t + 64, NULL) == RETRO_LINK_UNBOUNDED
                      && fx_adv(k, t + 1024, t + 1088, t + 1024, NULL) == RETRO_LINK_UNBOUNDED;
      k->tick += k->span;
   }
   k->finished++;
}

static void stuck_and_release(void)
{
   unsigned o;
   for (o = 0; o < FX_ORDERS; o++)
   {
      struct link_sched_stuck rec[4];
      uint64_t parks;
      memset(rec, 0, sizeof(rec));
      stuck_grant     = 0;
      stuck_flags     = 0;
      stuck_unbounded = false;
      fx_session(2, stuck_run);
      fx_c[0].tick = 4096;
      fx_set_pick(o);
      CHECK(link_sched_run_frame(fx_sched) == LINK_SCHED_FRAME_RELEASED);
      CHECK(stuck_grant == RETRO_LINK_UNBOUNDED && stuck_flags == RETRO_LINK_WAKE_DETACHED);
      CHECK(fx_c[0].finished == 1 && fx_c[1].finished == 1);
      CHECK(link_sched_stuck_report(fx_sched, rec, 4) == 1);
      CHECK(rec[0].console == 0 && rec[0].port == 0);
      CHECK(rec[0].local_tick == 4096 && rec[0].request_tick == 4160);
      CHECK(rec[0].bound == 4096);
      CHECK(rec[0].bound_console == 1 && rec[0].bound_port == 0 && rec[0].edge_unknown);
      parks = fx_stats().parks;
      CHECK(link_sched_run_frame(fx_sched) == LINK_SCHED_FRAME_DONE);
      CHECK(stuck_unbounded && fx_c[0].finished == 2);
      CHECK(fx_stats().parks == parks && fx_stats().spin_parks == 0);
      fx_free();
   }
}

static unsigned spin_count, spin_seen;
static bool spin_published;

/* Console 0 repeats one advance that asks for nothing; console 1 publishes */
static void spin_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   uint64_t t           = k->tick;
   k->runs++;
   if (k->index == 0)
   {
      fx_adv(k, t, t, t, NULL);
      while (!spin_published && spin_count < 4 * LINK_SCHED_SPIN_LIMIT)
      {
         spin_count++;
         fx_adv(k, t, t, t, NULL);
      }
   }
   else
   {
      spin_seen = spin_count;
      fx_adv(k, t, t + 1088, t, NULL);
      spin_published = true;
   }
   fx_end(k, 64);
   k->finished++;
}

static void no_progress_park(void)
{
   spin_count     = 0;
   spin_seen      = 0;
   spin_published = false;
   fx_session(2, spin_run);
   CHECK(link_sched_run_frame(fx_sched) == LINK_SCHED_FRAME_DONE);
   CHECK(spin_seen == LINK_SCHED_SPIN_LIMIT);
   CHECK(spin_count == LINK_SCHED_SPIN_LIMIT); /* resumed after the publish */
   CHECK(fx_stats().spin_parks == 1 && fx_stats().parks == 0);
   CHECK(fx_c[0].finished == 1 && fx_c[1].finished == 1);
   fx_free();
}

static uint64_t stall_last;
static unsigned stall_calls;

/* Console 0 never progresses at all; console 1 finishes its frame */
static void stall_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   uint64_t t           = k->tick;
   k->runs++;
   if (k->index == 0)
      do
      {
         stall_calls++;
         stall_last = fx_adv(k, t, t, t, NULL);
      } while (stall_last != RETRO_LINK_UNBOUNDED && stall_calls < 8 * LINK_SCHED_SPIN_LIMIT);
   else
   {
      fx_adv(k, t, t + 64, t, NULL);
      fx_end(k, 64);
   }
   k->finished++;
}

static void no_progress_stuck(void)
{
   unsigned o;
   for (o = 0; o < 2; o++)
   {
      struct link_sched_stuck rec[4];
      memset(rec, 0, sizeof(rec));
      stall_last  = 0;
      stall_calls = 0;
      fx_session(2, stall_run);
      fx_set_pick(o);
      CHECK(link_sched_run_frame(fx_sched) == LINK_SCHED_FRAME_RELEASED);
      CHECK(stall_last == RETRO_LINK_UNBOUNDED);
      CHECK(fx_c[0].finished == 1 && fx_c[1].finished == 1);
      CHECK(link_sched_stuck_report(fx_sched, rec, 4) == 1);
      CHECK(rec[0].console == 0 && rec[0].bound_console == 0);
      fx_free();
   }
}

static uint64_t held_grant;

static void held_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   uint64_t t           = k->tick;
   k->runs++;
   fx_adv(k, t, t + 1088, t, NULL);
   if (k->index == 0 && k->runs == 2)
      held_grant = fx_adv(k, t, t + 1088, t + 512, NULL);
   fx_end(k, 64);
   k->finished++;
}

static void held_console_not_run(void)
{
   struct link_bus_counts counts;
   held_grant = 0;
   fx_session(2, held_run);
   CHECK(link_sched_run_frame(fx_sched) == LINK_SCHED_FRAME_DONE);
   CHECK(fx_c[0].runs == 1 && fx_c[1].runs == 1);
   CHECK(link_bus_hold(fx_bus, 1));
   CHECK(link_sched_run_frame(fx_sched) == LINK_SCHED_FRAME_DONE);
   CHECK(fx_c[0].runs == 2 && fx_c[1].runs == 1);
   CHECK(held_grant == 1536);
   link_bus_get_counts(fx_bus, &counts);
   CHECK(counts.skipped_publish == 0);
   fx_free();
}

/* fx_span_run: a conforming core whose frame is span ticks in steps steps.
 * Each request is the next step, its promise ahead steps further; after
 * each step's grant it sends at the promise and takes every message, into
 * sum with each grant. */
static uint64_t fx_step_tick(const struct fx_console *k, unsigned j)
{
   return k->tick + (uint64_t)j * k->span / k->steps;
}

static void fx_step(struct fx_console *k, unsigned local, unsigned request)
{
   uint64_t to = fx_step_tick(k, request);
   uint64_t g  = fx_adv(k, fx_step_tick(k, local), fx_step_tick(k, request + k->ahead), to, NULL);
   k->sum = pick_fold(k->sum, g);
   pick_take(k);
   if (g >= to)
      k->passed = to;
}

static void fx_span_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   unsigned m;
   k->runs++;
   if (k->h)
   {
      fx_step(k, 0, 0);
      for (m = 0; m < k->steps; m++)
      {
         unsigned char msg[16];
         unsigned len = 8 + (unsigned)(k->sum % 9);
         unsigned i;
         fx_step(k, m, m + 1);
         for (i = 0; i < len; i++)
            msg[i] = (unsigned char)((k->sum >> (8 * (i % 8))) ^ k->index ^ m);
         k->sum = pick_fold(k->sum, k->link.send_msg(k->h,
                  fx_step_tick(k, m + 1 + k->ahead), RETRO_LINK_BROADCAST, msg, len));
      }
      fx_step(k, k->steps, k->steps);
   }
   k->tick = fx_step_tick(k, k->steps);
   k->finished++;
}

static void fx_spans(unsigned c, uint64_t span, unsigned steps, unsigned ahead)
{
   fx_c[c].span  = span;
   fx_c[c].steps = steps;
   fx_c[c].ahead = ahead;
}

static enum link_sched_frame fx_run2(uint64_t t0, uint64_t t1)
{
   uint64_t targets[2];
   targets[0] = t0;
   targets[1] = t1;
   return link_sched_run(fx_sched, targets);
}

static bool fx_frames2(uint64_t f0, uint64_t f1)
{
   return link_sched_frames(fx_sched, 0) == f0 && link_sched_frames(fx_sched, 1) == f1;
}

/* Console 0 at 60000 and console 1 at 16777216 on one cable */
static void fx_two_clocks(unsigned ahead0, unsigned ahead1)
{
   fx_new(2, fx_span_run);
   fx_join(0, 60000, 2);
   fx_join(1, 16777216, 2);
   fx_spans(0, 1001, 4, ahead0);
   fx_spans(1, 280896, 4, ahead1);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
}

static void targets_reach_own_edges(void)
{
   unsigned o;
   for (o = 0; o < 3; o++)
   {
      fx_two_clocks(1, 8); /* console 1 promises two frames ahead */
      fx_set_pick(o);
      CHECK(fx_run2(5, 4) == LINK_SCHED_FRAME_DONE);
      CHECK(fx_frames2(5, 4));
      CHECK(fx_c[0].finished == 5 && fx_c[1].finished == 4);
      CHECK(!link_sched_in_frame(fx_sched, 0) && !link_sched_in_frame(fx_sched, 1));
      CHECK(fx_c[0].received > 0 && fx_c[1].received > 0);
      CHECK(!fx_c[0].late && !fx_c[1].late);
      fx_free();
   }
}

static void waiting_on_console_at_target(void)
{
   unsigned o;
   for (o = 0; o < 3; o++)
   {
      fx_session(2, fx_span_run);
      fx_spans(0, 1024, 4, 1);
      fx_spans(1, 1024, 4, 1);
      fx_set_pick(o);
      CHECK(fx_run2(3, 1) == LINK_SCHED_FRAME_WAITING);
      CHECK(fx_frames2(1, 1));
      CHECK(link_sched_in_frame(fx_sched, 0) && !link_sched_in_frame(fx_sched, 1));
      CHECK(link_sched_stuck_report(fx_sched, NULL, 0) == 0);
      CHECK(fx_run2(3, 3) == LINK_SCHED_FRAME_DONE);
      CHECK(fx_frames2(3, 3));
      CHECK(!link_sched_in_frame(fx_sched, 0) && !link_sched_in_frame(fx_sched, 1));
      CHECK(!fx_c[0].late && !fx_c[1].late);
      fx_free();
   }
}

/* What ran, in order: a frame starting is its console, a done hook 2 more */
static unsigned char at_once_log[32];
static unsigned at_once_len;
static bool at_once_parked;

static void at_once_note(unsigned v)
{
   if (at_once_len < sizeof(at_once_log))
      at_once_log[at_once_len++] = (unsigned char)v;
}

static void at_once_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   at_once_note(k->index);
   if (k->index == 1 && k->runs == 1)
      at_once_parked = link_sched_in_frame(fx_sched, 0) && link_sched_frames(fx_sched, 0) == 0;
   fx_span_run(user);
}

static void at_once_done(void *user)
{
   at_once_note(2 + ((struct fx_console*)user)->index);
}

static void next_frame_starts_at_once(void)
{
   unsigned o;
   for (o = 0; o < 3; o++)
   {
      unsigned i, done0 = 0, done1 = 0, start1 = 0, starts = 0;
      at_once_len    = 0;
      at_once_parked = false;
      fx_session(2, at_once_run);
      fx_spans(0, 2048, 4, 1); /* console 0's frame is two of console 1's */
      fx_spans(1, 1024, 4, 1);
      link_sched_set_done(fx_sched, at_once_done);
      fx_set_pick(o);
      CHECK(fx_run2(1, 2) == LINK_SCHED_FRAME_DONE);
      CHECK(fx_frames2(1, 2));
      for (i = 0; i < at_once_len; i++)
      {
         if (at_once_log[i] == 1 && ++starts == 2)
            start1 = i;
         if (at_once_log[i] == 2)
            done0 = i;
         if (at_once_log[i] == 3 && !done1)
            done1 = i;
      }
      /* Console 1 ends its first frame and starts its second while console
       * 0 is still inside its first */
      CHECK(starts == 2 && done1 && done1 < start1 && start1 < done0);
      CHECK(at_once_parked);
      fx_free();
   }
}

/* Two ports each, cabled 0 to 0 and 1 to 1 */
static void fx_two_ports(link_sched_run_t run)
{
   unsigned c;
   fx_new(2, run);
   for (c = 0; c < 2; c++)
   {
      struct fx_console *k = &fx_c[c];
      unsigned p;
      for (p = 0; p < 2; p++)
      {
         fx_list[c][p].port        = p;
         fx_list[c][p].protocol_id = "test-cable";
         fx_list[c][p].desc        = "Link";
         fx_list[c][p].clock_rate  = FX_RATE;
         fx_list[c][p].max_ports   = 2;
         fx_ptrs[c][p]             = &fx_list[c][p];
      }
      link_sched_enter(fx_sched, c);
      k->link.interface_version = RETRO_LINK_INTERFACE_VERSION;
      k->link.ports             = fx_ptrs[c];
      CHECK(link_sched_environment(fx_sched, &k->link));
      if (fx_linked(&k->link))
      {
         k->h  = k->link.attach(0, "test-cable", FX_RATE);
         k->h1 = k->link.attach(1, "test-cable", FX_RATE);
      }
      CHECK(k->h != NULL && k->h1 != NULL);
      link_sched_leave(fx_sched);
   }
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_cable(fx_bus, 0, 1, 1, 1));
}

static void fx_wait(struct retro_link_wait *w, retro_link_port_t *h, uint64_t local,
      uint64_t safe, uint64_t request, uint32_t ask)
{
   w->handle       = h;
   w->local_tick   = local;
   w->safe_tick    = safe;
   w->request_tick = request;
   w->grant        = 0;
   w->wake_flags   = ask;
}

/* Console 0 waits on console 1's port 1, which console 1 leaves out of its
 * advance while it waits on console 0's port 0 */
static void cycle_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   struct retro_link_wait w[2];
   struct retro_link_wait *list[2];
   uint64_t ahead = k->index ? 128 : 64;
   k->runs++;
   fx_wait(&w[0], k->h, 0, ahead, ahead, RETRO_LINK_WAKE_NONE);
   fx_wait(&w[1], k->h1, 0, ahead, ahead, RETRO_LINK_WAKE_NONE);
   list[0] = &w[0];
   list[1] = &w[1];
   k->link.advance_multi(list, k->index ? 1 : 2);
   k->finished++;
}

/* Console 1's second frame makes its frame-start advance only */
static void skip_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   if (k->index == 0 || k->runs != 1)
   {
      fx_span_run(user);
      return;
   }
   k->runs++;
   fx_adv(k, k->tick, k->tick + k->span + k->span / 4, k->tick, NULL);
   k->tick += k->span;
   k->finished++;
}

static void release_only_on_real_stall(void)
{
   unsigned o;
   for (o = 0; o < 3; o++)
   {
      struct link_sched_stuck rec[4];
      memset(rec, 0, sizeof(rec));
      fx_two_ports(cycle_run);
      fx_set_pick(o);
      CHECK(fx_run2(1, 1) == LINK_SCHED_FRAME_RELEASED);
      CHECK(fx_frames2(1, 1));
      CHECK(link_sched_stuck_report(fx_sched, rec, 4) == 2);
      CHECK(rec[0].console == 0 && rec[0].port == 1 && rec[0].bound_console == 1);
      CHECK(rec[0].bound_port == 1 && rec[0].edge_unknown);
      CHECK(rec[1].console == 1 && rec[1].port == 0 && rec[1].bound_console == 0);
      CHECK(rec[1].bound == 64 && !rec[1].edge_unknown);
      fx_free();

      memset(rec, 0, sizeof(rec));
      fx_session(2, skip_run);
      fx_spans(0, 1024, 4, 1);
      fx_spans(1, 1024, 4, 1);
      fx_set_pick(o);
      /* Console 0 parks on console 1, at its target with its edge tick not
       * given; the frame in progress ends and none starts */
      CHECK(fx_run2(4, 2) == LINK_SCHED_FRAME_RELEASED);
      CHECK(fx_frames2(3, 2));
      CHECK(!link_sched_in_frame(fx_sched, 0) && !link_sched_in_frame(fx_sched, 1));
      CHECK(link_sched_stuck_report(fx_sched, rec, 4) == 1);
      CHECK(rec[0].console == 0 && rec[0].bound_console == 1 && rec[0].edge_unknown);
      CHECK(rec[0].bound == 2048 && rec[0].request_tick == 2304);
      fx_free();
   }
}

static cothread_t done_main;
static unsigned done_calls[FX_MAX];
static bool done_wrong;

static void done_hook(void *user)
{
   const struct fx_console *k = (const struct fx_console*)user;
   unsigned c                 = k->index;
   if (co_active() != done_main || k != &fx_c[c] || c >= 2)
   {
      done_wrong = true;
      return;
   }
   done_calls[c]++;
   if (     link_sched_frames(fx_sched, c) != done_calls[c] || k->finished != done_calls[c]
         || link_sched_in_frame(fx_sched, c) || link_sched_console(fx_sched) != -1)
      done_wrong = true;
}

static void done_hook_on_main(void)
{
   memset(done_calls, 0, sizeof(done_calls));
   done_wrong = false;
   done_main  = co_active();
   fx_session(2, fx_span_run);
   fx_spans(0, 1024, 4, 8); /* two frames ahead: console 1 runs a frame past it */
   fx_spans(1, 1024, 4, 1);
   link_sched_set_done(fx_sched, done_hook);
   CHECK(link_sched_run_frame(fx_sched) == LINK_SCHED_FRAME_DONE);
   CHECK(done_calls[0] == 1 && done_calls[1] == 1);
   CHECK(fx_run2(3, 4) == LINK_SCHED_FRAME_DONE);
   CHECK(done_calls[0] == 3 && done_calls[1] == 4);
   CHECK(!done_wrong);
   fx_free();
}

static void targets_to_first_edge_at_or_after(void)
{
   uint64_t t[2];
   fx_two_clocks(8, 8);
   t[0] = t[1] = 7;
   link_sched_targets_to(fx_sched, 0, t);
   CHECK(t[0] == 1 && t[1] == 1); /* no span shown yet */
   CHECK(fx_run2(599, 597) == LINK_SCHED_FRAME_DONE);
   CHECK(fx_frames2(599, 597));
   link_sched_targets_to(fx_sched, 0, t);
   CHECK(t[0] == 600 && t[1] == 598);
   link_sched_targets_to(fx_sched, 1, t);
   CHECK(t[0] == 601 && t[1] == 598);
   CHECK(link_sched_run(fx_sched, t) == LINK_SCHED_FRAME_DONE);
   CHECK(fx_frames2(601, 598));
   fx_free();
}

/* The journal holds a run_frame's sends; link_sched_run keeps adding */
static void run_frame_resets_journal(void)
{
   size_t n;
   fx_session(2, fx_span_run);
   fx_spans(0, 1024, 4, 1);
   fx_spans(1, 1024, 4, 1);
   CHECK(link_sched_run_frame(fx_sched) == LINK_SCHED_FRAME_DONE);
   n = link_bus_journal(fx_bus, NULL, 0);
   CHECK(n == 8);
   CHECK(link_sched_run_frame(fx_sched) == LINK_SCHED_FRAME_DONE);
   CHECK(link_bus_journal(fx_bus, NULL, 0) == n);
   CHECK(fx_run2(3, 3) == LINK_SCHED_FRAME_DONE);
   CHECK(link_bus_journal(fx_bus, NULL, 0) == 2 * n);
   CHECK(fx_run2(4, 4) == LINK_SCHED_FRAME_DONE);
   CHECK(link_bus_journal(fx_bus, NULL, 0) == 3 * n);
   fx_free();
}

static struct retro_link_wait order_w[2];

/* Console 0 lists its entries back to front: port 1's first */
static void order_run(void *user)
{
   struct fx_console *k = (struct fx_console*)user;
   uint64_t t           = k->tick;
   struct retro_link_wait *list[2];
   k->runs++;
   if (k->index == 0)
   {
      fx_wait(&order_w[0], k->h, t, t + 400, t + 100, RETRO_LINK_WAKE_NONE);
      fx_wait(&order_w[1], k->h1, t, t + 400, t + 300, FX_ASK);
      list[0] = &order_w[1];
      list[1] = &order_w[0];
      k->link.advance_multi(list, 2);
   }
   else
   {
      struct retro_link_wait w[2];
      fx_wait(&w[0], k->h, t, t + 100, t, RETRO_LINK_WAKE_NONE);
      fx_wait(&w[1], k->h1, t, t + 300, t, RETRO_LINK_WAKE_NONE);
      list[0] = &w[0];
      list[1] = &w[1];
      k->link.advance_multi(list, 2);
      fx_wait(&w[0], k->h, t + 1024, t + 1088, t + 1024, RETRO_LINK_WAKE_NONE);
      fx_wait(&w[1], k->h1, t + 1024, t + 1088, t + 1024, RETRO_LINK_WAKE_NONE);
      k->link.advance_multi(list, 2);
      k->tick += 1024;
   }
   k->finished++;
}

static void advance_multi_list_out_of_order(void)
{
   unsigned o;
   for (o = 0; o < 2; o++)
   {
      memset(order_w, 0, sizeof(order_w));
      fx_two_ports(order_run);
      fx_set_pick(o);
      CHECK(fx_run2(1, 1) == LINK_SCHED_FRAME_DONE);
      CHECK(order_w[0].grant == 100 && order_w[0].wake_flags == RETRO_LINK_WAKE_NONE);
      CHECK(order_w[1].grant == 300 && order_w[1].wake_flags == RETRO_LINK_WAKE_TOPOLOGY);
      fx_free();
   }
}

#define FUZZ_SESSIONS 100
#define FUZZ_STEPS    30
#define FUZZ_ORDERS   3

static const uint64_t fuzz_rates[] = { 60000, 61440, 4194304, 16777216, 1000000 };

struct fuzz_session
{
   uint64_t rate[FX_MAX], span[FX_MAX];
   unsigned steps[FX_MAX], ahead[FX_MAX];
   unsigned n;
   unsigned late_step; /* the last console joins before this step; 0 at the start */
   bool radio;
};

struct fuzz_result
{
   uint64_t hash;
   uint64_t sum[FX_MAX], frames[FX_MAX];
   unsigned late, released, waiting;
   bool in_frame[FX_MAX];
};

static uint32_t fuzz_rng;

static uint32_t fuzz_next(void)
{
   fuzz_rng ^= fuzz_rng << 13;
   fuzz_rng ^= fuzz_rng >> 17;
   fuzz_rng ^= fuzz_rng << 5;
   return fuzz_rng;
}

static void fuzz_run(const struct fuzz_session *u, unsigned order, uint32_t seed,
      struct fuzz_result *out)
{
   uint64_t targets[FX_MAX];
   unsigned c, f, last = u->n - 1;
   memset(out, 0, sizeof(*out));
   fx_new(u->n, fx_span_run);
   for (c = 0; c < u->n; c++)
   {
      if (u->radio)
         fx_join_port(c, "test-radio", u->rate[c], 0, RETRO_LINK_PORT_SHARED_MEDIUM,
               c != last || !u->late_step);
      else
         fx_join_port(c, "test-cable", u->rate[c], u->n, 0, true);
      fx_spans(c, u->span[c], u->steps[c], u->ahead[c]);
      fx_c[c].sum = (uint64_t)seed * 64 + c;
   }
   for (c = 1; !u->radio && c < u->n; c++)
      if (c != last || !u->late_step)
         CHECK(link_bus_cable(fx_bus, c - 1, 0, c, 0));
   if (order == 0)
      link_sched_set_pick(fx_sched, LINK_SCHED_PICK_LOWEST, 0);
   else if (order == 1)
      link_sched_set_pick(fx_sched, LINK_SCHED_PICK_REVERSED, 0);
   else
      link_sched_set_pick(fx_sched, LINK_SCHED_PICK_SHUFFLED, seed);
   for (f = 0; f < FUZZ_STEPS; f++)
   {
      enum link_sched_frame r;
      if (u->late_step && f == u->late_step)
      {
         if (u->radio)
            fx_attach_now(last);
         else
            CHECK(link_bus_cable(fx_bus, last - 1, 0, last, 0));
      }
      link_sched_targets_to(fx_sched, 0, targets);
      r = link_sched_run(fx_sched, targets);
      if (r == LINK_SCHED_FRAME_RELEASED)
         out->released++;
      else if (r == LINK_SCHED_FRAME_WAITING)
         out->waiting++;
   }
   out->hash = link_bus_hash(fx_bus);
   for (c = 0; c < u->n; c++)
   {
      out->sum[c]      = fx_c[c].sum;
      out->frames[c]   = link_sched_frames(fx_sched, c);
      out->in_frame[c] = link_sched_in_frame(fx_sched, c);
      out->late       += fx_c[c].late;
   }
   fx_free();
}

/* Session u under each pick order: never released, no message late, and
 * the same bus and cores in every order; false after printing it if not */
static bool fuzz_same(const struct fuzz_session *u, uint32_t seed, unsigned *waiting)
{
   struct fuzz_result r[FUZZ_ORDERS];
   unsigned o, c;
   bool bad = false;
   for (o = 0; o < FUZZ_ORDERS; o++)
   {
      fuzz_run(u, o, seed, &r[o]);
      if (r[o].released || r[o].late)
         bad = true;
      if (     o && (r[o].hash != r[0].hash
            || memcmp(r[o].sum, r[0].sum, sizeof(r[0].sum))
            || memcmp(r[o].frames, r[0].frames, sizeof(r[0].frames))
            || memcmp(r[o].in_frame, r[0].in_frame, sizeof(r[0].in_frame))))
         bad = true;
      *waiting += r[o].waiting;
   }
   if (!bad)
      return true;
   fprintf(stderr, "fuzz seed %lu: %u consoles on a %s, late step %u:", (unsigned long)seed,
         u->n, u->radio ? "radio" : "cable", u->late_step);
   for (c = 0; c < u->n; c++)
      fprintf(stderr, " %lu/%lu m%u L%u", (unsigned long)u->span[c],
            (unsigned long)u->rate[c], u->steps[c], u->ahead[c]);
   fprintf(stderr, "\n");
   for (o = 0; o < FUZZ_ORDERS; o++)
   {
      fprintf(stderr, "   order %u: released %u late %u hash %08lx%08lx frames", o,
            r[o].released, r[o].late, (unsigned long)(r[o].hash >> 32),
            (unsigned long)(r[o].hash & 0xFFFFFFFFUL));
      for (c = 0; c < u->n; c++)
         fprintf(stderr, " %lu%s", (unsigned long)r[o].frames[c], r[o].in_frame[c] ? "+" : "");
      fprintf(stderr, "\n");
   }
   return false;
}

static void fuzz_spans_never_release(void)
{
   unsigned i, c, waiting = 0, failed = 0;
   fuzz_rng = 2463534242UL;
   for (i = 0; i < FUZZ_SESSIONS; i++)
   {
      struct fuzz_session u;
      memset(&u, 0, sizeof(u));
      u.n         = 2 + fuzz_next() % 3;
      u.radio     = (fuzz_next() & 1) != 0;
      u.late_step = (fuzz_next() & 1) ? 1 + fuzz_next() % 15 : 0;
      for (c = 0; c < u.n; c++)
      {
         u.rate[c]  = fuzz_rates[fuzz_next() % 5];
         u.span[c]  = u.rate[c] / (50 + fuzz_next() % 26);
         u.steps[c] = 1 + fuzz_next() % 16;
         u.ahead[c] = fuzz_next() % 5;
      }
      if (!fuzz_same(&u, i + 1, &waiting) && failed++ >= 8)
         break;
   }
   CHECK(!failed);
   CHECK(waiting > 0); /* some steps end with a console parked */
}

static const struct
{
   const char *name;
   void (*run)(void);
} tests[] = {
   { "env_call_versions",                 env_call_versions },
   { "env_needs_a_console",               env_needs_a_console },
   { "enter_leave_attach",                enter_leave_attach },
   { "calls_outside_entry_points",        calls_outside_entry_points },
   { "single_advance_is_one_entry_multi", single_advance_is_one_entry_multi },
   { "park_and_resume",                   park_and_resume },
   { "pick_orders_same_result",           pick_orders_same_result },
   { "stuck_and_release",                 stuck_and_release },
   { "no_progress_park",                  no_progress_park },
   { "no_progress_stuck",                 no_progress_stuck },
   { "held_console_not_run",              held_console_not_run },
   { "targets_reach_own_edges",           targets_reach_own_edges },
   { "waiting_on_console_at_target",      waiting_on_console_at_target },
   { "next_frame_starts_at_once",         next_frame_starts_at_once },
   { "release_only_on_real_stall",        release_only_on_real_stall },
   { "done_hook_on_main",                 done_hook_on_main },
   { "targets_to_first_edge_at_or_after", targets_to_first_edge_at_or_after },
   { "run_frame_resets_journal",          run_frame_resets_journal },
   { "advance_multi_list_out_of_order",   advance_multi_list_out_of_order },
   { "fuzz_spans_never_release",          fuzz_spans_never_release }
};

int main(void)
{
   unsigned i;
   for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
   {
      int before = failures;
      tests[i].run();
      printf("%s %s\n", failures == before ? "PASS" : "FAIL", tests[i].name);
   }
   return failures ? 1 : 0;
}
