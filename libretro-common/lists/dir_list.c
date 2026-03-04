/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (dir_list.c).
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

#if defined(_WIN32) && defined(_XBOX)
#include <xtl.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <file/file_path.h>

#include <compat/strl.h>
#include <retro_dirent.h>

#include <string/stdstring.h>
#include <retro_miscellaneous.h>

/* Helper: compute the byte-length of the stem (filename without extension).
 * Returns strlen(path) when there is no extension. */
static size_t stem_length(const char *path)
{
   const char *ext = path_get_extension(path);
   if (string_is_empty(ext))
      return strlen(path);
   /* ext points into path; step back over the '.' separator */
   return (size_t)(ext - path - 1);
}

static int qstrcmp_plain(const void *a_, const void *b_)
{
   const struct string_list_elem *a = (const struct string_list_elem*)a_;
   const struct string_list_elem *b = (const struct string_list_elem*)b_;

   return strcasecmp(a->data, b->data);
}

/* -------------------------------------------------------------------------
 * No-extension comparator helpers.
 *
 * qsort calls the comparator O(n log n) times.  Recomputing the stem
 * length inside the comparator means path_get_extension() + strlen() fire
 * twice per call per side.  Instead we build a side-table of stem lengths
 * once (O(n)) and hand a decorated array to qsort.
 * ------------------------------------------------------------------------- */

/* Decorated element used only during a no-ext sort. */
typedef struct {
   const struct string_list_elem *elem;
   size_t stem_len;
} noext_elem_t;

/* Comparator that works on noext_elem_t pointers. */
static int noext_cmp_plain(const void *a_, const void *b_)
{
   const noext_elem_t *a  = (const noext_elem_t*)a_;
   const noext_elem_t *b  = (const noext_elem_t*)b_;
   size_t min_len         = a->stem_len < b->stem_len ? a->stem_len : b->stem_len;
   int rv                 = strncasecmp(a->elem->data, b->elem->data, min_len);

   if (rv == 0 && a->stem_len != b->stem_len)
      return (a->stem_len < b->stem_len) ? -1 : 1;
   return rv;
}

static int noext_cmp_dir(const void *a_, const void *b_)
{
   const noext_elem_t *a  = (const noext_elem_t*)a_;
   const noext_elem_t *b  = (const noext_elem_t*)b_;
   int ta                 = a->elem->attr.i;
   int tb                 = b->elem->attr.i;

   /* Sort directories before files. */
   if (ta != tb)
      return tb - ta;
   return noext_cmp_plain(a_, b_);
}

static int qstrcmp_dir(const void *a_, const void *b_)
{
   const struct string_list_elem *a = (const struct string_list_elem*)a_;
   const struct string_list_elem *b = (const struct string_list_elem*)b_;
   int a_type = a->attr.i;
   int b_type = b->attr.i;

   /* Sort directories before files. */
   if (a_type != b_type)
      return b_type - a_type;
   return strcasecmp(a->data, b->data);
}

/* -------------------------------------------------------------------------
 * Shared implementation for both dir_list_sort variants.
 * Avoids duplicating the small-list early-out and the noext path.
 * ------------------------------------------------------------------------- */
static void dir_list_sort_impl(struct string_list *list,
      bool dir_first, bool ignore_ext)
{
   size_t i;
   noext_elem_t *decorated;

   if (!list || list->size < 2)
      return;

   if (!ignore_ext)
   {
      qsort(list->elems, list->size, sizeof(struct string_list_elem),
            dir_first ? qstrcmp_dir : qstrcmp_plain);
      return;
   }

   /* Build a decorated array so stem_length() is called once per element
    * (O(n)) rather than twice per comparator call (O(n log n)). */
   decorated = (noext_elem_t*)malloc(list->size * sizeof(noext_elem_t));
   if (!decorated)
   {
      /* Fallback: sort without the optimisation rather than silently skip. */
      qsort(list->elems, list->size, sizeof(struct string_list_elem),
            dir_first ? qstrcmp_dir : qstrcmp_plain);
      return;
   }

   for (i = 0; i < list->size; i++)
   {
      decorated[i].elem     = &list->elems[i];
      decorated[i].stem_len = stem_length(list->elems[i].data);
   }

   qsort(decorated, list->size, sizeof(noext_elem_t),
         dir_first ? noext_cmp_dir : noext_cmp_plain);

   /* Write the sorted order back into the original list in-place.
    * We need a temporary copy of the elements since we are reordering. */
   {
      struct string_list_elem *tmp = (struct string_list_elem*)
            malloc(list->size * sizeof(struct string_list_elem));
      if (tmp)
      {
         for (i = 0; i < list->size; i++)
            tmp[i] = *decorated[i].elem;
         memcpy(list->elems, tmp, list->size * sizeof(struct string_list_elem));
         free(tmp);
      }
      /* If tmp allocation fails the list keeps its original unsorted order,
       * which is a graceful degradation. */
   }

   free(decorated);
}

