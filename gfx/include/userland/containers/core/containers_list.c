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
#include <stdio.h>
#include <string.h>

#include "containers/core/containers_common.h"
#include "containers/core/containers_list.h"

/******************************************************************************
Defines and constants.
******************************************************************************/

/******************************************************************************
Type definitions
******************************************************************************/

/******************************************************************************
Function prototypes
******************************************************************************/

/******************************************************************************
Local Functions
******************************************************************************/

/** Find an entry in the list, or the insertion point.
 * Uses binary sub-division to find the search item. If index is not NULL, the
 * index of the matching entry, or the point at which to insert if not found, is
 * written to that address.
 *
 * \param list The list to be searched.
 * \param entry The entry for which to search.
 * \param index Set to index of match, or insertion point if not found. May be NULL.
 * \return True if a match was found, false if not. */
static bool vc_containers_list_find_index(const VC_CONTAINERS_LIST_T *list,
      const void *entry,
      uint32_t *index)
{
   const char *entries = (const char *)list->entries;
   size_t entry_size = list->entry_size;
   VC_CONTAINERS_LIST_COMPARATOR_T comparator = list->comparator;
   uint32_t start = 0, end = list->size;
   uint32_t mid = end >> 1;
   bool match = false;

   while (mid < end)
   {
      int comparison = comparator(entry, entries + mid * entry_size);

      if (comparison < 0)
         end = mid;
      else if (comparison > 0)
         start = mid + 1;
      else {
         match = true;
         break;
      }

      mid = (start + end) >> 1;
   }

   if (index) *index = mid;
   return match;
}

/******************************************************************************
Functions exported as part of the API
******************************************************************************/

/*****************************************************************************/
VC_CONTAINERS_LIST_T *vc_containers_list_create(uint32_t capacity,
      size_t entry_size,
      VC_CONTAINERS_LIST_COMPARATOR_T comparator)
{
   VC_CONTAINERS_LIST_T *list;

   list = (VC_CONTAINERS_LIST_T *)malloc(sizeof(VC_CONTAINERS_LIST_T));
   if (!list)
      return NULL;

   /* Ensure non-zero capacity, as that signifies a read-only list */
   if (!capacity) capacity = 1;

   list->entries = malloc(capacity * entry_size);
   if (!list->entries)
   {
      free(list);
      return NULL;
   }

   list->size = 0;
   list->capacity = capacity;
   list->entry_size = entry_size;
   list->comparator = comparator;

   return list;
}

/*****************************************************************************/
void vc_containers_list_destroy(VC_CONTAINERS_LIST_T *list)
{
   /* Avoid trying to destroy read-only lists */
   if (list && list->capacity)
   {
      if (list->entries)
         free(list->entries);
      free(list);
   }
}

/*****************************************************************************/
void vc_containers_list_reset(VC_CONTAINERS_LIST_T *list)
{
   /* Avoid trying to reset read-only lists */
   if (list && list->capacity)
      list->size = 0;
}

/*****************************************************************************/
bool vc_containers_list_insert(VC_CONTAINERS_LIST_T *list,
      void *new_entry,
      bool allow_duplicates)
{
   uint32_t insert_idx;
   char *insert_ptr;
   size_t entry_size;
   bool match;

   if (!list || !list->capacity) return false;

   entry_size = list->entry_size;
   match = vc_containers_list_find_index(list, new_entry, &insert_idx);
   insert_ptr = (char *)list->entries + entry_size * insert_idx;

   if (!match || allow_duplicates)
   {
      /* Ensure there is space for the new entry */
      if (list->size == list->capacity)
      {
         void *new_entries = realloc(list->entries, (list->size + 1) * entry_size);

         if (!new_entries)
            return false;
         list->entries = new_entries;
         list->capacity++;
      }

      /* Move up anything above the insertion point */
      if (insert_idx < list->size)
         memmove(insert_ptr + entry_size, insert_ptr, (list->size - insert_idx) * entry_size);

      list->size++;
   }

   /* Copy in the new entry (overwriting the old one if necessary) */
   memcpy(insert_ptr, new_entry, list->entry_size);

   return true;
}

/*****************************************************************************/
bool vc_containers_list_find_entry(const VC_CONTAINERS_LIST_T *list,
      void *entry)
{
   uint32_t index;
   size_t entry_size;

   if (!vc_containers_list_find_index(list, entry, &index))
      return false;

   entry_size = list->entry_size;
   memcpy(entry, (const char *)list->entries + entry_size * index, entry_size);

   return true;
}

/*****************************************************************************/
void vc_containers_list_validate(const VC_CONTAINERS_LIST_T *list)
{
   uint32_t ii, entry_size;
   const uint8_t *entry_ptr;

   vc_container_assert(list);
   vc_container_assert(!list->capacity || list->size <= list->capacity);
   vc_container_assert(list->entry_size);
   vc_container_assert(list->comparator);
   vc_container_assert(list->entries);

   /* Check all entries are in sorted order */
   entry_ptr = (const uint8_t *)list->entries;
   entry_size = list->entry_size;
   for (ii = 1; ii < list->size; ii++)
   {
      vc_container_assert(list->comparator(entry_ptr, entry_ptr + entry_size) <= 0);
      entry_ptr += entry_size;
   }
}
