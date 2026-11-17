/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "pressure_alloc.h"
#include <assert.h>
#undef malloc
#undef calloc
#undef realloc
#undef free

/* Fixed test-only accounting across the shipping session, queue, mux and
 * transport TUs. No platform linker wrapping or runtime hook is required. */
struct pressure_allocation
{
   void *pointer;
   size_t size;
};
static struct pressure_allocation pressure_allocations[128];
size_t pressure_bytes, pressure_calls, pressure_live;

static void pressure_forget(void *pointer)
{
   unsigned i;
   if (!pointer)
      return;
   for (i = 0; i < 128; i++)
      if (pressure_allocations[i].pointer == pointer)
      {
         pressure_bytes -= pressure_allocations[i].size;
         pressure_live--;
         pressure_allocations[i].pointer = NULL;
         return;
      }
   assert(!"free of untracked allocation");
}
static void *pressure_track(void *pointer, size_t size)
{
   unsigned i;
   pressure_calls++;
   if (!pointer)
      return NULL;
   for (i = 0; i < 128; i++)
      if (!pressure_allocations[i].pointer)
      {
         pressure_allocations[i].pointer = pointer;
         pressure_allocations[i].size = size;
         pressure_bytes += size;
         pressure_live++;
         return pointer;
      }
   assert(!"allocation ledger exhausted");
   return NULL;
}
void *pressure_malloc(size_t size)
{
   return pressure_track(malloc(size), size);
}
void *pressure_calloc(size_t count, size_t size)
{
   assert(!count || size <= (size_t)-1 / count);
   return pressure_track(calloc(count, size), count * size);
}
void *pressure_realloc(void *pointer, size_t size)
{
   void *result;
   unsigned i;
   if (!pointer)
      return pressure_malloc(size);
   if (!size)
   {
      pressure_calls++;
      pressure_free(pointer);
      return NULL;
   }
   for (i = 0; i < 128; i++)
      if (pressure_allocations[i].pointer == pointer)
         break;
   assert(i < 128);
   result = realloc(pointer, size);
   pressure_calls++;
   if (result)
   {
      pressure_bytes -= pressure_allocations[i].size;
      pressure_allocations[i].pointer = result;
      pressure_allocations[i].size = size;
      pressure_bytes += size;
   }
   return result;
}
void pressure_free(void *pointer)
{
   pressure_forget(pointer);
   free(pointer);
}
