/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rvz.c).
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

/* No file I/O: every range of the file is asked of the caller (see
 * rvz.h). An operation that needs one says so and returns, and is run
 * again from where it stood once the range has come; the work done
 * before it asked is kept, and what it asks for the second time is
 * the range that came.
 *
 * The disc is a sorted list of regions, each read from a run of
 * groups: raw data, stored as the disc has it, and Wii partition data,
 * stored decrypted with the hashes taken out. Partition data is put
 * back together a 2 MiB hash block at a time - the 64 sectors one H2
 * table covers - and kept, encrypted, for the reads that follow; raw
 * data is read from the last group decoded. */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/rvz.h>
#include <crypto/crypto.h>
#include <lrc_hash.h>
#include <7z/r7z_lzma.h>
#include <7z/r7z_lzma_stream.h>
#include <7z/r7z_lzma2.h>
#include <encodings/rbzip2.h>
#ifdef HAVE_RZSTD
#include <encodings/rzstd.h>
#endif

#define RVZ_HEAD_SIZE      0x48
#define RVZ_DISC_MIN       0xDC     /* every field this reads */
#define RVZ_DHEAD          0x80
#define RVZ_SECTOR         0x8000
#define RVZ_SECTOR_HASHES  0x400
#define RVZ_SECTOR_DATA    (RVZ_SECTOR - RVZ_SECTOR_HASHES)
#define RVZ_BLOCK_SECTORS  64
#define RVZ_BLOCK          (RVZ_BLOCK_SECTORS * RVZ_SECTOR)
#define RVZ_PART_ENTRY     48
#define RVZ_RAW_ENTRY      24
#define RVZ_GROUP_ENTRY    12
#define WIA_GROUP_ENTRY    8
#define RVZ_EXCEPTION      22
#define RVZ_COMP_NONE      0
#define RVZ_COMP_PURGE     1    /* WIA only */
#define RVZ_COMP_BZIP2     2
#define RVZ_COMP_LZMA      3
#define RVZ_COMP_LZMA2     4
#define RVZ_COMP_ZSTD      5    /* RVZ only */
#define RVZ_SEED_WORDS     17
#define RVZ_PRNG_WORDS     521
/* the groups one hash block can need: one per chunk, and one more
 * where a segment ends inside the block */
#define RVZ_MAX_SLOTS      (RVZ_BLOCK / RVZ_SECTOR + 1)

/* opening, a stage at a time */
#define RVZ_STAGE_HEAD     0
#define RVZ_STAGE_DISC     1
#define RVZ_STAGE_PARTS    2
#define RVZ_STAGE_GROUPS   3
#define RVZ_STAGE_RAW      4
#define RVZ_STAGE_OPEN     5
#define RVZ_STAGE_FAILED   6

struct rvz_group
{
   uint64_t off;         /* in the file */
   uint32_t size;        /* stored octets, 0: all zero */
   uint32_t packed;      /* the packed stream's size, 0 when not packed */
   bool     compressed;
};

/* A stretch of the disc and the run of groups it is read from. A
 * partition region is one of its partition's two data segments. */
struct rvz_region
{
   uint64_t start;
   uint64_t end;
   uint64_t origin;      /* where its first group starts */
   uint32_t group;
   uint32_t n_groups;
   int      part;        /* -1 for raw data */
};

struct rvz_part
{
   struct aes_ctx aes;
   uint32_t first[2];    /* each segment's first sector on the disc */
   uint32_t count[2];    /* and its sectors */
   uint32_t group[2];
   uint32_t n_groups[2];
   uint32_t base;        /* the partition data's first sector */
   uint32_t sectors;     /* through the end of the last segment */
};

/* One decoded group: the exception lists and the data after them */
struct rvz_slot
{
   uint8_t       *raw;      /* the group as stored or decompressed */
   uint8_t       *out;      /* the unpacked data, for packed groups */
   const uint8_t *data;     /* its data: into raw, out, or zeros */
   const uint8_t *exc;      /* its exception lists, NULL for none */
   size_t         raw_cap;
   size_t         out_cap;
   size_t         exc_len;
   uint32_t       group;    /* the group held, or UINT32_MAX */
};

struct rvz
{
   struct rvz_region *regions;
   struct rvz_part   *parts;
   struct rvz_group  *groups;
   struct rvz_slot   *slots;    /* for a hash block's groups */
   uint8_t           *zbuf;     /* a compressed group, as read */
   uint16_t          *probs;    /* the LZMA model, for LZMA and LZMA2 */
   rbzip2_t          *bz;       /* the bzip2 decoder, for bzip2 */
   uint8_t           *zeros;    /* a group's worth of zeroed data */
   uint8_t           *block;    /* the hash block reassembled */
   uint8_t           *hashes;   /* its 64 sectors' hash areas */
   uint8_t           *h0;       /* H0 hashes of a run of sectors */
   uint8_t            zero_h0[20]; /* of a KiB of zeros */
   /* Working state kept here rather than on the stack, which is 8 KiB
    * on a PSP or GX thread: the padding generator's, and a hash
    * block's sectors and the groups they came from. */
   uint32_t           junk[RVZ_PRNG_WORDS];
   const uint8_t     *sec_data[RVZ_BLOCK_SECTORS];
   struct rvz_slot   *held[RVZ_BLOCK_SECTORS];
   long               held_first[RVZ_BLOCK_SECTORS];
   uint64_t           disc_size;
   size_t             zbuf_cap;
   size_t             data_max; /* a group's largest data */
   struct rvz_slot    raw;      /* the last raw data group */
   /* the range asked for: 1 while it is coming, 2 once it has */
   uint64_t           fetch_off;
   uint8_t           *fetch_dst;
   size_t             fetch_len;
   size_t             fetch_got;
   int                fetching;
   bool               failed;
   /* opening: the stage, and the heads and tables as they come */
   int                stage;
   uint8_t           *disc;
   uint8_t           *tab;
   uint64_t           file_size;
   uint64_t           part_off, raw_off, group_off;
   uint32_t           disc_len, part_size, n_raw, raw_size, group_size;
   /* the armed read */
   uint8_t           *rd_dst;
   uint64_t           rd_off;
   size_t             rd_len;
   size_t             rd_done;
   bool               rd_armed;
   uint32_t           n_regions;
   uint32_t           n_parts;
   uint32_t           n_groups;
   uint32_t           chunk;
   uint32_t           compression;
   uint32_t           n_slots;
   uint32_t           block_part;  /* the hash block held: partition, */
   uint32_t           block_index; /* and block, or UINT32_MAX */
   int                disc_type;
   bool               encrypted;
   bool               wia;      /* WIA rather than RVZ */
   uint8_t            lzma_props[RLZMA_PROPS_SIZE]; /* LZMA2: [0] alone */
   uint8_t            head[RVZ_HEAD_SIZE];
   uint8_t            dhead[RVZ_DHEAD];
};

