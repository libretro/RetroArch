/* rvz_test: writes RVZ and WIA files from disc images it makes up and
 * checks that formats/rvz reads each back as its image, octet for octet.
 *
 * The images exercise what the formats store: GameCube and Wii discs,
 * chunks of 32 KiB to 4 MiB, groups stored as they are and compressed
 * side by side, with Zstandard, bzip2, LZMA and LZMA2, WIA's PURGE
 * groups, groups of zeros, padding stored as a
 * generator seed (cut where a group ends, and runs over many words),
 * Wii partitions in one segment and two (the first a whole number of
 * hash blocks, and not), encrypted and not, with hash
 * exceptions in the hashes and in their padding, and a partition whose
 * last hash block is short.
 *
 * The writer follows the format description, not the reader: the
 * padding comes from a generator written out here as described, and
 * the hashes and encryption are worked out one sector at a time.
 *
 * The files are made and read in memory, each range the reader asks
 * for supplied from there, now and then short.
 *
 * Usage: rvz_test [dir [sectors]]
 *   with dir, the files are also written there; with sectors, the Wii
 *   images' partitions are that large (at least 72). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/rvz.h>
#include <crypto/crypto.h>
#include <lrc_hash.h>
#ifdef HAVE_RZSTD
#include <encodings/rzstd.h>
#endif

#include "../../encodings/rbzip2/bz_write.h"

#define SECTOR    0x8000
#define SDATA     0x7c00
#define BLOCK     (64 * SECTOR)
#define MAX_JUNK  256
#define MAX_EXC   4096

static int failures = 0;
/* the Wii partition's data, in sectors: a full hash block and 37 */
static uint32_t n_sectors = 101;
static uint32_t rng_state;

static uint32_t rnd(void)
{
   rng_state ^= rng_state << 13;
   rng_state ^= rng_state >> 17;
   rng_state ^= rng_state << 5;
   return rng_state;
}

static void put32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);
   p[3] = (uint8_t)v;
}

static void put64(uint8_t *p, uint64_t v)
{
   put32(p, (uint32_t)(v >> 32));
   put32(p + 4, (uint32_t)v);
}

static void fail(const char *what, const char *name)
{
   fprintf(stderr, "FAIL: %s: %s\n", name, what);
   failures++;
}

/* ---- padding, as the format describes it ---- */

static void lfg_forward(uint32_t *b)
{
   unsigned i;
   for (i = 0; i < 32; i++)
      b[i] ^= b[i + 521 - 32];
   for (i = 32; i < 521; i++)
      b[i] ^= b[i - 32];
}

/* @len octets the generator seeded with @seed puts out from offset
 * @skip of its output */
static void lfg_bytes(const uint8_t *seed, size_t skip, uint8_t *dst, size_t len)
{
   uint32_t b[521];
   size_t   pos = 0, w = 0;
   unsigned i;
   for (i = 0; i < 17; i++)
      b[i] = ((uint32_t)seed[4 * i] << 24) | ((uint32_t)seed[4 * i + 1] << 16)
           | ((uint32_t)seed[4 * i + 2] << 8) | seed[4 * i + 3];
   for (i = 17; i < 521; i++)
      b[i] = (b[i - 17] << 23) ^ (b[i - 16] >> 9) ^ b[i - 1];
   for (i = 0; i < 4; i++)
      lfg_forward(b);
   while (pos < skip + len)
   {
      uint8_t o[4];
      o[0] = (uint8_t)(b[w] >> 24);
      o[1] = (uint8_t)(b[w] >> 18);
      o[2] = (uint8_t)(b[w] >> 8);
      o[3] = (uint8_t)b[w];
      for (i = 0; i < 4; i++, pos++)
         if (pos >= skip && pos < skip + len)
            dst[pos - skip] = o[i];
      if (++w == 521)
      {
         lfg_forward(b);
         w = 0;
      }
   }
}

/* ---- the made-up disc ---- */

/* A run of padding: from @off, @len octets of what @seed puts out
 * from offset @off % 32 KiB. */
struct junk
{
   uint64_t off;
   uint32_t len;
   uint8_t  seed[68];
};

/* Disc data, or a partition's data without its hashes, with the
 * padding in it */
struct space
{
   uint8_t    *d;
   uint64_t    len;
   struct junk j[MAX_JUNK];
   unsigned    nj;
};

static void add_junk(struct space *sp, uint64_t off, uint32_t len)
{
   struct junk *j;
   unsigned     i;
   if (!len || sp->nj == MAX_JUNK)
      return;
   j      = &sp->j[sp->nj++];
   j->off = off;
   j->len = len;
   for (i = 0; i < 68; i++)
      j->seed[i] = (uint8_t)rnd();
   lfg_bytes(j->seed, (size_t)(off % SECTOR), sp->d + off, len);
}

/* Fills [@from, @to) of @sp 32 KiB at a time - with noise, zeros,
 * padding over all or part of it, or a mix - and when @long_at is not
 * 0, one run of padding over 0x14000 octets from @long_at, which the
 * caller puts inside one group. */
static void fill(struct space *sp, uint64_t from, uint64_t to, uint64_t long_at)
{
   uint64_t w;
   for (w = from - from % SECTOR; w < to; w += SECTOR)
   {
      uint64_t a = w < from ? from : w, e = w + SECTOR > to ? to : w + SECTOR, x;
      unsigned kind = rnd() % 6;
      if (long_at && e > long_at && a < long_at + 0x14000)
         continue;
      for (x = a; x < e; x++)
         sp->d[x] = kind == 1 ? 0 : (uint8_t)rnd();
      if (kind == 2)
         add_junk(sp, a, (uint32_t)(e - a));
      else if (kind == 3 && e - a > 0x100)
      {
         uint32_t s = rnd() % (uint32_t)(e - a - 0x80);
         uint32_t l = 1 + rnd() % (uint32_t)(e - a - s);
         add_junk(sp, a + s, l);
      }
      else if (kind == 4)
      {
         /* two short runs with a few zeros between */
         uint32_t h = (uint32_t)(e - a) / 2;
         add_junk(sp, a, h - 4);
         memset(sp->d + a + h - 4, 0, 4);
         add_junk(sp, a + h, (uint32_t)(e - a) - h);
      }
   }
   if (long_at)
   {
      for (w = long_at; w < long_at + 0x14000; w++)
         sp->d[w] = 0;
      add_junk(sp, long_at, 0x14000);
   }
}

