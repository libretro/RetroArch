#ifndef __LIBRETRODB_H__
#define __LIBRETRODB_H__

#include <stdint.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "query.h"
#include "rmsgpack_dom.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libretrodb libretrodb_t;

typedef struct libretrodb_cursor libretrodb_cursor_t;

typedef struct libretrodb_index libretrodb_index_t;

typedef int (*libretrodb_value_provider)(void *ctx, struct rmsgpack_dom_value *out);

int libretrodb_create(RFILE *fd, libretrodb_value_provider value_provider, void *ctx);

void libretrodb_close(libretrodb_t *db);

int libretrodb_open(const char *path, libretrodb_t *db);

int libretrodb_create_index(libretrodb_t *db, const char *name,
      const char *field_name);

int libretrodb_find_entry(libretrodb_t *db, const char *index_name,
        const void *key, struct rmsgpack_dom_value *out);

libretrodb_t *libretrodb_new(void);

void libretrodb_free(libretrodb_t *db);

libretrodb_cursor_t *libretrodb_cursor_new(void);

void libretrodb_cursor_free(libretrodb_cursor_t *dbc);

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
      libretrodb_query_t *query);

/**
 * libretrodb_cursor_reset:
 * @cursor              : Handle to database cursor.
 *
 * Resets cursor.
 *
 * Returns: ???.
 **/
int libretrodb_cursor_reset(libretrodb_cursor_t *cursor);

/**
 * libretrodb_cursor_close:
 * @cursor              : Handle to database cursor.
 *
 * Closes cursor and frees up allocated memory.
 **/
void libretrodb_cursor_close(libretrodb_cursor_t *cursor);

void *libretrodb_query_compile(libretrodb_t *db, const char *query,
        size_t buff_len, const char **error);

void libretrodb_query_free(void *q);

int libretrodb_cursor_read_item(libretrodb_cursor_t *cursor,
      struct rmsgpack_dom_value *out);

#ifdef __cplusplus
}
#endif

#endif
