/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (link_bus.c).
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

#include <stdlib.h>
#include <string.h>

#include <link/link_bus.h>

/* The largest tick: RETRO_LINK_UNBOUNDED is never a cabled port's grant */
#define LINK_BUS_TICK_MAX (RETRO_LINK_UNBOUNDED - 1)

/* Message records, queue entries and payload cells come from free lists that
 * 64 KiB blocks fill; no block is freed before link_bus_free. */
#define LINK_BUS_BLOCK      65536
#define LINK_BUS_BLOCK_HEAD 64 /* the chain of blocks; keeps cells aligned */
#define LINK_BUS_CELLS      7  /* payload cells of 64, 128, ... 4096 bytes */
#define LINK_BUS_POOL_MSG   LINK_BUS_CELLS
#define LINK_BUS_POOL_ENTRY (LINK_BUS_CELLS + 1)
#define LINK_BUS_POOLS      (LINK_BUS_CELLS + 2)

/* The save blob: little-endian, every tick but an anchor taken relative to
 * its port's anchor and every instant as it is, so the hash leaves out only
 * anchors and counts */
#define LINK_BUS_MAGIC         "LBUS"
#define LINK_BUS_VERSION       2
#define LINK_BUS_SAVED_COUNTS  8
#define LINK_BUS_REC_ATTACHED  1
#define LINK_BUS_REC_EDGE      2
#define LINK_BUS_REC_PROMISE   4
#define LINK_BUS_REC_SPAN      8
#define LINK_BUS_REC_SENT      16
#define LINK_BUS_REC_RENUMBER  32
#define LINK_BUS_REC_SHOWN     64
#define LINK_BUS_REC_UNPLACED  128
#define LINK_BUS_REC_VIEWED    256
#define LINK_BUS_REC_CUT       512
#define LINK_BUS_REC_JOIN      1024
#define LINK_BUS_REC_BITS      2047
/* A port's link to another port: what it sees of it and when the two hear
 * each other */
#define LINK_BUS_LINK_TOGETHER 1   /* on one bus now */
#define LINK_BUS_LINK_VIEW     2   /* in this port's view */
#define LINK_BUS_LINK_WAS      4   /* in its view before view_at */
#define LINK_BUS_LINK_JOIN     8   /* they carry and bound from join */
#define LINK_BUS_LINK_CUT      16  /* they carry and bound before cut */
#define LINK_BUS_LINK_GAP      32  /* joined again after the cut: before cut or from join */
#define LINK_BUS_LINK_MINE     64  /* join waits for this port's console's first edge */
#define LINK_BUS_LINK_THEIRS   128 /* join waits for the other port's console's */
#define LINK_BUS_LINK_HOLD     256 /* a hold's end: join bounds, drops nothing */
#define LINK_BUS_LINK_FLIPPED  512 /* off and on again since its view last changed */
#define LINK_BUS_LINK_BITS     1023
#define LINK_BUS_FNV_BASIS     ((((uint64_t)0xCBF29CE4UL) << 32) | 0x84222325UL)
#define LINK_BUS_FNV_PRIME     ((((uint64_t)0x100UL) << 32) | 0x1B3UL)

enum link_bus_repair
{
   LINK_BUS_AS_REQUEST = 0,
   LINK_BUS_AS_PROMISE,
   LINK_BUS_AS_FRAME_END,
   LINK_BUS_AS_EARLIEST
};

enum link_bus_landing
{
   LINK_BUS_LANDED = 0, /* landing is on the receiver's clock */
   LINK_BUS_AHEAD,      /* the receiver's anchor is not known yet */
   LINK_BUS_REJOIN,     /* for a held console: lands at its re-join anchor */
   LINK_BUS_JOINING,    /* from a pair whose join instant is not known yet */
   LINK_BUS_PLACING     /* its sender waits to be placed: instant and sent_at
                         * hold its stamp and local ticks */
};

enum link_bus_limit   /* what a port's limit holds, in the blob */
{
   LINK_BUS_LIMIT_NONE = 0, /* nothing receivable: no advance since a re-join */
   LINK_BUS_LIMIT_UNBOUNDED,
   LINK_BUS_LIMIT_GRANT
};

enum link_bus_pass    /* the passes link_bus_load reads the blob in */
{
   LINK_BUS_CHECK = 0,  /* tests it and counts the pool items it needs */
   LINK_BUS_MAKE,       /* makes the records, strings and cable room it needs */
   LINK_BUS_APPLY       /* sets the bus to it; can no longer fail */
};

struct link_bus_item
{
   struct link_bus_item *next;
};

struct link_bus_pool
{
   struct link_bus_item *free;
   size_t size, count;
};

struct link_bus_msg   /* stored once for all its receivers */
{
   const struct retro_link_port *from;
   unsigned char *cell;
   size_t len;
   unsigned refs, cell_pool;
};

struct link_bus_entry /* one receiver's copy of a message */
{
   struct link_bus_entry *next;
   struct link_bus_msg *msg;
   uint64_t landing;
   uint64_t instant, rate; /* LINK_BUS_AHEAD: its stamp's instant, as ticks of rate */
   uint64_t sent_at, sent_rate; /* the instant its sender stood at, for from_index */
   uint64_t seq;         /* send order: link_bus::sends at send, or its loaded rank */
   uint64_t journal_gen;
   size_t journal;       /* its journal entry while journal_gen is the bus's */
   unsigned from_index;
   unsigned char state;
   bool seen;            /* receivable at its port's last grant */
};

struct link_bus_link
{
   uint64_t join, join_rate; /* J, as ticks of join_rate */
   uint64_t cut, cut_rate;   /* t, as ticks of cut_rate */
   unsigned flags;
};

struct retro_link_port
{
   link_bus_t *owner;
   struct link_bus_link *links; /* to every port, by that port's slot */
   char *protocol;
   char *loaded;         /* link_bus_load's new protocol, from MAKE to APPLY */
   struct retro_link_port *head;  /* first member of its bus; NULL when cabled to nothing */
   struct retro_link_port *next;  /* next member, by index */
   /* scratch of link_bus_form and link_bus_reach */
   struct retro_link_port *was_head, *was_next, *uf, *first, *last;
   uint64_t rate;
   uint64_t anchor;      /* tick at its console's current edge */
   uint64_t since;       /* that edge's instant, as ticks of rate */
   uint64_t last_anchor; /* the edge before, kept while a frame with no */
   uint64_t last_since;  /* frame-end publish leaves this edge's tick unknown */
   uint64_t promise;
   uint64_t local, request, target; /* latest publish; target is the request repaired */
   uint64_t start_local; /* this frame's frame-start advance */
   uint64_t span;        /* the last frame shown, frame-start advance to frame-end publish */
   uint64_t grant;
   uint64_t seq;         /* in a bus's first member: link_bus::seq at the bus's last change */
   uint64_t view_at;     /* the edge its view last changed at, as ticks of rate */
   uint64_t place;       /* scratch of link_bus_place, as ticks of rate */
   unsigned console, port;
   unsigned slot;        /* creation order: its place in every port's links */
   unsigned seen;        /* the other ports in its view */
   unsigned flags, max_ports;
   unsigned index, count, was_count, size;
   unsigned cap;         /* link_bus_group's, in a root: the lowest max_ports joined, 0 for none */
   uint32_t pending;     /* reports not yet given */
   struct link_bus_entry *queue, *queue_tail;     /* landed, in the order recv takes them */
   struct link_bus_entry *waiting, *waiting_tail; /* landing not known yet; send order per index at send */
   uint64_t limit;       /* latest grant: what lands below it is receivable */
   uint64_t idle;        /* the port list's idle_ticks */
   uint64_t last_stamp;  /* highest stamp sent */
   bool sent;
   bool renumber;        /* renumbered: its frame-start advance moves its queue and stamp */
   bool stale;           /* renumbered: re-joins when the next frame opens, or at its unhold */
   bool attached;        /* as the core sees it */
   bool live;            /* in the membership formed at the edge */
   bool cut;             /* detached in this frame: a silent member until the edge */
   bool join;            /* attached in this frame: joins at the edge */
   bool edge_known;
   bool promise_valid, span_valid, grant_valid;
   bool span_shown;      /* a span shown since placed: each frame end moves since by it */
   bool published;       /* in this frame */
   bool unplaced;        /* waits for link_bus_place: since not set */
   bool viewed;          /* view_at is set */
   bool apart;           /* cut from a port no longer on its bus */
   bool left_out;        /* scratch of link_bus_group: max_ports kept it off */
   bool admitted;        /* scratch of link_bus_group */
   bool placing;         /* scratch of link_bus_place: place is chosen */
   /* scratch of link_bus_defer: its bus as the cables make it, and in that
    * bus's root whether it joins ports and has a console of unknown edge */
   struct retro_link_port *grp, *grp_was;
   bool grp_seen, grp_join, grp_unsized;
};

struct link_bus_cable
{
   struct retro_link_port *a, *b;
};

struct link_bus_console
{
   struct retro_link_port_info *ports; /* the list, strings included, or NULL */
   uint64_t activity; /* its sends and recvs */
   bool in_frame;
   bool held;
   bool unholding;    /* its hold's end waits for a step's end */
};

struct link_bus
{
   struct link_bus_console *consoles;
   struct retro_link_port **recs; /* by console, then port number */
   struct link_bus_cable *cables;
   struct link_bus_counts counts;
   struct link_bus_pool pools[LINK_BUS_POOLS];
   struct link_bus_item *blocks;
   struct link_bus_sent *journal;
   uint64_t journal_gen; /* moves when the journal starts over */
   uint64_t sends;
   uint64_t seq;
   size_t journal_len, journal_cap;
   size_t recs_len, recs_cap;
   size_t cables_len, cables_cap;
   size_t links_cap;     /* every port's links */
   unsigned console_count;
   bool held_back;       /* link_bus_group kept a port's place for a console in a frame */
   bool released;
};

/* The counts a blob holds: all but blocks */
static const size_t link_bus_saved_counts[LINK_BUS_SAVED_COUNTS] = {
   offsetof(struct link_bus_counts, lowered_promise),
   offsetof(struct link_bus_counts, low_stamp),
   offsetof(struct link_bus_counts, request_past_promise),
   offsetof(struct link_bus_counts, request_past_frame_end),
   offsetof(struct link_bus_counts, request_past_earliest),
   offsetof(struct link_bus_counts, skipped_publish),
   offsetof(struct link_bus_counts, left_out),
   offsetof(struct link_bus_counts, misplaced_call)
};

static void *link_bus_alloc(link_bus_t *b, void *ptr, size_t size)
{
   b->counts.blocks++;
   return realloc(ptr, size);
}

/* Makes n items of the pool free, a block at a time */
static bool link_bus_reserve(link_bus_t *b, struct link_bus_pool *pool, size_t n)
{
   while (pool->count < n)
   {
      char *block = (char*)link_bus_alloc(b, NULL, LINK_BUS_BLOCK);
      size_t at;
      if (!block)
         return false;
      ((struct link_bus_item*)block)->next = b->blocks;
      b->blocks                            = (struct link_bus_item*)block;
      for (at = LINK_BUS_BLOCK_HEAD; at + pool->size <= LINK_BUS_BLOCK; at += pool->size)
      {
         struct link_bus_item *it = (struct link_bus_item*)(block + at);
         it->next                 = pool->free;
         pool->free               = it;
         pool->count++;
      }
   }
   return true;
}

static void *link_bus_take(struct link_bus_pool *pool)
{
   struct link_bus_item *it = pool->free;
   pool->free               = it->next;
   pool->count--;
   return it;
}

static void link_bus_give(struct link_bus_pool *pool, void *ptr)
{
   struct link_bus_item *it = (struct link_bus_item*)ptr;
   it->next                 = pool->free;
   pool->free               = it;
   pool->count++;
}

static bool link_bus_journal_room(link_bus_t *b, size_t n)
{
   size_t cap = b->journal_cap ? b->journal_cap : 16;
   struct link_bus_sent *j;
   if (b->journal_len + n <= b->journal_cap)
      return true;
   while (cap < b->journal_len + n)
      cap *= 2;
   if (!(j = (struct link_bus_sent*)link_bus_alloc(b, b->journal, cap * sizeof(*j))))
      return false;
   b->journal     = j;
   b->journal_cap = cap;
   return true;
}

