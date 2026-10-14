/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 The RetroArch team
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef __LIBRETRO_SDK_FORMAT_RH265_HW_H__
#define __LIBRETRO_SDK_FORMAT_RH265_HW_H__
#include <string.h>
#include <boolean.h>
#include <retro_inline.h>
#include <formats/rh265.h>

/* Owner-thread sequencing only. The backend still protects GPU writes and
 * exported frame leases with its fences; these masks are not GPU completion. */
typedef struct rh265_hw_state
{
   uint64_t serial, epoch, output_epoch[32];
   int32_t poc[32], prev_poc_tid0;
   uint32_t references, outputs;
   unsigned slots, width, height, bit_depth, poc_log2;
   int started, rasl_skip;
} rh265_hw_state_t;

typedef struct rh265_hw_picture
{
   uint64_t serial, epoch;
   int32_t poc, prev_poc_tid0;
   uint32_t keep_references, keep_outputs;
   unsigned slot, num_refs, width, height, bit_depth, poc_log2;
   int reference, output, rasl_skip;
   struct { int32_t poc; unsigned slot; } refs[15];
} rh265_hw_picture_t;

/* Reset only after pending transactions and backend leases are retired. */
static INLINE bool rh265_hw_state_init(rh265_hw_state_t *s, unsigned slots)
{
   if (!s) return false;
   memset(s, 0, sizeof(*s));
   if (!slots || slots > 32) return false;
   s->slots = slots; s->serial = s->epoch = 1;
   return true;
}

/* 0 ready, 1 backpressure, 2 skipped RASL, negative invalid/unsupported.
 * No state changes. Keep the packet and prepared metadata immutable while
 * the backend is busy; commit once only, after decode accepts this plan. */
static INLINE int rh265_hw_prepare(const rh265_hw_state_t *s,
      const rh265_video_params_t *params, const rh265_access_unit_t *au,
      rh265_hw_picture_t *out)
{
   const rh265_slice_info_t *h;
   int64_t poc, prev_lsb, prev_msb, msb, max_lsb;
   unsigned type, i, list, held = 0;
   bool anchor, idr;
   uint32_t occupied;
   if (!out) return -1;
   memset(out, 0, sizeof(*out));
   if (!s || !params || !au || !s->slots || s->slots > 32 || !s->serial
         || s->serial == UINT64_MAX || !au->num_slices || params->chroma_format_idc != 1
         || (params->bit_depth != 8 && params->bit_depth != 10)
         || params->width <= 0 || params->height <= 0
         || params->log2_max_poc_lsb < 4 || params->log2_max_poc_lsb > 16
         || params->max_dec_pic_buffering < 1 || params->max_dec_pic_buffering > 16
         || params->max_num_reorder_pics < 0
         || params->max_sub_layers_minus1 < 0 || params->max_sub_layers_minus1 > 6
         || au->intra < 0 || au->intra > 1
         || params->max_num_reorder_pics >= params->max_dec_pic_buffering)
      return -1;
   h = &au->first;
   type = (unsigned)h->nal_type;
   if (!(type <= 9 || (type >= 16 && type <= 21)) || h->first_slice_in_pic != 1
         || h->pps_id != params->pps_id
         || h->temporal_id < 0 || h->temporal_id > params->max_sub_layers_minus1
         || (type >= 16 && h->temporal_id) || h->no_output_of_prior_pics < 0
         || h->no_output_of_prior_pics > 1
         || h->num_negative < 0 || h->num_negative > 16
         || h->num_positive < 0 || h->num_positive > 16
         || h->pic_output_flag < 0 || h->pic_output_flag > 1)
      return -1;
   if (params->pcm_enabled || params->tiles_enabled || params->dependent_slice_segments_enabled
         || params->long_term_ref_pics_present
         || s->slots < (unsigned)params->max_dec_pic_buffering
         || (!s->started && type < 16) || (type >= 16 && !au->intra)
         || (s->started && (s->width != (unsigned)params->width
               || s->height != (unsigned)params->height || s->bit_depth != (unsigned)params->bit_depth
               || s->poc_log2 != (unsigned)params->log2_max_poc_lsb)))
      return -2;
   if (s->rasl_skip && (type == 8 || type == 9)) return 2;
   idr = type == 19 || type == 20;
   if (idr && (h->num_negative || h->num_positive || h->short_term_rps_bits
         || h->predictor_delta_pocs)) return -1;
   anchor = type >= 16 && (type != 21 || !s->started);
   max_lsb = INT64_C(1) << params->log2_max_poc_lsb;
   if (h->poc_lsb < 0 || h->poc_lsb >= max_lsb || (idr && h->poc_lsb)) return -1;
   if (anchor) poc = idr ? 0 : h->poc_lsb;
   else
   {
      prev_lsb = (uint32_t)s->prev_poc_tid0 & (uint32_t)(max_lsb - 1);
      prev_msb = (int64_t)s->prev_poc_tid0 - prev_lsb;
      msb = prev_msb;
      if (h->poc_lsb < prev_lsb && prev_lsb - h->poc_lsb >= max_lsb / 2)
         msb += max_lsb;
      else if (h->poc_lsb > prev_lsb && h->poc_lsb - prev_lsb > max_lsb / 2)
         msb -= max_lsb;
      poc = msb + h->poc_lsb;
   }
   if (poc < INT32_MIN || poc > INT32_MAX) return -1;
   out->poc = (int32_t)poc;
   out->prev_poc_tid0 = s->prev_poc_tid0;
   if (!h->temporal_id && !(type >= 6 && type <= 9) && !(type < 16 && !(type & 1)))
      out->prev_poc_tid0 = out->poc;
   out->serial = s->serial;
   out->epoch = s->epoch;
   if (anchor && s->started)
   {
      if (s->epoch == UINT64_MAX) goto invalid;
      out->epoch++;
   }
   out->keep_outputs = anchor && h->no_output_of_prior_pics ? 0 : s->outputs;
   out->rasl_skip = anchor || (s->rasl_skip && type >= 6 && type <= 9);
   for (list = 0; list < 2; list++)
   {
      unsigned count = (unsigned)(list ? h->num_positive : h->num_negative);
      const int32_t *delta = list ? h->delta_poc_after : h->delta_poc_before;
      const uint8_t *used = list ? h->used_after : h->used_before;
      for (i = 0; i < count; i++)
      {
         unsigned j, found = s->slots;
         int64_t ref_poc = poc + delta[i];
         if (used[i] > 1 || (list ? delta[i] <= 0 : delta[i] >= 0)
               || (i && (list ? delta[i] <= delta[i-1] : delta[i] >= delta[i-1]))
               || ref_poc < INT32_MIN || ref_poc > INT32_MAX) goto invalid;
         if (!anchor)
            for (j = 0; j < s->slots; j++)
               if ((s->references & (UINT32_C(1) << j)) && s->poc[j] == ref_poc)
               {
                  if (found != s->slots) goto invalid;
                  found = j;
               }
         if (found < s->slots) out->keep_references |= UINT32_C(1) << found;
         else if (used[i]) goto invalid;
      }
   }
   for (i = 0; i < s->slots; i++)
      if (out->keep_references & (UINT32_C(1) << i))
      {
         unsigned n = out->num_refs++;
         if (n >= 15) goto invalid;
         out->refs[n].poc = s->poc[i]; out->refs[n].slot = i;
      }
   if (out->num_refs >= (unsigned)params->max_dec_pic_buffering) goto invalid;
   occupied = out->keep_references | out->keep_outputs;
   out->slot = s->slots;
   for (i = 0; i < s->slots; i++)
   {
      if (occupied & (UINT32_C(1) << i)) held++;
      else if (out->slot == s->slots) out->slot = i;
   }
   if (out->slot == s->slots || held >= (unsigned)params->max_dec_pic_buffering)
   { memset(out, 0, sizeof(*out)); return 1; }
   out->reference = type >= 16 || (type & 1);
   out->output = h->pic_output_flag;
   out->width = (unsigned)params->width; out->height = (unsigned)params->height;
   out->bit_depth = (unsigned)params->bit_depth;
   out->poc_log2 = (unsigned)params->log2_max_poc_lsb;
   return 0;
invalid:
   memset(out, 0, sizeof(*out));
   return -1;
}

