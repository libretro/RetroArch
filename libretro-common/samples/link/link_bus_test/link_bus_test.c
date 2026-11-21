/* link/link_bus.c alone, one check per rule: records, membership, the
 * advance (bounds, repairs, rounding, reports), messages, holds and idle. The
 * values are worked out by hand; frames are 1/60 s, so 1024 ticks at 61440
 * and 1000 at 60000. */

#include <stdio.h>
#include <string.h>

#include <link/link_bus.h>

static int failures;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

#define FX_CONSOLES 4
#define FX_PORTS    2
#define FX_FAR      ((uint64_t)1 << 40)
#define FX_ASK      (~(uint32_t)0)
#define TICK_MAX    (RETRO_LINK_UNBOUNDED - 1)

/* The session under test: up to four consoles of up to two ports each */
static link_bus_t *fx_bus;
static unsigned fx_consoles;
static struct retro_link_port_info fx_list[FX_CONSOLES][FX_PORTS + 1];
static retro_link_port_t *fx_h[FX_CONSOLES][FX_PORTS];
static uint64_t fx_tick[FX_CONSOLES][FX_PORTS]; /* at the edge the session stands at */
static uint64_t fx_span[FX_CONSOLES][FX_PORTS];
static uint64_t fx_safe[FX_CONSOLES][FX_PORTS]; /* the highest promise published */

static void fx_new(unsigned consoles)
{
   memset(fx_list, 0, sizeof(fx_list));
   memset(fx_h, 0, sizeof(fx_h));
   memset(fx_tick, 0, sizeof(fx_tick));
   memset(fx_span, 0, sizeof(fx_span));
   memset(fx_safe, 0, sizeof(fx_safe));
   fx_consoles = consoles;
   fx_bus      = link_bus_new(consoles);
   CHECK(fx_bus != NULL);
}

static void fx_free(void)
{
   link_bus_free(fx_bus);
   fx_bus = NULL;
}

/* Port p of console c's list; ports are given in order from 0 */
static void fx_port(unsigned c, unsigned p, const char *protocol, uint64_t rate,
      unsigned max_ports, unsigned flags)
{
   struct retro_link_port_info *e = &fx_list[c][p];
   e->port        = p;
   e->protocol_id = protocol;
   e->desc        = "Link";
   e->clock_rate  = rate;
   e->max_ports   = max_ports;
   e->flags       = flags;
   e->idle_ticks  = 0;
}

static void fx_attach(unsigned c)
{
   unsigned p;
   CHECK(link_bus_set_ports(fx_bus, c, fx_list[c]));
   for (p = 0; p < FX_PORTS && fx_list[c][p].protocol_id; p++)
   {
      fx_h[c][p] = link_bus_attach(fx_bus, c, p, fx_list[c][p].protocol_id,
            fx_list[c][p].clock_rate);
      CHECK(fx_h[c][p] != NULL);
      fx_span[c][p] = fx_list[c][p].clock_rate / 60;
   }
}

/* The default: consoles 0 and 1, one test-cable port each, cabled */
static void fx_two(uint64_t rate0, uint64_t rate1)
{
   fx_new(2);
   fx_port(0, 0, "test-cable", rate0, 2, 0);
   fx_port(1, 0, "test-cable", rate1, 2, 0);
   fx_attach(0);
   fx_attach(1);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
}

static void fx_note_safe(unsigned c, retro_link_port_t *h, uint64_t safe)
{
   unsigned p;
   if (c >= FX_CONSOLES || !h)
      return;
   for (p = 0; p < FX_PORTS; p++)
      if (fx_h[c][p] == h && safe > fx_safe[c][p])
         fx_safe[c][p] = safe;
}

/* An advance by console c as its scheduler makes one: publish, and grant if
 * grantable; returns whether it was. */
static bool advm(unsigned c, struct retro_link_wait *w, unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
      fx_note_safe(c, w[i].handle, w[i].safe_tick);
   link_bus_publish(fx_bus, c, w, n);
   if (!link_bus_grantable(fx_bus, c, w, n))
      return false;
   link_bus_grant(fx_bus, c, w, n);
   return true;
}

static void fx_wait(struct retro_link_wait *w, retro_link_port_t *h,
      uint64_t local, uint64_t safe, uint64_t request, bool ask)
{
   w->handle       = h;
   w->local_tick   = local;
   w->safe_tick    = safe;
   w->request_tick = request;
   w->grant        = 0;
   w->wake_flags   = ask ? FX_ASK : RETRO_LINK_WAKE_NONE;
}

/* One-entry advance; flags NULL asks for no reports */
static bool adv(unsigned c, retro_link_port_t *h, uint64_t local, uint64_t safe,
      uint64_t request, uint64_t *grant, uint32_t *flags)
{
   struct retro_link_wait w;
   fx_wait(&w, h, local, safe, request, flags != NULL);
   if (!advm(c, &w, 1))
      return false;
   if (grant)
      *grant = w.grant;
   if (flags)
      *flags = w.wake_flags;
   return true;
}

static uint64_t fx_max(uint64_t a, uint64_t b) { return a > b ? a : b; }

/* Every port of console c at tick t of its own (the frame-end tick when end) */
static void fx_all_ports(unsigned c, bool end)
{
   struct retro_link_wait w[FX_PORTS];
   unsigned p, n = 0;
   for (p = 0; p < FX_PORTS; p++)
   {
      uint64_t t;
      if (!fx_h[c][p])
         continue;
      t = end ? fx_tick[c][p] + fx_span[c][p] : fx_tick[c][p];
      fx_wait(&w[n++], fx_h[c][p], t, fx_max(fx_safe[c][p], t), t, false);
   }
   if (n)
      CHECK(advm(c, w, n));
}

/* A held console stands at its edge: it runs no frame */
static bool fx_running(unsigned c)
{
   return !link_bus_is_held(fx_bus, c);
}

static void fx_begin(void)
{
   unsigned c;
   for (c = 0; c < fx_consoles; c++)
      if (fx_running(c))
         link_bus_frame_start(fx_bus, c);
}

/* Each console's frame-start advance, promising its local tick */
static void fx_starts(void)
{
   unsigned c;
   for (c = 0; c < fx_consoles; c++)
      if (fx_running(c))
         fx_all_ports(c, false);
}

/* The frame-start advance of console c's port 0, promising far ahead */
static void fx_far(unsigned c)
{
   uint64_t t = fx_tick[c][0];
   CHECK(adv(c, fx_h[c][0], t, t + FX_FAR, t, NULL, NULL));
}

/* Ends the frame: the frame-end publish of every port, frame_end for every
 * console, the edge. */
static void frame(void)
{
   bool run[FX_CONSOLES];
   unsigned c, p;
   for (c = 0; c < fx_consoles; c++)
      run[c] = fx_running(c);
   for (c = 0; c < fx_consoles; c++)
      if (run[c])
         fx_all_ports(c, true);
   for (c = 0; c < fx_consoles; c++)
      if (run[c])
         link_bus_frame_end(fx_bus, c);
   link_bus_edge(fx_bus);
   for (c = 0; c < fx_consoles; c++)
      for (p = 0; p < FX_PORTS; p++)
         if (fx_h[c][p] && run[c])
            fx_tick[c][p] += fx_span[c][p];
}

static void fx_run_frame(void)
{
   fx_begin();
   fx_starts();
   frame();
}

