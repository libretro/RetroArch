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

#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <formats/jsonsax_full.h>
#include <string/stdstring.h>

#include "file_path_special.h"
#include "dirs.h"
#include "core_info.h"
#include "configuration.h"
#include "verbosity.h"

#include "runtime_file.h"

#define LOG_FILE_RUNTIME_FORMAT_STR "%u:%02u:%02u"
#define LOG_FILE_LAST_PLAYED_FORMAT_STR "%04u-%02u-%02u %02u:%02u:%02u"

/* JSON Stuff... */

typedef struct
{
   JSON_Parser parser;
   JSON_Writer writer;
   RFILE *file;
   char **current_entry_val;
   char *runtime_string;
   char *last_played_string;
} RtlJSONContext;

static JSON_Parser_HandlerResult RtlJSONObjectMemberHandler(JSON_Parser parser, char *pValue, size_t length, JSON_StringAttributes attributes)
{
   RtlJSONContext *pCtx = (RtlJSONContext*)JSON_Parser_GetUserData(parser);
   (void)attributes; /* unused */
   
   if (pCtx->current_entry_val)
   {
      /* something went wrong */
      RARCH_ERR("JSON parsing failed at line %d.\n", __LINE__);
      return JSON_Parser_Abort;
   }
   
   if (length)
   {
      if (string_is_equal(pValue, "runtime"))
         pCtx->current_entry_val = &pCtx->runtime_string;
      else if (string_is_equal(pValue, "last_played"))
         pCtx->current_entry_val = &pCtx->last_played_string;
      /* ignore unknown members */
   }
   
   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult RtlJSONStringHandler(JSON_Parser parser, char *pValue, size_t length, JSON_StringAttributes attributes)
{
   RtlJSONContext *pCtx = (RtlJSONContext*)JSON_Parser_GetUserData(parser);
   (void)attributes; /* unused */
   
   if (pCtx->current_entry_val && length && !string_is_empty(pValue))
   {
      if (*pCtx->current_entry_val)
         free(*pCtx->current_entry_val);
      
      *pCtx->current_entry_val = strdup(pValue);
   }
   /* ignore unknown members */
   
   pCtx->current_entry_val = NULL;
   
   return JSON_Parser_Continue;
}

static JSON_Writer_HandlerResult RtlJSONOutputHandler(JSON_Writer writer, const char *pBytes, size_t length)
{
   RtlJSONContext *context = (RtlJSONContext*)JSON_Writer_GetUserData(writer);
   (void)writer; /* unused */
   
   return filestream_write(context->file, pBytes, length) == length ? JSON_Writer_Continue : JSON_Writer_Abort;
}

static void RtlJSONLogError(RtlJSONContext *pCtx)
{
   if (pCtx->parser && JSON_Parser_GetError(pCtx->parser) != JSON_Error_AbortedByHandler)
   {
      JSON_Error error            = JSON_Parser_GetError(pCtx->parser);
      JSON_Location errorLocation = { 0, 0, 0 };

      (void)JSON_Parser_GetErrorLocation(pCtx->parser, &errorLocation);
      RARCH_ERR("Error: Invalid JSON at line %d, column %d (input byte %d) - %s.\n",
            (int)errorLocation.line + 1,
            (int)errorLocation.column + 1,
            (int)errorLocation.byte,
            JSON_ErrorString(error));
   }
   else if (pCtx->writer && JSON_Writer_GetError(pCtx->writer) != JSON_Error_AbortedByHandler)
   {
      RARCH_ERR("Error: could not write output - %s.\n", JSON_ErrorString(JSON_Writer_GetError(pCtx->writer)));
   }
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
   /* Attempt to open log file */
   RFILE *file                 = filestream_open(runtime_log->path,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   
   if (!file)
   {
      RARCH_ERR("Failed to open runtime log file: %s\n", runtime_log->path);
      return;
   }
   
   /* Initialise JSON parser */
   context.runtime_string     = NULL;
   context.last_played_string = NULL;
   context.parser             = JSON_Parser_Create(NULL);
   context.file               = file;
   
   if (!context.parser)
   {
      RARCH_ERR("Failed to create JSON parser.\n");
      goto end;
   }
   
   /* Configure parser */
   JSON_Parser_SetAllowBOM(context.parser, JSON_True);
   JSON_Parser_SetStringHandler(context.parser, &RtlJSONStringHandler);
   JSON_Parser_SetObjectMemberHandler(context.parser, &RtlJSONObjectMemberHandler);
   JSON_Parser_SetUserData(context.parser, &context);
   
   /* Read file */
   while (!filestream_eof(file))
   {
      /* Runtime log files are tiny - use small chunk size */
      char chunk[128] = {0};
      int64_t length  = filestream_read(file, chunk, sizeof(chunk));
      
      /* Error checking... */
      if (!length && !filestream_eof(file))
      {
         RARCH_ERR("Failed to read runtime log file: %s\n", runtime_log->path);
         JSON_Parser_Free(context.parser);
         goto end;
      }
      
      /* Parse chunk */
      if (!JSON_Parser_Parse(context.parser, chunk, length, JSON_False))
      {
         RARCH_ERR("Error parsing chunk of runtime log file: %s\n---snip---\n%s\n---snip---\n", runtime_log->path, chunk);
         RtlJSONLogError(&context);
         JSON_Parser_Free(context.parser);
         goto end;
      }
   }
   
   /* Finalise parsing */
   if (!JSON_Parser_Parse(context.parser, NULL, 0, JSON_True))
   {
      RARCH_WARN("Error parsing runtime log file: %s\n", runtime_log->path);
      RtlJSONLogError(&context);
      JSON_Parser_Free(context.parser);
      goto end;
   }
   
   /* Free parser */
   JSON_Parser_Free(context.parser);
   
   /* Process string values read from JSON file */
   
   /* Runtime */
   if (!string_is_empty(context.runtime_string))
   {
      if (sscanf(context.runtime_string, LOG_FILE_RUNTIME_FORMAT_STR,
               &runtime_hours, &runtime_minutes, &runtime_seconds) != 3)
      {
         RARCH_ERR("Runtime log file - invalid 'runtime' entry detected: %s\n", runtime_log->path);
         goto end;
      }
   }
   
   /* Last played */
   if (!string_is_empty(context.last_played_string))
   {
      if (sscanf(context.last_played_string, LOG_FILE_LAST_PLAYED_FORMAT_STR,
               &last_played_year, &last_played_month, &last_played_day,
               &last_played_hour, &last_played_minute, &last_played_second) != 6)
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
 * Returns NULL if content_path and/or core_path are invalid */
runtime_log_t *runtime_log_init(const char *content_path,
      const char *core_path, bool log_per_core)
{
   unsigned i;
   char content_name[PATH_MAX_LENGTH];
   char core_name[PATH_MAX_LENGTH];
   char log_file_dir[PATH_MAX_LENGTH];
   char log_file_path[PATH_MAX_LENGTH];
   char tmp_buf[PATH_MAX_LENGTH];
   settings_t *settings           = config_get_ptr();
   core_info_list_t *core_info    = NULL;
   runtime_log_t *runtime_log     = NULL;
   const char *core_path_basename = NULL;
   
   content_name[0]                = '\0';
   core_name[0]                   = '\0';
   log_file_dir[0]                = '\0';
   log_file_path[0]               = '\0';
   tmp_buf[0]                     = '\0';
   
   /* Error checking */
   if (!settings)
      return NULL;
   
   if (  string_is_empty(settings->paths.directory_runtime_log) && 
         string_is_empty(settings->paths.directory_playlist))
   {
      RARCH_ERR("Runtime log directory is undefined - cannot save"
            " runtime log files.\n");
      return NULL;
   }

   core_path_basename = path_basename(core_path);
   
   if (  string_is_empty(content_path) || 
         string_is_empty(core_path_basename))
      return NULL;
   
   if (  string_is_equal(core_path, "builtin") || 
         string_is_equal(core_path, file_path_str(FILE_PATH_DETECT)))
      return NULL;
   
   /* Get core name
    * Note: An annoyance - this is required even when
    * we are performing aggregate (not per core) logging,
    * since content name is sometimes dependent upon core
    * (e.g. see TyrQuake below) */
   core_info_get_list(&core_info);
   
   if (!core_info)
      return NULL;
   
   for (i = 0; i < core_info->count; i++)
   {
      const char *entry_core_name = core_info->list[i].core_name;
      if (!string_is_equal(
               path_basename(core_info->list[i].path), core_path_basename))
         continue;

      if (string_is_empty(entry_core_name))
         return NULL;

      strlcpy(core_name, entry_core_name, sizeof(core_name));
      break;
   }
   
   if (string_is_empty(core_name))
      return NULL;
   
   /* Get runtime log directory */
   if (string_is_empty(settings->paths.directory_runtime_log))
   {
      /* If 'custom' runtime log path is undefined,
       * use default 'playlists/logs' directory... */
      fill_pathname_join(
            tmp_buf,
            settings->paths.directory_playlist,
            "logs",
            sizeof(tmp_buf));
   }
   else
      strlcpy(tmp_buf,
            settings->paths.directory_runtime_log, sizeof(tmp_buf));
   
   if (string_is_empty(tmp_buf))
      return NULL;
   
   if (log_per_core)
      fill_pathname_join(
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
            memset(tmp_buf, 0, sizeof(tmp_buf));
            strlcpy(tmp_buf, content_path, path_length * sizeof(char));
            strlcpy(content_name, path_basename(tmp_buf), sizeof(content_name));
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
   }
   
   if (string_is_empty(content_name))
      return NULL;
   
   /* Build final log file path */
   fill_pathname_join(log_file_path, log_file_dir, content_name, sizeof(log_file_path));
   strlcat(log_file_path, file_path_str(FILE_PATH_RUNTIME_EXTENSION), sizeof(log_file_path));
   
   if (string_is_empty(log_file_path))
      return NULL;

   /* Phew... If we get this far then all is well.
    * > Create 'runtime_log' object */
   runtime_log                     = (runtime_log_t*)calloc(1, sizeof(*runtime_log));
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
   
   strlcpy(runtime_log->path, log_file_path, sizeof(runtime_log->path));
   
   /* Load existing log file, if it exists */
   if (filestream_exists(runtime_log->path))
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
   runtime_log->last_played.year   = (unsigned)time_info->tm_year + 1900;
   runtime_log->last_played.month  = (unsigned)time_info->tm_mon + 1;
   runtime_log->last_played.day    = (unsigned)time_info->tm_mday;
   runtime_log->last_played.hour   = (unsigned)time_info->tm_hour;
   runtime_log->last_played.minute = (unsigned)time_info->tm_min;
   runtime_log->last_played.second = (unsigned)time_info->tm_sec;
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

/* Gets last played entry values as a time_t 'object'
 * (e.g. for printing with strftime()) */
void runtime_log_get_last_played_time(runtime_log_t *runtime_log, time_t *time)
{
   struct tm time_info;
   
   if (!runtime_log || !time)
      return;
   
   /* Set tm values */
   time_info.tm_year  = (int)runtime_log->last_played.year  - 1900;
   time_info.tm_mon   = (int)runtime_log->last_played.month - 1;
   time_info.tm_mday  = (int)runtime_log->last_played.day;
   time_info.tm_hour  = (int)runtime_log->last_played.hour;
   time_info.tm_min   = (int)runtime_log->last_played.minute;
   time_info.tm_sec   = (int)runtime_log->last_played.second;
   time_info.tm_isdst = -1;
   
   /* Get time */
   *time              = mktime(&time_info);
}

/* Status */

/* Returns true if log has a non-zero runtime entry */
bool runtime_log_has_runtime(runtime_log_t *runtime_log)
{
   if (!runtime_log)
      return false;
   
   return !((runtime_log->runtime.hours   == 0) &&
            (runtime_log->runtime.minutes == 0) &&
            (runtime_log->runtime.seconds == 0));
}

/* Returns true if log has a non-zero last played entry */
bool runtime_log_has_last_played(runtime_log_t *runtime_log)
{
   if (!runtime_log)
      return false;
   
   return !((runtime_log->last_played.year   == 0) &&
            (runtime_log->last_played.month  == 0) &&
            (runtime_log->last_played.day    == 0) &&
            (runtime_log->last_played.hour   == 0) &&
            (runtime_log->last_played.minute == 0) &&
            (runtime_log->last_played.second == 0));
}

/* Saving */

/* Saves specified runtime log to disk */
void runtime_log_save(runtime_log_t *runtime_log)
{
   int n;
   char value_string[64]; /* 64 characters should be 
                             enough for a very long runtime... :) */
   RtlJSONContext context = {0};
   RFILE *file            = NULL;
   
   if (!runtime_log)
      return;
   
   RARCH_LOG("Saving runtime log file: %s\n", runtime_log->path);
   
   /* Attempt to open log file */
   file = filestream_open(runtime_log->path,
         RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   
   if (!file)
   {
      RARCH_ERR("Failed to open runtime log file: %s\n", runtime_log->path);
      return;
   }
   
   /* Initialise JSON writer */
   context.writer = JSON_Writer_Create(NULL);
   context.file   = file;
   
   if (!context.writer)
   {
      RARCH_ERR("Failed to create JSON writer.\n");
      goto end;
   }
   
   /* Configure JSON writer */
   JSON_Writer_SetOutputEncoding(context.writer, JSON_UTF8);
   JSON_Writer_SetOutputHandler(context.writer, &RtlJSONOutputHandler);
   JSON_Writer_SetUserData(context.writer, &context);
   
   /* Write output file */
   JSON_Writer_WriteStartObject(context.writer);
   JSON_Writer_WriteNewLine(context.writer);
   
   /* > Version entry */
   JSON_Writer_WriteSpace(context.writer, 2);
   JSON_Writer_WriteString(context.writer, "version",
         STRLEN_CONST("version"), JSON_UTF8);
   JSON_Writer_WriteColon(context.writer);
   JSON_Writer_WriteSpace(context.writer, 1);
   JSON_Writer_WriteString(context.writer, "1.0",
         STRLEN_CONST("1.0"), JSON_UTF8);
   JSON_Writer_WriteComma(context.writer);
   JSON_Writer_WriteNewLine(context.writer);
   
   /* > Runtime entry */
   value_string[0] = '\0';
   n               = snprintf(value_string,
         sizeof(value_string), LOG_FILE_RUNTIME_FORMAT_STR,
         runtime_log->runtime.hours, runtime_log->runtime.minutes,
         runtime_log->runtime.seconds);
   if ((n < 0) || (n >= 64))
      n = 0; /* Silence GCC warnings... */
   
   JSON_Writer_WriteSpace(context.writer, 2);
   JSON_Writer_WriteString(context.writer, "runtime",
         STRLEN_CONST("runtime"), JSON_UTF8);
   JSON_Writer_WriteColon(context.writer);
   JSON_Writer_WriteSpace(context.writer, 1);
   JSON_Writer_WriteString(context.writer, value_string,
         strlen(value_string), JSON_UTF8);
   JSON_Writer_WriteComma(context.writer);
   JSON_Writer_WriteNewLine(context.writer);
   
   /* > Last played entry */
   value_string[0] = '\0';
   n               = snprintf(value_string, sizeof(value_string),
         LOG_FILE_LAST_PLAYED_FORMAT_STR,
         runtime_log->last_played.year, runtime_log->last_played.month,
         runtime_log->last_played.day,
         runtime_log->last_played.hour, runtime_log->last_played.minute,
         runtime_log->last_played.second);
   if ((n < 0) || (n >= 64))
      n = 0; /* Silence GCC warnings... */
   
   JSON_Writer_WriteSpace(context.writer, 2);
   JSON_Writer_WriteString(context.writer, "last_played",
         STRLEN_CONST("last_played"), JSON_UTF8);
   JSON_Writer_WriteColon(context.writer);
   JSON_Writer_WriteSpace(context.writer, 1);
   JSON_Writer_WriteString(context.writer, value_string,
         strlen(value_string), JSON_UTF8);
   JSON_Writer_WriteNewLine(context.writer);
   
   /* > Finalise */
   JSON_Writer_WriteEndObject(context.writer);
   JSON_Writer_WriteNewLine(context.writer);
   
   /* Free JSON writer */
   JSON_Writer_Free(context.writer);
   
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
