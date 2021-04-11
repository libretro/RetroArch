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
 * Gets delimiter of an archive file. Only the first '#'
 * after a compression extension is considered.
 *
 * Returns: pointer to the delimiter in the path if it contains
 * a compressed file, otherwise NULL.
 */
const char *path_get_archive_delim(const char *path);

/**
 * path_get_extension:
 * @path               : path
 *
 * Gets extension of file. Only '.'s
 * after the last slash are considered.
 *
 * Returns: extension part from the path.
 */
const char *path_get_extension(const char *path);

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
char *path_remove_extension(char *path);

/**
 * path_basename:
 * @path               : path
 *
 * Get basename from @path.
 *
 * Returns: basename from path.
 **/
const char *path_basename(const char *path);
const char *path_basename_nocompression(const char *path);

/**
 * path_basedir:
 * @path               : path
 *
 * Extracts base directory by mutating path.
 * Keeps trailing '/'.
 **/
void path_basedir(char *path);

/**
 * path_parent_dir:
 * @path               : path
 *
 * Extracts parent directory by mutating path.
 * Assumes that path is a directory. Keeps trailing '/'.
 * If the path was already at the root directory, returns empty string
 **/
void path_parent_dir(char *path);

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
char *path_resolve_realpath(char *buf, size_t size, bool resolve_symlinks);

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
 **/
size_t path_relative_to(char *out, const char *path, const char *base, size_t size);

/**
 * path_is_absolute:
 * @path               : path
 *
 * Checks if @path is an absolute path or a relative path.
 *
 * Returns: true if path is absolute, false if path is relative.
 **/
bool path_is_absolute(const char *path);

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
      const char *replace, size_t size);

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
      const char *ext, size_t size);

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
      const char *in_str, const char *ext, size_t size);

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
      const char *replace, size_t size);

/**
 * find_last_slash:
 * @str : input path
 *
 * Gets a pointer to the last slash in the input path.
 *
 * Returns: a pointer to the last slash in the input path.
 **/
char *find_last_slash(const char *str);

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
      const char *replace, size_t size);

/**
 * fill_pathname_base:
 * @out                : output path
 * @in_path            : input path
 * @size               : size of output path
 *
 * Copies basename of @in_path into @out_path.
 **/
size_t fill_pathname_base(char *out_path, const char *in_path, size_t size);

void fill_pathname_base_noext(char *out_dir,
      const char *in_path, size_t size);

size_t fill_pathname_base_ext(char *out,
      const char *in_path, const char *ext,
      size_t size);

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
void fill_pathname_basedir(char *out_path, const char *in_path, size_t size);

void fill_pathname_basedir_noext(char *out_dir,
      const char *in_path, size_t size);

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
      const char *in_dir, size_t size);

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
      const char *in_dir, size_t size);

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
void fill_pathname_resolve_relative(char *out_path, const char *in_refpath,
      const char *in_path, size_t size);

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
size_t fill_pathname_join(char *out_path, const char *dir,
      const char *path, size_t size);

size_t fill_pathname_join_special_ext(char *out_path,
      const char *dir,  const char *path,
      const char *last, const char *ext,
      size_t size);

size_t fill_pathname_join_concat_noext(char *out_path,
      const char *dir, const char *path,
      const char *concat,
      size_t size);

size_t fill_pathname_join_concat(char *out_path,
      const char *dir, const char *path,
      const char *concat,
      size_t size);

void fill_pathname_join_noext(char *out_path,
      const char *dir, const char *path, size_t size);

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
      const char *path, const char delim, size_t size);

size_t fill_pathname_join_delim_concat(char *out_path, const char *dir,
      const char *path, const char delim, const char *concat,
      size_t size);

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
      const char *in_path, size_t size);

void fill_short_pathname_representation_noext(char* out_rep,
      const char *in_path, size_t size);

void fill_pathname_expand_special(char *out_path,
      const char *in_path, size_t size);

void fill_pathname_abbreviate_special(char *out_path,
      const char *in_path, size_t size);

void fill_pathname_abbreviated_or_relative(char *out_path, const char *in_refpath, const char *in_path, size_t size);

void pathname_conform_slashes_to_os(char *path);

void pathname_make_slashes_portable(char *path);

/**
 * path_basedir:
 * @path               : path
 *
 * Extracts base directory by mutating path.
 * Keeps trailing '/'.
 **/
void path_basedir_wrapper(char *path);

/**
 * path_char_is_slash:
 * @c                  : character
 *
 * Checks if character (@c) is a slash.
 *
 * Returns: true (1) if character is a slash, otherwise false (0).
 */
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
 * Returns: default slash separator.
 */
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
 **/
void fill_pathname_slash(char *path, size_t size);

#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
void fill_pathname_application_path(char *buf, size_t size);
void fill_pathname_application_dir(char *buf, size_t size);
void fill_pathname_home_dir(char *buf, size_t size);
#endif

/**
 * path_mkdir:
 * @dir                : directory
 *
 * Create directory on filesystem.
 *
 * Returns: true (1) if directory could be created, otherwise false (0).
 **/
bool path_mkdir(const char *dir);

/**
 * path_is_directory:
 * @path               : path
 *
 * Checks if path is a directory.
 *
 * Returns: true (1) if path is a directory, otherwise false (0).
 */
bool path_is_directory(const char *path);

bool path_is_character_special(const char *path);

int path_stat(const char *path);

bool path_is_valid(const char *path);

int32_t path_get_size(const char *path);

bool is_path_accessible_using_standard_io(const char *path);

RETRO_END_DECLS

#endif
