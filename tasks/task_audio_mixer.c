/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include "tasks_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <audio/audio_mixer.h>
#include <streams/file_stream.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>

#include "../audio/audio_driver.h"

#include "../file_path_special.h"
#include "../verbosity.h"

#include "tasks_internal.h"

struct nbio_audio_mixer_handle
{
   enum audio_mixer_type type;
   char path[4095];
};

static void audio_mixer_stopped(audio_mixer_sound_t *sound, unsigned reason)
{
   if (sound)
      audio_mixer_destroy(sound);
   audio_set_bool(AUDIO_ACTION_MIXER, false);
}

static void task_audio_mixer_load_handler(retro_task_t *task)
{
   unsigned i;
   void *buffer                          = NULL;
   ssize_t size                          = 0;
   audio_mixer_sound_t *handle           = NULL;
   struct nbio_audio_mixer_handle *image = (struct nbio_audio_mixer_handle*)task->state;

   if (filestream_read_file(image->path, &buffer, &size) == 0)
   {
      task_set_finished(task, true);
      return;
   }

   switch (image->type)
   {
      case AUDIO_MIXER_TYPE_WAV:
         handle                          = audio_mixer_load_wav(buffer, size);
         break;
      case AUDIO_MIXER_TYPE_OGG:
         handle                          = audio_mixer_load_ogg(buffer, size);
         break;
      case AUDIO_MIXER_TYPE_NONE:
         break;
   }

   if (handle)
      audio_mixer_play(handle, true, 1.0f, audio_mixer_stopped);

   free(buffer);

   audio_set_bool(AUDIO_ACTION_MIXER, true);

   task_set_progress(task, 100);
#if 0
   task_set_title(task, strdup(msg_hash_to_str(MSG_WIFI_SCAN_COMPLETE)));
#endif
   task_set_finished(task, true);
}

bool task_push_audio_mixer_load(const char *fullpath, retro_task_callback_t cb, void *user_data)
{
   struct nbio_audio_mixer_handle   *image = NULL;
   retro_task_t                   *t = (retro_task_t*)calloc(1, sizeof(*t));

   if (!t)
      goto error_msg;

   image              = (struct nbio_audio_mixer_handle*)calloc(1, sizeof(*image));   
   if (!image)
      goto error;

   strlcpy(image->path, fullpath, sizeof(image->path));

   image->type        = AUDIO_MIXER_TYPE_NONE;

   if (strstr(fullpath, file_path_str(FILE_PATH_WAV_EXTENSION)))
   {
      image->type     = AUDIO_MIXER_TYPE_WAV;
   }
   else if (strstr(fullpath, file_path_str(FILE_PATH_OGG_EXTENSION)))
   {
      image->type     = AUDIO_MIXER_TYPE_OGG;
   }

   t->state           = image;
   t->handler         = task_audio_mixer_load_handler;
   t->callback        = NULL;

   task_queue_push(t);

   return true;

error:
   free(t);

error_msg:
   RARCH_ERR("[audio mixer load] Failed to open '%s': %s.\n",
         fullpath, strerror(errno));

   return false;
}
