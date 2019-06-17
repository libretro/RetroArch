/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Higor Euripedes
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#ifndef TASKS_HANDLER_AUDIO_MIXER_H
#define TASKS_HANDLER_AUDIO_MIXER_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>

#include <queues/task_queue.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../retroarch.h"

RETRO_BEGIN_DECLS

bool task_audio_mixer_load_handler(retro_task_t *task);

bool task_push_audio_mixer_load_and_play(
      const char *fullpath, retro_task_callback_t cb, void *user_data,
      bool system,
      enum audio_mixer_slot_selection_type slot_selection_type,
      int slot_selection_idx);

bool task_push_audio_mixer_load(
      const char *fullpath, retro_task_callback_t cb, void *user_data,
      bool system,
      enum audio_mixer_slot_selection_type slot_selection_type,
      int slot_selection_idx);

RETRO_END_DECLS

#endif
