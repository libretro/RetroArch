/* link/link_bus.c alone, one check per rule: records, membership, the
 * advance (bounds, repairs, rounding, reports), messages, holds, idle,
 * renumbering, release, breaches, save, load, inspection and the timeline.
 * The values are worked out by hand; frames are 1/60 s, so 1024 ticks at
 * 61440 and 1000 at 60000, except in the timeline checks, where each console
 * has a span of its own. */

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
static const struct retro_link_port_info *fx_ptrs[FX_CONSOLES][FX_PORTS + 1];
static retro_link_port_t *fx_h[FX_CONSOLES][FX_PORTS];
static uint64_t fx_tick[FX_CONSOLES][FX_PORTS]; /* at the edge the session stands at */
static uint64_t fx_span[FX_CONSOLES][FX_PORTS];
static uint64_t fx_safe[FX_CONSOLES][FX_PORTS]; /* the highest promise published */

static void fx_new(unsigned consoles)
{
   memset(fx_list, 0, sizeof(fx_list));
   memset(fx_ptrs, 0, sizeof(fx_ptrs));
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
   fx_ptrs[c][p]  = e;
}

static void fx_attach(unsigned c)
{
   unsigned p;
   CHECK(link_bus_set_ports(fx_bus, c, fx_ptrs[c]));
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
   struct retro_link_wait *list[FX_PORTS];
   unsigned i;
   CHECK(n <= FX_PORTS);
   if (n > FX_PORTS)
      return false;
   for (i = 0; i < n; i++)
   {
      list[i] = &w[i];
      fx_note_safe(c, w[i].handle, w[i].safe_tick);
   }
   link_bus_publish(fx_bus, c, list, n);
   if (!link_bus_grantable(fx_bus, c, list, n))
      return false;
   link_bus_grant(fx_bus, c, list, n);
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
   link_bus_journal_reset(fx_bus);
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
 * console, the step's end. */
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
   link_bus_step_end(fx_bus);
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

/* Opens console c's own frame: its frame-start advance on every port,
 * promising past ticks beyond the frame end */
static void fx_own_start(unsigned c, uint64_t past)
{
   struct retro_link_wait w[FX_PORTS];
   unsigned p, n = 0;
   link_bus_frame_start(fx_bus, c);
   for (p = 0; p < FX_PORTS; p++)
   {
      uint64_t t = fx_tick[c][p];
      if (fx_h[c][p])
         fx_wait(&w[n++], fx_h[c][p], t,
               fx_max(fx_safe[c][p], t + fx_span[c][p] + past), t, false);
   }
   if (n)
      CHECK(advm(c, w, n));
}

/* Ends console c's own frame: its frame-end publish and link_bus_frame_end */
static void fx_own_end(unsigned c)
{
   unsigned p;
   fx_all_ports(c, true);
   link_bus_frame_end(fx_bus, c);
   for (p = 0; p < FX_PORTS; p++)
      if (fx_h[c][p])
         fx_tick[c][p] += fx_span[c][p];
}

static void fx_own_frame(unsigned c, uint64_t past)
{
   fx_own_start(c, past);
   fx_own_end(c);
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
   CHECK(link_bus_set_ports(fx_bus, 0, fx_ptrs[0]));
   CHECK(link_bus_attach(fx_bus, 0, 1, "test-cable", 61440) == NULL);
   CHECK(link_bus_attach(fx_bus, 0, 0, "test-cable", 60000) == NULL);
   h = link_bus_attach(fx_bus, 0, 0, "test-cable", 61440);
   CHECK(h != NULL);
   fx_attach(1);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_peers(fx_bus, 0, h, &count) == 0 && count == 2);

   fx_list[0][0].clock_rate = 60000;
   CHECK(link_bus_set_ports(fx_bus, 0, fx_ptrs[0]));
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
   const struct retro_link_port_info *radio_list[2], *cable_list[2];
   retro_link_port_t *h[5];
   unsigned c, count;

   memset(radio, 0, sizeof(radio));
   memset(cable, 0, sizeof(cable));
   radio_list[0]        = &radio[0];
   radio_list[1]        = NULL;
   cable_list[0]        = &cable[0];
   cable_list[1]        = NULL;
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
      const struct retro_link_port_info *const *l = c < 3 ? radio_list : cable_list;
      CHECK(link_bus_set_ports(b, c, l));
      h[c] = link_bus_attach(b, c, 0, l[0]->protocol_id, 61440);
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
   count = 99;
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == -1 && count == 0);
   count = 99;
   CHECK(link_bus_peers(fx_bus, 1, fx_h[1][0], &count) == -1 && count == 0);
   fx_free();
}

static void cable_rules(void)
{
   struct retro_link_port_info l[2];
   const struct retro_link_port_info *list[2];
   link_bus_t *b;
   unsigned c, count = 0;
   retro_link_port_t *h[256];

   fx_new(2);
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_port(1, 0, "test-cable", 61440, 2, 0);
   fx_attach(0);
   fx_attach(1);
   link_bus_frame_start(fx_bus, 0);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0)); /* wherever the consoles stand */
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
   list[0]          = &l[0];
   list[1]          = NULL;
   b = link_bus_new(256);
   for (c = 0; c < 256; c++)
   {
      CHECK(link_bus_set_ports(b, c, list));
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

/* Three radios that each take two: the two lowest form the bus and the
 * third is cabled to nothing until a place frees */
static void max_ports_caps_shared_medium(void)
{
   uint64_t grant = 0;
   uint32_t flags;
   unsigned c, count = 99;

   fx_new(3);
   for (c = 0; c < 3; c++)
   {
      fx_port(c, 0, "test-radio", 61440, 2, RETRO_LINK_PORT_SHARED_MEDIUM);
      fx_attach(c);
   }
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == 0 && count == 2);
   CHECK(link_bus_peers(fx_bus, 1, fx_h[1][0], &count) == 1 && count == 2);
   CHECK(link_bus_peers(fx_bus, 2, fx_h[2][0], &count) == -1 && count == 0);
   fx_begin();
   flags = 0;
   CHECK(adv(0, fx_h[0][0], 0, 0, 0, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY);
   CHECK(adv(1, fx_h[1][0], 0, 0, 0, NULL, NULL));
   flags = 0;
   CHECK(adv(2, fx_h[2][0], 0, 0, 0, &grant, &flags));
   CHECK(grant == RETRO_LINK_UNBOUNDED);
   CHECK(flags == RETRO_LINK_WAKE_NONE);
   link_bus_detach(fx_bus, 1, fx_h[1][0]);
   CHECK(link_bus_peers(fx_bus, 2, fx_h[2][0], NULL) == -1);
   frame();
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == 0 && count == 2);
   CHECK(link_bus_peers(fx_bus, 2, fx_h[2][0], &count) == 1 && count == 2);
   fx_begin();
   for (c = 0; c < 3; c += 2)
   {
      flags = 0;
      CHECK(adv(c, fx_h[c][0], 1024, 1024, 1024, NULL, &flags));
      CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY);
   }
   fx_free();
}

/* Port 0 of console c, attached with no port list */
static void fx_bare(unsigned c, const char *protocol)
{
   fx_h[c][0]    = link_bus_attach(fx_bus, c, 0, protocol, 61440);
   fx_span[c][0] = 1024;
   CHECK(fx_h[c][0] != NULL);
}

/* A console that gives no port list: its ports have max_ports 0, flags 0
 * and idle_ticks 0 */
static void no_port_list_defaults(void)
{
   char tag;
   size_t len = 1;
   unsigned c, edge, count;

   /* flags 0: no shared medium, though the radios with a list share one */
   fx_new(3);
   for (c = 0; c < 2; c++)
   {
      fx_port(c, 0, "test-radio", 61440, 0, RETRO_LINK_PORT_SHARED_MEDIUM);
      fx_attach(c);
   }
   fx_bare(2, "test-radio");
   count = 0;
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == 0 && count == 2);
   CHECK(link_bus_peers(fx_bus, 2, fx_h[2][0], NULL) == -1);
   fx_free();

   /* max_ports 0: three cabled in a line make one bus */
   fx_new(3);
   for (c = 0; c < 3; c++)
      fx_bare(c, "test-cable");
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_cable(fx_bus, 1, 0, 2, 0));
   for (c = 0; c < 3; c++)
   {
      count = 0;
      CHECK(link_bus_peers(fx_bus, c, fx_h[c][0], &count) == (int)c && count == 3);
   }
   fx_free();

   /* idle_ticks 0: the default quiet span, as in hold_needs_idle */
   fx_new(2);
   fx_bare(0, "test-cable");
   fx_bare(1, "test-cable");
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   fx_begin();
   fx_starts();
   CHECK(snd(0, 0, 512, 1, "a", 1));
   CHECK(adv(0, fx_h[0][0], 0, 1024, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, 1024, 513, NULL, NULL));
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, &tag, &len));
   frame();
   for (edge = 1; edge <= 30; edge++)
   {
      CHECK(!link_bus_hold(fx_bus, 1));
      fx_run_frame();
   }
   CHECK(link_bus_hold(fx_bus, 1));
   fx_free();
}

/* At most 255 ports on a bus: of 256 radios on one shared medium, with no
 * limit and with a max_ports above 255, the last is cabled to nothing */
static void bus_size_caps_at_255(void)
{
   static retro_link_port_t *h[256];
   static const unsigned limits[2] = { 0, 1000 };
   struct retro_link_port_info l;
   const struct retro_link_port_info *list[2];
   link_bus_t *b;
   unsigned c, k, count;

   memset(&l, 0, sizeof(l));
   l.protocol_id = "test-radio";
   l.desc        = "Radio";
   l.clock_rate  = 61440;
   l.flags       = RETRO_LINK_PORT_SHARED_MEDIUM;
   list[0]       = &l;
   list[1]       = NULL;
   for (k = 0; k < 2; k++)
   {
      l.max_ports = limits[k];
      b           = link_bus_new(256);
      for (c = 0; c < 256; c++)
      {
         CHECK(link_bus_set_ports(b, c, list));
         h[c] = link_bus_attach(b, c, 0, "test-radio", 61440);
         CHECK(h[c] != NULL);
      }
      count = 0;
      CHECK(link_bus_peers(b, 0, h[0], &count) == 0 && count == 255);
      count = 0;
      CHECK(link_bus_peers(b, 254, h[254], &count) == 254 && count == 255);
      CHECK(link_bus_peers(b, 255, h[255], NULL) == -1);
      link_bus_free(b);
   }
}

/* Four ports cabled in a line, each taking four */
static void fx_line(void)
{
   unsigned c;
   fx_new(4);
   for (c = 0; c < 4; c++)
   {
      fx_port(c, 0, "test-cable", 61440, 4, 0);
      fx_attach(c);
   }
   for (c = 0; c < 3; c++)
      CHECK(link_bus_cable(fx_bus, c, 0, c + 1, 0));
   CHECK(link_bus_peers(fx_bus, 3, fx_h[3][0], NULL) == 3);
}

/* fx_line, then console k's port re-attached taking two */
static void fx_line_reattached(unsigned k)
{
   fx_line();
   link_bus_detach(fx_bus, k, fx_h[k][0]);
   fx_port(k, 0, "test-cable", 61440, 2, 0);
   fx_attach(k);
}

static void max_ports_caps_reattach(void)
{
   uint64_t grant = 0;
   unsigned count = 99;

   fx_line_reattached(0);
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == 0 && count == 2);
   CHECK(link_bus_peers(fx_bus, 1, fx_h[1][0], &count) == 1 && count == 2);
   CHECK(link_bus_peers(fx_bus, 2, fx_h[2][0], &count) == -1 && count == 0);
   CHECK(link_bus_peers(fx_bus, 3, fx_h[3][0], &count) == -1 && count == 0);
   fx_begin();
   CHECK(adv(3, fx_h[3][0], 0, 0, 0, &grant, NULL));
   CHECK(grant == RETRO_LINK_UNBOUNDED);
   fx_free();

   /* Console 2's port does not fit; console 3's, after it, does */
   fx_line_reattached(2);
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == 0 && count == 3);
   CHECK(link_bus_peers(fx_bus, 1, fx_h[1][0], &count) == 1 && count == 3);
   CHECK(link_bus_peers(fx_bus, 2, fx_h[2][0], &count) == -1 && count == 0);
   CHECK(link_bus_peers(fx_bus, 3, fx_h[3][0], &count) == 2 && count == 3);
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
   struct retro_link_wait *list = &w;
   struct link_bus_blocker why;

   fx_two(61440, 60000);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, 1000, 0, NULL, NULL));
   fx_wait(&w, fx_h[0][0], 0, 2048, 1025, false);
   link_bus_publish(fx_bus, 0, &list, 1);
   link_bus_publish(fx_bus, 0, &list, 1);
   CHECK(!link_bus_grantable(fx_bus, 0, &list, 1));
   memset(&why, 0, sizeof(why));
   CHECK(link_bus_blocked_by(fx_bus, 0, &list, 1, &why));
   CHECK(why.bound == 1024);
   fx_wait(&w, fx_h[0][0], 0, 2048, 1024, false);
   link_bus_publish(fx_bus, 0, &list, 1);
   link_bus_publish(fx_bus, 0, &list, 1);
   CHECK(link_bus_grantable(fx_bus, 0, &list, 1));
   link_bus_grant(fx_bus, 0, &list, 1);
   CHECK(w.grant == 1024);
   fx_free();
}

