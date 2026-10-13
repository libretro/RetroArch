/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>

#include <file/file_path.h>
#include <queues/task_queue.h>
#include <string/stdstring.h>

#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#define setmode _setmode
#define INVALID_FILE_ATTRIBUTES -1
#else
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#endif
#endif

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <boolean.h>

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <string/stdstring.h>

#include <vfs/vfs_implementation.h>
#ifdef HAVE_CDROM
#include <vfs/vfs_implementation_cdrom.h>
#endif

#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <memalign.h>

#include <string/stdstring.h>

#include "task_content.h"
#include "tasks_internal.h"

#include "../configuration.h"
#include "../msg_hash.h"
#include "../verbosity.h"

#ifdef HAVE_CDROM
enum cdrom_dump_state
{
   DUMP_STATE_TOC_PENDING = 0,
   DUMP_STATE_WRITE_CUE,
   DUMP_STATE_NEXT_TRACK,
   DUMP_STATE_READ_TRACK,
   DUMP_STATE_READ_IMAGE
};

typedef struct
{
   int64_t cur_track_bytes;
   int64_t track_written_bytes;
   int64_t disc_total_bytes;
   int64_t disc_read_bytes;

   void *data;
   RFILE *file;
   RFILE *output_file;
   libretro_vfs_implementation_file *stream;
   cdrom_toc_t toc;

   enum cdrom_dump_state state;
   unsigned char cur_track;
   char drive_letter[2];
   char cdrom_path[64];
   char output_path[PATH_MAX_LENGTH];
   char output_temp_path[PATH_MAX_LENGTH];
   char cue_path[PATH_MAX_LENGTH];
   char title[NAME_MAX_LENGTH];
   /* Captured on the main thread at push: the handler runs on the
    * threaded task queue's worker and writes dumps under this, never
    * under a live settings read. */
   char dir_core_assets[DIR_MAX_LENGTH];
   bool next;
   bool failed;
   bool output_pending;
   bool cue_pending;
} task_cdrom_dump_state_t;

static void task_cdrom_dump_fail(retro_task_t *task, enum msg_hash_enums msg)
{
   task_cdrom_dump_state_t *state = (task_cdrom_dump_state_t*)task->state;
   state->failed = true;
   task_set_error(task, strdup(msg_hash_to_str(msg)));
   task_set_progress(task, 100);
   task_free_title(task);
   task_set_title(task, strdup(msg_hash_to_str(msg)));
}

static bool task_cdrom_dump_printf(task_cdrom_dump_state_t *state,
      RFILE *file, const char *fmt, ...)
{
   va_list ap;
   int len;
   va_start(ap, fmt);
   len = vsnprintf((char*)state->data, 32 * 2048, fmt, ap);
   va_end(ap);
   return len >= 0 && len < 32 * 2048
      && filestream_write(file, state->data, len) == len;
}

/* Compare geometry, not padding or device timeout preferences. This catches
 * changed layouts between track opens; it is not a media identity check. */
static bool task_cdrom_dump_same_geometry(const cdrom_toc_t *a,
      const cdrom_toc_t *b)
{
   unsigned i;
   if (!a || !b || !a->num_tracks || a->num_tracks > 99
         || a->num_tracks != b->num_tracks)
      return false;
   for (i = 0; i < a->num_tracks; i++)
   {
      const cdrom_track_t *x = &a->track[i];
      const cdrom_track_t *y = &b->track[i];
      if (x->lba_start != y->lba_start || x->lba != y->lba
            || x->track_size != y->track_size || x->track_bytes != y->track_bytes
            || x->track_num != y->track_num || x->min != y->min
            || x->sec != y->sec || x->frame != y->frame
            || x->mode != y->mode || x->audio != y->audio)
         return false;
   }
   return true;
}

/* Native filestream close consumes the handle on both success and failure. */
static int task_cdrom_dump_close(RFILE **file)
{
   RFILE *owned = *file;
   *file = NULL;
   return owned ? filestream_close(owned) : 0;
}

