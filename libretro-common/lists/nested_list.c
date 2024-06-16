/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (nested_list.c).
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

#include <string/stdstring.h>
#include <lists/string_list.h>
#include <array/rbuf.h>
#include <array/rhmap.h>

#include <lists/nested_list.h>

struct nested_list_item
{
   nested_list_item_t *parent_item;
   nested_list_t *parent_list;
   nested_list_t *children;
   char *id;
   const void *value;
};

struct nested_list
{
   nested_list_item_t **items;
   nested_list_item_t **item_map;
};

/**************************************/
/* Initialisation / De-Initialisation */
/**************************************/

/* Forward declaration - required since
 * nested_list_free_list() is recursive */
static void nested_list_free_list(nested_list_t *list);

/* Frees contents of a nested list item */
static void nested_list_free_item(nested_list_item_t *item)
{
   if (!item)
      return;

   item->parent_item = NULL;
   item->parent_list = NULL;

   if (item->children)
   {
      nested_list_free_list(item->children);
      item->children = NULL;
   }

   if (item->id)
   {
      free(item->id);
      item->id = NULL;
   }

   item->value = NULL;
   free(item);
}

/* Frees contents of a nested list */
static void nested_list_free_list(nested_list_t *list)
{
   size_t i;

   if (!list)
      return;

   for (i = 0; i < RBUF_LEN(list->items); i++)
      nested_list_free_item(list->items[i]);

   RBUF_FREE(list->items);
   RHMAP_FREE(list->item_map);
   free(list);
}

/**
 * nested_list_init:
 *
 * Creates a new empty nested list. Returned pointer
 * must be freed using nested_list_free.
 *
 * Returns: Valid nested_list_t pointer if successful,
 * otherwise NULL.
 */
nested_list_t *nested_list_init(void)
{
   /* Create nested list */
   nested_list_t *list = (nested_list_t*)malloc(sizeof(*list));

   if (!list)
      return NULL;

   /* Initialise members */
   list->items    = NULL;
   list->item_map = NULL;

   return list;
}

/**
 * nested_list_free:
 * @list : pointer to nested_list_t object
 *
 * Frees specified nested list.
 */
void nested_list_free(nested_list_t *list)
{
   nested_list_free_list(list);
}

/***********/
/* Setters */
/***********/

/* Creates and adds a new item to the specified
 * nested list. Returns NULL if item matching 'id'
 * already exists */
static nested_list_item_t *nested_list_add_item_to_list(nested_list_t *list,
      nested_list_item_t *parent_item, const char *id, const void *value)
{
   size_t num_items             = 0;
   nested_list_item_t *new_item = NULL;
   nested_list_t *child_list    = NULL;

   if (!list || string_is_empty(id))
      goto end;

   num_items = RBUF_LEN(list->items);

   /* Ensure that item does not already exist */
   if (RHMAP_HAS_STR(list->item_map, id))
      goto end;

   /* Attempt to allocate a buffer slot for the
    * new item */
   if (!RBUF_TRYFIT(list->items, num_items + 1))
      goto end;

   /* Create new empty child list */
   child_list = nested_list_init();
   if (!child_list)
      goto end;

   /* Create new list item */
   new_item = (nested_list_item_t*)malloc(sizeof(*new_item));
   if (!new_item)
   {
      nested_list_free(child_list);
      goto end;
   }

   /* Assign members */
   new_item->parent_item = parent_item;
   new_item->parent_list = list;
   new_item->children    = child_list;
   new_item->id          = strdup(id);
   new_item->value       = value;

   /* Increment item buffer size */
   RBUF_RESIZE(list->items, num_items + 1);

   /* Add new item to buffer */
   list->items[num_items] = new_item;

   /* Update map */
   RHMAP_SET_STR(list->item_map, id, new_item);
end:
   return new_item;
}

/**
 * nested_list_add_item:
 *
 * @list    : pointer to nested_list_t object
 * @address : a delimited list of item identifiers,
 *            corresponding to item 'levels'
 * @delim   : delimiter to use when splitting @address
 *            into individual ids
 * @value   : optional value (user data) associated with
 *            new list item. This is added to the last
 *            item specified by @address
 *
 * Appends a new item to the specified nested list.
 * If @delim is NULL, item is added to the top level
 * list (@list itself) with id equal to @address.
 * Otherwise, @address is split by @delim and each
 * id is added as new 'layer'. For example:
 *
 * > @address = "one:two:three", @delim = ":" will
 *   produce:
 *      top_level_list:one
 *                     `- "one" list:two
 *                                   `- "two" list:three
 *   where @value is assigned to the "two" list:three
 *   item.
 *
 * Returns: true if successful, otherwise false. Will
 * always return false if item specified by @address
 * already exists in the nested list.
 */