static void blocked_by_names_peer(void)
{
   struct retro_link_wait w;
   struct retro_link_wait *list = &w;
   struct link_bus_blocker why;

   fx_two(61440, 60000);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, 1000, 0, NULL, NULL));
   fx_wait(&w, fx_h[0][0], 0, 2048, 1025, false);
   CHECK(!advm(0, &w, 1));
   memset(&why, 0xff, sizeof(why));
   CHECK(link_bus_blocked_by(fx_bus, 0, &list, 1, &why));
   CHECK(why.entry == 0 && why.port == 0);
   CHECK(why.request == 1025 && why.bound == 1024);
   CHECK(why.peer_console == 1 && why.peer_port == 0);
   CHECK(!why.edge_unknown);
   fx_wait(&w, fx_h[0][0], 0, 2048, 1024, false);
   CHECK(advm(0, &w, 1));
   CHECK(!link_bus_blocked_by(fx_bus, 0, &list, 1, &why));
   fx_free();

   fx_two(61440, 60000);
   fx_begin();
   fx_wait(&w, fx_h[0][0], 0, 2048, 1025, false);
   CHECK(!advm(0, &w, 1));
   memset(&why, 0, sizeof(why));
   CHECK(link_bus_blocked_by(fx_bus, 0, &list, 1, &why));
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
   /* Not attached or too long comes before cabled to nothing */
   CHECK(!snd(0, 0, 1024, 1, buf, RETRO_LINK_MAX_PAYLOAD + 1));
   CHECK(!snd(0, 0, 1024, 1, NULL, 4));
   link_bus_detach(fx_bus, 0, fx_h[0][0]);
   CHECK(!snd(0, 0, 1024, 1, buf, 4));
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

static unsigned char fx_blob[4096];

static size_t fx_save(void)
{
   size_t n = link_bus_save_size(fx_bus);
   CHECK(n > 0 && n <= sizeof(fx_blob));
   if (n > sizeof(fx_blob))
      return 0;
   CHECK(link_bus_save(fx_bus, fx_blob, n));
   return n;
}

/* fx_bus's state, loaded into a new bus with the same ports attached, dumps
 * the same */
static bool fx_reloads(void)
{
   static char dump_a[4096], dump_b[4096];
   link_bus_t *b = link_bus_new(fx_consoles);
   size_t n      = fx_save();
   unsigned c, p;
   bool same;
   for (c = 0; c < fx_consoles; c++)
   {
      CHECK(link_bus_set_ports(b, c, fx_ptrs[c]));
      for (p = 0; p < FX_PORTS && fx_list[c][p].protocol_id; p++)
         CHECK(link_bus_attach(b, c, p, fx_list[c][p].protocol_id,
                  fx_list[c][p].clock_rate) != NULL);
   }
   same =   link_bus_load(b, fx_blob, n)
         && link_bus_dump(fx_bus, dump_a, sizeof(dump_a)) < sizeof(dump_a)
         && link_bus_dump(b, dump_b, sizeof(dump_b)) < sizeof(dump_b)
         && !strcmp(dump_a, dump_b);
   link_bus_free(b);
   return same;
}

/* Frame 2: console 0 promises 4000 and sends console 1 a message stamped
 * 4000; at edge 3 console 1 is renumbered and stands at 100. Its joining
 * report is still pending. */
static void fx_renumbered(void)
{
   fx_two(61440, 61440);
   fx_run_frame();
   fx_run_frame();
   fx_begin();
   fx_starts();
   CHECK(adv(0, fx_h[0][0], 2048, 4000, 2048, NULL, NULL));
   CHECK(snd(0, 0, 4000, 1, "m", 1));
   frame();
   link_bus_renumbered(fx_bus, 1);
   fx_tick[1][0] = 100;
}

static void renumber_moves_messages(void)
{
   struct link_bus_sent j;
   uint64_t tick = 0;
   size_t len    = 0;

   fx_renumbered();
   CHECK(fx_reloads());
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 3072, 4500, 3072, NULL, NULL));
   CHECK(snd(0, 0, 4500, 1, "w", 1)); /* before console 1's frame-start advance */
   CHECK(adv(0, fx_h[0][0], 3072, 5000, 3072, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 100, 2000, 100, NULL, NULL));
   CHECK(link_bus_journal(fx_bus, &j, 1) == 1 && j.landing == 1528);
   CHECK(adv(1, fx_h[1][0], 100, 2000, 1028, NULL, NULL));
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], NULL, NULL, &len) == NULL);
   CHECK(adv(1, fx_h[1][0], 100, 2000, 1029, NULL, NULL));
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], &tick, NULL, NULL, NULL) && tick == 1028);
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], NULL, NULL, &len) == NULL);
   frame();                          /* 1528 is past its frame end, 1124 */
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 1124, 2000, 1529, NULL, NULL));
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], &tick, NULL, NULL, NULL) && tick == 1528);
   fx_free();
}

static void renumber_drops_old_promise(void)
{
   fx_two(61440, 61440);
   fx_run_frame();
   fx_run_frame();
   fx_begin();
   fx_starts();
   CHECK(adv(1, fx_h[1][0], 2048, 5000, 2048, NULL, NULL));
   frame();
   link_bus_renumbered(fx_bus, 1); /* edge 3 */
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 3072, 6000, 3072, NULL, NULL));
   CHECK(!adv(0, fx_h[0][0], 3072, 6000, 3073, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 100, 200, 100, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 3072, 6000, 3172, NULL, NULL));
   CHECK(!adv(0, fx_h[0][0], 3072, 6000, 3173, NULL, NULL));
   link_bus_renumbered(fx_bus, 0); /* mid-frame: nothing */
   CHECK(adv(1, fx_h[1][0], 100, 200, 200, NULL, NULL));
   fx_free();
}

/* Frame 2: console 1 is told of a message landing at 2100 and does not take
 * it; at edge 3 console 1 is renumbered */
static void fx_seen_renumbered(void)
{
   uint32_t flags = 0;

   fx_two(61440, 61440);
   fx_run_frame();
   fx_run_frame();
   fx_begin();
   fx_starts();
   CHECK(adv(0, fx_h[0][0], 2048, 2100, 2048, NULL, NULL));
   CHECK(snd(0, 0, 2100, 1, "m", 1));
   CHECK(adv(0, fx_h[0][0], 2048, 4000, 2048, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 2048, 4000, 2101, NULL, &flags));
   CHECK(flags == (RETRO_LINK_WAKE_TOPOLOGY | RETRO_LINK_WAKE_MESSAGE));
   frame();
   link_bus_renumbered(fx_bus, 1);
}

static void reset_keeps_messages_and_reports(void)
{
   uint64_t tick = 0;
   uint32_t flags;

   fx_renumbered();
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 3072, 5000, 3072, NULL, NULL));
   flags = 0;
   CHECK(adv(1, fx_h[1][0], 100, 2000, 100, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY);
   CHECK(adv(1, fx_h[1][0], 100, 2000, 1029, NULL, NULL));
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], &tick, NULL, NULL, NULL) && tick == 1028);
   fx_free();

   /* Receivable before the reset and not taken: not reported again, and
    * still receivable; an instant before the new tick 0 lands at 0 */
   fx_seen_renumbered();
   fx_begin();
   flags = 0;
   CHECK(adv(1, fx_h[1][0], 5000, 6000, 5000, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_NONE);
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], &tick, NULL, NULL, NULL) && tick == 4028);
   fx_free();
   fx_seen_renumbered();
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 100, 200, 100, NULL, NULL));
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], &tick, NULL, NULL, NULL) && tick == 0);
   fx_free();
}

/* Console 1 sends console 0 a message in frame 0; at edge 31 the bus is
 * quiet and console 1 is held, and at edge 33 unheld. Console 1 is
 * renumbered and stands 1000000 ticks on: in session 'L' at edge 31,
 * before the hold, after a load that renumbers both consoles; in 'H' at
 * edge 32, while held. hash[] is taken at edges 31 to 34. */
static void fx_held_hashed(char session, uint64_t hash[4])
{
   char tag;
   size_t len = 1;
   unsigned edge;

   fx_two(61440, 61440);
   fx_begin();
   fx_starts();
   CHECK(snd(1, 0, 512, 0, "a", 1));
   CHECK(adv(1, fx_h[1][0], 0, 1024, 0, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 0, 1024, 513, NULL, NULL));
   CHECK(link_bus_recv(fx_bus, 0, fx_h[0][0], NULL, NULL, &tag, &len));
   frame();
   for (edge = 1; edge < 31; edge++)
      fx_run_frame();
   if (session == 'L')
   {
      CHECK(link_bus_load(fx_bus, fx_blob, fx_save()));
      link_bus_renumbered(fx_bus, 0);
      link_bus_renumbered(fx_bus, 1);
      fx_tick[1][0] += 1000000;
   }
   CHECK(link_bus_hold(fx_bus, 1));
   hash[0] = link_bus_hash(fx_bus);
   fx_run_frame();
   if (session == 'H')
   {
      link_bus_renumbered(fx_bus, 1);
      fx_tick[1][0] += 1000000;
   }
   hash[1] = link_bus_hash(fx_bus);
   fx_run_frame();
   hash[2] = link_bus_hash(fx_bus);
   CHECK(link_bus_unhold(fx_bus, 1));
   fx_run_frame();
   hash[3] = link_bus_hash(fx_bus);
   fx_free();
}

static void hold_right_after_renumber(void)
{
   uint64_t a[4], l[4];
   unsigned i;

   fx_held_hashed('A', a);
   fx_held_hashed('L', l);
   for (i = 0; i < 4; i++)
      CHECK(a[i] != 0 && a[i] == l[i]);
}

static void renumber_held_waits_for_unhold(void)
{
   uint64_t a[4], h[4];
   uint64_t grant = 0;
   unsigned i;

   fx_held_hashed('A', a);
   fx_held_hashed('H', h);
   for (i = 0; i < 4; i++)
      CHECK(a[i] != 0 && a[i] == h[i]);

   /* The unhold drops the span the renumbering kept: no frame-end repair */
   fx_two(61440, 61440);
   fx_run_frame();
   CHECK(link_bus_hold(fx_bus, 1)); /* edge 1 */
   link_bus_renumbered(fx_bus, 1);
   fx_tick[1][0] = 100;
   CHECK(link_bus_unhold(fx_bus, 1));
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 1024, 8192, 1024, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 100, 4096, 100, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 100, 4096, 2100, &grant, NULL));
   CHECK(grant == 2100 && counts().request_past_frame_end == 0);
   fx_free();
}

/* The learned span is a duration on the port's own clock: the first frame
 * after a renumbering has its frame-end repair, and the first frame after
 * an unhold has none */
static void renumber_keeps_frame_end_repair(void)
{
   uint64_t grant = 0;

   fx_two(61440, 61440);
   fx_run_frame();
   link_bus_renumbered(fx_bus, 0);   /* edge 1 */
   fx_tick[0][0] = 100;
   CHECK(link_bus_hold(fx_bus, 1));
   CHECK(link_bus_unhold(fx_bus, 1));
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 100, 4096, 100, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 1024, 8192, 1024, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 100, 4096, 2100, &grant, NULL));
   CHECK(grant == 1124);
   CHECK(adv(1, fx_h[1][0], 1024, 8192, 3000, &grant, NULL));
   CHECK(grant == 3000);
   CHECK(counts().request_past_frame_end == 1);
   fx_free();
}

static void console_gone_detaches(void)
{
   uint64_t grant = 0;
   uint32_t flags = 0;
   unsigned count = 99;

   fx_two(61440, 61440);
   fx_run_frame();
   link_bus_console_gone(fx_bus, 1); /* edge 1 */
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == -1 && count == 0);
   count = 99;
   CHECK(link_bus_peers(fx_bus, 1, fx_h[1][0], &count) == -1 && count == 0);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 1024, 2048, 1500, &grant, &flags));
   CHECK(grant == RETRO_LINK_UNBOUNDED && flags == RETRO_LINK_WAKE_DETACHED);
   CHECK(!snd(1, 0, 2048, 0, "a", 1));
   CHECK(adv(0, fx_h[0][0], 1024, 2048, 1500, &grant, NULL));
   CHECK(grant == RETRO_LINK_UNBOUNDED);
   frame();
   CHECK(link_bus_attach(fx_bus, 1, 0, "test-cable", 61440) == fx_h[1][0]);
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == 0 && count == 2);
   fx_free();
}

static void release_unbounded_every_call(void)
{
   char tag      = 0;
   uint64_t grant = 0, tick = 0;
   uint32_t flags;
   size_t len    = 1;

   fx_two(61440, 61440);
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 0, 500, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, 1000, 0, NULL, NULL));
   CHECK(snd(0, 0, 500, 1, "q", 1));
   CHECK(!adv(1, fx_h[1][0], 0, 1000, 1000, NULL, NULL));
   CHECK(!link_bus_released(fx_bus));
   link_bus_release(fx_bus);
   CHECK(link_bus_released(fx_bus));
   flags = 0;
   CHECK(adv(1, fx_h[1][0], 0, 1000, 1000, &grant, &flags));
   CHECK(grant == RETRO_LINK_UNBOUNDED && flags == RETRO_LINK_WAKE_DETACHED);
   flags = 0;
   CHECK(adv(1, fx_h[1][0], 0, 1000, 1000, &grant, &flags));
   CHECK(grant == RETRO_LINK_UNBOUNDED && flags == RETRO_LINK_WAKE_DETACHED);
   CHECK(adv(0, fx_h[0][0], 0, 500, 5000, &grant, NULL));
   CHECK(grant == RETRO_LINK_UNBOUNDED);
   CHECK(link_bus_journal(fx_bus, NULL, 0) == 1);
   CHECK(snd(0, 0, 600, 1, "x", 1));
   CHECK(link_bus_journal(fx_bus, NULL, 0) == 1);
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], &tick, NULL, &tag, &len));
   CHECK(tick == 500 && tag == 'q');
   len = 1;
   CHECK(!link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, &tag, &len));
   fx_free();
}

static void left_out_breach(void)
{
   struct retro_link_wait w[2];

   fx_new(3);
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_port(0, 1, "test-cable", 61440, 2, 0);
   fx_port(1, 0, "test-cable", 61440, 2, 0);
   fx_port(2, 0, "test-cable", 61440, 2, 0);
   fx_attach(0);
   fx_attach(1);
   fx_attach(2);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_cable(fx_bus, 0, 1, 2, 0));
   fx_begin();
   fx_wait(&w[0], fx_h[0][0], 0, 300, 0, false);
   fx_wait(&w[1], fx_h[0][1], 0, 400, 0, false);
   CHECK(advm(0, w, 2));
   CHECK(counts().left_out == 0);
   CHECK(adv(0, fx_h[0][0], 0, 600, 0, NULL, NULL));
   CHECK(counts().left_out == 1);
   CHECK(adv(1, fx_h[1][0], 0, 1000, 600, NULL, NULL));
   CHECK(adv(2, fx_h[2][0], 0, 1000, 400, NULL, NULL));
   CHECK(!adv(2, fx_h[2][0], 0, 1000, 401, NULL, NULL));
   fx_free();
}