/* What the caller writes over [off, off + len) after fill() is not
 * padding: the runs over it are cut back to stored octets. */
static void unjunk(struct space *sp, uint64_t off, uint64_t len)
{
   unsigned i;
   for (i = 0; i < sp->nj; i++)
      if (sp->j[i].off < off + len && sp->j[i].off + sp->j[i].len > off)
         sp->j[i].len = 0;
}

/* ---- the RVZ file ---- */

struct out
{
   uint8_t *d;
   size_t   len, cap;
};

static void emit(struct out *o, const void *p, size_t len)
{
   if (!len)
      return;
   if (o->len + len > o->cap)
   {
      o->cap = (o->len + len) * 2 + 4096;
      o->d   = (uint8_t*)realloc(o->d, o->cap);
   }
   if (p)
      memcpy(o->d + o->len, p, len);
   else
      memset(o->d + o->len, 0, len);
   o->len += len;
}

/* the file's compression method, as the disc struct numbers it */
#define M_NONE  0
#define M_PURGE 1
#define M_BZIP2 2
#define M_LZMA  3
#define M_LZMA2 4
#define M_ZSTD  5

struct config
{
   const char *name;
   int      wii;
   uint32_t chunk;
   int      method;
   int      encrypted;
   int      segments;   /* the partition's data in 1 or 2; 3 is 2,
                         * the first not a whole hash block */
   int      exceptions;
   int      wia;        /* WIA rather than RVZ */
};

/* ---- LZMA, as an encoder that emits literals only ----
 *
 * Every octet is coded as a literal, with the range coder and the
 * adaptive model a real encoder uses (lc 3, lp 0, pb 2), so what the
 * reader decodes is a genuine LZMA stream; it simply finds no
 * matches. LZMA2 chunks come from it too, the model carried across
 * chunks or reset as their control octet says. */

#define LZ_PROPS 0x5d   /* lc 3, lp 0, pb 2 */

struct lzenc
{
   uint64_t low;
   uint32_t range;
   uint32_t cache_size;
   uint8_t  cache;
   uint16_t is_match[16];        /* state 0, by position */
   uint16_t lit[8 * 0x300];      /* by the previous octet's top three bits */
};

static void lz_model(struct lzenc *e)
{
   unsigned i;
   for (i = 0; i < 16; i++)
      e->is_match[i] = 1024;
   for (i = 0; i < 8 * 0x300; i++)
      e->lit[i] = 1024;
}

static void lz_coder(struct lzenc *e)
{
   e->low        = 0;
   e->range      = 0xffffffffu;
   e->cache      = 0;
   e->cache_size = 1;
}

static void lz_shift(struct lzenc *e, struct out *o)
{
   if ((uint32_t)e->low < 0xff000000u || (e->low >> 32) != 0)
   {
      uint8_t t = e->cache;
      do
      {
         uint8_t b = (uint8_t)(t + (uint8_t)(e->low >> 32));
         emit(o, &b, 1);
         t = 0xff;
      } while (--e->cache_size);
      e->cache = (uint8_t)(e->low >> 24);
   }
   e->cache_size++;
   e->low = (e->low & 0x00ffffffu) << 8;
}

static void lz_bit(struct lzenc *e, struct out *o, uint16_t *p, unsigned bit)
{
   uint32_t bound = (e->range >> 11) * *p;
   if (!bit)
   {
      e->range = bound;
      *p      += (2048 - *p) >> 5;
   }
   else
   {
      e->low   += bound;
      e->range -= bound;
      *p      -= *p >> 5;
   }
   while (e->range < (1u << 24))
   {
      e->range <<= 8;
      lz_shift(e, o);
   }
}

/* [from, from + len) of @d, as literals; @d from 0 is the window */
static void lz_literals(struct lzenc *e, struct out *o, const uint8_t *d,
      size_t from, size_t len)
{
   size_t at;
   for (at = from; at < from + len; at++)
   {
      unsigned prev = at ? d[at - 1] : 0, sym = 1, i;
      uint16_t *lit = e->lit + 0x300 * (prev >> 5);
      lz_bit(e, o, &e->is_match[at & 3], 0);
      for (i = 0; i < 8; i++)
      {
         unsigned bit = (d[at] >> (7 - i)) & 1;
         lz_bit(e, o, &lit[sym], bit);
         sym = sym * 2 + bit;
      }
   }
}

static void lz_flush(struct lzenc *e, struct out *o)
{
   unsigned i;
   for (i = 0; i < 5; i++)
      lz_shift(e, o);
}

/* A raw LZMA stream of @d, with no end marker, as WIA and RVZ store
 * one */
static void lzma_raw(struct out *o, const uint8_t *d, size_t len)
{
   struct lzenc e;
   lz_model(&e);
   lz_coder(&e);
   lz_literals(&e, o, d, 0, len);
   lz_flush(&e, o);
}

/* An LZMA2 stream of @d: LZMA chunks, continuing the model or
 * resetting it, and stored chunks, then the end */