/**
 * dir_list_sort:
 * @list      : pointer to the directory listing.
 * @dir_first : move the directories in the listing to the top?
 *
 * Sorts a directory listing.
 **/
void dir_list_sort(struct string_list *list, bool dir_first)
{
   dir_list_sort_impl(list, dir_first, false);
}

/**
 * dir_list_sort_ignore_ext:
 * @list      : pointer to the directory listing.
 * @dir_first : move the directories in the listing to the top?
 *
 * Sorts a directory listing. File extensions are ignored.
 **/
void dir_list_sort_ignore_ext(struct string_list *list, bool dir_first)
{
   dir_list_sort_impl(list, dir_first, true);
}

/**
 * dir_list_free:
 * @list : pointer to the directory listing
 *
 * Frees a directory listing.
 **/
void dir_list_free(struct string_list *list)
{
   string_list_free(list);
}

bool dir_list_deinitialize(struct string_list *list)
{
   if (!list)
      return false;
   return string_list_deinitialize(list);
}

/**
 * dir_list_read:
 * @dir                : directory path.
 * @list               : the string list to add files to
 * @ext_list           : the string list of extensions to include
 * @include_dirs       : include directories as part of the finished directory listing?
 * @include_hidden     : include hidden files and directories as part of the finished directory listing?
 * @include_compressed : Only include files which match ext. Do not try to match compressed files, etc.
 * @recursive          : list directory contents recursively
 *
 * Add files within a directory to an existing string list
 *
 * @return -1 on error, 0 on success.
 **/
static int dir_list_read(const char *dir,
      struct string_list *list, struct string_list *ext_list,
      bool include_dirs, bool include_hidden,
      bool include_compressed, bool recursive)
{
   struct RDIR *entry = retro_opendir_include_hidden(dir, include_hidden);

   if (!entry)
      return -1;
   if (retro_dirent_error(entry))
   {
      retro_closedir(entry);
      return -1;
   }

   while (retro_readdir(entry))
   {
      union string_list_elem_attr attr;
      char file_path[PATH_MAX_LENGTH];
      const char *name = retro_dirent_get_name(entry);

      /* ----------------------------------------------------------------
       * Dot / hidden-file filtering.
       * All checks operate solely on `name`, before the more expensive
       * fill_pathname_join_special() call below.
       * ---------------------------------------------------------------- */
      if (name[0] == '.' || name[0] == '$')
      {
         /* Do not include hidden files and directories */
         if (!include_hidden)
            continue;

         /* char-wise comparisons to avoid string comparison */

         /* Do not include current dir  ('.')  */
         if (name[1] == '\0')
            continue;
         /* Do not include parent dir   ('..') */
         if (name[1] == '.' && name[2] == '\0')
            continue;
      }

      /* "System Volume Information" is a hidden directory on some
       * platforms that do not expose a hidden file attribute.  Reject
       * it by name before building the full path. */
      if (!include_hidden && name[0] == 'S' &&
            strcmp(name, "System Volume Information") == 0)
         continue;

      /* Build the full path only after all cheap name-based filters pass. */
      fill_pathname_join_special(file_path, dir, name, sizeof(file_path));

      if (retro_dirent_is_dir(entry, NULL))
      {
#if defined(IOS) || defined(OSX)
         if (string_ends_with(name, ".framework"))
         {
            attr.i = RARCH_PLAIN_FILE;
            if (!string_list_append(list, file_path, attr))
            {
               retro_closedir(entry);
               return -1;
            }
            continue;
         }
#endif
         if (recursive)
            dir_list_read(file_path, list, ext_list, include_dirs,
                  include_hidden, include_compressed, recursive);

         if (!include_dirs)
            continue;
         attr.i = RARCH_DIRECTORY;
      }
      else
      {
         const char *file_ext      = path_get_extension(name);
         bool is_compressed_file   = false;

         attr.i = RARCH_FILETYPE_UNSET;

         /*
          * If the file format is explicitly supported by the libretro-core,
          * we need to immediately load it and not designate it as a
          * compressed file.
          *
          * Example: .zip could be supported as an image by the core and as a
          * compressed_file.  In that case, we have to interpret it as an image.
          */
         if (string_list_find_elem_prefix(ext_list, ".", file_ext))
            attr.i = RARCH_PLAIN_FILE;
         else
         {
            is_compressed_file = path_is_compressed_file(file_path);
            if (is_compressed_file)
               attr.i = RARCH_COMPRESSED_ARCHIVE;

            if (ext_list && (!is_compressed_file || !include_compressed))
               continue;
         }
      }

      if (!string_list_append(list, file_path, attr))
      {
         retro_closedir(entry);
         return -1;
      }
   }

   retro_closedir(entry);

   return 0;
}

