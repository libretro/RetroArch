/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vaapi_av1_pool.h"
#if RARCH_VAAPI_AV1_PARAMS_AVAILABLE

static bool vaapi_av1_pool_init_mode(vaapi_av1_pool_t *p,
      const vaapi_av1_session_t *s, const vaapi_av1_submit_ops_t *ops,
      unsigned logical_slots, bool grain)
{
   vaapi_av1_submit_ops_t native;
   uint64_t generation;
   unsigned i, j;
   if (!p || p->active || !s || !s->ready || s->closing || !s->display
         || s->context == VA_INVALID_ID || logical_slots < 9
         || logical_slots > 32 || s->slots < logical_slots || s->slots > 32
         || (grain && s->slots < 18)
         || !ops || vaapi_av1_overlap(p, sizeof(*p), s, sizeof(*s))
         || vaapi_av1_overlap(p, sizeof(*p), ops, sizeof(*ops))
         || !ops->create || !ops->destroy || !ops->begin
         || !ops->render || !ops->end || !ops->sync)
      return false;
   for (i = 0; i < s->slots; i++)
   {
      if (s->surfaces[i] == VA_INVALID_SURFACE)
         return false;
      for (j = 0; j < i; j++)
         if (s->surfaces[i] == s->surfaces[j])
            return false;
   }
   native = *ops;
   generation = p->next_generation;
   memset(p, 0, sizeof(*p));
   p->next_generation = generation;
   p->session = s;
   p->display = s->display;
   p->ops = native;
   p->logical_slots = logical_slots;
   p->grain = grain;
   for (i = 0; i < 32; i++)
   {
      p->mapping[i] = -1;
      p->outputs[i] = i;
   }
   p->active = true;
   return true;
}

bool vaapi_av1_pool_init(vaapi_av1_pool_t *p,
      const vaapi_av1_session_t *s, const vaapi_av1_submit_ops_t *ops,
      unsigned logical_slots)
{
   return vaapi_av1_pool_init_mode(p, s, ops, logical_slots, false);
}

bool vaapi_av1_pool_init_grain(vaapi_av1_pool_t *p,
      const vaapi_av1_session_t *s, const vaapi_av1_submit_ops_t *ops,
      unsigned logical_slots)
{
   return vaapi_av1_pool_init_mode(p, s, ops, logical_slots, true);
}

uint32_t vaapi_av1_pool_held(const vaapi_av1_pool_t *p)
{
   unsigned i;
   uint32_t held = 0;
   if (!p || !p->active)
      return 0;
   for (i = 0; i < p->session->slots; i++)
   {
      if (p->transactions[i].active || p->exports[i])
         held |= UINT32_C(1) << i;
      held |= p->reads[i] | p->writes[i];
   }
   return held;
}

int vaapi_av1_pool_decode(vaapi_av1_pool_t *p,
      const rav1_sequence_t *sequence, const rav1_hw_picture_t *picture,
      const uint8_t *data, size_t size)
{
   VASurfaceID view[32];
   vaapi_av1_submit_t *transaction;
   uint32_t held, reads = 0, bit;
   unsigned i, physical, output;
   bool accepted;
   if (!p || !p->active || p->broken || !sequence || !picture || !data || !size
         || size > UINT_MAX || picture->slot < 0
         || (unsigned)picture->slot >= p->logical_slots
         || picture->header.show_existing
         || (picture->grain.params.apply && !p->grain)
         || p->next_generation == (uint64_t)-1
         || sequence->max_width != p->session->width
         || sequence->max_height != p->session->height
         || sequence->bit_depth != p->session->depth
         || vaapi_av1_overlap(p, sizeof(*p), sequence, sizeof(*sequence))
         || vaapi_av1_overlap(p, sizeof(*p), picture, sizeof(*picture))
         || vaapi_av1_overlap(p, sizeof(*p), data, size))
      return -1;
   for (i = 0; i < 8; i++)
   {
      int slot = picture->reference_slots[i];
      int mapped;
      if (slot == -1)
         continue;
      if (slot < 0 || (unsigned)slot >= p->logical_slots
            || slot == picture->slot)
         return -1;
      mapped = p->mapping[slot];
      if (mapped < 0 || !(p->valid & (UINT32_C(1) << mapped)))
         return -1;
      reads |= (UINT32_C(1) << mapped)
            | (UINT32_C(1) << p->outputs[mapped]);
   }
   held = vaapi_av1_pool_held(p) | reads;
   for (physical = 0; physical < p->session->slots; physical++)
      if (!(held & (UINT32_C(1) << physical)))
         break;
   if (physical == p->session->slots)
      return 1;
   output = physical;
   if (picture->grain.params.apply)
   {
      for (output = physical + 1; output < p->session->slots; output++)
         if (!(held & (UINT32_C(1) << output)))
            break;
      if (output == p->session->slots)
         return 1;
   }
   bit = (UINT32_C(1) << physical) | (UINT32_C(1) << output);
   for (i = 0; i < p->logical_slots; i++)
   {
      int mapped = p->mapping[i];
      view[i] = mapped >= 0 &&
            !(bit & ((UINT32_C(1) << mapped) |
                     (UINT32_C(1) << p->outputs[mapped])))
            && (p->valid & (UINT32_C(1) << mapped))
            ? p->session->surfaces[mapped] : VA_INVALID_SURFACE;
   }
   view[picture->slot] = p->session->surfaces[physical];
   if (!vaapi_av1_picture_display(&p->params, p->slices, 4096, data, size,
         sequence, picture, view, p->logical_slots, output != physical
            ? p->session->surfaces[output] : VA_INVALID_SURFACE))
      return -1;
   transaction = &p->transactions[physical];
   accepted = vaapi_av1_submit_start(transaction, p->session, &p->ops,
         &p->params, p->slices, data, (unsigned)size);
   if (transaction->active)
   {
      p->reads[physical] = reads;
      p->writes[physical] = bit;
      p->valid &= ~bit;
      p->ready &= ~bit;
      for (i = 0; i < p->logical_slots; i++)
      {
         int mapped = p->mapping[i];
         if (mapped >= 0 && (bit & ((UINT32_C(1) << mapped) |
                                    (UINT32_C(1) << p->outputs[mapped]))))
            p->mapping[i] = -1;
      }
   }
   if (!accepted)
   {
      if (transaction->active)
         p->broken = true;
      return -1;
   }
   p->mapping[picture->slot] = (int)physical;
   p->outputs[physical] = output;
   p->generation[physical] = p->generation[output] = ++p->next_generation;
   p->valid |= bit;
   return 0;
}

