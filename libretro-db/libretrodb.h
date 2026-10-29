/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (libretrodb.h).
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

#ifndef __LIBRETRODB_H__
#define __LIBRETRODB_H__

#include <stdint.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <retro_common_api.h>

#include "query.h"
#include "rmsgpack_dom.h"

RETRO_BEGIN_DECLS

typedef struct libretrodb libretrodb_t;

typedef struct libretrodb_cursor libretrodb_cursor_t;

typedef struct libretrodb_index libretrodb_index_t;

typedef int (*libretrodb_value_provider)(void *ctx, struct rmsgpack_dom_value *out);

int libretrodb_create(intfstream_t *fd, libretrodb_value_provider value_provider, void *ctx);

void libretrodb_close(libretrodb_t *db);

int libretrodb_open(const char *path, libretrodb_t *db, bool write);

int libretrodb_create_index(libretrodb_t *db, const char *name,
      const char *field_name);

int libretrodb_find_entry(libretrodb_t *db, const char *index_name,
        const void *key, struct rmsgpack_dom_value *out);

/* Reported once per record that carries the scanned field, the second
 * field, the name field or the companion one: the binary values of the
 * two fields, the name field's string (NUL-terminated), the offset of
 * the record, and the companion numeric value when present (NULL
 * otherwise).  A field the record lacks comes as NULL with a length of
 * zero.
 *
 * A record that has only the companion is reported with @key NULL and
 * @key_len zero, so a caller accumulating over the companion sees
 * every record rather than only the ones it can key on.  A caller that
 * only wants keyed records should check @key_len first, as the
 * indexers here do.
 *
 * Returning non-zero stops the scan. */
typedef int (*libretrodb_scan2_cb)(void *ctx, const uint8_t *key,
      size_t key_len, const uint8_t *key2, size_t key2_len,
      const char *name, size_t name_len, uint64_t offset,
      const uint64_t *aux);

/* Walks a database a run of records at a time, reporting @field and
 * @field2, either of which may be NULL but not both, so a caller
 * indexing both walks it once.  @aux_field may be NULL; naming one
 * collects a numeric field in the same pass and reports the records
 * carrying only it.  @name_field may be NULL; naming one reports a
 * string field, one too long for a key as absent.  The scan reads
 * through a stream of its own, so @db may be closed once it has begun.
 * The field names are not copied and must outlive the scan. */
typedef struct libretrodb_scan libretrodb_scan_t;

libretrodb_scan_t *libretrodb_scan_begin(libretrodb_t *db,
      const char *field, const char *field2, const char *aux_field,
      const char *name_field);

/* Reports up to @max_records records, which must be non-zero.
 * Returns 1 while records remain, 0 once the walk is complete or @cb
 * stopped it, -1 on a read error. */
int libretrodb_scan_step(libretrodb_scan_t *scan, unsigned max_records,
      libretrodb_scan2_cb cb, void *ctx);

void libretrodb_scan_end(libretrodb_scan_t *scan);

int libretrodb_read_at(libretrodb_t *db, uint64_t offset,
      struct rmsgpack_dom_value *out);

libretrodb_t *libretrodb_new(void);

/* Scratch buffer used by libretrodb_query_compile() for error text.
 * Owned by the db handle so that concurrent compiles do not share it. */
char *libretrodb_query_err_buf(libretrodb_t *db, size_t *len);

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
        size_t buff_len, const char **err);

void libretrodb_query_free(void *q);

int libretrodb_cursor_read_item(libretrodb_cursor_t *cursor,
      struct rmsgpack_dom_value *out);

RETRO_END_DECLS

#endif
