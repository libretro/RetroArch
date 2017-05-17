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

#include <file/nbio.h>
#include <audio/audio_mixer.h>
#include <streams/file_stream.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>

#include "../audio/audio_driver.h"

#include "../file_path_special.h"
#include "../verbosity.h"

#include "tasks_internal.h"

struct audio_mixer_handle
{
   enum audio_mixer_type type;
   char path[4095];
};

static void audio_mixer_stopped(audio_mixer_sound_t *sound, unsigned reason)
{
   if (reason != AUDIO_MIXER_SOUND_REPEATED)
   {
      audio_mixer_destroy(sound);
      audio_set_bool(AUDIO_ACTION_MIXER, false);
   }
}

static void task_audio_mixer_load_free(retro_task_t *task)
{
   nbio_handle_t       *nbio  = task ? (nbio_handle_t*)task->state : NULL;

   if (nbio)
   {
      free(nbio);
   }
}

static int cb_nbio_audio_mixer_load(void *data, size_t len)
{
   void *ptr                       = NULL;
   audio_mixer_sound_t *handle     = NULL;
   nbio_handle_t *nbio             = (nbio_handle_t*)data; 
   struct audio_mixer_handle *image = nbio ? 
      (struct audio_mixer_handle*)nbio->data : NULL;

   ptr                             = nbio_get_ptr(nbio->handle, &len);
   
   image->handle = audio_mixer_load_ogg(ptr, len);

   audio_mixer_play(handle, true, 1.0f, audio_mixer_stopped);

   nbio->is_finished               = true;


   return 0;
}

void task_audio_mixer_handle_upload(void *task_data,
      void *user_data, const char *err)
{
   audio_set_bool(AUDIO_ACTION_MIXER, true);
}

bool task_push_audio_mixer_load(const char *fullpath, retro_task_callback_t cb, void *user_data)
{
   nbio_handle_t             *nbio    = NULL;
   struct audio_mixer_handle   *image = NULL;
   retro_task_t                   *t  = (retro_task_t*)calloc(1, sizeof(*t));

   if (!t)
      goto error_msg;

   nbio               = (nbio_handle_t*)calloc(1, sizeof(*nbio));

   if (!nbio)
      goto error;

   strlcpy(nbio->path, fullpath, sizeof(nbio->path));

   image              = (struct audio_mixer_handle*)calloc(1, sizeof(*image));   
   if (!image)
      goto error;

   strlcpy(image->path, fullpath, sizeof(image->path));

   nbio->type         = NBIO_TYPE_NONE;
   image->type        = AUDIO_MIXER_TYPE_NONE;

   if (strstr(fullpath, file_path_str(FILE_PATH_WAV_EXTENSION)))
   {
      image->type     = AUDIO_MIXER_TYPE_WAV;
   }
   else if (strstr(fullpath, file_path_str(FILE_PATH_OGG_EXTENSION)))
   {
      image->type     = AUDIO_MIXER_TYPE_OGG;
   }

   nbio->data         = (struct audio_mixer_handle*)image;
   nbio->is_finished  = false;
   nbio->cb           = &cb_nbio_audio_mixer_load;
   nbio->status       = NBIO_STATUS_INIT;

   t->state           = nbio;
   t->handler         = task_file_load_handler;
   t->cleanup         = task_audio_mixer_load_free;
   t->callback        = task_audio_mixer_handle_upload;
   t->user_data       = user_data;

   task_queue_push(t);

   return true;

error:
   free(t);

error_msg:
   RARCH_ERR("[audio mixer load] Failed to open '%s': %s.\n",
         fullpath, strerror(errno));

   return false;
}
