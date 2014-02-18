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
#include <string.h>
#include "msvc/msvc-stdint/stdint.h"

//#define NO_UNALIGNED_MEM
//Uncomment the above if alignment is enforced.

//format per frame:
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
//the start offsets point to 'nextstart' of any given compressed frame
//multibyte values are stored native endian if alignment is not enforced; if it is, little endian
//the start of the buffer contains a size pointing to the end of the buffer; the end points to its start
//wrapping is handled by returning to the start of the buffer if the compressed data could potentially hit the edge
//if the compressed data could potentially overwrite the tail pointer, the tail retreats until it can no longer collide
//so on average, ~2*maxcompsize is unused at any given moment
//
//if unaligned memory access is illegal, define NO_UNALIGNED_MEM

#if SIZE_MAX == 0xFFFFFFFF
extern char double_check_sizeof_size_t[(sizeof(size_t)==4)?1:-1];
#elif SIZE_MAX == 0xFFFFFFFFFFFFFFFF
extern char double_check_sizeof_size_t[(sizeof(size_t)==8)?1:-1];
#define USE_64BIT
#else
#error your compiler is insane.
#endif

#ifdef NO_UNALIGNED_MEM
//These functions assume 16bit alignment.
//They do not make any attempt at matching system native endian; values written by these can only be read by the matching partner.
#ifdef USE_64BIT
static inline void write_size_t(uint16_t* ptr, size_t val)
{
   ptr[0]=val>>0;
   ptr[1]=val>>16;
   ptr[2]=val>>32;
   ptr[3]=val>>48;
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
   ptr[0]=val;
   ptr[1]=val>>16;
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
   char * data;
   size_t capacity;
   char * head;//read and write here
   char * tail;//delete here if head is close

   char * thisblock;
   char * nextblock;
   bool thisblock_valid;

   size_t blocksize;//rounded up from reset::blocksize
   size_t maxcompsize;//size_t+(blocksize+131071)/131072*(blocksize+u16+u16)+u16+u32+size_t

   unsigned int entries;
};

state_manager_t *state_manager_new(size_t state_size, size_t buffer_size)
{
   state_manager_t * state=malloc(sizeof(*state));

   state->capacity=0;
   state->blocksize=0;

   int newblocksize=((state_size-1)|(sizeof(uint16_t)-1))+1;
   state->blocksize=newblocksize;

   const int maxcblkcover=UINT16_MAX*sizeof(uint16_t);
   const int maxcblks=(state->blocksize+maxcblkcover-1)/maxcblkcover;
   state->maxcompsize=state->blocksize + maxcblks*sizeof(uint16_t)*2 + sizeof(uint16_t)+sizeof(uint32_t) + sizeof(size_t)*2;

   state->data=malloc(buffer_size);

   state->thisblock=calloc(state->blocksize+sizeof(uint16_t)*8, 1);
   state->nextblock=calloc(state->blocksize+sizeof(uint16_t)*8, 1);
   if (!state->data || !state->thisblock || !state->nextblock)
   {
      free(state->data);
      free(state->thisblock);
      free(state->nextblock);
      free(state);
      return NULL;
   }
   //force in a different byte at the end, so we don't need to look for the buffer end in the innermost loop
   //there is also a large amount of data that's the same, to stop the other scan
   //and finally some padding so we don't read outside the buffer end if we're reading in large blocks
   *(uint16_t*)(state->thisblock+state->blocksize+sizeof(uint16_t)*3)=0xFFFF;
   *(uint16_t*)(state->nextblock+state->blocksize+sizeof(uint16_t)*3)=0x0000;

   state->capacity=buffer_size;

   state->head=state->data+sizeof(size_t);
   state->tail=state->data+sizeof(size_t);

   state->thisblock_valid=false;

   state->entries=0;

   return state;
}

void state_manager_free(state_manager_t *state)
{
   free(state->data);
   free(state->thisblock);
   free(state->nextblock);
   free(state);
}

bool state_manager_pop(state_manager_t *state, void **data)
{
   *data=NULL;

   if (state->thisblock_valid)
   {
      state->thisblock_valid=false;
      state->entries--;
      *data=state->thisblock;
      return true;
   }

   if (state->head==state->tail) return false;

   size_t start=read_size_t((uint16_t*)(state->head - sizeof(size_t)));
   state->head=state->data+start;

   const char * compressed=state->data+start+sizeof(size_t);
   char * out=state->thisblock;
   //begin decompression code
   //out is the previously returned state
   const uint16_t * compressed16=(const uint16_t*)compressed;
   uint16_t * out16=(uint16_t*)out;
   while (true)
   {
      uint16_t numchanged=*(compressed16++);
      if (numchanged)
      {
         out16+=*(compressed16++);
         //we could do memcpy, but it seems that function call overhead is high
         // enough that memcpy's higher speed for large blocks won't matter
         for (int i=0;i<numchanged;i++) out16[i]=compressed16[i];
         compressed16+=numchanged;
         out16+=numchanged;
      }
      else
      {
         uint32_t numunchanged=compressed16[0] | compressed16[1]<<16;
         if (!numunchanged) break;
         compressed16+=2;
         out16+=numunchanged;
      }
   }
   //end decompression code

   state->entries--;

   *data=state->thisblock;
   return true;
}

