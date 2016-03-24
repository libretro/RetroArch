/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "input_keyboard.h"

#include "../general.h"
#include "../system.h"

struct input_keyboard_line
{
   char *buffer;
   size_t ptr;
   size_t size;

   /** Line complete callback. 
    * Calls back after return is 
    * pressed with the completed line.
    * Line can be NULL.
    **/
   input_keyboard_line_complete_t cb;
   void *userdata;
};

static input_keyboard_line_t *g_keyboard_line;

static input_keyboard_press_t g_keyboard_press_cb;

static void *g_keyboard_press_data;

static void input_keyboard_line_toggle_osk(bool enable)
{
   settings_t *settings = config_get_ptr();

   if (!settings->osk.enable)
      return;

   if (enable)
      input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_SET_LINEFEED_ENABLED, NULL);
   else
      input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_UNSET_LINEFEED_ENABLED, NULL);
}

/**
 * input_keyboard_line_free:
 * @state                    : Input keyboard line handle.
 *
 * Frees input keyboard line handle.
 **/
static void input_keyboard_line_free(input_keyboard_line_t *state)
{
   if (!state)
      return;

   free(state->buffer);
   free(state);

   input_keyboard_line_toggle_osk(false);
}

/**
 * input_keyboard_line_new:
 * @userdata                 : Userdata.
 * @cb                       : Callback function.
 *
 * Creates and initializes input keyboard line handle.
 * Also sets callback function for keyboard line handle
 * to provided callback @cb.
 *
 * Returns: keyboard handle on success, otherwise NULL.
 **/
static input_keyboard_line_t *input_keyboard_line_new(void *userdata,
      input_keyboard_line_complete_t cb)
{
   input_keyboard_line_t *state = (input_keyboard_line_t*)
      calloc(1, sizeof(*state));
   if (!state)
      return NULL;

   state->cb       = cb;
   state->userdata = userdata;

   input_keyboard_line_toggle_osk(true);

   return state;
}

/**
 * input_keyboard_line_event:
 * @state                    : Input keyboard line handle.
 * @character                : Inputted character.
 *
 * Called on every keyboard character event.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
static bool input_keyboard_line_event(
      input_keyboard_line_t *state, uint32_t character)
{
   char c = character >= 128 ? '?' : character;
   /* Treat extended chars as ? as we cannot support 
    * printable characters for unicode stuff. */

   if (c == '\r' || c == '\n')
   {
      state->cb(state->userdata, state->buffer);
      return true;
   }

   if (c == '\b' || c == '\x7f') /* 0x7f is ASCII for del */
   {
      if (state->ptr)
      {
         memmove(state->buffer + state->ptr - 1,
               state->buffer + state->ptr,
               state->size - state->ptr + 1);
         state->ptr--;
         state->size--;
      }
   }
   else if (isprint((int)c))
   {
      /* Handle left/right here when suitable */

      char *newbuf = (char*)
         realloc(state->buffer, state->size + 2);
      if (!newbuf)
         return false;

      memmove(newbuf + state->ptr + 1,
            newbuf + state->ptr,
            state->size - state->ptr + 1);
      newbuf[state->ptr] = c;
      state->ptr++;
      state->size++;
      newbuf[state->size] = '\0';

      state->buffer = newbuf;
   }

   return false;
}

/**
 * input_keyboard_start_line:
 * @userdata                 : Userdata.
 * @cb                       : Line complete callback function.
 *
 * Sets function pointer for keyboard line handle.
 *
 * The underlying buffer can be reallocated at any time 
 * (or be NULL), but the pointer to it remains constant 
 * throughout the objects lifetime.
 *
 * Returns: underlying buffer of the keyboard line.
 **/
const char **input_keyboard_start_line(void *userdata,
      input_keyboard_line_complete_t cb)
{
   input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_LINE_FREE, NULL);

   g_keyboard_line = input_keyboard_line_new(userdata, cb);

   /* While reading keyboard line input, we have to block all hotkeys. */
   input_driver_keyboard_mapping_set_block(true);

   return (const char**)&g_keyboard_line->buffer;
}

/**
 * input_keyboard_event:
 * @down                     : Keycode was pressed down?
 * @code                     : Keycode.
 * @character                : Character inputted.
 * @mod                      : TODO/FIXME: ???
 *
 * Keyboard event utils. Called by drivers when keyboard events are fired.
 * This interfaces with the global system driver struct and libretro callbacks.
 **/
void input_keyboard_event(bool down, unsigned code,
      uint32_t character, uint16_t mod, unsigned device)
{
   static bool deferred_wait_keys;

   if (deferred_wait_keys)
   {
      if (down)
         return;

      input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_CANCEL_WAIT_KEYS, NULL);
      deferred_wait_keys = false;
   }
   else if (g_keyboard_press_cb)
   {
      if (!down || code == RETROK_UNKNOWN)
         return;
      if (g_keyboard_press_cb(g_keyboard_press_data, code))
         return;
      deferred_wait_keys = true;
   }
   else if (g_keyboard_line)
   {
      if (!down)
         return;

      switch (device)
      {
         case RETRO_DEVICE_POINTER:
            if (!input_keyboard_line_event(g_keyboard_line,
                     (code != 0x12d) ? (char)code : character))
               return;
            break;
         default:
            if (!input_keyboard_line_event(g_keyboard_line, character))
               return;
            break;
      }

      /* Line is complete, can free it now. */
      input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_LINE_FREE, NULL);

      /* Unblock all hotkeys. */
      input_driver_keyboard_mapping_set_block(false);
   }
   else
   {
      retro_keyboard_event_t *key_event = NULL;
      runloop_ctl(RUNLOOP_CTL_KEY_EVENT_GET, &key_event);

      if (key_event && *key_event)
         (*key_event)(down, code, character, mod);
   }
}

bool input_keyboard_ctl(enum rarch_input_keyboard_ctl_state state, void *data)
{
   static bool input_driver_keyboard_linefeed_enable = false;

   switch (state)
   {
      case RARCH_INPUT_KEYBOARD_CTL_LINE_FREE:
         if (g_keyboard_line)
            input_keyboard_line_free(g_keyboard_line);
         g_keyboard_line = NULL;
         break;
      case RARCH_INPUT_KEYBOARD_CTL_START_WAIT_KEYS:
         {
            input_keyboard_ctx_wait_t *keys = (input_keyboard_ctx_wait_t*)data;

            if (!keys)
               return false;

            g_keyboard_press_cb   = keys->cb;
            g_keyboard_press_data = keys->userdata;
         }

         /* While waiting for input, we have to block all hotkeys. */
         input_driver_keyboard_mapping_set_block(true);
         break;
      case RARCH_INPUT_KEYBOARD_CTL_CANCEL_WAIT_KEYS:
         g_keyboard_press_cb   = NULL;
         g_keyboard_press_data = NULL;
         input_driver_keyboard_mapping_set_block(false);
         break;
      case RARCH_INPUT_KEYBOARD_CTL_DESTROY:
         input_driver_keyboard_linefeed_enable = false;
         break;
      case RARCH_INPUT_KEYBOARD_CTL_SET_LINEFEED_ENABLED:
         input_driver_keyboard_linefeed_enable = true;
         break;
      case RARCH_INPUT_KEYBOARD_CTL_UNSET_LINEFEED_ENABLED:
         input_driver_keyboard_linefeed_enable = false;
         break;
      case RARCH_INPUT_KEYBOARD_CTL_IS_LINEFEED_ENABLED:
         return input_driver_keyboard_linefeed_enable;
      case RARCH_INPUT_KEYBOARD_CTL_NONE:
      default:
         break;
   }

   return true;
}
