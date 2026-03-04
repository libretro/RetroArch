/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (message_queue.c).
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

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <queues/message_queue.h>
#include <compat/strl.h>
#include <compat/posix_string.h>

/* Swap two heap element pointers in-place. */
static void heap_swap(struct queue_elem **elems, size_t a, size_t b)
{
   struct queue_elem *tmp = elems[a];
   elems[a]              = elems[b];
   elems[b]              = tmp;
}

/*
 * Sift the element at position @pos downward through the max-heap
 * rooted at index 1, restoring the heap invariant after a removal.
 *
 * Uses priority-field comparison (correct) rather than pointer
 * comparison (which was a latent bug in the original sift-down).
 *
 * The tight loop avoids redundant boundary checks by combining
 * left/right child selection into a single branch.
 */
static void heap_sift_down(struct queue_elem **elems, size_t ptr, size_t pos)
{
   for (;;)
   {
      size_t left         = pos << 1;      /* pos * 2      */
      size_t right        = left  + 1;     /* pos * 2 + 1  */
      size_t largest      = pos;

      if (left  <= ptr && elems[left ]->prio > elems[largest]->prio)
         largest = left;
      if (right <= ptr && elems[right]->prio > elems[largest]->prio)
         largest = right;

      if (largest == pos)
         break;

      heap_swap(elems, pos, largest);
      pos = largest;
   }
}

bool msg_queue_initialize(msg_queue_t *queue, size_t len)
{
   struct queue_elem **elems = NULL;

   if (!queue)
      return false;

   if (!(elems = (struct queue_elem**)
            calloc(len + 1, sizeof(struct queue_elem*))))
      return false;

   queue->tmp_msg = NULL;
   queue->elems   = elems;
   queue->ptr     = 1;
   queue->size    = len + 1;

   return true;
}

/**
 * msg_queue_new:
 * @len              : maximum number of queued messages
 *
 * Creates a message queue with capacity for @len messages.
 *
 * Returns: NULL on allocation failure, otherwise a pointer to the
 * new queue.  Must be freed with msg_queue_free().
 **/
msg_queue_t *msg_queue_new(size_t len)
{
   msg_queue_t *queue = (msg_queue_t*)malloc(sizeof(*queue));

   if (!msg_queue_initialize(queue, len))
   {
      free(queue); /* safe: free(NULL) is a no-op */
      return NULL;
   }

   return queue;
}

/**
 * msg_queue_free:
 * @queue             : pointer to queue object
 *
 * Clears and frees a heap-allocated message queue.
 **/
void msg_queue_free(msg_queue_t *queue)
{
   if (!queue)
      return;
   msg_queue_clear(queue);
   free(queue->elems);
   free(queue);
}

bool msg_queue_deinitialize(msg_queue_t *queue)
{
   if (!queue)
      return false;
   msg_queue_clear(queue);
   free(queue->elems);
   queue->elems   = NULL;
   queue->tmp_msg = NULL;
   queue->ptr     = 0;
   queue->size    = 0;
   return true;
}

/**
 * msg_queue_push:
 * @queue             : pointer to queue object
 * @msg               : message string to enqueue (copied internally)
 * @prio              : priority; higher value = higher priority
 * @duration          : number of pulls before the message expires
 * @title             : optional title string (may be NULL)
 * @icon              : icon identifier
 * @category          : message category
 *
 * Inserts a new message into the priority queue and sifts it up to
 * its correct heap position.  O(log n).
 **/
void msg_queue_push(msg_queue_t *queue, const char *msg,
      unsigned prio, unsigned duration,
      char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   struct queue_elem *new_elem = NULL;
   size_t tmp_ptr;

   if (!queue || queue->ptr >= queue->size)
      return;

   if (!(new_elem = (struct queue_elem*)malloc(sizeof(struct queue_elem))))
      return;

   new_elem->duration = duration;
   new_elem->prio     = prio;
   new_elem->msg      = msg   ? strdup(msg)   : NULL;
   new_elem->title    = title ? strdup(title) : NULL;
   new_elem->icon     = icon;
   new_elem->category = category;

   /* Place at the next free leaf, then sift up. */
   tmp_ptr                  = queue->ptr++;
   queue->elems[tmp_ptr]    = new_elem;

   while (tmp_ptr > 1)
   {
      size_t parent_idx = tmp_ptr >> 1;
      if (new_elem->prio <= queue->elems[parent_idx]->prio)
         break;
      heap_swap(queue->elems, tmp_ptr, parent_idx);
      tmp_ptr = parent_idx;
   }
}

