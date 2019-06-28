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

#include <stdio.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <string.h>
#include <errno.h>
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

struct node_iter_ctx
{
	libretrodb_t *db;
	libretrodb_index_t *idx;
};

struct libretrodb
{
	RFILE *fd;
	uint64_t root;
	uint64_t count;
	uint64_t first_index_offset;
   char *path;
};

struct libretrodb_index
{
	char name[50];
	uint64_t key_size;
	uint64_t next;
};

typedef struct libretrodb_metadata
{
	uint64_t count;
} libretrodb_metadata_t;

typedef struct libretrodb_header
{
	char magic_number[sizeof(MAGIC_NUMBER)-1];
	uint64_t metadata_offset;
} libretrodb_header_t;

struct libretrodb_cursor
{
	int is_valid;
   RFILE *fd;
	int eof;
	libretrodb_query_t *query;
	libretrodb_t *db;
};

static struct rmsgpack_dom_value sentinal;

static int libretrodb_read_metadata(RFILE *fd, libretrodb_metadata_t *md)
{
   return rmsgpack_dom_read_into(fd, "count", &md->count, NULL);
}

static int libretrodb_write_metadata(RFILE *fd, libretrodb_metadata_t *md)
{
   rmsgpack_write_map_header(fd, 1);
   rmsgpack_write_string(fd, "count", STRLEN_CONST("count"));
   return rmsgpack_write_uint(fd, md->count);
}

static int libretrodb_validate_document(const struct rmsgpack_dom_value *doc)
{
   unsigned i;
   struct rmsgpack_dom_value key, value;
   int rv = 0;

   if (doc->type != RDT_MAP)
      return -EINVAL;

   for (i = 0; i < doc->val.map.len; i++)
   {
      key = doc->val.map.items[i].key;
      value = doc->val.map.items[i].value;

      if (key.type != RDT_STRING)
         return -EINVAL;

      if (key.val.string.len <= 0)
         return -EINVAL;

      if (key.val.string.buff[0] == '$')
         return -EINVAL;

      if (value.type != RDT_MAP)
         continue;

      if ((rv == libretrodb_validate_document(&value)) != 0)
         return rv;
   }

   return rv;
}

int libretrodb_create(RFILE *fd, libretrodb_value_provider value_provider,
      void *ctx)
{
   int rv;
   libretrodb_metadata_t md;
   struct rmsgpack_dom_value item;
   uint64_t item_count        = 0;
   libretrodb_header_t header = {{0}};
   ssize_t root = filestream_tell(fd);

   memcpy(header.magic_number, MAGIC_NUMBER, sizeof(MAGIC_NUMBER)-1);

   /* We write the header in the end because we need to know the size of
    * the db first */

   filestream_seek(fd, sizeof(libretrodb_header_t),
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

   header.metadata_offset = swap_if_little64(filestream_seek(
            fd, 0, RETRO_VFS_SEEK_POSITION_CURRENT));
   md.count = item_count;
   libretrodb_write_metadata(fd, &md);
   filestream_seek(fd, root, RETRO_VFS_SEEK_POSITION_START);
   filestream_write(fd, &header, sizeof(header));
clean:
   rmsgpack_dom_value_free(&item);
   return rv;
}

static int libretrodb_read_index_header(RFILE *fd, libretrodb_index_t *idx)
{
   uint64_t name_len = 50;
   return rmsgpack_dom_read_into(fd,
         "name", idx->name, &name_len,
         "key_size", &idx->key_size,
         "next", &idx->next, NULL);
}

static void libretrodb_write_index_header(RFILE *fd, libretrodb_index_t *idx)
{
   rmsgpack_write_map_header(fd, 3);
   rmsgpack_write_string(fd, "name", STRLEN_CONST("name"));
   rmsgpack_write_string(fd, idx->name, (uint32_t)strlen(idx->name));
   rmsgpack_write_string(fd, "key_size", (uint32_t)STRLEN_CONST("key_size"));
   rmsgpack_write_uint(fd, idx->key_size);
   rmsgpack_write_string(fd, "next", STRLEN_CONST("next"));
   rmsgpack_write_uint(fd, idx->next);
}

void libretrodb_close(libretrodb_t *db)
{
   if (db->fd)
      filestream_close(db->fd);
   if (!string_is_empty(db->path))
      free(db->path);
   db->path = NULL;
   db->fd   = NULL;
}

int libretrodb_open(const char *path, libretrodb_t *db)
{
   libretrodb_header_t header;
   libretrodb_metadata_t md;
   int rv    = 0;
   RFILE *fd = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fd)
      return -errno;

   if (!string_is_empty(db->path))
      free(db->path);

   db->path  = strdup(path);
   db->root  = filestream_tell(fd);

   if ((rv = (int)filestream_read(fd, &header, sizeof(header))) == -1)
   {
      rv = -errno;
      goto error;
   }

   if (strncmp(header.magic_number, MAGIC_NUMBER, sizeof(MAGIC_NUMBER)) != 0)
   {
      rv = -EINVAL;
      goto error;
   }

   header.metadata_offset = swap_if_little64(header.metadata_offset);
   filestream_seek(fd, (ssize_t)header.metadata_offset,
         RETRO_VFS_SEEK_POSITION_START);

   if (libretrodb_read_metadata(fd, &md) < 0)
   {
      rv = -EINVAL;
      goto error;
   }

   db->count              = md.count;
   db->first_index_offset = filestream_tell(fd);
   db->fd                 = fd;
   return 0;

error:
   if (fd)
      filestream_close(fd);
   return rv;
}

