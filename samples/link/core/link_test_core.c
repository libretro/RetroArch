/* The link test core: a small deterministic machine on the link interface.
 * Core options set its ports, clocks, steps, promises and sends; it logs an
 * error when the bus breaks a promise the header makes to it. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>

#define LT_MAX_PORTS      3
#define LT_SYSRAM_SIZE    64
#define LT_SRAM_SIZE      256
#define LT_WIDTH          64
#define LT_HEIGHT         48
#define LT_AUDIO_FRAMES   800
#define LT_PENDING_MAX    64
#define LT_PROTOCOL_MAX   64
#define LT_SEND_NONE      0x100u
#define LT_RATE_LIMIT     2147483647UL /* keeps 64-bit tick products exact */
#define LT_FRAME_LIMIT    0xFFFFFFFFUL
#define LT_NEVER          (~(uint64_t)0)
#define LT_RANGES_MAX     8
#define LT_STALL_LIMIT    1048576UL    /* no_progress: advances before giving up on a release */
#define LT_EPOCH_LOAD     1000003
#define LT_EPOCH_RESET    7919
#define LT_STATE_MAGIC    0x4C545354UL /* "LTST" */
#define LT_ASK            (RETRO_LINK_WAKE_MESSAGE | RETRO_LINK_WAKE_TOPOLOGY | RETRO_LINK_WAKE_DETACHED)

/* System RAM layout */
#define LT_RAM_HASH       0x00
#define LT_RAM_ATTACHED   0x08
#define LT_RAM_PEERS      0x09
#define LT_RAM_INDEX      0x0C
#define LT_RAM_ERRORS     0x0F
#define LT_RAM_RECEIVED   0x10
#define LT_RAM_SENT       0x14
#define LT_RAM_TOPOLOGY   0x18
#define LT_RAM_DETACHED   0x19

enum lt_advance_kind
{
   LT_FRAME_START = 0,
   LT_STEP,
   LT_FRAME_END,
   LT_STALL,        /* no_progress: the frame-start advance again */
   LT_LOWER,        /* nonconform: a promise one tick below the previous one */
   LT_PAST_PROMISE, /* nonconform: a request one step past the promise */
   LT_PAST_END      /* nonconform: a request past the frame-end tick, promised */
};

enum lt_mode
{
   LT_CONFORM = 0,
   LT_NONCONFORM,
   LT_NEVER_ADVANCE,
   LT_NO_PROGRESS
};

struct lt_range
{
   uint64_t first, last;
};

struct lt_config
{
   uint64_t rate[LT_MAX_PORTS];
   uint64_t detach[LT_MAX_PORTS], reattach[LT_MAX_PORTS]; /* frames, or LT_NEVER */
   uint64_t bad_frame;
   unsigned lookahead[LT_MAX_PORTS];
   unsigned max_ports[LT_MAX_PORTS];
   unsigned send[LT_MAX_PORTS];
   bool shared[LT_MAX_PORTS];
   char protocol[LT_MAX_PORTS][LT_PROTOCOL_MAX];
   struct lt_range sending[LT_RANGES_MAX];
   unsigned ports, steps, sending_count;
   uint32_t seed;
   enum lt_mode mode;
   bool restart_ticks;
};

struct lt_message      /* received, applied once its landing tick is passed */
{
   uint64_t landing, digest;
   unsigned from, len;
};

struct lt_port
{
   uint64_t base;         /* the port's tick at the start of this frame */
   uint64_t grant;        /* the latest grant that was a tick */
   uint64_t last_landing; /* the latest message handed out */
   unsigned last_from;
   unsigned pending_count;
   bool has_grant, has_last;
   struct lt_message pending[LT_PENDING_MAX];
};

struct lt_stand        /* where each port stands in this frame, as positions */
{
   uint64_t local[LT_MAX_PORTS];
   uint64_t promise[LT_MAX_PORTS]; /* the highest promise published */
};

struct lt_machine
{
   uint64_t frame, hash;
   uint32_t rng;
   bool sram_folded;
   unsigned char sysram[LT_SYSRAM_SIZE];
   unsigned char sram[LT_SRAM_SIZE];
   struct lt_port port[LT_MAX_PORTS];
};