static void lzma2_raw(struct out *o, const uint8_t *d, size_t len)
{
   struct lzenc e;
   size_t       at = 0;
   int          first = 1, after_stored = 0;
   while (at < len)
   {
      size_t   n = len - at;
      unsigned kind = first ? 0 : rnd() % 5;
      uint8_t  h[6];
      if (kind == 4 && !first)
      {
         /* stored, up to 64 KiB */
         if (n > 0x10000)
            n = 0x10000;
         h[0] = 0x02;
         h[1] = (uint8_t)((n - 1) >> 8);
         h[2] = (uint8_t)(n - 1);
         emit(o, h, 3);
         emit(o, d + at, n);
         after_stored = 1;
      }
      else
      {
         struct out c;
         unsigned   ctl;
         /* the first chunk resets everything; after it, carry on, or
          * reset the state, or that and the props */
         if (first)
            ctl = 0xe0;
         else if (kind == 1 || after_stored)
            ctl = 0xa0;
         else if (kind == 2)
            ctl = 0xc0;
         else
            ctl = 0x80;
         if (n > 0xe000)
            n = 0xe000;
         if (ctl >= 0xa0 || first)
            lz_model(&e);
         lz_coder(&e);
         memset(&c, 0, sizeof(c));
         lz_literals(&e, &c, d, at, n);
         lz_flush(&e, &c);
         if (c.len > 0x10000)
            fail("an LZMA2 chunk over 64 KiB packed", "writer");
         h[0] = (uint8_t)(ctl | ((n - 1) >> 16));
         h[1] = (uint8_t)((n - 1) >> 8);
         h[2] = (uint8_t)(n - 1);
         h[3] = (uint8_t)((c.len - 1) >> 8);
         h[4] = (uint8_t)(c.len - 1);
         h[5] = LZ_PROPS;
         emit(o, h, ctl >= 0xc0 ? 6 : 5);
         emit(o, c.d, c.len);
         free(c.d);
         after_stored = 0;
      }
      first = 0;
      at   += n;
   }
   emit(o, NULL, 1);
}

/* @in compressed with the method into @o */
static void squeeze(int method, struct out *o, const uint8_t *in, size_t len)
{
   if (method == M_BZIP2)
   {
      size_t   cap = len + len / 4 + 4096, wrote;
      uint8_t *z   = (uint8_t*)malloc(cap);
      if (!(wrote = bz_write(z, cap, in, len, 1, (uint32_t)len)))
         fail("bzip2 encode", "writer");
      emit(o, z, wrote);
      free(z);
   }
   else if (method == M_LZMA)
      lzma_raw(o, in, len);
   else if (method == M_LZMA2)
      lzma2_raw(o, in, len);
#ifdef HAVE_RZSTD
   else if (method == M_ZSTD)
   {
      size_t   cap = rzstd_compress_bound(len), wrote = 0;
      uint8_t *z   = (uint8_t*)malloc(cap);
      if (rzstd_encode(z, cap, in, len, 1, &wrote) != RZSTD_PROCESS_END)
      {
         fail("zstd encode", "writer");
         wrote = 0;
      }
      emit(o, z, wrote);
      free(z);
   }
#endif
}

/* A PURGE body: the lists as they are, padded to 4 octets, a segment
 * for each run of octets not zero, and the SHA-1 of all of it */
static void purge(struct out *o, const uint8_t *lists, size_t lists_len,
      const uint8_t *d, size_t len)
{
   size_t from = o->len, at = 0;
   emit(o, lists, lists_len);
   while (lists_len && (o->len - from) & 3)
      emit(o, NULL, 1);
   while (at < len)
   {
      size_t  end, gap;
      uint8_t h[8];
      while (at < len && !d[at])
         at++;
      if (at == len)
         break;
      /* a run ends at eight zeros, or now and then sooner */
      for (end = at, gap = 0; end < len && gap < 8; end++)
         gap = d[end] ? 0 : gap + 1;
      end -= gap;
      if (end - at > 64 && (rnd() & 1))
         end = at + 1 + rnd() % (end - at - 1);
      put32(h, (uint32_t)at);
      put32(h + 4, (uint32_t)(end - at));
      emit(o, h, 8);
      emit(o, d + at, end - at);
      at = end;
   }
   {
      uint8_t hash[20];
      SHA1Digest(o->d + from, o->len - from, hash);
      emit(o, hash, 20);
   }
}

struct exc
{
   uint32_t sector;   /* in the partition's data */
   uint16_t at;       /* in its hash area */
   uint8_t  hash[20];
};

struct writer
{
   const struct config *cfg;
   struct out file;      /* groups first, the tables after */
   struct out groups;    /* the group table */
   uint32_t   n_groups;
   int        compressed_any, packed_any, zero_any;
};

/* The packed form of [a, b) of @sp: literal runs, and seeds where the
 * padding is. */
static void pack(struct out *o, const struct space *sp, uint64_t a, uint64_t b)
{
   uint64_t at = a;
   uint8_t  n[4];
   while (at < b)
   {
      const struct junk *best = NULL;
      unsigned i;
      for (i = 0; i < sp->nj; i++)
      {
         const struct junk *j = &sp->j[i];
         if (j->off + j->len <= at || j->off >= b)
            continue;
         if (!best || j->off < best->off)
            best = j;
      }
      if (best && best->off <= at)
      {
         uint64_t e = best->off + best->len < b ? best->off + best->len : b;
         put32(n, 0x80000000u | (uint32_t)(e - at));
         emit(o, n, 4);
         emit(o, best->seed, 68);
         at = e;
      }
      else
      {
         uint64_t e = best ? best->off : b;
         put32(n, (uint32_t)(e - at));
         emit(o, n, 4);
         emit(o, sp->d + at, (size_t)(e - at));
         at = e;
      }
   }
}

