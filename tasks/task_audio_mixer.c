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

#include <formats/data_transfer.h>
#include <formats/audio.h>
#include <file/file_path.h>
#include <audio/audio_mixer.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>
#include <queues/task_queue.h>

#include "../file_path_special.h"
#include "../audio/audio_driver.h"
#include "../verbosity.h"

#include "task_file_transfer.h"
#include "tasks_internal.h"

struct audio_mixer_userdata
{
   unsigned slot_selection_idx;
   enum audio_mixer_stream_type stream_type;
   enum audio_mixer_slot_selection_type slot_selection_type;
};

/* task_data handed to the upload callbacks: the classic buffer view
 * plus the data_transfer that owns the bytes, donated onward to the
 * mixer so no copy is ever made. */
struct audio_mixer_task_data
{
   nbio_buf_t b;
   struct data_transfer *xfer;
};

static void task_audio_mixer_release_xfer(void *owner)
{
   data_transfer_free((data_transfer_t*)owner);
}

struct audio_mixer_handle
{
   nbio_buf_t *buffer;
   retro_task_callback_t cb;
   enum audio_mixer_type type;
   char path[4095];
   bool copy_data_over;
   bool is_finished;
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

   if (nbio->path && *nbio->path)
      free(nbio->path);
   if (nbio->data)
      free(nbio->data);
   nbio_xfer_close(nbio);
   free(nbio);
}

