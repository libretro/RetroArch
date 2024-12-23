/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2024 The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Found- ation, either version 3 of the License, or (at your option) any later
 * version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 * RetroArch. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RETROARCH_PIPEWIRE
#define _RETROARCH_PIPEWIRE

#include <stdint.h>

#include <spa/param/audio/format-utils.h>
#include <spa/utils/ringbuffer.h>


#define RINGBUFFER_SIZE (1u << 22)
#define RINGBUFFER_MASK (RINGBUFFER_SIZE - 1)

typedef struct pipewire
{
   struct pw_thread_loop *thread_loop;
   struct pw_context *context;
   struct pw_core *core;
   struct spa_hook core_listener;
   int last_seq, pending_seq, error;

   struct pw_registry *registry;
   struct spa_hook registry_listener;
   struct pw_client *client;
   struct spa_hook client_listener;

   bool nonblock;
   struct string_list *devicelist;
} pipewire_core_t;

typedef struct pipewire_device_handle
{
   pipewire_core_t *pw;

   struct pw_stream *stream;
   struct spa_hook stream_listener;
   struct spa_audio_info_raw info;
   uint32_t highwater_mark;
   uint32_t frame_size;
   uint32_t req;
   struct spa_ringbuffer ring;
   uint8_t buffer[RINGBUFFER_SIZE];

   bool is_paused;
} pipewire_device_handle_t;

size_t calc_frame_size(enum spa_audio_format fmt, uint32_t nchannels);

void set_position(uint32_t channels, uint32_t position[SPA_AUDIO_MAX_CHANNELS]);

int pipewire_wait_resync(pipewire_core_t *pipewire);

bool pipewire_set_active(pipewire_core_t *pipewire, pipewire_device_handle_t *device, bool active);

#endif  /* _RETROARCH_PIPEWIRE */