/* One group: @lists (@lists_len octets) then the data [a, b) of @sp */
static void add_group(struct writer *w, const struct space *sp,
      uint64_t a, uint64_t b, const uint8_t *lists, size_t lists_len,
      int has_exc)
{
   const struct config *cfg = w->cfg;
   struct out body;
   uint8_t    ent[12];
   uint32_t   packed = 0, size;
   /* RVZ may store a group as it is in any method; WIA compresses all */
   int        compressible = cfg->method != M_NONE && cfg->method != M_PURGE;
   int        compress = compressible && (cfg->wia || (rnd() & 3) != 0);
   int        zero = !has_exc, junky = 0;
   size_t     ent_len = cfg->wia ? 8 : 12;
   unsigned   j;
   uint64_t   x;
   size_t     data_at;

   for (x = a; x < b && zero; x++)
      zero = !sp->d[x];
   memset(&body, 0, sizeof(body));
   while (w->file.len & 3)
      emit(&w->file, NULL, 1);
   if (zero && (rnd() & 1))
   {
      /* stored as nothing */
      put32(ent, (uint32_t)(w->file.len / 4));
      put32(ent + 4, 0);
      put32(ent + 8, 0);
      emit(&w->groups, ent, ent_len);
      w->n_groups++;
      w->zero_any = 1;
      return;
   }
   if (cfg->method == M_PURGE)
      purge(&body, lists, lists_len, sp->d + a, (size_t)(b - a));
   else
   {
      emit(&body, lists, lists_len);
      if (lists_len && !compress)
         while (body.len & 3)
            emit(&body, NULL, 1);
      data_at = body.len;
      for (j = 0; j < sp->nj && !junky && !cfg->wia; j++)
         junky = sp->j[j].len && sp->j[j].off < b && sp->j[j].off + sp->j[j].len > a;
      if (junky && (!w->packed_any || (rnd() % 4) != 0))
      {
         pack(&body, sp, a, b);
         packed = (uint32_t)(body.len - data_at);
         w->packed_any = 1;
      }
      else
         emit(&body, sp->d + a, (size_t)(b - a));
   }

   put32(ent, (uint32_t)(w->file.len / 4));
   if (compress)
   {
      size_t at = w->file.len;
      squeeze(cfg->method, &w->file, body.d, body.len);
      size = (uint32_t)(w->file.len - at) | (cfg->wia ? 0 : 0x80000000u);
      w->compressed_any = 1;
   }
   else
   {
      emit(&w->file, body.d, body.len);
      size = (uint32_t)body.len;
   }
   put32(ent + 4, size);
   put32(ent + 8, packed);
   emit(&w->groups, ent, ent_len);
   w->n_groups++;
   free(body.d);
}

/* A table, compressed as the file's method says */
static void add_table(struct writer *w, const struct out *t, uint64_t *off, uint32_t *size)
{
   while (w->file.len & 3)
      emit(&w->file, NULL, 1);
   *off = w->file.len;
   if (w->cfg->method != M_NONE && w->cfg->method != M_PURGE)
      squeeze(w->cfg->method, &w->file, t->d, t->len);
   else
      emit(&w->file, t->d, t->len);
   *size = (uint32_t)(w->file.len - *off);
}


/* The groups of raw data [start, start + size) of the disc */
static void add_raw(struct writer *w, struct out *raw, const struct space *disc,
      uint64_t start, uint64_t size)
{
   uint8_t  ent[24];
   uint64_t a = start - start % SECTOR, end = start + size;
   uint32_t first = w->n_groups;
   for (; a < end; a += w->cfg->chunk)
      add_group(w, disc, a, a + w->cfg->chunk < end ? a + w->cfg->chunk : end,
            NULL, 0, 0);
   put64(ent, start);
   put64(ent + 8, size);
   put32(ent + 16, first);
   put32(ent + 20, w->n_groups - first);
   emit(raw, ent, 24);
}

/* The exception lists of partition sectors [s, s + count), a group */
static void lists_for(const struct writer *w, const struct exc *e, unsigned ne,
      uint32_t s, uint32_t count, struct out *lists, int *has)
{
   unsigned n_lists = w->cfg->chunk >= BLOCK ? w->cfg->chunk / BLOCK : 1;
   unsigned l, i;
   *has = 0;
   for (l = 0; l < n_lists; l++)
   {
      uint32_t lo = s + (n_lists > 1 ? l * 64 : 0);
      uint32_t hi = n_lists > 1 ? lo + 64 : s + count;
      uint8_t  hdr[2];
      size_t   at = lists->len;
      unsigned n = 0;
      if (hi > s + count)
         hi = s + count;
      emit(lists, NULL, 2);
      for (i = 0; i < ne; i++)
         if (e[i].sector >= lo && e[i].sector < hi)
         {
            uint32_t off = (e[i].sector - lo) * 0x400 + e[i].at;
            hdr[0] = (uint8_t)(off >> 8);
            hdr[1] = (uint8_t)off;
            emit(lists, hdr, 2);
            emit(lists, e[i].hash, 20);
            n++;
         }
      lists->d[at]     = (uint8_t)(n >> 8);
      lists->d[at + 1] = (uint8_t)n;
      *has |= n != 0;
   }
}

/* ---- the Wii hashes, worked out one sector at a time ---- */

static void sector_hashes(const uint8_t *pdata, uint32_t n_sectors,
      uint32_t block, uint8_t *areas /* 64 x 0x400 */)
{
   static uint8_t zeros[SDATA];
   uint8_t  h2[160];
   unsigned s, i;
   memset(areas, 0, 64 * 0x400);
   for (s = 0; s < 64; s++)
   {
      uint32_t       sec = block * 64 + s;
      const uint8_t *d   = sec < n_sectors ? pdata + (size_t)sec * SDATA : zeros;
      for (i = 0; i < 31; i++)
         SHA1Digest(d + i * 0x400, 0x400, areas + s * 0x400 + i * 20);
   }
   for (s = 0; s < 64; s++)
   {
      unsigned g = s & ~7u;
      SHA1Digest(areas + s * 0x400, 620, areas + g * 0x400 + 0x280 + (s & 7) * 20);
   }
   for (s = 0; s < 64; s++)
      memcpy(areas + s * 0x400 + 0x280, areas + (s & ~7u) * 0x400 + 0x280, 160);
   for (i = 0; i < 8; i++)
      SHA1Digest(areas + i * 8 * 0x400 + 0x280, 160, h2 + i * 20);
   for (s = 0; s < 64; s++)
      memcpy(areas + s * 0x400 + 0x340, h2, 160);
}

