/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_av1_pool.h"
#include <limits.h>
#include <string.h>
bool vaapi_av1_pool_init(vaapi_av1_pool_t *p,
      const vaapi_av1_session_t *session, const vaapi_av1_submit_ops_t *ops)
{
   vaapi_av1_submit_ops_t native;
   uint64_t generation;
   if (!p || p->active || !session || !session->ready || session->closing
         || !session->display || session->context == VA_INVALID_ID
         || session->slots < 9 || session->slots > 32
         || (session->depth != 8 && session->depth != 10) || !ops
         || !ops->create || !ops->destroy || !ops->begin || !ops->render
         || !ops->end || !ops->sync || p->next_generation == (uint64_t)-1)
      return false;
#if VA_CHECK_VERSION(1, 8, 0)
   if (session->profile != VAProfileAV1Profile0)
      return false;
#else
   return false;
#endif
   native = *ops;
   generation = p->next_generation;
   memset(p, 0, sizeof(*p));
   p->next_generation = generation;
   p->session = session;
   p->display = session->display;
   p->ops = native;
   p->active = true;
   return true;
}
uint32_t vaapi_av1_pool_held(const vaapi_av1_pool_t *p)
{
   uint32_t held;
   unsigned i;
   if (!p || !p->active)
      return 0;
   held = p->exports;
   for (i = 0; i < p->session->slots; i++)
      if (p->transactions[i].native.active)
         held |= p->reads[i] | p->writes[i];
   return held;
}
int vaapi_av1_pool_decode(vaapi_av1_pool_t *p, const uint8_t *packet,
      size_t size, const rav1_sequence_t *sequence,
      const rav1_hw_picture_t *picture, unsigned display_slot)
{
   uint32_t reads = 0, writes, reference_bit;
   unsigned i, slot, count = 0;
   vaapi_av1_submit_t *transaction;
   bool accepted;
   if (!p || !p->active || p->broken || !sequence || !picture || !packet
         || !size || size > UINT_MAX || picture->slot < 0
         || (unsigned)picture->slot >= p->session->slots
         || display_slot >= p->session->slots
         || p->next_generation >= (uint64_t)-2)
      return -1;
   slot = (unsigned)picture->slot;
   if (!!picture->grain.params.apply != (slot != display_slot))
      return -1;
   reference_bit = UINT32_C(1) << slot;
   writes = reference_bit | (UINT32_C(1) << display_slot);
   if (vaapi_av1_pool_held(p) & writes)
      return 1;
   for (i = 0; i < 8; i++)
   {
      int reference = picture->reference_slots[i];
      uint32_t bit;
      if (reference == -1)
         continue;
      if (reference < 0 || (unsigned)reference >= p->session->slots)
         return -1;
      bit = UINT32_C(1) << reference;
      if ((writes & bit) || !(p->valid & p->references & bit))
         return -1;
      reads |= bit;
   }
   if (!vaapi_av1_picture(&p->work, sequence, picture, p->session->surfaces,
             p->session->slots, p->session->surfaces[display_slot])
         || !vaapi_av1_tiles(p->tiles, 4096, &count, packet, size, picture))
      return -1;
   transaction = &p->transactions[slot];
   accepted = vaapi_av1_submit_start(transaction, p->session, &p->ops, &p->work,
         p->tiles, count, packet, (unsigned)size);
   if (transaction->native.active)
   {
      p->reads[slot] = reads;
      p->writes[slot] = writes;
      p->valid &= ~writes;
      p->ready &= ~writes;
      p->references &= ~writes;
   }
   if (!accepted)
   {
      if (transaction->native.active)
         p->broken = true;
      return -1;
   }
   p->valid |= writes;
   p->references |= reference_bit;
   p->generation[slot] = ++p->next_generation;
   if (display_slot != slot)
      p->generation[display_slot] = ++p->next_generation;
   return 0;
}
int vaapi_av1_pool_poll(vaapi_av1_pool_t *p, unsigned slot)
{
   vaapi_av1_submit_t *transaction;
   uint32_t writes;
   int status;
   if (!p || !p->active || slot >= p->session->slots)
      return -1;
   transaction = &p->transactions[slot];
   if (!transaction->native.active)
      return (p->ready & (UINT32_C(1) << slot)) ? 0 : -1;
   writes = p->writes[slot];
   if (!transaction->native.submitted)
   {
      if (vaapi_av1_submit_retire(transaction))
         p->reads[slot] = p->writes[slot] = 0;
      return -1;
   }
   status = vaapi_av1_submit_poll(transaction);
   if (status == 1)
      return 1;
   if (status < 0)
   {
      p->broken = true;
      p->valid &= ~writes;
      p->references &= ~writes;
      p->ready &= ~writes;
   }
   else
      p->ready |= writes;
   if (vaapi_av1_submit_retire(transaction))
      p->reads[slot] = p->writes[slot] = 0;
   return status;
}
bool vaapi_av1_pool_acquire(
      vaapi_av1_pool_t *p, unsigned slot, uint64_t *ticket)
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
bool vaapi_av1_pool_release(vaapi_av1_pool_t *p, unsigned slot, uint64_t ticket)
{
   uint32_t bit;
   if (!p || !p->active || !ticket || slot >= p->session->slots)
      return false;
   bit = UINT32_C(1) << slot;
   if (!(p->exports & bit) || p->generation[slot] != ticket)
      return false;
   p->exports &= ~bit;
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
   if (!p || !p->active || !display || p->display != display || p->exports)
      return false;
   generation = p->next_generation;
   memset(p, 0, sizeof(*p));
   p->next_generation = generation;
   return true;
}
