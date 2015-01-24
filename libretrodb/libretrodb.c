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

static int libretrodb_read_metadata(int fd, libretrodb_metadata_t *md)
{
   return rmsgpack_dom_read_into(fd, "count", &md->count, NULL);
}

static int libretrodb_write_metadata(int fd, libretrodb_metadata_t *md)
{
   rmsgpack_write_map_header(fd, 1);
   rmsgpack_write_string(fd, "count", strlen("count"));
   return rmsgpack_write_uint(fd, md->count);
}

static int validate_document(const struct rmsgpack_dom_value * doc)
{
   unsigned i;
   struct rmsgpack_dom_value key;
   struct rmsgpack_dom_value value;
   int rv = 0;

   if (doc->type != RDT_MAP)
      return -EINVAL;

   for (i = 0; i < doc->map.len; i++)
   {
      key = doc->map.items[i].key;
      value = doc->map.items[i].value;

      if (key.type != RDT_STRING)
         return -EINVAL;

      if (key.string.len <= 0)
         return -EINVAL;

      if (key.string.buff[0] == '$')
         return -EINVAL;

      if (value.type != RDT_MAP)
         continue;

      if ((rv == validate_document(&value)) != 0)
         return rv;
   }

   return rv;
}

int libretrodb_create(int fd, libretrodb_value_provider value_provider,
        void * ctx)
{
   int rv;
   off_t root;
   libretrodb_metadata_t md;
   uint64_t item_count = 0;
   struct rmsgpack_dom_value item = {};
   libretrodb_header_t header = {};

   memcpy(header.magic_number, MAGIC_NUMBER, sizeof(MAGIC_NUMBER)-1);
   root = lseek(fd, 0, SEEK_CUR);

   /* We write the header in the end because we need to know the size of
    * the db first */

   lseek(fd, sizeof(libretrodb_header_t), SEEK_CUR);

   while ((rv = value_provider(ctx, &item)) == 0)
   {
      if ((rv = validate_document(&item)) < 0)
         goto clean;

      if ((rv = rmsgpack_dom_write(fd, &item)) < 0)
         goto clean;

      item_count++;
   }

   if (rv < 0)
      goto clean;

   if ((rv = rmsgpack_dom_write(fd, &sentinal)) < 0)
      goto clean;

   header.metadata_offset = httobe64(lseek(fd, 0, SEEK_CUR));
   md.count = item_count;
   libretrodb_write_metadata(fd, &md);
   lseek(fd, root, SEEK_SET);
   write(fd, &header, sizeof(header));
clean:
   rmsgpack_dom_value_free(&item);
   return rv;
}

static int libretrodb_read_index_header(int fd, libretrodb_index_t *idx)
{
   uint64_t name_len = 50;
   return rmsgpack_dom_read_into(fd,
         "name", idx->name, &name_len,
         "key_size", &idx->key_size,
         "next", &idx->next, NULL);
}

static void libretrodb_write_index_header(int fd, libretrodb_index_t * idx)
{
	rmsgpack_write_map_header(fd, 3);
	rmsgpack_write_string(fd, "name", strlen("name"));
	rmsgpack_write_string(fd, idx->name, strlen(idx->name));
	rmsgpack_write_string(fd, "key_size", strlen("key_size"));
	rmsgpack_write_uint(fd, idx->key_size);
	rmsgpack_write_string(fd, "next", strlen("next"));
	rmsgpack_write_uint(fd, idx->next);
}

void libretrodb_close(libretrodb_t * db)
{
	close(db->fd);
	db->fd = -1;
}

int libretrodb_open(const char * path, libretrodb_t * db)
{
   libretrodb_header_t header;
   libretrodb_metadata_t md;
   int rv;
   int fd = open(path, O_RDWR);

   if (fd == -1)
      return -errno;

   db->root = lseek(fd, 0, SEEK_CUR);

   if ((rv = read(fd, &header, sizeof(header))) == -1)
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
   lseek(fd, header.metadata_offset, SEEK_SET);

   if (libretrodb_read_metadata(fd, &md) < 0)
   {
      rv = -EINVAL;
      goto error;
   }

   db->count = md.count;
   db->first_index_offset = lseek(fd, 0, SEEK_CUR);
   db->fd = fd;
   return 0;
error:
   close(fd);
   return rv;
}

