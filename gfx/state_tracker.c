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

#include "state_tracker.h"
#include <stdlib.h>
#include "../compat/strl.h"
#include "../general.h"
#include "../libretro.h"

#ifdef HAVE_PYTHON
#include "py_state/py_state.h"
#endif

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

state_tracker_t* state_tracker_init(const struct state_tracker_info *info)
{
   unsigned i;
   state_tracker_t *tracker = (state_tracker_t*)calloc(1, sizeof(*tracker));
   if (!tracker)
      return NULL;

#ifdef HAVE_PYTHON
   if (info->script)
   {
      tracker->py = py_state_new(info->script, info->script_is_file, info->script_class ? info->script_class : "GameAware");
      if (!tracker->py)
      {
         free(tracker);
         RARCH_ERR("Failed to init Python script.\n");
         return NULL;
      }
   }
#endif

   tracker->info = (struct state_tracker_internal*)calloc(info->info_elem, sizeof(struct state_tracker_internal));
   tracker->info_elem = info->info_elem;

   for (i = 0; i < info->info_elem; i++)
   {
      strlcpy(tracker->info[i].id, info->info[i].id, sizeof(tracker->info[i].id));
      tracker->info[i].addr  = info->info[i].addr;
      tracker->info[i].type  = info->info[i].type;
      tracker->info[i].mask  = (info->info[i].mask == 0) ? 0xffff : info->info[i].mask;
      tracker->info[i].equal = info->info[i].equal;

#ifdef HAVE_PYTHON
      if (info->info[i].type == RARCH_STATE_PYTHON)
      {
         if (!tracker->py)
         {
            free(tracker->info);
            free(tracker);
            RARCH_ERR("Python semantic was requested, but Python tracker is not loaded.\n");
            return NULL;
         }
         tracker->info[i].py = tracker->py;
      }
#endif

      // If we don't have a valid pointer.
      static const uint8_t empty = 0;

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
}

void state_tracker_free(state_tracker_t *tracker)
{
   free(tracker->info);
#ifdef HAVE_PYTHON
   py_state_free(tracker->py);
#endif
   free(tracker);
}

static inline uint16_t fetch(const struct state_tracker_internal *info)
{
   uint16_t val = 0;
   if (info->is_input)
      val = *info->input_ptr;
   else
      val = info->ptr[info->addr];

   val &= info->mask;

   if (info->equal && val != info->equal)
      val = 0;

   return val;
}

static void update_element(
      struct state_tracker_uniform *uniform,
      struct state_tracker_internal *info,
      unsigned frame_count)
{
   uniform->id = info->id;

   switch (info->type)
   {
      case RARCH_STATE_CAPTURE:
         uniform->value = fetch(info);
         break;

      case RARCH_STATE_CAPTURE_PREV:
         if (info->prev[0] != fetch(info))
         {
            info->prev[1] = info->prev[0];
            info->prev[0] = fetch(info);
         }
         uniform->value = info->prev[1];
         break;

      case RARCH_STATE_TRANSITION:
         if (info->old_value != fetch(info))
         {
            info->old_value = fetch(info);
            info->frame_count = frame_count;
         }
         uniform->value = info->frame_count;
         break;

      case RARCH_STATE_TRANSITION_COUNT:
         if (info->old_value != fetch(info))
         {
            info->old_value = fetch(info);
            info->transition_count++;
         }
         uniform->value = info->transition_count;
         break;

      case RARCH_STATE_TRANSITION_PREV:
         if (info->old_value != fetch(info))
         {
            info->old_value = fetch(info);
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

// Updates 16-bit input in same format as SNES itself.
static void update_input(state_tracker_t *tracker)
{
   unsigned i;
   if (driver.input == NULL)
      return;

   static const unsigned buttons[] = {
      RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_L,
      RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_RIGHT,
      RETRO_DEVICE_ID_JOYPAD_LEFT,
      RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_UP,
      RETRO_DEVICE_ID_JOYPAD_START,
      RETRO_DEVICE_ID_JOYPAD_SELECT,
      RETRO_DEVICE_ID_JOYPAD_Y,
      RETRO_DEVICE_ID_JOYPAD_B,
   };

   // Only bind for up to two players for now.
   static const struct retro_keybind *binds[2] = {
      g_settings.input.binds[0],
      g_settings.input.binds[1],
   };

   uint16_t state[2] = {0};
   for (i = 4; i < 16; i++)
   {
      state[0] |= (input_input_state_func(binds, 0, RETRO_DEVICE_JOYPAD, 0, buttons[i - 4]) ? 1 : 0) << i;
      state[1] |= (input_input_state_func(binds, 1, RETRO_DEVICE_JOYPAD, 0, buttons[i - 4]) ? 1 : 0) << i;
   }

   for (i = 0; i < 2; i++)
      tracker->input_state[i] = state[i];
}

unsigned state_get_uniform(state_tracker_t *tracker, struct state_tracker_uniform *uniforms, unsigned elem, unsigned frame_count)
{
   unsigned i, elems;
   elems = tracker->info_elem < elem ? tracker->info_elem : elem;

   update_input(tracker);

   for (i = 0; i < elems; i++)
      update_element(&uniforms[i], &tracker->info[i], frame_count);

   return elems;
}