/* a * b as 128 bits, from four 32-bit partial products */
static void link_bus_mul(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo)
{
   uint64_t a0  = a & 0xFFFFFFFFUL, a1 = a >> 32;
   uint64_t b0  = b & 0xFFFFFFFFUL, b1 = b >> 32;
   uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
   uint64_t mid = (p00 >> 32) + (p01 & 0xFFFFFFFFUL) + (p10 & 0xFFFFFFFFUL);
   *lo = (p00 & 0xFFFFFFFFUL) | (mid << 32);
   *hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

/* t * num / den, rounded down or, with up, up; saturates at LINK_BUS_TICK_MAX */
static uint64_t link_bus_scale(uint64_t t, uint64_t num, uint64_t den, bool up)
{
   uint64_t hi, lo, q = 0, r;
   int i;
   link_bus_mul(t, num, &hi, &lo);
   if (hi >= den)
      return LINK_BUS_TICK_MAX;
   r = hi;
   for (i = 63; i >= 0; i--)
   {
      bool carry = (r >> 63) != 0;
      r  = (r << 1) | ((lo >> i) & 1);
      q <<= 1;
      if (carry || r >= den)
      {
         r -= den;
         q |= 1;
      }
   }
   if (up && r && q < LINK_BUS_TICK_MAX)
      q++;
   return q > LINK_BUS_TICK_MAX ? LINK_BUS_TICK_MAX : q;
}

static uint64_t link_bus_add(uint64_t a, uint64_t b)
{
   if (a >= LINK_BUS_TICK_MAX || b > LINK_BUS_TICK_MAX - a)
      return LINK_BUS_TICK_MAX;
   return a + b;
}

bool link_bus_instant_before(uint64_t a, uint64_t ra, uint64_t b, uint64_t rb)
{
   uint64_t ah, al, bh, bl;
   link_bus_mul(a, rb, &ah, &al);
   link_bus_mul(b, ra, &bh, &bl);
   return ah < bh || (ah == bh && al < bl);
}

/* The instant of tick t on a clock whose tick anchor stands at the instant
 * since, as ticks of that clock; a tick before anchor counts as anchor */
static uint64_t link_bus_when(uint64_t since, uint64_t anchor, uint64_t t)
{
   return link_bus_add(since, t > anchor ? t - anchor : 0);
}

static uint64_t link_bus_instant(const struct retro_link_port *p, uint64_t t)
{
   return link_bus_when(p->since, p->anchor, t);
}

/* The tick of the instant at, given as ticks of rate, on a clock of rate r
 * whose tick anchor stands at the instant since: rounded down for a bound
 * or up for a landing, never before anchor */
static uint64_t link_bus_tick(uint64_t since, uint64_t anchor, uint64_t r,
      uint64_t at, uint64_t rate, bool up)
{
   uint64_t t = link_bus_scale(at, r, rate, up);
   return t > since ? link_bus_add(anchor, t - since) : anchor;
}

static uint64_t link_bus_onto(const struct retro_link_port *p,
      uint64_t at, uint64_t rate, bool up)
{
   return link_bus_tick(p->since, p->anchor, p->rate, at, rate, up);
}

/* Its tick at its edge is not known because its frame ended with no
 * frame-end publish; its promise still binds, on the edge before */
static bool link_bus_skipped(const struct retro_link_port *p)
{
   return !p->edge_known && p->promise_valid;
}

static uint64_t link_bus_gcd(uint64_t a, uint64_t b)
{
   while (b)
   {
      uint64_t r = a % b;
      a          = b;
      b          = r;
   }
   return a;
}

/* t, given against the anchor from, given against the anchor to */
static uint64_t link_bus_move(uint64_t t, uint64_t from, uint64_t to)
{
   if (t >= from)
      return link_bus_add(to, t - from);
   return to > from - t ? to - (from - t) : 0;
}

static struct link_bus_link *link_bus_link(const struct retro_link_port *r,
      const struct retro_link_port *m)
{
   return &r->links[m->slot];
}

/* Room in every port's links for n ports */
static bool link_bus_links_room(link_bus_t *b, size_t n)
{
   size_t i, cap = b->links_cap ? b->links_cap : 4;
   if (n <= b->links_cap)
      return true;
   while (cap < n)
      cap *= 2;
   for (i = 0; i < b->recs_len; i++)
   {
      struct link_bus_link *k = (struct link_bus_link*)link_bus_alloc(b,
            b->recs[i]->links, cap * sizeof(*k));
      if (!k)
         return false;
      memset(k + b->links_cap, 0, (cap - b->links_cap) * sizeof(*k));
      b->recs[i]->links = k;
   }
   b->links_cap = cap;
   return true;
}

/* Where console's record of port is, or would go */
static size_t link_bus_at(const link_bus_t *b, unsigned console, unsigned port)
{
   size_t lo = 0, hi = b->recs_len;
   while (lo < hi)
   {
      size_t mid                     = lo + (hi - lo) / 2;
      const struct retro_link_port *r = b->recs[mid];
      if (r->console < console || (r->console == console && r->port < port))
         lo = mid + 1;
      else
         hi = mid;
   }
   return lo;
}

static struct retro_link_port *link_bus_find(const link_bus_t *b,
      unsigned console, unsigned port)
{
   size_t i = link_bus_at(b, console, port);
   if (i < b->recs_len && b->recs[i]->console == console && b->recs[i]->port == port)
      return b->recs[i];
   return NULL;
}

static bool link_bus_foreign(const link_bus_t *b, unsigned console,
      const retro_link_port_t *h)
{
   return h && (h->owner != b || h->console != console);
}

/* The record a call by console on h acts on; NULL answers as for a port not
 * attached. */
static struct retro_link_port *link_bus_own(const link_bus_t *b,
      unsigned console, retro_link_port_t *h)
{
   if (!h || link_bus_foreign(b, console, h) || !h->attached)
      return NULL;
   return h;
}

/* send, recv, peek and advance are allowed only inside the console's frame */
static bool link_bus_in_frame(const link_bus_t *b, unsigned console)
{
   return console < b->console_count && b->consoles[console].in_frame;
}

static bool link_bus_any_in_frame(const link_bus_t *b)
{
   unsigned c;
   for (c = 0; c < b->console_count; c++)
      if (b->consoles[c].in_frame)
         return true;
   return false;
}

/* link_bus_own for a call allowed only inside the console's frame */
static struct retro_link_port *link_bus_framed(const link_bus_t *b,
      unsigned console, retro_link_port_t *h)
{
   return link_bus_in_frame(b, console) ? link_bus_own(b, console, h) : NULL;
}

/* The record a send, recv or peek acts on; counts one made outside the
 * console's frame, before the port's frame-start advance (its anchor may be
 * stale) or on another console's handle */
static struct retro_link_port *link_bus_call(link_bus_t *b,
      unsigned console, retro_link_port_t *h)
{
   struct retro_link_port *p;
   if (!link_bus_in_frame(b, console) || link_bus_foreign(b, console, h))
   {
      b->counts.misplaced_call++;
      return NULL;
   }
   p = link_bus_own(b, console, h);
   if (p && p->live && !p->cut && !p->published)
   {
      b->counts.misplaced_call++;
      return NULL;
   }
   return p;
}

/* On a bus as its core sees it, so granted ticks, not RETRO_LINK_UNBOUNDED */
static bool link_bus_joined(const struct retro_link_port *p)
{
   return p->attached && p->live && !p->cut && p->seen;
}

/* p's index in its view */
static unsigned link_bus_index(const link_bus_t *b, const struct retro_link_port *p)
{
   unsigned n = 0;
   size_t i;
   for (i = 0; i < b->recs_len && b->recs[i] != p; i++)
      if (link_bus_link(p, b->recs[i])->flags & LINK_BUS_LINK_VIEW)
         n++;
   return n;
}

/* s's index in r's view at the instant x (ticks of rate): the ports before s
 * that r saw there, r among them; the view before its last change is kept */
static unsigned link_bus_from(const link_bus_t *b, const struct retro_link_port *r,
      const struct retro_link_port *s, uint64_t x, uint64_t rate)
{
   unsigned bit = r->viewed && link_bus_instant_before(x, rate, r->view_at, r->rate)
      ? LINK_BUS_LINK_WAS : LINK_BUS_LINK_VIEW;
   unsigned n   = 0;
   size_t i;
   for (i = 0; i < b->recs_len && b->recs[i] != s; i++)
      if (b->recs[i] == r || (link_bus_link(r, b->recs[i])->flags & bit))
         n++;
   return n;
}

/* Whether a link carries a message stamped at the instant at (ticks of
 * rate) that lands at the instant land (ticks of land_rate): stamped at or
 * after the join instant, landing before the cut */
static bool link_bus_carries(const struct link_bus_link *k, uint64_t at, uint64_t rate,
      uint64_t land, uint64_t land_rate)
{
   bool from_join = !(k->flags & LINK_BUS_LINK_JOIN) || (k->flags & LINK_BUS_LINK_HOLD)
      || !link_bus_instant_before(at, rate, k->join, k->join_rate);
   bool to_cut    = !(k->flags & LINK_BUS_LINK_CUT)
      || link_bus_instant_before(land, land_rate, k->cut, k->cut_rate);
   if (k->flags & LINK_BUS_LINK_GAP)
      return from_join || to_cut;
   return from_join && to_cut;
}

/* Starts a port's ticks over: nothing it published before binds */
static void link_bus_fresh(struct retro_link_port *p)
{
   p->edge_known    = false;
   p->promise_valid = false;
   p->span_valid    = false;
   p->span_shown    = false;
   p->grant_valid   = false;
   p->published     = false;
   p->limit         = 0;
}

/* A renumbered port's re-join keeps its learned span, a duration on its
 * own clock */
static void link_bus_rejoin(struct retro_link_port *p)
{
   bool span_valid = p->span_valid;
   bool span_shown = p->span_shown;
   link_bus_fresh(p);
   p->span_valid = span_valid;
   p->span_shown = span_shown;
   p->renumber   = true;
   p->stale      = false;
}

/* Moves the changes of p's bus, and of the buses of ports cut from it that
 * it still bounds */
static void link_bus_changed(link_bus_t *b, struct retro_link_port *p)
{
   size_t i;
   ++b->seq;
   if (p->head)
      p->head->seq = b->seq;
   if (p->apart)
      for (i = 0; i < b->recs_len; i++)
      {
         const struct link_bus_link *k = link_bus_link(p, b->recs[i]);
         if (     (k->flags & LINK_BUS_LINK_CUT) && !(k->flags & LINK_BUS_LINK_TOGETHER)
               && b->recs[i]->head)
            b->recs[i]->head->seq = b->seq;
      }
}

/* a comes out before c: by landing, then the sender's index, then the
 * sender, then send order */
static bool link_bus_before(const struct link_bus_entry *a, const struct link_bus_entry *c)
{
   const struct retro_link_port *fa = a->msg->from, *fc = c->msg->from;
   if (a->landing != c->landing)
      return a->landing < c->landing;
   if (a->from_index != c->from_index)
      return a->from_index < c->from_index;
   if (fa->console != fc->console)
      return fa->console < fc->console;
   if (fa->port != fc->port)
      return fa->port < fc->port;
   return a->seq < c->seq;
}

static void link_bus_enqueue(struct retro_link_port *p, struct link_bus_entry *e)
{
   struct link_bus_entry **at = &p->queue;
   if (p->queue_tail && !link_bus_before(e, p->queue_tail))
      at = &p->queue_tail->next;
   else
      while (*at && !link_bus_before(e, *at))
         at = &(*at)->next;
   e->next = *at;
   *at     = e;
   if (!e->next)
      p->queue_tail = e;
}

static void link_bus_wait(struct retro_link_port *p, struct link_bus_entry *e)
{
   e->next = NULL;
   if (p->waiting_tail)
      p->waiting_tail->next = e;
   else
      p->waiting            = e;
   p->waiting_tail = e;
}

static void link_bus_retire(link_bus_t *b, struct link_bus_entry *e)
{
   struct link_bus_msg *m = e->msg;
   if (!--m->refs)
   {
      link_bus_give(&b->pools[m->cell_pool], m->cell);
      link_bus_give(&b->pools[LINK_BUS_POOL_MSG], m);
   }
   link_bus_give(&b->pools[LINK_BUS_POOL_ENTRY], e);
}

/* Drops every message queued for p */
static void link_bus_drop(link_bus_t *b, struct retro_link_port *p)
{
   struct link_bus_entry *e, *next;
   for (e = p->queue; e; e = next)
   {
      next = e->next;
      link_bus_retire(b, e);
   }
   for (e = p->waiting; e; e = next)
   {
      next = e->next;
      link_bus_retire(b, e);
   }
   p->queue        = NULL;
   p->queue_tail   = NULL;
   p->waiting      = NULL;
   p->waiting_tail = NULL;
}

/* p's anchor has become known: the messages waiting for it land */
static void link_bus_land(link_bus_t *b, struct retro_link_port *p)
{
   struct link_bus_entry *e = p->waiting, *next;
   p->waiting               = NULL;
   p->waiting_tail          = NULL;
   for (; e; e = next)
   {
      next       = e->next;
      if (e->state == LINK_BUS_JOINING || e->state == LINK_BUS_PLACING)
      {
         link_bus_wait(p, e);
         continue;
      }
      e->landing = p->anchor;
      if (e->state == LINK_BUS_AHEAD)
         e->landing = link_bus_onto(p, e->instant, e->rate, true);
      e->state = LINK_BUS_LANDED;
      if (e->journal_gen == b->journal_gen)
         b->journal[e->journal].landing = e->landing;
      link_bus_enqueue(p, e);
   }
}

/* The frame-start advance of a renumbered port gives its new anchor: its
 * queue and its stamp move with it */
static void link_bus_rebase(struct retro_link_port *p, uint64_t anchor)
{
   struct link_bus_entry *e;
   for (e = p->queue; e; e = e->next)
      e->landing = link_bus_move(e->landing, p->anchor, anchor);
   if (p->sent)
      p->last_stamp = link_bus_move(p->last_stamp, p->anchor, anchor);
   p->renumber = false;
}

/* e is receivable at p: below its grant, and not from a port whose joining
 * p has not seen yet */
static bool link_bus_ready(const struct retro_link_port *p, const struct link_bus_entry *e)
{
   unsigned f = link_bus_link(p, e->msg->from)->flags;
   return e->landing < p->limit
      && (f & (LINK_BUS_LINK_TOGETHER | LINK_BUS_LINK_VIEW)) != LINK_BUS_LINK_TOGETHER;
}

/* The message p hands out next, or NULL */
static struct link_bus_entry *link_bus_next(const struct retro_link_port *p)
{
   if (p->cut || !p->queue || !link_bus_ready(p, p->queue))
      return NULL;
   return p->queue;
}

static void link_bus_pop(link_bus_t *b, struct retro_link_port *p)
{
   struct link_bus_entry *e = p->queue;
   if (!(p->queue = e->next))
      p->queue_tail = NULL;
   link_bus_retire(b, e);
}

/* Marks what p's grant has made receivable; true if any of it was not before */
static bool link_bus_reached(struct retro_link_port *p)
{
   struct link_bus_entry *e;
   bool fresh = false;
   for (e = p->queue; e && link_bus_ready(p, e); e = e->next)
      if (!e->seen)
      {
         e->seen = true;
         fresh   = true;
      }
   return fresh;
}

/* Nothing stamped within p's quiet span before its anchor */
static bool link_bus_quiet(const struct retro_link_port *p)
{
   uint64_t span = p->idle;
   if (!p->sent)
      return true;
   if (!span)
   {
      if (!p->span_valid)
         return false;
      span = p->span > LINK_BUS_TICK_MAX / LINK_BUS_IDLE_SPANS
         ? LINK_BUS_TICK_MAX : p->span * LINK_BUS_IDLE_SPANS;
   }
   return p->anchor >= p->last_stamp && p->anchor - p->last_stamp >= span;
}

/* The highest tick on p's clock its peers allow, and the peer that sets it;
 * RETRO_LINK_UNBOUNDED when no peer bounds p. A peer whose tick at its edge
 * is not known holds p at that edge's instant, or at its promise's if a
 * frame that ended with no frame-end publish left it earlier; a held or
 * silent one holds nobody. A peer bounds p from the pair's join instant and
 * before its cut; until the join instant is known, p is held at its own
 * console's first edge at or after the join. */
static uint64_t link_bus_bound(const struct retro_link_port *p,
      const struct retro_link_port **by)
{
   const link_bus_t *b = p->owner;
   uint64_t bound      = RETRO_LINK_UNBOUNDED;
   size_t i;
   for (i = 0; i < b->recs_len; i++)
   {
      const struct retro_link_port *q = b->recs[i];
      const struct link_bus_link *k   = link_bus_link(p, q);
      uint64_t at, rate = q->rate, t;
      if (     q == p || !(k->flags & (LINK_BUS_LINK_TOGETHER | LINK_BUS_LINK_CUT))
            || !q->live || q->cut || b->consoles[q->console].held)
         continue;
      at = q->edge_known ? link_bus_instant(q, q->promise) : q->since;
      if (link_bus_skipped(q))
      {
         uint64_t was = link_bus_when(q->last_since, q->last_anchor, q->promise);
         if (was < at)
            at = was;
      }
      if ((k->flags & LINK_BUS_LINK_JOIN) && (k->flags & (LINK_BUS_LINK_MINE | LINK_BUS_LINK_THEIRS)))
      {
         if (k->flags & LINK_BUS_LINK_MINE)
            continue;
         at   = k->join;
         rate = k->join_rate;
      }
      else if (q->unplaced)
         continue;
      else
      {
         bool early = (k->flags & LINK_BUS_LINK_JOIN)
            && link_bus_instant_before(at, rate, k->join, k->join_rate);
         bool late  = (k->flags & LINK_BUS_LINK_CUT)
            && !link_bus_instant_before(at, rate, k->cut, k->cut_rate);
         if (!(k->flags & LINK_BUS_LINK_GAP) && late)
            continue;
         if (early && (late || !(k->flags & LINK_BUS_LINK_GAP)))
         {
            at   = k->join;
            rate = k->join_rate;
            if (     !(k->flags & LINK_BUS_LINK_GAP) && (k->flags & LINK_BUS_LINK_CUT)
                  && !link_bus_instant_before(at, rate, k->cut, k->cut_rate))
               continue;
         }
      }
      t = link_bus_onto(p, at, rate, false);
      if (t < bound)
      {
         bound = t;
         *by   = q;
      }
   }
   return bound;
}

/* The lowest tick p may be granted without waiting */
static uint64_t link_bus_floor(const struct retro_link_port *p)
{
   if (p->grant_valid && p->grant > p->local)
      return p->grant;
   return p->local;
}

static struct retro_link_port *link_bus_root(struct retro_link_port *p)
{
   while (p->uf != p)
   {
      p->uf = p->uf->uf;
      p     = p->uf;
   }
   return p;
}

static void link_bus_unite(struct retro_link_port *a, struct retro_link_port *b)
{
   a = link_bus_root(a);
   b = link_bus_root(b);
   if (a != b)
      b->uf = a;
}

static bool link_bus_shared(const struct retro_link_port *p)
{
   return (p->flags & RETRO_LINK_PORT_SHARED_MEDIUM) != 0;
}

/* Takes the live ports in index order, by console then port number, each
 * joining only if its bus stays within every member's max_ports; with keep,
 * the ports that were on a bus first. True if a port whose console is
 * inside a frame is left off the bus it was on. */
static bool link_bus_admit(link_bus_t *b, bool keep)
{
   size_t i;
   unsigned pass;
   bool pushed = false;
   for (i = 0; i < b->recs_len; i++)
   {
      b->recs[i]->size     = 0;
      b->recs[i]->cap      = 0;
      b->recs[i]->admitted = false;
      b->recs[i]->left_out = false;
   }
   for (pass = 0; pass < 2; pass++)
      for (i = 0; i < b->recs_len; i++)
      {
         struct retro_link_port *p = b->recs[i], *r;
         unsigned cap;
         if (!p->live || p->admitted || (keep && p->was_head) != (pass == 0))
            continue;
         r   = link_bus_root(p);
         cap = r->cap;
         if (p->max_ports && (!cap || p->max_ports < cap))
            cap = p->max_ports;
         if (r->size >= LINK_BUS_MAX_BUS_PORTS || (cap && r->size >= cap))
         {
            p->left_out = true;
            if (p->was_head && b->consoles[p->console].in_frame)
               pushed = true;
            continue;
         }
         r->cap      = cap;
         p->admitted = true;
         r->size++;
      }
   return pushed;
}

/* Inside a frame and no port of it has shown its span since it was placed:
 * its first frame edge is not known */
static bool link_bus_unsized(const link_bus_t *b, unsigned c)
{
   size_t i;
   if (!b->consoles[c].in_frame || b->consoles[c].held)
      return false;
   for (i = link_bus_at(b, c, 0); i < b->recs_len && b->recs[i]->console == c; i++)
      if (b->recs[i]->live && !b->recs[i]->unplaced && b->recs[i]->span_shown)
         return false;
   return true;
}

/* A join waits while a console on the bus it would make has an unknown
 * first edge, which the join instant and a place need: that bus keeps the
 * ports that were together, and link_bus_step_end forms it again. True if
 * one waits. */
static bool link_bus_defer(link_bus_t *b)
{
   size_t i;
   bool any = false;
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i];
      p->grp         = p->live ? link_bus_root(p) : NULL;
      p->grp_seen    = false;
      p->grp_join    = false;
      p->grp_unsized = false;
   }
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i], *r = p->grp;
      if (!r)
         continue;
      if (!r->grp_seen)
      {
         r->grp_seen = true;
         r->grp_was  = p->was_head;
      }
      else if (!p->was_head || p->was_head != r->grp_was)
         r->grp_join = true;
      if (link_bus_unsized(b, p->console))
         r->grp_unsized = true;
   }
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i];
      if (p->grp && p->grp->grp_join && p->grp->grp_unsized)
         p->uf = p;
   }
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i], *m;
      if (!p->grp || !p->grp->grp_join || !p->grp->grp_unsized)
         continue;
      any = true;
      for (m = p->was_head; m && !(m->live && m->grp == p->grp); m = m->was_next);
      if (m && m != p)
         link_bus_unite(m, p);
   }
   return any;
}

