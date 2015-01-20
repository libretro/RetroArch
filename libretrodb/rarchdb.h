#ifndef __RARCHDB_H__
#define __RARCHDB_H__

#include <stdint.h>
#include <unistd.h>
#include "rmsgpack_dom.h"

typedef void rarchdb_query;

struct rarchdb {
	int fd;
	uint64_t root;
	uint64_t count;
	uint64_t first_index_offset;
};

struct rarchdb_cursor {
	int is_valid;
	int fd;
	int eof;
	rarchdb_query * query;
	struct rarchdb * db;
};

typedef int (* rarchdb_value_provider)(
        void * ctx,
        struct rmsgpack_dom_value * out
);

int rarchdb_create(
        int fd,
        rarchdb_value_provider value_provider,
        void * ctx
);

void rarchdb_close(struct rarchdb * db);
int rarchdb_open(
        const char * path,
        struct rarchdb * db
);

int rarchdb_create_index(
        struct rarchdb * db,
        const char * name,
        const char * field_name
);
int rarchdb_find_entry(
        struct rarchdb * db,
        const char * index_name,
        const void * key,
        struct rmsgpack_dom_value * out
);

int rarchdb_cursor_open(
        struct rarchdb * db,
        struct rarchdb_cursor * cursor,
        rarchdb_query * query
);

int rarchdb_cursor_reset(struct rarchdb_cursor * cursor);

void rarchdb_cursor_close(struct rarchdb_cursor * cursor);

rarchdb_query * rarchdb_query_compile(
        struct rarchdb * db,
        const char * query,
        size_t buff_len,
        const char ** error
);
void rarchdb_query_free(rarchdb_query * q);

int rarchdb_cursor_read_item(
        struct rarchdb_cursor * cursor,
        struct rmsgpack_dom_value * out
);

#endif