static void skipped_publish_breach(void)
{
   uint64_t grant = 0, tick = 0;

   fx_two(61440, 61440);
   fx_run_frame();
   fx_run_frame();
   fx_begin();
   fx_starts();
   CHECK(adv(1, fx_h[1][0], 2048, 3500, 2048, NULL, NULL));
   CHECK(snd(1, 0, 3500, 0, "s", 1));
   fx_all_ports(1, true); /* frame 2: console 0 makes no frame-end publish */
   link_bus_frame_end(fx_bus, 0);
   link_bus_frame_end(fx_bus, 1);
   CHECK(counts().skipped_publish == 1);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 3072, FX_FAR, 3072, NULL, NULL));
   CHECK(!adv(1, fx_h[1][0], 3072, FX_FAR, 3073, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 3072, 3500, 3072, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 3072, FX_FAR, 3500, NULL, NULL));
   CHECK(!adv(1, fx_h[1][0], 3072, FX_FAR, 3501, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 3072, 7000, 6000, &grant, NULL));
   CHECK(grant == 6000 && counts().request_past_frame_end == 0);
   CHECK(counts().skipped_publish == 1);
   CHECK(link_bus_recv(fx_bus, 0, fx_h[0][0], &tick, NULL, NULL, NULL) && tick == 3500);
   fx_free();
}

static void misplaced_calls(void)
{
   char tag      = 0;
   uint64_t grant = 0;
   uint32_t flags = 0;
   size_t len    = 1;

   fx_two(61440, 61440);
   fx_begin();
   CHECK(adv(1, fx_h[1][0], 0, 100, 0, NULL, NULL));
   CHECK(snd(1, 0, 100, 0, "a", 1));
   CHECK(adv(1, fx_h[1][0], 0, FX_FAR, 0, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 0, 1024, 200, NULL, NULL));
   frame();

   /* Edge 1: outside every frame */
   CHECK(!snd(0, 0, 2048, 1, "b", 1));
   CHECK(!link_bus_recv(fx_bus, 0, fx_h[0][0], NULL, NULL, &tag, &len) && len == 0);
   len = 1;
   CHECK(link_bus_peek(fx_bus, 0, fx_h[0][0], NULL, NULL, &len) == NULL && len == 0);
   CHECK(adv(0, fx_h[0][0], 1024, 4096, 2048, &grant, &flags));
   CHECK(grant == RETRO_LINK_UNBOUNDED && flags == RETRO_LINK_WAKE_DETACHED);
   CHECK(counts().misplaced_call == 4);

   /* The misplaced advance published nothing */
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 1024, 1024, 1024, NULL, NULL));
   CHECK(snd(0, 0, 1024, 1, "c", 1));
   CHECK(adv(1, fx_h[1][0], 1024, FX_FAR, 1024, NULL, NULL));
   CHECK(!adv(1, fx_h[1][0], 1024, FX_FAR, 1025, NULL, NULL));
   len = 1;
   CHECK(link_bus_recv(fx_bus, 0, fx_h[0][0], NULL, NULL, &tag, &len) && tag == 'a');
   CHECK(counts().misplaced_call == 4);
   frame();

   /* Frame 2: console 1 before its frame-start advance, 'c' receivable */
   fx_begin();
   fx_all_ports(0, false);
   len = 1;
   CHECK(link_bus_peek(fx_bus, 1, fx_h[1][0], NULL, NULL, &len) == NULL && len == 0);
   len = 1;
   CHECK(!link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, &tag, &len) && len == 0);
   CHECK(!snd(1, 0, FX_FAR, 0, "d", 1));
   CHECK(counts().misplaced_call == 7);
   fx_all_ports(1, false);
   len = 1;
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, &tag, &len) && tag == 'c');
   CHECK(snd(1, 0, FX_FAR, 0, "d", 1));
   CHECK(link_bus_journal(fx_bus, NULL, 0) == 1 && counts().misplaced_call == 7);
   fx_free();

   /* A renumbered console's send would land against its old anchor */
   fx_renumbered();
   fx_begin();
   CHECK(!snd(1, 0, 100, 0, "e", 1));
   CHECK(counts().misplaced_call == 1 && link_bus_journal(fx_bus, NULL, 0) == 0);
   fx_free();
}

/* A second session beside the globals; fx_swap trades them */
struct fx_session
{
   link_bus_t *bus;
   unsigned consoles;
   retro_link_port_t *h[FX_CONSOLES][FX_PORTS];
   uint64_t tick[FX_CONSOLES][FX_PORTS];
   uint64_t span[FX_CONSOLES][FX_PORTS];
   uint64_t safe[FX_CONSOLES][FX_PORTS];
};

static void fx_swap(struct fx_session *s)
{
   struct fx_session t = *s;
   s->bus      = fx_bus;
   s->consoles = fx_consoles;
   memcpy(s->h, fx_h, sizeof(fx_h));
   memcpy(s->tick, fx_tick, sizeof(fx_tick));
   memcpy(s->span, fx_span, sizeof(fx_span));
   memcpy(s->safe, fx_safe, sizeof(fx_safe));
   fx_bus      = t.bus;
   fx_consoles = t.consoles;
   memcpy(fx_h, t.h, sizeof(fx_h));
   memcpy(fx_tick, t.tick, sizeof(fx_tick));
   memcpy(fx_span, t.span, sizeof(fx_span));
   memcpy(fx_safe, t.safe, sizeof(fx_safe));
}

/* Takes every receivable message of console c's port p: a digest of them */
static uint64_t fx_drain(unsigned c, unsigned p)
{
   static unsigned char buf[RETRO_LINK_MAX_PAYLOAD];
   uint64_t digest = 0, tick = 0;
   unsigned from   = 0;
   size_t len      = sizeof(buf);
   while (link_bus_recv(fx_bus, c, fx_h[c][p], &tick, &from, buf, &len))
   {
      digest = digest * 31 + tick + from * 7 + len * 3 + buf[0];
      len    = sizeof(buf);
   }
   return digest;
}

/* Every console's port list; the ports of consoles below attached attached */
static void fx_roundtrip_ports(unsigned attached)
{
   unsigned c;
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_port(1, 0, "test-cable", 61440, 2, 0);
   fx_port(1, 1, "test-cable", 61440, 2, 0);
   fx_port(2, 0, "test-cable", 61440, 2, 0);
   for (c = 0; c < 3; c++)
      if (c < attached)
         fx_attach(c);
      else
         CHECK(link_bus_set_ports(fx_bus, c, fx_ptrs[c]));
}

/* Edge 6: 0:0 cabled to 1:0, 1:1 to 2:0, console 2 held since edge 1;
 * messages queued both ways between consoles 0 and 1 and one for console 2;
 * console 1 has its joining reports and a WAKE_MESSAGE pending, console 0
 * nothing. */
static void fx_roundtrip(void)
{
   struct retro_link_wait w[2];
   uint32_t flags = 0;
   unsigned f;

   fx_new(3);
   fx_roundtrip_ports(3);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_cable(fx_bus, 1, 1, 2, 0));
   fx_run_frame();
   CHECK(link_bus_hold(fx_bus, 2));
   for (f = 1; f < 5; f++)
      fx_run_frame();
   fx_begin();
   fx_starts();
   CHECK(adv(0, fx_h[0][0], 5120, 5200, 5120, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY);
   CHECK(snd(0, 0, 5200, 1, "a", 1));
   CHECK(adv(0, fx_h[0][0], 5120, 7000, 5120, NULL, NULL));
   CHECK(snd(0, 0, 7000, 1, "bb", 2));
   fx_wait(&w[0], fx_h[1][0], 5120, 6500, 5300, false);
   fx_wait(&w[1], fx_h[1][1], 5120, 5500, 5300, false);
   CHECK(advm(1, w, 2));
   CHECK(snd(1, 0, 6500, 0, "ccc", 3));
   CHECK(snd(1, 1, 5500, 1, "dddd", 4));
   frame();
}

/* One frame of fx_roundtrip's session, the same on any bus: console 2 is
 * unheld at edge 7; console 1 asks for reports. */
static void fx_roundtrip_frame(unsigned f, uint32_t flags[2], uint64_t *digest)
{
   struct retro_link_wait w[2];
   uint64_t t0 = fx_tick[0][0], t1 = fx_tick[1][0], t2 = fx_tick[2][0];
   unsigned i;

   if (f == 7)
      CHECK(link_bus_unhold(fx_bus, 2));
   fx_begin();
   fx_starts();
   for (i = 0; i < 2; i++)
   {
      uint64_t d = i ? 1024 : 0;
      CHECK(adv(0, fx_h[0][0], t0, t0 + 2048, t0 + d, NULL, NULL));
      fx_wait(&w[0], fx_h[1][0], t1, t1 + 2048, t1 + d, i != 0);
      fx_wait(&w[1], fx_h[1][1], t1, t1 + 2048, t1 + d, i != 0);
      CHECK(advm(1, w, 2));
      if (fx_running(2))
         CHECK(adv(2, fx_h[2][0], t2, t2 + 2048, t2 + d, NULL, NULL));
   }
   flags[0] = w[0].wake_flags;
   flags[1] = w[1].wake_flags;
   *digest  = fx_drain(0, 0) * 3 + fx_drain(1, 0) * 5 + fx_drain(1, 1) * 7;
   if (fx_running(2))
      *digest += fx_drain(2, 0) * 11;
   CHECK(snd(0, 0, t0 + 2048, 1, "e", 1));
   frame();
}

static void save_load_roundtrip(void)
{
   static char dump_a[4096], dump_b[4096];
   struct fx_session a;
   retro_link_port_t *hb[FX_CONSOLES][FX_PORTS];
   uint32_t flags_a[2], flags_b[2];
   uint64_t digest_a = 0, digest_b = 0;
   unsigned f, count = 0;
   size_t n;

   fx_roundtrip();
   n = fx_save();
   memset(&a, 0, sizeof(a));
   fx_swap(&a);
   fx_new(3);
   fx_roundtrip_ports(2); /* console 2's port: a record the load makes */
   memcpy(hb, fx_h, sizeof(hb));
   CHECK(link_bus_load(fx_bus, fx_blob, n));
   fx_h[2][0] = link_bus_attach(fx_bus, 2, 0, "test-cable", 61440);
   CHECK(fx_h[2][0] != NULL);
   memcpy(fx_tick, a.tick, sizeof(fx_tick));
   memcpy(fx_span, a.span, sizeof(fx_span));
   memcpy(fx_safe, a.safe, sizeof(fx_safe));
   CHECK(link_bus_hash(fx_bus) != 0 && link_bus_hash(fx_bus) == link_bus_hash(a.bus));
   CHECK(link_bus_dump(a.bus, dump_a, sizeof(dump_a)) < sizeof(dump_a));
   CHECK(link_bus_dump(fx_bus, dump_b, sizeof(dump_b)) < sizeof(dump_b));
   CHECK(dump_a[0] != '\0' && !strcmp(dump_a, dump_b));
   CHECK(link_bus_peers(fx_bus, 1, hb[1][1], &count) == 0 && count == 2);
   CHECK(link_bus_is_held(fx_bus, 2));
   for (f = 6; f < 9; f++)
   {
      fx_roundtrip_frame(f, flags_b, &digest_b);
      fx_swap(&a);
      fx_roundtrip_frame(f, flags_a, &digest_a);
      fx_swap(&a);
      CHECK(link_bus_hash(fx_bus) == link_bus_hash(a.bus));
      CHECK(digest_a == digest_b);
      CHECK(flags_a[0] == flags_b[0] && flags_a[1] == flags_b[1]);
      if (f == 6)
      {
         CHECK(flags_b[0] == (RETRO_LINK_WAKE_TOPOLOGY | RETRO_LINK_WAKE_MESSAGE));
         CHECK(flags_b[1] == RETRO_LINK_WAKE_TOPOLOGY);
      }
   }
   CHECK(fx_h[1][1] == hb[1][1] && hb[1][1] != a.h[1][1]);
   fx_free();
   link_bus_free(a.bus);
}

/* The save blob's layout, for checks that corrupt one field */
#define FX_BLOB_HEAD  84 /* magic, version, consoles, records, cables, counts */
#define FX_BLOB_PORT  103 /* a record, up to its protocol's length */
#define FX_BLOB_LINK  38  /* a link to another record */
#define FX_BLOB_ENTRY 59  /* a message, up to its payload */

static size_t fx_u32(const unsigned char *s)
{
   return (size_t)s[0] | ((size_t)s[1] << 8) | ((size_t)s[2] << 16) | ((size_t)s[3] << 24);
}

static size_t fx_cables_at(const unsigned char *blob)
{
   return FX_BLOB_HEAD + fx_u32(blob + 8) + fx_u32(blob + 12) * 8;
}

/* Where record k of blob starts; *first, where its first message starts */
static size_t fx_record_at(const unsigned char *blob, size_t k, size_t *first)
{
   size_t at = fx_cables_at(blob) + fx_u32(blob + 16) * 8;
   for (;;)
   {
      size_t rec = at, i, n;
      at += FX_BLOB_PORT;
      at += 4 + fx_u32(blob + at);
      at += 4 + fx_u32(blob + at) * FX_BLOB_LINK;
      n   = fx_u32(blob + at);
      at += 4;
      if (!k--)
      {
         *first = at;
         return rec;
      }
      for (i = 0; i < n; i++)
         at += FX_BLOB_ENTRY + fx_u32(blob + at + FX_BLOB_ENTRY - 4);
   }
}

