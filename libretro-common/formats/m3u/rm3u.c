/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rm3u.c).
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

/* m3u -- M3U playlist reader/writer.
 *
 * What it implements: line-based M3U parsing with three label styles -
 * the standard extended '#EXTINF:<runtime>,<label>', the non-standard
 * '#LABEL:<label>', and the RetroArch '<path>|<label>' suffix -
 * relative and absolute entry paths (resolved against the playlist
 * location), Windows line endings (trailing whitespace including the
 * carriage return is trimmed), and saving back out in a chosen label
 * style via rm3u_dump.
 *
 * What it does not implement: other extended directives (#EXTM3U,
 * #EXTGRP, #PLAYLIST and friends are treated as comments and dropped -
 * a load/save round trip keeps only paths and labels), URL entries
 * (paths are treated as filesystem paths), and duplicate detection.
 */

#include <retro_miscellaneous.h>

#include <string/stdstring.h>
#include <file/file_path.h>
#include <array/rbuf.h>

#include <formats/rm3u.h>

/* We parse the following types of entry label:
 * - '#LABEL:<label>' non-standard, but used by
 *   some cores
 * - '#EXTINF:<runtime>,<label>' standard extended
 *   M3U directive
 * - '<content path>|<label>' non-standard, but
 *   used by some cores
 * All other comments/directives are ignored */
#define RM3U_COMMENT            '#'
#define RM3U_NONSTD_LABEL       "#LABEL:"
#define RM3U_EXTSTD_LABEL       "#EXTINF:"
#define RM3U_EXTSTD_LABEL_TOKEN ','
#define RM3U_RETRO_LABEL_TOKEN  '|'

/* Holds all internal M3U file data */
struct rm3u
{
   char *path;
   rm3u_entry_t *entries;
};

/* File Initialisation / De-Initialisation */

/* Parses M3U data from a caller-supplied buffer.
 * A single bounded pass over the lines: no intermediate line list,
 * no per-line heap allocation - the only allocations are the
 * entries themselves. */
