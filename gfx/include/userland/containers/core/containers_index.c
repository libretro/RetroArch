/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <string.h>

#include "containers/containers.h"
#include "containers/core/containers_index.h"

typedef struct {
   int64_t file_offset;
   int64_t time;
} VC_CONTAINER_INDEX_POS_T;

struct  VC_CONTAINER_INDEX_T {
   int len;                           // log2 of length of entry array
   int next;                          // next array entry to write into
   int gap;                           // log2 of the passes through entry array to build the full list
   int mgap;                          // len-gap, stored for convenience
   int count;                         // number of calls to index_add since last entry added
   int max_count;                     // log2 of the number of calls to discard between each entry added
   int64_t max_time;                  // time of the latest entry
   VC_CONTAINER_INDEX_POS_T entry[0]; // array of position/time pairs
};

// We have a fixed length list, and when it is full we want to discard half the entries.
// This is done without coping data by mapping the entry number to the index to the array,
// in the following way:
// Length is a power of two, so the entry number is a fixed constant bit width.  The highest gap
// bits are used as a direct offset into the array (o), the lowest mgap bits are right shifted by
// gap to increment this (S).  Each time we double the number of passes through the actual array.
// So if len=3, we start off with mgap=3, gap=0, we have a single pass with the trivial mapping:
// |S|S|S|  [0 1 2 3 4 5 6 7]
// when this is full we change to mgap=2, gap=1, so we iterate this way:
// |o|S|S|  [0 2 4 6] [1 3 5 7]
// when this is full we change to mgap=1, gap=2
// |o|o|S|  [0 4] [1 5] [2 6] [3 7]
// when this is full we change to this, which is equivalent to where we started
// |o|o|o|  [0] [1] [2] [3] [4] [5] [6] [7]

#define ENTRY(x, i) ((x)->gap == 0 ? (i) : ((i)>>(x)->mgap) + (((i) & ((1<<(x)->mgap)-1)) << (x)->gap))

VC_CONTAINER_STATUS_T vc_container_index_create( VC_CONTAINER_INDEX_T **index, int length )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   VC_CONTAINER_INDEX_T *id = NULL;
   int len = 0;

   if(length < 16) length = 16;
   if(length > 4096) length = 4096;
   while((length >>= 1) != 0)
      len++;

   id = malloc(sizeof(VC_CONTAINER_INDEX_T) + (sizeof(VC_CONTAINER_INDEX_POS_T)<<len));
   if(id == NULL) { goto error; }

   memset(id, 0, sizeof(VC_CONTAINER_INDEX_T));

   id->len = id->mgap = len;

   *index = id;
   return VC_CONTAINER_SUCCESS;

 error:
   return status;
}

VC_CONTAINER_STATUS_T vc_container_index_free( VC_CONTAINER_INDEX_T *index )
{
   if(index == NULL)
      return VC_CONTAINER_ERROR_FAILED;

   free(index);
   return VC_CONTAINER_SUCCESS;
}

VC_CONTAINER_STATUS_T vc_container_index_add( VC_CONTAINER_INDEX_T *index, int64_t time, int64_t file_offset )
{
   if(index == NULL)
      return VC_CONTAINER_ERROR_FAILED;

   // reject entries if they are in part of the time covered
   if(index->next != 0 && time <= index->max_time)
      return VC_CONTAINER_SUCCESS;

   index->count++;
   if(index->count == (1<<index->max_count))
   {
      int entry;
      if(index->next == (1<<index->len))
      {
         // New entry doesn't fit, we discard every other index record
         // by changing how we map index positions to array entry indexes.
         index->next >>= 1;
         index->gap++;
         index->mgap--;
         index->max_count++;

         if(index->gap == index->len)
         {
            index->gap = 0;
            index->mgap = index->len;
         }
      }

      entry = ENTRY(index, index->next);

      index->entry[entry].file_offset = file_offset;
      index->entry[entry].time = time;
      index->count = 0;
      index->next++;
      index->max_time = time;
   }

   return VC_CONTAINER_SUCCESS;
}

VC_CONTAINER_STATUS_T vc_container_index_get( VC_CONTAINER_INDEX_T *index, int later, int64_t *time, int64_t *file_offset, int *past )
{
   int guess, start, end, entry;

   if(index == NULL || index->next == 0)
      return VC_CONTAINER_ERROR_FAILED;

   guess = start = 0;
   end = index->next-1;

   *past = *time > index->max_time;

   while(end-start > 1)
   {
      int64_t gtime;
      guess = (start+end)>>1;
      gtime = index->entry[ENTRY(index, guess)].time;

      if(*time < gtime)
         end = guess;
      else if(*time > gtime)
         start = guess;
      else
         break;
   }

   if (*time != index->entry[ENTRY(index, guess)].time)
   {
      if(later)
      {
         if(*time <= index->entry[ENTRY(index, start)].time)
            guess = start;
         else
            guess = end;
      }
      else
      {
         if(*time >= index->entry[ENTRY(index, end)].time)
            guess = end;
         else
            guess = start;
      }
   }

   entry = ENTRY(index, guess);
   *time = index->entry[entry].time;
   *file_offset = index->entry[entry].file_offset;

   return VC_CONTAINER_SUCCESS;
}