static void load_refuses_bad_input(void)
{
   static unsigned char bad[sizeof(fx_blob)];
   struct fx_session a;
   uint64_t hash_a, hash_b;
   size_t n, first;

   fx_roundtrip();
   n      = fx_save();
   hash_a = link_bus_hash(fx_bus);
   memset(&a, 0, sizeof(a));
   fx_swap(&a);
   fx_new(3);
   fx_roundtrip_ports(3);
   hash_b = link_bus_hash(fx_bus);
   CHECK(hash_b != 0 && hash_b != hash_a);
   memcpy(bad, fx_blob, n);
   bad[3] = 'X';
   CHECK(!link_bus_load(fx_bus, bad, n));
   CHECK(link_bus_hash(fx_bus) == hash_b);
   CHECK(!link_bus_load(fx_bus, fx_blob, n ? n - 1 : 0));
   CHECK(link_bus_hash(fx_bus) == hash_b);
   memcpy(bad, fx_blob, n);
   fx_record_at(bad, 0, &first);
   bad[first + 7] = 0xFF; /* the rank of console 0's one message */
   CHECK(!link_bus_load(fx_bus, bad, n));
   CHECK(link_bus_hash(fx_bus) == hash_b);
   CHECK(link_bus_load(fx_bus, fx_blob, n));
   CHECK(link_bus_hash(fx_bus) == hash_a);
   fx_free();
   link_bus_free(a.bus);
}

static void load_checks_cables_and_reports(void)
{
   static unsigned char bad[sizeof(fx_blob)];
   struct fx_session a;
   uint64_t hash_a, hash_b;
   size_t n, rec, first, cables;

   /* A detach and re-attach can leave a cable between two protocols or to
    * a shared medium; a load takes such a state */
   fx_two(61440, 61440);
   fx_run_frame();
   link_bus_detach(fx_bus, 0, fx_h[0][0]);
   link_bus_detach(fx_bus, 1, fx_h[1][0]);
   fx_port(0, 0, "test-other", 61440, 2, 0);
   fx_port(1, 0, "test-cable", 61440, 2, RETRO_LINK_PORT_SHARED_MEDIUM);
   fx_attach(0);
   fx_attach(1);
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], NULL) == -1);
   CHECK(fx_reloads());
   fx_free();

   fx_roundtrip();
   n      = fx_save();
   hash_a = link_bus_hash(fx_bus);
   memset(&a, 0, sizeof(a));
   fx_swap(&a);
   fx_new(3);
   fx_roundtrip_ports(3);
   hash_b = link_bus_hash(fx_bus);

   /* The second cable, 1:1 to 2:0, made the first again, ends swapped */
   memcpy(bad, fx_blob, n);
   cables           = fx_cables_at(bad);
   bad[cables + 8]  = 1;
   bad[cables + 12] = 0;
   CHECK(!link_bus_load(fx_bus, bad, n));
   CHECK(link_bus_hash(fx_bus) == hash_b);

   /* Console 1's port 0 with reports no port can have pending */
   memcpy(bad, fx_blob, n);
   rec = fx_record_at(bad, 1, &first);
   bad[rec + 11] |= RETRO_LINK_WAKE_DETACHED | 8;
   bad[rec + 14] |= 0x80;
   CHECK(link_bus_load(fx_bus, bad, n));
   CHECK(link_bus_hash(fx_bus) == hash_a);
   fx_free();
   link_bus_free(a.bus);

   /* The third of three cables, 2:0 to 3:0, made the first again */
   fx_line();
   n      = fx_save();
   hash_b = link_bus_hash(fx_bus);
   cables = fx_cables_at(fx_blob);
   fx_blob[cables + 16] = 1;
   fx_blob[cables + 20] = 0;
   CHECK(!link_bus_load(fx_bus, fx_blob, n));
   CHECK(link_bus_hash(fx_bus) == hash_b);
   fx_free();
}

/* A state whose cables join more ports than one of them takes, as a
 * re-attach taking fewer leaves them, loads capped */
static void load_caps_cables(void)
{
   size_t n;
   unsigned c, count = 99;

   fx_line_reattached(0);
   n = fx_save();
   fx_free();
   fx_new(4);
   for (c = 0; c < 4; c++)
   {
      fx_port(c, 0, "test-cable", 61440, c ? 4 : 2, 0);
      fx_attach(c);
   }
   CHECK(link_bus_load(fx_bus, fx_blob, n));
   CHECK(link_bus_peers(fx_bus, 0, fx_h[0][0], &count) == 0 && count == 2);
   CHECK(link_bus_peers(fx_bus, 1, fx_h[1][0], &count) == 1 && count == 2);
   CHECK(link_bus_peers(fx_bus, 2, fx_h[2][0], &count) == -1 && count == 0);
   CHECK(link_bus_peers(fx_bus, 3, fx_h[3][0], &count) == -1 && count == 0);
   fx_free();
}

/* Consoles 0 and 1 with ports 0 and 1 each, cabled port to port; with run,
 * a frame in which ports 1 send each other a message */
static void fx_two_pairs(bool run)
{
   unsigned c;
   fx_new(2);
   for (c = 0; c < 2; c++)
   {
      fx_port(c, 0, "test-cable", 61440, 2, 0);
      fx_port(c, 1, "test-cable", 61440, 2, 0);
      fx_attach(c);
   }
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_cable(fx_bus, 0, 1, 1, 1));
   if (!run)
      return;
   fx_begin();
   fx_starts();
   CHECK(snd(0, 1, 0, 1, "x", 1));
   CHECK(snd(1, 1, 0, 0, "y", 1));
   frame();
}

/* Ports 1 are not in the blob: they end up as ports never attached, run or not */
static void load_resets_unlisted_records(void)
{
   uint64_t hash;
   size_t n;

   fx_two(61440, 61440);
   fx_run_frame();
   n = fx_save();
   fx_free();
   fx_two_pairs(true);
   CHECK(link_bus_load(fx_bus, fx_blob, n));
   hash = link_bus_hash(fx_bus);
   fx_free();
   fx_two_pairs(false);
   CHECK(link_bus_load(fx_bus, fx_blob, n));
   CHECK(link_bus_hash(fx_bus) == hash);
   fx_free();
}

/* Frames 0 to 6 of two consoles sending each other a message every frame,
 * landing in the next, console 1 only before edge 4; the hashes at edges 5
 * to 7. In session 'B' console 1 is renumbered at edge 4, where a misplaced
 * call moves a count; in 'C' console 0 sends one more at frame 4, still
 * queued at edge 7. */
static void fx_hashed(char session, uint64_t hash[3])
{
   unsigned f, c;

   fx_two(61440, 61440);
   for (f = 0; f < 7; f++)
   {
      if (session == 'B' && f == 4)
      {
         link_bus_renumbered(fx_bus, 1);
         fx_tick[1][0] += 1000000;
         CHECK(link_bus_peek(fx_bus, 0, fx_h[0][0], NULL, NULL, NULL) == NULL);
      }
      fx_begin();
      for (c = 0; c < 2; c++)
         CHECK(adv(c, fx_h[c][0], fx_tick[c][0], fx_tick[c][0] + 1500, fx_tick[c][0], NULL, NULL));
      for (c = 0; c < 2; c++)
         CHECK(adv(c, fx_h[c][0], fx_tick[c][0], fx_tick[c][0] + 1500, fx_tick[c][0] + 1024, NULL, NULL));
      for (c = 0; c < 2; c++)
      {
         fx_drain(c, 0);
         if (c == 0 || f < 4)
            CHECK(snd(c, 0, fx_tick[c][0] + 1500, 1 - c, "h", 1));
      }
      if (session == 'C' && f == 4)
         CHECK(snd(0, 0, fx_tick[0][0] + 5000, 1, "late", 4));
      frame();
      if (f >= 4)
         hash[f - 4] = link_bus_hash(fx_bus);
   }
   fx_free();
}

static void hash_ignores_numbering(void)
{
   uint64_t ha[3], hb[3], hc[3];
   unsigned i;

   fx_hashed('A', ha);
   fx_hashed('B', hb);
   fx_hashed('C', hc);
   for (i = 0; i < 3; i++)
      CHECK(ha[i] != 0 && ha[i] == hb[i]);
   CHECK(hc[0] != ha[0]);
}

/* Lines of text that start with prefix */
static unsigned fx_lines(const char *text, const char *prefix)
{
   unsigned n      = 0;
   size_t plen     = strlen(prefix);
   const char *line = text;
   while (*line)
   {
      const char *end = strchr(line, '\n');
      if (!strncmp(line, prefix, plen))
         n++;
      if (!end)
         break;
      line = end + 1;
   }
   return n;
}

static void dump_lists_ports_and_messages(void)
{
   static char text[4096];
   char cut[10];
   size_t n;

   fx_two(61440, 61440);
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 0, 3000, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, 3000, 0, NULL, NULL));
   CHECK(snd(0, 0, 3000, 1, "a", 1));
   CHECK(snd(0, 0, 3001, 1, "b", 1));
   CHECK(snd(1, 0, 3000, 0, "c", 1));
   frame();
   n = link_bus_dump(fx_bus, text, sizeof(text));
   CHECK(n > 0 && n < sizeof(text));
   CHECK(fx_lines(text, "port 0:0 ") == 1);
   CHECK(fx_lines(text, "port 1:0 ") == 1);
   CHECK(fx_lines(text, "msg ") == 3);
   memset(cut, 'x', sizeof(cut));
   CHECK(link_bus_dump(fx_bus, cut, sizeof(cut)) == n);
   CHECK(cut[9] == '\0' && !strncmp(cut, text, 9));
   fx_free();
}

static void counts_survive_load(void)
{
   struct fx_session a;
   struct link_bus_counts ka, kb;
   uint64_t blocks;
   unsigned i;
   size_t n;

   fx_two(61440, 61440);
   fx_begin();
   fx_far(1);
   CHECK(adv(0, fx_h[0][0], 0, 500, 0, NULL, NULL));
   for (i = 0; i < 3; i++)
      CHECK(adv(0, fx_h[0][0], 0, 400, 0, NULL, NULL));
   CHECK(snd(0, 0, 500, 1, "a", 1));
   CHECK(adv(0, fx_h[0][0], 0, 600, 0, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 0, FX_FAR, 501, NULL, NULL));
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, NULL, NULL));
   frame();
   CHECK(counts().lowered_promise == 3);
   CHECK(link_bus_set_ports(fx_bus, 0, fx_ptrs[0])); /* a block B has not */
   n  = fx_save();
   ka = counts();
   memset(&a, 0, sizeof(a));
   fx_swap(&a);
   fx_two(61440, 61440);
   fx_begin();
   fx_far(0);
   CHECK(snd(0, 0, FX_FAR, 1, "z", 1)); /* dropped by the load */
   frame();
   blocks = counts().blocks;
   CHECK(link_bus_load(fx_bus, fx_blob, n));
   CHECK(link_bus_hash(fx_bus) == link_bus_hash(a.bus));
   kb = counts();
   CHECK(kb.lowered_promise == 3);
   CHECK(kb.blocks == blocks && blocks != ka.blocks);
   kb.blocks = ka.blocks;
   CHECK(!memcmp(&ka, &kb, sizeof(ka)));
   fx_free();
   link_bus_free(a.bus);
}

/* Call k of console c in frame 1 of fx_interleaved: its frame-start advance,
 * two sends to its peer at one stamp, one to held console 2, one to all */
static void fx_mixed(unsigned c, unsigned k)
{
   char tag = (char)('a' + c * 5 + k);
   if (k == 0)
      CHECK(adv(c, fx_h[c][0], 1024, 1536, 1024, NULL, NULL));
   else if (k < 3)
      CHECK(snd(c, 0, 5000, 1 - c, &tag, 1));
   else if (k == 3)
      CHECK(snd(c, 0, 1600, 2, &tag, 1));
   else
      CHECK(snd(c, 0, 1700, RETRO_LINK_BROADCAST, &tag, 1));
}

/* Three consoles on one bus, console 2 held from edge 1; in frame 1
 * consoles 0 and 1 make the same calls, one after the other when order is 0,
 * interleaved from console 1 when it is 1 */
static void fx_interleaved(unsigned order)
{
   unsigned c, k;

   fx_new(3);
   for (c = 0; c < 3; c++)
   {
      fx_port(c, 0, "test-cable", 61440, 3, 0);
      fx_attach(c);
   }
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_cable(fx_bus, 1, 0, 2, 0));
   fx_run_frame();
   CHECK(link_bus_hold(fx_bus, 2));
   fx_begin();
   if (order)
      for (k = 0; k < 5; k++)
      {
         fx_mixed(1, k);
         fx_mixed(0, k);
      }
   else
      for (c = 0; c < 2; c++)
         for (k = 0; k < 5; k++)
            fx_mixed(c, k);
   frame();
}

/* Frame 2 of fx_interleaved: console 2 unheld, console 0 sends one more that
 * ties with its two to console 1, each console takes what has landed; a
 * digest of what they took */
static uint64_t fx_mixed_after(void)
{
   uint64_t digest = 0;
   unsigned c;

   CHECK(link_bus_unhold(fx_bus, 2));
   fx_begin();
   for (c = 0; c < 3; c++)
      CHECK(adv(c, fx_h[c][0], fx_tick[c][0], fx_tick[c][0] + 1024, fx_tick[c][0], NULL, NULL));
   CHECK(snd(0, 0, 5000, 1, "n", 1));
   for (c = 0; c < 3; c++)
      CHECK(adv(c, fx_h[c][0], fx_tick[c][0], fx_tick[c][0] + 1024, fx_tick[c][0] + 1024, NULL, NULL));
   for (c = 0; c < 3; c++)
      digest = digest * 31 + fx_drain(c, 0);
   frame();
   return digest;
}

static void hash_and_save_ignore_interleaving(void)
{
   static unsigned char first[sizeof(fx_blob)];
   uint64_t hash[3], after[3], digest[3];
   size_t n[2];
   unsigned i, c;

   for (i = 0; i < 2; i++)
   {
      fx_interleaved(i);
      hash[i] = link_bus_hash(fx_bus);
      n[i]    = fx_save();
      if (!i)
         memcpy(first, fx_blob, n[0]);
      digest[i] = fx_mixed_after();
      after[i]  = link_bus_hash(fx_bus);
      fx_free();
   }
   CHECK(hash[0] != 0 && hash[0] == hash[1]);
   CHECK(n[0] == n[1] && !memcmp(first, fx_blob, n[0]));

   /* Loaded from the blob, the bus hands out the same messages in the same
    * order, a later send included */
   fx_interleaved(0);
   link_bus_free(fx_bus);
   fx_bus = link_bus_new(3);
   for (c = 0; c < 3; c++)
   {
      CHECK(link_bus_set_ports(fx_bus, c, fx_ptrs[c]));
      fx_h[c][0] = link_bus_attach(fx_bus, c, 0, "test-cable", 61440);
   }
   CHECK(link_bus_load(fx_bus, first, n[0]));
   hash[2]   = link_bus_hash(fx_bus);
   digest[2] = fx_mixed_after();
   after[2]  = link_bus_hash(fx_bus);
   fx_free();
   CHECK(hash[2] == hash[0]);
   CHECK(digest[0] == digest[1] && digest[1] == digest[2]);
   CHECK(after[0] == after[1] && after[1] == after[2]);
}

