/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __DRIVER__H
#define __DRIVER__H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define SNES_FAST_FORWARD_KEY 0x666 // Hurr, durr
void set_fast_forward_button(bool state);

struct snes_keybind
{
   int id;
   int key;
   int joykey;
};

typedef struct video_info
{
   int width;
   int height;
   bool fullscreen;
   bool vsync;
   bool force_aspect;
   bool smooth;
   int input_scale; // HQ2X => 2, HQ4X => 4, None => 1
} video_info_t;

typedef struct audio_driver
{
   void* (*init)(const char* device, int rate, int latency);
   ssize_t (*write)(void* data, const void* buf, size_t size);
   bool (*stop)(void* data);
   bool (*start)(void* data);
   void (*set_nonblock_state)(void* data, bool toggle); // Should we care about blocking in audio thread? Fast forwarding.
   void (*free)(void* data);
} audio_driver_t;

typedef struct input_driver
{
   void* (*init)(void);
   void (*poll)(void* data);
   int16_t (*input_state)(void* data, const struct snes_keybind **snes_keybinds, bool port, unsigned device, unsigned index, unsigned id);
   void (*free)(void* data);
} input_driver_t;

typedef struct video_driver
{
   void* (*init)(video_info_t *video, const input_driver_t **input); 
   // Should the video driver act as an input driver as well? :)
   bool (*frame)(void* data, const uint16_t* frame, int width, int height);
   void (*set_nonblock_state)(void* data, bool toggle); // Should we care about syncing to vblank? Fast forwarding.
   void (*free)(void* data);
} video_driver_t;


typedef struct driver
{
   const audio_driver_t *audio;
   const video_driver_t *video;
   const input_driver_t *input;
   void *audio_data;
   void *video_data;
   void *input_data;
} driver_t;

#endif
