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

#ifndef MMAL_LIST_H
#define MMAL_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup MmalList Generic Linked List
 * This provides a thread-safe implementation of a linked list which can be used
 * with any data type. */
/* @{ */

/** Single element in the list */
typedef struct MMAL_LIST_ELEMENT_T
{
   struct MMAL_LIST_ELEMENT_T *next;
   struct MMAL_LIST_ELEMENT_T *prev;
} MMAL_LIST_ELEMENT_T;

/** Linked list type.
 * Clients shouldn't modify this directly. Use the provided API functions to
 * add new elements. The public members are only for debug purposes.
 * */
typedef struct MMAL_LIST_T
{
   unsigned int length;         /**< Number of elements in the list (read-only) */
   MMAL_LIST_ELEMENT_T *first;  /**< First element in the list (read-only) */
   MMAL_LIST_ELEMENT_T *last;   /**< Last element in the list (read-only) */
} MMAL_LIST_T;

/** Create a new linked list.
 *
 * @return Pointer to new queue (NULL on failure).
 */
MMAL_LIST_T* mmal_list_create(void);

/** Destroy a linked list.
 *
 * @param list List to destroy
 */
void mmal_list_destroy(MMAL_LIST_T *list);

/** Remove the last element in the list.
 *
 * @param list    List to remove from
 *
 * @return Pointer to the last element (or NULL if empty)
 */
MMAL_LIST_ELEMENT_T* mmal_list_pop_back(MMAL_LIST_T *list);

/** Remove the first element in the list.
 *
 * @param list    List to remove from
 *
 * @return Pointer to the first element (or NULL if empty)
 */
MMAL_LIST_ELEMENT_T* mmal_list_pop_front(MMAL_LIST_T *list);

/** Add an element to the front of the list.
 *
 * @param list    List to add to
 * @param element The element to add
 */
void mmal_list_push_front(MMAL_LIST_T *list, MMAL_LIST_ELEMENT_T *element);

/** Add an element to the back of the list.
 *
 * @param list    List to add to
 * @param element The element to add
 */
void mmal_list_push_back(MMAL_LIST_T *list, MMAL_LIST_ELEMENT_T *element);

/** List comparison function.
 * This is supplied by a client when inserting an element in
 * the middle of the list. The list will always insert a smaller
 * element in front of a larger element.
 *
 * @return TRUE:  lhs <  rhs
 *         FALSE: lhs >= rhs
 */
typedef int (*MMAL_LIST_COMPARE_T)(MMAL_LIST_ELEMENT_T *lhs, MMAL_LIST_ELEMENT_T *rhs);

/** Insert an element into the list.
 * The location where the element is inserted is determined using
 * the supplied comparison function. Smaller elements are inserted
 * in front of larger elements.
 *
 * @param list    List to add to
 * @param element The element to insert
 * @param compare Comparison function supplied by the client
 */
void mmal_list_insert(MMAL_LIST_T *list, MMAL_LIST_ELEMENT_T *element, MMAL_LIST_COMPARE_T compare);

/* @} */

#ifdef __cplusplus
}
#endif

#endif /* MMAL_LIST_H */
