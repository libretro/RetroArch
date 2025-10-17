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
#include <time/rtime.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <retro_endianness.h>

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
#include "../input/bsv/bsvmovie.h"

#ifdef HAVE_STATESTREAM
#if DEBUG
#include "input/bsv/uint32s_index.h"
#endif

#define REPLAY_DEFAULT_COMMIT_INTERVAL 4
#define REPLAY_DEFAULT_COMMIT_THRESHOLD 2

/* Superblock and block sizes for incremental savestates. */
#define DEFAULT_SUPERBLOCK_SIZE 16  /* measured in blocks */
#define DEFAULT_BLOCK_SIZE      16384 /* measured in bytes  */

#define SMALL_STATE_THRESHOLD (1<<20) /* states < 1MB are "small" and are tuned differently */
#define SMALL_SUPERBLOCK_SIZE 16  /* measured in blocks */
#define SMALL_BLOCK_SIZE      128 /* measured in bytes  */

#endif

/* Forward declaration */
bool content_load_state_in_progress(void* data);

/* Private functions */

static bool bsv_movie_init_playback(bsv_movie_t *handle, const char *path)
{
   int64_t *identifier_loc;
   uint32_t state_size         = 0;
   uint32_t header[REPLAY_HEADER_LEN] = {0};
   uint64_t header_size = REPLAY_HEADER_LEN_BYTES;
   uint32_t vsn                = 0;
   intfstream_t *file          = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("[Replay] Could not open replay file for playback: \"%s\".\n", path);
      return false;
   }

   handle->file              = file;
   handle->playback          = true;

   intfstream_read(handle->file, header, sizeof(uint32_t) * REPLAY_HEADER_LEN);
   if (swap_if_big32(header[REPLAY_HEADER_MAGIC_INDEX]) != REPLAY_MAGIC)
   {
      RARCH_ERR("[Replay] %s : %s : magic %d vs %d \n", msg_hash_to_str(MSG_MOVIE_FILE_IS_NOT_A_VALID_REPLAY_FILE), path, swap_if_big32(header[REPLAY_HEADER_MAGIC_INDEX]), REPLAY_MAGIC);
      return false;
   }
   vsn                = swap_if_big32(header[REPLAY_HEADER_VERSION_INDEX]);
   if (vsn > REPLAY_FORMAT_VERSION)
   {
      RARCH_ERR("[Replay] %s : vsn %d vs %d\n", msg_hash_to_str(MSG_MOVIE_FILE_IS_NOT_A_VALID_REPLAY_FILE), vsn, REPLAY_FORMAT_VERSION);
      return false;
   }
   if (vsn < 2)
      header_size = REPLAY_HEADER_V0V1_LEN_BYTES;
   handle->version    = vsn;

   state_size         = swap_if_big32(header[REPLAY_HEADER_STATE_SIZE_INDEX]);
   identifier_loc     = (int64_t *)(header+REPLAY_HEADER_IDENTIFIER_INDEX);
   handle->identifier = swap_if_big64(*identifier_loc);

   handle->min_file_pos = header_size + state_size;
   return bsv_movie_reset_playback(handle);
}