static const struct retro_variable lt_variables[] = {
   { "link_test_ports",       "Ports; 1|2|3" },
   { "link_test_steps",       "Steps a frame; 16|1|2|4|8|10|32|64" },
   { "link_test_seed",        "Seed; 1|2|3|7|11|23" },
   { "link_test_rate_0",      "Port 0 ticks a second; 61440|60000" },
   { "link_test_rate_1",      "Port 1 ticks a second; 61440|60000" },
   { "link_test_rate_2",      "Port 2 ticks a second; 61440|60000" },
   { "link_test_lookahead_0", "Port 0 promise, steps ahead; 1|0|2|3|4|8|16|64" },
   { "link_test_lookahead_1", "Port 1 promise, steps ahead; 1|0|2|3|4|8|16|64" },
   { "link_test_lookahead_2", "Port 2 promise, steps ahead; 1|0|2|3|4|8|16|64" },
   { "link_test_protocol_0",  "Port 0 protocol; test-cable|test-radio" },
   { "link_test_protocol_1",  "Port 1 protocol; test-cable|test-radio" },
   { "link_test_protocol_2",  "Port 2 protocol; test-cable|test-radio" },
   { "link_test_max_ports_0", "Port 0 most ports on its bus; 2|0|3|4|255" },
   { "link_test_max_ports_1", "Port 1 most ports on its bus; 2|0|3|4|255" },
   { "link_test_max_ports_2", "Port 2 most ports on its bus; 2|0|3|4|255" },
   { "link_test_shared_0",    "Port 0 on a shared medium; off|on" },
   { "link_test_shared_1",    "Port 1 on a shared medium; off|on" },
   { "link_test_shared_2",    "Port 2 on a shared medium; off|on" },
   { "link_test_send_0",      "Port 0 sends to; broadcast|none|0|1|2|3" },
   { "link_test_send_1",      "Port 1 sends to; broadcast|none|0|1|2|3" },
   { "link_test_send_2",      "Port 2 sends to; broadcast|none|0|1|2|3" },
   { "link_test_send_frames", "Frames that may send, A-B[,A-B]...; 0-999999" },
   { "link_test_detach_0",    "Frame port 0 detaches in; -1" },
   { "link_test_detach_1",    "Frame port 1 detaches in; -1" },
   { "link_test_detach_2",    "Frame port 2 detaches in; -1" },
   { "link_test_reattach_0",  "Frame port 0 attaches again in; -1" },
   { "link_test_reattach_1",  "Frame port 1 attaches again in; -1" },
   { "link_test_reattach_2",  "Frame port 2 attaches again in; -1" },
   { "link_test_restart_ticks", "Ticks restart at a load and move at a reset; off|on" },
   { "link_test_mode",        "Mode; conform|nonconform|never_advance|no_progress" },
   { "link_test_bad_frame",   "First frame of the mode; 0" },
   { NULL, NULL }
};

static retro_environment_t lt_environ;
static retro_video_refresh_t lt_video;
static retro_audio_sample_batch_t lt_audio;
static retro_input_poll_t lt_input_poll;
static retro_input_state_t lt_input_state;
static retro_log_printf_t lt_log;

static struct lt_config lt_cfg;
static struct lt_machine lt_state;
static struct retro_link_interface lt_link;
static struct retro_link_port_info lt_port_list[LT_MAX_PORTS + 1];
static retro_link_port_t *lt_handle[LT_MAX_PORTS];
static bool lt_linked;

/* restart_ticks: each port's ticks are lt_epoch + its position; only the
 * position is saved */
static uint64_t lt_epoch;
static uint64_t lt_loads;

static uint32_t lt_picture[LT_WIDTH * LT_HEIGHT];
static int16_t lt_samples[LT_AUDIO_FRAMES * 2];
static unsigned char lt_payload[RETRO_LINK_MAX_PAYLOAD];

static uint64_t lt_mix(uint64_t x)
{
   x ^= x >> 33;
   x *= ((uint64_t)0xFF51AFD7UL << 32) | 0xED558CCDUL;
   x ^= x >> 33;
   x *= ((uint64_t)0xC4CEB9FEUL << 32) | 0x1A85EC53UL;
   x ^= x >> 33;
   return x;
}

static void lt_fold(uint64_t v)
{
   lt_state.hash = lt_mix(lt_state.hash + v + 1);
}

/* FNV-1a 64 */
static uint64_t lt_digest(const void *data, size_t len)
{
   const unsigned char *s = (const unsigned char*)data;
   uint64_t h             = ((uint64_t)0xCBF29CE4UL << 32) | 0x84222325UL;
   size_t i;
   for (i = 0; i < len; i++)
   {
      h ^= s[i];
      h *= ((uint64_t)0x100UL << 32) | 0x1B3UL;
   }
   return h;
}

static uint32_t lt_next(void)
{
   uint32_t x = lt_state.rng;
   x ^= (uint32_t)(x << 13);
   x ^= x >> 17;
   x ^= (uint32_t)(x << 5);
   lt_state.rng = x;
   return x;
}

static void lt_bump8(unsigned offset)
{
   if (lt_state.sysram[offset] < 0xFF)
      lt_state.sysram[offset]++;
}

static void lt_bump32(unsigned offset)
{
   unsigned char *s = &lt_state.sysram[offset];
   uint32_t v       = (uint32_t)s[0] | ((uint32_t)s[1] << 8)
      | ((uint32_t)s[2] << 16) | ((uint32_t)s[3] << 24);
   v++;
   s[0] = (unsigned char)v;
   s[1] = (unsigned char)(v >> 8);
   s[2] = (unsigned char)(v >> 16);
   s[3] = (unsigned char)(v >> 24);
}

static void lt_fail(const char *what, unsigned port, uint64_t a, uint64_t b)
{
   lt_bump8(LT_RAM_ERRORS);
   if (lt_log)
      lt_log(RETRO_LOG_ERROR,
            "link test: frame %lu port %u: %s (%08lx%08lx, %08lx%08lx)\n",
            (unsigned long)lt_state.frame, port, what,
            (unsigned long)(a >> 32), (unsigned long)(a & 0xFFFFFFFFUL),
            (unsigned long)(b >> 32), (unsigned long)(b & 0xFFFFFFFFUL));
}

static void lt_fail_option(const char *key, const char *value)
{
   lt_bump8(LT_RAM_ERRORS);
   if (lt_log)
      lt_log(RETRO_LOG_ERROR, "link test: %s: unknown value \"%s\"\n", key, value);
}