/* A send on console c's port p */
static bool snd(unsigned c, unsigned p, uint64_t tick, unsigned to,
      const void *buf, size_t len)
{
   return link_bus_send(fx_bus, c, fx_h[c][p], tick, to, buf, len);
}

static struct link_bus_counts counts(void)
{
   struct link_bus_counts k;
   memset(&k, 0, sizeof(k));
   link_bus_get_counts(fx_bus, &k);
   return k;
}

static void attach_handles(void)
{
   char buf[16];
   retro_link_port_t *h;

   fx_new(1);
   h = link_bus_attach(fx_bus, 0, 0, "test-cable", 61440);
   CHECK(h != NULL);
   CHECK(link_bus_attach(fx_bus, 0, 0, "test-cable", 61440) == h);
   CHECK(link_bus_attach(fx_bus, 0, 0, "other", 61440) == NULL);
   CHECK(link_bus_attach(fx_bus, 0, 0, "test-cable", 60000) == NULL);
   CHECK(link_bus_attach(fx_bus, 0, 1, "test-cable", 0) == NULL);
   CHECK(link_bus_attach(fx_bus, 0, 1, NULL, 61440) == NULL);
   link_bus_detach(fx_bus, 0, h);
   CHECK(link_bus_attach(fx_bus, 0, 0, "test-cable", 61440) == h);

   strcpy(buf, "test-cable");
   h = link_bus_attach(fx_bus, 0, 1, buf, 61440);
   CHECK(h != NULL);
   memset(buf, 'x', sizeof(buf) - 1);
   buf[sizeof(buf) - 1] = '\0';
   CHECK(link_bus_attach(fx_bus, 0, 1, "test-cable", 61440) == h);
   fx_free();
}

static void attach_matches_port_list(void)
{
   unsigned count = 0;
   retro_link_port_t *h;

   fx_new(2);
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_port(1, 0, "test-cable", 61440, 2, 0);
   CHECK(link_bus_set_ports(fx_bus, 0, fx_list[0]));
   CHECK(link_bus_attach(fx_bus, 0, 1, "test-cable", 61440) == NULL);
   CHECK(link_bus_attach(fx_bus, 0, 0, "test-cable", 60000) == NULL);
   h = link_bus_attach(fx_bus, 0, 0, "test-cable", 61440);
   CHECK(h != NULL);
   fx_attach(1);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_peers(fx_bus, 0, h, &count) == 0 && count == 2);

   fx_list[0][0].clock_rate = 60000;
   CHECK(link_bus_set_ports(fx_bus, 0, fx_list[0]));
   count = 0;
   CHECK(link_bus_peers(fx_bus, 0, h, &count) == 0 && count == 2);
   CHECK(link_bus_attach(fx_bus, 0, 0, "test-cable", 61440) == h);

   link_bus_detach(fx_bus, 0, h);
   CHECK(link_bus_attach(fx_bus, 0, 0, "test-cable", 61440) == NULL);
   CHECK(link_bus_attach(fx_bus, 0, 0, "test-cable", 60000) == h);
   fx_free();
}

static void unknown_flag_bits_ignored(void)
{
   link_bus_t *b = link_bus_new(5);
   struct retro_link_port_info radio[2], cable[2];
   retro_link_port_t *h[5];
   unsigned c, count;

   memset(radio, 0, sizeof(radio));
   memset(cable, 0, sizeof(cable));
   radio[0].protocol_id = "test-radio";
   radio[0].desc        = "Radio";
   radio[0].clock_rate  = 61440;
   radio[0].flags       = RETRO_LINK_PORT_SHARED_MEDIUM | (1u << 31);
   cable[0].protocol_id = "test-cable";
   cable[0].desc        = "Link";
   cable[0].clock_rate  = 61440;
   cable[0].max_ports   = 2;
   cable[0].flags       = 1u << 5;
   for (c = 0; c < 5; c++)
   {
      const struct retro_link_port_info *l = c < 3 ? radio : cable;
      CHECK(link_bus_set_ports(b, c, l));
      h[c] = link_bus_attach(b, c, 0, l[0].protocol_id, 61440);
      CHECK(h[c] != NULL);
   }
   for (c = 0; c < 3; c++)
   {
      count = 0;
      CHECK(link_bus_peers(b, c, h[c], &count) == (int)c && count == 3);
   }
   CHECK(link_bus_peers(b, 3, h[3], NULL) == -1);
   CHECK(link_bus_cable(b, 3, 0, 4, 0));
   count = 0;
   CHECK(link_bus_peers(b, 3, h[3], &count) == 0 && count == 2);
   count = 0;
   CHECK(link_bus_peers(b, 4, h[4], &count) == 1 && count == 2);
   link_bus_free(b);
}

static void null_and_foreign_handles(void)
{
   uint64_t grant = 0;
   uint32_t flags = 0;
   unsigned count = 99;

   fx_two(61440, 61440);
   fx_begin();
   CHECK(adv(1, fx_h[0][0], 0, 100, 50, &grant, &flags));
   CHECK(grant == RETRO_LINK_UNBOUNDED);
   CHECK(flags == RETRO_LINK_WAKE_DETACHED);
   CHECK(link_bus_peers(fx_bus, 1, fx_h[0][0], &count) == -1 && count == 0);
   CHECK(counts().misplaced_call == 2);

   grant = 0;
   flags = 0;
   count = 99;
   CHECK(adv(1, NULL, 0, 100, 50, &grant, &flags));
   CHECK(grant == RETRO_LINK_UNBOUNDED);
   CHECK(flags == RETRO_LINK_WAKE_DETACHED);
   CHECK(link_bus_peers(fx_bus, 1, NULL, &count) == -1 && count == 0);
   CHECK(counts().misplaced_call == 2);
   fx_free();
}

static void cabled_to_nothing_unbounded(void)
{
   uint64_t grant = 0;
   uint32_t flags = FX_ASK;

   fx_new(1);
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_attach(0);
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 0, 10, 5, &grant, &flags));
   CHECK(grant == RETRO_LINK_UNBOUNDED);
   CHECK(flags == RETRO_LINK_WAKE_NONE);
   fx_free();
}

static void peers_index_and_count(void)
{
   unsigned count = 0;

   fx_two(61440, 61440);
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == 0 && count == 2);
   count = 0;
   CHECK(link_bus_peers(fx_bus, 1, fx_h[1][0], &count) == 1 && count == 2);
   CHECK(link_bus_uncable(fx_bus, 0, 0));
   link_bus_edge(fx_bus);
   count = 99;
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == -1 && count == 0);
   count = 99;
   CHECK(link_bus_peers(fx_bus, 1, fx_h[1][0], &count) == -1 && count == 0);
   fx_free();
}

