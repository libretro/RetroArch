/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <encodings/crc32.h>

#include "patch_stream.h"

#ifdef HAVE_XDELTA
#include <encodings/encoding_vcdiff.h>
#endif

enum patch_stream_fmt
{
   PATCH_STREAM_IPS = 0,
   PATCH_STREAM_UPS,
   PATCH_STREAM_BPS,
   PATCH_STREAM_XDELTA
};

/* One IPS record, pre-parsed at open().  The patch file is small and
 * fully resident, so indexing it up front costs nothing and turns the
 * per-chunk overlay into a bounded scan. */
struct patch_stream_rec
{
   uint32_t addr;
   uint32_t len;
   uint32_t off;      /* offset of the payload inside the patch  */
   uint8_t  rle;      /* 1: fill with rle_val rather than copy   */
   uint8_t  rle_val;
};

struct patch_stream
{
   const uint8_t *patch;
   size_t         patch_len;
   size_t         src_len;

   uint8_t       *out;        /* growing target                        */
   size_t         out_len;
   size_t         out_cap;

   size_t         src_seen;   /* source bytes fed so far               */
   size_t         p_off;      /* patch cursor                          */
   size_t         s_off;      /* logical source cursor                 */
   size_t         t_off;      /* logical target cursor / BPS output    */
   size_t         tgt_len;    /* declared target length                */

   /* IPS */
   struct patch_stream_rec *recs;
   size_t         recs_len;

   /* UPS carries a checksum for the patch, the source and the target,
    * and the whole-buffer applier refuses a patch whose checksums do
    * not agree.  Track the same three so this applier can refuse the
    * same patches: a caller that keeps the unpatched buffer when a
    * patch is rejected would otherwise be handed a silently corrupt
    * one instead. */
   uint32_t       src_crc;      /* over the source as it is fed  */
   uint32_t       ups_declared_src_len;
   uint32_t       ups_declared_tgt_len;

   /* UPS: source not yet consumed by the state machine */
   uint8_t       *carry;
   size_t         carry_len;
   size_t         carry_cap;
   size_t         carry_base;

   /* BPS: retained source (SourceCopy seeks arbitrarily); `src` is
    * where it is read, src_buf or the caller's src_base */
   const uint8_t *src;
   uint8_t       *src_buf;
   size_t         src_cap;

   /* The fed source where the caller keeps it, read in place; NULL
    * once a chunk arrives from elsewhere. See patch_stream_set_source(). */
   const uint8_t *src_base;
   size_t         bt_off;     /* BPS target_offset cursor              */

   uint8_t        fmt;
   uint8_t        failed;

#ifdef HAVE_XDELTA
   /* The VCDIFF decoder carries its own streaming state, so this
    * format is a shell around it rather than a second implementation. */
   vcdiff_stream_t *vcd;
#endif
};

/* --------------------------------------------------------------------
 * target sink
 * -------------------------------------------------------------------- */

static bool patch_stream_reserve(patch_stream_t *ps, size_t need)
{
   size_t   cap;
   uint8_t *buf;

   if (need <= ps->out_cap)
      return true;

   cap = ps->out_cap ? ps->out_cap : 4096;
   while (cap < need)
   {
      /* Refuse to wrap rather than allocating a bogus small buffer. */
      if (cap > (size_t)-1 / 2)
         return false;
      cap <<= 1;
   }

   if (!(buf = (uint8_t*)realloc(ps->out, cap)))
      return false;

   ps->out     = buf;
   ps->out_cap = cap;
   return true;
}

static bool patch_stream_put(patch_stream_t *ps, size_t at,
      const uint8_t *data, size_t len)
{
   if (!patch_stream_reserve(ps, at + len))
      return false;
   /* reserve() is a no-op for a zero-byte request, so out can still be
    * NULL here; NULL + 0 and memcpy(NULL, ..., 0) are both undefined. */
   if (len)
      memcpy(ps->out + at, data, len);
   if (at + len > ps->out_len)
      ps->out_len = at + len;
   return true;
}

static bool patch_stream_fill(patch_stream_t *ps, size_t at,
      uint8_t val, size_t len)
{
   if (!patch_stream_reserve(ps, at + len))
      return false;
   if (len)
      memset(ps->out + at, val, len);
   if (at + len > ps->out_len)
      ps->out_len = at + len;
   return true;
}

/* BPS and UPS both end with a CRC-32 of the patch's own bytes in the
 * last four, little endian.  It depends on nothing but the patch, which
 * open() already has whole, so it is settled here - before the declared
 * target length, which the patch alone chooses, becomes an allocation.
 * A 20-byte file can otherwise name a 4 GiB target and get it reserved
 * and walked before anything is verified.  finish() still checks the
 * trailer in full; this is a gate in front of the allocator. */
