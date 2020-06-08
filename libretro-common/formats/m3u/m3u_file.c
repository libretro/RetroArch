/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (m3u_file.c).
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

#include <retro_miscellaneous.h>

#include <string/stdstring.h>
#include <lists/string_list.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

#include <formats/m3u_file.h>

/* We parse the following types of entry label:
 * - '#LABEL:<label>' non-standard, but used by
 *   some cores
 * - '#EXTINF:<runtime>,<label>' standard extended
 *   M3U directive
 * - '<content path>|<label>' non-standard, but
 *   used by some cores
 * All other comments/directives are ignored */
#define M3U_FILE_COMMENT            '#'
#define M3U_FILE_NONSTD_LABEL       "#LABEL:"
#define M3U_FILE_EXTSTD_LABEL       "#EXTINF:"
#define M3U_FILE_EXTSTD_LABEL_TOKEN ','
#define M3U_FILE_RETRO_LABEL_TOKEN  '|'

/* Holds all internal M3U file data
 * > Note the awkward name: 'content_m3u_file'
 *   If we used just 'm3u_file' here, it would
 *   lead to conflicts elsewhere... */
struct content_m3u_file
{
   char *path;
   size_t size;
   size_t capacity;
   m3u_file_entry_t *entries;
};

/* File Initialisation / De-Initialisation */

/* Reads M3U file contents from disk
 * - Does nothing if file does not exist 
 * - Returns false in the event of an error */
static bool m3u_file_load(m3u_file_t *m3u_file)
{
   const char *file_ext      = NULL;
   int64_t file_len          = 0;
   uint8_t *file_buf         = NULL;
   struct string_list *lines = NULL;
   size_t i;
   char entry_path[PATH_MAX_LENGTH];
   char entry_label[PATH_MAX_LENGTH];

   entry_path[0]  = '\0';
   entry_label[0] = '\0';

   if (!m3u_file)
      return false;

   /* Check whether file exists
    * > If path is empty, then an error
    *   has occurred... */
   if (string_is_empty(m3u_file->path))
      return false;

   /* > File must have the correct extension */
   file_ext = path_get_extension(m3u_file->path);

   if (string_is_empty(file_ext))
      return false;

   if (!string_is_equal_noncase(file_ext, M3U_FILE_EXT))
      return false;

   /* > If file does not exist, no action
    *   is required */
   if (!path_is_valid(m3u_file->path))
      return true;

   /* Read file from disk */
   if (filestream_read_file(m3u_file->path, (void**)&file_buf, &file_len) >= 0)
   {
      /* Split file into lines */
      if (file_len > 0)
         lines = string_split((const char*)file_buf, "\n");

      /* File buffer no longer required */
      if (file_buf)
         free(file_buf);
   }
   else
   {
      /* File IO error... */
      if (file_buf)
         free(file_buf);
      return false;
   }

   /* If file was empty, no action is required */
   if (!lines)
      return true;

   /* Parse lines of file */
   for (i = 0; i < lines->size; i++)
   {
      size_t m3u_size;
      const char *line = lines->elems[i].data;

      if (string_is_empty(line))
         continue;

      /* Determine line 'type' */
      m3u_size = strlen(M3U_FILE_NONSTD_LABEL);

      /* > '#LABEL:' */
      if (!strncmp(
            line, M3U_FILE_NONSTD_LABEL,
            m3u_size))
      {
         /* Label is the string to the right
          * of '#LABEL:' */
         const char *label = line + m3u_size;

         if (!string_is_empty(label))
         {
            strlcpy(
                  entry_label, line + m3u_size,
                  sizeof(entry_label));
            string_trim_whitespace(entry_label);
         }
      }
      /* > '#EXTINF:' */
      else if (!strncmp(
            line, M3U_FILE_EXTSTD_LABEL,
            strlen(M3U_FILE_EXTSTD_LABEL)))
      {
         /* Label is the string to the right
          * of the first comma */
         const char* label_ptr = strchr(
               line + strlen(M3U_FILE_EXTSTD_LABEL),
               M3U_FILE_EXTSTD_LABEL_TOKEN);

         if (!string_is_empty(label_ptr))
         {
            label_ptr++;
            if (!string_is_empty(label_ptr))
            {
               strlcpy(entry_label, label_ptr, sizeof(entry_label));
               string_trim_whitespace(entry_label);
            }
         }
      }
      /* > Ignore other comments/directives */
      else if (line[0] == M3U_FILE_COMMENT)
         continue;
      /* > An actual 'content' line */
      else
      {
         /* This is normally a file name/path, but may
          * have the format <content path>|<label> */
         const char *token_ptr = strchr(line, M3U_FILE_RETRO_LABEL_TOKEN);

         if (token_ptr)
         {
            size_t len = (size_t)(1 + token_ptr - line);

            /* Get entry_path segment */
            if (len > 0)
            {
               memset(entry_path, 0, sizeof(entry_path));
               strlcpy(
                     entry_path, line,
                     ((len < PATH_MAX_LENGTH ?
                           len : PATH_MAX_LENGTH) * sizeof(char)));
               string_trim_whitespace(entry_path);
            }

            /* Get entry_label segment */
            token_ptr++;
            if (*token_ptr != '\0')
            {
               strlcpy(entry_label, token_ptr, sizeof(entry_label));
               string_trim_whitespace(entry_label);
            }
         }
         else
         {
            /* Just a normal file name/path */
            strlcpy(entry_path, line, sizeof(entry_path));
            string_trim_whitespace(entry_path);
         }

         /* Add entry to file
          * > Ignore errors here - invalid entries
          *   will just be omitted */
         m3u_file_add_entry(m3u_file, entry_path, entry_label);

         /* Reset entry_path/entry_label */
         entry_path[0]  = '\0';
         entry_label[0] = '\0';
      }
   }

   /* Clean up */
   if (lines)
   {
      string_list_free(lines);
      lines = NULL;
   }

   return true;
}

