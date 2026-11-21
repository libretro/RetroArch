/* link/link_sched.c over the bus, with fake consoles: plain run functions
 * that make the link calls a core makes, get the interface inside
 * link_sched_enter and keep what they saw for the test to read. Ticks at
 * 61440, so a frame is 1024 ticks, unless a test says otherwise. */

#include <stdio.h>
#include <string.h>

#include <link/link_sched.h>

#ifndef LINK_SCHED_TEST_STACK
#define LINK_SCHED_TEST_STACK 0
#endif

static int failures;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

#define FX_MAX     3
#define FX_RATE    61440
#define FX_ASK     (~(uint32_t)0)
#define FX_ORDERS  18 /* lowest, reversed, shuffled with seeds 1 to 16 */
#define FX_FNV     ((((uint64_t)0x100UL) << 32) | 0x1B3UL)

struct fx_console
{
   struct retro_link_interface link; /* as the environment call gave it */
   retro_link_port_t *h;             /* port 0 */
   unsigned index;
   uint64_t span;                    /* ticks a frame */
   uint64_t tick;                    /* at the edge the session stands at */
   unsigned runs;                    /* retro_run calls begun */
   unsigned finished;                /* and returned */
   uint64_t sum;                     /* of the messages received */
   unsigned received;
};

static link_bus_t *fx_bus;
static link_sched_t *fx_sched;
static struct fx_console fx_c[FX_MAX];
static struct retro_link_port_info fx_list[FX_MAX][2];

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

/* Console c gets the interface inside link_sched_enter, giving its port
 * list, and attaches port 0 there */
static void fx_join(unsigned c, uint64_t rate, unsigned max_ports)
{
   struct fx_console *k           = &fx_c[c];
   struct retro_link_port_info *e = &fx_list[c][0];
   e->port        = 0;
   e->protocol_id = "test-cable";
   e->desc        = "Link";
   e->clock_rate  = rate;
   e->max_ports   = max_ports;
   k->span        = rate / 60;
   link_sched_enter(fx_sched, c);
   memset(&k->link, 0, sizeof(k->link));
   k->link.interface_version = RETRO_LINK_INTERFACE_VERSION;
   k->link.ports             = fx_list[c];
   CHECK(link_sched_environment(fx_sched, &k->link));
   if (fx_linked(&k->link))
      k->h = k->link.attach(0, "test-cable", rate);
   CHECK(k->h != NULL);
   link_sched_leave(fx_sched);
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
   link.ports                = fx_list[0];
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
   k->link.advance_multi(&w, 1);
   CHECK(w.grant == RETRO_LINK_UNBOUNDED && w.wake_flags == RETRO_LINK_WAKE_DETACHED);
   w.grant      = 0;
   w.wake_flags = RETRO_LINK_WAKE_NONE;
   k->link.advance_multi(&w, 1);
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
      single_grant[0] = fx_adv(k, t, t + 100, t + 500, NULL);
      w.handle        = k->h;
      w.local_tick    = t;
      w.safe_tick     = t + 100;
      w.request_tick  = t + 500;
      w.grant         = 0;
      w.wake_flags    = RETRO_LINK_WAKE_NONE;
      k->link.advance_multi(&w, 1);
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
      for (c = 0; c < FX_MAX; c++)
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
   { "held_console_not_run",              held_console_not_run }
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