static uint32_t rvz_be32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static uint64_t rvz_be64(const uint8_t *p)
{
   return ((uint64_t)rvz_be32(p) << 32) | rvz_be32(p + 4);
}

/* @len octets of the file at @off into @dst: RVZ_OK once they are
 * there, RVZ_PENDING after asking for them. The same range asked for
 * again while it is coming is the same request. */
static int rvz_need(rvz_t *z, uint64_t off, uint8_t *dst, size_t len)
{
   if (     z->fetching && z->fetch_off == off
         && z->fetch_dst == dst && z->fetch_len == len)
   {
      if (z->fetching == 1)
         return RVZ_PENDING;
      z->fetching = 0;
      return RVZ_OK;
   }
   if (!len)
      return RVZ_OK;
   if (off > z->file_size || len > z->file_size - off)
      return RVZ_ERROR_DATA;
   z->fetching  = 1;
   z->fetch_off = off;
   z->fetch_dst = dst;
   z->fetch_len = len;
   z->fetch_got = 0;
   return RVZ_PENDING;
}

static bool rvz_grow(uint8_t **buf, size_t *cap, size_t need)
{
   uint8_t *p;
   if (need <= *cap)
      return true;
   if (!(p = (uint8_t*)realloc(*buf, need)))
      return false;
   *buf = p;
   *cap = need;
   return true;
}

/* An LZMA stream carries no end: a group is decoded as far as its
 * @n_lists exception lists say, then @tail octets more. */
static int rvz_lzma(rvz_t *z, uint8_t *dst, size_t cap,
      const uint8_t *src, size_t src_len, unsigned n_lists, size_t tail,
      size_t *len)
{
   rlzma_stream_t lz;
   size_t         want = 0, used = 0;
   unsigned       l;
   int            status;

   if (rlzma_stream_init(&lz, z->lzma_props, z->probs, dst, cap) != RLZMA_OK)
      return RVZ_ERROR_DATA;
   rlzma_stream_reset(&lz);
   /* each list's count, then its exceptions; then the rest */
   for (l = 0; l <= 2 * n_lists; l++)
   {
      size_t in = src_len - used;
      if (l == 2 * n_lists)
         want += tail;
      else if (!(l & 1))
         want += 2;
      else
         want += (((size_t)dst[want - 2] << 8) | dst[want - 1]) * RVZ_EXCEPTION;
      if (want > cap)
         return RVZ_ERROR_DATA;
      if (want == lz.dic_pos)
         continue;
      if (     rlzma_stream_decode(&lz, want, src + used, &in, 1, &status) != RLZMA_OK
            || lz.dic_pos != want)
         return RVZ_ERROR_DATA;
      used += in;
   }
   *len = want;
   return RVZ_OK;
}

/* @src (@src_len octets) decompressed with the file's method into @dst,
 * at most @cap octets; @len receives what came out. Where the method
 * does not mark its own end, the output is @n_lists exception lists
 * and then @tail octets. */
static int rvz_decompress(rvz_t *z, uint8_t *dst, size_t cap,
      const uint8_t *src, size_t src_len, unsigned n_lists, size_t tail,
      size_t *len)
{
   size_t in = src_len;
   int    status;

   *len = 0;
   if (!cap)
      return RVZ_ERROR_DATA;
   switch (z->compression)
   {
#ifdef HAVE_RZSTD
      case RVZ_COMP_ZSTD:
         return rzstd_decode(dst, cap, src, src_len, len) == RZSTD_PROCESS_END
            ? RVZ_OK : RVZ_ERROR_DATA;
#endif
      case RVZ_COMP_BZIP2:
         switch (rbzip2_decode(z->bz, dst, cap, src, src_len, len))
         {
            case RBZIP2_OK:
               return RVZ_OK;
            case RBZIP2_ERROR_MEM:
               return RVZ_ERROR_MEM;
            default:
               break;
         }
         return RVZ_ERROR_DATA;
      case RVZ_COMP_LZMA:
         /* the output is the window: a group is one stream */
         return rvz_lzma(z, dst, cap, src, src_len, n_lists, tail, len);
      case RVZ_COMP_LZMA2:
      {
         rlzma2_dec_t lz;
         if (rlzma2_dec_init(&lz, z->lzma_props[0], z->probs, dst, cap) != RLZMA_OK)
            return RVZ_ERROR_DATA;
         if (     rlzma2_dec_decode(&lz, cap, src, &in, 1, &status) != RLZMA_OK
               || status == RLZMA2_STATUS_NEEDS_MORE_INPUT)
            return RVZ_ERROR_DATA;
         *len = lz.lzma.dic_pos;
         return RVZ_OK;
      }
      default:
         break;
   }
   return RVZ_ERROR_UNSUPPORTED;
}

/* A table stored compressed with the file's method, or as it is, into
 * z->tab: exactly @len octets. */
static int rvz_table(rvz_t *z, uint64_t off, uint32_t stored, size_t len)
{
   int e;
   if (!z->tab && !(z->tab = (uint8_t*)malloc(len ? len : 1)))
      return RVZ_ERROR_MEM;
   /* tables are stored as they are under NONE and PURGE alike */
   if (z->compression == RVZ_COMP_NONE || z->compression == RVZ_COMP_PURGE)
   {
      if (stored < len)
         return RVZ_ERROR_DATA;
      return rvz_need(z, off, z->tab, len);
   }
   {
      size_t wrote = 0;
      if (!rvz_grow(&z->zbuf, &z->zbuf_cap, stored))
         return RVZ_ERROR_MEM;
      if ((e = rvz_need(z, off, z->zbuf, stored)) != RVZ_OK)
         return e;
      if ((e = rvz_decompress(z, z->tab, len, z->zbuf, stored, 0, len, &wrote)) != RVZ_OK)
         return e;
      return wrote == len ? RVZ_OK : RVZ_ERROR_DATA;
   }
}

/* A PURGE group (@src, @src_len octets) into @dst: @n_lists exception
 * lists, stored as they are and padded to 4 octets, then @data_len
 * octets of zeros with its segments laid over them. A SHA-1 ends the
 * group; the heads' hashes already cover the tables that locate it.
 * Returns the octets written, 0 for a group that does not parse. */
