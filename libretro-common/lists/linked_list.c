/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (linked_list.c).
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

#include <lists/linked_list.h>

struct linked_list_item_t
{
   void *value;
   struct linked_list_item_t *previous;
   struct linked_list_item_t *next;
};

struct linked_list
{
   struct linked_list_item_t *first_item;
   struct linked_list_item_t *last_item;
   size_t length;
};

struct linked_list_iterator
{
   linked_list_t *list;
   struct linked_list_item_t *item;
   bool forward;
};

linked_list_t *linked_list_new(void)
{
   linked_list_t *list;

   list = (linked_list_t *)calloc(sizeof(linked_list_t), 1);
   return list;
}

void linked_list_free(linked_list_t *list, void (*free_value)(void *value))
{
   if (!list)
   {
      return;
   }

   while (list->first_item)
   {
      struct linked_list_item_t *next;

      next = list->first_item->next;
      if (free_value)
         free_value(list->first_item->value);
      free(list->first_item);

      list->first_item = next;
   }

   free (list);
}

void linked_list_add(linked_list_t *list, void *value)
{
   struct linked_list_item_t *new_item;

   if (!list)
      return;

   new_item = (struct linked_list_item_t *)malloc(sizeof(struct linked_list_item_t));
   new_item->value = value;
   new_item->previous = list->last_item;
   new_item->next = NULL;

   if (list->length == 0)
      list->first_item = new_item;
   else
      list->last_item->next = new_item;

   list->last_item = new_item;
   list->length++;
}

void linked_list_insert(linked_list_t *list, size_t index, void *value)
{
   size_t i;
   struct linked_list_item_t *previous_item;
   struct linked_list_item_t *next_item;
   struct linked_list_item_t *new_item;

   if (!list || index > list->length)
      return;

   previous_item = NULL;
   next_item = list->first_item;
   for (i = 1; i <= index; i++)
   {
      previous_item = next_item;
      next_item = next_item->next;
   }

   new_item = (struct linked_list_item_t *)malloc(sizeof(struct linked_list_item_t));
   new_item->value = value;

   if (previous_item)
      previous_item->next = new_item;
   else
      list->first_item = new_item;
   new_item->previous = previous_item;

   if (next_item)
      next_item->previous = new_item;
   else
      list->last_item = new_item;
   new_item->next = next_item;

   list->length++;
}

void *linked_list_get(linked_list_t *list, size_t index)
{
   size_t i;
   struct linked_list_item_t *item;

   if (!list)
      return NULL;

   if (index >= list->length)
      return NULL;

   item = list->first_item;
   for (i = 1; i <= index; i++)
      item = item->next;

   return item->value;
}

void *linked_list_get_first_matching(linked_list_t *list, bool (*matches)(void *item, void *usrptr), void *usrptr)
{
   struct linked_list_item_t *item;

   if (!list || !matches)
      return NULL;

   for (item = list->first_item; item; item = item->next)
   {
      if (matches(item->value, usrptr))
         return item->value;
   }

   return NULL;
}

void *linked_list_get_last_matching(linked_list_t *list, bool (*matches)(void *item, void *usrptr), void *usrptr)
{
   struct linked_list_item_t *item;

   if (!list || !matches)
      return NULL;

   for (item = list->last_item; item; item = item->previous)
   {
      if (matches(item->value, usrptr))
         return item->value;
   }

   return NULL;
}

static void _linked_list_remove_item(linked_list_t *list, struct linked_list_item_t *item)
{
   struct linked_list_item_t *previous_item;
   struct linked_list_item_t *next_item;

   previous_item = item->previous;
   next_item = item->next;
   free(item);
   list->length--;

   if (previous_item)
      previous_item->next = next_item;
   else
      list->first_item = next_item;

   if (next_item)
      next_item->previous = previous_item;
   else
      list->last_item = previous_item;
}

void *linked_list_remove_at(linked_list_t *list, size_t index)
{
   size_t i = 0;
   struct linked_list_item_t *item;
   void *value;

   if (!list || list->length == 0 || index >= list->length)
      return NULL;

   item = list->first_item;
   for (i = 1; i <= index; i++)
      item = item->next;

   value = item->value;
   _linked_list_remove_item(list, item);
   return value;
}