static bool patch_stream_self_checksum_ok(const uint8_t *patch,
      size_t patch_len)
{
   uint32_t want = 0;
   unsigned i;

   if (patch_len < 4)
      return false;
   for (i = 0; i < 4; i++)
      want |= (uint32_t)patch[patch_len - 4 + i] << (i * 8);
   return encoding_crc32(0, patch, patch_len - 4) == want;
}

/* Variable-length integer, as used by both UPS and BPS. */
static bool patch_stream_decode(patch_stream_t *ps, uint64_t *out)
{
   uint64_t val   = 0;
   uint64_t shift = 1;

   for (;;)
   {
      uint8_t x;

      if (ps->p_off >= ps->patch_len)
         return false;
      x = ps->patch[ps->p_off++];

      if ((uint64_t)(x & 0x7f) > (UINT64_MAX - val) / shift)
         return false;
      val += (x & 0x7f) * shift;
      if (x & 0x80)
      {
         *out = val;
         return true;
      }
      if (shift > UINT64_MAX >> 7)
         return false;
      shift <<= 7;
      if (val > UINT64_MAX - shift)
         return false;
      val   += shift;
   }
}

/* --------------------------------------------------------------------
 * IPS
 *
 * The whole-buffer applier copies the source and overlays records onto
 * it.  Streaming form: copy each arriving span straight through at its
 * own offset, then overlay every record that intersects the span now
 * resident.  Both the copy-through and the record writes are absolute
 * addressed, so the result does not depend on the chunk boundaries.
 * -------------------------------------------------------------------- */

