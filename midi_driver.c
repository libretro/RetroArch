/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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

#include <boolean.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

#include "configuration.h"
#include "midi_driver.h"
#include "verbosity.h"

#define MIDI_DRIVER_BUF_SIZE 4096

static void *rarch_midi_drv_data;
static struct string_list *rarch_midi_drv_inputs;
static struct string_list *rarch_midi_drv_outputs;
static uint8_t *rarch_midi_drv_input_buffer;
static uint8_t *rarch_midi_drv_output_buffer;

static midi_event_t rarch_midi_drv_input_event;                    /* ptr alignment */
static midi_event_t rarch_midi_drv_output_event;                   /* ptr alignment */

static bool rarch_midi_drv_input_enabled;
static bool rarch_midi_drv_output_enabled;
static bool rarch_midi_drv_output_pending;

static void null_midi_free(void *p) { }
static void *null_midi_init(const char *input, const char *output) { return (void*)-1; }
static bool null_midi_get_avail_inputs(struct string_list *inputs) { union string_list_elem_attr attr = {0}; return string_list_append(inputs, "Null", attr); }
static bool null_midi_get_avail_outputs(struct string_list *outputs) { union string_list_elem_attr attr = {0}; return string_list_append(outputs, "Null", attr); }
static bool null_midi_set_input(void *p, const char *input) { return input == NULL || string_is_equal(input, "Null"); }
static bool null_midi_set_output(void *p, const char *output) { return output == NULL || string_is_equal(output, "Null"); }
static bool null_midi_read(void *p, midi_event_t *event) { return false; }
static bool null_midi_write(void *p, const midi_event_t *event) { return true; }
static bool null_midi_flush(void *p) { return true; }

static midi_driver_t midi_null = {
   "null",
   null_midi_get_avail_inputs,
   null_midi_get_avail_outputs,
   null_midi_init,
   null_midi_free,
   null_midi_set_input,
   null_midi_set_output,
   null_midi_read,
   null_midi_write,
   null_midi_flush
};

static midi_driver_t *midi_drv  = &midi_null;

midi_driver_t *midi_drivers[]   = {
#if defined(HAVE_ALSA) && !defined(HAVE_HAKCHI) && !defined(HAVE_SEGAM) && !defined(DINGUX)
   &midi_alsa,
#endif
#ifdef HAVE_WINMM
   &midi_winmm,
#endif
   &midi_null
};

static midi_driver_t *midi_driver_find_driver(const char *ident)
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(midi_drivers); ++i)
   {
      if (string_is_equal(midi_drivers[i]->ident, ident))
         return midi_drivers[i];
   }

   RARCH_ERR("[MIDI]: Unknown driver \"%s\", falling back to \"null\" driver.\n", ident);

   return &midi_null;
}

const void *midi_driver_find_handle(int index)
{
   if (index < 0 || index >= ARRAY_SIZE(midi_drivers))
      return NULL;

   return midi_drivers[index];
}

struct string_list *midi_driver_get_avail_inputs(void)
{
   return rarch_midi_drv_inputs;
}

struct string_list *midi_driver_get_avail_outputs(void)
{
   return rarch_midi_drv_outputs;
}

bool midi_driver_set_all_sounds_off(void)
{
   midi_event_t event;
   uint8_t i;
   uint8_t data[3]     = { 0xB0, 120, 0 };
   bool result         = true;

   if (!rarch_midi_drv_data || !rarch_midi_drv_output_enabled)
      return false;

   event.data       = data;
   event.data_size  = sizeof(data);
   event.delta_time = 0;

   for (i = 0; i < 16; ++i)
   {
      data[0] = 0xB0 | i;

      if (!midi_drv->write(rarch_midi_drv_data, &event))
         result = false;
   }

   if (!midi_drv->flush(rarch_midi_drv_data))
      result = false;

   if (!result)
      RARCH_ERR("[MIDI]: All sounds off failed.\n");

   return result;
}

bool midi_driver_set_volume(unsigned volume)
{
   midi_event_t event;
   uint8_t         data[8]     = {
      0xF0, 0x7F, 0x7F, 0x04, 0x01, 0, 0, 0xF7};

   if (!rarch_midi_drv_data || !rarch_midi_drv_output_enabled)
      return false;

   volume           = (unsigned)(163.83 * volume + 0.5);
   if (volume > 16383)
      volume        = 16383;

   data[5]          = (uint8_t)(volume & 0x7F);
   data[6]          = (uint8_t)(volume >> 7);

   event.data       = data;
   event.data_size  = sizeof(data);
   event.delta_time = 0;

   if (!midi_drv->write(rarch_midi_drv_data, &event))
   {
      RARCH_ERR("[MIDI]: Volume change failed.\n");
      return false;
   }

   return true;
}

