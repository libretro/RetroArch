/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdlib.h>

#include <compat/strl.h>
#include <retro_inline.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_PYTHON
#include "drivers_tracker/video_state_python.h"
#endif

#include "video_state_tracker.h"

#include "../input/input_driver.h"

#include "../verbosity.h"

struct state_tracker_internal
{
   char id[64];

   bool is_input;
   const uint16_t *input_ptr;
   const uint8_t *ptr;
#ifdef HAVE_PYTHON
   py_state_t *py;
#endif

   uint32_t addr;
   uint16_t mask;

   uint16_t equal;

   enum state_tracker_type type;

   uint32_t prev[2];
   int frame_count;
   int frame_count_prev;
   uint32_t old_value; 
   int transition_count;
};

struct state_tracker
{
   struct state_tracker_internal *info;
   unsigned info_elem;

   uint16_t input_state[2];

#ifdef HAVE_PYTHON
   py_state_t *py;
#endif
};

/**
 * state_tracker_init:
 * @info                         : State tracker info handle.
 *
 * Creates and initializes graphics state tracker.
 *
 * Returns: new state tracker handle if successful, otherwise NULL.
 **/
state_tracker_t* state_tracker_init(const struct state_tracker_info *info)
{
   unsigned i;
   struct state_tracker_internal *tracker_info = NULL;
   state_tracker_t                    *tracker = (state_tracker_t*)
      calloc(1, sizeof(*tracker));

   if (!tracker)
      return NULL;

#ifdef HAVE_PYTHON
   if (info->script)
   {
      tracker->py = py_state_new(info->script, info->script_is_file,
            info->script_class ? info->script_class : "GameAware");

      if (!tracker->py)
      {
         RARCH_ERR("Failed to initialize Python script.\n");
         goto error;
      }
   }
#endif

   tracker_info       = (struct state_tracker_internal*)
      calloc(info->info_elem, sizeof(struct state_tracker_internal));

   if (!tracker_info)
      goto error;

   tracker->info      = tracker_info;
   tracker->info_elem = info->info_elem;

   for (i = 0; i < info->info_elem; i++)
   {
      /* If we don't have a valid pointer. */
      static const uint8_t empty = 0;

      strlcpy(tracker->info[i].id, info->info[i].id,
            sizeof(tracker->info[i].id));

      tracker->info[i].addr  = info->info[i].addr;
      tracker->info[i].type  = info->info[i].type;
      tracker->info[i].mask  = (info->info[i].mask == 0) 
         ? 0xffff : info->info[i].mask;
      tracker->info[i].equal = info->info[i].equal;

#ifdef HAVE_PYTHON
      if (info->info[i].type == RARCH_STATE_PYTHON)
      {
         if (!tracker->py)
         {
            RARCH_ERR("Python semantic was requested, but Python tracker is not loaded.\n");

            free(tracker->info);
            goto error;
         }
         tracker->info[i].py = tracker->py;
      }
#endif

      switch (info->info[i].ram_type)
      {
         case RARCH_STATE_WRAM:
            tracker->info[i].ptr = info->wram ? info->wram : &empty;
            break;
         case RARCH_STATE_INPUT_SLOT1:
            tracker->info[i].input_ptr = &tracker->input_state[0];
            tracker->info[i].is_input = true;
            break;
         case RARCH_STATE_INPUT_SLOT2:
            tracker->info[i].input_ptr = &tracker->input_state[1];
            tracker->info[i].is_input = true;
            break;

         default:
            tracker->info[i].ptr = &empty;
      }
   }

   return tracker;

error:
   RARCH_ERR("Allocation of state tracker info failed.\n");
   free(tracker);
   return NULL;
}

/**
 * state_tracker_free:
 * @tracker                      : State tracker handle.
 *
 * Frees a state tracker handle.
 **/
void state_tracker_free(state_tracker_t *tracker)
{
   if (tracker)
   {
      free(tracker->info);
#ifdef HAVE_PYTHON
      py_state_free(tracker->py);
#endif
   }

   free(tracker);
}

static INLINE uint16_t state_tracker_fetch(
      const struct state_tracker_internal *info)
{
   uint16_t val = info->ptr[info->addr];

   if (info->is_input)
      val = *info->input_ptr;

   val &= info->mask;

   if (info->equal && val != info->equal)
      val = 0;

   return val;
}

static void state_tracker_update_element(
      struct state_tracker_uniform *uniform,
      struct state_tracker_internal *info,
      unsigned frame_count)
{
   uniform->id = info->id;

   switch (info->type)
   {
      case RARCH_STATE_CAPTURE:
         uniform->value = state_tracker_fetch(info);
         break;

      case RARCH_STATE_CAPTURE_PREV:
         if (info->prev[0] != state_tracker_fetch(info))
         {
            info->prev[1] = info->prev[0];
            info->prev[0] = state_tracker_fetch(info);
         }
         uniform->value = info->prev[1];
         break;

      case RARCH_STATE_TRANSITION:
         if (info->old_value != state_tracker_fetch(info))
         {
            info->old_value = state_tracker_fetch(info);
            info->frame_count = frame_count;
         }
         uniform->value = info->frame_count;
         break;

      case RARCH_STATE_TRANSITION_COUNT:
         if (info->old_value != state_tracker_fetch(info))
         {
            info->old_value = state_tracker_fetch(info);
            info->transition_count++;
         }
         uniform->value = info->transition_count;
         break;

      case RARCH_STATE_TRANSITION_PREV:
         if (info->old_value != state_tracker_fetch(info))
         {
            info->old_value = state_tracker_fetch(info);
            info->frame_count_prev = info->frame_count;
            info->frame_count = frame_count;
         }
         uniform->value = info->frame_count_prev;
         break;
      
#ifdef HAVE_PYTHON
      case RARCH_STATE_PYTHON:
         uniform->value = py_state_get(info->py, info->id, frame_count);
         break;
#endif
      
      default:
         break;
   }
}

void state_tracker_update_input(uint16_t *input1, uint16_t *input2);

/**
 * state_tracker_get_uniform:
 * @tracker                      : State tracker handle.
 * @uniforms                     : State tracker uniforms.
 * @elem                         : Amount of uniform elements.
 * @frame_count                  : Frame count.
 *
 * Calls state_tracker_update_input(), and updates each uniform
 * element accordingly.
 *
 * Returns: Amount of state elements (either equal to @elem
 * or equal to @tracker->info_eleme).
 **/
unsigned state_tracker_get_uniform(state_tracker_t *tracker,
      struct state_tracker_uniform *uniforms,
      unsigned elem, unsigned frame_count)
{
   unsigned i, elems = elem;
   
   if (tracker->info_elem < elem)
      elems = tracker->info_elem;

   state_tracker_update_input(&tracker->input_state[0], &tracker->input_state[1]);


   for (i = 0; i < elems; i++)
      state_tracker_update_element(
            &uniforms[i], &tracker->info[i], frame_count);

   return elems;
}
