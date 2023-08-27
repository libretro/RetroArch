#include <stdio.h>

#include "webhooks.h"
#include "webhooks_macro_manager.h"
#include "webhooks_oauth.h"

#include "../tasks/tasks_internal.h"
#include "rc_api_request.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

//  ---------------------------------------------------------------------------


//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
static void wmm_end_http_request(async_http_request_t* request)
{
  rc_api_destroy_request(&request->request);

  if (request->callback/* && !rcheevos_load_aborted()*/)
    request->callback(request->callback_data);

  /* rich presence request will be reused on next ping - reset the attempt
   * counter. for all other request types, free the request object */
  //free(request->request.url);
  //request->request.url = NULL;

  //free(request->headers);
  //request->headers = NULL;

  //free(request);
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
static void wmm_send_http_request_callback
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
    strlcpy(buffer, "Load aborted", sizeof(buffer));
  }
  else*/ if (error)
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
      //if (request->id)
        //CHEEVOS_LOG(RCHEEVOS_TAG "%s %u\n", request->success_message, request->id);
      //else
        //CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", request->success_message);
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

    //CHEEVOS_LOG(RCHEEVOS_TAG "%s\n", errbuf);
  }
}

//  ---------------------------------------------------------------------------
//  Configures the HTTP request to GET the macro from the webhook server.
//  ---------------------------------------------------------------------------
static void wmm_send_http_request(const wb_locals_t* locals, async_http_request_t* request)
{
  task_push_http_transfer_with_headers
  (
    request->request.url,
    true,
    "GET",
    request->headers,
    wmm_send_http_request_callback,
    request
  );
}

//  ---------------------------------------------------------------------------
//  Builds and sets the request's header, mainly the bearer token.
//  ---------------------------------------------------------------------------
static void wmm_set_request_header(async_http_request_t* request)
{
  //  Builds the header containing the authorization.
  const char* access_token = woauth_get_accesstoken();

  if (access_token == NULL)
  {
    //CHEEVOS_LOG(RCHEEVOS_TAG "Failed to retrieve an access token\n");
    return;
  }

  const char* authorization_header = "Authorization: Bearer ";
  const size_t auth_header_len = strlen(authorization_header);
  const size_t token_len = strlen(access_token);

  //  Adds 3 because we're appending two characters and a null character at the end
  const size_t header_length = auth_header_len + token_len + 3;

  char* headers = (char*)malloc(header_length);

  if (headers == NULL) {
    //CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate header\n");
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
static void wmm_set_request_url(const wb_locals_t* locals, async_http_request_t* request)
{
  const settings_t *settings = config_get_ptr();
  const char* base_url = settings->arrays.cheevos_webhook_url;

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
static void wmm_prepare_http_request(const wb_locals_t* locals, async_http_request_t* request)
{
  wmm_set_request_url(locals, request);

  wmm_set_request_header(request);
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
static void wmm_initiate_macro_request(wb_locals_t* locals, async_http_request_t* request)
{
  wmm_prepare_http_request(locals, request);

  wmm_send_http_request(locals, request);
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
static void wmm_on_request_completed
(
  async_http_request_t *request,
  http_transfer_data_t *data,
  char buffer[],
  size_t buffer_size
)
{
  on_macro_downloaded_t on_macro_downloaded = (on_macro_downloaded_t)(request->callback);
  struct wb_locals_t* locals = (struct wb_locals_t*)(request->callback_data);

  on_macro_downloaded(locals, data->data, data->len);
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
void wmm_download_macro(wb_locals_t* locals, on_macro_downloaded_t on_macro_downloaded)
{
  //  Clears any progress.
  locals->macro[0] = '\0';

  async_http_request_t *request = (async_http_request_t*) malloc(sizeof(async_http_request_t));

  if (!request)
  {
    //CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate rich presence request\n");
    return;
  }

  request->handler = wmm_on_request_completed;
  request->callback = on_macro_downloaded;
  request->callback_data = locals;

  wmm_initiate_macro_request(locals, request);
}
