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
#include <unistd.h>
#endif

#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
#ifdef __WINRT__
#include <uwp/uwp_func.h>
#endif
#endif

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)
#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif
#endif

/* Time format strings with AM-PM designation require special
 * handling due to platform dependence */
size_t strftime_am_pm(char *s, size_t len, const char *format,
      const void *ptr)
{
   size_t _len              = 0;
#if !(defined(__linux__) && !defined(ANDROID))
   char *local              = NULL;
#endif
   const struct tm *timeptr = (const struct tm*)ptr;
   setlocale(LC_TIME, "");
   _len = strftime(s, len, format, timeptr);
#if !(defined(__linux__) && !defined(ANDROID))
   if ((local = local_to_utf8_string_alloc(s)))
   {
      if (!string_is_empty(local))
         _len = strlcpy(s, local, len);
      free(local);
   }
#endif
   return _len;
}

/**
 * Create a new linked list with one node in it
 * The path on this node will be set to NULL
**/
struct path_linked_list *path_linked_list_new(void)
{
   struct path_linked_list *paths_list =
      (struct path_linked_list*)malloc(sizeof(*paths_list));
   if (!paths_list)
      return NULL;
   paths_list->next = NULL;
   paths_list->path = NULL;
   return paths_list;
}

/**
 * path_linked_list_free:
 *
 * Free the entire linked list
 **/
