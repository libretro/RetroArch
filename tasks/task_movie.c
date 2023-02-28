/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifdef HAVE_BSV_MOVIE
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <compat/strl.h>
#include <file/file_path.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../msg_hash.h"
#include "../verbosity.h"
#include "../core.h"
#include "../content.h"
#include "../runloop.h"
#include "tasks_internal.h"
#include "../input/input_driver.h"

#define MAGIC_INDEX        0
#define SERIALIZER_INDEX   1
#define CRC_INDEX          2
#define STATE_SIZE_INDEX   3

#define BSV_MAGIC          0x42535631

/* Forward declaration */
bool content_load_state_in_progress(void* data);

/* Private functions */

static bool bsv_movie_init_playback(
      bsv_movie_t *handle, const char *path)
{
   uint32_t state_size       = 0;
   uint32_t header[4]        = {0};
   intfstream_t *file        = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open BSV file for playback, path : \"%s\".\n", path);
      return false;
   }

   handle->file              = file;
   handle->playback          = true;

   intfstream_read(handle->file, header, sizeof(uint32_t) * 4);
   /* Compatibility with old implementation that
    * used incorrect documentation. */
   if (swap_if_little32(header[MAGIC_INDEX]) != BSV_MAGIC
         && swap_if_big32(header[MAGIC_INDEX]) != BSV_MAGIC)
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE));
      return false;
   }

   state_size = swap_if_big32(header[STATE_SIZE_INDEX]);

#if 0
   RARCH_ERR("----- debug %u -----\n", header[0]);
   RARCH_ERR("----- debug %u -----\n", header[1]);
   RARCH_ERR("----- debug %u -----\n", header[2]);
   RARCH_ERR("----- debug %u -----\n", header[3]);
#endif

   if (state_size)
   {
      retro_ctx_size_info_t info;
      retro_ctx_serialize_info_t serial_info;
      uint8_t *buf       = (uint8_t*)malloc(state_size);

      if (!buf)
         return false;

      handle->state      = buf;
      handle->state_size = state_size;
      if (intfstream_read(handle->file,
               handle->state, state_size) != state_size)
      {
         RARCH_ERR("%s\n", msg_hash_to_str(MSG_COULD_NOT_READ_STATE_FROM_MOVIE));
         return false;
      }
      core_serialize_size( &info);
      /* For cores like dosbox, the reported size is not always
         correct. So we just give a warning if they don't match up. */
      serial_info.data_const = handle->state;
      serial_info.size       = state_size;
      core_unserialize(&serial_info);
      if (info.size != state_size)
      {
         RARCH_WARN("%s\n",
               msg_hash_to_str(MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION));
      }
   }

   handle->min_file_pos = sizeof(header) + state_size;

   return true;
}

static bool bsv_movie_init_record(
      bsv_movie_t *handle, const char *path)
{
   retro_ctx_size_info_t info;
   uint32_t state_size       = 0;
   uint32_t content_crc      = 0;
   uint32_t header[4]        = {0};
   intfstream_t *file        = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open BSV file for recording, path : \"%s\".\n", path);
      return false;
   }

   handle->file             = file;

   content_crc              = content_get_crc();

   /* This value is supposed to show up as
    * BSV1 in a HEX editor, big-endian. */
   header[MAGIC_INDEX]      = swap_if_little32(BSV_MAGIC);
   header[CRC_INDEX]        = swap_if_big32(content_crc);
   core_serialize_size(&info);

   state_size               = (unsigned)info.size;

   header[STATE_SIZE_INDEX] = swap_if_big32(state_size);

   intfstream_write(handle->file, header, 4 * sizeof(uint32_t));

   handle->min_file_pos     = sizeof(header) + state_size;
   handle->state_size       = state_size;

   if (state_size)
   {
      retro_ctx_serialize_info_t serial_info;
      uint8_t *st      = (uint8_t*)malloc(state_size);

      if (!st)
         return false;

      handle->state    = st;

      serial_info.data = handle->state;
      serial_info.size = state_size;

      core_serialize(&serial_info);

      intfstream_write(handle->file,
            handle->state, state_size);
   }

   return true;
}

