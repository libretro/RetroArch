/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2024 Viachaslau Khalikin
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

#include "pipewire.h"

#include <pipewire/pipewire.h>

#include <retro_assert.h>

#include "verbosity.h"

size_t calc_frame_size(enum spa_audio_format fmt, uint32_t nchannels)
{
   uint32_t sample_size = 1;
   switch (fmt)
   {
      case SPA_AUDIO_FORMAT_S8:
      case SPA_AUDIO_FORMAT_U8:
         sample_size = 1;
         break;
      case SPA_AUDIO_FORMAT_S16_BE:
      case SPA_AUDIO_FORMAT_S16_LE:
      case SPA_AUDIO_FORMAT_U16_BE:
      case SPA_AUDIO_FORMAT_U16_LE:
         sample_size = 2;
         break;
      case SPA_AUDIO_FORMAT_S32_BE:
      case SPA_AUDIO_FORMAT_S32_LE:
      case SPA_AUDIO_FORMAT_U32_BE:
      case SPA_AUDIO_FORMAT_U32_LE:
      case SPA_AUDIO_FORMAT_F32_BE:
      case SPA_AUDIO_FORMAT_F32_LE:
         sample_size = 4;
         break;
      default:
         RARCH_ERR("[PipeWire]: Bad spa_audio_format %d\n", fmt);
         break;
   }
   return sample_size * nchannels;
}

void set_position(uint32_t channels, uint32_t position[SPA_AUDIO_MAX_CHANNELS])
{
   memcpy(position, (uint32_t[SPA_AUDIO_MAX_CHANNELS]) { SPA_AUDIO_CHANNEL_UNKNOWN, },
         sizeof(uint32_t) * SPA_AUDIO_MAX_CHANNELS);

   switch (channels)
   {
      case 8:
         position[6] = SPA_AUDIO_CHANNEL_SL;
         position[7] = SPA_AUDIO_CHANNEL_SR;
         /* fallthrough */
      case 6:
         position[2] = SPA_AUDIO_CHANNEL_FC;
         position[3] = SPA_AUDIO_CHANNEL_LFE;
         position[4] = SPA_AUDIO_CHANNEL_RL;
         position[5] = SPA_AUDIO_CHANNEL_RR;
         /* fallthrough */
      case 2:
         position[0] = SPA_AUDIO_CHANNEL_FL;
         position[1] = SPA_AUDIO_CHANNEL_FR;
         break;
      case 1:
         position[0] = SPA_AUDIO_CHANNEL_MONO;
         break;
      default:
         RARCH_ERR("[PipeWire]: Internal error: unsupported channel count %d\n", channels);
   }
}

int pipewire_wait_resync(pipewire_core_t *pipewire)
{
   int res;
   retro_assert(pipewire != NULL);

   pipewire->pending_seq = pw_core_sync(pipewire->core, PW_ID_CORE, pipewire->pending_seq);

   for (;;)
   {
      pw_thread_loop_wait(pipewire->thread_loop);

      res = pipewire->error;
      if (res < 0)
      {
         pipewire->error = 0;
         return res;
      }
      if (pipewire->pending_seq == pipewire->last_seq)
         break;
   }
   return 0;
}

bool pipewire_set_active(pipewire_core_t *pipewire, pipewire_device_handle_t *device, bool active)
{
   RARCH_LOG("[PipeWire]: %s.\n", active? "Unpausing": "Pausing");

   pw_thread_loop_lock(pipewire->thread_loop);
   pw_stream_set_active(device->stream, active);
   pw_thread_loop_wait(pipewire->thread_loop);
   pw_thread_loop_unlock(pipewire->thread_loop);

   return device->is_paused != active;
}
