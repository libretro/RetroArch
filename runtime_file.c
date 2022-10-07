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
#include <locale.h>

#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <formats/rjson.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <time/rtime.h>

#include "file_path_special.h"
#include "paths.h"
#include "core_info.h"
#include "verbosity.h"
#include "msg_hash.h"

#if defined(HAVE_MENU)
#include "menu/menu_driver.h"
#endif

#include "runtime_file.h"

#define LOG_FILE_RUNTIME_FORMAT_STR "%u:%02u:%02u"
#define LOG_FILE_LAST_PLAYED_FORMAT_STR "%04u-%02u-%02u %02u:%02u:%02u"

/* JSON Stuff... */

typedef struct
{
   char **current_entry_val;
   char *runtime_string;
   char *last_played_string;
} RtlJSONContext;

static bool RtlJSONObjectMemberHandler(void *ctx, const char *s, size_t len)
{
   RtlJSONContext *p_ctx = (RtlJSONContext*)ctx;

   if (p_ctx->current_entry_val)
   {
      /* something went wrong */
      return false;
   }

   if (len)
   {
      if (string_is_equal(s, "runtime"))
         p_ctx->current_entry_val = &p_ctx->runtime_string;
      else if (string_is_equal(s, "last_played"))
         p_ctx->current_entry_val = &p_ctx->last_played_string;
      /* ignore unknown members */
   }

   return true;
}

static bool RtlJSONStringHandler(void *ctx, const char *s, size_t len)
{
   RtlJSONContext *p_ctx = (RtlJSONContext*)ctx;

   if (p_ctx->current_entry_val && len && !string_is_empty(s))
   {
      if (*p_ctx->current_entry_val)
         free(*p_ctx->current_entry_val);

      *p_ctx->current_entry_val = strdup(s);
   }
   /* ignore unknown members */

   p_ctx->current_entry_val = NULL;

   return true;
}

/* Initialisation */

/* Parses log file referenced by runtime_log->path.
 * Does nothing if log file does not exist. */
static void runtime_log_read_file(runtime_log_t *runtime_log)
{
   unsigned runtime_hours      = 0;
   unsigned runtime_minutes    = 0;
   unsigned runtime_seconds    = 0;

   unsigned last_played_year   = 0;
   unsigned last_played_month  = 0;
   unsigned last_played_day    = 0;
   unsigned last_played_hour   = 0;
   unsigned last_played_minute = 0;
   unsigned last_played_second = 0;

   RtlJSONContext context      = {0};
   rjson_t* parser;

   /* Attempt to open log file */
   RFILE *file                 = filestream_open(runtime_log->path,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Failed to open runtime log file: %s\n", runtime_log->path);
      return;
   }

   /* Initialise JSON parser */
   parser = rjson_open_rfile(file);
   if (!parser)
   {
      RARCH_ERR("Failed to create JSON parser.\n");
      goto end;
   }

   /* Configure parser */
   rjson_set_options(parser, RJSON_OPTION_ALLOW_UTF8BOM);

   /* Read file */
   if (rjson_parse(parser, &context,
         RtlJSONObjectMemberHandler,
         RtlJSONStringHandler,
         NULL,                   /* unused number handler */
         NULL, NULL, NULL, NULL, /* unused object/array handlers */
         NULL, NULL)             /* unused boolean/null handlers */
         != RJSON_DONE)
   {
      if (rjson_get_source_context_len(parser))
      {
         RARCH_ERR("Error parsing chunk of runtime log file: %s\n---snip---\n%.*s\n---snip---\n",
               runtime_log->path,
               rjson_get_source_context_len(parser),
               rjson_get_source_context_buf(parser));
      }
      RARCH_WARN("Error parsing runtime log file: %s\n", runtime_log->path);
      RARCH_ERR("Error: Invalid JSON at line %d, column %d - %s.\n",
            (int)rjson_get_source_line(parser),
            (int)rjson_get_source_column(parser),
            (*rjson_get_error(parser) ? rjson_get_error(parser) : "format error"));
   }

   /* Free parser */
   rjson_free(parser);

   /* Process string values read from JSON file */

   /* Runtime */
   if (!string_is_empty(context.runtime_string))
   {
      if (sscanf(context.runtime_string,
               LOG_FILE_RUNTIME_FORMAT_STR,
               &runtime_hours,
               &runtime_minutes,
               &runtime_seconds) != 3)
      {
         RARCH_ERR("Runtime log file - invalid 'runtime' entry detected: %s\n", runtime_log->path);
         goto end;
      }
   }

   /* Last played */
   if (!string_is_empty(context.last_played_string))
   {
      if (sscanf(context.last_played_string,
               LOG_FILE_LAST_PLAYED_FORMAT_STR,
               &last_played_year,
               &last_played_month,
               &last_played_day,
               &last_played_hour,
               &last_played_minute,
               &last_played_second) != 6)
      {
         RARCH_ERR("Runtime log file - invalid 'last played' entry detected: %s\n", runtime_log->path);
         goto end;
      }
   }

   /* If we reach this point then all is well
    * > Assign values to runtime_log object */
   runtime_log->runtime.hours      = runtime_hours;
   runtime_log->runtime.minutes    = runtime_minutes;
   runtime_log->runtime.seconds    = runtime_seconds;

   runtime_log->last_played.year   = last_played_year;
   runtime_log->last_played.month  = last_played_month;
   runtime_log->last_played.day    = last_played_day;
   runtime_log->last_played.hour   = last_played_hour;
   runtime_log->last_played.minute = last_played_minute;
   runtime_log->last_played.second = last_played_second;

end:

   /* Clean up leftover strings */
   if (context.runtime_string)
      free(context.runtime_string);
   if (context.last_played_string)
      free(context.last_played_string);

   /* Close log file */
   filestream_close(file);
}