void bsv_movie_free(bsv_movie_t *handle)
{
   intfstream_close(handle->file);
   free(handle->file);

   free(handle->state);
   free(handle->frame_pos);
   free(handle);
}

static bsv_movie_t *bsv_movie_init_internal(const char *path, enum rarch_movie_type type)
{
   size_t *frame_pos   = NULL;
   bsv_movie_t *handle = (bsv_movie_t*)calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   if (type == RARCH_MOVIE_PLAYBACK)
   {
      if (!bsv_movie_init_playback(handle, path))
         goto error;
   }
   else if (!bsv_movie_init_record(handle, path))
      goto error;

   /* Just pick something really large
    * ~1 million frames rewind should do the trick. */
   if (!(frame_pos = (size_t*)calloc((1 << 20), sizeof(size_t))))
      goto error;

   handle->frame_pos       = frame_pos;

   handle->frame_pos[0]    = handle->min_file_pos;
   handle->frame_mask      = (1 << 20) - 1;

   return handle;

error:
   if (handle)
      bsv_movie_free(handle);
   return NULL;
}

static bool bsv_movie_start_record(input_driver_state_t * input_st, char *path)
{
   char msg[8192];
   bsv_movie_t *state                       = NULL;
   const char *movie_rec_str                = msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO);

   /* this should trigger a start recording task which on failure or
      success prints a message and on success sets the
      input_st->bsv_movie_state_handle. */
   if (!(state = bsv_movie_init_internal(path, RARCH_MOVIE_RECORD)))
   {
      const char *movie_rec_fail_str        =
         msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD);
      runloop_msg_queue_push(movie_rec_fail_str,
            1, 180, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_ERR("%s.\n", movie_rec_fail_str);
      return false;
   }

   input_st->bsv_movie_state_handle         = state;
   input_st->bsv_movie_state.flags         |= BSV_FLAG_MOVIE_RECORDING;
   snprintf(msg, sizeof(msg),
            "%s \"%s\".", movie_rec_str,
            path);

   runloop_msg_queue_push(msg, 2, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s \"%s\".\n", movie_rec_str, path);

   return true;
}

static bool bsv_movie_start_playback(input_driver_state_t *input_st, char *path)
{
   bsv_movie_t *state                       = NULL;
   const char *starting_movie_str           = NULL;
   /* This should trigger a start playback task which on failure or
      success prints a message and on success sets the
      input_st->bsv_movie_state_handle. */
   if (!(state = bsv_movie_init_internal(path, RARCH_MOVIE_PLAYBACK)))
   {
      RARCH_ERR("%s: \"%s\".\n",
            msg_hash_to_str(MSG_FAILED_TO_LOAD_MOVIE_FILE),
            path);
      return false;
   }

   input_st->bsv_movie_state_handle         = state;
   input_st->bsv_movie_state.flags         |= BSV_FLAG_MOVIE_PLAYBACK;
   starting_movie_str                       =
      msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK);

   runloop_msg_queue_push(starting_movie_str,
         2, 180, false,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s.\n", starting_movie_str);

   return true;
}


/* Task infrastructure (also private) */

/* Future: replace stop functions with tasks that do the same. then
   later we can replace the start_record/start_playback flags and
   remove the entirety of input_driver_st bsv_state, which is only
   needed due to mixing sync and async during initialization. */
typedef struct bsv_state moviectl_task_state_t;

static void task_moviectl_playback_handler(retro_task_t *task)
{
  /* trivial handler */
  task_set_finished(task, true);
  if (!task_get_error(task) && task_get_cancelled(task))
    task_set_error(task, strdup("Task canceled"));

  task_set_data(task, task->state);
  task->state = NULL;
  /* no need to free state here since I'm recycling it as data */
}

static void moviectl_start_playback_cb(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error)
{
  struct bsv_state *state        = (struct bsv_state *)task_data;
  input_driver_state_t *input_st = input_state_get_ptr();
  input_st->bsv_movie_state      = *state;
  bsv_movie_start_playback(input_st, state->movie_start_path);
  free(state);
}

