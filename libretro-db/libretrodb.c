/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (libretrodb.c).
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

#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

#include <streams/file_stream.h>
#include <retro_endianness.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#include "libretrodb.h"
#include "rmsgpack_dom.h"
#include "rmsgpack.h"
#include "bintree.h"
#include "query.h"
#include "libretrodb.h"

#define MAGIC_NUMBER "RARCHDB"

/* Must match MAX_ERROR_LEN in query.c */
#define LIBRETRODB_QUERY_ERR_LEN 256

/* Sliding window used for the cursor walk.  Has to comfortably exceed
 * the largest backwards jump the cursor makes - a rewind to the start
 * of the record being inspected - so that the rewind stays inside the
 * window and costs nothing. */
#ifndef LIBRETRODB_WINDOW_SIZE
#define LIBRETRODB_WINDOW_SIZE (64 * 1024)
#endif

/* MsgPack type bytes — needed by the fast cursor read path */
#define _MPF_FIXMAP   0x80
#define _MPF_FIXARRAY 0x90
#define _MPF_FIXSTR   0xa0
#define _MPF_NIL      0xc0
#define _MPF_STR8     0xd9
#define _MPF_STR16    0xda
#define _MPF_STR32    0xdb
#define _MPF_MAP16    0xde
#define _MPF_MAP32    0xdf

struct node_iter_ctx
{
   libretrodb_t *db;
   libretrodb_index_t *idx;
};

struct libretrodb
{
   intfstream_t *fd;
   /* Scratch for libretrodb_query_compile()'s error text.  It used to
    * be a function-scope static, so the pointer handed back through
    * err_string was shared by every caller in the process and two
    * concurrent compiles overwrote each other's message.  Tying it to
    * the db handle keeps the pointer valid for as long as the caller
    * can meaningfully use it while making it per-handle. */
   char query_err[LIBRETRODB_QUERY_ERR_LEN];
   char *path;
   bool can_write;
   uint64_t root;
   uint64_t count;
   uint64_t first_index_offset;
};

/* Widest index key the format is expected to carry (SHA-1 is 20
 * bytes).  Used to reject nonsense key sizes from a malformed index
 * header before they reach binsearch(). */
#define LIBRETRODB_MAX_KEY_SIZE 256

struct libretrodb_index
{
   char name[50];
   uint64_t key_size;
   uint64_t next;
   uint64_t count;
};

typedef struct libretrodb_metadata
{
   uint64_t count;
} libretrodb_metadata_t;

typedef struct libretrodb_header
{
   char magic_number[sizeof(MAGIC_NUMBER)];
   uint64_t metadata_offset;
} libretrodb_header_t;

struct libretrodb_cursor
{
   intfstream_t *fd;
   libretrodb_query_t *query;
   libretrodb_t *db;
   int is_valid;
   int eof;
};

static int libretrodb_validate_document(const struct rmsgpack_dom_value *doc)
{
   unsigned i;

   if (doc->type != RDT_MAP)
      return -1;

   for (i = 0; i < doc->val.map.len; i++)
   {
      int rv                          = 0;
      struct rmsgpack_dom_value key   = doc->val.map.items[i].key;
      struct rmsgpack_dom_value value = doc->val.map.items[i].value;

      if (key.type != RDT_STRING)
         return -1;

      if (key.val.string.len <= 0)
         return -1;

      if (key.val.string.buff[0] == '$')
         return -1;

      if (value.type != RDT_MAP)
         continue;

      if ((rv = libretrodb_validate_document(&value)) != 0)
         return rv;
   }

   return 0;
}

int libretrodb_create(intfstream_t *fd, libretrodb_value_provider value_provider,
      void *ctx)
{
   int rv;
   libretrodb_metadata_t md;
   static struct rmsgpack_dom_value sentinal;
   struct rmsgpack_dom_value item;
   uint64_t item_count        = 0;
   libretrodb_header_t header = {{0}};
   ssize_t root               = intfstream_tell(fd);

   memcpy(header.magic_number, MAGIC_NUMBER, sizeof(MAGIC_NUMBER)-1);

   /* We write the header in the end because we need to know the size of
    * the db first */

   intfstream_seek(fd, sizeof(libretrodb_header_t),
         RETRO_VFS_SEEK_POSITION_CURRENT);

   item.type = RDT_NULL;
   while ((rv = value_provider(ctx, &item)) == 0)
   {
      if ((rv = libretrodb_validate_document(&item)) < 0)
         goto clean;

      if ((rv = rmsgpack_dom_write(fd, &item)) < 0)
         goto clean;

      rmsgpack_dom_value_free(&item);
      item.type = RDT_NULL;
      item_count++;
   }

   if (rv < 0)
      goto clean;

   if ((rv = rmsgpack_dom_write(fd, &sentinal)) < 0)
      goto clean;

   header.metadata_offset = swap_if_little64(intfstream_tell(fd));
   md.count               = item_count;
   rmsgpack_write_map_header(fd, 1);
   rmsgpack_write_string(fd, "count", STRLEN_CONST("count"));
   rmsgpack_write_uint(fd, md.count);
   intfstream_seek(fd, root, RETRO_VFS_SEEK_POSITION_START);
   intfstream_write(fd, &header, sizeof(header));
clean:
   rmsgpack_dom_value_free(&item);
   return rv;
}

