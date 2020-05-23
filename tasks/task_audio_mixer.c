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
#include <file/file_path.h>
#include <audio/audio_mixer.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>
#include <queues/task_queue.h>

#include "../file_path_special.h"
#include "../retroarch.h"
#include "../verbosity.h"

#include "task_file_transfer.h"
#include "tasks_internal.h"

struct audio_mixer_userdata
{
   enum audio_mixer_stream_type stream_type;
   enum audio_mixer_slot_selection_type slot_selection_type;
   unsigned slot_selection_idx;
};

struct audio_mixer_handle
{
   nbio_buf_t *buffer;
   bool copy_data_over;
   bool is_finished;
   enum audio_mixer_type type;
   char path[4095];
   retro_task_callback_t cb;
};

static void task_audio_mixer_load_free(retro_task_t *task)
{
   nbio_handle_t       *nbio        = (nbio_handle_t*)task->state;
   struct audio_mixer_handle *mixer = (struct audio_mixer_handle*)nbio->data;

   if (mixer)
   {
      if (mixer->buffer)
      {
         if (mixer->buffer->path)
            free(mixer->buffer->path);
         free(mixer->buffer);
      }

      if (mixer->cb)
         mixer->cb(task, NULL, NULL, NULL);
   }

   if (!string_is_empty(nbio->path))
      free(nbio->path);
   if (nbio->data)
      free(nbio->data);
   nbio_free(nbio->handle);
   free(nbio);
}

static int cb_nbio_audio_mixer_load(void *data, size_t len)
{
   nbio_handle_t *nbio             = (nbio_handle_t*)data;
   struct audio_mixer_handle *mixer= (struct audio_mixer_handle*)nbio->data;
   void *ptr                       = nbio_get_ptr(nbio->handle, &len);
   nbio_buf_t *buffer              = (nbio_buf_t*)calloc(1, sizeof(*mixer->buffer));

   if (!buffer)
      return -1;

   mixer->buffer                   = buffer;
   mixer->buffer->buf              = ptr;
   mixer->buffer->bufsize          = (unsigned)len;
   mixer->copy_data_over           = true;
   nbio->is_finished               = true;

   return 0;
}