/* Forms the buses from the cable map, the shared media and the live ports */
static void link_bus_group(link_bus_t *b)
{
   size_t i, j;
   b->seq++;
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i];
      p->was_head  = p->head;
      p->was_next  = p->next;
      p->was_count = p->count;
      p->head      = NULL;
      p->next      = NULL;
      p->first     = NULL;
      p->last      = NULL;
      p->uf        = p;
      p->size      = 0;
      p->cap       = 0;
      p->index     = 0;
      p->count     = 0;
   }
   for (i = 0; i < b->cables_len; i++)
   {
      struct retro_link_port *x = b->cables[i].a, *y = b->cables[i].b;
      if (     x->live && y->live && !link_bus_shared(x) && !link_bus_shared(y)
            && !strcmp(x->protocol, y->protocol))
         link_bus_unite(x, y);
   }
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i];
      if (!p->live || !link_bus_shared(p))
         continue;
      for (j = 0; j < i; j++)
      {
         struct retro_link_port *q = b->recs[j];
         if (q->live && link_bus_shared(q) && !strcmp(q->protocol, p->protocol))
         {
            link_bus_unite(q, p);
            break;
         }
      }
   }
   /* A join that would push out a port whose console is inside a frame
    * waits for that console's edge: the ports on a bus go first until then */
   b->held_back = link_bus_defer(b);
   if (link_bus_admit(b, false))
   {
      b->held_back = true;
      link_bus_admit(b, true);
   }
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i], *r;
      if (!p->admitted)
         continue;
      r = link_bus_root(p);
      if (r->last)
         r->last->next = p;
      else
         r->first      = p;
      r->last  = p;
      p->index = r->count++;
      p->head  = r->first;
   }
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i], *r;
      if (!p->head)
         continue;
      r = link_bus_root(p);
      if (r->size < 2)
      {
         p->head  = NULL;
         p->next  = NULL;
         p->index = 0;
         p->count = 0;
      }
      else
         p->count = r->size;
   }
   for (i = 0; i < b->recs_len; i++)
      if (b->recs[i]->head == b->recs[i])
         b->recs[i]->seq = b->seq;
}

/* Its since is its console's current edge's instant */
static bool link_bus_placed(const struct retro_link_port *p)
{
   return p->live && !p->cut && !p->unplaced && !p->owner->consoles[p->console].held;
}

/* The gcd of the clock rates in console's port list or, with none, of its
 * ports: its edges stand on whole multiples of 1 / grid seconds */
static uint64_t link_bus_grid(const link_bus_t *b, unsigned console)
{
   const struct retro_link_port_info *e = b->consoles[console].ports;
   uint64_t g                           = 0;
   size_t i;
   if (e)
      for (; e->protocol_id; e++)
         g = link_bus_gcd(g, e->clock_rate);
   else
      for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
         g = link_bus_gcd(g, b->recs[i]->rate);
   return g ? g : 1;
}

/* The first frame edge at or after now of p's console, as ticks of p's
 * rate; false while it is not known: held, not placed, or inside a frame
 * whose span no port of it has shown */
static bool link_bus_first_edge(const link_bus_t *b, const struct retro_link_port *p,
      uint64_t *at)
{
   const struct link_bus_console *k = &b->consoles[p->console];
   size_t i;
   if (k->held || p->unplaced)
      return false;
   if (!k->in_frame)
   {
      *at = p->since;
      return true;
   }
   for (i = link_bus_at(b, p->console, 0); i < b->recs_len && b->recs[i]->console == p->console; i++)
   {
      const struct retro_link_port *q = b->recs[i];
      if (q->live && !q->unplaced && q->span_shown)
      {
         *at = link_bus_scale(link_bus_add(q->since, q->span), p->rate, q->rate, true);
         return true;
      }
   }
   return false;
}

/* p's console stands at or past the instant at (ticks of rate) */
static bool link_bus_stands(const link_bus_t *b, const struct retro_link_port *p,
      uint64_t at, uint64_t rate)
{
   return !b->consoles[p->console].held && !p->unplaced
      && !link_bus_instant_before(p->since, p->rate, at, rate);
}

static void link_bus_on(struct link_bus_link *k, uint64_t j, uint64_t rate,
      bool mine, bool theirs, unsigned hold)
{
   unsigned f = k->flags & (LINK_BUS_LINK_VIEW | LINK_BUS_LINK_WAS | LINK_BUS_LINK_CUT
         | (hold ? LINK_BUS_LINK_FLIPPED | LINK_BUS_LINK_GAP : 0));
   f |= LINK_BUS_LINK_TOGETHER | LINK_BUS_LINK_JOIN | hold;
   if (!hold && (k->flags & LINK_BUS_LINK_CUT))
      f |= LINK_BUS_LINK_GAP;
   if (!hold && (k->flags & LINK_BUS_LINK_VIEW))
      f |= LINK_BUS_LINK_FLIPPED;
   if (mine)
      f |= LINK_BUS_LINK_MINE;
   if (theirs)
      f |= LINK_BUS_LINK_THEIRS;
   k->flags     = f;
   k->join      = j;
   k->join_rate = rate;
}

/* r and m come onto one bus: they hear each other from J, the later of
 * their consoles' first edges at or after now. One not known yet is waited
 * for (MINE on the link of the port whose console it is). hold is
 * LINK_BUS_LINK_HOLD for a hold's end. */
static void link_bus_came_on(const link_bus_t *b, struct retro_link_port *r,
      struct retro_link_port *m, unsigned hold)
{
   uint64_t a = 0, c = 0, j = 0, rate = 1;
   bool ka = link_bus_first_edge(b, r, &a);
   bool kc = link_bus_first_edge(b, m, &c);
   if (ka)
   {
      j    = a;
      rate = r->rate;
   }
   if (kc && (!ka || link_bus_instant_before(j, rate, c, m->rate)))
   {
      j    = c;
      rate = m->rate;
   }
   link_bus_on(link_bus_link(r, m), j, rate, !ka, !kc, hold);
   link_bus_on(link_bus_link(m, r), j, rate, !kc, !ka, hold);
}

/* r and m leave one bus at t (ticks of rate): from now on they carry and
 * bound only before it; messages already sent stay */
static void link_bus_off(struct retro_link_port *r, struct retro_link_port *m,
      uint64_t t, uint64_t rate)
{
   struct link_bus_link *k = link_bus_link(r, m);
   k->flags    = (k->flags & ~(LINK_BUS_LINK_TOGETHER | LINK_BUS_LINK_GAP
            | LINK_BUS_LINK_FLIPPED | LINK_BUS_LINK_HOLD)) | LINK_BUS_LINK_CUT;
   k->cut      = t;
   k->cut_rate = rate;
   r->apart    = true;
}

/* A console at an edge none of whose ports has shown a span since it was
 * placed is placed again when a port of it joins a bus */
static void link_bus_unplace(link_bus_t *b, unsigned c)
{
   size_t i, first = link_bus_at(b, c, 0);
   if (b->consoles[c].held || b->consoles[c].in_frame)
      return;
   for (i = first; i < b->recs_len && b->recs[i]->console == c; i++)
      if (b->recs[i]->span_shown)
         return;
   for (i = first; i < b->recs_len && b->recs[i]->console == c; i++)
      if (b->recs[i]->live)
         b->recs[i]->unplaced = true;
}

/* After link_bus_group: each pair of ports that came onto one bus or left
 * one. A pair leaves at the edge of a port max_ports left out, else at t
 * (ticks of rate), the edge of the change. */
static void link_bus_relate(link_bus_t *b, uint64_t t, uint64_t rate)
{
   size_t i;
   struct retro_link_port *m;
   for (i = 0; i < b->recs_len; i++)
      for (m = b->recs[i]->head ? b->recs[i]->next : NULL; m; m = m->next)
         if (!(link_bus_link(b->recs[i], m)->flags & LINK_BUS_LINK_TOGETHER))
         {
            link_bus_unplace(b, b->recs[i]->console);
            link_bus_unplace(b, m->console);
         }
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *r = b->recs[i];
      for (m = r->head ? r->next : NULL; m; m = m->next)
         if (!(link_bus_link(r, m)->flags & LINK_BUS_LINK_TOGETHER))
            link_bus_came_on(b, r, m, 0);
      for (m = r->was_head ? r->was_next : NULL; m; m = m->was_next)
         if (     (link_bus_link(r, m)->flags & LINK_BUS_LINK_TOGETHER)
               && (!r->head || r->head != m->head))
         {
            const struct retro_link_port *x = m->live && m->left_out ? m
               : r->live && r->left_out ? r : NULL;
            uint64_t at = x ? x->since : t, ar = x ? x->rate : rate;
            link_bus_off(r, m, at, ar);
            link_bus_off(m, r, at, ar);
         }
   }
}

/* The messages for r held while their join instant was not known: dropped
 * before it, else on their way */
static void link_bus_join_known(link_bus_t *b, struct retro_link_port *r)
{
   struct link_bus_entry **at = &r->waiting, *e, *last = NULL;
   bool any = false;
   while ((e = *at) != NULL)
   {
      const struct link_bus_link *k = link_bus_link(r, e->msg->from);
      if (e->state == LINK_BUS_JOINING && !(k->flags & (LINK_BUS_LINK_MINE | LINK_BUS_LINK_THEIRS)))
      {
         if (!link_bus_carries(k, e->instant, e->rate,
                  link_bus_scale(e->instant, r->rate, e->rate, true), r->rate))
         {
            *at = e->next;
            link_bus_retire(b, e);
            continue;
         }
         e->state = LINK_BUS_AHEAD;
         any      = true;
      }
      last = e;
      at   = &e->next;
   }
   r->waiting_tail = last;
   if (any && r->edge_known && !r->unplaced)
      link_bus_land(b, r);
}

/* p's console stands at its first edge at or after a join that waited for
 * it: the join instant is the later of that edge and the other's, and what
 * the other port held for it goes on */
static void link_bus_known(link_bus_t *b, struct retro_link_port *p)
{
   size_t i;
   for (i = 0; i < b->recs_len; i++)
   {
      struct link_bus_link *k = link_bus_link(p, b->recs[i]), *l;
      if (b->recs[i] == p || !(k->flags & LINK_BUS_LINK_MINE))
         continue;
      l = link_bus_link(b->recs[i], p);
      if (link_bus_instant_before(k->join, k->join_rate, p->since, p->rate))
      {
         k->join      = p->since;
         k->join_rate = p->rate;
      }
      k->flags     &= ~LINK_BUS_LINK_MINE;
      l->flags     &= ~LINK_BUS_LINK_THEIRS;
      l->join       = k->join;
      l->join_rate  = k->join_rate;
      link_bus_join_known(b, b->recs[i]);
   }
}

/* e, from p to q, stamped at the instant at and sent from the instant
 * sent_at (both ticks of p's rate), goes on its way: landed, waiting for
 * what it needs, or dropped */
