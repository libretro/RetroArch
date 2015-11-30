/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Alfred Agrell
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _NET_HTTP_H
#define _NET_HTTP_H

#include <stdint.h>
#include <boolean.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

struct http_t;
struct http_connection_t;

struct http_connection_t *net_http_connection_new(const char *url);

bool net_http_connection_iterate(struct http_connection_t *conn);

bool net_http_connection_done(struct http_connection_t *conn);

void net_http_connection_free(struct http_connection_t *conn);

const char *net_http_connection_url(struct http_connection_t *conn);

struct http_t *net_http_new(struct http_connection_t *conn);

/* You can use this to call net_http_update 
 * only when something will happen; select() it for reading. */
int net_http_fd(struct http_t *state);

/* Returns true if it's done, or if something broke.
 * 'total' will be 0 if it's not known. */
bool net_http_update(struct http_t *state, size_t* progress, size_t* total);

/* 200, 404, or whatever.  */
int net_http_status(struct http_t *state);

bool net_http_error(struct http_t *state);

/* Returns the downloaded data. The returned buffer is owned by the 
 * HTTP handler; it's freed by net_http_delete. 
 *
 * If the status is not 20x and accept_error is false, it returns NULL. */
uint8_t* net_http_data(struct http_t *state, size_t* len, bool accept_error);

/* Cleans up all memory. */
void net_http_delete(struct http_t *state);

#ifdef __cplusplus
}
#endif

#endif
