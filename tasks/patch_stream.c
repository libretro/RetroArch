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

#include "patch_stream.h"

enum patch_stream_fmt
{
   PATCH_STREAM_IPS = 0,
   PATCH_STREAM_UPS,
   PATCH_STREAM_BPS
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

   /* UPS: source not yet consumed by the state machine */
   uint8_t       *carry;
   size_t         carry_len;
   size_t         carry_cap;
   size_t         carry_base;

   /* BPS: retained source (SourceCopy seeks arbitrarily) */
   uint8_t       *src_buf;
   size_t         src_cap;
   size_t         bt_off;     /* BPS target_offset cursor              */

   uint8_t        fmt;
   uint8_t        failed;
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
   memset(ps->out + at, val, len);
   if (at + len > ps->out_len)
      ps->out_len = at + len;
   return true;
}

/* Variable-length integer, as used by both UPS and BPS. */
static uint64_t patch_stream_decode(patch_stream_t *ps)
{
   uint64_t val   = 0;
   uint64_t shift = 1;

   for (;;)
   {
      uint8_t x = (ps->p_off < ps->patch_len) ? ps->patch[ps->p_off] : 0;

      if (ps->p_off < ps->patch_len)
         ps->p_off++;

      val += (x & 0x7f) * shift;
      if (x & 0x80)
         break;
      shift <<= 7;
      val   += shift;
   }

   return val;
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
         if (off > patch_len - len)
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

   if (!patch_stream_reserve(ps, ps->tgt_len))
   {
      patch_stream_free(ps);
      return NULL;
   }

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

static size_t patch_stream_ips_feed(patch_stream_t *ps,
      const uint8_t *chunk, size_t len)
{
   size_t at   = ps->src_seen;
   size_t copy = len;

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

   if (patch_len < 18 || memcmp(patch, "UPS1", 4))
      return NULL;

   if (!(ps = (patch_stream_t*)calloc(1, sizeof(*ps))))
      return NULL;

   ps->fmt       = PATCH_STREAM_UPS;
   ps->patch     = patch;
   ps->patch_len = patch_len;
   ps->src_len   = src_len;
   ps->p_off     = 4;

   /* declared source length is read but not used: the caller's src_len
    * is authoritative, exactly as in the whole-buffer applier */
   patch_stream_decode(ps);
   ps->tgt_len   = (size_t)patch_stream_decode(ps);

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
   return ps->s_off < ps->carry_base + ps->carry_len;
}

static uint8_t patch_stream_ups_src(patch_stream_t *ps)
{
   uint8_t n = 0;

   if (ps->s_off < ps->src_len)
      n = ps->carry[ps->s_off - ps->carry_base];
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
      uint64_t skip   = patch_stream_decode(ps);

      while (skip--)
      {
         if (ps->s_off < ps->src_len && !patch_stream_ups_src_ready(ps))
         {
            ps->p_off = save_p;
            ps->s_off = save_s;
            ps->t_off = save_t;
            return;
         }
         patch_stream_ups_out(ps, patch_stream_ups_src(ps));
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

      /* Release source the machine has passed. */
      if (ps->s_off > ps->carry_base)
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

static size_t patch_stream_ups_feed(patch_stream_t *ps,
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
            return 0;
         }
         cap <<= 1;
      }
      if (!(tmp = (uint8_t*)realloc(ps->carry, cap)))
      {
         ps->failed = 1;
         return 0;
      }
      ps->carry     = tmp;
      ps->carry_cap = cap;
   }

   memcpy(ps->carry + ps->carry_len, chunk, len);
   ps->carry_len += len;
   ps->src_seen  += len;
   patch_stream_ups_run(ps);
   return len;
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

   if (patch_len < 19 || memcmp(patch, "BPS1", 4))
      return NULL;

   if (!(ps = (patch_stream_t*)calloc(1, sizeof(*ps))))
      return NULL;

   ps->fmt       = PATCH_STREAM_BPS;
   ps->patch     = patch;
   ps->patch_len = patch_len;
   ps->src_len   = src_len;
   ps->p_off     = 4;

   decl_src = patch_stream_decode(ps);
   decl_tgt = patch_stream_decode(ps);
   markup   = patch_stream_decode(ps);

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
      uint64_t len    = patch_stream_decode(ps);
      unsigned mode   = (unsigned)(len & 3);
      uint64_t k;

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
               uint8_t v = ps->src_buf[ps->t_off];
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
            int64_t  raw = (int64_t)patch_stream_decode(ps);
            int      neg = (int)(raw & 1);
            int64_t  off;

            raw >>= 1;
            off   = neg ? -raw : raw;

            if (mode == 2)
            {
               int64_t so = (int64_t)ps->s_off + off;

               if (     so < 0
                     || (uint64_t)so > (uint64_t)ps->src_len
                     || len > (uint64_t)ps->src_len - (uint64_t)so)
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
               ps->s_off = (size_t)so;
               for (k = 0; k < len; k++)
               {
                  uint8_t v = ps->src_buf[ps->s_off++];
                  if (!patch_stream_put(ps, ps->t_off, &v, 1))
                     ps->failed = 1;
                  ps->t_off++;
               }
            }
            else
            {
               int64_t to = (int64_t)ps->bt_off + off;

               if (     to < 0
                     || (uint64_t)to > (uint64_t)ps->tgt_len
                     || len > (uint64_t)ps->tgt_len - (uint64_t)to)
               {
                  ps->failed = 1;
                  return;
               }
               ps->bt_off = (size_t)to;
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

static size_t patch_stream_bps_feed(patch_stream_t *ps,
      const uint8_t *chunk, size_t len)
{
   if (ps->src_seen + len > ps->src_cap)
   {
      size_t   cap = ps->src_cap ? ps->src_cap : 65536;
      uint8_t *tmp;

      while (ps->src_seen + len > cap)
      {
         if (cap > (size_t)-1 / 2)
         {
            ps->failed = 1;
            return 0;
         }
         cap <<= 1;
      }
      if (!(tmp = (uint8_t*)realloc(ps->src_buf, cap)))
      {
         ps->failed = 1;
         return 0;
      }
      ps->src_buf = tmp;
      ps->src_cap = cap;
   }

   memcpy(ps->src_buf + ps->src_seen, chunk, len);
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
   *out        = ps->out;
   *out_len    = ps->out_len;
   ps->out     = NULL;
   return true;
}

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
      default:
         break;
   }
   return patch_stream_bps_finish(ps, out, out_len);
}

void patch_stream_free(patch_stream_t *ps)
{
   if (!ps)
      return;
   free(ps->out);
   free(ps->recs);
   free(ps->carry);
   free(ps->src_buf);
   free(ps);
}