static void link_bus_route(link_bus_t *b, struct retro_link_port *p,
      struct retro_link_port *q, struct link_bus_entry *e, uint64_t at, uint64_t sent_at)
{
   const struct link_bus_link *k = link_bus_link(p, q);
   uint64_t land;
   e->instant    = at;
   e->rate       = p->rate;
   e->sent_at    = sent_at;
   e->sent_rate  = p->rate;
   e->landing    = RETRO_LINK_UNBOUNDED;
   e->from_index = link_bus_from(b, q, p, sent_at, p->rate);
   if (b->consoles[q->console].held)
      e->state = LINK_BUS_REJOIN;
   else if (     (k->flags & LINK_BUS_LINK_JOIN) && !(k->flags & LINK_BUS_LINK_HOLD)
              && (k->flags & (LINK_BUS_LINK_MINE | LINK_BUS_LINK_THEIRS)))
      e->state = LINK_BUS_JOINING;
   else if (q->unplaced)
      e->state = LINK_BUS_AHEAD;
   else if (q->edge_known)
   {
      e->state   = LINK_BUS_LANDED;
      e->landing = link_bus_onto(q, at, p->rate, true);
   }
   else if (link_bus_skipped(q)
         && link_bus_instant_before(p->since, p->rate, q->since, q->rate))
   {
      /* Sent from a frame begun before q's edge: on the edge before */
      e->state   = LINK_BUS_LANDED;
      e->landing = link_bus_tick(q->last_since, q->last_anchor, q->rate, at, p->rate, true);
   }
   else
      e->state = LINK_BUS_AHEAD;
   if (e->journal_gen == b->journal_gen)
      b->journal[e->journal].landing = e->landing;
   /* It lands no earlier than q's edge */
   land = link_bus_scale(at, q->rate, p->rate, true);
   if (!q->unplaced && land < q->since)
      land = q->since;
   if (     !q->live || (e->state != LINK_BUS_JOINING
            && !link_bus_carries(k, at, p->rate, land, q->rate)))
      link_bus_retire(b, e);
   else if (e->state == LINK_BUS_LANDED)
      link_bus_enqueue(q, e);
   else
      link_bus_wait(q, e);
}

/* p has been placed: what it sent while it was not goes on its way, in
 * the order it was sent */
static void link_bus_placed_sends(link_bus_t *b, struct retro_link_port *p)
{
   size_t i;
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *q    = b->recs[i];
      struct link_bus_entry *e     = q->waiting, *next, *held = NULL;
      struct link_bus_entry **tail = &held;
      if (q == p)
         continue;
      q->waiting      = NULL;
      q->waiting_tail = NULL;
      for (; e; e = next)
      {
         next = e->next;
         if (e->state == LINK_BUS_PLACING && e->msg->from == p)
         {
            e->next = NULL;
            *tail   = e;
            tail    = &e->next;
         }
         else
            link_bus_wait(q, e);
      }
      for (e = held; e; e = next)
      {
         next = e->next;
         link_bus_route(b, p, q, e, link_bus_instant(p, e->instant),
               link_bus_instant(p, e->sent_at));
      }
   }
}

/* The latest join instant of p's links to placed new peers; wait when one
 * is not known yet */
static void link_bus_latest_join(const link_bus_t *b, const struct retro_link_port *p,
      uint64_t *at, uint64_t *rate, bool *found, bool *wait)
{
   size_t i;
   for (i = 0; i < b->recs_len; i++)
   {
      const struct retro_link_port *q = b->recs[i];
      const struct link_bus_link *k   = link_bus_link(p, q);
      if (     q->console == p->console || !(k->flags & LINK_BUS_LINK_TOGETHER)
            || !(k->flags & LINK_BUS_LINK_JOIN) || !link_bus_placed(q))
         continue;
      if (k->flags & LINK_BUS_LINK_THEIRS)
         *wait = true;
      else if (!*found || link_bus_instant_before(*at, *rate, k->join, k->join_rate))
      {
         *at    = k->join;
         *rate  = k->join_rate;
         *found = true;
      }
   }
}

/* Gives each console with unplaced ports its place: the edge of a placed
 * port of its own; else, once all are known, the latest first edge at or
 * after the join among its placed new peers; with none, the latest first
 * edge of every placed console; with none, 0. A place is rounded up onto
 * the console's rate grid. A new port whose clock does not see its placed
 * console's edge as a whole tick places that console again there, as a
 * hold's end does. Every place is chosen before any is set. */
static void link_bus_place(link_bus_t *b)
{
   size_t i, j, k;
   for (i = 0; i < b->recs_len; i = j)
   {
      const struct retro_link_port *ref = NULL;
      unsigned c                        = b->recs[i]->console;
      uint64_t at                       = 0, rate = 1, grid;
      bool any = false, wait = false, found = false, again;
      for (j = i; j < b->recs_len && b->recs[j]->console == c; j++)
      {
         b->recs[j]->placing = false;
         if (b->recs[j]->unplaced)
            any = true;
         else if (!ref && link_bus_placed(b->recs[j]))
            ref = b->recs[j];
      }
      if (!any || b->consoles[c].held)
         continue;
      if (ref)
      {
         at    = ref->since;
         rate  = ref->rate;
         found = true;
      }
      for (k = i; k < j && !ref; k++)
         if (b->recs[k]->unplaced)
            link_bus_latest_join(b, b->recs[k], &at, &rate, &found, &wait);
      if (wait)
         continue;
      if (!found)
         for (k = 0; k < b->recs_len; k++)
         {
            const struct retro_link_port *q = b->recs[k];
            uint64_t e;
            if (q->console == c || !link_bus_placed(q))
               continue;
            if (!link_bus_first_edge(b, q, &e))
               e = q->since;
            if (!found || link_bus_instant_before(at, rate, e, q->rate))
            {
               at    = e;
               rate  = q->rate;
               found = true;
            }
         }
      grid  = link_bus_grid(b, c);
      at    = link_bus_scale(at, grid, rate, true);
      again = ref && link_bus_scale(at, ref->rate, grid, false) != ref->since;
      for (k = i; k < j; k++)
         if (b->recs[k]->unplaced || (again && b->recs[k]->live))
         {
            b->recs[k]->place   = link_bus_scale(at, b->recs[k]->rate, grid, false);
            b->recs[k]->placing = true;
         }
   }
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i];
      if (!p->placing)
         continue;
      p->placing = false;
      if (!p->unplaced)
         link_bus_fresh(p);
      p->since    = p->place;
      p->unplaced = false;
      link_bus_known(b, p);
      link_bus_join_known(b, p);
      link_bus_placed_sends(b, p);
      if (p->edge_known)
         link_bus_land(b, p);
   }
}

/* r's view of the port its link k leads to, at an edge of r's console: a
 * join made before now is seen, a leave once the edge is at or after it */
static unsigned link_bus_view(const link_bus_t *b, const struct retro_link_port *r,
      const struct link_bus_link *k)
{
   if (!r->live || r->cut)
      return 0;
   if (k->flags & LINK_BUS_LINK_TOGETHER)
      return LINK_BUS_LINK_VIEW;
   if (     (k->flags & LINK_BUS_LINK_VIEW) && (k->flags & LINK_BUS_LINK_CUT)
         && !b->consoles[r->console].held && !r->unplaced
         && link_bus_instant_before(r->since, r->rate, k->cut, k->cut_rate))
      return LINK_BUS_LINK_VIEW;
   return 0;
}

/* r's view has changed at its edge view_at: the messages sent to it take
 * their sender's index in the view at the instant each was sent */
static void link_bus_refrom(link_bus_t *b, struct retro_link_port *r)
{
   struct link_bus_entry *e, *next, *list = r->queue;
   bool moved = false;
   for (e = r->waiting; e; e = e->next)
      if (e->state != LINK_BUS_PLACING)
         e->from_index = link_bus_from(b, r, e->msg->from, e->sent_at, e->sent_rate);
   for (e = r->queue; e; e = e->next)
   {
      unsigned at   = link_bus_from(b, r, e->msg->from, e->sent_at, e->sent_rate);
      moved        |= at != e->from_index;
      e->from_index = at;
   }
   if (!moved)
      return;
   r->queue      = NULL;
   r->queue_tail = NULL;
   for (e = list; e; e = next)
   {
      next = e->next;
      link_bus_enqueue(r, e);
   }
}

/* Console c stands at a frame edge: each of its ports sees what has changed
 * of its bus by then, and a changed view is reported (WAKE_TOPOLOGY) */
static void link_bus_see(link_bus_t *b, unsigned c)
{
   size_t i, j;
   for (i = link_bus_at(b, c, 0); i < b->recs_len && b->recs[i]->console == c; i++)
   {
      struct retro_link_port *r = b->recs[i];
      bool moved                = false;
      for (j = 0; j < b->recs_len && !moved; j++)
      {
         const struct link_bus_link *k = link_bus_link(r, b->recs[j]);
         moved = b->recs[j] != r && ((k->flags & LINK_BUS_LINK_FLIPPED)
               || link_bus_view(b, r, k) != (k->flags & LINK_BUS_LINK_VIEW));
      }
      if (!moved)
         continue;
      r->seen = 0;
      for (j = 0; j < b->recs_len; j++)
      {
         struct link_bus_link *k = link_bus_link(r, b->recs[j]);
         unsigned view           = link_bus_view(b, r, k);
         if (b->recs[j] == r)
            continue;
         k->flags = (k->flags & ~(LINK_BUS_LINK_VIEW | LINK_BUS_LINK_WAS | LINK_BUS_LINK_FLIPPED))
            | view | ((k->flags & LINK_BUS_LINK_VIEW) ? LINK_BUS_LINK_WAS : 0);
         if (view)
            r->seen++;
      }
      r->view_at  = r->since;
      r->viewed   = true;
      r->pending |= RETRO_LINK_WAKE_TOPOLOGY;
      link_bus_refrom(b, r);
   }
}

/* Ends what no longer matters on r's link to m: a join both consoles stand
 * at or past, a cut neither sees across */
static void link_bus_settle(const link_bus_t *b, struct retro_link_port *r,
      const struct retro_link_port *m)
{
   struct link_bus_link *k = link_bus_link(r, m);
   unsigned f              = k->flags;
   if (     (f & LINK_BUS_LINK_JOIN) && !(f & (LINK_BUS_LINK_MINE | LINK_BUS_LINK_THEIRS))
         && link_bus_stands(b, r, k->join, k->join_rate)
         && link_bus_stands(b, m, k->join, k->join_rate))
   {
      f &= ~(LINK_BUS_LINK_JOIN | LINK_BUS_LINK_HOLD);
      if (f & LINK_BUS_LINK_TOGETHER)
         f &= ~(LINK_BUS_LINK_CUT | LINK_BUS_LINK_GAP);
   }
   if (     (f & LINK_BUS_LINK_CUT) && !(f & (LINK_BUS_LINK_TOGETHER | LINK_BUS_LINK_VIEW))
         && !(link_bus_link(m, r)->flags & LINK_BUS_LINK_VIEW))
      f &= ~(LINK_BUS_LINK_CUT | LINK_BUS_LINK_JOIN | LINK_BUS_LINK_MINE
            | LINK_BUS_LINK_THEIRS | LINK_BUS_LINK_HOLD);
   k->flags = f;
}

static void link_bus_settle_all(link_bus_t *b)
{
   size_t i, j;
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *r = b->recs[i];
      r->apart                  = false;
      for (j = 0; j < b->recs_len; j++)
      {
         const struct link_bus_link *k = link_bus_link(r, b->recs[j]);
         if (j == i)
            continue;
         link_bus_settle(b, r, b->recs[j]);
         if ((k->flags & LINK_BUS_LINK_CUT) && !(k->flags & LINK_BUS_LINK_TOGETHER))
            r->apart = true;
      }
   }
}

/* The buses formed again after a change made at the edge t (ticks of
 * rate); each console at an edge sees it */
static void link_bus_reform(link_bus_t *b, uint64_t t, uint64_t rate)
{
   unsigned c;
   link_bus_group(b);
   link_bus_relate(b, t, rate);
   link_bus_place(b);
   for (c = 0; c < b->console_count; c++)
      if (!b->consoles[c].in_frame)
         link_bus_see(b, c);
   link_bus_settle_all(b);
}

/* An attached port of console's that none of the entries names */
static bool link_bus_left_out(const link_bus_t *b, unsigned console,
      struct retro_link_wait *const *w, unsigned n)
{
   size_t i;
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
   {
      unsigned k = 0;
      if (!b->recs[i]->attached)
         continue;
      while (k < n && w[k]->handle != b->recs[i])
         k++;
      if (k == n)
         return true;
   }
   return false;
}

/* The ports the cable map joins to p, with the cable x-y added when x is
 * given: how many, and the lowest max_ports among them */
static unsigned link_bus_reach(link_bus_t *b, struct retro_link_port *p,
      struct retro_link_port *x, struct retro_link_port *y, unsigned *limit)
{
   size_t i;
   unsigned size = 0;
   struct retro_link_port *r;
   for (i = 0; i < b->recs_len; i++)
      b->recs[i]->uf = b->recs[i];
   for (i = 0; i < b->cables_len; i++)
      link_bus_unite(b->cables[i].a, b->cables[i].b);
   if (x)
      link_bus_unite(x, y);
   r      = link_bus_root(p);
   *limit = 0;
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *q = b->recs[i];
      if (link_bus_root(q) != r)
         continue;
      size++;
      if (q->max_ports && (!*limit || q->max_ports < *limit))
         *limit = q->max_ports;
   }
   return size;
}

static struct retro_link_port *link_bus_new_port(link_bus_t *b,
      unsigned console, unsigned port)
{
   size_t at = link_bus_at(b, console, port);
   struct retro_link_port *p;
   if (b->recs_len == b->recs_cap)
   {
      size_t cap = b->recs_cap ? b->recs_cap * 2 : 8;
      struct retro_link_port **recs = (struct retro_link_port**)
         link_bus_alloc(b, b->recs, cap * sizeof(*recs));
      if (!recs)
         return NULL;
      b->recs     = recs;
      b->recs_cap = cap;
   }
   if (!link_bus_links_room(b, b->recs_len + 1))
      return NULL;
   p = (struct retro_link_port*)link_bus_alloc(b, NULL, sizeof(*p));
   if (!p)
      return NULL;
   memset(p, 0, sizeof(*p));
   p->links = (struct link_bus_link*)link_bus_alloc(b, NULL, b->links_cap * sizeof(*p->links));
   if (!p->links)
   {
      free(p);
      return NULL;
   }
   memset(p->links, 0, b->links_cap * sizeof(*p->links));
   p->slot    = (unsigned)b->recs_len;
   p->owner   = b;
   p->console = console;
   p->port    = port;
   memmove(b->recs + at + 1, b->recs + at, (b->recs_len - at) * sizeof(*b->recs));
   b->recs[at] = p;
   b->recs_len++;
   return p;
}

static char *link_bus_put(char **s, const char *str)
{
   char *d    = *s;
   size_t len = strlen(str) + 1;
   memcpy(d, str, len);
   *s += len;
   return d;
}

link_bus_t *link_bus_new(unsigned consoles)
{
   link_bus_t *b;
   unsigned i;
   if (!consoles)
      return NULL;
   b = (link_bus_t*)calloc(1, sizeof(*b));
   if (!b)
      return NULL;
   b->consoles = (struct link_bus_console*)calloc(consoles, sizeof(*b->consoles));
   if (!b->consoles)
   {
      free(b);
      return NULL;
   }
   b->console_count = consoles;
   for (i = 0; i < LINK_BUS_CELLS; i++)
      b->pools[i].size = (size_t)64 << i;
   b->pools[LINK_BUS_POOL_MSG].size   = sizeof(struct link_bus_msg);
   b->pools[LINK_BUS_POOL_ENTRY].size = sizeof(struct link_bus_entry);
   return b;
}

