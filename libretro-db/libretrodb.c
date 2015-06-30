#include "libretrodb.h"

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
#include <fcntl.h>

#include <stdio.h>

#include "rmsgpack_dom.h"
#include "rmsgpack.h"
#include "bintree.h"
#include "libretrodb_endian.h"
#include "query.h"

struct node_iter_ctx
{
   libretrodb_t *db;
   libretrodb_index_t *idx;
};

static struct rmsgpack_dom_value sentinal;

static INLINE off_t flseek(FILE *fp, int offset, int whence)
{
   if (fseek(fp, offset, whence) != 0)
      return (off_t)-1;
   return (off_t)ftell(fp);
}

static int libretrodb_read_metadata(FILE *fp, libretrodb_metadata_t *md)
{
   return rmsgpack_dom_read_into(fp, "count", &md->count, NULL);
}

static int libretrodb_write_metadata(FILE *fp, libretrodb_metadata_t *md)
{
   rmsgpack_write_map_header(fp, 1);
   rmsgpack_write_string(fp, "count", strlen("count"));
   return rmsgpack_write_uint(fp, md->count);
}

static int validate_document(const struct rmsgpack_dom_value * doc)
{
   unsigned i;
   struct rmsgpack_dom_value key, value;
   int rv = 0;

   if (doc->type != RDT_MAP)
      return -EINVAL;

   for (i = 0; i < doc->val.map.len; i++)
   {
      key   = doc->val.map.items[i].key;
      value = doc->val.map.items[i].value;

      if (key.type != RDT_STRING)
         return -EINVAL;

      if (key.val.string.len <= 0)
         return -EINVAL;

      if (key.val.string.buff[0] == '$')
         return -EINVAL;

      if (value.type != RDT_MAP)
         continue;

      if ((rv == validate_document(&value)) != 0)
         return rv;
   }

   return rv;
}

int libretrodb_create(FILE *fp, libretrodb_value_provider value_provider,
      void * ctx)
{
   int rv;
   off_t root;
   libretrodb_metadata_t md;
   struct rmsgpack_dom_value item;
   uint64_t item_count            = 0;
   libretrodb_header_t header     = {{0}};

   memcpy(header.magic_number, MAGIC_NUMBER, sizeof(MAGIC_NUMBER)-1);
   root = flseek(fp, 0, SEEK_CUR);

   /* We write the header in the end because we need to know the size of
    * the db first */

   flseek(fp, sizeof(libretrodb_header_t), SEEK_CUR);

   while ((rv = value_provider(ctx, &item)) == 0)
   {
      if ((rv = validate_document(&item)) < 0)
         goto clean;

      if ((rv = rmsgpack_dom_write(fp, &item)) < 0)
         goto clean;

      item_count++;
   }

   if (rv < 0)
      goto clean;

   if ((rv = rmsgpack_dom_write(fp, &sentinal)) < 0)
      goto clean;

   header.metadata_offset = httobe64(flseek(fp, 0, SEEK_CUR));
   md.count = item_count;
   libretrodb_write_metadata(fp, &md);
   flseek(fp, root, SEEK_SET);
   fwrite(&header, 1, sizeof(header), fp);
clean:
   rmsgpack_dom_value_free(&item);
   return rv;
}

static int libretrodb_read_index_header(FILE *fp, libretrodb_index_t *idx)
{
   uint64_t name_len = 50;
   return rmsgpack_dom_read_into(fp,
         "name", idx->name, &name_len,
         "key_size", &idx->key_size,
         "next", &idx->next, NULL);
}

static void libretrodb_write_index_header(FILE *fp, libretrodb_index_t * idx)
{
   rmsgpack_write_map_header(fp, 3);
   rmsgpack_write_string(fp, "name", strlen("name"));
   rmsgpack_write_string(fp, idx->name, strlen(idx->name));
   rmsgpack_write_string(fp, "key_size", strlen("key_size"));
   rmsgpack_write_uint(fp, idx->key_size);
   rmsgpack_write_string(fp, "next", strlen("next"));
   rmsgpack_write_uint(fp, idx->next);
}

