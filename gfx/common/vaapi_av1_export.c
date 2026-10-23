/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vaapi_av1_export.h"
static bool vaapi_av1_export_acquire(
      void *owner, unsigned slot, uint64_t *ticket)
{
   return vaapi_av1_pool_acquire((vaapi_av1_pool_t *)owner, slot, ticket);
}
static bool vaapi_av1_export_current(
      void *owner, unsigned slot, uint64_t ticket)
{
   const vaapi_av1_pool_t *pool = (const vaapi_av1_pool_t *)owner;
   return pool && pool->active && !pool->broken && slot < 32 && ticket
         && (pool->exports & (UINT32_C(1) << slot))
         && pool->generation[slot] == ticket;
}
static bool vaapi_av1_export_release(
      void *owner, unsigned slot, uint64_t ticket)
{
   return vaapi_av1_pool_release((vaapi_av1_pool_t *)owner, slot, ticket);
}
bool vaapi_av1_export_open(vaapi_av1_export_t *source, vaapi_av1_pool_t *pool,
      unsigned slot, const vaapi_av1_export_ops_t *ops)
{
   static const vaapi_export_owner_ops_t ownership = {vaapi_av1_export_acquire,
         vaapi_av1_export_current, vaapi_av1_export_release};
   if (!pool)
      return false;
   return vaapi_export_open(source, pool, pool->session, &ownership, slot, ops);
}