/* Decimal digits with no sign, at most limit: the text after them, or NULL */
static const char *lt_digits(const char *s, uint64_t limit, uint64_t *out)
{
   const char *first = s;
   uint64_t v        = 0;
   for (; *s >= '0' && *s <= '9'; s++)
   {
      unsigned d = (unsigned)(*s - '0');
      if (d > limit || v > (limit - d) / 10)
         return NULL;
      v = v * 10 + d;
   }
   if (s == first)
      return NULL;
   *out = v;
   return s;
}

/* A decimal number with no sign, at most limit */
static bool lt_number(const char *s, uint64_t limit, uint64_t *out)
{
   return s && (s = lt_digits(s, limit, out)) && !*s;
}

/* A-B[,A-B]... */
static bool lt_ranges(const char *s)
{
   struct lt_range r[LT_RANGES_MAX];
   unsigned n = 0;
   for (;;)
   {
      if (     n == LT_RANGES_MAX
            || !(s = lt_digits(s, LT_FRAME_LIMIT, &r[n].first)) || *s++ != '-'
            || !(s = lt_digits(s, LT_FRAME_LIMIT, &r[n].last)) || r[n].first > r[n].last)
         return false;
      n++;
      if (!*s)
         break;
      if (*s++ != ',')
         return false;
   }
   memcpy(lt_cfg.sending, r, n * sizeof(*r));
   lt_cfg.sending_count = n;
   return true;
}

