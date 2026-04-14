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
   char *path;
   bool can_write;
   uint64_t root;
   uint64_t count;
   uint64_t first_index_offset;
};

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

      if ((rv == libretrodb_validate_document(&value)) != 0)
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
   if (db->fd)
      intfstream_close(db->fd);
   if (db->path && *db->path)
      free(db->path);
   db->path = NULL;
   db->fd   = NULL;
}

int libretrodb_open(const char *path, libretrodb_t *db, bool write)
{
   libretrodb_header_t header;
   libretrodb_metadata_t md;
   unsigned mode = write ? RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING : RETRO_VFS_FILE_ACCESS_READ;
   intfstream_t *fd = intfstream_open_file(path, mode, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   db->can_write = write;
   if (!fd)
     return -1;

   if (db->path && *db->path)
      free(db->path);

   db->path  = strdup(path);
   db->root  = intfstream_tell(fd);

   if ((int)intfstream_read(fd, &header, sizeof(header)) == -1)
      goto error;

   if (strncmp(header.magic_number, MAGIC_NUMBER, sizeof(MAGIC_NUMBER)) != 0)
      goto error;

   header.metadata_offset = swap_if_little64(header.metadata_offset);
   intfstream_seek(fd, (ssize_t)header.metadata_offset,
         RETRO_VFS_SEEK_POSITION_START);

   if (rmsgpack_dom_read_into(fd, "count", &md.count, NULL) < 0)
      goto error;

   db->count              = md.count;
   db->first_index_offset = intfstream_tell(fd);
   db->fd                 = fd;
   return 0;

error:
   if (fd)
      intfstream_close(fd);
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
            "name",     idx->name, &name_len,
            "key_size", &idx->key_size,
            "next",     &idx->next,
            "count",    &idx->count,
                                 NULL) < 0)
      {
        printf("Invalid index header\n");
        break;
      }

      if (strncmp(index_name, idx->name, strlen(idx->name)) == 0)
         return 0;

      intfstream_seek(db->fd, (ssize_t)idx->next,
            RETRO_VFS_SEEK_POSITION_CURRENT);
   }

   return -1;
}

static int binsearch(const void *buff, const void *item,
      uint64_t count, uint8_t field_size, uint64_t *offset)
{
   int mid            = (int)(count / 2);
   int item_size      = field_size + sizeof(uint64_t);
   uint8_t *current   = ((uint8_t *)buff + (mid * item_size));
   int rv             = memcmp(current, item, field_size);

   if (rv == 0)
   {
      *offset         = *(uint64_t *)(current + field_size);
      return 0;
   }

   if (count == 0)
      return -1;

   if (rv > 0)
      return binsearch(buff, item, mid, field_size, offset);

   return binsearch(current + item_size, item,
         count - mid, field_size, offset);
}

int libretrodb_find_entry(libretrodb_t *db, const char *index_name,
      const void *key, struct rmsgpack_dom_value *out)
{
   libretrodb_index_t idx;
   int rv;
   uint8_t *buff;
   uint64_t offset;
   ssize_t bufflen, nread = 0;

   if (libretrodb_find_index(db, index_name, &idx) < 0)
      return -1;

   bufflen        = idx.next;
   if (!(buff = (uint8_t*)malloc(bufflen)))
      return -1;

   while (nread < bufflen)
   {
      void *buff_ = (buff + nread);
      rv          = (int)intfstream_read(db->fd, buff_, bufflen - nread);

      if (rv <= 0)
      {
         free(buff);
         return -1;
      }
      nread += rv;
   }

   rv = binsearch(buff, key, idx.count, (ssize_t)idx.key_size, &offset);
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

   if (intfstream_read(fd, &type, 1) == -1)
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

   if (intfstream_read(fd, &type, 1) == -1)
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

   if (intfstream_read(fd, buf, (size_t)len) == -1)
      return -1;

   buf[len] = '\0';
   return (int32_t)len;
}

/* Maximum number of query filter fields we can handle in the
 * fast path. If a query has more fields than this, we fall
 * back to the full DOM parse path. */
#define CURSOR_MAX_FILTER_FIELDS 8

/* Maximum number of map fields in a single record for the
 * partial DOM. Typical .rdb records have 10-15 fields. */
