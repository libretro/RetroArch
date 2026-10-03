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

/**
 * net_http_sink_t:
 *
 * Called with each run of decoded body bytes as they arrive.  Return
 * false to abort the transfer (a failed write, a full disk); the
 * handle then reports an error like any other transport failure.
 *
 * Runs on whichever thread drives net_http_update().
 **/
/**
 * net_http_init:
 *
 * Call once at startup, before the first transfer; it creates the
 * eventcount a DNS wait parks on, and nothing creates it lazily.
 * Idempotent, but not safe to call concurrently.
 *
 * Threading: net_http takes no locks.  The DNS cache and connection
 * pool belong to the thread that drives transfers - every
 * net_http_new/update/wait/delete sharing them must come from one
 * thread at a time, and net_http_init()/net_http_deinit() run on it or
 * while it is stopped.  Handing that role to another thread needs an
 * ordering point in between (a thread join, a queue handoff).  DNS
 * lookups run on threads of their own and publish into the cache with
 * an atomic store.  Building connection objects
 * (net_http_connection_*) touches no shared state and is fine from any
 * thread.
 **/
void net_http_init(void);

/**
 * net_http_deinit:
 *
 * Closes pooled connections and frees the DNS cache and the locks.
 * Call at shutdown, after the last transfer has finished.
 **/
void net_http_deinit(void);

typedef bool (*net_http_sink_t)(void *userdata, const void *data, size_t len);

/**
 * net_http_connection_set_sink:
 *
 * Stream the response body to @cb instead of accumulating it, so peak
 * memory is the receive window rather than the whole payload.  With a
 * sink set, net_http_data() returns NULL/0; status and headers are
 * unaffected.
 **/
void net_http_connection_set_sink(struct http_connection_t *conn,
      net_http_sink_t cb, void *userdata);

void net_http_connection_set_content(struct http_connection_t *conn, const char *content_type,
      size_t content_length, const void *content);

/**
 * net_http_source_t:
 *
 * Called for the next run of request-body bytes: fill up to @len bytes
 * of @buf and return how many were filled. Return 0 at the end of the
 * body and a negative value on error; either one short of the
 * advertised Content-Length fails the request like a transport error.
 * Runs on whichever thread drives net_http_update().
 **/
typedef int64_t (*net_http_source_t)(void *userdata, void *buf, size_t len);

/**
 * net_http_source_rewind_t:
 *
 * Asked to restart the body from its first byte when the request has
 * to be replayed on a fresh connection (a pooled connection the peer
 * closed while idle). Return false if that is not possible; the
 * request then fails instead of being replayed.
 **/
typedef bool (*net_http_source_rewind_t)(void *userdata);

/**
 * net_http_connection_set_content_source:
 *
 * Send a request body of @content_length bytes pulled from @source as
 * the socket takes them, so peak memory is one send buffer rather than
 * the whole body. Replaces any body given by
 * net_http_connection_set_content(). @rewind may be NULL, in which
 * case the request is never replayed on a fresh connection.
 **/
