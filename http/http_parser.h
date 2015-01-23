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

#ifndef _HTTP_PARSER_H
#define _HTTP_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

struct http;

struct http* http_new(const char * url);

/* You can use this to call http_update 
 * only when something will happen; select() it for reading. */
int http_fd(struct http* state);

/* Returns true if it's done, or if something broke.
 * 'total' will be 0 if it's not known. */
bool http_update(struct http* state, size_t* progress, size_t* total);

/* 200, 404, or whatever.  */
int http_status(struct http* state);

/* Returns the downloaded data. The returned buffer is owned by the 
 * HTTP handler; it's freed by http_delete. 
 *
 * If the status is not 20x and accept_error is false, it returns NULL. */
uint8_t* http_data(struct http* state, size_t* len, bool accept_error);

/* Cleans up all memory. */
void http_delete(struct http* state);

#ifdef __cplusplus
}
#endif

#endif
