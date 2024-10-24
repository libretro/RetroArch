/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2023 - Daniel De Matteis
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

#ifndef __RUNAHEAD_H
#define __RUNAHEAD_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "core.h"

#define MAX_RUNAHEAD_FRAMES 12

typedef void *(*constructor_t)(void);
typedef void  (*destructor_t )(void*);

typedef struct my_list_t
{
   void **data;
   constructor_t constructor;
   destructor_t destructor;
   int capacity;
   int size;
} my_list;

typedef struct preemptive_frames_data
{
   /* Savestate buffer */
   void* buffer[MAX_RUNAHEAD_FRAMES];
   size_t state_size;

   /* Frame count since buffer init/reset */
   uint64_t frame_count;

   /* Mask of analog states requested */
   uint32_t analog_mask[MAX_USERS];

   /* Input states. Replays triggered on changes */
   int16_t joypad_state[MAX_USERS];
   int16_t analog_state[MAX_USERS][20];
   int16_t ptrdev_state[MAX_USERS][4];

   /* Pointing device requested */
   uint8_t ptr_dev_needed[MAX_USERS];
   /* Device ID of ptrdev_state */
   uint8_t ptr_dev_polled[MAX_USERS];
   /* Buffer indexes for replays */
   uint8_t start_ptr;
   uint8_t replay_ptr;
   /* Number of latency frames to remove */
   uint8_t frames;
} preempt_t;

RETRO_BEGIN_DECLS

typedef bool(*runahead_load_state_function)(const void*, size_t);

void runahead_run(
      void *data,
      int runahead_count,
      bool runahead_hide_warnings,
      bool use_secondary);

void runahead_clear_variables(void *data);

void runahead_remember_controller_port_device(void *data,
      long port, long device);
void runahead_clear_controller_port_map(void *data);

void runahead_set_load_content_info(
      void *data,
      const retro_ctx_load_content_info_t *ctx);

void runahead_secondary_core_destroy(void *data);

bool preempt_init(void *data);
void preempt_deinit(void *data);

void preempt_run(preempt_t *preempt, void *data);

RETRO_END_DECLS

#endif