bool rm3u_parse(rm3u_t *m3u, const char *data)
{
   char entry_label[NAME_MAX_LENGTH];
   char entry_path[PATH_MAX_LENGTH];
   const char *line = data;

   entry_path[0]  = '\0';
   entry_label[0] = '\0';

   if (!m3u)
      return false;

   /* No data is an empty playlist, not an error */
   if (!data)
      return true;

   while (*line)
   {
      const char *line_end = strchr(line, '\n');
      size_t line_len      = line_end
            ? (size_t)(line_end - line) : strlen(line);

      if (line_len == 0)
      {
         line = line_end ? line_end + 1 : line + line_len;
         continue;
      }

      /* Determine line 'type' */

      /* > '#LABEL:' */
      if (    line_len > STRLEN_CONST(RM3U_NONSTD_LABEL)
          && string_starts_with_size(line, RM3U_NONSTD_LABEL,
               STRLEN_CONST(RM3U_NONSTD_LABEL)))
      {
         /* Label is the string to the right of '#LABEL:' */
         const char *seg = line + STRLEN_CONST(RM3U_NONSTD_LABEL);
         size_t seg_len  = line_len - STRLEN_CONST(RM3U_NONSTD_LABEL);
         size_t n        = (seg_len < sizeof(entry_label) - 1)
               ? seg_len : sizeof(entry_label) - 1;
         memcpy(entry_label, seg, n);
         entry_label[n]  = '\0';
         string_trim_whitespace_right(entry_label);
         string_trim_whitespace_left(entry_label);
      }
      /* > '#EXTINF:' */
      else if (    line_len > STRLEN_CONST(RM3U_EXTSTD_LABEL)
               && string_starts_with_size(line, RM3U_EXTSTD_LABEL,
                    STRLEN_CONST(RM3U_EXTSTD_LABEL)))
      {
         /* Label is the string to the right of the first comma */
         const char *seg = line + STRLEN_CONST(RM3U_EXTSTD_LABEL);
         size_t seg_len  = line_len - STRLEN_CONST(RM3U_EXTSTD_LABEL);
         const char *label_ptr = (const char*)memchr(seg,
               RM3U_EXTSTD_LABEL_TOKEN, seg_len);

         if (label_ptr && (size_t)(label_ptr + 1 - seg) < seg_len)
         {
            size_t label_len = seg_len - (size_t)(label_ptr + 1 - seg);
            size_t n         = (label_len < sizeof(entry_label) - 1)
                  ? label_len : sizeof(entry_label) - 1;
            memcpy(entry_label, label_ptr + 1, n);
            entry_label[n]   = '\0';
            string_trim_whitespace_right(entry_label);
            string_trim_whitespace_left(entry_label);
         }
      }
      /* > Ignore other comments/directives */
      else if (line[0] == RM3U_COMMENT)
         ;
      /* > An actual 'content' line */
      else
      {
         /* This is normally a file name/path, but may
          * have the format <content path>|<label> */
         const char *token_ptr = (const char*)memchr(line,
               RM3U_RETRO_LABEL_TOKEN, line_len);

         if (token_ptr)
         {
            size_t path_len = (size_t)(token_ptr - line);
            size_t n        = (path_len < sizeof(entry_path) - 1)
                  ? path_len : sizeof(entry_path) - 1;

            /* Get entry_path segment */
            memcpy(entry_path, line, n);
            entry_path[n]   = '\0';
            string_trim_whitespace_right(entry_path);
            string_trim_whitespace_left(entry_path);

            /* Get entry_label segment */
            if ((size_t)(token_ptr + 1 - line) < line_len)
            {
               size_t label_len = line_len
                     - (size_t)(token_ptr + 1 - line);
               n                = (label_len < sizeof(entry_label) - 1)
                     ? label_len : sizeof(entry_label) - 1;
               memcpy(entry_label, token_ptr + 1, n);
               entry_label[n]   = '\0';
               string_trim_whitespace_right(entry_label);
               string_trim_whitespace_left(entry_label);
            }
         }
         else
         {
            /* Just a normal file name/path */
            size_t n      = (line_len < sizeof(entry_path) - 1)
                  ? line_len : sizeof(entry_path) - 1;
            memcpy(entry_path, line, n);
            entry_path[n] = '\0';
            string_trim_whitespace_right(entry_path);
            string_trim_whitespace_left(entry_path);
         }

         /* Add entry to file
          * > Note: The only way that rm3u_add_entry()
          *   can fail here is if we run out of memory.
          *   This is a critical error, and m3u must
          *   be considered invalid in this case */
         if (*entry_path
             && !rm3u_add_entry(m3u, entry_path, entry_label))
            return false;

         /* Reset entry_path/entry_label */
         entry_path[0]  = '\0';
         entry_label[0] = '\0';
      }

      line = line_end ? line_end + 1 : line + line_len;
   }

   return true;
}

/* Creates an empty M3U handle for @path.  No file I/O occurs:
 * feed existing contents through rm3u_parse(), render for writing
 * with rm3u_dump() - or use the filestream adapters in
 * formats/rm3u_stream.h, which reproduce the old load/save
 * behaviour.
 * - Returned rm3u_t object must be free'd using rm3u_free()
 * - Returns NULL in the event of an error */
rm3u_t *rm3u_init(const char *path)
{
   rm3u_t *m3u = NULL;
   char m3u_path[PATH_MAX_LENGTH];

   /* Sanity check */
   if (!path || !*path)
      return NULL;

   /* Get 'real' file path */
   strlcpy(m3u_path, path, sizeof(m3u_path));
   path_resolve_realpath(m3u_path, sizeof(m3u_path), false);

   if (!*m3u_path)
      return NULL;

   /* Create rm3u_t object */
   if (!(m3u = (rm3u_t*)malloc(sizeof(*m3u))))
      return NULL;

   /* Initialise members */
   m3u->path    = NULL;
   m3u->entries = NULL;

   /* Copy file path */
   if (!(m3u->path = strdup(m3u_path)))
   {
      free(m3u);
      return NULL;
   }

   return m3u;
}

/* Frees specified M3U file entry */
static void rm3u_free_entry(rm3u_entry_t *entry)
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
void rm3u_free(rm3u_t *m3u)
{
   size_t i;

   if (!m3u)
      return;

   if (m3u->path)
      free(m3u->path);

   m3u->path = NULL;

   /* Free entries */
   if (m3u->entries)
   {
      for (i = 0; i < RBUF_LEN(m3u->entries); i++)
      {
         rm3u_entry_t *entry = &m3u->entries[i];
         rm3u_free_entry(entry);
      }

      RBUF_FREE(m3u->entries);
   }

   free(m3u);
}

