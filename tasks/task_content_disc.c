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
   DUMP_STATE_READ_TRACK
};

typedef struct
{
   int64_t cur_track_bytes;
   int64_t track_written_bytes;
   int64_t disc_total_bytes;
   int64_t disc_read_bytes;

   RFILE *file;
   RFILE *output_file;
   libretro_vfs_implementation_file *stream;
   const cdrom_toc_t *toc;

   enum cdrom_dump_state state;
   unsigned char cur_track;
   char drive_letter[2];
   char cdrom_path[64];
   char title[512];
   bool next;
} task_cdrom_dump_state_t;

static void task_cdrom_dump_handler(retro_task_t *task)
{
   task_cdrom_dump_state_t *state    = (task_cdrom_dump_state_t*)task->state;

   if (task_get_progress(task) == 100)
   {
      if (state->file)
         filestream_close(state->file);
      if (state->output_file)
         filestream_close(state->output_file);
      state->file        = NULL;
      state->output_file = NULL;

      task_set_finished(task, true);

      RARCH_LOG("[CDROM]: Dump finished.\n");

      return;
   }

   switch (state->state)
   {
      case DUMP_STATE_TOC_PENDING:
         {
            /* open cuesheet file from drive */
            char cue_path[PATH_MAX_LENGTH] = {0};

            cdrom_device_fillpath(cue_path, sizeof(cue_path), state->drive_letter[0], 0, true);

            state->file = filestream_open(cue_path, RETRO_VFS_FILE_ACCESS_READ, 0);

            if (!state->file)
            {
               RARCH_ERR("[CDROM]: Error opening file for reading: %s\n", cue_path);
               task_set_progress(task, 100);
               task_free_title(task);
               task_set_title(task, strdup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED)));
               return;
            }

            state->state = DUMP_STATE_WRITE_CUE;

            break;
         }
      case DUMP_STATE_WRITE_CUE:
         {
            size_t _len;
            char output_file[PATH_MAX_LENGTH];
            char cue_filename[PATH_MAX_LENGTH];
            settings_t              *settings = config_get_ptr();
            const char *directory_core_assets = settings 
               ? settings->paths.directory_core_assets : NULL;
            /* write cuesheet to a file */
            int64_t cue_size     = filestream_get_size(state->file);
            char *cue_data       = (char*)calloc(1, cue_size);

            filestream_read(state->file, cue_data, cue_size);

            state->stream        = filestream_get_vfs_handle(state->file);
            state->toc           = retro_vfs_file_get_cdrom_toc();

            if (cdrom_has_atip(state->stream))
               RARCH_LOG("[CDROM]: This disc is not genuine.\n");

            filestream_close(state->file);

            _len                 = strlcpy(cue_filename,
                                   state->title, sizeof(cue_filename));
            cue_filename[_len  ] = '.';
            cue_filename[_len+1] = 'c';
            cue_filename[_len+2] = 'u';
            cue_filename[_len+3] = 'e';
            cue_filename[_len+4] = '\0';

            fill_pathname_join_special(output_file,
                  directory_core_assets, cue_filename, sizeof(output_file));

            {
               RFILE         *file = filestream_open(output_file, RETRO_VFS_FILE_ACCESS_WRITE, 0);
               unsigned char point = 0;

               if (!file)
               {
                  RARCH_ERR("[CDROM]: Error opening file for writing: %s\n", output_file);
                  task_set_progress(task, 100);
                  task_free_title(task);
                  task_set_title(task, strdup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED)));
                  return;
               }

               for (point = 1; point <= state->toc->num_tracks; point++)
               {
                  const char *track_type = "MODE1/2352";
                  char track_filename[PATH_MAX_LENGTH];

                  state->disc_total_bytes += state->toc->track[point - 1].track_bytes;

                  track_filename[0] = '\0';

                  if (state->toc->track[point - 1].audio)
                     track_type = "AUDIO";
                  else if (state->toc->track[point - 1].mode == 1)
                     track_type = "MODE1/2352";
                  else if (state->toc->track[point - 1].mode == 2)
                     track_type = "MODE2/2352";

                  snprintf(track_filename, sizeof(track_filename), "%s (Track %02d).bin", state->title, point);

                  filestream_printf(file, "FILE \"%s\" BINARY\n", track_filename);
                  filestream_printf(file, "  TRACK %02d %s\n", point, track_type);

                  {
                     unsigned pregap_lba_len = state->toc->track[point - 1].lba - state->toc->track[point - 1].lba_start;

                     if (state->toc->track[point - 1].audio && pregap_lba_len > 0)
                     {
                        unsigned char min = 0;
                        unsigned char sec = 0;
                        unsigned char frame = 0;

                        cdrom_lba_to_msf(pregap_lba_len, &min, &sec, &frame);

                        filestream_printf(file, "    INDEX 00 00:00:00\n");
                        filestream_printf(file, "    INDEX 01 %02u:%02u:%02u\n", (unsigned)min, (unsigned)sec, (unsigned)frame);
                     }
                     else
                        filestream_printf(file, "    INDEX 01 00:00:00\n");
                  }
               }

               filestream_close(file);
            }

            state->file  = NULL;
            state->state = DUMP_STATE_NEXT_TRACK;

            free(cue_data);
         }
         break;
      case DUMP_STATE_NEXT_TRACK:
         {
            /* no file is open as we either just started or just finished a track, need to start dumping the next track */
            state->cur_track++;

            /* no more tracks to dump, we're done */
            if (state->toc && state->cur_track > state->toc->num_tracks)
            {
               task_set_progress(task, 100);
               return;
            }

            RARCH_LOG("[CDROM]: Dumping track %d...\n", state->cur_track);

            memset(state->cdrom_path, 0, sizeof(state->cdrom_path));

            cdrom_device_fillpath(state->cdrom_path, sizeof(state->cdrom_path), state->drive_letter[0], state->cur_track, false);

            state->track_written_bytes = 0;
            state->file = filestream_open(state->cdrom_path, RETRO_VFS_FILE_ACCESS_READ, 0);

            /* open a new file for writing for this next track */
            if (state->file)
            {
               char output_path[PATH_MAX_LENGTH];
               char track_filename[PATH_MAX_LENGTH];
               settings_t              *settings = config_get_ptr();
               const char *directory_core_assets = settings 
                  ? settings->paths.directory_core_assets : NULL;

               track_filename[0] = '\0';

               snprintf(track_filename, sizeof(track_filename), "%s (Track %02d).bin", state->title, state->cur_track);

               state->cur_track_bytes = filestream_get_size(state->file);

               fill_pathname_join_special(output_path,
                     directory_core_assets, track_filename, sizeof(output_path));

               if (!(state->output_file = filestream_open(output_path, RETRO_VFS_FILE_ACCESS_WRITE, 0)))
               {
                  RARCH_ERR("[CDROM]: Error opening file for writing: %s\n", output_path);
                  task_set_progress(task, 100);
                  task_free_title(task);
                  task_set_title(task, strdup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED)));
                  return;
               }
            }
            else
            {
               RARCH_ERR("[CDROM]: Error opening file for writing: %s\n", state->cdrom_path);
               task_set_progress(task, 100);
               task_free_title(task);
               task_set_title(task, strdup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED)));
               return;
            }

            state->state = DUMP_STATE_READ_TRACK;
         }
         break;
      case DUMP_STATE_READ_TRACK:
         /* read data from track and write it to a file in chunks */
         if (state->cur_track_bytes > state->track_written_bytes)
         {
            char data[2352 * 2] = {0};
            int64_t read_bytes  = filestream_read(state->file, data, sizeof(data));
            int progress        = 0;

            if (read_bytes <= 0)
            {
               task_set_progress(task, 100);
               task_free_title(task);
               task_set_title(task, strdup(msg_hash_to_str(MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE)));
               return;
            }

            state->track_written_bytes += read_bytes;
            state->disc_read_bytes += read_bytes;
            progress = (state->disc_read_bytes / (double)state->disc_total_bytes) * 100.0;

#ifdef CDROM_DEBUG
            RARCH_LOG("[CDROM]: Read %" PRId64 " bytes, totalling %" PRId64 " of %" PRId64 " bytes. Progress: %d%%\n", read_bytes, state->track_written_bytes, state->cur_track_bytes, progress);
#endif

            if (filestream_write(state->output_file, data, read_bytes) <= 0)
            {
               task_set_progress(task, 100);
               task_free_title(task);
               task_set_title(task, strdup(msg_hash_to_str(MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK)));
               return;
            }

            task_set_progress(task, progress);
         }
         else if (state->cur_track_bytes == state->track_written_bytes)
         {
            /* TODO: FIXME: this stops after only the first track */
            if (state->file)
            {
               filestream_close(state->file);
               state->file = NULL;
            }
            if (state->output_file)
            {
               filestream_close(state->output_file);
               state->file = NULL;
            }

            state->state = DUMP_STATE_NEXT_TRACK;
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
      free(state);
}

void task_push_cdrom_dump(const char *drive)
{
   retro_task_t *task             = task_init();
   task_cdrom_dump_state_t *state = (task_cdrom_dump_state_t*)calloc(1, sizeof(*state));

   state->drive_letter[0]         = drive[0];
   state->next                    = true;
   state->cur_track               = 0;
   state->state                   = DUMP_STATE_TOC_PENDING;

   fill_str_dated_filename(state->title, "cdrom", NULL, sizeof(state->title));

   task->state                    = state;
   task->handler                  = task_cdrom_dump_handler;
   task->callback                 = task_cdrom_dump_callback;
   task->title                    = strdup(msg_hash_to_str(MSG_DUMPING_DISC));

   RARCH_LOG("[CDROM]: Starting disc dump...\n");

   task_queue_push(task);
}
#endif