static int cb_nbio_audio_mixer_load(void *data, size_t len)
{
   nbio_handle_t *nbio             = (nbio_handle_t*)data;
   struct audio_mixer_handle *mixer= (struct audio_mixer_handle*)nbio->data;
   void *ptr                       = (void*)nbio_xfer_ptr(nbio, &len);
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
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_OGG;
   params.state                = AUDIO_STREAM_STATE_STOPPED;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_ogg_and_play(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_OGG;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_flac(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_FLAC;
   params.state                = AUDIO_STREAM_STATE_STOPPED;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_flac_and_play(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_FLAC;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_mp3(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_MP3;
   params.state                = AUDIO_STREAM_STATE_STOPPED;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_mp3_and_play(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_MP3;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_m4a(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_M4A;
   params.state                = AUDIO_STREAM_STATE_STOPPED;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_m4a_and_play(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_M4A;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_opus(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_OPUS;
   params.state                = AUDIO_STREAM_STATE_STOPPED;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_opus_and_play(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_OPUS;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_weba(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_WEBA;
   params.state                = AUDIO_STREAM_STATE_STOPPED;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_weba_and_play(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_WEBA;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_mod(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_MOD;
   params.state                = AUDIO_STREAM_STATE_STOPPED;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_mod_and_play(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_MOD;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

#ifdef HAVE_RWAV
static void task_audio_mixer_handle_upload_wav(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_WAV;
   params.state                = AUDIO_STREAM_STATE_STOPPED;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}

static void task_audio_mixer_handle_upload_wav_and_play(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   audio_mixer_stream_params_t params;
   struct audio_mixer_task_data *img =
         (struct audio_mixer_task_data*)task_data;
   struct audio_mixer_userdata *user = (struct audio_mixer_userdata*)user_data;
   if (!img || !user)
   {
      if (img)
      {
         if (img->xfer)
            task_audio_mixer_release_xfer(img->xfer);
         if (img->b.path)
            free(img->b.path);
         free(img);
      }
      free(user_data);
      return;
   }

   params.volume               = 1.0f;
   params.slot_selection_type  = user->slot_selection_type;
   params.slot_selection_idx   = user->slot_selection_idx;
   params.stream_type          = user->stream_type;
   params.type                 = AUDIO_MIXER_TYPE_WAV;
   params.state                = AUDIO_STREAM_STATE_PLAYING;
   params.buf                  = img->b.buf;
   params.bufsize              = img->b.bufsize;
   params.cb                   = NULL;
   params.buf_owner            = img->xfer;
   params.buf_owner_free       = task_audio_mixer_release_xfer;
   params.out_slot             = NULL;
   params.basename             = (img->b.path && *img->b.path) ? (char*)path_basename_nocompression(img->b.path) : NULL;

   audio_driver_mixer_add_stream(&params);

   if (img->b.path)
      free(img->b.path);
   free(img);
   free(user_data);
}
#endif

bool task_audio_mixer_load_handler(retro_task_t *task)
{
   nbio_handle_t             *nbio  = (nbio_handle_t*)task->state;
   struct audio_mixer_handle *mixer = (struct audio_mixer_handle*)nbio->data;
   uint8_t flg                      = task_get_flags(task);

   if (
         nbio->is_finished
         && (mixer && !mixer->is_finished)
         && (mixer->copy_data_over)
         && (!((flg & RETRO_TASK_FLG_CANCELLED) > 0)))
   {
      struct audio_mixer_task_data *img = (struct audio_mixer_task_data*)
            malloc(sizeof(*img));

      if (img)
      {
         img->b.buf     = mixer->buffer->buf;
         img->b.bufsize = mixer->buffer->bufsize;
         img->b.path    = strdup(nbio->path);
         /* donate the transfer: the buffer lives inside it, and the
          * upload callback lends both onward to the mixer */
         img->xfer      = nbio->xfer;
         nbio->xfer     = NULL;
      }

      task_set_data(task, img);

      mixer->copy_data_over = false;
      mixer->is_finished    = true;

      return false;
   }

   return true;
}

#if defined(HAVE_AUDIOMIXER) && (defined(HAVE_RVORBIS) || defined(HAVE_RMP3) || defined(HAVE_RFLAC))
/* ---- windowed streaming for large background music ----
 * Files past the threshold, in codecs whose buffer-mode access
 * pattern is head-bounded-open + monotonic reads + loop-to-head
 * (Vorbis, MP3, and FLAC, verified against the decoders), do not
 * get read
 * into memory at all: the sound borrows a data_transfer window - the
 * head resident forever, a window sliding with the decoder - and
 * this task, after opening the window and adding the stream, becomes
 * its feeder for the stream's lifetime.  Residency is head plus
 * window instead of the file. */

/* the fallback path below re-enters the public push functions */
bool task_push_audio_mixer_load(const char *fullpath,
      retro_task_callback_t cb, void *user_data, bool system,
      enum audio_mixer_slot_selection_type slot_selection_type,
      int slot_selection_idx);
bool task_push_audio_mixer_load_and_play(const char *fullpath,
      retro_task_callback_t cb, void *user_data, bool system,
      enum audio_mixer_slot_selection_type slot_selection_type,
      int slot_selection_idx);

#define AMIX_WINDOW_THRESHOLD (8 * 1024 * 1024)
#define AMIX_WINDOW_KEEP      (2 * 1024 * 1024)
#define AMIX_WINDOW_LOOKAHEAD (2 * 1024 * 1024)
#define AMIX_WINDOW_MARGIN    (1 * 1024 * 1024)

struct audio_mixer_wfeed
{
   char *path;
   struct data_transfer *dt;
   struct audio_mixer_userdata *user;
   int   slot;
   enum audio_mixer_type mtype;
   bool  play;
   bool  added;          /* the sound borrows dt once this is set */
   bool  keep_set;       /* head sized to the decoder's start     */
   size_t punch_lo;      /* inert metadata span released from the  */
   size_t punch_hi;      /*  head after the decoder skips it       */
   volatile bool dead;   /* set by the sound's release: wrap up    */
};

#ifdef HAVE_RFLAC
/* Walk the FLAC metadata block headers (peeking 4 bytes per block,
 * never reading block bodies) to learn where the first audio frame
 * begins - PICTURE blocks can push it megabytes in - and where any
 * large inert span sits.  Returns 0 on anything unexpected; the
 * caller then falls back to the classic full load. */
static size_t task_audio_mixer_flac_first_frame(struct data_transfer *dt,
      size_t flen, size_t *inert_lo, size_t *inert_hi)
{
   uint8_t h[4];
   size_t  off  = 4;             /* past the fLaC magic */
   unsigned i;
   *inert_lo = 0;
   *inert_hi = 0;
   if (!data_transfer_window_peek(dt, 0, h, 4))
      return 0;
   if (memcmp(h, "fLaC", 4) != 0)
      return 0;
   for (i = 0; i < 64; i++)
   {
      size_t  blen;
      uint8_t last;
      if (off + 4 > flen || !data_transfer_window_peek(dt, off, h, 4))
         return 0;
      last = h[0] >> 7;
      blen = ((size_t)h[1] << 16) | ((size_t)h[2] << 8) | (size_t)h[3];
      if ((h[0] & 0x7f) == 6 && blen > *inert_hi - *inert_lo)
      {
         /* the largest PICTURE block is the punch candidate */
         *inert_lo = off + 4;
         *inert_hi = off + 4 + blen;
      }
      off += 4 + blen;
      if (last)
         return off <= flen ? off : 0;
   }
   return 0;
}
#endif

static void task_audio_mixer_wfeed_release(void *owner)
{
   /* runs when the sound is destroyed; the feeder task frees */
   ((struct audio_mixer_wfeed*)owner)->dead = true;
}

static void task_audio_mixer_wfeed_free(retro_task_t *task)
{
   struct audio_mixer_wfeed *w =
         (struct audio_mixer_wfeed*)task->state;
   if (!w)
      return;
   if (!w->added && w->dt)
      data_transfer_free(w->dt);   /* never handed over */
   if (w->path)
      free(w->path);
   if (w->user)
      free(w->user);
   free(w);
}

static void task_audio_mixer_handle_wfeed(retro_task_t *task)
{
   struct audio_mixer_wfeed *w =
         (struct audio_mixer_wfeed*)task->state;
   int64_t tell;

   if (w->dead)
   {
      /* the borrowing sound is gone: the window with it */
      data_transfer_free(w->dt);
      w->dt    = NULL;
      w->added = false;
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }

   if (!w->added)
   {
      /* first tick: open the window, identify the codec, hand the
       * stream over, then settle into feeding */
      audio_mixer_stream_params_t params;
      size_t flen           = 0;
      const uint8_t *base   = NULL;

      if (!(w->dt = data_transfer_open_window(w->path,
            AMIX_WINDOW_KEEP)))
         goto bail;
      base = data_transfer_window_base(w->dt, &flen);

      if (w->mtype == AUDIO_MIXER_TYPE_OGG)
      {
         /* an .ogg can wrap Opus, whose seek pattern is not
          * window-verified: only Vorbis streams windowed */
         if (audio_transfer_ogg_audio_type(base,
               flen < AMIX_WINDOW_KEEP ? flen : AMIX_WINDOW_KEEP)
               != AUDIO_TYPE_VORBIS)
            goto bail;
      }
#ifdef HAVE_RAAC
      if (w->mtype == AUDIO_MIXER_TYPE_M4A)
      {
         /* Confirm a raw ADTS syncword (0xFFF*) at the head: only the
          * ADTS buffer path exposes a windowable cursor.  A file that
          * reached here as .aac but is really an MP4 (or anything
          * without the syncword) falls back to the classic load. */
         uint8_t sync[2];
         if (!data_transfer_window_peek(w->dt, 0, sync, 2)
               || sync[0] != 0xFF || (sync[1] & 0xF6) != 0xF0)
            goto bail;
      }
#endif
#ifdef HAVE_RFLAC
      if (w->mtype == AUDIO_MIXER_TYPE_FLAC)
      {
         /* the loop seek lands on the first audio frame - on the
          * audio thread, before any feeder tick - and PICTURE
          * metadata can push that frame past any fixed head.  Walk
          * the block headers, grow the head to cover the frame, and
          * remember the largest inert block to punch back out once
          * the decoder has skipped it. */
         size_t ff = task_audio_mixer_flac_first_frame(w->dt, flen,
               &w->punch_lo, &w->punch_hi);
         if (!ff)
            goto bail;
         if (ff + (512 * 1024) > AMIX_WINDOW_KEEP
               && !data_transfer_window_grow_keep(w->dt,
                     ff + AMIX_WINDOW_MARGIN))
            goto bail;
      }
#endif

      params.volume               = 1.0f;
      params.slot_selection_type  = w->user->slot_selection_type;
      params.slot_selection_idx   = w->user->slot_selection_idx;
      params.stream_type          = w->user->stream_type;
      params.type                 = w->mtype;
      params.state                = w->play
            ? AUDIO_STREAM_STATE_PLAYING : AUDIO_STREAM_STATE_STOPPED;
      params.buf                  = (void*)base;
      params.bufsize              = flen;
      params.cb                   = NULL;
      params.basename             = (char*)path_basename_nocompression(
            w->path);
      params.buf_owner            = w;
      params.buf_owner_free       = task_audio_mixer_wfeed_release;
      params.out_slot             = &w->slot;

      w->added = true;   /* ownership transfers on the call in every
                          * outcome: from here the release runs */
      if (!audio_driver_mixer_add_stream(&params))
      {
         /* release already ran synchronously: dead is set and the
          * next tick frees the window */
         return;
      }
      /* the decoder has opened and seek-skipped the metadata: an
       * inert block inside the grown head can leave residency now */
      if (w->punch_hi > w->punch_lo
            && w->punch_hi - w->punch_lo >= (256 * 1024))
         data_transfer_window_punch(w->dt, w->punch_lo, w->punch_hi);
      return;
   }

   tell = audio_driver_mixer_stream_byte_tell((unsigned)w->slot);
   if (tell >= 0)
   {
      if (!w->keep_set)
      {
         /* First sighting of the decoder's start position.  The
          * loop seek lands here on the audio thread, before any
          * feeder tick, so the head must cover it - and FLAC
          * metadata (PICTURE blocks) can push the first frame past
          * any fixed head.  Grow the head to the decoder's start
          * plus slack once, now that the position is known. */
         if ((size_t)tell + (512 * 1024) > AMIX_WINDOW_KEEP)
            data_transfer_window_grow_keep(w->dt,
                  (size_t)tell + (1024 * 1024));
         w->keep_set = true;
      }
      data_transfer_window_feed(w->dt, (size_t)tell,
            AMIX_WINDOW_LOOKAHEAD, AMIX_WINDOW_MARGIN);
   }
   /* a stopped stream (tell < 0 while not dead) just idles */
   return;

bail:
   if (w->dt)
   {
      data_transfer_free(w->dt);
      w->dt = NULL;
   }
   /* fall back to the classic full-load path */
   if (w->play)
      task_push_audio_mixer_load_and_play(w->path, NULL, NULL,
            w->user->stream_type == AUDIO_STREAM_TYPE_SYSTEM,
            w->user->slot_selection_type, w->user->slot_selection_idx);
   else
      task_push_audio_mixer_load(w->path, NULL, NULL,
            w->user->stream_type == AUDIO_STREAM_TYPE_SYSTEM,
            w->user->slot_selection_type, w->user->slot_selection_idx);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

/* Returns true when the load was taken over by the windowed path. */
static bool task_audio_mixer_try_windowed(const char *fullpath,
      bool system, bool play,
      enum audio_mixer_slot_selection_type slot_selection_type,
      int slot_selection_idx)
{
   enum audio_mixer_type mtype = AUDIO_MIXER_TYPE_NONE;
   struct audio_mixer_wfeed *w = NULL;
   retro_task_t *t             = NULL;
   const char *ext             = path_get_extension(fullpath);
   int64_t sz;

   if (!ext)
      return false;
#ifdef HAVE_RVORBIS
   if (string_is_equal_noncase(ext, "ogg"))
      mtype = AUDIO_MIXER_TYPE_OGG;
#endif
#ifdef HAVE_RMP3
   if (string_is_equal_noncase(ext, "mp3"))
      mtype = AUDIO_MIXER_TYPE_MP3;
#endif
#ifdef HAVE_RFLAC
   if (string_is_equal_noncase(ext, "flac"))
      mtype = AUDIO_MIXER_TYPE_FLAC;
#endif
#ifdef HAVE_RAAC
   /* Only a raw ADTS .aac stream is window-eligible: its decoder open
    * reads just the first frame header, reads run monotonically
    * forward (adts_pos), and the loop seek returns to the head.  An
    * .m4a/MP4 is excluded here - its moov can be trailing and the
    * demuxer seeks - so it keeps the classic full load.  The actual
    * ADTS syncword is still verified from the head below, the way OGG
    * confirms Vorbis rather than Opus. */
   if (string_is_equal_noncase(ext, "aac"))
      mtype = AUDIO_MIXER_TYPE_M4A;
#endif
   if (mtype == AUDIO_MIXER_TYPE_NONE)
      return false;
   sz = path_get_size(fullpath);
   if (sz < (int64_t)AMIX_WINDOW_THRESHOLD)
      return false;

   if (!(t = task_init()))
      return false;
   if (!(w = (struct audio_mixer_wfeed*)calloc(1, sizeof(*w))))
      goto error;
   if (!(w->user = (struct audio_mixer_userdata*)
         calloc(1, sizeof(*w->user))))
      goto error;
   if (!(w->path = strdup(fullpath)))
      goto error;
   w->mtype                     = mtype;
   w->play                      = play;
   w->slot                      = -1;
   w->user->stream_type         = system
         ? AUDIO_STREAM_TYPE_SYSTEM : AUDIO_STREAM_TYPE_USER;
   w->user->slot_selection_type = slot_selection_type;
   w->user->slot_selection_idx  = slot_selection_idx;

   t->state   = w;
   t->handler = task_audio_mixer_handle_wfeed;
   t->cleanup = task_audio_mixer_wfeed_free;
   t->flags  |= RETRO_TASK_FLG_MUTE;
   task_queue_push(t);
   return true;

error:
   if (w)
   {
      if (w->user)
         free(w->user);
      if (w->path)
         free(w->path);
      free(w);
   }
   free(t);
   return false;
}
#endif /* windowed streaming */

bool task_push_audio_mixer_load_and_play(
      const char *fullpath, retro_task_callback_t cb, void *user_data,
      bool system,
      enum audio_mixer_slot_selection_type slot_selection_type,
      int slot_selection_idx)
{
#if defined(HAVE_AUDIOMIXER) && (defined(HAVE_RVORBIS) || defined(HAVE_RMP3))
   if (task_audio_mixer_try_windowed(fullpath, system, true,
         slot_selection_type, slot_selection_idx))
      return true;
#endif
   {
   nbio_handle_t             *nbio    = NULL;
   struct audio_mixer_handle   *mixer = NULL;
   retro_task_t                   *t  = task_init();
   struct audio_mixer_userdata *user  = (struct audio_mixer_userdata*)calloc(1, sizeof(*user));
   /* We are comparing against a fixed list of file
    * extensions, the longest (jpeg) being 4 characters
    * in length. We therefore only need to extract the first
    * 5 characters from the extension of the input path
    * to correctly validate a match */
   const char *ext                    = NULL;
   char ext_lower[6];

   if (!t || !user)
      goto error;

   if (!(nbio = (nbio_handle_t*)calloc(1, sizeof(*nbio))))
      goto error;

   nbio->path         = strdup(fullpath);
   /* NULL-check strdup: downstream strdup(nbio->path) calls at
    * lines 72 and 429 assume this is non-NULL.  strdup(NULL) is
    * UB per POSIX; glibc crashes.  Fail the task setup so the
    * caller can surface the error. */
   if (!nbio->path)
      goto error;

   if (!(mixer = (struct audio_mixer_handle*)calloc(1, sizeof(*mixer))))
      goto error;

   mixer->is_finished = false;

   strlcpy(mixer->path, fullpath, sizeof(mixer->path));

   nbio->type         = NBIO_TYPE_NONE;
   mixer->type        = AUDIO_MIXER_TYPE_NONE;

   /* Get file extension */
   ext                = strrchr(fullpath, '.');

   if (!ext || (*(++ext) == '\0'))
      goto error;

   /* Copy and convert to lower case */
   strlcpy(ext_lower, ext, sizeof(ext_lower));
   string_to_lower(ext_lower);

#ifdef HAVE_RWAV
   if (string_is_equal(ext_lower, "wav"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_WAV;
      nbio->type      = NBIO_TYPE_WAV;
      t->callback     = task_audio_mixer_handle_upload_wav_and_play;
   }
   else
#endif
      if (string_is_equal(ext_lower, "ogg"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_OGG;
      nbio->type      = NBIO_TYPE_OGG;
      t->callback     = task_audio_mixer_handle_upload_ogg_and_play;
   }
   else if (string_is_equal(ext_lower, "mp3"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_MP3;
      nbio->type      = NBIO_TYPE_MP3;
      t->callback     = task_audio_mixer_handle_upload_mp3_and_play;
   }
   else if (string_is_equal(ext_lower, "m4a")
         || string_is_equal(ext_lower, "aac"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_M4A;
      nbio->type      = NBIO_TYPE_M4A;
      t->callback     = task_audio_mixer_handle_upload_m4a_and_play;
   }
   else if (string_is_equal(ext_lower, "opus"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_OPUS;
      nbio->type      = NBIO_TYPE_OPUS;
      t->callback     = task_audio_mixer_handle_upload_opus_and_play;
   }
   else if (string_is_equal(ext_lower, "weba"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_WEBA;
      nbio->type      = NBIO_TYPE_WEBM;
      t->callback     = task_audio_mixer_handle_upload_weba_and_play;
   }
   else if (string_is_equal(ext_lower, "flac"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_FLAC;
      nbio->type      = NBIO_TYPE_FLAC;
      t->callback     = task_audio_mixer_handle_upload_flac_and_play;
   }
   else if (
            string_is_equal(ext_lower, "mod")
         || string_is_equal(ext_lower, "s3m")
         || string_is_equal(ext_lower, "xm"))
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
      if (nbio->path && *nbio->path)
         free(nbio->path);
      if (nbio->data)
         free(nbio->data);
      nbio_xfer_close(nbio);
      free(nbio);
   }
   if (user)
      free(user);
   if (t)
      free(t);

   RARCH_ERR("[Audio mixer] Failed to open \"%s\".\n",
         fullpath);

   return false;
   }
}

bool task_push_audio_mixer_load(
      const char *fullpath, retro_task_callback_t cb, void *user_data,
      bool system,
      enum audio_mixer_slot_selection_type slot_selection_type,
      int slot_selection_idx)
{
#if defined(HAVE_AUDIOMIXER) && (defined(HAVE_RVORBIS) || defined(HAVE_RMP3))
   if (task_audio_mixer_try_windowed(fullpath, system, false,
         slot_selection_type, slot_selection_idx))
      return true;
#endif
   {
   nbio_handle_t             *nbio    = NULL;
   struct audio_mixer_handle   *mixer = NULL;
   retro_task_t                   *t  = task_init();
   struct audio_mixer_userdata *user  = (struct audio_mixer_userdata*)calloc(1, sizeof(*user));
   /* We are comparing against a fixed list of file
    * extensions, the longest (jpeg) being 4 characters
    * in length. We therefore only need to extract the first
    * 5 characters from the extension of the input path
    * to correctly validate a match */
   const char *ext                    = NULL;
   char ext_lower[6];

   if (!t || !user)
      goto error;

   if (!(nbio = (nbio_handle_t*)calloc(1, sizeof(*nbio))))
      goto error;

   nbio->path         = strdup(fullpath);
   /* NULL-check strdup: see the twin check in the sibling
    * task_push_audio_mixer_load for the reasoning.  strdup(NULL)
    * is UB and downstream code dereferences nbio->path. */
   if (!nbio->path)
      goto error;

   if (!(mixer = (struct audio_mixer_handle*)calloc(1, sizeof(*mixer))))
      goto error;

   mixer->is_finished = false;
   mixer->cb          = cb;

   strlcpy(mixer->path, fullpath, sizeof(mixer->path));

   nbio->type         = NBIO_TYPE_NONE;
   mixer->type        = AUDIO_MIXER_TYPE_NONE;

   /* Get file extension */
   ext                = strrchr(fullpath, '.');

   if (!ext || (*(++ext) == '\0'))
      goto error;

   /* Copy and convert to lower case */
   strlcpy(ext_lower, ext, sizeof(ext_lower));
   string_to_lower(ext_lower);

#ifdef HAVE_RWAV
   if (string_is_equal(ext_lower, "wav"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_WAV;
      nbio->type      = NBIO_TYPE_WAV;
      t->callback     = task_audio_mixer_handle_upload_wav;
   }
   else
#endif
      if (string_is_equal(ext_lower, "ogg"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_OGG;
      nbio->type      = NBIO_TYPE_OGG;
      t->callback     = task_audio_mixer_handle_upload_ogg;
   }
   else if (string_is_equal(ext_lower, "mp3"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_MP3;
      nbio->type      = NBIO_TYPE_MP3;
      t->callback     = task_audio_mixer_handle_upload_mp3;
   }
   else if (string_is_equal(ext_lower, "m4a")
         || string_is_equal(ext_lower, "aac"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_M4A;
      nbio->type      = NBIO_TYPE_M4A;
      t->callback     = task_audio_mixer_handle_upload_m4a;
   }
   else if (string_is_equal(ext_lower, "opus"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_OPUS;
      nbio->type      = NBIO_TYPE_OPUS;
      t->callback     = task_audio_mixer_handle_upload_opus;
   }
   else if (string_is_equal(ext_lower, "weba"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_WEBA;
      nbio->type      = NBIO_TYPE_WEBM;
      t->callback     = task_audio_mixer_handle_upload_weba;
   }
   else if (string_is_equal(ext_lower, "flac"))
   {
      mixer->type     = AUDIO_MIXER_TYPE_FLAC;
      nbio->type      = NBIO_TYPE_FLAC;
      t->callback     = task_audio_mixer_handle_upload_flac;
   }
   else if (
            string_is_equal(ext_lower, "mod")
         || string_is_equal(ext_lower, "s3m")
         || string_is_equal(ext_lower, "xm"))
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
      if (nbio->path && *nbio->path)
         free(nbio->path);
      if (nbio->data)
         free(nbio->data);
      nbio_xfer_close(nbio);
      free(nbio);
   }
   if (user)
      free(user);
   if (t)
      free(t);

   RARCH_ERR("[Audio mixer] Failed to open \"%s\".\n",
         fullpath);

   return false;
   }
}