static size_t rvz_purge(uint8_t *dst, size_t data_len, unsigned n_lists,
      const uint8_t *src, size_t src_len)
{
   size_t   pos = 0, lists;
   unsigned l;
   if (src_len < 20)
      return 0;
   src_len -= 20;
   for (l = 0; l < n_lists; l++)
   {
      size_t n;
      if (src_len - pos < 2)
         return 0;
      n = ((size_t)src[pos] << 8) | src[pos + 1];
      if ((src_len - pos - 2) / RVZ_EXCEPTION < n)
         return 0;
      pos += 2 + n * RVZ_EXCEPTION;
   }
   if (n_lists)
      pos = (pos + 3) & ~(size_t)3;
   if (pos > src_len)
      return 0;
   lists = pos;
   memcpy(dst, src, lists);
   memset(dst + lists, 0, data_len);
   while (pos < src_len)
   {
      uint32_t at, size;
      if (src_len - pos < 8)
         return 0;
      at   = rvz_be32(src + pos);
      size = rvz_be32(src + pos + 4);
      pos += 8;
      if (size > src_len - pos || at > data_len || size > data_len - at)
         return 0;
      memcpy(dst + lists + at, src + pos, size);
      pos += size;
   }
   return lists + data_len;
}

/* ---- padding: the lagged Fibonacci generator the discs use ---- */

static void rvz_prng_advance(uint32_t *b)
{
   unsigned i;
   for (i = 0; i < 32; i++)
      b[i] ^= b[i + RVZ_PRNG_WORDS - 32];
   for (i = 32; i < RVZ_PRNG_WORDS; i++)
      b[i] ^= b[i - 32];
}

/* @len octets of padding from @seed, at disc data offset @off (its
 * position in the 32 KiB the seed starts).
 *
 * A word w stands for the octets w >> 24, w >> 18, w >> 8 and w. Taken
 * through (w & 0xff00ffff) | ((w >> 2) & 0x00ff0000), the second is in
 * place and the word, stored big endian, is the octets themselves.
 * That map is linear over xor, and advancing the generator is only
 * xors, so the state is kept mapped and stored: its octets are the
 * output, and the output is copied out of it whole. */
static void rvz_junk(uint32_t *b, uint8_t *dst, size_t len,
      const uint8_t *seed, uint64_t off)
{
   const size_t state = RVZ_PRNG_WORDS * sizeof(uint32_t);
   size_t       pos   = (size_t)(off % RVZ_SECTOR);
   unsigned i;

   for (i = 0; i < RVZ_SEED_WORDS; i++)
      b[i] = rvz_be32(seed + 4 * i);
   for (i = RVZ_SEED_WORDS; i < RVZ_PRNG_WORDS; i++)
      b[i] = (b[i - 17] << 23) ^ (b[i - 16] >> 9) ^ b[i - 1];
   for (i = 0; i < RVZ_PRNG_WORDS; i++)
   {
      uint32_t v = (b[i] & 0xff00ffffu) | ((b[i] >> 2) & 0x00ff0000u);
      uint8_t *o = (uint8_t*)&b[i];
      o[0] = (uint8_t)(v >> 24);
      o[1] = (uint8_t)(v >> 16);
      o[2] = (uint8_t)(v >> 8);
      o[3] = (uint8_t)v;
   }
   for (i = 0; i < 4; i++)
      rvz_prng_advance(b);
   for (; pos >= state; pos -= state)
      rvz_prng_advance(b);
   while (len)
   {
      size_t take = state - pos;
      if (take > len)
         take = len;
      memcpy(dst, (const uint8_t*)b + pos, take);
      dst += take;
      len -= take;
      pos += take;
      if (pos == state)
      {
         rvz_prng_advance(b);
         pos = 0;
      }
   }
}

/* The packed stream at @src (@len octets) unpacked into @dst, exactly
 * @want octets; @off is the data offset @dst starts at. */
static bool rvz_unpack(uint32_t *junk, uint8_t *dst, size_t want,
      const uint8_t *src, size_t len, uint64_t off)
{
   size_t done = 0;
   while (done < want)
   {
      uint32_t size;
      if (len < 4)
         return false;
      size = rvz_be32(src);
      src += 4;
      len -= 4;
      if (size & 0x80000000u)
      {
         size &= 0x7fffffffu;
         if (len < 4 * RVZ_SEED_WORDS || size > want - done)
            return false;
         rvz_junk(junk, dst + done, size, src, off + done);
         src += 4 * RVZ_SEED_WORDS;
         len -= 4 * RVZ_SEED_WORDS;
      }
      else
      {
         if (size > len || size > want - done)
            return false;
         memcpy(dst + done, src, size);
         src += size;
         len -= size;
      }
      done += size;
   }
   return true;
}

/* ---- groups ---- */

/* Loads group @g into @s: @data_len octets of data at data offset
 * @data_off (for the padding generator), after @n_lists exception
 * lists when @n_lists is not 0.
 *
 * With @into, the data goes there instead and @s keeps nothing: a raw
 * data group read whole is put where the caller wants it, the
 * decoder's output being the data itself, rather than decoded aside
 * and copied. */
