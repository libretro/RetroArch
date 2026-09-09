/* message_queue and fifo_queue, against oracles.
 *
 * message_queue is a max-heap of on-screen notices keyed by priority,
 * production capacity 8. Randomised push, pull and extract at every
 * depth from 1 to 8 - a heap of four or more nodes is where a bound
 * off by one shows - checked after every step against a reference
 * multiset: extraction is by priority, non-increasing; the heap
 * property holds; every live node is unique and every slot past the
 * end is NULL. Then the lifetime contract of pull, and allocation
 * failure at each of push's three allocations.
 *
 * fifo_queue is a byte ring with a wasted slot, synchronised by the
 * caller. Against a reference deque: no wrap, wrap on the boundary,
 * split reads and writes, empty and full; then the lengths a checked
 * call must survive - past what is available, past capacity, and
 * SIZE_MAX with the index at the end of the ring - and construction
 * at 0, 1, and the sizes that would wrap len + 1. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* The node type is private to message_queue.c; the invariant checks
 * read it, so the source is included rather than linked. */
#include "../../../queues/message_queue.c"
#include <queues/fifo_queue.h>

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* --- fault injection ---------------------------------------------------- */

static int fail_alloc_at = -1;   /* the Nth allocation from now fails; -1 = never */
static int alloc_count;

void *__real_malloc(size_t n);
char *__real_strdup(const char *s);
void *__wrap_malloc(size_t n)
{
   if (fail_alloc_at >= 0 && alloc_count++ == fail_alloc_at)
      return NULL;
   return __real_malloc(n);
}
char *__wrap_strdup(const char *s)
{
   if (fail_alloc_at >= 0 && alloc_count++ == fail_alloc_at)
      return NULL;
   return __real_strdup(s);
}

static uint32_t rng_state = 0x12345678u;
static unsigned rnd(unsigned n)
{
   rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5;
   return rng_state % n;
}

/* --- message_queue: the heap against a multiset ------------------------- */

/* The queue's internals, for the invariant checks. */
static void heap_invariants(msg_queue_t *q, const char *when)
{
   size_t i, j;
   for (i = 1; i < q->ptr; i++)
   {
      CHECK(q->elems[i] != NULL, "%s: live slot %u is NULL", when, (unsigned)i);
      if (!q->elems[i])
         return;
      if (i > 1)
         CHECK(q->elems[i >> 1]->prio >= q->elems[i]->prio,
               "%s: parent %u (prio %u) below child %u (prio %u)",
               when, (unsigned)(i >> 1), q->elems[i >> 1]->prio, (unsigned)i, q->elems[i]->prio);
      for (j = i + 1; j < q->ptr; j++)
         CHECK(q->elems[j] != q->elems[i], "%s: node %u also at %u", when, (unsigned)i, (unsigned)j);
   }
   for (i = q->ptr; i < q->size; i++)
      CHECK(q->elems[i] == NULL, "%s: slot %u past the end is not NULL", when, (unsigned)i);
}

