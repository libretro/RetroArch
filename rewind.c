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

#define __STDC_LIMIT_MACROS
#include "rewind.h"
#include <stdlib.h>
#include "msvc/msvc-stdint/stdint.h"

//#define NO_UNALIGNED_MEM
//Uncomment the above if alignment is enforced.

//Format per frame:
//size nextstart;
//repeat {
//  uint16 numchanged; // everything is counted in units of uint16
//  if (numchanged) {
//    uint16 numunchanged; // skip these before handling numchanged
//    uint16[numchanged] changeddata;
//  }
//  else
//  {
//    uint32 numunchanged;
//    if (!numunchanged) break;
//  }
//}
//size thisstart;
//
//The start offsets point to 'nextstart' of any given compressed frame.
//Each uint16 is stored native endian; anything that claims any other endianness refers to the endianness of this specific item.
//The uint32 is stored little endian.
//Each size value is stored native endian if alignment is not enforced; if it is, they're little endian.
//The start of the buffer contains a size pointing to the end of the buffer; the end points to its start.
//Wrapping is handled by returning to the start of the buffer if the compressed data could potentially hit the edge;
// if the compressed data could potentially overwrite the tail pointer, the tail retreats until it can no longer collide.
//This means that on average, ~2*maxcompsize is unused at any given moment.

#if SIZE_MAX == 0xFFFFFFFF
extern char double_check_sizeof_size_t[(sizeof(size_t)==4)?1:-1];
#elif SIZE_MAX == 0xFFFFFFFFFFFFFFFF
extern char double_check_sizeof_size_t[(sizeof(size_t)==8)?1:-1];
#define USE_64BIT
#else
#error This item is only tested on 32bit and 64bit.
#endif

#ifdef NO_UNALIGNED_MEM
//These functions assume 16bit alignment.
//They do not make any attempt at matching system native endian; values written by these can only be read by the matching partner.
#ifdef USE_64BIT
static inline void write_size_t(uint16_t* ptr, size_t val)
{
   ptr[0] = val>>0;
   ptr[1] = val>>16;
   ptr[2] = val>>32;
   ptr[3] = val>>48;
}

static inline size_t read_size_t(uint16_t* ptr)
{
   return ((size_t)ptr[0]<<0  |
           (size_t)ptr[1]<<16 |
           (size_t)ptr[2]<<32 |
           (size_t)ptr[3]<<48);
}
#else
static inline void write_size_t(uint16_t* ptr, size_t val)
{
   ptr[0] = val;
   ptr[1] = val>>16;
}

static inline size_t read_size_t(uint16_t* ptr)
{
   return (ptr[0] | (size_t)ptr[1]<<16);
}
#endif

#else
#define read_size_t(ptr) (*(size_t*)(ptr))
#define write_size_t(ptr, val) (*(size_t*)(ptr) = (val))
#endif

struct state_manager {
   char *data;
   size_t capacity;
   char *head;//Reading and writing is done here.
   char *tail;//If head comes close to this, discard a frame.

   char *thisblock;
   char *nextblock;
   bool thisblock_valid;

   size_t blocksize;//This one is runded up from reset::blocksize.
   size_t maxcompsize;//size_t+(blocksize+131071)/131072*(blocksize+u16+u16)+u16+u32+size_t (yes, the math is a bit ugly)

   unsigned int entries;
};

state_manager_t *state_manager_new(size_t state_size, size_t buffer_size)
{
   state_manager_t *state = (state_manager_t*)malloc(sizeof(*state));

   state->capacity = 0;
   state->blocksize = 0;

   int newblocksize = ((state_size-1)|(sizeof(uint16_t)-1))+1;
   state->blocksize = newblocksize;

   const int maxcblkcover = UINT16_MAX*sizeof(uint16_t);
   const int maxcblks = (state->blocksize+maxcblkcover-1)/maxcblkcover;
   state->maxcompsize = state->blocksize + maxcblks*sizeof(uint16_t)*2 + sizeof(uint16_t)+sizeof(uint32_t) + sizeof(size_t)*2;

   state->data = (char*)malloc(buffer_size);

   state->thisblock = (char*)calloc(state->blocksize+sizeof(uint16_t)*8, 1);
   state->nextblock = (char*)calloc(state->blocksize+sizeof(uint16_t)*8, 1);
   if (!state->data || !state->thisblock || !state->nextblock)
   {
      free(state->data);
      free(state->thisblock);
      free(state->nextblock);
      free(state);
      return NULL;
   }
   //Force in a different byte at the end, so we don't need to check bounds in the innermost loop (it's expensive).
   //There is also a large amount of data that's the same, to stop the other scan
   //There is also some padding at the end. This is so we don't read outside the buffer end if we're reading in large blocks;
   // it doesn't make any difference to us, but sacrificing 16 bytes to get Valgrind happy is worth it.
   *(uint16_t*)(state->thisblock+state->blocksize+sizeof(uint16_t)*3) = 0xFFFF;
   *(uint16_t*)(state->nextblock+state->blocksize+sizeof(uint16_t)*3) = 0x0000;

   state->capacity=buffer_size;

   state->head = state->data+sizeof(size_t);
   state->tail = state->data+sizeof(size_t);

   state->thisblock_valid = false;

   state->entries = 0;

   return state;
}