void libretrodb_close(libretrodb_t *db)
{
   if (!db)
      return;

   fclose(db->fp);
   db->fp = NULL;
}

int libretrodb_open(const char *path, libretrodb_t *db)
{
   int rv;
   libretrodb_header_t header;
   libretrodb_metadata_t md;
   FILE *fp = fopen(path, "rb");

   if (fp == NULL)
      return -errno;

   strcpy(db->path, path);
   db->root = flseek(fp, 0, SEEK_CUR);

   if ((rv = fread(&header, 1, sizeof(header), fp)) != sizeof(header))
   {
      rv = -errno;
      goto error;
   }

   if (strncmp(header.magic_number, MAGIC_NUMBER, sizeof(MAGIC_NUMBER)) != 0)
   {
      rv = -EINVAL;
      goto error;
   }

   header.metadata_offset = betoht64(header.metadata_offset);
   flseek(fp, (int)header.metadata_offset, SEEK_SET);

   if (libretrodb_read_metadata(fp, &md) < 0)
   {
      rv = -EINVAL;
      goto error;
   }

   db->count              = md.count;
   db->first_index_offset = flseek(fp, 0, SEEK_CUR);
   db->fp                 = fp;
   return 0;
error:
   fclose(fp);
   return rv;
}

static int libretrodb_find_index(libretrodb_t *db, const char *index_name,
      libretrodb_index_t *idx)
{
   off_t eof    = flseek(db->fp, 0, SEEK_END);
   off_t offset = flseek(db->fp, (int)db->first_index_offset, SEEK_SET);

   while (offset < eof)
   {
      libretrodb_read_index_header(db->fp, idx);

      if (strncmp(index_name, idx->name, strlen(idx->name)) == 0)
         return 0;

      offset = flseek(db->fp, (int)idx->next, SEEK_CUR);
   }

   return -1;
}

static int node_compare(const void * a, const void * b, void * ctx)
{
   return memcmp(a, b, *(uint8_t *)ctx);
}

static int binsearch(const void * buff, const void * item,
      uint64_t count, uint8_t field_size, uint64_t * offset)
{
   int mid            = count / 2;
   int item_size      = field_size + sizeof(uint64_t);
   uint64_t * current = (uint64_t *)buff + (mid * item_size);
   int rv             = node_compare(current, item, &field_size);

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
   uint64_t offset;
   void *buff = NULL;
   ssize_t bufflen, nread = 0;

   if (libretrodb_find_index(db, index_name, &idx) < 0)
      return -1;

   bufflen = idx.next;
   buff    = malloc(bufflen);

   if (!buff)
      return -ENOMEM;

   while (nread < bufflen)
   {
      void *buff_ = (uint64_t *)buff + nread;
      rv = fread(buff_, 1, bufflen - nread, db->fp);

      if (rv <= 0)
      {
         free(buff);
         return -errno;
      }
      nread += rv;
   }

   rv = binsearch(buff, key, db->count, (uint8_t)idx.key_size, &offset);
   free(buff);

   if (rv == 0)
      flseek(db->fp, (int)offset, SEEK_SET);

   return rmsgpack_dom_read(db->fp, out);
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
   return flseek(cursor->fp,
         (int)cursor->db->root + sizeof(libretrodb_header_t),
         SEEK_SET);
}

int libretrodb_cursor_read_item(libretrodb_cursor_t *cursor,
      struct rmsgpack_dom_value * out)
{
   int rv;

   if (cursor->eof)
      return EOF;

retry:
   rv = rmsgpack_dom_read(cursor->fp, out);
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

   fclose(cursor->fp);

   if (cursor->query)
      libretrodb_query_free(cursor->query);

   cursor->is_valid = 0;
   cursor->fp       = NULL;
   cursor->eof      = 1;
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
   cursor->fp = fopen(db->path, "rb");