static void task_cdrom_dump_handler(retro_task_t *task)
{
   task_cdrom_dump_state_t *state    = (task_cdrom_dump_state_t*)task->state;

   if (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
      task_set_progress(task, 100);

   if (task_get_progress(task) == 100)
   {
      int read_error  = task_cdrom_dump_close(&state->file);
      int write_error = task_cdrom_dump_close(&state->output_file);
      state->stream = NULL;
      if (!state->failed && !(task_get_flags(task) & RETRO_TASK_FLG_CANCELLED))
      {
         if (write_error)
            task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK);
         else if (read_error)
            task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE);
      }

      if (state->output_pending)
      {
         bool publish = !state->failed &&
            !(task_get_flags(task) & RETRO_TASK_FLG_CANCELLED);
         state->output_pending = false;
         if (filestream_finish_atomic(state->output_path, publish) != 0)
         {
            filestream_finish_atomic(state->output_path, false);
            if (publish)
               task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK);
         }
      }

      if (state->cue_pending)
      {
         bool publish = !state->failed &&
            !(task_get_flags(task) & RETRO_TASK_FLG_CANCELLED);
         state->cue_pending = false;
         if (filestream_finish_atomic(state->cue_path, publish) != 0)
         {
            filestream_finish_atomic(state->cue_path, false);
            if (publish)
               task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK);
         }
      }

      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);

      if (!state->failed && !(task_get_flags(task) & RETRO_TASK_FLG_CANCELLED))
         RARCH_LOG("[CDROM] Dump finished.\n");

      return;
   }

   switch (state->state)
   {
      case DUMP_STATE_TOC_PENDING:
         {
            /* open cuesheet file from drive */
            char cue_path[64] = {0};
            uint16_t profile = 0;
            RFILE *probe;

            if (!state->data)
            {
               state->data = memalign_alloc(4096, 32 * 2048);
               if (!state->data)
               {
                  task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE);
                  return;
               }
            }
            cdrom_device_filldevicepath(cue_path, sizeof(cue_path),
                  state->drive_letter[0]);
            probe = filestream_open(cue_path, RETRO_VFS_FILE_ACCESS_READ, 0);
            if (probe)
            {
               cdrom_get_media_profile(filestream_get_vfs_handle(probe), &profile);
               filestream_close(probe);
            }
            if ((profile >= 0x10 && profile <= 0x17)
                  || profile == 0x1a || profile == 0x1b
                  || profile == 0x2a || profile == 0x2b)
            {
               char *output_path = state->output_path;
               char filename[NAME_MAX_LENGTH];
               cdrom_device_fillimagepath(cue_path, sizeof(cue_path),
                     state->drive_letter[0]);
               state->file = filestream_open(cue_path, RETRO_VFS_FILE_ACCESS_READ, 0);
               if (!state->file)
               {
                  task_cdrom_dump_fail(task, MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED);
                  return;
               }
               state->disc_total_bytes = filestream_get_size(state->file);
               fill_pathname(filename, state->title, ".iso", sizeof(filename));
               fill_pathname_join_special(output_path, state->dir_core_assets,
                     filename, sizeof(state->output_path));
               if (state->disc_total_bytes <= 0 || state->disc_total_bytes % 2048 ||
                     !filestream_atomic_temp_name(state->output_temp_path,
                        sizeof(state->output_temp_path), output_path))
               {
                  task_cdrom_dump_fail(task, MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED);
                  return;
               }
               state->output_file = filestream_open(state->output_temp_path,
                     RETRO_VFS_FILE_ACCESS_WRITE, 0);
               state->output_pending = state->output_file != NULL;
               if (!state->output_file)
               {
                  task_cdrom_dump_fail(task, MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED);
                  return;
               }
               state->state = DUMP_STATE_READ_IMAGE;
               break;
            }

            cdrom_device_fillpath(cue_path, sizeof(cue_path), state->drive_letter[0], 0, true);

            state->file = filestream_open(cue_path, RETRO_VFS_FILE_ACCESS_READ, 0);

            if (!state->file)
            {
               RARCH_ERR("[CDROM] Error opening file for reading: %s.\n", cue_path);
               task_cdrom_dump_fail(task, MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED);
               return;
            }

            state->state = DUMP_STATE_WRITE_CUE;

            break;
         }
      case DUMP_STATE_WRITE_CUE:
         {
            char *output_file = state->output_path;
            char cue_filename[NAME_MAX_LENGTH + 8];
            const char *directory_core_assets = state->dir_core_assets;
            const cdrom_toc_t *toc;
            state->stream        = filestream_get_vfs_handle(state->file);
            toc = retro_vfs_file_get_cdrom_toc(state->stream);
            if (!toc)
            {
               task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE);
               return;
            }
            memcpy(&state->toc, toc, sizeof(state->toc));

            if (cdrom_has_atip(state->stream))
               RARCH_LOG("[CDROM] This disc is not genuine.\n");

            state->stream = NULL;
            if (task_cdrom_dump_close(&state->file) != 0)
            {
               task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE);
               return;
            }

            fill_pathname(cue_filename, state->title,
                  ".cue", sizeof(cue_filename));

            fill_pathname_join_special(output_file,
                  directory_core_assets, cue_filename, sizeof(state->output_path));

            {
               RFILE         *file;
               unsigned char point = 0;
               bool cue_ok = true;

               strlcpy(state->cue_path, output_file, sizeof(state->cue_path));
               if (!filestream_atomic_temp_name(state->output_temp_path,
                        sizeof(state->output_temp_path), output_file))
               {
                  task_cdrom_dump_fail(task, MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED);
                  return;
               }
               file = filestream_open(state->output_temp_path,
                     RETRO_VFS_FILE_ACCESS_WRITE, 0);
               state->cue_pending = file != NULL;
               if (!file)
               {
                  RARCH_ERR("[CDROM] Error opening file for writing: %s.\n", output_file);
                  task_cdrom_dump_fail(task, MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED);
                  return;
               }

               for (point = 1; point <= state->toc.num_tracks; point++)
               {
                  const char *track_type = "MODE1/2352";
                  char track_filename[PATH_MAX_LENGTH];

                  state->disc_total_bytes += state->toc.track[point - 1].track_bytes;

                  track_filename[0] = '\0';

                  if (state->toc.track[point - 1].audio)
                     track_type = "AUDIO";
                  else if (state->toc.track[point - 1].mode == 1)
                     track_type = "MODE1/2352";
                  else if (state->toc.track[point - 1].mode == 2)
                     track_type = "MODE2/2352";

                  snprintf(track_filename, sizeof(track_filename), "%s (Track %02d).bin", state->title, point);

                  cue_ok &= task_cdrom_dump_printf(state, file, "FILE \"%s\" BINARY\n", track_filename);
                  cue_ok &= task_cdrom_dump_printf(state, file, "  TRACK %02d %s\n", point, track_type);

                  {
                     unsigned pregap_lba_len = state->toc.track[point - 1].lba - state->toc.track[point - 1].lba_start;

                     if (state->toc.track[point - 1].audio && pregap_lba_len > 0)
                     {
                        unsigned char min = 0;
                        unsigned char sec = 0;
                        unsigned char frame = 0;

                        cdrom_lba_to_msf(pregap_lba_len, &min, &sec, &frame);

                        cue_ok &= task_cdrom_dump_printf(state, file, "    INDEX 00 00:00:00\n");
                        cue_ok &= task_cdrom_dump_printf(state, file, "    INDEX 01 %02u:%02u:%02u\n", (unsigned)min, (unsigned)sec, (unsigned)frame);
                     }
                     else
                        cue_ok &= task_cdrom_dump_printf(state, file, "    INDEX 01 00:00:00\n");
                  }
               }

               if (!cue_ok || filestream_flush(file) != 0)
               {
                  filestream_close(file);
                  task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK);
                  return;
               }
               if (task_cdrom_dump_close(&file) != 0)
               {
                  task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK);
                  return;
               }
            }

            state->file  = NULL;
            state->state = DUMP_STATE_NEXT_TRACK;

         }
         break;
      case DUMP_STATE_NEXT_TRACK:
         {
            /* no file is open as we either just started or just finished a track, need to start dumping the next track */
            state->cur_track++;

            /* no more tracks to dump, we're done */
            if (state->cur_track > state->toc.num_tracks)
            {
               task_set_progress(task, 100);
               return;
            }

            RARCH_LOG("[CDROM] Dumping track %d...\n", state->cur_track);

            memset(state->cdrom_path, 0, sizeof(state->cdrom_path));

            cdrom_device_fillpath(state->cdrom_path, sizeof(state->cdrom_path), state->drive_letter[0], state->cur_track, false);

            state->track_written_bytes = 0;
            state->file = filestream_open(state->cdrom_path, RETRO_VFS_FILE_ACCESS_READ, 0);

            /* open a new file for writing for this next track */
            if (state->file)
            {
               char *output_path = state->output_path;
               char track_filename[PATH_MAX_LENGTH];
               const char *directory_core_assets = state->dir_core_assets;

               if (!task_cdrom_dump_same_geometry(&state->toc,
                        retro_vfs_file_get_cdrom_toc(
                           filestream_get_vfs_handle(state->file))))
               {
                  task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE);
                  return;
               }
               track_filename[0] = '\0';

               snprintf(track_filename, sizeof(track_filename), "%s (Track %02d).bin", state->title, state->cur_track);

               state->cur_track_bytes = filestream_get_size(state->file);
               if (state->cur_track_bytes <= 0 ||
                     state->cur_track_bytes != state->toc.track[state->cur_track - 1].track_bytes)
               {
                  task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE);
                  return;
               }

               fill_pathname_join_special(output_path,
                     directory_core_assets, track_filename, sizeof(state->output_path));

               if (!filestream_atomic_temp_name(state->output_temp_path,
                        sizeof(state->output_temp_path), output_path))
               {
                  task_cdrom_dump_fail(task, MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED);
                  return;
               }
               state->output_file = filestream_open(state->output_temp_path,
                     RETRO_VFS_FILE_ACCESS_WRITE, 0);
               state->output_pending = state->output_file != NULL;
               if (!state->output_file)
               {
                  RARCH_ERR("[CDROM] Error opening file for writing: %s.\n", output_path);
                  task_cdrom_dump_fail(task, MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED);
                  return;
               }
            }
            else
            {
               RARCH_ERR("[CDROM] Error opening file for writing: %s.\n", state->cdrom_path);
               task_cdrom_dump_fail(task, MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED);
               return;
            }

            state->state = DUMP_STATE_READ_TRACK;
         }
         break;
      case DUMP_STATE_READ_TRACK:
         /* read data from track and write it to a file in chunks */
         if (state->cur_track_bytes > state->track_written_bytes)
         {
            int64_t remaining = state->cur_track_bytes - state->track_written_bytes;
            int64_t want = remaining > 2352 * 2 ? 2352 * 2 : remaining;
            int64_t read_bytes = filestream_read(state->file, state->data, want);
            int progress        = 0;

            if (read_bytes != want)
            {
               task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE);
               return;
            }

            if (filestream_write(state->output_file, state->data, read_bytes) != read_bytes)
            {
               task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK);
               return;
            }

            state->track_written_bytes += read_bytes;
            state->disc_read_bytes += read_bytes;
            progress = (state->disc_read_bytes / (double)state->disc_total_bytes) * 99.0;