/**
 * dir_list_append:
 * @list               : existing list to append to.
 * @dir                : directory path.
 * @ext                : allowed extensions of file directory entries to include.
 * @include_dirs       : include directories as part of the finished directory listing?
 * @include_hidden     : include hidden files and directories as part of the finished directory listing?
 * @include_compressed : Only include files which match ext. Do not try to match compressed files, etc.
 * @recursive          : list directory contents recursively
 *
 * Create a directory listing, appending to an existing list
 *
 * @return Returns true on success, otherwise false.
 **/
bool dir_list_append(struct string_list *list,
      const char *dir,
      const char *ext, bool include_dirs,
      bool include_hidden, bool include_compressed,
      bool recursive)
{
   bool ret                         = false;
   struct string_list ext_list      = {0};
   struct string_list *ext_list_ptr = NULL;

   if (ext)
   {
      string_list_initialize(&ext_list);
      string_split_noalloc(&ext_list, ext, "|");
      ext_list_ptr = &ext_list;
   }
   ret = dir_list_read(dir, list, ext_list_ptr,
         include_dirs, include_hidden, include_compressed, recursive) != -1;
   string_list_deinitialize(&ext_list);
   return ret;
}

/**
 * dir_list_new:
 * @dir                : directory path.
 * @ext                : allowed extensions of file directory entries to include.
 * @include_dirs       : include directories as part of the finished directory listing?
 * @include_hidden     : include hidden files and directories as part of the finished directory listing?
 * @include_compressed : Only include files which match ext. Do not try to match compressed files, etc.
 * @recursive          : list directory contents recursively
 *
 * Create a directory listing.
 *
 * @return pointer to a directory listing of type 'struct string_list *' on success,
 * NULL in case of error. Has to be freed manually.
 **/
struct string_list *dir_list_new(const char *dir,
      const char *ext, bool include_dirs,
      bool include_hidden, bool include_compressed,
      bool recursive)
{
   struct string_list *list = string_list_new();

   if (!list)
      return NULL;

   if (!dir_list_append(list, dir, ext, include_dirs,
            include_hidden, include_compressed, recursive))
   {
      string_list_free(list);
      return NULL;
   }

   return list;
}

/**
 * dir_list_initialize:
 *
 * NOTE: @list must be zero-initialised before
 * calling this function, otherwise UB.
 **/
bool dir_list_initialize(struct string_list *list,
      const char *dir,
      const char *ext, bool include_dirs,
      bool include_hidden, bool include_compressed,
      bool recursive)
{
   if (list && string_list_initialize(list))
      return dir_list_append(list, dir, ext, include_dirs,
            include_hidden, include_compressed, recursive);
   return false;
}