/* Console 0 at 60000 with a span of 1001, console 1 at 16777216 with a span
 * of 280896, both from tick 0, cabled */
static void fx_spans(void)
{
   fx_new(2);
   fx_port(0, 0, "test-cable", 60000, 2, 0);
   fx_port(1, 0, "test-cable", 16777216, 2, 0);
   fx_attach(0);
   fx_attach(1);
   fx_span[0][0] = 1001;
   fx_span[1][0] = 280896;
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
}

static bool fx_edge_is(unsigned c, uint64_t since, uint64_t span, uint64_t rate)
{
   uint64_t s = 0, n = 0, r = 0;
   return link_bus_console_edge(fx_bus, c, &s, &n, &r)
      && s == since && n == span && r == rate;
}

/* fx_spans run to console 0's edge 3 and console 1's edge 2, each promising
 * far ahead; no frame of one console moves the other's edge */
static void fx_spans_frames(void)
{
   fx_own_start(0, FX_FAR);
   fx_own_start(1, FX_FAR);
   fx_own_end(0);
   CHECK(fx_edge_is(1, 0, 0, 16777216));
   fx_own_end(1);
   CHECK(fx_edge_is(0, 1001, 1001, 60000));
   fx_own_frame(0, FX_FAR);
   CHECK(fx_edge_is(1, 280896, 280896, 16777216));
   fx_own_frame(1, FX_FAR);
   CHECK(fx_edge_is(0, 2002, 1001, 60000));
   fx_own_frame(0, FX_FAR);
   CHECK(fx_edge_is(1, 561792, 280896, 16777216));
}

static void instants_across_spans(void)
{
   struct retro_link_wait w;
   struct retro_link_wait *list = &w;
   struct link_bus_blocker why;

   fx_spans();
   link_bus_frame_start(fx_bus, 0);
   fx_far(0);
   fx_own_frame(1, 0);           /* stands at 280896, promising it */
   fx_own_end(0);
   link_bus_frame_start(fx_bus, 0);
   fx_wait(&w, fx_h[0][0], 1001, fx_max(fx_safe[0][0], 1001), 2002, false);
   CHECK(!advm(0, &w, 1));
   memset(&why, 0, sizeof(why));
   CHECK(link_bus_blocked_by(fx_bus, 0, &list, 1, &why));
   CHECK(why.request == 2002 && why.bound == 1004); /* 1004.56, down */
   CHECK(why.peer_console == 1 && !why.edge_unknown);
   fx_free();
}

static void instants_rounding_both_ends(void)
{
   struct link_bus_sent j;
   uint64_t tick = 0, grant = 0;
   size_t len    = 0;

   fx_spans();
   link_bus_frame_start(fx_bus, 0);
   fx_far(0);
   fx_own_start(1, 0);
   CHECK(snd(1, 0, 280896, 0, "a", 1));
   CHECK(link_bus_journal(fx_bus, &j, 1) == 1 && j.landing == 1005); /* 1004.56, up */
   fx_own_end(1);
   fx_own_end(0);
   link_bus_frame_start(fx_bus, 0);
   CHECK(!adv(0, fx_h[0][0], 1001, fx_safe[0][0], 2002, NULL, NULL));
   CHECK(link_bus_peek(fx_bus, 0, fx_h[0][0], NULL, NULL, &len) == NULL);
   fx_own_start(1, 0);           /* promises 561792: 2009.12 on console 0 */
   CHECK(adv(0, fx_h[0][0], 1001, fx_safe[0][0], 2002, &grant, NULL));
   CHECK(grant == 2002);
   CHECK(link_bus_recv(fx_bus, 0, fx_h[0][0], &tick, NULL, NULL, NULL) && tick == 1005);
   fx_free();
}

static void edges_at_own_spans(void)
{
   fx_spans();
   CHECK(fx_edge_is(0, 0, 0, 60000));
   CHECK(fx_edge_is(1, 0, 0, 16777216));
   fx_spans_frames();
   CHECK(fx_edge_is(0, 3003, 1001, 60000));
   CHECK(fx_edge_is(1, 561792, 280896, 16777216));
   fx_free();
}

/* fx_spans after a frame of each, console 0 promising far ahead and console
 * 1 its frame end; then console 0 opens its frame 1 and console 1 runs its
 * frame 1 promising past ticks beyond its edge and makes no frame-end
 * publish, which leaves its tick at edge 561792 unknown */
static void fx_skipped(uint64_t past)
{
   fx_spans();
   fx_own_frame(0, FX_FAR);
   fx_own_frame(1, 0);
   link_bus_frame_start(fx_bus, 0);
   CHECK(adv(0, fx_h[0][0], 1001, fx_safe[0][0], 1001, NULL, NULL));
   link_bus_frame_start(fx_bus, 1);
   CHECK(adv(1, fx_h[1][0], 280896, 280896 + past, 280896, NULL, NULL));
   link_bus_frame_end(fx_bus, 1);
}

/* Such a port holds its peers at its promise, never past its new edge,
 * until its frame-start advance */
static void skipped_publish_holds_at_promise(void)
{
   struct retro_link_wait w;
   struct retro_link_wait *list = &w;
   struct link_bus_blocker why;

   fx_skipped(100000);           /* 380896: 1362.18 on console 0 */
   CHECK(fx_edge_is(1, 561792, 280896, 16777216));
   CHECK(adv(0, fx_h[0][0], 1001, fx_safe[0][0], 1362, NULL, NULL));
   fx_wait(&w, fx_h[0][0], 1001, fx_safe[0][0], 1363, false);
   CHECK(!advm(0, &w, 1));
   memset(&why, 0, sizeof(why));
   CHECK(link_bus_blocked_by(fx_bus, 0, &list, 1, &why));
   CHECK(why.bound == 1362 && why.peer_console == 1 && why.edge_unknown);
   link_bus_frame_start(fx_bus, 1);
   CHECK(adv(1, fx_h[1][0], 561792, 842688, 561792, NULL, NULL));
   CHECK(adv(0, fx_h[0][0], 1001, fx_safe[0][0], 2002, NULL, NULL));
   fx_free();

   fx_skipped(FX_FAR);           /* edge 561792: 2009.12 on console 0 */
   fx_own_end(0);
   link_bus_frame_start(fx_bus, 0);
   CHECK(adv(0, fx_h[0][0], 2002, fx_safe[0][0], 2009, NULL, NULL));
   CHECK(!adv(0, fx_h[0][0], 2002, fx_safe[0][0], 2010, NULL, NULL));
   fx_free();
}

/* A message for a port whose frame ended with no frame-end publish, sent
 * from a frame begun before that port's new edge, lands on the edge before,
 * whether sent before or after the port's frame ended */
static void skipped_publish_lands_on_edge_before(void)
{
   struct link_bus_sent j[2];
   uint64_t hash[2];
   unsigned i;

   for (i = 0; i < 2; i++)
   {
      fx_spans();
      fx_own_frame(0, 0);
      fx_own_frame(1, 0);
      link_bus_frame_start(fx_bus, 0);
      CHECK(adv(0, fx_h[0][0], 1001, 1001, 1001, NULL, NULL));
      link_bus_frame_start(fx_bus, 1);
      CHECK(adv(1, fx_h[1][0], 280896, 380896, 280896, NULL, NULL));
      if (i)
         link_bus_frame_end(fx_bus, 1);
      CHECK(snd(0, 0, 1500, 1, "a", 1));
      if (!i)
         link_bus_frame_end(fx_bus, 1);
      memset(&j[i], 0, sizeof(j[i]));
      CHECK(link_bus_journal(fx_bus, &j[i], 1) == 1);
      hash[i] = link_bus_hash(fx_bus);
      fx_free();
   }
   CHECK(j[0].landing == 419431 && j[1].landing == 419431); /* 419430.4, up */
   CHECK(hash[0] == hash[1]);
}

/* fx_bus replaced by a new bus loaded from its save, the same ports attached */
static void fx_reload_bus(void)
{
   link_bus_t *b = link_bus_new(fx_consoles);
   size_t n      = fx_save();
   unsigned c, p;
   for (c = 0; c < fx_consoles; c++)
   {
      CHECK(link_bus_set_ports(b, c, fx_ptrs[c]));
      for (p = 0; p < FX_PORTS && fx_list[c][p].protocol_id; p++)
         fx_h[c][p] = link_bus_attach(b, c, p, fx_list[c][p].protocol_id,
               fx_list[c][p].clock_rate);
   }
   CHECK(link_bus_load(b, fx_blob, n));
   link_bus_free(fx_bus);
   fx_bus = b;
}

/* Equal spans: console 1 makes no frame-end publish in frames 1 and 2 and
 * keeps its place. Console 0 sends to it in frame 2 before its frame ends
 * (run 0), after (run 1), or before on a bus loaded between the two (run 2):
 * one landing and one hash. */
static void skipped_twice_keeps_place(void)
{
   struct link_bus_sent j[3];
   uint64_t hash[3];
   unsigned i, f;

   for (i = 0; i < 3; i++)
   {
      fx_two(61440, 61440);
      fx_own_frame(0, 0);
      fx_own_frame(1, 0);
      for (f = 1; f < 3; f++)
      {
         link_bus_frame_start(fx_bus, 0);
         CHECK(adv(0, fx_h[0][0], fx_tick[0][0], fx_tick[0][0] + 1024, fx_tick[0][0], NULL, NULL));
         link_bus_frame_start(fx_bus, 1);
         CHECK(adv(1, fx_h[1][0], fx_tick[1][0], fx_tick[1][0] + 1024, fx_tick[1][0], NULL, NULL));
         if (f == 2 && i == 1)
            link_bus_frame_end(fx_bus, 1);
         if (f == 2)
            CHECK(snd(0, 0, fx_tick[0][0] + 1500, 1, "a", 1));
         if (f != 2 || i != 1)
            link_bus_frame_end(fx_bus, 1);
         fx_tick[1][0] += 1024;
         fx_own_end(0);
         if (f == 1 && i == 2)
            fx_reload_bus();
      }
      CHECK(fx_edge_is(0, 3072, 1024, 61440));
      CHECK(fx_edge_is(1, 3072, 1024, 61440));
      fx_own_start(1, 0);
      memset(&j[i], 0, sizeof(j[i]));
      CHECK(link_bus_journal(fx_bus, &j[i], 1) == 1);
      hash[i] = link_bus_hash(fx_bus);
      fx_free();
   }
   for (i = 0; i < 3; i++)
      CHECK(j[i].landing == 3548 && hash[i] == hash[0]);
}

static void instants_near_top(void)
{
   uint64_t top = (uint64_t)1 << 62;
   unsigned f;

   fx_new(1);
   fx_port(0, 0, "test-cable", top, 2, 0);
   fx_attach(0);
   fx_span[0][0] = top;
   for (f = 0; f < 3; f++)
      fx_own_frame(0, 0);
   CHECK(fx_edge_is(0, 3 * top, top, top));
   link_bus_renumbered(fx_bus, 0);
   fx_tick[0][0] = 0;
   for (f = 0; f < 2; f++)
      fx_own_frame(0, 0);
   CHECK(fx_tick[0][0] == (uint64_t)1 << 63);
   CHECK(fx_edge_is(0, TICK_MAX, top, top));
   fx_free();

   CHECK(link_bus_instant_before((uint64_t)-3, 3, (uint64_t)-2, 3));
   CHECK(!link_bus_instant_before((uint64_t)-2, 3, (uint64_t)-3, 3));
   CHECK(link_bus_instant_before((uint64_t)-2, (uint64_t)-1, 1, 1));
   CHECK(!link_bus_instant_before((uint64_t)-1, (uint64_t)-2, 1, 1));
}

/* A lone port's hash after f frames: only since differs */
static uint64_t fx_lone_hash(unsigned f)
{
   uint64_t hash;
   fx_new(1);
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_attach(0);
   while (f--)
      fx_own_frame(0, 0);
   hash = link_bus_hash(fx_bus);
   fx_free();
   return hash;
}

static void blob_v2_since(void)
{
   static unsigned char old[sizeof(fx_blob)];
   uint64_t s[2], n[2], r[2], hash;
   link_bus_t *b;
   size_t len;
   unsigned c;

   fx_spans();
   fx_spans_frames();
   len = fx_save();
   memcpy(old, fx_blob, len);
   old[4] = 1;                   /* version 1 */
   b = link_bus_new(2);
   for (c = 0; c < 2; c++)
   {
      CHECK(link_bus_set_ports(b, c, fx_ptrs[c]));
      CHECK(link_bus_attach(b, c, 0, "test-cable", fx_list[c][0].clock_rate) != NULL);
   }
   hash = link_bus_hash(b);
   CHECK(!link_bus_load(b, old, len));
   CHECK(link_bus_hash(b) == hash);
   CHECK(link_bus_load(b, fx_blob, len));
   CHECK(link_bus_hash(b) == link_bus_hash(fx_bus));
   for (c = 0; c < 2; c++)
   {
      CHECK(link_bus_console_edge(b, c, &s[0], &n[0], &r[0]));
      CHECK(link_bus_console_edge(fx_bus, c, &s[1], &n[1], &r[1]));
      CHECK(s[0] == s[1] && n[0] == n[1] && r[0] == r[1]);
   }
   CHECK(fx_reloads());
   link_bus_free(b);
   fx_free();

   CHECK(fx_lone_hash(2) != fx_lone_hash(3));
}

