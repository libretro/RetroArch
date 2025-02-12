/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

/* Possible improvement list:
 * Add vid/pid to autoconf profile step, if autoconf matching needs testing
 * Multiple device autoconf profiles, with different analog/digital setup, analog buttons, extra buttons, unconfigured buttons...
 * Unimplemented functions (get_button, rumble)
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>

#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <formats/rjson.h>

#include "../../config.def.h"
#include "../../verbosity.h"
#include "../input_driver.h"
#include "../../tasks/tasks_internal.h"
#include "../../gfx/video_driver.h"

#define MAX_TEST_STEPS 200
#define NUM_BUTTONS 32
#ifndef MAX_AXIS
#define MAX_AXIS 10
#endif

#define JOYPAD_TEST_COMMAND_ADD_CONTROLLER          1
#define JOYPAD_TEST_COMMAND_REMOVE_CONTROLLER       2
#define JOYPAD_TEST_COMMAND_BUTTON_PRESS_FIRST     16
#define JOYPAD_TEST_COMMAND_BUTTON_PRESS_LAST      31
#define JOYPAD_TEST_COMMAND_BUTTON_RELEASE_FIRST   32
#define JOYPAD_TEST_COMMAND_BUTTON_RELEASE_LAST    47
#define JOYPAD_TEST_COMMAND_BUTTON_AXIS_FIRST    1000
#define JOYPAD_TEST_COMMAND_BUTTON_AXIS_LAST     JOYPAD_TEST_COMMAND_BUTTON_AXIS_FIRST+MAX_AXIS*DEFAULT_MAX_PADS

typedef struct
{
   char* name;
   uint32_t button_state;
   int32_t axis_state[MAX_AXIS];
} test_joypad_data;

static test_joypad_data test_joypads[MAX_USERS];

typedef struct
{
   unsigned frame;
   unsigned action;
   unsigned param_num;
   char param_str[256];
   bool handled;
} input_test_step_t;

static input_test_step_t input_test_steps[MAX_TEST_STEPS];

static uint32_t current_frame         = 0;
static uint32_t next_teststep_frame   = 0;
static unsigned current_test_step     = 0;
static unsigned last_test_step        = MAX_TEST_STEPS + 1;
static uint32_t input_state_validated = 0;
static uint32_t combo_state_validated = 0;
static bool     dump_state_blocked    = false;

/************************************/
/* JSON Helpers for test input file */
/************************************/

typedef struct
{
   unsigned *current_entry_uint_val;
   char **current_entry_str_val;
   unsigned frame;
   unsigned action;
   unsigned param_num;
   char *param_str;
} JTifJSONContext;

static bool JTifJSONObjectEndHandler(void* context)
{
   JTifJSONContext *pCtx = (JTifJSONContext*)context;

   /* Too long input is handled elsewhere, it should not lead to parse error */
   if (current_test_step >= MAX_TEST_STEPS)
      return true;

   /* Copy values read from JSON file + fill defaults */
   if (pCtx->frame == 0xffff)
      input_test_steps[current_test_step].frame  = input_test_steps[current_test_step-1].frame + 60;
   else
      input_test_steps[current_test_step].frame  = pCtx->frame;

   input_test_steps[current_test_step].action    = pCtx->action;
   input_test_steps[current_test_step].param_num = pCtx->param_num;
   input_test_steps[current_test_step].handled   = false;

   if (!string_is_empty(pCtx->param_str))
      strlcpy(
            input_test_steps[current_test_step].param_str, pCtx->param_str,
            sizeof(input_test_steps[current_test_step].param_str));
   else
      input_test_steps[current_test_step].param_str[0] = '\0';

   current_test_step++;
   last_test_step = current_test_step;
   pCtx->frame = 0xffff;
   return true;
}