void libretrodb_close(libretrodb_t *db)
{
   /* intfstream_close closes the inner file but does not free
    * the intfstream_t struct itself (existing libretro-common
    * convention).  Match the cleanup pattern used by
    * core_info.c / core_backup.c / cdfs.c / rpng_encode.c which
    * also free the struct after closing. */
   if (db->fd)
   {
      intfstream_close(db->fd);
      free(db->fd);
   }
   if (db->path && *db->path)
      free(db->path);
   db->path = NULL;
   db->fd   = NULL;
}

int libretrodb_open(const char *path, libretrodb_t *db, bool write)
{
   libretrodb_header_t header;
   libretrodb_metadata_t md;
   int64_t       file_size;
   unsigned mode = write ? RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING : RETRO_VFS_FILE_ACCESS_READ;
   intfstream_t *fd = intfstream_open_file(path, mode, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   db->can_write = write;
   if (!fd)
     return -1;

   if (db->path && *db->path)
      free(db->path);

   db->path  = strdup(path);
   db->root  = intfstream_tell(fd);

   /* intfstream_read() signals EOF as a short read, not as -1, so a
    * file smaller than the header used to leave the tail of 'header'
    * uninitialised and then feed it to strncmp() below.  Require the
    * full header. */
   if (intfstream_read(fd, &header, sizeof(header)) != (int64_t)sizeof(header))
      goto error;

   if (strncmp(header.magic_number, MAGIC_NUMBER, sizeof(MAGIC_NUMBER)) != 0)
      goto error;

   header.metadata_offset = swap_if_little64(header.metadata_offset);

   /* Pre-patch the metadata_offset field was an attacker-
    * controlled uint64 from the .rdb header, cast to ssize_t and
    * fed straight to intfstream_seek without any bounds check.
    * On 32-bit that cast truncates; on 64-bit a value past EOF
    * left the stream in a state where the subsequent
    * rmsgpack_dom_read_into either failed cleanly or, depending
    * on the VFS implementation's seek-past-EOF semantics, read
    * stale buffered bytes.
    *
    * Reject metadata_offset that doesn't fit in the actual file
    * (must leave room for at least the 1-byte fixmap header). */
   file_size = intfstream_get_size(fd);
   if (file_size < 0)
      goto error;
   if (header.metadata_offset >= (uint64_t)file_size)
      goto error;
   if (intfstream_seek(fd, (ssize_t)header.metadata_offset,
         RETRO_VFS_SEEK_POSITION_START) < 0)
      goto error;

   if (rmsgpack_dom_read_into(fd, "count", RDF_UINT, &md.count, NULL) < 0)
      goto error;

   db->count              = md.count;
   db->first_index_offset = intfstream_tell(fd);
   db->fd                 = fd;
   return 0;

error:
   /* Free the strdup'd path (assigned at line 209) on the error
    * path: pre-this-commit it was leaked unconditionally on bad
    * magic, bad metadata_offset, or any rmsgpack_dom_read_into
    * failure.  Reachable on any malformed .rdb the user has, so
    * the leak compounds across a directory scan.
    *
    * Also free the intfstream_t struct itself.  intfstream_close
    * intentionally only closes the inner file (existing libretro-
    * common convention -- see the trailing 'free(file)' calls in
    * core_info.c, core_backup.c, cdfs.c, rpng_encode.c which
    * compensate for this), but libretrodb_open didn't.  This was
    * a 48-byte leak per failed open. */
   if (db->path)
   {
      free(db->path);
      db->path = NULL;
   }
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   return -1;
}

static int libretrodb_find_index(libretrodb_t *db, const char *index_name,
      libretrodb_index_t *idx)
{
   intfstream_seek(db->fd,
                   (ssize_t)db->first_index_offset,
                   RETRO_VFS_SEEK_POSITION_START);

   while (!intfstream_eof(db->fd))
   {
      uint64_t name_len = 50;
      /* Read index header */
      if (rmsgpack_dom_read_into(db->fd,
            "name",     RDF_STRING, idx->name, &name_len,
            "key_size", RDF_UINT,   &idx->key_size,
            "next",     RDF_UINT,   &idx->next,
            "count",    RDF_UINT,   &idx->count,
                                    NULL) < 0)
      {
        printf("Invalid index header\n");
        break;
      }

      if (strncmp(index_name, idx->name, strlen(idx->name)) == 0)
         return 0;

      /* idx->next is a file-supplied relative seek.  Zero re-reads
       * the same header forever and a value that wraps negative
       * seeks backwards, so intfstream_eof() is never reached and
       * the lookup hangs.  Require forward progress. */
      if (idx->next == 0 || idx->next > (uint64_t)INT64_MAX)
         break;

      if (intfstream_seek(db->fd, (ssize_t)idx->next,
            RETRO_VFS_SEEK_POSITION_CURRENT) < 0)
         break;
   }

   return -1;
}

/**
 * binsearch:
 *
 * Locate @item in the @count fixed-size entries at @buff and return
 * the record offset stored alongside it.  Each entry is @field_size
 * key bytes followed by a uint64 offset.
 *
 * The previous recursive form had three defects, all reachable from
 * a malformed index header:
 *
 *  - the memcmp() ran before the "count == 0" test, so an empty
 *    sub-range was compared against buff[0..field_size) - a read past
 *    the end of the allocation whenever the search narrowed to
 *    nothing;
 *  - the tail call passed "count - mid", and mid is 0 when count is
 *    1, so a one-element range recursed on itself forever, walking
 *    further past the buffer on each step until it faulted;
 *  - the offset was read with *(uint64_t*)(current + field_size).
 *    field_size comes from the file, so unless it is a multiple of
 *    eight every entry after the first is misaligned - undefined
 *    behaviour that UBSan flags and that faults outright on
 *    strict-alignment targets.
 *
 * Iterative, half-open [lo, hi), and the offset is assembled with
 * memcpy so alignment never matters.
 */
static int binsearch(const void *buff, const void *item,
      uint64_t count, uint64_t field_size, uint64_t *offset)
{
   uint64_t lo        = 0;
   uint64_t hi        = count;
   uint64_t item_size = field_size + sizeof(uint64_t);

   if (field_size == 0)
      return -1;

   while (lo < hi)
   {
      uint64_t mid           = lo + ((hi - lo) / 2);
      const uint8_t *current = (const uint8_t *)buff + (mid * item_size);
      int rv                 = memcmp(current, item, (size_t)field_size);

      if (rv == 0)
      {
         memcpy(offset, current + field_size, sizeof(uint64_t));
         return 0;
      }

      if (rv > 0)
         hi = mid;
      else
         lo = mid + 1;
   }

   return -1;
}

int libretrodb_find_entry(libretrodb_t *db, const char *index_name,
      const void *key, struct rmsgpack_dom_value *out)
{
   libretrodb_index_t idx;
   int rv;
   uint8_t *buff;
   uint64_t offset;
   uint64_t item_size;
   uint64_t bufflen;
   int64_t  nread = 0;

   if (libretrodb_find_index(db, index_name, &idx) < 0)
      return -1;

   /* idx.next, idx.count and idx.key_size all come from the file
    * with no relationship enforced between them.  binsearch() walks
    * idx.count entries of (key_size + 8) bytes, so without this
    * check a small "next" and a large "count" send it off the end of
    * the allocation.  Require the payload the header describes to
    * actually fit in the payload the header reserved, and reject
    * degenerate key sizes outright. */
   if (idx.key_size == 0 || idx.key_size > LIBRETRODB_MAX_KEY_SIZE)
      return -1;

   item_size = idx.key_size + sizeof(uint64_t);
   if (idx.count > idx.next / item_size)
      return -1;

   bufflen        = idx.next;
   if (bufflen == 0 || bufflen > (uint64_t)INT64_MAX)
      return -1;
   if (!(buff = (uint8_t*)malloc((size_t)bufflen)))
      return -1;

   while (nread < (int64_t)bufflen)
   {
      void *buff_ = (buff + nread);
      rv          = (int)intfstream_read(db->fd, buff_,
            (uint64_t)((int64_t)bufflen - nread));

      if (rv <= 0)
      {
         free(buff);
         return -1;
      }
      nread += rv;
   }

   rv = binsearch(buff, key, idx.count, idx.key_size, &offset);
   free(buff);

   if (rv == 0)
   {
      intfstream_seek(db->fd, (ssize_t)offset, RETRO_VFS_SEEK_POSITION_START);
      rmsgpack_dom_read(db->fd, out);
      return 0;
   }
   return -1;
}

/**
 * libretrodb_cursor_reset:
 * @cursor              : Handle to database cursor.
 *
 * Resets cursor.
 *
 * Returns: ???.
 **/
int libretrodb_cursor_reset(libretrodb_cursor_t *cursor)
{
   cursor->eof = 0;
   return (int)intfstream_seek(cursor->fd,
         (ssize_t)(cursor->db->root + sizeof(libretrodb_header_t)),
         RETRO_VFS_SEEK_POSITION_START);
}

/**
 * rmsgpack_read_map_header:
 *
 * Read a MsgPack map header from the stream and return the number
 * of key-value pairs. Returns -1 on error or if the value is not
 * a map. If the value is nil, returns -2 to signal end-of-records.
 */
static int32_t rmsgpack_read_map_header(intfstream_t *fd)
{
   uint8_t  type = 0;
   uint64_t len  = 0;

   if (intfstream_read(fd, &type, 1) != 1)
      return -1;

   if (type == _MPF_NIL)
      return -2;

   if (type >= _MPF_FIXMAP && type < _MPF_FIXARRAY)
      return (int32_t)(type - _MPF_FIXMAP);

   if (type == _MPF_MAP16)
   {
      if (rmsgpack_read_uint(fd, &len, 2) == -1)
         return -1;
      return (int32_t)len;
   }
   if (type == _MPF_MAP32)
   {
      if (rmsgpack_read_uint(fd, &len, 4) == -1)
         return -1;
      return (int32_t)len;
   }

   return -1;
}

/**
 * rmsgpack_read_key_string:
 *
 * Read a MsgPack string value into a caller-supplied buffer without
 * allocating. Returns the string length, or -1 on error / not a string.
 * The output is NOT null-terminated if the buffer is exactly filled.
 */
static int32_t rmsgpack_read_key_string(intfstream_t *fd,
      char *buf, size_t buf_size)
{
   uint8_t  type = 0;
   uint64_t len  = 0;

   if (intfstream_read(fd, &type, 1) != 1)
      return -1;

   /* fixstr: length embedded in type byte */
   if (type >= _MPF_FIXSTR && type < _MPF_NIL)
   {
      len = type - _MPF_FIXSTR;
   }
   else if (type == _MPF_STR8)
   {
      if (rmsgpack_read_uint(fd, &len, 1) == -1)
         return -1;
   }
   else if (type == _MPF_STR16)
   {
      if (rmsgpack_read_uint(fd, &len, 2) == -1)
         return -1;
   }
   else if (type == _MPF_STR32)
   {
      if (rmsgpack_read_uint(fd, &len, 4) == -1)
         return -1;
   }
   else
      return -1;

   if (len >= buf_size)
   {
      /* Key too long for buffer — skip it */
      intfstream_seek(fd, (int64_t)len, RETRO_VFS_SEEK_POSITION_CURRENT);
      return -1;
   }

   if (intfstream_read(fd, buf, (size_t)len) != (int64_t)len)
      return -1;

   buf[len] = '\0';
   return (int32_t)len;
}

/* Maximum number of map fields in a single record for the
 * fast path. Typical .rdb records have 10-15 fields. */
#define CURSOR_MAX_MAP_FIELDS    24

/**
 * libretrodb_scan_field:
 *
 * Walk the database once, reporting the binary value of @field and
 * the offset of the record carrying it.  Records without the field,
 * or carrying it as something other than binary, are skipped.
 *
 * This exists so a caller can build its own lookup structure in one
 * pass instead of re-walking the whole database for every key it
 * wants to test.  Nothing is parsed beyond the field itself: values
 * the caller did not ask for are stepped over, not decoded.
 *
 * @cb is called for each match and may return non-zero to stop the
 * scan, which is then reported as success.
 *
 * Returns: 0 when the database was walked, -1 on a malformed stream.
 */
/**
 * rmsgpack_read_bin_inplace:
 *
 * Read a MsgPack binary value into a caller buffer.  Returns -1 and
 * leaves the stream positioned after the value when it is not binary
 * or does not fit, so the caller can carry on scanning.
 */
static int rmsgpack_read_bin_inplace(intfstream_t *fd, uint8_t *buf,
      size_t buf_size, uint64_t *len)
{
   uint8_t  type = 0;
   uint64_t n    = 0;

   if (intfstream_read(fd, &type, 1) != 1)
      return -1;

   switch (type)
   {
      case 0xc4:
         if (rmsgpack_read_uint(fd, &n, 1) == -1)
            return -1;
         break;
      case 0xc5:
         if (rmsgpack_read_uint(fd, &n, 2) == -1)
            return -1;
         break;
      case 0xc6:
         if (rmsgpack_read_uint(fd, &n, 4) == -1)
            return -1;
         break;
      default:
         /* Not binary: step back over the type byte and let the
          * generic skip deal with the whole value. */
         if (intfstream_seek(fd, -1, RETRO_VFS_SEEK_POSITION_CURRENT) < 0)
            return -1;
         if (rmsgpack_skip_value(fd) < 0)
            return -1;
         return -1;
   }

   if (n > buf_size)
   {
      if (intfstream_seek(fd, (int64_t)n,
               RETRO_VFS_SEEK_POSITION_CURRENT) < 0)
         return -1;
      return -1;
   }

   if (n && intfstream_read(fd, buf, n) != (int64_t)n)
      return -1;

   *len = n;
   return 0;
}

/**
 * rmsgpack_read_uint_inplace:
 *
 * Read a MsgPack unsigned (or non-negative signed) value.  Returns -1
 * and leaves the stream positioned after the value for anything else,
 * so the caller can carry on scanning.
 */
static int rmsgpack_read_uint_inplace(intfstream_t *fd, uint64_t *out)
{
   uint8_t  type = 0;
   uint64_t n    = 0;

   if (intfstream_read(fd, &type, 1) != 1)
      return -1;

   /* positive fixint */
   if (type < 0x80)
   {
      *out = type;
      return 0;
   }

   switch (type)
   {
      case 0xcc: case 0xcd: case 0xce: case 0xcf:
         if (rmsgpack_read_uint(fd, &n, (size_t)(1 << (type - 0xcc))) == -1)
            return -1;
         *out = n;
         return 0;
      default:
         break;
   }

   if (intfstream_seek(fd, -1, RETRO_VFS_SEEK_POSITION_CURRENT) < 0)
      return -1;
   if (rmsgpack_skip_value(fd) < 0)
      return -1;
   return -1;
}

int libretrodb_scan_field(libretrodb_t *db, const char *field,
      const char *aux_field, libretrodb_scan_cb cb, void *ctx)
{
   intfstream_t *fd;
   size_t        field_len;
   size_t        aux_len = 0;
   int           rv      = -1;

   if (!db || !db->path || !*db->path || !field || !*field || !cb)
      return -1;

   field_len = strlen(field);
   if (aux_field && *aux_field)
      aux_len = strlen(aux_field);

   if (!(fd = intfstream_open_buffered(db->path, LIBRETRODB_WINDOW_SIZE)))
      if (!(fd = intfstream_open_file(db->path,
                  RETRO_VFS_FILE_ACCESS_READ,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         return -1;

   if (intfstream_seek(fd, (int64_t)(db->root + sizeof(libretrodb_header_t)),
            RETRO_VFS_SEEK_POSITION_START) < 0)
      goto end;

   for (;;)
   {
      int64_t record_start = intfstream_tell(fd);
      int32_t  map_len;
      int32_t  i;
      int      stop        = 0;
      int      have_key    = 0;
      int      have_aux    = 0;
      uint8_t  key_val[LIBRETRODB_MAX_KEY_SIZE];
      uint64_t key_val_len = 0;
      uint64_t aux_val     = 0;

      if (record_start < 0)
         goto end;

      map_len = rmsgpack_read_map_header(fd);

      if (map_len == -2)   /* nil sentinel: end of records */
      {
         rv = 0;
         goto end;
      }
      if (map_len < 0)
         goto end;

      /* The field and its companion can sit either way round in the
       * map, so both are collected before the record is reported. */
      have_key = 0;
      have_aux = 0;

      for (i = 0; i < map_len; i++)
      {
         char    key_buf[64];
         int32_t key_len = rmsgpack_read_key_string(fd, key_buf,
               sizeof(key_buf));

         if (key_len < 0)
         {
            /* Key was unreadable or too long for the buffer; its
             * value still has to be stepped over. */
            if (rmsgpack_skip_value(fd) < 0)
               goto end;
            continue;
         }

         if (     !have_key
               && (size_t)key_len == field_len
               && memcmp(key_buf, field, field_len) == 0)
         {
            /* Not binary, or wider than any key we index: the reader
             * has already stepped past the value. */
            if (rmsgpack_read_bin_inplace(fd, key_val, sizeof(key_val),
                     &key_val_len) == 0)
               have_key = 1;
            continue;
         }

         if (     aux_len
               && !have_aux
               && (size_t)key_len == aux_len
               && memcmp(key_buf, aux_field, aux_len) == 0)
         {
            if (rmsgpack_read_uint_inplace(fd, &aux_val) == 0)
               have_aux = 1;
            continue;
         }

         if (rmsgpack_skip_value(fd) < 0)
            goto end;
      }

      /* A record carrying the companion field but not the key is still
       * reported, with a zero-length key, so a caller accumulating
       * something over the companion - a size range, say - sees every
       * record rather than only the ones it can index.  Reporting only
       * on the key gave a range narrower than the data, which is the
       * wrong direction for a range used to decide what to skip. */
      if (have_key || have_aux)
      {
         int64_t resume = intfstream_tell(fd);

         if (cb(ctx, have_key ? key_val : NULL,
                  have_key ? (size_t)key_val_len : 0,
                  (uint64_t)record_start, have_aux ? &aux_val : NULL))
            stop = 1;

         if (resume < 0
               || intfstream_seek(fd, resume,
                  RETRO_VFS_SEEK_POSITION_START) < 0)
            goto end;
         if (stop)
         {
            rv = 0;
            goto end;
         }
      }
   }

end:
   intfstream_close(fd);
   free(fd);
   return rv;
}

/**
 * libretrodb_read_at:
 *
 * Read the record beginning at @offset, which must have come from
 * libretrodb_scan_field().  Lets a caller that kept only offsets
 * fetch a record without walking to it.
 */
int libretrodb_read_at(libretrodb_t *db, uint64_t offset,
      struct rmsgpack_dom_value *out)
{
   intfstream_t *fd;
   int rv = -1;

   if (!db || !db->path || !*db->path || !out)
      return -1;

   if (!(fd = intfstream_open_buffered(db->path, LIBRETRODB_WINDOW_SIZE)))
      if (!(fd = intfstream_open_file(db->path,
                  RETRO_VFS_FILE_ACCESS_READ,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         return -1;

   if (intfstream_seek(fd, (int64_t)offset,
            RETRO_VFS_SEEK_POSITION_START) >= 0)
      rv = (rmsgpack_dom_read(fd, out) < 0) ? -1 : 0;

   intfstream_close(fd);
   free(fd);
   return rv;
}

int libretrodb_cursor_read_item(libretrodb_cursor_t *cursor,
      struct rmsgpack_dom_value *out)
{
   int rv;

   if (cursor->eof)
      return EOF;

   /* If no query is active, use the original full-DOM path */
   if (!cursor->query)
   {
      if ((rv = rmsgpack_dom_read(cursor->fd, out)) < 0)
         return rv;
      if (out->type == RDT_NULL)
      {
         cursor->eof = 1;
         return EOF;
      }
      return 0;
   }

   /* --- Fast path: folded field-level scan + inline evaluation --- */
   {
      int num_qfields;

      num_qfields = libretrodb_query_get_filter_fields(
            cursor->query, NULL, NULL, 0);

      /* If we can't extract field names (non-table query),
       * fall back to the full DOM path */
      if (num_qfields <= 0)
         goto slow_path;

      for (;;)
      {
         int32_t  map_len;
         int32_t  i;
         int64_t  record_start;
         int      conditions_met   = 0;
         int      rejected         = 0;
         int      skip_rest        = 0;
         int      error            = 0;

         /* Remember where this record starts so we can rewind
          * if the query matches */
         record_start = intfstream_tell(cursor->fd);
         if (record_start < 0)
            return -1;

         /* Read the map header */
         map_len = rmsgpack_read_map_header(cursor->fd);

         if (map_len == -2)
         {
            /* nil sentinel — end of records */
            cursor->eof = 1;
            return EOF;
         }

         if (map_len < 0 || map_len > CURSOR_MAX_MAP_FIELDS)
         {
            /* Not a map, or too many fields for fast path.
             * Rewind and fall through to slow path for this
             * one record, then continue with fast path. */
            intfstream_seek(cursor->fd, record_start,
                  RETRO_VFS_SEEK_POSITION_START);
            goto slow_path_single;
         }

         /* Scan fields: for each key-value pair, read the key into
          * a stack buffer, check if the query cares, and either skip
          * the value or parse + evaluate it immediately inline. */
         for (i = 0; i < map_len; i++)
         {
            char     key_buf[64];
            int32_t  key_len;

            if (skip_rest)
            {
               /* Already decided — skip both key and value */
               if (  rmsgpack_skip_value(cursor->fd) < 0
                  || rmsgpack_skip_value(cursor->fd) < 0)
               { error = 1; break; }
               continue;
            }

            /* Read the key string into stack buffer */
            key_len = rmsgpack_read_key_string(
                  cursor->fd, key_buf, sizeof(key_buf));

            if (key_len < 0)
            {
               /* Key read failed — skip value and continue */
               if (rmsgpack_skip_value(cursor->fd) < 0)
               { error = 1; break; }
               continue;
            }

            /* Evaluate this field against the query inline.
             * eval_field returns:
             *   -1 = field not in query (skip it)
             *    0 = condition failed (reject record)
             *    1 = condition passed */
            {
               struct rmsgpack_dom_value field_val;
               int eval_result;

               /* Peek: is this field in the query at all?
                * Check before parsing the value to avoid
                * unnecessary DOM allocation */
               eval_result = libretrodb_query_eval_field(
                     cursor->query, key_buf, (uint32_t)key_len,
                     NULL);

               if (eval_result == -1)
               {
                  /* Field not in query — skip value entirely */
                  if (rmsgpack_skip_value(cursor->fd) < 0)
                  { error = 1; break; }
                  continue;
               }

               /* Field IS in query — parse value and evaluate */
               if (rmsgpack_dom_read(cursor->fd, &field_val) < 0)
               { error = 1; break; }

               eval_result = libretrodb_query_eval_field(
                     cursor->query, key_buf, (uint32_t)key_len,
                     &field_val);

               rmsgpack_dom_value_free(&field_val);

               if (eval_result == 0)
               {
                  /* Condition failed — reject this record.
                   * Skip remaining fields to advance to next record. */
                  rejected  = 1;
                  skip_rest = 1;
               }
               else if (eval_result == 1)
               {
                  conditions_met++;
                  /* If all conditions satisfied, we can also skip
                   * remaining fields (they're not query-relevant) */
                  if (conditions_met >= num_qfields)
                     skip_rest = 1;
               }
            }
         }

         if (error)
            return -1;

         /* Reject: all conditions not met, or explicit mismatch */
         if (rejected || conditions_met < num_qfields)
            continue;

         /* Match! Rewind and do a full DOM parse so the caller
          * gets the complete record */
         intfstream_seek(cursor->fd, record_start,
               RETRO_VFS_SEEK_POSITION_START);

         if ((rv = rmsgpack_dom_read(cursor->fd, out)) < 0)
            return rv;

         return 0;
      }
   }

slow_path:
   /* Original full-DOM path — used when no query, or when the query
    * structure isn't a simple table filter */
   for (;;)
   {
slow_path_single:
      if ((rv = rmsgpack_dom_read(cursor->fd, out)) < 0)
         return rv;

      if (out->type == RDT_NULL)
      {
         cursor->eof = 1;
         return EOF;
      }

      if (cursor->query)
      {
         if (!libretrodb_query_filter(cursor->query, out))
         {
            rmsgpack_dom_value_free(out);
            continue;
         }
      }

      return 0;
   }
}

/**
 * libretrodb_cursor_close:
 * @cursor              : Handle to database cursor.
 *
 * Closes cursor and frees up allocated memory.
 **/
void libretrodb_cursor_close(libretrodb_cursor_t *cursor)
{
   if (!cursor)
      return;

   /* See libretrodb_close: intfstream_close does not free the
    * struct.  Match the convention. */
   if (cursor->fd)
   {
      intfstream_close(cursor->fd);
      free(cursor->fd);
   }

   if (cursor->query)
      libretrodb_query_free(cursor->query);

   cursor->is_valid = 0;
   cursor->eof      = 1;
   cursor->fd       = NULL;
   cursor->db       = NULL;
   cursor->query    = NULL;
}

/**
 * libretrodb_cursor_open:
 * @db                  : Handle to database.
 * @cursor              : Handle to database cursor.
 * @q                   : Query to execute.
 *
 * Opens cursor to database based on query @q.
 *
 * Returns: 0 if successful, otherwise negative.
 **/
int libretrodb_cursor_open(libretrodb_t *db,
      libretrodb_cursor_t *cursor,
      libretrodb_query_t *q)
{
   intfstream_t *fd = NULL;

   if (!db || !db->path || !*db->path)
      return -1;

   /* Walking the record stream is what a content scan spends its time
    * on, and most of that is the per-read trip through
    * filestream/VFS/fread rather than parsing.  A sliding window turns
    * those reads into a memcpy while keeping the resident cost fixed
    * at LIBRETRODB_WINDOW_SIZE instead of the size of the database. */
   if (!(fd = intfstream_open_buffered(db->path, LIBRETRODB_WINDOW_SIZE)))
      if (!(fd = intfstream_open_file(db->path,
                  RETRO_VFS_FILE_ACCESS_READ,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         return -1;

   cursor->fd       = fd;
   cursor->db       = db;
   cursor->is_valid = 1;
   libretrodb_cursor_reset(cursor);
   cursor->query    = q;

   if (q)
   {
      libretrodb_query_inc_ref(q);
      /* min()/max() accumulate across the rows a cursor yields, so
       * their state belongs to the walk, not to the compiled query.
       * Reset it here as well as at compile time: a query outlives
       * the cursor that references it, and a second walk over the
       * same query used to inherit the first walk's extreme. */
      libretrodb_query_reset_accumulator(q);
   }

   return 0;
}

/* bintree stores values by pointer and does not own them, so the
 * key buffers handed to bintree_insert() have to be released here.
 * They were not, leaking (key_size + 8) bytes per indexed record -
 * 240 KB for a 20000-record index. */
static int node_free_iter(void *value, void *ctx)
{
   (void)ctx;
   free(value);
   return 0;
}

static int node_iter(void *value, void *ctx)
{
   struct node_iter_ctx *nictx = (struct node_iter_ctx*)ctx;

   if (intfstream_write(nictx->db->fd, value,
                        (ssize_t)(nictx->idx->key_size + sizeof(uint64_t))) > 0)
      return 0;

   return -1;
}

static int node_compare(const void *a, const void *b, void *ctx)
{
   return memcmp(a, b, *(uint8_t *)ctx);
}

int libretrodb_create_index(libretrodb_t *db,
      const char *name, const char *field_name)
{
   struct node_iter_ctx nictx;
   struct rmsgpack_dom_value key;
   libretrodb_index_t idx;
   struct rmsgpack_dom_value item;
   libretrodb_cursor_t cur          = {0};
   struct rmsgpack_dom_value *field = NULL;
   void *buff                       = NULL;
   uint64_t *buff_u64               = NULL;
   uint8_t field_size               = 0;
   uint64_t item_loc                = intfstream_tell(db->fd);
   bintree_t *tree;
   uint64_t item_count              = 0;
   int rval                         = -1;

   if (libretrodb_find_index(db, name, &idx) >= 0)
     return 1;
   if (!db->can_write)
     return -1;

   tree = bintree_new(node_compare, &field_size);

   item.type                        = RDT_NULL;

   if (!tree || (libretrodb_cursor_open(db, &cur, NULL) != 0))
      goto clean;

   key.type                         = RDT_STRING;
   key.val.string.len               = (uint32_t)strlen(field_name);
   key.val.string.buff              = (char *)field_name;   /* We know we aren't going to change it */

   while (libretrodb_cursor_read_item(&cur, &item) == 0)
   {
      /* Only map keys are supported */
      if (item.type != RDT_MAP)
         goto clean;

      /* Field not found in item?  The free at the end of the loop is
       * skipped by this continue, so do it here. */
      if (!(field = rmsgpack_dom_value_map_value(&item, &key)))
      {
         rmsgpack_dom_value_free(&item);
         item.type = RDT_NULL;
         item_loc  = intfstream_tell(cur.fd);
         continue;
      }

      /* Field is not binary? */
      if (field->type != RDT_BINARY)
         goto clean;

      /* Field is empty? */
      if (field->val.binary.len == 0)
         goto clean;

      if (field_size == 0)
         field_size = field->val.binary.len;
      /* Field is not of correct size */
      else if (field->val.binary.len != field_size)
         goto clean;

      if (!(buff = malloc(field_size + sizeof(uint64_t))))
         goto clean;

      memcpy(buff, field->val.binary.buff, field_size);

      buff_u64 = (uint64_t *)((uint8_t *)buff + field_size);

      memcpy(buff_u64, &item_loc, sizeof(uint64_t));

      /* Value is not unique? */
      if (bintree_insert(tree, tree->root, buff) != 0)
      {
         rmsgpack_dom_value_print(field);
         goto clean;
      }
      item_count++;
      buff     = NULL;
      rmsgpack_dom_value_free(&item);
      item_loc = intfstream_tell(cur.fd);
   }
   rval = 0;

   intfstream_seek(db->fd, 0, RETRO_VFS_SEEK_POSITION_END);

   strlcpy(idx.name, name, sizeof(idx.name));

   idx.key_size = field_size;
   idx.next     = item_count * (field_size + sizeof(uint64_t));
   idx.count    = item_count;
   /* Write index header */
   rmsgpack_write_map_header(db->fd, 4);
   rmsgpack_write_string(db->fd, "name", STRLEN_CONST("name"));
   rmsgpack_write_string(db->fd, idx.name, (uint32_t)strlen(idx.name));
   rmsgpack_write_string(db->fd, "key_size", (uint32_t)STRLEN_CONST("key_size"));
   rmsgpack_write_uint  (db->fd, idx.key_size);
   rmsgpack_write_string(db->fd, "next", STRLEN_CONST("next"));
   rmsgpack_write_uint  (db->fd, idx.next);
   rmsgpack_write_string(db->fd, "count", STRLEN_CONST("count"));
   rmsgpack_write_uint  (db->fd, idx.count);

   nictx.db     = db;
   nictx.idx    = &idx;
   bintree_iterate(tree->root, node_iter, &nictx);

   intfstream_flush(db->fd);
clean:
   rmsgpack_dom_value_free(&item);
   if (buff)
      free(buff);
   if (cur.is_valid)
      libretrodb_cursor_close(&cur);
   if (tree && tree->root)
   {
      bintree_iterate(tree->root, node_free_iter, NULL);
      bintree_free(tree->root);
   }
   free(tree);
   return rval;
}

libretrodb_cursor_t *libretrodb_cursor_new(void)
{
   libretrodb_cursor_t *dbc = (libretrodb_cursor_t*)
      malloc(sizeof(*dbc));

   if (!dbc)
      return NULL;

   dbc->is_valid            = 0;
   dbc->fd                  = NULL;
   dbc->eof                 = 0;
   dbc->query               = NULL;
   dbc->db                  = NULL;

   return dbc;
}

void libretrodb_cursor_free(libretrodb_cursor_t *dbc)
{
   if (dbc)
      free(dbc);
}

libretrodb_t *libretrodb_new(void)
{
   libretrodb_t *db = (libretrodb_t*)malloc(sizeof(*db));

   if (!db)
      return NULL;

   db->fd                 = NULL;
   db->root               = 0;
   db->count              = 0;
   db->first_index_offset = 0;
   db->path               = NULL;

   return db;
}

void libretrodb_free(libretrodb_t *db)
{
   if (db)
      free(db);
}

/* Accessor for query.c, which owns the formatting but not the
 * storage.  Returns NULL when there is no db handle to borrow from;
 * the caller then falls back to a fixed message. */
char *libretrodb_query_err_buf(libretrodb_t *db, size_t *len)
{
   if (!db)
      return NULL;
   if (len)
      *len = sizeof(db->query_err);
   return db->query_err;
}
