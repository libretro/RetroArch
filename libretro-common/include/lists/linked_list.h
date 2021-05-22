/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (linked_list.h).
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

#ifndef __LIBRETRO_SDK_LINKED_LIST_H
#define __LIBRETRO_SDK_LINKED_LIST_H

#include <retro_common_api.h>

#include <boolean.h>
#include <stddef.h>

RETRO_BEGIN_DECLS

/**
 * Represents a linked list. Contains any number of elements.
 */
typedef struct linked_list linked_list_t;

/**
 * Represents an iterator for iterating over a linked list. The iterator can
 * go through the linked list forwards or backwards.
 */
typedef struct linked_list_iterator linked_list_iterator_t;

/**
 * Creates a new linked list with no elements.
 *
 * @return New linked list
 */
linked_list_t *linked_list_new(void);

/**
 * @brief frees the memory used by the linked list
 * 
 * Frees all of the memory used by this linked list. The values of all
 * remaining elements are freed using the "free_value" function. Does
 * nothing if "list" is NULL.
 *
 * @param list linked list to free
 * @param free_value function to use to free remaining values
 */
void linked_list_free(linked_list_t *list, void (*free_value)(void *value));

/**
 * @brief adds an element to the linked list
 * 
 * Add a new element to the end of this linked list. Does nothing if
 * "list" is NULL.
 *
 * @param list list to add the element to
 * @param value new value to add to the linked list
 */
void linked_list_add(linked_list_t *list, void *value);

/**
 * @brief inserts a value into the linked list
 *
 * Inserts a value into the linked list at the specified index. Does
 * nothing if "list" is NULL.
 *
 * @param list list to insert the value into
 * @param index index where the value should be inserted at (can be equal to list size)
 * @param value value to insert into the linked list
 */
void linked_list_insert(linked_list_t *list, size_t index, void *value);

/**
 * @brief Get the value in the linked list at the provided index.
 *
 * Return the value vstored in the linked list at the provided index. Does
 * nothing if "list" is NULL.
 *
 * @param list list to get the value from
 * @param index index of the value to return
 * @return value in the list at the provided index
 */
void *linked_list_get(linked_list_t *list, size_t index);

/**
 * @brief Get the first value that is matched by the provided function
 *
 * Return the first value that the function matches. The matches function
 * parameters are value from the linked list and the provided state.
 *
 * @param list list to get the value from
 * @param matches function to test the values with
 * @param usrptr user data to pass to the matches function
 * @return first value that matches otherwise NULL
 */
void *linked_list_get_first_matching(linked_list_t *list, bool (*matches)(void *item, void *usrptr), void *usrptr);

/**
 * @brief Get the last value that is matched by the provided function
 *
 * Return the last value that the function matches. The matches function
 * parameters are value from the linked list and the provided state.
 *
 * @param list list to get the value from
 * @param matches function to test the values with
 * @param usrptr user data to pass to the matches function
 * @return last value that matches otherwise NULL
 */
void *linked_list_get_last_matching(linked_list_t *list, bool (*matches)(void *item, void *usrptr), void *usrptr);

/**
 * @brief Remove the element at the provided index
 *
 * Removes the element of the linked list at the provided index.
 *
 * @param list linked list to remove the element from
 * @param index index of the element to remove
 * @return value of the element that was removed, NULL if list is NULL or
 *         index is invalid
 */
void *linked_list_remove_at(linked_list_t *list, size_t index);

/**
 * @brief Remove the first element with the provided value
 *
 * Removes the first element with a value equal to the provided value.
 * Does nothing if "list" is NULL.
 *
 * @param list linked list to remove the element from
 * @param value value of the element to remove
 * @return value if a matching element was removed
 */
void *linked_list_remove_first(linked_list_t *list, void *value);

/**
 * @brief Remove the last element with the provided value
 *
 * Removes the last element with a value equal to the provided value.
 * Does nothing if "list" is NULL.
 *
 * @param list linked list to remove the element from
 * @param value value of the element to remove
 * @return value if a matching element was removed
 */
void *linked_list_remove_last(linked_list_t *list, void *value);