static void cable_rules(void)
{
   struct retro_link_port_info l[2];
   link_bus_t *b;
   unsigned c, count = 0;
   retro_link_port_t *h[256];

   fx_new(2);
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_port(1, 0, "test-cable", 61440, 2, 0);
   fx_attach(0);
   fx_attach(1);
   link_bus_frame_start(fx_bus, 0);
   CHECK(!link_bus_cable(fx_bus, 0, 0, 1, 0));
   link_bus_frame_end(fx_bus, 0);
   link_bus_edge(fx_bus);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   fx_free();

   fx_new(2);
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_port(1, 0, "other", 61440, 2, 0);
   fx_attach(0);
   fx_attach(1);
   CHECK(!link_bus_cable(fx_bus, 0, 0, 1, 0));
   fx_free();

   fx_new(3);
   for (c = 0; c < 3; c++)
   {
      fx_port(c, 0, "test-cable", 61440, 2, 0);
      fx_attach(c);
   }
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(!link_bus_cable(fx_bus, 1, 0, 2, 0));
   CHECK(!link_bus_cable(fx_bus, 2, 0, 0, 0));
   fx_free();

   fx_new(2);
   fx_port(0, 0, "test-radio", 61440, 0, RETRO_LINK_PORT_SHARED_MEDIUM);
   fx_port(1, 0, "test-radio", 61440, 0, RETRO_LINK_PORT_SHARED_MEDIUM);
   fx_attach(0);
   fx_attach(1);
   CHECK(!link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(!link_bus_uncable(fx_bus, 0, 0));
   fx_free();

   memset(l, 0, sizeof(l));
   l[0].protocol_id = "test-cable";
   l[0].desc        = "Link";
   l[0].clock_rate  = 61440;
   b = link_bus_new(256);
   for (c = 0; c < 256; c++)
   {
      CHECK(link_bus_set_ports(b, c, l));
      h[c] = link_bus_attach(b, c, 0, "test-cable", 61440);
      CHECK(h[c] != NULL);
   }
   for (c = 0; c + 1 < 255; c++)
      CHECK(link_bus_cable(b, c, 0, c + 1, 0));
   CHECK(link_bus_peers(b, 254, h[254], &count) == 254 && count == 255);
   CHECK(!link_bus_cable(b, 254, 0, 255, 0));
   CHECK(link_bus_peers(b, 255, h[255], NULL) == -1);
   link_bus_free(b);
}

static void shared_medium_joins(void)
{
   unsigned c, count;
   uint32_t flags;

   fx_new(4);
   for (c = 0; c < 4; c++)
      fx_port(c, 0, "test-radio", 61440, 0, RETRO_LINK_PORT_SHARED_MEDIUM);
   for (c = 0; c < 3; c++)
      fx_attach(c);
   for (c = 0; c < 3; c++)
   {
      count = 0;
      CHECK(link_bus_peers(fx_bus, c, fx_h[c][0], &count) == (int)c && count == 3);
   }
   fx_begin();
   fx_starts();
   fx_attach(3);
   CHECK(link_bus_peers(fx_bus, 3, fx_h[3][0], NULL) == -1);
   count = 0;
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == 0 && count == 3);
   frame();
   for (c = 0; c < 4; c++)
   {
      count = 0;
      CHECK(link_bus_peers(fx_bus, c, fx_h[c][0], &count) == (int)c && count == 4);
   }
   fx_begin();
   for (c = 0; c < 4; c++)
   {
      flags = 0;
      CHECK(adv(c, fx_h[c][0], 1024, 1024, 1024, NULL, &flags));
      CHECK(flags & RETRO_LINK_WAKE_TOPOLOGY);
   }
   fx_free();
}

static void grant_bounded_by_peer_promise(void)
{
   uint64_t grant = 0;

   fx_two(61440, 60000);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, 1000, 0, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 0, 2048, 1024, &grant, NULL));
   CHECK(grant == 1024);
   CHECK(!adv(0, fx_h[0][0], 0, 2048, 1025, NULL, NULL));
   fx_free();
}

static void rounding_down_128(void)
{
   uint64_t x = ((uint64_t)0x0044B82FUL << 32) | 0x98895147UL;

   fx_two(61440, 60000);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, 1001, 0, NULL, NULL));
   CHECK(!adv(0, fx_h[0][0], 0, 2048, 1026, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 0, 2048, 1025, NULL, NULL));
   fx_free();

   fx_two(1000000007, 16777216);
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 0, ((uint64_t)1 << 60) + 1, 0, NULL, NULL));
   CHECK(!adv(1, fx_h[1][0], 0, (uint64_t)1 << 56, x + 1, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, (uint64_t)1 << 56, x, NULL, NULL));
   fx_free();
}

static void promise_never_lowered(void)
{
   fx_two(61440, 60000);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, 1000, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, 500, 0, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 0, 2048, 1024, NULL, NULL));
   CHECK(counts().lowered_promise == 1);
   fx_free();
}

static void request_capped_at_promise(void)
{
   uint64_t grant = 0;

   fx_two(61440, 61440);
   fx_begin();
   fx_far(1);
   CHECK(adv(0, fx_h[0][0], 0, 512, 800, &grant, NULL));
   CHECK(grant == 512);
   CHECK(counts().request_past_promise == 1);
   fx_free();
}

static void frame_end_repair(void)
{
   uint64_t grant = 0;

   fx_two(61440, 61440);
   fx_begin();
   fx_far(1);
   CHECK(adv(0, fx_h[0][0], 0, 4096, 0, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 0, 4096, 1024, &grant, NULL));
   CHECK(grant == 1024);
   frame();
   fx_begin();
   fx_far(1);
   CHECK(adv(0, fx_h[0][0], 1024, 4096, 2100, &grant, NULL));
   CHECK(grant == 2048);
   CHECK(counts().request_past_frame_end == 1);
   fx_free();
}

static void no_frame_end_repair_first_frame(void)
{
   uint64_t grant = 0;

   fx_two(61440, 61440);
   fx_begin();
   fx_far(1);
   CHECK(adv(0, fx_h[0][0], 0, 4096, 2100, &grant, NULL));
   CHECK(grant == 2100);
   CHECK(counts().request_past_frame_end == 0);
   fx_free();
}

static void earliest_promise_multi(void)
{
   struct retro_link_wait w[2];

   fx_new(3);
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_port(0, 1, "test-cable", 60000, 2, 0);
   fx_port(1, 0, "test-cable", 61440, 2, 0);
   fx_port(2, 0, "test-cable", 60000, 2, 0);
   fx_attach(0);
   fx_attach(1);
   fx_attach(2);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_cable(fx_bus, 0, 1, 2, 0));
   fx_begin();
   fx_far(1);
   fx_far(2);
   fx_wait(&w[0], fx_h[0][0], 0, 2048, 2048, false);
   fx_wait(&w[1], fx_h[0][1], 0, 500, 500, false);
   CHECK(advm(0, w, 2));
   CHECK(w[0].grant == 512);
   CHECK(w[1].grant == 500);
   CHECK(counts().request_past_earliest == 1);
   fx_free();
}

static void earliest_promise_skips_unbounded(void)
{
   struct retro_link_wait w[2];

   fx_new(2);
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_port(0, 1, "test-cable", 60000, 2, 0);
   fx_port(1, 0, "test-cable", 61440, 2, 0);
   fx_attach(0);
   fx_attach(1);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   fx_begin();
   fx_far(1);
   fx_wait(&w[0], fx_h[0][0], 0, 2048, 1024, false);
   fx_wait(&w[1], fx_h[0][1], 0, 0, 0, false);
   CHECK(advm(0, w, 2));
   CHECK(w[0].grant == 1024);
   CHECK(w[1].grant == RETRO_LINK_UNBOUNDED);
   CHECK(counts().request_past_earliest == 0);
   fx_free();
}

