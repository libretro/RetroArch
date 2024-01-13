#include <stdio.h>

#include "include/webhooks.h"
#include "include/webhooks_progress_downloader.h"
#include "include/webhooks_oauth.h"

#include "../tasks/tasks_internal.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

//  ---------------------------------------------------------------------------


// //  ---------------------------------------------------------------------------
// //
// //  ---------------------------------------------------------------------------
// static void wpd_end_http_request(async_http_request_t* request)
// {
//   rc_api_destroy_request(&request->request);
//
//   if (request->callback/* && !rcheevos_load_aborted()*/)
//     request->callback(request->callback_data);
//
//   /* rich presence request will be reused on next ping - reset the attempt
//    * counter. for all other request types, free the request object */
//   //free(request->request.url);
//   //request->request.url = NULL;
//
//   //free(request->headers);
//   //request->headers = NULL;
//
//   //free(request);
// }

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
static void wpd_send_http_request_callback
(
  retro_task_t* task,
  void* task_data,
  void* user_data,
  const char* error
)
{
  struct async_http_request_t *request = (struct async_http_request_t*)user_data;
  http_transfer_data_t      *data    = (http_transfer_data_t*)task_data;
  //const bool                 aborted = rcheevos_load_aborted();
  char buffer[224];

  /*if (aborted)
  {
    // load was aborted. don't process the response
    strncpy(buffer, "Load aborted", sizeof(buffer));
    buffer[sizeof(buffer)-1] = '\0';
  }
  else*/ if (error)
  {
    strncpy(buffer, error, sizeof(buffer));
    buffer[sizeof(buffer)-1] = '\0';
  }
  else if (!data)
  {
    /* Server did not return HTTP headers */
    strncpy(buffer, "Server communication error", sizeof(buffer));
    buffer[sizeof(buffer)-1] = '\0';
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
    else {
      /* Server sent empty response without error status code */
      strncpy(buffer, "No response from server", sizeof(buffer));
      buffer[sizeof(buffer)-1] = '\0';
    }
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
        WEBHOOKS_LOG(WEBHOOKS_TAG "%s %u\n", request->success_message, request->id);
      else
        WEBHOOKS_LOG(WEBHOOKS_TAG "%s\n", request->success_message);
    }
  }
  else
  {
    /* encountered an error */
    char errbuf[256];
    if (request->id)
      snprintf(errbuf, sizeof(errbuf), "%s %u: %s", request->failure_message, request->id, buffer);
    else
      snprintf(errbuf, sizeof(errbuf), "%s: %s", request->failure_message, buffer);

    WEBHOOKS_LOG(WEBHOOKS_TAG "%s\n", errbuf);
  }
}

//  ---------------------------------------------------------------------------
//  Configures the HTTP request to GET the macro from the webhook server.
//  ---------------------------------------------------------------------------
static void wpd_send_http_request(const wb_locals_t* locals, async_http_request_t* request)
{
  task_push_http_transfer_with_headers
  (
    request->request.url,
    true,
    "GET",
    request->headers,
    wpd_send_http_request_callback,
    request
  );
}

//  ---------------------------------------------------------------------------
//  Builds and sets the request's header, mainly the bearer token.
//  ---------------------------------------------------------------------------
static void wpd_set_request_header(async_http_request_t* request)
{
  //  Builds the header containing the authorization.
  const char* access_token = woauth_get_accesstoken();

  if (access_token == NULL)
  {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Failed to retrieve an access token\n");
    return;
  }

  const char* authorization_header = "Authorization: Bearer ";
  const size_t auth_header_len = strlen(authorization_header);
  const size_t token_len = strlen(access_token);

  //  Adds 3 because we're appending two characters and a null character at the end
  const size_t header_length = auth_header_len + token_len + 3;

  char* headers = (char*)malloc(header_length);

  if (headers == NULL) {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Failed to allocate header\n");
    return;
  }

  strncpy(headers, authorization_header, auth_header_len);
  strncpy(headers + auth_header_len, access_token, token_len);
  
  headers[auth_header_len + token_len] = '\r';
  headers[auth_header_len + token_len + 1] = '\n';
  headers[auth_header_len + token_len + 2] = '\0';

  request->headers = headers;
}

//  ---------------------------------------------------------------------------
//  Builds and sets the request's URL.
//  ---------------------------------------------------------------------------
static void wpd_set_request_url(const wb_locals_t* locals, async_http_request_t* request)
{
  const settings_t *settings = config_get_ptr();
  const char* base_url = settings->arrays.webhook_url;

  const size_t url_len = strlen(base_url);
  const size_t hash_length = sizeof(locals->hash);

  const char* hash_query = "/macros/";
  const size_t hash_query_length = strlen(hash_query);
  const size_t total_url_length = url_len + hash_query_length + hash_length + 1;

  char* request_url = malloc(total_url_length);
  
  //  Assembles the URL
  snprintf
  (
    request_url,
    total_url_length,
    "%s%s%s",
    base_url,
    hash_query,
    locals->hash
  );
  
  request->request.url = request_url;
}

//  ---------------------------------------------------------------------------
//  Configures the HTTP request to GET the macro from the webhook server.
//  ---------------------------------------------------------------------------
static void wpd_prepare_http_request(const wb_locals_t* locals, async_http_request_t* request)
{
  wpd_set_request_url(locals, request);

  wpd_set_request_header(request);
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
static void wpd_initiate_macro_request(wb_locals_t* locals, async_http_request_t* request)
{
  wpd_prepare_http_request(locals, request);

  wpd_send_http_request(locals, request);
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
static void wpd_on_request_completed
(
  async_http_request_t *request,
  http_transfer_data_t *data,
  char buffer[],
  size_t buffer_size
)
{
  on_game_progress_downloaded_t on_game_progress_downloaded = (on_game_progress_downloaded_t)(request->callback);
  struct wb_locals_t* locals = (struct wb_locals_t*)(request->callback_data);

  WEBHOOKS_LOG(WEBHOOKS_TAG "Received progress for game's hash '%s'\n", locals->hash);

  on_game_progress_downloaded(locals, data->data, data->len);
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
void wpd_download_game_progress(wb_locals_t* locals, on_game_progress_downloaded_t on_game_progress_downloaded)
{
  WEBHOOKS_LOG(WEBHOOKS_TAG "Requesting progress for game's hash '%s'\n", locals->hash);

  //  Clears any previous progress.
  locals->game_progress[0] = '\0';

  async_http_request_t *request = (async_http_request_t*) malloc(sizeof(async_http_request_t));

  if (!request)
  {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Failed to allocate rich presence request\n");
    return;
  }

  request->handler = wpd_on_request_completed;
  request->callback = (async_client_callback)on_game_progress_downloaded;
  request->callback_data = locals;

  wpd_initiate_macro_request(locals, request);
}
