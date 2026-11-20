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

enum link_bus_repair
{
   LINK_BUS_AS_REQUEST = 0,
   LINK_BUS_AS_PROMISE,
   LINK_BUS_AS_FRAME_END,
   LINK_BUS_AS_EARLIEST
};

struct retro_link_port
{
   link_bus_t *owner;
   char *protocol;
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
   uint32_t pending;     /* reports not yet given */
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
   bool in_frame;
};

struct link_bus
{
   struct link_bus_console *consoles;
   struct retro_link_port **recs; /* by console, then port number */
   struct link_bus_cable *cables;
   struct link_bus_counts counts;
   uint64_t seq;
   size_t recs_len, recs_cap;
   size_t cables_len, cables_cap;
   unsigned console_count;
   bool frame_open; /* from the first frame_start to the edge */
};

static void *link_bus_alloc(link_bus_t *b, void *ptr, size_t size)
{
   b->counts.blocks++;
   return realloc(ptr, size);
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
}

static void link_bus_changed(link_bus_t *b, struct retro_link_port *p)
{
   if (p->head)
      p->head->seq = ++b->seq;
}

/* The highest tick on p's clock its peers allow, and the peer that sets it;
 * RETRO_LINK_UNBOUNDED when no peer bounds p. A peer whose tick at this edge
 * is not known holds p at the edge. */
static uint64_t link_bus_bound(const struct retro_link_port *p,
      const struct retro_link_port **by)
{
   const struct retro_link_port *q;
   uint64_t bound = RETRO_LINK_UNBOUNDED;
   for (q = p->head; q; q = q->next)
   {
      uint64_t t;
      if (q == p || q->cut)
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

/* Forms the buses from the cable map, the shared media and the live ports,
 * and reports WAKE_TOPOLOGY to each port whose bus changed. */
static void link_bus_form(link_bus_t *b)
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
   /* Chains in index order: by console, then port number */
   for (i = 0; i < b->recs_len; i++)
   {
      struct retro_link_port *p = b->recs[i], *r;
      if (!p->live)
         continue;
      r = link_bus_root(p);
      if (r->size >= LINK_BUS_MAX_BUS_PORTS)
         continue;
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
   {
      struct retro_link_port *p = b->recs[i];
      if (link_bus_moved(p))
         p->pending |= RETRO_LINK_WAKE_TOPOLOGY;
      if (p->head == p)
         p->seq = b->seq;
   }
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
   (void)b; (void)console; (void)h; (void)tick; (void)to; (void)buf; (void)len;
   return false;
}

bool link_bus_recv(link_bus_t *b, unsigned console, retro_link_port_t *h, uint64_t *tick,
      unsigned *from, void *buf, size_t *len)
{
   (void)b; (void)console; (void)h; (void)tick; (void)from; (void)buf;
   if (len)
      *len = 0;
   return false;
}

const void *link_bus_peek(link_bus_t *b, unsigned console, retro_link_port_t *h,
      uint64_t *tick, unsigned *from, size_t *len)
{
   (void)b; (void)console; (void)h; (void)tick; (void)from;
   if (len)
      *len = 0;
   return NULL;
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
            p->anchor     = w[i].local_tick;
            p->edge_known = true;
            moved         = true;
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
      struct retro_link_port *p = link_bus_own(b, console, w[i].handle);
      bool ask                  = w[i].wake_flags != RETRO_LINK_WAKE_NONE;
      w[i].wake_flags           = RETRO_LINK_WAKE_NONE;
      if (!p)
      {
         w[i].grant = RETRO_LINK_UNBOUNDED;
         if (ask)
            w[i].wake_flags = RETRO_LINK_WAKE_DETACHED;
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
   if (!b)
      return false;
   for (i = 0; i < n; i++)
   {
      const struct retro_link_port *p  = link_bus_own(b, console, w[i].handle);
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
   (void)b; (void)console;
   return 0;
}

void link_bus_frame_start(link_bus_t *b, unsigned console)
{
   size_t i;
   if (!b || console >= b->console_count)
      return;
   b->consoles[console].in_frame = true;
   b->frame_open                 = true;
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
      b->recs[i]->published = false;
}

void link_bus_frame_end(link_bus_t *b, unsigned console)
{
   size_t i;
   if (!b || console >= b->console_count)
      return;
   b->consoles[console].in_frame = false;
   for (i = link_bus_at(b, console, 0); i < b->recs_len && b->recs[i]->console == console; i++)
   {
      struct retro_link_port *p = b->recs[i];
      if (!p->live || p->cut)
         continue;
      /* The frame-end publish is the latest, asking for nothing */
      p->ended       = true;
      p->next_known  = p->published && p->local == p->request;
      p->next_anchor = p->local;
      p->span_valid  = p->next_known && p->local > p->start_local;
      p->span        = p->local - p->start_local;
   }
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
   (void)b; (void)console;
   return false;
}

bool link_bus_unhold(link_bus_t *b, unsigned console)
{
   (void)b; (void)console;
   return false;
}

bool link_bus_is_held(const link_bus_t *b, unsigned console)
{
   (void)b; (void)console;
   return false;
}

void link_bus_renumbered(link_bus_t *b, unsigned console) { (void)b; (void)console; }
void link_bus_console_gone(link_bus_t *b, unsigned console) { (void)b; (void)console; }
void link_bus_release(link_bus_t *b) { (void)b; }
bool link_bus_released(const link_bus_t *b) { (void)b; return false; }
size_t link_bus_save_size(const link_bus_t *b) { (void)b; return 0; }

bool link_bus_save(const link_bus_t *b, void *buf, size_t len)
{
   (void)b; (void)buf; (void)len;
   return false;
}

bool link_bus_load(link_bus_t *b, const void *buf, size_t len)
{
   (void)b; (void)buf; (void)len;
   return false;
}

uint64_t link_bus_hash(const link_bus_t *b) { (void)b; return 0; }

size_t link_bus_dump(const link_bus_t *b, char *s, size_t len)
{
   (void)b;
   if (s && len)
      *s = '\0';
   return 0;
}

void link_bus_get_counts(const link_bus_t *b, struct link_bus_counts *out)
{
   if (b && out)
      *out = b->counts;
}

size_t link_bus_journal(const link_bus_t *b, struct link_bus_sent *out, size_t cap)
{
   (void)b; (void)out; (void)cap;
   return 0;
}
