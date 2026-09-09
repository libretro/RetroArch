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

struct queue_elem
{
   char *msg;                          /* both point into the node's own block */
   char *title;
   unsigned duration;
   unsigned prio;
   enum message_queue_icon icon;
   enum message_queue_category category;
};
#include <compat/strl.h>
#include <compat/posix_string.h>

bool msg_queue_initialize(msg_queue_t *queue, size_t len)
{
   struct queue_elem **elems = NULL;

   /* The heap is len + 1 slots; SIZE_MAX would wrap that to nothing. */
   if (!queue || len == (size_t)-1)
      return false;

   if (!(elems = (struct queue_elem**)
            calloc(len + 1, sizeof(struct queue_elem*))))
      return false;

   queue->tmp                = NULL;
   queue->elems              = elems;
   queue->ptr                = 1;
   queue->size               = len + 1;

   return true;
}

/**
 * msg_queue_new:
 * @len              : maximum size of message
 *
 * Creates a message queue with maximum size different messages.
 *
 * Returns: NULL if allocation error, pointer to a message queue
 * if successful. Has to be freed manually.
 **/
msg_queue_t *msg_queue_new(size_t len)
{
   msg_queue_t *queue = (msg_queue_t*)malloc(sizeof(*queue));

   if (!msg_queue_initialize(queue, len))
   {
      if (queue)
         free(queue);
      return NULL;
   }

   return queue;
}

/**
 * msg_queue_free:
 * @queue             : pointer to queue object
 *
 * Frees message queue..
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
   queue->tmp = NULL;
   queue->ptr     = 0;
   queue->size    = 0;
   return true;
}

/**
 * msg_queue_push:
 * @queue             : pointer to queue object
 * @msg               : message to add to the queue
 * @prio              : priority level of the message
 * @duration          : how many times the message can be pulled
 *                      before it vanishes (E.g. show a message for
 *                      3 seconds @ 60fps = 180 duration). Zero is
 *                      taken as one.
 *
 * Push a new message onto the queue. Silent when the queue is full
 * or an allocation fails; msg_queue_try_push() says so instead.
 **/
bool msg_queue_try_push(msg_queue_t *queue, const char *msg,
      unsigned prio, unsigned duration,
      const char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   size_t tmp_ptr = 0;
   struct queue_elem *new_elem = NULL;

   if (!queue || queue->ptr >= queue->size)
      return false;

   /* The node and its strings are one block - the node first, the
    * strings after it - allocated before the heap is touched, so a
    * failure leaves the queue as it was, and freed as one. The node's
    * pointers point into the block; nothing frees them on their own. */
   {
      size_t msg_len   = msg   ? strlen(msg)   + 1 : 0;
      size_t title_len = title ? strlen(title) + 1 : 0;
      char  *block     = (char*)malloc(sizeof(struct queue_elem) + msg_len + title_len);
      if (!block)
         return false;
      new_elem        = (struct queue_elem*)block;
      block          += sizeof(struct queue_elem);
      new_elem->msg   = NULL;
      new_elem->title = NULL;
      if (msg)
      {
         new_elem->msg = block;
         memcpy(block, msg, msg_len);
         block += msg_len;
      }
      if (title)
      {
         new_elem->title = block;
         memcpy(block, title, title_len);
      }
   }

   /* A duration is pulls: a message asked for none at all is shown
    * once and gone, not kept until something replaces it. Nothing in
    * the tree asks for zero; a core's message shorter than a frame
    * rounds to it. */
   new_elem->duration            = duration ? duration : 1;
   new_elem->prio                = prio;
   new_elem->icon                = icon;
   new_elem->category            = category;

   queue->elems[queue->ptr]      = new_elem;

   tmp_ptr                       = queue->ptr++;

   while (tmp_ptr > 1)
   {
      struct queue_elem *parent  = queue->elems[tmp_ptr >> 1];
      struct queue_elem *child   = queue->elems[tmp_ptr];

      if (child->prio <= parent->prio)
         break;

      queue->elems[tmp_ptr >> 1] = child;
      queue->elems[tmp_ptr]      = parent;

      tmp_ptr >>= 1;
   }
   return true;
}

