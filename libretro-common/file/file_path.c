/* Copyright  (C) 2010-2015 The RetroArch team
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


#include <stdlib.h>
#include <boolean.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef __HAIKU__
#include <kernel/image.h>
#endif

#include <sys/stat.h>

#include <file/file_path.h>

#ifndef __MACH__
#include <compat/strl.h>
#include <compat/posix_string.h>
#endif
#include <retro_assert.h>
#include <retro_stat.h>
#include <retro_miscellaneous.h>

/**
 * path_mkdir:
 * @dir                : directory
 *
 * Create directory on filesystem.
 *
 * Returns: true (1) if directory could be created, otherwise false (0).
 **/
bool path_mkdir(const char *dir)
{
   const char *target = NULL;
   /* Use heap. Real chance of stack overflow if we recurse too hard. */
   char     *basedir = strdup(dir);
   bool          ret = false;

   if (!basedir)
      return false;

   path_parent_dir(basedir);
   if (!*basedir || !strcmp(basedir, dir))
      goto end;

   if (path_is_directory(basedir))
   {
      target = dir;
      ret    = mkdir_norecurse(dir);
   }
   else
   {
      target = basedir;
      ret    = path_mkdir(basedir);

      if (ret)
      {
         target = dir;
         ret    = mkdir_norecurse(dir);
      }
   }

end:
   if (target && !ret)
      printf("Failed to create directory: \"%s\".\n", target);
   free(basedir);
   return ret;
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
   const char *ext = strrchr(path_basename(path), '.');
   if (!ext)
      return "";
   return ext + 1;
}

/**
 * path_remove_extension:
 * @path               : path
 *
 * Removes the extension from the path and returns the result.
 * Removes all text after and including the last '.'.
 * Only '.'s after the last slash are considered.
 *
 * Returns: path with the extension part removed.
 */
char *path_remove_extension(char *path)
{
   char *last = (char*)strrchr(path_basename(path), '.');
   if (!last)
      return NULL;
   if (*last)
      *last = '\0';
   return last;
}

/**
 * path_contains_compressed_file:
 * @path               : path
 *
 * Checks if path contains a compressed file.
 *
 * Currently we only check for hash symbol (#) inside the pathname.
 * If path is ever expanded to a general URI, we should check for that here.
 *
 * Example:  Somewhere in the path there might be a compressed file
 * E.g.: /path/to/file.7z#mygame.img
 *
 * Returns: true (1) if path contains compressed file, otherwise false (0).
 **/
bool path_contains_compressed_file(const char *path)
{
   return (strchr(path,'#') != NULL);
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
#if defined(HAVE_COMPRESSION) && (defined(HAVE_ZLIB) || defined(HAVE_7ZIP))
   const char* file_ext   = path_get_extension(path);

#ifdef HAVE_ZLIB
   if (!strcmp(file_ext, "zip"))
      return true;
#endif

#ifdef HAVE_7ZIP
   if (!strcmp(file_ext, "7z"))
      return true;
#endif

#endif
   return false;
}

/**
 * path_file_exists:
 * @path               : path
 *
 * Checks if a file already exists at the specified path (@path).
 *
 * Returns: true (1) if file already exists, otherwise false (0).
 */