static const char *lt_option(const char *key)
{
   struct retro_variable var;
   var.key   = key;
   var.value = NULL;
   if (!lt_environ || !lt_environ(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
      return NULL;
   return var.value;
}

static void lt_option_number(const char *key, uint64_t lo, uint64_t hi, unsigned *out)
{
   const char *s = lt_option(key);
   uint64_t v;
   if (!s)
      return;
   if (lt_number(s, hi, &v) && v >= lo)
      *out = (unsigned)v;
   else
      lt_fail_option(key, s);
}

/* A frame, or -1 for none */
static void lt_option_frame(const char *key, uint64_t *out)
{
   const char *s = lt_option(key);
   uint64_t v;
   if (!s)
      return;
   if (!strcmp(s, "-1"))
      *out = LT_NEVER;
   else if (lt_number(s, LT_FRAME_LIMIT, &v))
      *out = v;
   else
      lt_fail_option(key, s);
}

static void lt_read_port(unsigned p)
{
   char key[32];
   const char *s;
   uint64_t v;

   sprintf(key, "link_test_rate_%u", p);
   if ((s = lt_option(key)))
   {
      if (lt_number(s, LT_RATE_LIMIT, &v) && v && v % 60 == 0)
         lt_cfg.rate[p] = v;
      else
         lt_fail_option(key, s);
   }
   sprintf(key, "link_test_lookahead_%u", p);
   lt_option_number(key, 0, 64, &lt_cfg.lookahead[p]);
   sprintf(key, "link_test_max_ports_%u", p);
   lt_option_number(key, 0, 255, &lt_cfg.max_ports[p]);
   sprintf(key, "link_test_protocol_%u", p);
   if ((s = lt_option(key)))
   {
      if (*s && strlen(s) < LT_PROTOCOL_MAX)
         strcpy(lt_cfg.protocol[p], s);
      else
         lt_fail_option(key, s);
   }
   sprintf(key, "link_test_shared_%u", p);
   if ((s = lt_option(key)))
   {
      if (!strcmp(s, "on") || !strcmp(s, "off"))
         lt_cfg.shared[p] = !strcmp(s, "on");
      else
         lt_fail_option(key, s);
   }
   sprintf(key, "link_test_send_%u", p);
   if ((s = lt_option(key)))
   {
      if (!strcmp(s, "none"))
         lt_cfg.send[p] = LT_SEND_NONE;
      else if (!strcmp(s, "broadcast"))
         lt_cfg.send[p] = RETRO_LINK_BROADCAST;
      else if (lt_number(s, 254, &v))
         lt_cfg.send[p] = (unsigned)v;
      else
         lt_fail_option(key, s);
   }
   sprintf(key, "link_test_detach_%u", p);
   lt_option_frame(key, &lt_cfg.detach[p]);
   sprintf(key, "link_test_reattach_%u", p);
   lt_option_frame(key, &lt_cfg.reattach[p]);
}

static void lt_read_mode(void)
{
   static const char *const names[] = { "conform", "nonconform", "never_advance", "no_progress" };
   const char *s = lt_option("link_test_mode");
   unsigned i;
   uint64_t v;
   if (s)
   {
      for (i = 0; i < sizeof(names) / sizeof(*names); i++)
         if (!strcmp(s, names[i]))
            break;
      if (i < sizeof(names) / sizeof(*names))
         lt_cfg.mode = (enum lt_mode)i;
      else
         lt_fail_option("link_test_mode", s);
   }
   if ((s = lt_option("link_test_restart_ticks")))
   {
      if (!strcmp(s, "on") || !strcmp(s, "off"))
         lt_cfg.restart_ticks = !strcmp(s, "on");
      else
         lt_fail_option("link_test_restart_ticks", s);
   }
   if ((s = lt_option("link_test_send_frames")) && !lt_ranges(s))
      lt_fail_option("link_test_send_frames", s);
   /* nonconform asks past a frame-end tick, which the bus knows from frame 1 */
   if ((s = lt_option("link_test_bad_frame")))
   {
      if (lt_number(s, LT_FRAME_LIMIT, &v) && (v || lt_cfg.mode != LT_NONCONFORM))
         lt_cfg.bad_frame = v;
      else
         lt_fail_option("link_test_bad_frame", s);
   }
}

static void lt_read_options(void)
{
   unsigned p;
   const char *s;
   uint64_t v;

   memset(&lt_cfg, 0, sizeof(lt_cfg));
   lt_cfg.ports = 1;
   lt_cfg.steps = 16;
   lt_cfg.seed  = 1;
   lt_cfg.sending[0].last = 999999;
   lt_cfg.sending_count   = 1;
   for (p = 0; p < LT_MAX_PORTS; p++)
   {
      lt_cfg.rate[p]      = 61440;
      lt_cfg.lookahead[p] = 1;
      lt_cfg.max_ports[p] = 2;
      lt_cfg.send[p]      = RETRO_LINK_BROADCAST;
      lt_cfg.detach[p]    = LT_NEVER;
      lt_cfg.reattach[p]  = LT_NEVER;
      strcpy(lt_cfg.protocol[p], "test-cable");
   }
   lt_option_number("link_test_ports", 1, LT_MAX_PORTS, &lt_cfg.ports);
   lt_option_number("link_test_steps", 1, 64, &lt_cfg.steps);
   if ((s = lt_option("link_test_seed")))
   {
      if (lt_number(s, 0xFFFFFFFFUL, &v))
         lt_cfg.seed = (uint32_t)v;
      else
         lt_fail_option("link_test_seed", s);
   }
   for (p = 0; p < LT_MAX_PORTS; p++)
      lt_read_port(p);
   lt_read_mode();
}

static uint64_t lt_tick(unsigned p, unsigned m)
{
   return lt_state.port[p].base + (uint64_t)m * (lt_cfg.rate[p] / 60) / lt_cfg.steps;
}

/* A position as the bus sees it, and back */
static uint64_t lt_out(uint64_t position)
{
   return position + lt_epoch;
}

static uint64_t lt_in(uint64_t tick)
{
   if (tick == RETRO_LINK_UNBOUNDED)
      return tick;
   return tick < lt_epoch ? 0 : tick - lt_epoch;
}

static bool lt_sending(void)
{
   unsigned i;
   for (i = 0; i < lt_cfg.sending_count; i++)
      if (lt_state.frame >= lt_cfg.sending[i].first && lt_state.frame <= lt_cfg.sending[i].last)
         return true;
   return false;
}

static void lt_peers(unsigned p)
{
   unsigned count = 0;
   int index      = lt_link.peers(lt_handle[p], &count);
   lt_state.sysram[LT_RAM_PEERS + p] = (unsigned char)(index < 0 ? 0 : (count > 0xFF ? 0xFF : count));
   lt_state.sysram[LT_RAM_INDEX + p] = (unsigned char)(index < 0 ? 0xFF : index);
}

static bool lt_joined(unsigned p)
{
   return lt_state.sysram[LT_RAM_INDEX + p] != 0xFF;
}

/* Caps v of each joined entry at the earliest promise among the joined
 * entries, taken onto its own clock rounding down, as the bus does */
static void lt_cap_earliest(const uint64_t *safe, const unsigned *port, unsigned n, uint64_t *v)
{
   unsigned i, early = n;
   uint64_t early_ahead = 0;
   for (i = 0; i < n; i++)
   {
      uint64_t ahead = safe[i] - lt_state.port[port[i]].base;
      if (!lt_joined(port[i]))
         continue;
      if (early == n || ahead * lt_cfg.rate[port[early]] < early_ahead * lt_cfg.rate[port[i]])
      {
         early       = i;
         early_ahead = ahead;
      }
   }
   if (early == n)
      return;
   for (i = 0; i < n; i++)
   {
      uint64_t cap = lt_state.port[port[i]].base
         + early_ahead * lt_cfg.rate[port[i]] / lt_cfg.rate[port[early]];
      if (lt_joined(port[i]) && v[i] > cap)
         v[i] = cap;
   }
}

/* lawful: the lowest grant the request allows once the bus has repaired it */
static void lt_granted(unsigned p, uint64_t grant, uint64_t lawful, unsigned wake)
{
   struct lt_port *k = &lt_state.port[p];
   if (grant != RETRO_LINK_UNBOUNDED)
   {
      if (k->has_grant && grant < k->grant)
         lt_fail("grant below the previous grant", p, grant, k->grant);
      if (grant < lawful)
         lt_fail("grant short of the request", p, grant, lawful);
      k->grant     = grant;
      k->has_grant = true;
   }
   if (wake & RETRO_LINK_WAKE_TOPOLOGY)
   {
      lt_bump8(LT_RAM_TOPOLOGY);
      lt_peers(p);
   }
   if (wake & RETRO_LINK_WAKE_DETACHED)
      lt_bump8(LT_RAM_DETACHED);
}

/* Takes every message that can be handed out, checking each against the
 * peek before it and the grant and tick of the advance that made it so */
static void lt_receive(unsigned p, uint64_t local, uint64_t grant)
{
   struct lt_port *k = &lt_state.port[p];
   for (;;)
   {
      uint64_t peek_tick = 0, tick = 0, peek_digest = 0;
      unsigned peek_from = 0, from = 0;
      size_t peek_len    = 0, len = sizeof(lt_payload);
      const void *peeked = lt_link.peek(lt_handle[p], &peek_tick, &peek_from, &peek_len);
      bool took;
      if (peeked)
         peek_digest = lt_digest(peeked, peek_len);
      took = lt_link.recv_msg(lt_handle[p], &tick, &from, lt_payload, &len);
      if (!peeked != !took)
         lt_fail("peek and recv disagree", p, peek_tick, tick);
      if (!took)
         break;
      if (peeked && (peek_tick != tick || peek_from != from || peek_len != len
               || peek_digest != lt_digest(lt_payload, len)))
         lt_fail("peek and recv disagree", p, peek_tick, tick);
      tick = lt_in(tick);
      lt_bump32(LT_RAM_RECEIVED);
      if (grant != RETRO_LINK_UNBOUNDED && tick >= grant)
         lt_fail("message handed out at or above the grant", p, tick, grant);
      if (tick < local)
         lt_fail("message handed out below the port's tick", p, tick, local);
      if (k->has_last && (tick < k->last_landing
               || (tick == k->last_landing && from < k->last_from)))
         lt_fail("message out of order", p, tick, k->last_landing);
      k->last_landing = tick;
      k->last_from    = from;
      k->has_last     = true;
      if (k->pending_count == LT_PENDING_MAX)
      {
         lt_fail("too many messages waiting", p, tick, local);
         continue;
      }
      k->pending[k->pending_count].landing = tick;
      k->pending[k->pending_count].digest  = lt_digest(lt_payload, len);
      k->pending[k->pending_count].from    = from;
      k->pending[k->pending_count].len     = (unsigned)len;
      k->pending_count++;
   }
}

/* Applies, in landing order, the messages landing before tick */
static void lt_apply(unsigned p, uint64_t tick)
{
   struct lt_port *k = &lt_state.port[p];
   unsigned done     = 0;
   while (done < k->pending_count && k->pending[done].landing < tick)
   {
      const struct lt_message *e = &k->pending[done++];
      lt_fold(p);
      lt_fold(e->landing);
      lt_fold(e->from);
      lt_fold(e->len);
      lt_fold(e->digest);
   }
   if (!done)
      return;
   k->pending_count -= done;
   memmove(k->pending, k->pending + done, k->pending_count * sizeof(*k->pending));
}

/* One advance of every attached port, as kind says; at says where each
 * port stands. A step ends at its end tick or at the grant, if that is
 * lower, so the core never runs past a grant. True when every entry came
 * back RETRO_LINK_UNBOUNDED with RETRO_LINK_WAKE_DETACHED, as after a
 * release. */
static bool lt_advance(enum lt_advance_kind kind, unsigned m, struct lt_stand *at)
{
   struct retro_link_wait w[LT_MAX_PORTS];
   uint64_t local[LT_MAX_PORTS], safe[LT_MAX_PORTS], request[LT_MAX_PORTS];
   uint64_t lawful[LT_MAX_PORTS]; /* the request as the bus repairs it */
   unsigned port[LT_MAX_PORTS];
   unsigned i, n = 0;
   bool released  = true;
   for (i = 0; i < lt_cfg.ports; i++)
   {
      uint64_t end = lt_tick(i, lt_cfg.steps);
      if (!lt_handle[i])
         continue;
      local[n]   = at->local[i];
      safe[n]    = at->promise[i];
      request[n] = local[n];
      switch (kind)
      {
         case LT_FRAME_START:
            safe[n] = lt_tick(i, lt_cfg.lookahead[i]);
            break;
         case LT_STEP:
            safe[n]    = lt_tick(i, m + lt_cfg.lookahead[i]);
            request[n] = lt_tick(i, m + 1) < safe[n] ? lt_tick(i, m + 1) : safe[n];
            break;
         case LT_FRAME_END:
            local[n]   = end;
            safe[n]    = lt_tick(i, lt_cfg.steps + lt_cfg.lookahead[i]);
            request[n] = end;
            break;
         case LT_LOWER:
            safe[n]--;
            break;
         case LT_PAST_PROMISE:
            request[n] = safe[n] + lt_tick(i, 1) - lt_tick(i, 0);
            break;
         case LT_PAST_END:
            request[n] = end + 1;
            if (safe[n] < request[n])
               safe[n] = request[n];
            break;
         default:
            break;
      }
      lawful[n] = request[n] < end ? request[n] : end;
      if (lawful[n] > safe[n] && lawful[n] > at->promise[i])
         lawful[n] = safe[n] > at->promise[i] ? safe[n] : at->promise[i];
      port[n++] = i;
   }
   if (!n)
      return true;
   if (n > 1)
   {
      lt_cap_earliest(safe, port, n, lawful);
      if (kind == LT_STEP)
         lt_cap_earliest(safe, port, n, request);
   }
   for (i = 0; i < n; i++)
   {
      w[i].handle       = lt_handle[port[i]];
      w[i].local_tick   = lt_out(local[i]);
      w[i].safe_tick    = lt_out(safe[i]);
      w[i].request_tick = lt_out(request[i]);
      w[i].grant        = 0;
      w[i].wake_flags   = LT_ASK;
   }
   if (n == 1)
      w[0].grant = lt_link.advance(w[0].handle, w[0].local_tick, w[0].safe_tick,
            w[0].request_tick, &w[0].wake_flags);
   else
      lt_link.advance_multi(w, n);
   for (i = 0; i < n; i++)
   {
      unsigned p     = port[i];
      uint64_t grant = lt_in(w[i].grant);
      lt_granted(p, grant, lawful[i], w[i].wake_flags);
      lt_receive(p, local[i], grant);
      if (safe[i] > at->promise[p])
         at->promise[p] = safe[i];
      if (kind == LT_STEP)
         at->local[p] = grant < lt_tick(p, m + 1) ? grant : lt_tick(p, m + 1);
      if (w[i].grant != RETRO_LINK_UNBOUNDED || !(w[i].wake_flags & RETRO_LINK_WAKE_DETACHED))
         released = false;
   }
   return released;
}

/* A payload of 8 to 64 bytes from the state, stamped at a position */
static void lt_send(unsigned p, uint64_t stamp, uint32_t r)
{
   unsigned len = 8 + r % 57;
   uint64_t v   = lt_state.hash ^ r;
   unsigned i;
   if (lt_cfg.send[p] == LT_SEND_NONE || !lt_sending())
      return;
   for (i = 0; i < len; i++)
   {
      if (!(i & 7))
         v = lt_mix(v + i);
      lt_payload[i] = (unsigned char)(v >> ((i & 7) * 8));
   }
   if (lt_link.send_msg(lt_handle[p], lt_out(stamp), lt_cfg.send[p], lt_payload, len))
   {
      lt_bump32(LT_RAM_SENT);
      lt_fold(lt_digest(lt_payload, len));
   }
}

static void lt_attach(unsigned p)
{
   if (!(lt_handle[p] = lt_link.attach(p, lt_cfg.protocol[p], lt_cfg.rate[p])))
      return;
   lt_state.sysram[LT_RAM_ATTACHED]++;
   lt_peers(p);
}

/* The detaches and re-attaches the options set for this frame */
static void lt_membership(void)
{
   unsigned p;
   for (p = 0; p < lt_cfg.ports && lt_linked; p++)
   {
      if (lt_handle[p] && lt_state.frame == lt_cfg.detach[p])
      {
         lt_link.detach(lt_handle[p]);
         lt_handle[p] = NULL;
         lt_state.sysram[LT_RAM_ATTACHED]--;
         lt_peers(p);
      }
      if (!lt_handle[p] && lt_state.frame == lt_cfg.reattach[p])
         lt_attach(p);
   }
}

/* nonconform, after the frame-start advance: three breaches, each in a
 * call of its own */
static void lt_nonconform(struct lt_stand *at)
{
   unsigned p;
   lt_advance(LT_LOWER, 0, at);
   lt_advance(LT_PAST_PROMISE, 0, at);
   for (p = 0; p < lt_cfg.ports; p++)
      if (lt_handle[p])
         lt_send(p, at->promise[p] - 1, lt_next());
}

/* no_progress: the frame-start advance again and again, until a release */
static void lt_stall(struct lt_stand *at)
{
   unsigned long calls;
   for (calls = 0; calls < LT_STALL_LIMIT; calls++)
      if (lt_advance(LT_STALL, 0, at))
         return;
   lt_fail("no release after advances without progress", 0, calls, 0);
}

static void lt_store_hash(void)
{
   unsigned i;
   for (i = 0; i < 8; i++)
      lt_state.sysram[LT_RAM_HASH + i] = (unsigned char)(lt_state.hash >> (i * 8));
}

static void lt_draw(void)
{
   unsigned x, y;
   for (y = 0; y < LT_HEIGHT; y++)
   {
      uint64_t row = lt_mix(lt_state.hash + (y >> 3));
      for (x = 0; x < LT_WIDTH; x++)
         lt_picture[y * LT_WIDTH + x] = (uint32_t)((x >> 3) & 1
               ? (row & 0xFFFFFFUL) : ((row >> 24) & 0xFFFFFFUL));
   }
   lt_video(lt_picture, LT_WIDTH, LT_HEIGHT, LT_WIDTH * sizeof(*lt_picture));
}

static void lt_sound(void)
{
   unsigned i;
   for (i = 0; i < LT_AUDIO_FRAMES * 2; i++)
      lt_samples[i] = (int16_t)((int)((lt_mix(lt_state.hash + i) >> 52) & 0x0FFF) - 0x800);
   lt_audio(lt_samples, LT_AUDIO_FRAMES);
}

void retro_set_environment(retro_environment_t cb)
{
   struct retro_log_callback log;
   bool no_game = true;
   lt_environ   = cb;
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)lt_variables);
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
   log.log = NULL;
   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      lt_log = log.log;
}

