/* Stand-in for Emscripten's emscripten.h on the host: what
 * tasks/task_http_emscripten.c needs from it, which is nothing beyond
 * the result type fetch.h uses. */
#ifndef HTTP_EMSCRIPTEN_TEST_EMSCRIPTEN_H
#define HTTP_EMSCRIPTEN_TEST_EMSCRIPTEN_H

typedef int EMSCRIPTEN_RESULT;

#endif
