/* A RAR 5 archive of one member, written for rrar_archive_test.c: what a
 * test cannot get from an archiver, members bigger than the ring they
 * stream round and filters' blocks across where it comes round.
 *
 * The member is one block of LZ symbols with a delta or E8 filter now
 * and then, as RAR 5 lays them out: codes of fixed lengths (every
 * literal and length slot 9 bits, every distance slot 6, every low
 * distance 4), so the coding is plain and only the symbols matter.
 * r5_member() writes the symbols as it makes up what they decode to;
 * r5_expect() runs the filters over that the way a reader does, which
 * gives the member as it comes out. */

#ifndef RAR5_WRITE_H
#define RAR5_WRITE_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <encodings/crc32.h>

/* ------------------------------------------------------------- the bits */

typedef struct
{
   uint8_t *d;
   size_t   len;
   size_t   cap;
   uint32_t acc;      /* bits not yet whole octets, the first highest */
   unsigned n;
} r5_bits_t;

static void r5_byte(r5_bits_t *b, uint8_t v)
{
   if (b->len == b->cap)
   {
      b->cap = b->cap ? b->cap * 2 : 1 << 16;
      b->d   = (uint8_t*)realloc(b->d, b->cap);
   }
   b->d[b->len++] = v;
}

/* @n bits of @v, the highest first, as the reader takes them */
static void r5_put(r5_bits_t *b, uint32_t v, unsigned n)
{
   while (n)
   {
      unsigned k = n > 8 ? 8 : n;
      n         -= k;
      b->acc     = (b->acc << k) | ((v >> n) & ((1u << k) - 1));
      b->n      += k;
      if (b->n >= 8)
      {
         b->n -= 8;
         r5_byte(b, (uint8_t)(b->acc >> b->n));
      }
   }
}

/* ---------------------------------------------------------- the symbols */

/* The code tables: a code of 20 whose every length is 5, and in it the
 * lengths of the four codes, each length written as itself */
static void r5_tables(r5_bits_t *b)
{
   unsigned i;
   for (i = 0; i < 20; i++)
      r5_put(b, 5, 4);
   for (i = 0; i < 306; i++)
      r5_put(b, 9, 5);
   for (i = 0; i < 64; i++)
      r5_put(b, 6, 5);
   for (i = 0; i < 16; i++)
      r5_put(b, 4, 5);
   for (i = 0; i < 44; i++)
      r5_put(b, 6, 5);
}

static void r5_literal(r5_bits_t *b, uint8_t c)
{
   r5_put(b, c, 9);
}

/* @len octets from @dist back */
static void r5_match(r5_bits_t *b, uint32_t len, uint32_t dist)
{
   uint32_t l = len, base, d = dist - 1;
   unsigned slot, bits;

   /* the reader adds one for each of these */
   if (dist > 0x100)
      l--;
   if (dist > 0x2000)
      l--;
   if (dist > 0x40000)
      l--;
   l -= 2;
   if (l < 8)
   {
      slot = l;
      bits = 0;
      base = l;
   }
   else
   {
      for (slot = 8; ; slot++)
      {
         bits = slot / 4 - 1;
         base = (uint32_t)(4 | (slot & 3)) << bits;
         if (l < base + (1u << bits))
            break;
      }
   }
   r5_put(b, 262 + slot, 9);
   if (bits)
      r5_put(b, l - base, bits);

   if (d < 4)
   {
      r5_put(b, d, 6);
      return;
   }
   for (slot = 4; ; slot++)
   {
      bits = slot / 2 - 1;
      base = (uint32_t)(2 | (slot & 1)) << bits;
      if (d < base + (1u << bits))
         break;
   }
   r5_put(b, slot, 6);
   d -= base;
   if (bits >= 4)
   {
      if (bits > 4)
         r5_put(b, d >> 4, bits - 4);
      r5_put(b, d & 15, 4);
   }
   else
      r5_put(b, d, bits);
}

static void r5_number(r5_bits_t *b, uint32_t v)
{
   unsigned bytes = v < 0x100 ? 1 : v < 0x10000 ? 2 : v < 0x1000000 ? 3 : 4;
   unsigned i;
   r5_put(b, bytes - 1, 2);
   for (i = 0; i < bytes; i++)
      r5_put(b, (v >> (i * 8)) & 0xff, 8);
}

