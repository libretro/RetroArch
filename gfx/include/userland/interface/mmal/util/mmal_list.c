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

#include "interface/vcos/vcos.h"
#include "interface/mmal/util/mmal_list.h"


/* Private list context */
typedef struct MMAL_LIST_PRIVATE_T
{
   MMAL_LIST_T list;    /* must be first */
   VCOS_MUTEX_T lock;
} MMAL_LIST_PRIVATE_T;


/* Lock the list. */
static inline void mmal_list_lock(MMAL_LIST_T *list)
{
   vcos_mutex_lock(&((MMAL_LIST_PRIVATE_T*)list)->lock);
}

/* Unlock the list. */
static inline void mmal_list_unlock(MMAL_LIST_T *list)
{
   vcos_mutex_unlock(&((MMAL_LIST_PRIVATE_T*)list)->lock);
}

/* Create a new linked list. */
MMAL_LIST_T* mmal_list_create(void)
{
   MMAL_LIST_PRIVATE_T *private;

   private = vcos_malloc(sizeof(MMAL_LIST_PRIVATE_T), "mmal-list");
   if (private == NULL)
      goto error;

   if (vcos_mutex_create(&private->lock, "mmal-list lock") != VCOS_SUCCESS)
      goto error;

   /* lock to keep coverity happy */
   vcos_mutex_lock(&private->lock);
   private->list.first = NULL;
   private->list.last = NULL;
   private->list.length = 0;
   vcos_mutex_unlock(&private->lock);

   return &private->list;

error:
   vcos_free(private);
   return NULL;
}

/* Destroy a linked list. */
void mmal_list_destroy(MMAL_LIST_T *list)
{
   MMAL_LIST_PRIVATE_T *private = (MMAL_LIST_PRIVATE_T*)list;

   vcos_mutex_delete(&private->lock);
   vcos_free(private);
}

/* Remove the last element in the list. */
MMAL_LIST_ELEMENT_T* mmal_list_pop_back(MMAL_LIST_T *list)
{
   MMAL_LIST_ELEMENT_T *element;

   mmal_list_lock(list);

   element = list->last;
   if (element != NULL)
   {
      list->length--;

      list->last = element->prev;
      if (list->last)
         list->last->next = NULL;
      else
         list->first = NULL; /* list is now empty */

      element->prev = NULL;
      element->next = NULL;
   }

   mmal_list_unlock(list);

   return element;
}

/* Remove the first element in the list. */
MMAL_LIST_ELEMENT_T* mmal_list_pop_front(MMAL_LIST_T *list)
{
   MMAL_LIST_ELEMENT_T *element;

   mmal_list_lock(list);

   element = list->first;
   if (element != NULL)
   {
      list->length--;

      list->first = element->next;
      if (list->first)
         list->first->prev = NULL;
      else
         list->last = NULL; /* list is now empty */

      element->prev = NULL;
      element->next = NULL;
   }

   mmal_list_unlock(list);

   return element;
}

/* Add an element to the front of the list. */
void mmal_list_push_front(MMAL_LIST_T *list, MMAL_LIST_ELEMENT_T *element)
{
   mmal_list_lock(list);

   list->length++;

   element->prev = NULL;
   element->next = list->first;

   if (list->first)
      list->first->prev = element;
   else
      list->last = element; /* list was empty */

   list->first = element;

   mmal_list_unlock(list);
}

/* Add an element to the back of the list. */
void mmal_list_push_back(MMAL_LIST_T *list, MMAL_LIST_ELEMENT_T *element)
{
   mmal_list_lock(list);

   list->length++;

   element->next = NULL;
   element->prev = list->last;

   if (list->last)
      list->last->next = element;
   else
      list->first = element; /* list was empty */

   list->last = element;

   mmal_list_unlock(list);
}

/* Insert an element into the list. */
void mmal_list_insert(MMAL_LIST_T *list, MMAL_LIST_ELEMENT_T *element, MMAL_LIST_COMPARE_T compare)
{
   MMAL_LIST_ELEMENT_T *cur;

   mmal_list_lock(list);

   if (list->first == NULL)
   {
      /* List empty */
      mmal_list_unlock(list);
      mmal_list_push_front(list, element);
      return;
   }

   cur = list->first;
   while (cur)
   {
      if (compare(element, cur))
      {
         /* Slot found! */
         list->length++;
         if (cur == list->first)
            list->first = element;
         else
            cur->prev->next = element;
         element->prev = cur->prev;
         element->next = cur;
         cur->prev = element;
         mmal_list_unlock(list);
         return;
      }

      cur = cur->next;
   }

   /* If we get here, none of the existing elements are greater
    * than the new on, so just add it to the back of the list */
   mmal_list_unlock(list);
   mmal_list_push_back(list, element);
}
