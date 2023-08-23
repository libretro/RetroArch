/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_path.c).
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>

#include <sys/stat.h>

#include <boolean.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <time/rtime.h>

/* TODO: There are probably some unnecessary things on this huge include list now but I'm too afraid to touch it */
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif
#ifdef __HAIKU__
#include <kernel/image.h>
#endif
#ifndef __MACH__
#include <compat/strl.h>
#include <compat/posix_string.h>
#endif
#include <retro_miscellaneous.h>
#include <encodings/utf.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h> /* stat() is defined here */
#endif

#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
#ifdef __WINRT__
#include <uwp/uwp_func.h>
#endif
#endif

/* Assume W-functions do not work below Win2K and Xbox platforms */
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)

#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif

#endif

/* Time format strings with AM-PM designation require special
 * handling due to platform dependence */
void strftime_am_pm(char *s, size_t len, const char* format,
      const void *ptr)
{
   char *local = NULL;
   const struct tm *timeptr = (const struct tm*)ptr;

   /* Ensure correct locale is set
    * > Required for localised AM/PM strings */
   setlocale(LC_TIME, "");

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

/**
 * Create a new linked list with one node in it
 * The path on this node will be set to NULL
**/
struct path_linked_list* path_linked_list_new(void)
{
   struct path_linked_list* paths_list = (struct path_linked_list*)malloc(sizeof(*paths_list));
   paths_list->next = NULL;
   paths_list->path = NULL;
   return paths_list;
}

/**
 * path_linked_list_free:
 *
 * Free the entire linked list
 **/
void path_linked_list_free(struct path_linked_list *in_path_linked_list)
{
   struct path_linked_list *node_tmp = (struct path_linked_list*)in_path_linked_list;
   while (node_tmp)
   {
      struct path_linked_list *hold = NULL;
      if (node_tmp->path)
         free(node_tmp->path);
      hold     = (struct path_linked_list*)node_tmp;
      node_tmp = node_tmp->next;
      if (hold)
         free(hold);
   }
}

/**
 * path_linked_list_add_path:
 *
 * Add a node to the linked list with this path
 * If the first node's path if it's not yet set the path
 * on this node instead
**/
void path_linked_list_add_path(struct path_linked_list *in_path_linked_list,
      char *path)
{
    /* If the first item does not have a path this is
      a list which has just been created, so we just fill 
      the path for the first item
   */
   if (!in_path_linked_list->path)
      in_path_linked_list->path = strdup(path);
   else
   {  
      struct path_linked_list *node = (struct path_linked_list*) malloc(sizeof(*node));

      if (node)
      {
         struct path_linked_list *head = in_path_linked_list;

         node->next                    = NULL;
         node->path                    = strdup(path);

         if (head)
         {
            while (head->next)
               head        = head->next;

            head->next     = node;
         }
         else
            in_path_linked_list = node;
      }
   }
}

/**
 * path_get_archive_delim:
 * @path               : path
 *
 * Find delimiter of an archive file. Only the first '#'
 * after a compression extension is considered.
 *
 * @return pointer to the delimiter in the path if it contains
 * a path inside a compressed file, otherwise NULL.
 **/
const char *path_get_archive_delim(const char *path)
{
   char buf[5];
   /* Find delimiter position
    * > Since filenames may contain '#' characters,
    *   must loop until we find the first '#' that
    *   is directly *after* a compression extension */
   const char *delim      = strchr(path, '#');

   while (delim)
   {
      /* Check whether this is a known archive type
       * > Note: The code duplication here is
       *   deliberate, to maximise performance */
      if (delim - path > 4)
      {
         strlcpy(buf, delim - 4, sizeof(buf));
         buf[4] = '\0';

         string_to_lower(buf);

         /* Check if this is a '.zip', '.apk' or '.7z' file */
         if (   string_is_equal(buf,     ".zip")
             || string_is_equal(buf,     ".apk")
             || string_is_equal(buf + 1, ".7z"))
            return delim;
      }
      else if (delim - path > 3)
      {
         strlcpy(buf, delim - 3, sizeof(buf));
         buf[3] = '\0';

         string_to_lower(buf);

         /* Check if this is a '.7z' file */
         if (string_is_equal(buf, ".7z"))
            return delim;
      }

      delim++;
      delim = strchr(delim, '#');
   }

   return NULL;
}

/**
 * path_get_extension:
 * @path               : path
 *
 * Gets extension of file. Only '.'s
 * after the last slash are considered.
 *
 * @return extension part from the path.
 **/
const char *path_get_extension(const char *path)
{
   const char *ext;
   if (!string_is_empty(path) && ((ext = (char*)strrchr(path_basename(path), '.'))))
      return ext + 1;
   return "";
}

/**
 * path_get_extension_mutable:
 * @path               : path
 *
 * Specialized version of path_get_extension(). Return
 * value is mutable.
 *
 * Gets extension of file. Only '.'s
 * after the last slash are considered.
 *
 * @return extension part from the path.
 **/
char *path_get_extension_mutable(const char *path)
{
   char *ext = NULL;
   if (!string_is_empty(path) && ((ext = (char*)strrchr(path_basename(path), '.'))))
      return ext;
   return NULL;
}

/**
 * path_remove_extension:
 * @path               : path
 *
 * Mutates path by removing its extension. Removes all
 * text after and including the last '.'.
 * Only '.'s after the last slash are considered.
 *
 * @return
 * 1) If path has an extension, returns path with the
 *    extension removed.
 * 2) If there is no extension, returns NULL.
 * 3) If path is empty or NULL, returns NULL
 **/
char *path_remove_extension(char *path)
{
   char *last = !string_is_empty(path)
      ? (char*)strrchr(path_basename(path), '.') : NULL;
   if (!last)
      return NULL;
   if (*last)
      *last = '\0';
   return path;
}

/**
 * path_is_compressed_file:
 * @path               : path
 *
 * Checks if path is a compressed file.
 *
 * @return true if path is a compressed file, otherwise false.
 **/
bool path_is_compressed_file(const char* path)
{
   const char *ext = path_get_extension(path);
   if (!string_is_empty(ext))
      return (   string_is_equal_noncase(ext, "zip")
              || string_is_equal_noncase(ext, "apk")
              || string_is_equal_noncase(ext, "7z"));
   return false;
}

/**
 * fill_pathname:
 * @out_path           : output path
 * @in_path            : input  path
 * @replace            : what to replace
 * @size               : buffer size of output path
 *
 * FIXME: Verify
 *
 * Replaces filename extension with 'replace' and outputs result to out_path.
 * The extension here is considered to be the string from the last '.'
 * to the end.
 *
 * Only '.'s after the last slash are considered as extensions.
 * If no '.' is present, in_path and replace will simply be concatenated.
 * 'size' is buffer size of 'out_path'.
 * E.g.: in_path = "/foo/bar/baz/boo.c", replace = ".asm" =>
 * out_path = "/foo/bar/baz/boo.asm"
 * E.g.: in_path = "/foo/bar/baz/boo.c", replace = ""     =>
 * out_path = "/foo/bar/baz/boo"
 *
 * @return Length of the string copied into @out
 */
size_t fill_pathname(char *out_path, const char *in_path,
      const char *replace, size_t size)
{
   size_t _len;
   char tmp_path[PATH_MAX_LENGTH];
   char *tok   = NULL;
   strlcpy(tmp_path, in_path, sizeof(tmp_path));
   if ((tok = (char*)strrchr(path_basename(tmp_path), '.')))
      *tok = '\0';

   _len  = strlcpy(out_path, tmp_path, size);
   _len += strlcpy(out_path + _len, replace, size - _len);
   return _len;
}


/**
 * find_last_slash:
 * @str                : path
 * @size               : size of path
 *
 * Find last slash in path. Tries to find
 * a backslash as used for Windows paths,
 * otherwise checks for a regular slash.

 * @return pointer to last slash/backslash found in @str.
 **/
char *find_last_slash(const char *str)
{
   const char *slash     = strrchr(str, '/');
   const char *backslash = strrchr(str, '\\');

   if (!slash || (backslash > slash))
      return (char*)backslash;
   else
      return (char*)slash;
}

/**
 * fill_pathname_slash:
 * @path               : path
 * @size               : size of path
 *
 * Assumes path is a directory. Appends a slash
 * if not already there.
 **/
size_t fill_pathname_slash(char *path, size_t size)
{
   size_t path_len;
   const char *last_slash = find_last_slash(path);
   if (!last_slash)
      return strlcat(path, PATH_DEFAULT_SLASH(), size);
   path_len            = strlen(path);
   /* Try to preserve slash type. */
   if (last_slash != (path + path_len - 1))
   {
      path[  path_len] = last_slash[0];
      path[++path_len] = '\0';
   }
   return path_len;
}

/**
 * fill_pathname_dir:
 * @in_dir             : input directory path
 * @in_basename        : input basename to be appended to @in_dir
 * @replace            : replacement to be appended to @in_basename
 * @size               : size of buffer
 *
 * Appends basename of 'in_basename', to 'in_dir', along with 'replace'.
 * Basename of in_basename is the string after the last '/' or '\\',
 * i.e the filename without directories.
 *
 * If in_basename has no '/' or '\\', the whole 'in_basename' will be used.
 * 'size' is buffer size of 'in_dir'.
 *
 * E.g..: in_dir = "/tmp/some_dir", in_basename = "/some_content/foo.c",
 * replace = ".asm" => in_dir = "/tmp/some_dir/foo.c.asm"
 **/
size_t fill_pathname_dir(char *in_dir, const char *in_basename,
      const char *replace, size_t size)
{
   size_t _len      = fill_pathname_slash(in_dir, size);
   const char *base = path_basename(in_basename);
   _len            += strlcpy(in_dir + _len, base,    size - _len);
   _len            += strlcpy(in_dir + _len, replace, size - _len);
   return _len;
}

/**
 * fill_pathname_base:
 * @out                : output path
 * @in_path            : input path
 * @size               : size of output path
 *
 * Copies basename of @in_path into @out_path.
 *
 * @return Length of the string copied into @out
 **/
size_t fill_pathname_base(char *out, const char *in_path, size_t size)
{
   const char     *ptr = path_basename(in_path);
   if (ptr)
      return strlcpy(out, ptr, size);
   return strlcpy(out, in_path, size);
}

/**
 * fill_pathname_basedir:
 * @out_dir            : output directory
 * @in_path            : input path
 * @size               : size of output directory
 *
 * Copies base directory of @in_path into @out_path.
 * If in_path is a path without any slashes (relative current directory),
 * @out_path will get path "./".
 **/
void fill_pathname_basedir(char *out_dir,
      const char *in_path, size_t size)
{
   if (out_dir != in_path)
      strlcpy(out_dir, in_path, size);
   path_basedir(out_dir);
}

/**
 * fill_pathname_parent_dir_name:
 * @out_dir            : output directory
 * @in_dir             : input directory
 * @size               : size of output directory
 *
 * Copies only the parent directory name of @in_dir into @out_dir.
 * The two buffers must not overlap. Removes trailing '/'.
 *
 * @return true on success, false if a slash was not found in the path.
 **/
bool fill_pathname_parent_dir_name(char *out_dir,
      const char *in_dir, size_t size)
{
   char *temp   = strdup(in_dir);
   char *last   = find_last_slash(temp);

   if (last && last[1] == 0)
   {
      *last     = '\0';
      last      = find_last_slash(temp);
   }

   /* Cut the last part of the string (the filename) after the slash,
      leaving the directory name (or nested directory names) only. */
   if (last)
      *last     = '\0';

   /* Point in_dir to the address of the last slash. */
   /* If find_last_slash returns NULL, it means there was no slash in temp,
      so use temp as-is. */
   if (!(in_dir = find_last_slash(temp)))
       in_dir   = temp;

   if (in_dir && in_dir[1])
   {
       /* If path starts with an slash, eliminate it. */
       if (path_is_absolute(in_dir))
           strlcpy(out_dir, in_dir + 1, size);
       else
           strlcpy(out_dir, in_dir, size);
       free(temp);
       return true;
   }

   free(temp);
   return false;
}

/**
 * fill_pathname_parent_dir:
 * @out_dir            : output directory
 * @in_dir             : input directory
 * @size               : size of output directory
 *
 * Copies parent directory of @in_dir into @out_dir.
 * Assumes @in_dir is a directory. Keeps trailing '/'.
 * If the path was already at the root directory,
 * @out_dir will be an empty string.
 **/
void fill_pathname_parent_dir(char *out_dir,
      const char *in_dir, size_t size)
{
   size_t len = 0;
   if (out_dir != in_dir)
      len = strlcpy(out_dir, in_dir, size);
   else
      len = strlen(out_dir);
   path_parent_dir(out_dir, len);
}

/**
 * fill_dated_filename:
 * @out_filename       : output filename
 * @ext                : extension of output filename
 * @size               : buffer size of output filename
 *
 * Creates a 'dated' filename prefixed by 'RetroArch', and
 * concatenates extension (@ext) to it.
 *
 * E.g.:
 * out_filename = "RetroArch-{month}{day}-{Hours}{Minutes}.{@ext}"
 **/
size_t fill_dated_filename(char *out_filename,
      const char *ext, size_t size)
{
   size_t _len;
   struct tm tm_;
   time_t cur_time = time(NULL);
   rtime_localtime(&cur_time, &tm_);
   _len  = strftime(out_filename, size,
         "RetroArch-%m%d-%H%M%S", &tm_);
   _len += strlcpy(out_filename + _len, ext, size - _len);
   return _len;
}

/**
 * fill_str_dated_filename:
 * @out_filename       : output filename
 * @in_str             : input string
 * @ext                : extension of output filename
 * @size               : buffer size of output filename
 *
 * Creates a 'dated' filename prefixed by the string @in_str, and
 * concatenates extension (@ext) to it.
 *
 * E.g.:
 * out_filename = "RetroArch-{year}{month}{day}-{Hour}{Minute}{Second}.{@ext}"
 *
 * @return Length of the string copied into @out_path
 **/
size_t fill_str_dated_filename(char *out_filename,
      const char *in_str, const char *ext, size_t size)
{
   struct tm tm_;
   char format[NAME_MAX_LENGTH];
   size_t _len     = 0;
   time_t cur_time = time(NULL);
   rtime_localtime(&cur_time, &tm_);
   _len      = strlcpy(out_filename, in_str, size);
   if (string_is_empty(ext))
   {
      strftime(format, sizeof(format), "-%y%m%d-%H%M%S", &tm_);
      _len  += strlcpy(out_filename + _len, format, size - _len);
   }
   else
   {
      strftime(format, sizeof(format), "-%y%m%d-%H%M%S.", &tm_);
      _len  += strlcpy(out_filename + _len, format, size - _len);
      _len  += strlcpy(out_filename + _len, ext,    size - _len);
   }
   return _len;
}

/**
 * path_basedir:
 * @path               : path
 *
 * Extracts base directory by mutating path.
 * Keeps trailing '/'.
 **/
void path_basedir(char *path)
{
   char *last = NULL;
   if (!path || path[0] == '\0' || path[1] == '\0')
      return;

   if ((last = find_last_slash(path)))
      last[1] = '\0';
   else
   {
      path[0] = '.';
      path[1] = PATH_DEFAULT_SLASH_C();
      path[2] = '\0';
   }
}

/**
 * path_parent_dir:
 * @path               : path
 * @len                : length of @path
 *
 * Extracts parent directory by mutating path.
 * Assumes that path is a directory. Keeps trailing '/'.
 * If the path was already at the root directory, returns empty string
 **/
void path_parent_dir(char *path, size_t len)
{
   if (!path)
      return;
   
   if (len && PATH_CHAR_IS_SLASH(path[len - 1]))
   {
      bool path_was_absolute = path_is_absolute(path);

      path[len - 1] = '\0';

      if (path_was_absolute && !find_last_slash(path))
      {
         /* We removed the only slash from what used to be an absolute path.
          * On Linux, this goes from "/" to an empty string and everything works fine,
          * but on Windows, we went from C:\ to C:, which is not a valid path and that later
          * gets errornously treated as a relative one by path_basedir and returns "./".
          * What we really wanted is an empty string. */
         path[0] = '\0';
         return;
      }
   }
   path_basedir(path);
}

/**
 * path_basename:
 * @path               : path
 *
 * Get basename from @path.
 *
 * @return basename from path.
 **/
const char *path_basename(const char *path)
{
   /* We cut either at the first compression-related hash,
    * or we cut at the last slash */
   const char *ptr = NULL;
   if (     (ptr = path_get_archive_delim(path))
         || (ptr = find_last_slash(path)))
      return ptr + 1;
   return path;
}

/* Specialized version */
/**
 * path_basename_nocompression:
 * @path               : path
 *
 * Specialized version of path_basename().
 * Get basename from @path.
 *
 * @return basename from path.
 **/
const char *path_basename_nocompression(const char *path)
{
   /* We cut at the last slash */
   const char *last  = find_last_slash(path);
   if (last)
      return last + 1;
   return path;
}

/**
 * path_is_absolute:
 * @path               : path
 *
 * Checks if @path is an absolute path or a relative path.
 *
 * @return true if path is absolute, false if path is relative.
 **/
bool path_is_absolute(const char *path)
{
   if (!string_is_empty(path))
   {
      if (path[0] == '/')
         return true;
#if defined(_WIN32)
      /* Many roads lead to Rome...
       * Note: Drive letter can only be 1 character long */
      return ( string_starts_with_size(path,     "\\\\", STRLEN_CONST("\\\\"))
            || string_starts_with_size(path + 1, ":/",   STRLEN_CONST(":/")) 
            || string_starts_with_size(path + 1, ":\\",  STRLEN_CONST(":\\")));
#elif defined(__wiiu__) || defined(VITA)
      {
         const char *seperator = strchr(path, ':');
         return (seperator && (seperator[1] == '/'));
      }
#endif
   }

   return false;
}

/**
 * path_resolve_realpath:
 * @buf                : input and output buffer for path
 * @size               : size of buffer
 * @resolve_symlinks   : whether to resolve symlinks or not
 *
 * Resolves use of ".", "..", multiple slashes etc in absolute paths.
 *
 * Relative paths are rebased on the current working dir.
 *
 * @return @buf if successful, NULL otherwise.
 * Note: Not implemented on consoles
 * Note: Symlinks are only resolved on Unix-likes
 * Note: The current working dir might not be what you expect,
 *       e.g. on Android it is "/"
 *       Use of fill_pathname_resolve_relative() should be prefered
 **/
char *path_resolve_realpath(char *buf, size_t size, bool resolve_symlinks)
{
#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
#ifdef _WIN32
   char *ret         = NULL;
   wchar_t *rel_path = utf8_to_utf16_string_alloc(buf);

   if (rel_path)
   {
      wchar_t abs_path[PATH_MAX_LENGTH];

      if (_wfullpath(abs_path, rel_path, PATH_MAX_LENGTH))
      {
         char *tmp = utf16_to_utf8_string_alloc(abs_path);

         if (tmp)
         {
            strlcpy(buf, tmp, size);
            free(tmp);
            ret = buf;
         }
      }

      free(rel_path);
   }

   return ret;
#else
   char tmp[PATH_MAX_LENGTH];
   size_t t;
   char *p;
   const char *next;
   const char *buf_end;

   if (resolve_symlinks)
   {
      strlcpy(tmp, buf, sizeof(tmp));

      /* NOTE: realpath() expects at least PATH_MAX_LENGTH bytes in buf.
       * Technically, PATH_MAX_LENGTH needn't be defined, but we rely on it anyways.
       * POSIX 2008 can automatically allocate for you,
       * but don't rely on that. */
      if (!realpath(tmp, buf))
      {
         strlcpy(buf, tmp, size);
         return NULL;
      }

      return buf;
   }

   t       = 0; /* length of output */
   buf_end = buf + strlen(buf);

   if (!path_is_absolute(buf))
   {
      size_t len;
      /* rebase on working directory */
      if (!getcwd(tmp, PATH_MAX_LENGTH - 1))
         return NULL;

      len = strlen(tmp);
      t  += len;

      if (tmp[len - 1] != '/')
         tmp[t++] = '/';

      if (string_is_empty(buf))
         goto end;

      p = buf;
   }
   else
   {
      /* UNIX paths can start with multiple '/', copy those */
      for (p = buf; *p == '/'; p++)
         tmp[t++] = '/';
   }

   /* p points to just after a slash while 'next' points to the next slash
    * if there are no slashes, they point relative to where one would be */
   do
   {
      if (!(next = strchr(p, '/')))
         next = buf_end;

      if ((next - p == 2 && p[0] == '.' && p[1] == '.'))
      {
         p += 3;

         /* fail for illegal /.., //.. etc */
         if (t == 1 || tmp[t-2] == '/')
            return NULL;

         /* delete previous segment in tmp by adjusting size t
          * tmp[t - 1] == '/', find '/' before that */
         t -= 2;
         while (tmp[t] != '/')
            t--;
         t++;
      }
      else if (next - p == 1 && p[0] == '.')
         p += 2;
      else if (next - p == 0)
         p += 1;
      else
      {
         /* fail when truncating */
         if (t + next - p + 1 > PATH_MAX_LENGTH - 1)
            return NULL;

         while (p <= next)
            tmp[t++] = *p++;
      }
   }while(next < buf_end);
   

end:
   tmp[t] = '\0';
   strlcpy(buf, tmp, size);
   return buf;
#endif
#endif
   return NULL;
}

/**
 * path_relative_to:
 * @out                : buffer to write the relative path to
 * @path               : path to be expressed relatively
 * @base               : base directory to start out on
 * @size               : size of output buffer
 *
 * Turns @path into a path relative to @base and writes it to @out.
 *
 * @base is assumed to be a base directory, i.e. a path ending with '/' or '\'.
 * Both @path and @base are assumed to be absolute paths without "." or "..".
 *
 * E.g. path /a/b/e/f.cg with base /a/b/c/d/ turns into ../../e/f.cg
 *
 * @return Length of the string copied into @out
 **/
size_t path_relative_to(char *out,
      const char *path, const char *base, size_t size)
{
   size_t i, j;
   const char *trimmed_path, *trimmed_base;

#ifdef _WIN32
   /* For different drives, return absolute path */
   if (
            path
         && base
         && path[0] != '\0' 
         && path[1] != '\0'
         && base[0] != '\0'
         && base[1] != '\0'
         && path[1] == ':' 
         && base[1] == ':'
         && path[0] != base[0])
      return strlcpy(out, path, size);
#endif

   /* Trim common beginning */
   for (i = 0, j = 0; path[i] && base[i] && path[i] == base[i]; i++)
      if (path[i] == PATH_DEFAULT_SLASH_C())
         j = i + 1;

   trimmed_path = path+j;
   trimmed_base = base+i;

   /* Each segment of base turns into ".." */
   out[0] = '\0';
   for (i = 0; trimmed_base[i]; i++)
      if (trimmed_base[i] == PATH_DEFAULT_SLASH_C())
         strlcat(out, ".." PATH_DEFAULT_SLASH(), size);

   return strlcat(out, trimmed_path, size);
}

/**
 * fill_pathname_resolve_relative:
 * @out_path           : output path
 * @in_refpath         : input reference path
 * @in_path            : input path
 * @size               : size of @out_path
 *
 * Joins basedir of @in_refpath together with @in_path.
 * If @in_path is an absolute path, out_path = in_path.
 * E.g.: in_refpath = "/foo/bar/baz.a", in_path = "foobar.cg",
 * out_path = "/foo/bar/foobar.cg".
 **/
void fill_pathname_resolve_relative(char *out_path,
      const char *in_refpath, const char *in_path, size_t size)
{
   if (path_is_absolute(in_path))
   {
      strlcpy(out_path, in_path, size);
      return;
   }

   if (out_path != in_refpath)
      strlcpy(out_path, in_refpath, size);
   path_basedir(out_path);
   strlcat(out_path, in_path, size);
   path_resolve_realpath(out_path, size, false);
}

/**
 * fill_pathname_join:
 * @out_path           : output path
 * @dir                : directory
 * @path               : path
 * @size               : size of output path
 *
 * Joins a directory (@dir) and path (@path) together.
 * Makes sure not to get  two consecutive slashes
 * between directory and path.
 *
 * Deprecated. Use fill_pathname_join_special() instead
 * if you can ensure @dir and @out_path won't overlap.
 *
 * @return Length of the string copied into @out_path
 **/
size_t fill_pathname_join(char *out_path,
      const char *dir, const char *path, size_t size)
{
   size_t _len = 0;
   if (out_path != dir)
      _len = strlcpy(out_path, dir, size);
   if (*out_path)
      _len = fill_pathname_slash(out_path, size);
   _len   += strlcpy(out_path + _len, path, size - _len);
   return _len;
}

/**
 * fill_pathname_join_special:
 * @out_path           : output path
 * @dir                : directory. Cannot be identical to @out_path
 * @path               : path
 * @size               : size of output path
 *
 *
 * Specialized version of fill_pathname_join.
 * Unlike fill_pathname_join(),
 * @dir and @out_path CANNOT be identical.
 *
 * Joins a directory (@dir) and path (@path) together.
 * Makes sure not to get  two consecutive slashes
 * between directory and path.
 *
 * @return Length of the string copied into @out_path
 **/
size_t fill_pathname_join_special(char *out_path,
      const char *dir, const char *path, size_t size)
{
   size_t len = strlcpy(out_path, dir, size);

   if (*out_path)
   {
      const char *last_slash = find_last_slash(out_path);
      if (last_slash)
      {
         /* Try to preserve slash type. */
         if (last_slash != (out_path + len - 1))
         {
            out_path[  len] = last_slash[0];
            out_path[++len] = '\0';
         }
      }
      else
      {
         out_path[  len]    = PATH_DEFAULT_SLASH_C();
         out_path[++len]    = '\0';
      }
   }

   len += strlcpy(out_path + len, path, size - len);
   return len;
}

size_t fill_pathname_join_special_ext(char *out_path,
      const char *dir,  const char *path,
      const char *last, const char *ext,
      size_t size)
{
   size_t _len = fill_pathname_join(out_path, dir, path, size);
   if (*out_path)
      _len     = fill_pathname_slash(out_path, size);
   _len       += strlcpy(out_path + _len, last, size - _len);
   _len       += strlcpy(out_path + _len, ext,  size - _len);
   return _len;
}

/**
 * fill_pathname_join_delim:
 * @out_path           : output path
 * @dir                : directory
 * @path               : path
 * @delim              : delimiter
 * @size               : size of output path
 *
 * Joins a directory (@dir) and path (@path) together
 * using the given delimiter (@delim).
 **/
size_t fill_pathname_join_delim(char *out_path, const char *dir,
      const char *path, const char delim, size_t size)
{
   size_t _len;
   /* Behavior of strlcpy is undefined if dst and src overlap */
   if (out_path == dir)
      _len          = strlen(dir);
   else
      _len          = strlcpy(out_path, dir, size);

   out_path[_len]   = delim;
   out_path[_len+1] = '\0';

   if (path)
      return strlcat(out_path, path, size);
   return _len;
}

size_t fill_pathname_expand_special(char *out_path,
      const char *in_path, size_t size)
{
#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
   char *app_dir = NULL;
   if (in_path[0] == '~')
   {
      app_dir    = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      fill_pathname_home_dir(app_dir, PATH_MAX_LENGTH * sizeof(char));
   }
   else if (in_path[0] == ':')
   {
      app_dir    = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      app_dir[0] = '\0';
      fill_pathname_application_dir(app_dir, PATH_MAX_LENGTH * sizeof(char));
   }

   if (app_dir)
   {
      if (*app_dir)
      {
         size_t src_size = strlcpy(out_path, app_dir, size);

         out_path       += src_size;
         size           -= src_size;

         if (!PATH_CHAR_IS_SLASH(out_path[-1]))
         {
            src_size  = strlcpy(out_path, PATH_DEFAULT_SLASH(), size);

            out_path += src_size;
            size     -= src_size;
         }

         in_path += 2;
      }

      free(app_dir);
   }
#endif
   return strlcpy(out_path, in_path, size);
}

size_t fill_pathname_abbreviate_special(char *out_path,
      const char *in_path, size_t size)
{
#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
   unsigned i;
   const char *candidates[3];
   const char *notations[3];
   char application_dir[PATH_MAX_LENGTH];
   char home_dir[PATH_MAX_LENGTH];

   application_dir[0] = '\0';

   /* application_dir could be zero-string. Safeguard against this.
    *
    * Keep application dir in front of home, moving app dir to a
    * new location inside home would break otherwise. */

   /* ugly hack - use application_dir pointer
    * before filling it in. C89 reasons */
   candidates[0] = application_dir;
   candidates[1] = home_dir;
   candidates[2] = NULL;

   notations [0] = ":";
   notations [1] = "~";
   notations [2] = NULL;

   fill_pathname_application_dir(application_dir, sizeof(application_dir));
   fill_pathname_home_dir(home_dir, sizeof(home_dir));

   for (i = 0; candidates[i]; i++)
   {
      if (!string_is_empty(candidates[i]) &&
          string_starts_with(in_path, candidates[i]))
      {
         size_t src_size  = strlcpy(out_path, notations[i], size);

         out_path        += src_size;
         size            -= src_size;
         in_path         += strlen(candidates[i]);

         if (!PATH_CHAR_IS_SLASH(*in_path))
         {
            strcpy_literal(out_path, PATH_DEFAULT_SLASH());
            out_path++;
            size--;
         }

         break; /* Don't allow more abbrevs to take place. */
      }
   }
#endif
   return strlcpy(out_path, in_path, size);
}

/**
 * pathname_conform_slashes_to_os:
 *
 * @path               : path
 *
 * Leaf function.
 *
 * Changes the slashes to the correct kind for the OS 
 * So forward slash on linux and backslash on Windows
 **/
void pathname_conform_slashes_to_os(char *path)
{
   /* Conform slashes to OS standard so we get proper matching */
   char *p;
   for (p = path; *p; p++)
      if (*p == '/' || *p == '\\')
         *p = PATH_DEFAULT_SLASH_C();
}

/**
 * pathname_make_slashes_portable:
 * @path               : path
 *
 * Leaf function.
 *
 * Change all slashes to forward so they are more 
 * portable between Windows and Linux
 **/
void pathname_make_slashes_portable(char *path)
{
   /* Conform slashes to OS standard so we get proper matching */
   char *p;
   for (p = path; *p; p++)
      if (*p == '/' || *p == '\\')
         *p = '/';
}

/**
 * get_pathname_num_slashes:
 * @in_path            : input path
 *
 * Leaf function.
 *
 * Get the number of slashes in a path.
 *
 * @return number of slashes found in @in_path.
 **/
static int get_pathname_num_slashes(const char *in_path)
{
   int num_slashes = 0;
   int i = 0;

   for (i = 0; i < PATH_MAX_LENGTH; i++)
   {
      if (PATH_CHAR_IS_SLASH(in_path[i]))
         num_slashes++;
      if (in_path[i] == '\0')
         break;
   }

   return num_slashes;
}

/**
 * fill_pathname_abbreviated_or_relative:
 *
 * Fills the supplied path with either the abbreviated path or 
 * the relative path, which ever one has less depth / number of slashes
 * 
 * If lengths of abbreviated and relative paths are the same,
 * the relative path will be used
 * @in_path can be an absolute, relative or abbreviated path
 *
 * @return Length of the string copied into @out_path
 **/
size_t fill_pathname_abbreviated_or_relative(char *out_path,
      const char *in_refpath, const char *in_path, size_t size)
{
   char in_path_conformed[PATH_MAX_LENGTH];
   char in_refpath_conformed[PATH_MAX_LENGTH];
   char expanded_path[PATH_MAX_LENGTH];
   char absolute_path[PATH_MAX_LENGTH];
   char relative_path[PATH_MAX_LENGTH];
   char abbreviated_path[PATH_MAX_LENGTH];
   
   expanded_path[0]        = '\0';
   absolute_path[0]        = '\0';
   relative_path[0]        = '\0';
   abbreviated_path[0]     = '\0';

   strlcpy(in_path_conformed, in_path, sizeof(in_path_conformed));
   strlcpy(in_refpath_conformed, in_refpath, sizeof(in_refpath_conformed));

   pathname_conform_slashes_to_os(in_path_conformed);
   pathname_conform_slashes_to_os(in_refpath_conformed);

   /* Expand paths which start with :\ to an absolute path */
   fill_pathname_expand_special(expanded_path,
         in_path_conformed, sizeof(expanded_path));

   /* Get the absolute path if it is not already */
   if (path_is_absolute(expanded_path))
      strlcpy(absolute_path, expanded_path, PATH_MAX_LENGTH);
   else
      fill_pathname_resolve_relative(absolute_path,
            in_refpath_conformed, in_path_conformed, PATH_MAX_LENGTH);

   pathname_conform_slashes_to_os(absolute_path);

   /* Get the relative path and see how many directories long it is */
   path_relative_to(relative_path, absolute_path,
         in_refpath_conformed, sizeof(relative_path));

   /* Get the abbreviated path and see how many directories long it is */
   fill_pathname_abbreviate_special(abbreviated_path,
         absolute_path, sizeof(abbreviated_path));

   /* Use the shortest path, preferring the relative path*/
   if (  get_pathname_num_slashes(relative_path) <= 
         get_pathname_num_slashes(abbreviated_path))
      return strlcpy(out_path, relative_path, size);
   return strlcpy(out_path, abbreviated_path, size);
}

/**
 * path_basedir:
 * @path               : path
 *
 * Extracts base directory by mutating path.
 * Keeps trailing '/'.
 **/
void path_basedir_wrapper(char *path)
{
   char *last = NULL;
   if (!path || path[0] == '\0' || path[1] == '\0')
      return;

#ifdef HAVE_COMPRESSION
   /* We want to find the directory with the archive in basedir. */
   if ((last = (char*)path_get_archive_delim(path)))
      *last = '\0';
#endif

   if ((last = find_last_slash(path)))
      last[1] = '\0';
   else
   {
      path[0] = '.';
      path[1] = PATH_DEFAULT_SLASH_C();
      path[2] = '\0';
   }
}

#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
void fill_pathname_application_path(char *s, size_t len)
{
   size_t i;
#ifdef __APPLE__
  CFBundleRef bundle = CFBundleGetMainBundle();
#endif
#ifdef _WIN32
   DWORD ret = 0;
   wchar_t wstr[PATH_MAX_LENGTH] = {0};
#endif
#ifdef __HAIKU__
   image_info info;
   int32_t cookie = 0;
#endif
   (void)i;

   if (!len)
      return;

#if defined(_WIN32)
#ifdef LEGACY_WIN32
   ret    = GetModuleFileNameA(NULL, s, len);
#else
   ret    = GetModuleFileNameW(NULL, wstr, ARRAY_SIZE(wstr));

   if (*wstr)
   {
      char *str = utf16_to_utf8_string_alloc(wstr);

      if (str)
      {
         strlcpy(s, str, len);
         free(str);
      }
   }
#endif
   s[ret] = '\0';
#elif defined(__APPLE__)
   if (bundle)
   {
      CFURLRef bundle_url     = CFBundleCopyBundleURL(bundle);
      CFStringRef bundle_path = CFURLCopyPath(bundle_url);
      CFStringGetCString(bundle_path, s, len, kCFStringEncodingUTF8);
#ifdef HAVE_COCOATOUCH
      {
         /* This needs to be done so that the path becomes 
          * /private/var/... and this
          * is used consistently throughout for the iOS bundle path */
         char resolved_bundle_dir_buf[PATH_MAX_LENGTH] = {0};
         if (realpath(s, resolved_bundle_dir_buf))
         {
            size_t _len = strlcpy(s, resolved_bundle_dir_buf, len - 1);
            s[  _len]   = '/';
            s[++_len]   = '\0';
         }
      }
#endif

      CFRelease(bundle_path);
      CFRelease(bundle_url);
#ifndef HAVE_COCOATOUCH
      /* Not sure what this does but it breaks 
       * stuff for iOS, so skipping */
      strlcat(s, "nobin", len);
#endif
      return;
   }
#elif defined(__HAIKU__)
   while (get_next_image_info(0, &cookie, &info) == B_OK)
   {
      if (info.type == B_APP_IMAGE)
      {
         strlcpy(s, info.name, len);
         return;
      }
   }
#elif defined(__QNX__)
   char *buff = malloc(len);

   if (_cmdname(buff))
      strlcpy(s, buff, len);

   free(buff);
#else
   {
      pid_t pid;
      static const char *exts[] = { "exe", "file", "path/a.out" };
      char link_path[255];

      link_path[0] = *s = '\0';
      pid       = getpid();

      /* Linux, BSD and Solaris paths. Not standardized. */
      for (i = 0; i < ARRAY_SIZE(exts); i++)
      {
         ssize_t ret;

         snprintf(link_path, sizeof(link_path), "/proc/%u/%s",
               (unsigned)pid, exts[i]);

         if ((ret = readlink(link_path, s, len - 1)) >= 0)
         {
            s[ret] = '\0';
            return;
         }
      }
   }
#endif
}

void fill_pathname_application_dir(char *s, size_t len)
{
#ifdef __WINRT__
   strlcpy(s, uwp_dir_install, len);
#else
   fill_pathname_application_path(s, len);
   path_basedir_wrapper(s);
#endif
}

void fill_pathname_home_dir(char *s, size_t len)
{
#ifdef __WINRT__
   const char *home = uwp_dir_data;
#else
   const char *home = getenv("HOME");
#endif
   if (home)
      strlcpy(s, home, len);
   else
      *s = 0;
}
#endif

bool is_path_accessible_using_standard_io(const char *path)
{
#ifdef __WINRT__
   return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
   return true;
#endif
}