void link_bus_free(link_bus_t *b)
{
   size_t i;
   unsigned c;
   if (!b)
      return;
   for (i = 0; i < b->recs_len; i++)
   {
      free(b->recs[i]->protocol);
      free(b->recs[i]->links);
      free(b->recs[i]);
   }
   for (c = 0; c < b->console_count; c++)
      free(b->consoles[c].ports);
   while (b->blocks)
   {
      struct link_bus_item *next = b->blocks->next;
      free(b->blocks);
      b->blocks = next;
   }
   free(b->journal);
   free(b->recs);
   free(b->cables);
   free(b->consoles);
   free(b);
}

bool link_bus_set_ports(link_bus_t *b, unsigned console,
      const struct retro_link_port_info *const *ports)
{
   struct retro_link_port_info *copy = NULL;
   if (!b || console >= b->console_count)
      return false;
   if (ports)
   {
      size_t n, i, bytes = 0;
      char *s;
      for (n = 0; ports[n] && ports[n]->protocol_id; n++)
         bytes += strlen(ports[n]->protocol_id) + 1
               + (ports[n]->desc ? strlen(ports[n]->desc) + 1 : 0);
      copy = (struct retro_link_port_info*)link_bus_alloc(b, NULL,
            (n + 1) * sizeof(*copy) + bytes);
      if (!copy)
         return false;
      s = (char*)(copy + n + 1);
      for (i = 0; i < n; i++)
      {
         copy[i]             = *ports[i];
         copy[i].protocol_id = link_bus_put(&s, ports[i]->protocol_id);
         copy[i].desc        = ports[i]->desc ? link_bus_put(&s, ports[i]->desc) : NULL;
      }
      memset(&copy[n], 0, sizeof(copy[n]));
   }
   free(b->consoles[console].ports);
   b->consoles[console].ports = copy;
   return true;
}

retro_link_port_t *link_bus_attach(link_bus_t *b, unsigned console, unsigned port,
      const char *protocol_id, uint64_t clock_rate)
{
   const struct retro_link_port_info *e = NULL;
   struct retro_link_port *p;
   char *protocol                       = NULL;
   if (!b || console >= b->console_count || !protocol_id || !clock_rate)
      return NULL;
   p = link_bus_find(b, console, port);
   if (p && p->attached)
      return (p->rate == clock_rate && !strcmp(p->protocol, protocol_id)) ? p : NULL;
   if (b->consoles[console].ports)
   {
      for (e = b->consoles[console].ports; e->protocol_id; e++)
         if (     e->port == port && e->clock_rate == clock_rate
               && !strcmp(e->protocol_id, protocol_id))
            break;
      if (!e->protocol_id)
         return NULL;
   }
   if (!p || strcmp(p->protocol, protocol_id))
   {
      size_t len = strlen(protocol_id) + 1;
      if (!(protocol = (char*)link_bus_alloc(b, NULL, len)))
         return NULL;
      memcpy(protocol, protocol_id, len);
   }
   if (!p && !(p = link_bus_new_port(b, console, port)))
   {
      free(protocol);
      return NULL;
   }
   if (protocol)
   {
      free(p->protocol);
      p->protocol = protocol;
   }
   p->rate      = clock_rate;
   p->flags     = e ? (e->flags & RETRO_LINK_PORT_SHARED_MEDIUM) : 0;
   p->max_ports = e ? e->max_ports : 0;
   p->idle      = e ? e->idle_ticks : 0;
   p->attached  = true;
   p->pending   = 0;
   if (b->consoles[console].in_frame)
      p->join = true;
   else
   {
      p->live     = true;
      p->unplaced = true;
      link_bus_fresh(p);
      link_bus_reform(b, 0, 1);
   }
   return p;
}

void link_bus_detach(link_bus_t *b, unsigned console, retro_link_port_t *h)
{
   struct retro_link_port *p;
   if (!b)
      return;
   if (link_bus_foreign(b, console, h))
      b->counts.misplaced_call++;
   if (!(p = link_bus_own(b, console, h)))
      return;
   p->attached = false;
   if (b->consoles[console].in_frame)
   {
      if (p->live && !p->cut)
      {
         p->cut = true;
         link_bus_changed(b, p);
      }
   }
   else
   {
      p->live = false;
      p->join = false;
      link_bus_drop(b, p);
      link_bus_reform(b, p->since, p->rate);
   }
}

int link_bus_peers(link_bus_t *b, unsigned console, retro_link_port_t *h, unsigned *count)
{
   struct retro_link_port *p = NULL;
   if (b)
   {
      if (link_bus_foreign(b, console, h))
         b->counts.misplaced_call++;
      p = link_bus_own(b, console, h);
   }
   if (!p || !link_bus_joined(p))
   {
      if (count)
         *count = 0;
      return -1;
   }
   if (count)
      *count = p->seen + 1;
   return (int)link_bus_index(b, p);
}

bool link_bus_send(link_bus_t *b, unsigned console, retro_link_port_t *h, uint64_t tick,
      unsigned to, const void *buf, size_t len)
{
   struct retro_link_port *p = NULL;
   struct link_bus_msg *m;
   unsigned cell = 0, n, index = 0;
   uint64_t at, from_at;
   size_t i;
   if (b)
      p = link_bus_call(b, console, h);
   if (!p || len > RETRO_LINK_MAX_PAYLOAD || (!buf && len))
      return false;
   if (b->released || !link_bus_joined(p))
   {
      /* Released or cabled to nothing: dropped */
      b->consoles[console].activity++;
      return true;
   }
   if (to != RETRO_LINK_BROADCAST && (to > p->seen || to == link_bus_index(b, p)))
      return false;
   n = to == RETRO_LINK_BROADCAST ? p->seen : 1;
   while (((size_t)64 << cell) < len)
      cell++;
   if (     !link_bus_reserve(b, &b->pools[cell], 1)
         || !link_bus_reserve(b, &b->pools[LINK_BUS_POOL_MSG], 1)
         || !link_bus_reserve(b, &b->pools[LINK_BUS_POOL_ENTRY], n)
         || !link_bus_journal_room(b, n))
      return false;

   if (p->promise_valid && tick < p->promise)
   {
      tick = p->promise;
      b->counts.low_stamp++;
   }
   if (!p->sent || tick > p->last_stamp)
      p->last_stamp = tick;
   p->sent = true;
   at      = link_bus_instant(p, tick);
   from_at = link_bus_instant(p, p->local);

   /* The one copy: the core reuses its buffer */
   m            = (struct link_bus_msg*)link_bus_take(&b->pools[LINK_BUS_POOL_MSG]);
   m->from      = p;
   m->cell      = (unsigned char*)link_bus_take(&b->pools[cell]);
   m->cell_pool = cell;
   m->len       = len;
   m->refs      = n;
   if (len)
      memcpy(m->cell, buf, len);

   /* To the ports in its view, a silent or held one included */
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *q    = b->recs[i];
      const struct link_bus_link *k = link_bus_link(p, q);
      struct link_bus_entry *e;
      struct link_bus_sent *j;
      if (q == p)
         index++;
      if (q == p || !(k->flags & LINK_BUS_LINK_VIEW))
         continue;
      if (to != RETRO_LINK_BROADCAST && index++ != to)
         continue;
      e              = (struct link_bus_entry*)link_bus_take(&b->pools[LINK_BUS_POOL_ENTRY]);
      e->msg         = m;
      e->landing     = RETRO_LINK_UNBOUNDED;
      e->seq         = b->sends;
      e->journal_gen = b->journal_gen;
      e->journal     = b->journal_len;
      e->seen        = false;
      j               = &b->journal[b->journal_len++];
      j->from_console = p->console;
      j->from_port    = p->port;
      j->to_console   = q->console;
      j->to_port      = q->port;
      j->stamp        = tick;
      j->landing      = e->landing;
      j->len          = len;
      memset(j->head, 0, sizeof(j->head));
      memcpy(j->head, m->cell, len < sizeof(j->head) ? len : sizeof(j->head));
      if (p->unplaced && q->live)
      {
         /* Its instants are known once p is placed */
         e->state      = LINK_BUS_PLACING;
         e->instant    = tick;
         e->rate       = p->rate;
         e->sent_at    = p->local;
         e->sent_rate  = p->rate;
         e->from_index = link_bus_index(b, p);
         link_bus_wait(q, e);
      }
      else
         link_bus_route(b, p, q, e, at, from_at);
   }
   b->sends++;
   link_bus_changed(b, p);
   b->consoles[console].activity++;
   return true;
}

bool link_bus_recv(link_bus_t *b, unsigned console, retro_link_port_t *h, uint64_t *tick,
      unsigned *from, void *buf, size_t *len)
{
   struct retro_link_port *p = NULL;
   struct link_bus_entry *e  = NULL;
   if (b && (p = link_bus_call(b, console, h)))
      e = link_bus_next(p);
   if (!e)
   {
      if (len)
         *len = 0;
      return false;
   }
   if (buf)
   {
      if (!len)
         return false;
      if (*len < e->msg->len)
      {
         *len = e->msg->len;
         return false;
      }
      memcpy(buf, e->msg->cell, e->msg->len);
   }
   if (len)
      *len = e->msg->len;
   if (tick)
      *tick = e->landing;
   if (from)
      *from = e->from_index;
   link_bus_pop(b, p);
   b->consoles[console].activity++;
   return true;
}

const void *link_bus_peek(link_bus_t *b, unsigned console, retro_link_port_t *h,
      uint64_t *tick, unsigned *from, size_t *len)
{
   struct retro_link_port *p;
   const struct link_bus_entry *e = NULL;
   if (b && (p = link_bus_call(b, console, h)))
      e = link_bus_next(p);
   if (len)
      *len = e ? e->msg->len : 0;
   if (!e)
      return NULL;
   if (tick)
      *tick = e->landing;
   if (from)
      *from = e->from_index;
   return e->msg->cell;
}

void link_bus_publish(link_bus_t *b, unsigned console, struct retro_link_wait *const *w, unsigned n)
{
   unsigned i;
   bool foreign                         = false;
   bool lowered                         = false;
   bool as[LINK_BUS_AS_EARLIEST + 1]    = { false, false, false, false };
   const struct retro_link_port *early = NULL;
   uint64_t early_at                    = 0;
   if (!b)
      return;
   if (!link_bus_in_frame(b, console))
   {
      b->counts.misplaced_call++;
      return;
   }

   for (i = 0; i < n; i++)
   {
      struct retro_link_port *p = link_bus_own(b, console, w[i]->handle);
      bool moved                = false;
      if (link_bus_foreign(b, console, w[i]->handle))
         foreign = true;
      if (!p || !p->live || p->cut)
         continue;
      if (!p->published)
      {
         /* The frame-start advance gives the tick at the edge if none is known */
         p->published   = true;
         p->start_local = w[i]->local_tick;
         if (!p->edge_known)
         {
            if (p->renumber)
               link_bus_rebase(p, w[i]->local_tick);
            p->anchor     = w[i]->local_tick;
            p->edge_known = true;
            moved         = true;
            if (!p->unplaced)
               link_bus_land(b, p);
         }
      }
      if (p->promise_valid && w[i]->safe_tick < p->promise)
         lowered = true;
      else if (!p->promise_valid || w[i]->safe_tick > p->promise)
      {
         p->promise       = w[i]->safe_tick;
         p->promise_valid = true;
         moved            = true;
      }
      if (p->local != w[i]->local_tick || p->request != w[i]->request_tick)
      {
         p->local   = w[i]->local_tick;
         p->request = w[i]->request_tick;
         moved      = true;
      }
      if (moved)
         link_bus_changed(b, p);
   }

   /* The earliest promise among the entries not granted RETRO_LINK_UNBOUNDED */
   for (i = 0; i < n; i++)
   {
      const struct retro_link_port *p = link_bus_own(b, console, w[i]->handle);
      uint64_t at;
      if (!p || !link_bus_joined(p))
         continue;
      at = link_bus_instant(p, p->promise);
      if (!early || link_bus_instant_before(at, p->rate, early_at, early->rate))
      {
         early    = p;
         early_at = at;
      }
   }

   /* A request past a bound is granted as the lowest bound it passed */
   for (i = 0; i < n; i++)
   {
      struct retro_link_port *p = link_bus_own(b, console, w[i]->handle);
      enum link_bus_repair was   = LINK_BUS_AS_REQUEST;
      uint64_t end, earliest;
      if (!p || !p->live || p->cut)
         continue;
      p->target = p->request;
      if (!link_bus_joined(p) || p->target <= link_bus_floor(p))
         continue;
      end      = link_bus_add(p->anchor, p->span);
      earliest = link_bus_onto(p, early_at, early->rate, false);
      if (p->target > p->promise)
      {
         p->target = p->promise;
         was       = LINK_BUS_AS_PROMISE;
      }
      if (p->span_valid && p->target > end)
      {
         p->target = end;
         was       = LINK_BUS_AS_FRAME_END;
      }
      if (p->target > earliest)
      {
         p->target = earliest;
         was       = LINK_BUS_AS_EARLIEST;
      }
      if (p->target > LINK_BUS_TICK_MAX)
         p->target = LINK_BUS_TICK_MAX;
      as[was] = true;
   }

   if (foreign)
      b->counts.misplaced_call++;
   if (link_bus_left_out(b, console, w, n))
      b->counts.left_out++;
   if (lowered)
      b->counts.lowered_promise++;
   if (as[LINK_BUS_AS_PROMISE])
      b->counts.request_past_promise++;
   if (as[LINK_BUS_AS_FRAME_END])
      b->counts.request_past_frame_end++;
   if (as[LINK_BUS_AS_EARLIEST])
      b->counts.request_past_earliest++;
}

bool link_bus_grantable(const link_bus_t *b, unsigned console,
      struct retro_link_wait *const *w, unsigned n)
{
   return !link_bus_blocked_by(b, console, w, n, NULL);
}

void link_bus_grant(link_bus_t *b, unsigned console, struct retro_link_wait *const *w, unsigned n)
{
   unsigned i;
   if (!b)
      return;
   for (i = 0; i < n; i++)
   {
      struct retro_link_port *p = link_bus_framed(b, console, w[i]->handle);
      bool ask                  = w[i]->wake_flags != RETRO_LINK_WAKE_NONE;
      w[i]->wake_flags          = RETRO_LINK_WAKE_NONE;
      if (!p || b->released)
      {
         w[i]->grant = RETRO_LINK_UNBOUNDED;
         if (ask)
            w[i]->wake_flags = RETRO_LINK_WAKE_DETACHED;
         if (p)
         {
            p->grant_valid = false;
            p->limit       = RETRO_LINK_UNBOUNDED;
         }
         continue;
      }
      if (link_bus_joined(p))
      {
         uint64_t g = p->target;
         if (g < link_bus_floor(p))
            g = link_bus_floor(p);
         if (g > LINK_BUS_TICK_MAX)
            g = LINK_BUS_TICK_MAX;
         p->grant       = g;
         p->grant_valid = true;
         w[i]->grant    = g;
      }
      else
      {
         p->grant_valid = false;
         w[i]->grant    = RETRO_LINK_UNBOUNDED;
      }
      p->limit = w[i]->grant;
      if (!p->cut && link_bus_reached(p))
         p->pending |= RETRO_LINK_WAKE_MESSAGE;
      if (ask)
      {
         w[i]->wake_flags = p->pending;
         p->pending       = RETRO_LINK_WAKE_NONE;
      }
   }
}

