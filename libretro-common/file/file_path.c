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
#include <errno.h>

#include <sys/stat.h>

#include <boolean.h>
#include <file/file_path.h>
#include <retro_assert.h>
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

/**
 * path_get_archive_delim:
 * @path               : path
 *
 * Find delimiter of an archive file. Only the first '#'
 * after a compression extension is considered.
 *
 * Returns: pointer to the delimiter in the path if it contains
 * a path inside a compressed file, otherwise NULL.
 */
const char *path_get_archive_delim(const char *path)
{
   const char *last_slash = find_last_slash(path);
   const char *delim      = NULL;
   char buf[5];

   buf[0] = '\0';

   /* We search for delimiters after the last slash
    * in the file path to avoid capturing delimiter
    * characters in any parent directory names.
    * If there are no slashes in the file name, then
    * the path is just the file basename - in this
    * case we search the path in its entirety */
   if (!last_slash)
      last_slash = path;

   /* Find delimiter position
    * > Since filenames may contain '#' characters,
    *   must loop until we find the first '#' that
    *   is directly *after* a compression extension */
   delim = strchr(last_slash, '#');

   while (delim)
   {
      /* Check whether this is a known archive type
       * > Note: The code duplication here is
       *   deliberate, to maximise performance */
      if (delim - last_slash > 4)
      {
         strlcpy(buf, delim - 4, sizeof(buf));
         buf[4] = '\0';

         string_to_lower(buf);

         /* Check if this is a '.zip', '.apk' or '.7z' file */
         if (string_is_equal(buf,     ".zip") ||
             string_is_equal(buf,     ".apk") ||
             string_is_equal(buf + 1, ".7z"))
            return delim;
      }
      else if (delim - last_slash > 3)
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
 * Returns: extension part from the path.
 */
const char *path_get_extension(const char *path)
{
   const char *ext;
   if (!string_is_empty(path) && ((ext = strrchr(path_basename(path), '.'))))
      return ext + 1;
   return "";
}

/**
 * path_remove_extension:
 * @path               : path
 *
 * Mutates path by removing its extension. Removes all
 * text after and including the last '.'.
 * Only '.'s after the last slash are considered.
 *
 * Returns:
 * 1) If path has an extension, returns path with the
 *    extension removed.
 * 2) If there is no extension, returns NULL.
 * 3) If path is empty or NULL, returns NULL
 */
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
 * Returns: true (1) if path is a compressed file, otherwise false (0).
 **/
bool path_is_compressed_file(const char* path)
{
   const char *ext = path_get_extension(path);
   if (!string_is_empty(ext))
      if (  string_is_equal_noncase(ext, "zip") ||
            string_is_equal_noncase(ext, "apk") ||
            string_is_equal_noncase(ext, "7z"))
         return true;
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
 */
void fill_pathname(char *out_path, const char *in_path,
      const char *replace, size_t size)
{
   char tmp_path[PATH_MAX_LENGTH];
   char *tok                      = NULL;

   tmp_path[0] = '\0';

   strlcpy(tmp_path, in_path, sizeof(tmp_path));
   if ((tok = (char*)strrchr(path_basename(tmp_path), '.')))
      *tok = '\0';

   fill_pathname_noext(out_path, tmp_path, replace, size);
}

/**
 * fill_pathname_noext:
 * @out_path           : output path
 * @in_path            : input  path
 * @replace            : what to replace
 * @size               : buffer size of output path
 *
 * Appends a filename extension 'replace' to 'in_path', and outputs
 * result in 'out_path'.
 *
 * Assumes in_path has no extension. If an extension is still
 * present in 'in_path', it will be ignored.
 *
 */
size_t fill_pathname_noext(char *out_path, const char *in_path,
      const char *replace, size_t size)
{
   strlcpy(out_path, in_path, size);
   return strlcat(out_path, replace, size);
}

char *find_last_slash(const char *str)
{
   const char *slash     = strrchr(str, '/');
#ifdef _WIN32
   const char *backslash = strrchr(str, '\\');

   if (!slash || (backslash > slash))
      return (char*)backslash;
#endif
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
void fill_pathname_slash(char *path, size_t size)
{
   size_t path_len;
   const char *last_slash = find_last_slash(path);

   if (!last_slash)
   {
      strlcat(path, PATH_DEFAULT_SLASH(), size);
      return;
   }

   path_len               = strlen(path);
   /* Try to preserve slash type. */
   if (last_slash != (path + path_len - 1))
   {
      path[path_len]   = last_slash[0];
      path[path_len+1] = '\0';
   }
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
   const char *base = NULL;

   fill_pathname_slash(in_dir, size);
   base = path_basename(in_basename);
   strlcat(in_dir, base, size);
   return strlcat(in_dir, replace, size);
}

/**
 * fill_pathname_base:
 * @out                : output path
 * @in_path            : input path
 * @size               : size of output path
 *
 * Copies basename of @in_path into @out_path.
 **/
size_t fill_pathname_base(char *out, const char *in_path, size_t size)
{
   const char     *ptr = path_basename(in_path);

   if (!ptr)
      ptr = in_path;

   return strlcpy(out, ptr, size);
}

void fill_pathname_base_noext(char *out,
      const char *in_path, size_t size)
{
   fill_pathname_base(out, in_path, size);
   path_remove_extension(out);
}

size_t fill_pathname_base_ext(char *out,
      const char *in_path, const char *ext,
      size_t size)
{
   fill_pathname_base_noext(out, in_path, size);
   return strlcat(out, ext, size);
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

void fill_pathname_basedir_noext(char *out_dir,
      const char *in_path, size_t size)
{
   fill_pathname_basedir(out_dir, in_path, size);
   path_remove_extension(out_dir);
}

/**
 * fill_pathname_parent_dir_name:
 * @out_dir            : output directory
 * @in_dir             : input directory
 * @size               : size of output directory
 *
 * Copies only the parent directory name of @in_dir into @out_dir.
 * The two buffers must not overlap. Removes trailing '/'.
 * Returns true on success, false if a slash was not found in the path.
 **/
bool fill_pathname_parent_dir_name(char *out_dir,
      const char *in_dir, size_t size)
{
   bool success = false;
   char *temp   = strdup(in_dir);
   char *last   = find_last_slash(temp);

   if (last && last[1] == 0)
   {
      *last     = '\0';
      last      = find_last_slash(temp);
   }

   if (last)
      *last     = '\0';

   in_dir       = find_last_slash(temp);

   success      = in_dir && in_dir[1];

   if (success)
      strlcpy(out_dir, in_dir + 1, size);

   free(temp);
   return success;
}

/**
 * fill_pathname_parent_dir:
 * @out_dir            : output directory
 * @in_dir             : input directory
 * @size               : size of output directory
 *
 * Copies parent directory of @in_dir into @out_dir.
 * Assumes @in_dir is a directory. Keeps trailing '/'.
 * If the path was already at the root directory, @out_dir will be an empty string.
 **/
void fill_pathname_parent_dir(char *out_dir,
      const char *in_dir, size_t size)
{
   if (out_dir != in_dir)
      strlcpy(out_dir, in_dir, size);
   path_parent_dir(out_dir);
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
   time_t cur_time = time(NULL);
   struct tm tm_;

   rtime_localtime(&cur_time, &tm_);

   strftime(out_filename, size,
         "RetroArch-%m%d-%H%M%S", &tm_);
   return strlcat(out_filename, ext, size);
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
 **/
void fill_str_dated_filename(char *out_filename,
      const char *in_str, const char *ext, size_t size)
{
   char format[256];
   struct tm tm_;
   time_t cur_time = time(NULL);

   format[0]       = '\0';

   rtime_localtime(&cur_time, &tm_);

   if (string_is_empty(ext))
   {
      strftime(format, sizeof(format), "-%y%m%d-%H%M%S", &tm_);
      fill_pathname_noext(out_filename, in_str, format, size);
   }
   else
   {
      strftime(format, sizeof(format), "-%y%m%d-%H%M%S.", &tm_);

      fill_pathname_join_concat_noext(out_filename,
            in_str, format, ext,
            size);
   }
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

   if (strlen(path) < 2)
      return;

   last = find_last_slash(path);

   if (last)
      last[1] = '\0';
   else
      strlcpy(path, "." PATH_DEFAULT_SLASH(), 3);
}

/**
 * path_parent_dir:
 * @path               : path
 *
 * Extracts parent directory by mutating path.
 * Assumes that path is a directory. Keeps trailing '/'.
 * If the path was already at the root directory, returns empty string
 **/
void path_parent_dir(char *path)
{
   size_t len = 0;

   if (!path)
      return;
   
   len = strlen(path);

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
 * Returns: basename from path.
 **/
const char *path_basename(const char *path)
{
   /* We cut at the first compression-related hash */
   const char *delim = path_get_archive_delim(path);
   if (delim)
      return delim + 1;

   {
      /* We cut at the last slash */
      const char *last  = find_last_slash(path);
      if (last)
         return last + 1;
   }

   return path;
}

/* Specialized version */
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
 * Returns: true if path is absolute, false if path is relative.
 **/
bool path_is_absolute(const char *path)
{
   if (string_is_empty(path))
      return false;

   if (path[0] == '/')
      return true;

#if defined(_WIN32)
   /* Many roads lead to Rome...
    * Note: Drive letter can only be 1 character long */
   if (string_starts_with_size(path,     "\\\\", STRLEN_CONST("\\\\")) ||
       string_starts_with_size(path + 1, ":/",   STRLEN_CONST(":/"))   ||
       string_starts_with_size(path + 1, ":\\",  STRLEN_CONST(":\\")))
      return true;
#elif defined(__wiiu__) || defined(VITA)
   {
      const char *seperator = strchr(path, ':');
      if (seperator && (seperator[1] == '/'))
         return true;
   }
#endif

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
 * Returns: @buf if successful, NULL otherwise.
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
   char *ret = NULL;
   wchar_t abs_path[PATH_MAX_LENGTH];
   wchar_t *rel_path = utf8_to_utf16_string_alloc(buf);

   if (rel_path)
   {
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

   t = 0; /* length of output */
   buf_end = buf + strlen(buf);

   if (!path_is_absolute(buf))
   {
      size_t len;
      /* rebase on working directory */
      if (!getcwd(tmp, PATH_MAX_LENGTH-1))
         return NULL;

      len = strlen(tmp);
      t += len;

      if (tmp[len-1] != '/')
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
      next = strchr(p, '/');
      if (!next)
         next = buf_end;

      if ((next - p == 2 && p[0] == '.' && p[1] == '.'))
      {
         p += 3;

         /* fail for illegal /.., //.. etc */
         if (t == 1 || tmp[t-2] == '/')
            return NULL;

         /* delete previous segment in tmp by adjusting size t
          * tmp[t-1] == '/', find '/' before that */
         t = t-2;
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
         if (t + next-p+1 > PATH_MAX_LENGTH-1)
            return NULL;

         while (p <= next)
            tmp[t++] = *p++;
      }

   }
   while (next < buf_end);

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
 **/
size_t path_relative_to(char *out,
      const char *path, const char *base, size_t size)
{
   size_t i, j;
   const char *trimmed_path, *trimmed_base;

#ifdef _WIN32
   /* For different drives, return absolute path */
   if (strlen(path) >= 2 && strlen(base) >= 2
         && path[1] == ':' && base[1] == ':'
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

   fill_pathname_basedir(out_path, in_refpath, size);
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
 **/
size_t fill_pathname_join(char *out_path,
      const char *dir, const char *path, size_t size)
{
   if (out_path != dir)
      strlcpy(out_path, dir, size);

   if (*out_path)
      fill_pathname_slash(out_path, size);

   return strlcat(out_path, path, size);
}

size_t fill_pathname_join_special_ext(char *out_path,
      const char *dir,  const char *path,
      const char *last, const char *ext,
      size_t size)
{
   fill_pathname_join(out_path, dir, path, size);
   if (*out_path)
      fill_pathname_slash(out_path, size);

   strlcat(out_path, last, size);
   return strlcat(out_path, ext, size);
}

size_t fill_pathname_join_concat_noext(char *out_path,
      const char *dir, const char *path,
      const char *concat,
      size_t size)
{
   fill_pathname_noext(out_path, dir, path, size);
   return strlcat(out_path, concat, size);
}

size_t fill_pathname_join_concat(char *out_path,
      const char *dir, const char *path,
      const char *concat,
      size_t size)
{
   fill_pathname_join(out_path, dir, path, size);
   return strlcat(out_path, concat, size);
}

void fill_pathname_join_noext(char *out_path,
      const char *dir, const char *path, size_t size)
{
   fill_pathname_join(out_path, dir, path, size);
   path_remove_extension(out_path);
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
   size_t copied;
   /* behavior of strlcpy is undefined if dst and src overlap */
   if (out_path == dir)
      copied = strlen(dir);
   else
      copied = strlcpy(out_path, dir, size);

   out_path[copied]   = delim;
   out_path[copied+1] = '\0';

   if (path)
      copied = strlcat(out_path, path, size);
   return copied;
}

size_t fill_pathname_join_delim_concat(char *out_path, const char *dir,
      const char *path, const char delim, const char *concat,
      size_t size)
{
   fill_pathname_join_delim(out_path, dir, path, delim, size);
   return strlcat(out_path, concat, size);
}

/**
 * fill_short_pathname_representation:
 * @out_rep            : output representation
 * @in_path            : input path
 * @size               : size of output representation
 *
 * Generates a short representation of path. It should only
 * be used for displaying the result; the output representation is not
 * binding in any meaningful way (for a normal path, this is the same as basename)
 * In case of more complex URLs, this should cut everything except for
 * the main image file.
 *
 * E.g.: "/path/to/game.img" -> game.img
 *       "/path/to/myarchive.7z#folder/to/game.img" -> game.img
 */
size_t fill_short_pathname_representation(char* out_rep,
      const char *in_path, size_t size)
{
   char path_short[PATH_MAX_LENGTH];

   path_short[0] = '\0';

   fill_pathname(path_short, path_basename(in_path), "",
            sizeof(path_short));

   return strlcpy(out_rep, path_short, size);
}

void fill_short_pathname_representation_noext(char* out_rep,
      const char *in_path, size_t size)
{
   fill_short_pathname_representation(out_rep, in_path, size);
   path_remove_extension(out_rep);
}

void fill_pathname_expand_special(char *out_path,
      const char *in_path, size_t size)
{
#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
   if (in_path[0] == '~')
   {
      char *home_dir = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      home_dir[0] = '\0';

      fill_pathname_home_dir(home_dir,
         PATH_MAX_LENGTH * sizeof(char));

      if (*home_dir)
      {
         size_t src_size = strlcpy(out_path, home_dir, size);
         retro_assert(src_size < size);

         out_path  += src_size;
         size      -= src_size;

         if (!PATH_CHAR_IS_SLASH(out_path[-1]))
         {
            src_size = strlcpy(out_path, PATH_DEFAULT_SLASH(), size);
            retro_assert(src_size < size);

            out_path += src_size;
            size     -= src_size;
         }

         in_path += 2;
      }

      free(home_dir);
   }
   else if (in_path[0] == ':')
   {
      char *application_dir = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      application_dir[0]    = '\0';

      fill_pathname_application_dir(application_dir,
            PATH_MAX_LENGTH * sizeof(char));

      if (*application_dir)
      {
         size_t src_size   = strlcpy(out_path, application_dir, size);
         retro_assert(src_size < size);

         out_path  += src_size;
         size      -= src_size;

         if (!PATH_CHAR_IS_SLASH(out_path[-1]))
         {
            src_size = strlcpy(out_path, PATH_DEFAULT_SLASH(), size);
            retro_assert(src_size < size);

            out_path += src_size;
            size     -= src_size;
         }

         in_path += 2;
      }

      free(application_dir);
   }
#endif

   retro_assert(strlcpy(out_path, in_path, size) < size);
}

void fill_pathname_abbreviate_special(char *out_path,
      const char *in_path, size_t size)
{
#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
   unsigned i;
   const char *candidates[3];
   const char *notations[3];
   char application_dir[PATH_MAX_LENGTH];
   char home_dir[PATH_MAX_LENGTH];

   application_dir[0] = '\0';
   home_dir[0]        = '\0';

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

         retro_assert(src_size < size);

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

   retro_assert(strlcpy(out_path, in_path, size) < size);
}

/* Changes the slashes to the correct kind for the os 
 * So forward slash on linux and backslash on Windows */
void pathname_conform_slashes_to_os(char *path)
{
   /* Conform slashes to os standard so we get proper matching */
   char* p;
   for (p = path; *p; p++)
      if (*p == '/' || *p == '\\')
         *p = PATH_DEFAULT_SLASH_C();
}

/* Change all shashes to forward so they are more portable between windows and linux */
void pathname_make_slashes_portable(char *path)
{
   /* Conform slashes to os standard so we get proper matching */
   char* p;
   for (p = path; *p; p++)
      if (*p == '/' || *p == '\\')
         *p = '/';
}

/* Get the number of slashes in a path, returns an integer */
int get_pathname_num_slashes(const char *in_path)
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

/* Fills the supplied path with either the abbreviated path or the relative path, which ever
 * one is has less depth / number of slashes
 * If lengths of abbreviated and relative paths are the same the relative path will be used
 * in_path can be an absolute, relative or abbreviated path */
void fill_pathname_abbreviated_or_relative(char *out_path, const char *in_refpath, const char *in_path, size_t size)
{
   char in_path_conformed[PATH_MAX_LENGTH];
   char in_refpath_conformed[PATH_MAX_LENGTH];
   char expanded_path[PATH_MAX_LENGTH];
   char absolute_path[PATH_MAX_LENGTH];
   char relative_path[PATH_MAX_LENGTH];
   char abbreviated_path[PATH_MAX_LENGTH];
   
   in_path_conformed[0]    = '\0';
   in_refpath_conformed[0] = '\0';
   expanded_path[0]        = '\0';
   absolute_path[0]        = '\0';
   relative_path[0]        = '\0';
   abbreviated_path[0]     = '\0';

   strcpy_literal(in_path_conformed, in_path);
   strcpy_literal(in_refpath_conformed, in_refpath);

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
      retro_assert(strlcpy(out_path, relative_path, size) < size);
   else
      retro_assert(strlcpy(out_path, abbreviated_path, size) < size);
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
   if (strlen(path) < 2)
      return;

#ifdef HAVE_COMPRESSION
   /* We want to find the directory with the archive in basedir. */
   last = (char*)path_get_archive_delim(path);
   if (last)
      *last = '\0';
#endif

   last = find_last_slash(path);

   if (last)
      last[1] = '\0';
   else
      strlcpy(path, "." PATH_DEFAULT_SLASH(), 3);
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
            strlcpy(s, resolved_bundle_dir_buf, len - 1);
            strlcat(s, "/", len);
         }
      }
#endif

      CFRelease(bundle_path);
      CFRelease(bundle_url);
#ifndef HAVE_COCOATOUCH
      /* Not sure what this does but it breaks 
       * stuff for iOS, so skipping */
      retro_assert(strlcat(s, "nobin", len) < len);
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
         ret = readlink(link_path, s, len - 1);

         if (ret >= 0)
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
   char relative_path_abbrev[PATH_MAX_LENGTH];
   fill_pathname_abbreviate_special(relative_path_abbrev,
         path, sizeof(relative_path_abbrev));
   return (strlen(relative_path_abbrev) >= 2 )
      &&  (    relative_path_abbrev[0] == ':'
            || relative_path_abbrev[0] == '~')
      && PATH_CHAR_IS_SLASH(relative_path_abbrev[1]);
#else
   return true;
#endif
}