static bool JTifJSONObjectMemberHandler(void* context, const char *pValue, size_t len)
{
   JTifJSONContext *pCtx = (JTifJSONContext*)context;

   /* something went wrong */
   if (pCtx->current_entry_str_val)
      return false;

   if (len)
   {
      if (string_is_equal(pValue, "frame"))
         pCtx->current_entry_uint_val = &pCtx->frame;
      else if (string_is_equal(pValue, "action"))
         pCtx->current_entry_uint_val = &pCtx->action;
      else if (string_is_equal(pValue, "param_num"))
         pCtx->current_entry_uint_val = &pCtx->param_num;
      else if (string_is_equal(pValue, "param_str"))
         pCtx->current_entry_str_val = &pCtx->param_str;
      /* ignore unknown members */
   }

   return true;
}

static bool JTifJSONNumberHandler(void* context, const char *pValue, size_t len)
{
   JTifJSONContext *pCtx = (JTifJSONContext*)context;

   if (pCtx->current_entry_uint_val && len && !string_is_empty(pValue))
      *pCtx->current_entry_uint_val = string_to_unsigned(pValue);
   /* ignore unknown members */

   pCtx->current_entry_uint_val = NULL;

   return true;
}

static bool JTifJSONStringHandler(void* context, const char *pValue, size_t len)
{
   JTifJSONContext *pCtx = (JTifJSONContext*)context;

   if (pCtx->current_entry_str_val && len && !string_is_empty(pValue))
   {
      if (*pCtx->current_entry_str_val)
         free(*pCtx->current_entry_str_val);

      *pCtx->current_entry_str_val = strdup(pValue);
   }
   /* ignore unknown members */

   pCtx->current_entry_str_val = NULL;

   return true;
}

/* Parses test input file referenced by file_path.
 * Does nothing if test input file does not exist. */
static bool input_test_file_read(const char* file_path)
{
   bool success            = false;
   JTifJSONContext context = {0};
   RFILE *file             = NULL;
   rjson_t* parser;

   /* Sanity check */
   if (    string_is_empty(file_path)
       || !path_is_valid(file_path)
      )
   {
      RARCH_DBG("[Test joypad driver]: No test input file supplied.\n");
      return false;
   }

   /* Attempt to open test input file */
   file = filestream_open(
         file_path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("[Test joypad driver]: Failed to open test input file: \"%s\".\n",
            file_path);
      return false;
   }

   /* Initialise JSON parser */
   if (!(parser = rjson_open_rfile(file)))
   {
      RARCH_ERR("[Test joypad driver]: Failed to create JSON parser.\n");
      goto end;
   }

   /* Configure parser */
   rjson_set_options(parser, RJSON_OPTION_ALLOW_UTF8BOM);

   /* Read file */
   if (rjson_parse(parser, &context,
         JTifJSONObjectMemberHandler,
         JTifJSONStringHandler,
         JTifJSONNumberHandler,
         NULL, JTifJSONObjectEndHandler, NULL, NULL, /* object/array handlers */
         NULL, NULL) /* unused boolean/null handlers */
         != RJSON_DONE)
   {
      if (rjson_get_source_context_len(parser))
      {
         RARCH_ERR(
               "[Test joypad driver]: Error parsing chunk of test input file: %s\n---snip---\n%.*s\n---snip---\n",
               file_path,
               rjson_get_source_context_len(parser),
               rjson_get_source_context_buf(parser));
      }
      RARCH_WARN(
            "[Test joypad driver]: Error parsing test input file: %s\n",
            file_path);
      RARCH_ERR(
            "[Test joypad driver]: Error: Invalid JSON at line %d, column %d - %s.\n",
            (int)rjson_get_source_line(parser),
            (int)rjson_get_source_column(parser),
            (*rjson_get_error(parser) ? rjson_get_error(parser) : "format error"));
   }

   /* Free parser */
   rjson_free(parser);

   success = true;
end:
   /* Clean up leftover strings */
   if (context.param_str)
      free(context.param_str);

   /* Close log file */
   filestream_close(file);

   if (last_test_step >= MAX_TEST_STEPS)
   {
      RARCH_WARN("[Test joypad driver]: too long test input json, maximum size: %d\n",MAX_TEST_STEPS);
   }
   for (current_test_step = 0; current_test_step < last_test_step; current_test_step++)
   {
      RARCH_DBG(
         "[Test joypad driver]: test step %02d read from file: frame %d, action %x, num %x, str %s\n",
         current_test_step,
         input_test_steps[current_test_step].frame,
         input_test_steps[current_test_step].action,
         input_test_steps[current_test_step].param_num,
         input_test_steps[current_test_step].param_str);
   }
   current_test_step = 0;
   return success;
}