bool nested_list_add_item(nested_list_t *list,
      const char *address, const char *delim, const void *value)
{
   struct string_list id_list = {0};
   const char *top_id         = NULL;
   bool success               = false;

   if (!list || string_is_empty(address))
      goto end;

   /* If delim is NULL or address contains a single
    * token, then we are adding an item to the top
    * level list */
   if (string_is_empty(delim))
      top_id = address;
   else
   {
      string_list_initialize(&id_list);
      if (!string_split_noalloc(&id_list, address, delim) ||
          (id_list.size < 1))
         goto end;

      if (id_list.size == 1)
         top_id = id_list.elems[0].data;
   }

   if (!string_is_empty(top_id))
   {
      if (nested_list_add_item_to_list(list, NULL, top_id, value))
         success = true;
   }
   else
   {
      nested_list_t *current_list     = list;
      nested_list_item_t *parent_item = NULL;
      nested_list_item_t *next_item   = NULL;
      size_t i;

      /* Loop over list item ids */
      for (i = 0; i < id_list.size; i++)
      {
         const char *id = id_list.elems[i].data;

         if (string_is_empty(id))
            goto end;

         /* If this is the last entry in the id list,
          * then we are adding the item itself */
         if (i == (id_list.size - 1))
         {
            if (nested_list_add_item_to_list(current_list,
                  parent_item, id, value))
               success = true;

            break;
         }
         /* Otherwise, id corresponds to a 'category' */
         else
         {
            /* Check whether category item already exists */
            next_item = RHMAP_GET_STR(current_list->item_map, id);

            /* Create it, if required */
            if (!next_item)
               next_item = nested_list_add_item_to_list(current_list,
                     parent_item, id, NULL);

            if (!next_item)
               break;

            /* Update pointers */
            parent_item  = next_item;
            current_list = next_item->children;
         }
      }
   }

end:
   string_list_deinitialize(&id_list);
   return success;
}

/***********/
/* Getters */
/***********/

/**
 * nested_list_get_size:
 *
 * @list : pointer to nested_list_t object
 *
 * Fetches the current size (number of items) in
 * the specified list.
 *
 * Returns: list size.
 */
size_t nested_list_get_size(nested_list_t *list)
{
   if (!list)
      return 0;

   return RBUF_LEN(list->items);
}

/**
 * nested_list_get_item:
 *
 * @list    : pointer to nested_list_t object
 * @address : a delimited list of item identifiers,
 *            corresponding to item 'levels'
 * @delim   : delimiter to use when splitting @address
 *            into individual ids
 *
 * Searches for (and returns) the list item corresponding
 * to @address. If @delim is NULL, the top level list
 * (@list itself) is searched for an item with an id
 * equal to @address. Otherwise, @address is split by
 * @delim and each id is searched for in a subsequent
 * list level.
 *
 * Returns: valid nested_list_item_t pointer if item
 * is found, otherwise NULL.
 */
nested_list_item_t *nested_list_get_item(nested_list_t *list,
      const char *address, const char *delim)
{
   nested_list_item_t *search_item = NULL;
   struct string_list id_list      = {0};
   const char *top_id              = NULL;

   if (!list || string_is_empty(address))
      goto end;

   /* If delim is NULL or address contains a single
    * token, then we are fetching an item from the
    * top level list */
   if (string_is_empty(delim))
      top_id = address;
   else
   {
      string_list_initialize(&id_list);
      if (!string_split_noalloc(&id_list, address, delim) ||
          (id_list.size < 1))
         goto end;

      if (id_list.size == 1)
         top_id = id_list.elems[0].data;
   }

   if (!string_is_empty(top_id))
      search_item = RHMAP_GET_STR(list->item_map, top_id);
   else
   {
      /* Otherwise, search 'category' levels */
      nested_list_t *current_list   = list;
      nested_list_item_t *next_item = NULL;
      size_t i;

      /* Loop over list item ids */
      for (i = 0; i < id_list.size; i++)
      {
         const char *id = id_list.elems[i].data;

         if (string_is_empty(id))
            goto end;

         /* If this is the last entry in the id list,
          * then we are searching for the item itself */
         if (i == (id_list.size - 1))
         {
            search_item = RHMAP_GET_STR(current_list->item_map, id);
            break;
         }
         /* Otherwise, id corresponds to a 'category' */
         else
         {
            next_item = RHMAP_GET_STR(current_list->item_map, id);

            if (!next_item)
               break;

            /* Update pointer */
            current_list = next_item->children;
         }
      }
   }

end:
   string_list_deinitialize(&id_list);
   return search_item;
}

/**
 * nested_list_get_item_idx:
 *
 * @list : pointer to nested_list_t object
 * @idx  : item index
 *
 * Fetches the item corresponding to index @idx in
 * the top level list (@list itself) of the specified
 * nested list.
 *
 * Returns: valid nested_list_item_t pointer if item
 * exists, otherwise NULL.
 */