/* ---- one image ---- */

struct image
{
   uint8_t *iso;
   uint64_t size;
   uint8_t *file;       /* the RVZ file, in memory */
   size_t   file_len;
   unsigned n_exc;
};

static void put_be32_iso(uint8_t *iso, uint64_t off, uint32_t v)
{
   put32(iso + off, v);
}

/* The file is written leaving room before the groups for the heads */
static void reserve_heads(struct writer *w)
{
   emit(&w->file, NULL, 0x48 + 0xdc);
}

static bool write_rvz(const struct config *cfg, const char *path, struct image *img)
{
   struct writer w;
   struct out    raw, part, lists;
   struct space  disc, pd;
   uint8_t       head[0x48], dt[0xdc], key[16];
   uint64_t      raw_off = 0, group_off = 0;
   uint32_t      raw_size = 0, group_size = 0, n_raw = 0;
   struct exc   *exc = (struct exc*)calloc(MAX_EXC, sizeof(*exc));
   unsigned      ne = 0, i;
   FILE         *f;
   /* the Wii layout: partition at 0x50000, its data from 0x70000 */
   const uint64_t part_at = 0x50000, data_at = 0x70000;
   const uint64_t data_end = data_at + (uint64_t)n_sectors * SECTOR;

   memset(&w, 0, sizeof(w));
   memset(&raw, 0, sizeof(raw));
   memset(&part, 0, sizeof(part));
   memset(&disc, 0, sizeof(disc));
   memset(&pd, 0, sizeof(pd));
   w.cfg = cfg;
   reserve_heads(&w);

   disc.len  = cfg->wii ? data_end + 0x13000
             : (uint64_t)(n_sectors < 180 ? 180 : n_sectors) * SECTOR + 0x1234;
   disc.d    = (uint8_t*)calloc(1, (size_t)disc.len);
   img->size = disc.len;
   img->n_exc = 0;

   if (!cfg->wii)
   {
      fill(&disc, 0x80, disc.len, cfg->chunk >= 0x20000 ? cfg->chunk + 0x300 : 0);
      memcpy(disc.d, "GALE01", 6);
      put32(disc.d + 0x1c, 0xc2339f3d);
   }
   else
   {
      /* the disc's own areas, then the partition's data */
      fill(&disc, 0x80, data_at, 0);
      fill(&disc, data_end, disc.len, 0);
      memcpy(disc.d, "RSBE01", 6);
      put32(disc.d + 0x18, 0x5d1c9ea3);
      disc.d[0x61] = cfg->encrypted ? 0 : 1;
      disc.d[0x60] = 0;
      memset(disc.d + 0x40000, 0, 0x40);
      put32(disc.d + 0x40000, 1);
      put32(disc.d + 0x40004, 0x40020 >> 2);
      put32(disc.d + 0x40020, (uint32_t)(part_at >> 2));
      put32(disc.d + 0x40024, 0);
      put_be32_iso(disc.d, part_at + 0x2a4, 0x208);
      put_be32_iso(disc.d, part_at + 0x2a8, 0x2c0 >> 2);
      put_be32_iso(disc.d, part_at + 0x2b8, (uint32_t)((data_at - part_at) >> 2));
      put_be32_iso(disc.d, part_at + 0x2bc, (uint32_t)((data_end - data_at) >> 2));
      unjunk(&disc, 0x40000, 0x40);
      unjunk(&disc, part_at + 0x2a0, 0x20);

      pd.len = (uint64_t)n_sectors * SDATA;
      pd.d   = (uint8_t*)calloc(1, (size_t)pd.len);
      fill(&pd, 0, pd.len, cfg->chunk < 0x20000 ? 0
            : (uint64_t)(cfg->chunk < 16 * SECTOR ? cfg->chunk / SECTOR : 4) * SDATA + 0x100);
      /* with one sector zero to the end, as unused space often is */
      memset(pd.d + 70 * SDATA, 0, SDATA);
      unjunk(&pd, 70 * SDATA, SDATA);

      for (i = 0; i < 16; i++)
         key[i] = (uint8_t)rnd();

      /* each hash block hashed, the exceptions laid over the hashes,
       * then encrypted one sector at a time */
      {
         uint8_t       *areas = (uint8_t*)malloc(64 * 0x400);
         struct aes_ctx aes;
         uint32_t       b, s;
         aes_init(&aes, key, 16);
         for (b = 0; b * 64 < n_sectors; b++)
         {
            sector_hashes(pd.d, n_sectors, b, areas);
            if (cfg->exceptions)
            {
               unsigned k, n = 3 + rnd() % 20;
               for (k = 0; k < n && ne < MAX_EXC; k++)
               {
                  struct exc *e = &exc[ne];
                  uint32_t    span = n_sectors - b * 64 < 64 ? n_sectors - b * 64 : 64;
                  unsigned    where = rnd() % 4, h;
                  e->sector = b * 64 + rnd() % span;
                  /* in H0, H1, H2, or the padding after each */
                  e->at = (uint16_t)(where == 0 ? 20 * (rnd() % 31)
                        : where == 1 ? 0x280 + 20 * (rnd() % 8)
                        : where == 2 ? 0x340 + 20 * (rnd() % 8)
                        : (rnd() & 1) ? 0x26c : 0x3ec);
                  for (h = 0; h < 20; h++)
                     e->hash[h] = (uint8_t)rnd();
                  memcpy(areas + (e->sector - b * 64) * 0x400 + e->at, e->hash, 20);
                  ne++;
               }
            }
            for (s = 0; s < 64 && b * 64 + s < n_sectors; s++)
            {
               uint8_t *sec = disc.d + data_at + (size_t)(b * 64 + s) * SECTOR;
               memcpy(sec, areas + s * 0x400, 0x400);
               memcpy(sec + 0x400, pd.d + (size_t)(b * 64 + s) * SDATA, SDATA);
               if (cfg->encrypted)
               {
                  uint8_t iv[16];
                  memset(iv, 0, 16);
                  aes_cbc_encrypt(&aes, iv, sec, sec, 0x400);
                  memcpy(iv, sec + 0x3d0, 16);
                  aes_cbc_encrypt(&aes, iv, sec + 0x400, sec + 0x400, SDATA);
               }
            }
         }
         free(areas);
      }
      img->n_exc = ne;
   }

   /* the groups: raw data, then the partition's segments */
   n_raw++;
   add_raw(&w, &raw, &disc, 0x80, (cfg->wii ? data_at : disc.len) - 0x80);
   if (cfg->wii)
   {
      uint32_t seg_len[2], seg_first[2], k, spg = cfg->chunk / SECTOR;
      uint8_t  ent[48];
      seg_len[0]   = cfg->segments == 2 ? 64 : cfg->segments == 3 ? 37 : n_sectors;
      seg_len[1]   = n_sectors - seg_len[0];
      seg_first[0] = 0;
      seg_first[1] = seg_len[0];
      memset(ent, 0, sizeof(ent));
      memcpy(ent, key, 16);
      for (k = 0; k < 2; k++)
      {
         uint32_t first_group = w.n_groups, s;
         for (s = seg_first[k]; s < seg_first[k] + seg_len[k]; s += spg)
         {
            uint32_t cnt = seg_first[k] + seg_len[k] - s < spg
                         ? seg_first[k] + seg_len[k] - s : spg;
            int      has = 0;
            memset(&lists, 0, sizeof(lists));
            lists_for(&w, exc, ne, s, cnt, &lists, &has);
            add_group(&w, &pd, (uint64_t)s * SDATA, (uint64_t)(s + cnt) * SDATA,
                  lists.d, lists.len, has);
            free(lists.d);
         }
         put32(ent + 16 + 16 * k, seg_len[k] ? (uint32_t)(data_at / SECTOR) + seg_first[k] : 0);
         put32(ent + 20 + 16 * k, seg_len[k]);
         put32(ent + 24 + 16 * k, first_group);
         put32(ent + 28 + 16 * k, w.n_groups - first_group);
      }
      emit(&part, ent, 48);
      n_raw++;
      add_raw(&w, &raw, &disc, data_end, disc.len - data_end);
   }

   /* the tables after the groups, then the two heads */
   add_table(&w, &raw, &raw_off, &raw_size);
   add_table(&w, &w.groups, &group_off, &group_size);
   memset(dt, 0, sizeof(dt));
   put32(dt + 0x00, cfg->wii ? 2 : 1);
   put32(dt + 0x04, (uint32_t)cfg->method);
   put32(dt + 0x08, 1);
   put32(dt + 0x0c, cfg->chunk);
   memcpy(dt + 0x10, disc.d, 0x80);
   put32(dt + 0x90, part.len ? 1 : 0);
   put32(dt + 0x94, 48);
   {
      uint64_t at = w.file.len;
      emit(&w.file, part.d, part.len);
      put64(dt + 0x98, at);
      SHA1Digest(part.d ? part.d : dt, part.len, dt + 0xa0);
   }
   put32(dt + 0xb4, n_raw);
   put64(dt + 0xb8, raw_off);
   put32(dt + 0xc0, raw_size);
   put32(dt + 0xc4, w.n_groups);
   put64(dt + 0xc8, group_off);
   put32(dt + 0xd0, group_size);
   /* the LZMA properties as the 7-Zip SDK stores them: LZMA's octet
    * and dictionary size, LZMA2's dictionary code */
   if (cfg->method == M_LZMA)
   {
      dt[0xd4] = 5;
      dt[0xd5] = LZ_PROPS;
      dt[0xd8] = 0x40;   /* 4 MiB, little endian */
   }
   else if (cfg->method == M_LZMA2)
   {
      dt[0xd4] = 1;
      dt[0xd5] = 22;     /* 4 MiB */
   }

   memset(head, 0, sizeof(head));
   memcpy(head, cfg->wia ? "WIA\x01" : "RVZ\x01", 4);
   put32(head + 4, 0x01000000);
   put32(head + 8, 0x00030000);
   put32(head + 0x0c, sizeof(dt));
   SHA1Digest(dt, sizeof(dt), head + 0x10);
   put64(head + 0x24, disc.len);
   put64(head + 0x2c, w.file.len);
   SHA1Digest(head, 0x34, head + 0x34);
   memcpy(w.file.d, head, 0x48);
   memcpy(w.file.d + 0x48, dt, sizeof(dt));

   if (cfg->method != M_NONE && cfg->method != M_PURGE && !w.compressed_any)
      fail("the writer stored no group compressed", cfg->name);
   if (!cfg->wia && !w.packed_any)
      fail("the writer packed no group", cfg->name);

   img->iso      = disc.d;
   img->file     = w.file.d;
   img->file_len = w.file.len;
   /* kept on disk only when asked to be */
   if (path)
   {
      f = fopen(path, "wb");
      if (!f || fwrite(w.file.d, 1, w.file.len, f) != w.file.len)
         fail("cannot write the file", cfg->name);
      if (f)
         fclose(f);
   }
   free(w.groups.d);
   free(raw.d);
   free(part.d);
   free(pd.d);
   free(exc);
   return true;
}

