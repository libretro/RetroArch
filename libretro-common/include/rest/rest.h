/* Copyright  (C) 2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rest.h).
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

#ifndef _REST_H
#define _REST_H

#include <boolean.h>
#include <stddef.h>
#include <time.h>

#include <retro_common_api.h>

#include <net/net_http.h>

RETRO_BEGIN_DECLS

typedef struct rest_request_t rest_request_t;

typedef struct rest_retry_policy_t rest_retry_policy_t;

rest_retry_policy_t *rest_retry_policy_new(
   int *status_codes,
   size_t status_codes_len,
   time_t *delays,
   size_t delays_len);

rest_request_t *rest_request_new(struct http_request_t *http_request);

void rest_request_set_header(rest_request_t *request, char *name, char *value, bool replace);

void rest_request_free(rest_request_t *request);

void rest_request_set_retry_policy(
   rest_request_t *request,
   rest_retry_policy_t *retry_policy);

struct http_response_t *rest_request_execute(rest_request_t *request);

RETRO_END_DECLS

#endif