static int rvz_group_load(rvz_t *z, struct rvz_slot *s, uint32_t g,
      size_t data_len, unsigned n_lists, uint64_t data_off, uint8_t *into)
{
   const struct rvz_group *grp;
   const uint8_t          *p;
   size_t                  len, pos = 0;
   unsigned                l;
   int                     e;

   if (s->group == g && !into)
      return RVZ_OK;
   s->group = UINT32_MAX;
   if (g >= z->n_groups || data_len > z->data_max)
      return RVZ_ERROR_DATA;
   grp = &z->groups[g];

   /* nothing stored: zeros, and no exceptions */
   if (!grp->size)
   {
      if (into)
      {
         memset(into, 0, data_len);
         return RVZ_OK;
      }
      s->data    = z->zeros;
      s->exc     = NULL;
      s->exc_len = 0;
      s->group   = g;
      return RVZ_OK;
   }
   if (into && !n_lists && !grp->packed && z->compression != RVZ_COMP_PURGE)
   {
      if (!grp->compressed)
      {
         if (grp->size < data_len)
            return RVZ_ERROR_DATA;
         return rvz_need(z, grp->off, into, data_len);
      }
      {
         size_t wrote = 0;
         if (!rvz_grow(&z->zbuf, &z->zbuf_cap, grp->size))
            return RVZ_ERROR_MEM;
         if ((e = rvz_need(z, grp->off, z->zbuf, grp->size)) != RVZ_OK)
            return e;
         if ((e = rvz_decompress(z, into, data_len, z->zbuf, grp->size,
                     0, data_len, &wrote)) != RVZ_OK)
            return e;
         return wrote == data_len ? RVZ_OK : RVZ_ERROR_DATA;
      }
   }

   if (z->compression == RVZ_COMP_PURGE)
   {
      size_t cap = n_lists * (2 + (size_t)0xffff * RVZ_EXCEPTION) + 4 + data_len;
      if (     !rvz_grow(&z->zbuf, &z->zbuf_cap, grp->size)
            || !rvz_grow(&s->raw, &s->raw_cap, cap))
         return RVZ_ERROR_MEM;
      if ((e = rvz_need(z, grp->off, z->zbuf, grp->size)) != RVZ_OK)
         return e;
      if (!(len = rvz_purge(s->raw, data_len, n_lists, z->zbuf, grp->size)))
         return RVZ_ERROR_DATA;
   }
   else if (!grp->compressed)
   {
      if (!rvz_grow(&s->raw, &s->raw_cap, grp->size))
         return RVZ_ERROR_MEM;
      if ((e = rvz_need(z, grp->off, s->raw, grp->size)) != RVZ_OK)
         return e;
      len = grp->size;
   }
   else
   {
      /* the most a group can hold: its lists and its data, packed or
       * not; a Zstandard frame may say exactly */
      size_t cap = n_lists * (2 + (size_t)0xffff * RVZ_EXCEPTION)
                 + (grp->packed ? grp->packed : data_len);
      if (!rvz_grow(&z->zbuf, &z->zbuf_cap, grp->size))
         return RVZ_ERROR_MEM;
      if ((e = rvz_need(z, grp->off, z->zbuf, grp->size)) != RVZ_OK)
         return e;
#ifdef HAVE_RZSTD
      if (z->compression == RVZ_COMP_ZSTD)
      {
         int64_t cs = rzstd_frame_content_size(z->zbuf, grp->size);
         if (cs >= 0 && (uint64_t)cs < cap)
            cap = (size_t)cs;
      }
#endif
      if (!rvz_grow(&s->raw, &s->raw_cap, cap))
         return RVZ_ERROR_MEM;
      if ((e = rvz_decompress(z, s->raw, cap, z->zbuf, grp->size, n_lists,
               grp->packed ? grp->packed : data_len, &len)) != RVZ_OK)
         return e;
   }
   p = s->raw;

   /* the exception lists, then (stored as is) padding to 4 octets */
   for (l = 0; l < n_lists; l++)
   {
      size_t n;
      if (len - pos < 2)
         return RVZ_ERROR_DATA;
      n = ((size_t)p[pos] << 8) | p[pos + 1];
      if ((len - pos - 2) / RVZ_EXCEPTION < n)
         return RVZ_ERROR_DATA;
      pos += 2 + n * RVZ_EXCEPTION;
   }
   s->exc     = n_lists ? p : NULL;
   s->exc_len = pos;
   if (n_lists && (!grp->compressed || z->compression == RVZ_COMP_PURGE))
      pos = (pos + 3) & ~(size_t)3;
   if (pos > len)
      return RVZ_ERROR_DATA;

   if (grp->packed)
   {
      uint8_t *out = into;
      if (!out)
      {
         if (!rvz_grow(&s->out, &s->out_cap, data_len))
            return RVZ_ERROR_MEM;
         out = s->out;
      }
      if (!rvz_unpack(z->junk, out, data_len, p + pos, len - pos, data_off))
         return RVZ_ERROR_DATA;
      if (into)
         return RVZ_OK;
      s->data = s->out;
   }
   else
   {
      if (len - pos < data_len)
         return RVZ_ERROR_DATA;
      if (into)
      {
         memcpy(into, p + pos, data_len);
         return RVZ_OK;
      }
      s->data = p + pos;
   }
   s->group = g;
   return RVZ_OK;
}

/* ---- Wii partition data ---- */

/* Where sector @s of partition @pt's data lies: its group, the group's
 * first sector and sector count, and the sector's place in it.
 * false past the partition's data. */
static bool rvz_part_locate(const rvz_t *z, const struct rvz_part *pt,
      uint32_t s, uint32_t *g, uint32_t *g_first, uint32_t *g_count,
      uint32_t *in_g)
{
   uint32_t spg = z->chunk / RVZ_SECTOR;
   unsigned k;
   for (k = 0; k < 2; k++)
   {
      uint32_t rel;
      if (!pt->count[k])
         continue;
      rel = s + pt->base - pt->first[k];
      if (s + pt->base < pt->first[k] || rel >= pt->count[k])
         continue;
      if (rel / spg >= pt->n_groups[k])
         return false;
      *g       = pt->group[k] + rel / spg;
      *g_first = s - rel % spg;
      *g_count = pt->count[k] - (rel - rel % spg);
      if (*g_count > spg)
         *g_count = spg;
      *in_g    = rel % spg;
      return true;
   }
   return false;
}

/* Applies a group's exception lists, list j over the 64 sectors from
 * its sector 64 j, to the hash areas from block sector @first. */
static void rvz_exceptions(rvz_t *z, const struct rvz_slot *s,
      unsigned n_lists, long first)
{
   const uint8_t *p = s->exc;
   unsigned       l;
   if (!p)
      return;
   for (l = 0; l < n_lists; l++)
   {
      size_t n = ((size_t)p[0] << 8) | p[1], e;
      p += 2;
      for (e = 0; e < n; e++, p += RVZ_EXCEPTION)
      {
         long     sec = first + (long)l * RVZ_BLOCK_SECTORS
                      + (long)(((unsigned)p[0] << 8 | p[1]) / RVZ_SECTOR_HASHES);
         unsigned at  = (((unsigned)p[0] << 8) | p[1]) % RVZ_SECTOR_HASHES;
         size_t   take = RVZ_SECTOR_HASHES - at < 20 ? RVZ_SECTOR_HASHES - at : 20;
         if (sec >= 0 && sec < RVZ_BLOCK_SECTORS)
            memcpy(z->hashes + (size_t)sec * RVZ_SECTOR_HASHES + at, p + 2, take);
      }
   }
}