/* Membership at each console's own edge: fx_spans' rates unless said,
 * console 0 at 60000 with a span of 1001 and console 1 at 16777216 with
 * 280896 */

/* Console c opens its frame: its frame-start advance on port 0 promising
 * safe, asking for reports */
static uint32_t fx_open(unsigned c, uint64_t safe)
{
   uint32_t flags = 0;
   link_bus_frame_start(fx_bus, c);
   CHECK(adv(c, fx_h[c][0], fx_tick[c][0], safe, fx_tick[c][0], NULL, &flags));
   return flags;
}

/* fx_spans' ports, not cabled; console 0 at edge 2 */
static void fx_apart(void)
{
   fx_new(2);
   fx_port(0, 0, "test-cable", 60000, 2, 0);
   fx_port(1, 0, "test-cable", 16777216, 2, 0);
   fx_attach(0);
   fx_attach(1);
   fx_span[0][0] = 1001;
   fx_span[1][0] = 280896;
   fx_own_frame(0, 0);
   fx_own_frame(0, 0);
}

/* p's index and count, as one number */
static int fx_peers(unsigned c, unsigned p)
{
   unsigned count = 99;
   int index      = link_bus_peers(fx_bus, c, fx_h[c][p], &count);
   return (index + 1) * 16 + (int)count;
}

static void plug_joins_at_later_first_edge(void)
{
   char buf[4];
   uint64_t grant = 0, tick = 0;
   size_t len     = sizeof(buf);

   fx_apart();
   fx_own_frame(1, 0);
   CHECK(fx_open(1, 280896) == RETRO_LINK_WAKE_NONE); /* frame 1, to 561792 */
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(fx_peers(1, 0) == 0);
   CHECK(fx_open(0, 2005) == RETRO_LINK_WAKE_TOPOLOGY);
   CHECK(fx_peers(0, 0) == 1 * 16 + 2);
   /* J is console 1's frame end, 561792: 2009.12 on console 0 */
   CHECK(snd(0, 0, 2005, 1, "a", 1));
   CHECK(adv(0, fx_h[0][0], 2002, 2010, 2002, NULL, NULL));
   CHECK(snd(0, 0, 2010, 1, "b", 1)); /* lands at 562037 */
   CHECK(adv(0, fx_h[0][0], 2002, 2100, 2009, &grant, NULL) && grant == 2009);
   CHECK(!adv(0, fx_h[0][0], 2002, 2100, 2010, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 280896, 280896, 280896, &grant, NULL));
   CHECK(grant == RETRO_LINK_UNBOUNDED);
   fx_own_end(1);
   CHECK(fx_open(1, 562200) == RETRO_LINK_WAKE_TOPOLOGY);
   CHECK(fx_peers(1, 0) == 2 * 16 + 2);
   CHECK(adv(0, fx_h[0][0], 2002, 2100, 2010, &grant, NULL) && grant == 2010);
   CHECK(!adv(0, fx_h[0][0], 2002, 2100, 2011, NULL, NULL));
   CHECK(!adv(1, fx_h[1][0], 561792, 600000, 590000, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 561792, 600000, 587202, &grant, NULL) && grant == 587202);
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], &tick, NULL, buf, &len));
   CHECK(buf[0] == 'b' && tick == 562037);
   len = sizeof(buf);
   CHECK(!link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, buf, &len));
   fx_free();
}

static void join_waits_for_unknown_frame_end(void)
{
   uint64_t grant = 0;

   fx_new(2);
   fx_port(0, 0, "test-cable", 60000, 2, 0);
   fx_port(1, 0, "test-cable", 16777216, 2, 0);
   fx_attach(0);
   fx_span[0][0] = 1001;
   fx_own_frame(0, 0);
   fx_own_frame(0, 0);
   fx_attach(1);                  /* placed at console 0's edge, 2002: 559800 */
   fx_span[1][0] = 280896;
   CHECK(fx_edge_is(1, 559800, 0, 16777216));
   CHECK(fx_open(1, 0) == RETRO_LINK_WAKE_NONE); /* its first frame: no span */
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0)); /* waits for a step's end */
   fx_open(0, 4000);
   CHECK(adv(0, fx_h[0][0], 2002, 4000, 2003, NULL, NULL));
   fx_own_end(1);                 /* edge 840696: 3006.5 on console 0 */
   link_bus_step_end(fx_bus);     /* the join, at that later first edge */
   CHECK(adv(0, fx_h[0][0], 2002, 4000, 3003, NULL, NULL));
   fx_own_end(0);
   fx_open(0, 4000);
   CHECK(adv(0, fx_h[0][0], 3003, 4000, 3006, &grant, NULL) && grant == 3006);
   CHECK(!adv(0, fx_h[0][0], 3003, 4000, 3007, NULL, NULL));
   fx_free();
}

static void unplug_silent_until_own_edge(void)
{
   uint64_t grant = 0;

   fx_spans();
   fx_own_frame(0, 0);            /* edge 1001 */
   fx_own_frame(1, 0);            /* edge t = 280896: 1004.56 on console 0 */
   fx_open(0, 3000);              /* frame 1, begun before t */
   CHECK(!adv(0, fx_h[0][0], 1001, 3000, 1500, NULL, NULL));
   CHECK(link_bus_uncable(fx_bus, 1, 0));
   CHECK(fx_peers(1, 0) == 0);
   CHECK(adv(0, fx_h[0][0], 1001, 3000, 2002, &grant, NULL) && grant == 2002);
   CHECK(fx_peers(0, 0) == 1 * 16 + 2);
   fx_own_end(0);
   CHECK(fx_peers(0, 0) == 0);
   CHECK(fx_open(0, 3000) == RETRO_LINK_WAKE_TOPOLOGY);
   fx_free();
}

static void unplug_drops_late_landings(void)
{
   char buf[8];
   uint64_t tick = 0, grant = 0;
   size_t len    = sizeof(buf);

   fx_spans();
   fx_own_frame(0, 0);            /* edge 1001, promising it */
   link_bus_frame_start(fx_bus, 1);
   CHECK(adv(1, fx_h[1][0], 0, 280000, 0, NULL, NULL));
   CHECK(snd(1, 0, 280000, 0, "own", 3)); /* lands at 1002 */
   link_bus_frame_start(fx_bus, 0);
   CHECK(adv(0, fx_h[0][0], 1001, 1002, 1001, NULL, NULL));
   CHECK(snd(0, 0, 1002, 1, "early", 5)); /* lands at 280180 */
   fx_own_end(1);                 /* edge t = 280896 */
   CHECK(link_bus_uncable(fx_bus, 1, 0));
   CHECK(snd(0, 0, 1003, 1, "late", 4));  /* at console 1's edge, t */
   CHECK(snd(0, 0, 1020, 1, "late2", 5)); /* 285213 */
   CHECK(adv(0, fx_h[0][0], 1001, 1100, 1100, &grant, NULL) && grant == 1100);
   CHECK(link_bus_recv(fx_bus, 0, fx_h[0][0], &tick, NULL, buf, &len));
   CHECK(len == 3 && !memcmp(buf, "own", 3) && tick == 1002);
   link_bus_frame_start(fx_bus, 1);
   CHECK(adv(1, fx_h[1][0], 280896, 290000, 280896, &grant, NULL));
   CHECK(grant == RETRO_LINK_UNBOUNDED);
   len = sizeof(buf);
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], &tick, NULL, buf, &len));
   CHECK(len == 5 && !memcmp(buf, "early", 5) && tick == 280180);
   len = sizeof(buf);
   CHECK(!link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, buf, &len));
   fx_free();
}

static void uncable_refused_past_first_edge(void)
{
   fx_spans();
   fx_own_start(1, 0);
   CHECK(!link_bus_uncable(fx_bus, 1, 0)); /* console 1 inside a frame */
   fx_own_end(1);                 /* t = 280896 */
   fx_own_frame(0, FX_FAR);
   fx_own_start(0, FX_FAR);       /* begun at 1001, before t */
   CHECK(link_bus_uncable(fx_bus, 1, 0));
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   fx_own_end(0);
   fx_own_start(0, FX_FAR);       /* begun at 2002, after t */
   CHECK(!link_bus_uncable(fx_bus, 1, 0));
   fx_own_end(0);
   CHECK(link_bus_uncable(fx_bus, 1, 0));
   fx_free();
}

/* Console 2 sees console 1 pulled at its edge and sends from 1001; console
 * 0, whose edge is at 280896, still saw console 2 as index 2 there */
static void from_index_in_receiver_view(void)
{
   uint64_t grant = 0;
   unsigned from  = 99, c;
   size_t len     = 0;

   fx_new(3);
   fx_port(0, 0, "test-cable", 16777216, 3, 0);
   fx_port(1, 0, "test-cable", 60000, 3, 0);
   fx_port(2, 0, "test-cable", 60000, 3, 0);
   for (c = 0; c < 3; c++)
      fx_attach(c);
   fx_span[0][0] = 280896;
   fx_span[1][0] = 1001;
   fx_span[2][0] = 1001;
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_cable(fx_bus, 0, 0, 2, 0));
   fx_own_frame(1, FX_FAR);       /* t = 1001 */
   fx_own_start(0, FX_FAR);
   fx_own_start(2, 0);
   CHECK(link_bus_uncable(fx_bus, 1, 0));
   fx_own_end(2);
   CHECK(fx_peers(2, 0) == 2 * 16 + 2);
   link_bus_frame_start(fx_bus, 2);
   CHECK(adv(2, fx_h[2][0], 1001, 1001, 1001, NULL, NULL));
   CHECK(snd(2, 0, 1001, 0, "x", 1)); /* lands at 279900 */
   CHECK(adv(2, fx_h[2][0], 1001, 1100, 1001, NULL, NULL));
   CHECK(fx_peers(0, 0) == 1 * 16 + 3);
   CHECK(adv(0, fx_h[0][0], 0, FX_FAR, 279901, &grant, NULL) && grant == 279901);
   CHECK(link_bus_peek(fx_bus, 0, fx_h[0][0], NULL, &from, &len) != NULL);
   CHECK(from == 2);
   fx_free();
}

static void hold_at_own_edge(void)
{
   fx_spans();
   fx_own_start(1, 0);
   CHECK(!link_bus_hold(fx_bus, 1));
   fx_own_end(1);
   fx_own_start(0, 0);
   CHECK(link_bus_hold(fx_bus, 1));
   CHECK(link_bus_is_held(fx_bus, 1));
   fx_free();
}

/* Console 1's clocks, 16777216 and 4194304, have whole ticks every
 * 1/4194304 s; console 0 has a span of 1000 at 60000 */
static void unhold_places_on_rate_grid(void)
{
   struct link_bus_sent j;
   uint64_t grant = 0;

   fx_new(2);
   fx_port(0, 0, "test-cable", 60000, 2, 0);
   fx_port(1, 0, "test-cable", 16777216, 2, 0);
   fx_port(1, 1, "test-other", 4194304, 2, 0);
   fx_attach(0);
   fx_attach(1);
   fx_span[0][0] = 1000;
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_hold(fx_bus, 1));
   fx_own_frame(0, 0);            /* edge 1000 */
   link_bus_frame_start(fx_bus, 0);
   CHECK(adv(0, fx_h[0][0], 1000, 1500, 1000, NULL, NULL));
   CHECK(link_bus_unhold(fx_bus, 1));
   /* console 0's frame end, 2000: 139810.13 of 1/4194304 s, rounded up */
   CHECK(fx_edge_is(1, 559244, 0, 16777216));
   CHECK(snd(0, 0, 1500, 1, "m", 1));
   CHECK(adv(0, fx_h[0][0], 1000, 1500, 1500, &grant, NULL) && grant == 1500);
   link_bus_frame_start(fx_bus, 1);
   fx_all_ports(1, false);
   CHECK(link_bus_journal(fx_bus, &j, 1) == 1 && j.landing == 0);
   fx_free();
}

static void attach_in_frame_joins_at_own_edge(void)
{
   retro_link_port_t *h;

   fx_new(2);
   fx_port(0, 0, "test-radio", 60000, 0, RETRO_LINK_PORT_SHARED_MEDIUM);
   fx_port(1, 0, "test-radio", 16777216, 0, RETRO_LINK_PORT_SHARED_MEDIUM);
   fx_attach(0);
   fx_span[0][0] = 1001;
   CHECK(link_bus_set_ports(fx_bus, 1, fx_ptrs[1]));
   fx_own_frame(0, 0);            /* edge 1001 */
   fx_open(0, 1001);              /* frame 1, to 2002 */
   link_bus_frame_start(fx_bus, 1);
   h = link_bus_attach(fx_bus, 1, 0, "test-radio", 16777216);
   fx_h[1][0] = h;
   CHECK(h != NULL && fx_peers(1, 0) == 0);
   link_bus_frame_end(fx_bus, 1);
   CHECK(fx_peers(1, 0) == 0);
   link_bus_step_end(fx_bus);     /* placed at console 0's frame end: 559800 */
   CHECK(fx_edge_is(1, 559800, 0, 16777216));
   CHECK(fx_peers(1, 0) == 2 * 16 + 2);
   CHECK(fx_peers(0, 0) == 0);
   fx_own_end(0);
   CHECK(fx_peers(0, 0) == 1 * 16 + 2);
   CHECK(fx_open(0, 2002) == RETRO_LINK_WAKE_TOPOLOGY);
   fx_free();
}

