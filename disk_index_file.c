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
#include <formats/rjson.h>

#include "file_path_special.h"
#include "verbosity.h"
#include "msg_hash.h"

#include "disk_index_file.h"

/****************/
/* JSON Helpers */
/****************/

typedef struct
{
   unsigned *current_entry_uint_val;
   char **current_entry_str_val;
   unsigned image_index;
   char *image_path;
} DCifJSONContext;

static bool DCifJSONObjectMemberHandler(void* context, const char *pValue, size_t length)
{
   DCifJSONContext *pCtx = (DCifJSONContext*)context;

   /* something went wrong */
   if (pCtx->current_entry_str_val)
      return false;

   if (length)
   {
      if (string_is_equal(pValue, "image_index"))
         pCtx->current_entry_uint_val = &pCtx->image_index;
      else if (string_is_equal(pValue, "image_path"))
         pCtx->current_entry_str_val = &pCtx->image_path;
      /* ignore unknown members */
   }

   return true;
}

static bool DCifJSONNumberHandler(void* context, const char *pValue, size_t length)
{
   DCifJSONContext *pCtx = (DCifJSONContext*)context;

   if (pCtx->current_entry_uint_val && length && !string_is_empty(pValue))
      *pCtx->current_entry_uint_val = string_to_unsigned(pValue);
   /* ignore unknown members */

   pCtx->current_entry_uint_val = NULL;

   return true;
}

static bool DCifJSONStringHandler(void* context, const char *pValue, size_t length)
{
   DCifJSONContext *pCtx = (DCifJSONContext*)context;

   if (pCtx->current_entry_str_val && length && !string_is_empty(pValue))
   {
      if (*pCtx->current_entry_str_val)
         free(*pCtx->current_entry_str_val);

      *pCtx->current_entry_str_val = strdup(pValue);
   }
   /* ignore unknown members */

   pCtx->current_entry_str_val = NULL;

   return true;
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
   const char *file_path   = NULL;
   bool success            = false;
   DCifJSONContext context = {0};
   RFILE *file             = NULL;
   rjson_t* parser;

   /* Sanity check */
   if (!disk_index_file)
      return false;

   file_path = disk_index_file->file_path;

   if (   string_is_empty(file_path) ||
         !path_is_valid(file_path)
      )
      return false;

   /* Attempt to open disk index file */
   file = filestream_open(
         file_path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR(
            "[disk index file] Failed to open disk index record file: %s\n",
            file_path);
      return false;
   }

   /* Initialise JSON parser */
   parser = rjson_open_rfile(file);
   if (!parser)
   {
      RARCH_ERR("[disk index file] Failed to create JSON parser.\n");
      goto end;
   }

   /* Configure parser */
   rjson_set_options(parser, RJSON_OPTION_ALLOW_UTF8BOM);

   /* Read file */
   if (rjson_parse(parser, &context,
         DCifJSONObjectMemberHandler,
         DCifJSONStringHandler,
         DCifJSONNumberHandler,
         NULL, NULL, NULL, NULL, /* unused object/array handlers */
         NULL, NULL) /* unused boolean/null handlers */
         != RJSON_DONE)
   {
      if (rjson_get_source_context_len(parser))
      {
         RARCH_ERR(
               "[disk index file] Error parsing chunk of disk index file: %s\n---snip---\n%.*s\n---snip---\n",
               file_path,
               rjson_get_source_context_len(parser),
               rjson_get_source_context_buf(parser));
      }
      RARCH_WARN(
            "[disk index file] Error parsing disk index file: %s\n",
            file_path);
      RARCH_ERR("[disk index file] Error: Invalid JSON at line %d, column %d - %s.\n",
            (int)rjson_get_source_line(parser),
            (int)rjson_get_source_column(parser),
            (*rjson_get_error(parser) ? rjson_get_error(parser) : "format error"));
   }

   /* Free parser */
   rjson_free(parser);

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
   size_t len;
   const char *content_file = NULL;
   char content_name[256];
   char disk_index_file_dir[PATH_MAX_LENGTH];
   char disk_index_file_path[PATH_MAX_LENGTH];

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
   len = fill_pathname_join_special(
         disk_index_file_path, disk_index_file_dir,
         content_name, sizeof(disk_index_file_path));
   disk_index_file_path[len  ] = '.';
   disk_index_file_path[len+1] = 'l';
   disk_index_file_path[len+2] = 'd';
   disk_index_file_path[len+3] = 'c';
   disk_index_file_path[len+4] = 'i';
   disk_index_file_path[len+5] = '\0';
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
   const char *file_path;
   rjsonwriter_t* writer;
   RFILE *file             = NULL;
   bool success            = false;

   /* Sanity check */
   if (!disk_index_file)
      return false;

   /* > Only save file if record has been modified.
    *   We return true in this case - since there
    *   was nothing to write, there can be no
    *   'failure' */
   if (!disk_index_file->modified)
      return true;

   file_path = disk_index_file->file_path;
   
   if (string_is_empty(file_path))
      return false;

   RARCH_LOG(
         "[disk index file] Saving disk index file: %s\n",
         file_path);
   
   /* Attempt to open disk index file */
   if (!(file = filestream_open(
         file_path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      RARCH_ERR(
            "[disk index file] Failed to open disk index file: %s\n",
            file_path);
      return false;
   }

   /* Initialise JSON writer */
   if (!(writer = rjsonwriter_open_rfile(file)))
   {
      RARCH_ERR("[disk index file] Failed to create JSON writer.\n");
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

   /* > image index entry */
   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_add_string(writer, "image_index");
   rjsonwriter_raw(writer, ":", 1);
   rjsonwriter_raw(writer, " ", 1);
   rjsonwriter_rawf(writer, "%u", disk_index_file->image_index);
   rjsonwriter_raw(writer, ",", 1);
   rjsonwriter_raw(writer, "\n", 1);

   /* > image path entry */
   rjsonwriter_add_spaces(writer, 2);
   rjsonwriter_add_string(writer, "image_path");
   rjsonwriter_raw(writer, ":", 1);
   rjsonwriter_raw(writer, " ", 1);
   rjsonwriter_add_string(writer, disk_index_file->image_path);
   rjsonwriter_raw(writer, "\n", 1);

   /* > Finalise */
   rjsonwriter_raw(writer, "}", 1);
   rjsonwriter_raw(writer, "\n", 1);

   /* Free JSON writer */
   if (!rjsonwriter_free(writer))
   {
      RARCH_ERR("[disk index file] Error writing disk index file: %s\n", file_path);
   }

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