bool path_file_exists(const char *path)
{
   FILE *dummy = fopen(path, "rb");

   if (!dummy)
      return false;

   fclose(dummy);
   return true;
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
   char tmp_path[PATH_MAX_LENGTH] = {0};
   char *tok                      = NULL;

   rarch_assert(strlcpy(tmp_path, in_path,
            sizeof(tmp_path)) < sizeof(tmp_path));
   if ((tok = (char*)strrchr(path_basename(tmp_path), '.')))
      *tok = '\0';

   rarch_assert(strlcpy(out_path, tmp_path, size) < size);
   rarch_assert(strlcat(out_path, replace, size) < size);
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
void fill_pathname_noext(char *out_path, const char *in_path,
      const char *replace, size_t size)
{
   rarch_assert(strlcpy(out_path, in_path, size) < size);
   rarch_assert(strlcat(out_path, replace, size) < size);
}

static char *find_last_slash(const char *str)
{
   const char *slash = strrchr(str, '/');
#ifdef _WIN32
   const char *backslash = strrchr(str, '\\');

   if (backslash && ((slash && backslash > slash) || !slash))
      slash = backslash;
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
   size_t path_len = strlen(path);
   const char *last_slash = find_last_slash(path);

   /* Try to preserve slash type. */
   if (last_slash && (last_slash != (path + path_len - 1)))
   {
      char join_str[2];
      strlcpy(join_str, last_slash, sizeof(join_str));
      rarch_assert(strlcat(path, join_str, size) < size);
   }
   else if (!last_slash)
      rarch_assert(strlcat(path, path_default_slash(), size) < size);
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
void fill_pathname_dir(char *in_dir, const char *in_basename,
      const char *replace, size_t size)
{
   const char *base = NULL;

   fill_pathname_slash(in_dir, size);
   base = path_basename(in_basename);
   rarch_assert(strlcat(in_dir, base, size) < size);
   rarch_assert(strlcat(in_dir, replace, size) < size);
}

/**
 * fill_pathname_base:
 * @out                : output path
 * @in_path            : input path
 * @size               : size of output path
 *
 * Copies basename of @in_path into @out_path.
 **/
void fill_pathname_base(char *out, const char *in_path, size_t size)
{
   const char *ptr_bak = NULL;
   const char     *ptr = find_last_slash(in_path);

   (void)ptr_bak;

   if (ptr)
      ptr++;
   else
      ptr = in_path;

#ifdef HAVE_COMPRESSION
   /* In case of compression, we also have to consider paths like
    *   /path/to/archive.7z#mygame.img
    *   and
    *   /path/to/archive.7z#folder/mygame.img
    *   basename would be mygame.img in both cases
    */
   ptr_bak = ptr;
   ptr     = strchr(ptr_bak,'#');
   if (ptr)
      ptr++;
   else
      ptr = ptr_bak;
#endif

   rarch_assert(strlcpy(out, ptr, size) < size);
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
   rarch_assert(strlcpy(out_dir, in_path, size) < size);
   path_basedir(out_dir);
}

/**
 * fill_pathname_parent_dir:
 * @out_dir            : output directory
 * @in_dir             : input directory
 * @size               : size of output directory
 *
 * Copies parent directory of @in_dir into @out_dir.
 * Assumes @in_dir is a directory. Keeps trailing '/'.
 **/
void fill_pathname_parent_dir(char *out_dir,
      const char *in_dir, size_t size)
{
   rarch_assert(strlcpy(out_dir, in_dir, size) < size);
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
void fill_dated_filename(char *out_filename,
      const char *ext, size_t size)
{
   time_t cur_time;
   time(&cur_time);

   strftime(out_filename, size,
         "RetroArch-%m%d-%H%M%S.", localtime(&cur_time));
   strlcat(out_filename, ext, size);
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

#ifdef HAVE_COMPRESSION
   /* We want to find the directory with the zipfile in basedir. */
   last = strchr(path,'#');
   if (last)
      *last = '\0';
#endif

   last = find_last_slash(path);

   if (last)
      last[1] = '\0';
   else
      snprintf(path, 3, ".%s", path_default_slash());
}

/**
 * path_parent_dir:
 * @path               : path
 *
 * Extracts parent directory by mutating path.
 * Assumes that path is a directory. Keeps trailing '/'.
 **/
void path_parent_dir(char *path)
{
   size_t len = strlen(path);
   if (len && path_char_is_slash(path[len - 1]))
      path[len - 1] = '\0';
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
   const char *last_hash = NULL;
   const char *last = find_last_slash(path);

   (void)last_hash;

#ifdef HAVE_COMPRESSION
   /* We cut either at the last hash or the last slash; whichever comes last */
   last_hash = strchr(path,'#');

   if (last_hash > last)
      return last_hash + 1;
#endif

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
 * Returns: true (1) if path is absolute, false (1) if path is relative.
 **/
bool path_is_absolute(const char *path)
{
   if (path[0] == '/')
      return true;
#ifdef _WIN32
   /* Many roads lead to Rome ... */
   if ((strstr(path, "\\\\") == path)
         || strstr(path, ":/") || strstr(path, ":\\") || strstr(path, ":\\\\"))
      return true;
#endif
   return false;
}

/**
 * path_resolve_realpath:
 * @buf                : buffer for path
 * @size               : size of buffer
 *
 * Turns relative paths into absolute path.
 * If relative, rebases on current working dir.
 **/
void path_resolve_realpath(char *buf, size_t size)
{
#ifndef RARCH_CONSOLE
   char tmp[PATH_MAX_LENGTH];

   strlcpy(tmp, buf, sizeof(tmp));

#ifdef _WIN32
   if (!_fullpath(buf, tmp, size))
      strlcpy(buf, tmp, size);
#else
   rarch_assert(size >= PATH_MAX_LENGTH);

   /* NOTE: realpath() expects at least PATH_MAX_LENGTH bytes in buf.
    * Technically, PATH_MAX_LENGTH needn't be defined, but we rely on it anyways.
    * POSIX 2008 can automatically allocate for you,
    * but don't rely on that. */
   if (!realpath(tmp, buf))
      strlcpy(buf, tmp, size);
#endif
#endif
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
      rarch_assert(strlcpy(out_path, in_path, size) < size);
      return;
   }

   rarch_assert(strlcpy(out_path, in_refpath, size) < size);
   path_basedir(out_path);
   rarch_assert(strlcat(out_path, in_path, size) < size);
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
void fill_pathname_join(char *out_path,
      const char *dir, const char *path, size_t size)
{
   rarch_assert(strlcpy(out_path, dir, size) < size);

   if (*out_path)
      fill_pathname_slash(out_path, size);

   rarch_assert(strlcat(out_path, path, size) < size);
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
void fill_pathname_join_delim(char *out_path, const char *dir,
      const char *path, const char delim, size_t size)
{
   size_t copied = strlcpy(out_path, dir, size);
   rarch_assert(copied < size+1);

   out_path[copied]   = delim;
   out_path[copied+1] = '\0';

   rarch_assert(strlcat(out_path, path, size) < size);
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
void fill_short_pathname_representation(char* out_rep,
      const char *in_path, size_t size)
{
   char path_short[PATH_MAX_LENGTH] = {0};
   char *last_hash                  = NULL;

   fill_pathname(path_short, path_basename(in_path), "",
            sizeof(path_short));

   last_hash = (char*)strchr(path_short,'#');
   if(last_hash != NULL)
   {
      /* We handle paths like:
       * /path/to/file.7z#mygame.img
       * short_name: mygame.img:
       *
       * We check whether something is actually
       * after the hash to avoid going over the buffer.
       */
      rarch_assert(strlen(last_hash) > 1);
      strlcpy(out_rep, last_hash + 1, size);
   }
   else
      strlcpy(out_rep, path_short, size);
}
