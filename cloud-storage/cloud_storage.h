/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cloud_storage.h).
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

#ifndef _CLOUD_STORAGE_H
#define _CLOUD_STORAGE_H

#include <retro_common_api.h>
#include <configuration.h>

#include <boolean.h>
#include <time.h>

RETRO_BEGIN_DECLS

#define GAME_SAVES_FOLDER_NAME "save_games"
#define GAME_STATES_FOLDER_NAME "save_states"

enum cloud_storage_hash_type_t
{
   SHA1,
   SHA256,
   MD5
};
typedef enum cloud_storage_hash_type_t cloud_storage_hash_type_t;

struct cloud_storage_file_t
{
   cloud_storage_hash_type_t hash_type;
   char *hash_value;
   char *download_url;
};
typedef struct cloud_storage_file_t cloud_storage_file_t;

typedef struct cloud_storage_item_t cloud_storage_item_t;

struct cloud_storage_folder_t
{
   cloud_storage_item_t *children;
};
typedef struct cloud_storage_folder_t cloud_storage_folder_t;

struct cloud_storage_item_t
{
   char *id;
   char *name;
   enum
   {
      CLOUD_STORAGE_FILE,
      CLOUD_STORAGE_FOLDER
   } item_type;
   union
   {
      cloud_storage_file_t file;
      cloud_storage_folder_t folder;
   } type_data;
   cloud_storage_item_t *next;
};

enum cloud_storage_provider_status_t
{
   CLOUD_STORAGE_PROVIDER_PERM_FAIL,
   CLOUD_STORAGE_PROVIDER_IN_BACKOFF,
   CLOUD_STORAGE_PROVIDER_READY
};
typedef enum cloud_storage_provider_status_t cloud_storage_provider_status_t;

typedef void (*cloud_storage_free_provider_data_function)(void *provider_data);

struct cloud_storage_provider_state_t
{
   cloud_storage_item_t *game_states;
   cloud_storage_item_t *game_saves;
   cloud_storage_provider_status_t status;
   uint8_t retry_count;
   time_t backoff_end;
   void *provider_data;

   cloud_storage_free_provider_data_function free_data;
};
typedef struct cloud_storage_provider_state_t cloud_storage_provider_state_t;

enum cloud_storage_status_response_t
{
   CLOUD_STORAGE_STATUS_NOT_READY,
   CLOUD_STORAGE_STATUS_NEED_AUTHENTICATION,
   CLOUD_STORAGE_STATUS_READY
};
typedef enum cloud_storage_status_response_t cloud_storage_status_response_t;

enum cloud_storage_operation_type_t
{
   CLOUD_STORAGE_SYNC,
   CLOUD_STORAGE_UPLOAD
};
typedef enum cloud_storage_operation_type_t cloud_storage_operation_type_t;

enum cloud_storage_result_type_t
{
   CLOUD_STORAGE_METADATA,
   CLOUD_STORAGE_LIST_FILES,
   CLOUD_STORAGE_DOWNLOAD_FILE,
   CLOUD_STORAGE_UPLOAD_FILE
};
typedef enum cloud_storage_result_type_t cloud_storage_result_type_t;

typedef struct cloud_storage_upload_item_t cloud_storage_upload_item_t;
struct cloud_storage_upload_item_t
{
   char *local_file;
   cloud_storage_upload_item_t *next;
};

enum folder_type_t
{
   CLOUD_STORAGE_GAME_STATES,
   CLOUD_STORAGE_GAME_SAVES
};
typedef enum folder_type_t folder_type_t;