/* Initialise runtime log, loading current parameters
 * if log file exists. Returned object must be free()'d.
 * Returns NULL if core_path is invalid, or content_path
 * is invalid and core does not support contentless
 * operation */
runtime_log_t *runtime_log_init(
      const char *content_path,
      const char *core_path,
      const char *dir_runtime_log,
      const char *dir_playlist,
      bool log_per_core)
{
   char content_name[PATH_MAX_LENGTH];
   char core_name[PATH_MAX_LENGTH];
   char log_file_dir[PATH_MAX_LENGTH];
   char log_file_path[PATH_MAX_LENGTH];
   char tmp_buf[PATH_MAX_LENGTH];
   bool supports_no_game      = false;
   core_info_t *core_info     = NULL;
   runtime_log_t *runtime_log = NULL;

   content_name[0]            = '\0';
   core_name[0]               = '\0';

   if (  string_is_empty(dir_runtime_log) &&
         string_is_empty(dir_playlist))
   {
      RARCH_ERR("Runtime log directory is undefined - cannot save"
            " runtime log files.\n");
      return NULL;
   }

   if (  string_is_empty(core_path) ||
         string_is_equal(core_path, "builtin") ||
         string_is_equal(core_path, "DETECT"))
      return NULL;

   /* Get core info:
    * - Need to know if core supports contentless operation
    * - Need core name in order to generate file path when
    *   per-core logging is enabled
    * Note: An annoyance - core name is required even when
    * we are performing aggregate logging, since content
    * name is sometimes dependent upon core
    * (e.g. see TyrQuake below) */
   if (core_info_find(core_path, &core_info))
   {
      supports_no_game = core_info->supports_no_game;
      if (!string_is_empty(core_info->core_name))
         strlcpy(core_name, core_info->core_name, sizeof(core_name));
   }

   if (string_is_empty(core_name))
      return NULL;

   /* Get runtime log directory
    * If 'custom' runtime log path is undefined,
    * use default 'playlists/logs' directory... */
   if (string_is_empty(dir_runtime_log))
      fill_pathname_join_special(
            tmp_buf,
            dir_playlist,
            "logs",
            sizeof(tmp_buf));
   else
      strlcpy(tmp_buf, dir_runtime_log, sizeof(tmp_buf));

   if (string_is_empty(tmp_buf))
      return NULL;

   if (log_per_core)
      fill_pathname_join_special(
            log_file_dir,
            tmp_buf,
            core_name,
            sizeof(log_file_dir));
   else
      strlcpy(log_file_dir, tmp_buf, sizeof(log_file_dir));

   if (string_is_empty(log_file_dir))
      return NULL;

   /* Create directory, if required */
   if (!path_is_directory(log_file_dir))
   {
      if (!path_mkdir(log_file_dir))
      {
         RARCH_ERR("[runtime] failed to create directory for"
               " runtime log: %s.\n", log_file_dir);
         return NULL;
      }
   }

   /* Get content name */
   if (string_is_empty(content_path))
   {
      /* If core supports contentless operation and
       * no content is provided, 'content' is simply
       * the name of the core itself */
      if (supports_no_game)
      {
         strlcpy(content_name, core_name, sizeof(content_name));
         strlcat(content_name, ".lrtl", sizeof(content_name));
      }
   }
   /* NOTE: TyrQuake requires a specific hack, since all
    * content has the same name... */
   else if (string_is_equal(core_name, "TyrQuake"))
   {
      const char *last_slash = find_last_slash(content_path);
      if (last_slash)
      {
         size_t path_length = last_slash + 1 - content_path;
         if (path_length < PATH_MAX_LENGTH)
         {
            memset(tmp_buf, 0, sizeof(tmp_buf));
            strlcpy(tmp_buf,
                  content_path, path_length * sizeof(char));
            strlcpy(content_name,
                  path_basename(tmp_buf), sizeof(content_name));
            strlcat(content_name, ".lrtl", sizeof(content_name));
         }
      }
   }
   else
   {
      /* path_remove_extension() requires a char * (not const)
       * so have to use a temporary buffer... */
      char *tmp_buf_no_ext = NULL;
      tmp_buf[0]           = '\0';
      strlcpy(tmp_buf, path_basename(content_path), sizeof(tmp_buf));
      tmp_buf_no_ext       = path_remove_extension(tmp_buf);

      if (string_is_empty(tmp_buf_no_ext))
         return NULL;

      strlcpy(content_name, tmp_buf_no_ext, sizeof(content_name));
      strlcat(content_name, ".lrtl", sizeof(content_name));
   }

   if (string_is_empty(content_name))
      return NULL;

   /* Build final log file path */
   fill_pathname_join_special(log_file_path, log_file_dir,
         content_name, sizeof(log_file_path));

   if (string_is_empty(log_file_path))
      return NULL;

   /* Phew... If we get this far then all is well.
    * > Create 'runtime_log' object */
   runtime_log                     = (runtime_log_t*)
      malloc(sizeof(*runtime_log));
   if (!runtime_log)
      return NULL;

   /* > Populate default values */
   runtime_log->runtime.hours      = 0;
   runtime_log->runtime.minutes    = 0;
   runtime_log->runtime.seconds    = 0;

   runtime_log->last_played.year   = 0;
   runtime_log->last_played.month  = 0;
   runtime_log->last_played.day    = 0;
   runtime_log->last_played.hour   = 0;
   runtime_log->last_played.minute = 0;
   runtime_log->last_played.second = 0;

   runtime_log->path[0]            = '\0';

   strlcpy(runtime_log->path, log_file_path, sizeof(runtime_log->path));

   /* Load existing log file, if it exists */
   if (path_is_valid(runtime_log->path))
      runtime_log_read_file(runtime_log);

   return runtime_log;
}