void path_linked_list_free(struct path_linked_list *in_path_llist)
{
   struct path_linked_list *node_tmp = in_path_llist;
   while (node_tmp)
   {
      struct path_linked_list *hold = node_tmp;
      if (node_tmp->path)
         free(node_tmp->path);
      node_tmp = node_tmp->next;
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
void path_linked_list_add_path(struct path_linked_list *in_path_llist,
      char *path)
{
   if (!in_path_llist->path)
      in_path_llist->path = strdup(path);
   else
   {
      struct path_linked_list *node =
         (struct path_linked_list*)malloc(sizeof(*node));
      if (node)
      {
         struct path_linked_list *head = in_path_llist;
         node->next = NULL;
         node->path = strdup(path);
         while (head->next)
            head   = head->next;
         head->next = node;
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
   const char *delim = strchr(path, '#');

   while (delim)
   {
      ptrdiff_t dist = delim - path;

      if (dist > 4)
      {
         /* Check for ".zip", ".apk", ".7z" — compare lowercased in-place */
         const char *p = delim - 4;
         /* .zip */
         if (  (p[0] == '.' )
             && (p[1] == 'z' || p[1] == 'Z')
             && (p[2] == 'i' || p[2] == 'I')
             && (p[3] == 'p' || p[3] == 'P'))
            return delim;
         /* .apk */
         if (  (p[0] == '.')
             && (p[1] == 'a' || p[1] == 'A')
             && (p[2] == 'p' || p[2] == 'P')
             && (p[3] == 'k' || p[3] == 'K'))
            return delim;
         /* .7z (last 3 chars of the 4-char window) */
         p = delim - 3;
         if (  (p[0] == '.')
             && (p[1] == '7')
             && (p[2] == 'z' || p[2] == 'Z'))
            return delim;
      }
      else if (dist > 3)
      {
         const char *p = delim - 3;
         if (  (p[0] == '.')
             && (p[1] == '7')
             && (p[2] == 'z' || p[2] == 'Z'))
            return delim;
      }

      delim = strchr(delim + 1, '#');
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
   if (   !string_is_empty(path)
       && ((ext = (char*)strrchr(path_basename(path), '.'))))
      return ext;
   return NULL;
}

/**
 * path_remove_extension:
 * @s                  : path
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
char *path_remove_extension(char *s)
{
   char *last = path_get_extension_mutable(s);
   if (!last)
      return NULL;
   if (*last)
      *last = '\0';
   return s;
}

/**
 * path_is_compressed_file:
 * @path               : path
 *
 * Checks if path is a compressed file.
 *
 * @return true if path is a compressed file, otherwise false.
 **/
bool path_is_compressed_file(const char *path)
{
   const char *ext = path_get_extension(path);
   if (!string_is_empty(ext))
   {
      switch (ext[0] | 0x20) /* lowercase first char */
      {
         case 'z':
            return string_is_equal_noncase(ext, "zip");
         case 'a':
            return string_is_equal_noncase(ext, "apk");
         case '7':
            return string_is_equal_noncase(ext, "7z");
         default:
            break;
      }
   }
   return false;
}

/**
 * fill_pathname:
 * @s                  : output path
 * @in_path            : input  path
 * @replace            : what to replace
 * @len                : buffer size of output path
 *
 * Replaces filename extension with 'replace' and outputs result to @s.
 * The extension here is considered to be the string from the last '.'
 * to the end.
 *
 * Only '.'s after the last slash are considered as extensions.
 * If no '.' is present, in_path and replace will simply be concatenated.
 * 'len' is buffer size of 's'.
 * E.g.: in_path = "/foo/bar/baz/boo.c", replace = ".asm" =>
 * s = "/foo/bar/baz/boo.asm"
 * E.g.: in_path = "/foo/bar/baz/boo.c", replace = ""     =>
 * s = "/foo/bar/baz/boo"
 *
 * @return Length of the string copied into @out
 */
size_t fill_pathname(char *s, const char *in_path,
      const char *replace, size_t len)
{
   char   *tok  = NULL;
   size_t  _len = strlcpy(s, in_path, len);
   if ((tok = (char*)strrchr(path_basename(s), '.')))
   {
      *tok = '\0';
      _len = (size_t)(tok - s);
   }
   _len += strlcpy(s + _len, replace, len - _len);
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
   const char *last = NULL;
   const char *p;
   for (p = str; *p != '\0'; p++)
      if (*p == '/' || *p == '\\')
         last = p;
   return (char*)last;
}

/**
 * fill_pathname_slash:
 * @s                  : path
 * @len                : size of @s
 *
 * Assumes path is a directory. Appends a slash
 * if not already there.
 **/
size_t fill_pathname_slash(char *s, size_t len)
{
   size_t   slen      = strlen(s);
   char    *last_slash = find_last_slash(s);

   if (!last_slash || last_slash != (s + slen - 1))
   {
      char slash = last_slash ? last_slash[0] : PATH_DEFAULT_SLASH_C();
      if (slen + 1 < len)
      {
         s[slen  ] = slash;
         s[++slen] = '\0';
      }
   }
   return slen;
}

/**
 * fill_pathname_dir:
 * @s                  : input directory path
 * @in_basename        : input basename to be appended to @s
 * @replace            : replacement to be appended to @in_basename
 * @size               : size of buffer
 *
 * Appends basename of 'in_basename', to 's', along with 'replace'.
 * Basename of in_basename is the string after the last '/' or '\\',
 * i.e the filename without directories.
 *
 * If in_basename has no '/' or '\\', the whole 'in_basename' will be used.
 * 'size' is buffer size of 's'.
 *
 * E.g..: s = "/tmp/some_dir", in_basename = "/some_content/foo.c",
 * replace = ".asm" => s = "/tmp/some_dir/foo.c.asm"
 **/
size_t fill_pathname_dir(char *s, const char *in_basename,
      const char *replace, size_t len)
{
   size_t _len  = fill_pathname_slash(s, len);
   _len        += strlcpy(s + _len, path_basename(in_basename), len - _len);
   _len        += strlcpy(s + _len, replace,                    len - _len);
   return _len;
}

/**
 * fill_pathname_base:
 * @s                  : output path
 * @in_path            : input path
 * @len                : size of output path
 *
 * Copies basename of @in_path into @s.
 *
 * @return Length of the string copied into @s
 **/
size_t fill_pathname_base(char *s, const char *in_path, size_t len)
{
   const char *ptr = path_basename(in_path);
   return strlcpy(s, ptr ? ptr : in_path, len);
}

/**
 * fill_pathname_basedir:
 * @s                  : output directory
 * @in_path            : input path
 * @len                : size of output directory
 *
 * Copies base directory of @in_path into @s.
 * If in_path is a path without any slashes (relative current directory),
 * @s will get path "./".
 *
 * @return Length of the string copied in @s
 **/
size_t fill_pathname_basedir(char *s, const char *in_path, size_t len)
{
   if (s != in_path)
      strlcpy(s, in_path, len);
   return path_basedir(s);
}

/**
 * fill_pathname_parent_dir_name:
 * @s                  : output string
 * @in_dir             : input directory
 * @len                : size of @s
 *
 * Copies only the parent directory name of @in_dir into @s.
 * The two buffers must not overlap. Removes trailing '/'.
 *
 * @return Length of the string copied into @s
 **/
size_t fill_pathname_parent_dir_name(char *s, const char *in_dir, size_t len)
{
   size_t _len      = 0;
   char  *tmp       = strdup(in_dir);
   char  *last_slash;

   if (!tmp)
      return 0;

   last_slash = find_last_slash(tmp);

   /* Strip trailing slash */
   if (last_slash && last_slash[1] == '\0')
   {
      *last_slash = '\0';
      last_slash  = find_last_slash(tmp);
   }

   /* Remove filename portion */
   if (last_slash)
      *last_slash = '\0';

   /* Find the parent component */
   in_dir = find_last_slash(tmp);
   if (!in_dir)
      in_dir = tmp;

   if (in_dir && in_dir[1])
   {
      if (path_is_absolute(in_dir) || in_dir[0] == '\\')
         _len = strlcpy(s, in_dir + 1, len);
      else
         _len = strlcpy(s, in_dir,     len);
   }

   free(tmp);
   return _len;
}

/**
 * fill_pathname_parent_dir:
 * @s                  : output directory
 * @in_dir             : input directory
 * @len                : size of @s
 *
 * Copies parent directory of @in_dir into @s.
 * Assumes @in_dir is a directory. Keeps trailing '/'.
 * If the path was already at the root directory,
 * @s will be an empty string.
 **/
size_t fill_pathname_parent_dir(char *s, const char *in_dir, size_t len)
{
   size_t _len = (s == in_dir) ? strlen(s) : strlcpy(s, in_dir, len);
   return path_parent_dir(s, _len);
}

/**
 * fill_dated_filename:
 * @s                  : output filename
 * @ext                : extension of output filename
 * @len                : buffer size of output filename
 *
 * Creates a 'dated' filename prefixed by 'retroarch', and
 * concatenates extension (@ext) to it.
 *
 * E.g.:
 * s = "retroarch-{year}{month}{day}-{Hour}{Minute}{Second}.{@ext}"
 **/
size_t fill_dated_filename(char *s, const char *ext, size_t len)
{
   size_t    _len;
   struct tm  tm_;
   time_t     cur_time = time(NULL);
   rtime_localtime(&cur_time, &tm_);
   _len  = strftime(s, len, "retroarch-%y%m%d-%H%M%S", &tm_);
   _len += strlcpy(s + _len, ext, len - _len);
   return _len;
}

/**
 * fill_str_dated_filename:
 * @s                  : output filename
 * @in_str             : input string
 * @ext                : extension of output filename
 * @len                : buffer size of output filename
 *
 * Creates a 'dated' filename prefixed by the string @in_str, and
 * concatenates extension (@ext) to it.
 *
 * E.g.:
 * s = "RetroArch-{year}{month}{day}-{Hour}{Minute}{Second}.{@ext}"
 *
 * @return Length of the string copied into @s
 **/
size_t fill_str_dated_filename(char *s,
      const char *in_str, const char *ext, size_t len)
{
   struct tm  tm_;
   size_t     _len    = 0;
   time_t     cur_time = time(NULL);
   rtime_localtime(&cur_time, &tm_);
   _len = strlcpy(s, in_str, len);
   if (string_is_empty(ext))
      _len += strftime(s + _len, len - _len, "-%y%m%d-%H%M%S", &tm_);
   else
   {
      _len += strftime(s + _len, len - _len, "-%y%m%d-%H%M%S.", &tm_);
      _len += strlcpy(s + _len, ext, len - _len);
   }
   return _len;
}

/**
 * path_basedir:
 * @s                  : path
 *
 * Extracts base directory by mutating path.
 * Keeps trailing '/'.
 *
 * @return The new size of @s
 **/
size_t path_basedir(char *s)
{
   char *last_slash = NULL;
   if (!s || s[0] == '\0' || s[1] == '\0')
      return (s && s[0] != '\0') ? 1 : 0;
   last_slash = find_last_slash(s);
   if (last_slash)
   {
      last_slash[1] = '\0';
      return (size_t)(last_slash + 1 - s);
   }
   s[0] = '.';
   s[1] = PATH_DEFAULT_SLASH_C();
   s[2] = '\0';
   return 2;
}

/**
 * path_parent_dir:
 * @s                  : path
 * @len                : length of @path
 *
 * Extracts parent directory by mutating path.
 * Assumes that @s is a directory. Keeps trailing '/'.
 * If the path was already at the root directory, returns empty string
 *
 * @return The new size of @s
 **/
size_t path_parent_dir(char *s, size_t len)
{
   if (!s)
      return 0;
   if (len && PATH_CHAR_IS_SLASH(s[len - 1]))
   {
      char *last_slash;
      bool  was_absolute = path_is_absolute(s);
      s[len - 1]         = '\0';
      last_slash         = find_last_slash(s);
      if (was_absolute && !last_slash)
      {
         s[0] = '\0';
         return 0;
      }
   }
   return path_basedir(s);
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
   const char *ptr;
   char       *last_slash = find_last_slash(path);

   /* Archive delimiter takes priority */
   if ((ptr = path_get_archive_delim(path)))
      return ptr + 1;
   return last_slash ? last_slash + 1 : path;
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
   char *last_slash = find_last_slash(path);
   return last_slash ? last_slash + 1 : path;
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
   if (!path || path[0] == '\0')
      return false;
   if (path[0] == '/')
      return true;
#if defined(_WIN32)
   return (   string_starts_with_size(path,     "\\\\", STRLEN_CONST("\\\\"))
           || string_starts_with_size(path + 1, ":/",   STRLEN_CONST(":/"))
           || string_starts_with_size(path + 1, ":\\",  STRLEN_CONST(":\\")));
#elif defined(__wiiu__) || defined(VITA)
   {
      const char *sep = strchr(path, ':');
      return (sep && sep[1] == '/');
   }
#else
   return false;
#endif
}

/**
 * path_resolve_realpath:
 * @s                  : input and output buffer for path
 * @len                : size of @s
 * @resolve_symlinks   : whether to resolve symlinks or not
 *
 * Resolves use of ".", "..", multiple slashes etc in absolute paths.
 *
 * Relative paths are rebased on the current working dir.
 *
 * @return @s if successful, NULL otherwise.
 * Note: Not implemented on consoles
 * Note: Symlinks are only resolved on Unix-likes
 * Note: The current working dir might not be what you expect,
 *       e.g. on Android it is "/"
 *       Use of fill_pathname_resolve_relative() should be preferred
 **/
char *path_resolve_realpath(char *s, size_t len, bool resolve_symlinks)
{
#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
#ifdef _WIN32
   char    *ret      = NULL;
   wchar_t *rel_path = utf8_to_utf16_string_alloc(s);
   if (rel_path)
   {
      wchar_t abs_path[PATH_MAX_LENGTH];
      if (_wfullpath(abs_path, rel_path, PATH_MAX_LENGTH))
      {
         char *tmp = utf16_to_utf8_string_alloc(abs_path);
         if (tmp)
         {
            strlcpy(s, tmp, len);
            free(tmp);
            ret = s;
         }
      }
      free(rel_path);
   }
   return ret;
#else
   char        tmp[PATH_MAX_LENGTH];
   size_t      t;
   char       *p;
   const char *next;
   const char *buf_end;

   if (resolve_symlinks)
   {
      strlcpy(tmp, s, sizeof(tmp));
      if (!realpath(tmp, s))
      {
         strlcpy(s, tmp, len);
         return NULL;
      }
      return s;
   }

   t       = 0;
   buf_end = s + strlen(s);

   if (!path_is_absolute(s))
   {
      size_t _len;
      if (!getcwd(tmp, PATH_MAX_LENGTH - 1))
         return NULL;
      _len  = strlen(tmp);
      t    += _len;
      if (tmp[_len - 1] != '/')
         tmp[t++] = '/';
      if (string_is_empty(s))
      {
         tmp[t] = '\0';
         strlcpy(s, tmp, len);
         return s;
      }
      p = s;
   }
   else
   {
      for (p = s; *p == '/'; p++)
         tmp[t++] = '/';
   }

   do
   {
      if (!(next = strchr(p, '/')))
         next = buf_end;
      if (next - p == 2 && p[0] == '.' && p[1] == '.')
      {
         p += 3;
         if (t == 1 || tmp[t - 2] == '/')
            return NULL;
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
         if (t + (size_t)(next - p) + 1 > PATH_MAX_LENGTH - 1)
            return NULL;
         while (p <= next)
            tmp[t++] = *p++;
      }
   } while (next < buf_end);

   tmp[t] = '\0';
   strlcpy(s, tmp, len);
   return s;
#endif
#endif
   return NULL;
}

/**
 * path_relative_to:
 * @s                  : buffer to write the relative path to
 * @path               : path to be expressed relatively
 * @base               : base directory to start out on
 * @len                : size of @s
 *
 * Turns @path into a path relative to @base and writes it to @s.
 *
 * @base is assumed to be a base directory, i.e. a path ending with '/' or '\'.
 * Both @path and @base are assumed to be absolute paths without "." or "..".
 *
 * E.g. path /a/b/e/f.cg with base /a/b/c/d/ turns into ../../e/f.cg
 *
 * @return Length of the string copied into @s
 **/
size_t path_relative_to(char *s,
      const char *path, const char *base, size_t len)
{
   size_t      i, j;
   size_t      out = 0;
   const char *trimmed_path;
   const char *trimmed_base;

#ifdef _WIN32
   if (   path && base
       && path[0] && path[1] && base[0] && base[1]
       && path[1] == ':' && base[1] == ':'
       && path[0] != base[0])
      return strlcpy(s, path, len);
#endif

   for (i = 0, j = 0; path[i] && base[i] && path[i] == base[i]; i++)
      if (path[i] == PATH_DEFAULT_SLASH_C())
         j = i + 1;

   trimmed_path = path + j;
   trimmed_base = base + i;

   /* Write "../" for each remaining segment in base — single pass */
   s[0] = '\0';
   for (i = 0; trimmed_base[i]; i++)
   {
      if (trimmed_base[i] == PATH_DEFAULT_SLASH_C())
      {
         if (out + 3 < len)
         {
            s[out++] = '.';
            s[out++] = '.';
            s[out++] = PATH_DEFAULT_SLASH_C();
         }
      }
   }
   s[out] = '\0';

   return out + strlcpy(s + out, trimmed_path, len - out);
}

/**
 * fill_pathname_resolve_relative:
 * @s                  : output path
 * @in_refpath         : input reference path
 * @in_path            : input path
 * @len                : size of @s
 *
 * Joins basedir of @in_refpath together with @in_path.
 * If @in_path is an absolute path, s = in_path.
 * E.g.: in_refpath = "/foo/bar/baz.a", in_path = "foobar.cg",
 * s = "/foo/bar/foobar.cg".
 **/
void fill_pathname_resolve_relative(char *s,
      const char *in_refpath, const char *in_path, size_t len)
{
   size_t _len;
   if (path_is_absolute(in_path))
   {
      strlcpy(s, in_path, len);
      return;
   }
   _len = fill_pathname_basedir(s, in_refpath, len);
   strlcpy(s + _len, in_path, len - _len);
   path_resolve_realpath(s, len, false);
}

/**
 * fill_pathname_join:
 * @s                  : output path
 * @dir                : directory
 * @path               : path
 * @len                : size of output path
 *
 * Joins a directory (@dir) and path (@path) together.
 * Makes sure not to get  two consecutive slashes
 * between directory and path.
 *
 * Deprecated. Use fill_pathname_join_special() instead
 * if you can ensure @dir and @s won't overlap.
 *
 * @return Length of the string copied into @s
 **/
size_t fill_pathname_join(char *s, const char *dir,
      const char *path, size_t len)
{
   size_t _len = 0;
   if (s != dir)
      _len = strlcpy(s, dir, len);
   if (*s)
      _len = fill_pathname_slash(s, len);
   _len += strlcpy(s + _len, path, len - _len);
   return _len;
}

/**
 * fill_pathname_join_special:
 * @s                  : output path
 * @dir                : directory. Cannot be identical to @s
 * @path               : path
 * @len                : size of @s
 *
 * Specialized version of fill_pathname_join.
 * Unlike fill_pathname_join(),
 * @dir and @s CANNOT be identical.
 *
 * Joins a directory (@dir) and path (@path) together.
 * Makes sure not to get  two consecutive slashes
 * between directory and path.
 *
 * @return Length of the string copied into @s
 **/
size_t fill_pathname_join_special(char *s,
      const char *dir, const char *path, size_t len)
{
   size_t _len = strlcpy(s, dir, len);
   if (*s)
   {
      char *last_slash = find_last_slash(s);
      char  slash      = last_slash ? last_slash[0] : PATH_DEFAULT_SLASH_C();
      if (!last_slash || last_slash != (s + _len - 1))
      {
         if (_len + 1 < len)
         {
            s[  _len] = slash;
            s[++_len] = '\0';
         }
      }
   }
   _len += strlcpy(s + _len, path, len - _len);
   return _len;
}

size_t fill_pathname_join_special_ext(char *s,
      const char *dir,  const char *path,
      const char *last, const char *ext,
      size_t len)
{
   size_t _len = fill_pathname_join(s, dir, path, len);
   if (*s)
      _len     = fill_pathname_slash(s, len);
   _len       += strlcpy(s + _len, last, len - _len);
   _len       += strlcpy(s + _len, ext,  len - _len);
   return _len;
}

/**
 * fill_pathname_join_delim:
 * @s                  : output path
 * @dir                : directory
 * @path               : path
 * @delim              : delimiter
 * @len                : size of output path
 *
 * Joins a directory (@dir) and path (@path) together
 * using the given delimiter (@delim).
 **/
size_t fill_pathname_join_delim(char *s, const char *dir,
      const char *path, const char delim, size_t len)
{
   size_t _len = (s == dir) ? strlen(dir) : strlcpy(s, dir, len);
   if (len - _len < 2)
      return _len;
   s[_len++] = delim;
   s[_len  ] = '\0';
   if (path)
      _len += strlcpy(s + _len, path, len - _len);
   return _len;
}

size_t fill_pathname_expand_special(char *s, const char *in_path, size_t len)
{
#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
   char *app_dir = NULL;
   if (in_path[0] == '~')
   {
      app_dir    = (char*)malloc(DIR_MAX_LENGTH * sizeof(char));
      fill_pathname_home_dir(app_dir, DIR_MAX_LENGTH * sizeof(char));
   }
   else if (in_path[0] == ':')
   {
      app_dir    = (char*)malloc(DIR_MAX_LENGTH * sizeof(char));
      app_dir[0] = '\0';
      fill_pathname_application_dir(app_dir, DIR_MAX_LENGTH * sizeof(char));
   }
   if (app_dir)
   {
      if (*app_dir)
      {
         size_t _len = strlcpy(s, app_dir, len);
         s          += _len;
         len        -= _len;
         if (!PATH_CHAR_IS_SLASH(s[-1]))
         {
            _len  = strlcpy(s, PATH_DEFAULT_SLASH(), len);
            s    += _len;
            len  -= _len;
         }
         in_path += 2;
      }
      free(app_dir);
   }
#endif
   return strlcpy(s, in_path, len);
}

size_t fill_pathname_abbreviate_special(char *s,
      const char *in_path, size_t len)
{
#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
   unsigned    i;
   const char *candidates[3];
   const char *notations[3];
   char        application_dir[DIR_MAX_LENGTH];
   char        home_dir[DIR_MAX_LENGTH];

   application_dir[0] = '\0';

   candidates[0] = application_dir;
   candidates[1] = home_dir;
   candidates[2] = NULL;

   notations[0]  = ":";
   notations[1]  = "~";
   notations[2]  = NULL;

   fill_pathname_application_dir(application_dir, sizeof(application_dir));
   fill_pathname_home_dir(home_dir, sizeof(home_dir));

   for (i = 0; candidates[i]; i++)
   {
      if (   !string_is_empty(candidates[i])
          &&  string_starts_with(in_path, candidates[i]))
      {
         size_t _len = strlcpy(s, notations[i], len);
         s          += _len;
         len        -= _len;
         in_path    += strlen(candidates[i]);
         if (!PATH_CHAR_IS_SLASH(*in_path))
         {
            strlcpy(s, PATH_DEFAULT_SLASH(), len);
            s++;
            len--;
         }
         break;
      }
   }
#endif
   return strlcpy(s, in_path, len);
}

/**
 * sanitize_path_part:
 *
 * @path_part               : directory or filename
 *
 * Takes single part of a path eg. single filename
 * or directory, and removes any special chars that are
 * unavailable.
 *
 * @returns new string that has been sanitized
 **/
const char *sanitize_path_part(const char *path_part, size_t len)
{
   /* Lookup table: 1 = forbidden character */
   /* '<' = 60, '>' = 62, ':' = 58, '"' = 34, '/' = 47,
      '\' = 92, '|' = 124, '?' = 63, '*' = 42            */
   static unsigned char bad[256];
   static int bad_initialized = 0;
   size_t i, j = 0;
   char  *tmp;
   if (!bad_initialized)
   {
      bad[(unsigned char)'"']  = 1;
      bad[(unsigned char)'*']  = 1;
      bad[(unsigned char)'/']  = 1;
      bad[(unsigned char)':']  = 1;
      bad[(unsigned char)'<']  = 1;
      bad[(unsigned char)'>']  = 1;
      bad[(unsigned char)'?']  = 1;
      bad[(unsigned char)'\\'] = 1;
      bad[(unsigned char)'|']  = 1;
      bad_initialized          = 1;
   }

   if (string_is_empty(path_part))
      return NULL;

   tmp = (char*)malloc((len + 1) * sizeof(char));
   if (!tmp)
      return NULL;

   for (i = 0; i < len && path_part[i] != '\0'; i++)
      if (!bad[(unsigned char)path_part[i]])
         tmp[j++] = path_part[i];

   tmp[j] = '\0';
   return tmp;
}

/**
 * pathname_conform_slashes_to_os:
 *
 * @s                  : path
 *
 * Leaf function.
 *
 * Changes the slashes to the correct kind for the OS
 * So forward slash on linux and backslash on Windows
 **/
void pathname_conform_slashes_to_os(char *s)
{
   char *p;
   for (p = s; *p; p++)
      if (*p == '/' || *p == '\\')
         *p = PATH_DEFAULT_SLASH_C();
}

/**
 * pathname_make_slashes_portable:
 * @s                  : path
 *
 * Leaf function.
 *
 * Change all slashes to forward so they are more
 * portable between Windows and Linux
 **/
void pathname_make_slashes_portable(char *s)
{
   char *p;
   for (p = s; *p; p++)
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
   int         num = 0;
   const char *p   = in_path;
   for (; *p; p++)
      if (PATH_CHAR_IS_SLASH(*p))
         num++;
   return num;
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
 * @return Length of the string copied into @s
 **/
size_t fill_pathname_abbreviated_or_relative(char *s,
      const char *in_refpath, const char *in_path, size_t len)
{
   size_t _len;
   char in_path_conformed   [PATH_MAX_LENGTH];
   char in_refpath_conformed[PATH_MAX_LENGTH];
   char absolute_path       [PATH_MAX_LENGTH];
   char relative_path       [PATH_MAX_LENGTH];

   absolute_path[0] = '\0';
   relative_path[0] = '\0';

   strlcpy(in_path_conformed,    in_path,    sizeof(in_path_conformed));
   strlcpy(in_refpath_conformed, in_refpath, sizeof(in_refpath_conformed));

   pathname_conform_slashes_to_os(in_path_conformed);
   pathname_conform_slashes_to_os(in_refpath_conformed);

   fill_pathname_expand_special(absolute_path,
         in_path_conformed, sizeof(absolute_path));

   if (!path_is_absolute(absolute_path))
      fill_pathname_resolve_relative(absolute_path,
            in_refpath_conformed, in_path_conformed,
            sizeof(absolute_path));
   pathname_conform_slashes_to_os(absolute_path);

   path_relative_to(relative_path, absolute_path,
         in_refpath_conformed, sizeof(relative_path));

   _len = fill_pathname_abbreviate_special(s, absolute_path, len);

   if (get_pathname_num_slashes(relative_path) <= get_pathname_num_slashes(s))
      return strlcpy(s, relative_path, len);
   return _len;
}

/**
 * path_basedir:
 * @s                  : path
 *
 * Extracts base directory by mutating path.
 * Keeps trailing '/'.
 **/
void path_basedir_wrapper(char *s)
{
   char *last_slash = NULL;
   if (!s || s[0] == '\0' || s[1] == '\0')
      return;
#ifdef HAVE_COMPRESSION
   if ((last_slash = (char*)path_get_archive_delim(s)))
      *last_slash = '\0';
#endif
   last_slash = find_last_slash(s);
   if (!last_slash)
   {
      s[0] = '.';
      s[1] = PATH_DEFAULT_SLASH_C();
      s[2] = '\0';
   }
   else
      last_slash[1] = '\0';
}

#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
size_t fill_pathname_application_path(char *s, size_t len)
{
   if (len)
   {
#if defined(_WIN32)
#ifdef LEGACY_WIN32
      DWORD ret = GetModuleFileNameA(NULL, s, len);
#else
      wchar_t wstr[PATH_MAX_LENGTH] = {0};
      DWORD   ret = GetModuleFileNameW(NULL, wstr, ARRAY_SIZE(wstr));
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
      return ret;
#elif defined(__APPLE__)
      CFBundleRef bundle = CFBundleGetMainBundle();
      if (bundle)
      {
         size_t     rv          = 0;
         CFURLRef   bundle_url  = CFBundleCopyBundleURL(bundle);
         CFStringRef bundle_path = CFURLCopyPath(bundle_url);
         CFStringGetCString(bundle_path, s, len, kCFStringEncodingUTF8);
#ifdef HAVE_COCOATOUCH
         {
            char resolved_bundle_dir_buf[DIR_MAX_LENGTH] = {0};
            if (realpath(s, resolved_bundle_dir_buf))
            {
               size_t _len = strlcpy(s, resolved_bundle_dir_buf, len - 1);
               s[  _len]   = '/';
               s[++_len]   = '\0';
               rv          = _len;
            }
         }
#else
         rv = CFStringGetLength(bundle_path);
#endif
         CFRelease(bundle_path);
         CFRelease(bundle_url);
         return rv;
      }
#elif defined(__HAIKU__)
      image_info info;
      int32_t    cookie = 0;
      while (get_next_image_info(0, &cookie, &info) == B_OK)
         if (info.type == B_APP_IMAGE)
            return strlcpy(s, info.name, len);
#elif defined(__QNX__)
      char   *buff  = (char*)malloc(len);
      size_t  _len  = 0;
      if (_cmdname(buff))
         _len = strlcpy(s, buff, len);
      free(buff);
      return _len;
#else
      size_t            i;
      static const char *exts[] = { "exe", "file", "path/a.out" };
      char              link_path[255];
      pid_t             pid  = getpid();
      size_t            _len = snprintf(link_path, sizeof(link_path),
                                 "/proc/%u/", (unsigned)pid);
      *s = '\0';
      for (i = 0; i < ARRAY_SIZE(exts); i++)
      {
         ssize_t ret;
         strlcpy(link_path + _len, exts[i], sizeof(link_path) - _len);
         if ((ret = readlink(link_path, s, len - 1)) >= 0)
         {
            s[ret] = '\0';
            return (size_t)ret;
         }
      }
#endif
   }
   return 0;
}

size_t fill_pathname_application_dir(char *s, size_t len)
{
#ifdef __WINRT__
   return strlcpy(s, uwp_dir_install, len);
#else
   fill_pathname_application_path(s, len);
   return path_basedir(s);
#endif
}

size_t fill_pathname_home_dir(char *s, size_t len)
{
#ifdef __WINRT__
   const char *home = uwp_dir_data;
#else
   const char *home = getenv("HOME");
#endif
   if (home)
      return strlcpy(s, home, len);
   *s = '\0';
   return 0;
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