/**
 * msg_queue_clear:
 * @queue             : pointer to queue object
 *
 * Frees all messages in the queue and resets it to empty.
 **/
void msg_queue_clear(msg_queue_t *queue)
{
   size_t i;

   if (!queue)
      return;

   for (i = 1; i < queue->ptr; i++)
   {
      if (queue->elems[i])
      {
         free(queue->elems[i]->msg);
         free(queue->elems[i]->title);
         free(queue->elems[i]);
         queue->elems[i] = NULL;
      }
   }

   queue->ptr = 1;
   free(queue->tmp_msg);
   queue->tmp_msg = NULL;
}

/**
 * msg_queue_pull:
 * @queue             : pointer to queue object
 *
 * Returns the message string of the highest-priority element.
 * Decrements its duration counter; once it reaches zero the element
 * is removed from the heap, the heap is restored, and the (now
 * expired) message string is returned one final time via tmp_msg.
 *
 * The returned pointer is valid only until the next call that
 * modifies the queue.
 *
 * Returns: NULL if the queue is empty, otherwise the message string.
 **/
const char *msg_queue_pull(msg_queue_t *queue)
{
   struct queue_elem *front = NULL;
   struct queue_elem *last  = NULL;

   /* Nothing in queue. */
   if (!queue || queue->ptr == 1)
      return NULL;

   front = queue->elems[1];
   if (--front->duration > 0)
      return front->msg;

   /* Duration expired: remove root, replace with last leaf, sift down. */
   free(queue->tmp_msg);
   queue->tmp_msg = front->msg; /* transfer ownership; returned below */
   front->msg     = NULL;

   last            = queue->elems[--queue->ptr];
   queue->elems[1] = last;

   free(front->title);
   free(front);

   /* Restore heap if there are remaining elements. */
   if (queue->ptr > 1)
      heap_sift_down(queue->elems, queue->ptr - 1, 1);

   return queue->tmp_msg;
}

/**
 * msg_queue_extract:
 * @queue             : pointer to queue object
 * @queue_entry       : destination struct for the removed element
 *
 * Atomically removes the highest-priority message from the queue and
 * copies its fields into @queue_entry.  The heap is restored in
 * O(log n).
 *
 * Returns: false if the queue is empty or arguments are invalid,
 * otherwise true.
 **/
bool msg_queue_extract(msg_queue_t *queue, msg_queue_entry_t *queue_entry)
{
   struct queue_elem *front = NULL;
   struct queue_elem *last  = NULL;

   if (!queue || queue->ptr == 1 || !queue_entry)
      return false;

   front           = queue->elems[1];
   last            = queue->elems[--queue->ptr];
   queue->elems[1] = last;

   /* Copy fields to caller-owned struct. */
   queue_entry->duration  = front->duration;
   queue_entry->prio      = front->prio;
   queue_entry->icon      = front->icon;
   queue_entry->category  = front->category;
   queue_entry->msg[0]    = '\0';
   queue_entry->title[0]  = '\0';

   if (front->msg)
      strlcpy(queue_entry->msg,   front->msg,   sizeof(queue_entry->msg));
   if (front->title)
      strlcpy(queue_entry->title, front->title, sizeof(queue_entry->title));

   free(front->msg);
   free(front->title);
   free(front);

   /* Restore heap if there are remaining elements. */
   if (queue->ptr > 1)
      heap_sift_down(queue->elems, queue->ptr - 1, 1);

   return true;
}

/**
 * msg_queue_size:
 * @queue             : pointer to queue object
 *
 * Returns: number of messages currently in the queue.
 **/
size_t msg_queue_size(msg_queue_t *queue)
{
   if (!queue || queue->ptr <= 1)
      return 0;
   return queue->ptr - 1;
}
