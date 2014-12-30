#ifndef __RARCHDB_H__
#define __RARCHDB_H__

#include <stdint.h>
#include "rmsgpack_dom.h"

struct rarchdb {
	int fd;
	int eof;
	uint64_t root;
	uint64_t count;
	uint64_t first_index_offset;
};


typedef int(*rarchdb_value_provider)(void *ctx, struct rmsgpack_dom_value *out);

int rarchdb_create(int fd, rarchdb_value_provider value_provider, void *ctx);

void rarchdb_close(struct rarchdb *db);
int rarchdb_open(const char *path, struct rarchdb *db);

int rarchdb_read_item(struct rarchdb *db, struct rmsgpack_dom_value *out);
int rarchdb_read_reset(struct rarchdb *db);
int rarchdb_create_index(struct rarchdb *db, const char* name, const char *field_name);
int rarchdb_find_entry(struct rarchdb *db, const char *index_name, const void *key);

#endif
