/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vaapi_vp9_pool.h"

bool vaapi_vp9_pool_init(vaapi_vp9_pool_t *p,
      const vaapi_vp9_session_t *s, const vaapi_vp9_submit_ops_t *ops,
      unsigned logical_slots)
{
   vaapi_vp9_submit_ops_t native;
   uint64_t generation;
   unsigned i, j;
   if (!p || p->active || !s || !s->ready || s->closing || !s->display
         || s->context == VA_INVALID_ID || logical_slots < 10
         || logical_slots > 32 || s->slots < logical_slots || s->slots > 32
         || !ops || !ops->create || !ops->destroy || !ops->begin
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
   for (i = 0; i < 32; i++)
      p->mapping[i] = -1;
   p->active = true;
   return true;
}

uint32_t vaapi_vp9_pool_held(const vaapi_vp9_pool_t *p)
{
   unsigned i;
   uint32_t held = 0;
   if (!p || !p->active)
      return 0;
   for (i = 0; i < p->session->slots; i++)
   {
      if (p->transactions[i].active || p->exports[i])
         held |= UINT32_C(1) << i;
      held |= p->reads[i];
   }
   return held;
}

int vaapi_vp9_pool_decode(vaapi_vp9_pool_t *p,
      const rvp9_hw_picture_t *picture, const uint8_t *data, size_t size)
{
   VASurfaceID view[32];
   vaapi_vp9_params_t params;
   vaapi_vp9_submit_t *transaction;
   uint32_t held, reads = 0, protect = 0, bit;
   unsigned i, physical;
   bool accepted;
   if (!p || !p->active || p->broken || !picture || !data || !size
         || size > UINT_MAX || picture->slot < 0
         || (unsigned)picture->slot >= p->logical_slots
         || picture->header.show_existing_frame
         || p->next_generation == (uint64_t)-1)
      return -1;
   for (i = 0; i < 9; i++)
   {
      int slot = i < 8 ? picture->reference_slots[i] : picture->previous_slot;
      int mapped;
      if (slot == -1)
         continue;
      if (slot < 0 || (unsigned)slot >= p->logical_slots
            || slot == picture->slot)
         return -1;
      mapped = p->mapping[slot];
      if (mapped < 0 || !(p->valid & (UINT32_C(1) << mapped)))
         return -1;
      protect |= UINT32_C(1) << mapped;
      if (i < 8 || picture->use_prev_frame_mvs)
         reads |= UINT32_C(1) << mapped;
   }
   held = vaapi_vp9_pool_held(p) | protect;
   for (physical = 0; physical < p->session->slots; physical++)
      if (!(held & (UINT32_C(1) << physical)))
         break;
   if (physical == p->session->slots)
      return 1;
   for (i = 0; i < p->logical_slots; i++)
   {
      int mapped = p->mapping[i];
      view[i] = mapped >= 0 && mapped != (int)physical
            && (p->valid & (UINT32_C(1) << mapped))
            ? p->session->surfaces[mapped] : VA_INVALID_SURFACE;
   }
   view[picture->slot] = p->session->surfaces[physical];
   if (!vaapi_vp9_picture(&params, picture, size, view, p->logical_slots))
      return -1;
   transaction = &p->transactions[physical];
   accepted = vaapi_vp9_submit_start(transaction, p->session, &p->ops,
         &params, data, (unsigned)size);
   bit = UINT32_C(1) << physical;
   if (transaction->active)
   {
      p->reads[physical] = reads;
      p->valid &= ~bit;
      p->ready &= ~bit;
      for (i = 0; i < p->logical_slots; i++)
         if (p->mapping[i] == (int)physical)
            p->mapping[i] = -1;
   }
   if (!accepted)
   {
      if (transaction->active)
         p->broken = true;
      return -1;
   }
   p->mapping[picture->slot] = (int)physical;
   p->generation[physical] = ++p->next_generation;
   p->valid |= bit;
   return 0;
}

int vaapi_vp9_pool_poll(vaapi_vp9_pool_t *p, unsigned slot)
{
   vaapi_vp9_submit_t *t;
   uint32_t bit;
   int status;
   if (!p || !p->active || slot >= p->session->slots)
      return -1;
   bit = UINT32_C(1) << slot;
   t = &p->transactions[slot];
   if (!t->active)
      return p->ready & bit ? 0 : -1;
   if (!t->submitted)
   {
      if (vaapi_vp9_submit_retire(t))
         p->reads[slot] = 0;
      return -1;
   }
   status = vaapi_vp9_submit_poll(t);
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
      vaapi_vp9_submit_retire(t);
   }
   return status;
}

bool vaapi_vp9_pool_acquire(vaapi_vp9_pool_t *p, unsigned slot, uint64_t *ticket)
{
   if (!p || !p->active || p->broken || !ticket || slot >= p->session->slots
         || !(p->ready & (UINT32_C(1) << slot)) || !p->generation[slot]
         || p->exports[slot] == UINT_MAX)
      return false;
   p->exports[slot]++;
   *ticket = p->generation[slot];
   return true;
}

bool vaapi_vp9_pool_current(const vaapi_vp9_pool_t *p,
      unsigned slot, uint64_t ticket)
{
   return p && p->active && slot < p->session->slots && ticket
         && p->exports[slot] && p->generation[slot] == ticket;
}

bool vaapi_vp9_pool_release(vaapi_vp9_pool_t *p, unsigned slot, uint64_t ticket)
{
   if (!vaapi_vp9_pool_current(p, slot, ticket))
      return false;
   p->exports[slot]--;
   return true;
}

bool vaapi_vp9_pool_close(vaapi_vp9_pool_t *p)
{
   uint64_t generation;
   if (!p || !p->active)
      return true;
   if (p->broken || vaapi_vp9_pool_held(p))
      return false;
   generation = p->next_generation;
   memset(p, 0, sizeof(*p));
   p->next_generation = generation;
   return true;
}

bool vaapi_vp9_pool_terminated(vaapi_vp9_pool_t *p, VADisplay display)
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
