#include "rarchdb.h"

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
#include "rarchdb_endian.h"

#define MAGIC_NUMBER "RARCHDB"

struct rarchdb_header
{
	char magic_number[sizeof(MAGIC_NUMBER)-1];
	uint64_t metadata_offset;
};

struct rarchdb_metadata
{
	uint64_t count;
};

struct rarchdb_index
{
	char name[50];
	uint64_t key_size;
	uint64_t next;
};

struct node_iter_ctx
{
	struct rarchdb *db;
	struct rarchdb_index *idx;
};

static struct rmsgpack_dom_value sentinal;

static int rarchdb_read_metadata(int fd, struct rarchdb_metadata *md)
{
	return rmsgpack_dom_read_into(fd, "count", &md->count, NULL);
}

static int rarchdb_write_metadata(int fd, struct rarchdb_metadata *md)
{
   rmsgpack_write_map_header(fd, 1);
   rmsgpack_write_string(fd, "count", strlen("count"));
   return rmsgpack_write_uint(fd, md->count);
}

int rarchdb_create(int fd, rarchdb_value_provider value_provider, void *ctx)
{
   int rv;
   uint64_t item_count = 0;
   struct rmsgpack_dom_value item = {};
   struct rarchdb_header header = {};
   struct rarchdb_metadata md;
   memcpy(header.magic_number, MAGIC_NUMBER, sizeof(MAGIC_NUMBER)-1);
   off_t root = lseek(fd, 0, SEEK_CUR);
   // We write the header in the end because we need to know the size of
   // the db first
   lseek(fd, sizeof(struct rarchdb_header), SEEK_CUR);
   while ((rv = value_provider(ctx, &item)) == 0)
   {
      if((rv = rmsgpack_dom_write(fd, &item)) < 0)
         return rv;
      rmsgpack_dom_value_free(&item);
      item_count++;
   }

   if (rv < 0)
      return rv;

   if((rv = rmsgpack_dom_write(fd, &sentinal)) < 0)
      return rv;

   header.metadata_offset = httobe64(lseek(fd, 0, SEEK_CUR));
   md.count = item_count;
   rarchdb_write_metadata(fd, &md);
   lseek(fd, root, SEEK_SET);
   write(fd, &header, sizeof(header));
   printf(
#ifdef _WIN32
   "Created DB with %I64u entries\n"
#else
   "Created DB with %llu entries\n"
#endif
   ,(unsigned long long)item_count);
   return 0;
}

static int rarchdb_read_index_header(int fd, struct rarchdb_index *idx)
{
   uint64_t name_len = 50;
   return rmsgpack_dom_read_into(fd,
         "name", idx->name, &name_len,
         "key_size", &idx->key_size,
         "next", &idx->next,
         NULL);
}

static void rarchdb_write_index_header(int fd, struct rarchdb_index *idx)
{
	rmsgpack_write_map_header(fd, 3);
	rmsgpack_write_string(fd, "name", strlen("name"));
	rmsgpack_write_string(fd, idx->name, strlen(idx->name));
	rmsgpack_write_string(fd, "key_size", strlen("key_size"));
	rmsgpack_write_uint(fd, idx->key_size);
	rmsgpack_write_string(fd, "next", strlen("next"));
	rmsgpack_write_uint(fd, idx->next);
}

void rarchdb_close(struct rarchdb *db)
{
	close(db->fd);
	db->fd = -1;
}

int rarchdb_open(const char *path, struct rarchdb *db)
{
   struct rarchdb_header header;
   struct rarchdb_metadata md;
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
   if (rarchdb_read_metadata(fd, &md) < 0)
   {
      rv = -EINVAL;
      goto error;
   }
   db->count = md.count;
   db->first_index_offset = lseek(fd, 0, SEEK_CUR);
   db->fd = fd;
   rarchdb_read_reset(db);
   return 0;
error:
   close(fd);
   return rv;
}

static int rarchdb_find_index(struct rarchdb *db, const char *index_name, struct rarchdb_index *idx)
{
   off_t eof = lseek(db->fd, 0, SEEK_END);
   off_t offset = lseek(db->fd, db->first_index_offset, SEEK_SET);
   while (offset < eof)
   {
      rarchdb_read_index_header(db->fd, idx);
      if (strncmp(index_name, idx->name, strlen(idx->name)) == 0)
         return 0;
      offset = lseek(db->fd, idx->next, SEEK_CUR);
   }
   return -1;
}

