/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_path.h).
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

#ifndef __LIBRETRO_SDK_FILE_PATH_H
#define __LIBRETRO_SDK_FILE_PATH_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#include <libretro.h>
#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

#define PATH_REQUIRED_VFS_VERSION 3

void path_vfs_init(const struct retro_vfs_interface_info* vfs_info);

/* Order in this enum is equivalent to negative sort order in filelist
 *  (i.e. DIRECTORY is on top of PLAIN_FILE) */
enum
{
   RARCH_FILETYPE_UNSET,
   RARCH_PLAIN_FILE,
   RARCH_COMPRESSED_FILE_IN_ARCHIVE,
   RARCH_COMPRESSED_ARCHIVE,
   RARCH_DIRECTORY,
   RARCH_FILE_UNSUPPORTED
};

struct path_linked_list
{
   char *path;
   struct path_linked_list *next;
};

/**
 * Create a new linked list with one item in it
 * The path on this item will be set to NULL
**/
struct path_linked_list* path_linked_list_new(void);

/* Free the entire linked list */
void path_linked_list_free(struct path_linked_list *in_path_linked_list);

/**
 * Add a node to the linked list with this path
 * If the first node's path if it's not yet set,
 * set this instead
**/
void path_linked_list_add_path(struct path_linked_list *in_path_linked_list, char *path);

/**
 * path_is_compressed_file:
 * @path               : path
 *
 * Checks if path is a compressed file.
 *
 * Returns: true (1) if path is a compressed file, otherwise false (0).
 **/
bool path_is_compressed_file(const char *path);

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
#define path_contains_compressed_file(path) (path_get_archive_delim((path)) != NULL)

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
const char *path_get_archive_delim(const char *path);

/**
 * path_get_extension:
 * @path               : path
 *
 * Gets extension of file. Only '.'s
 * after the last slash are considered.
 *
 * Hidden non-leaf function cost:
 * - calls string_is_empty()
 * - calls strrchr
 *
 * @return extension part from the path.
 **/
const char *path_get_extension(const char *path);

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
char *path_get_extension_mutable(const char *path);

/**
 * path_remove_extension:
 * @path               : path
 *
 * Mutates path by removing its extension. Removes all
 * text after and including the last '.'.
 * Only '.'s after the last slash are considered.
 *
 * Hidden non-leaf function cost:
 * - calls strrchr
 *
 * @return
 * 1) If path has an extension, returns path with the
 *    extension removed.
 * 2) If there is no extension, returns NULL.
 * 3) If path is empty or NULL, returns NULL
 */
char *path_remove_extension(char *path);

/**
 * path_basename:
 * @path               : path
 *
 * Get basename from @path.
 *
 * Hidden non-leaf function cost:
 * - Calls path_get_archive_delim()
 *
 * @return basename from path.
 **/
const char *path_basename(const char *path);

/**
 * path_basename_nocompression:
 * @path               : path
 *
 * Specialized version of path_basename().
 * Get basename from @path.
 *
 * @return basename from path.
 **/
const char *path_basename_nocompression(const char *path);

/**
 * path_basedir:
 * @path               : path
 *
 * Extracts base directory by mutating path.
 * Keeps trailing '/'.
 **/
size_t path_basedir(char *path);

/**
 * path_parent_dir:
 * @path               : path
 * @len                : length of @path
 *
 * Extracts parent directory by mutating path.
 * Assumes that path is a directory. Keeps trailing '/'.
 * If the path was already at the root directory, returns empty string
 **/
size_t path_parent_dir(char *path, size_t len);

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
 *       Use of fill_pathname_resolve_relative() should be preferred
 **/
char *path_resolve_realpath(char *buf, size_t len, bool resolve_symlinks);

