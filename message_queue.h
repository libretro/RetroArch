/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __RARCH_MSG_QUEUE_H
#define __RARCH_MSG_QUEUE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msg_queue msg_queue_t;

/**
 * msg_queue_new:
 * @size              : maximum size of message
 *
 * Creates a message queue with maximum size different messages.
 *
 * Returns: NULL if allocation error, pointer to a message queue
 * if successful. Has to be freed manually.
 **/
msg_queue_t *msg_queue_new(size_t size);

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
      unsigned prio, unsigned duration);

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

#ifdef __cplusplus
}
#endif

#endif