static int node_compare(const void *a, const void* b, void *ctx)
{
	return memcmp(a, b, *(uint8_t*)ctx);
}

static int binsearch(const void *buff, const void *item, uint64_t count, uint8_t field_size, uint64_t *offset)
{
   int mid = count / 2;
   int item_size = field_size + sizeof(uint64_t);
   uint64_t *current = (uint64_t*)buff + (mid * item_size);
   int rv = node_compare(current, item, &field_size);

   if (rv == 0)
   {
      *offset = *(uint64_t*)(current + field_size);
      return 0;
   }
   else if (count == 0)
      return -1;
   else if (rv > 0)
      return binsearch(buff, item, mid, field_size, offset);

   return binsearch(current + item_size, item, count - mid, field_size, offset);
}

int rarchdb_find_entry(struct rarchdb *db, const char *index_name, const void *key)
{
   struct rarchdb_index idx;
   int rv;
   void *buff;
   uint64_t offset;
   ssize_t bufflen, nread = 0;

   if (rarchdb_find_index(db, index_name, &idx) < 0)
   {
      rarchdb_read_reset(db);
      return -1;
   }
   
   bufflen = idx.next;
   buff = malloc(bufflen);
   
   if (!buff)
      return -ENOMEM;

   while (nread < bufflen)
   {
      void *buff_ = (uint64_t*)buff + nread;
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
   rarchdb_read_reset(db);

   if (rv == 0)
      lseek(db->fd, offset, SEEK_SET);

   return rv;
}

int rarchdb_read_reset(struct rarchdb *db)
{
   db->eof = 0;
   return lseek(db->fd, db->root + sizeof(struct rarchdb_header), SEEK_SET);
}

int rarchdb_read_item(struct rarchdb *db, struct rmsgpack_dom_value *out)
{
   int rv;
	if (db->eof)
		return EOF;

	rv = rmsgpack_dom_read(db->fd, out);
	if (rv < 0)
		return rv;

	if (out->type == RDT_NULL)
   {
		db->eof = 1;
		return EOF;
	}

	return 0;
}

static int node_iter(void *value, void *ctx)
{
	struct node_iter_ctx *nictx = (struct node_iter_ctx*)ctx;

	if (write(nictx->db->fd, value, nictx->idx->key_size + sizeof(uint64_t)) > 0)
		return 0;

   return -1;
}

uint64_t rarchdb_tell(struct rarchdb *db)
{
	return lseek(db->fd, 0, SEEK_CUR);
}

int rarchdb_create_index(struct rarchdb *db, const char* name, const char *field_name)
{
   int rv;
   struct node_iter_ctx nictx;
   struct rmsgpack_dom_value key;
   struct rarchdb_index idx;
   struct rmsgpack_dom_value item;
   struct rmsgpack_dom_value *field;
   void* buff = NULL;
   uint64_t *buff_u64 = NULL;
   uint8_t field_size = 0;
   struct bintree tree;
   uint64_t item_loc = rarchdb_tell(db);
   uint64_t idx_header_offset;

   bintree_new(&tree, node_compare, &field_size);
   rarchdb_read_reset(db);

   key.type = RDT_STRING;
   key.string.len = strlen(field_name);
   // We know we aren't going to change it
   key.string.buff = (char *) field_name;
   while(rarchdb_read_item(db, &item) == 0)
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
      if(!buff)
      {
         rv = -ENOMEM;
         goto clean;
      }

      memcpy(buff, field->binary.buff, field_size);
	  buff_u64 = (uint64_t*)buff + field_size;
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
      item_loc = rarchdb_tell(db);
   }

   (void)rv;
   (void)idx_header_offset;

   idx_header_offset = lseek(db->fd, 0, SEEK_END);
   strncpy(idx.name, name, 50);

   idx.name[49] = '\0';
   idx.key_size = field_size;
   idx.next = db->count * (field_size + sizeof(uint64_t));
   rarchdb_write_index_header(db->fd, &idx);

   nictx.db = db;
   nictx.idx = &idx;
   bintree_iterate(&tree, node_iter, &nictx);
   bintree_free(&tree);
clean:
   rmsgpack_dom_value_free(&item);
   if (buff)
      free(buff);
   rarchdb_read_reset(db);
   return 0;
}
