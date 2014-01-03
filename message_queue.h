/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msg_queue msg_queue_t;

// Creates a message queue with maximum size different messages. Returns NULL if allocation error.
msg_queue_t *msg_queue_new(size_t size);

// Higher prio is... higher prio :) Duration is how many times a message can be pulled from queue before it vanishes. (E.g. show a message for 3 seconds @ 60fps = 180 duration). 
void msg_queue_push(msg_queue_t *queue, const char *msg, unsigned prio, unsigned duration);

// Pulls highest prio message in queue. Returns NULL if no message in queue.
const char *msg_queue_pull(msg_queue_t *queue);

// Clear out everything in queue.
void msg_queue_clear(msg_queue_t *queue);

void msg_queue_free(msg_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif
