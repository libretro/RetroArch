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
#include <ctype.h>

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

#include <retro_miscellaneous.h>
#ifdef __MACH__
#include <TargetConditionals.h>
#endif

static int qstrcmp_plain(const void *a_, const void *b_)
{
   const struct string_list_elem *a = (const struct string_list_elem*)a_;
   const struct string_list_elem *b = (const struct string_list_elem*)b_;
   const char *s1 = a->data;
   const char *s2 = b->data;

   for (;;)
   {
      int c1 = tolower((unsigned char)*s1);
      int c2 = tolower((unsigned char)*s2);
      if (c1 != c2)
         return c1 - c2;
      if (c1 == '\0')
         return 0;
      s1++;
      s2++;
   }
}

/**
 * find_ext_dot:
 * @path : file path
 *
 * Finds the '.' that begins the file extension, considering only
 * dots after the last directory separator. This avoids treating
 * dots in directory names (e.g. ".config") as extension separators.
 *
 * @return pointer to the extension '.', or to the trailing '\0'
 * if no extension is found.
 **/
static const char *find_ext_dot(const char *path)
{
   const char *last_slash = strrchr(path, '/');
#ifdef _WIN32
   {
      const char *last_bslash = strrchr(path, '\\');
      if (last_bslash && (!last_slash || last_bslash > last_slash))
         last_slash = last_bslash;
   }
#endif
   {
      const char *start = last_slash ? last_slash + 1 : path;
      const char *dot   = strrchr(start, '.');
      return dot ? dot : path + strlen(path);
   }
}

