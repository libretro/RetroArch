/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __SSNES_MSG_QUEUE_H
#define __SSNES_MSG_QUEUE_H

#include <stddef.h>

typedef struct msg_queue msg_queue_t;

// Creates a message queue with maximum size different messages. Returns NULL if allocation error.
msg_queue_t *msg_queue_new(size_t size);

// Higher prio is... higher prio :) Duration is how many times a message can be pulled from queue before it vanishes. (E.g. show a message for 3 seconds @ 60fps = 180 duration). 

void msg_queue_push(msg_queue_t *queue, const char *msg, unsigned prio, unsigned duration);

// Pulls highest prio message in queue. Returns NULL if no message in queue.
const char *msg_queue_pull(msg_queue_t *queue);

void msg_queue_free(msg_queue_t *queue);

#endif