nested_list_item_t *nested_list_get_item_idx(nested_list_t *list,
      size_t idx)
{
   if (!list || (idx >= RBUF_LEN(list->items)))
      return NULL;

   return list->items[idx];
}

/**
 * nested_list_item_get_parent:
 *
 * @list_item : pointer to nested_list_item_t object
 *
 * Fetches the parent item of the specified nested
 * list item. If returned value is NULL, specified
 * nested list item belongs to a top level list.
 *
 * Returns: valid nested_list_item_t pointer if item
 * has a parent, otherwise NULL.
 */
nested_list_item_t *nested_list_item_get_parent(nested_list_item_t *list_item)
{
   if (!list_item)
      return NULL;

   return list_item->parent_item;
}

/**
 * nested_list_item_get_parent_list:
 *
 * @list_item : pointer to nested_list_item_t object
 *
 * Fetches a pointer to the nested list of which the
 * specified list item is a direct member.
 *
 * Returns: valid nested_list_t pointer if successful,
 * otherwise NULL.
 */
nested_list_t *nested_list_item_get_parent_list(nested_list_item_t *list_item)
{
   if (!list_item)
      return NULL;

   return list_item->parent_list;
}

/**
 * nested_list_item_get_children:
 *
 * @list_item : pointer to nested_list_item_t object
 *
 * Fetches a pointer to the nested list of child items
 * belonging to the specified list item.
 *
 * Returns: valid nested_list_t pointer if item has
 * children, otherwise NULL.
 */
nested_list_t *nested_list_item_get_children(nested_list_item_t *list_item)
{
   if (!list_item ||
       !list_item->children ||
       (RBUF_LEN(list_item->children->items) < 1))
      return NULL;

   return list_item->children;
}

/**
 * nested_list_item_get_id:
 *
 * @list_item : pointer to nested_list_item_t object
 *
 * Fetches the id string of the specified list item,
 * as set by nested_list_add_item().
 *
 * Returns: item id if successful, otherwise NULL.
 */
const char *nested_list_item_get_id(nested_list_item_t *list_item)
{
   if (!list_item)
      return NULL;

   return list_item->id;
}

/**
 * nested_list_item_get_address:
 *
 * @list_item : pointer to nested_list_item_t object
 * @delim     : delimiter to use when concatenating
 *              individual item ids into a an @address
 *              string
 * @address   : a delimited list of item identifiers,
 *              corresponding to item 'levels'
 * @len       : length of supplied @address char array

 * Fetches a compound @address string corresponding to
 * the specified item's 'position' in the top level
 * nested list of which it is a member. The resultant
 * @address may be used to find the item when calling
 * nested_list_get_item() on the top level nested list.
 *
 * Returns: true if successful, otherwise false.
 */
bool nested_list_item_get_address(nested_list_item_t *list_item,
      const char *delim, char *address, size_t len)
{
   nested_list_item_t *current_item = list_item;
   struct string_list id_list       = {0};
   bool success                     = false;
   union string_list_elem_attr attr;
   size_t i;

   if (!list_item ||
       string_is_empty(delim) ||
       !address ||
       (len < 1))
      goto end;

   address[0] = '\0';
   attr.i     = 0;

   /* If this is an item of the top level
    * list, just copy the item id directly */
   if (!list_item->parent_item)
   {
      strlcpy(address, list_item->id, len);
      success = true;
      goto end;
   }

   /* ...otherwise we have to combine the ids
    * of the item and all of its 'ancestors' */
   string_list_initialize(&id_list);

   /* Fetch all ids */
   do
   {
      const char *id = current_item->id;

      if (string_is_empty(id) ||
          !string_list_append(&id_list, id, attr))
         goto end;

      current_item = current_item->parent_item;
   }
   while (current_item);

   if (id_list.size < 1)
      goto end;

   /* Build address string */
   for (i = id_list.size; i > 0; i--)
   {
      size_t _len;
      const char *id = id_list.elems[i - 1].data;

      if (string_is_empty(id))
         goto end;

      _len = strlcat(address, id, len);
      if (i > 1)
         strlcpy(address + _len, delim, len - _len);
   }

   success = true;
end:
   string_list_deinitialize(&id_list);
   return success;
}

/**
 * nested_list_item_get_value:
 *
 * @list_item : pointer to nested_list_item_t object
 *
 * Fetches the value (user data) associated with the
 * specified list item.
 *
 * Returns: pointer to user data if set, otherwise
 * NULL.
 */
const void *nested_list_item_get_value(nested_list_item_t *list_item)
{
   if (!list_item)
      return NULL;

   return list_item->value;
}
