/* Copyright  (C) 2010-2020 The RetroArch team
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

#if defined(RARCH_INTERNAL) && defined(HAVE_CONFIG_H)
#include "../../../config.h" /* for HAVE_MMAP */
#endif

RETRO_BEGIN_DECLS

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
   uint8_t  *data;
   uint32_t real_checksum;
} file_archive_file_handle_t;

typedef struct file_archive_transfer
{
   int64_t archive_size;
   void *context;
   struct RFILE *archive_file;
   const struct file_archive_file_backend *backend;
#ifdef HAVE_MMAP
   uint8_t *archive_mmap_data;
   int archive_mmap_fd;
#endif
   unsigned step_total;
   unsigned step_current;
   enum file_archive_transfer_type type;
} file_archive_transfer_t;

typedef struct
{
   file_archive_transfer_t archive;             /* int64_t alignment */
   char *source_file;
   char *subdir;
   char *target_dir;
   char *target_file;
   char *valid_ext;
   char *callback_error;
   struct archive_extract_userdata *userdata;
} decompress_state_t;

struct archive_extract_userdata
{
   /* These are set or read by the archive processing */
   char *first_extracted_file_path;
   const char *extraction_directory;
   struct string_list *ext;
   struct string_list *list;
   file_archive_transfer_t *transfer;
   /* Not used by the processing, free to use outside or in iterate callback */
   decompress_state_t *dec;
   void* cb_data;
   uint32_t crc;
   char archive_path[PATH_MAX_LENGTH];
   char current_file_path[PATH_MAX_LENGTH];
   bool found_file;
   bool list_only;
};

/* Returns true when parsing should continue. False to stop. */
typedef int (*file_archive_file_cb)(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata);

struct file_archive_file_backend
{
   int (*archive_parse_file_init)(
      file_archive_transfer_t *state,
      const char *file);
   int (*archive_parse_file_iterate_step)(
      void *context,
      const char *valid_exts,
      struct archive_extract_userdata *userdata,
      file_archive_file_cb file_cb);
   void (*archive_parse_file_free)(
      void *context);

   bool     (*stream_decompress_data_to_file_init)(
      void *context, file_archive_file_handle_t *handle,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size);
   int      (*stream_decompress_data_to_file_iterate)(
      void *context,
      file_archive_file_handle_t *handle);

   uint32_t (*stream_crc_calculate)(uint32_t, const uint8_t *, size_t);
   int64_t (*compressed_file_read)(const char *path, const char *needle, void **buf,
         const char *optional_outfile);
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
 * @archive_path                : filename path to ZIP archive.
 * @valid_exts                  : valid extensions for a file.
 * @extraction_directory        : the directory to extract the temporary
 *                                file to.
 *
 * Extract file from archive. If no file inside the archive is
 * specified, the first file found will be used.
 *
 * Returns : true (1) on success, otherwise false (0).
 **/
bool file_archive_extract_file(const char *archive_path,
      const char *valid_exts, const char *extraction_dir,
      char *out_path, size_t len);

/* Warning: 'list' must zero initialised before
 * calling this function, otherwise memory leaks/
 * undefined behaviour will occur */
bool file_archive_get_file_list_noalloc(struct string_list *list,
      const char *path,
      const char *valid_exts);

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
