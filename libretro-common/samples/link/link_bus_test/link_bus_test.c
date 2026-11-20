/* link/link_bus.c alone, one check per rule: records, membership and the
 * advance (bounds, repairs, rounding, reports). The values are worked out by
 * hand; frames are 1/60 s, so 1024 ticks at 61440 and 1000 at 60000. */

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

static void fx_begin(void)
{
   unsigned c;
   for (c = 0; c < fx_consoles; c++)
      link_bus_frame_start(fx_bus, c);
}

/* Each console's frame-start advance, promising its local tick */
static void fx_starts(void)
{
   unsigned c;
   for (c = 0; c < fx_consoles; c++)
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
   unsigned c, p;
   for (c = 0; c < fx_consoles; c++)
      fx_all_ports(c, true);
   for (c = 0; c < fx_consoles; c++)
      link_bus_frame_end(fx_bus, c);
   link_bus_edge(fx_bus);
   for (c = 0; c < fx_consoles; c++)
      for (p = 0; p < FX_PORTS; p++)
         if (fx_h[c][p])
            fx_tick[c][p] += fx_span[c][p];
}

static void fx_run_frame(void)
{
   fx_begin();
   fx_starts();
   frame();
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

static const struct
{
   const char *name;
   void (*run)(void);
} tests[] = {
   { "attach_handles",                   attach_handles },
   { "attach_matches_port_list",         attach_matches_port_list },
   { "unknown_flag_bits_ignored",        unknown_flag_bits_ignored },
   { "null_and_foreign_handles",         null_and_foreign_handles },
   { "cabled_to_nothing_unbounded",      cabled_to_nothing_unbounded },
   { "peers_index_and_count",            peers_index_and_count },
   { "cable_rules",                      cable_rules },
   { "shared_medium_joins",              shared_medium_joins },
   { "grant_bounded_by_peer_promise",    grant_bounded_by_peer_promise },
   { "rounding_down_128",                rounding_down_128 },
   { "promise_never_lowered",            promise_never_lowered },
   { "request_capped_at_promise",        request_capped_at_promise },
   { "frame_end_repair",                 frame_end_repair },
   { "no_frame_end_repair_first_frame",  no_frame_end_repair_first_frame },
   { "earliest_promise_multi",           earliest_promise_multi },
   { "earliest_promise_skips_unbounded", earliest_promise_skips_unbounded },
   { "unknown_edge_tick_bounds_at_edge", unknown_edge_tick_bounds_at_edge },
   { "detached_peer_silent",             detached_peer_silent },
   { "reattach_in_frame_reports_peers",  reattach_in_frame_reports_peers },
   { "first_report_after_attach",        first_report_after_attach },
   { "reports_pending_until_asked",      reports_pending_until_asked },
   { "extreme_ticks",                    extreme_ticks },
   { "publish_idempotent",               publish_idempotent },
   { "blocked_by_names_peer",            blocked_by_names_peer },
   { "changes_count_moves",              changes_count_moves }
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