void retro_set_video_refresh(retro_video_refresh_t cb) { lt_video = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { lt_audio = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { lt_input_poll = cb; }
void retro_set_input_state(retro_input_state_t cb) { lt_input_state = cb; }

void retro_init(void)
{
   unsigned p;
   memset(&lt_state, 0, sizeof(lt_state));
   memset(lt_handle, 0, sizeof(lt_handle));
   lt_epoch = 0;
   lt_loads = 0;
   lt_read_options();
   lt_state.rng = lt_cfg.seed ? lt_cfg.seed : 2463534242UL;
   memset(lt_port_list, 0, sizeof(lt_port_list));
   for (p = 0; p < lt_cfg.ports; p++)
   {
      lt_port_list[p].port        = p;
      lt_port_list[p].protocol_id = lt_cfg.protocol[p];
      lt_port_list[p].desc        = "Test port";
      lt_port_list[p].clock_rate  = lt_cfg.rate[p];
      lt_port_list[p].max_ports   = lt_cfg.max_ports[p];
      lt_port_list[p].flags       = lt_cfg.shared[p] ? RETRO_LINK_PORT_SHARED_MEDIUM : 0;
   }
   memset(&lt_link, 0, sizeof(lt_link));
   lt_link.interface_version = RETRO_LINK_INTERFACE_VERSION;
   lt_link.ports             = lt_port_list;
   lt_linked = lt_environ(RETRO_ENVIRONMENT_GET_LINK_INTERFACE, &lt_link);
}

void retro_deinit(void)
{
   lt_linked = false;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "link test";
   info->library_version  = "1";
   info->valid_extensions = "";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->geometry.base_width   = LT_WIDTH;
   info->geometry.base_height  = LT_HEIGHT;
   info->geometry.max_width    = LT_WIDTH;
   info->geometry.max_height   = LT_HEIGHT;
   info->geometry.aspect_ratio = 4.0f / 3.0f;
   info->timing.fps            = 60.0;
   info->timing.sample_rate    = 48000.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_reset(void)
{
   if (lt_cfg.restart_ticks)
      lt_epoch += LT_EPOCH_RESET;
}

void retro_run(void)
{
   struct lt_stand at;
   unsigned m, p;
   bool bad = lt_cfg.mode != LT_CONFORM && lt_state.frame >= lt_cfg.bad_frame;
   if (!lt_state.sram_folded)
   {
      lt_fold(lt_digest(lt_state.sram, sizeof(lt_state.sram)));
      lt_state.sram_folded = true;
   }
   lt_input_poll();
   lt_fold(lt_state.frame);
   lt_fold((uint16_t)lt_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK));

   memset(&at, 0, sizeof(at));
   for (p = 0; p < lt_cfg.ports; p++)
      at.local[p] = lt_tick(p, 0);
   if (!bad || lt_cfg.mode != LT_NEVER_ADVANCE)
   {
      lt_advance(LT_FRAME_START, 0, &at);
      lt_membership();
      if (bad && lt_cfg.mode == LT_NO_PROGRESS && lt_state.frame == lt_cfg.bad_frame)
         lt_stall(&at);
      if (bad && lt_cfg.mode == LT_NONCONFORM)
         lt_nonconform(&at);
      for (m = 0; m < lt_cfg.steps; m++)
      {
         lt_advance(LT_STEP, m, &at);
         for (p = 0; p < lt_cfg.ports; p++)
         {
            uint32_t r = lt_next();
            if (!lt_handle[p])
               continue;
            lt_apply(p, at.local[p]);
            if (!(r & 3))
               lt_send(p, lt_tick(p, m + lt_cfg.lookahead[p]), r);
         }
      }
      if (bad && lt_cfg.mode == LT_NONCONFORM)
         lt_advance(LT_PAST_END, lt_cfg.steps, &at);
      /* nonconform: no frame-end publish in its fourth frame */
      if (!bad || lt_cfg.mode != LT_NONCONFORM || lt_state.frame != lt_cfg.bad_frame + 3)
         lt_advance(LT_FRAME_END, lt_cfg.steps, &at);
   }

   for (p = 0; p < lt_cfg.ports; p++)
      lt_state.port[p].base = lt_tick(p, lt_cfg.steps);
   lt_state.frame++;
   lt_store_hash();
   lt_draw();
   lt_sound();
}

/* Save state: little-endian, one fixed size; ticks as positions */
#define LT_PORT_STATE_SIZE  (8 + 8 + 8 + 4 + 4 + 1 + 1 + 1 + LT_PENDING_MAX * (8 + 8 + 4 + 4))
#define LT_STATE_SIZE       (4 + 8 + 8 + 4 + 1 + LT_SYSRAM_SIZE + LT_SRAM_SIZE \
                             + LT_MAX_PORTS * LT_PORT_STATE_SIZE)

static unsigned char *lt_put(unsigned char *s, uint64_t v, unsigned bytes)
{
   unsigned i;
   for (i = 0; i < bytes; i++)
      *s++ = (unsigned char)(v >> (i * 8));
   return s;
}

static const unsigned char *lt_get(const unsigned char *s, uint64_t *v, unsigned bytes)
{
   unsigned i;
   *v = 0;
   for (i = 0; i < bytes; i++)
      *v |= (uint64_t)*s++ << (i * 8);
   return s;
}

size_t retro_serialize_size(void)
{
   return LT_STATE_SIZE;
}

bool retro_serialize(void *data, size_t size)
{
   unsigned char *s = (unsigned char*)data;
   unsigned p, i;
   if (size < LT_STATE_SIZE)
      return false;
   s = lt_put(s, LT_STATE_MAGIC, 4);
   s = lt_put(s, lt_state.frame, 8);
   s = lt_put(s, lt_state.hash, 8);
   s = lt_put(s, lt_state.rng, 4);
   s = lt_put(s, lt_state.sram_folded, 1);
   memcpy(s, lt_state.sysram, LT_SYSRAM_SIZE);
   s += LT_SYSRAM_SIZE;
   memcpy(s, lt_state.sram, LT_SRAM_SIZE);
   s += LT_SRAM_SIZE;
   for (p = 0; p < LT_MAX_PORTS; p++)
   {
      const struct lt_port *k = &lt_state.port[p];
      s = lt_put(s, k->base, 8);
      s = lt_put(s, k->grant, 8);
      s = lt_put(s, k->last_landing, 8);
      s = lt_put(s, k->last_from, 4);
      s = lt_put(s, k->pending_count, 4);
      s = lt_put(s, k->has_grant, 1);
      s = lt_put(s, k->has_last, 1);
      s = lt_put(s, lt_handle[p] != NULL, 1);
      for (i = 0; i < LT_PENDING_MAX; i++)
      {
         const struct lt_message *e = &k->pending[i];
         bool live                  = i < k->pending_count;
         s = lt_put(s, live ? e->landing : 0, 8);
         s = lt_put(s, live ? e->digest : 0, 8);
         s = lt_put(s, live ? e->from : 0, 4);
         s = lt_put(s, live ? e->len : 0, 4);
      }
   }
   return true;
}

bool retro_unserialize(const void *data, size_t size)
{
   const unsigned char *s = (const unsigned char*)data;
   struct lt_machine *loaded;
   bool attached[LT_MAX_PORTS];
   unsigned p, i;
   uint64_t v;
   if (size < LT_STATE_SIZE)
      return false;
   s = lt_get(s, &v, 4);
   if (v != LT_STATE_MAGIC || !(loaded = (struct lt_machine*)calloc(1, sizeof(*loaded))))
      return false;
   s = lt_get(s, &loaded->frame, 8);
   s = lt_get(s, &loaded->hash, 8);
   s = lt_get(s, &v, 4);
   loaded->rng = (uint32_t)v;
   s = lt_get(s, &v, 1);
   loaded->sram_folded = v != 0;
   memcpy(loaded->sysram, s, LT_SYSRAM_SIZE);
   s += LT_SYSRAM_SIZE;
   memcpy(loaded->sram, s, LT_SRAM_SIZE);
   s += LT_SRAM_SIZE;
   for (p = 0; p < LT_MAX_PORTS; p++)
   {
      struct lt_port *k = &loaded->port[p];
      s = lt_get(s, &k->base, 8);
      s = lt_get(s, &k->grant, 8);
      s = lt_get(s, &k->last_landing, 8);
      s = lt_get(s, &v, 4);
      k->last_from = (unsigned)v;
      s = lt_get(s, &v, 4);
      k->pending_count = (unsigned)v;
      s = lt_get(s, &v, 1);
      k->has_grant = v != 0;
      s = lt_get(s, &v, 1);
      k->has_last = v != 0;
      s = lt_get(s, &v, 1);
      attached[p] = v != 0;
      for (i = 0; i < LT_PENDING_MAX; i++)
      {
         struct lt_message *e = &k->pending[i];
         s = lt_get(s, &e->landing, 8);
         s = lt_get(s, &e->digest, 8);
         s = lt_get(s, &v, 4);
         e->from = (unsigned)v;
         s = lt_get(s, &v, 4);
         e->len = (unsigned)v;
      }
      if (k->pending_count > LT_PENDING_MAX)
      {
         free(loaded);
         return false;
      }
   }
   lt_state = *loaded;
   free(loaded);
   /* A port attached in one of the states and not in the other */
   for (p = 0; p < lt_cfg.ports && lt_linked; p++)
   {
      if (lt_handle[p] && !attached[p])
      {
         lt_link.detach(lt_handle[p]);
         lt_handle[p] = NULL;
      }
      else if (!lt_handle[p] && attached[p])
         lt_handle[p] = lt_link.attach(p, lt_cfg.protocol[p], lt_cfg.rate[p]);
   }
   lt_loads++;
   if (lt_cfg.restart_ticks)
      lt_epoch = lt_loads * LT_EPOCH_LOAD;
   return true;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
   unsigned p;
   (void)game;
   if (!lt_environ(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format))
      return false;
   for (p = 0; p < LT_MAX_PORTS; p++)
      lt_state.sysram[LT_RAM_INDEX + p] = 0xFF;
   if (!lt_linked)
      return true;
   for (p = 0; p < lt_cfg.ports; p++)
      lt_attach(p);
   return true;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

void retro_unload_game(void)
{
   unsigned p;
   for (p = 0; p < LT_MAX_PORTS; p++)
   {
      if (lt_handle[p])
         lt_link.detach(lt_handle[p]);
      lt_handle[p] = NULL;
   }
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

void *retro_get_memory_data(unsigned id)
{
   if (id == RETRO_MEMORY_SAVE_RAM)
      return lt_state.sram;
   if (id == RETRO_MEMORY_SYSTEM_RAM)
      return lt_state.sysram;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   if (id == RETRO_MEMORY_SAVE_RAM)
      return LT_SRAM_SIZE;
   if (id == RETRO_MEMORY_SYSTEM_RAM)
      return LT_SYSRAM_SIZE;
   return 0;
}