static int libretrodb_find_index(libretrodb_t *db, const char *index_name,
      libretrodb_index_t *idx)
{
   ssize_t eof    = filestream_get_size(db->fd);
   ssize_t offset = filestream_seek(db->fd,
         (ssize_t)db->first_index_offset,
         RETRO_VFS_SEEK_POSITION_START);

   /* TODO: this should use filestream_eof instead */
   while (offset < eof)
   {
      libretrodb_read_index_header(db->fd, idx);

      if (strncmp(index_name, idx->name, strlen(idx->name)) == 0)
         return 0;

      filestream_seek(db->fd, (ssize_t)idx->next,
            RETRO_VFS_SEEK_POSITION_CURRENT);
      offset = filestream_tell(db->fd);
   }

   return -1;
}

static int binsearch(const void *buff, const void *item,
      uint64_t count, uint8_t field_size, uint64_t *offset)
{
   int mid            = (int)(count / 2);
   int item_size      = field_size + sizeof(uint64_t);
   uint64_t *current  = (uint64_t *)buff + (mid * item_size);
   int rv             = memcmp(current, item, field_size);

   if (rv == 0)
   {
      *offset = *(uint64_t *)(current + field_size);
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
   void *buff;
   uint64_t offset;
   ssize_t bufflen, nread = 0;

   if (libretrodb_find_index(db, index_name, &idx) < 0)
      return -1;

   bufflen = idx.next;
   buff = malloc(bufflen);

   if (!buff)
      return -ENOMEM;

   while (nread < bufflen)
   {
      void *buff_ = (uint64_t *)buff + nread;
      rv = (int)filestream_read(db->fd, buff_, bufflen - nread);

      if (rv <= 0)
      {
         free(buff);
         return -errno;
      }
      nread += rv;
   }

   rv = binsearch(buff, key, db->count, (ssize_t)idx.key_size, &offset);
   free(buff);

   if (rv == 0)
      filestream_seek(db->fd, (ssize_t)offset,
            RETRO_VFS_SEEK_POSITION_START);

   return rmsgpack_dom_read(db->fd, out);
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
   return (int)filestream_seek(cursor->fd,
         (ssize_t)(cursor->db->root + sizeof(libretrodb_header_t)),
         RETRO_VFS_SEEK_POSITION_START);
}

int libretrodb_cursor_read_item(libretrodb_cursor_t *cursor,
      struct rmsgpack_dom_value *out)
{
   int rv;

   if (cursor->eof)
      return EOF;

retry:
   rv = rmsgpack_dom_read(cursor->fd, out);
   if (rv < 0)
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
         goto retry;
      }
   }

   return 0;
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
      filestream_close(cursor->fd);

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
int libretrodb_cursor_open(libretrodb_t *db, libretrodb_cursor_t *cursor,
      libretrodb_query_t *q)
{
   RFILE *fd = NULL;
   if (!db || string_is_empty(db->path))
      return -errno;

   fd = filestream_open(db->path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fd)
      return -errno;

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

   if (filestream_write(nictx->db->fd, value,
            (ssize_t)(nictx->idx->key_size + sizeof(uint64_t))) > 0)
      return 0;

   return -1;
}