/* Getters */

/* Returns M3U file path */
char *rm3u_get_path(rm3u_t *m3u)
{
   if (!m3u)
      return NULL;

   return m3u->path;
}

/* Returns number of entries in M3U file */
size_t rm3u_get_size(rm3u_t *m3u)
{
   if (!m3u)
      return 0;

   return RBUF_LEN(m3u->entries);
}

/* Fetches specified M3U file entry
 * - Returns false if 'idx' is invalid, or internal
 *   entry is NULL */
bool rm3u_get_entry(
      rm3u_t *m3u, size_t idx, rm3u_entry_t **entry)
{
   if (!m3u ||
       !entry ||
       (idx >= RBUF_LEN(m3u->entries)))
      return false;

   *entry = &m3u->entries[idx];

   if (!*entry)
      return false;

   return true;
}

/* Setters */

/* Adds specified entry to the M3U file
 * - Returns false if path is invalid, or
 *   memory could not be allocated for the
 *   entry */
bool rm3u_add_entry(
      rm3u_t *m3u, const char *path, const char *label)
{
   rm3u_entry_t *entry = NULL;
   size_t num_entries;
   char full_path[PATH_MAX_LENGTH];

   full_path[0] = '\0';

   if (!m3u || (!path || !*path))
      return false;

   /* Get current number of file entries */
   num_entries = RBUF_LEN(m3u->entries);

   /* Attempt to allocate memory for new entry */
   if (!RBUF_TRYFIT(m3u->entries, num_entries + 1))
      return false;

   /* Allocation successful - increment array size */
   RBUF_RESIZE(m3u->entries, num_entries + 1);

   /* Fetch entry at end of list, and zero-initialise
    * members */
   entry = &m3u->entries[num_entries];
   memset(entry, 0, sizeof(*entry));

   /* Copy path and label */
   entry->path = strdup(path);

   if (label && *label)
      entry->label = strdup(label);

   /* Populate 'full_path' field */
   if (path_is_absolute(path))
   {
      strlcpy(full_path, path, sizeof(full_path));
      path_resolve_realpath(full_path, sizeof(full_path), false);
   }
   else
      fill_pathname_resolve_relative(
            full_path, m3u->path, path,
            sizeof(full_path));

   /* Handle unforeseen errors... */
   if (!*full_path)
   {
      rm3u_free_entry(entry);
      return false;
   }

   entry->full_path = strdup(full_path);

   return true;
}

/* Removes all entries in M3U file */
void rm3u_clear(rm3u_t *m3u)
{
   size_t i;

   if (!m3u)
      return;

   if (m3u->entries)
   {
      for (i = 0; i < RBUF_LEN(m3u->entries); i++)
      {
         rm3u_entry_t *entry = &m3u->entries[i];
         rm3u_free_entry(entry);
      }

      RBUF_FREE(m3u->entries);
   }
}

/* Saving */

/* Renders the M3U file contents as a single heap string
 * (the exact bytes rm3u_save wrote line-by-line before it), so a
 * caller writes the whole file with one I/O operation instead of
 * one or two per entry.  Returns NULL if there is nothing to
 * write or on allocation failure; the caller frees the result.
 * When @out_len is non-NULL it receives the string length. */
char *rm3u_dump(rm3u_t *m3u,
      enum rm3u_label_type label_type, size_t *out_len)
{
   size_t i;
   char base_dir[DIR_MAX_LENGTH];
   char scratch[PATH_MAX_LENGTH + NAME_MAX_LENGTH + 16];
   char *out    = NULL;
   size_t _len  = 0;
   size_t cap   = 0;

   if (!m3u || !m3u->entries)
      return NULL;

   /* This should never happen */
   if (!m3u->path || !*m3u->path)
      return NULL;

   /* Get M3U file base directory */
   if (find_last_slash(m3u->path))
      fill_pathname_basedir(base_dir, m3u->path, sizeof(base_dir));
   else
      base_dir[0]   = '\0';