/* Creates and initialises an M3U file
 * - If 'path' refers to an existing file,
 *   contents is parsed
 * - If path does not exist, an empty M3U file
 *   is created
 * - Returned m3u_file_t object must be free'd using
 *   m3u_file_free()
 * - Returns NULL in the event of an error */
m3u_file_t *m3u_file_init(const char *path, size_t size)
{
   m3u_file_entry_t *entries = NULL;
   m3u_file_t *m3u_file      = NULL;
   char m3u_path[PATH_MAX_LENGTH];

   m3u_path[0] = '\0';

   /* Sanity check */
   if (string_is_empty(path) || (size < 1))
      return NULL;

   /* Get 'real' file path */
   strlcpy(m3u_path, path, sizeof(m3u_path));
   path_resolve_realpath(m3u_path, sizeof(m3u_path), false);

   if (string_is_empty(m3u_path))
      return NULL;

   /* Create m3u_file_t object */
   m3u_file = (m3u_file_t*)calloc(1, sizeof(*m3u_file));

   if (!m3u_file)
      return NULL;

   /* Create m3u_file_entry_t array */
   entries = (m3u_file_entry_t*)calloc(size, sizeof(*entries));

   if (!entries)
   {
      free(m3u_file);
      return NULL;
   }

   /* Copy file path */
   m3u_file->path = strdup(m3u_path);

   /* Set remaining values */
   m3u_file->size     = 0;
   m3u_file->capacity = size;
   m3u_file->entries  = entries;

   /* Read existing file contents from
    * disk, if required */
   if (!m3u_file_load(m3u_file))
   {
      m3u_file_free(m3u_file);
      return NULL;
   }

   return m3u_file;
}

/* Frees specified M3U file entry */
static void m3u_file_free_entry(m3u_file_entry_t *entry)
{
   if (!entry)
      return;

   if (entry->path)
      free(entry->path);

   if (entry->full_path)
      free(entry->full_path);

   if (entry->label)
      free(entry->label);

   entry->path      = NULL;
   entry->full_path = NULL;
   entry->label     = NULL;
}

