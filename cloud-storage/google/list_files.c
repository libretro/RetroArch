/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (list_files.c).
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

#include <stdlib.h>

#include <net/net_http.h>

#include "../cloud_storage.h"
#include "../rest-lib/rest_api.h"
#include "../json.h"
#include "../driver_utils.h"
#include "google_internal.h"

#define LIST_FILES_URL "https://www.googleapis.com/drive/v3/files"

struct cloud_storage_google_list_files_state_t
{
   cloud_storage_item_t *folder;
   cloud_storage_item_t *children;
};

static struct http_request_t *create_http_request(cloud_storage_item_t *folder, char *next_page_token)
{
   struct http_request_t *request;
   char *parent_folder_id;
   char *q;

   request = net_http_request_new();
   net_http_request_set_url(request, LIST_FILES_URL);
   net_http_request_set_method(request, "GET");

   q = cloud_storage_join_strings(NULL, "\"", folder->id, "\" in parents", NULL);
   net_http_request_set_url_param(request, "q", q, true);
   free(q);

   net_http_request_set_url_param(request, "spaces", "appDataFolder", true);
   net_http_request_set_url_param(request, "fields", "nextPageToken,files(id,name,mimeType,md5Checksum)", true);

   if (next_page_token)
   {
      net_http_request_set_url_param(request, "nextPageToken", next_page_token, true);
      free(next_page_token);
   }

   net_http_request_set_log_request_body(request, true);
   net_http_request_set_log_response_body(request, true);

   return request;
}

static void get_list_files_next_page(
   cloud_storage_operation_state_t *state,
   char *next_page_token);

static bool parse_list_files_response(
   struct json_node_t *json,
   struct cloud_storage_item_t **parsed_files,
   char **next_page_token)
{
   char *temp_string = NULL;
   size_t temp_string_length;
   struct json_array_t *files;
   cloud_storage_item_t *first_file = NULL;
   cloud_storage_item_t *current_file = NULL;
   cloud_storage_item_t *new_file;
   struct json_array_item_t *file_json;

   *next_page_token = NULL;

   if (!json || json->node_type != OBJECT_VALUE)
   {
      return false;
   }

   if (json_map_get_value_string(json->value.map_value, "nextPageToken", &temp_string, &temp_string_length))
   {
      *next_page_token = (char *)calloc(temp_string_length + 1, sizeof(char));
      strncpy(*next_page_token, temp_string, temp_string_length);
   }

   if (!json_map_get_value_array(json->value.map_value, "files", &files))
   {
      if (*next_page_token)
      {
         free(*next_page_token);
         *next_page_token = NULL;
      }
      return false;
   }

   file_json = files->element;
   while (file_json)
   {
      if (file_json->value->node_type == OBJECT_VALUE)
      {
         new_file = cloud_storage_google_parse_file_from_json(file_json->value->value.map_value);
         if (new_file)
         {
            if (!current_file)
            {
               first_file = new_file;
               current_file = new_file;
            } else
            {
               current_file->next = new_file;
            }
         }
      }

      file_json = file_json->next;
   }

   *parsed_files = first_file;
   return true;
}

static void list_files_success_handler(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   char *json_text;
   struct json_node_t *json;
   struct cloud_storage_google_list_files_state_t *extra_state;
   cloud_storage_item_t *files_to_add = NULL;
   cloud_storage_item_t *last_file;
   char *next_page_token = NULL;
   uint8_t *data;
   size_t data_len;

   extra_state = (struct cloud_storage_google_list_files_state_t *)state->extra_state;

   data = net_http_response_get_data(response, &data_len, false);

   json_text = (char *)malloc(data_len + 1);
   strncpy(json_text, (char *)data, data_len);
   json_text[data_len] = '\0';

   json = string_to_json(json_text);

   if (json)
   {
      if (parse_list_files_response(json, &files_to_add, &next_page_token))
      {
         json_node_free(json);
         if (!extra_state->children)
         {
            extra_state->children = files_to_add;
         } else if (files_to_add)
         {
            last_file = extra_state->children;
            while (last_file->next)
            {
               last_file = last_file->next;
            }
            last_file->next = files_to_add;
         }
      }
   }

   free(json_text);

   if (next_page_token)
   {
      get_list_files_next_page(state, next_page_token);
   } else
   {
      state->result = extra_state->children;
      state->callback(state, true);

      state->complete = true;
   }
}

static void failure_handler(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *state)
{
   state->result = NULL;
   state->callback(state, CLOUD_STORAGE_PROVIDER_PERM_FAIL);

   state->complete = true;
}

static void get_list_files_next_page(
   cloud_storage_operation_state_t *state,
   char *next_page_token)
{
   struct http_request_t *http_request;
   rest_api_request_t *rest_request;
   struct cloud_storage_google_list_files_state_t *extra_state;

   extra_state = (struct cloud_storage_google_list_files_state_t *)state->extra_state;

   http_request = create_http_request(extra_state->folder, NULL);

   rest_request = rest_api_request_new(http_request);
   rest_api_request_set_response_handler(rest_request, 200, false, list_files_success_handler, state);
   rest_api_request_set_response_handler(rest_request, 404, false, failure_handler, state);

   google_rest_execute_request(rest_request, state);
}

static void free_extra_state(void *extra_state)
{
   free(extra_state);
}

void cloud_storage_google_list_files(
   cloud_storage_item_t *folder,
   cloud_storage_continuation_data_t *continuation_data,
   cloud_storage_operation_callback callback)
{
   cloud_storage_operation_state_t *state;
   struct cloud_storage_google_list_files_state_t *extra_state;

   extra_state = (struct cloud_storage_google_list_files_state_t *)calloc(1, sizeof(struct cloud_storage_google_list_files_state_t));
   extra_state->folder = folder;
   extra_state->children = NULL;

   state = (cloud_storage_operation_state_t *)calloc(1, sizeof(cloud_storage_operation_state_t));
   state->continuation_data = continuation_data;
   state->callback = callback;
   state->extra_state = extra_state;
   state->free_extra_state = free_extra_state;

   get_list_files_next_page(state, NULL);
}