/* Setters */

/* Set runtime to specified hours, minutes, seconds value */
void runtime_log_set_runtime_hms(runtime_log_t *runtime_log,
      unsigned hours, unsigned minutes, unsigned seconds)
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
         &runtime_log->runtime.hours,
         &runtime_log->runtime.minutes,
         &runtime_log->runtime.seconds);
}

/* Set runtime to specified microseconds value */
void runtime_log_set_runtime_usec(
      runtime_log_t *runtime_log, retro_time_t usec)
{
   if (!runtime_log)
      return;

   runtime_log_convert_usec2hms(usec,
         &runtime_log->runtime.hours,
         &runtime_log->runtime.minutes,
         &runtime_log->runtime.seconds);
}

/* Adds specified hours, minutes, seconds value to current runtime */
void runtime_log_add_runtime_hms(
      runtime_log_t *runtime_log,
      unsigned hours,
      unsigned minutes,
      unsigned seconds)
{
   retro_time_t usec_old;
   retro_time_t usec_new;

   if (!runtime_log)
      return;

   runtime_log_convert_hms2usec(
         runtime_log->runtime.hours,
         runtime_log->runtime.minutes,
         runtime_log->runtime.seconds,
         &usec_old);

   runtime_log_convert_hms2usec(hours, minutes, seconds, &usec_new);

   runtime_log_convert_usec2hms(usec_old + usec_new,
         &runtime_log->runtime.hours,
         &runtime_log->runtime.minutes,
         &runtime_log->runtime.seconds);
}

/* Adds specified microseconds value to current runtime */
void runtime_log_add_runtime_usec(
      runtime_log_t *runtime_log, retro_time_t usec)
{
   retro_time_t usec_old;

   if (!runtime_log)
      return;

   runtime_log_convert_hms2usec(
         runtime_log->runtime.hours,
         runtime_log->runtime.minutes,
         runtime_log->runtime.seconds,
         &usec_old);

   runtime_log_convert_usec2hms(usec_old + usec,
         &runtime_log->runtime.hours,
         &runtime_log->runtime.minutes,
         &runtime_log->runtime.seconds);
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
   runtime_log->last_played.year   = year;
   runtime_log->last_played.month  = month;
   runtime_log->last_played.day    = day;
   runtime_log->last_played.hour   = hour;
   runtime_log->last_played.minute = minute;
   runtime_log->last_played.second = second;
}

/* Sets last played entry to current date/time */
void runtime_log_set_last_played_now(runtime_log_t *runtime_log)
{
   time_t current_time;
   struct tm time_info;

   if (!runtime_log)
      return;

   /* Get current time */
   time(&current_time);
   rtime_localtime(&current_time, &time_info);

   /* Extract values */
   runtime_log->last_played.year   = (unsigned)time_info.tm_year + 1900;
   runtime_log->last_played.month  = (unsigned)time_info.tm_mon + 1;
   runtime_log->last_played.day    = (unsigned)time_info.tm_mday;
   runtime_log->last_played.hour   = (unsigned)time_info.tm_hour;
   runtime_log->last_played.minute = (unsigned)time_info.tm_min;
   runtime_log->last_played.second = (unsigned)time_info.tm_sec;
}

/* Resets log to default (zero) values */
void runtime_log_reset(runtime_log_t *runtime_log)
{
   if (!runtime_log)
      return;

   runtime_log->runtime.hours      = 0;
   runtime_log->runtime.minutes    = 0;
   runtime_log->runtime.seconds    = 0;

   runtime_log->last_played.year   = 0;
   runtime_log->last_played.month  = 0;
   runtime_log->last_played.day    = 0;
   runtime_log->last_played.hour   = 0;
   runtime_log->last_played.minute = 0;
   runtime_log->last_played.second = 0;
}

