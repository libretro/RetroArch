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
/* VFS_HAVE_FILE_MAPPING: HAVE_MMAP or Win32, see vfs.h. */
#include <vfs/vfs.h>

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
#ifdef VFS_HAVE_FILE_MAPPING
   /* The whole archive, mapped: the VFS's mapping of archive_file
    * (opened with RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS), borrowed
    * for as long as the file is open, or NULL when there is none - a
    * file too large to map, a VFS URL scheme, no mapping support.
    * Read-only; declared non-const only because the decoders hand it
    * to zlib's next_in.  Was a private mmap() over a second open() of
    * the path, POSIX only; the VFS maps on Win32 too, so the Windows
    * build now takes the zero-copy decode paths as well. */
   uint8_t *archive_mmap_data;
#endif
   unsigned step_total;
   unsigned step_current;
   enum file_archive_transfer_type type;

   /* A member whose decode has started but not finished.
    *
    * Decoding one member used to be atomic however long it took: the
    * backends can say "not finished, call me again" but every caller
    * consumed that in a do/while loop, so it never reached a frame
    * boundary. The work now parks here between frames instead, and
    * file_archive_parse_file_iterate() picks it up before stepping to
    * the next entry.
    *
    * pending_active is what makes this visible to the iterate loop;
    * everything else is the state that call needs to resume. */
   file_archive_file_handle_t pending_handle;
   uint64_t pending_size;
   /* The member's output, opened beside pending_path and renamed over
    * it when the member completes (filestream_open_atomic()).  A
    * backend that can produce output a piece at a time writes it here
    * as it goes and leaves pending_handle.data NULL; any other fills
    * pending_handle.data, which is written here at the end. */
   struct RFILE *pending_sink;
   char     pending_path[PATH_MAX_LENGTH];
   bool     pending_active;
   /* The scan asked to stop while a member was still parked. The stop
    * is honoured once the pending decode has drained, not before, or
    * the member would be dropped half-written. */
   bool     pending_stop;
   /* The scan already counted this member towards progress. A callback
    * that parks a member and lets the scan continue is counted when it
    * is found; one that parks and stops is counted when it drains. */
   bool     pending_counted;
   /* The zip's end record carries a TorrentZip comment.  Set by the zip
    * reader when it opens the archive, false for any other. */
   bool     torrentzip;
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
   /* Directory the previous member was extracted into, already known
    * to exist.  Archive members are grouped by directory, so this
    * lets the next member skip the stat that path_mkdir() would do
    * to find that out again - which on a FAT card is a directory
    * scan, per file. */
   char last_dir[PATH_MAX_LENGTH];
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
   uint64_t size;
   char archive_path[PATH_MAX_LENGTH];
   char current_file_path[PATH_MAX_LENGTH];
   bool found_file;
   bool list_only;
   /* The member's CRC is wanted: a backend whose archive does not
    * record one decodes the member to have it */
   bool crc_wanted;
};

/* Returns true when parsing should continue. False to stop. */
typedef int (*file_archive_file_cb)(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size,
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
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size);
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

/**
 * file_archive_read_at:
 * @ud         : the archive's open RFILE
 * @off        : offset to read at
 * @dst        : receives the octets
 * @len        : octets wanted
 *
 * Positioned read of an archive, the shape the 7z and RAR parsers read
 * through when the archive is not mapped. Short reads are retried, so
 * the result is @len unless the file ends first.
 *
 * Returns: octets read, 0 at the end, -1 on error.
 */
int64_t file_archive_read_at(void *ud, uint64_t off, void *dst, size_t len);

/**
 * file_archive_perform_mode_start:
 *
 * Begins decoding one member into the transfer's pending slot and does
 * a first slice of the work.
 *
 * Returns: 1 if the member finished immediately, 0 if it is parked and
 * should be resumed with file_archive_perform_mode_step(), -1 on
 * failure.
 */
int file_archive_perform_mode_start(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata);

/**
 * file_archive_perform_mode_step:
 *
 * Does one more slice of a parked member's decode.
 *
 * Returns: 1 when the member is complete and written, 0 when there is
 * more to do, -1 on failure. The pending slot is cleared on 1 and -1.
 */
int file_archive_perform_mode_step(file_archive_transfer_t *state);

bool file_archive_perform_mode(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size,
      uint32_t crc32, struct archive_extract_userdata *userdata);

