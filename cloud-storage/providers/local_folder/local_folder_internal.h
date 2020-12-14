/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (local_folder_internal.h).
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

#include <retro_common_api.h>

#include <formats/rjson.h>
#include <net/net_http.h>

#include "../../cloud_storage.h"

#ifndef _CLOUD_STORAGE_GOOGLE_INTERNAL_H
#define _CLOUD_STORAGE_GOOGLE_INTERNAL_H

RETRO_BEGIN_DECLS

const char *cloud_storage_local_folder_get_base_dir();

cloud_storage_item_t *cloud_storage_local_folder_get_metadata(const char *absolute_filename);

void cloud_storage_local_folder_list_files(cloud_storage_item_t *folder);

bool cloud_storage_local_folder_download_file(
   cloud_storage_item_t *file_to_download,
   char *local_file);

bool cloud_storage_local_folder_upload_file(
   cloud_storage_item_t *remote_dir,
   cloud_storage_item_t *remote_file,
   char *local_file);

cloud_storage_item_t *cloud_storage_local_folder_get_folder_metadata(
   const char *folder_name);

cloud_storage_item_t *cloud_storage_local_folder_get_file_metadata(cloud_storage_item_t *file);

cloud_storage_item_t *cloud_storage_local_folder_get_file_metadata_by_name(
   cloud_storage_item_t *folder,
   char *filename);

bool cloud_storage_local_folder_delete_file(cloud_storage_item_t *file);

cloud_storage_item_t *cloud_storage_local_folder_create_folder(const char *folder_name);

RETRO_END_DECLS

#endif