static int libretrodb_find_index(libretrodb_t *db, const char *index_name,
      libretrodb_index_t *idx)
{
   off_t eof = lseek(db->fd, 0, SEEK_END);
   off_t offset = lseek(db->fd, db->first_index_offset, SEEK_SET);

   while (offset < eof)
   {
      libretrodb_read_index_header(db->fd, idx);
      if (strncmp(index_name, idx->name, strlen(idx->name)) == 0)
         return 0;
      offset = lseek(db->fd, idx->next, SEEK_CUR);
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
   else if (count == 0)
      return -1;
   else if (rv > 0)
      return binsearch(buff, item, mid, field_size, offset);

   return binsearch(current + item_size, item,
         count - mid, field_size, offset);
}

int libretrodb_find_entry(libretrodb_t *db, const char *index_name,
        const void *key, struct rmsgpack_dom_value *out)
{
   libretrodb_index_t idx;
   int rv;
   void * buff;
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
      void * buff_ = (uint64_t *)buff + nread;
      rv = read(db->fd, buff_, bufflen - nread);

      if (rv <= 0)
      {
         free(buff);
         return -errno;
      }
      nread += rv;
   }

   rv = binsearch(buff, key, db->count, idx.key_size, &offset);
   free(buff);

   if (rv == 0)
      lseek(db->fd, offset, SEEK_SET);

   rv = rmsgpack_dom_read(db->fd, out);
   if (rv < 0)
      return rv;


   return rv;
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
	return lseek(cursor->fd,
         cursor->db->root + sizeof(libretrodb_header_t),
         SEEK_SET);
}

int libretrodb_cursor_read_item(libretrodb_cursor_t *cursor,
      struct rmsgpack_dom_value * out)
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
         goto retry;
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
	close(cursor->fd);
	cursor->is_valid = 0;
	cursor->fd = -1;
	cursor->eof = 1;
	cursor->db = NULL;
	if (cursor->query)
		libretrodb_query_free(cursor->query);
	cursor->query = NULL;
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
int libretrodb_cursor_open(
        libretrodb_t *db,
        libretrodb_cursor_t *cursor,
        libretrodb_query_t *q
)
{
	cursor->fd = dup(db->fd);

	if (cursor->fd == -1)
		return -errno;

	cursor->db = db;
	cursor->is_valid = 1;
	libretrodb_cursor_reset(cursor);
	cursor->query = q;

	if (q)
		libretrodb_query_inc_ref(q);

	return 0;
}

static int node_iter(void * value, void * ctx)
{
	struct node_iter_ctx * nictx = (struct node_iter_ctx *)ctx;

	if (write(nictx->db->fd, value, nictx->idx->key_size + sizeof(uint64_t)) > 0)
		return 0;

	return -1;
}

static uint64_t libretrodb_tell(libretrodb_t *db)
{
	return lseek(db->fd, 0, SEEK_CUR);
}

int libretrodb_create_index(libretrodb_t *db,
      const char *name, const char *field_name)
{
	int rv;
	struct node_iter_ctx nictx;
	struct rmsgpack_dom_value key;
	libretrodb_index_t idx;
	struct rmsgpack_dom_value item;
	struct rmsgpack_dom_value * field;
	struct bintree tree;
	libretrodb_cursor_t cur;
	uint64_t idx_header_offset;
	void * buff = NULL;
	uint64_t * buff_u64 = NULL;
	uint8_t field_size = 0;
	uint64_t item_loc = libretrodb_tell(db);

	bintree_new(&tree, node_compare, &field_size);

	if (libretrodb_cursor_open(db, &cur, NULL) != 0)
   {
		rv = -1;
		goto clean;
	}

	key.type = RDT_STRING;
	key.string.len = strlen(field_name);
	/* We know we aren't going to change it */
	key.string.buff = (char *) field_name;
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

		if (field->binary.len == 0)
      {
			rv = -EINVAL;
			printf("field is empty\n");
			goto clean;
		}

		if (field_size == 0)
			field_size = field->binary.len;
		else if (field->binary.len != field_size)
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

		memcpy(buff, field->binary.buff, field_size);
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

	idx_header_offset = lseek(db->fd, 0, SEEK_END);
	strncpy(idx.name, name, 50);

	idx.name[49] = '\0';
	idx.key_size = field_size;
	idx.next = db->count * (field_size + sizeof(uint64_t));
	libretrodb_write_index_header(db->fd, &idx);

	nictx.db = db;
	nictx.idx = &idx;
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