int file_archive_compressed_read(
      const char* path, void **buf,
      const char* optional_filename, int64_t *length);

const struct file_archive_file_backend* file_archive_get_zlib_file_backend(void);
const struct file_archive_file_backend* file_archive_get_7z_file_backend(void);
const struct file_archive_file_backend* file_archive_get_zstd_file_backend(void);
const struct file_archive_file_backend* file_archive_get_rar_file_backend(void);

const struct file_archive_file_backend* file_archive_get_file_backend(const char *path);

/**
 * file_archive_open_hints:
 * @size                         : size of the archive, or <= 0 if unknown
 *
 * Returns: the hints an archive of @size is opened with, asking for a
 * mapping where the host can afford one.
 **/
unsigned file_archive_open_hints(int64_t size);

/**
 * file_archive_whole:
 * @size                         : octets in @file
 * @owned                        : receives a buffer for the caller to
 *                                 free, or NULL
 *
 * Returns: all of @file, where the stream lends it whole (a mapping, a
 * read-ahead window) or else read into *@owned; NULL on failure. What
 * is lent holds until the next call on @file.
 **/
const uint8_t *file_archive_whole(struct RFILE *file, int64_t size,
      void **owned);

/**
 * file_archive_get_file_crc32:
 * @path                         : filename path of archive
 *
 * Returns: CRC32 of the specified file in the archive, otherwise 0.
 * If no path within the archive is specified, the first
 * file found inside is used.
 **/
uint32_t file_archive_get_file_crc32(const char *path);

/**
 * file_archive_get_file_crc32_and_size:
 * @path                         : filename path of archive
 * @size                         : size of the file inside the archive
 *
 * Returns: CRC32 of the specified file in the archive, otherwise 0.
 * If no path within the archive is specified, the first
 * file found inside is used.
 **/
uint32_t file_archive_get_file_crc32_and_size(const char *path, uint64_t *size);

/**
 * file_archive_get_file_crc32_size_and_ext:
 * @path                         : filename path of archive
 * @size                         : size of the file inside the archive
 * @ext                          : that file's extension, or "" when
 *                                 not found; may be NULL
 * @len                          : size of @ext in bytes
 * @entries                      : the archive's entry count as its
 *                                 header gives it, or 0 when not
 *                                 found; may be NULL
 *
 * Returns: CRC32 of the specified file in the archive, otherwise 0.
 * If no path within the archive is specified, the first
 * file found inside is used.
 **/
uint32_t file_archive_get_file_crc32_size_and_ext(const char *path,
      uint64_t *size, char *ext, size_t len, unsigned *entries);

extern const struct file_archive_file_backend zlib_backend;
extern const struct file_archive_file_backend sevenzip_backend;
extern const struct file_archive_file_backend zstd_backend;
extern const struct file_archive_file_backend bz2_backend;
extern const struct file_archive_file_backend rar_backend;

/* ---- incremental archive entry source ----
 *
 * Pull decompressed bytes of a single archive entry ("/path/file.zip
 * #entry") in caller-paced chunks, instead of extracting the whole
 * entry in one blocking call.  Decompression happens inside read():
 * each call inflates just enough compressed input (directly from the
 * archive's mapping where available) to produce up to n output
 * bytes, so I/O and decode interleave per chunk - the counterpart of
 * the image and audio layers' decode-as-the-bytes-arrive.
 *
 * Contract: dst regions across successive read() calls must be
 * sequential and contiguous (the data_transfer source fill satisfies
 * this naturally), and dst has room to the entry's end: n paces the
 * decode, and a read may produce more - the built-in inflate a whole
 * input slice, a Zstandard or bzip2 member, which does not stop part
 * way, all of itself straight into dst at the first read.  The
 * built-in inflate binds its output window once and the source
 * verifies the cursor.  Returns bytes produced, 0 at end of entry, -1
 * on error.  Only backends whose entries are
 * independently decodable provide sources (ZIP; 7z solid blocks
 * cannot), so a NULL open is a normal "use the classic whole-entry
 * path" signal, not an error. */
typedef struct file_archive_entry_source file_archive_entry_source_t;

file_archive_entry_source_t *file_archive_entry_source_open(
      const char *path, int64_t *usize);
int64_t file_archive_entry_source_read(file_archive_entry_source_t *s,
      uint8_t *dst, int64_t n);
void file_archive_entry_source_close(file_archive_entry_source_t *s);

RETRO_END_DECLS

#endif