/* Reassembles hash block @b of partition @pi into z->block. */
static int rvz_block_load(rvz_t *z, uint32_t pi, uint32_t b)
{
   const struct rvz_part *pt = &z->parts[pi];
   const uint8_t  **data       = z->sec_data;
   struct rvz_slot **held       = z->held;
   long            *held_first = z->held_first;
   unsigned char busy[RVZ_MAX_SLOTS];
   uint32_t first   = b * RVZ_BLOCK_SECTORS;
   uint32_t n       = pt->sectors - first;
   unsigned n_lists = z->chunk >= RVZ_BLOCK ? z->chunk / RVZ_BLOCK : 1;
   unsigned s, i, n_held = 0;
   int      e;

   if (z->block_part == pi && z->block_index == b)
      return RVZ_OK;
   z->block_part = z->block_index = UINT32_MAX;
   if (n > RVZ_BLOCK_SECTORS)
      n = RVZ_BLOCK_SECTORS;
   memset(busy, 0, sizeof(busy));

   /* each sector's data where its group decoded it; sectors in no
    * segment, and past the partition's end, hash as zeros */
   for (s = 0; s < RVZ_BLOCK_SECTORS; s++)
      data[s] = z->zeros;
   for (s = 0; s < n; )
   {
      uint32_t g, g_first, g_count, in_g, k;
      struct rvz_slot *slot = NULL;
      if (!rvz_part_locate(z, pt, first + s, &g, &g_first, &g_count, &in_g))
      {
         s++;
         continue;
      }
      /* a slot holding the group already, kept from the block before
       * when a group spans blocks; else one this block has not used */
      for (k = 0; k < z->n_slots && !slot; k++)
         if (z->slots[k].group == g)
            slot = &z->slots[k];
      for (k = 0; k < z->n_slots && !slot; k++)
         if (!busy[k])
            slot = &z->slots[k];
      if (!slot)
         return RVZ_ERROR_DATA;
      if ((e = rvz_group_load(z, slot, g, (size_t)g_count * RVZ_SECTOR_DATA,
               n_lists, (uint64_t)g_first * RVZ_SECTOR_DATA, NULL)) != RVZ_OK)
         return e;
      busy[slot - z->slots]  = 1;
      held[n_held]           = slot;
      held_first[n_held++]   = (long)g_first - (long)first;
      for (k = in_g; k < g_count && s < n; k++, s++)
         data[s] = slot->data + (size_t)k * RVZ_SECTOR_DATA;
   }

   /* H0 per sector, H1 per eight, H2 per 64, the padding zero.
    *
    * H0 is the SHA-1 of each KiB, and a run of sectors one group holds
    * is one run of KiBs, hashed as one: SHA1DigestMany() takes them
    * two at a time. A sector of zeros, which every sector past the
    * end of the partition is, hashes to the zero KiB's hash 31 times. */
   memset(z->hashes, 0, RVZ_BLOCK_SECTORS * RVZ_SECTOR_HASHES);
   for (s = 0; s < RVZ_BLOCK_SECTORS; )
   {
      unsigned run = 1, k;
      if (data[s] == z->zeros)
      {
         for (i = 0; i < RVZ_SECTOR_DATA / RVZ_SECTOR_HASHES; i++)
            memcpy(z->hashes + (size_t)s * RVZ_SECTOR_HASHES + 20 * i, z->zero_h0, 20);
         s++;
         continue;
      }
      while (s + run < RVZ_BLOCK_SECTORS
            && data[s + run] == data[s] + (size_t)run * RVZ_SECTOR_DATA)
         run++;
      SHA1DigestMany(data[s], RVZ_SECTOR_HASHES, RVZ_SECTOR_HASHES,
            run * (RVZ_SECTOR_DATA / RVZ_SECTOR_HASHES), z->h0);
      for (k = 0; k < run; k++)
         memcpy(z->hashes + (size_t)(s + k) * RVZ_SECTOR_HASHES,
               z->h0 + (size_t)k * 31 * 20, 31 * 20);
      s += run;
   }
   {
      uint8_t h2[8 * 20];
      for (s = 0; s < RVZ_BLOCK_SECTORS; s += 8)
      {
         uint8_t h1[8 * 20];
         SHA1DigestMany(z->hashes + (size_t)s * RVZ_SECTOR_HASHES,
               RVZ_SECTOR_HASHES, 31 * 20, 8, h1);
         for (i = 0; i < 8; i++)
            memcpy(z->hashes + (size_t)(s + i) * RVZ_SECTOR_HASHES + 0x280,
                  h1, sizeof(h1));
      }
      SHA1DigestMany(z->hashes + 0x280, 8 * RVZ_SECTOR_HASHES, 8 * 20, 8, h2);
      for (s = 0; s < RVZ_BLOCK_SECTORS; s++)
         memcpy(z->hashes + (size_t)s * RVZ_SECTOR_HASHES + 0x340, h2, sizeof(h2));
   }
   /* then the exceptions over them, in group order */
   for (i = 0; i < n_held; i++)
      rvz_exceptions(z, held[i], n_lists, held_first[i]);

   /* the hash areas under IV zero, then each sector's data under
    * octets 0x3d0-0x3df of its encrypted hash area */
   if (z->encrypted)
   {
      uint8_t       *iv[8], *out[8], ivs[8][16];
      const uint8_t *in[8];
      for (s = 0; s < n; s += 8)
      {
         unsigned l, m = n - s < 8 ? n - s : 8;
         for (l = 0; l < m; l++)
         {
            memset(ivs[l], 0, 16);
            iv[l]  = ivs[l];
            in[l]  = z->hashes + (size_t)(s + l) * RVZ_SECTOR_HASHES;
            out[l] = z->block  + (size_t)(s + l) * RVZ_SECTOR;
         }
         aes_cbc_encrypt_lanes(&pt->aes, m, iv, in, out, RVZ_SECTOR_HASHES);
         for (l = 0; l < m; l++)
         {
            memcpy(ivs[l], out[l] + 0x3d0, 16);
            in[l]   = data[s + l];
            out[l] += RVZ_SECTOR_HASHES;
         }
         aes_cbc_encrypt_lanes(&pt->aes, m, iv, in, out, RVZ_SECTOR_DATA);
      }
   }
   else
      for (s = 0; s < n; s++)
      {
         memcpy(z->block + (size_t)s * RVZ_SECTOR,
               z->hashes + (size_t)s * RVZ_SECTOR_HASHES, RVZ_SECTOR_HASHES);
         memcpy(z->block + (size_t)s * RVZ_SECTOR + RVZ_SECTOR_HASHES,
               data[s], RVZ_SECTOR_DATA);
      }
   z->block_part  = pi;
   z->block_index = b;
   return RVZ_OK;
}

/* ---- the disc ---- */

static const struct rvz_region *rvz_region_at(const rvz_t *z, uint64_t off,
      uint64_t *next)
{
   uint32_t lo = 0, hi = z->n_regions;
   while (lo < hi)
   {
      uint32_t mid = lo + (hi - lo) / 2;
      if (z->regions[mid].end <= off)
         lo = mid + 1;
      else
         hi = mid;
   }
   *next = lo < z->n_regions ? z->regions[lo].start : z->disc_size;
   if (lo < z->n_regions && z->regions[lo].start <= off)
      return &z->regions[lo];
   return NULL;
}