/* ---- the reader, fed from memory ---- */

static const uint8_t *feed_lo, *feed_hi; /* the read's own destination */
static unsigned long  fed_in_place;      /* requests made straight into it */

/* What the reader asks for, from the file in memory: now and then
 * short, which it must ask for again, and nothing past the end. */
static void supply(rvz_t *z, const rvz_request_t *req,
      const uint8_t *file, size_t file_len)
{
   size_t n = 0;
   if (req->dst >= feed_lo && req->dst < feed_hi)
      fed_in_place++;
   if (req->offset < file_len)
   {
      n = file_len - (size_t)req->offset;
      if (n > req->length)
         n = req->length;
      if (n > 1 && (rnd() & 7) == 0)
         n = 1 + rnd() % n;
      memcpy(req->dst, file + req->offset, n);
   }
   rvz_feed(z, n);
}

static rvz_t *open_mem(const uint8_t *file, size_t file_len)
{
   rvz_t        *z = rvz_new();
   rvz_request_t req;
   int           e;
   if (!z)
      return NULL;
   while ((e = rvz_open_step(z, &req)) == RVZ_PENDING)
      supply(z, &req, file, file_len);
   if (e != RVZ_OK)
   {
      rvz_free(z);
      return NULL;
   }
   return z;
}

static int64_t read_mem(rvz_t *z, const struct image *img,
      uint64_t off, void *dst, size_t len)
{
   rvz_request_t req;
   int           e;
   if (rvz_read_begin(z, off, dst, len) != RVZ_OK)
      return -1;
   feed_lo = (const uint8_t*)dst;
   feed_hi = feed_lo + len;
   while ((e = rvz_read_step(z, &req)) == RVZ_PENDING)
      supply(z, &req, img->file, img->file_len);
   feed_lo = feed_hi = NULL;
   return e == RVZ_OK ? (int64_t)rvz_read_progress(z) : -1;
}

