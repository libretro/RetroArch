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

bool msg_queue_initialize(msg_queue_t *queue, size_t len)
{
   struct queue_elem **elems = NULL;

   if (!queue)
      return false;

   if (!(elems = (struct queue_elem**)
            calloc(len + 1, sizeof(struct queue_elem*))))
      return false;

   queue->tmp_msg            = NULL;
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
   queue->tmp_msg = NULL;
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
 *                      3 seconds @ 60fps = 180 duration).
 *
 * Push a new message onto the queue.
 **/
void msg_queue_push(msg_queue_t *queue, const char *msg,
      unsigned prio, unsigned duration,
      char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   size_t tmp_ptr = 0;
   struct queue_elem *new_elem = NULL;

   if (!queue || queue->ptr >= queue->size)
      return;

   if (!(new_elem = (struct queue_elem*)malloc(sizeof(struct queue_elem))))
      return;

   new_elem->duration            = duration;
   new_elem->prio                = prio;
   new_elem->msg                 = msg   ? strdup(msg)   : NULL;
   new_elem->title               = title ? strdup(title) : NULL;
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
         free(queue->elems[i]->msg);
         free(queue->elems[i]->title);
         free(queue->elems[i]);
         queue->elems[i] = NULL;
      }
   }
   queue->ptr     = 1;
   free(queue->tmp_msg);
   queue->tmp_msg = NULL;
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
   struct queue_elem *front  = NULL, *last = NULL;
   size_t tmp_ptr = 1;

   /* Nothing in queue. */
   if (!queue || queue->ptr == 1)
      return NULL;

   front = (struct queue_elem*)queue->elems[1];
   front->duration--;
   if (front->duration > 0)
      return front->msg;

   free(queue->tmp_msg);
   queue->tmp_msg = front->msg;
   front->msg = NULL;

   last  = (struct queue_elem*)queue->elems[--queue->ptr];
   queue->elems[1] = last;
   free(front->title);
   free(front);

   for (;;)
   {
      struct queue_elem *parent = NULL;
      struct queue_elem *child  = NULL;
      size_t switch_index       = tmp_ptr;
      bool left                 = (tmp_ptr * 2 <= queue->ptr)
         && (queue->elems[tmp_ptr] < queue->elems[tmp_ptr * 2]);
      bool right                = (tmp_ptr * 2 + 1 <= queue->ptr)
         && (queue->elems[tmp_ptr] < queue->elems[tmp_ptr * 2 + 1]);

      if (!left && !right)
         break;

      if (left && !right)
         switch_index <<= 1;
      else if (right && !left)
         switch_index += switch_index + 1;
      else
      {
         if (queue->elems[tmp_ptr * 2]
               >= queue->elems[tmp_ptr * 2 + 1])
            switch_index <<= 1;
         else
            switch_index += switch_index + 1;
      }

      parent = (struct queue_elem*)queue->elems[tmp_ptr];
      child  = (struct queue_elem*)queue->elems[switch_index];
      queue->elems[tmp_ptr]      = child;
      queue->elems[switch_index] = parent;
      tmp_ptr                    = switch_index;
   }

   return queue->tmp_msg;
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
   struct queue_elem *front  = NULL, *last = NULL;
   size_t tmp_ptr = 1;

   /* Ensure arguments are valid and queue is not
    * empty */
   if (!queue || queue->ptr == 1 || !queue_entry)
      return false;

   front = (struct queue_elem*)queue->elems[1];
   last  = (struct queue_elem*)queue->elems[--queue->ptr];
   queue->elems[1] = last;

   /* Copy element parameters */
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

   /* Delete element */
   free(front->msg);
   free(front->title);
   free(front);

   for (;;)
   {
      struct queue_elem *parent = NULL;
      struct queue_elem *child  = NULL;
      size_t switch_index       = tmp_ptr;
      bool left                 = (tmp_ptr * 2 <= queue->ptr)
         && (queue->elems[tmp_ptr] < queue->elems[tmp_ptr * 2]);
      bool right                = (tmp_ptr * 2 + 1 <= queue->ptr)
         && (queue->elems[tmp_ptr] < queue->elems[tmp_ptr * 2 + 1]);

      if (!left && !right)
         break;

      if (left && !right)
         switch_index <<= 1;
      else if (right && !left)
         switch_index += switch_index + 1;
      else
      {
         if (queue->elems[tmp_ptr * 2]
               >= queue->elems[tmp_ptr * 2 + 1])
            switch_index <<= 1;
         else
            switch_index += switch_index + 1;
      }

      parent = (struct queue_elem*)queue->elems[tmp_ptr];
      child  = (struct queue_elem*)queue->elems[switch_index];
      queue->elems[tmp_ptr]      = child;
      queue->elems[switch_index] = parent;
      tmp_ptr                    = switch_index;
   }

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