void net_http_connection_set_content_source(struct http_connection_t *conn,
      const char *content_type, size_t content_length,
      net_http_source_t source, net_http_source_rewind_t rewind,
      void *userdata);

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
 * net_http_update_budget:
 *
 * net_http_update(), with the socket drained while
 * @within_budget(@budget, 0, 0) allows - one read, of at most
 * 256 KiB, per work item - instead of up to a fixed 256 KiB per
 * call.  For a caller that paces its I/O with a time window (the
 * frontend's shared per-frame I/O window).  A NULL @within_budget is
 * net_http_update().
 **/
bool net_http_update_budget(struct http_t *state,
      size_t* progress, size_t* total,
      bool (*within_budget)(void *budget, size_t avail, size_t len),
      void *budget);

/**
 * net_http_wait:
 *
 * Waits until the transfer can make progress again, or until
 * @timeout_ms elapses, for callers driving a transfer from their own
 * thread rather than once per frame. Unlike net_http_fd(), which hands
 * out the descriptor and leaves the caller to work out what to select
 * for, this knows which way the transfer is waiting and knows when the
 * last pass left bytes already buffered, in which case it returns at
 * once.
 *
 * @return true if the transport is ready or no wait was needed,
 * false if the timeout elapsed first.
 **/
bool net_http_wait(struct http_t *state, int timeout_ms);

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

/**
 * net_http_failure:
 *
 * Where a transfer that never produced a status failed: the transport
 * stage, as a literal ("dns_lookup_failed", "ssl_connect_failed",
 * ...), and through @code the library's own error for it when there
 * is one - the TLS library's for the ssl stages (negative), the OS
 * socket error (errno, or WSAGetLastError on Windows; positive) for
 * socket_create/connect/send, 0 otherwise.  NULL
 * when the transport did not fail.  For turning "HTTP -1" into a
 * message that says what went wrong.
 **/
const char *net_http_failure(struct http_t *state, int *code);

/**
 * net_http_headers_take:
 * @accept_err : also return them when the transfer failed
 *
 * The response headers as one block: each "Name: value" line
 * NUL-terminated, the block ending in an empty line ("\0\0" at its
 * end; a response with no header lines is a single "\0").  Walk it
 * with net_http_header_next(), or look a field up with
 * net_http_header_value().
 *
 * Ownership moves to the caller, who frees it with free(); repeated
 * calls return the same pointer.  NULL when no status line was parsed,
 * and on a transport error unless @accept_err - headers are returned
 * for any parsed response, HTTP error statuses such as 401 included
 * (needed for auth challenges).
 **/
char *net_http_headers_take(struct http_t *state, bool accept_err);

/**
 * net_http_headers_compact:
 * @raw : header text, one "Name: value" line per LF or CRLF, NUL-terminated,
 *        with one spare byte after the terminator
 *
 * Rewrites @raw in place into the block net_http_headers_take()
 * returns: each line NUL-terminated, trailing CR and spaces trimmed,
 * blank lines dropped, an empty line at the end.  Output never outgrows
 * input, except for that closing NUL, which is what the spare byte is
 * for.  For backends that get the headers as text from somewhere else
 * (the browser, under Emscripten).
 *
 * Returns: the block's length, closing NUL excluded.
 **/
size_t net_http_headers_compact(char *raw);

/**
 * net_http_header_next:
 *
 * The line after @line in the block @headers (the first line when
 * @line is NULL), or NULL at the end:
 *
 *    for (h = net_http_header_next(hdrs, NULL); h;
 *         h = net_http_header_next(hdrs, h))
 **/
const char *net_http_header_next(const char *headers, const char *line);

/**
 * net_http_header_value:
 *
 * The value of the first field named @name (case-insensitive, whole
 * name), leading whitespace skipped; NULL if there is none.  Points
 * into @headers.  Fields that may repeat (WWW-Authenticate) want
 * net_http_header_next() instead.
 **/
const char *net_http_header_value(const char *headers, const char *name);

/**
 * net_http_header:
 *
 * The value of response header @name, looked up without taking the
 * block: usable mid-transfer, from a sink callback, once the status line
 * and headers are in.  Points into the handle's own storage, valid until
 * the handle follows a redirect, is deleted, or its headers are taken.
 * NULL before the headers arrive, after a transport failure, or when the
 * field is absent.
 **/
const char *net_http_header(struct http_t *state, const char *name);

/**
 * net_http_headers:
 *
 * Legacy: the headers as a newly built string_list, owned by the
 * caller.  Costs an allocation per line; nothing in RetroArch uses it.
 * Same NULL rules as net_http_headers_take().
 **/
struct string_list *net_http_headers(struct http_t *state);
struct string_list *net_http_headers_ex(struct http_t *state, bool accept_error);

/**
 * net_http_body_is_framed:
 * @headers : header block as net_http_headers_take() returns it
 *
 * True when the response frames its body with Content-Length or
 * "Transfer-Encoding: chunked", tested exactly as the receiver picks
 * the body type. A transfer that ends short of either framing fails,
 * but a body delimited only by the connection closing cannot be told
 * apart from one cut off mid-transfer, so callers that must not act
 * on a truncated body (writing a downloaded file over a local one)
 * check this first.
 **/
bool net_http_body_is_framed(const char *headers);

/**
 * net_http_data:
 *
 * Leaf function.
 *
 * @return the downloaded data. Ownership of the returned buffer passes
 * to the caller, who frees it with free(); net_http_delete() no longer
 * does. A NULL return leaves the buffer with the handler.
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

/**
 * net_http_url_join:
 * @dst      : output buffer
 * @dst_size : size of @dst including the NUL
 * @base     : absolute http:// or https:// URL
 * @ref      : reference to resolve against @base (RFC 3986 section 5)
 *
 * An empty @ref gives @base; an absolute http(s) URL replaces it; a
 * reference starting with "//" keeps only the scheme, "/" keeps scheme
 * and authority, "?" keeps the path and "#" keeps path and query;
 * anything else replaces the last segment of the base path.  "." and
 * ".." segments are then removed.  No allocation.
 *
 * Returns: length of the result, or -1 when @base is not an http(s)
 * URL, @ref has another scheme, or @dst is too small (@dst is then an
 * empty string).
 **/
int net_http_url_join(char *dst, size_t dst_size,
      const char *base, const char *ref);

/**
 * net_http_urldecode:
 *
 * Decode %XX escapes from @src into @dst.  A '%' not followed by two
 * hex digits is copied as is; '+' is left alone (it is a space only in
 * form bodies).  Returns the decoded length, or -1 when @dst is too
 * small (@dst holds the truncated, terminated prefix).
 *
 * Not for a URL that is about to be requested: decoding "%20" puts a
 * space in the request line.  Use it on names meant for display or for
 * the local filesystem.
 **/
int net_http_urldecode(char *dst, size_t dst_size, const char *src);

/**
 * net_http_urldecode_inplace:
 *
 * As net_http_urldecode(), in @s itself; decoded text is never longer
 * than its source.  Returns the decoded length.
 **/
int net_http_urldecode_inplace(char *s);

RETRO_END_DECLS

#endif
