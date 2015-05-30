#ifndef __LIBRETRODB_H__
#define __LIBRETRODB_H__

#include <stdio.h>
#include <stdint.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "rmsgpack_dom.h"

#define MAGIC_NUMBER "RARCHDB"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libretrodb_query libretrodb_query_t;

typedef struct libretrodb
{
	FILE *fp;
	uint64_t root;
	uint64_t count;
	uint64_t first_index_offset;
   char path[1024];
} libretrodb_t;

typedef struct libretrodb_index
{
	char name[50];
	uint64_t key_size;
	uint64_t next;
} libretrodb_index_t;

typedef struct libretrodb_metadata
{
	uint64_t count;
} libretrodb_metadata_t;

typedef struct libretrodb_header
{
	char magic_number[sizeof(MAGIC_NUMBER)-1];
	uint64_t metadata_offset;
} libretrodb_header_t;

typedef struct libretrodb_cursor
{
	int is_valid;
	FILE *fp;
	int eof;
	libretrodb_query_t * query;
	libretrodb_t * db;
} libretrodb_cursor_t;

typedef int (* libretrodb_value_provider)(void * ctx,
      struct rmsgpack_dom_value * out);

int libretrodb_create(FILE *fp, libretrodb_value_provider value_provider,
      void * ctx);

void libretrodb_close(libretrodb_t * db);

int libretrodb_open(const char * path, libretrodb_t * db);

int libretrodb_create_index(libretrodb_t * db, const char *name,
      const char *field_name);

int libretrodb_find_entry(
        libretrodb_t * db,
        const char * index_name,
        const void * key,
        struct rmsgpack_dom_value * out
);

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
        libretrodb_query_t *query
);

/**
 * libretrodb_cursor_reset:
 * @cursor              : Handle to database cursor.
 *
 * Resets cursor.
 *
 * Returns: ???.
 **/
int libretrodb_cursor_reset(libretrodb_cursor_t * cursor);

/**
 * libretrodb_cursor_close:
 * @cursor              : Handle to database cursor.
 *
 * Closes cursor and frees up allocated memory.
 **/
void libretrodb_cursor_close(libretrodb_cursor_t * cursor);

void *libretrodb_query_compile(
        libretrodb_t * db,
        const char * query,
        size_t buff_len,
        const char ** error
);

void libretrodb_query_free(void *q);

int libretrodb_cursor_read_item(libretrodb_cursor_t * cursor,
      struct rmsgpack_dom_value * out);

#ifdef __cplusplus
}
#endif

#endif