/* Runs the armed read on from where it stands */
static int rvz_read_run(rvz_t *z)
{
   uint8_t *out = z->rd_dst;
   int      e;

   while (z->rd_done < z->rd_len)
   {
      uint64_t pos  = z->rd_off + z->rd_done, next;
      size_t   take = z->rd_len - z->rd_done;
      uint8_t *at   = out + z->rd_done;
      const struct rvz_region *r;

      if (pos < RVZ_DHEAD)
      {
         if (take > RVZ_DHEAD - pos)
            take = (size_t)(RVZ_DHEAD - pos);
         memcpy(at, z->dhead + pos, take);
      }
      else if (!(r = rvz_region_at(z, pos, &next)))
      {
         /* nothing stored here: the disc has zeros */
         if (take > next - pos)
            take = (size_t)(next - pos);
         memset(at, 0, take);
      }
      else if (r->part < 0)
      {
         uint64_t rel   = pos - r->origin;
         uint32_t g     = (uint32_t)(rel / z->chunk);
         uint64_t g_off = (uint64_t)g * z->chunk;
         uint64_t g_len = r->end - r->origin - g_off;
         uint64_t in_g;
         const struct rvz_group *grp;
         if (g_len > z->chunk)
            g_len = z->chunk;
         if (g >= r->n_groups)
            return RVZ_ERROR_DATA;
         in_g = rel - g_off;
         grp  = &z->groups[r->group + g];
         if (take > g_len - in_g)
            take = (size_t)(g_len - in_g);
         if (z->raw.group == r->group + g)
            memcpy(at, z->raw.data + in_g, take);
         else if (grp->size && !grp->compressed && !grp->packed
               && z->compression != RVZ_COMP_PURGE)
         {
            /* stored as it is: asked for where it is to go */
            if (grp->size < g_len)
               return RVZ_ERROR_DATA;
            if ((e = rvz_need(z, grp->off + in_g, at, take)) != RVZ_OK)
               return e;
         }
         else
         {
            if ((e = rvz_group_load(z, &z->raw, r->group + g, (size_t)g_len, 0,
                     r->origin + g_off, take == g_len ? at : NULL)) != RVZ_OK)
               return e;
            if (take != g_len)
               memcpy(at, z->raw.data + in_g, take);
         }
      }
      else
      {
         const struct rvz_part *pt = &z->parts[r->part];
         uint64_t rel   = pos - (uint64_t)pt->base * RVZ_SECTOR;
         uint32_t b     = (uint32_t)(rel / RVZ_BLOCK);
         uint64_t in_b  = rel % RVZ_BLOCK;
         uint64_t b_end = (uint64_t)pt->base * RVZ_SECTOR + (uint64_t)(b + 1) * RVZ_BLOCK;
         if (b_end > r->end)
            b_end = r->end;
         if ((e = rvz_block_load(z, (uint32_t)r->part, b)) != RVZ_OK)
            return e;
         if (take > b_end - pos)
            take = (size_t)(b_end - pos);
         memcpy(at, z->block + in_b, take);
      }
      z->rd_done += take;
   }
   return RVZ_OK;
}

/* What an operation's return comes to for the caller: a request for
 * the range still to come, or the end of the operation. */
static int rvz_step_result(rvz_t *z, int e, rvz_request_t *req)
{
   if (e == RVZ_PENDING)
   {
      req->offset = z->fetch_off + z->fetch_got;
      req->length = z->fetch_len - z->fetch_got;
      req->dst    = z->fetch_dst + z->fetch_got;
      return RVZ_PENDING;
   }
   z->fetching = 0;
   return e;
}

int rvz_read_begin(rvz_t *z, uint64_t off, void *dst, size_t len)
{
   if (!z || (!dst && len))
      return RVZ_ERROR_PARAM;
   if (z->stage != RVZ_STAGE_OPEN)
      return RVZ_ERROR_STATE;
   if (off >= z->disc_size)
      len = 0;
   else if (len > z->disc_size - off)
      len = (size_t)(z->disc_size - off);
   z->fetching = 0;
   z->failed   = false;
   z->rd_dst   = (uint8_t*)dst;
   z->rd_off   = off;
   z->rd_len   = len;
   z->rd_done  = 0;
   z->rd_armed = true;
   return RVZ_OK;
}

int rvz_read_step(rvz_t *z, rvz_request_t *req)
{
   int e;
   if (!z || !req)
      return RVZ_ERROR_PARAM;
   if (!z->rd_armed)
      return RVZ_ERROR_STATE;
   if (z->failed)
      e = RVZ_ERROR_DATA;
   else
      e = rvz_read_run(z);
   if (e != RVZ_PENDING)
      z->rd_armed = false;
   return rvz_step_result(z, e, req);
}

size_t rvz_read_progress(const rvz_t *z)
{
   return z ? z->rd_done : 0;
}

int rvz_feed(rvz_t *z, size_t len)
{
   if (!z || z->fetching != 1)
      return RVZ_ERROR_STATE;
   if (!len)
   {
      /* nothing came: the operation cannot go on */
      z->fetching = 0;
      z->failed   = true;
      return RVZ_ERROR_DATA;
   }
   if (len > z->fetch_len - z->fetch_got)
      len = z->fetch_len - z->fetch_got;
   z->fetch_got += len;
   if (z->fetch_got == z->fetch_len)
      z->fetching = 2;
   return RVZ_OK;
}

static int rvz_region_cmp(const void *a, const void *b)
{
   const struct rvz_region *x = (const struct rvz_region*)a;
   const struct rvz_region *y = (const struct rvz_region*)b;
   return x->start < y->start ? -1 : x->start > y->start;
}

static void rvz_slot_free(struct rvz_slot *s)
{
   free(s->raw);
   free(s->out);
}

void rvz_free(rvz_t *z)
{
   uint32_t i;
   if (!z)
      return;
   if (z->slots)
      for (i = 0; i < z->n_slots; i++)
         rvz_slot_free(&z->slots[i]);
   rvz_slot_free(&z->raw);
   if (z->parts)
      crypto_memzero(z->parts, z->n_parts * sizeof(*z->parts));
   free(z->slots);
   free(z->regions);
   free(z->parts);
   free(z->groups);
   free(z->zbuf);
   free(z->zeros);
   free(z->block);
   free(z->hashes);
   free(z->h0);
   free(z->probs);
   rbzip2_free(z->bz);
   free(z->disc);
   free(z->tab);
   free(z);
}

int rvz_disc_type(const rvz_t *z)
{
   return z ? z->disc_type : 0;
}

uint64_t rvz_disc_size(const rvz_t *z)
{
   return z ? z->disc_size : 0;
}

rvz_t *rvz_new(void)
{
   rvz_t *z = (rvz_t*)calloc(1, sizeof(*z));
   if (!z)
      return NULL;
   z->raw.group   = UINT32_MAX;
   z->block_part  = UINT32_MAX;
   z->block_index = UINT32_MAX;
   /* nothing is known of the file's size before its head */
   z->file_size   = (uint64_t)-1;
   return z;
}

/* The partition entries, stored as they are, short entries zero
 * filled */
