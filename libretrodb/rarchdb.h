#ifndef __RARCHDB_H__
#define __RARCHDB_H__

#include <stdint.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "rmsgpack_dom.h"

typedef void rarchdb_query;

struct rarchdb
{
	int fd;
	uint64_t root;
	uint64_t count;
	uint64_t first_index_offset;
};

struct rarchdb_cursor
{
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

/**
 * rarchdb_cursor_open:
 * @db                  : Handle to database.
 * @cursor              : Handle to database cursor.
 * @q                   : Query to execute.
 *
 * Opens cursor to database based on query @q.
 *
 * Returns: 0 if successful, otherwise negative.
 **/
int rarchdb_cursor_open(
        struct rarchdb * db,
        struct rarchdb_cursor * cursor,
        rarchdb_query * query
);

/**
 * rarchdb_cursor_reset:
 * @cursor              : Handle to database cursor.
 *
 * Resets cursor.
 *
 * Returns: ???.
 **/
int rarchdb_cursor_reset(struct rarchdb_cursor * cursor);

/**
 * rarchdb_cursor_close:
 * @cursor              : Handle to database cursor.
 *
 * Closes cursor and frees up allocated memory.
 **/
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