static bool bsv_movie_init_record(
      bsv_movie_t *handle, const char *path)
{
   size_t info_size;
   time_t t                     = time(NULL);
   time_t time_lil              = swap_if_big64(t);
   uint32_t state_size          = 0;
   uint32_t content_crc         = 0;
   uint32_t header[REPLAY_HEADER_LEN]  = {0};
   intfstream_t *file           = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   settings_t *settings         = config_get_ptr();
#ifdef HAVE_STATESTREAM
   bool is_small                = false;
   uint32_t superblock_size;
   uint32_t block_size;
#endif

   if (!file)
   {
      RARCH_ERR("[Replay] Could not open replay file for recording: \"%s\".\n", path);
      return false;
   }

   handle->file             = file;
   handle->version          = REPLAY_FORMAT_VERSION;
#ifdef HAVE_STATESTREAM
   handle->commit_interval  = REPLAY_DEFAULT_COMMIT_INTERVAL;
   handle->commit_threshold = REPLAY_DEFAULT_COMMIT_THRESHOLD;
#endif
   handle->checkpoint_compression = REPLAY_CHECKPOINT2_COMPRESSION_NONE;
   if (settings->bools.savestate_file_compression)
#if defined(HAVE_ZSTD)
      handle->checkpoint_compression = REPLAY_CHECKPOINT2_COMPRESSION_ZSTD;
#elif defined(HAVE_ZLIB)
      handle->checkpoint_compression = REPLAY_CHECKPOINT2_COMPRESSION_ZLIB;
#else
      {}
#endif

   content_crc              = content_get_crc();

   header[REPLAY_HEADER_MAGIC_INDEX] = swap_if_big32(REPLAY_MAGIC);
   header[REPLAY_HEADER_VERSION_INDEX] = swap_if_big32(handle->version);
   header[REPLAY_HEADER_CRC_INDEX] = swap_if_big32(content_crc);

   info_size                = core_serialize_size();
   state_size               = (unsigned)info_size;
#ifdef HAVE_STATESTREAM
   is_small                 = info_size < SMALL_STATE_THRESHOLD;
   superblock_size          = is_small ? SMALL_SUPERBLOCK_SIZE : DEFAULT_SUPERBLOCK_SIZE;
   block_size               = is_small ? SMALL_BLOCK_SIZE : DEFAULT_BLOCK_SIZE;
#endif
   header[REPLAY_HEADER_STATE_SIZE_INDEX]      = 0; /* Will fill this in later */
   header[REPLAY_HEADER_FRAME_COUNT_INDEX]     = 0;
#ifdef HAVE_STATESTREAM
   header[REPLAY_HEADER_BLOCK_SIZE_INDEX]      = swap_if_big32(block_size);
   header[REPLAY_HEADER_SUPERBLOCK_SIZE_INDEX] = swap_if_big32(superblock_size);
   header[REPLAY_HEADER_CHECKPOINT_CONFIG_INDEX] = (((uint32_t)handle->commit_interval) << 24) |
      ((uint32_t)handle->commit_threshold << 16) |
      (((uint32_t)handle->checkpoint_compression) << 8);
#else
   header[REPLAY_HEADER_BLOCK_SIZE_INDEX]      = 0;
   header[REPLAY_HEADER_SUPERBLOCK_SIZE_INDEX] = 0;
   header[REPLAY_HEADER_CHECKPOINT_CONFIG_INDEX] = 0;
#endif
   handle->identifier       = (int64_t)t;
   *((int64_t *)(header+REPLAY_HEADER_IDENTIFIER_INDEX)) = time_lil;
   intfstream_write(handle->file, header, REPLAY_HEADER_LEN_BYTES);

#ifdef HAVE_STATESTREAM
   handle->superblocks      = uint32s_index_new(superblock_size, handle->commit_interval, handle->commit_threshold);
   handle->blocks           = uint32s_index_new(block_size/4, handle->commit_interval, handle->commit_threshold);
#endif
   if (state_size)
      return bsv_movie_reset_recording(handle);
   
   handle->min_file_pos = sizeof(header);

   return true;
}

void bsv_movie_free(bsv_movie_t *handle)
{
   intfstream_close(handle->file);
   free(handle->file);

   free(handle->frame_pos);

#ifdef HAVE_STATESTREAM
   uint32s_index_free(handle->superblocks);
   uint32s_index_free(handle->blocks);
   free(handle->superblock_seq);
#endif
   if (handle->last_save)
      free(handle->last_save);
   if (handle->cur_save)
      free(handle->cur_save);

   free(handle);
}

static bsv_movie_t *bsv_movie_init_internal(const char *path, enum rarch_movie_type type)
{
   size_t *frame_pos   = NULL;
   bsv_movie_t *handle = (bsv_movie_t*)calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   /* Just pick something really large
    * ~1 million frames rewind should do the trick. */
   if (!(frame_pos = (size_t*)calloc((1 << 20), sizeof(size_t))))
      goto error;

   handle->frame_pos       = frame_pos;
   handle->frame_mask      = (1 << 20) - 1;

   if (type == RARCH_MOVIE_PLAYBACK)
   {
      if (!bsv_movie_init_playback(handle, path))
         goto error;
   }
   else if (!bsv_movie_init_record(handle, path))
      goto error;

   handle->frame_pos[0]    = handle->min_file_pos;

   return handle;

error:
   if (handle)
      bsv_movie_free(handle);
   return NULL;
}

static bool bsv_movie_start_record(input_driver_state_t * input_st, char *path)
{
   size_t _len;
   char msg[128];
   bsv_movie_t *state              = NULL;
   const char *_msg                = NULL;

   /* this should trigger a start recording task which on failure or
      success prints a message and on success sets the
      input_st->bsv_movie_state_handle. */
   if (!(state = bsv_movie_init_internal(path, RARCH_MOVIE_RECORD)))
   {
      const char *_msg = msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD);
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_ERR("[Replay] %s\n", _msg);
      return false;
   }

   bsv_movie_enqueue(input_st, state, BSV_FLAG_MOVIE_RECORDING);
   _msg  = msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO);
   _len  = strlcpy(msg, _msg, sizeof(msg));
   _len += snprintf(msg + _len, sizeof(msg) - _len, " \"%s\".", path);
   runloop_msg_queue_push(msg, _len, 2, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("[Replay] %s \"%s\".\n", _msg, path);

   return true;
}