static int rvz_open_parts(rvz_t *z)
{
   uint8_t  ent[RVZ_PART_ENTRY], hash[20];
   size_t   all = (size_t)z->n_parts * z->part_size;
   uint32_t i, k;
   if (!(z->parts = (struct rvz_part*)calloc(z->n_parts, sizeof(*z->parts))))
      return RVZ_ERROR_MEM;
   SHA1Digest(z->tab, all, hash);
   if (memcmp(hash, z->disc + 0xa0, 20))
      return RVZ_ERROR_DATA;
   for (i = 0; i < z->n_parts; i++)
   {
      struct rvz_part *pt = &z->parts[i];
      memset(ent, 0, sizeof(ent));
      memcpy(ent, z->tab + (size_t)i * z->part_size,
            z->part_size < RVZ_PART_ENTRY ? z->part_size : RVZ_PART_ENTRY);
      aes_init(&pt->aes, ent, 16);
      for (k = 0; k < 2; k++)
      {
         pt->first[k]    = rvz_be32(ent + 16 + 16 * k);
         pt->count[k]    = rvz_be32(ent + 20 + 16 * k);
         pt->group[k]    = rvz_be32(ent + 24 + 16 * k);
         pt->n_groups[k] = rvz_be32(ent + 28 + 16 * k);
         if (     pt->count[k] > 0x1000000 || pt->first[k] > 0x1000000
               || (uint64_t)pt->group[k] + pt->n_groups[k] > z->n_groups)
            return RVZ_ERROR_DATA;
      }
      pt->base    = pt->count[0] ? pt->first[0] : pt->first[1];
      if (pt->count[0] && pt->count[1] && pt->first[1] < pt->first[0] + pt->count[0])
         return RVZ_ERROR_DATA;
      pt->sectors = (pt->count[1] ? pt->first[1] + pt->count[1]
                                  : pt->first[0] + pt->count[0]) - pt->base;
   }
   crypto_memzero(ent, sizeof(ent));
   crypto_memzero(z->tab, all);
   return RVZ_OK;
}

/* The groups: offsets in words, the top bit of the size the method */
static int rvz_open_groups(rvz_t *z)
{
   uint32_t i;
   if (!(z->groups = (struct rvz_group*)calloc(z->n_groups ? z->n_groups : 1,
               sizeof(*z->groups))))
      return RVZ_ERROR_MEM;
   for (i = 0; i < z->n_groups; i++)
   {
      const uint8_t *e  = z->tab + (size_t)i * (z->wia ? WIA_GROUP_ENTRY : RVZ_GROUP_ENTRY);
      uint32_t       sz = rvz_be32(e + 4);
      z->groups[i].off        = (uint64_t)rvz_be32(e) * 4;
      if (z->wia)
      {
         /* every group in the file's method; no packing */
         z->groups[i].size       = sz;
         z->groups[i].compressed = z->compression != RVZ_COMP_NONE
                                && z->compression != RVZ_COMP_PURGE;
         z->groups[i].packed     = 0;
         if (z->groups[i].off + z->groups[i].size > z->file_size)
            return RVZ_ERROR_DATA;
         continue;
      }
      z->groups[i].size       = sz & 0x7fffffffu;
      /* nothing is read from past the end of the file */
      if (z->groups[i].off + z->groups[i].size > z->file_size)
         return RVZ_ERROR_DATA;
      /* the top bit names the file's method, which may be none */
      z->groups[i].compressed = (sz & 0x80000000u) != 0
                             && z->compression != RVZ_COMP_NONE;
      z->groups[i].packed     = rvz_be32(e + 8);
   }
   return RVZ_OK;
}

/* The regions: each raw data entry, rounded down to its sector, and
 * each partition data segment; then what a read works in */
static int rvz_open_regions(rvz_t *z)
{
   uint32_t i, k;
   if (!(z->regions = (struct rvz_region*)calloc(z->n_raw + 2 * z->n_parts + 1,
               sizeof(*z->regions))))
      return RVZ_ERROR_MEM;
   for (i = 0; i < z->n_raw; i++)
   {
      const uint8_t *e = z->tab + (size_t)i * RVZ_RAW_ENTRY;
      uint64_t start   = rvz_be64(e), size = rvz_be64(e + 8);
      struct rvz_region *r = &z->regions[z->n_regions];
      if (start > z->disc_size || size > z->disc_size - start)
         return RVZ_ERROR_DATA;
      r->start    = r->origin = start - start % RVZ_SECTOR;
      r->end      = start + size;
      r->group    = rvz_be32(e + 16);
      r->n_groups = rvz_be32(e + 20);
      r->part     = -1;
      if ((uint64_t)r->group + r->n_groups > z->n_groups)
         return RVZ_ERROR_DATA;
      if (r->end > r->start)
         z->n_regions++;
   }
   for (i = 0; i < z->n_parts; i++)
      for (k = 0; k < 2; k++)
      {
         struct rvz_region *r = &z->regions[z->n_regions];
         if (!z->parts[i].count[k])
            continue;
         r->start    = r->origin = (uint64_t)z->parts[i].first[k] * RVZ_SECTOR;
         r->end      = r->start + (uint64_t)z->parts[i].count[k] * RVZ_SECTOR;
         r->group    = z->parts[i].group[k];
         r->n_groups = z->parts[i].n_groups[k];
         r->part     = (int)i;
         if (r->end > z->disc_size)
            return RVZ_ERROR_DATA;
         z->n_regions++;
      }
   qsort(z->regions, z->n_regions, sizeof(*z->regions), rvz_region_cmp);
   /* raw data rounded down to its sector may reach back over what
    * came before it; it starts, as read, where that ends */
   for (i = 1; i < z->n_regions; i++)
      if (z->regions[i].start < z->regions[i - 1].end)
      {
         if (     z->regions[i].part >= 0
               || z->regions[i].start + RVZ_SECTOR <= z->regions[i - 1].end
               || z->regions[i].end <= z->regions[i - 1].end)
            return RVZ_ERROR_DATA;
         z->regions[i].start = z->regions[i - 1].end;
      }

   /* a hash block's groups at once: one when a group holds a block or
    * more, else as many as it takes, and one more */
   z->n_slots = (z->chunk >= RVZ_BLOCK ? 1 : RVZ_BLOCK / z->chunk) + 1;
   if (!(z->zeros = (uint8_t*)calloc(1, z->data_max)))
      return RVZ_ERROR_MEM;
   if (z->n_parts)
   {
      if (     !(z->slots  = (struct rvz_slot*)calloc(z->n_slots, sizeof(*z->slots)))
            || !(z->block  = (uint8_t*)malloc(RVZ_BLOCK))
            || !(z->hashes = (uint8_t*)malloc(RVZ_BLOCK_SECTORS * RVZ_SECTOR_HASHES))
            || !(z->h0     = (uint8_t*)malloc(RVZ_BLOCK_SECTORS * 31 * 20)))
         return RVZ_ERROR_MEM;
      SHA1Digest(z->zeros, RVZ_SECTOR_HASHES, z->zero_h0);
      for (i = 0; i < z->n_slots; i++)
         z->slots[i].group = UINT32_MAX;
   }
   return RVZ_OK;
}

