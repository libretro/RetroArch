/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018 The RetroArch team
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

#include <string.h>

#include <libretro.h>
#include <string/stdstring.h>
#include <lists/string_list.h>

#include "../midi_driver.h"

static bool null_midi_get_avail_inputs(struct string_list *inputs)
{
   union string_list_elem_attr attr = {0};

   return string_list_append(inputs, "Null", attr);
}

static bool null_midi_get_avail_outputs(struct string_list *outputs)
{
   union string_list_elem_attr attr = {0};

   return string_list_append(outputs, "Null", attr);
}

static void *null_midi_init(const char *input, const char *output)
{
   (void)input;
   (void)output;

   return (void*)-1;
}

static void null_midi_free(void *p)
{
   (void)p;
}

static bool null_midi_set_input(void *p, const char *input)
{
   (void)p;

   return input == NULL || string_is_equal(input, "Null");
}

static bool null_midi_set_output(void *p, const char *output)
{
   (void)p;

   return output == NULL || string_is_equal(output, "Null");
}

static bool null_midi_read(void *p, midi_event_t *event)
{
   (void)p;
   (void)event;

   return false;
}

static bool null_midi_write(void *p, const midi_event_t *event)
{
   (void)p;
   (void)event;

   return true;
}

static bool null_midi_flush(void *p)
{
   (void)p;

   return true;
}

midi_driver_t midi_null = {
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