/* Getters */

/* Gets runtime in hours, minutes, seconds */
void runtime_log_get_runtime_hms(runtime_log_t *runtime_log,
      unsigned *hours, unsigned *minutes, unsigned *seconds)
{
   if (!runtime_log)
      return;

   *hours   = runtime_log->runtime.hours;
   *minutes = runtime_log->runtime.minutes;
   *seconds = runtime_log->runtime.seconds;
}

/* Gets runtime in microseconds */
void runtime_log_get_runtime_usec(
      runtime_log_t *runtime_log, retro_time_t *usec)
{
   if (runtime_log)
      runtime_log_convert_hms2usec( runtime_log->runtime.hours,
            runtime_log->runtime.minutes, runtime_log->runtime.seconds,
            usec);
}

/* Gets runtime as a pre-formatted string */
void runtime_log_get_runtime_str(runtime_log_t *runtime_log,
      char *s, size_t len)
{
   size_t _len = strlcpy(s,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME),
         len);
   s[_len  ]   = ' ';
   s[_len+1]   = '\0';
   if (runtime_log)
   {
      char t[64];
      t[0] = '\0';
      snprintf(t, sizeof(t), "%02u:%02u:%02u",
            runtime_log->runtime.hours, runtime_log->runtime.minutes,
            runtime_log->runtime.seconds);
      strlcat(s, t, len);
   }
   else
   {
      s[_len+1]   = '0';
      s[_len+2]   = '0';
      s[_len+3]   = ':';
      s[_len+4]   = '0';
      s[_len+5]   = '0';
      s[_len+6]   = ':';
      s[_len+7]   = '0';
      s[_len+8]   = '0';
      s[_len+9]   = '\0';
   }
}

/* Gets last played entry values */
void runtime_log_get_last_played(runtime_log_t *runtime_log,
      unsigned *year, unsigned *month, unsigned *day,
      unsigned *hour, unsigned *minute, unsigned *second)
{
   if (!runtime_log)
      return;

   *year   = runtime_log->last_played.year;
   *month  = runtime_log->last_played.month;
   *day    = runtime_log->last_played.day;
   *hour   = runtime_log->last_played.hour;
   *minute = runtime_log->last_played.minute;
   *second = runtime_log->last_played.second;
}

/* Gets last played entry values as a struct tm 'object'
 * (e.g. for printing with strftime()) */
void runtime_log_get_last_played_time(runtime_log_t *runtime_log,
      struct tm *time_info)
{
   if (!runtime_log || !time_info)
      return;

   /* Set tm values */
   time_info->tm_year  = (int)runtime_log->last_played.year  - 1900;
   time_info->tm_mon   = (int)runtime_log->last_played.month - 1;
   time_info->tm_mday  = (int)runtime_log->last_played.day;
   time_info->tm_hour  = (int)runtime_log->last_played.hour;
   time_info->tm_min   = (int)runtime_log->last_played.minute;
   time_info->tm_sec   = (int)runtime_log->last_played.second;
   time_info->tm_isdst = -1;

   /* Perform any required range adjustment + populate
    * missing entries */
   mktime(time_info);
}

static void runtime_last_played_strftime(
		char *s, size_t len, const char *format,
      const struct tm *timeptr)
{
   char *local = NULL;

   /* Ensure correct locale is set */
   setlocale(LC_TIME, "");

   /* Generate string */
   strftime(s, len, format, timeptr);
#if !(defined(__linux__) && !defined(ANDROID))
   if ((local = local_to_utf8_string_alloc(s)))
   {
      if (!string_is_empty(local))
         strlcpy(s, local, len);

      free(local);
      local = NULL;
   }
#endif
}

static bool runtime_last_played_human(runtime_log_t *runtime_log,
      char *str, size_t len)
{
   struct tm time_info;
   time_t last_played;
   time_t current;
   time_t delta;
   unsigned i;
   char tmp[32];

   unsigned units[7][2] =
   {
      {MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_SINGLE, MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL},
      {MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_SINGLE, MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_PLURAL},
      {MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_SINGLE, MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_PLURAL},
      {MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_SINGLE, MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_PLURAL},
      {MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_SINGLE, MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_PLURAL},
      {MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_SINGLE, MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_PLURAL},
      {MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_SINGLE, MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_PLURAL},
   };

   float periods[6] = {60.0f, 60.0f, 24.0f, 7.0f, 4.35f, 12.0f};

   tmp[0]           = '\0';

   if (!runtime_log)
      return false;

   /* Get time */
   runtime_log_get_last_played_time(runtime_log, &time_info);

   last_played = mktime(&time_info);
   current     = time(NULL);

   if ((delta = current - last_played) <= 0)
      return false;

   for (i = 0; delta >= periods[i] && i < sizeof(periods) - 1; i++)
      delta /= periods[i];

   /* Generate string */
   snprintf(tmp, sizeof(tmp), "%u ", (int)delta);
   if (delta == 1)
      strlcat(tmp, msg_hash_to_str((enum msg_hash_enums)units[i][0]),
            sizeof(tmp));
   else
      strlcat(tmp, msg_hash_to_str((enum msg_hash_enums)units[i][1]),
            sizeof(tmp));
   strlcat(str, tmp, len);
   strlcat(str, " ", len);
   strlcat(str, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TIME_UNIT_AGO), len);

   return true;
}