bool link_bus_blocked_by(const link_bus_t *b, unsigned console,
      struct retro_link_wait *const *w, unsigned n, struct link_bus_blocker *out)
{
   unsigned i;
   if (!b || b->released)
      return false;
   for (i = 0; i < n; i++)
   {
      const struct retro_link_port *p  = link_bus_framed(b, console, w[i]->handle);
      const struct retro_link_port *by = NULL;
      uint64_t bound;
      size_t k;
      if (!p || !link_bus_joined(p) || p->target <= link_bus_floor(p))
         continue;
      if (p->unplaced)
      {
         /* Not placed: held where it stands until the peer it waits for
          * reaches its edge */
         bound = link_bus_floor(p);
         by    = p;
         for (k = 0; k < b->recs_len; k++)
            if (link_bus_link(p, b->recs[k])->flags & LINK_BUS_LINK_THEIRS)
            {
               by = b->recs[k];
               break;
            }
      }
      else if (p->target <= (bound = link_bus_bound(p, &by)))
         continue;
      if (out)
      {
         out->entry        = i;
         out->port         = p->port;
         out->request      = p->target;
         out->bound        = bound;
         out->peer_console = by->console;
         out->peer_port    = by->port;
         out->edge_unknown = !by->edge_known;
      }
      return true;
   }
   return false;
}

uint64_t link_bus_changes(const link_bus_t *b, unsigned console)
{
   size_t i;
   uint64_t seq = 0;
   if (!b)
      return 0;
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
   {
      const struct retro_link_port *head = b->recs[i]->head;
      if (head && head->seq > seq)
         seq = head->seq;
   }
   return seq;
}

uint64_t link_bus_activity(const link_bus_t *b, unsigned console)
{
   if (!b || console >= b->console_count)
      return 0;
   return b->consoles[console].activity;
}

void link_bus_frame_start(link_bus_t *b, unsigned console)
{
   size_t i;
   if (!b || console >= b->console_count)
      return;
   b->consoles[console].in_frame = true;
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
   {
      if (b->recs[i]->stale && !b->consoles[console].held)
         link_bus_rejoin(b->recs[i]);
      b->recs[i]->published = false;
   }
}

void link_bus_journal_reset(link_bus_t *b)
{
   if (!b)
      return;
   b->journal_len = 0;
   b->journal_gen++;
}

void link_bus_frame_end(link_bus_t *b, unsigned console)
{
   size_t i, first;
   bool skipped = false;
   if (!b || console >= b->console_count)
      return;
   b->consoles[console].in_frame = false;
   first                         = link_bus_at(b, console, 0);
   for (i = first; i < b->recs_len && b->recs[i]->console == console; i++)
   {
      struct retro_link_port *p = b->recs[i];
      bool known;
      if (!p->live)
         continue;
      if (p->cut)
      {
         /* A silent member until the step's end: it keeps its console's edge */
         if (p->span_shown)
            p->since = link_bus_add(p->since, p->span);
         continue;
      }
      /* The frame-end publish is the latest, asking for nothing, past the
       * frame-start advance. Without it the new edge's tick is not known,
       * and its instant follows by the last span shown since the port was
       * placed, if any. */
      known = p->published && p->local == p->request && p->local > p->start_local;
      if (known)
      {
         p->span       = p->local - p->start_local;
         p->span_shown = true;
      }
      else
      {
         p->last_anchor = p->anchor;
         p->last_since  = p->since;
      }
      if (p->span_shown)
         p->since = link_bus_add(p->since, p->span);
      p->anchor     = p->local;
      p->edge_known = known;
      p->span_valid = known;
      if (!known)
         skipped = true;
   }
   if (skipped)
      b->counts.skipped_publish++;

   /* The joins that waited for this edge are known, and each of its ports
    * sees what has changed of its bus by now */
   for (i = first; i < b->recs_len && b->recs[i]->console == console; i++)
   {
      if (!b->recs[i]->unplaced)
         link_bus_known(b, b->recs[i]);
      link_bus_join_known(b, b->recs[i]);
   }
   link_bus_place(b);
   link_bus_see(b, console);
   link_bus_settle_all(b);
}

/* A console on one of console's buses has an unknown first edge */
static bool link_bus_meets_unsized(const link_bus_t *b, unsigned console)
{
   size_t i;
   const struct retro_link_port *m;
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
      for (m = b->recs[i]->head; m; m = m->next)
         if (link_bus_unsized(b, m->console))
            return true;
   return false;
}

/* A re-join, placed as a plug places and joined as a plug joins: each port's
 * next frame-start advance gives its tick */
static void link_bus_end_hold(link_bus_t *b, unsigned console)
{
   size_t i, first;
   struct retro_link_port *m;
   b->consoles[console].held      = false;
   b->consoles[console].unholding = false;
   first                          = link_bus_at(b, console, 0);
   for (i = first; i < b->recs_len && b->recs[i]->console == console; i++)
   {
      if (b->recs[i]->stale)
         link_bus_rejoin(b->recs[i]);
      link_bus_fresh(b->recs[i]);
      b->recs[i]->unplaced = b->recs[i]->live;
   }
   for (i = first; i < b->recs_len && b->recs[i]->console == console; i++)
      for (m = b->recs[i]->head; m; m = m->next)
         if (m != b->recs[i])
            link_bus_came_on(b, b->recs[i], m, LINK_BUS_LINK_HOLD);
   link_bus_place(b);
   link_bus_settle_all(b);
}

void link_bus_step_end(link_bus_t *b)
{
   size_t i;
   bool reform = false, any = false;
   uint64_t t  = 0, rate = 1;
   if (!b)
      return;
   reform = b->held_back;
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i];
      if (b->consoles[p->console].in_frame)
         continue;
      if (p->cut)
      {
         struct retro_link_port *m;
         /* It leaves at its console's edge, off its bus before any re-join
          * so that both are seen */
         for (m = p->head; m; m = m->next)
            if (m != p && (link_bus_link(p, m)->flags & LINK_BUS_LINK_TOGETHER))
            {
               link_bus_off(p, m, p->since, p->rate);
               link_bus_off(m, p, p->since, p->rate);
            }
         if (!any)
         {
            t    = p->since;
            rate = p->rate;
            any  = true;
         }
         p->cut  = false;
         p->live = false;
         reform  = true;
         link_bus_drop(b, p);
      }
      if (p->join)
      {
         p->join = false;
         if (p->attached)
         {
            p->live     = true;
            p->unplaced = true;
            link_bus_fresh(p);
            reform      = true;
         }
      }
   }
   if (reform)
      link_bus_reform(b, t, rate);
   for (i = 0; i < b->console_count; i++)
      if (b->consoles[i].unholding && !link_bus_meets_unsized(b, (unsigned)i))
         link_bus_end_hold(b, (unsigned)i);
}

bool link_bus_console_edge(const link_bus_t *b, unsigned console,
      uint64_t *since, uint64_t *span, uint64_t *rate)
{
   const struct retro_link_port *at = NULL;
   size_t i;
   if (!b || console >= b->console_count || b->consoles[console].held)
      return false;
   /* Its first placed port, one that showed its span if any did */
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
      if (link_bus_placed(b->recs[i]) && (!at || (!at->span_shown && b->recs[i]->span_shown)))
         at = b->recs[i];
   if (!at)
      return false;
   if (since)
      *since = at->since;
   if (span)
      *span = at->span_shown ? at->span : 0;
   if (rate)
      *rate = at->rate;
   return true;
}

bool link_bus_cable(link_bus_t *b, unsigned console_a, unsigned port_a,
      unsigned console_b, unsigned port_b)
{
   struct retro_link_port *x, *y;
   unsigned size, limit;
   size_t i;
   if (!b)
      return false;
   x = link_bus_find(b, console_a, port_a);
   y = link_bus_find(b, console_b, port_b);
   if (     !x || !y || x == y || link_bus_shared(x) || link_bus_shared(y)
         || strcmp(x->protocol, y->protocol))
      return false;
   size = link_bus_reach(b, x, x, y, &limit);
   if (size > LINK_BUS_MAX_BUS_PORTS || (limit && size > limit))
      return false;
   for (i = 0; i < b->cables_len; i++)
      if (     (b->cables[i].a == x && b->cables[i].b == y)
            || (b->cables[i].a == y && b->cables[i].b == x))
         return true;
   if (b->cables_len == b->cables_cap)
   {
      size_t cap                    = b->cables_cap ? b->cables_cap * 2 : 8;
      struct link_bus_cable *cables = (struct link_bus_cable*)
         link_bus_alloc(b, b->cables, cap * sizeof(*cables));
      if (!cables)
         return false;
      b->cables     = cables;
      b->cables_cap = cap;
   }
   b->cables[b->cables_len].a = x;
   b->cables[b->cables_len].b = y;
   b->cables_len++;
   link_bus_reform(b, 0, 1);
   return true;
}

bool link_bus_uncable(link_bus_t *b, unsigned console, unsigned port)
{
   struct retro_link_port *p;
   const struct retro_link_port *q;
   size_t i, kept = 0;
   if (     !b || !(p = link_bus_find(b, console, port)) || link_bus_shared(p)
         || b->consoles[console].in_frame)
      return false;
   /* Pulled at its edge t, while no running peer has passed its first edge
    * at or after t */
   for (q = p->head; q; q = q->next)
      if (     q != p && b->consoles[q->console].in_frame && !b->consoles[q->console].held
            && !link_bus_instant_before(q->since, q->rate, p->since, p->rate))
         return false;
   for (i = 0; i < b->cables_len; i++)
      if (b->cables[i].a != p && b->cables[i].b != p)
         b->cables[kept++] = b->cables[i];
   if (kept != b->cables_len)
   {
      b->cables_len = kept;
      link_bus_reform(b, p->since, p->rate);
   }
   return true;
}

bool link_bus_hold(link_bus_t *b, unsigned console)
{
   size_t i;
   if (!b || console >= b->console_count || b->consoles[console].in_frame)
      return false;
   /* Every bus of its ports idle: nothing undelivered, every port quiet */
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
   {
      const struct retro_link_port *q;
      for (q = b->recs[i]->head; q; q = q->next)
         if (q->queue || q->waiting || !link_bus_quiet(q))
            return false;
   }
   b->consoles[console].held      = true;
   b->consoles[console].unholding = false;
   return true;
}

bool link_bus_unhold(link_bus_t *b, unsigned console)
{
   if (!b || console >= b->console_count)
      return false;
   /* It waits as a join does */
   if (b->consoles[console].held && link_bus_meets_unsized(b, console))
      b->consoles[console].unholding = true;
   else if (b->consoles[console].held)
      link_bus_end_hold(b, console);
   return true;
}

bool link_bus_is_held(const link_bus_t *b, unsigned console)
{
   return b && console < b->console_count && b->consoles[console].held;
}

void link_bus_renumbered(link_bus_t *b, unsigned console)
{
   size_t i;
   if (!b || console >= b->console_count || b->consoles[console].in_frame)
      return;
   /* A re-join when the next frame opens, or at the unhold of a held
    * console: a hold or a save before then finds the session as it was */
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
      b->recs[i]->stale = true;
}

void link_bus_console_gone(link_bus_t *b, unsigned console)
{
   size_t i;
   if (!b || console >= b->console_count)
      return;
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
      if (b->recs[i]->attached)
         link_bus_detach(b, console, b->recs[i]);
}

void link_bus_release(link_bus_t *b)
{
   if (b)
      b->released = true;
}

bool link_bus_released(const link_bus_t *b)
{
   return b && b->released;
}

struct link_bus_out   /* a blob being written, measured or hashed */
{
   unsigned char *buf; /* NULL: measured only */
   size_t len;
   uint64_t hash;
};

/* Appends v as bytes; an absolute anchor or a count is not hashed */
static void link_bus_put_u(struct link_bus_out *o, uint64_t v, unsigned bytes, bool hashed)
{
   unsigned i;
   for (i = 0; i < bytes; i++, v >>= 8)
   {
      unsigned char c = (unsigned char)(v & 0xFF);
      if (o->buf)
         o->buf[o->len] = c;
      if (hashed)
         o->hash = (o->hash ^ c) * LINK_BUS_FNV_PRIME;
      o->len++;
   }
}

static void link_bus_put_bytes(struct link_bus_out *o, const void *s, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
      link_bus_put_u(o, ((const unsigned char*)s)[i], 1, true);
}

/* A walk of a port's messages in an order the interleaving of its senders
 * cannot change: by index at send, queued before waiting, then list order.
 * rank counts the messages before e with its index at send. */
struct link_bus_order
{
   const struct link_bus_entry *e; /* NULL before the first step */
   uint64_t rank;
   unsigned at;                    /* e's index at send */
   bool waiting;                   /* e is on the waiting list */
};

/* The lowest index at send of p's messages from index from on */
static bool link_bus_lowest(const struct retro_link_port *p, unsigned from, unsigned *at)
{
   const struct link_bus_entry *e;
   unsigned list;
   bool found = false;
   for (list = 0; list < 2; list++)
      for (e = list ? p->waiting : p->queue; e; e = e->next)
         if (e->from_index >= from && (!found || e->from_index < *at))
         {
            *at   = e->from_index;
            found = true;
         }
   return found;
}

/* Steps o to p's next message; false after the last */
static bool link_bus_step(const struct retro_link_port *p, struct link_bus_order *o)
{
   const struct link_bus_entry *e = o->e ? o->e->next : NULL;
   if (o->e)
      o->rank++;
   else
   {
      if (!link_bus_lowest(p, 0, &o->at))
         return false;
      o->rank    = 0;
      o->waiting = false;
      e          = p->queue;
   }
   for (;;)
   {
      while (e && e->from_index != o->at)
         e = e->next;
      if (e)
      {
         o->e = e;
         return true;
      }
      if (!o->waiting)
      {
         o->waiting = true;
         e          = p->waiting;
      }
      else if (link_bus_lowest(p, o->at + 1, &o->at))
      {
         o->rank    = 0;
         o->waiting = false;
         e          = p->queue;
      }
      else
         return false;
   }
}

static void link_bus_walk_entry(const link_bus_t *b, const struct retro_link_port *p,
      const struct link_bus_entry *e, uint64_t rank, struct link_bus_out *o)
{
   const struct link_bus_msg *m = e->msg;
   link_bus_put_u(o, e->state, 1, true);
   link_bus_put_u(o, e->seen, 1, true);
   link_bus_put_u(o, link_bus_at(b, m->from->console, m->from->port), 4, true);
   link_bus_put_u(o, e->from_index, 1, true);
   link_bus_put_u(o, rank, 8, true);
   link_bus_put_u(o, e->state == LINK_BUS_LANDED ? e->landing - p->anchor : 0, 8, true);
   link_bus_put_u(o, e->state >= LINK_BUS_AHEAD && e->state != LINK_BUS_REJOIN ? e->instant : 0, 8, true);
   link_bus_put_u(o, e->state >= LINK_BUS_AHEAD && e->state != LINK_BUS_REJOIN ? e->rate : 0, 8, true);
   link_bus_put_u(o, e->sent_at, 8, true);
   link_bus_put_u(o, e->sent_rate, 8, true);
   link_bus_put_u(o, m->len, 4, true);
   link_bus_put_bytes(o, m->cell, m->len);
}

