/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (disk_index_file.c).
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

#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <formats/jsonsax_full.h>

#include "file_path_special.h"
#include "verbosity.h"
#include "msg_hash.h"

#include "disk_index_file.h"

/****************/
/* JSON Helpers */
/****************/

typedef struct
{
   JSON_Parser parser;
   JSON_Writer writer;
   RFILE *file;
   unsigned *current_entry_uint_val;
   char **current_entry_str_val;
   unsigned image_index;
   char *image_path;
} DCifJSONContext;

static JSON_Parser_HandlerResult DCifJSONObjectMemberHandler(JSON_Parser parser, char *pValue, size_t length, JSON_StringAttributes attributes)
{
   DCifJSONContext *pCtx = (DCifJSONContext*)JSON_Parser_GetUserData(parser);
   (void)attributes; /* unused */

   if (pCtx->current_entry_str_val)
   {
      /* something went wrong */
      RARCH_ERR("[disk index file] JSON parsing failed at line %d.\n", __LINE__);
      return JSON_Parser_Abort;
   }

   if (length)
   {
      if (string_is_equal(pValue, "image_index"))
         pCtx->current_entry_uint_val = &pCtx->image_index;
      else if (string_is_equal(pValue, "image_path"))
         pCtx->current_entry_str_val = &pCtx->image_path;
      /* ignore unknown members */
   }

   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult DCifJSONNumberHandler(JSON_Parser parser, char *pValue, size_t length, JSON_StringAttributes attributes)
{
   DCifJSONContext *pCtx = (DCifJSONContext*)JSON_Parser_GetUserData(parser);
   (void)attributes; /* unused */

   if (pCtx->current_entry_uint_val && length && !string_is_empty(pValue))
      *pCtx->current_entry_uint_val = string_to_unsigned(pValue);
   /* ignore unknown members */

   pCtx->current_entry_uint_val = NULL;

   return JSON_Parser_Continue;
}

static JSON_Parser_HandlerResult DCifJSONStringHandler(JSON_Parser parser, char *pValue, size_t length, JSON_StringAttributes attributes)
{
   DCifJSONContext *pCtx = (DCifJSONContext*)JSON_Parser_GetUserData(parser);
   (void)attributes; /* unused */

   if (pCtx->current_entry_str_val && length && !string_is_empty(pValue))
   {
      if (*pCtx->current_entry_str_val)
         free(*pCtx->current_entry_str_val);

      *pCtx->current_entry_str_val = strdup(pValue);
   }
   /* ignore unknown members */

   pCtx->current_entry_str_val = NULL;

   return JSON_Parser_Continue;
}

static JSON_Writer_HandlerResult DCifJSONOutputHandler(JSON_Writer writer, const char *pBytes, size_t length)
{
   DCifJSONContext *context = (DCifJSONContext*)JSON_Writer_GetUserData(writer);
   (void)writer; /* unused */

   return filestream_write(context->file, pBytes, length) == length ? JSON_Writer_Continue : JSON_Writer_Abort;
}

static void DCifJSONLogError(DCifJSONContext *pCtx)
{
   if (pCtx->parser && JSON_Parser_GetError(pCtx->parser) != JSON_Error_AbortedByHandler)
   {
      JSON_Error error            = JSON_Parser_GetError(pCtx->parser);
      JSON_Location errorLocation = { 0, 0, 0 };

      (void)JSON_Parser_GetErrorLocation(pCtx->parser, &errorLocation);
      RARCH_ERR("[disk index file] Error: Invalid JSON at line %d, column %d (input byte %d) - %s.\n",
            (int)errorLocation.line + 1,
            (int)errorLocation.column + 1,
            (int)errorLocation.byte,
            JSON_ErrorString(error));
   }
   else if (pCtx->writer && JSON_Writer_GetError(pCtx->writer) != JSON_Error_AbortedByHandler)
   {
      RARCH_ERR("[disk index file] Error: could not write output - %s.\n", JSON_ErrorString(JSON_Writer_GetError(pCtx->writer)));
   }
}

/******************/
/* Initialisation */
/******************/

/* Resets existing disk index record */
static void disk_index_file_reset(disk_index_file_t *disk_index_file)
{
   if (!disk_index_file)
      return;

   disk_index_file->modified      = false;
   disk_index_file->image_index   = 0;
   disk_index_file->image_path[0] = '\0';
   disk_index_file->file_path[0]  = '\0';
}

/* Parses disk index file referenced by
 * disk_index_file->file_path.
 * Does nothing if disk index file does not exist. */
static bool disk_index_file_read(disk_index_file_t *disk_index_file)
{
   bool success            = false;
   DCifJSONContext context = {0};
   RFILE *file             = NULL;

   /* Sanity check */
   if (!disk_index_file)
      return false;

   if (string_is_empty(disk_index_file->file_path))
      return false;

   if (!path_is_valid(disk_index_file->file_path))
      return false;

   /* Attempt to open disk index file */
   file = filestream_open(
         disk_index_file->file_path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR(
            "[disk index file] Failed to open disk index record file: %s\n",
            disk_index_file->file_path);
      return false;
   }

   /* Initialise JSON parser */
   context.image_index = 0;
   context.image_path  = NULL;
   context.parser      = JSON_Parser_Create(NULL);
   context.file        = file;

   if (!context.parser)
   {
      RARCH_ERR("[disk index file] Failed to create JSON parser.\n");
      goto end;
   }

   /* Configure parser */
   JSON_Parser_SetAllowBOM(context.parser, JSON_True);
   JSON_Parser_SetNumberHandler(context.parser,       &DCifJSONNumberHandler);
   JSON_Parser_SetStringHandler(context.parser,       &DCifJSONStringHandler);
   JSON_Parser_SetObjectMemberHandler(context.parser, &DCifJSONObjectMemberHandler);
   JSON_Parser_SetUserData(context.parser, &context);

   /* Read file */
   while (!filestream_eof(file))
   {
      /* Disk index files are tiny - use small chunk size */
      char chunk[128] = {0};
      int64_t length  = filestream_read(file, chunk, sizeof(chunk));

      /* Error checking... */
      if (!length && !filestream_eof(file))
      {
         RARCH_ERR(
               "[disk index file] Failed to read disk index file: %s\n",
               disk_index_file->file_path);
         JSON_Parser_Free(context.parser);
         goto end;
      }

      /* Parse chunk */
      if (!JSON_Parser_Parse(context.parser, chunk, length, JSON_False))
      {
         RARCH_ERR(
               "[disk index file] Error parsing chunk of disk index file: %s\n---snip---\n%s\n---snip---\n",
               disk_index_file->file_path, chunk);
         DCifJSONLogError(&context);
         JSON_Parser_Free(context.parser);
         goto end;
      }
   }

   /* Finalise parsing */
   if (!JSON_Parser_Parse(context.parser, NULL, 0, JSON_True))
   {
      RARCH_WARN(
            "[disk index file] Error parsing disk index file: %s\n",
            disk_index_file->file_path);
      DCifJSONLogError(&context);
      JSON_Parser_Free(context.parser);
      goto end;
   }

   /* Free parser */
   JSON_Parser_Free(context.parser);

   /* Copy values read from JSON file */
   disk_index_file->image_index = context.image_index;

   if (!string_is_empty(context.image_path))
      strlcpy(
            disk_index_file->image_path, context.image_path,
            sizeof(disk_index_file->image_path));
   else
      disk_index_file->image_path[0] = '\0';

   success = true;

end:
   /* Clean up leftover strings */
   if (context.image_path)
      free(context.image_path);

   /* Close log file */
   filestream_close(file);

   return success;
}

/* Initialises existing disk index record, loading
 * current parameters if a record file exists.
 * Returns false if arguments are invalid. */
bool disk_index_file_init(
      disk_index_file_t *disk_index_file,
      const char *content_path,
      const char *dir_savefile)
{
   const char *content_file = NULL;
   char content_name[PATH_MAX_LENGTH];
   char disk_index_file_dir[PATH_MAX_LENGTH];
   char disk_index_file_path[PATH_MAX_LENGTH];

   content_name[0]         = '\0';
   disk_index_file_dir[0]  = '\0';
   disk_index_file_path[0] = '\0';

   /* Sanity check */
   if (!disk_index_file)
      return false;

   /* Disk index records are only valid when loading
    * content (i.e. they do not apply to contentless
    * cores) */
   if (string_is_empty(content_path))
      goto error;

   /* Build disk index file path */

   /* > Get content name */
   content_file = path_basename(content_path);
   if (string_is_empty(content_file))
      goto error;

   strlcpy(content_name, content_file, sizeof(content_name));
   path_remove_extension(content_name);
   if (string_is_empty(content_name))
      goto error;

   /* > Get disk index file directory */
   if (!string_is_empty(dir_savefile))
      strlcpy(disk_index_file_dir, dir_savefile, sizeof(disk_index_file_dir));
   else
   {
      /* Use content directory */
      strlcpy(disk_index_file_dir, content_path, sizeof(disk_index_file_dir));
      path_basedir(disk_index_file_dir);
   }

   /* > Create directory, if required */
   if (!path_is_directory(disk_index_file_dir))
   {
      if (!path_mkdir(disk_index_file_dir))
      {
         RARCH_ERR(
               "[disk index file] failed to create directory for disk index file: %s\n",
               disk_index_file_dir);
         goto error;
      }
   }

   /* > Generate final path */
   fill_pathname_join(
         disk_index_file_path, disk_index_file_dir,
         content_name, sizeof(disk_index_file_path));
   strlcat(
         disk_index_file_path,
         file_path_str(FILE_PATH_DISK_CONTROL_INDEX_EXTENSION),
         sizeof(disk_index_file_path));
   if (string_is_empty(disk_index_file_path))
      goto error;

   /* All is well - reset disk_index_file_t and
    * attempt to load values from file */
   disk_index_file_reset(disk_index_file);
   strlcpy(
         disk_index_file->file_path,
         disk_index_file_path,
         sizeof(disk_index_file->file_path));

   /* > If file does not exist (or some other
    *   error occurs) then this is a new record
    *   - in this case, 'modified' flag should
    *   be set to 'true' */
   if (!disk_index_file_read(disk_index_file))
      disk_index_file->modified = true;

   return true;

error:
   disk_index_file_reset(disk_index_file);
   return false;
}

/***********/
/* Setters */
/***********/

/* Sets image index and path */
void disk_index_file_set(
      disk_index_file_t *disk_index_file,
      unsigned image_index,
      const char *image_path)
{
   if (!disk_index_file)
      return;

   /* Check whether image index should be updated */
   if (disk_index_file->image_index != image_index)
   {
      disk_index_file->image_index = image_index;
      disk_index_file->modified    = true;
   }

   /* Check whether image path should be updated */
   if (!string_is_empty(image_path))
   {
      if (!string_is_equal(disk_index_file->image_path, image_path))
      {
         strlcpy(
               disk_index_file->image_path, image_path,
               sizeof(disk_index_file->image_path));
         disk_index_file->modified = true;
      }
   }
   else if (!string_is_empty(disk_index_file->image_path))
   {
      disk_index_file->image_path[0] = '\0';
      disk_index_file->modified      = true;
   }
}

/**********/
/* Saving */
/**********/

/* Saves specified disk index file to disk */
bool disk_index_file_save(disk_index_file_t *disk_index_file)
{
   char value_string[32];
   DCifJSONContext context = {0};
   RFILE *file             = NULL;
   bool success            = false;
   int n;

   value_string[0] = '\0';

   /* Sanity check */
   if (!disk_index_file)
      return false;

   /* > Only save file if record has been modified.
    *   We return true in this case - since there
    *   was nothing to write, there can be no
    *   'failure' */
   if (!disk_index_file->modified)
      return true;
   
   if (string_is_empty(disk_index_file->file_path))
      return false;

   RARCH_LOG(
         "[disk index file] Saving disk index file: %s\n",
         disk_index_file->file_path);
   
   /* Attempt to open disk index file */
   file = filestream_open(
         disk_index_file->file_path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR(
            "[disk index file] Failed to open disk index file: %s\n",
            disk_index_file->file_path);
      return false;
   }

   /* Initialise JSON writer */
   context.writer = JSON_Writer_Create(NULL);
   context.file   = file;

   if (!context.writer)
   {
      RARCH_ERR("[disk index file] Failed to create JSON writer.\n");
      goto end;
   }

   /* Configure JSON writer */
   JSON_Writer_SetOutputEncoding(context.writer, JSON_UTF8);
   JSON_Writer_SetOutputHandler(context.writer, &DCifJSONOutputHandler);
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

   /* > image index entry */
   n = snprintf(
         value_string, sizeof(value_string),
         "%u", disk_index_file->image_index);
   if ((n < 0) || (n >= 32))
      n = 0; /* Silence GCC warnings... */

   JSON_Writer_WriteSpace(context.writer, 2);
   JSON_Writer_WriteString(context.writer, "image_index",
         STRLEN_CONST("image_index"), JSON_UTF8);
   JSON_Writer_WriteColon(context.writer);
   JSON_Writer_WriteSpace(context.writer, 1);
   JSON_Writer_WriteNumber(context.writer, value_string,
         strlen(value_string), JSON_UTF8);
   JSON_Writer_WriteComma(context.writer);
   JSON_Writer_WriteNewLine(context.writer);

   /* > image path entry */
   JSON_Writer_WriteSpace(context.writer, 2);
   JSON_Writer_WriteString(context.writer, "image_path",
         STRLEN_CONST("image_path"), JSON_UTF8);
   JSON_Writer_WriteColon(context.writer);
   JSON_Writer_WriteSpace(context.writer, 1);
   JSON_Writer_WriteString(context.writer,
         disk_index_file->image_path,
         strlen(disk_index_file->image_path), JSON_UTF8);
   JSON_Writer_WriteNewLine(context.writer);

   /* > Finalise */
   JSON_Writer_WriteEndObject(context.writer);
   JSON_Writer_WriteNewLine(context.writer);

   /* Free JSON writer */
   JSON_Writer_Free(context.writer);

   /* Changes have been written - record
    * is no longer considered to be in a
    * 'modified' state */
   disk_index_file->modified = false;
   success                   = true;

end:
   /* Close disk index file */
   filestream_close(file);

   return success;
}