static void detach_in_frame_leaves_at_own_edge(void)
{
   uint64_t grant = 0;

   fx_new(2);
   fx_port(0, 0, "test-radio", 60000, 0, RETRO_LINK_PORT_SHARED_MEDIUM);
   fx_port(1, 0, "test-radio", 16777216, 0, RETRO_LINK_PORT_SHARED_MEDIUM);
   fx_attach(0);
   fx_attach(1);
   fx_span[0][0] = 1001;
   fx_span[1][0] = 280896;
   fx_own_frame(0, 0);            /* edge 1001 */
   fx_own_frame(1, 0);            /* edge 280896 */
   fx_open(1, 280896);            /* frame 1, to t = 561792: 2009.12 on console 0 */
   fx_open(0, 3000);              /* frame 1, to 2002 */
   CHECK(!adv(0, fx_h[0][0], 1001, 3000, 1500, NULL, NULL));
   link_bus_detach(fx_bus, 1, fx_h[1][0]);
   CHECK(adv(0, fx_h[0][0], 1001, 3000, 2002, &grant, NULL) && grant == 2002);
   link_bus_frame_end(fx_bus, 1);
   link_bus_step_end(fx_bus);     /* leaves at t */
   fx_own_end(0);                 /* 2002, before t */
   CHECK(fx_peers(0, 0) == 1 * 16 + 2);
   CHECK(fx_open(0, 3000) == RETRO_LINK_WAKE_NONE);
   fx_own_end(0);                 /* 3003 */
   CHECK(fx_peers(0, 0) == 0);
   CHECK(fx_open(0, 3003) == RETRO_LINK_WAKE_TOPOLOGY);
   fx_free();
}

/* A port at 60000 attached beside one at 16777216 standing at 280896: the
 * clocks share whole ticks every 1/32 s */
static void attach_off_grid_places_again(void)
{
   fx_new(1);
   fx_h[0][0] = link_bus_attach(fx_bus, 0, 0, "test-cable", 16777216);
   CHECK(fx_h[0][0] != NULL);
   fx_span[0][0] = 280896;
   fx_own_frame(0, 0);
   CHECK(fx_edge_is(0, 280896, 280896, 16777216));
   CHECK(link_bus_attach(fx_bus, 0, 1, "test-other", 60000) != NULL);
   CHECK(fx_edge_is(0, 524288, 0, 16777216));
   fx_free();
}

/* Console 1 attaches its radio inside its frame 0 while console 0 runs two
 * frames, in the order given: the join is made at the step's end, so its
 * instants and the bus do not depend on the order */
static uint64_t fx_attach_in_order(unsigned order)
{
   uint64_t hash;
   fx_new(2);
   fx_port(0, 0, "test-radio", 60000, 0, RETRO_LINK_PORT_SHARED_MEDIUM);
   fx_port(1, 0, "test-radio", 16777216, 0, RETRO_LINK_PORT_SHARED_MEDIUM);
   fx_attach(0);
   fx_span[0][0] = 1001;
   CHECK(link_bus_set_ports(fx_bus, 1, fx_ptrs[1]));
   if (order)
      fx_own_frame(0, 0);
   if (order == 1)
      fx_own_frame(0, 0);
   link_bus_frame_start(fx_bus, 1);
   fx_h[1][0] = link_bus_attach(fx_bus, 1, 0, "test-radio", 16777216);
   CHECK(fx_h[1][0] != NULL);
   link_bus_frame_end(fx_bus, 1);
   if (order != 1)
      fx_own_frame(0, 0);
   if (!order)
      fx_own_frame(0, 0);
   link_bus_step_end(fx_bus);
   /* placed at console 0's edge 2, 2002: 559800 */
   CHECK(fx_edge_is(1, 559800, 0, 16777216));
   CHECK(fx_peers(0, 0) == 1 * 16 + 2 && fx_peers(1, 0) == 2 * 16 + 2);
   hash = link_bus_hash(fx_bus);
   fx_free();
   return hash;
}

static void attach_in_frame_same_in_any_order(void)
{
   uint64_t hash = fx_attach_in_order(0);
   CHECK(fx_attach_in_order(1) == hash);
   CHECK(fx_attach_in_order(2) == hash);
}

/* Console 1 detaches its port inside its frame 1, and console 0 sends to it
 * at 1500, inside that frame, before (order 0) or after (order 1) console
 * 1's frame ends: the message lands at the same tick either way */
static uint64_t fx_pulled_send(unsigned order)
{
   struct link_bus_sent j;
   uint64_t landing = 0;
   fx_two(61440, 61440);
   fx_run_frame();
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 1024, 1500, 1024, NULL, NULL));
   CHECK(adv(1, fx_h[1][0], 1024, 2048, 1024, NULL, NULL));
   link_bus_detach(fx_bus, 1, fx_h[1][0]);
   if (order)
      link_bus_frame_end(fx_bus, 1);
   CHECK(snd(0, 0, 1500, 1, "a", 1));
   if (!order)
      link_bus_frame_end(fx_bus, 1);
   CHECK(link_bus_journal(fx_bus, &j, 1) == 1);
   landing = j.landing;
   fx_free();
   return landing;
}

static void detached_in_frame_lands_in_any_order(void)
{
   CHECK(fx_pulled_send(0) == 1500);
   CHECK(fx_pulled_send(1) == 1500);
}

/* Console 0, at its edge 1024, is cabled to console 1 at its edge 2048 and
 * sends to it at 1500, before the join instant: the message is dropped when
 * sent. In order 1 a load has renumbered console 1, whose new frame has not
 * published its tick yet; the message lands nowhere either way */
static uint64_t fx_dropped_send(unsigned order)
{
   struct link_bus_sent j;
   uint64_t landing = 0;
   fx_new(2);
   fx_port(0, 0, "test-cable", 61440, 2, 0);
   fx_port(1, 0, "test-cable", 61440, 2, 0);
   fx_attach(0);
   fx_attach(1);
   fx_own_frame(0, 0);
   fx_own_frame(1, 0);
   fx_own_frame(1, 0);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   if (order)
      link_bus_renumbered(fx_bus, 1);
   link_bus_journal_reset(fx_bus);
   link_bus_frame_start(fx_bus, 1);
   link_bus_frame_start(fx_bus, 0);
   CHECK(adv(0, fx_h[0][0], 1024, 1500, 1024, NULL, NULL));
   CHECK(snd(0, 0, 1500, 1, "a", 1));
   CHECK(link_bus_journal(fx_bus, &j, 1) == 1);
   landing = j.landing;
   fx_free();
   return landing;
}

static void dropped_send_lands_nowhere(void)
{
   uint64_t landing = fx_dropped_send(0);
   CHECK(fx_dropped_send(1) == landing);
   CHECK(landing == RETRO_LINK_UNBOUNDED);
}

/* Console 1 is in its first frame, its span not known; console 2 is cabled
 * to it, and the join waits for a step's end. Console 2 opens its frame and
 * sends to console 1 before (order 0) or after (order 1) console 1's frame
 * ends: it reaches nobody either way, and the join is made at the step's
 * end */
static void fx_unplaced_send(unsigned order, uint64_t out[4])
{
   char buf[4];
   size_t len     = sizeof(buf);
   uint64_t tick  = 0, grant = 0;
   unsigned count = 0;
   fx_new(3);
   fx_port(0, 0, "test-cable", 60000, 2, 0);
   fx_port(1, 0, "test-cable", 16777216, 2, 0);
   fx_port(2, 0, "test-cable", 60000, 2, 0);
   fx_attach(0);
   fx_span[0][0] = 1001;
   fx_own_frame(0, 0);
   fx_own_frame(0, 0);
   fx_attach(1);
   fx_span[1][0] = 280896;
   fx_open(1, 0);
   fx_attach(2);
   fx_span[2][0] = 1001;
   CHECK(link_bus_cable(fx_bus, 1, 0, 2, 0));
   link_bus_frame_start(fx_bus, 2);
   if (order)
      fx_own_end(1);
   CHECK(adv(2, fx_h[2][0], 0, 0, 0, NULL, NULL));
   out[0] = snd(2, 0, 0, 0, "z", 1);
   if (!order)
      fx_own_end(1);
   fx_own_end(2);
   fx_open(1, FX_FAR);
   out[1] = adv(1, fx_h[1][0], 280896, FX_FAR, 290000, &grant, NULL);
   out[2] = link_bus_recv(fx_bus, 1, fx_h[1][0], &tick, NULL, buf, &len) ? tick : 0;
   link_bus_step_end(fx_bus);
   out[3] = link_bus_peers(fx_bus, 2, fx_h[2][0], &count) == 1 && count == 2;
   fx_free();
}

static void unplaced_sender_same_in_any_order(void)
{
   uint64_t out[2][4];
   fx_unplaced_send(0, out[0]);
   fx_unplaced_send(1, out[1]);
   CHECK(out[1][0] == 1); /* sent, to nobody */
   CHECK(out[1][1] == 1);
   CHECK(out[1][2] == 0);
   CHECK(out[1][3] == 1);
   CHECK(!memcmp(out[0], out[1], sizeof(out[0])));
}

/* Radios taking two: consoles 1 and 2 on the medium, console 2 inside its
 * frame 1; console 0's radio, attached at its edge, would push console 2's
 * out, so it waits for console 2's edge */
static void max_ports_keeps_place_in_frame(void)
{
   unsigned c;
   fx_new(3);
   for (c = 0; c < 3; c++)
      fx_port(c, 0, "test-radio", 61440, 2, RETRO_LINK_PORT_SHARED_MEDIUM);
   CHECK(link_bus_set_ports(fx_bus, 0, fx_ptrs[0]));
   fx_attach(1);
   fx_attach(2);
   fx_own_frame(1, 0);
   fx_own_frame(1, 0);
   fx_own_frame(2, 0);
   fx_own_start(2, 0);
   fx_h[0][0] = link_bus_attach(fx_bus, 0, 0, "test-radio", 61440);
   CHECK(fx_h[0][0] != NULL);
   CHECK(fx_peers(0, 0) == 0);
   CHECK(fx_peers(2, 0) == 2 * 16 + 2);
   fx_own_end(2);
   link_bus_step_end(fx_bus);
   CHECK(fx_peers(2, 0) == 0);
   CHECK(fx_peers(0, 0) == 1 * 16 + 2);
   CHECK(fx_peers(1, 0) == 2 * 16 + 2);
   fx_free();
}

/* Console 1 has shown no span since it was placed at 0: its port joining a
 * bus places it again, at console 0's edge 2 */
static void join_places_console_with_no_span(void)
{
   fx_apart();
   link_bus_frame_start(fx_bus, 1);
   CHECK(adv(1, fx_h[1][0], 0, 0, 0, NULL, NULL));
   link_bus_frame_end(fx_bus, 1);
   CHECK(fx_edge_is(1, 0, 0, 16777216));
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(fx_edge_is(1, 559800, 0, 16777216));
   fx_free();
}

/* Consoles 0 and 2 at 60000 with spans of 1001, console 1 as fx_spans';
 * console 0 at edge 1 is cabled to console 1 at its edge 1, so they hear
 * each other from 280896 (1004.56 on console 0), and console 2 at edge 2
 * (2002) is pulled from console 0, which sees it until its edge 2 */
static void fx_pending(void)
{
   unsigned c;
   fx_new(3);
   fx_port(0, 0, "test-cable", 60000, 3, 0);
   fx_port(1, 0, "test-cable", 16777216, 3, 0);
   fx_port(2, 0, "test-cable", 60000, 3, 0);
   for (c = 0; c < 3; c++)
      fx_attach(c);
   fx_span[0][0] = 1001;
   fx_span[1][0] = 280896;
   fx_span[2][0] = 1001;
   CHECK(link_bus_cable(fx_bus, 0, 0, 2, 0));
   fx_own_frame(0, 0);
   fx_own_frame(1, 0);
   fx_own_frame(2, FX_FAR);
   fx_own_frame(2, FX_FAR);
   CHECK(link_bus_uncable(fx_bus, 2, 0));
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
}

/* What fx_pending's session does next, the same on any bus */
static void fx_pending_run(uint64_t out[8])
{
   char buf[4];
   size_t len = sizeof(buf);
   out[0] = fx_open(0, 1002);
   out[1] = (uint64_t)fx_peers(0, 0);
   CHECK(snd(0, 0, 1002, 1, "a", 1)); /* before 1004.56: dropped */
   CHECK(adv(0, fx_h[0][0], 1001, 1005, 1001, NULL, NULL));
   CHECK(snd(0, 0, 1005, 1, "b", 1));
   out[2] = adv(0, fx_h[0][0], 1001, 1100, 1004, NULL, NULL);
   out[3] = adv(0, fx_h[0][0], 1001, 1100, 1005, NULL, NULL);
   fx_own_end(0);
   out[4] = (uint64_t)fx_peers(0, 0);
   out[5] = fx_open(1, 300000);
   CHECK(adv(1, fx_h[1][0], 280896, 300000, 290000, NULL, NULL));
   CHECK(link_bus_recv(fx_bus, 1, fx_h[1][0], &out[6], NULL, buf, &len) && buf[0] == 'b');
   len = sizeof(buf);
   CHECK(!link_bus_recv(fx_bus, 1, fx_h[1][0], NULL, NULL, buf, &len));
   out[7] = link_bus_hash(fx_bus);
}

/* Console 0 is inside its first frame and has shown no span, so its first
 * edge is not known: a hold's end and a join wait for a step's end at
 * which it is */
static void join_waits_for_first_span(void)
{
   unsigned count = 0;
   fx_new(3);
   fx_port(0, 0, "test-cable", 61440, 3, 0);
   fx_port(1, 0, "test-cable", 61440, 3, 0);
   fx_port(2, 0, "test-cable", 61440, 3, 0);
   fx_attach(0);
   fx_attach(1);
   fx_attach(2);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_hold(fx_bus, 1));
   fx_own_start(0, 0);
   CHECK(link_bus_unhold(fx_bus, 1));
   CHECK(link_bus_is_held(fx_bus, 1));
   CHECK(link_bus_cable(fx_bus, 1, 0, 2, 0));
   CHECK(link_bus_peers(fx_bus, 2, fx_h[2][0], &count) == -1);
   link_bus_step_end(fx_bus);
   CHECK(link_bus_is_held(fx_bus, 1));
   CHECK(link_bus_peers(fx_bus, 2, fx_h[2][0], &count) == -1);
   fx_own_end(0);
   CHECK(link_bus_is_held(fx_bus, 1));
   link_bus_step_end(fx_bus);
   CHECK(!link_bus_is_held(fx_bus, 1));
   CHECK(fx_edge_is(1, 1024, 0, 61440));
   CHECK(link_bus_peers(fx_bus, 2, fx_h[2][0], &count) == 2 && count == 3);
   fx_free();
}