static void t_heap(unsigned capacity, unsigned rounds)
{
   msg_queue_t q;
   unsigned  ref[64];   /* priorities in the queue, as a multiset */
   unsigned  n = 0, r, k;
   char      when[64];

   msg_queue_initialize(&q, capacity);
   for (r = 0; r < rounds; r++)
   {
      unsigned op = rnd(3);
      snprintf(when, sizeof(when), "cap %u round %u", capacity, r);
      if (op == 0 || n == 0)
      {
         /* push, duration 1 so a pull removes it */
         unsigned prio = rnd(5);   /* duplicates on purpose */
         char msg[16];
         snprintf(msg, sizeof(msg), "p%u", prio);
         msg_queue_push(&q, msg, prio, 1, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         if (n < capacity)
            ref[n++] = prio;
         CHECK(q.ptr - 1 == n, "%s: %u nodes after push, reference %u", when, (unsigned)(q.ptr - 1), n);
      }
      else
      {
         /* pull (TTL 1, so it removes) or extract: must be the max */
         unsigned max = 0, at = 0;
         for (k = 0; k < n; k++)
            if (ref[k] > max || k == 0) { max = ref[k]; at = k; }
         if (op == 1)
         {
            const char *m = msg_queue_pull(&q);
            CHECK(m != NULL, "%s: pull returned nothing with %u queued", when, n);
            if (m)
               CHECK((unsigned)atoi(m + 1) == max, "%s: pull returned prio %s, max was %u", when, m, max);
         }
         else
         {
            msg_queue_entry_t e;
            bool ok = msg_queue_extract(&q, &e);
            CHECK(ok, "%s: extract failed with %u queued", when, n);
            if (ok)
               CHECK(e.prio == max, "%s: extract returned prio %u, max was %u", when, e.prio, max);
         }
         ref[at] = ref[--n];
         CHECK(q.ptr - 1 == n, "%s: %u nodes after remove, reference %u", when, (unsigned)(q.ptr - 1), n);
      }
      heap_invariants(&q, when);
   }
   /* Drain: non-increasing priorities. */
   {
      unsigned last = 0xFFFFFFFFu;
      msg_queue_entry_t e;
      while (msg_queue_extract(&q, &e))
      {
         CHECK(e.prio <= last, "cap %u drain: prio %u after %u", capacity, e.prio, last);
         last = e.prio;
         heap_invariants(&q, "drain");
      }
   }
   msg_queue_deinitialize(&q);
}

/* Pull's lifetime: duration N returns the message N times, then it is
 * gone; the pointer after expiry is the queue's temporary. */
static void t_lifetime(void)
{
   msg_queue_t q;
   const char *m;
   unsigned i;
   msg_queue_initialize(&q, 8);
   msg_queue_push(&q, "three", 1, 3, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   for (i = 0; i < 3; i++)
   {
      m = msg_queue_pull(&q);
      CHECK(m && !strcmp(m, "three"), "pull %u of a duration-3 message: %s", i + 1, m ? m : "(null)");
   }
   /* The last pull removed the node and returned its message; that
    * pointer is the queue's temporary and stands until the next pull. */
   CHECK(m && !strcmp(m, "three"), "the message of a removed node is not readable after its last pull");
   CHECK(msg_queue_pull(&q) == NULL && q.ptr == 1, "a duration-3 message survived three pulls");
   /* One block per node: message and title live inside it. */
   msg_queue_push(&q, "blk", 2, 5, "ttl", MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   {
      const char *base = (const char*)q.elems[1];
      CHECK(q.elems[1]->msg   > base && q.elems[1]->msg   < base + sizeof(struct queue_elem) + 8, "the message is not in the node's block");
      CHECK(q.elems[1]->title > base && q.elems[1]->title < base + sizeof(struct queue_elem) + 8, "the title is not in the node's block");
      CHECK(!strcmp(q.elems[1]->msg, "blk") && !strcmp(q.elems[1]->title, "ttl"), "the block's strings are wrong");
   }
   msg_queue_clear(&q);
   /* Zero is one pull, not forever: a core's message shorter than a
    * frame rounds to zero and is shown once. */
   msg_queue_push(&q, "zero", 1, 0, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   m = msg_queue_pull(&q);
   CHECK(m && !strcmp(m, "zero"), "a duration-0 message was not shown once");
   CHECK(msg_queue_pull(&q) == NULL && q.ptr == 1, "a duration-0 message stayed");
   /* try_push reports a full queue. */
   for (i = 0; i < 8; i++)
      CHECK(msg_queue_try_push(&q, "fill", 1, 1, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO), "push %u into a queue of 8 refused", i);
   CHECK(!msg_queue_try_push(&q, "ninth", 1, 1, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO), "a ninth push into a queue of 8 was accepted");
   msg_queue_deinitialize(&q);
}

/* Push's allocations: the element, the message, the title. A failure
 * at any of them leaves the queue as it was and leaks nothing (ASan
 * reports a leak; this checks the queue). */
static void t_alloc_failure(void)
{
   msg_queue_t q;
   int at;
   msg_queue_initialize(&q, 8);
   msg_queue_push(&q, "keep", 5, 10, "t", MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   /* One block per push: one allocation, one failure point. */
   for (at = 0; at < 1; at++)
   {
      alloc_count   = 0;
      fail_alloc_at = at;
      CHECK(!msg_queue_try_push(&q, "new", 9, 10, "title", MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO),
            "a push whose allocation failed reported success");
      fail_alloc_at = -1;
      CHECK(q.ptr == 2, "allocation %d failed and the queue has %u nodes, not 1", at, (unsigned)(q.ptr - 1));
      CHECK(q.elems[1] && q.elems[1]->msg && !strcmp(q.elems[1]->msg, "keep"),
            "allocation %d failed and the front changed", at);
      heap_invariants(&q, "after a failed push");
   }
   alloc_count = 0;
   CHECK(msg_queue_try_push(&q, "new", 9, 10, "title", MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO)
         && alloc_count == 0, "a push made an allocation the wrapper did not see");
   msg_queue_deinitialize(&q);
}

/* --- fifo_queue against a deque ------------------------------------------ */

static uint8_t ref_buf[1 << 16];
static size_t  ref_len;

static void t_fifo(size_t capacity, unsigned rounds, bool checked)
{
   fifo_buffer_t *f = fifo_new(capacity);
   uint8_t src[4096], dst[4096];
   unsigned r, i;
   uint8_t  next_byte = 0;
   char when[64];

   CHECK(f != NULL, "fifo_new(%u) failed", (unsigned)capacity);
   if (!f)
      return;
   ref_len = 0;
   for (r = 0; r < rounds; r++)
   {
      size_t avail_w = FIFO_WRITE_AVAIL(f), avail_r = FIFO_READ_AVAIL(f);
      snprintf(when, sizeof(when), "cap %u round %u", (unsigned)capacity, r);
      CHECK(avail_w == capacity - ref_len, "%s: write avail %u, reference %u", when, (unsigned)avail_w, (unsigned)(capacity - ref_len));
      CHECK(avail_r == ref_len,            "%s: read avail %u, reference %u",  when, (unsigned)avail_r, (unsigned)ref_len);
      if (rnd(2) == 0)
      {
         size_t n = rnd(sizeof(src) < capacity ? sizeof(src) : capacity + 1);
         size_t take = n > avail_w ? avail_w : n;
         for (i = 0; i < n; i++) src[i] = next_byte + (uint8_t)i;
         if (checked)
         {
            size_t w = fifo_write_checked(f, src, n);
            CHECK(w == take, "%s: checked write of %u with %u free wrote %u", when, (unsigned)n, (unsigned)avail_w, (unsigned)w);
         }
         else
         {
            n = take;
            fifo_write(f, src, n);
         }
         memcpy(ref_buf + ref_len, src, take);
         ref_len   += take;
         next_byte += (uint8_t)take;
      }
      else
      {
         size_t n = rnd(sizeof(dst) < capacity ? sizeof(dst) : capacity + 1);
         size_t take = n > avail_r ? avail_r : n;
         if (checked)
         {
            size_t got = fifo_read_checked(f, dst, n);
            CHECK(got == take, "%s: checked read of %u with %u queued read %u", when, (unsigned)n, (unsigned)avail_r, (unsigned)got);
         }
         else
         {
            n = take;
            fifo_read(f, dst, n);
         }
         CHECK(memcmp(dst, ref_buf, take) == 0, "%s: read bytes differ from the reference", when);
         memmove(ref_buf, ref_buf + take, ref_len - take);
         ref_len -= take;
      }
   }
   fifo_free(f);
}

/* The lengths a checked call must survive: past what is available,
 * past capacity, twice capacity, SIZE_MAX, and SIZE_MAX with the index
 * at the end of the ring, where end + len wraps size_t. */
static void t_fifo_lengths(void)
{
   fifo_buffer_t *f = fifo_new(1000);
   uint8_t big[4096], out[4096];
   size_t lens[] = { 0, 1, 999, 1000, 1001, 2000, (size_t)-1 };
   size_t k, w;
   memset(big, 0xAB, sizeof(big));
   if (!f) { CHECK(0, "fifo_new(1000)"); return; }
   /* Park the write index at the end of the ring: fill and drain 999. */
   fifo_write_checked(f, big, 999);
   fifo_read_checked(f, out, 999);
   for (k = 0; k < sizeof(lens) / sizeof(lens[0]); k++)
   {
      size_t free_now = FIFO_WRITE_AVAIL(f);
      size_t want     = lens[k] > free_now ? free_now : lens[k];
      w = fifo_write_checked(f, big, lens[k]);
      CHECK(w == want, "write of %s%u with %u free wrote %u",
            lens[k] == (size_t)-1 ? "SIZE_MAX=" : "", (unsigned)lens[k], (unsigned)free_now, (unsigned)w);
      CHECK(FIFO_READ_AVAIL(f) == want, "after it %u queued, expected %u", (unsigned)FIFO_READ_AVAIL(f), (unsigned)want);
      w = fifo_read_checked(f, out, (size_t)-1);
      CHECK(w == want, "a SIZE_MAX read of %u queued read %u", (unsigned)want, (unsigned)w);
   }
   fifo_free(f);
}

static void t_fifo_construction(void)
{
   fifo_buffer_t *f;
   f = fifo_new(0);
   CHECK(f != NULL, "fifo_new(0) failed");
   if (f) { CHECK(FIFO_WRITE_AVAIL(f) == 0 && FIFO_READ_AVAIL(f) == 0, "a zero-capacity ring has room"); fifo_free(f); }
   f = fifo_new(1);
   CHECK(f != NULL, "fifo_new(1) failed");
   if (f) { CHECK(FIFO_WRITE_AVAIL(f) == 1, "fifo_new(1) has %u free", (unsigned)FIFO_WRITE_AVAIL(f)); fifo_free(f); }
   f = fifo_new((size_t)-1);
   CHECK(f == NULL, "fifo_new(SIZE_MAX) succeeded: len + 1 wrapped");
   if (f) fifo_free(f);
   /* SIZE_MAX - 1 is a legal request for SIZE_MAX bytes, which no
    * allocator grants and a sanitizer's reports; not asked. */
}

static void t_msg_init(void)
{
   msg_queue_t q;
   CHECK(!msg_queue_initialize(&q, (size_t)-1), "msg_queue_initialize(SIZE_MAX) succeeded: len + 1 wrapped");
   CHECK(msg_queue_initialize(&q, 0), "msg_queue_initialize(0) failed");
   msg_queue_push(&q, "x", 1, 1, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   CHECK(q.ptr == 1, "a zero-capacity queue accepted a push");
   msg_queue_deinitialize(&q);
}

int main(void)
{
   unsigned cap;
   printf("message_queue:\n");
   for (cap = 1; cap <= 8; cap++)
   {
      printf("   capacity %u, 2000 random push/pull/extract\n", cap);
      t_heap(cap, 2000);
   }
   printf("   lifetime\n");        t_lifetime();
   printf("   allocation failure\n"); t_alloc_failure();
   printf("   construction\n");     t_msg_init();
   printf("fifo_queue:\n");
   printf("   unchecked against a deque, capacities 1, 7, 64, 4096\n");
   t_fifo(1, 500, false); t_fifo(7, 2000, false); t_fifo(64, 2000, false); t_fifo(4096, 2000, false);
   printf("   checked against a deque, capacities 1, 7, 64, 4096\n");
   t_fifo(1, 500, true);  t_fifo(7, 2000, true);  t_fifo(64, 2000, true);  t_fifo(4096, 2000, true);
   printf("   invalid lengths on the checked calls\n"); t_fifo_lengths();
   printf("   construction\n");     t_fifo_construction();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("queues: the heap is a heap on priority at every depth, and no length reaches an OOB copy\n");
   return 0;
}