/* Gets last played entry value as a pre-formatted string */
void runtime_log_get_last_played_str(runtime_log_t *runtime_log,
      char *str, size_t len,
      enum playlist_sublabel_last_played_style_type timedate_style,
      enum playlist_sublabel_last_played_date_separator_type date_separator)
{
   size_t _len;
   char tmp[64];
   bool has_am_pm         = false;
   const char *format_str = "";

   tmp[0] = '\0';

   if (runtime_log)
   {
      /* Handle 12-hour clock options
       * > These require extra work, due to AM/PM localisation */
      switch (timedate_style)
      {
         case PLAYLIST_LAST_PLAYED_STYLE_YMD_HMS_AMPM:
            has_am_pm = true;
            /* Using switch statements to set the format
             * string is verbose, but has far less performance
             * impact than setting the date separator dynamically
             * (i.e. no snprintf() or character replacement...) */
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = " %Y/%m/%d %I:%M:%S %p";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = " %Y.%m.%d %I:%M:%S %p";
                  break;
               default:
                  format_str = " %Y-%m-%d %I:%M:%S %p";
                  break;
            }
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_YMD_HM_AMPM:
            has_am_pm = true;
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = " %Y/%m/%d %I:%M %p";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = " %Y.%m.%d %I:%M %p";
                  break;
               default:
                  format_str = " %Y-%m-%d %I:%M %p";
                  break;
            }
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HMS_AMPM:
            has_am_pm = true;
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = " %m/%d/%Y %I:%M:%S %p";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = " %m.%d.%Y %I:%M:%S %p";
                  break;
               default:
                  format_str = " %m-%d-%Y %I:%M:%S %p";
                  break;
            }
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HM_AMPM:
            has_am_pm = true;
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = " %m/%d/%Y %I:%M %p";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = " %m.%d.%Y %I:%M %p";
                  break;
               default:
                  format_str = " %m-%d-%Y %I:%M %p";
                  break;
            }
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_MD_HM_AMPM:
            has_am_pm = true;
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = " %m/%d %I:%M %p";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = " %m.%d %I:%M %p";
                  break;
               default:
                  format_str = " %m-%d %I:%M %p";
                  break;
            }
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HMS_AMPM:
            has_am_pm = true;
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = " %d/%m/%Y %I:%M:%S %p";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = " %d.%m.%Y %I:%M:%S %p";
                  break;
               default:
                  format_str = " %d-%m-%Y %I:%M:%S %p";
                  break;
            }
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HM_AMPM:
            has_am_pm = true;
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = " %d/%m/%Y %I:%M %p";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = " %d.%m.%Y %I:%M %p";
                  break;
               default:
                  format_str = " %d-%m-%Y %I:%M %p";
                  break;
            }
            break;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMM_HM_AMPM:
            has_am_pm = true;
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = " %d/%m %I:%M %p";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = " %d.%m %I:%M %p";
                  break;
               default:
                  format_str = " %d-%m %I:%M %p";
                  break;
            }
            break;
         default:
            has_am_pm = false;
            break;
      }

      if (has_am_pm)
      {
         if (runtime_log)
         {
            /* Get time */
            struct tm time_info;
            runtime_log_get_last_played_time(runtime_log, &time_info);
            runtime_last_played_strftime(tmp, sizeof(tmp), format_str, &time_info);
         }
         _len        = strlcpy(str, msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED), len);
         str[_len  ] = ' ';
         str[_len+1] = '\0';
         strlcat(str, tmp, len);
         return;
      }

      /* Handle non-12-hour clock options */
      switch (timedate_style)
      {
         case PLAYLIST_LAST_PLAYED_STYLE_YMD_HM:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %04u/%02u/%02u %02u:%02u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %04u.%02u.%02u %02u:%02u";
                  break;
               default:
                  format_str = "%s %04u-%02u-%02u %02u:%02u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.year,
                  runtime_log->last_played.month,
                  runtime_log->last_played.day,
                  runtime_log->last_played.hour,
                  runtime_log->last_played.minute);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_YMD:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %04u/%02u/%02u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %04u.%02u.%02u";
                  break;
               default:
                  format_str = "%s %04u-%02u-%02u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.year,
                  runtime_log->last_played.month,
                  runtime_log->last_played.day);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_YM:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %04u/%02u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %04u.%02u";
                  break;
               default:
                  format_str = "%s %04u-%02u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.year,
                  runtime_log->last_played.month);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HMS:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %02u/%02u/%04u %02u:%02u:%02u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %02u.%02u.%04u %02u:%02u:%02u";
                  break;
               default:
                  format_str = "%s %02u-%02u-%04u %02u:%02u:%02u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.month,
                  runtime_log->last_played.day,
                  runtime_log->last_played.year,
                  runtime_log->last_played.hour,
                  runtime_log->last_played.minute,
                  runtime_log->last_played.second);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HM:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %02u/%02u/%04u %02u:%02u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %02u.%02u.%04u %02u:%02u";
                  break;
               default:
                  format_str = "%s %02u-%02u-%04u %02u:%02u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.month,
                  runtime_log->last_played.day,
                  runtime_log->last_played.year,
                  runtime_log->last_played.hour,
                  runtime_log->last_played.minute);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_MD_HM:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %02u/%02u %02u:%02u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %02u.%02u %02u:%02u";
                  break;
               default:
                  format_str = "%s %02u-%02u %02u:%02u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.month,
                  runtime_log->last_played.day,
                  runtime_log->last_played.hour,
                  runtime_log->last_played.minute);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_MDYYYY:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %02u/%02u/%04u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %02u.%02u.%04u";
                  break;
               default:
                  format_str = "%s %02u-%02u-%04u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.month,
                  runtime_log->last_played.day,
                  runtime_log->last_played.year);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_MD:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %02u/%02u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %02u.%02u";
                  break;
               default:
                  format_str = "%s %02u-%02u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.month,
                  runtime_log->last_played.day);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HMS:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %02u/%02u/%04u %02u:%02u:%02u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %02u.%02u.%04u %02u:%02u:%02u";
                  break;
               default:
                  format_str = "%s %02u-%02u-%04u %02u:%02u:%02u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.day,
                  runtime_log->last_played.month,
                  runtime_log->last_played.year,
                  runtime_log->last_played.hour,
                  runtime_log->last_played.minute,
                  runtime_log->last_played.second);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HM:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %02u/%02u/%04u %02u:%02u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %02u.%02u.%04u %02u:%02u";
                  break;
               default:
                  format_str = "%s %02u-%02u-%04u %02u:%02u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.day,
                  runtime_log->last_played.month,
                  runtime_log->last_played.year,
                  runtime_log->last_played.hour,
                  runtime_log->last_played.minute);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMM_HM:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %02u/%02u %02u:%02u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %02u.%02u %02u:%02u";
                  break;
               default:
                  format_str = "%s %02u-%02u %02u:%02u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.day,
                  runtime_log->last_played.month,
                  runtime_log->last_played.hour,
                  runtime_log->last_played.minute);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %02u/%02u/%04u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %02u.%02u.%04u";
                  break;
               default:
                  format_str = "%s %02u-%02u-%04u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.day,
                  runtime_log->last_played.month,
                  runtime_log->last_played.year);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_DDMM:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %02u/%02u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %02u.%02u";
                  break;
               default:
                  format_str = "%s %02u-%02u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.day, runtime_log->last_played.month);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_AGO:
            if (!(runtime_last_played_human(runtime_log, tmp, sizeof(tmp))))
               strlcat(tmp,
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER),
                     sizeof(tmp));
            _len        =  strlcpy(str, msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED), len);
            str[_len  ] = ' ';
            str[_len+1] = '\0';
            strlcat(str, tmp, len);
            return;
         case PLAYLIST_LAST_PLAYED_STYLE_YMD_HMS:
         default:
            switch (date_separator)
            {
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH:
                  format_str = "%s %04u/%02u/%02u %02u:%02u:%02u";
                  break;
               case PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD:
                  format_str = "%s %04u.%02u.%02u %02u:%02u:%02u";
                  break;
               default:
                  format_str = "%s %04u-%02u-%02u %02u:%02u:%02u";
                  break;
            }
            snprintf(str, len, format_str,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
                  runtime_log->last_played.year,
                  runtime_log->last_played.month,
                  runtime_log->last_played.day,
                  runtime_log->last_played.hour,
                  runtime_log->last_played.minute,
                  runtime_log->last_played.second);
            return;
      }
   }
   else
      snprintf(str, len,
            "%s %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER)
            );
}