static int qstrcmp_plain_noext(const void *a_, const void *b_)
{
   const struct string_list_elem *a = (const struct string_list_elem*)a_;
   const struct string_list_elem *b = (const struct string_list_elem*)b_;
   const char *ea = find_ext_dot(a->data);
   const char *eb = find_ext_dot(b->data);
   size_t la      = (size_t)(ea - a->data);
   size_t lb      = (size_t)(eb - b->data);
   size_t len     = la < lb ? la : lb;
   int rv         = strncasecmp(a->data, b->data, len);
   if (rv != 0)
      return rv;
   if (la != lb)
      return (la < lb) ? -1 : 1;
   return 0;
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

static int qstrcmp_dir_noext(const void *a_, const void *b_)
{
   const struct string_list_elem *a = (const struct string_list_elem*)a_;
   const struct string_list_elem *b = (const struct string_list_elem*)b_;
   int a_type = a->attr.i;
   int b_type = b->attr.i;

   /* Sort directories before files. */
   if (a_type != b_type)
      return b_type - a_type;
   return qstrcmp_plain_noext(a, b);
}

/**
 * dir_list_sort_cmp:
 * @dir_first  : sort directories before files?
 * @ignore_ext : ignore file extensions when comparing?
 *
 * The exact comparator dir_list_sort() / dir_list_sort_ignore_ext()
 * order a listing with, for callers that need to apply the same
 * ordering through a different sorting strategy (e.g. an
 * incremental sort under a time budget).  Operates on
 * struct string_list_elem.
 **/
dir_list_sort_cmp_t dir_list_sort_cmp(bool dir_first, bool ignore_ext)
{
   if (ignore_ext)
      return dir_first ? qstrcmp_dir_noext : qstrcmp_plain_noext;
   return dir_first ? qstrcmp_dir : qstrcmp_plain;
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
   if (list)
      qsort(list->elems, list->size, sizeof(struct string_list_elem),
            dir_list_sort_cmp(dir_first, false));
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
   if (list)
      qsort(list->elems, list->size, sizeof(struct string_list_elem),
            dir_list_sort_cmp(dir_first, true));
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
struct dir_list_ctx
{
   struct string_list *list;
   struct string_list *ext_list;
   char               *path;  /* single buffer, used as a prefix stack */
   bool                include_dirs;
   bool                include_hidden;
   bool                include_compressed;
   bool                recursive;
};

/* @dir_len : length of the current directory prefix already sitting
 * in ctx->path. The buffer is shared across recursion levels: each
 * level only ever writes at [dir_len, ...) and restores the NUL on
 * the way back out, so no level can clobber its parent's prefix. */
static int dir_list_read_ctx(size_t dir_len, struct dir_list_ctx *ctx)
{
   char *path         = ctx->path;
   struct RDIR *entry = retro_opendir_include_hidden(path,
         ctx->include_hidden);

   if (!entry)
      return -1;
   if (retro_dirent_error(entry))
   {
      retro_closedir(entry);
      return -1;
   }

   while (retro_readdir(entry))
   {
      size_t _len;
      union string_list_elem_attr attr;
      const char *name = retro_dirent_get_name(entry);

      if (name[0] == '.' || name[0] == '$')
      {
         /* Do not include hidden files and directories */
         if (!ctx->include_hidden)
            continue;
         /* char-wise comparisons to avoid string comparison */
         /* Do not include current dir */
         if (name[1] == '\0')
            continue;
         /* Do not include parent dir */
         if (name[1] == '.' && name[2] == '\0')
            continue;
      }

      /* Append @name to the prefix instead of rebuilding the whole
       * path: the prefix is already in place from the parent level. */
      _len = dir_len;
      if (_len && path[_len - 1] != '/' && path[_len - 1] != '\\')
         path[_len++] = '/';
      _len += strlcpy(path + _len, name, PATH_MAX_LENGTH - _len);

      if (retro_dirent_is_dir(entry, NULL))
      {
         /* Exclude this frequent hidden dir on platforms which can not handle hidden attribute */
         if (!ctx->include_hidden && strcmp(name, "System Volume Information") == 0)
            continue;

#if TARGET_OS_IPHONE || TARGET_OS_OSX
         {
            size_t name_len = strlen(name);
            if (name_len >= 10
                  && !memcmp(name + name_len - 10, ".framework", 10))
            {
               attr.i = RARCH_PLAIN_FILE;
               if (!string_list_append(ctx->list, path, attr))
               {
                  retro_closedir(entry);
                  return -1;
               }
               continue;
            }
         }
#endif
         if (ctx->recursive)
         {
            dir_list_read_ctx(_len, ctx);
            path[_len] = '\0';
         }

         if (!ctx->include_dirs)
            continue;
         attr.i = RARCH_DIRECTORY;
      }
      else
      {
         const char *file_ext    = path_get_extension(name);

         attr.i                  = RARCH_FILETYPE_UNSET;

         if (string_list_find_elem_prefix(ctx->ext_list, ".", file_ext))
            attr.i            = RARCH_PLAIN_FILE;
         else
         {
            bool is_compressed_file;
            if ((is_compressed_file = path_is_compressed_file(path)))
               attr.i               = RARCH_COMPRESSED_ARCHIVE;

            if (ctx->ext_list &&
                  (!is_compressed_file || !ctx->include_compressed))
               continue;
         }
      }

      if (!string_list_append(ctx->list, path, attr))
      {
         retro_closedir(entry);
         return -1;
      }
   }

   retro_closedir(entry);

   return 0;
}

/* Resumable directory walk ------------------------------------------
 *
 * dir_list_read_ctx() above is a depth-first recursion over a shared
 * path-prefix buffer.  The iterator below is the same walk with the
 * recursion made explicit - a frame per open directory - so it can
 * stop between any two entries and resume later.  Every filtering
 * decision is a transliteration of the recursive body, and the
 * equivalence is pinned by the parity lane of
 * samples/tasks/database/scan_begin_budget_test.c, which diffs the
 * iterator's output against dir_list_new() on the same tree.
 *
 * Two behaviours of the recursion that read like accidents are
 * contract here and preserved deliberately:
 *  - the return value of the recursive call is ignored, so an
 *    unreadable subdirectory is skipped silently while the walk
 *    continues, and
 *  - a directory itself is appended (under include_dirs) after its
 *    contents, i.e. post-order, because the append sits after the
 *    recursive call. */

struct dir_list_iter_frame
{
   struct RDIR *entry;    /* open directory handle for this level    */
   size_t       dir_len;  /* prefix length children append at        */
   size_t       pend_len; /* path length of the dir itself, for the  *
                           * post-order append on pop (root: unused) */
   bool         pend_append; /* append this dir on pop?              */
};

struct dir_list_iter
{
   struct string_list *list;             /* borrowed                 */
   struct string_list  ext_list;         /* owned storage...         */
   struct string_list *ext_list_ptr;     /* ...NULL when no filter   */
   char               *path;             /* shared prefix buffer     */
   struct dir_list_iter_frame *stack;
   size_t              depth;            /* frames in use            */
   size_t              cap;              /* frames allocated         */
   bool                include_dirs;
   bool                include_hidden;
   bool                include_compressed;
   bool                recursive;
   bool                done;
   bool                failed;
};

static bool dir_list_iter_push(dir_list_iter_t *iter,
      struct RDIR *entry, size_t dir_len, size_t pend_len,
      bool pend_append)
{
   struct dir_list_iter_frame *frame;
   if (iter->depth == iter->cap)
   {
      size_t new_cap = iter->cap ? (iter->cap * 2) : 8;
      struct dir_list_iter_frame *new_stack =
            (struct dir_list_iter_frame*)realloc(iter->stack,
                  new_cap * sizeof(*new_stack));
      if (!new_stack)
         return false;
      iter->stack = new_stack;
      iter->cap   = new_cap;
   }
   frame              = &iter->stack[iter->depth++];
   frame->entry       = entry;
   frame->dir_len     = dir_len;
   frame->pend_len    = pend_len;
   frame->pend_append = pend_append;
   return true;
}

dir_list_iter_t *dir_list_iter_new(const char *dir, const char *ext,
      bool include_dirs, bool include_hidden, bool include_compressed,
      bool recursive, struct string_list *list)
{
   dir_list_iter_t *iter;
   struct RDIR *entry;
   size_t dir_len;

   if (!dir || !list)
      return NULL;

   if (!(iter = (dir_list_iter_t*)calloc(1, sizeof(*iter))))
      return NULL;

   if (!(iter->path = (char*)malloc(PATH_MAX_LENGTH)))
   {
      free(iter);
      return NULL;
   }

   if (ext)
   {
      string_list_initialize(&iter->ext_list);
      string_split_noalloc(&iter->ext_list, ext, "|");
      iter->ext_list_ptr    = &iter->ext_list;
   }

   iter->list               = list;
   iter->include_dirs       = include_dirs;
   iter->include_hidden     = include_hidden;
   iter->include_compressed = include_compressed;
   iter->recursive          = recursive;

   dir_len = strlcpy(iter->path, dir, PATH_MAX_LENGTH);

   /* A root that cannot be opened is the condition under which
    * dir_list_new() fails outright - unlike a child, which is
    * skipped. */
   entry   = retro_opendir_include_hidden(iter->path, include_hidden);
   if (!entry)
      goto error;
   if (retro_dirent_error(entry))
   {
      retro_closedir(entry);
      goto error;
   }
   if (!dir_list_iter_push(iter, entry, dir_len, 0, false))
   {
      retro_closedir(entry);
      goto error;
   }
   return iter;

error:
   dir_list_iter_free(iter);
   return NULL;
}

int dir_list_iter_step(dir_list_iter_t *iter,
      bool (*within_budget)(void *userdata), void *userdata)
{
   char *path;
   bool first = true;

   if (!iter || iter->failed)
      return -1;
   if (iter->done)
      return 1;

   path = iter->path;

   while (iter->depth)
   {
      struct dir_list_iter_frame *frame = &iter->stack[iter->depth - 1];
      struct RDIR *entry                = frame->entry;
      size_t dir_len                    = frame->dir_len;
      const char *name;
      size_t _len;
      union string_list_elem_attr attr;

      /* The floor: one entry per call regardless of the budget, so a
       * spent window still makes progress.  After it, yield between
       * any two entries. */
      if (!first && within_budget && !within_budget(userdata))
         return 0;
      first = false;

      if (!retro_readdir(entry))
      {
         /* Level exhausted: close, pop, and perform the parent's
          * pending post-order append of the directory itself (the
          * code after the recursive call in dir_list_read_ctx). */
         bool pend_append = frame->pend_append;
         size_t pend_len  = frame->pend_len;

         retro_closedir(entry);
         iter->depth--;

         if (pend_append)
         {
            path[pend_len] = '\0';
            attr.i         = RARCH_DIRECTORY;
            if (!string_list_append(iter->list, path, attr))
               goto fail;
         }
         continue;
      }

      name = retro_dirent_get_name(entry);

      if (name[0] == '.' || name[0] == '$')
      {
         /* Do not include hidden files and directories */
         if (!iter->include_hidden)
            continue;
         /* char-wise comparisons to avoid string comparison */
         /* Do not include current dir */
         if (name[1] == '\0')
            continue;
         /* Do not include parent dir */
         if (name[1] == '.' && name[2] == '\0')
            continue;
      }

      /* Append @name to the prefix instead of rebuilding the whole
       * path: the prefix is already in place from the parent level. */
      _len = dir_len;
      if (_len && path[_len - 1] != '/' && path[_len - 1] != '\\')
         path[_len++] = '/';
      _len += strlcpy(path + _len, name, PATH_MAX_LENGTH - _len);

      if (retro_dirent_is_dir(entry, NULL))
      {
         /* Exclude this frequent hidden dir on platforms which can not handle hidden attribute */
         if (!iter->include_hidden && strcmp(name, "System Volume Information") == 0)
            continue;

#if TARGET_OS_IPHONE || TARGET_OS_OSX
         {
            size_t name_len = strlen(name);
            if (name_len >= 10
                  && !memcmp(name + name_len - 10, ".framework", 10))
            {
               attr.i = RARCH_PLAIN_FILE;
               if (!string_list_append(iter->list, path, attr))
                  goto fail;
               continue;
            }
         }
#endif
         if (iter->recursive)
         {
            /* Descend.  A child that cannot be opened is skipped
             * silently - the recursion ignored the child call's
             * return value - but the post-order append it owed
             * still happens, so perform it here directly. */
            struct RDIR *child = retro_opendir_include_hidden(path,
                  iter->include_hidden);

            if (child && retro_dirent_error(child))
            {
               retro_closedir(child);
               child = NULL;
            }

            if (child)
            {
               if (!dir_list_iter_push(iter, child, _len, _len,
                        iter->include_dirs))
               {
                  retro_closedir(child);
                  goto fail;
               }
               continue;
            }

            path[_len] = '\0';
         }

         if (!iter->include_dirs)
            continue;
         attr.i = RARCH_DIRECTORY;
      }
      else
      {
         const char *file_ext    = path_get_extension(name);

         attr.i                  = RARCH_FILETYPE_UNSET;

         if (string_list_find_elem_prefix(iter->ext_list_ptr, ".", file_ext))
            attr.i            = RARCH_PLAIN_FILE;
         else
         {
            bool is_compressed_file;
            if ((is_compressed_file = path_is_compressed_file(path)))
               attr.i               = RARCH_COMPRESSED_ARCHIVE;

            if (iter->ext_list_ptr &&
                  (!is_compressed_file || !iter->include_compressed))
               continue;
         }
      }

      if (!string_list_append(iter->list, path, attr))
         goto fail;
   }

   iter->done = true;
   return 1;

fail:
   iter->failed = true;
   return -1;
}

void dir_list_iter_free(dir_list_iter_t *iter)
{
   size_t i;
   if (!iter)
      return;
   for (i = 0; i < iter->depth; i++)
      retro_closedir(iter->stack[i].entry);
   free(iter->stack);
   free(iter->path);
   string_list_deinitialize(&iter->ext_list);
   free(iter);
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
      ext_list_ptr                  = &ext_list;
   }
   {
      struct dir_list_ctx ctx;
      char *path = (char*)malloc(PATH_MAX_LENGTH);
      if (!path)
      {
         string_list_deinitialize(&ext_list);
         return false;
      }
      ctx.list               = list;
      ctx.ext_list           = ext_list_ptr;
      ctx.path               = path;
      ctx.include_dirs       = include_dirs;
      ctx.include_hidden     = include_hidden;
      ctx.include_compressed = include_compressed;
      ctx.recursive          = recursive;
      ret = dir_list_read_ctx(strlcpy(path, dir, PATH_MAX_LENGTH), &ctx) != -1;
      free(path);
   }
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
   struct string_list *list       = string_list_new();

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
 * NOTE: @list must zero initialised before
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