static void unknown_edge_tick_bounds_at_edge(void)
{
   fx_two(61440, 60000);
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 0, 4096, 0, NULL, NULL));
   CHECK(!adv(0, fx_h[0][0], 0, 4096, 1, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, 1000, 0, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 0, 4096, 1024, NULL, NULL));
   CHECK(!adv(0, fx_h[0][0], 0, 4096, 1025, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, 3000, 0, NULL, NULL));
   frame();

   /* Re-attached at the edge: its old promise (3000) binds nothing */
   link_bus_detach(fx_bus, 1, fx_h[1][0]);
   CHECK(link_bus_attach(fx_bus, 1, 0, "test-cable", 60000) == fx_h[1][0]);
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 1024, 4096, 1024, NULL, NULL));
   CHECK(!adv(0, fx_h[0][0], 1024, 4096, 1025, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 1000, 1500, 1000, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 1024, 4096, 1536, NULL, NULL));
   CHECK(!adv(0, fx_h[0][0], 1024, 4096, 1537, NULL, NULL));
   fx_free();
}

static void detached_peer_silent(void)
{
   uint64_t grant = 0;
   uint32_t flags = 0;
   unsigned count = 0;

   fx_two(61440, 61440);
   fx_run_frame();
   fx_run_frame();
   fx_run_frame();
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 3072, 8192, 3072, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY); /* the joining, never asked for */
   CHECK(adv(1, fx_h[1][0], 3072, 3100, 3072, NULL, NULL));
   CHECK(!adv(0, fx_h[0][0], 3072, 8192, 4000, NULL, NULL));
   link_bus_detach(fx_bus, 1, fx_h[1][0]);
   CHECK(adv(0, fx_h[0][0], 3072, 8192, 4000, &grant, NULL));
   CHECK(grant == 4000);
   flags = 0;
   CHECK(adv(1, fx_h[1][0], 3072, 3100, 3200, &grant, &flags));
   CHECK(grant == RETRO_LINK_UNBOUNDED);
   CHECK(flags == RETRO_LINK_WAKE_DETACHED);
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == 0 && count == 2);
   frame();
   count = 99;
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == -1 && count == 0);
   fx_begin();
   flags = 0;
   CHECK(adv(0, fx_h[0][0], 4096, 8192, 4096, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY);
   flags = FX_ASK;
   CHECK(adv(0, fx_h[0][0], 4096, 8192, 4096, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_NONE);
   fx_free();
}

static void reattach_in_frame_reports_peers(void)
{
   uint32_t flags;
   unsigned count = 0;

   fx_two(61440, 61440);
   fx_begin();
   flags = 0;
   CHECK(adv(0, fx_h[0][0], 0, 0, 0, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY); /* the joining */
   frame();
   fx_begin();
   fx_starts();
   link_bus_detach(fx_bus, 1, fx_h[1][0]);
   CHECK(link_bus_attach(fx_bus, 1, 0, "test-cable", 61440) == fx_h[1][0]);
   frame();
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == 0 && count == 2);
   fx_begin();
   flags = 0;
   CHECK(adv(0, fx_h[0][0], 2048, 2048, 2048, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY);
   flags = 0;
   CHECK(adv(1, fx_h[1][0], 2048, 2048, 2048, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY);
   flags = FX_ASK;
   CHECK(adv(0, fx_h[0][0], 2048, 2048, 2048, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_NONE);
   fx_free();
}

static void first_report_after_attach(void)
{
   uint32_t flags;

   fx_new(3);
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_port(1, 0, "test-cable", 61440, 2, 0);
   fx_port(2, 0, "test-cable", 61440, 2, 0);
   fx_attach(0);
   fx_attach(1);
   fx_attach(2);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   fx_begin();
   fx_far(1);
   flags = 0;
   CHECK(adv(0, fx_h[0][0], 0, 1024, 0, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY);
   flags = FX_ASK;
   CHECK(adv(2, fx_h[2][0], 0, 1024, 0, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_NONE);
   flags = FX_ASK;
   CHECK(adv(0, fx_h[0][0], 0, 1024, 0, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_NONE);
   flags = FX_ASK;
   CHECK(adv(2, fx_h[2][0], 0, 1024, 0, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_NONE);
   fx_free();
}

static void reports_pending_until_asked(void)
{
   uint32_t flags;
   uint64_t grant = 0;

   fx_two(61440, 61440);
   fx_begin();
   flags = 0;
   CHECK(adv(0, fx_h[0][0], 0, 0, 0, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY);
   frame();
   CHECK(link_bus_uncable(fx_bus, 0, 0));
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 1024, 2048, 1024, &grant, NULL));
   CHECK(grant == RETRO_LINK_UNBOUNDED);
   CHECK(adv(0, fx_h[0][0], 1024, 2048, 1500, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 1500, 2048, 2000, NULL, NULL));
   flags = 0;
   CHECK(adv(0, fx_h[0][0], 2000, 2048, 2048, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY);
   flags = FX_ASK;
   CHECK(adv(0, fx_h[0][0], 2048, 2048, 2048, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_NONE);
   fx_free();
}

static void extreme_ticks(void)
{
   uint64_t top   = RETRO_LINK_UNBOUNDED;
   uint64_t local = top - 1500 + 1024;
   uint64_t grant = 0;

   fx_two(61440, 61440);
   fx_tick[0][0] = top - 1500;
   fx_begin();
   fx_far(1);
   CHECK(adv(0, fx_h[0][0], top - 1500, top - 1500, top - 1500, NULL, NULL));
   frame();
   CHECK(fx_tick[0][0] == local);
   fx_begin();
   fx_far(1);
   CHECK(adv(0, fx_h[0][0], local, top - 1, top, &grant, NULL));
   CHECK(grant == top - 1);
   CHECK(grant != RETRO_LINK_UNBOUNDED);
   CHECK(grant >= local);
   CHECK(counts().request_past_promise == 1);
   fx_free();
}

static void publish_idempotent(void)
{
   struct retro_link_wait w;
   struct link_bus_blocker why;

   fx_two(61440, 60000);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, 1000, 0, NULL, NULL));
   fx_wait(&w, fx_h[0][0], 0, 2048, 1025, false);
   link_bus_publish(fx_bus, 0, &w, 1);
   link_bus_publish(fx_bus, 0, &w, 1);
   CHECK(!link_bus_grantable(fx_bus, 0, &w, 1));
   memset(&why, 0, sizeof(why));
   CHECK(link_bus_blocked_by(fx_bus, 0, &w, 1, &why));
   CHECK(why.bound == 1024);
   fx_wait(&w, fx_h[0][0], 0, 2048, 1024, false);
   link_bus_publish(fx_bus, 0, &w, 1);
   link_bus_publish(fx_bus, 0, &w, 1);
   CHECK(link_bus_grantable(fx_bus, 0, &w, 1));
   link_bus_grant(fx_bus, 0, &w, 1);
   CHECK(w.grant == 1024);
   fx_free();
}

static void blocked_by_names_peer(void)
{
   struct retro_link_wait w;
   struct link_bus_blocker why;

   fx_two(61440, 60000);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, 1000, 0, NULL, NULL));
   fx_wait(&w, fx_h[0][0], 0, 2048, 1025, false);
   CHECK(!advm(0, &w, 1));
   memset(&why, 0xff, sizeof(why));
   CHECK(link_bus_blocked_by(fx_bus, 0, &w, 1, &why));
   CHECK(why.entry == 0 && why.port == 0);
   CHECK(why.request == 1025 && why.bound == 1024);
   CHECK(why.peer_console == 1 && why.peer_port == 0);
   CHECK(!why.edge_unknown);
   fx_wait(&w, fx_h[0][0], 0, 2048, 1024, false);
   CHECK(advm(0, &w, 1));
   CHECK(!link_bus_blocked_by(fx_bus, 0, &w, 1, &why));
   fx_free();

   fx_two(61440, 60000);
   fx_begin();
   fx_wait(&w, fx_h[0][0], 0, 2048, 1025, false);
   CHECK(!advm(0, &w, 1));
   memset(&why, 0, sizeof(why));
   CHECK(link_bus_blocked_by(fx_bus, 0, &w, 1, &why));
   CHECK(why.bound == 0);
   CHECK(why.peer_console == 1 && why.peer_port == 0);
   CHECK(why.edge_unknown);
   fx_free();
}

static void changes_count_moves(void)
{
   uint64_t before;

   fx_two(61440, 61440);
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 0, 0, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, FX_FAR, 0, NULL, NULL));
   before = link_bus_changes(fx_bus, 0);
   CHECK(adv(1, fx_h[1][0], 0, FX_FAR, 0, NULL, NULL));
   CHECK(link_bus_changes(fx_bus, 0) == before);
   CHECK(adv(1, fx_h[1][0], 0, FX_FAR + 1, 0, NULL, NULL));
   CHECK(link_bus_changes(fx_bus, 0) != before);
   fx_free();
}

static void send_results(void)
{
   static unsigned char buf[RETRO_LINK_MAX_PAYLOAD + 1];
   struct link_bus_sent j[4];
   size_t len;

   fx_two(61440, 61440);
   fx_begin();
   fx_starts();
   CHECK(adv(0, fx_h[0][0], 0, 100, 0, NULL, NULL));
   CHECK(!link_bus_send(fx_bus, 0, NULL, 100, 1, buf, 4));
   CHECK(!link_bus_send(fx_bus, 1, fx_h[0][0], 100, 1, buf, 4));
   CHECK(counts().misplaced_call == 1);
   CHECK(!snd(0, 0, 100, 1, NULL, 4));
   CHECK(!snd(0, 0, 100, 1, buf, RETRO_LINK_MAX_PAYLOAD + 1));
   CHECK(snd(0, 0, 100, 1, buf, RETRO_LINK_MAX_PAYLOAD));
   CHECK(snd(0, 0, 100, 1, NULL, 0));
   CHECK(!snd(0, 0, 100, 0, buf, 4));
   CHECK(!snd(0, 0, 100, 5, buf, 4));
   CHECK(link_bus_journal(fx_bus, j, 4) == 2);
   CHECK(j[0].len == RETRO_LINK_MAX_PAYLOAD && j[1].len == 0);
   CHECK(adv(0, fx_h[0][0], 0, 200, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, 200, 101, NULL, NULL));
   len = sizeof(buf);
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, buf, &len));
   CHECK(len == RETRO_LINK_MAX_PAYLOAD);
   len = sizeof(buf);
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, buf, &len) && len == 0);
   len = sizeof(buf);
   CHECK(!link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, buf, &len) && len == 0);
   frame();

   CHECK(link_bus_uncable(fx_bus, 0, 0));
   fx_begin();
   fx_starts();
   CHECK(snd(0, 0, 1024, 1, buf, 4));
   CHECK(snd(0, 0, 1024, 5, buf, 4));
   CHECK(link_bus_journal(fx_bus, j, 4) == 0);
   fx_free();
}