   /* Loop over entries */
   for (i = 0; i < RBUF_LEN(m3u->entries); i++)
   {
      rm3u_entry_t *entry = &m3u->entries[i];
      char entry_path[PATH_MAX_LENGTH];
      size_t line_len;

      entry_path[0] = '\0';

      if (!entry || (!entry->full_path || !*entry->full_path))
         continue;

      /* When writing M3U files, entry paths are
       * always relative */
      if (!*base_dir)
         strlcpy(
               entry_path, entry->full_path,
               sizeof(entry_path));
      else
         path_relative_to(
               entry_path, entry->full_path, base_dir,
               sizeof(entry_path));

      if (!*entry_path)
         continue;

      /* Format this entry - the format strings are the ones the
       * per-line writer used, unchanged.  The scratch buffer is
       * sized for the largest possible line pair, so snprintf
       * cannot truncate. */
      if (entry->label && *entry->label)
      {
         switch (label_type)
         {
            case RM3U_LABEL_NONSTD:
               line_len = (size_t)snprintf(
                     scratch, sizeof(scratch), "%s%s\n%s\n",
                     RM3U_NONSTD_LABEL, entry->label,
                     entry_path);
               break;
            case RM3U_LABEL_EXTSTD:
               line_len = (size_t)snprintf(
                     scratch, sizeof(scratch), "%s%c%s\n%s\n",
                     RM3U_EXTSTD_LABEL, RM3U_EXTSTD_LABEL_TOKEN, entry->label,
                     entry_path);
               break;
            case RM3U_LABEL_RETRO:
               line_len = (size_t)snprintf(
                     scratch, sizeof(scratch), "%s%c%s\n",
                     entry_path, RM3U_RETRO_LABEL_TOKEN, entry->label);
               break;
            case RM3U_LABEL_NONE:
            default:
               line_len = (size_t)snprintf(
                     scratch, sizeof(scratch), "%s\n", entry_path);
               break;
         }
      }
      /* No label - just write entry path */
      else
         line_len = (size_t)snprintf(
               scratch, sizeof(scratch), "%s\n", entry_path);

      /* Append to the output, growing by doubling */
      if (_len + line_len + 1 > cap)
      {
         char *tmp;
         size_t new_cap = (cap == 0) ? 512 : cap;
         while (_len + line_len + 1 > new_cap)
            new_cap <<= 1;
         if (!(tmp = (char*)realloc(out, new_cap)))
         {
            free(out);
            return NULL;
         }
         out = tmp;
         cap = new_cap;
      }
      memcpy(out + _len, scratch, line_len);
      _len      += line_len;
      out[_len]  = '\0';
   }

   if (!out)
      return NULL;   /* every entry filtered out: nothing to write */

   if (out_len)
      *out_len = _len;
   return out;
}

/* Utilities */

/* Internal qsort function */
static int rm3u_qsort_func(
      const rm3u_entry_t *a, const rm3u_entry_t *b)
{
   if (!a || !b)
      return 0;

   if ((!a->full_path || !*a->full_path) || (!b->full_path || !*b->full_path))
      return 0;

   return strcasecmp(a->full_path, b->full_path);
}

/* Sorts M3U file entries in alphabetical order */
void rm3u_qsort(rm3u_t *m3u)
{
   size_t num_entries;

   if (!m3u)
      return;

   num_entries = RBUF_LEN(m3u->entries);

   if (num_entries < 2)
      return;

   qsort(
         m3u->entries, num_entries,
         sizeof(rm3u_entry_t),
         (int (*)(const void *, const void *))rm3u_qsort_func);
}

/* Returns true if specified path corresponds
 * to an M3U file (simple convenience function) */
/* True when @path names an M3U file by extension.  A pure string
 * check: whether such a file exists on any filesystem is the
 * caller's question (rm3u_is_m3u_filestream() in the adapter asks
 * it with a single stat). */
bool rm3u_is_m3u(const char *path)
{
   const char *file_ext = NULL;
   if (!path || !*path)
      return false;
   file_ext = path_get_extension(path);
   if (!file_ext || !*file_ext)
      return false;
   return string_is_equal_noncase(file_ext, RM3U_EXT);
}
