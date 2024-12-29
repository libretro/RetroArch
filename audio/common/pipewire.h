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

#include <pipewire/pipewire.h>

#define PW_RARCH_APPNAME                   "RetroArch"

/* String literals are part of the PipeWire specification */
#define PW_RARCH_MEDIA_TYPE_AUDIO          "Audio"
#define PW_RARCH_MEDIA_TYPE_VIDEO          "Video"
#define PW_RARCH_MEDIA_TYPE_MIDI           "Midi"
#define PW_RARCH_MEDIA_CATEGORY_PLAYBACK   "Playback"
#define PW_RARCH_MEDIA_CATEGORY_RECORD	    "Capture"
#define PW_RARCH_MEDIA_ROLE                "Game"

typedef struct pipewire_core
{
   struct pw_thread_loop *thread_loop;
   struct pw_context *ctx;
   struct pw_core *core;
   struct spa_hook core_listener;
   int last_seq, pending_seq;

   struct pw_registry *registry;
   struct spa_hook registry_listener;
   struct pw_client *client;
   struct spa_hook client_listener;

   bool nonblock;
   struct string_list *devicelist;
} pipewire_core_t;

size_t calc_frame_size(enum spa_audio_format fmt, uint32_t nchannels);

void set_position(uint32_t channels, uint32_t position[SPA_AUDIO_MAX_CHANNELS]);

void pipewire_wait_resync(pipewire_core_t *pipewire);

bool pipewire_set_active(struct pw_thread_loop *loop, struct pw_stream *stream, bool active);

bool pipewire_core_init(pipewire_core_t *pipewire, const char *loop_name);

#endif  /* _RETROARCH_PIPEWIRE */