static void stamp_raised_to_promise(void)
{
   struct link_bus_sent j[3];

   fx_two(61440, 61440);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, 0, 0, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 0, 2048, 0, NULL, NULL));
   CHECK(snd(0, 0, 100, 1, "a", 1));
   CHECK(adv(0, fx_h[0][0], 1000, 2048, 1000, NULL, NULL));
   CHECK(snd(0, 0, 500, 1, "b", 1));
   CHECK(link_bus_journal(fx_bus, j, 3) == 2);
   CHECK(j[0].stamp == 2048 && j[0].landing == 2048);
   CHECK(j[1].stamp == 2048 && j[1].landing == 2048);
   CHECK(j[0].from_console == 0 && j[0].from_port == 0);
   CHECK(j[0].to_console == 1 && j[0].to_port == 0);
   CHECK(j[0].len == 1 && j[0].head[0] == 'a' && j[0].head[1] == 0);
   CHECK(counts().low_stamp == 2);
   fx_free();
}

static void landing_rounds_up(void)
{
   uint64_t stamp = ((uint64_t)1 << 60) + 1;
   uint64_t want  = ((uint64_t)0x0044B82FUL << 32) | 0x98895148UL;
   struct link_bus_sent j;

   fx_two(61440, 60000);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, 0, 0, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 0, 1025, 0, NULL, NULL));
   CHECK(snd(0, 0, 1025, 1, "a", 1));
   CHECK(link_bus_journal(fx_bus, &j, 1) == 1 && j.landing == 1001);
   fx_free();

   fx_two(1000000007, 16777216);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, 0, 0, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 0, stamp, 0, NULL, NULL));
   CHECK(snd(0, 0, stamp, 1, "a", 1));
   CHECK(link_bus_journal(fx_bus, &j, 1) == 1 && j.stamp == stamp && j.landing == want);
   fx_free();
}

static void receivable_below_grant(void)
{
   uint64_t grant = 0, tick = 0;
   unsigned from  = 99;
   size_t len     = 99;

   fx_two(61440, 60000);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, 2000, 0, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 0, 1025, 0, NULL, NULL));
   CHECK(snd(0, 0, 1025, 1, "a", 1));
   CHECK(adv(0, fx_h[0][0], 0, 1027, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, 2000, 1001, &grant, NULL) && grant == 1001);
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], &tick, &from, &len) == NULL && len == 0);
   CHECK(adv(1, fx_h[1][0], 0, 2000, 1002, &grant, NULL) && grant == 1002);
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], &tick, &from, &len) != NULL);
   CHECK(tick == 1001 && from == 0 && len == 1);
   fx_free();
}

static void order_landing_sender_sent(void)
{
   static const uint64_t want_tick[4] = { 2000, 3000, 3000, 3000 };
   static const unsigned want_from[4] = { 2, 1, 1, 2 };
   static const char want_tag[4]      = { 'd', 'b', 'c', 'a' };
   unsigned c, i;

   fx_new(3);
   for (c = 0; c < 3; c++)
   {
      fx_port(c, 0, "test-cable", 61440, 3, 0);
      fx_attach(c);
   }
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_cable(fx_bus, 1, 0, 2, 0));
   fx_begin();
   fx_starts();
   CHECK(snd(2, 0, 3000, 0, "a", 1));
   CHECK(snd(1, 0, 3000, 0, "b", 1));
   CHECK(snd(1, 0, 3000, 0, "c", 1));
   CHECK(snd(2, 0, 2000, 0, "d", 1));
   CHECK(adv(1, fx_h[1][0], 0, 4096, 0, NULL, NULL));
   CHECK(adv(2, fx_h[2][0], 0, 4096, 0, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 0, 4096, 3001, NULL, NULL));
   for (i = 0; i < 4; i++)
   {
      char tag      = 0;
      uint64_t tick = 0;
      unsigned from = 99;
      size_t len    = 1;
      CHECK(link_bus_recv(fx_bus, 0, fx_h[0][0], &tick, &from, &tag, &len));
      CHECK(tick == want_tick[i] && from == want_from[i] && tag == want_tag[i]);
   }
   fx_free();
}