static bool midi_driver_init_io_buffers(void)
{
   uint8_t *midi_drv_input_buffer  = (uint8_t*)malloc(MIDI_DRIVER_BUF_SIZE);
   uint8_t *midi_drv_output_buffer = (uint8_t*)malloc(MIDI_DRIVER_BUF_SIZE);

   if (!midi_drv_input_buffer || !midi_drv_output_buffer)
   {
      if (midi_drv_input_buffer)
         free(midi_drv_input_buffer);
      if (midi_drv_output_buffer)
         free(midi_drv_output_buffer);
      return false;
   }

   rarch_midi_drv_input_buffer           = midi_drv_input_buffer;
   rarch_midi_drv_output_buffer          = midi_drv_output_buffer;

   rarch_midi_drv_input_event.data       = midi_drv_input_buffer;
   rarch_midi_drv_input_event.data_size  = 0;

   rarch_midi_drv_output_event.data      = midi_drv_output_buffer;
   rarch_midi_drv_output_event.data_size = 0;

   return true;
}

void midi_driver_free(void)
{
   if (rarch_midi_drv_data)
   {
      midi_drv->free(rarch_midi_drv_data);
      rarch_midi_drv_data = NULL;
   }

   if (rarch_midi_drv_inputs)
   {
      string_list_free(rarch_midi_drv_inputs);
      rarch_midi_drv_inputs = NULL;
   }

   if (rarch_midi_drv_outputs)
   {
      string_list_free(rarch_midi_drv_outputs);
      rarch_midi_drv_outputs = NULL;
   }

   if (rarch_midi_drv_input_buffer)
   {
      free(rarch_midi_drv_input_buffer);
      rarch_midi_drv_input_buffer = NULL;
   }

   if (rarch_midi_drv_output_buffer)
   {
      free(rarch_midi_drv_output_buffer);
      rarch_midi_drv_output_buffer = NULL;
   }

   rarch_midi_drv_input_enabled  = false;
   rarch_midi_drv_output_enabled = false;
}

bool midi_driver_init(void *data)
{
   union string_list_elem_attr 
      attr                        = {0};
   bool ret                       = true;
   settings_t *settings           = (settings_t*)data;

   rarch_midi_drv_inputs          = string_list_new();
   rarch_midi_drv_outputs         = string_list_new();

   if (!rarch_midi_drv_inputs || !rarch_midi_drv_outputs)
      ret = false;
   else if (!string_list_append(rarch_midi_drv_inputs, "Off", attr) ||
            !string_list_append(rarch_midi_drv_outputs, "Off", attr))
      ret = false;
   else
   {
      char * input  = NULL;
      char * output = NULL;

      midi_drv      = midi_driver_find_driver(
            settings->arrays.midi_driver);

      if (strcmp(midi_drv->ident, settings->arrays.midi_driver))
      {
         configuration_set_string(settings,
               settings->arrays.midi_driver, midi_drv->ident);
      }

      if (!midi_drv->get_avail_inputs(rarch_midi_drv_inputs))
         ret = false;
      else if (!midi_drv->get_avail_outputs(rarch_midi_drv_outputs))
         ret = false;
      else
      {
         if (string_is_not_equal(settings->arrays.midi_input, "Off"))
         {
            if (string_list_find_elem(rarch_midi_drv_inputs, settings->arrays.midi_input))
               input = settings->arrays.midi_input;
            else
            {
               RARCH_WARN("[MIDI]: Input device \"%s\" unavailable.\n",
                     settings->arrays.midi_input);
               configuration_set_string(settings,
                     settings->arrays.midi_input, "Off");
            }
         }

         if (string_is_not_equal(settings->arrays.midi_output, "Off"))
         {
            if (string_list_find_elem(rarch_midi_drv_outputs, settings->arrays.midi_output))
               output = settings->arrays.midi_output;
            else
            {
               RARCH_WARN("[MIDI]: Output device \"%s\" unavailable.\n",
                     settings->arrays.midi_output);
               configuration_set_string(settings,
                     settings->arrays.midi_output, "Off");
            }
         }

         rarch_midi_drv_data = midi_drv->init(input, output);
         if (!rarch_midi_drv_data)
            ret = false;
         else
         {
            rarch_midi_drv_input_enabled  = (input  != NULL);
            rarch_midi_drv_output_enabled = (output != NULL);

            if (!midi_driver_init_io_buffers())
               ret = false;
            else
            {
               if (input)
                  RARCH_LOG("[MIDI]: Input device \"%s\".\n", input);

               if (output)
               {
                  RARCH_LOG("[MIDI]: Output device \"%s\".\n", output);
                  midi_driver_set_volume(settings->uints.midi_volume);
               }
            }
         }
      }
   }

   if (!ret)
   {
      midi_driver_free();
      RARCH_ERR("[MIDI]: Initialization failed.\n");
      return false;
   }
   return true;
}