/**
 * @brief Remove all elements with the provided value
 *
 * Removes all elements with a value equal to the provided value.
 * Does nothing if "list" is NULL.
 *
 * @param list linked list to remove the elements from
 * @param value value of the elements to remove
 * @return value if any matching element(s) where removed
 */
void *linked_list_remove_all(linked_list_t *list, void *value);

/**
 * @brief Remove the first matching element
 *
 * Removes the first matching element from the linked list. The "matches" function
 * is used to test for matching element values. Does nothing if "list" is NULL.
 *
 * @param list linked list to remove the element from
 * @param matches function to use for testing element values for a match
 * @return value if a matching element was removed
 */
void *linked_list_remove_first_matching(linked_list_t *list, bool (*matches)(void *value));

/**
 * @brief Remove the last matching element
 *
 * Removes the last matching element from the linked list. The "matches" function
 * is used to test for matching element values.
 *
 * @param list linked list to remove the element from
 * @param matches function to use for testing element value for a match
 * @return value if a matching element was removed
 */
void *linked_list_remove_last_matching(linked_list_t *list, bool (*matches)(void *value));

/**
 * @brief Remove all matching elements
 *
 * Removes all matching elements from the linked list. The "matches" function
 * is used to test for matching element values. Does nothing if "list" is NULL.
 *
 * @param list linked list to remove the elements from
 * @param matches function to use for testing element values for a match
 */
void linked_list_remove_all_matching(linked_list_t *list, bool (*matches)(void *value));

/**
 * @brief Replace the value of the element at the provided index
 *
 * Replaces the value of the element at the provided index. The linked list must
 * contain an element at the index.
 *
 * @param list linked list to replace the value in
 * @param index index of the element to replace the value of
 * @param value new value for the selected element
 * @return whether an element was updated
 */
bool linked_list_set_at(linked_list_t *list, size_t index, void *value);

/**
 * @brief Get the size of the linked list
 *
 * Returns the number of elements in the linked list.
 *
 * @param linked list to get the size of
 * @return number of elements in the linked list, 0 if linked list is NULL
 */
size_t linked_list_size(linked_list_t *list);

/**
 * @brief Get an iterator for the linked list
 *
 * Returns a new iterator for the linked list. Can be either a forward or backward
 * iterator.
 *
 * @param list linked list to iterate over
 * @param forward true for a forward iterator, false for backwards
 * @return new iterator for the linked list in the specified direction, NULL if
 *         "list" is NULL
 */
linked_list_iterator_t *linked_list_iterator(linked_list_t *list, bool forward);

/**
 * @brief Move to the next element in the linked list
 *
 * Moves the iterator to the next element in the linked list. The direction is
 * specified when retrieving a new iterator.
 *
 * @param iterator iterator for the current element
 * @return iterator for the next element, NULL if iterator is NULL or "iterator"
 *         is at the last element
 */
linked_list_iterator_t *linked_list_iterator_next(linked_list_iterator_t *iterator);

/**
 * @brief Get the value of the element for the iterator
 *
 * Returns the value of the element that the iterator is at.
 *
 * @param iterator iterator for the current element
 * @return value of the element for the iterator
 */
void *linked_list_iterator_value(linked_list_iterator_t *iterator);

/**
 * @brief Remove the element that the iterator is at
 *
 * Removes the element that the iterator is at. The iterator is updated to the
 * next element.
 *
 * @param iterator iterator for the current element
 * @return updated iterator or NULL if the last element was removed
 */
linked_list_iterator_t *linked_list_iterator_remove(linked_list_iterator_t *iterator);

/**
 * @brief Release the memory for the iterator
 *
 * Frees the memory for the provided iterator. Does nothing if "iterator" is NULL.
 *
 * @param iterator iterator to free
 */
void linked_list_iterator_free(linked_list_iterator_t *iterator);

/**
 * @brief Apply the provided function to all values in the linked list
 *
 * Apply the provied function to all values in the linked list. The values are applied
 * in the forward direction. Does nothing if "list" is NULL.
 *
 * @param list linked list to apply the function to
 * @param fn function to apply to all elements
 */
void linked_list_foreach(linked_list_t *list, void (*fn)(size_t index, void *value));

RETRO_END_DECLS

#endif