/* Output releases may occur between prepare and commit; another decode or
 * reset may not. Backend acceptance must precede this owner-only commit. */
static INLINE bool rh265_hw_commit(rh265_hw_state_t *s, const rh265_hw_picture_t *p)
{
   uint32_t bit;
   if (!s || !p || p->serial != s->serial || !s->serial || s->serial == UINT64_MAX
         || p->slot >= s->slots || p->slot >= 32 || p->num_refs > 15
         || (p->keep_references & ~s->references)) return false;
   bit = UINT32_C(1) << p->slot;
   if ((p->keep_references | (p->keep_outputs & s->outputs)) & bit) return false;
   s->references = p->keep_references | (p->reference ? bit : 0);
   s->outputs = (s->outputs & p->keep_outputs) | (p->output ? bit : 0);
   s->poc[p->slot] = p->poc; s->output_epoch[p->slot] = p->epoch;
   s->prev_poc_tid0 = p->prev_poc_tid0; s->epoch = p->epoch;
   s->width = p->width; s->height = p->height; s->bit_depth = p->bit_depth; s->poc_log2 = p->poc_log2;
   s->started = 1; s->rasl_skip = p->rasl_skip; s->serial++;
   return true;
}

/* Pending output is held until the backend successfully exports it. */
static INLINE int rh265_hw_output_pick(const rh265_hw_state_t *s,
      unsigned max_reorder, bool drain)
{
   unsigned i, count = 0;
   int best = -1;
   if (!s || s->slots > 32) return -1;
   for (i = 0; i < s->slots; i++)
      if (s->outputs & (UINT32_C(1) << i))
      {
         count++;
         if (best < 0 || s->output_epoch[i] < s->output_epoch[best]
               || (s->output_epoch[i] == s->output_epoch[best] && s->poc[i] < s->poc[best]))
            best = (int)i;
      }
   if (best < 0 || (!drain && count <= max_reorder && s->output_epoch[best] == s->epoch))
      return -1;
   return best;
}

static INLINE bool rh265_hw_output_release(rh265_hw_state_t *s, unsigned slot)
{
   uint32_t bit;
   if (!s || slot >= s->slots || slot >= 32) return false;
   bit = UINT32_C(1) << slot;
   if (!(s->outputs & bit)) return false;
   s->outputs &= ~bit;
   return true;
}
#endif