static void membership_blob_roundtrip(void)
{
   uint64_t out[2][8];
   unsigned run;
   for (run = 0; run < 2; run++)
   {
      fx_pending();
      if (run)
         fx_reload_bus();
      fx_pending_run(out[run]);
      fx_free();
   }
   CHECK(out[0][0] == RETRO_LINK_WAKE_TOPOLOGY && out[0][1] == 1 * 16 + 3);
   CHECK(out[0][2] == 1 && out[0][3] == 0 && out[0][4] == 1 * 16 + 2);
   CHECK(out[0][5] == RETRO_LINK_WAKE_TOPOLOGY);
   CHECK(!memcmp(out[0], out[1], sizeof(out[0])));
}

/* Console 1 inside a frame and console 0 at an edge, then both in a frame:
 * a save needs the flag of every console inside one, and a load waits for
 * every console at an edge */
static void save_needs_flag_in_frame(void)
{
   static unsigned char edge[4096];
   size_t n;
   fx_two(61440, 61440);
   fx_run_frame();
   n = fx_save();
   memcpy(edge, fx_blob, n);
   link_bus_frame_start(fx_bus, 1);
   fx_all_ports(1, false);
   CHECK(!link_bus_save(fx_bus, fx_blob, sizeof(fx_blob)));
   link_bus_set_flags(fx_bus, 1, ~0u);
   CHECK(link_bus_get_flags(fx_bus, 1) == RETRO_LINK_SAVE_IN_ADVANCE);
   CHECK(link_bus_get_flags(fx_bus, 0) == 0);
   CHECK(link_bus_save(fx_bus, fx_blob, sizeof(fx_blob)));
   link_bus_frame_start(fx_bus, 0);
   CHECK(!link_bus_save(fx_bus, fx_blob, sizeof(fx_blob)));
   link_bus_set_flags(fx_bus, 0, RETRO_LINK_SAVE_IN_ADVANCE);
   CHECK(link_bus_save(fx_bus, fx_blob, sizeof(fx_blob)));
   CHECK(!link_bus_load(fx_bus, edge, n));
   fx_all_ports(0, false);
   frame();
   CHECK(link_bus_load(fx_bus, edge, n));
   fx_free();
}

/* Console 1, flagged, is inside an advance requesting request that waits
 * on console 0: console 0 has run its frame, promising 1100, and sent it a
 * message stamped 600; console 1's frame-start advance asked for nothing
 * and promised 2048 */
static void fx_parked(uint64_t request, struct retro_link_wait *w)
{
   fx_two(61440, 61440);
   link_bus_set_flags(fx_bus, 1, RETRO_LINK_SAVE_IN_ADVANCE);
   link_bus_frame_start(fx_bus, 0);
   CHECK(adv(0, fx_h[0][0], 0, 600, 0, NULL, NULL));
   CHECK(snd(0, 0, 600, 1, "m", 1));
   CHECK(adv(0, fx_h[0][0], 1024, 1100, 1024, NULL, NULL));
   link_bus_frame_end(fx_bus, 0);
   link_bus_frame_start(fx_bus, 1);
   CHECK(adv(1, fx_h[1][0], 0, 2048, 0, NULL, NULL));
   fx_wait(w, fx_h[1][0], 0, 2048, request, true);
   CHECK(!advm(1, w, 1));
}

/* Console 0's next frame-start advance promises 2048 and the waiting
 * advance is granted: its grant, its wake flags and the hash */
static void fx_parked_run(struct retro_link_wait *w, uint64_t out[3])
{
   struct retro_link_wait *list = w;
   link_bus_frame_start(fx_bus, 0);
   CHECK(adv(0, fx_h[0][0], 1024, 2048, 1024, NULL, NULL));
   CHECK(link_bus_grantable(fx_bus, 1, &list, 1));
   link_bus_grant(fx_bus, 1, &list, 1);
   out[0] = w->grant;
   out[1] = w->wake_flags;
   out[2] = link_bus_hash(fx_bus);
}

/* fx_parked's session run on (run 0), and saved there and loaded into a
 * new bus, on which console 1 makes the advance again (run 1) */
static void fx_parked_twice(uint64_t request, uint64_t out[2][3],
      struct link_bus_counts k[2])
{
   unsigned run;
   for (run = 0; run < 2; run++)
   {
      struct retro_link_wait w;
      fx_parked(request, &w);
      if (run)
      {
         fx_reload_bus();
         CHECK(link_bus_resume_pending(fx_bus, 1));
         link_bus_frame_resume(fx_bus, 1);
         CHECK(!link_bus_resume_pending(fx_bus, 1));
         fx_wait(&w, fx_h[1][0], 0, 2048, request, true);
         CHECK(!advm(1, &w, 1));
      }
      fx_parked_run(&w, out[run]);
      k[run]        = counts();
      k[run].blocks = 0;
      fx_free();
   }
}

static void parked_save_load_remakes_advance(void)
{
   uint64_t out[2][3];
   struct link_bus_counts k[2];
   fx_parked_twice(1500, out, k);
   CHECK(out[0][0] == 1500);
   CHECK(out[0][1] == (RETRO_LINK_WAKE_TOPOLOGY | RETRO_LINK_WAKE_MESSAGE));
   CHECK(!memcmp(out[0], out[1], sizeof(out[0])));
   CHECK(!memcmp(&k[0], &k[1], sizeof(k[0])));
}

/* A request past the promise is repaired once: the advance made again
 * after the load counts nothing again */
static void parked_save_repaired_request(void)
{
   uint64_t out[2][3];
   struct link_bus_counts k[2];
   fx_parked_twice(3000, out, k);
   CHECK(out[0][0] == 2048);
   CHECK(k[0].request_past_promise == 1 && k[1].request_past_promise == 1);
   CHECK(!memcmp(out[0], out[1], sizeof(out[0])));
   CHECK(!memcmp(&k[0], &k[1], sizeof(k[0])));
}

/* The loaded console stays inside its frame, so the new bus saves only
 * with its flag, and then the blob it loaded */
static void save_right_after_load_equal(void)
{
   static unsigned char saved[4096];
   struct retro_link_wait w;
   size_t n;
   fx_parked(1500, &w);
   n = fx_save();
   memcpy(saved, fx_blob, n);
   fx_reload_bus();
   CHECK(!link_bus_save(fx_bus, fx_blob, sizeof(fx_blob)));
   link_bus_set_flags(fx_bus, 1, RETRO_LINK_SAVE_IN_ADVANCE);
   CHECK(fx_save() == n && !memcmp(saved, fx_blob, n));
   fx_free();
}

/* Console 0 takes its pending report, then the session is released: the
 * load ends the release and the report is pending again */
static void load_ends_release(void)
{
   uint64_t grant = 0;
   uint32_t flags = 0;
   size_t n;
   fx_two(61440, 61440);
   fx_run_frame();
   n = fx_save();
   fx_begin();
   CHECK(adv(0, fx_h[0][0], 1024, 1100, 1024, NULL, &flags));
   CHECK(flags == RETRO_LINK_WAKE_TOPOLOGY);
   link_bus_release(fx_bus);
   fx_starts();
   frame();
   CHECK(link_bus_released(fx_bus));
   CHECK(link_bus_load(fx_bus, fx_blob, n));
   CHECK(!link_bus_released(fx_bus));
   fx_tick[0][0] = fx_tick[1][0] = 1024;
   fx_begin();
   flags = 0;
   CHECK(adv(0, fx_h[0][0], 1024, 1100, 1024, &grant, &flags));
   CHECK(grant == 1024 && flags == RETRO_LINK_WAKE_TOPOLOGY);
   fx_free();
}

/* join_waits_for_first_span's session with console 0 flagged, at the step
 * that ends inside console 0's first frame: the hold's end and the join
 * that wait for its edge */
static void fx_unholding(void)
{
   unsigned c;
   fx_new(3);
   for (c = 0; c < 3; c++)
   {
      fx_port(c, 0, "test-cable", 61440, 3, 0);
      fx_attach(c);
   }
   link_bus_set_flags(fx_bus, 0, RETRO_LINK_SAVE_IN_ADVANCE);
   CHECK(link_bus_cable(fx_bus, 0, 0, 1, 0));
   CHECK(link_bus_hold(fx_bus, 1));
   fx_own_start(0, 0);
   CHECK(link_bus_unhold(fx_bus, 1));
   CHECK(link_bus_cable(fx_bus, 1, 0, 2, 0));
   link_bus_step_end(fx_bus);
}

static void fx_unholding_run(uint64_t out[6])
{
   link_bus_step_end(fx_bus);
   out[0] = link_bus_is_held(fx_bus, 1);
   out[1] = (uint64_t)fx_peers(2, 0);
   fx_own_end(0);
   link_bus_step_end(fx_bus);
   out[2] = link_bus_is_held(fx_bus, 1);
   out[3] = (uint64_t)fx_peers(2, 0);
   out[4] = (uint64_t)fx_peers(0, 0);
   out[5] = link_bus_hash(fx_bus);
}

/* Saved there and loaded into a new bus, the session keeps the hold and
 * the join waiting while console 0 is inside its frame, and ends the hold
 * and makes the join at the same step */
static void hold_and_join_survive_load(void)
{
   static unsigned char saved[4096];
   uint64_t out[2][6];
   unsigned run;
   for (run = 0; run < 2; run++)
   {
      fx_unholding();
      if (run)
      {
         size_t n = fx_save();
         memcpy(saved, fx_blob, n);
         fx_reload_bus();
         link_bus_set_flags(fx_bus, 0, RETRO_LINK_SAVE_IN_ADVANCE);
         CHECK(fx_save() == n && !memcmp(saved, fx_blob, n));
         link_bus_frame_resume(fx_bus, 0);
      }
      fx_unholding_run(out[run]);
      fx_free();
   }
   CHECK(out[0][0] == 1 && out[0][1] == 0);
   CHECK(out[0][2] == 0 && out[0][3] == 3 * 16 + 3 && out[0][4] == 1 * 16 + 3);
   CHECK(!memcmp(out[0], out[1], sizeof(out[0])));
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
   { "max_ports_caps_shared_medium",         max_ports_caps_shared_medium },
   { "no_port_list_defaults",                no_port_list_defaults },
   { "bus_size_caps_at_255",                 bus_size_caps_at_255 },
   { "max_ports_caps_reattach",              max_ports_caps_reattach },
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
   { "activity_counts_sends_and_recvs",      activity_counts_sends_and_recvs },
   { "renumber_moves_messages",              renumber_moves_messages },
   { "renumber_drops_old_promise",           renumber_drops_old_promise },
   { "reset_keeps_messages_and_reports",     reset_keeps_messages_and_reports },
   { "hold_right_after_renumber",            hold_right_after_renumber },
   { "renumber_held_waits_for_unhold",       renumber_held_waits_for_unhold },
   { "renumber_keeps_frame_end_repair",      renumber_keeps_frame_end_repair },
   { "console_gone_detaches",                console_gone_detaches },
   { "release_unbounded_every_call",         release_unbounded_every_call },
   { "left_out_breach",                      left_out_breach },
   { "skipped_publish_breach",               skipped_publish_breach },
   { "misplaced_calls",                      misplaced_calls },
   { "save_load_roundtrip",                  save_load_roundtrip },
   { "load_refuses_bad_input",               load_refuses_bad_input },
   { "load_checks_cables_and_reports",       load_checks_cables_and_reports },
   { "load_caps_cables",                     load_caps_cables },
   { "load_resets_unlisted_records",         load_resets_unlisted_records },
   { "hash_ignores_numbering",               hash_ignores_numbering },
   { "dump_lists_ports_and_messages",        dump_lists_ports_and_messages },
   { "counts_survive_load",                  counts_survive_load },
   { "hash_and_save_ignore_interleaving",    hash_and_save_ignore_interleaving },
   { "instants_across_spans",                instants_across_spans },
   { "instants_rounding_both_ends",          instants_rounding_both_ends },
   { "edges_at_own_spans",                   edges_at_own_spans },
   { "skipped_publish_holds_at_promise",     skipped_publish_holds_at_promise },
   { "skipped_publish_lands_on_edge_before", skipped_publish_lands_on_edge_before },
   { "skipped_twice_keeps_place",            skipped_twice_keeps_place },
   { "instants_near_top",                    instants_near_top },
   { "blob_v2_since",                        blob_v2_since },
   { "plug_joins_at_later_first_edge",       plug_joins_at_later_first_edge },
   { "join_waits_for_unknown_frame_end",     join_waits_for_unknown_frame_end },
   { "unplug_silent_until_own_edge",         unplug_silent_until_own_edge },
   { "unplug_drops_late_landings",           unplug_drops_late_landings },
   { "uncable_refused_past_first_edge",      uncable_refused_past_first_edge },
   { "from_index_in_receiver_view",          from_index_in_receiver_view },
   { "hold_at_own_edge",                     hold_at_own_edge },
   { "unhold_places_on_rate_grid",           unhold_places_on_rate_grid },
   { "attach_in_frame_joins_at_own_edge",    attach_in_frame_joins_at_own_edge },
   { "detach_in_frame_leaves_at_own_edge",   detach_in_frame_leaves_at_own_edge },
   { "detached_in_frame_lands_in_any_order", detached_in_frame_lands_in_any_order },
   { "dropped_send_lands_nowhere",           dropped_send_lands_nowhere },
   { "attach_off_grid_places_again",         attach_off_grid_places_again },
   { "attach_in_frame_same_in_any_order",    attach_in_frame_same_in_any_order },
   { "unplaced_sender_same_in_any_order",    unplaced_sender_same_in_any_order },
   { "max_ports_keeps_place_in_frame",       max_ports_keeps_place_in_frame },
   { "join_places_console_with_no_span",     join_places_console_with_no_span },
   { "membership_blob_roundtrip",            membership_blob_roundtrip },
   { "join_waits_for_first_span",            join_waits_for_first_span },
   { "save_needs_flag_in_frame",             save_needs_flag_in_frame },
   { "parked_save_load_remakes_advance",     parked_save_load_remakes_advance },
   { "parked_save_repaired_request",         parked_save_repaired_request },
   { "save_right_after_load_equal",          save_right_after_load_equal },
   { "load_ends_release",                    load_ends_release },
   { "hold_and_join_survive_load",           hold_and_join_survive_load }
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