bool midi_driver_set_input(const char *input)
{
   if (!rarch_midi_drv_data)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: midi_driver_set_input called on uninitialized driver.\n");
#endif
      return false;
   }

   if (string_is_equal(input, "Off"))
      input = NULL;

   if (!midi_drv->set_input(rarch_midi_drv_data, input))
   {
      if (input)
         RARCH_ERR("[MIDI]: Failed to change input device to \"%s\".\n", input);
      else
         RARCH_ERR("[MIDI]: Failed to disable input.\n");
      return false;
   }

   if (input)
      RARCH_LOG("[MIDI]: Input device changed to \"%s\".\n", input);
   else
      RARCH_LOG("[MIDI]: Input disabled.\n");

   rarch_midi_drv_input_enabled = input != NULL;

   return true;
}

bool midi_driver_set_output(void *settings_data, const char *output)
{
   settings_t        *settings = (settings_t*)settings_data; 

   if (!rarch_midi_drv_data)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: midi_driver_set_output called on uninitialized driver.\n");
#endif
      return false;
   }

   if (string_is_equal(output, "Off"))
      output = NULL;

   if (!midi_drv->set_output(rarch_midi_drv_data, output))
   {
      if (output)
         RARCH_ERR("[MIDI]: Failed to change output device to \"%s\".\n", output);
      else
         RARCH_ERR("[MIDI]: Failed to disable output.\n");
      return false;
   }

   if (output)
   {
      rarch_midi_drv_output_enabled = true;
      RARCH_LOG("[MIDI]: Output device changed to \"%s\".\n", output);

      midi_driver_set_volume(settings->uints.midi_volume);
   }
   else
   {
      rarch_midi_drv_output_enabled = false;
      RARCH_LOG("[MIDI]: Output disabled.\n");
   }

   return true;
}

bool midi_driver_input_enabled(void)
{
   return rarch_midi_drv_input_enabled;
}

bool midi_driver_output_enabled(void)
{
   return rarch_midi_drv_output_enabled;
}

bool midi_driver_read(uint8_t *byte)
{
   static int i;

   if (!rarch_midi_drv_data || !rarch_midi_drv_input_enabled || !byte)
   {
#ifdef DEBUG
      if (!rarch_midi_drv_data)
         RARCH_ERR("[MIDI]: midi_driver_read called on uninitialized driver.\n");
      else if (!rarch_midi_drv_input_enabled)
         RARCH_ERR("[MIDI]: midi_driver_read called when input is disabled.\n");
      else
         RARCH_ERR("[MIDI]: midi_driver_read called with null pointer.\n");
#endif
      return false;
   }

   if (i == rarch_midi_drv_input_event.data_size)
   {
      rarch_midi_drv_input_event.data_size = MIDI_DRIVER_BUF_SIZE;
      if (!midi_drv->read(rarch_midi_drv_data,
               &rarch_midi_drv_input_event))
      {
         rarch_midi_drv_input_event.data_size = i;
         return false;
      }

      i = 0;

#ifdef DEBUG
      if (rarch_midi_drv_input_event.data_size == 1)
         RARCH_LOG("[MIDI]: In [0x%02X].\n",
               rarch_midi_drv_input_event.data[0]);
      else if (rarch_midi_drv_input_event.data_size == 2)
         RARCH_LOG("[MIDI]: In [0x%02X, 0x%02X].\n",
               rarch_midi_drv_input_event.data[0],
               rarch_midi_drv_input_event.data[1]);
      else if (rarch_midi_drv_input_event.data_size == 3)
         RARCH_LOG("[MIDI]: In [0x%02X, 0x%02X, 0x%02X].\n",
               rarch_midi_drv_input_event.data[0],
               rarch_midi_drv_input_event.data[1],
               rarch_midi_drv_input_event.data[2]);
      else
         RARCH_LOG("[MIDI]: In [0x%02X, ...], size %u.\n",
               rarch_midi_drv_input_event.data[0],
               rarch_midi_drv_input_event.data_size);
#endif
   }

   *byte = rarch_midi_drv_input_event.data[i++];

   return true;
}