static void task_audio_mixer_handle_upload_ogg(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   nbio_buf_t             *img = (nbio_buf_t*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
      return;

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_OGG;
   params.state                = AUDIO_STREAM_STATE_STOPPED;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;
   params.basename             = !string_is_empty(img->path) ? strdup(path_basename(img->path)) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->path)
      free(img->path);
   if (params.basename != NULL)
      free(params.basename);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_ogg_and_play(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   nbio_buf_t             *img = (nbio_buf_t*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;

   if (!img || !user)
      return;

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_OGG;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;
   params.basename             = !string_is_empty(img->path) ? strdup(path_basename(img->path)) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->path)
      free(img->path);
   if (params.basename != NULL)
      free(params.basename);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_flac(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   nbio_buf_t             *img = (nbio_buf_t*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;

   if (!img || !user)
      return;

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_FLAC;
   params.state                = AUDIO_STREAM_STATE_STOPPED;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;
   params.basename             = !string_is_empty(img->path) ? strdup(path_basename(img->path)) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->path)
      free(img->path);
   if (params.basename != NULL)
      free(params.basename);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_flac_and_play(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   nbio_buf_t             *img = (nbio_buf_t*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;

   if (!img || !user)
      return;

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_FLAC;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;
   params.basename             = !string_is_empty(img->path) ? strdup(path_basename(img->path)) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->path)
      free(img->path);
   if (params.basename != NULL)
      free(params.basename);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_mp3(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   nbio_buf_t             *img = (nbio_buf_t*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;

   if (!img || !user)
      return;

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_MP3;
   params.state                = AUDIO_STREAM_STATE_STOPPED;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;
   params.basename             = !string_is_empty(img->path) ? strdup(path_basename(img->path)) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->path)
      free(img->path);
   if (params.basename != NULL)
      free(params.basename);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_mp3_and_play(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   nbio_buf_t             *img = (nbio_buf_t*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;

   if (!img || !user)
      return;

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_MP3;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;
   params.basename             = !string_is_empty(img->path) ? strdup(path_basename(img->path)) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->path)
      free(img->path);
   if (params.basename != NULL)
      free(params.basename);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_mod(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   nbio_buf_t             *img = (nbio_buf_t*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;

   if (!img || !user)
      return;

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_MOD;
   params.state                = AUDIO_STREAM_STATE_STOPPED;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;
   params.basename             = !string_is_empty(img->path) ? strdup(path_basename(img->path)) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->path)
      free(img->path);
   if (params.basename != NULL)
      free(params.basename);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_mod_and_play(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   nbio_buf_t             *img = (nbio_buf_t*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;

   if (!img || !user)
      return;

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_MOD;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;
   params.basename             = !string_is_empty(img->path) ? strdup(path_basename(img->path)) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->path)
      free(img->path);
   if (params.basename != NULL)
      free(params.basename);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_wav(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   nbio_buf_t *img = (nbio_buf_t*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;

   if (!img || !user)
      return;

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_WAV;
   params.state                = AUDIO_STREAM_STATE_STOPPED;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;
   params.basename             = !string_is_empty(img->path) ? strdup(path_basename(img->path)) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->path)
      free(img->path);
   if (params.basename != NULL)
      free(params.basename);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_wav_and_play(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   nbio_buf_t *img = (nbio_buf_t*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;

   if (!img || !user)
      return;

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_WAV;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->buf;
   params.bufsize              = img->bufsize;
   params.cb                   = NULL;
   params.basename             = !string_is_empty(img->path) ? strdup(path_basename(img->path)) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->path)
      free(img->path);
   if (params.basename != NULL)
      free(params.basename);
   free(img);
   free(user_data);
}

bool task_audio_mixer_load_handler(retro_task_t *task)
{
   nbio_handle_t             *nbio  = (nbio_handle_t*)task->state;
   struct audio_mixer_handle *mixer = (struct audio_mixer_handle*)nbio->data;

   if (
         nbio->is_finished
         && (mixer && !mixer->is_finished)
         && (mixer->copy_data_over)
         && (!task_get_cancelled(task)))
   {
      nbio_buf_t *img = (nbio_buf_t*)calloc(1, sizeof(*img));

      if (img)
      {
         img->buf     = mixer->buffer->buf;
         img->bufsize = mixer->buffer->bufsize;
         img->path    = strdup(nbio->path);
      }

      task_set_data(task, img);

      mixer->copy_data_over = false;
      mixer->is_finished    = true;

      return false;
   }

   return true;
}

bool task_push_audio_mixer_load_and_play(
      const char *fullpath, retro_task_callback_t cb, void *user_data,
      bool system,
      enum audio_mixer_slot_selection_type slot_selection_type,
      int slot_selection_idx)
{
   nbio_handle_t             *nbio    = NULL;
   struct audio_mixer_handle   *mixer = NULL;
   retro_task_t                   *t  = task_init();
   struct audio_mixer_userdata *user  = (struct audio_mixer_userdata*)calloc(1, sizeof(*user));

   if (!t || !user)
      goto error;

   nbio               = (nbio_handle_t*)calloc(1, sizeof(*nbio));

   if (!nbio)
      goto error;

   nbio->path         = strdup(fullpath);

   mixer              = (struct audio_mixer_handle*)calloc(1, sizeof(*mixer));
   if (!mixer)
      goto error;

   mixer->is_finished = false;

   strlcpy(mixer->path, fullpath, sizeof(mixer->path));

   nbio->type         = NBIO_TYPE_NONE;
   mixer->type        = AUDIO_MIXER_TYPE_NONE;

   if (strstr(fullpath, ".wav"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_WAV;
      nbio->type      = NBIO_TYPE_WAV;
      t->callback     = task_audio_mixer_handle_upload_wav_and_play;
   }
   else if (strstr(fullpath, ".ogg"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_OGG;
      nbio->type      = NBIO_TYPE_OGG;
      t->callback     = task_audio_mixer_handle_upload_ogg_and_play;
   }
   else if (strstr(fullpath, ".mp3"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_MP3;
      nbio->type      = NBIO_TYPE_MP3;
      t->callback     = task_audio_mixer_handle_upload_mp3_and_play;
   }
   else if (strstr(fullpath, ".flac"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_FLAC;
      nbio->type      = NBIO_TYPE_FLAC;
      t->callback     = task_audio_mixer_handle_upload_flac_and_play;
   }
   else if (	
         strstr(fullpath, ".mod") ||
         strstr(fullpath, ".s3m") ||
         strstr(fullpath, ".xm"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_MOD;
      nbio->type      = NBIO_TYPE_MOD;
      t->callback     = task_audio_mixer_handle_upload_mod_and_play;
   }

   if (system)
      user->stream_type      = AUDIO_STREAM_TYPE_SYSTEM;
   else
      user->stream_type      = AUDIO_STREAM_TYPE_USER;

   user->slot_selection_type = slot_selection_type;
   user->slot_selection_idx  = slot_selection_idx;

   nbio->data                = (struct audio_mixer_handle*)mixer;
   nbio->is_finished         = false;
   nbio->cb                  = &cb_nbio_audio_mixer_load;
   nbio->status              = NBIO_STATUS_INIT;

   t->state           = nbio;
   t->handler         = task_file_load_handler;
   t->cleanup         = task_audio_mixer_load_free;
   t->user_data       = user;

   task_queue_push(t);

   return true;

error:
   if (nbio)
   {
      if (!string_is_empty(nbio->path))
         free(nbio->path);
      if (nbio->data)
         free(nbio->data);
      nbio_free(nbio->handle);
      free(nbio);
   }
   if (user)
      free(user);
   if (t)
      free(t);

   RARCH_ERR("[audio mixer load] Failed to open '%s': %s.\n",
         fullpath, strerror(errno));

   return false;
}

bool task_push_audio_mixer_load(
      const char *fullpath, retro_task_callback_t cb, void *user_data,
      bool system,
      enum audio_mixer_slot_selection_type slot_selection_type,
      int slot_selection_idx)
{
   nbio_handle_t             *nbio    = NULL;
   struct audio_mixer_handle   *mixer = NULL;
   retro_task_t                   *t  = task_init();
   struct audio_mixer_userdata *user  = (struct audio_mixer_userdata*)calloc(1, sizeof(*user));

   if (!t || !user)
      goto error;

   nbio               = (nbio_handle_t*)calloc(1, sizeof(*nbio));

   if (!nbio)
      goto error;

   nbio->path         = strdup(fullpath);

   mixer              = (struct audio_mixer_handle*)calloc(1, sizeof(*mixer));
   if (!mixer)
      goto error;

   mixer->is_finished = false;
   mixer->cb          = cb;

   strlcpy(mixer->path, fullpath, sizeof(mixer->path));

   nbio->type         = NBIO_TYPE_NONE;
   mixer->type        = AUDIO_MIXER_TYPE_NONE;

   if (strstr(fullpath, ".wav"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_WAV;
      nbio->type      = NBIO_TYPE_WAV;
      t->callback     = task_audio_mixer_handle_upload_wav;
   }
   else if (strstr(fullpath, ".ogg"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_OGG;
      nbio->type      = NBIO_TYPE_OGG;
      t->callback     = task_audio_mixer_handle_upload_ogg;
   }
   else if (strstr(fullpath, ".mp3"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_MP3;
      nbio->type      = NBIO_TYPE_MP3;
      t->callback     = task_audio_mixer_handle_upload_mp3;
   }
   else if (strstr(fullpath, ".flac"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_FLAC;
      nbio->type      = NBIO_TYPE_FLAC;
      t->callback     = task_audio_mixer_handle_upload_flac;
   }
   else if (	
         strstr(fullpath, ".mod") ||
         strstr(fullpath, ".s3m") ||
         strstr(fullpath, ".xm"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_MOD;
      nbio->type      = NBIO_TYPE_MOD;
      t->callback     = task_audio_mixer_handle_upload_mod;
   }

   nbio->data         = (struct audio_mixer_handle*)mixer;
   nbio->is_finished  = false;
   nbio->cb           = &cb_nbio_audio_mixer_load;
   nbio->status       = NBIO_STATUS_INIT;

   if (system)
      user->stream_type      = AUDIO_STREAM_TYPE_SYSTEM;
   else
      user->stream_type      = AUDIO_STREAM_TYPE_USER;

   user->slot_selection_type = slot_selection_type;
   user->slot_selection_idx  = slot_selection_idx;

   t->state                  = nbio;
   t->handler                = task_file_load_handler;
   t->cleanup                = task_audio_mixer_load_free;
   t->user_data              = user;

   task_queue_push(t);

   return true;

error:
   if (nbio)
   {
      if (!string_is_empty(nbio->path))
         free(nbio->path);
      if (nbio->data)
         free(nbio->data);
      nbio_free(nbio->handle);
      free(nbio);
   }
   if (user)
      free(user);
   if (t)
      free(t);

   RARCH_ERR("[audio mixer load] Failed to open '%s': %s.\n",
         fullpath, strerror(errno));

   return false;
}
