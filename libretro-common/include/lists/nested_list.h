/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (nested_list.h).
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

#ifndef __LIBRETRO_SDK_NESTED_LIST_H__
#define __LIBRETRO_SDK_NESTED_LIST_H__

#include <retro_common_api.h>

#include <boolean.h>
#include <stdlib.h>
#include <stddef.h>

RETRO_BEGIN_DECLS

/* Prevent direct access to nested_list_* members */
typedef struct nested_list_item nested_list_item_t;
typedef struct nested_list nested_list_t;

/**************************************/
/* Initialisation / De-Initialisation */
/**************************************/

/**
 * nested_list_init:
 *
 * Creates a new empty nested list. Returned pointer
 * must be freed using nested_list_free.
 *
 * Returns: Valid nested_list_t pointer if successful,
 * otherwise NULL.
 */
nested_list_t *nested_list_init(void);

/**
 * nested_list_free:
 *
 * @list : pointer to nested_list_t object
 *
 * Frees specified nested list.
 */
void nested_list_free(nested_list_t *list);

/***********/
/* Setters */
/***********/

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
      const char *address, const char *delim, const void *value);

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
size_t nested_list_get_size(nested_list_t *list);

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
      const char *address, const char *delim);

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
      size_t idx);

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
nested_list_item_t *nested_list_item_get_parent(nested_list_item_t *list_item);

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
nested_list_t *nested_list_item_get_parent_list(nested_list_item_t *list_item);

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
nested_list_t *nested_list_item_get_children(nested_list_item_t *list_item);

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
const char *nested_list_item_get_id(nested_list_item_t *list_item);

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
      const char *delim, char *address, size_t len);

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
const void *nested_list_item_get_value(nested_list_item_t *list_item);

RETRO_END_DECLS

#endif