#ifdef CDROM_DEBUG
            RARCH_LOG("[CDROM] Read %" PRId64 " bytes, totalling %" PRId64 " of %" PRId64 " bytes. Progress: %d%%\n", read_bytes, state->track_written_bytes, state->cur_track_bytes, progress);
#endif

            task_set_progress(task, progress);
         }
         else if (state->cur_track_bytes == state->track_written_bytes)
         {
            if (task_cdrom_dump_close(&state->file) != 0)
            {
               task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE);
               return;
            }
            if (state->output_file)
            {
               if (filestream_flush(state->output_file) != 0)
               {
                  task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK);
                  return;
               }
               if (task_cdrom_dump_close(&state->output_file) != 0)
               {
                  task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK);
                  return;
               }
            }

            if (filestream_finish_atomic(state->output_path, true) != 0)
            {
               task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK);
               return;
            }
            state->output_pending = false;
            state->state = DUMP_STATE_NEXT_TRACK;
         }
         break;
      case DUMP_STATE_READ_IMAGE:
         {
            int64_t remaining = state->disc_total_bytes - state->disc_read_bytes;
            int64_t bytes;
            int64_t want = remaining > 32 * 2048 ? 32 * 2048 : remaining;
            if (!remaining)
            {
               if (filestream_flush(state->output_file) != 0)
                  task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK);
               task_set_progress(task, 100);
               return;
            }
            bytes = filestream_read(state->file, state->data, want);
            if (bytes != want)
            {
               task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE);
               return;
            }
            if (filestream_write(state->output_file, state->data, bytes) != bytes)
            {
               task_cdrom_dump_fail(task, MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK);
               return;
            }
            state->disc_read_bytes += bytes;
            task_set_progress(task, (int8_t)((state->disc_read_bytes /
                     (double)state->disc_total_bytes) * 99.0));
         }
         break;
   }
}