static void link_bus_walk_port(const link_bus_t *b, const struct retro_link_port *p,
      struct link_bus_out *o)
{
   const struct link_bus_entry *e;
   struct link_bus_order order;
   size_t n        = 0;
   size_t plen     = strlen(p->protocol);
   unsigned limit  = p->limit == RETRO_LINK_UNBOUNDED ? LINK_BUS_LIMIT_UNBOUNDED
      : p->grant_valid ? LINK_BUS_LIMIT_GRANT : LINK_BUS_LIMIT_NONE;
   unsigned bits   = (p->attached      ? LINK_BUS_REC_ATTACHED : 0)
                   | (p->edge_known    ? LINK_BUS_REC_EDGE     : 0)
                   | (p->promise_valid ? LINK_BUS_REC_PROMISE  : 0)
                   | (p->span_valid    ? LINK_BUS_REC_SPAN     : 0)
                   | (p->sent          ? LINK_BUS_REC_SENT     : 0)
                   | (p->renumber      ? LINK_BUS_REC_RENUMBER : 0)
                   | (p->span_shown    ? LINK_BUS_REC_SHOWN    : 0)
                   | (p->unplaced      ? LINK_BUS_REC_UNPLACED : 0)
                   | (p->viewed        ? LINK_BUS_REC_VIEWED   : 0)
                   | (p->cut           ? LINK_BUS_REC_CUT      : 0)
                   | (p->join          ? LINK_BUS_REC_JOIN     : 0);
   size_t i, links = 0;
   for (e = p->queue; e; e = e->next)
      n++;
   for (e = p->waiting; e; e = e->next)
      n++;
   for (i = 0; i < b->recs_len; i++)
      if (link_bus_link(p, b->recs[i])->flags)
         links++;
   link_bus_put_u(o, bits, 2, true);
   link_bus_put_u(o, limit, 1, true);
   link_bus_put_u(o, p->flags, 4, true);
   link_bus_put_u(o, p->max_ports, 4, true);
   link_bus_put_u(o, p->pending, 4, true);
   link_bus_put_u(o, p->rate, 8, true);
   link_bus_put_u(o, p->idle, 8, true);
   link_bus_put_u(o, p->anchor, 8, false);
   link_bus_put_u(o, p->since, 8, true);
   link_bus_put_u(o, link_bus_skipped(p) ? p->last_since : 0, 8, true);
   link_bus_put_u(o, link_bus_skipped(p) ? p->last_anchor - p->anchor : 0, 8, true);
   link_bus_put_u(o, p->promise_valid ? p->promise - p->anchor : 0, 8, true);
   link_bus_put_u(o, limit == LINK_BUS_LIMIT_GRANT ? p->limit - p->anchor : 0, 8, true);
   link_bus_put_u(o, p->span_shown ? p->span : 0, 8, true);
   link_bus_put_u(o, p->sent ? p->last_stamp - p->anchor : 0, 8, true);
   link_bus_put_u(o, p->viewed ? p->view_at : 0, 8, true);
   link_bus_put_u(o, plen, 4, true);
   link_bus_put_bytes(o, p->protocol, plen);
   link_bus_put_u(o, links, 4, true);
   for (i = 0; i < b->recs_len; i++)
   {
      const struct link_bus_link *k = link_bus_link(p, b->recs[i]);
      if (!k->flags)
         continue;
      link_bus_put_u(o, i, 4, true);
      link_bus_put_u(o, k->flags, 2, true);
      link_bus_put_u(o, k->join, 8, true);
      link_bus_put_u(o, k->join_rate, 8, true);
      link_bus_put_u(o, k->cut, 8, true);
      link_bus_put_u(o, k->cut_rate, 8, true);
   }
   link_bus_put_u(o, n, 4, true);
   order.e = NULL;
   while (link_bus_step(p, &order))
      link_bus_walk_entry(b, p, order.e, order.rank, o);
}

/* The blob: header, counts, holds, the records' consoles and ports, the
 * cables between them, then each record with its queue */
static void link_bus_walk(const link_bus_t *b, struct link_bus_out *o)
{
   size_t i;
   unsigned c;
   o->len  = 0;
   o->hash = LINK_BUS_FNV_BASIS;
   link_bus_put_bytes(o, LINK_BUS_MAGIC, 4);
   link_bus_put_u(o, LINK_BUS_VERSION, 4, true);
   link_bus_put_u(o, b->console_count, 4, true);
   link_bus_put_u(o, b->recs_len, 4, true);
   link_bus_put_u(o, b->cables_len, 4, true);
   for (i = 0; i < LINK_BUS_SAVED_COUNTS; i++)
      link_bus_put_u(o, *(const uint64_t*)((const char*)&b->counts
               + link_bus_saved_counts[i]), 8, false);
   for (c = 0; c < b->console_count; c++)
      link_bus_put_u(o, b->consoles[c].held, 1, true);
   for (i = 0; i < b->recs_len; i++)
   {
      link_bus_put_u(o, b->recs[i]->console, 4, true);
      link_bus_put_u(o, b->recs[i]->port, 4, true);
   }
   for (i = 0; i < b->cables_len; i++)
   {
      const struct retro_link_port *x = b->cables[i].a, *y = b->cables[i].b;
      link_bus_put_u(o, link_bus_at(b, x->console, x->port), 4, true);
      link_bus_put_u(o, link_bus_at(b, y->console, y->port), 4, true);
   }
   for (i = 0; i < b->recs_len; i++)
      link_bus_walk_port(b, b->recs[i], o);
}

size_t link_bus_save_size(const link_bus_t *b)
{
   struct link_bus_out o;
   if (!b)
      return 0;
   o.buf = NULL;
   link_bus_walk(b, &o);
   return o.len;
}

bool link_bus_save(const link_bus_t *b, void *buf, size_t len)
{
   struct link_bus_out o;
   if (!b || !buf || link_bus_any_in_frame(b) || len < link_bus_save_size(b))
      return false;
   o.buf = (unsigned char*)buf;
   link_bus_walk(b, &o);
   return true;
}

uint64_t link_bus_hash(const link_bus_t *b)
{
   struct link_bus_out o;
   if (!b)
      return 0;
   o.buf = NULL;
   link_bus_walk(b, &o);
   return o.hash;
}

struct link_bus_in    /* a blob being read by link_bus_load */
{
   const unsigned char *at, *end;
   const unsigned char *table; /* the records' consoles and ports, 8 bytes each */
   size_t recs;
   size_t need[LINK_BUS_POOLS]; /* CHECK: pool items its queues take */
   enum link_bus_pass pass;
   bool bad;                    /* it ended early */
};

static uint64_t link_bus_get_u(struct link_bus_in *in, unsigned bytes)
{
   uint64_t v = 0;
   unsigned i;
   if ((size_t)(in->end - in->at) < bytes)
   {
      in->bad = true;
      in->at  = in->end;
      return 0;
   }
   for (i = 0; i < bytes; i++)
      v |= (uint64_t)in->at[i] << (8 * i);
   in->at += bytes;
   return v;
}

/* n bytes in place, or NULL past the end */
static const unsigned char *link_bus_get_bytes(struct link_bus_in *in, size_t n)
{
   const unsigned char *s = in->at;
   if ((size_t)(in->end - in->at) < n)
   {
      in->bad = true;
      in->at  = in->end;
      return NULL;
   }
   in->at += n;
   return s;
}

/* Console and port of the blob's record k */
static void link_bus_row(const struct link_bus_in *in, size_t k,
      unsigned *console, unsigned *port)
{
   const unsigned char *s = in->table + k * 8;
   *console = (unsigned)((uint32_t)s[0] | ((uint32_t)s[1] << 8)
         | ((uint32_t)s[2] << 16) | ((uint32_t)s[3] << 24));
   *port    = (unsigned)((uint32_t)s[4] | ((uint32_t)s[5] << 8)
         | ((uint32_t)s[6] << 16) | ((uint32_t)s[7] << 24));
}

static struct retro_link_port *link_bus_listed(const link_bus_t *b,
      const struct link_bus_in *in, size_t k)
{
   unsigned console, port;
   link_bus_row(in, k, &console, &port);
   return link_bus_find(b, console, port);
}

/* A heap sort: n log n whatever the order of k */
static void link_bus_sort(uint64_t *k, size_t n)
{
   size_t i = n / 2, end = n;
   for (;;)
   {
      size_t at, child;
      uint64_t v;
      if (i)
         i--;
      else if (--end)
      {
         v      = k[end];
         k[end] = k[0];
         k[0]   = v;
      }
      else
         return;
      at = i;
      v  = k[at];
      while ((child = 2 * at + 1) < end)
      {
         if (child + 1 < end && k[child + 1] > k[child])
            child++;
         if (k[child] <= v)
            break;
         k[at] = k[child];
         at    = child;
      }
      k[at] = v;
   }
}

/* Whether no two of the n cables at s join the same two records; sorts
 * them in a block of 8 bytes a cable, freed here */
static bool link_bus_distinct(link_bus_t *b, const unsigned char *s, size_t n)
{
   struct link_bus_in t;
   uint64_t *keys;
   size_t i;
   bool distinct = true;
   if (n < 2)
      return true;
   if (!(keys = (uint64_t*)link_bus_alloc(b, NULL, n * sizeof(*keys))))
      return false;
   t.at  = s;
   t.end = s + n * 8;
   t.bad = false;
   for (i = 0; i < n; i++)
   {
      uint64_t x = link_bus_get_u(&t, 4);
      uint64_t y = link_bus_get_u(&t, 4);
      keys[i]    = x < y ? (x << 32) | y : (y << 32) | x;
   }
   link_bus_sort(keys, n);
   for (i = 1; i < n && distinct; i++)
      distinct = keys[i] != keys[i - 1];
   free(keys);
   return distinct;
}

/* One of the n messages of p in the blob */
static bool link_bus_read_entry(link_bus_t *b, struct link_bus_in *in,
      struct retro_link_port *p, size_t n)
{
   unsigned state  = (unsigned)link_bus_get_u(in, 1);
   unsigned seen   = (unsigned)link_bus_get_u(in, 1);
   size_t from     = (size_t)link_bus_get_u(in, 4);
   unsigned index  = (unsigned)link_bus_get_u(in, 1);
   uint64_t rank   = link_bus_get_u(in, 8);
   uint64_t landed = link_bus_get_u(in, 8);
   uint64_t at     = link_bus_get_u(in, 8);
   uint64_t rate   = link_bus_get_u(in, 8);
   uint64_t sent   = link_bus_get_u(in, 8);
   uint64_t srate  = link_bus_get_u(in, 8);
   size_t len      = (size_t)link_bus_get_u(in, 4);
   const unsigned char *payload = link_bus_get_bytes(in, len);
   unsigned cell   = 0;
   if (     in->bad || state > LINK_BUS_PLACING || seen > 1 || from >= in->recs || rank >= n
         || index >= LINK_BUS_MAX_BUS_PORTS || len > RETRO_LINK_MAX_PAYLOAD
         || (state >= LINK_BUS_AHEAD && state != LINK_BUS_REJOIN && !rate))
      return false;
   while (((size_t)64 << cell) < len)
      cell++;
   if (in->pass == LINK_BUS_CHECK)
   {
      in->need[cell]++;
      in->need[LINK_BUS_POOL_MSG]++;
      in->need[LINK_BUS_POOL_ENTRY]++;
   }
   else if (in->pass == LINK_BUS_APPLY)
   {
      struct link_bus_msg *m   = (struct link_bus_msg*)link_bus_take(&b->pools[LINK_BUS_POOL_MSG]);
      struct link_bus_entry *e = (struct link_bus_entry*)link_bus_take(&b->pools[LINK_BUS_POOL_ENTRY]);
      m->from        = link_bus_listed(b, in, from);
      m->cell        = (unsigned char*)link_bus_take(&b->pools[cell]);
      m->cell_pool   = cell;
      m->len         = len;
      m->refs        = 1;
      if (len)
         memcpy(m->cell, payload, len);
      e->msg         = m;
      e->state       = (unsigned char)state;
      e->seen        = seen != 0;
      e->from_index  = index;
      e->seq         = rank;
      e->landing     = state == LINK_BUS_LANDED ? p->anchor + landed : RETRO_LINK_UNBOUNDED;
      e->instant     = at;
      e->rate        = rate;
      e->sent_at     = sent;
      e->sent_rate   = srate;
      e->journal_gen = b->journal_gen;
      e->journal     = 0;
      if (state == LINK_BUS_LANDED)
         link_bus_enqueue(p, e);
      else
         link_bus_wait(p, e);
   }
   return true;
}

