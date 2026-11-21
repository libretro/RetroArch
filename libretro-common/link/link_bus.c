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
 * its port's anchor, so the hash leaves out only anchors and counts */
#define LINK_BUS_MAGIC         "LBUS"
#define LINK_BUS_VERSION       1
#define LINK_BUS_SAVED_COUNTS  8
#define LINK_BUS_REC_ATTACHED  1
#define LINK_BUS_REC_EDGE      2
#define LINK_BUS_REC_PROMISE   4
#define LINK_BUS_REC_SPAN      8
#define LINK_BUS_REC_SENT      16
#define LINK_BUS_REC_RENUMBER  32
#define LINK_BUS_REC_BITS      63
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
   LINK_BUS_REJOIN      /* for a held console: lands at its re-join anchor */
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
   uint64_t ahead, rate; /* LINK_BUS_AHEAD: ticks of rate past the sender's anchor */
   uint64_t seq;         /* send order: link_bus::sends at send, or its loaded rank */
   uint64_t journal_gen;
   size_t journal;       /* its journal entry while journal_gen is the bus's */
   unsigned from_index;
   unsigned char state;
   bool seen;            /* receivable at its port's last grant */
};

struct retro_link_port
{
   link_bus_t *owner;
   char *protocol;
   char *loaded;         /* link_bus_load's new protocol, from MAKE to APPLY */
   struct retro_link_port *head;  /* first member of its bus; NULL when cabled to nothing */
   struct retro_link_port *next;  /* next member, by index */
   /* scratch of link_bus_form and link_bus_reach */
   struct retro_link_port *was_head, *was_next, *uf, *first, *last;
   uint64_t rate;
   uint64_t anchor;      /* tick at the edge the session stands at */
   uint64_t next_anchor; /* tick at the coming edge, from the frame-end publish */
   uint64_t promise;
   uint64_t local, request, target; /* latest publish; target is the request repaired */
   uint64_t start_local; /* this frame's frame-start advance */
   uint64_t span;        /* previous frame, frame-start advance to frame-end publish */
   uint64_t grant;
   uint64_t seq;         /* in a bus's first member: link_bus::seq at the bus's last change */
   unsigned console, port;
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
   bool edge_known, next_known;
   bool promise_valid, span_valid, grant_valid;
   bool published;       /* in this frame */
   bool ended;           /* frame_end seen; the anchor moves at the edge */
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
   unsigned console_count;
   bool frame_open; /* from the first frame_start to the edge */
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

/* a / ra is before b / rb */
static bool link_bus_sooner(uint64_t a, uint64_t ra, uint64_t b, uint64_t rb)
{
   uint64_t ah, al, bh, bl;
   link_bus_mul(a, rb, &ah, &al);
   link_bus_mul(b, ra, &bh, &bl);
   return ah < bh || (ah == bh && al < bl);
}

/* The promise as ticks past the edge */
static uint64_t link_bus_ahead(const struct retro_link_port *p)
{
   return p->promise > p->anchor ? p->promise - p->anchor : 0;
}

/* The instant ahead ticks of q past the edge, on p's clock, rounded down */
static uint64_t link_bus_onto(const struct retro_link_port *p,
      const struct retro_link_port *q, uint64_t ahead)
{
   return link_bus_add(p->anchor, link_bus_scale(ahead, p->rate, q->rate, false));
}

/* t, given against the anchor from, given against the anchor to */
static uint64_t link_bus_move(uint64_t t, uint64_t from, uint64_t to)
{
   if (t >= from)
      return link_bus_add(to, t - from);
   return to > from - t ? to - (from - t) : 0;
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
   return p->attached && p->live && !p->cut && p->head;
}

/* Starts a port's ticks over: nothing it published before binds */
static void link_bus_fresh(struct retro_link_port *p)
{
   p->edge_known    = false;
   p->next_known    = false;
   p->promise_valid = false;
   p->span_valid    = false;
   p->grant_valid   = false;
   p->published     = false;
   p->ended         = false;
   p->limit         = 0;
}

/* A renumbered port's re-join keeps its learned span, a duration on its
 * own clock */
static void link_bus_rejoin(struct retro_link_port *p)
{
   bool span_valid = p->span_valid;
   link_bus_fresh(p);
   p->span_valid = span_valid;
   p->renumber   = true;
   p->stale      = false;
}

static void link_bus_changed(link_bus_t *b, struct retro_link_port *p)
{
   if (p->head)
      p->head->seq = ++b->seq;
}

/* a comes out before c: by landing, then the sender's index, then send order */
static bool link_bus_before(const struct link_bus_entry *a, const struct link_bus_entry *c)
{
   if (a->landing != c->landing)
      return a->landing < c->landing;
   if (a->from_index != c->from_index)
      return a->from_index < c->from_index;
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
      e->landing = p->anchor;
      if (e->state == LINK_BUS_AHEAD)
         e->landing = link_bus_add(p->anchor,
               link_bus_scale(e->ahead, p->rate, e->rate, true));
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

/* The message p hands out next, or NULL */
static struct link_bus_entry *link_bus_next(const struct retro_link_port *p)
{
   if (p->cut || !p->queue || p->queue->landing >= p->limit)
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
   for (e = p->queue; e && e->landing < p->limit; e = e->next)
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
 * RETRO_LINK_UNBOUNDED when no peer bounds p. A peer whose tick at this edge
 * is not known holds p at the edge; a held or silent one holds nobody. */
static uint64_t link_bus_bound(const struct retro_link_port *p,
      const struct retro_link_port **by)
{
   const struct retro_link_port *q;
   uint64_t bound = RETRO_LINK_UNBOUNDED;
   for (q = p->head; q; q = q->next)
   {
      uint64_t t;
      if (q == p || q->cut || q->owner->consoles[q->console].held)
         continue;
      t = q->edge_known ? link_bus_onto(p, q, link_bus_ahead(q)) : p->anchor;
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

/* p's bus is not the one it was on before link_bus_form */
static bool link_bus_moved(const struct retro_link_port *p)
{
   const struct retro_link_port *a = p->was_head, *c = p->head;
   if (!a || !c)
      return a != c;
   if (p->was_count != p->count)
      return true;
   for (; a && c; a = a->was_next, c = c->next)
      if (a != c)
         return true;
   return a != c;
}

static bool link_bus_shared(const struct retro_link_port *p)
{
   return (p->flags & RETRO_LINK_PORT_SHARED_MEDIUM) != 0;
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
   /* Chains in index order: by console, then port number. A port joins
    * only if its bus stays within every member's max_ports. */
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i], *r;
      unsigned cap;
      if (!p->live)
         continue;
      r   = link_bus_root(p);
      cap = r->cap;
      if (p->max_ports && (!cap || p->max_ports < cap))
         cap = p->max_ports;
      if (r->size >= LINK_BUS_MAX_BUS_PORTS || (cap && r->size >= cap))
         continue;
      r->cap = cap;
      if (r->last)
         r->last->next = p;
      else
         r->first      = p;
      r->last  = p;
      p->index = r->size++;
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
      }
      else
         p->count = r->size;
   }
   for (i = 0; i < b->recs_len; i++)
      if (b->recs[i]->head == b->recs[i])
         b->recs[i]->seq = b->seq;
}

/* link_bus_group, reporting WAKE_TOPOLOGY to each port whose bus changed */
static void link_bus_form(link_bus_t *b)
{
   size_t i;
   link_bus_group(b);
   for (i = 0; i < b->recs_len; i++)
      if (link_bus_moved(b->recs[i]))
         b->recs[i]->pending |= RETRO_LINK_WAKE_TOPOLOGY;
}

/* An attached port of console's that none of the entries names */
static bool link_bus_left_out(const link_bus_t *b, unsigned console,
      const struct retro_link_wait *w, unsigned n)
{
   size_t i;
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
   {
      unsigned k = 0;
      if (!b->recs[i]->attached)
         continue;
      while (k < n && w[k].handle != b->recs[i])
         k++;
      if (k == n)
         return true;
   }
   return false;
}

/* The ports the cable map joins to p, with the cable x-y added when x is
 * given: how many, the lowest max_ports among them, and whether a console of
 * theirs is mid-frame. */
static unsigned link_bus_reach(link_bus_t *b, struct retro_link_port *p,
      struct retro_link_port *x, struct retro_link_port *y,
      unsigned *limit, bool *busy)
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
   *busy  = false;
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *q = b->recs[i];
      if (link_bus_root(q) != r)
         continue;
      size++;
      if (q->max_ports && (!*limit || q->max_ports < *limit))
         *limit = q->max_ports;
      if (b->consoles[q->console].in_frame)
         *busy = true;
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
   p = (struct retro_link_port*)link_bus_alloc(b, NULL, sizeof(*p));
   if (!p)
      return NULL;
   memset(p, 0, sizeof(*p));
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
      const struct retro_link_port_info *ports)
{
   struct retro_link_port_info *copy = NULL;
   if (!b || console >= b->console_count)
      return false;
   if (ports)
   {
      size_t n, i, bytes = 0;
      char *s;
      for (n = 0; ports[n].protocol_id; n++)
         bytes += strlen(ports[n].protocol_id) + 1
               + (ports[n].desc ? strlen(ports[n].desc) + 1 : 0);
      copy = (struct retro_link_port_info*)link_bus_alloc(b, NULL,
            (n + 1) * sizeof(*copy) + bytes);
      if (!copy)
         return false;
      s = (char*)(copy + n + 1);
      for (i = 0; i < n; i++)
      {
         copy[i]             = ports[i];
         copy[i].protocol_id = link_bus_put(&s, ports[i].protocol_id);
         copy[i].desc        = ports[i].desc ? link_bus_put(&s, ports[i].desc) : NULL;
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
   if (b->frame_open)
      p->join = true;
   else
   {
      p->live = true;
      link_bus_fresh(p);
      link_bus_form(b);
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
   if (b->frame_open)
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
      link_bus_form(b);
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
      *count = p->count;
   return (int)p->index;
}

bool link_bus_send(link_bus_t *b, unsigned console, retro_link_port_t *h, uint64_t tick,
      unsigned to, const void *buf, size_t len)
{
   struct retro_link_port *p = NULL, *q;
   struct link_bus_msg *m;
   unsigned cell = 0, n;
   uint64_t ahead;
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
   if (to != RETRO_LINK_BROADCAST && (to >= p->count || to == p->index))
      return false;
   n = to == RETRO_LINK_BROADCAST ? p->count - 1 : 1;
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
   ahead   = tick > p->anchor ? tick - p->anchor : 0;

   /* The one copy: the core reuses its buffer */
   m            = (struct link_bus_msg*)link_bus_take(&b->pools[LINK_BUS_POOL_MSG]);
   m->from      = p;
   m->cell      = (unsigned char*)link_bus_take(&b->pools[cell]);
   m->cell_pool = cell;
   m->len       = len;
   m->refs      = n;
   if (len)
      memcpy(m->cell, buf, len);

   /* To the ports on the bus now, a silent or held one included */
   for (q = p->head; q; q = q->next)
   {
      struct link_bus_entry *e;
      struct link_bus_sent *j;
      if (q == p || (to != RETRO_LINK_BROADCAST && q->index != to))
         continue;
      e              = (struct link_bus_entry*)link_bus_take(&b->pools[LINK_BUS_POOL_ENTRY]);
      e->msg         = m;
      e->landing     = RETRO_LINK_UNBOUNDED;
      e->ahead       = ahead;
      e->rate        = p->rate;
      e->seq         = b->sends;
      e->journal_gen = b->journal_gen;
      e->journal     = b->journal_len;
      e->from_index  = p->index;
      e->seen        = false;
      if (b->consoles[q->console].held)
         e->state = LINK_BUS_REJOIN;
      else if (!q->edge_known)
         e->state = LINK_BUS_AHEAD;
      else
      {
         e->state   = LINK_BUS_LANDED;
         e->landing = link_bus_add(q->anchor,
               link_bus_scale(ahead, q->rate, p->rate, true));
      }
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
      if (e->state == LINK_BUS_LANDED)
         link_bus_enqueue(q, e);
      else
         link_bus_wait(q, e);
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

void link_bus_publish(link_bus_t *b, unsigned console, struct retro_link_wait *w, unsigned n)
{
   unsigned i;
   bool foreign                         = false;
   bool lowered                         = false;
   bool as[LINK_BUS_AS_EARLIEST + 1]    = { false, false, false, false };
   const struct retro_link_port *early = NULL;
   uint64_t early_ahead                 = 0;
   if (!b)
      return;
   if (!link_bus_in_frame(b, console))
   {
      b->counts.misplaced_call++;
      return;
   }

   for (i = 0; i < n; i++)
   {
      struct retro_link_port *p = link_bus_own(b, console, w[i].handle);
      bool moved                = false;
      if (link_bus_foreign(b, console, w[i].handle))
         foreign = true;
      if (!p || !p->live || p->cut)
         continue;
      if (!p->published)
      {
         /* The frame-start advance gives the tick at the edge if none is known */
         p->published   = true;
         p->start_local = w[i].local_tick;
         if (!p->edge_known)
         {
            if (p->renumber)
               link_bus_rebase(p, w[i].local_tick);
            p->anchor     = w[i].local_tick;
            p->edge_known = true;
            moved         = true;
            link_bus_land(b, p);
         }
      }
      if (p->promise_valid && w[i].safe_tick < p->promise)
         lowered = true;
      else if (!p->promise_valid || w[i].safe_tick > p->promise)
      {
         p->promise       = w[i].safe_tick;
         p->promise_valid = true;
         moved            = true;
      }
      if (p->local != w[i].local_tick || p->request != w[i].request_tick)
      {
         p->local   = w[i].local_tick;
         p->request = w[i].request_tick;
         moved      = true;
      }
      if (moved)
         link_bus_changed(b, p);
   }

   /* The earliest promise among the entries not granted RETRO_LINK_UNBOUNDED */
   for (i = 0; i < n; i++)
   {
      const struct retro_link_port *p = link_bus_own(b, console, w[i].handle);
      if (!p || !link_bus_joined(p))
         continue;
      if (!early || link_bus_sooner(link_bus_ahead(p), p->rate, early_ahead, early->rate))
      {
         early       = p;
         early_ahead = link_bus_ahead(p);
      }
   }

   /* A request past a bound is granted as the lowest bound it passed */
   for (i = 0; i < n; i++)
   {
      struct retro_link_port *p = link_bus_own(b, console, w[i].handle);
      enum link_bus_repair was   = LINK_BUS_AS_REQUEST;
      uint64_t end, earliest;
      if (!p || !p->live || p->cut)
         continue;
      p->target = p->request;
      if (!link_bus_joined(p) || p->target <= link_bus_floor(p))
         continue;
      end      = link_bus_add(p->anchor, p->span);
      earliest = link_bus_onto(p, early, early_ahead);
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
      const struct retro_link_wait *w, unsigned n)
{
   return !link_bus_blocked_by(b, console, w, n, NULL);
}

void link_bus_grant(link_bus_t *b, unsigned console, struct retro_link_wait *w, unsigned n)
{
   unsigned i;
   if (!b)
      return;
   for (i = 0; i < n; i++)
   {
      struct retro_link_port *p = link_bus_framed(b, console, w[i].handle);
      bool ask                  = w[i].wake_flags != RETRO_LINK_WAKE_NONE;
      w[i].wake_flags           = RETRO_LINK_WAKE_NONE;
      if (!p || b->released)
      {
         w[i].grant = RETRO_LINK_UNBOUNDED;
         if (ask)
            w[i].wake_flags = RETRO_LINK_WAKE_DETACHED;
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
         w[i].grant     = g;
      }
      else
      {
         p->grant_valid = false;
         w[i].grant     = RETRO_LINK_UNBOUNDED;
      }
      p->limit = w[i].grant;
      if (!p->cut && link_bus_reached(p))
         p->pending |= RETRO_LINK_WAKE_MESSAGE;
      if (ask)
      {
         w[i].wake_flags = p->pending;
         p->pending      = RETRO_LINK_WAKE_NONE;
      }
   }
}

bool link_bus_blocked_by(const link_bus_t *b, unsigned console,
      const struct retro_link_wait *w, unsigned n, struct link_bus_blocker *out)
{
   unsigned i;
   if (!b || b->released)
      return false;
   for (i = 0; i < n; i++)
   {
      const struct retro_link_port *p  = link_bus_framed(b, console, w[i].handle);
      const struct retro_link_port *by = NULL;
      uint64_t bound;
      if (!p || !link_bus_joined(p) || p->target <= link_bus_floor(p))
         continue;
      bound = link_bus_bound(p, &by);
      if (p->target <= bound)
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
   if (!b->frame_open)
   {
      /* The journal holds what is sent from here to the edge */
      b->journal_len = 0;
      b->journal_gen++;
      for (i = 0; i < b->recs_len; i++)
         if (b->recs[i]->stale && !b->consoles[b->recs[i]->console].held)
            link_bus_rejoin(b->recs[i]);
   }
   b->consoles[console].in_frame = true;
   b->frame_open                 = true;
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
      b->recs[i]->published = false;
}

void link_bus_frame_end(link_bus_t *b, unsigned console)
{
   size_t i;
   bool skipped = false;
   if (!b || console >= b->console_count)
      return;
   b->consoles[console].in_frame = false;
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
   {
      struct retro_link_port *p = b->recs[i];
      if (!p->live || p->cut)
         continue;
      /* The frame-end publish is the latest, asking for nothing, past the
       * frame-start advance */
      p->ended       = true;
      p->next_known  = p->published && p->local == p->request && p->local > p->start_local;
      p->next_anchor = p->local;
      p->span_valid  = p->next_known;
      p->span        = p->local - p->start_local;
      if (!p->next_known)
         skipped = true;
   }
   if (skipped)
      b->counts.skipped_publish++;
}

void link_bus_edge(link_bus_t *b)
{
   size_t i;
   unsigned c;
   bool reform = false;
   if (!b)
      return;
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i];
      if (p->ended)
      {
         p->anchor     = p->next_anchor;
         p->edge_known = p->next_known;
         p->ended      = false;
      }
      if (p->cut)
      {
         struct retro_link_port *q;
         /* Its peers see it leave even when it re-joins at this edge; off
          * its bus before forming, so a re-attach reports its joining */
         for (q = p->head; q; q = q->next)
            if (q != p)
               q->pending |= RETRO_LINK_WAKE_TOPOLOGY;
         p->cut  = false;
         p->live = false;
         p->head = NULL;
         reform  = true;
         link_bus_drop(b, p);
      }
      if (p->join)
      {
         p->join = false;
         if (p->attached)
         {
            p->live = true;
            link_bus_fresh(p);
            reform  = true;
         }
      }
   }
   for (c = 0; c < b->console_count; c++)
      b->consoles[c].in_frame = false;
   b->frame_open = false;
   if (reform)
      link_bus_form(b);
}

bool link_bus_cable(link_bus_t *b, unsigned console_a, unsigned port_a,
      unsigned console_b, unsigned port_b)
{
   struct retro_link_port *x, *y;
   unsigned size, limit;
   bool busy;
   size_t i;
   if (!b)
      return false;
   x = link_bus_find(b, console_a, port_a);
   y = link_bus_find(b, console_b, port_b);
   if (     !x || !y || x == y || link_bus_shared(x) || link_bus_shared(y)
         || strcmp(x->protocol, y->protocol))
      return false;
   size = link_bus_reach(b, x, x, y, &limit, &busy);
   if (busy || size > LINK_BUS_MAX_BUS_PORTS || (limit && size > limit))
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
   link_bus_form(b);
   return true;
}

bool link_bus_uncable(link_bus_t *b, unsigned console, unsigned port)
{
   struct retro_link_port *p;
   unsigned limit;
   bool busy;
   size_t i, kept = 0;
   if (!b || !(p = link_bus_find(b, console, port)) || link_bus_shared(p))
      return false;
   link_bus_reach(b, p, NULL, NULL, &limit, &busy);
   if (busy)
      return false;
   for (i = 0; i < b->cables_len; i++)
      if (b->cables[i].a != p && b->cables[i].b != p)
         b->cables[kept++] = b->cables[i];
   if (kept != b->cables_len)
   {
      b->cables_len = kept;
      link_bus_form(b);
   }
   return true;
}

bool link_bus_hold(link_bus_t *b, unsigned console)
{
   size_t i;
   if (!b || console >= b->console_count || b->frame_open)
      return false;
   /* Every bus of its ports idle: nothing undelivered, every port quiet */
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
   {
      const struct retro_link_port *q;
      for (q = b->recs[i]->head; q; q = q->next)
         if (q->queue || q->waiting || !link_bus_quiet(q))
            return false;
   }
   b->consoles[console].held = true;
   return true;
}

bool link_bus_unhold(link_bus_t *b, unsigned console)
{
   size_t i;
   if (!b || console >= b->console_count || b->frame_open)
      return false;
   if (b->consoles[console].held)
   {
      /* A re-join: each port's next frame-start advance gives its tick */
      b->consoles[console].held = false;
      for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
      {
         if (b->recs[i]->stale)
            link_bus_rejoin(b->recs[i]);
         link_bus_fresh(b->recs[i]);
      }
   }
   return true;
}

bool link_bus_is_held(const link_bus_t *b, unsigned console)
{
   return b && console < b->console_count && b->consoles[console].held;
}

void link_bus_renumbered(link_bus_t *b, unsigned console)
{
   size_t i;
   if (!b || console >= b->console_count || b->frame_open)
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
   link_bus_put_u(o, e->state == LINK_BUS_AHEAD ? e->ahead : 0, 8, true);
   link_bus_put_u(o, e->state == LINK_BUS_AHEAD ? e->rate : 0, 8, true);
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
                   | (p->renumber      ? LINK_BUS_REC_RENUMBER : 0);
   for (e = p->queue; e; e = e->next)
      n++;
   for (e = p->waiting; e; e = e->next)
      n++;
   link_bus_put_u(o, bits, 1, true);
   link_bus_put_u(o, limit, 1, true);
   link_bus_put_u(o, p->flags, 4, true);
   link_bus_put_u(o, p->max_ports, 4, true);
   link_bus_put_u(o, p->pending, 4, true);
   link_bus_put_u(o, p->rate, 8, true);
   link_bus_put_u(o, p->idle, 8, true);
   link_bus_put_u(o, p->anchor, 8, false);
   link_bus_put_u(o, p->promise_valid ? p->promise - p->anchor : 0, 8, true);
   link_bus_put_u(o, limit == LINK_BUS_LIMIT_GRANT ? p->limit - p->anchor : 0, 8, true);
   link_bus_put_u(o, p->span_valid ? p->span : 0, 8, true);
   link_bus_put_u(o, p->sent ? p->last_stamp - p->anchor : 0, 8, true);
   link_bus_put_u(o, plen, 4, true);
   link_bus_put_bytes(o, p->protocol, plen);
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
   if (!b || !buf || b->frame_open || len < link_bus_save_size(b))
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
   unsigned at     = (unsigned)link_bus_get_u(in, 1);
   uint64_t rank   = link_bus_get_u(in, 8);
   uint64_t landed = link_bus_get_u(in, 8);
   uint64_t ahead  = link_bus_get_u(in, 8);
   uint64_t rate   = link_bus_get_u(in, 8);
   size_t len      = (size_t)link_bus_get_u(in, 4);
   const unsigned char *payload = link_bus_get_bytes(in, len);
   unsigned cell   = 0;
   if (     in->bad || state > LINK_BUS_REJOIN || seen > 1 || from >= in->recs || rank >= n
         || at >= LINK_BUS_MAX_BUS_PORTS || len > RETRO_LINK_MAX_PAYLOAD
         || (state == LINK_BUS_AHEAD && !rate))
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
      e->from_index  = at;
      e->seq         = rank;
      e->landing     = state == LINK_BUS_LANDED ? p->anchor + landed : RETRO_LINK_UNBOUNDED;
      e->ahead       = ahead;
      e->rate        = rate;
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
   unsigned bits      = (unsigned)link_bus_get_u(in, 1);
   unsigned limit     = (unsigned)link_bus_get_u(in, 1);
   unsigned flags     = (unsigned)link_bus_get_u(in, 4);
   unsigned max_ports = (unsigned)link_bus_get_u(in, 4);
   uint32_t pending   = (uint32_t)link_bus_get_u(in, 4);
   uint64_t rate      = link_bus_get_u(in, 8);
   uint64_t idle      = link_bus_get_u(in, 8);
   uint64_t anchor    = link_bus_get_u(in, 8);
   uint64_t promise   = link_bus_get_u(in, 8);
   uint64_t grant     = link_bus_get_u(in, 8);
   uint64_t span      = link_bus_get_u(in, 8);
   uint64_t stamp     = link_bus_get_u(in, 8);
   size_t plen        = (size_t)link_bus_get_u(in, 4);
   const unsigned char *protocol = link_bus_get_bytes(in, plen);
   size_t n           = (size_t)link_bus_get_u(in, 4);
   size_t i;
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
      p->live          = p->attached;
      p->edge_known    = (bits & LINK_BUS_REC_EDGE) != 0;
      p->promise_valid = (bits & LINK_BUS_REC_PROMISE) != 0;
      p->span_valid    = (bits & LINK_BUS_REC_SPAN) != 0;
      p->sent          = (bits & LINK_BUS_REC_SENT) != 0;
      p->renumber      = (bits & LINK_BUS_REC_RENUMBER) != 0;
      p->grant_valid   = limit == LINK_BUS_LIMIT_GRANT;
      p->next_known    = false;
      p->published     = false;
      p->ended         = false;
      p->flags         = flags & RETRO_LINK_PORT_SHARED_MEDIUM;
      p->max_ports     = max_ports;
      p->pending       = pending & (RETRO_LINK_WAKE_MESSAGE | RETRO_LINK_WAKE_TOPOLOGY);
      p->rate          = rate;
      p->idle          = idle;
      p->anchor        = anchor;
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
      p->next_anchor   = anchor;
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
         b->consoles[c].held = held != 0;
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
   if (!b || !buf || b->frame_open)
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
   else if (e->state == LINK_BUS_AHEAD)
   {
      link_bus_text_add(t, " ahead ");
      link_bus_text_tick(t, true, e->ahead);
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