int vaapi_av1_pool_poll(vaapi_av1_pool_t *p, unsigned slot)
{
   vaapi_av1_submit_t *t;
   uint32_t bit;
   int status;
   unsigned i;
   if (!p || !p->active || slot >= p->session->slots)
      return -1;
   bit = UINT32_C(1) << slot;
   t = &p->transactions[slot];
   if (!t->active)
   {
      for (i = 0; i < p->session->slots; i++)
         if (p->writes[i] & bit)
         {
            slot = i;
            t = &p->transactions[slot];
            break;
         }
      if (!t->active)
         return p->ready & bit ? 0 : -1;
   }
   bit = p->writes[slot];
   if (!t->submitted)
   {
      if (vaapi_av1_submit_retire(t))
         p->reads[slot] = p->writes[slot] = 0;
      return -1;
   }
   status = vaapi_av1_submit_poll(t);
   if (status == 1)
      return 1;
   if (status < 0)
   {
      p->broken = true;
      p->valid &= ~bit;
      p->ready &= ~bit;
   }
   else
      p->ready |= bit;
   if (t->complete)
   {
      p->reads[slot] = 0;
      if (vaapi_av1_submit_retire(t))
         p->writes[slot] = 0;
   }
   return status;
}

bool vaapi_av1_pool_acquire(vaapi_av1_pool_t *p, unsigned slot, uint64_t *ticket)
{
   if (!p || !p->active || p->broken || !ticket || slot >= p->session->slots
         || !(p->ready & (UINT32_C(1) << slot)) || !p->generation[slot]
         || p->exports[slot] == UINT_MAX)
      return false;
   p->exports[slot]++;
   *ticket = p->generation[slot];
   return true;
}

bool vaapi_av1_pool_current(const vaapi_av1_pool_t *p,
      unsigned slot, uint64_t ticket)
{
   return p && p->active && slot < p->session->slots && ticket
         && p->exports[slot] && p->generation[slot] == ticket;
}

bool vaapi_av1_pool_release(vaapi_av1_pool_t *p, unsigned slot, uint64_t ticket)
{
   if (!vaapi_av1_pool_current(p, slot, ticket))
      return false;
   p->exports[slot]--;
   return true;
}

bool vaapi_av1_pool_close(vaapi_av1_pool_t *p)
{
   uint64_t generation;
   if (!p || !p->active)
      return true;
   if (p->broken || vaapi_av1_pool_held(p))
      return false;
   generation = p->next_generation;
   memset(p, 0, sizeof(*p));
   p->next_generation = generation;
   return true;
}

bool vaapi_av1_pool_terminated(vaapi_av1_pool_t *p, VADisplay display)
{
   uint64_t generation;
   unsigned i;
   if (!p || !p->active || !display || p->display != display)
      return false;
   for (i = 0; i < p->session->slots; i++)
      if (p->exports[i])
         return false;
   generation = p->next_generation;
   memset(p, 0, sizeof(*p));
   p->next_generation = generation;
   return true;
}

#endif