   if (cursor->fp == NULL)
      return -errno;

   cursor->db       = db;
   cursor->is_valid = 1;

   libretrodb_cursor_reset(cursor);

   cursor->query    = q;

   if (q)
      libretrodb_query_inc_ref(q);

   return 0;
}

static int node_iter(void * value, void * ctx)
{
   struct node_iter_ctx *nictx = (struct node_iter_ctx*)ctx;
   size_t size = nictx->idx->key_size + sizeof(uint64_t);

   if (fwrite(value, 1, size, nictx->db->fp) == size)
      return 0;

   return -1;
}

static uint64_t libretrodb_tell(libretrodb_t *db)
{
   return (uint64_t)ftell(db->fp);
}

int libretrodb_create_index(libretrodb_t *db,
      const char *name, const char *field_name)
{
   int rv;
   struct node_iter_ctx nictx;
   struct rmsgpack_dom_value key;
   libretrodb_index_t idx;
   struct rmsgpack_dom_value item;
   struct rmsgpack_dom_value *field;
   struct bintree tree;
   libretrodb_cursor_t cur    = {0};
   void *buff                 = NULL;
   uint64_t *buff_u64         = NULL;
   uint64_t idx_header_offset = 0;
   uint8_t field_size         = 0;
   uint64_t item_loc          = libretrodb_tell(db);

   bintree_new(&tree, node_compare, &field_size);

   if (libretrodb_cursor_open(db, &cur, NULL) != 0)
   {
      rv = -1;
      goto clean;
   }

   key.type        = RDT_STRING;
   key.val.string.len  = strlen(field_name);

   /* We know we aren't going to change it */
   key.val.string.buff = (char*)field_name;

   while (libretrodb_cursor_read_item(&cur, &item) == 0)
   {
      if (item.type != RDT_MAP)
      {
         rv = -EINVAL;
         printf("Only map keys are supported\n");
         goto clean;
      }

      field = rmsgpack_dom_value_map_value(&item, &key);

      if (!field)
      {
         rv = -EINVAL;
         printf("field not found in item\n");
         goto clean;
      }

      if (field->type != RDT_BINARY)
      {
         rv = -EINVAL;
         printf("field is not binary\n");
         goto clean;
      }

      if (field->val.binary.len == 0)
      {
         rv = -EINVAL;
         printf("field is empty\n");
         goto clean;
      }

      if (field_size == 0)
         field_size = field->val.binary.len;
      else if (field->val.binary.len != field_size)
      {
         rv = -EINVAL;
         printf("field is not of correct size\n");
         goto clean;
      }

      buff = malloc(field_size + sizeof(uint64_t));
      if (!buff)
      {
         rv = -ENOMEM;
         goto clean;
      }

      memcpy(buff, field->val.binary.buff, field_size);

      buff_u64 = (uint64_t *)buff + field_size;

      memcpy(buff_u64, &item_loc, sizeof(uint64_t));

      if (bintree_insert(&tree, buff) != 0)
      {
         printf("Value is not unique: ");
         rmsgpack_dom_value_print(field);
         printf("\n");
         rv = -EINVAL;
         goto clean;
      }
      buff = NULL;
      rmsgpack_dom_value_free(&item);
      item_loc = libretrodb_tell(db);
   }

   (void)rv;
   (void)idx_header_offset;

   idx_header_offset = flseek(db->fp, 0, SEEK_END);
   strncpy(idx.name, name, 50);

   idx.name[49] = '\0';
   idx.key_size = field_size;
   idx.next     = db->count * (field_size + sizeof(uint64_t));
   libretrodb_write_index_header(db->fp, &idx);

   nictx.db     = db;
   nictx.idx    = &idx;
   bintree_iterate(&tree, node_iter, &nictx);
   bintree_free(&tree);
clean:
   rmsgpack_dom_value_free(&item);
   if (buff)
      free(buff);
   if (cur.is_valid)
      libretrodb_cursor_close(&cur);
   return 0;
}