static void recv_buffer_rules(void)
{
   char buf[16];
   size_t len = 99;

   fx_two(61440, 61440);
   fx_begin();
   fx_starts();
   CHECK(!link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, buf, &len) && len == 0);
   CHECK(adv(0, fx_h[0][0], 0, 100, 0, NULL, NULL));
   CHECK(snd(0, 0, 100, 1, "0123456789", 10));
   CHECK(snd(0, 0, 100, 1, "abcdefghij", 10));
   CHECK(adv(0, fx_h[0][0], 0, 200, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, 200, 101, NULL, NULL));
   len = 4;
   CHECK(!link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, buf, &len) && len == 10);
   len = 0;
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], NULL, NULL, &len) != NULL && len == 10);
   len = sizeof(buf);
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, buf, &len));
   CHECK(len == 10 && !memcmp(buf, "0123456789", 10));
   len = 0;
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, NULL, &len));
   len = 99;
   CHECK(!link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, buf, &len) && len == 0);
   fx_free();
}

static void peek_lends_in_place(void)
{
   const void *first, *again;
   size_t len = 0;

   fx_two(61440, 61440);
   fx_begin();
   fx_starts();
   CHECK(adv(0, fx_h[0][0], 0, 100, 0, NULL, NULL));
   CHECK(snd(0, 0, 100, 1, "first", 5));
   CHECK(snd(0, 0, 100, 1, "second", 6));
   CHECK(adv(0, fx_h[0][0], 0, 200, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, 200, 101, NULL, NULL));
   first = link_bus_peek(fx_bus, 1, fx_h[1][0], NULL, NULL, &len);
   CHECK(first != NULL && len == 5 && !memcmp(first, "first", 5));
   again = link_bus_peek(fx_bus, 1, fx_h[1][0], NULL, NULL, &len);
   CHECK(again == first);
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, NULL, &len));
   again = link_bus_peek(fx_bus, 1, fx_h[1][0], NULL, NULL, &len);
   CHECK(again != NULL && len == 6 && !memcmp(again, "second", 6));
   CHECK(again != first); /* each message lent from its own storage */
   fx_free();
}

static void broadcast_membership_at_send(void)
{
   struct link_bus_sent j[3];
   unsigned c;

   fx_new(4);
   for (c = 0; c < 4; c++)
      fx_port(c, 0, "test-cable", 61440, 4, 0);
   for (c = 0; c < 3; c++)
      fx_attach(c);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_cable(fx_bus, 1, 0, 2, 0));
   fx_begin();
   fx_all_ports(0, false);
   fx_all_ports(2, false);
   fx_attach(3);
   CHECK(snd(0, 0, 100, RETRO_LINK_BROADCAST, "a", 1));
   CHECK(link_bus_journal(fx_bus, j, 3) == 2);
   CHECK(j[0].to_console == 1 && j[1].to_console == 2);
   fx_all_ports(1, false);
   frame();
   CHECK(link_bus_cable(fx_bus, 2, 0, 3, 0));
   CHECK(link_bus_journal(fx_bus, j, 3) == 2);
   fx_begin();
   fx_starts();
   for (c = 1; c < 4; c++)
   {
      size_t len    = 99;
      const void *m = link_bus_peek(fx_bus, c, fx_h[c][0], NULL, NULL, &len);
      CHECK(c < 3 ? (m != NULL && len == 1) : (m == NULL && len == 0));
   }
   fx_free();
}

static void from_is_index_at_send(void)
{
   uint64_t tick = 0;
   unsigned c, from = 99, count = 0;
   char tag   = 0;
   size_t len = 1;

   fx_new(3);
   for (c = 0; c < 3; c++)
   {
      fx_port(c, 0, "test-cable", 61440, 3, 0);
      fx_attach(c);
   }
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_cable(fx_bus, 0, 0, 2, 0));
   fx_begin();
   fx_starts();
   CHECK(snd(2, 0, 2000, 0, "a", 1));
   frame();
   CHECK(link_bus_uncable(fx_bus, 1, 0));
   CHECK(link_bus_peers(fx_bus, 2, fx_h[2][0], &count) == 1 && count == 2);
   fx_begin();
   fx_starts();
   CHECK(adv(2, fx_h[2][0], 1024, 4096, 1024, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 1024, 4096, 2001, NULL, NULL));
   CHECK(link_bus_recv(fx_bus, 0, fx_h[0][0], &tick, &from, &tag, &len));
   CHECK(tick == 2000 && from == 2 && tag == 'a');
   fx_free();
}

/* Console 1 has taken the joining report; console 0 sends to it at 600,
 * then promises 700 */
static void fx_wake_message(void)
{
   uint32_t flags = 0;

   fx_two(61440, 61440);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, 4096, 0, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY);
   CHECK(adv(0, fx_h[0][0], 0, 600, 0, NULL, NULL));
   CHECK(snd(0, 0, 600, 1, "a", 1));
   CHECK(adv(0, fx_h[0][0], 0, 700, 0, NULL, NULL));
}

static void wake_message_report(void)
{
   uint32_t flags;

   fx_wake_message();
   flags = FX_ASK;
   CHECK(adv(1, fx_h[1][0], 0, 4096, 600, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_NONE);
   flags = 0;
   CHECK(adv(1, fx_h[1][0], 0, 4096, 601, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_MESSAGE);
   flags = FX_ASK;
   CHECK(adv(1, fx_h[1][0], 0, 4096, 602, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_NONE);
   fx_free();

   fx_wake_message();
   CHECK(adv(1, fx_h[1][0], 0, 4096, 601, NULL, NULL));
   flags = 0;
   CHECK(adv(1, fx_h[1][0], 0, 4096, 602, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_MESSAGE);
   fx_free();

   /* Taken before the advance that asks: still reported */
   fx_wake_message();
   CHECK(adv(1, fx_h[1][0], 0, 4096, 601, NULL, NULL));
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, NULL, NULL));
   flags = 0;
   CHECK(adv(1, fx_h[1][0], 0, 4096, 602, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_MESSAGE);
   fx_free();
}

static void detach_drops_messages_at_edge(void)
{
   uint64_t grant = 0, tick = 0;
   size_t len     = 0;

   fx_two(61440, 61440);
   fx_run_frame();
   fx_run_frame();
   fx_run_frame();
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 3072, 4096, 3072, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 3072, 3100, 3072, NULL, NULL));
   CHECK(snd(0, 0, 3100, 1, "a", 1));
   CHECK(adv(0, fx_h[0][0], 3072, 3200, 3072, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 3072, 4096, 3101, &grant, NULL) && grant == 3101);
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], &tick, NULL, &len) != NULL && tick == 3100);
   link_bus_detach(fx_bus, 1, fx_h[1][0]);
   len = 99;
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], NULL, NULL, &len) == NULL && len == 0);
   CHECK(snd(0, 0, 3200, 1, "b", 1)); /* to a silent member: dropped at the edge */
   frame();
   CHECK(link_bus_attach(fx_bus, 1, 0, "test-cable", 61440) == fx_h[1][0]);
   fx_begin();
   fx_starts();
   len = 99;
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], NULL, NULL, &len) == NULL && len == 0);
   len = 99;
   CHECK(!link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, NULL, &len) && len == 0);

   /* A detach at an edge drops them at once */
   CHECK(snd(0, 0, 4200, 1, "c", 1));
   frame();
   link_bus_detach(fx_bus, 1, fx_h[1][0]);
   CHECK(link_bus_attach(fx_bus, 1, 0, "test-cable", 61440) == fx_h[1][0]);
   fx_begin();
   fx_starts();
   len = 99;
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], NULL, NULL, &len) == NULL && len == 0);
   fx_free();
}