void *linked_list_remove_first(linked_list_t *list, void *value)
{
   struct linked_list_item_t *item;

   if (!list)
      return NULL;

   for (item = list->first_item; item; item = item->next)
   {
      if (item->value == value)
         break;
   }

   if (item)
   {
      _linked_list_remove_item(list, item);
      return value;
   }

   return NULL;
}

void *linked_list_remove_last(linked_list_t *list, void *value)
{
   struct linked_list_item_t *item;

   if (!list)
      return NULL;

   for (item = list->last_item; item; item = item->previous)
   {
      if (item->value == value)
         break;
   }

   if (item)
   {
      _linked_list_remove_item(list, item);
      return value;
   }

   return NULL;
}

void *linked_list_remove_all(linked_list_t *list, void *value)
{
   struct linked_list_item_t *item;
   bool found = false;

   if (!list)
      return NULL;

   for (item = list->first_item; item;)
   {
      if (item->value == value)
      {
         struct linked_list_item_t *next_item;

         next_item = item->next;
         _linked_list_remove_item(list, item);
         found = true;
         item = next_item;
      } else
      {
         item = item->next;
      }
   }

   return found ? value : NULL;
}

void *linked_list_remove_first_matching(linked_list_t *list, bool (*matches)(void *value))
{
   struct linked_list_item_t *item;

   if (!list)
      return NULL;

   for (item = list->first_item; item; item = item->next)
   {
      if (matches(item->value))
         break;
   }

   if (item)
   {
      void *value;

      value = item->value;
      _linked_list_remove_item(list, item);
      return value;
   }

   return NULL;
}

void *linked_list_remove_last_matching(linked_list_t *list, bool (*matches)(void *value))
{
   struct linked_list_item_t *item;

   if (!list)
      return NULL;

   for (item = list->last_item; item; item = item->previous)
   {
      if (matches(item->value))
         break;
   }

   if (item)
   {
      void *value;

      value = item->value;
      _linked_list_remove_item(list, item);
      return value;
   }

   return NULL;
}

void linked_list_remove_all_matching(linked_list_t *list, bool (*matches)(void *value))
{
   struct linked_list_item_t *item;

   if (!list)
      return;

   for (item = list->first_item; item;)
   {
      if (matches(item->value))
      {
         struct linked_list_item_t *next_item;

         next_item = item->next;
         _linked_list_remove_item(list, item);
         item = next_item;
      } else
      {
         item = item->next;
      }
   }
}

bool linked_list_set_at(linked_list_t *list, size_t index, void *value)
{
   struct linked_list_item_t *item;
   size_t i;

   if (!list || list->length == 0 || index >= list->length)
      return false;

   item = list->first_item;
   for (i = 1; i <= index; i++)
      item = item->next;

   if (item)
   {
      item->value = value;
      return true;
   }

   return false;
}

size_t linked_list_size(linked_list_t *list)
{
   if (list)
      return list->length;

   return 0;
}

linked_list_iterator_t *linked_list_iterator(linked_list_t *list, bool forward)
{
   linked_list_iterator_t *iterator;

   if (!list || !list->first_item)
      return NULL;

   iterator = (linked_list_iterator_t *)malloc(sizeof(linked_list_iterator_t));
   iterator->list = list;
   iterator->item = forward ? list->first_item : list->last_item;
   iterator->forward = forward;

   return iterator;
}

linked_list_iterator_t *linked_list_iterator_next(linked_list_iterator_t *iterator)
{
   struct linked_list_item_t *item;

   if (!iterator)
      return NULL;

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

void *linked_list_iterator_value(linked_list_iterator_t *iterator)
{
   if (iterator)
      return iterator->item->value;

   return NULL;
}

linked_list_iterator_t *linked_list_iterator_remove(linked_list_iterator_t *iterator)
{
   struct linked_list_item_t *next_item;

   if (!iterator)
      return NULL;

   next_item = iterator->forward ? iterator->item->next : iterator->item->previous;
   _linked_list_remove_item(iterator->list, iterator->item);

   if (next_item)
   {
      iterator->item = next_item;
      return iterator;
   } else
   {
      free(iterator);
      return NULL;
   }
}

void linked_list_iterator_free(linked_list_iterator_t *iterator)
{
   if (iterator)
      free(iterator);
}

void linked_list_foreach(linked_list_t *list, void (*fn)(size_t index, void *value))
{
   size_t i;
   struct linked_list_item_t *item;

   if (!list || !fn)
      return;

   i = 0;
   for (item = list->first_item; item; item = item->next)
      fn(i++, item->value);
}
