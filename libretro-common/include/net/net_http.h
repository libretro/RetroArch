/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_http.h).
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

#ifndef _LIBRETRO_SDK_NET_HTTP_H
#define _LIBRETRO_SDK_NET_HTTP_H

#include <stdint.h>
#include <boolean.h>
#include <string.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

struct http_t;
struct http_connection_t;

typedef enum net_http_phase
{
   NET_HTTP_PHASE_CONNECT = 0,
   NET_HTTP_PHASE_SEND,
   NET_HTTP_PHASE_RECV_HEADER,
   NET_HTTP_PHASE_RECV_BODY,
   NET_HTTP_PHASE_REDIRECT,
   NET_HTTP_PHASE_DONE,
   NET_HTTP_PHASE_ERROR
} net_http_phase_t;

const char *net_http_phase_to_string(net_http_phase_t phase);

/* Caller-facing, point-in-time HTTP transport snapshot.
 *
 * Notes:
 * - Some fields intentionally mirror live http_t state (e.g. status/fd/err)
 *   so callers can log a single self-contained payload without issuing
 *   additional net_http_* calls.
 * - Snapshot data is safe to copy and retain after transfer completion,
 *   unlike the opaque internal state which may be freed. */
typedef struct net_http_debug_state
{
   int fd;
   int status;
   int last_errno;
   int64_t last_io_len;
   size_t progress;
   size_t total;
   unsigned redirects;
   bool ssl;
   bool connected;
   bool request_sent;
   bool err;
   net_http_phase_t phase;
} net_http_debug_state_t;

struct http_connection_t *net_http_connection_new(const char *url, const char *method, const char *data);

/**
 * net_http_connection_iterate:
 *
 * Leaf function.
 **/
bool net_http_connection_iterate(struct http_connection_t *conn);

bool net_http_connection_done(struct http_connection_t *conn);

void net_http_connection_free(struct http_connection_t *conn);

void net_http_connection_set_user_agent(struct http_connection_t *conn, const char *user_agent);

void net_http_connection_set_headers(struct http_connection_t *conn, const char *headers);

void net_http_connection_set_content(struct http_connection_t *conn, const char *content_type,
      size_t content_length, const void *content);

const char *net_http_connection_url(struct http_connection_t *conn);

const char* net_http_connection_method(struct http_connection_t* conn);

struct http_t *net_http_new(struct http_connection_t *conn);

/**
 * net_http_fd:
 *
 * Leaf function.
 *
 * You can use this to call net_http_update
 * only when something will happen; select() it for reading.
 **/
int net_http_fd(struct http_t *state);

/**
 * net_http_update:
 *
 * @return true if it's done, or if something broke.
 * @total will be 0 if it's not known.
 **/
bool net_http_update(struct http_t *state, size_t* progress, size_t* total);

/**
 * net_http_status:
 *
 * Report HTTP status. 200, 404, or whatever.
 *
 * Leaf function.
 *
 * @return HTTP status code.
 **/
int net_http_status(struct http_t *state);

/**
 * net_http_error:
 *
 * Leaf function
 **/
bool net_http_error(struct http_t *state);

/* Copies the current transport snapshot into @out.
 *
 * Returns false if inputs are invalid; true on success.
 * The returned snapshot is independent from internal http_t lifetime. */
bool net_http_get_debug_state(struct http_t *state, net_http_debug_state_t *out);

/**
 * net_http_headers:
 *
 * Leaf function.
 *
 * @return the response headers. The returned buffer is owned by the
 * caller of net_http_new; it is not freed by net_http_delete.
 * If the status is not 20x and accept_error is false, it returns NULL.
 **/
struct string_list *net_http_headers(struct http_t *state);
struct string_list *net_http_headers_ex(struct http_t *state, bool accept_error);

/**
 * net_http_data:
 *
 * Leaf function.
 *
 * @return the downloaded data. The returned buffer is owned by the
 * HTTP handler; it's freed by net_http_delete.
 * If the status is not 20x and accept_error is false, it returns NULL.
 **/
uint8_t* net_http_data(struct http_t *state, size_t* len, bool accept_error);

/**
 * net_http_delete:
 *
 * Cleans up all memory.
 **/
void net_http_delete(struct http_t *state);

/**
 * net_http_urlencode:
 *
 * URL Encode a string
 * caller is responsible for deleting the destination buffer
 **/
void net_http_urlencode(char **dest, const char *source);

/**
 * net_http_urlencode_full:
 *
 * Re-encode a full URL
 **/
void net_http_urlencode_full(char *s, const char *source, size_t len);

RETRO_END_DECLS

#endif