/* ---- the checks ---- */

static void check_image(const struct config *cfg, const struct image *img)
{
   static uint8_t buf[0x300000];
   rvz_t   *z = open_mem(img->file, img->file_len);
   uint64_t at;
   unsigned i;

   if (!z)
   {
      fail("open", cfg->name);
      return;
   }
   if (rvz_disc_type(z) != (cfg->wii ? RVZ_DISC_WII : RVZ_DISC_GAMECUBE))
      fail("disc type", cfg->name);
   if (rvz_disc_size(z) != img->size)
      fail("disc size", cfg->name);

   /* the whole image in order, in pieces of every size */
   for (at = 0; at < img->size; )
   {
      size_t  want = 1 + rnd() % 0x50000;
      int64_t got  = read_mem(z, img, at, buf, want);
      size_t  left = (size_t)(img->size - at);
      if (got != (int64_t)(want < left ? want : left)
            || memcmp(buf, img->iso + at, (size_t)got))
      {
         uint64_t k;
         for (k = 0; got > 0 && k < (uint64_t)got && buf[k] == img->iso[at + k]; k++) ;
         fprintf(stderr, "  at 0x%llx: got %lld, first difference at 0x%llx\n",
               (unsigned long long)at, (long long)got, (unsigned long long)(at + k));
         fail("reading in order", cfg->name);
         break;
      }
      at += (uint64_t)got;
   }

   /* anywhere, any size, across every boundary */
   /* fewer where a group is slow to decode: LZMA and bzip2, and 2 MiB
    * groups of literals the LZMA encoder here makes */
   for (i = 0; i < (cfg->method == M_LZMA || cfg->method == M_LZMA2
            || cfg->method == M_BZIP2 ? 12u : 300u); i++)
   {
      uint64_t off  = ((uint64_t)rnd() << 16 ^ rnd()) % img->size;
      size_t   want = (i & 7) == 0 ? 1 + rnd() % sizeof(buf) : 1 + rnd() % 0x9000;
      int64_t  got  = read_mem(z, img, off, buf, want);
      size_t   left = (size_t)(img->size - off);
      if (got != (int64_t)(want < left ? want : left)
            || memcmp(buf, img->iso + off, (size_t)got))
      {
         fprintf(stderr, "  at 0x%llx + 0x%lx\n", (unsigned long long)off, (unsigned long)want);
         fail("reading anywhere", cfg->name);
         break;
      }
   }

   /* at and past the end */
   if (read_mem(z, img, img->size, buf, 16) != 0 || read_mem(z, img, img->size + 5, buf, 16) != 0)
      fail("a read past the end", cfg->name);
   if (read_mem(z, img, img->size - 3, buf, 16) != 3 || memcmp(buf, img->iso + img->size - 3, 3))
      fail("a read over the end", cfg->name);
   rvz_free(z);

   /* a range that does not come fails the read, and the next read
    * goes on as before; on a reader holding nothing, so that the read
    * has to ask */
   z = open_mem(img->file, img->file_len);
   if (z)
   {
      rvz_request_t req;
      int           e = RVZ_OK;
      if (rvz_read_begin(z, img->size / 2, buf, 0x40000) == RVZ_OK)
         while ((e = rvz_read_step(z, &req)) == RVZ_PENDING)
            if (rvz_feed(z, 0) != RVZ_ERROR_DATA)
               break;
      if (e != RVZ_ERROR_DATA)
         fail("a range that did not come", cfg->name);
      if (read_mem(z, img, img->size / 2, buf, 0x40000) != 0x40000
            || memcmp(buf, img->iso + img->size / 2, 0x40000))
         fail("a read after a failed one", cfg->name);
      rvz_free(z);
   }
}

/* A damaged head, a damaged disc struct, and a cut-off file are not
 * read as images */