bool midi_driver_write(uint8_t byte, uint32_t delta_time)
{
   static int event_size;

   if (!rarch_midi_drv_data || !rarch_midi_drv_output_enabled)
   {
#ifdef DEBUG
      if (!rarch_midi_drv_data)
         RARCH_ERR("[MIDI]: midi_driver_write called on uninitialized driver.\n");
      else
         RARCH_ERR("[MIDI]: midi_driver_write called when output is disabled.\n");
#endif
      return false;
   }

   if (byte >= 0x80)
   {
      if (  rarch_midi_drv_output_event.data_size &&
            rarch_midi_drv_output_event.data[0] == 0xF0)
      {
         if (byte == 0xF7)
            event_size = (int)rarch_midi_drv_output_event.data_size + 1;
         else
         {
            if (!midi_drv->write(rarch_midi_drv_data,
                     &rarch_midi_drv_output_event))
               return false;

#ifdef DEBUG
            switch (rarch_midi_drv_output_event.data_size)
            {
               case 1:
                  RARCH_LOG("[MIDI]: Out [0x%02X].\n",
                        rarch_midi_drv_output_event.data[0]);
                  break;
               case 2:
                  RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X].\n",
                        rarch_midi_drv_output_event.data[0],
                        rarch_midi_drv_output_event.data[1]);
                  break;
               case 3:
                  RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X, 0x%02X].\n",
                        rarch_midi_drv_output_event.data[0],
                        rarch_midi_drv_output_event.data[1],
                        rarch_midi_drv_output_event.data[2]);
                  break;
               default:
                  RARCH_LOG("[MIDI]: Out [0x%02X, ...], size %u.\n",
                        rarch_midi_drv_output_event.data[0],
                        rarch_midi_drv_output_event.data_size);
                  break;
            }
#endif

            rarch_midi_drv_output_pending             = true;
            event_size                                = (int)midi_driver_get_event_size(byte);
            rarch_midi_drv_output_event.data_size     = 0;
            rarch_midi_drv_output_event.delta_time    = 0;
         }
      }
      else
      {
         event_size                                   = (int)midi_driver_get_event_size(byte);
         rarch_midi_drv_output_event.data_size        = 0;
         rarch_midi_drv_output_event.delta_time       = 0;
      }
   }

   if (rarch_midi_drv_output_event.data_size < MIDI_DRIVER_BUF_SIZE)
   {
      rarch_midi_drv_output_event.data[rarch_midi_drv_output_event.data_size] = byte;
      ++rarch_midi_drv_output_event.data_size;
      rarch_midi_drv_output_event.delta_time += delta_time;
   }
   else
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: Output event dropped.\n");
#endif
      return false;
   }

   if (rarch_midi_drv_output_event.data_size == event_size)
   {
      if (!midi_drv->write(rarch_midi_drv_data,
               &rarch_midi_drv_output_event))
         return false;

#ifdef DEBUG
      switch (rarch_midi_drv_output_event.data_size)
      {
         case 1:
            RARCH_LOG("[MIDI]: Out [0x%02X].\n",
                  rarch_midi_drv_output_event.data[0]);
            break;
         case 2:
            RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X].\n",
                  rarch_midi_drv_output_event.data[0],
                  rarch_midi_drv_output_event.data[1]);
            break;
         case 3:
            RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X, 0x%02X].\n",
                  rarch_midi_drv_output_event.data[0],
                  rarch_midi_drv_output_event.data[1],
                  rarch_midi_drv_output_event.data[2]);
            break;
         default:
            RARCH_LOG("[MIDI]: Out [0x%02X, ...], size %u.\n",
                  rarch_midi_drv_output_event.data[0],
                  rarch_midi_drv_output_event.data_size);
            break;
      }
#endif

      rarch_midi_drv_output_pending             = true;
      rarch_midi_drv_output_event.data_size     = 0;
      rarch_midi_drv_output_event.delta_time    = 0;
   }

   return true;
}

bool midi_driver_flush(void)
{
   if (!rarch_midi_drv_data)
      return false;

   if (rarch_midi_drv_output_pending)
      rarch_midi_drv_output_pending =
         !midi_drv->flush(rarch_midi_drv_data);

   return !rarch_midi_drv_output_pending;
}

size_t midi_driver_get_event_size(uint8_t status)
{
   static const uint8_t midi_drv_ev_sizes[128]                     =
   {
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      0, 2, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
   };

   if (status < 0x80)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: midi_driver_get_event_size called with invalid status.\n");
#endif
      return 0;
   }

   return midi_drv_ev_sizes[status - 0x80];
}