#define CURSOR_MAX_MAP_FIELDS    24

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

   /* --- Fast path: field-level skip for query-filtered scans --- */
   {
      const char *qfield_names[CURSOR_MAX_FILTER_FIELDS];
      uint32_t    qfield_lens[CURSOR_MAX_FILTER_FIELDS];
      int         num_qfields;

      num_qfields = libretrodb_query_get_filter_fields(
            cursor->query, qfield_names, qfield_lens,
            CURSOR_MAX_FILTER_FIELDS);

      /* If we can't extract field names (non-table query or too many
       * fields), fall back to the full DOM path */
      if (num_qfields <= 0 || num_qfields > CURSOR_MAX_FILTER_FIELDS)
         goto slow_path;

      for (;;)
      {
         int32_t  map_len;
         int32_t  i;
         int      matched;
         int64_t  record_start;

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

         /* Scan fields: read each key, check if the query cares,
          * skip or parse the value accordingly */
         {
            /* Partial DOM: only allocate pairs for matched fields.
             * We stack-allocate the pairs array and only malloc
             * the individual string/binary values that the query
             * actually needs to inspect. */
            struct rmsgpack_dom_pair pairs[CURSOR_MAX_MAP_FIELDS];
            unsigned partial_count = 0;
            int      skip_rest     = 0;
            int      error         = 0;

            for (i = 0; i < map_len; i++)
            {
               char     key_buf[64];
               int32_t  key_len;
               int      is_query_field = 0;
               int      j;

               if (skip_rest)
               {
                  /* Already know we don't match — skip both
                   * key and value to reach the next record */
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

               /* Check if this key is one the query filters on */
               for (j = 0; j < num_qfields; j++)
               {
                  if (  (uint32_t)key_len == qfield_lens[j]
                     && memcmp(key_buf, qfield_names[j], key_len) == 0)
                  {
                     is_query_field = 1;
                     break;
                  }
               }

               if (!is_query_field)
               {
                  /* Query doesn't care about this field — skip value */
                  if (rmsgpack_skip_value(cursor->fd) < 0)
                  { error = 1; break; }
                  continue;
               }

               /* Query needs this field — parse the value with full
                * DOM read. Set up the key in the partial DOM. */
               pairs[partial_count].key.type            = RDT_STRING;
               pairs[partial_count].key.val.string.len  = (uint32_t)key_len;
               /* Must malloc the key string because
                * rmsgpack_dom_value_free will free it */
               pairs[partial_count].key.val.string.buff =
                  (char*)malloc(key_len + 1);
               if (!pairs[partial_count].key.val.string.buff)
               { error = 1; break; }
               memcpy(pairs[partial_count].key.val.string.buff,
                     key_buf, key_len + 1);

               /* Read the value */
               if (rmsgpack_dom_read(cursor->fd,
                     &pairs[partial_count].value) < 0)
               {
                  free(pairs[partial_count].key.val.string.buff);
                  error = 1;
                  break;
               }

               partial_count++;
            }

            /* On error, clean up any partially parsed fields
             * and return failure */
            if (error)
            {
               unsigned pi;
               for (pi = 0; pi < partial_count; pi++)
               {
                  rmsgpack_dom_value_free(&pairs[pi].key);
                  rmsgpack_dom_value_free(&pairs[pi].value);
               }
               return -1;
            }

            /* Build partial map and run query filter */
            {
               struct rmsgpack_dom_value partial_map;
               partial_map.type         = RDT_MAP;
               partial_map.val.map.len  = partial_count;
               partial_map.val.map.items = pairs;

               matched = libretrodb_query_filter(
                     cursor->query, &partial_map);

               /* Free only the parsed fields (not the pairs array
                * itself — it's on the stack) */
               for (i = 0; i < (int32_t)partial_count; i++)
               {
                  rmsgpack_dom_value_free(&pairs[i].key);
                  rmsgpack_dom_value_free(&pairs[i].value);
               }
            }

            if (!matched)
               continue; /* Next record — the stream is already
                          * positioned past the current record */

            /* Match! Rewind and do a full DOM parse so the caller
             * gets the complete record */
            intfstream_seek(cursor->fd, record_start,
                  RETRO_VFS_SEEK_POSITION_START);

            if ((rv = rmsgpack_dom_read(cursor->fd, out)) < 0)
               return rv;

            return 0;
         }
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

   if (cursor->fd)
      intfstream_close(cursor->fd);

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
      libretrodb_query_inc_ref(q);

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

      /* Field not found in item? */
      if (!(field = rmsgpack_dom_value_map_value(&item, &key)))
        continue;

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
      bintree_free(tree->root);
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
