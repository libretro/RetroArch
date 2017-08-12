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

#include "../audio/audio_driver.h"

#include "../file_path_special.h"
#include "../verbosity.h"

#include "tasks_internal.h"

struct audio_mixer_handle
{
   nbio_buf_t *buffer;
   bool copy_data_over;
   bool is_finished;
   enum audio_mixer_type type;
   char path[4095];
};

static void task_audio_mixer_load_free(retro_task_t *task)
{
   nbio_handle_t       *nbio        = (nbio_handle_t*)task->state;
   struct audio_mixer_handle *image = (struct audio_mixer_handle*)nbio->data;

   if (image)
   {
      if (image->buffer)
         free(image->buffer);
   }

   if (nbio->data)
      free(nbio->data);
   nbio_free(nbio->handle);
   free(nbio);
}

static int cb_nbio_audio_mixer_load(void *data, size_t len)
{
   nbio_handle_t *nbio             = (nbio_handle_t*)data; 
   struct audio_mixer_handle *image= (struct audio_mixer_handle*)nbio->data;
   void *ptr                       = nbio_get_ptr(nbio->handle, &len);
   nbio_buf_t *buffer              = (nbio_buf_t*)calloc(1, sizeof(*image->buffer));

   if (!buffer)
      return -1;

   image->buffer                   = buffer;
   image->buffer->buf              = ptr;
   image->buffer->bufsize          = len;
   image->copy_data_over           = true;
   nbio->is_finished               = true;

   return 0;
}

static void task_audio_mixer_handle_upload_ogg(void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   nbio_buf_t             *img = (nbio_buf_t*)task_data;

   if (!img)
      return;

   params.volume               = 1.0f;
   params.type                 = AUDIO_MIXER_TYPE_OGG;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;

   audio_driver_mixer_add_stream(&params);

   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_mod(void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   nbio_buf_t             *img = (nbio_buf_t*)task_data;

   if (!img)
      return;

   params.volume               = 1.0f;
   params.type                 = AUDIO_MIXER_TYPE_MOD;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;

   audio_driver_mixer_add_stream(&params);

   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_wav(void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   nbio_buf_t *img = (nbio_buf_t*)task_data;

   if (!img)
      return;

   params.volume               = 1.0f;
   params.type                 = AUDIO_MIXER_TYPE_WAV;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;

   audio_driver_mixer_add_stream(&params);

   free(img);
   free(user_data);
}

bool task_audio_mixer_load_handler(retro_task_t *task)
{
   nbio_handle_t             *nbio  = (nbio_handle_t*)task->state;
   struct audio_mixer_handle *image = (struct audio_mixer_handle*)nbio->data;

   if (       
         nbio->is_finished 
         && (image && !image->is_finished)
         && (image->copy_data_over)
         && (!task_get_cancelled(task)))
   {
      nbio_buf_t *img = (nbio_buf_t*)calloc(1, sizeof(*img));

      if (img)
      {
         img->buf     = image->buffer->buf;
         img->bufsize = image->buffer->bufsize;
      }

      task_set_data(task, img);

      image->copy_data_over = false;
      image->is_finished    = true;


      return false;
   }

   return true;
}

bool task_push_audio_mixer_load(const char *fullpath, retro_task_callback_t cb, void *user_data)
{
   nbio_handle_t             *nbio    = NULL;
   struct audio_mixer_handle   *image = NULL;
   retro_task_t                   *t  = (retro_task_t*)calloc(1, sizeof(*t));

   if (!t)
      goto error;

   nbio               = (nbio_handle_t*)calloc(1, sizeof(*nbio));

   if (!nbio)
      goto error;

   strlcpy(nbio->path, fullpath, sizeof(nbio->path));

   image              = (struct audio_mixer_handle*)calloc(1, sizeof(*image));   
   if (!image)
      goto error;

   image->is_finished = false;

   strlcpy(image->path, fullpath, sizeof(image->path));

   nbio->type         = NBIO_TYPE_NONE;
   image->type        = AUDIO_MIXER_TYPE_NONE;

   if (strstr(fullpath, file_path_str(FILE_PATH_WAV_EXTENSION)))
   {
      image->type     = AUDIO_MIXER_TYPE_WAV;
      nbio->type      = NBIO_TYPE_WAV;
      t->callback     = task_audio_mixer_handle_upload_wav;
   }
   else if (strstr(fullpath, file_path_str(FILE_PATH_OGG_EXTENSION)))
   {
      image->type     = AUDIO_MIXER_TYPE_OGG;
      nbio->type      = NBIO_TYPE_OGG;
      t->callback     = task_audio_mixer_handle_upload_ogg;
   }
   else if (	strstr(fullpath, file_path_str(FILE_PATH_MOD_EXTENSION)) ||
		strstr(fullpath, file_path_str(FILE_PATH_S3M_EXTENSION)) ||
		strstr(fullpath, file_path_str(FILE_PATH_XM_EXTENSION)))
   {
      image->type     = AUDIO_MIXER_TYPE_MOD;
      nbio->type      = NBIO_TYPE_MOD;
      t->callback     = task_audio_mixer_handle_upload_mod;
   }

   nbio->data         = (struct audio_mixer_handle*)image;
   nbio->is_finished  = false;
   nbio->cb           = &cb_nbio_audio_mixer_load;
   nbio->status       = NBIO_STATUS_INIT;

   t->state           = nbio;
   t->handler         = task_file_load_handler;
   t->cleanup         = task_audio_mixer_load_free;
   t->user_data       = user_data;

   task_queue_push(t);

   return true;

error:
   if (nbio)
   {
      if (nbio->data)
         free(nbio->data);
      nbio_free(nbio->handle);
      free(nbio);
   }
   if (t)
      free(t);

   RARCH_ERR("[audio mixer load] Failed to open '%s': %s.\n",
         fullpath, strerror(errno));

   return false;
}