/* A filter over @length octets from @ahead on: delta (@type 0) over
 * @channels, or E8 (1) */
static void r5_filter(r5_bits_t *b, uint32_t ahead, uint32_t length,
      unsigned type, unsigned channels)
{
   r5_put(b, 256, 9);
   r5_number(b, ahead);
   r5_number(b, length);
   r5_put(b, type, 3);
   if (type == 0)
      r5_put(b, channels - 1, 5);
}

/* ---------------------------------------------------------- the member */

typedef struct
{
   uint64_t start;
   uint32_t length;
   unsigned type;
   unsigned channels;
} r5_filter_t;

static uint32_t r5_rng;

static uint32_t r5_rand(void)
{
   r5_rng = r5_rng * 1103515245u + 12345u;
   return r5_rng >> 8;
}

/* What the member's symbols decode to, @n octets of it into @d, and the
 * symbols into @b: runs of literals and matches up to @reach back, and
 * the @nf filters' records ahead of their blocks. In an E8 filter's
 * block the literals have E8 octets among them. */
static void r5_member(r5_bits_t *b, uint8_t *d, size_t n, uint32_t reach,
      const r5_filter_t *f, unsigned nf)
{
   size_t   pos = 0;
   unsigned next = 0;

   r5_tables(b);
   while (pos < n)
   {
      uint32_t r = r5_rand();
      unsigned k;
      int      e8 = 0;

      /* a filter's record, a thousand octets or more ahead of its block */
      if (next < nf && pos + 2000 >= f[next].start)
      {
         r5_filter(b, (uint32_t)(f[next].start - pos), f[next].length,
               f[next].type, f[next].channels);
         next++;
      }
      for (k = 0; k < nf; k++)
         if (f[k].type == 1 && pos >= f[k].start
               && pos < f[k].start + f[k].length)
            e8 = 1;

      if (pos < 64 || r % 8 == 0)
      {
         /* a few literals */
         unsigned cnt = 1 + (r >> 4) % 16;
         for (; cnt && pos < n; cnt--)
         {
            uint32_t v = r5_rand();
            uint8_t  c = e8 && v % 5 == 0 ? 0xe8 : (uint8_t)('a' + v % 16);
            r5_literal(b, c);
            d[pos++] = c;
         }
      }
      else
      {
         /* a match: near most often, now and then far */
         uint32_t len  = 3 + (r >> 4) % 1000;
         uint32_t most = pos < reach ? (uint32_t)pos : reach;
         uint32_t dist = (r >> 20) % 4 == 0
            ? 1 + r5_rand() % most
            : 1 + r5_rand() % (most < 65536 ? most : 65536);
         uint32_t i;
         if (len > n - pos)
            len = (uint32_t)(n - pos);
         if (len < 5)
            len = 5;
         if (len > n - pos)
         {
            r5_literal(b, 'a');
            d[pos++] = 'a';
            continue;
         }
         r5_match(b, len, dist);
         for (i = 0; i < len; i++, pos++)
            d[pos] = d[pos - dist];
      }
   }
}

/* The filters run over @d, @n octets, as a reader does */
static void r5_expect(uint8_t *d, const r5_filter_t *f, unsigned nf)
{
   unsigned k;
   for (k = 0; k < nf; k++)
   {
      uint8_t *m   = d + (size_t)f[k].start;
      uint32_t len = f[k].length;
      if (f[k].type == 0)
      {
         uint8_t *src = (uint8_t*)malloc(len);
         uint32_t c, i, at = 0;
         memcpy(src, m, len);
         for (c = 0; c < f[k].channels; c++)
         {
            uint8_t last = 0;
            for (i = c; i < len; i += f[k].channels)
               last = m[i] = (uint8_t)(last - src[at++]);
         }
         free(src);
      }
      else
      {
         const uint32_t size = 0x1000000;
         uint32_t       i;
         for (i = 0; i + 4 < len;)
         {
            if (m[i++] == 0xe8)
            {
               uint32_t off  = (uint32_t)((f[k].start + i) % size);
               uint32_t addr = (uint32_t)m[i] | ((uint32_t)m[i + 1] << 8)
                  | ((uint32_t)m[i + 2] << 16) | ((uint32_t)m[i + 3] << 24);
               uint32_t to   = addr;
               if (addr & 0x80000000u)
               {
                  if (!((addr + off) & 0x80000000u))
                     to = addr + size;
               }
               else if ((addr - size) & 0x80000000u)
                  to = addr - off;
               m[i]     = (uint8_t)to;
               m[i + 1] = (uint8_t)(to >> 8);
               m[i + 2] = (uint8_t)(to >> 16);
               m[i + 3] = (uint8_t)(to >> 24);
               i += 4;
            }
         }
      }
   }
}