/* Frame 0: console 0 sends to console 1 at 512, received when deliver;
 * ends at edge 1 */
static void fx_hold(uint64_t idle_ticks, bool deliver)
{
   char tag;
   size_t len = 1;

   fx_new(2);
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_port(1, 0, "test-cable", 61440, 2, 0);
   fx_list[0][0].idle_ticks = idle_ticks;
   fx_attach(0);
   fx_attach(1);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   fx_begin();
   fx_starts();
   CHECK(snd(0, 0, 512, 1, "a", 1));
   CHECK(adv(0, fx_h[0][0], 0, 1024, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, 1024, 513, NULL, NULL));
   if (deliver)
      CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, &tag, &len));
   frame();
}

static void hold_needs_idle(void)
{
   char tag;
   size_t len = 1;
   unsigned edge;

   fx_hold(0, true);
   for (edge = 1; edge <= 30; edge++)
   {
      CHECK(fx_tick[0][0] == 1024 * edge);
      CHECK(!link_bus_hold(fx_bus, 1));
      fx_run_frame();
   }
   CHECK(fx_tick[0][0] == 31744);
   CHECK(!link_bus_is_held(fx_bus, 1));
   CHECK(link_bus_hold(fx_bus, 1));
   CHECK(link_bus_is_held(fx_bus, 1));
   fx_free();

   fx_hold(2048, true);
   CHECK(!link_bus_hold(fx_bus, 1)); /* edge 1 */
   fx_run_frame();
   CHECK(!link_bus_hold(fx_bus, 1)); /* edge 2 */
   fx_run_frame();
   CHECK(link_bus_hold(fx_bus, 1));  /* edge 3 */
   fx_free();

   fx_hold(2048, false);
   fx_run_frame();
   fx_run_frame();
   CHECK(!link_bus_hold(fx_bus, 1)); /* edge 3, undelivered */
   CHECK(!link_bus_hold(fx_bus, 0));
   fx_begin();
   fx_starts();
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, &tag, &len));
   CHECK(!link_bus_hold(fx_bus, 1)); /* mid-frame */
   frame();
   CHECK(link_bus_hold(fx_bus, 1));  /* edge 4 */
   fx_free();

   /* A stamp exactly the quiet span before the anchor is quiet */
   fx_hold(2560, true);
   fx_run_frame();
   CHECK(!link_bus_hold(fx_bus, 1)); /* edge 2 */
   fx_run_frame();
   CHECK(link_bus_hold(fx_bus, 1));  /* edge 3: 3072 - 512 */
   fx_free();

   /* A port that has sent and has no learned span is not quiet: frame 40
    * has no frame-end publish from console 0 */
   fx_hold(0, true);
   for (edge = 1; edge < 40; edge++)
      fx_run_frame();
   fx_begin();
   fx_starts();
   fx_all_ports(1, true);
   link_bus_frame_end(fx_bus, 0);
   link_bus_frame_end(fx_bus, 1);
   link_bus_edge(fx_bus);
   fx_tick[0][0] += 1024;
   fx_tick[1][0] += 1024;
   CHECK(!link_bus_hold(fx_bus, 1)); /* edge 41 */
   fx_run_frame();
   CHECK(link_bus_hold(fx_bus, 1));  /* edge 42 */
   fx_free();
}

static void held_peer_silent(void)
{
   uint64_t grant = 0;

   fx_two(61440, 61440);
   fx_begin();
   fx_starts();
   CHECK(adv(1, fx_h[1][0], 1024, 1100, 1024, NULL, NULL));
   frame();
   CHECK(link_bus_hold(fx_bus, 1));
   fx_begin();
   fx_starts();
   CHECK(adv(0, fx_h[0][0], 1024, 4096, 1500, &grant, NULL));
   CHECK(grant == 1500);
   fx_free();
}

static void held_console_message_lands_at_rejoin(void)
{
   struct retro_link_wait w[2];
   struct link_bus_sent j;
   uint64_t tick = 0;
   unsigned from = 99;
   size_t len    = 0;

   fx_new(3);
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_port(1, 0, "test-cable", 61440, 2, 0);
   fx_port(1, 1, "test-cable", 61440, 2, 0);
   fx_port(2, 0, "test-cable", 61440, 2, 0);
   fx_attach(0);
   fx_attach(1);
   fx_attach(2);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   fx_begin();
   fx_wait(&w[0], fx_h[1][0], 0, 0, 0, true);
   fx_wait(&w[1], fx_h[1][1], 0, 0, 0, true);
   CHECK(advm(1, w, 2));
   CHECK(w[0].wake_flags == RETRO_LINK_WAKE_TOPOLOGY);
   CHECK(w[1].wake_flags == RETRO_LINK_WAKE_NONE);
   fx_all_ports(0, false);
   fx_all_ports(2, false);
   frame();
   CHECK(link_bus_cable(fx_bus, 1, 1, 2, 0));
   CHECK(link_bus_hold(fx_bus, 1));  /* edge 1 */
   fx_run_frame();
   fx_run_frame();
   fx_begin();                       /* frame 3 */
   fx_starts();
   CHECK(snd(0, 0, 3500, 1, "a", 1));
   frame();
   CHECK(link_bus_journal(fx_bus, &j, 1) == 1);
   CHECK(j.to_console == 1 && j.stamp == 3500 && j.landing == RETRO_LINK_UNBOUNDED);
   fx_run_frame();
   CHECK(link_bus_unhold(fx_bus, 1)); /* edge 5 */
   CHECK(!link_bus_is_held(fx_bus, 1));
   CHECK(fx_tick[1][0] == 1024 && fx_tick[0][0] == 5120);

   fx_begin();
   fx_wait(&w[0], fx_h[1][0], 1024, 2048, 1024, true);
   fx_wait(&w[1], fx_h[1][1], 1024, 2048, 1024, true);
   CHECK(advm(1, w, 2));
   CHECK(w[0].grant == 1024 && w[0].wake_flags == RETRO_LINK_WAKE_NONE);
   CHECK(w[1].wake_flags == RETRO_LINK_WAKE_TOPOLOGY); /* kept over the hold */
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], NULL, NULL, &len) == NULL);
   CHECK(adv(0, fx_h[0][0], 5120, 6144, 5120, NULL, NULL));
   CHECK(adv(2, fx_h[2][0], 5120, 6144, 5120, NULL, NULL));
   fx_wait(&w[0], fx_h[1][0], 1024, 2048, 1025, true);
   fx_wait(&w[1], fx_h[1][1], 1024, 2048, 1025, true);
   CHECK(advm(1, w, 2));
   CHECK(w[0].grant == 1025 && w[0].wake_flags == RETRO_LINK_WAKE_MESSAGE);
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], &tick, &from, &len) != NULL);
   CHECK(tick == 1024 && from == 0);
   fx_free();
}

