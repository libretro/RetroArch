/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vaapi_vp9_pool.h"
#include <limits.h>
#include <string.h>
bool vaapi_vp9_pool_init(vaapi_vp9_pool_t *p,
      const vaapi_vp9_session_t *session, const vaapi_vp9_submit_ops_t *ops)
{
   vaapi_vp9_submit_ops_t native;
   uint64_t generation;
   if (!p || p->active || !session || !session->ready || session->closing
         || !session->display || session->context == VA_INVALID_ID
         || session->slots < 10 || session->slots > 32
         || (session->depth != 8 && session->depth != 10)
         || session->profile
               != (session->depth == 10 ? VAProfileVP9Profile2
                                        : VAProfileVP9Profile0)
         || !ops || !ops->create || !ops->destroy || !ops->begin || !ops->render
         || !ops->end || !ops->sync || p->next_generation == (uint64_t)-1)
      return false;
   native = *ops;
   generation = p->next_generation;
   memset(p, 0, sizeof(*p));
   p->next_generation = generation;
   p->session = session;
   p->display = session->display;
   p->ops = native;
   p->previous_slot = -1;
   p->active = true;
   return true;
}
uint32_t vaapi_vp9_pool_held(const vaapi_vp9_pool_t *p)
{
   uint32_t held;
   unsigned i;
   if (!p || !p->active)
      return 0;
   held = p->exports;
   for (i = 0; i < p->session->slots; i++)
   {
      if (p->transactions[i].active)
         held |= UINT32_C(1) << i;
      held |= p->reads[i];
   }
   return held;
}
int vaapi_vp9_pool_decode(vaapi_vp9_pool_t *p, const uint8_t *packet,
      size_t size, const rvp9_hw_picture_t *picture)
{
   uint32_t bit, reads = 0;
   unsigned i, slot;
   vaapi_vp9_submit_t *transaction;
   bool accepted;
   if (!p || !p->active || p->broken || !picture || !packet || !size
         || size > UINT_MAX || picture->slot < 0
         || (unsigned)picture->slot >= p->session->slots
         || picture->previous_slot != p->previous_slot
         || (p->previous_slot < 0 && picture->header.frame_type)
         || p->next_generation == (uint64_t)-1)
      return -1;
   slot = (unsigned)picture->slot;
   bit = UINT32_C(1) << slot;
   if (vaapi_vp9_pool_held(p) & bit)
      return 1;
   for (i = 0; i < RVP9_REF_FRAMES; i++)
   {
      int reference = picture->reference_slots[i];
      if (reference == -1)
         continue;
      if (reference < 0 || (unsigned)reference >= p->session->slots
            || reference == picture->slot
            || !(p->valid & (UINT32_C(1) << reference)))
         return -1;
      reads |= UINT32_C(1) << reference;
   }
   if (picture->previous_slot >= 0)
   {
      if ((unsigned)picture->previous_slot >= p->session->slots
            || picture->previous_slot == picture->slot
            || !(p->valid & (UINT32_C(1) << picture->previous_slot)))
         return -1;
      reads |= UINT32_C(1) << picture->previous_slot;
   }
   if (!vaapi_vp9_picture(
             &p->work, picture, size, p->session->surfaces, p->session->slots))
      return -1;
   transaction = &p->transactions[slot];
   accepted = vaapi_vp9_submit_start(transaction, p->session, &p->ops, slot,
         &p->work, packet, (unsigned)size);
   if (transaction->active)
   {
      p->reads[slot] = reads;
      p->valid &= ~bit;
      p->ready &= ~bit;
   }
   if (!accepted)
   {
      if (transaction->active)
         p->broken = true;
      return -1;
   }
   p->valid |= bit;
   p->generation[slot] = ++p->next_generation;
   p->previous_slot = (int)slot;
   return 0;
}
int vaapi_vp9_pool_poll(vaapi_vp9_pool_t *p, unsigned slot)
{
   vaapi_vp9_submit_t *transaction;
   uint32_t bit;
   int status;
   if (!p || !p->active || slot >= p->session->slots)
      return -1;
   bit = UINT32_C(1) << slot;
   transaction = &p->transactions[slot];
   if (!transaction->active)
      return (p->ready & bit) ? 0 : -1;
   if (!transaction->submitted)
   {
      if (vaapi_vp9_submit_retire(transaction))
         p->reads[slot] = 0;
      return -1;
   }
   status = vaapi_vp9_submit_poll(transaction);
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
   if (transaction->complete)
   {
      p->reads[slot] = 0;
      vaapi_vp9_submit_retire(transaction);
   }
   return status;
}
bool vaapi_vp9_pool_acquire(
      vaapi_vp9_pool_t *p, unsigned slot, uint64_t *ticket)
{
   uint32_t bit;
   if (!p || !p->active || p->broken || !ticket || slot >= p->session->slots)
      return false;
   bit = UINT32_C(1) << slot;
   if (!(p->ready & p->valid & bit) || (p->exports & bit)
         || !p->generation[slot])
      return false;
   p->exports |= bit;
   *ticket = p->generation[slot];
   return true;
}
bool vaapi_vp9_pool_release(vaapi_vp9_pool_t *p, unsigned slot, uint64_t ticket)
{
   uint32_t bit;
   if (!p || !p->active || slot >= p->session->slots || !ticket)
      return false;
   bit = UINT32_C(1) << slot;
   if (!(p->exports & bit) || p->generation[slot] != ticket)
      return false;
   p->exports &= ~bit;
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
   if (!p || !p->active || !display || p->display != display || p->exports)
      return false;
   generation = p->next_generation;
   memset(p, 0, sizeof(*p));
   p->next_generation = generation;
   return true;
}