static bool link_bus_read_port(link_bus_t *b, struct link_bus_in *in, size_t k)
{
   struct retro_link_port *p = NULL;
   unsigned bits      = (unsigned)link_bus_get_u(in, 2);
   unsigned limit     = (unsigned)link_bus_get_u(in, 1);
   unsigned flags     = (unsigned)link_bus_get_u(in, 4);
   unsigned max_ports = (unsigned)link_bus_get_u(in, 4);
   uint32_t pending   = (uint32_t)link_bus_get_u(in, 4);
   uint64_t rate      = link_bus_get_u(in, 8);
   uint64_t idle      = link_bus_get_u(in, 8);
   uint64_t anchor    = link_bus_get_u(in, 8);
   uint64_t since     = link_bus_get_u(in, 8);
   uint64_t last_at   = link_bus_get_u(in, 8);
   uint64_t last_tick = link_bus_get_u(in, 8);
   uint64_t promise   = link_bus_get_u(in, 8);
   uint64_t grant     = link_bus_get_u(in, 8);
   uint64_t span      = link_bus_get_u(in, 8);
   uint64_t stamp     = link_bus_get_u(in, 8);
   uint64_t view_at   = link_bus_get_u(in, 8);
   size_t plen        = (size_t)link_bus_get_u(in, 4);
   const unsigned char *protocol = link_bus_get_bytes(in, plen);
   size_t links       = (size_t)link_bus_get_u(in, 4);
   const unsigned char *link_at  = in->at;
   size_t n, i;
   if (in->bad || links > in->recs)
      return false;
   for (i = 0; i < links; i++)
   {
      size_t other    = (size_t)link_bus_get_u(in, 4);
      unsigned flags  = (unsigned)link_bus_get_u(in, 2);
      uint64_t join_r, cut_r;
      link_bus_get_u(in, 8);
      join_r = link_bus_get_u(in, 8);
      link_bus_get_u(in, 8);
      cut_r  = link_bus_get_u(in, 8);
      if (     in->bad || other >= in->recs || other == k || !flags || flags > LINK_BUS_LINK_BITS
            || ((flags & LINK_BUS_LINK_JOIN) && !join_r) || ((flags & LINK_BUS_LINK_CUT) && !cut_r))
         return false;
   }
   n = (size_t)link_bus_get_u(in, 4);
   if (     in->bad || bits > LINK_BUS_REC_BITS || limit > LINK_BUS_LIMIT_GRANT || !rate
         || memchr(protocol, '\0', plen))
      return false;
   if (in->pass == LINK_BUS_MAKE)
   {
      unsigned console, port;
      link_bus_row(in, k, &console, &port);
      p = link_bus_find(b, console, port);
      if (!p || strlen(p->protocol) != plen || memcmp(p->protocol, protocol, plen))
      {
         char *s = (char*)link_bus_alloc(b, NULL, plen + 1);
         if (!s)
            return false;
         memcpy(s, protocol, plen);
         s[plen] = '\0';
         if (p)
            p->loaded = s;
         else
         {
            if (!(p = link_bus_new_port(b, console, port)))
            {
               free(s);
               return false;
            }
            p->rate     = rate;
            p->protocol = s;
         }
      }
   }
   else if (in->pass == LINK_BUS_APPLY)
   {
      p                = link_bus_listed(b, in, k);
      if (p->loaded)
      {
         free(p->protocol);
         p->protocol = p->loaded;
         p->loaded   = NULL;
      }
      p->attached      = (bits & LINK_BUS_REC_ATTACHED) != 0;
      p->cut           = (bits & LINK_BUS_REC_CUT) != 0;
      p->join          = (bits & LINK_BUS_REC_JOIN) != 0;
      p->live          = (p->attached && !p->join) || p->cut;
      p->edge_known    = (bits & LINK_BUS_REC_EDGE) != 0;
      p->promise_valid = (bits & LINK_BUS_REC_PROMISE) != 0;
      p->span_valid    = (bits & LINK_BUS_REC_SPAN) != 0;
      p->sent          = (bits & LINK_BUS_REC_SENT) != 0;
      p->renumber      = (bits & LINK_BUS_REC_RENUMBER) != 0;
      p->span_shown    = (bits & LINK_BUS_REC_SHOWN) != 0;
      p->unplaced      = (bits & LINK_BUS_REC_UNPLACED) != 0;
      p->viewed        = (bits & LINK_BUS_REC_VIEWED) != 0;
      p->view_at       = view_at;
      p->grant_valid   = limit == LINK_BUS_LIMIT_GRANT;
      p->published     = false;
      p->flags         = flags & RETRO_LINK_PORT_SHARED_MEDIUM;
      p->max_ports     = max_ports;
      p->pending       = pending & (RETRO_LINK_WAKE_MESSAGE | RETRO_LINK_WAKE_TOPOLOGY);
      p->rate          = rate;
      p->idle          = idle;
      p->anchor        = anchor;
      p->since         = since;
      p->last_since    = last_at;
      p->last_anchor   = anchor + last_tick;
      p->promise       = anchor + promise;
      p->span          = span;
      p->last_stamp    = anchor + stamp;
      p->limit         = limit == LINK_BUS_LIMIT_GRANT ? anchor + grant
         : limit == LINK_BUS_LIMIT_UNBOUNDED ? RETRO_LINK_UNBOUNDED : 0;
      p->grant         = p->limit;
      p->local         = anchor;
      p->request       = anchor;
      p->target        = anchor;
      p->start_local   = anchor;
      p->seen          = 0;
      for (i = 0; i < links; i++)
      {
         struct link_bus_in t;
         struct link_bus_link *l;
         t.at          = link_at + i * 38;
         t.end         = t.at + 38;
         t.bad         = false;
         l             = link_bus_link(p, link_bus_listed(b, in, (size_t)link_bus_get_u(&t, 4)));
         l->flags      = (unsigned)link_bus_get_u(&t, 2);
         l->join       = link_bus_get_u(&t, 8);
         l->join_rate  = link_bus_get_u(&t, 8);
         l->cut        = link_bus_get_u(&t, 8);
         l->cut_rate   = link_bus_get_u(&t, 8);
         if (l->flags & LINK_BUS_LINK_VIEW)
            p->seen++;
         if ((l->flags & LINK_BUS_LINK_CUT) && !(l->flags & LINK_BUS_LINK_TOGETHER))
            p->apart = true;
      }
   }
   for (i = 0; i < n; i++)
      if (!link_bus_read_entry(b, in, p, n))
         return false;
   return true;
}

/* One pass over the blob; see link_bus_pass */
static bool link_bus_read(link_bus_t *b, struct link_bus_in *in, const void *buf, size_t len)
{
   const unsigned char *magic, *pairs;
   uint64_t counts[LINK_BUS_SAVED_COUNTS];
   size_t i, cables;
   unsigned c;
   in->at  = (const unsigned char*)buf;
   in->end = in->at + len;
   in->bad = false;
   magic   = link_bus_get_bytes(in, 4);
   if (     !magic || memcmp(magic, LINK_BUS_MAGIC, 4)
         || link_bus_get_u(in, 4) != LINK_BUS_VERSION
         || link_bus_get_u(in, 4) != b->console_count)
      return false;
   in->recs = (size_t)link_bus_get_u(in, 4);
   cables   = (size_t)link_bus_get_u(in, 4);
   for (i = 0; i < LINK_BUS_SAVED_COUNTS; i++)
      counts[i] = link_bus_get_u(in, 8);
   for (c = 0; c < b->console_count; c++)
   {
      unsigned held = (unsigned)link_bus_get_u(in, 1);
      if (held > 1)
         return false;
      if (in->pass == LINK_BUS_APPLY)
      {
         b->consoles[c].held      = held != 0;
         b->consoles[c].unholding = false;
      }
   }
   if (in->bad || in->recs > (size_t)(in->end - in->at) / 8)
      return false;
   in->table = link_bus_get_bytes(in, in->recs * 8);
   for (i = 0; i < in->recs; i++)
   {
      unsigned console, port, was_console, was_port;
      link_bus_row(in, i, &console, &port);
      if (console >= b->console_count)
         return false;
      if (i)
      {
         link_bus_row(in, i - 1, &was_console, &was_port);
         if (console < was_console || (console == was_console && port <= was_port))
            return false;
      }
   }
   if (in->bad || cables > (size_t)(in->end - in->at) / 8)
      return false;
   if (in->pass == LINK_BUS_MAKE && cables > b->cables_cap)
   {
      struct link_bus_cable *room = (struct link_bus_cable*)
         link_bus_alloc(b, b->cables, cables * sizeof(*room));
      if (!room)
         return false;
      b->cables     = room;
      b->cables_cap = cables;
   }
   pairs = in->at;
   for (i = 0; i < cables; i++)
   {
      size_t x = (size_t)link_bus_get_u(in, 4);
      size_t y = (size_t)link_bus_get_u(in, 4);
      if (x >= in->recs || y >= in->recs || x == y)
         return false;
      if (in->pass == LINK_BUS_APPLY)
      {
         b->cables[i].a = link_bus_listed(b, in, x);
         b->cables[i].b = link_bus_listed(b, in, y);
      }
   }
   if (in->pass == LINK_BUS_CHECK && !link_bus_distinct(b, pairs, cables))
      return false;
   for (i = 0; i < in->recs; i++)
      if (!link_bus_read_port(b, in, i))
         return false;
   if (in->bad || in->at != in->end)
      return false;
   if (in->pass == LINK_BUS_APPLY)
   {
      b->cables_len = cables;
      for (i = 0; i < LINK_BUS_SAVED_COUNTS; i++)
         *(uint64_t*)((char*)&b->counts + link_bus_saved_counts[i]) = counts[i];
   }
   return true;
}

bool link_bus_load(link_bus_t *b, const void *buf, size_t len)
{
   struct link_bus_in in;
   size_t i;
   if (!b || !buf || link_bus_any_in_frame(b))
      return false;
   memset(&in, 0, sizeof(in));
   in.pass = LINK_BUS_CHECK;
   if (!link_bus_read(b, &in, buf, len))
      return false;
   for (i = 0; i < LINK_BUS_POOLS; i++)
      if (!link_bus_reserve(b, &b->pools[i], in.need[i]))
         return false;
   in.pass = LINK_BUS_MAKE;
   if (!link_bus_read(b, &in, buf, len))
   {
      for (i = 0; i < b->recs_len; i++)
      {
         free(b->recs[i]->loaded);
         b->recs[i]->loaded = NULL;
      }
      return false;
   }
   /* A port the blob does not list ends up detached, as one never attached */
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i];
      link_bus_drop(b, p);
      link_bus_fresh(p);
      p->attached = false;
      p->live     = false;
      p->cut      = false;
      p->join     = false;
      p->stale    = false;
      p->renumber = false;
      p->sent     = false;
      p->pending  = 0;
      p->since    = 0;
      p->unplaced = false;
      p->viewed   = false;
      p->apart    = false;
      p->seen     = 0;
      memset(p->links, 0, b->links_cap * sizeof(*p->links));
   }
   in.pass = LINK_BUS_APPLY;
   link_bus_read(b, &in, buf, len);
   b->sends       = in.need[LINK_BUS_POOL_ENTRY]; /* above every loaded rank */
   b->journal_len = 0;
   b->journal_gen++;
   link_bus_group(b);
   return true;
}

struct link_bus_text  /* the dump being written */
{
   char *s;
   size_t len, at;
};

static void link_bus_text_add(struct link_bus_text *t, const char *str)
{
   for (; *str; str++, t->at++)
      if (t->at + 1 < t->len)
         t->s[t->at] = *str;
}

static void link_bus_text_dec(struct link_bus_text *t, uint64_t v)
{
   char d[24];
   size_t i = sizeof(d) - 1;
   d[i]     = '\0';
   do
   {
      d[--i] = (char)('0' + (unsigned)(v % 10));
      v     /= 10;
   } while (v);
   link_bus_text_add(t, d + i);
}

/* A tick as 16 hex digits, or - when not valid */
static void link_bus_text_tick(struct link_bus_text *t, bool valid, uint64_t v)
{
   static const char digits[] = "0123456789abcdef";
   char d[17];
   int i;
   if (!valid)
   {
      link_bus_text_add(t, "-");
      return;
   }
   for (i = 15; i >= 0; i--, v >>= 4)
      d[i] = digits[v & 15];
   d[16] = '\0';
   link_bus_text_add(t, d);
}

static void link_bus_text_port(struct link_bus_text *t, const struct retro_link_port *p)
{
   link_bus_text_dec(t, p->console);
   link_bus_text_add(t, ":");
   link_bus_text_dec(t, p->port);
}

static void link_bus_dump_entry(struct link_bus_text *t, const struct retro_link_port *p,
      const struct link_bus_entry *e, uint64_t rank)
{
   link_bus_text_add(t, "msg ");
   link_bus_text_port(t, p);
   link_bus_text_add(t, " <- ");
   link_bus_text_port(t, e->msg->from);
   link_bus_text_add(t, " index ");
   link_bus_text_dec(t, e->from_index);
   link_bus_text_add(t, " rank ");
   link_bus_text_dec(t, rank);
   if (e->state == LINK_BUS_LANDED)
   {
      link_bus_text_add(t, " landing ");
      link_bus_text_tick(t, true, e->landing);
   }
   else if (e->state != LINK_BUS_REJOIN)
   {
      link_bus_text_add(t, e->state == LINK_BUS_AHEAD ? " at "
            : e->state == LINK_BUS_JOINING ? " joining at " : " placing tick ");
      link_bus_text_tick(t, true, e->instant);
      link_bus_text_add(t, " rate ");
      link_bus_text_dec(t, e->rate);
   }
   else
      link_bus_text_add(t, " rejoin");
   link_bus_text_add(t, e->seen ? " seen 1 len " : " seen 0 len ");
   link_bus_text_dec(t, e->msg->len);
   link_bus_text_add(t, "\n");
}

static void link_bus_dump_port(struct link_bus_text *t, const link_bus_t *b,
      const struct retro_link_port *p)
{
   struct link_bus_order order;
   size_t i;
   link_bus_text_add(t, "port ");
   link_bus_text_port(t, p);
   link_bus_text_add(t, " ");
   link_bus_text_add(t, p->protocol);
   link_bus_text_add(t, " rate ");
   link_bus_text_dec(t, p->rate);
   link_bus_text_add(t, p->attached ? " attached 1" : " attached 0");
   link_bus_text_add(t, b->consoles[p->console].held ? " held 1 bus " : " held 0 bus ");
   if (p->head)
   {
      link_bus_text_dec(t, p->index);
      link_bus_text_add(t, "/");
      link_bus_text_dec(t, p->count);
   }
   else
      link_bus_text_add(t, "-");
   link_bus_text_add(t, " anchor ");
   link_bus_text_tick(t, true, p->anchor);
   link_bus_text_add(t, " since ");
   link_bus_text_tick(t, true, p->since);
   link_bus_text_add(t, p->edge_known ? " edge 1 promise " : " edge 0 promise ");
   link_bus_text_tick(t, p->promise_valid, p->promise);
   link_bus_text_add(t, " limit ");
   if (p->limit == RETRO_LINK_UNBOUNDED)
      link_bus_text_add(t, "unbounded");
   else
      link_bus_text_tick(t, p->grant_valid, p->limit);
   link_bus_text_add(t, " span ");
   link_bus_text_tick(t, p->span_valid, p->span);
   link_bus_text_add(t, " stamp ");
   link_bus_text_tick(t, p->sent, p->last_stamp);
   link_bus_text_add(t, " idle ");
   link_bus_text_dec(t, p->idle);
   link_bus_text_add(t, " pending ");
   link_bus_text_dec(t, p->pending);
   if (p->renumber)
      link_bus_text_add(t, " renumber");
   if (p->unplaced)
      link_bus_text_add(t, " unplaced");
   for (i = 0; i < b->recs_len; i++)
   {
      unsigned f = link_bus_link(p, b->recs[i])->flags;
      if (f & ~(LINK_BUS_LINK_TOGETHER | LINK_BUS_LINK_VIEW | LINK_BUS_LINK_WAS))
      {
         link_bus_text_add(t, " link ");
         link_bus_text_port(t, b->recs[i]);
         link_bus_text_add(t, "/");
         link_bus_text_dec(t, f);
      }
   }
   for (i = 0; i < b->cables_len; i++)
   {
      if (b->cables[i].a == p)
      {
         link_bus_text_add(t, " cable ");
         link_bus_text_port(t, b->cables[i].b);
      }
      else if (b->cables[i].b == p)
      {
         link_bus_text_add(t, " cable ");
         link_bus_text_port(t, b->cables[i].a);
      }
   }
   link_bus_text_add(t, "\n");
   order.e = NULL;
   while (link_bus_step(p, &order))
      link_bus_dump_entry(t, p, order.e, order.rank);
}

size_t link_bus_dump(const link_bus_t *b, char *s, size_t len)
{
   struct link_bus_text t;
   size_t i;
   t.s   = s;
   t.len = s ? len : 0;
   t.at  = 0;
   if (b)
      for (i = 0; i < b->recs_len; i++)
         link_bus_dump_port(&t, b, b->recs[i]);
   if (t.len)
      t.s[t.at < t.len ? t.at : t.len - 1] = '\0';
   return t.at;
}

void link_bus_get_counts(const link_bus_t *b, struct link_bus_counts *out)
{
   if (b && out)
      *out = b->counts;
}

size_t link_bus_journal(const link_bus_t *b, struct link_bus_sent *out, size_t cap)
{
   if (!b)
      return 0;
   if (out && cap && b->journal_len)
      memcpy(out, b->journal, (cap < b->journal_len ? cap : b->journal_len) * sizeof(*out));
   return b->journal_len;
}