void msg_queue_push(msg_queue_t *queue, const char *msg,
      unsigned prio, unsigned duration,
      char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   msg_queue_try_push(queue, msg, prio, duration, title, icon, category);
}

/**
 * msg_queue_clear:
 * @queue             : pointer to queue object
 *
 * Clears out everything in the queue.
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
         free(queue->elems[i]);
         queue->elems[i] = NULL;
      }
   }
   queue->ptr     = 1;
   free(queue->tmp);
   queue->tmp = NULL;
}

/* Takes the front out of the heap: the last node moves into the hole
 * and sifts down on priority. The nodes are [1, ptr) with ptr the next
 * free index, so a child is a node only below ptr; the vacated index
 * is cleared. The caller owns the front it is handed. */
static struct queue_elem *msg_queue_remove_front(msg_queue_t *queue)
{
   struct queue_elem *front = queue->elems[1];
   size_t pos               = 1;

   queue->ptr--;
   queue->elems[1]          = queue->elems[queue->ptr];
   queue->elems[queue->ptr] = NULL;

   while ((pos << 1) < queue->ptr)
   {
      size_t child = pos << 1;
      size_t right = child + 1;
      struct queue_elem *tmp;
      if (     right < queue->ptr
            && queue->elems[right]->prio > queue->elems[child]->prio)
         child = right;
      if (queue->elems[pos]->prio >= queue->elems[child]->prio)
         break;
      tmp                 = queue->elems[pos];
      queue->elems[pos]   = queue->elems[child];
      queue->elems[child] = tmp;
      pos                 = child;
   }
   return front;
}

/**
 * msg_queue_pull:
 * @queue             : pointer to queue object
 *
 * Pulls highest priority message in queue.
 *
 * Returns: NULL if no message in queue, otherwise a string
 * containing the message.
 **/
const char *msg_queue_pull(msg_queue_t *queue)
{
   struct queue_elem *front  = NULL;

   /* Nothing in queue. */
   if (!queue || queue->ptr == 1)
      return NULL;

   front = queue->elems[1];
   front->duration--;
   if (front->duration > 0)
      return front->msg;

   /* The removed node is kept whole until the next pull or clear, so
    * the message it returns stays valid that long. */
   free(queue->tmp);
   front      = msg_queue_remove_front(queue);
   queue->tmp = front;

   return front->msg;
}

/**
 * msg_queue_extract:
 * @queue             : pointer to queue object
 * @queue_entry       : pointer to external queue entry struct
 *
 * Removes highest priority message from queue, copying
 * contents into queue_entry struct.
 *
 * Returns: false if no messages in queue, otherwise true
 **/
bool msg_queue_extract(msg_queue_t *queue, msg_queue_entry_t *queue_entry)
{
   struct queue_elem *front  = NULL;

   if (!queue || queue->ptr == 1 || !queue_entry)
      return false;

   front = msg_queue_remove_front(queue);

   queue_entry->duration = front->duration;
   queue_entry->prio     = front->prio;
   queue_entry->icon     = front->icon;
   queue_entry->category = front->category;
   queue_entry->msg[0]   = '\0';
   queue_entry->title[0] = '\0';

   if (front->msg)
      strlcpy(queue_entry->msg, front->msg, sizeof(queue_entry->msg));

   if (front->title)
      strlcpy(queue_entry->title, front->title, sizeof(queue_entry->title));

   free(front);

   return true;
}

/**
 * msg_queue_size:
 * @queue             : pointer to queue object
 *
 * Fetches number of messages in queue.
 *
 * Returns: Number of messages in queue.
 **/
size_t msg_queue_size(msg_queue_t *queue)
{
   if (!queue || queue->ptr <= 1)
      return 0;

   return queue->ptr - 1;
}