static bool bsv_movie_start_playback(input_driver_state_t *input_st, char *path)
{
   bsv_movie_t *state                       = NULL;
   const char *_msg                         = NULL;
   /* This should trigger a start playback task which on failure or
      success prints a message and on success sets the
      input_st->bsv_movie_state_handle. */
   if (!(state = bsv_movie_init_internal(path, RARCH_MOVIE_PLAYBACK)))
   {
      RARCH_ERR("[Replay] %s: \"%s\".\n",
            msg_hash_to_str(MSG_FAILED_TO_LOAD_MOVIE_FILE),
            path);
      return false;
   }

   bsv_movie_enqueue(input_st, state, BSV_FLAG_MOVIE_PLAYBACK);
   _msg = msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK);

   runloop_msg_queue_push(_msg, strlen(_msg), 2, 180, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("[Replay] %s\n", _msg);

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
   uint8_t flg;
   /* trivial handler */
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   flg = task_get_flags(task);
   if (!task_get_error(task) && ((flg & RETRO_TASK_FLG_CANCELLED) > 0))
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
   uint8_t flg;
   /* Hang on until the state is loaded */
   if (content_load_state_in_progress(NULL))
      return;

   /* trivial handler */
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   flg = task_get_flags(task);
   if (!task_get_error(task) && ((flg & RETRO_TASK_FLG_CANCELLED) > 0))
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

/* In the future this should probably be a deferred task as well */
bool movie_stop_playback(input_driver_state_t *input_st)
{
   const char *_msg = NULL;
   /* Checks if movie is being played back. */
   if (!(input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK))
      return false;
#ifdef HAVE_STATESTREAM
#if DEBUG
   RARCH_DBG("[Replay] superblock histogram\n");
   uint32s_index_print_count_data(input_st->bsv_movie_state_handle->superblocks);
   RARCH_DBG("[Replay] block histogram\n");
   uint32s_index_print_count_data(input_st->bsv_movie_state_handle->blocks);
#endif
#endif
   _msg = msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED);
   runloop_msg_queue_push(_msg, strlen(_msg), 2, 180, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("[Replay] %s\n", _msg);

   bsv_movie_deinit_full(input_st);

   input_st->bsv_movie_state.flags &= ~(
         BSV_FLAG_MOVIE_END
         | BSV_FLAG_MOVIE_PLAYBACK);
   return true;
}
/* in the future this should probably be a deferred task as well */
bool movie_stop_record(input_driver_state_t *input_st)
{
   bsv_movie_t *movie = input_st->bsv_movie_state_handle;
   uint32_t frame_count;
   const char *_msg = msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED);
   if (!movie)
      return false;
   runloop_msg_queue_push(_msg, strlen(_msg), 2, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("[Replay] %s\n", _msg);
#ifdef HAVE_STATESTREAM
#if DEBUG
   RARCH_DBG("[Replay] superblock histogram\n");
   uint32s_index_print_count_data(movie->superblocks);
   RARCH_DBG("[Replay] block histogram\n");
   uint32s_index_print_count_data(movie->blocks);
#endif
#endif
   frame_count = swap_if_big32(movie->frame_counter);
   intfstream_seek(movie->file, REPLAY_HEADER_FRAME_COUNT_INDEX*sizeof(uint32_t), SEEK_SET);
   intfstream_write(movie->file, &frame_count, sizeof(uint32_t));
   bsv_movie_deinit_full(input_st);
   input_st->bsv_movie_state.flags &= ~(
         BSV_FLAG_MOVIE_END
         | BSV_FLAG_MOVIE_RECORDING);
   return true;

}

bool movie_stop(input_driver_state_t *input_st)
{
   if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK)
      return movie_stop_playback(input_st);
   else if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_RECORDING)
      return movie_stop_record(input_st);
   if (input_st->bsv_movie_state_handle)
      RARCH_ERR("[Replay] Didn't really stop movie!\n");
   return true;
}

bool movie_start_playback(input_driver_state_t *input_st, char *path)
{
  retro_task_t       *task      = task_init();
  moviectl_task_state_t *state  = (moviectl_task_state_t *)calloc(1, sizeof(*state));
  bool file_exists              = filestream_exists(path);

  if (task && state && file_exists)
  {
     *state                        = input_st->bsv_movie_state;
     strlcpy(state->movie_start_path, path, sizeof(state->movie_start_path));
     task->type                    = TASK_TYPE_NONE;
     task->state                   = state;
     task->handler                 = task_moviectl_playback_handler;
     task->callback                = moviectl_start_playback_cb;
     task->title                   = strdup(msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK));

     if (task_queue_push(task))
        return true;
  }

   if (state)
      free(state);
   if (task)
      free(task);

   return false;
}

bool movie_start_record(input_driver_state_t *input_st, char*path)
{
   const char *_msg              = msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO);
   retro_task_t       *task      = task_init();
   moviectl_task_state_t *state  = (moviectl_task_state_t *)calloc(1, sizeof(*state));

   if (task && state)
   {
      size_t _len;
      char msg[128];
      *state                     = input_st->bsv_movie_state;
      strlcpy(state->movie_start_path, path, sizeof(state->movie_start_path));

      _len                       = strlcpy(msg, _msg, sizeof(msg));
      snprintf(msg + _len, sizeof(msg) - _len, " \"%s\".", path);

      task->type                 = TASK_TYPE_NONE;
      task->state                = state;
      task->handler              = task_moviectl_record_handler;
      task->callback             = moviectl_start_record_cb;

      task->title                = strdup(msg);

      if (task_queue_push(task))
         return true;
   }

   if (state)
      free(state);
   if (task)
      free(task);

   return false;
}
#endif
