/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vaapi_vp9_export.h"
static bool vaapi_vp9_export_acquire(void *owner, unsigned slot, uint64_t *ticket)
{
   return vaapi_vp9_pool_acquire((vaapi_vp9_pool_t *)owner, slot, ticket);
}
static bool vaapi_vp9_export_current(void *owner, unsigned slot, uint64_t ticket)
{
   const vaapi_vp9_pool_t *pool = (const vaapi_vp9_pool_t *)owner;
   return pool && pool->active && !pool->broken && slot < 32 && ticket
         && (pool->exports & (UINT32_C(1) << slot))
         && pool->generation[slot] == ticket;
}
static bool vaapi_vp9_export_release(void *owner, unsigned slot, uint64_t ticket)
{
   return vaapi_vp9_pool_release((vaapi_vp9_pool_t *)owner, slot, ticket);
}
bool vaapi_vp9_export_open(vaapi_vp9_export_t *source, vaapi_vp9_pool_t *pool,
      unsigned slot, const vaapi_vp9_export_ops_t *ops)
{
   static const vaapi_export_owner_ops_t ownership = {
         vaapi_vp9_export_acquire, vaapi_vp9_export_current,
         vaapi_vp9_export_release};
   if (!pool)
      return false;
   return vaapi_export_open(source, pool, pool->session, &ownership, slot, ops);
}