void state_manager_free(state_manager_t *state)
{
   free(state->data);
   free(state->thisblock);
   free(state->nextblock);
   free(state);
}

bool state_manager_pop(state_manager_t *state, const void **data)
{
   *data = NULL;

   if (state->thisblock_valid)
   {
      state->thisblock_valid = false;
      state->entries--;
      *data = state->thisblock;
      return true;
   }

   if (state->head == state->tail) return false;

   size_t start = read_size_t((uint16_t*)(state->head - sizeof(size_t)));
   state->head = state->data+start;

   const char *compressed = state->data+start+sizeof(size_t);
   char *out = state->thisblock;
   //Begin decompression code
   //out is the last pushed (or returned) state
   const uint16_t *compressed16 = (const uint16_t*)compressed;
   uint16_t *out16 = (uint16_t*)out;
   while (true)
   {
      uint16_t numchanged = *(compressed16++);
      if (numchanged)
      {
         out16 += *(compressed16++);
         //We could do memcpy, but it seems that memcpy has a constant-per-call overhead that actually shows up.
         //Our average size in here seems to be 8 or something.
         //Therefore, we do something with lower overhead.
         for (int i=0;i<numchanged;i++) out16[i] = compressed16[i];
         compressed16 += numchanged;
         out16 += numchanged;
      }
      else
      {
         uint32_t numunchanged = compressed16[0] | compressed16[1]<<16;
         if (!numunchanged) break;
         compressed16 += 2;
         out16 += numunchanged;
      }
   }
   //End decompression code

   state->entries--;

   *data = state->thisblock;
   return true;
}

void state_manager_push_where(state_manager_t *state, void **data)
{
   //We need to ensure we have an uncompressed copy of the last pushed state, or we could
   // end up applying a 'patch' to wrong savestate, and that'd blow up rather quickly.
   if (!state->thisblock_valid) 
   {
      const void *ignored;
      if (state_manager_pop(state, &ignored))
      {
         state->thisblock_valid = true;
         state->entries++;
      }
   }
   
   *data=state->nextblock;
}

#if __SSE2__
#if defined(__GNUC__)
static inline int compat_ctz(unsigned int x)
{
   return __builtin_ctz(v);
}
#else
// Only checks at nibble granularity, because that's what we need.
static inline int compat_ctz(unsigned int x)
{
   if (x & 0x000f)
      return 0;
   if (x & 0x00f0)
      return 4;
   if (x & 0x0f00)
      return 8;
   if (x & 0xf000)
      return 12;
   return 16;
}
#endif

#include <emmintrin.h>
// There's no equivalent in libc, you'd think so ... std::mismatch exists, but it's not optimized at all. :(
static inline size_t find_change(const uint16_t * a, const uint16_t * b)
{
	const __m128i * a128=(const __m128i*)a;
	const __m128i * b128=(const __m128i*)b;
	
	while (true)
	{
		__m128i v0 = _mm_loadu_si128(a128);
		__m128i v1 = _mm_loadu_si128(b128);
		__m128i c = _mm_cmpeq_epi32(v0, v1);
		uint32_t mask = _mm_movemask_epi8(c);
		if (mask != 0xffff) // Something has changed, figure out where.
		{
			size_t ret=(((char*)a128-(char*)a) | (compat_ctz(~mask))) >> 1;
			return (ret | (a[ret]==b[ret]));
		}
		a128++;
		b128++;
	}
}
#else
static inline size_t find_change(const uint16_t * a, const uint16_t * b)
{
	const uint16_t * a_org=a;
#ifdef NO_UNALIGNED_MEM
	while ((uintptr_t)a & (sizeof(size_t)-1) && *a==*b)
	{
		a++;
		b++;
	}
	if (*a==*b)
#endif
	{
		const size_t* a_big=(const size_t*)a;
		const size_t* b_big=(const size_t*)b;
		
		while (*a_big==*b_big)
		{
			a_big++;
			b_big++;
		}
		a=(const uint16_t*)a_big;
		b=(const uint16_t*)b_big;
		
		while (*b==*a)
		{
			a++;
			b++;
		}
	}
	return a-a_org;
}
#endif

