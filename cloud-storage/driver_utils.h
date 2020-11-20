/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (driver_utils.h).
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

#ifndef _DRIVER_UTILS_H
#define _DRIVER_UTILS_H

#include <retro_common_api.h>

#include <boolean.h>
#include <time.h>

#include <net/net_http.h>

#include "cloud_storage.h"

RETRO_BEGIN_DECLS

char *cloud_storage_join_strings(size_t *length, ...);

cloud_storage_item_t *cloud_storage_clone_item_list(cloud_storage_item_t *items);

void cloud_storage_item_free(cloud_storage_item_t *item);

char *get_temp_directory_alloc(void);

bool cloud_storage_save_access_token(const char *driver_name, char *new_access_token, time_t expiration_time);

void cloud_storage_load_access_token(const char *driver_name, char **access_token, int64_t *expiration_time);

bool cloud_storage_save_file(char *file_name, uint8_t *data, size_t data_len);

int64_t cloud_storage_get_file_size(char *local_file);

void cloud_storage_add_request_body_data(
   char *filename,
   size_t offset,
   size_t segment_length,
   struct http_request_t *request
);

RETRO_END_DECLS

#endif
