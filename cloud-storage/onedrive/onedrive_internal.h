/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (onedrive_internal.h).
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

#include "../json.h"
#include "../cloud_storage.h"
#include "../rest-lib/rest_api.h"

#ifndef _CLOUD_STORAGE_ONEDRIVE_INTERNAL_H
#define _CLOUD_STORAGE_ONEDRIVE_INTERNAL_H

RETRO_BEGIN_DECLS

struct cloud_storage_onedrive_provider_data_t
{
   char *access_token;
   time_t access_token_expiration_time;
   char *refresh_token;
};

cloud_storage_item_t *cloud_storage_onedrive_parse_file_from_json(struct json_map_t file_json);

bool cloud_storage_onedrive_save_access_token(char *new_access_token, time_t expiration_time);

void onedrive_rest_execute_request(
   rest_api_request_t *rest_request,
   cloud_storage_operation_state_t *state);

void cloud_storage_onedrive_authenticate(
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_authenticate_callback callback,
   void *callback_data);

authorization_status_t cloud_storage_onedrive_authorize(
   settings_t *settings,
   void *onedrive_state_ptr,
   void (*callback)(bool success));

void cloud_storage_onedrive_list_files(
   cloud_storage_item_t *folder,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback);

void cloud_storage_onedrive_download_file(
   cloud_storage_item_t *file_to_download,
   char *local_file,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback);

void cloud_storage_onedrive_upload_file(
   cloud_storage_item_t *remote_dir,
   cloud_storage_item_t *remote_file,
   char *local_file,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback);

void cloud_storage_onedrive_get_folder_metadata(
   char *folder_name,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback);

void cloud_storage_onedrive_get_file_metadata(
   cloud_storage_item_t *file,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback);

void cloud_storage_onedrive_get_file_metadata_by_name(
   cloud_storage_item_t *folder,
   char *filename,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback);

void cloud_storage_onedrive_delete_file(
   cloud_storage_item_t *file,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback);

void cloud_storage_onedrive_create_folder(
   char *folder_name,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback);

RETRO_END_DECLS

#endif