/**
 * path_relative_to:
 * @out                : buffer to write the relative path to
 * @path               : path to be expressed relatively
 * @base               : relative to this
 * @size               : size of output buffer
 *
 * Turns @path into a path relative to @base and writes it to @out.
 *
 * @base is assumed to be a base directory, i.e. a path ending with '/' or '\'.
 * Both @path and @base are assumed to be absolute paths without "." or "..".
 *
 * E.g. path /a/b/e/f.cgp with base /a/b/c/d/ turns into ../../e/f.cgp
 *
 * @return Length of the string copied into @out
 **/
size_t path_relative_to(char *s, const char *path, const char *base,
      size_t len);

/**
 * path_is_absolute:
 * @path               : path
 *
 * Checks if @path is an absolute path or a relative path.
 *
 * @return true if path is absolute, false if path is relative.
 **/
bool path_is_absolute(const char *path);

/**
 * fill_pathname:
 * @s                  : output path
 * @in_path            : input  path
 * @replace            : what to replace
 * @len                : buffer size of output path
 *
 * FIXME: Verify
 *
 * Replaces filename extension with 'replace' and outputs result to s.
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
 * Hidden non-leaf function cost:
 * - calls strlcpy 2x
 * - calls strrchr
 * - calls strlcat
 *
 * @return Length of the string copied into @out
 */
size_t fill_pathname(char *s, const char *in_path,
      const char *replace, size_t len);

/**
 * fill_dated_filename:
 * @s                  : output filename
 * @ext                : extension of output filename
 * @len                : buffer size of output filename
 *
 * Creates a 'dated' filename prefixed by 'RetroArch', and
 * concatenates extension (@ext) to it.
 *
 * E.g.:
 * s = "RetroArch-{month}{day}-{Hours}{Minutes}.{@ext}"
 *
 * Hidden non-leaf function cost:
 * - Calls rtime_localtime()
 * - Calls strftime
 * - Calls strlcat
 *
 **/
size_t fill_dated_filename(char *s, const char *ext, size_t len);

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
 * Hidden non-leaf function cost:
 * - Calls time
 * - Calls rtime_localtime()
 * - Calls strlcpy 2x
 * - Calls string_is_empty()
 * - Calls strftime
 * - Calls strlcat
 *
 * @return Length of the string copied into @s
 **/
size_t fill_str_dated_filename(char *s, const char *in_str, const char *ext, size_t len);

/**
 * find_last_slash:
 * @str                : path
 *
 * Find last slash in path. Tries to find
 * a backslash on Windows too which takes precedence
 * over regular slash.

 * Hidden non-leaf function cost:
 * - calls strrchr
 *
 * @return pointer to last slash/backslash found in @str.
 **/
char *find_last_slash(const char *str);

/**
 * fill_pathname_dir:
 * @s                  : input directory path
 * @in_basename        : input basename to be appended to @s
 * @replace            : replacement to be appended to @in_basename
 * @len                : size of buffer
 *
 * Appends basename of 'in_basename', to 's', along with 'replace'.
 * Basename of in_basename is the string after the last '/' or '\\',
 * i.e the filename without directories.
 *
 * If in_basename has no '/' or '\\', the whole 'in_basename' will be used.
 * 'len' is buffer size of 's'.
 *
 * E.g..: s = "/tmp/some_dir", in_basename = "/some_content/foo.c",
 * replace = ".asm" => s = "/tmp/some_dir/foo.c.asm"
 *
 * Hidden non-leaf function cost:
 * - Calls fill_pathname_slash()
 * - Calls path_basename()
 * - Calls strlcpy 2x
 **/
size_t fill_pathname_dir(char *s, const char *in_basename,
      const char *replace, size_t len);

/**
 * fill_pathname_base:
 * @s                  : output path
 * @in_path            : input path
 * @size               : size of output path
 *
 * Copies basename of @in_path into @s.
 *
 * Hidden non-leaf function cost:
 * - Calls path_basename()
 * - Calls strlcpy
 *
 * @return length of the string copied into @out
 **/
size_t fill_pathname_base(char *s, const char *in_path, size_t len);

