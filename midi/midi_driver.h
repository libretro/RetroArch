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

#ifndef __MIDI_DRIVER__H
#define __MIDI_DRIVER__H

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

struct string_list;

typedef struct
{
   uint8_t *data;
   size_t data_size;
   uint32_t delta_time;
} midi_event_t;

typedef struct midi_driver
{
   const char *ident;

   bool (*get_avail_inputs)(struct string_list *inputs);
   bool (*get_avail_outputs)(struct string_list *outputs);

   void *(*init)(const char *input, const char *output);
   void (*free)(void *p);

   bool (*set_input)(void *p, const char *input);
   bool (*set_output)(void *p, const char *output);

   bool (*read)(void *p, midi_event_t *event);
   bool (*write)(void *p, const midi_event_t *event);
   bool (*flush)(void *p);
} midi_driver_t;

const void *midi_driver_find_handle(int index);
const char *midi_driver_find_ident(int index);

struct string_list *midi_driver_get_avail_inputs(void);
struct string_list *midi_driver_get_avail_outputs(void);

bool midi_driver_set_all_sounds_off(void);
bool midi_driver_set_volume(unsigned volume);

bool midi_driver_init(void);
void midi_driver_free(void);

bool midi_driver_set_input(const char *input);
bool midi_driver_set_output(const char *output);

bool midi_driver_input_enabled(void);
bool midi_driver_output_enabled(void);

bool midi_driver_read(uint8_t *byte);
bool midi_driver_write(uint8_t byte, uint32_t delta_time);
bool midi_driver_flush(void);

size_t midi_driver_get_event_size(uint8_t status);

RETRO_END_DECLS

#endif
