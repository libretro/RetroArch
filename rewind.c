/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "rewind.h"
#include "performance.h"
#include <stdlib.h>
#include <stdint.h>
#include "boolean.h"
#include <string.h>
#include <limits.h>
#include "general.h"

struct state_manager
{
   uint64_t *buffer;
   size_t buf_size;
   size_t buf_size_mask;
   uint32_t *tmp_state;
   size_t top_ptr;
   size_t bottom_ptr;
   size_t state_size;
   bool first_pop;
};

static inline size_t nearest_pow2_size(size_t v)
{
   size_t orig = v;
   v--;
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
#if SIZE_MAX >= UINT16_C(0xffff)
      v |= v >> 8;
#endif
#if SIZE_MAX >= UINT32_C(0xffffffff)
      v |= v >> 16;
#endif
#if SIZE_MAX >= UINT64_C(0xffffffffffffffff)
      v |= v >> 32;
#endif
   v++;

   size_t next = v;
   size_t prev = v >> 1;

   if ((next - orig) < (orig - prev))
      return next;
   else
      return prev;
}

state_manager_t *state_manager_new(size_t state_size, size_t buffer_size, void *init_buffer)
{
   if (buffer_size <= state_size * 4) // Need a sufficient buffer size.
      return NULL;

   state_manager_t *state = (state_manager_t*)calloc(1, sizeof(*state));
   if (!state)
      return NULL;

   // We need 4-byte aligned state_size to avoid having to enforce this with unneeded memcpy's!
   rarch_assert(state_size % 4 == 0);
   state->top_ptr = 1;

   state->state_size = state_size / sizeof(uint32_t); // Works in multiple of 4.
   state->buf_size = nearest_pow2_size(buffer_size) / sizeof(uint64_t); // Works in multiple of 8.
   state->buf_size_mask = state->buf_size - 1;
   RARCH_LOG("Readjusted rewind buffer size to %u MiB\n", (unsigned)(sizeof(uint64_t) * (state->buf_size >> 20)));

   if (!(state->buffer = (uint64_t*)calloc(1, state->buf_size * sizeof(uint64_t))))
      goto error;
   if (!(state->tmp_state = (uint32_t*)calloc(1, state->state_size * sizeof(uint32_t))))
      goto error;

   memcpy(state->tmp_state, init_buffer, state_size);

   return state;

error:
   if (state)
   {
      free(state->buffer);
      free(state->tmp_state);
      free(state);
   }
   return NULL;
}

void state_manager_free(state_manager_t *state)
{
   free(state->buffer);
   free(state->tmp_state);
   free(state);
}

bool state_manager_pop(state_manager_t *state, void **data)
{ 
   *data = state->tmp_state;
   if (state->first_pop)
   {
      state->first_pop = false;
      return true;
   }

   state->top_ptr = (state->top_ptr - 1) & state->buf_size_mask;

   if (state->top_ptr == state->bottom_ptr) // Our stack is completely empty... :v
   {
      state->top_ptr = (state->top_ptr + 1) & state->buf_size_mask;
      return false;
   }

   while (state->buffer[state->top_ptr])
   {
      // Apply the xor patch.
      uint32_t addr = state->buffer[state->top_ptr] >> 32;
      uint32_t xor_ = state->buffer[state->top_ptr] & 0xFFFFFFFFU;
      state->tmp_state[addr] ^= xor_;

      state->top_ptr = (state->top_ptr - 1) & state->buf_size_mask;
   }

   if (state->top_ptr == state->bottom_ptr) // Our stack is completely empty... :v
   {
      state->top_ptr = (state->top_ptr + 1) & state->buf_size_mask;
      return true;
   }

   return true;
}

static void reassign_bottom(state_manager_t *state)
{
   state->bottom_ptr = (state->top_ptr + 1) & state->buf_size_mask;
   while (state->buffer[state->bottom_ptr]) // Skip ahead until we find the first 0 (boundary for state delta).
      state->bottom_ptr = (state->bottom_ptr + 1) & state->buf_size_mask;
}

#if __SSE2__

#if defined(__GNUC__)
static inline unsigned compat_ctz(uint32_t v)
{
   return __builtin_ctz(v);
}
#else
// Only checks at nibble granularity, because that's what we need.
static inline unsigned compat_ctz(uint32_t v)
{
   if (v & 0x000f)
      return 0;
   if (v & 0x00f0)
      return 4;
   if (v & 0x0f00)
      return 8;
   if (v & 0xf000)
      return 12;
   return 16;
}
#endif

#include <emmintrin.h>
// There's no equivalent in libc, you'd think so ... std::mismatch exists, but it's not optimized at all. :(
static unsigned find_mismatch(const uint32_t *a, const uint32_t *b, unsigned samples)
{
   unsigned i;
   unsigned sse_samples = samples & ~3;
   for (i = 0; i < sse_samples; i += 4)
   {
      __m128i v0 = _mm_loadu_si128((const __m128i*)(a + i));
      __m128i v1 = _mm_loadu_si128((const __m128i*)(b + i));
      __m128i c = _mm_cmpeq_epi32(v0, v1);
      uint32_t mask = _mm_movemask_epi8(c);
      if (mask != 0xffff) // Something has changed, figure out where.
         return i + (compat_ctz(~mask) >> 2);
   }

   for (; i < samples; i++)
      if (a[i] != b[i])
         return i;

   return samples;
}
#else
static unsigned find_mismatch(const uint32_t *a, const uint32_t *b, unsigned samples)
{
   unsigned i;
   for (i = 0; i < samples; i++)
      if (a[i] != b[i])
         return i;
   return samples;
}
#endif

static void generate_delta(state_manager_t *state, const void *data)
{
   size_t i;
   bool crossed = false;
   uint32_t *old_state = state->tmp_state;
   const uint32_t *new_state = (const uint32_t*)data;

   state->buffer[state->top_ptr++] = 0; // For each separate delta, we have a 0 value sentinel in between.
   state->top_ptr &= state->buf_size_mask;

   // Check if top_ptr and bottom_ptr crossed each other, which means we need to delete old cruft.
   if (state->top_ptr == state->bottom_ptr)
      crossed = true;

   for (i = 0; i < state->state_size; i++)
   {
      unsigned avail = state->state_size - i;
      unsigned pos = find_mismatch(old_state + i, new_state + i, avail);
      if (pos == avail)
         break;

      i += pos;

      // If the data differs (xor != 0), we push that xor on the stack with index and xor.
      // This can be reversed by reapplying the xor.
      // This, if states don't really differ much, we'll save lots of space :)
      // Hopefully this will work really well with save states.
      uint32_t xor_ = old_state[i] ^ new_state[i];
      old_state[i] = new_state[i];

      state->buffer[state->top_ptr] = ((uint64_t)i << 32) | xor_;
      state->top_ptr = (state->top_ptr + 1) & state->buf_size_mask;

      if (state->top_ptr == state->bottom_ptr)
         crossed = true;
   }

   if (crossed)
      reassign_bottom(state);
}

bool state_manager_push(state_manager_t *state, const void *data)
{
   RARCH_PERFORMANCE_INIT(gen_delta);
   RARCH_PERFORMANCE_START(gen_delta);
   generate_delta(state, data);
   RARCH_PERFORMANCE_STOP(gen_delta);

   state->first_pop = true;

   return true;
}

