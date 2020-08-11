/* Copyright  (C) 2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rest_api.c).
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
#include <queues/task_queue.h>

#include "rest_api.h"
#include "../cloud_storage.h"

struct rest_api_response_callback_with_status_t rest_api_response_callback_with_status_t;

struct rest_api_response_callback_with_status_t
{
   int status_code;
   rest_api_response_callback_t callback;
   cloud_storage_operation_state_t *operation_state;
   bool retryable;
   struct rest_api_response_callback_with_status_t *next;
};

struct rest_api_request_t
{
   struct http_request_t *http_request;
   struct http_connection_t *http_conn;
   struct http_t *http;
   bool default_response_retryable;
   rest_api_response_callback_t default_callback;
   cloud_storage_operation_state_t *default_callback_operation_state;
   struct rest_api_response_callback_with_status_t *callbacks;
   rest_api_completed_callback_t completed_callback;
   rest_api_retry_policy_t *retry_policy;
   int retry_count;
   time_t delay_end;
};

struct rest_api_retry_policy_t
{
   int *status_codes;
   size_t status_codes_len;
   time_t *delays;
   size_t delays_len;
};

rest_api_retry_policy_t *rest_api_retry_policy_new(
   int status_codes[],
   size_t status_codes_len,
   time_t delays[],
   size_t delays_len)
{
   rest_api_retry_policy_t *retry_policy;

   retry_policy = (rest_api_retry_policy_t *)malloc(sizeof(rest_api_retry_policy_t));
   retry_policy->status_codes = (int *)malloc(status_codes_len * sizeof(int));
   memcpy(retry_policy->status_codes, status_codes, status_codes_len * sizeof(int));
   retry_policy->status_codes_len = status_codes_len;

   retry_policy->delays = (time_t *)malloc(delays_len * sizeof(time_t));
   memcpy(retry_policy->delays, delays, delays_len * sizeof(time_t));
   retry_policy->delays_len = delays_len;

   return retry_policy;
}

rest_api_request_t *rest_api_request_new(struct http_request_t *http_request)
{
   struct rest_api_request_t *request;

   request = (rest_api_request_t *)calloc(1, sizeof(rest_api_request_t));
   request->http_request = http_request;

   return request;
}

struct http_request_t *rest_api_get_http_request(rest_api_request_t *request)
{
   if (request)
   {
      return request->http_request;
   } else
   {
      return NULL;
   }
}

void rest_api_request_set_retry_policy(rest_api_request_t *request, rest_api_retry_policy_t *retry_policy)
{
   if (request)
   {
      request->retry_policy = retry_policy;
   }
}

void rest_api_request_release_http_request(rest_api_request_t *request)
{
   request->http_request = NULL;
}

void rest_api_request_free(rest_api_request_t *request)
{
   if (!request)
   {
      return;
   }

   if (request->http)
      net_http_delete(request->http);

   if (request->http_conn)
      net_http_connection_free(request->http_conn, false);
   else if (request->http_request)
      net_http_request_free(request->http_request);

   if (request->http_request)
   {
      net_http_request_free(request->http_request);
   }

   while (request->callbacks)
   {
      struct rest_api_response_callback_with_status_t *next = request->callbacks->next;
      free(request->callbacks);
      request->callbacks = next;
   }

   if (request->retry_policy)
   {
      free(request->retry_policy->status_codes);
      free(request->retry_policy->delays);
      free(request->retry_policy);
   }

   free(request);
}

void rest_api_request_set_default_response_handler(
   rest_api_request_t *request,
   bool retryable,
   rest_api_response_callback_t callback,
   cloud_storage_operation_state_t *operation_state)
{
   request->default_response_retryable = retryable;
   request->default_callback = callback;
   request->default_callback_operation_state = operation_state;
}

void rest_api_request_set_response_handler(
   rest_api_request_t *request,
   int status_code,
   bool retryable,
   rest_api_response_callback_t callback,
   cloud_storage_operation_state_t *operation_state)
{
   struct rest_api_response_callback_with_status_t *last_callback;

   if (request->callbacks == NULL)
   {
      request->callbacks = (struct rest_api_response_callback_with_status_t *)malloc(sizeof(struct rest_api_response_callback_with_status_t));
      request->callbacks->status_code = status_code;
      request->callbacks->retryable = retryable;
      request->callbacks->callback = callback;
      request->callbacks->operation_state = operation_state;
      request->callbacks->next = NULL;

      return;
   }
   
   last_callback = request->callbacks;
   while (last_callback->next)
   {
      if (last_callback->status_code == status_code)
      {
         last_callback->callback = callback;
         return;
      }
      last_callback = last_callback->next;
   }

   last_callback->next = (struct rest_api_response_callback_with_status_t *)malloc(sizeof(struct rest_api_response_callback_with_status_t));
   last_callback->next->status_code = status_code;
   last_callback->next->retryable = retryable;
   last_callback->next->callback = callback;
   last_callback->next->operation_state = operation_state;
   last_callback->next->next = NULL;
}

int *rest_api_get_response_callback_status_codes(rest_api_request_t *request)
{
   size_t count = 0;
   struct rest_api_response_callback_with_status_t *current_callback_with_status_code;
   int *ret_val;

   current_callback_with_status_code = request->callbacks;
   while (current_callback_with_status_code)
   {
      count++;
      current_callback_with_status_code = current_callback_with_status_code->next;
   }

   ret_val = (int *)malloc(count * sizeof(int));
   count = 0;

   current_callback_with_status_code = request->callbacks;
   while (current_callback_with_status_code)
   {
      ret_val[count++] = current_callback_with_status_code->status_code;
      current_callback_with_status_code = current_callback_with_status_code->next;
   }

   return ret_val;
}

rest_api_response_callback_t rest_api_request_get_default_callback(rest_api_request_t *request)
{
   return request->default_callback;
}

void *rest_api_get_default_callback_user_data(rest_api_request_t *request)
{
   return request->default_callback_operation_state;
}

rest_api_response_callback_t rest_api_request_get_callback(
   rest_api_request_t *request,
   int status_code)
{
   struct rest_api_response_callback_with_status_t *current_callback_with_status_code;

   current_callback_with_status_code = request->callbacks;
   while (current_callback_with_status_code)
   {
      if (current_callback_with_status_code->status_code == status_code)
      {
         return current_callback_with_status_code->callback;
      }
   }

   return NULL;
}

cloud_storage_operation_state_t *rest_api_get_callback_user_data(
   rest_api_request_t *request,
   int status_code)
{
   struct rest_api_response_callback_with_status_t *current_callback_with_status_code;

   current_callback_with_status_code = request->callbacks;
   while (current_callback_with_status_code)
   {
      if (current_callback_with_status_code->status_code == status_code)
      {
         return current_callback_with_status_code->operation_state;
      }
   }

   return NULL;
}

static void free_operation_state(cloud_storage_operation_state_t *operation_state)
{
   free(operation_state);
}

static void rest_api_execute_callback(
   rest_api_request_t *request,
   struct http_response_t *response)
{
   rest_api_response_callback_t callback;
   cloud_storage_operation_state_t *operation_state;
   int status_code = -1;

   if (!request)
   {
      return;
   }

   callback = request->default_callback;
   operation_state = request->default_callback_operation_state;

   if (response)
   {
      status_code = net_http_response_get_status(response);
   }

   if (request->callbacks)
   {
      struct rest_api_response_callback_with_status_t *current_entry = request->callbacks;
      while (current_entry)
      {
         if (current_entry->status_code == status_code)
         {
            callback = current_entry->callback;
            operation_state = current_entry->operation_state;
            break;
         }
         current_entry = current_entry->next;
      }
   }

   if (callback)
   {
      (*callback)(request, response, operation_state);

      if (operation_state->complete)
      {
         if (operation_state->extra_state && operation_state->free_extra_state)
         {
            operation_state->free_extra_state(operation_state->extra_state);
         }
         free_operation_state(operation_state);
      }
   }
}

void rest_api_task_handler(retro_task_t *task)
{
   rest_api_request_t *request;
   size_t progress;
   size_t total;

   request = (rest_api_request_t *)task->user_data;

   if (request->delay_end > 0)
   {
      if (time(NULL) >= request->delay_end)
      {
         request->delay_end = 0;
         request->http_conn = net_http_connection_new(request->http_request);
         if (!request->http_conn || !net_http_connection_iterate(request->http_conn) || !net_http_connection_done(request->http_conn))
         {
            request->http_conn = NULL;
            task_set_finished(task, true);
            return;
         }

         request->http = net_http_new(request->http_conn);
      }
   } else if (net_http_update(request->http, &progress, &total))
   {
      if (request->retry_policy)
      {
         struct http_response_t *response;
         int status_code;
         int i;
         bool should_retry = request->default_response_retryable;
         struct rest_api_response_callback_with_status_t *current_entry;

         response = net_http_get_response(request->http);
         status_code = net_http_response_get_status(response);

         for (current_entry = request->callbacks;current_entry;current_entry = current_entry->next)
         {
            if (current_entry->status_code == status_code)
            {
               should_retry = current_entry->retryable;
               break;
            }
         }

         if (should_retry && request->retry_count < request->retry_policy->delays_len - 1)
         {
            net_http_delete(request->http);
            net_http_connection_free(request->http_conn, false);
            request->http = NULL;
            request->http_conn = NULL;

            request->delay_end = time(NULL) + request->retry_policy->delays[request->retry_count];
            request->retry_count++;

            return;
         }
      }

      task_set_finished(task, true);
   }
}

void rest_api_task_callback(
   retro_task_t *task,
   void *task_data,
   void *user_data,
   const char *error)
{
   rest_api_request_t *request;

   request = (rest_api_request_t *)user_data;
   if (request->http)
   {
      struct http_response_t *response = net_http_get_response(request->http);
      rest_api_execute_callback(request, response);
   } else
   {
      rest_api_execute_callback(request, NULL);
   }
}

void rest_api_task_cleanup(retro_task_t *task)
{
   rest_api_request_t *request;

   request = (rest_api_request_t *)task->user_data;
   if (request->http)
   {
      struct http_response_t *response = net_http_get_response(request->http);
      net_http_response_free(response);
   }

   if (request->completed_callback)
   {
      (*(request->completed_callback))(request);
   }

   rest_api_request_free(request);
}

void rest_api_request_execute(rest_api_request_t *request)
{
   retro_task_t *task;

   request->http_conn = net_http_connection_new(request->http_request);
   if (!request->http_conn || !net_http_connection_iterate(request->http_conn) || !net_http_connection_done(request->http_conn))
      goto error;

   request->http = net_http_new(request->http_conn);
   if (!request->http)
      goto error;

   task = task_init();
   task->type = TASK_TYPE_NONE;
   task->handler = rest_api_task_handler;
   task->callback = rest_api_task_callback;
   task->cleanup = rest_api_task_cleanup;
   task->user_data = request;

   task_queue_push(task);
   return;

error:
   rest_api_execute_callback(request, NULL);
   rest_api_request_free(request);
}