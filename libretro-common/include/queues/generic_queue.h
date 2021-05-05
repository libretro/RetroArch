/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (generic_queue.h).
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

#ifndef __LIBRETRO_SDK_GENERIC_QUEUE_H
#define __LIBRETRO_SDK_GENERIC_QUEUE_H

#include <retro_common_api.h>

#include <boolean.h>
#include <stddef.h>

RETRO_BEGIN_DECLS

/**
 * Represents a generic queue. Can contain any number of elements. Each element contains
 * a value of type "void *". Can be used as a FIFO or LIFO queue.
 */
typedef struct generic_queue generic_queue_t;

/**
 * Represents an iterator for iterating over a queue.
 */
typedef struct generic_queue_iterator generic_queue_iterator_t;

/**
 * Creates a new queue with no elements.
 *
 * @return New queue
 */
generic_queue_t *generic_queue_new(void);

/**
 * @brief frees the memory used by the queue
 * 
 * Frees all of the memory used by this queue. The values of all
 * remaining elements are freed using the "free_value" function. Does
 * nothing if "queue" is NULL.
 *
 * @param queue queue to free
 * @param free_value function to use to free remaining values
 */
void generic_queue_free(generic_queue_t *queue, void (*free_value)(void *value));

/**
 * @brief Push a new value onto the queue
 *
 * Pushes a new value onto the end of the queue. Does nothing if "queue"
 * is NULL.
 *
 * @param queue queue to the push the value onto
 * @param value value to push onto the queue
 */
void generic_queue_push(generic_queue_t *queue, void *value);

/**
 * @brief Remove the last value from the queue
 *
 * Removes the last element from the queue. Does nothing if the queue is
 * NULL.
 *
 * @param queue queue to get the value from
 * @return value of the last element, NULL if queue is empty or NULL
 */
void *generic_queue_pop(generic_queue_t *queue);

/**
 * @brief Get the last value from the queue
 *
 * Returns the value of the last element in the queue. Returns NULL if the
 * queue is NULL or empty.
 *
 * @param queue queue to get the last value from
 * @return value of the last element or NULL
 */
void *generic_queue_peek(generic_queue_t *queue);

/**
 * @brief Get the first value from the queue
 *
 * Returns the value of the first element in the queue. Returns NULL if the
 * queue is NULL or empty.
 *
 * @param queue queue to get the first value from
 * @return value of the first element or NULL
 */
void *generic_queue_peek_first(generic_queue_t *queue);

/**
 * @brief Push a new value onto the front of the queue
 *
 * Pushes a new value onto the front of the queue. Does nothing if "queue"
 * is NULL.
 *
 * @param queue queue to the push the value onto
 * @param value value to push onto the queue
 */
void generic_queue_shift(generic_queue_t *queue, void *value);

/**
 * @brief Remove the first value from the queue
 *
 * Removes the first element from the queue. Does nothing if the queue is
 * NULL.
 *
 * @param queue queue to get the value from
 * @return value of the last element, NULL if queue is empty or NULL
 */
void *generic_queue_unshift(generic_queue_t *queue);

/**
 * @brief Get the size of the queue
 *
 * Returns the number of elements in the queue.
 *
 * @param queue queue to get the size of
 * @return number of elements in the queue, 0 if queue is NULL
 */
size_t generic_queue_length(generic_queue_t *queue);

/**
 * @brief Remove the first element in the queue with the provided value
 *
 * Removes the first element with a value matching the provided value. Does
 * nothing if queue is NULL.
 *
 * @param queue queue to remove the element from
 * @param value value to look for in the queue
 * @return the value of the element removed, NULL if no element was removed
 */
void *generic_queue_remove(generic_queue_t *queue, void *value);

/**
 * @brief Get an iterator for the queue
 *
 * Returns a new iterator for the queue. Can be either a forward or backward
 * iterator.
 *
 * @param queue queue to iterate over
 * @param forward true for a forward iterator, false for backwards
 * @return new iterator for the queue in the specified direction, NULL if
 *         "queue" is NULL
 */
generic_queue_iterator_t *generic_queue_iterator(generic_queue_t *queue, bool forward);

/**
 * @brief Move to the next element in the queue
 *
 * Moves the iterator to the next element in the queue. The direction is
 * specified when retrieving a new iterator.
 *
 * @param iterator iterator for the current element
 * @return iterator for the next element, NULL if iterator is NULL or "iterator"
 *         is at the last element
 */
generic_queue_iterator_t *generic_queue_iterator_next(generic_queue_iterator_t *iterator);

/**
 * @brief Get the value of the element for the iterator
 *
 * Returns the value of the element that the iterator is at.
 *
 * @param iterator iterator for the current element
 * @return value of the element for the iterator
 */
void *generic_queue_iterator_value(generic_queue_iterator_t *iterator);

/**
 * @brief Remove the element that the iterator is at
 *
 * Removes the element that the iterator is at. The iterator is updated to the
 * next element.
 *
 * @param iterator iterator for the current element
 * @return updated iterator or NULL if the last element was removed
 */
generic_queue_iterator_t *generic_queue_iterator_remove(generic_queue_iterator_t *iterator);

/**
 * @brief Release the memory for the iterator
 *
 * Frees the memory for the provided iterator. Does nothing if "iterator" is NULL.
 *
 * @param iterator iterator to free
 */
void generic_queue_iterator_free(generic_queue_iterator_t *iterator);

RETRO_END_DECLS

#endif
