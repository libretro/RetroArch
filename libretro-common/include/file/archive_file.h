/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_file.h).
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

#ifndef LIBRETRO_SDK_ARCHIVE_FILE_H__
#define LIBRETRO_SDK_ARCHIVE_FILE_H__

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <retro_miscellaneous.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

struct archive_extract_userdata;

enum file_archive_transfer_type
{
   ARCHIVE_TRANSFER_NONE = 0,
   ARCHIVE_TRANSFER_INIT,
   ARCHIVE_TRANSFER_ITERATE,
   ARCHIVE_TRANSFER_DEINIT,
   ARCHIVE_TRANSFER_DEINIT_ERROR
};

typedef struct file_archive_handle
{
   void     *stream;
   uint8_t  *data;
   uint32_t real_checksum;
   const struct file_archive_file_backend *backend;
} file_archive_file_handle_t;

typedef struct file_archive_file_data file_archive_file_data_t;

typedef struct file_archive_transfer
{
   enum file_archive_transfer_type type;
   int32_t archive_size;
   ptrdiff_t start_delta;
   file_archive_file_data_t *handle;
   void *stream;
   const uint8_t *footer;
   const uint8_t *directory;
   const uint8_t *data;
   const struct file_archive_file_backend *backend;
} file_archive_transfer_t;

enum file_archive_compression_mode
{
   ARCHIVE_MODE_UNCOMPRESSED = 0,
   ARCHIVE_MODE_COMPRESSED   = 8
};

struct decomp_state_t
{
   char *opt_file;
   char *needle;
   void **buf;
   size_t size;
   bool found;
};

typedef struct
{
   char *source_file;
   char *subdir;
   char *target_dir;
   char *target_file;
   char *valid_ext;

   char *callback_error;

   file_archive_transfer_t archive;
   struct archive_extract_userdata *userdata;
} decompress_state_t;

struct archive_extract_userdata
{
   char archive_path[PATH_MAX_LENGTH];
   char *first_extracted_file_path;
   char *extracted_file_path;
   const char *extraction_directory;
   size_t archive_path_size;
   struct string_list *ext;
   struct string_list *list;
   bool found_file;
   bool list_only;
   void *context;
   char archive_name[PATH_MAX_LENGTH];
   uint32_t crc;
   struct decomp_state_t decomp_state;
   decompress_state_t *dec;
};

/* Returns true when parsing should continue. False to stop. */
typedef int (*file_archive_file_cb)(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata);

struct file_archive_file_backend
{
   void *(*stream_new)(void);
   void  (*stream_free)(void *);
   bool     (*stream_decompress_data_to_file_init)(
         file_archive_file_handle_t *, const uint8_t *,  uint32_t, uint32_t);
   int      (*stream_decompress_data_to_file_iterate)(void *);
   uint32_t (*stream_crc_calculate)(uint32_t, const uint8_t *, size_t);
   int (*compressed_file_read)(const char *path, const char *needle, void **buf,
         const char *optional_outfile);
   int (*archive_parse_file_init)(
      file_archive_transfer_t *state,
      const char *file);
   int (*archive_parse_file_iterate_step)(
      file_archive_transfer_t *state,
      const char *valid_exts,
      struct archive_extract_userdata *userdata,
      file_archive_file_cb file_cb);
   const char *ident;
};

int file_archive_parse_file_iterate(
      file_archive_transfer_t *state,
      bool *returnerr,
      const char *file,
      const char *valid_exts,
      file_archive_file_cb file_cb,
      struct archive_extract_userdata *userdata);

void file_archive_parse_file_iterate_stop(file_archive_transfer_t *state);

int file_archive_parse_file_progress(file_archive_transfer_t *state);

/**
 * file_archive_extract_file:
 * @archive_path                    : filename path to ZIP archive.
 * @archive_path_size               : size of ZIP archive.
 * @valid_exts                  : valid extensions for a file.
 * @extraction_directory        : the directory to extract the temporary
 *                                file to.
 *
 * Extract file from archive. If no file inside the archive is
 * specified, the first file found will be used.
 *
 * Returns : true (1) on success, otherwise false (0).
 **/
bool file_archive_extract_file(char *archive_path, size_t archive_path_size,
      const char *valid_exts, const char *extraction_dir,
      char *out_path, size_t len);

/**
 * file_archive_get_file_list:
 * @path                        : filename path of archive
 * @valid_exts                  : Valid extensions of archive to be parsed.
 *                                If NULL, allow all.
 *
 * Returns: string listing of files from archive on success, otherwise NULL.
 **/
struct string_list* file_archive_get_file_list(const char *path, const char *valid_exts);

bool file_archive_perform_mode(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata);

int file_archive_compressed_read(
      const char* path, void **buf,
      const char* optional_filename, int64_t *length);

const struct file_archive_file_backend* file_archive_get_zlib_file_backend(void);
const struct file_archive_file_backend* file_archive_get_7z_file_backend(void);

const struct file_archive_file_backend* file_archive_get_file_backend(const char *path);

/**
 * file_archive_get_file_crc32:
 * @path                         : filename path of archive
 *
 * Returns: CRC32 of the specified file in the archive, otherwise 0.
 * If no path within the archive is specified, the first
 * file found inside is used.
 **/
uint32_t file_archive_get_file_crc32(const char *path);

extern const struct file_archive_file_backend zlib_backend;
extern const struct file_archive_file_backend sevenzip_backend;

RETRO_END_DECLS

#endif