#if __SSE2__x // this is not a typo - do not fix unless you can show evidence that this version is faster
//This one can give different answers than the C version in some cases. However, the compression ratio remains unaffected.
//It also appears to be slower. Probably due to the low average duration of this loop.
static inline size_t find_same(const uint16_t * a, const uint16_t * b)
{
	if (a[0]==b[0] && a[1]==b[1]) return 0;
	if (a[1]==b[1] && a[2]==b[2]) return 1;
	if (a[2]==b[2] && a[3]==b[3]) return 2;
	if (a[3]==b[3] && a[4]==b[4]) return 3;
	
	const __m128i * a128=((const __m128i*)a);
	const __m128i * b128=((const __m128i*)b);
	
	while (true)
	{
		__m128i v0 = _mm_loadu_si128(a128);
		__m128i v1 = _mm_loadu_si128(b128);
		__m128i c = _mm_cmpeq_epi32(v0, v1);
		uint32_t mask = _mm_movemask_epi8(c);
		if (mask != 0x0000) // Something remains unchanged, figure out where.
		{
			size_t ret=(((char*)a128-(char*)a) | (__builtin_ctz(mask))) >> 1;
			return (ret - (a[ret-1]==b[ret-1]));
		}
		a128++;
		b128++;
	}
}
#else
static inline size_t find_same(const uint16_t * a, const uint16_t * b)
{
	const uint16_t * a_org=a;
	
	//Comparing two or three words makes no real difference.
	//With two, the smaller blocks are less likely to be chopped up elsewhere due to 64KB;
	// with three, we get larger blocks which should be a minuscle bit faster to decompress,
	// but probably a little slower to compress. Since compression is more bottleneck than decompression is, we favor that.
	while (a[0]!=b[0] || a[1]!=b[1])
	{
		a++;
		b++;
		//Optimize this by only checking one at the time for as long as possible.
		while (*a!=*b)
		{
			a++;
			b++;
		}
	}
	
	return a-a_org;
}
#endif

void state_manager_push_do(state_manager_t *state)
{
   if (state->thisblock_valid)
   {
      if (state->capacity<sizeof(size_t)+state->maxcompsize) return;

   recheckcapacity:;
      size_t headpos = (state->head-state->data);
      size_t tailpos = (state->tail-state->data);
      size_t remaining = (tailpos+state->capacity-sizeof(size_t)-headpos-1)%state->capacity + 1;
      if (remaining <= state->maxcompsize)
      {
         state->tail = state->data + read_size_t((uint16_t*)state->tail);
         state->entries--;
         goto recheckcapacity;
      }

      const char *oldb = state->thisblock;
      const char *newb = state->nextblock;
      char *compressed = state->head+sizeof(size_t);

      //Begin compression code; 'compressed' will point to the end of the compressed data (excluding the prev pointer).
      const uint16_t *old16 = (const uint16_t*)oldb;
      const uint16_t *new16 = (const uint16_t*)newb;
      uint16_t *compressed16 = (uint16_t*)compressed;
      size_t num16s = state->blocksize/sizeof(uint16_t);
      while (num16s)
      {
         size_t skip = find_change(old16, new16);

         if (skip >= num16s) break;
         old16+=skip;
         new16+=skip;
         num16s-=skip;

         if (skip > UINT16_MAX)
         {
            if (skip > UINT32_MAX)
            {
               //This will make it scan the entire thing again, but it only hits on 8GB unchanged
               //data anyways, and if you're doing that, you've got bigger problems.
               skip = UINT32_MAX;
            }
            *(compressed16++) = 0;
            *(compressed16++) = skip;
            *(compressed16++) = skip>>16;
            skip = 0;
            continue;
         }

         size_t changed=find_same(old16, new16);
         if (changed>UINT16_MAX) changed=UINT16_MAX;
         *(compressed16++)=changed;
         *(compressed16++)=skip;
         for (int i=0;i<changed;i++) compressed16[i]=old16[i];
         old16+=changed;
         new16+=changed;
         compressed16+=changed;
         num16s-=changed;
      }
      compressed16[0] = 0;
      compressed16[1] = 0;
      compressed16[2] = 0;
      compressed = (char*)(compressed16+3);
      //End compression code.

      if (compressed - state->data + state->maxcompsize > state->capacity)
      {
         compressed = state->data;
         if (state->tail == state->data+sizeof(size_t)) state->tail = state->data + *(size_t*)state->tail;
      }
      write_size_t((uint16_t*)compressed, state->head-state->data);
      compressed += sizeof(size_t);
      write_size_t((uint16_t*)state->head, compressed-state->data);
      state->head = compressed;
   }
   else
   {
      state->thisblock_valid = true;
   }

   char *swap = state->thisblock;
   state->thisblock = state->nextblock;
   state->nextblock = swap;

   state->entries++;

   return;
}

void state_manager_capacity(state_manager_t *state, unsigned int * entries, size_t * bytes, bool * full)
{
   size_t headpos = (state->head-state->data);
   size_t tailpos = (state->tail-state->data);
   size_t remaining = (tailpos+state->capacity-sizeof(size_t)-headpos-1)%state->capacity + 1;

   if (entries) *entries = state->entries;
   if (bytes) *bytes = (state->capacity-remaining);
   if (full) *full = (remaining<=state->maxcompsize*2);
}