/* ------------------------------------------------------------ the archive */

static void r5_vint(r5_bits_t *o, uint64_t v)
{
   do
   {
      uint8_t c = (uint8_t)(v & 0x7f);
      v >>= 7;
      r5_byte(o, (uint8_t)(c | (v ? 0x80 : 0)));
   } while (v);
}

/* A header: its CRC-32, its size, then @body */
static void r5_header(r5_bits_t *o, const r5_bits_t *body)
{
   r5_bits_t h;
   uint32_t  crc;
   size_t    i;
   memset(&h, 0, sizeof(h));
   r5_vint(&h, body->len);
   for (i = 0; i < body->len; i++)
      r5_byte(&h, body->d[i]);
   crc = encoding_crc32(0, h.d, h.len);
   for (i = 0; i < 4; i++)
      r5_byte(o, (uint8_t)(crc >> (i * 8)));
   for (i = 0; i < h.len; i++)
      r5_byte(o, h.d[i]);
   free(h.d);
}

/* The archive of one member named @name, @size octets with @crc, in a
 * dictionary of 128 KiB << @dict, its symbols @b; the caller frees
 * what it returns */
static uint8_t *r5_archive(r5_bits_t *b, const char *name, uint64_t size,
      uint32_t crc, unsigned dict, size_t *len)
{
   static const uint8_t sig[8] = { 'R', 'a', 'r', '!', 0x1a, 7, 1, 0 };
   r5_bits_t o, h, blk;
   unsigned  last_bits = b->n ? b->n : 8;
   size_t    i, data;

   /* the last octet's bits, and the block's header */
   if (b->n)
      r5_put(b, 0, 8 - b->n);
   memset(&blk, 0, sizeof(blk));
   r5_byte(&blk, (uint8_t)(0x80 | 0x40 | (2 << 3) | (last_bits - 1)));
   r5_byte(&blk, (uint8_t)(0x5a ^ (0x80 | 0x40 | (2 << 3) | (last_bits - 1))
         ^ (b->len & 0xff) ^ ((b->len >> 8) & 0xff) ^ ((b->len >> 16) & 0xff)));
   r5_byte(&blk, (uint8_t)b->len);
   r5_byte(&blk, (uint8_t)(b->len >> 8));
   r5_byte(&blk, (uint8_t)(b->len >> 16));
   data = blk.len + b->len;

   memset(&o, 0, sizeof(o));
   for (i = 0; i < 8; i++)
      r5_byte(&o, sig[i]);
   /* the archive's header */
   memset(&h, 0, sizeof(h));
   r5_vint(&h, 1);
   r5_vint(&h, 0);
   r5_vint(&h, 0);
   r5_header(&o, &h);
   /* the file's: with data, its CRC-32, LZ at the best method */
   h.len = 0;
   r5_vint(&h, 2);
   r5_vint(&h, 2);
   r5_vint(&h, data);
   r5_vint(&h, 4);
   r5_vint(&h, size);
   r5_vint(&h, 0);
   for (i = 0; i < 4; i++)
      r5_byte(&h, (uint8_t)(crc >> (i * 8)));
   r5_vint(&h, (5u << 7) | ((uint64_t)dict << 10));
   r5_vint(&h, 0);
   r5_vint(&h, strlen(name));
   for (i = 0; name[i]; i++)
      r5_byte(&h, (uint8_t)name[i]);
   r5_header(&o, &h);
   for (i = 0; i < blk.len; i++)
      r5_byte(&o, blk.d[i]);
   for (i = 0; i < b->len; i++)
      r5_byte(&o, b->d[i]);
   /* the end */
   h.len = 0;
   r5_vint(&h, 5);
   r5_vint(&h, 0);
   r5_vint(&h, 0);
   r5_header(&o, &h);
   free(h.d);
   free(blk.d);
   *len = o.len;
   return o.d;
}

#endif
