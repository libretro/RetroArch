/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (message_queue.h).
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

#ifndef __LIBRETRO_SDK_MSG_QUEUE_H
#define __LIBRETRO_SDK_MSG_QUEUE_H

#include <stddef.h>

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

enum message_queue_icon
{
   MESSAGE_QUEUE_ICON_DEFAULT = 0 /* default icon is tied to category */
};

enum message_queue_category
{
   MESSAGE_QUEUE_CATEGORY_INFO = 0,
   MESSAGE_QUEUE_CATEGORY_ERROR,
   MESSAGE_QUEUE_CATEGORY_WARNING,
   MESSAGE_QUEUE_CATEGORY_SUCCESS
};

typedef struct queue_elem
{
   char *msg;
   char *title;
   unsigned duration;
   unsigned prio;
   enum message_queue_icon icon;
   enum message_queue_category category;
} queue_elem_t;

typedef struct msg_queue
{
   char *tmp_msg;
   queue_elem_t **elems;
   size_t ptr;
   size_t size;
} msg_queue_t;

typedef struct
{
   unsigned duration;
   unsigned prio;
   enum message_queue_icon icon;
   enum message_queue_category category;
   char msg[1024];
   char title[1024];
} msg_queue_entry_t;

/**
 * msg_queue_new:
 * @len               : maximum size of message
 *
 * Creates a message queue with maximum size different messages.
 *
 * Returns: NULL if allocation error, pointer to a message queue
 * if successful. Has to be freed manually.
 **/
msg_queue_t *msg_queue_new(size_t len);

bool msg_queue_initialize(msg_queue_t *queue, size_t len);

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
      enum message_queue_icon icon, enum message_queue_category category);

/**
 * msg_queue_pull:
 * @queue             : pointer to queue object
 *
 * Pulls highest priority message in queue.
 *
 * Returns: NULL if no message in queue, otherwise a string
 * containing the message.
 **/
const char *msg_queue_pull(msg_queue_t *queue);

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
bool msg_queue_extract(msg_queue_t *queue, msg_queue_entry_t *queue_entry);

/**
 * msg_queue_size:
 * @queue             : pointer to queue object
 *
 * Fetches number of messages in queue.
 *
 * Returns: Number of messages in queue.
 **/
size_t msg_queue_size(msg_queue_t *queue);

/**
 * msg_queue_clear:
 * @queue             : pointer to queue object
 *
 * Clears out everything in the queue.
 **/
void msg_queue_clear(msg_queue_t *queue);

/**
 * msg_queue_free:
 * @queue             : pointer to queue object
 *
 * Frees message queue..
 **/
void msg_queue_free(msg_queue_t *queue);

bool msg_queue_deinitialize(msg_queue_t *queue);

RETRO_END_DECLS

#endif