/**
 * fill_pathname_basedir:
 * @s                  : output directory
 * @in_path            : input path
 * @size               : size of output directory
 *
 * Copies base directory of @in_path into @s.
 * If in_path is a path without any slashes (relative current directory),
 * @s will get path "./".
 *
 * Hidden non-leaf function cost:
 * - Calls strlcpy
 * - Calls path_basedir()
 **/
size_t fill_pathname_basedir(char *s, const char *in_path, size_t len);

/**
 * fill_pathname_parent_dir_name:
 * @s                  : output string
 * @in_dir             : input directory
 * @len                : size of @s
 *
 * Copies only the parent directory name of @in_dir into @out_dir.
 * The two buffers must not overlap. Removes trailing '/'.
 *
 * Hidden non-leaf function cost:
 * - Calls strdup
 * - Can call strlcpy
 *
 * @return Length of the string copied into @s
 **/
size_t fill_pathname_parent_dir_name(char *s,
      const char *in_dir, size_t len);

/**
 * fill_pathname_parent_dir:
 * @out_dir            : output directory
 * @in_dir             : input directory
 * @size               : size of output directory
 *
 * Copies parent directory of @in_dir into @out_dir.
 * Assumes @in_dir is a directory. Keeps trailing '/'.
 * If the path was already at the root directory, @out_dir will be an empty string.
 *
 * Hidden non-leaf function cost:
 * - Can call strlcpy if (@out_dir != @in_dir)
 * - Calls strlen if (@out_dir == @in_dir)
 * - Calls path_parent_dir()
 **/
void fill_pathname_parent_dir(char *s,
      const char *in_dir, size_t len);

/**
 * fill_pathname_resolve_relative:
 * @s                  : output path
 * @in_refpath         : input reference path
 * @in_path            : input path
 * @size               : size of @s
 *
 * Joins basedir of @in_refpath together with @in_path.
 * If @in_path is an absolute path, s = in_path.
 * E.g.: in_refpath = "/foo/bar/baz.a", in_path = "foobar.cg",
 * s = "/foo/bar/foobar.cg".
 **/
void fill_pathname_resolve_relative(char *s, const char *in_refpath,
      const char *in_path, size_t len);

/**
 * fill_pathname_join:
 * @s                  : output path
 * @dir                : directory
 * @path               : path
 * @size               : size of output path
 *
 * Joins a directory (@dir) and path (@path) together.
 * Makes sure not to get two consecutive slashes
 * between directory and path.
 *
 * Hidden non-leaf function cost:
 * - calls strlcpy at least once
 * - calls fill_pathname_slash()
 *
 * Deprecated. Use fill_pathname_join_special() instead
 * if you can ensure @dir != @s
 *
 * @return Length of the string copied into @s
 **/
size_t fill_pathname_join(char *s, const char *dir,
      const char *path, size_t len);

/**
 * fill_pathname_join_special:
 * @s                  : output path
 * @dir                : directory. Cannot be identical to @s
 * @path               : path
 * @size               : size of output path
 *
 *
 * Specialized version of fill_pathname_join.
 * Unlike fill_pathname_join(),
 * @dir and @s CANNOT be identical.
 *
 * Joins a directory (@dir) and path (@path) together.
 * Makes sure not to get two consecutive slashes
 * between directory and path.
 *
 * Hidden non-leaf function cost:
 * - calls strlcpy 2x
 *
 * @return Length of the string copied into @s
 **/
size_t fill_pathname_join_special(char *s,
      const char *dir, const char *path, size_t len);

size_t fill_pathname_join_special_ext(char *s,
      const char *dir,  const char *path,
      const char *last, const char *ext,
      size_t len);

/**
 * fill_pathname_join_delim:
 * @s                  : output path
 * @dir                : directory
 * @path               : path
 * @delim              : delimiter
 * @size               : size of output path
 *
 * Joins a directory (@dir) and path (@path) together
 * using the given delimiter (@delim).
 *
 * Hidden non-leaf function cost:
 * - can call strlen
 * - can call strlcpy
 * - can call strlcat
 **/