static void task_moviectl_record_handler(retro_task_t *task)
{
   /* Hang on until the state is loaded */
   if (content_load_state_in_progress(NULL))
      return;

   /* trivial handler */
   task_set_finished(task, true);
   if (!task_get_error(task) && task_get_cancelled(task))
      task_set_error(task, strdup("Task canceled"));

   task_set_data(task, task->state);
   task->state = NULL;
   /* no need to free state here since I'm recycling it as data */
}

static void moviectl_start_record_cb(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error)
{
  struct bsv_state *state        = (struct bsv_state *)task_data;
  input_driver_state_t *input_st = input_state_get_ptr();
  input_st->bsv_movie_state      = *state;
  bsv_movie_start_record(input_st, state->movie_start_path);
  free(state);
}

/* Public functions */

bool movie_toggle_record(input_driver_state_t *input_st, settings_t *settings)
{
   if (!input_st->bsv_movie_state_handle)
   {
      char path[8192];
      configuration_set_uint(settings, settings->uints.rewind_granularity, 1);
      // TODO use slot number, qua runloop_get_current_savestate_path; look at something like command.c:command_event_main_state to see how that's done and how cleanup works
      fill_str_dated_filename(path, input_st->bsv_movie_state.movie_auto_path, "bsv", sizeof(path));
      return movie_start_record(input_st, path);
   }

   return movie_stop(input_st);
}

/* In the future this should probably be a deferred task as well */
bool movie_stop_playback(input_driver_state_t *input_st)
{
   const char *movie_playback_end_str = NULL;
   /* Checks if movie is being played back. */
   if (!(input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK))
      return false;
   movie_playback_end_str = msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED);
   runloop_msg_queue_push(
         movie_playback_end_str, 2, 180, false,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s\n", movie_playback_end_str);

   bsv_movie_deinit(input_st);

   input_st->bsv_movie_state.flags &= ~(
         BSV_FLAG_MOVIE_END
         | BSV_FLAG_MOVIE_PLAYBACK);
   return true;
}
/* in the future this should probably be a deferred task as well */
bool movie_stop_record(input_driver_state_t *input_st)
{
   const char *movie_rec_stopped_str = msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED);
   if (!(input_st->bsv_movie_state_handle))
      return false;
   runloop_msg_queue_push(movie_rec_stopped_str,
         2, 180, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s\n", movie_rec_stopped_str);
   bsv_movie_deinit(input_st);
   return true;

}

bool movie_stop(input_driver_state_t *input_st)
{
   if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK)
      return movie_stop_playback(input_st);
   else if (input_st->bsv_movie_state_handle)
      return movie_stop_record(input_st);
   return true;
}

bool movie_start_playback(input_driver_state_t *input_st, char *path)
{
  retro_task_t       *task      = task_init();
  moviectl_task_state_t *state  = (moviectl_task_state_t *) calloc(1, sizeof(*state));
  if (!task || !state)
    goto error;
  *state                        = input_st->bsv_movie_state;
  strlcpy(state->movie_start_path, path, sizeof(state->movie_start_path));
  task->type                    = TASK_TYPE_NONE;
  task->state                   = state;
  task->handler                 = task_moviectl_playback_handler;
  task->callback                = moviectl_start_playback_cb;
  task->title                   = strdup(msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK));

  if (task_queue_push(task))
     return true;

error:
   if (state)
      free(state);
   if (task)
      free(task);

   return false;
}
bool movie_start_record(input_driver_state_t *input_st, char*path)
{
   char msg[8192];
   const char *movie_rec_str     = msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO);
   retro_task_t       *task      = task_init();
   moviectl_task_state_t *state  = (moviectl_task_state_t *) calloc(1, sizeof(*state));
   if (!task || !state)
      goto error;

   *state                        = input_st->bsv_movie_state;
   strlcpy(state->movie_start_path, path, sizeof(state->movie_start_path));

   msg[0]                        = '\0';
   snprintf(msg, sizeof(msg), "%s \"%s\".", movie_rec_str, path);

   task->type                    = TASK_TYPE_NONE;
   task->state                   = state;
   task->handler                 = task_moviectl_record_handler;
   task->callback                = moviectl_start_record_cb;

   task->title                   = strdup(msg);

   if (!task_queue_push(task))
      goto error;

   return true;

error:
   if (state)
      free(state);
   if (task)
      free(task);

   return false;
}
#endif