void *state_manager_push_where(state_manager_t *state)
{
   return state->nextblock;
}

bool state_manager_push_do(state_manager_t *state)
{
   if (state->thisblock_valid)
   {
      if (state->capacity<sizeof(size_t)+state->maxcompsize) return false;

   recheckcapacity:;
      size_t headpos=(state->head-state->data);
      size_t tailpos=(state->tail-state->data);
      size_t remaining=(tailpos+state->capacity-sizeof(size_t)-headpos-1)%state->capacity + 1;
      if (remaining<=state->maxcompsize)
      {
         state->tail=state->data + read_size_t((uint16_t*)state->tail);
         state->entries--;
         goto recheckcapacity;
      }

      const char* old=state->thisblock;
      const char* new=state->nextblock;
      char* compressed=state->head+sizeof(size_t);

      //at the end, 'compressed' must point to the end of the compressed data
      //do not include the next/prev pointers
      //begin compression code
      const uint16_t * old16=(const uint16_t*)old;
      const uint16_t * new16=(const uint16_t*)new;
      uint16_t * compressed16=(uint16_t*)compressed;
      size_t num16s=state->blocksize/sizeof(uint16_t);
      while (num16s)
      {
         const uint16_t * oldprev=old16;
#ifdef NO_UNALIGNED_MEM
         while ((uintptr_t)old16 & (sizeof(size_t)-1) && *old16==*new16)
         {
            old16++;
            new16++;
         }
         if (*old16==*new16)
#endif
         {
            const size_t* olds=(const size_t*)old16;
            const size_t* news=(const size_t*)new16;

            while (*olds==*news)
            {
               olds++;
               news++;
            }
            old16=(const uint16_t*)olds;
            new16=(const uint16_t*)news;

            while (*old16==*new16)
            {
               old16++;
               new16++;
            }
         }
         size_t skip=(old16-oldprev);

         if (skip>=num16s) break;
         num16s-=skip;

         if (skip>UINT16_MAX)
         {
            if (skip>UINT32_MAX)
            {
               old16-=skip;
               new16-=skip;
               skip=UINT32_MAX;
               old16+=skip;
               new16+=skip;
            }
            *(compressed16++)=0;
            *(compressed16++)=skip;
            *(compressed16++)=skip>>16;
            compressed16+=2;
            skip=0;
            continue;
         }

         size_t changed;
         const uint16_t * old16prev=old16;
         //comparing two or three words makes no real difference
         //with two, the smaller blocks are less likely to be chopped up elsewhere due to 64KB
         //with three, we get larger blocks which should be a minuscle bit faster to decompress, but probably a little slower to compress
         while (old16[0]!=new16[0] || old16[1]!=new16[1])
         {
            old16++;
            new16++;
            while (*old16!=*new16)
            {
               old16++;
               new16++;
            }
         }
         changed=(old16-old16prev);
         if (!changed) continue;
         if (changed>UINT16_MAX)
         {
            old16-=changed;
            new16-=changed;
            changed=UINT16_MAX;
            old16+=changed;
            new16+=changed;
         }
         num16s-=changed;
         *(compressed16++)=changed;
         *(compressed16++)=skip;
         memcpy(compressed16, old16prev, changed*sizeof(uint16_t));
         compressed16+=changed;
      }
      compressed16[0]=0;
      compressed16[1]=0;
      compressed16[2]=0;
      compressed=(char*)(compressed16+3);
      //end compression code

      if (compressed-state->data+state->maxcompsize > state->capacity)
      {
         compressed=state->data;
         if (state->tail==state->data+sizeof(size_t)) state->tail=state->data + *(size_t*)state->tail;
      }
      write_size_t((uint16_t*)compressed, state->head-state->data);
      compressed+=sizeof(size_t);
      write_size_t((uint16_t*)state->head, compressed-state->data);
      state->head=compressed;
   }
   else
   {
      state->thisblock_valid=true;
   }

   char * swap=state->thisblock;
   state->thisblock=state->nextblock;
   state->nextblock=swap;

   state->entries++;

   return true;
}

void state_manager_capacity(state_manager_t *state, unsigned int * entries, size_t * bytes, bool * full)
{
   size_t headpos=(state->head-state->data);
   size_t tailpos=(state->tail-state->data);
   size_t remaining=(tailpos+state->capacity-sizeof(size_t)-headpos-1)%state->capacity + 1;

   if (entries) *entries=state->entries;
   if (bytes) *bytes=(state->capacity-remaining);
   if (full) *full=(remaining<=state->maxcompsize*2);
}
