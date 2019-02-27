/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (runtime_file.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <retro_miscellaneous.h>
#include <file_path_special.h>
#include <dirs.h>
#include <core_info.h>
#include <configuration.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <verbosity.h>

#include <file/runtime_file.h>

#define LOG_FILE_FORMAT_STR "%u:%02u:%02u\n%04u-%02u-%02u %02u:%02u:%02u\n"

/* Initialisation */

/* Parses log file referenced by runtime_log->path.
 * Does nothing if log file does not exist. */
static void runtime_log_read_file(runtime_log_t *runtime_log)
{
   unsigned runtime_hours = 0;
   unsigned runtime_minutes = 0;
   unsigned runtime_seconds = 0;
   
   unsigned last_played_year = 0;
   unsigned last_played_month = 0;
   unsigned last_played_day = 0;
   unsigned last_played_hour = 0;
   unsigned last_played_minute = 0;
   unsigned last_played_second = 0;
   
   int ret = 0;
   RFILE *file = NULL;
   
   /* Check if log file exists */
   if (!filestream_exists(runtime_log->path))
      return;
   
   /* Attempt to open log file */
   file = filestream_open(runtime_log->path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   
   if (!file)
   {
      RARCH_ERR("Failed to open runtime log file: %s\n", runtime_log->path);
      return;
   }
   
   /* Parse log file */
   ret = filestream_scanf(file, LOG_FILE_FORMAT_STR,
               &runtime_hours, &runtime_minutes, &runtime_seconds,
               &last_played_year, &last_played_month, &last_played_day,
               &last_played_hour, &last_played_minute, &last_played_second);
   
   if (ret == 9)
   {
      /* All is well - assign values to runtime_log object */
      runtime_log->runtime.hours = runtime_hours;
      runtime_log->runtime.minutes = runtime_minutes;
      runtime_log->runtime.seconds = runtime_seconds;
      
      runtime_log->last_played.year = last_played_year;
      runtime_log->last_played.month = last_played_month;
      runtime_log->last_played.day = last_played_day;
      runtime_log->last_played.hour = last_played_hour;
      runtime_log->last_played.minute = last_played_minute;
      runtime_log->last_played.second = last_played_second;
   }
   else
      RARCH_ERR("Invalid runtime log file: %s\n", runtime_log->path);
   
   /* Close log file */
   filestream_close(file);
}

/* Initialise runtime log, loading current parameters
 * if log file exists. Returned object must be free()'d.
 * Returns NULL if content_path and/or core_path are invalid */
runtime_log_t *runtime_log_init(const char *content_path, const char *core_path)
{
   settings_t *settings = config_get_ptr();
   core_info_list_t *core_info = NULL;
   runtime_log_t *runtime_log = NULL;
   
   const char *savefile_dir = dir_get(RARCH_DIR_SAVEFILE);
   char content_name[PATH_MAX_LENGTH];
   char core_name[PATH_MAX_LENGTH];
   char log_file_dir[PATH_MAX_LENGTH];
   char log_file_path[PATH_MAX_LENGTH];
   
   unsigned i;
   
   content_name[0] = '\0';
   core_name[0] = '\0';
   log_file_dir[0] = '\0';
   log_file_path[0] = '\0';
   
   /* Error checking */
   if (!settings)
      return NULL;
   
   if (string_is_empty(content_path) || string_is_empty(core_path) || string_is_empty(savefile_dir))
      return NULL;
   
   if (string_is_equal(core_path, "builtin"))
      return NULL;
   
   /* Get core name */
   core_info_get_list(&core_info);
   
   if (!core_info)
      return NULL;
   
   for (i = 0; i < core_info->count; i++)
   {
      if (string_is_equal(core_info->list[i].path, core_path))
      {
         strlcpy(core_name, core_info->list[i].core_name, sizeof(core_name));
         break;
      }
   }
   
   if (string_is_empty(core_name))
      return NULL;
   
   /* Get runtime log directory */
   if (settings->bools.sort_savefiles_enable)
   {
      fill_pathname_join(
            log_file_dir,
            savefile_dir,
            core_name,
            sizeof(log_file_dir));
   }
   else
   {
      strlcpy(log_file_dir, savefile_dir, sizeof(log_file_dir));
   }
   
   if (string_is_empty(log_file_dir))
      return NULL;
   
   /* Create directory, if required */
   if (!path_is_directory(log_file_dir))
   {
      path_mkdir(log_file_dir);
      
      if(!path_is_directory(log_file_dir))
      {
         RARCH_ERR("Failed to create directory for runtime log: %s.\n", log_file_dir);
         return NULL;
      }
   }
   
   /* Get content name
    * Note: TyrQuake requires a specific hack, since all
    * content has the same name... */
   if (string_is_equal(core_name, "TyrQuake"))
   {
      const char *last_slash = find_last_slash(content_path);
      if (last_slash)
      {
         size_t path_length = last_slash + 1 - content_path;
         if (path_length < PATH_MAX_LENGTH)
         {
            char tmp[PATH_MAX_LENGTH];
            memset(tmp, 0, sizeof(tmp));
            strlcpy(tmp, content_path, path_length * sizeof(char));
            strlcpy(content_name, path_basename(tmp), sizeof(content_name));
         }
      }
   }
   else
   {
      /* path_remove_extension() requires a char * (not const)
       * so have to use a temporary buffer... */
      char *tmp = strdup(path_basename(content_path));
      
      strlcpy(
            content_name,
            path_remove_extension(tmp),
            sizeof(content_name));
      
      if (!string_is_empty(tmp))
         free(tmp);
   }
   
   if (string_is_empty(content_name))
      return NULL;
   
   /* Build final log file path */
   fill_pathname_join(log_file_path, log_file_dir, content_name, sizeof(log_file_path));
   
   if (!settings->bools.sort_savefiles_enable)
   {
      strlcat(log_file_path, " - ", sizeof(log_file_path));
      strlcat(log_file_path, core_name, sizeof(log_file_path));
   }
   
   strlcat(log_file_path, file_path_str(FILE_PATH_RUNTIME_EXTENSION), sizeof(log_file_path));
   
   if (string_is_empty(log_file_path))
      return NULL;
   
   /* Phew... If we get this far then all is well.
    * > Create 'runtime_log' object */
   runtime_log = (runtime_log_t*)calloc(1, sizeof(*runtime_log));
   if (!runtime_log)
      return NULL;
   
   /* > Populate default values */
   runtime_log->runtime.hours = 0;
   runtime_log->runtime.minutes = 0;
   runtime_log->runtime.seconds = 0;
   
   runtime_log->last_played.year = 0;
   runtime_log->last_played.month = 0;
   runtime_log->last_played.day = 0;
   runtime_log->last_played.hour = 0;
   runtime_log->last_played.minute = 0;
   runtime_log->last_played.second = 0;
   
   strlcpy(runtime_log->path, log_file_path, sizeof(runtime_log->path));
   
   /* Load existing log file, if it exists */
   runtime_log_read_file(runtime_log);
   
   return runtime_log;
}

/* Setters */

/* Set runtime to specified hours, minutes, seconds value */
void runtime_log_set_runtime_hms(runtime_log_t *runtime_log, unsigned hours, unsigned minutes, unsigned seconds)
{
   retro_time_t usec;
   
   if (!runtime_log)
      return;
   
   /* Converting to usec and back again may be considered a
    * waste of CPU cycles, but this allows us to handle any
    * kind of broken input without issue - i.e. user can enter
    * minutes and seconds values > 59, and everything still
    * works correctly */
   runtime_log_convert_hms2usec(hours, minutes, seconds, &usec);
   
   runtime_log_convert_usec2hms(usec,
         &runtime_log->runtime.hours, &runtime_log->runtime.minutes, &runtime_log->runtime.seconds);
}

/* Set runtime to specified microseconds value */
void runtime_log_set_runtime_usec(runtime_log_t *runtime_log, retro_time_t usec)
{
   if (!runtime_log)
      return;
   
   runtime_log_convert_usec2hms(usec,
         &runtime_log->runtime.hours, &runtime_log->runtime.minutes, &runtime_log->runtime.seconds);
}

/* Adds specified hours, minutes, seconds value to current runtime */
void runtime_log_add_runtime_hms(runtime_log_t *runtime_log, unsigned hours, unsigned minutes, unsigned seconds)
{
   retro_time_t usec_old;
   retro_time_t usec_new;
   
   if (!runtime_log)
      return;
   
   runtime_log_convert_hms2usec(
         runtime_log->runtime.hours, runtime_log->runtime.minutes, runtime_log->runtime.seconds,
         &usec_old);
   
   runtime_log_convert_hms2usec(hours, minutes, seconds, &usec_new);
   
   runtime_log_convert_usec2hms(usec_old + usec_new,
         &runtime_log->runtime.hours, &runtime_log->runtime.minutes, &runtime_log->runtime.seconds);
}

/* Adds specified microseconds value to current runtime */
void runtime_log_add_runtime_usec(runtime_log_t *runtime_log, retro_time_t usec)
{
   retro_time_t usec_old;
   
   if (!runtime_log)
      return;
   
   runtime_log_convert_hms2usec(
         runtime_log->runtime.hours, runtime_log->runtime.minutes, runtime_log->runtime.seconds,
         &usec_old);
   
   runtime_log_convert_usec2hms(usec_old + usec,
         &runtime_log->runtime.hours, &runtime_log->runtime.minutes, &runtime_log->runtime.seconds);
}

/* Sets last played entry to specified value */
void runtime_log_set_last_played(runtime_log_t *runtime_log,
      unsigned year, unsigned month, unsigned day,
      unsigned hour, unsigned minute, unsigned second)
{
   if (!runtime_log)
      return;
   
   /* This function should never be needed, so just
    * perform dumb value assignment (i.e. no validation
    * using mktime()) */
   runtime_log->last_played.year = year;
   runtime_log->last_played.month = month;
   runtime_log->last_played.day = day;
   runtime_log->last_played.hour = hour;
   runtime_log->last_played.minute = minute;
   runtime_log->last_played.second = second;
}

/* Sets last played entry to current date/time */
void runtime_log_set_last_played_now(runtime_log_t *runtime_log)
{
   time_t current_time;
   struct tm * time_info;
   
   if (!runtime_log)
      return;
   
   /* Get current time */
   time(&current_time);
   time_info = localtime(&current_time);
   
   /* This can actually happen, but if does we probably
    * have bigger problems to worry about... */
   if(!time_info)
   {
      RARCH_ERR("Failed to get current time.\n");
      return;
   }
   
   /* Extract values */
   runtime_log->last_played.year = (unsigned)time_info->tm_year + 1900;
   runtime_log->last_played.month = (unsigned)time_info->tm_mon + 1;
   runtime_log->last_played.day = (unsigned)time_info->tm_mday;
   runtime_log->last_played.hour = (unsigned)time_info->tm_hour;
   runtime_log->last_played.minute = (unsigned)time_info->tm_min;
   runtime_log->last_played.second = (unsigned)time_info->tm_sec;
}

/* Resets log to default (zero) values */
void runtime_log_reset(runtime_log_t *runtime_log)
{
   if (!runtime_log)
      return;
   
   runtime_log->runtime.hours = 0;
   runtime_log->runtime.minutes = 0;
   runtime_log->runtime.seconds = 0;
   
   runtime_log->last_played.year = 0;
   runtime_log->last_played.month = 0;
   runtime_log->last_played.day = 0;
   runtime_log->last_played.hour = 0;
   runtime_log->last_played.minute = 0;
   runtime_log->last_played.second = 0;
}

/* Getters */

/* Gets runtime in hours, minutes, seconds */
void runtime_log_get_runtime_hms(runtime_log_t *runtime_log, unsigned *hours, unsigned *minutes, unsigned *seconds)
{
   if (!runtime_log)
      return;
   
   *hours = runtime_log->runtime.hours;
   *minutes = runtime_log->runtime.minutes;
   *seconds = runtime_log->runtime.seconds;
}

/* Gets runtime in microseconds */
void runtime_log_get_runtime_usec(runtime_log_t *runtime_log, retro_time_t *usec)
{
   if (!runtime_log)
      return;
   
   runtime_log_convert_hms2usec(
         runtime_log->runtime.hours, runtime_log->runtime.minutes, runtime_log->runtime.seconds,
         usec);
}

/* Gets last played entry values */
void runtime_log_get_last_played(runtime_log_t *runtime_log,
      unsigned *year, unsigned *month, unsigned *day,
      unsigned *hour, unsigned *minute, unsigned *second)
{
   if (!runtime_log)
      return;
   
   *year = runtime_log->last_played.year;
   *month = runtime_log->last_played.month;
   *day = runtime_log->last_played.day;
   *hour = runtime_log->last_played.hour;
   *minute = runtime_log->last_played.minute;
   *second = runtime_log->last_played.second;
}

/* Gets last played entry values as a time_t 'object'
 * (e.g. for printing with strftime()) */
void runtime_log_get_last_played_time(runtime_log_t *runtime_log, time_t *time)
{
   struct tm time_info;
   
   if (!runtime_log)
      return;
   
   if (!time)
      return;
   
   /* Set tm values */
   time_info.tm_year = (int)runtime_log->last_played.year - 1900;
   time_info.tm_mon = (int)runtime_log->last_played.month - 1;
   time_info.tm_mday = (int)runtime_log->last_played.day;
   time_info.tm_hour = (int)runtime_log->last_played.hour;
   time_info.tm_min = (int)runtime_log->last_played.minute;
   time_info.tm_sec = (int)runtime_log->last_played.second;
   time_info.tm_isdst = -1;
   
   /* Get time */
   *time = mktime(&time_info);
}

/* Status */

/* Returns true if log has a non-zero runtime entry */
bool runtime_log_has_runtime(runtime_log_t *runtime_log)
{
   if (!runtime_log)
      return false;
   
   return !((runtime_log->runtime.hours == 0) &&
            (runtime_log->runtime.minutes == 0) &&
            (runtime_log->runtime.seconds == 0));
}

/* Returns true if log has a non-zero last played entry */
bool runtime_log_has_last_played(runtime_log_t *runtime_log)
{
   if (!runtime_log)
      return false;
   
   return !((runtime_log->last_played.year == 0) &&
            (runtime_log->last_played.month == 0) &&
            (runtime_log->last_played.day == 0) &&
            (runtime_log->last_played.hour == 0) &&
            (runtime_log->last_played.minute == 0) &&
            (runtime_log->last_played.second == 0));
}

/* Saving */

/* Saves specified runtime log to disk */
void runtime_log_save(runtime_log_t *runtime_log)
{
   int ret = 0;
   RFILE *file = NULL;
   
   if (!runtime_log)
      return;
   
   /* Attempt to open log file */
   file = filestream_open(runtime_log->path, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   
   if (!file)
   {
      RARCH_ERR("Failed to open runtime log file: %s\n", runtime_log->path);
      return;
   }
   
   /* Write log file contents */
   ret = filestream_printf(file, LOG_FILE_FORMAT_STR,
               runtime_log->runtime.hours, runtime_log->runtime.minutes, runtime_log->runtime.seconds,
               runtime_log->last_played.year, runtime_log->last_played.month, runtime_log->last_played.day,
               runtime_log->last_played.hour, runtime_log->last_played.minute, runtime_log->last_played.second);
   
   if (ret <= 0)
      RARCH_ERR("Failed to write runtime log file: %s\n", runtime_log->path);
   
   /* Close log file */
   filestream_close(file);
}

/* Utility functions */

/* Convert from hours, minutes, seconds to microseconds */
void runtime_log_convert_hms2usec(unsigned hours, unsigned minutes, unsigned seconds, retro_time_t *usec)
{
   *usec = ((retro_time_t)hours * 60 * 60 * 1000000) +
           ((retro_time_t)minutes * 60 * 1000000) +
           ((retro_time_t)seconds * 1000000);
}

/* Convert from microseconds to hours, minutes, seconds */
void runtime_log_convert_usec2hms(retro_time_t usec, unsigned *hours, unsigned *minutes, unsigned *seconds)
{
   *seconds = usec / 1000000;
   *minutes = *seconds / 60;
   *hours = *minutes / 60;
   
   *seconds -= *minutes * 60;
   *minutes -= *hours * 60;
}
