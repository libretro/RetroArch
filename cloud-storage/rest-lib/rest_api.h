/* Copyright  (C) 2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rest_api.h).
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

#ifndef _REST_API_H
#define _REST_API_H

#include <retro_common_api.h>

#include <time.h>

#include <net/net_http.h>

#include "../cloud_storage.h"

RETRO_BEGIN_DECLS

typedef struct rest_api_request_t rest_api_request_t;

typedef struct rest_api_retry_policy_t rest_api_retry_policy_t;

typedef void (*rest_api_response_callback_t)(
   rest_api_request_t *request,
   struct http_response_t *response,
   cloud_storage_operation_state_t *user_data);

typedef void (*rest_api_completed_callback_t)(rest_api_request_t *request);

rest_api_retry_policy_t *rest_api_retry_policy_new(
   int status_codes[],
   size_t status_codes_len,
   time_t delays[],
   size_t delays_len);

rest_api_request_t *rest_api_request_new(struct http_request_t *http_request);

void rest_api_request_free(rest_api_request_t *request);

struct http_request_t *rest_api_get_http_request(rest_api_request_t *request);

void rest_api_request_set_retry_policy(
   rest_api_request_t *request,
   rest_api_retry_policy_t *retry_policy);

void rest_api_request_set_response_handler(
   rest_api_request_t *request,
   int status_code,
   bool retryable,
   rest_api_response_callback_t callback,
   cloud_storage_operation_state_t *user_data);

void rest_api_request_set_default_response_handler(
   rest_api_request_t *request,
   bool retryable,
   rest_api_response_callback_t callback,
   cloud_storage_operation_state_t *user_data);

int *rest_api_get_response_callback_status_codes(rest_api_request_t *request);

rest_api_response_callback_t rest_api_request_get_default_callback(rest_api_request_t *request);

void *rest_api_get_default_callback_user_data(rest_api_request_t *request);

rest_api_response_callback_t rest_api_request_get_callback(
   rest_api_request_t *request,
   int status_code);

cloud_storage_operation_state_t *rest_api_get_callback_user_data(
   rest_api_request_t *request,
   int status_code);

void rest_api_request_execute(rest_api_request_t *request);

RETRO_END_DECLS

#endif
