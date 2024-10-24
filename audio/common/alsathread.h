/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 Daniel De Matteis
 *  Copyright (C) 2023 Jesse Talavera-Greenberg
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

#ifndef RETROARCH_ALSATHREAD_H
#define RETROARCH_ALSATHREAD_H

#include <alsa/asoundlib.h>
#include <boolean.h>
#include <queues/fifo_queue.h>
#include <rthreads/rthreads.h>
#include "alsa.h"

typedef struct alsa_thread_info
{
   snd_pcm_t *pcm;
   fifo_buffer_t *buffer;
   sthread_t *worker_thread;
   slock_t *fifo_lock;
   scond_t *cond;
   slock_t *cond_lock;
   alsa_stream_info_t stream_info;
   volatile bool thread_dead;
} alsa_thread_info_t;

void alsa_thread_free_info_members(alsa_thread_info_t *info);

#endif