size_t fill_pathname_join_delim(char *s, const char *dir,
      const char *path, const char delim, size_t len);

size_t fill_pathname_expand_special(char *s,
      const char *in_path, size_t len);

size_t fill_pathname_abbreviate_special(char *s,
      const char *in_path, size_t len);

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
		const char *in_refpath, const char *in_path, size_t len);

/**
 * sanitize_path_part:
 *
 * @path_part          : directory or filename
 * @size               : length of path_part
 *
 * Takes single part of a path eg. single filename
 * or directory, and removes any special chars that are
 * unavailable.
 *
 * @returns new string that has been sanitized
 **/
const char *sanitize_path_part(const char *path_part, size_t len);

/**
 * pathname_conform_slashes_to_os:
 *
 * @path               : path
 *
 * Leaf function.
 *
 * Changes the slashes to the correct kind for the os
 * So forward slash on linux and backslash on Windows
 **/
void pathname_conform_slashes_to_os(char *s);

/**
 * pathname_make_slashes_portable:
 * @path               : path
 *
 * Leaf function.
 *
 * Change all slashes to forward so they are more
 * portable between Windows and Linux
 **/
void pathname_make_slashes_portable(char *s);

/**
 * path_basedir:
 * @path               : path
 *
 * Extracts base directory by mutating path.
 * Keeps trailing '/'.
 **/
void path_basedir_wrapper(char *s);

/**
 * path_char_is_slash:
 * @c                  : character
 *
 * Checks if character (@c) is a slash.
 *
 * @return true if character is a slash, otherwise false.
 **/
#ifdef _WIN32
#define PATH_CHAR_IS_SLASH(c) (((c) == '/') || ((c) == '\\'))
#else
#define PATH_CHAR_IS_SLASH(c) ((c) == '/')
#endif

/**
 * path_default_slash and path_default_slash_c:
 *
 * Gets the default slash separator.
 *
 * @return default slash separator.
 **/
#ifdef _WIN32
#define PATH_DEFAULT_SLASH() "\\"
#define PATH_DEFAULT_SLASH_C() '\\'
#else
#define PATH_DEFAULT_SLASH() "/"
#define PATH_DEFAULT_SLASH_C() '/'
#endif

/**
 * fill_pathname_slash:
 * @path               : path
 * @size               : size of path
 *
 * Assumes path is a directory. Appends a slash
 * if not already there.

 * Hidden non-leaf function cost:
 * - can call strlcat once if it returns false
 * - calls strlen
 **/
size_t fill_pathname_slash(char *s, size_t len);

#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
size_t fill_pathname_application_path(char *s, size_t len);
size_t fill_pathname_application_dir(char *s, size_t len);
size_t fill_pathname_home_dir(char *s, size_t len);
#endif

/**
 * path_mkdir:
 * @dir                : directory
 *
 * Create directory on filesystem.
 *
 * Recursive function.
 *
 * Hidden non-leaf function cost:
 * - Calls strdup
 * - Calls path_parent_dir()
 * - Calls strcmp
 * - Calls path_is_directory()
 * - Calls path_mkdir()
 *
 * @return true if directory could be created, otherwise false.
 **/
bool path_mkdir(const char *dir);

/**
 * path_is_directory:
 * @path               : path
 *
 * Checks if path is a directory.
 *
 * @return true if path is a directory, otherwise false.
 */
bool path_is_directory(const char *path);

/* Time format strings with AM-PM designation require special
 * handling due to platform dependence
 * @return Length of the string written to @s
 */
size_t strftime_am_pm(char *s, size_t len, const char* format,
      const void* timeptr);

bool path_is_character_special(const char *path);

int path_stat(const char *path);

bool path_is_valid(const char *path);

int32_t path_get_size(const char *path);

bool is_path_accessible_using_standard_io(const char *path);

RETRO_END_DECLS

#endif
