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

static bool DCifJSONObjectMemberHandler(void* context, const char *pValue, size_t len)
{
   DCifJSONContext *pCtx = (DCifJSONContext*)context;

   /* something went wrong */
   if (pCtx->current_entry_str_val)
      return false;

   if (len)
   {
      if (string_is_equal(pValue, "image_index"))
         pCtx->current_entry_uint_val = &pCtx->image_index;
      else if (string_is_equal(pValue, "image_path"))
         pCtx->current_entry_str_val = &pCtx->image_path;
      /* ignore unknown members */
   }

   return true;
}

static bool DCifJSONNumberHandler(void* context, const char *pValue, size_t len)
{
   DCifJSONContext *pCtx = (DCifJSONContext*)context;

   if (pCtx->current_entry_uint_val && len && (pValue && *pValue))
      *pCtx->current_entry_uint_val = string_to_unsigned(pValue);
   /* ignore unknown members */

   pCtx->current_entry_uint_val = NULL;

   return true;
}

static bool DCifJSONStringHandler(void* context, const char *pValue, size_t len)
{
   DCifJSONContext *pCtx = (DCifJSONContext*)context;

   if (pCtx->current_entry_str_val && len && (pValue && *pValue))
   {
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

/* Parses disk index file referenced by
 * disk_index_file->file_path.
 * Does nothing if disk index file does not exist. */
static bool disk_index_file_read(disk_index_file_t *disk_index_file)
{
   const char *file_path   = NULL;
   bool success            = false;
   DCifJSONContext context = {0};
   uint8_t *file_buf       = NULL;
   int64_t file_len        = 0;
   rjson_t* parser;

   /* Sanity check */
   if (!disk_index_file)
      return false;

   file_path = disk_index_file->file_path;

   if (!file_path || !*file_path)
      return false;

   /* Read the whole record in one operation: these files are tiny
    * and always parsed in full, so a single open/size/read/close
    * beats a pre-open stat plus the chunked callback path (which
    * itself sizes the stream with an extra fstat).  Most content
    * has no disk index record - that common case is one failed
    * open, and the stat runs only to classify a failure as worth
    * logging. */
   if (!filestream_read_file(file_path,
         (void**)&file_buf, &file_len))
   {
      if (path_is_valid(file_path))
         RARCH_ERR(
               "[Disk index file] Failed to open disk index record file: \"%s\".\n",
               file_path);
      return false;
   }

   /* A zero-length record is the residue of an interrupted
    * write, not a JSON document: treat it like an absent file
    * - the caller marks the record modified and the next save
    * replaces it - instead of reporting a JSON format error on
    * every launch for a file that never held any data. */
   if (file_len == 0)
   {
      RARCH_WARN(
            "[Disk index file] Empty disk index file: \"%s\". Record will be regenerated.\n",
            file_path);
      goto end;
   }

   /* Initialise JSON parser */
   if (!(parser = rjson_open_buffer(file_buf, (size_t)file_len)))
   {
      RARCH_ERR("[Disk index file] Failed to create JSON parser.\n");
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
               "[Disk index file] Error parsing chunk of disk index file: %s\n---snip---\n%.*s\n---snip---\n",
               file_path,
               rjson_get_source_context_len(parser),
               rjson_get_source_context_buf(parser));
      }
      RARCH_WARN(
            "[Disk index file] Error parsing disk index file: \"%s\".\n",
            file_path);
      RARCH_ERR("[Disk index file] Error: Invalid JSON at line %d, column %d - %s.\n",
            (int)rjson_get_source_line(parser),
            (int)rjson_get_source_column(parser),
            (*rjson_get_error(parser) ? rjson_get_error(parser) : "format error"));

      /* A record that does not parse cannot be trusted - discard
       * any partially extracted values and report failure, so the
       * caller marks the record modified and the next save
       * replaces the broken file.  This restores the pre-rjson
       * behaviour: the jsonsax reader failed here, but the
       * migration in ba1ed2da4b fell through to success, leaving
       * corrupt records in place to fail again on every launch. */
      rjson_free(parser);
      goto end;
   }

   /* Free parser */
   rjson_free(parser);

   /* Copy values read from JSON file */
   disk_index_file->image_index = context.image_index;

   if (context.image_path && *context.image_path)
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

   /* Release file contents */
   free(file_buf);

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
   size_t _len;
   char content_name[NAME_MAX_LENGTH];
   char disk_index_file_dir[DIR_MAX_LENGTH];

   /* Sanity check */
   if (!disk_index_file)
      return false;

   /* Disk index records are only valid when loading
    * content (i.e. they do not apply to contentless
    * cores) */
   if (!content_path || !*content_path)
      goto error;

   /* Build disk index file path */
   fill_pathname(content_name, path_basename(content_path), "",
         sizeof(content_name));
   if (!*content_name)
      goto error;

   /* > Get disk index file directory */
   if (dir_savefile && *dir_savefile)
      strlcpy(disk_index_file_dir, dir_savefile, sizeof(disk_index_file_dir));
   else /* Use content directory */
      fill_pathname_basedir(disk_index_file_dir, content_path,
            sizeof(disk_index_file_dir));

   /* > Generate final path
    * Note: the directory is not created here - reading an
    * existing record does not need it to exist, and most
    * content never writes one.  disk_index_file_save()
    * creates it when a record is actually written, so the
    * common load path no longer pays a stat (plus a possible
    * mkdir) per content load. */
   _len = fill_pathname_join_special(
         disk_index_file->file_path, disk_index_file_dir,
         content_name, sizeof(disk_index_file->file_path));
   strlcpy(disk_index_file->file_path       + _len,
         FILE_PATH_DISK_CONTROL_INDEX_EXTENSION,
         sizeof(disk_index_file->file_path) - _len);

   /* All is well - reset disk_index_file_t and
    * attempt to load values from file */
   disk_index_file->modified      = false;
   disk_index_file->image_index   = 0;
   disk_index_file->image_path[0] = '\0';

   /* > If file does not exist (or some other
    *   error occurs) then this is a new record
    *   - in this case, 'modified' flag should
    *   be set to 'true' */
   if (!disk_index_file_read(disk_index_file))
      disk_index_file->modified   = true;

   return true;

error:
   disk_index_file->modified      = false;
   disk_index_file->image_index   = 0;
   disk_index_file->image_path[0] = '\0';
   disk_index_file->file_path[0]  = '\0';
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
      disk_index_file->image_index   = image_index;
      disk_index_file->modified      = true;
   }

   /* Check whether image path should be updated */
   if (image_path && *image_path)
   {
      if (!string_is_equal(disk_index_file->image_path, image_path))
      {
         strlcpy(
               disk_index_file->image_path, image_path,
               sizeof(disk_index_file->image_path));
         disk_index_file->modified   = true;
      }
   }
   else if (*disk_index_file->image_path)
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
   int _len;
   char dir[DIR_MAX_LENGTH];
   const char *file_path;
   const char *buf;
   rjsonwriter_t* writer;

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

   if (!file_path || !*file_path)
      return false;

   RARCH_LOG(
         "[Disk index file] Saving disk index file: \"%s\".\n",
         file_path);

   /* Create the record directory, if required (deferred from
    * disk_index_file_init(), which runs on every content load
    * whether or not a record will ever be written) */
   fill_pathname_basedir(dir, file_path, sizeof(dir));

   if (     !path_is_directory(dir)
         && !path_mkdir(dir))
   {
      RARCH_ERR(
            "[Disk index file] Failed to create directory for disk index file: \"%s\".\n",
            dir);
      return false;
   }

   /* Serialise the whole record in memory and write it with a
    * single filestream_write_file() call.  Opening the output
    * before the JSON exists truncates the previous record, and
    * any failure past that point - writer allocation, a write
    * error, a crash mid-save - left a zero-length file behind
    * in place of the record it destroyed.  The record is a few
    * hundred bytes; nothing here needs to stream. */
   if (!(writer = rjsonwriter_open_memory()))
   {
      RARCH_ERR("[Disk index file] Failed to create JSON writer.\n");
      return false;
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

   /* NULL means the writer hit an error while serialising */
   buf = rjsonwriter_get_memory_buffer(writer, &_len);

   if (!buf || !filestream_write_file(file_path, buf, _len))
   {
      RARCH_ERR("[Disk index file] Error writing disk index file: \"%s\".\n", file_path);
      rjsonwriter_free(writer);
      /* The record stays 'modified': a later save retries the
       * write instead of reporting success over a failure (the
       * previous code cleared the flag and returned true even
       * when the writer reported an error). */
      return false;
   }

   rjsonwriter_free(writer);

   /* Changes have been written - record
    * is no longer considered to be in a
    * 'modified' state */
   disk_index_file->modified = false;
   return true;
}