/* Frees specified M3U file */
void m3u_file_free(m3u_file_t *m3u_file)
{
   size_t i;

   if (!m3u_file)
      return;

   if (m3u_file->path)
      free(m3u_file->path);

   m3u_file->path = NULL;

   /* Free entries */
   if (m3u_file->entries)
   {
      for (i = 0; i < m3u_file->size; i++)
      {
         m3u_file_entry_t *entry = &m3u_file->entries[i];
         m3u_file_free_entry(entry);
      }

      free(m3u_file->entries);
   }
   m3u_file->entries = NULL;

   free(m3u_file);
}

/* Getters */

/* Returns M3U file path */
char *m3u_file_get_path(m3u_file_t *m3u_file)
{
   if (!m3u_file)
      return NULL;

   return m3u_file->path;
}

/* Returns number of entries in M3U file */
size_t m3u_file_get_size(m3u_file_t *m3u_file)
{
   if (!m3u_file)
      return 0;

   return m3u_file->size;
}

/* Returns maximum number of entries permitted
 * in M3U file */
size_t m3u_file_get_capacity(m3u_file_t *m3u_file)
{
   if (!m3u_file)
      return 0;

   return m3u_file->capacity;
}

/* Fetches specified M3U file entry
 * - Returns false if 'idx' is invalid, or internal
 *   entry is NULL */
bool m3u_file_get_entry(
      m3u_file_t *m3u_file, size_t idx, m3u_file_entry_t **entry)
{
   if (!m3u_file ||
       !entry ||
       (idx >= m3u_file->size) ||
       !m3u_file->entries)
      return false;

   *entry = &m3u_file->entries[idx];

   if (!*entry)
      return false;

   return true;
}

/* Setters */

/* Adds specified entry to the M3U file
 * - Returns false if path is invalid, or M3U
 *   file capacity is exceeded */
bool m3u_file_add_entry(
      m3u_file_t *m3u_file, const char *path, const char *label)
{
   m3u_file_entry_t *entry = NULL;
   char full_path[PATH_MAX_LENGTH];

   full_path[0] = '\0';

   if (!m3u_file ||
       !m3u_file->entries ||
       (m3u_file->size >= m3u_file->capacity) ||
       string_is_empty(path))
      return false;

   /* Get new entry at end of list */
   entry = &m3u_file->entries[m3u_file->size];

   if (!entry)
      return false;

   /* Ensure entry is free'd */
   m3u_file_free_entry(entry);

   /* Copy path and label */
   entry->path = strdup(path);

   if (!string_is_empty(label))
      entry->label = strdup(label);

   /* Populate 'full_path' field */
   if (path_is_absolute(path))
   {
      strlcpy(full_path, path, sizeof(full_path));
      path_resolve_realpath(full_path, sizeof(full_path), false);
   }
   else
      fill_pathname_resolve_relative(
            full_path, m3u_file->path, path,
            sizeof(full_path));

   /* Handle unforeseen errors... */
   if (string_is_empty(full_path))
   {
      m3u_file_free_entry(entry);
      return false;
   }

   entry->full_path = strdup(full_path);

   /* Increment size counter */
   m3u_file->size++;

   return true;
}

/* Removes all entries in M3U file */
void m3u_file_clear(m3u_file_t *m3u_file)
{
   size_t i;

   if (!m3u_file)
      return;

   if (m3u_file->entries)
   {
      for (i = 0; i < m3u_file->size; i++)
      {
         m3u_file_entry_t *entry = &m3u_file->entries[i];
         m3u_file_free_entry(entry);
      }
   }

   m3u_file->size = 0;
}

/* Saving */

/* Saves M3U file to disk
 * - Setting 'label_type' to M3U_FILE_LABEL_NONE
 *   just outputs entry paths - this the most
 *   common format supported by most cores
 * - Returns false in the event of an error */
