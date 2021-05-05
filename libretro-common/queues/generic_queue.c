/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (generic_queue.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <boolean.h>
#include <stddef.h>
#include <stdlib.h>

#include <queues/generic_queue.h>

struct generic_queue_item_t
{
   void *value;
   struct generic_queue_item_t *previous;
   struct generic_queue_item_t *next;
};

struct generic_queue
{
   struct generic_queue_item_t *first_item;
   struct generic_queue_item_t *last_item;
   size_t length;
};

struct generic_queue_iterator
{
   generic_queue_t *queue;
   struct generic_queue_item_t *item;
   bool forward;
};

generic_queue_t *generic_queue_new(void)
{
   return (generic_queue_t *)calloc(1, sizeof(generic_queue_t));
}

void generic_queue_free(generic_queue_t *queue, void (*free_value)(void *value))
{
   struct generic_queue_item_t *next_item;

   if (!queue)
      return;

   while (queue->first_item)
   {
      if (free_value)
         free_value(queue->first_item->value);

      next_item = queue->first_item->next;
      free(queue->first_item);
      queue->first_item = next_item;
   }

   free(queue);
}

void generic_queue_push(generic_queue_t *queue, void *value)
{
   struct generic_queue_item_t *new_item;

   new_item = (struct generic_queue_item_t *)calloc(1, sizeof(struct generic_queue_item_t));
   new_item->value = value;
   new_item->previous = queue->last_item;
   new_item->next = NULL;

   queue->last_item = new_item;
   queue->length++;

   if (!queue->first_item)
      queue->first_item = new_item;
   else
      new_item->previous->next = new_item;
}

void *generic_queue_pop(generic_queue_t *queue)
{
   void *value;
   struct generic_queue_item_t *item;

   if (!queue || !queue->last_item)
      return NULL;

   item = queue->last_item;
   queue->last_item = queue->last_item->previous;
   queue->length--;

   if (queue->length == 0)
      queue->first_item = NULL;

   value = item->value;
   free(item);

   return value;
}

void *generic_queue_peek(generic_queue_t *queue)
{
   if (!queue || !queue->last_item)
      return NULL;

   return queue->last_item->value;
}

void *generic_queue_peek_first(generic_queue_t *queue)
{
   if (!queue || !queue->first_item)
      return NULL;

   return queue->first_item->value;
}

void generic_queue_shift(generic_queue_t *queue, void *value)
{
   struct generic_queue_item_t *new_item;

   if (!queue)
      return;

   new_item = (struct generic_queue_item_t *)calloc(1, sizeof(struct generic_queue_item_t));
   new_item->value = value;
   new_item->previous = NULL;
   new_item->next = queue->first_item;

   queue->first_item = new_item;
   queue->length++;

   if (!queue->last_item)
      queue->last_item = new_item;
   else
      new_item->next->previous = new_item;
}

void *generic_queue_unshift(generic_queue_t *queue)
{
   void *value;
   struct generic_queue_item_t *item;

   if (!queue || !queue->first_item)
      return NULL;

   item = queue->first_item;
   queue->first_item = queue->first_item->next;
   queue->length--;

   if (queue->length == 0)
      queue->last_item = NULL;

   value = item->value;
   free(item);

   return value;
}

size_t generic_queue_length(generic_queue_t *queue)
{
   if (queue)
      return queue->length;

   return 0;
}

void *generic_queue_remove(generic_queue_t *queue, void *value)
{
   struct generic_queue_item_t *item;

   if (!queue)
      return NULL;

   for (item = queue->first_item; item; item = item->next)
   {
      if (item->value == value)
      {
         if (item->previous)
            item->previous->next = item->next;
         else
            queue->first_item = item->next;

         if (item->next)
            item->next->previous = item->previous;
         else
            queue->last_item = item->previous;

         free(item);
         queue->length--;

         return value;
      }
   }

   return NULL;
}

generic_queue_iterator_t *generic_queue_iterator(generic_queue_t *queue, bool forward)
{
   if (queue && queue->first_item)
   {
      generic_queue_iterator_t *iterator;

      iterator = (generic_queue_iterator_t *)malloc(sizeof(generic_queue_iterator_t));
      iterator->queue = queue;
      iterator->item = forward ? queue->first_item : queue->last_item;
      iterator->forward = forward;

      return iterator;
   }

   return NULL;
}

generic_queue_iterator_t *generic_queue_iterator_next(generic_queue_iterator_t *iterator)
{
   if (iterator)
   {
      struct generic_queue_item_t *item;

      item = iterator->forward ? iterator->item->next : iterator->item->previous;
      if (item)
      {
         iterator->item = item;
         return iterator;
      } else
      {
         free(iterator);
         return NULL;
      }
   }

   return NULL;
}

void *generic_queue_iterator_value(generic_queue_iterator_t *iterator)
{
   if (iterator)
      return iterator->item->value;

   return NULL;
}

generic_queue_iterator_t *generic_queue_iterator_remove(generic_queue_iterator_t *iterator)
{
   struct generic_queue_item_t *item;

   if (!iterator)
      return NULL;

   item = iterator->forward ? iterator->queue->first_item : iterator->queue->last_item;
   while (item)
   {
      if (iterator->item == item)
      {
         if (iterator->queue->first_item == item)
            iterator->queue->first_item = item->next;
         else
            item->previous->next = item->next;

         if (iterator->queue->last_item == item)
            iterator->queue->last_item = item->previous;
         else
            item->next->previous = item->previous;

         iterator->queue->length--;

         iterator->item = iterator->forward ? item->next : item->previous;
         free(item);
         if (iterator->item)
         {
            return iterator;
         } else
         {
            free(iterator);
            return NULL;
         }
      }

      item = iterator->forward ? item->next : item->previous;
   }

   return iterator;
}

void generic_queue_iterator_free(generic_queue_iterator_t *iterator)
{
   if (iterator)
      free(iterator);
}