static uint64_t libretrodb_tell(libretrodb_t *db)
{
   return filestream_tell(db->fd);
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
   uint64_t item_loc                = libretrodb_tell(db);
   bintree_t *tree                  = bintree_new(node_compare, &field_size);

   item.type                        = RDT_NULL;

   if (!tree || (libretrodb_cursor_open(db, &cur, NULL) != 0))
      goto clean;

   key.type            = RDT_STRING;
   key.val.string.len  = (uint32_t)strlen(field_name);
   key.val.string.buff = (char *) field_name;   /* We know we aren't going to change it */

   while (libretrodb_cursor_read_item(&cur, &item) == 0)
   {
      if (item.type != RDT_MAP)
      {
         printf("Only map keys are supported\n");
         goto clean;
      }

      field = rmsgpack_dom_value_map_value(&item, &key);

      if (!field)
      {
         printf("field not found in item\n");
         goto clean;
      }

      if (field->type != RDT_BINARY)
      {
         printf("field is not binary\n");
         goto clean;
      }

      if (field->val.binary.len == 0)
      {
         printf("field is empty\n");
         goto clean;
      }

      if (field_size == 0)
         field_size = field->val.binary.len;
      else if (field->val.binary.len != field_size)
      {
         printf("field is not of correct size\n");
         goto clean;
      }

      buff = malloc(field_size + sizeof(uint64_t));
      if (!buff)
         goto clean;

      memcpy(buff, field->val.binary.buff, field_size);

      buff_u64 = (uint64_t *)buff + field_size;

      memcpy(buff_u64, &item_loc, sizeof(uint64_t));

      if (bintree_insert(tree, buff) != 0)
      {
         printf("Value is not unique: ");
         rmsgpack_dom_value_print(field);
         printf("\n");
         goto clean;
      }
      buff     = NULL;
      rmsgpack_dom_value_free(&item);
      item_loc = libretrodb_tell(db);
   }

   filestream_seek(db->fd, 0, RETRO_VFS_SEEK_POSITION_END);

   strncpy(idx.name, name, 50);

   idx.name[49] = '\0';
   idx.key_size = field_size;
   idx.next     = db->count * (field_size + sizeof(uint64_t));
   libretrodb_write_index_header(db->fd, &idx);

   nictx.db  = db;
   nictx.idx = &idx;
   bintree_iterate(tree, node_iter, &nictx);

clean:
   rmsgpack_dom_value_free(&item);
   if (buff)
      free(buff);
   if (cur.is_valid)
      libretrodb_cursor_close(&cur);
   if (tree)
      bintree_free(tree);
   free(tree);
   return 0;
}

libretrodb_cursor_t *libretrodb_cursor_new(void)
{
   libretrodb_cursor_t *dbc = (libretrodb_cursor_t*)
      calloc(1, sizeof(*dbc));

   if (!dbc)
      return NULL;

   return dbc;
}

void libretrodb_cursor_free(libretrodb_cursor_t *dbc)
{
   if (!dbc)
      return;

   free(dbc);
}

libretrodb_t *libretrodb_new(void)
{
   libretrodb_t *db = (libretrodb_t*)calloc(1, sizeof(*db));

   if (!db)
      return NULL;

   return db;
}

void libretrodb_free(libretrodb_t *db)
{
   if (!db)
      return;

   free(db);
}