static uint32_t patch_stream_read24(const uint8_t *p)
{
   return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

patch_stream_t *patch_stream_ips_open(const uint8_t *patch, size_t patch_len,
      size_t src_len)
{
   patch_stream_t          *ps;
   struct patch_stream_rec *recs;
   size_t                   off = 5;
   size_t                   cap = 16;
   size_t                   n   = 0;

   if (patch_len < 8 || memcmp(patch, "PATCH", 5))
      return NULL;

   if (!(ps = (patch_stream_t*)calloc(1, sizeof(*ps))))
      return NULL;

   if (!(recs = (struct patch_stream_rec*)malloc(cap * sizeof(*recs))))
   {
      free(ps);
      return NULL;
   }

   ps->fmt       = PATCH_STREAM_IPS;
   ps->patch     = patch;
   ps->patch_len = patch_len;
   ps->src_len   = src_len;
   ps->tgt_len   = src_len;

   for (;;)
   {
      uint32_t addr;
      uint32_t len;

      if (off > patch_len - 3)
         break;

      addr = patch_stream_read24(patch + off);
      off += 3;

      if (addr == 0x454f46) /* EOF */
      {
         if (off == patch_len)
            break;
         if (off == patch_len - 3)
         {
            ps->tgt_len = patch_stream_read24(patch + off);
            off += 3;
            break;
         }
      }

      if (off > patch_len - 2)
         break;

      len  = ((uint32_t)patch[off] << 8) | patch[off + 1];
      off += 2;

      if (n == cap)
      {
         struct patch_stream_rec *tmp;
         cap <<= 1;
         if (!(tmp = (struct patch_stream_rec*)realloc(recs,
                     cap * sizeof(*recs))))
            break;
         recs = tmp;
      }

      if (len)
      {
         /* off <= patch_len here, so the remaining-bytes form is safe;
          * "off > patch_len - len" is not, because a record length
          * larger than the whole patch wraps the subtraction and lets
          * the record through. */
         if (len > patch_len - off)
            break;
         recs[n].addr    = addr;
         recs[n].len     = len;
         recs[n].off     = (uint32_t)off;
         recs[n].rle     = 0;
         recs[n].rle_val = 0;
         off            += len;
      }
      else
      {
         uint32_t rle_len;

         if (off > patch_len - 3)
            break;
         rle_len = ((uint32_t)patch[off] << 8) | patch[off + 1];
         off    += 2;
         if (rle_len == 0) /* illegal */
            break;
         recs[n].addr    = addr;
         recs[n].len     = rle_len;
         recs[n].off     = 0;
         recs[n].rle     = 1;
         recs[n].rle_val = patch[off];
         off++;
         len             = rle_len;
      }

      if ((size_t)addr + len > ps->tgt_len)
         ps->tgt_len = (size_t)addr + len;
      n++;
   }

   ps->recs     = recs;
   ps->recs_len = n;

   /* The target is taken by the first copy-through, not here: a source
    * patched in place (patch_stream_finish_into()) never needs one */
   return ps;
}

static void patch_stream_ips_overlay(patch_stream_t *ps, size_t lo, size_t hi)
{
   size_t i;

   for (i = 0; i < ps->recs_len; i++)
   {
      struct patch_stream_rec *r = &ps->recs[i];
      size_t                   a = r->addr;
      size_t                   b = (size_t)r->addr + r->len;
      size_t                   x = (a < lo) ? lo : a;
      size_t                   y = (b < hi) ? b  : hi;

      if (x >= y)
         continue;

      if (r->rle)
      {
         if (!patch_stream_fill(ps, x, r->rle_val, y - x))
            ps->failed = 1;
      }
      else if (!patch_stream_put(ps, x,
               ps->patch + r->off + (x - a), y - x))
         ps->failed = 1;
   }
}

/* The records over [lo, hi) of a target at @dst that holds it all */
static void patch_stream_ips_overlay_into(const patch_stream_t *ps,
      uint8_t *dst, size_t lo, size_t hi)
{
   size_t i;

   for (i = 0; i < ps->recs_len; i++)
   {
      const struct patch_stream_rec *r = &ps->recs[i];
      size_t                         a = r->addr;
      size_t                         b = (size_t)r->addr + r->len;
      size_t                         x = (a < lo) ? lo : a;
      size_t                         y = (b < hi) ? b  : hi;

      if (x >= y)
         continue;
      if (r->rle)
         memset(dst + x, r->rle_val, y - x);
      else
         memcpy(dst + x, ps->patch + r->off + (x - a), y - x);
   }
}

/* A source read in place so far is copied through to the target after
 * all, with the records over it: from here the target is built as the
 * source arrives */
static bool patch_stream_ips_catch_up(patch_stream_t *ps)
{
   const uint8_t *base = ps->src_base;
   size_t         upto = ps->src_seen < ps->src_len
         ? ps->src_seen : ps->src_len;

   ps->src_base = NULL;
   if (!patch_stream_reserve(ps, ps->tgt_len))
   {
      ps->failed = 1;
      return false;
   }
   if (upto)
   {
      if (!patch_stream_put(ps, 0, base, upto))
      {
         ps->failed = 1;
         return false;
      }
      patch_stream_ips_overlay(ps, 0, upto);
   }
   return true;
}

static size_t patch_stream_ips_feed(patch_stream_t *ps,
      const uint8_t *chunk, size_t len)
{
   size_t at;
   size_t copy = len;

   if (ps->src_base)
   {
      /* Where the source lies whole, nothing is copied yet: the
       * records go on it in place when it is finished */
      if (chunk == ps->src_base + ps->src_seen)
      {
         ps->src_seen += len;
         return len;
      }
      if (!patch_stream_ips_catch_up(ps))
         return 0;
   }
   else if (!ps->out_cap && !patch_stream_reserve(ps, ps->tgt_len))
   {
      /* the whole target at once, as the first copy-through */
      ps->failed = 1;
      return 0;
   }

   at = ps->src_seen;

   if (at >= ps->src_len)
      copy = 0;
   else if (at + copy > ps->src_len)
      copy = ps->src_len - at;

   if (copy)
   {
      if (!patch_stream_put(ps, at, chunk, copy))
      {
         ps->failed = 1;
         return 0;
      }
      patch_stream_ips_overlay(ps, at, at + copy);
   }

   ps->src_seen += len;
   return len;
}

static bool patch_stream_ips_finish(patch_stream_t *ps,
      uint8_t **out, size_t *out_len)
{
   if (ps->src_base && !patch_stream_ips_catch_up(ps))
      return false;

   /* A source that delivered fewer bytes than declared - or none at
    * all, for a patch that is pure insertion - leaves a gap the
    * copy-through never covered.  Zero it and overlay the records that
    * fall inside, matching what the whole-buffer applier produces over
    * its own zero-filled tail. */
   if (ps->src_seen < ps->src_len)
   {
      if (!patch_stream_fill(ps, ps->src_seen, 0,
               ps->src_len - ps->src_seen))
         return false;
      patch_stream_ips_overlay(ps, ps->src_seen, ps->src_len);
      ps->src_seen = ps->src_len;
   }

   if (ps->tgt_len > ps->src_seen)
   {
      if (!patch_stream_fill(ps, ps->src_seen, 0,
               ps->tgt_len - ps->src_seen))
         return false;
      patch_stream_ips_overlay(ps, ps->src_seen, ps->tgt_len);
   }

   if (ps->out_len < ps->tgt_len)
   {
      if (!patch_stream_fill(ps, ps->out_len, 0,
               ps->tgt_len - ps->out_len))
         return false;
   }

   if (ps->failed)
      return false;

   ps->out_len = ps->tgt_len;
   *out        = ps->out;
   *out_len    = ps->out_len;
   ps->out     = NULL;
   return true;
}

/* --------------------------------------------------------------------
 * UPS
 *
 * Source and target advance in lockstep and strictly sequentially, so
 * the machine runs as far as the fed source allows and then suspends.
 * A command that would need source past the frontier rolls the cursors
 * back to the command boundary and retries on the next feed.
 * -------------------------------------------------------------------- */

patch_stream_t *patch_stream_ups_open(const uint8_t *patch, size_t patch_len,
      size_t src_len)
{
   patch_stream_t *ps;
   uint64_t declared_src;
   uint64_t declared_tgt;

   if (     patch_len < 18
         || memcmp(patch, "UPS1", 4)
         || !patch_stream_self_checksum_ok(patch, patch_len))
      return NULL;

   if (!(ps = (patch_stream_t*)calloc(1, sizeof(*ps))))
      return NULL;

   ps->fmt       = PATCH_STREAM_UPS;
   ps->patch     = patch;
   ps->patch_len = patch_len;
   ps->src_len   = src_len;
   ps->p_off     = 4;

   /* The caller's src_len stays authoritative for decoding, exactly as
    * in the whole-buffer applier, but the declared lengths are part of
    * what the trailer is checked against. */
   if (   !patch_stream_decode(ps, &declared_src)
       || !patch_stream_decode(ps, &declared_tgt)
       || declared_src > UINT32_MAX
       || declared_tgt > UINT32_MAX)
   {
      patch_stream_free(ps);
      return NULL;
   }
   ps->ups_declared_src_len = (uint32_t)declared_src;
   ps->tgt_len              = (size_t)declared_tgt;
   ps->ups_declared_tgt_len = (uint32_t)declared_tgt;

   ps->carry_cap = 65536;
   if (     !(ps->carry = (uint8_t*)malloc(ps->carry_cap))
         || !patch_stream_reserve(ps, ps->tgt_len))
   {
      patch_stream_free(ps);
      return NULL;
   }

   return ps;
}

static bool patch_stream_ups_src_ready(patch_stream_t *ps)
{
   if (ps->src_base)
      return ps->s_off < ps->src_seen;
   return ps->s_off < ps->carry_base + ps->carry_len;
}

static uint8_t patch_stream_ups_src(patch_stream_t *ps)
{
   uint8_t n = 0;

   if (ps->s_off < ps->src_len)
      n = ps->src_base ? ps->src_base[ps->s_off]
                       : ps->carry[ps->s_off - ps->carry_base];
   ps->s_off++;
   return n;
}

static void patch_stream_ups_out(patch_stream_t *ps, uint8_t n)
{
   if (ps->t_off < ps->tgt_len)
   {
      if (!patch_stream_put(ps, ps->t_off, &n, 1))
         ps->failed = 1;
   }
   ps->t_off++;
}

static uint8_t patch_stream_ups_patch(patch_stream_t *ps)
{
   uint8_t n = 0;

   if (ps->p_off < ps->patch_len)
      n = ps->patch[ps->p_off++];
   return n;
}

static void patch_stream_ups_run(patch_stream_t *ps)
{
   while (ps->p_off < ps->patch_len - 12)
   {
      size_t   save_p = ps->p_off;
      size_t   save_s = ps->s_off;
      size_t   save_t = ps->t_off;
      uint64_t skip;

      if (!patch_stream_decode(ps, &skip))
      {
         ps->failed = 1;
         return;
      }

      while (skip)
      {
         if (ps->s_off < ps->src_len && !patch_stream_ups_src_ready(ps))
         {
            ps->p_off = save_p;
            ps->s_off = save_s;
            ps->t_off = save_t;
            return;
         }

         /* Once both cursors are past their buffers the body of this
          * walk reads nothing and writes nothing - patch_stream_ups_src
          * returns 0 without touching the carry and
          * patch_stream_ups_out drops the byte - so it is pure cursor
          * arithmetic whose result no later step can observe, and the
          * remaining iterations can be dropped.  Rejecting the skip
          * outright is not an option: the whole-buffer applier lets a
          * run overshoot the target and this reader has to match it.
          * Without the fold, a skip varint is bounded only by the
          * decoder, and a well-formed one encoding 2^60 walks for
          * hours - a 27-byte UPS file hangs content loading. */
         if (ps->s_off >= ps->src_len && ps->t_off >= ps->tgt_len)
            break;

         patch_stream_ups_out(ps, patch_stream_ups_src(ps));
         skip--;
      }

      for (;;)
      {
         uint8_t px;
         uint8_t sb;

         if (ps->s_off < ps->src_len && !patch_stream_ups_src_ready(ps))
         {
            ps->p_off = save_p;
            ps->s_off = save_s;
            ps->t_off = save_t;
            return;
         }

         px = patch_stream_ups_patch(ps);
         sb = patch_stream_ups_src(ps);
         patch_stream_ups_out(ps, (uint8_t)(px ^ sb));
         if (px == 0)
            break;
      }

      /* Release source the machine has passed: nothing to do where
       * the caller keeps it. */
      if (!ps->src_base && ps->s_off > ps->carry_base)
      {
         size_t drop = ps->s_off - ps->carry_base;

         if (drop > ps->carry_len)
            drop = ps->carry_len;
         memmove(ps->carry, ps->carry + drop, ps->carry_len - drop);
         ps->carry_len  -= drop;
         ps->carry_base += drop;
      }
   }
}

static bool patch_stream_ups_feed_copy(patch_stream_t *ps,
      const uint8_t *chunk, size_t len);

static size_t patch_stream_ups_feed(patch_stream_t *ps,
      const uint8_t *chunk, size_t len)
{
   if (ps->src_base)
   {
      if (chunk == ps->src_base + ps->src_seen)
      {
         ps->src_crc   = encoding_crc32(ps->src_crc, chunk, len);
         ps->src_seen += len;
         patch_stream_ups_run(ps);
         return len;
      }
      /* Fed from elsewhere after all: the machine is suspended at a
       * command boundary, so what it can still read starts at s_off,
       * copied into the carry before this chunk joins it. */
      {
         size_t from = ps->s_off < ps->src_seen ? ps->s_off : ps->src_seen;
         size_t keep = ps->src_seen - from;
         const uint8_t *base = ps->src_base;
         ps->src_base   = NULL;
         ps->carry_len  = 0;
         ps->carry_base = from;
         if (keep && !patch_stream_ups_feed_copy(ps, base + from, keep))
            return 0;
      }
   }
   if (!patch_stream_ups_feed_copy(ps, chunk, len))
      return 0;
   ps->src_crc = encoding_crc32(ps->src_crc, chunk, len);
   ps->src_seen  += len;
   patch_stream_ups_run(ps);
   return len;
}

/* @len bytes onto the carry's end */
static bool patch_stream_ups_feed_copy(patch_stream_t *ps,
      const uint8_t *chunk, size_t len)
{
   if (ps->carry_len + len > ps->carry_cap)
   {
      size_t   cap = ps->carry_cap;
      uint8_t *tmp;

      while (ps->carry_len + len > cap)
      {
         if (cap > (size_t)-1 / 2)
         {
            ps->failed = 1;
            return false;
         }
         cap <<= 1;
      }
      if (!(tmp = (uint8_t*)realloc(ps->carry, cap)))
      {
         ps->failed = 1;
         return false;
      }
      ps->carry     = tmp;
      ps->carry_cap = cap;
   }

   memcpy(ps->carry + ps->carry_len, chunk, len);
   ps->carry_len += len;
   return true;
}

static bool patch_stream_ups_finish(patch_stream_t *ps,
      uint8_t **out, size_t *out_len)
{
   /* No more source will arrive, so source-exhausted reads now yield 0
    * deterministically and the machine can always make progress.  This
    * is also the only chance to run it at all when the source was empty
    * and feed() was therefore never called. */
   patch_stream_ups_run(ps);

   while (ps->t_off < ps->tgt_len)
      patch_stream_ups_out(ps, patch_stream_ups_src(ps));

   if (ps->out_len < ps->tgt_len)
   {
      if (!patch_stream_fill(ps, ps->out_len, 0, ps->tgt_len - ps->out_len))
         return false;
   }

   if (ps->failed)
      return false;

   ps->out_len = ps->tgt_len;

   /* The trailer: source checksum, target checksum, patch checksum,
    * four bytes each, little endian.  Refuse exactly what the
    * whole-buffer applier refuses - including its allowance for a
    * patch applied in reverse, where the two content checksums swap. */
   {
      uint32_t want_src = 0, want_tgt = 0, want_pat = 0;
      uint32_t have_pat, tgt_crc;
      unsigned i;

      if (ps->patch_len < 12)
         return false;
      for (i = 0; i < 4; i++)
      {
         want_src |= (uint32_t)ps->patch[ps->patch_len - 12 + i] << (i * 8);
         want_tgt |= (uint32_t)ps->patch[ps->patch_len -  8 + i] << (i * 8);
         want_pat |= (uint32_t)ps->patch[ps->patch_len -  4 + i] << (i * 8);
      }
      have_pat = encoding_crc32(0, ps->patch, ps->patch_len - 4);
      if (have_pat != want_pat)
         return false;
      tgt_crc = encoding_crc32(0, ps->out, ps->out_len);

      if (      ps->src_crc == want_src
            &&  ps->ups_declared_src_len == (uint32_t)ps->src_len)
      {
         if (      tgt_crc != want_tgt
               ||  ps->ups_declared_tgt_len != (uint32_t)ps->out_len)
            return false;
      }
      else if (      ps->src_crc == want_tgt
                 &&  ps->ups_declared_tgt_len == (uint32_t)ps->src_len)
      {
         if (      tgt_crc != want_src
               ||  ps->ups_declared_src_len != (uint32_t)ps->out_len)
            return false;
      }
      else
         return false;
   }

   *out        = ps->out;
   *out_len    = ps->out_len;
   ps->out     = NULL;
   return true;
}

/* --------------------------------------------------------------------
 * BPS
 *
 * SourceCopy seeks arbitrarily, so the source is retained in full - as
 * it is in the whole-buffer applier.  Each command is attempted and, if
 * it would read source at or beyond the fed frontier, rolled back whole
 * and retried on the next feed.  Because a command is either fully
 * applied or fully rolled back, the output does not depend on where the
 * chunk boundaries fall.
 * -------------------------------------------------------------------- */

patch_stream_t *patch_stream_bps_open(const uint8_t *patch, size_t patch_len,
      size_t src_len)
{
   patch_stream_t *ps;
   uint64_t        decl_src;
   uint64_t        decl_tgt;
   uint64_t        markup;

   if (     patch_len < 19
         || memcmp(patch, "BPS1", 4)
         || !patch_stream_self_checksum_ok(patch, patch_len))
      return NULL;

   if (!(ps = (patch_stream_t*)calloc(1, sizeof(*ps))))
      return NULL;

   ps->fmt       = PATCH_STREAM_BPS;
   ps->patch     = patch;
   ps->patch_len = patch_len;
   ps->src_len   = src_len;
   ps->p_off     = 4;

   if (   !patch_stream_decode(ps, &decl_src)
       || !patch_stream_decode(ps, &decl_tgt)
       || !patch_stream_decode(ps, &markup))
   {
      patch_stream_free(ps);
      return NULL;
   }

   if (     markup   > patch_len - ps->p_off
         || decl_src > src_len
         || decl_tgt > (uint64_t)((size_t)-1))
   {
      patch_stream_free(ps);
      return NULL;
   }

   ps->p_off  += (size_t)markup;
   ps->tgt_len = (size_t)decl_tgt;

   if (!patch_stream_fill(ps, 0, 0, ps->tgt_len))
   {
      patch_stream_free(ps);
      return NULL;
   }

   return ps;
}

static void patch_stream_bps_run(patch_stream_t *ps)
{
   while (ps->p_off < ps->patch_len - 12)
   {
      size_t   save_p = ps->p_off;
      size_t   save_o = ps->t_off;
      size_t   save_s = ps->s_off;
      size_t   save_t = ps->bt_off;
      uint64_t len;
      unsigned mode;
      uint64_t k;

      if (!patch_stream_decode(ps, &len))
      {
         ps->failed = 1;
         return;
      }
      mode = (unsigned)(len & 3);

      len = (len >> 2) + 1;

      if (ps->t_off >= ps->tgt_len || len > ps->tgt_len - ps->t_off)
      {
         ps->failed = 1;
         return;
      }

      switch (mode)
      {
         case 0: /* SourceRead: source at the output offset */
            if (ps->t_off + len > ps->src_len)
            {
               ps->failed = 1;
               return;
            }
            if (ps->t_off + len > ps->src_seen)
            {
               ps->p_off  = save_p;
               ps->t_off  = save_o;
               ps->s_off  = save_s;
               ps->bt_off = save_t;
               return;
            }
            for (k = 0; k < len; k++)
            {
               uint8_t v = ps->src[ps->t_off];
               if (!patch_stream_put(ps, ps->t_off, &v, 1))
                  ps->failed = 1;
               ps->t_off++;
            }
            break;

         case 1: /* TargetRead: literal bytes from the patch */
            if (ps->p_off + len > ps->patch_len - 12)
            {
               ps->failed = 1;
               return;
            }
            for (k = 0; k < len; k++)
            {
               uint8_t v = ps->patch[ps->p_off++];
               if (!patch_stream_put(ps, ps->t_off, &v, 1))
                  ps->failed = 1;
               ps->t_off++;
            }
            break;

         default: /* SourceCopy / TargetCopy */
         {
            uint64_t decoded;
            uint64_t distance;
            int      neg;

            if (!patch_stream_decode(ps, &decoded))
            {
               ps->failed = 1;
               return;
            }

            /* Sign-magnitude in the varint, so it stays unsigned -
             * see the matching note in tasks/task_patch.c.  Going
             * through int64_t here was correct up to a magnitude of
             * 2^62, but (int64_t)(uint64) above INT64_MAX and the
             * right shift of the negative result it produces are both
             * implementation-defined, and a patch can encode that. */
            neg      = (int)(decoded & 1);
            distance = decoded >> 1;

            if (distance > (uint64_t)(size_t)-1)
            {
               ps->failed = 1;
               return;
            }

            if (mode == 2)
            {
               size_t so = ps->s_off;

               if (neg)
               {
                  if (distance > (uint64_t)so)
                  {
                     ps->failed = 1;
                     return;
                  }
                  so -= (size_t)distance;
               }
               else
               {
                  if (distance > (uint64_t)(ps->src_len - so))
                  {
                     ps->failed = 1;
                     return;
                  }
                  so += (size_t)distance;
               }

               if (len > (uint64_t)(ps->src_len - so))
               {
                  ps->failed = 1;
                  return;
               }
               if ((uint64_t)so + len > (uint64_t)ps->src_seen)
               {
                  ps->p_off  = save_p;
                  ps->t_off  = save_o;
                  ps->s_off  = save_s;
                  ps->bt_off = save_t;
                  return;
               }
               ps->s_off = so;
               for (k = 0; k < len; k++)
               {
                  uint8_t v = ps->src[ps->s_off++];
                  if (!patch_stream_put(ps, ps->t_off, &v, 1))
                     ps->failed = 1;
                  ps->t_off++;
               }
            }
            else
            {
               size_t to = ps->bt_off;

               if (neg)
               {
                  if (distance > (uint64_t)to)
                  {
                     ps->failed = 1;
                     return;
                  }
                  to -= (size_t)distance;
               }
               else
               {
                  if (distance > (uint64_t)(ps->tgt_len - to))
                  {
                     ps->failed = 1;
                     return;
                  }
                  to += (size_t)distance;
               }

               if (len > (uint64_t)(ps->tgt_len - to))
               {
                  ps->failed = 1;
                  return;
               }
               ps->bt_off = to;
               for (k = 0; k < len; k++)
               {
                  uint8_t v = ps->out[ps->bt_off++];
                  if (!patch_stream_put(ps, ps->t_off, &v, 1))
                     ps->failed = 1;
                  ps->t_off++;
               }
            }
            break;
         }
      }
   }
}

/* Room in src_buf for @need bytes */
static bool patch_stream_bps_src_reserve(patch_stream_t *ps, size_t need)
{
   if (need > ps->src_cap)
   {
      size_t   cap = ps->src_cap ? ps->src_cap : 65536;
      uint8_t *tmp;

      while (need > cap)
      {
         if (cap > (size_t)-1 / 2)
         {
            ps->failed = 1;
            return false;
         }
         cap <<= 1;
      }
      if (!(tmp = (uint8_t*)realloc(ps->src_buf, cap)))
      {
         ps->failed = 1;
         return false;
      }
      ps->src_buf = tmp;
      ps->src_cap = cap;
   }
   return true;
}

static size_t patch_stream_bps_feed(patch_stream_t *ps,
      const uint8_t *chunk, size_t len)
{
   if (ps->src_base && chunk == ps->src_base + ps->src_seen)
      /* The whole source is where the caller keeps it: read there */
      ps->src = ps->src_base;
   else
   {
      if (ps->src_base)
      {
         /* Fed from elsewhere after all: what came before is copied
          * out of the caller's buffer, and the source kept from here */
         const uint8_t *base = ps->src_base;
         ps->src_base = NULL;
         if (!patch_stream_bps_src_reserve(ps, ps->src_seen))
            return 0;
         if (ps->src_seen)
            memcpy(ps->src_buf, base, ps->src_seen);
      }
      if (     ps->src_seen > (size_t)-1 - len
            || !patch_stream_bps_src_reserve(ps, ps->src_seen + len))
      {
         ps->failed = 1;
         return 0;
      }
      memcpy(ps->src_buf + ps->src_seen, chunk, len);
      ps->src = ps->src_buf;
   }

   ps->src_crc   = encoding_crc32(ps->src_crc, chunk, len);
   ps->src_seen += len;
   patch_stream_bps_run(ps);
   return len;
}

static bool patch_stream_bps_finish(patch_stream_t *ps,
      uint8_t **out, size_t *out_len)
{
   patch_stream_bps_run(ps);

   if (ps->failed)
      return false;

   if (ps->out_len < ps->tgt_len)
   {
      if (!patch_stream_fill(ps, ps->out_len, 0, ps->tgt_len - ps->out_len))
         return false;
   }

   ps->out_len = ps->tgt_len;

   /* The footer: source, target and patch checksums, four bytes each,
    * little endian.  Refuse what bps_apply_patch refuses, and in the
    * same order, so the reason a patch is rejected matches too. */
   {
      uint32_t want_src = 0, want_tgt = 0, want_pat = 0;
      unsigned i;

      if (ps->patch_len < 12)
         return false;
      for (i = 0; i < 4; i++)
      {
         want_src |= (uint32_t)ps->patch[ps->patch_len - 12 + i] << (i * 8);
         want_tgt |= (uint32_t)ps->patch[ps->patch_len -  8 + i] << (i * 8);
         want_pat |= (uint32_t)ps->patch[ps->patch_len -  4 + i] << (i * 8);
      }
      if (ps->src_crc != want_src)
         return false;
      if (encoding_crc32(0, ps->out, ps->out_len) != want_tgt)
         return false;
      if (encoding_crc32(0, ps->patch, ps->patch_len - 4) != want_pat)
         return false;
   }
   *out        = ps->out;
   *out_len    = ps->out_len;
   ps->out     = NULL;
   return true;
}

/* --------------------------------------------------------------------
 * xdelta (VCDIFF)
 * -------------------------------------------------------------------- */

#ifdef HAVE_XDELTA
patch_stream_t *patch_stream_xdelta_open(const uint8_t *patch,
      size_t patch_len, size_t src_len)
{
   patch_stream_t *ps;

   if (!(ps = (patch_stream_t*)calloc(1, sizeof(*ps))))
      return NULL;
   if (!(ps->vcd = vcdiff_stream_open(patch, patch_len, src_len)))
   {
      free(ps);
      return NULL;
   }
   ps->fmt       = PATCH_STREAM_XDELTA;
   ps->patch     = patch;
   ps->patch_len = patch_len;
   ps->src_len   = src_len;
   return ps;
}
#else
patch_stream_t *patch_stream_xdelta_open(const uint8_t *patch,
      size_t patch_len, size_t src_len)
{
   (void)patch;
   (void)patch_len;
   (void)src_len;
   return NULL;
}
#endif

/* --------------------------------------------------------------------
 * dispatch
 * -------------------------------------------------------------------- */

size_t patch_stream_feed(patch_stream_t *ps, const uint8_t *chunk, size_t len)
{
   if (!ps || !chunk || !len)
      return 0;

   switch (ps->fmt)
   {
      case PATCH_STREAM_IPS:
         return patch_stream_ips_feed(ps, chunk, len);
      case PATCH_STREAM_UPS:
         return patch_stream_ups_feed(ps, chunk, len);
#ifdef HAVE_XDELTA
      case PATCH_STREAM_XDELTA:
         ps->src_seen += len;
         if (vcdiff_stream_feed(ps->vcd, chunk, len) != len)
            ps->failed = 1;
         return len;
#endif
      default:
         break;
   }
   return patch_stream_bps_feed(ps, chunk, len);
}

bool patch_stream_finish(patch_stream_t *ps, uint8_t **out, size_t *out_len)
{
   if (!ps || !out || !out_len)
      return false;

   /* Refuse to finish a stream whose source never fully arrived.
    *
    * The appliers cannot tell a truncated feed from a legitimately
    * short source - both look like "the bytes stopped" - so they would
    * zero-fill the remainder and hand back a plausible-looking buffer
    * built from a partial ROM.  A caller whose read failed part way
    * must fall back to its own error path, not adopt that result, so
    * make the failure explicit here rather than trusting every caller
    * to notice. */
   if (ps->src_seen < ps->src_len)
      return false;

   switch (ps->fmt)
   {
      case PATCH_STREAM_IPS:
         return patch_stream_ips_finish(ps, out, out_len);
      case PATCH_STREAM_UPS:
         return patch_stream_ups_finish(ps, out, out_len);
#ifdef HAVE_XDELTA
      case PATCH_STREAM_XDELTA:
         return vcdiff_stream_finish(ps->vcd, out, out_len);
#endif
      default:
         break;
   }
   return patch_stream_bps_finish(ps, out, out_len);
}

bool patch_stream_failed(patch_stream_t *ps)
{
   return !ps || ps->failed;
}

void patch_stream_set_source(patch_stream_t *ps, const uint8_t *base)
{
   if (!ps || ps->src_seen)
      return;
#ifdef HAVE_XDELTA
   if (ps->fmt == PATCH_STREAM_XDELTA)
   {
      vcdiff_stream_set_source(ps->vcd, base);
      return;
   }
#endif
   ps->src_base = base;
}

bool patch_stream_finish_into(patch_stream_t *ps, uint8_t **buf,
      size_t *len)
{
   uint8_t *out     = NULL;
   size_t   out_len = 0;

   if (!ps || !buf || !*buf || !len)
      return false;

   if (     ps->fmt == PATCH_STREAM_IPS
         && ps->src_base == *buf
         && !ps->failed
         && ps->src_seen >= ps->src_len
         && ps->src_len  <= *len)
   {
      /* The target is the source with the records on it: put them there */
      uint8_t *dst = *buf;
      size_t   tgt = ps->tgt_len;

      /* nothing to hand back, as the copying finish has none */
      if (!tgt)
         return false;
      if (tgt > *len)
      {
         if (!(dst = (uint8_t*)realloc(dst, tgt)))
            return false;
         *buf         = dst;
         ps->src_base = dst;
      }
      if (tgt > ps->src_len)
         memset(dst + ps->src_len, 0, tgt - ps->src_len);
      patch_stream_ips_overlay_into(ps, dst, 0, tgt);
      *len = tgt;
      return true;
   }

   if (!patch_stream_finish(ps, &out, &out_len) || !out)
      return false;
   free(*buf);
   *buf = out;
   *len = out_len;
   return true;
}

void patch_stream_free(patch_stream_t *ps)
{
   if (!ps)
      return;
#ifdef HAVE_XDELTA
   vcdiff_stream_free(ps->vcd);
#endif
   free(ps->out);
   free(ps->recs);
   free(ps->carry);
   free(ps->src_buf);
   free(ps);
}