static void task_cdrom_dump_callback(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error)
{
   task_cdrom_dump_state_t *state = (task_cdrom_dump_state_t*)task->state;

   if (state)
   {
      if (state->data)
         memalign_free(state->data);
      free(state);
   }
}

void task_push_cdrom_dump(const char *drive)
{
   retro_task_t *task             = task_init();
   task_cdrom_dump_state_t *state = (task_cdrom_dump_state_t*)calloc(1, sizeof(*state));

   if (!task || !state || !drive || !*drive)
   {
      free(task);
      free(state);
      return;
   }
   state->drive_letter[0]         = drive[0];
   state->next                    = true;
   state->cur_track               = 0;
   state->state                   = DUMP_STATE_TOC_PENDING;
   strlcpy(state->dir_core_assets,
         config_get_ptr()->paths.directory_core_assets,
         sizeof(state->dir_core_assets));

   fill_str_dated_filename(state->title, "cdrom", NULL, sizeof(state->title));

   task->state                    = state;
   task->handler                  = task_cdrom_dump_handler;
   task->callback                 = task_cdrom_dump_callback;
   task->title                    = strdup(msg_hash_to_str(MSG_DUMPING_DISC));

   RARCH_LOG("[CDROM] Starting disc dump...\n");

   task_queue_push(task);
}
#endif