/********************************/
/* Test input file handling end */
/********************************/

static const char *test_joypad_name(unsigned pad)
{
   if (pad >= MAX_USERS || string_is_empty(test_joypads[pad].name))
      return NULL;

   if (strstr(test_joypads[pad].name, ") "))
      return strstr(test_joypads[pad].name, ") ") + 2;
   else
      return test_joypads[pad].name;
}

static void test_joypad_autodetect_add(unsigned autoconf_pad)
{
   int vid = 0;
   int pid = 0;

   sscanf(strstr(test_joypads[autoconf_pad].name, "(") + 1, "%04x:%04x", &vid, &pid);
   RARCH_DBG("[Test input driver]: Autoconf vid/pid %x:%x\n",vid,pid);

   input_autoconfigure_connect(
         test_joypad_name(autoconf_pad),
         NULL,
         "test",
         autoconf_pad,
         vid,
         pid
         );
}

static void test_joypad_autodetect_remove(unsigned autoconf_pad)
{
   RARCH_DBG("[Test input driver]: Autoremove port %d\n", autoconf_pad);

   input_autoconfigure_disconnect(autoconf_pad, test_joypad_name(autoconf_pad));
}

static void *test_joypad_init(void *data)
{
   settings_t *settings = config_get_ptr();
   unsigned i;

   input_test_file_read(settings->paths.test_input_file_joypad);
   if (last_test_step > MAX_TEST_STEPS)
      last_test_step = 0;

   for(i=0; i<last_test_step; i++)
   {
      if (input_test_steps[i].frame > 0)
         continue;
      if (input_test_steps[i].action == JOYPAD_TEST_COMMAND_ADD_CONTROLLER)
      {
         test_joypads[input_test_steps[i].param_num].name = input_test_steps[i].param_str;
         test_joypad_autodetect_add(input_test_steps[i].param_num);
         input_test_steps[i].handled = true;
      }

   }
   return (void*)-1;
}

static int32_t test_joypad_button(unsigned port_num, uint16_t joykey)
{
   int16_t ret                          = 0;
   if (port_num >= DEFAULT_MAX_PADS)
      return 0;
   if (joykey < NUM_BUTTONS)
      return (BIT32_GET(test_joypads[port_num].button_state, joykey));

   return 0;
}

static int16_t test_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   /*RARCH_DBG("test_joypad_axis %d / %u\n",port_num, joyaxis);*/
   if (port_num >= DEFAULT_MAX_PADS)
      return 0;
   if (AXIS_NEG_GET(joyaxis) < MAX_AXIS)
   {
      /* Kernel returns values in range [-0x7fff, 0x7fff]. */
      int16_t val = test_joypads[port_num].axis_state[AXIS_NEG_GET(joyaxis)];
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < MAX_AXIS)
   {
      int16_t val = test_joypads[port_num].axis_state[AXIS_POS_GET(joyaxis)];
      if (val > 0)
         return val;
   }
   return 0;

}

static int16_t test_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{

   unsigned i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;

   if (port_idx < DEFAULT_MAX_PADS)
   {
	   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
	   {
		   /* Auto-binds are per joypad, not per user. */
		   const uint16_t joykey  = (binds[i].joykey != NO_BTN)
			   ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
		   /* Test input driver uses same button layout internally as RA, so no conversion is needed */
		   if (joykey != NO_BTN && (test_joypads[port_idx].button_state & (1 << i)))
			   ret |= ( 1 << i);
	   }
   }

   return ret;
}