static void unknown_anchor_message_resolves(void)
{
   struct link_bus_sent j;
   uint64_t tick = 0;
   size_t len    = 0;

   fx_two(61440, 60000);
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 0, 76, 0, NULL, NULL));
   CHECK(snd(0, 0, 76, 1, "a", 1));
   CHECK(link_bus_journal(fx_bus, &j, 1) == 1 && j.landing == RETRO_LINK_UNBOUNDED);
   CHECK(adv(1, fx_h[1][0], 1000, 2000, 1000, NULL, NULL));
   CHECK(link_bus_journal(fx_bus, &j, 1) == 1 && j.landing == 1075);
   CHECK(adv(0, fx_h[0][0], 0, 78, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 1000, 2000, 1075, NULL, NULL));
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], NULL, NULL, &len) == NULL);
   CHECK(adv(1, fx_h[1][0], 1000, 2000, 1076, NULL, NULL));
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], &tick, NULL, &len) != NULL && tick == 1075);
   CHECK(!adv(1, fx_h[1][0], 1000, 2000, 1077, NULL, NULL));
   fx_free();
}

static void steady_state_allocates_nothing(void)
{
   static const size_t lens[4] = { 1, 100, 1000, RETRO_LINK_MAX_PAYLOAD };
   static unsigned char buf[RETRO_LINK_MAX_PAYLOAD];
   unsigned f, c, i, got[2] = { 0, 0 }, ok = 0;
   uint64_t blocks = 0;

   fx_two(61440, 61440);
   for (f = 0; f <= 200; f++)
   {
      fx_begin();
      for (c = 0; c < 2; c++)
         CHECK(adv(c, fx_h[c][0], fx_tick[c][0], fx_tick[c][0] + 512, fx_tick[c][0], NULL, NULL));
      for (c = 0; c < 2; c++)
      {
         size_t len = sizeof(buf);
         while (link_bus_recv(fx_bus, c, fx_h[c][0], NULL, NULL, buf, &len))
         {
            if (len == lens[got[c] % 4])
               ok++;
            got[c]++;
            len = sizeof(buf);
         }
      }
      if (f < 200)
         for (c = 0; c < 2; c++)
            for (i = 0; i < 4; i++)
               CHECK(snd(c, 0, fx_tick[c][0] + 512, 1 - c, buf, lens[i]));
      frame();
      if (f == 20)
         blocks = counts().blocks;
   }
   CHECK(got[0] == 800 && got[1] == 800 && ok == 1600);
   CHECK(blocks > 0 && counts().blocks == blocks);
   fx_free();
}

static void thousands_in_flight(void)
{
   unsigned i, sent = 0, got = 0, ok = 0;

   fx_two(61440, 61440);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, FX_FAR, 0, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 0, 2048, 0, NULL, NULL));
   for (i = 0; i < 10000; i++)
   {
      unsigned char pay[4];
      pay[0] = i & 0xFF;
      pay[1] = (i >> 8) & 0xFF;
      pay[2] = (i >> 16) & 0xFF;
      pay[3] = 0;
      if (snd(0, 0, 2048 + i, 1, pay, 4))
         sent++;
   }
   CHECK(sent == 10000);
   CHECK(adv(0, fx_h[0][0], 0, FX_FAR, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, FX_FAR, 2048 + 10000, NULL, NULL));
   for (;;)
   {
      unsigned char pay[4];
      uint64_t tick = 0;
      size_t len    = sizeof(pay);
      if (!link_bus_recv(fx_bus, 1, fx_h[1][0], &tick, NULL, pay, &len))
         break;
      if (     len == 4 && tick == 2048 + got
            && (pay[0] | (pay[1] << 8) | ((unsigned)pay[2] << 16)) == got)
         ok++;
      got++;
   }
   CHECK(got == 10000 && ok == 10000);
   CHECK(counts().low_stamp == 0);
   CHECK(counts().blocks > 0);
   fx_free();
}

static void activity_counts_sends_and_recvs(void)
{
   uint64_t a0, a1, c1;
   char tag;
   size_t len = 1;

   fx_two(61440, 61440);
   fx_begin();
   fx_starts();
   a0 = link_bus_activity(fx_bus, 0);
   a1 = link_bus_activity(fx_bus, 1);
   CHECK(adv(0, fx_h[0][0], 0, 100, 0, NULL, NULL));
   CHECK(link_bus_activity(fx_bus, 0) == a0);
   c1 = link_bus_changes(fx_bus, 1);
   CHECK(snd(0, 0, 100, 1, "a", 1));
   CHECK(link_bus_activity(fx_bus, 0) != a0);
   CHECK(link_bus_changes(fx_bus, 1) != c1);
   CHECK(link_bus_activity(fx_bus, 1) == a1);
   CHECK(adv(0, fx_h[0][0], 0, 200, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, 200, 101, NULL, NULL));
   CHECK(link_bus_activity(fx_bus, 1) == a1);
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, &tag, &len));
   CHECK(link_bus_activity(fx_bus, 1) != a1);
   fx_free();
}

static const struct
{
   const char *name;
   void (*run)(void);
} tests[] = {
   { "attach_handles",                       attach_handles },
   { "attach_matches_port_list",             attach_matches_port_list },
   { "unknown_flag_bits_ignored",            unknown_flag_bits_ignored },
   { "null_and_foreign_handles",             null_and_foreign_handles },
   { "cabled_to_nothing_unbounded",          cabled_to_nothing_unbounded },
   { "peers_index_and_count",                peers_index_and_count },
   { "cable_rules",                          cable_rules },
   { "shared_medium_joins",                  shared_medium_joins },
   { "grant_bounded_by_peer_promise",        grant_bounded_by_peer_promise },
   { "rounding_down_128",                    rounding_down_128 },
   { "promise_never_lowered",                promise_never_lowered },
   { "request_capped_at_promise",            request_capped_at_promise },
   { "frame_end_repair",                     frame_end_repair },
   { "no_frame_end_repair_first_frame",      no_frame_end_repair_first_frame },
   { "earliest_promise_multi",               earliest_promise_multi },
   { "earliest_promise_skips_unbounded",     earliest_promise_skips_unbounded },
   { "unknown_edge_tick_bounds_at_edge",     unknown_edge_tick_bounds_at_edge },
   { "detached_peer_silent",                 detached_peer_silent },
   { "reattach_in_frame_reports_peers",      reattach_in_frame_reports_peers },
   { "first_report_after_attach",            first_report_after_attach },
   { "reports_pending_until_asked",          reports_pending_until_asked },
   { "extreme_ticks",                        extreme_ticks },
   { "publish_idempotent",                   publish_idempotent },
   { "blocked_by_names_peer",                blocked_by_names_peer },
   { "changes_count_moves",                  changes_count_moves },
   { "send_results",                         send_results },
   { "stamp_raised_to_promise",              stamp_raised_to_promise },
   { "landing_rounds_up",                    landing_rounds_up },
   { "receivable_below_grant",               receivable_below_grant },
   { "order_landing_sender_sent",            order_landing_sender_sent },
   { "recv_buffer_rules",                    recv_buffer_rules },
   { "peek_lends_in_place",                  peek_lends_in_place },
   { "broadcast_membership_at_send",         broadcast_membership_at_send },
   { "from_is_index_at_send",                from_is_index_at_send },
   { "wake_message_report",                  wake_message_report },
   { "detach_drops_messages_at_edge",        detach_drops_messages_at_edge },
   { "hold_needs_idle",                      hold_needs_idle },
   { "held_peer_silent",                     held_peer_silent },
   { "held_console_message_lands_at_rejoin", held_console_message_lands_at_rejoin },
   { "unknown_anchor_message_resolves",      unknown_anchor_message_resolves },
   { "steady_state_allocates_nothing",       steady_state_allocates_nothing },
   { "thousands_in_flight",                  thousands_in_flight },
   { "activity_counts_sends_and_recvs",      activity_counts_sends_and_recvs }
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