struct cloud_storage_continuation_data_t
{
   cloud_storage_provider_state_t *provider_state;
   cloud_storage_operation_type_t operation_type;
   union
   {
      struct
      {
         folder_type_t current_folder_type;
         enum
         {
            NOT_STARTED,
            GET_FOLDER_METADATA,
            CREATE_FOLDER,
            LISTING_FOLDER,
            DOWNLOADING_ITEMS,
            UPLOADING_ITEMS
         } phase;
         cloud_storage_item_t *files_to_download;
         cloud_storage_upload_item_t *upload_items;
      } sync_files;
      struct
      {
         cloud_storage_item_t *file_to_upload;
      } upload_file;
   } operation_data;
};
typedef struct cloud_storage_continuation_data_t cloud_storage_continuation_data_t;

typedef void (*cloud_storage_free_provider_data_function)(void *provider_data);

enum authorization_status_t
{
   AUTH_PENDING_ASYNC,
   AUTH_COMPLETE,
   AUTH_FAILED
};
typedef enum authorization_status_t authorization_status_t;

typedef struct cloud_storage_operation_state_t cloud_storage_operation_state_t;
typedef void (*cloud_storage_operation_callback)(
   cloud_storage_operation_state_t *operation_state,
   bool succeeded);

struct cloud_storage_operation_state_t
{
   cloud_storage_continuation_data_t *continuation_data;
   cloud_storage_operation_callback callback;
   cloud_storage_item_t *result;
   bool complete;
   void *extra_state;
   void (*free_extra_state)(void *extra_state);
};

typedef void (*cloud_storage_authenticate_callback)(
   cloud_storage_continuation_data_t *continuation_data,
   bool success,
   void *data);

struct cloud_storage_provider_t
{
   time_t *retry_delays;
   uint8_t max_retries;

   void *(*initialize)(settings_t *settings);

   bool need_authorization;

   cloud_storage_free_provider_data_function free_provider_data;

   cloud_storage_status_response_t (*get_status)(
      cloud_storage_provider_state_t *provider_state
   );

   void (*authenticate)(
      cloud_storage_continuation_data_t *continuation_data,
      cloud_storage_authenticate_callback callback,
      void *data
   );

   authorization_status_t (*authorize)(
      settings_t *settings,
      void *google_state_ptr,
      void (*callback)(bool success)
   );

   void (*list_files)(
      cloud_storage_item_t *folder,
      cloud_storage_continuation_data_t *continuation_data,
      cloud_storage_operation_callback callback
   );

   void (*download_file)(
      cloud_storage_item_t *file_to_download,
      char *local_file,
      cloud_storage_continuation_data_t *continuation_data,
      cloud_storage_operation_callback callback
   );

   void (*upload_file)(
      cloud_storage_item_t *remote_dir,
      cloud_storage_item_t *remote_file,
      char *local_file,
      cloud_storage_continuation_data_t *continuation_data,
      cloud_storage_operation_callback callback
   );

   void (*get_folder_metadata)(
      char *folder_name,
      cloud_storage_continuation_data_t *continuation_data,
      cloud_storage_operation_callback callback
   );

   void (*get_file_metadata)(
      cloud_storage_item_t *file,
      cloud_storage_continuation_data_t *continuation_data,
      cloud_storage_operation_callback callback
   );

   void (*get_file_metadata_by_name)(
      cloud_storage_item_t *folder,
      char *filename,
      cloud_storage_continuation_data_t *continuation_data,
      cloud_storage_operation_callback callback
   );

   void (*delete_file)(
      cloud_storage_item_t *file,
      cloud_storage_continuation_data_t *continuation_data,
      cloud_storage_operation_callback callback
   );

   void (*create_folder)(
      char *folder_name,
      cloud_storage_continuation_data_t *continuation_data,
      cloud_storage_operation_callback callback
   );
};
typedef struct cloud_storage_provider_t cloud_storage_provider_t;

void cloud_storage_init(void);

bool cloud_storage_need_authorization(void);

void cloud_storage_authorize(
   unsigned provider_id,
   void (*callback)(bool success));

void cloud_storage_sync_files(void);

void cloud_storage_upload_file(folder_type_t folder_type, char *file_name);

void cloud_storage_set_active_provider(unsigned provider_id);

RETRO_END_DECLS

#endif
