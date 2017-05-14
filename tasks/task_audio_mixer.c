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
#include <compat/strl.h>
#include <retro_miscellaneous.h>

#include "../file_path_special.h"
#include "../verbosity.h"

#include "tasks_internal.h"

struct nbio_audio_mixer_handle
{
   enum nbio_type type;
   bool is_finished;
   void *handle;
};

static void task_audio_mixer_cleanup(nbio_handle_t *nbio)
{
   struct nbio_audio_mixer_handle *image = (struct nbio_audio_mixer_handle*)nbio->data;

   if (nbio->data)
      free(nbio->data);
   nbio_free(nbio->handle);
   nbio->data        = NULL;
   nbio->handle      = NULL;
}

static void task_audio_mixer_load_free(retro_task_t *task)
{
   nbio_handle_t       *nbio  = task ? (nbio_handle_t*)task->state : NULL;

   if (nbio)
   {
      task_audio_mixer_cleanup(nbio);
      free(nbio);
   }
}

static int cb_nbio_audio_wav_loaded(void *data, size_t len)
{
   nbio_handle_t *nbio             = (nbio_handle_t*)data; 
   struct nbio_audio_mixer_handle *image = nbio ? 
      (struct nbio_audio_mixer_handle*)nbio->data : NULL;
   void *ptr                       = nbio_get_ptr(nbio->handle, &len);
   audio_mixer_sound_t *audmix     = audio_mixer_load_wav(ptr, len);

   if (!audmix ||
       !audio_mixer_play(audmix, true, 0.0f, NULL)
      )
   {
      task_audio_mixer_cleanup(nbio);
      return -1;
   }

   free(ptr);

   image->is_finished              = true;
   nbio->is_finished               = true;

   return 0;
}

static int cb_nbio_audio_ogg_loaded(void *data, size_t len)
{
   nbio_handle_t *nbio             = (nbio_handle_t*)data; 
   struct nbio_audio_mixer_handle 
      *image                       = nbio ? 
      (struct nbio_audio_mixer_handle*)nbio->data : NULL;

   void *ptr                       = nbio_get_ptr(nbio->handle, &len);
   audio_mixer_sound_t *audmix     = audio_mixer_load_ogg(ptr, len);

   if (!audmix ||
       !audio_mixer_play(audmix, true, 0.0f, NULL)
      )
   {
      task_audio_mixer_cleanup(nbio);
      return -1;
   }

   image->is_finished              = true;
   nbio->is_finished               = true;

   return 0;
}

bool task_audio_mixer_load_handler(retro_task_t *task)
{
   unsigned i;
   nbio_handle_t            *nbio  = (nbio_handle_t*)task->state;
   struct nbio_audio_mixer_handle *image = (struct nbio_audio_mixer_handle*)nbio->data;

   if (     nbio->is_finished
         && (image && image->is_finished )
         && (!task_get_cancelled(task)))
      return false;

   return true;
}

bool task_push_audio_mixer_load(const char *fullpath, retro_task_callback_t cb, void *user_data)
{
   nbio_handle_t             *nbio   = NULL;
   struct nbio_audio_mixer_handle   *image = NULL;
   retro_task_t                   *t = (retro_task_t*)calloc(1, sizeof(*t));

   if (!t)
      goto error_msg;

   nbio = (nbio_handle_t*)calloc(1, sizeof(*nbio));

   if (!nbio)
      goto error;

   strlcpy(nbio->path, fullpath, sizeof(nbio->path));

   image              = (struct nbio_audio_mixer_handle*)calloc(1, sizeof(*image));   
   if (!image)
      goto error;

   nbio->type         = NBIO_TYPE_NONE;
   image->type        = NBIO_TYPE_NONE;

   if (strstr(fullpath, file_path_str(FILE_PATH_WAV_EXTENSION)))
   {
      nbio->type      = NBIO_TYPE_WAV;
      image->type     = NBIO_TYPE_WAV;
      nbio->cb        = &cb_nbio_audio_wav_loaded;
   }
   else if (strstr(fullpath, file_path_str(FILE_PATH_OGG_EXTENSION)))
   {
      nbio->type      = NBIO_TYPE_OGG;
      image->type     = NBIO_TYPE_OGG;
      nbio->cb        = &cb_nbio_audio_ogg_loaded;
   }

   nbio->data         = (struct nbio_audio_mixer_handle*)image;
   nbio->is_finished  = false;
   nbio->status       = NBIO_STATUS_INIT;

   t->state           = nbio;
   t->handler         = task_file_load_handler;
   t->cleanup         = task_audio_mixer_load_free;
   t->callback        = cb;
   t->user_data       = user_data;

   task_queue_push(t);

   return true;

error:
   task_audio_mixer_load_free(t);
   free(t);
   if (nbio)
      free(nbio);

error_msg:
   RARCH_ERR("[audio mixer load] Failed to open '%s': %s.\n",
         fullpath, strerror(errno));

   return false;
}
