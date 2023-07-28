/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2016 - Andre Leiradella
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

#include <string.h>
#include <ctype.h>

#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <features/features_cpu.h>
#include <formats/cdfs.h>
#include <formats/m3u_file.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <retro_timers.h>
#include <net/net_http.h>
#include <libretro.h>
#include <lrc_hash.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "webhooks.h"
#include "../deps/rcheevos/include/rc_api_runtime.h"

static void end_webhook_http_request
(
  richpresence_async_io_request* request
)
{
  rc_api_destroy_request(&request->request);

  if (request->callback && !rcheevos_load_aborted())
    request->callback(request->callback_data);

  /* rich presence request will be reused on next ping - reset the attempt
   * counter. for all other request types, free the request object */
  //free(request->request.url);
  //request->request.url = NULL;
  
  //free(request->headers);
  //request->headers = NULL;
  
  //free(request);
}

static void handle_webhook_http_callback
(
  retro_task_t* task,
  void* task_data,
  void* user_data,
  const char* error
)
{
  struct richpresence_async_io_request *request = (struct richpresence_async_io_request*)user_data;
  http_transfer_data_t      *data    = (http_transfer_data_t*)task_data;
  const bool                 aborted = rcheevos_load_aborted();
  char buffer[224];

  if (aborted)
  {
    /* load was aborted. don't process the response */
    strlcpy(buffer, "Load aborted", sizeof(buffer));
  }
  else if (error)
  {
    strlcpy(buffer, error, sizeof(buffer));
  }
  else if (!data)
  {
    /* Server did not return HTTP headers */
    strlcpy(buffer, "Server communication error", sizeof(buffer));
  }
  else if (!data->data || !data->len)
  {
    if (data->status <= 0)
    {
      /* something occurred which prevented the response from being processed.
       * assume the server request hasn't happened and try again. */
      snprintf(buffer, sizeof(buffer), "task status code %d", data->status);
      return;
    }

    if (data->status != 200) /* Server returned error via status code. */
    {
      snprintf(buffer, sizeof(buffer), "HTTP error code %d", data->status);
    }
    else /* Server sent empty response without error status code */
      strlcpy(buffer, "No response from server", sizeof(buffer));
  }
  else
  {
    /* indicate success unless handler provides error */
    buffer[0] = '\0';

    /* Call appropriate handler to process the response */
    /* NOTE: data->data is not null-terminated. Most handlers assume the
     * response is properly formatted or will encounter a parse failure
     * before reading past the end of the data */
    if (request->handler)
      request->handler(request, data, buffer, sizeof(buffer));
  }

  if (!buffer[0])
  {
    /* success */
    if (request->success_message)
    {
      if (request->id)
        CHEEVOS_LOG(RCHEEVOS_TAG "%s %u\n", request->success_message, request->id);
      else
        CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", request->success_message);
    }
  }
  else
  {
    /* encountered an error */
    char errbuf[256];
    if (request->id)
      snprintf(errbuf, sizeof(errbuf), "%s %u: %s",
               request->failure_message, request->id, buffer);
    else
      snprintf(errbuf, sizeof(errbuf), "%s: %s",
               request->failure_message, buffer);

    CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", errbuf);
  }

  end_webhook_http_request(request);
}

static void begin_webhook_http_request
(
  richpresence_async_io_request* request
)
{
  task_push_http_post_transfer_with_headers
  (
    request->request.url,
    request->request.post_data,
    true,
    "POST",
    request->headers,
    handle_webhook_http_callback,
    request
  );
}

static void prepare_http_request
(
  richpresence_async_io_request* request
)
{
  char buffer[512] = "";
  const settings_t *settings = config_get_ptr();
  const char* webhook_url = settings->arrays.cheevos_webhook_url;

  //  Retrieves the Rich Presence data from the cheevos API.
  rcheevos_get_richpresence(buffer, sizeof(buffer));
  
  const size_t url_len = strlen(webhook_url);
  request->request.url = malloc(url_len);
  strncpy(request->request.url , webhook_url, url_len);
  
  rc_api_url_builder_t builder;
  rc_url_builder_init(&builder, &request->request.buffer, 48);

  rc_url_builder_append_unum_param(&builder, "g", rcheevos_locals.game.id);
  rc_url_builder_append_str_param(&builder, "m", buffer);
  request->request.post_data = rc_url_builder_finalize(&builder);
}

static void async_send_presence
(
  richpresence_async_io_request* request
)
{
  prepare_http_request(request);

  begin_webhook_http_request(request);
}

void update_presence()
{
  richpresence_async_io_request *request = (richpresence_async_io_request*) calloc(1, sizeof(richpresence_async_io_request));

  if (!request)
  {
    CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate rich presence request\n");
    return;
  }

  request->type = CHEEVOS_ASYNC_RICHPRESENCE;

  //  Builds the header containing the authorization.
  const char* access_token = woauth_get_accesstoken();

  if (access_token == NULL) {
    CHEEVOS_LOG(RCHEEVOS_TAG "Failed to retrieve an access token\n");
    return;
  }

  const char* authorization_header = "Authorization: Bearer ";
  const size_t auth_header_len = strlen(authorization_header);
  const size_t token_len = strlen(access_token);

  //  Adds 3 because we're appending two characters and a null character at the end
  const size_t header_length = auth_header_len + token_len + 3;

  char* headers = (char*)malloc(header_length);

  if (headers == NULL) {
    CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate header\n");
    return;
  }

  strncpy(headers, authorization_header, auth_header_len);
  strncpy(headers + auth_header_len, access_token, token_len);
  headers[auth_header_len + token_len] = '\r';
  headers[auth_header_len + token_len + 1] = '\n';
  headers[auth_header_len + token_len + 2] = '\0';

  request->headers = headers;
  request->failure_message = "Error sending webhook data";

  async_send_presence(request);
}
