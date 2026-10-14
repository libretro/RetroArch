/* Stand-in for Emscripten's emscripten/fetch.h on the host: the fields
 * and calls tasks/task_http_emscripten.c uses, with the types the real
 * header gives them.  The test supplies the calls. */
#ifndef HTTP_EMSCRIPTEN_TEST_FETCH_H
#define HTTP_EMSCRIPTEN_TEST_FETCH_H

#include <stddef.h>
#include <stdint.h>

#include "emscripten.h"

#define EMSCRIPTEN_FETCH_LOAD_TO_MEMORY 1
#define EMSCRIPTEN_FETCH_REPLACE        16

struct emscripten_fetch_t;

typedef struct emscripten_fetch_attr_t
{
   char requestMethod[32];
   void *userData;
   void (*onsuccess)(struct emscripten_fetch_t *fetch);
   void (*onerror)(struct emscripten_fetch_t *fetch);
   void (*onprogress)(struct emscripten_fetch_t *fetch);
   void (*onreadystatechange)(struct emscripten_fetch_t *fetch);
   uint32_t attributes;
   const char * const *requestHeaders;
   const char *requestData;
   size_t requestDataSize;
} emscripten_fetch_attr_t;

typedef struct emscripten_fetch_t
{
   void *userData;
   const char *data;
   uint64_t numBytes;
   uint64_t dataOffset;
   uint64_t totalBytes;
   unsigned short status;
   emscripten_fetch_attr_t __attributes;
} emscripten_fetch_t;

void emscripten_fetch_attr_init(emscripten_fetch_attr_t *fetch_attr);
emscripten_fetch_t *emscripten_fetch(emscripten_fetch_attr_t *fetch_attr,
      const char *url);
EMSCRIPTEN_RESULT emscripten_fetch_close(emscripten_fetch_t *fetch);
size_t emscripten_fetch_get_response_headers_length(emscripten_fetch_t *fetch);
size_t emscripten_fetch_get_response_headers(emscripten_fetch_t *fetch,
      char *dst, size_t dstSizeBytes);

#endif