bool m3u_file_save(
      m3u_file_t *m3u_file, enum m3u_file_label_type label_type)
{
   RFILE *file = NULL;
   size_t i;
   char base_dir[PATH_MAX_LENGTH];

   base_dir[0] = '\0';

   if (!m3u_file || !m3u_file->entries)
      return false;

   /* This should never happen */
   if (string_is_empty(m3u_file->path))
      return false;

   /* Get M3U file base directory */
   if (find_last_slash(m3u_file->path))
   {
      strlcpy(base_dir, m3u_file->path, sizeof(base_dir));
      path_basedir(base_dir);
   }

   /* Open file for writing */
   file = filestream_open(
         m3u_file->path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
      return false;

   /* Loop over entries */
   for (i = 0; i < m3u_file->size; i++)
   {
      m3u_file_entry_t *entry = &m3u_file->entries[i];
      char entry_path[PATH_MAX_LENGTH];

      entry_path[0] = '\0';

      if (!entry || string_is_empty(entry->full_path))
         continue;

      /* When writing M3U files, entry paths are
       * always relative */
      if (string_is_empty(base_dir))
         strlcpy(
               entry_path, entry->full_path,
               sizeof(entry_path));
      else
         path_relative_to(
               entry_path, entry->full_path, base_dir,
               sizeof(entry_path));

      if (string_is_empty(entry_path))
         continue;

      /* Check if we need to write a label */
      if (!string_is_empty(entry->label))
      {
         switch (label_type)
         {
            case M3U_FILE_LABEL_NONSTD:
               filestream_printf(
                     file, "%s%s\n%s\n",
                     M3U_FILE_NONSTD_LABEL, entry->label,
                     entry_path);
               break;
            case M3U_FILE_LABEL_EXTSTD:
               filestream_printf(
                     file, "%s%c%s\n%s\n",
                     M3U_FILE_EXTSTD_LABEL, M3U_FILE_EXTSTD_LABEL_TOKEN, entry->label,
                     entry_path);
               break;
            case M3U_FILE_LABEL_RETRO:
               filestream_printf(
                     file, "%s%c%s\n",
                     entry_path, M3U_FILE_RETRO_LABEL_TOKEN, entry->label);
               break;
            case M3U_FILE_LABEL_NONE:
            default:
               filestream_printf(
                     file, "%s\n", entry_path);
               break;
         }
      }
      /* No label - just write entry path */
      else
         filestream_printf(
               file, "%s\n", entry_path);
   }

   /* Close file */
   filestream_close(file);

   return true;
}

/* Utilities */

/* Internal qsort function */
static int m3u_file_qsort_func(
      const m3u_file_entry_t *a, const m3u_file_entry_t *b)
{
   if (!a || !b)
      return 0;

   if (string_is_empty(a->full_path) || string_is_empty(b->full_path))
      return 0;

   return strcasecmp(a->full_path, b->full_path);
}

/* Sorts M3U file entries in alphabetical order */
void m3u_file_qsort(m3u_file_t *m3u_file)
{
   if (!m3u_file ||
       !m3u_file->entries ||
       (m3u_file->size < 2))
      return;

   qsort(
         m3u_file->entries, m3u_file->size,
         sizeof(m3u_file_entry_t),
         (int (*)(const void *, const void *))m3u_file_qsort_func);
}

/* Returns true if specified path corresponds
 * to an M3U file (simple convenience function) */
bool m3u_file_is_m3u(const char *path)
{
   const char *file_ext = NULL;
   int32_t file_size;

   if (string_is_empty(path))
      return false;

   /* Check file extension */
   file_ext = path_get_extension(path);

   if (string_is_empty(file_ext))
      return false;

   if (!string_is_equal_noncase(file_ext, M3U_FILE_EXT))
      return false;

   /* Ensure file exists */
   if (!path_is_valid(path))
      return false;

   /* Ensure we have non-zero file size */
   file_size = path_get_size(path);

   if (file_size <= 0)
      return false;

   return true;
}