static void check_damage(const struct config *cfg, const struct image *img)
{
   uint8_t *d = (uint8_t*)malloc(img->file_len);
   unsigned k;
   memcpy(d, img->file, img->file_len);
   for (k = 0; k < 3; k++)
   {
      rvz_t *z;
      size_t at = k == 0 ? 0x24 : k == 1 ? 0x48 + 0x0c : 0;
      if (k < 2)
         d[at] ^= 0x40;
      if ((z = open_mem(d, k == 2 ? 0x100 : img->file_len)))
      {
         fail(k == 0 ? "opened with a damaged head"
               : k == 1 ? "opened with a damaged disc struct"
               : "opened cut off", cfg->name);
         rvz_free(z);
      }
      if (k < 2)
         d[at] ^= 0x40;
   }
   free(d);
   /* nothing is read before the image is open */
   {
      rvz_t  *z = rvz_new();
      uint8_t b[4];
      if (z && rvz_read_begin(z, 0, b, sizeof(b)) != RVZ_ERROR_STATE)
         fail("a read armed before opening", cfg->name);
      rvz_free(z);
   }
}

int main(int argc, char **argv)
{
   static const struct config cfgs[] = {
      /* name                wii chunk    method   enc seg exc wia */
      { "gc-32k-none",       0, 0x8000,   M_NONE,  0, 1, 0, 0 },
      { "gc-128k-zstd",      0, 0x20000,  M_ZSTD,  0, 1, 0, 0 },
      { "gc-2m-zstd",        0, 0x200000, M_ZSTD,  0, 1, 0, 0 },
      { "gc-4m-none",        0, 0x400000, M_NONE,  0, 1, 0, 0 },
      { "wii-32k-zstd",      1, 0x8000,   M_ZSTD,  1, 2, 1, 0 },
      { "wii-32k-none",      1, 0x8000,   M_NONE,  1, 1, 1, 0 },
      { "wii-128k-zstd",     1, 0x20000,  M_ZSTD,  1, 2, 1, 0 },
      { "wii-128k-none",     1, 0x20000,  M_NONE,  1, 2, 1, 0 },
      { "wii-512k-zstd",     1, 0x80000,  M_ZSTD,  1, 1, 1, 0 },
      { "wii-2m-zstd",       1, 0x200000, M_ZSTD,  1, 2, 1, 0 },
      { "wii-2m-none",       1, 0x200000, M_NONE,  1, 1, 1, 0 },
      { "wii-4m-zstd",       1, 0x400000, M_ZSTD,  1, 2, 1, 0 },
      { "wii-4m-none",       1, 0x400000, M_NONE,  1, 1, 1, 0 },
      { "wii-128k-plain",    1, 0x20000,  M_ZSTD,  0, 2, 1, 0 },
      { "wii-32k-seg37",     1, 0x8000,   M_ZSTD,  1, 3, 1, 0 },
      { "wii-128k-seg37",    1, 0x20000,  M_NONE,  1, 3, 1, 0 },
      { "wii-2m-no-exc",     1, 0x200000, M_ZSTD,  1, 2, 0, 0 },
      { "gc-128k-lzma",      0, 0x20000,  M_LZMA,  0, 1, 0, 0 },
      { "wii-128k-lzma",     1, 0x20000,  M_LZMA,  1, 2, 1, 0 },
      { "gc-2m-lzma2",       0, 0x200000, M_LZMA2, 0, 1, 0, 0 },
      { "wii-32k-lzma2",     1, 0x8000,   M_LZMA2, 1, 3, 1, 0 },
      { "wia-gc-2m-none",    0, 0x200000, M_NONE,  0, 1, 0, 1 },
      { "wia-gc-2m-purge",   0, 0x200000, M_PURGE, 0, 1, 0, 1 },
      { "wia-wii-2m-purge",  1, 0x200000, M_PURGE, 1, 2, 1, 1 },
      { "wia-wii-2m-lzma",   1, 0x200000, M_LZMA,  1, 2, 1, 1 },
      { "wia-wii-4m-lzma2",  1, 0x400000, M_LZMA2, 1, 2, 1, 1 },
      { "wia-gc-4m-lzma2",   0, 0x400000, M_LZMA2, 0, 1, 0, 1 },
      { "wii-128k-bzip2",    1, 0x20000,  M_BZIP2, 1, 2, 1, 0 },
      { "wia-wii-2m-bzip2",  1, 0x200000, M_BZIP2, 1, 2, 1, 1 },
      { "wia-gc-2m-bzip2",   0, 0x200000, M_BZIP2, 0, 1, 0, 1 }
   };
   const char *dir = argc > 1 ? argv[1] : NULL;
   unsigned c, run = 0;

   if (argc > 2 && atoi(argv[2]) >= 72)
      n_sectors = (uint32_t)atoi(argv[2]);

   rng_state = 0x2545f491u;
   for (c = 0; c < sizeof(cfgs) / sizeof(cfgs[0]); c++)
   {
      const struct config *cfg = &cfgs[c];
      struct image img;
      char path[1024];
#ifndef HAVE_RZSTD
      if (cfg->method == M_ZSTD)
         continue;
#endif
      if (dir && strlen(dir) > sizeof(path) - 64)
         return 2;
      sprintf(path, "%s%s%s.rvz", dir ? dir : "", dir ? "/" : "rvz_test_", cfg->name);
      if (!write_rvz(cfg, dir ? path : NULL, &img))
         continue;
      if (cfg->wii && cfg->exceptions && !img.n_exc)
         fail("the writer made no exceptions", cfg->name);
      check_image(cfg, &img);
      if (c == 0 || c == 6 || c == 18 || c == 24)
         check_damage(cfg, &img);
      free(img.iso);
      free(img.file);
      run++;
   }
   /* groups stored as they are go straight where the read asked */
   if (!fed_in_place)
      fail("no request was made into a read's own buffer", "all");
   if (failures)
   {
      fprintf(stderr, "FAIL: %d checks\n", failures);
      return 1;
   }
   printf("ok: %u images read back octet for octet, %lu requests made into the read's own buffer\n",
         run, fed_in_place);
   return 0;
}