/* Reads the heads and the tables, a stage at a time */
static int rvz_open_run(rvz_t *z)
{
   uint8_t hash[20];
   int     e;

   for (;;)
   {
      switch (z->stage)
      {
         case RVZ_STAGE_HEAD:
            /* the file head, its own hash over everything before it */
            if ((e = rvz_need(z, 0, z->head, RVZ_HEAD_SIZE)) != RVZ_OK)
               return e;
            if (!memcmp(z->head, "WIA\x01", 4))
               z->wia = true;
            else if (memcmp(z->head, "RVZ\x01", 4))
               return RVZ_ERROR_DATA;
            SHA1Digest(z->head, RVZ_HEAD_SIZE - 20, hash);
            if (memcmp(hash, z->head + RVZ_HEAD_SIZE - 20, 20))
               return RVZ_ERROR_DATA;
            z->disc_len  = rvz_be32(z->head + 0x0c);
            z->disc_size = rvz_be64(z->head + 0x24);
            z->file_size = rvz_be64(z->head + 0x2c);
            if (     z->disc_len < RVZ_DISC_MIN || z->disc_len > 0x10000
                  || z->file_size < RVZ_HEAD_SIZE + (uint64_t)z->disc_len)
               return RVZ_ERROR_DATA;
            if (!(z->disc = (uint8_t*)malloc(z->disc_len)))
               return RVZ_ERROR_MEM;
            z->stage = RVZ_STAGE_DISC;
            break;
         case RVZ_STAGE_DISC:
         {
            const uint8_t *d;
            if ((e = rvz_need(z, RVZ_HEAD_SIZE, z->disc, z->disc_len)) != RVZ_OK)
               return e;
            SHA1Digest(z->disc, z->disc_len, hash);
            if (memcmp(hash, z->head + 0x10, 20))
               return RVZ_ERROR_DATA;
            d              = z->disc;
            z->disc_type   = (int)rvz_be32(d + 0x00);
            z->compression = rvz_be32(d + 0x04);
            z->chunk       = rvz_be32(d + 0x0c);
            memcpy(z->dhead, d + 0x10, RVZ_DHEAD);
            z->n_parts     = rvz_be32(d + 0x90);
            z->part_size   = rvz_be32(d + 0x94);
            z->part_off    = rvz_be64(d + 0x98);
            z->n_raw       = rvz_be32(d + 0xb4);
            z->raw_off     = rvz_be64(d + 0xb8);
            z->raw_size    = rvz_be32(d + 0xc0);
            z->n_groups    = rvz_be32(d + 0xc4);
            z->group_off   = rvz_be64(d + 0xc8);
            z->group_size  = rvz_be32(d + 0xd0);
            if (z->disc_type != RVZ_DISC_GAMECUBE && z->disc_type != RVZ_DISC_WII)
               z->disc_type = 0;
            /* at least 32 KiB; a power of two up to 2 MiB, multiples above */
            if (     z->chunk < RVZ_SECTOR
                  || (z->chunk < RVZ_BLOCK ? (z->chunk & (z->chunk - 1)) != 0
                                           : (z->chunk % RVZ_BLOCK) != 0)
                  || z->n_parts > 64 || z->part_size > 0x1000
                  || z->n_raw > 0x100000 || z->n_groups > 0x1000000)
               return RVZ_ERROR_DATA;
            /* RVZ alone has chunks under 2 MiB, Zstandard, and not
             * PURGE */
            if (     (z->wia && (z->chunk % RVZ_BLOCK
                        || z->compression == RVZ_COMP_ZSTD))
                  || (!z->wia && z->compression == RVZ_COMP_PURGE))
               return RVZ_ERROR_DATA;
            /* the LZMA properties, as the 7-Zip SDK stores them */
            if (z->compression == RVZ_COMP_LZMA || z->compression == RVZ_COMP_LZMA2)
            {
               unsigned n = z->compression == RVZ_COMP_LZMA ? RLZMA_PROPS_SIZE : 1;
               size_t   probs;
               if (d[0xd4] < n)
                  return RVZ_ERROR_DATA;
               memcpy(z->lzma_props, d + 0xd5, n);
               if (z->compression == RVZ_COMP_LZMA)
               {
                  unsigned v = z->lzma_props[0];
                  if (v >= 9 * 5 * 5)
                     return RVZ_ERROR_DATA;
                  probs = RLZMA_STREAM_NUM_PROBS(v % 9, (v / 9) % 5);
               }
               else
                  probs = RLZMA2_NUM_PROBS;
               if (!(z->probs = (uint16_t*)malloc(probs * sizeof(uint16_t))))
                  return RVZ_ERROR_MEM;
            }
            if (z->compression == RVZ_COMP_BZIP2 && !(z->bz = rbzip2_new()))
               return RVZ_ERROR_MEM;
            z->encrypted = z->dhead[0x61] == 0;
            z->data_max  = z->chunk;
            z->stage     = RVZ_STAGE_PARTS;
            break;
         }
         case RVZ_STAGE_PARTS:
            if (z->n_parts)
            {
               size_t all = (size_t)z->n_parts * z->part_size;
               if (!z->tab && !(z->tab = (uint8_t*)malloc(all ? all : 1)))
                  return RVZ_ERROR_MEM;
               if ((e = rvz_need(z, z->part_off, z->tab, all)) != RVZ_OK)
                  return e;
               if ((e = rvz_open_parts(z)) != RVZ_OK)
                  return e;
               free(z->tab);
               z->tab = NULL;
            }
            z->stage = RVZ_STAGE_GROUPS;
            break;
         case RVZ_STAGE_GROUPS:
            if ((e = rvz_table(z, z->group_off, z->group_size, (size_t)z->n_groups
                     * (z->wia ? WIA_GROUP_ENTRY : RVZ_GROUP_ENTRY))) != RVZ_OK)
               return e;
            if ((e = rvz_open_groups(z)) != RVZ_OK)
               return e;
            free(z->tab);
            z->tab   = NULL;
            z->stage = RVZ_STAGE_RAW;
            break;
         case RVZ_STAGE_RAW:
            if ((e = rvz_table(z, z->raw_off, z->raw_size,
                     (size_t)z->n_raw * RVZ_RAW_ENTRY)) != RVZ_OK)
               return e;
            if ((e = rvz_open_regions(z)) != RVZ_OK)
               return e;
            free(z->tab);
            free(z->disc);
            z->tab   = NULL;
            z->disc  = NULL;
            z->stage = RVZ_STAGE_OPEN;
            return RVZ_OK;
         case RVZ_STAGE_OPEN:
            return RVZ_OK;
         default:
            return RVZ_ERROR_STATE;
      }
   }
}

int rvz_open_step(rvz_t *z, rvz_request_t *req)
{
   int e;
   if (!z || !req)
      return RVZ_ERROR_PARAM;
   if (z->stage == RVZ_STAGE_FAILED || z->failed)
      e = RVZ_ERROR_DATA;
   else
      e = rvz_open_run(z);
   /* an image that did not open stays unopened */
   if (e < 0)
      z->stage = RVZ_STAGE_FAILED;
   return rvz_step_result(z, e, req);
}