/* Status */

/* Returns true if log has a non-zero runtime entry */
bool runtime_log_has_runtime(runtime_log_t *runtime_log)
{
   if (runtime_log)
      return !(
            (runtime_log->runtime.hours   == 0) &&
            (runtime_log->runtime.minutes == 0) &&
            (runtime_log->runtime.seconds == 0));
   return false;
}

/* Returns true if log has a non-zero last played entry */
bool runtime_log_has_last_played(runtime_log_t *runtime_log)
{
   if (runtime_log)
      return !(
            (runtime_log->last_played.year   == 0) &&
            (runtime_log->last_played.month  == 0) &&
            (runtime_log->last_played.day    == 0) &&
            (runtime_log->last_played.hour   == 0) &&
            (runtime_log->last_played.minute == 0) &&
            (runtime_log->last_played.second == 0));
   return false;
}

/* Saving */

/* Saves specified runtime log to disk */
void runtime_log_save(runtime_log_t *runtime_log)
{
   char value_string[64]; /* 64 characters should be
                             enough for a very long runtime... :) */
   RFILE *file            = NULL;
   rjsonwriter_t* writer;

   if (!runtime_log)
      return;

   RARCH_LOG("[Runtime]: Saving runtime log file: \"%s\".\n", runtime_log->path);

   /* Attempt to open log file */
   if (!(file = filestream_open(runtime_log->path,
         RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      RARCH_ERR("[Runtime]: Failed to open runtime log file: \"%s\".\n", runtime_log->path);
      return;
   }

   /* Initialise JSON writer */
   if (!(writer = rjsonwriter_open_rfile(file)))
   {
      RARCH_ERR("[Runtime]: Failed to create JSON writer.\n");
      goto end;
   }

   /* Write output file */
   rjsonwriter_raw(writer, "{", 1);
   rjsonwriter_raw(writer, "\n", 1);

   /* > Version entry */
   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_add_string(writer, "version");
   rjsonwriter_raw(writer, ":", 1);
   rjsonwriter_raw(writer, " ", 1);
   rjsonwriter_add_string(writer, "1.0");
   rjsonwriter_raw(writer, ",", 1);
   rjsonwriter_raw(writer, "\n", 1);

   /* > Runtime entry */
   snprintf(value_string,
         sizeof(value_string),
         LOG_FILE_RUNTIME_FORMAT_STR,
         runtime_log->runtime.hours, runtime_log->runtime.minutes,
         runtime_log->runtime.seconds);
    
   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_add_string(writer, "runtime");
   rjsonwriter_raw(writer, ":", 1);
   rjsonwriter_raw(writer, " ", 1);
   rjsonwriter_add_string(writer, value_string);
   rjsonwriter_raw(writer, ",", 1);
   rjsonwriter_raw(writer, "\n", 1);

   /* > Last played entry */
   value_string[0] = '\0';
   snprintf(value_string, sizeof(value_string),
         LOG_FILE_LAST_PLAYED_FORMAT_STR,
         runtime_log->last_played.year, runtime_log->last_played.month,
         runtime_log->last_played.day,
         runtime_log->last_played.hour, runtime_log->last_played.minute,
         runtime_log->last_played.second);

   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_add_string(writer, "last_played");
   rjsonwriter_raw(writer, ":", 1);
   rjsonwriter_raw(writer, " ", 1);
   rjsonwriter_add_string(writer, value_string);
   rjsonwriter_raw(writer, "\n", 1);

   /* > Finalise */
   rjsonwriter_raw(writer, "}", 1);
   rjsonwriter_raw(writer, "\n", 1);

   /* Free JSON writer */
   if (!rjsonwriter_free(writer))
   {
      RARCH_ERR("Error writing runtime log file: %s\n", runtime_log->path);
   }

end:
   /* Close log file */
   filestream_close(file);
}

/* Utility functions */

/* Convert from hours, minutes, seconds to microseconds */
void runtime_log_convert_hms2usec(unsigned hours,
      unsigned minutes, unsigned seconds, retro_time_t *usec)
{
   *usec = ((retro_time_t)hours   * 60 * 60 * 1000000) +
           ((retro_time_t)minutes * 60      * 1000000) +
           ((retro_time_t)seconds           * 1000000);
}

/* Convert from microseconds to hours, minutes, seconds */
void runtime_log_convert_usec2hms(retro_time_t usec,
      unsigned *hours, unsigned *minutes, unsigned *seconds)
{
   *seconds  = (unsigned)(usec / 1000000);
   *minutes  = *seconds / 60;
   *hours    = *minutes / 60;

   *seconds -= *minutes * 60;
   *minutes -= *hours * 60;
}

/* Playlist manipulation */

/* Updates specified playlist entry runtime values with
 * contents of associated log file */
void runtime_update_playlist(
      playlist_t *playlist, size_t idx,
      const char *dir_runtime_log,
      const char *dir_playlist,
      bool log_per_core,
      enum playlist_sublabel_last_played_style_type timedate_style,
      enum playlist_sublabel_last_played_date_separator_type date_separator)
{
   char runtime_str[64];
   char last_played_str[64];
   runtime_log_t *runtime_log             = NULL;
   const struct playlist_entry *entry     = NULL;
   struct playlist_entry update_entry     = {0};
#if defined(HAVE_MENU) && (defined(HAVE_OZONE) || defined(HAVE_MATERIALUI))
   const char *menu_ident                 = menu_driver_ident();
#endif

   /* Sanity check */
   if (!playlist)
      return;

   if (idx >= playlist_get_size(playlist))
      return;

   /* Set fallback playlist 'runtime_status'
    * (saves 'if' checks later...) */
   update_entry.runtime_status = PLAYLIST_RUNTIME_MISSING;

   /* 'Attach' runtime/last played strings */
   runtime_str[0]               = '\0';
   last_played_str[0]           = '\0';
   update_entry.runtime_str     = runtime_str;
   update_entry.last_played_str = last_played_str;

   /* Read current playlist entry */
   playlist_get_index(playlist, idx, &entry);

   /* Attempt to open log file */
   if ((runtime_log = runtime_log_init(
         entry->path,
         entry->core_path,
         dir_runtime_log,
         dir_playlist,
         log_per_core)))
   {
      /* Check whether a non-zero runtime has been recorded */
      if (runtime_log_has_runtime(runtime_log))
      {
         /* Read current runtime */
         runtime_log_get_runtime_hms(runtime_log,
               &update_entry.runtime_hours,
               &update_entry.runtime_minutes,
               &update_entry.runtime_seconds);

         runtime_log_get_runtime_str(runtime_log,
               runtime_str, sizeof(runtime_str));

         /* Read last played timestamp */
         runtime_log_get_last_played(runtime_log,
               &update_entry.last_played_year,
               &update_entry.last_played_month,
               &update_entry.last_played_day,
               &update_entry.last_played_hour,
               &update_entry.last_played_minute,
               &update_entry.last_played_second);

         runtime_log_get_last_played_str(runtime_log,
               last_played_str, sizeof(last_played_str),
               timedate_style, date_separator);

         /* Playlist entry now contains valid runtime data */
         update_entry.runtime_status = PLAYLIST_RUNTIME_VALID;
      }

      /* Clean up */
      free(runtime_log);
   }

#if defined(HAVE_MENU) && (defined(HAVE_OZONE) || defined(HAVE_MATERIALUI))
   /* Ozone and GLUI require runtime/last played strings
    * to be populated even when no runtime is recorded */
   if (update_entry.runtime_status != PLAYLIST_RUNTIME_VALID)
   {
      if (string_is_equal(menu_ident, "ozone") ||
          string_is_equal(menu_ident, "glui"))
      {
         runtime_log_get_runtime_str(NULL,
               runtime_str, sizeof(runtime_str));
         runtime_log_get_last_played_str(NULL,
               last_played_str, sizeof(last_played_str),
               timedate_style, date_separator);

         /* While runtime data does not exist, the playlist
          * entry does now contain valid information... */
         update_entry.runtime_status = PLAYLIST_RUNTIME_VALID;
      }
   }
#endif

   /* Update playlist */
   playlist_update_runtime(playlist, idx, &update_entry, false);
}

#if defined(HAVE_MENU)
/* Contentless cores manipulation */

/* Updates specified contentless core runtime values with
 * contents of associated log file */
void runtime_update_contentless_core(
      const char *core_path,
      const char *dir_runtime_log,
      const char *dir_playlist,
      bool log_per_core,
      enum playlist_sublabel_last_played_style_type timedate_style,
      enum playlist_sublabel_last_played_date_separator_type date_separator)
{
   char runtime_str[64];
   char last_played_str[64];
   core_info_t *core_info                       = NULL;
   runtime_log_t *runtime_log                   = NULL;
   contentless_core_runtime_info_t runtime_info = {0};
#if (defined(HAVE_OZONE) || defined(HAVE_MATERIALUI))
   const char *menu_ident                       = menu_driver_ident();
#endif

   /* Sanity check */
   if (string_is_empty(core_path) ||
       !core_info_find(core_path, &core_info) ||
       !core_info->supports_no_game)
      return;

   /* Set fallback runtime status
    * (saves 'if' checks later...) */
   runtime_info.status = CONTENTLESS_CORE_RUNTIME_MISSING;

   /* 'Attach' runtime/last played strings */
   runtime_str[0]               = '\0';
   last_played_str[0]           = '\0';
   runtime_info.runtime_str     = runtime_str;
   runtime_info.last_played_str = last_played_str;

   /* Attempt to open log file */
   runtime_log = runtime_log_init(
         NULL,
         core_path,
         dir_runtime_log,
         dir_playlist,
         log_per_core);

   if (runtime_log)
   {
      /* Check whether a non-zero runtime has been recorded */
      if (runtime_log_has_runtime(runtime_log))
      {
         /* Read current runtime */
         runtime_log_get_runtime_str(runtime_log,
               runtime_str, sizeof(runtime_str));

         /* Read last played timestamp */
         runtime_log_get_last_played_str(runtime_log,
               last_played_str, sizeof(last_played_str),
               timedate_style, date_separator);

         /* Contentless core entry now contains valid runtime data */
         runtime_info.status = CONTENTLESS_CORE_RUNTIME_VALID;
      }

      /* Clean up */
      free(runtime_log);
   }

#if (defined(HAVE_OZONE) || defined(HAVE_MATERIALUI))
   /* Ozone and GLUI require runtime/last played strings
    * to be populated even when no runtime is recorded */
   if (runtime_info.status != CONTENTLESS_CORE_RUNTIME_VALID)
   {
      if (string_is_equal(menu_ident, "ozone") ||
          string_is_equal(menu_ident, "glui"))
      {
         runtime_log_get_runtime_str(NULL,
               runtime_str, sizeof(runtime_str));
         runtime_log_get_last_played_str(NULL,
               last_played_str, sizeof(last_played_str),
               timedate_style, date_separator);

         /* While runtime data does not exist, the contentless
          * core entry does now contain valid information... */
         runtime_info.status = CONTENTLESS_CORE_RUNTIME_VALID;
      }
   }
#endif

   /* Update contentless core */
   menu_contentless_cores_set_runtime(core_info->core_file_id.str,
         &runtime_info);
}
#endif