static void test_joypad_poll(void)
{

   video_driver_state_t *video_st = video_state_get_ptr();
   uint64_t curr_frame            = video_st->frame_count;
   unsigned i;

   for (i=0; i<last_test_step; i++)
   {
      if (!input_test_steps[i].handled && curr_frame > input_test_steps[i].frame)
      {
         if (input_test_steps[i].action == JOYPAD_TEST_COMMAND_ADD_CONTROLLER)
         {
            test_joypads[input_test_steps[i].param_num].name = input_test_steps[i].param_str;
            test_joypad_autodetect_add(input_test_steps[i].param_num);
            input_test_steps[i].handled = true;
         }
         else if (input_test_steps[i].action == JOYPAD_TEST_COMMAND_REMOVE_CONTROLLER)
         {
            test_joypad_autodetect_remove(input_test_steps[i].param_num);
            input_test_steps[i].handled = true;
         }
         else if (   input_test_steps[i].action >= JOYPAD_TEST_COMMAND_BUTTON_PRESS_FIRST
                  && input_test_steps[i].action <= JOYPAD_TEST_COMMAND_BUTTON_PRESS_LAST)
         {
            unsigned targetpad = input_test_steps[i].action - JOYPAD_TEST_COMMAND_BUTTON_PRESS_FIRST;
            test_joypads[targetpad].button_state |= input_test_steps[i].param_num;
            input_test_steps[i].handled = true;
            RARCH_DBG(
               "[Test joypad driver]: Pressing device %d buttons %x, new state %x.\n",
               targetpad,input_test_steps[i].param_num,test_joypads[targetpad].button_state);
         }
         else if (   input_test_steps[i].action >= JOYPAD_TEST_COMMAND_BUTTON_RELEASE_FIRST
                  && input_test_steps[i].action <= JOYPAD_TEST_COMMAND_BUTTON_RELEASE_LAST)
         {
            unsigned targetpad = input_test_steps[i].action - JOYPAD_TEST_COMMAND_BUTTON_RELEASE_FIRST;
            test_joypads[targetpad].button_state &= ~input_test_steps[i].param_num;
            input_test_steps[i].handled = true;
            RARCH_DBG(
               "[Test joypad driver]: Releasing device %d buttons %x, new state %x.\n",
               targetpad,input_test_steps[i].param_num,test_joypads[targetpad].button_state);
         }
         else if (   input_test_steps[i].action >= JOYPAD_TEST_COMMAND_BUTTON_AXIS_FIRST
                  && input_test_steps[i].action <= JOYPAD_TEST_COMMAND_BUTTON_AXIS_LAST)
         {
            unsigned targetpad =
               (input_test_steps[i].action - JOYPAD_TEST_COMMAND_BUTTON_AXIS_FIRST) / MAX_AXIS;
            unsigned targetaxis =
               input_test_steps[i].action - JOYPAD_TEST_COMMAND_BUTTON_AXIS_FIRST - (targetpad*MAX_AXIS);
            if (targetpad < DEFAULT_MAX_PADS && targetaxis < MAX_AXIS)
               test_joypads[targetpad].axis_state[targetaxis] = (int16_t) input_test_steps[i].param_num;
            else
               RARCH_WARN(
                  "[Test joypad driver]: Decoded axis outside target range: action %d pad %d axis %d.\n",
                  input_test_steps[i].action, targetpad, targetaxis);

            input_test_steps[i].handled = true;
            RARCH_DBG(
               "[Test joypad driver]: Setting axis device %d axis %d value %d.\n",
               targetpad, targetaxis, (int16_t)input_test_steps[i].param_num);
         }
         else
         {
            input_test_steps[i].handled = true;
            RARCH_WARN(
               "[Test joypad driver]: Unrecognized action %d in step %d, skipping\n",
               input_test_steps[i].action,i);
         }

      }
   }
}

static bool test_joypad_query_pad(unsigned pad)
{
   return (pad < MAX_USERS);
}

static void test_joypad_destroy(void) { }

input_device_driver_t test_joypad = {
   test_joypad_init,
   test_joypad_query_pad,
   test_joypad_destroy,
   test_joypad_button,
   test_joypad_state,
   NULL, /* get_buttons */
   test_joypad_axis,
   test_joypad_poll,
   NULL, /* set_rumble */
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   test_joypad_name,
   "test",
};
