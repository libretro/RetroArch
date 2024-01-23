#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include <stdio.h>
#include <string.h>
#include <net/net_http.h>

#include "include/webhooks.h"
#include "include/webhooks_oauth.h"

#ifdef HAVE_CHEEVOS
#include "../deps/rcheevos/src/rapi/rc_api_common.h"
#endif

#include "../libretro-common/include/features/features_cpu.h"

#include "../command.h"

enum oauth_async_io_type
{
    DEVICE_CODE  = 0,
    ACCESS_TOKEN = 1
};

#define ACCESS_TOKEN_FREQUENCY (5 * 1000 * 1000) + cpu_features_get_time_usec();

#define MAX_DEVICECODE_LENGTH 128
#define MAX_USERCODE_LENGTH 128
#define OAUTH_ERROR_MESSAGE_LENGTH 512
#define OAUTH_TOKEN_LENGTH 2048

const char* DEFAULT_CLIENT_ID = "retro_arch";

typedef struct woauth_code_response_t {
    char client_id[128];
    const char* device_code;
    const char* user_code;
} woauth_code_response_t;

typedef struct woauth_token_response_t {
    int succeeded;
    char error_message[OAUTH_ERROR_MESSAGE_LENGTH];
    const char* access_token;
    const char* refresh_token;
    int expires_in;
} woauth_token_response_t;

bool token_refresh_scheduled;
woauth_code_response_t oauth_code_response;
woauth_token_response_t oauth_token_response;

bool is_pairing;

static void woauth_schedule_accesstoken_retrieval
(
  void
);

static void woauth_trigger_accesstoken_retrieval
(
  void
);

// static void woauth_end_pairing()
// {
//   is_pairing = false;
//   command_event(CMD_EVENT_WEBHOOK_ABORT_ASSOCIATION, NULL);
// }

//  ------------------------------------------------------------------------------
//  Drops the User Code from the configuration.
//  This instance can't be linked to another webhook server.
//  ------------------------------------------------------------------------------
static void woauth_clear_usercode
(
  void
)
{
  settings_t *settings = config_get_ptr();
  configuration_set_string(settings, settings->arrays.webhook_usercode, "");
}

//  ------------------------------------------------------------------------------
//  Called when the access token must be retrieved.
//  ------------------------------------------------------------------------------
static void woauth_retry_accesstoken_retrieval(retro_task_t* task)
{
  woauth_trigger_accesstoken_retrieval();

  task_set_finished(task, 1);
}

//  ------------------------------------------------------------------------------
//  Schedules the retrieval of a new access token after it has expired.
//  ------------------------------------------------------------------------------
static void woauth_schedule_accesstoken_retrieval
(
  void
)
{
  retro_task_t* retry_task = task_init();
  retry_task->when = ACCESS_TOKEN_FREQUENCY;
  retry_task->handler = woauth_retry_accesstoken_retrieval;
  task_queue_push(retry_task);
}

// //  ------------------------------------------------------------------------------
// //  Clears the memory from the request.
// //  ------------------------------------------------------------------------------
// static void woauth_free_request
// (
//   async_http_request_t* request
// )
// {
//   rc_api_destroy_request(&request->request);
//
//   if (request->callback)
//     request->callback(request->callback_data);
//
//   free(request);
// }

//  ------------------------------------------------------------------------------
//  Handles the HTTP response.
//  ------------------------------------------------------------------------------
static void woauth_end_http_request
(
  retro_task_t* task,
  void* task_data,
  void* user_data,
  const char* error
)
{
  async_http_request_t *request = (async_http_request_t*)user_data;
  http_transfer_data_t *data  = (http_transfer_data_t*)task_data;
  char buffer[224];

  if (error)
  {
    strlcpy(buffer, error, sizeof(buffer));
  }
  else if (!data)
  {
    // Server did not return HTTP headers
    strlcpy(buffer, "Server communication error", sizeof(buffer));
  }
  else if (!data->data || !data->len)
  {
    if (data->status <= 0)
    {
      // Something occurred which prevented the response from being processed.
      // assume the server request hasn't happened and try again.
      snprintf(buffer, sizeof(buffer), "task status code %d", data->status);
      return;
    }

    if (data->status != 200) // Server returned error via status code.
    {
      snprintf(buffer, sizeof(buffer), "Received HTTP status code %d", data->status);
    }
    else {
      // Server sent empty response without error status code
      strlcpy(buffer, "No response from server", sizeof(buffer));
    }
  }
  else
  {
    // indicate success unless handler provides error
    buffer[0] = '\0';

    // Call appropriate handler to process the response
    // NOTE: data->data is not null-terminated. Most handlers assume the
    // response is properly formatted or will encounter a parse failure
    // before reading past the end of the data
    if (request->handler)
      request->handler(request, data, buffer, sizeof(buffer));
  }

  if (!buffer[0])
  {
    // success
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
    // encountered an error
    char errbuf[256];
    if (request->id)
      snprintf(errbuf, sizeof(errbuf), "%s %u: %s", request->failure_message, request->id, buffer);
    else
      snprintf(errbuf, sizeof(errbuf), "%s: %s", request->failure_message, buffer);

    WEBHOOKS_LOG(WEBHOOKS_TAG "%s\n", errbuf);
  }

  // Check again a moment later.
  if (data != NULL) {
    if (data->status == 204 && request->type == ACCESS_TOKEN) {
      WEBHOOKS_LOG(WEBHOOKS_TAG "User has not entered the code on the website. Retrying...\n");
      woauth_schedule_accesstoken_retrieval();
      //woauth_free_request(request);
    }// else if (data->status != 200 && request->type == ACCESS_TOKEN) {
      //woauth_schedule_accesstoken_retrieval();
    //}
  }
}

//  ------------------------------------------------------------------------------
//  Sends an HTTP request.
//  ------------------------------------------------------------------------------
static void woauth_begin_http_request
(
  async_http_request_t* request
)
{
  task_push_http_post_transfer_with_headers
  (
    request->request.url,
    request->request.post_data,
    true,
    "POST",
    NULL,
    woauth_end_http_request,
    request
  );
}

//  ------------------------------------------------------------------------------
//  Retrieves the Access Token, Refresh Token and Expiry fields from the response.
//  ------------------------------------------------------------------------------
static void woauth_handle_accesstoken_response
(
  async_http_request_t *request,
  http_transfer_data_t *data,
  char buffer[],
  size_t buffer_size
)
{
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),       //  Unused. Still required by rc_api_common.c.
    RC_JSON_NEW_FIELD("Error"),         //  Unused. Still required by rc_api_common.c.
    RC_JSON_NEW_FIELD("access_token"),
    RC_JSON_NEW_FIELD("refresh_token"),
    RC_JSON_NEW_FIELD("expires_in"),
    //JSON_NEW_FIELD("id_token"),       // This is part of the standard but it is not used in the webhook.
  };

  rc_api_response_t* api_response = (rc_api_response_t*)&oauth_token_response;

  rc_api_server_response_t response_obj;
  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = data->data;
  response_obj.body_length = rc_json_get_object_string_length(data->data);

  int result = rc_json_parse_server_response(api_response, &response_obj, fields, sizeof(fields) / sizeof(fields[0]));

  if (result != 0) {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Unable to parse the response for a new access token request\n");
    return;
  }

  if (!rc_json_get_required_string(&oauth_token_response.access_token, api_response, &fields[2], "access_token")) {
    WEBHOOKS_LOG(WEBHOOKS_TAG "No access token received in the response\n");
    return;
  }

  if (!rc_json_get_required_string(&oauth_token_response.refresh_token, api_response, &fields[3], "refresh_token")) {
    WEBHOOKS_LOG(WEBHOOKS_TAG "No refresh token received in the response\n");
    return;
  }
  
  if (!rc_json_get_required_num(&oauth_token_response.expires_in, api_response, &fields[4], "expires_in")) {
    WEBHOOKS_LOG(WEBHOOKS_TAG "No expiration received in the response\n");
    return;
  }
  
  //  Sets the device code in the configuration as well so that it is visible to the user.
  retro_time_t now = cpu_features_get_time_usec();
  retro_time_t expiration_timestamp = now + 1000 * 1000 * (long long)oauth_token_response.expires_in;
  char expiration[64];
  sprintf(expiration, "%lld", expiration_timestamp);
  
  settings_t *settings = config_get_ptr();
  configuration_set_string(settings, settings->arrays.webhook_expiresin, expiration);
  configuration_set_string(settings, settings->arrays.webhook_accesstoken, oauth_token_response.access_token);
  configuration_set_string(settings, settings->arrays.webhook_refreshtoken, oauth_token_response.refresh_token);

  woauth_clear_usercode();
  token_refresh_scheduled = false;

  WEBHOOKS_LOG(WEBHOOKS_TAG "Access token received and saved\n");
  
  free(request);
  request = NULL;

  is_pairing = false;
  //if (is_pairing)
  //  is_end_pairing_scheduled = true;
}

bool initialized = false;

//  ------------------------------------------------------------------------------
//  Initializes the HTTP request used to get an access token.
//  ------------------------------------------------------------------------------
static void woauth_initialize_accesstoken_request
(
  async_http_request_t* request
)
{
  rc_api_url_builder_t builder;

  const settings_t *settings = config_get_ptr();

  const char* client_id = DEFAULT_CLIENT_ID;
  const char* device_code = oauth_code_response.device_code;

  rc_url_builder_init(&builder, &(request->request.buffer), 48);
  rc_url_builder_append_str_param(&builder, "client_id", client_id);

  if (device_code != NULL && strlen(device_code) > 0)
    rc_url_builder_append_str_param(&builder, "device_code", device_code);
  else
    rc_url_builder_append_str_param(&builder, "refresh_token", settings->arrays.webhook_refreshtoken);

  request->type = ACCESS_TOKEN;
  request->handler = (async_http_handler)woauth_handle_accesstoken_response;
  request->request.url = settings->arrays.webhook_token_url;
  request->request.post_data = rc_url_builder_finalize(&builder);
}

//  ------------------------------------------------------------------------------
//  Creates the HTTP request to get an access token and sends it.
//  ------------------------------------------------------------------------------
static void woauth_trigger_accesstoken_retrieval
(
  void
)
{
  async_http_request_t *request = (async_http_request_t*) calloc(1, sizeof(async_http_request_t));

  if (!request) {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Failed to allocate an OAuth2 request\n");
    return;
  }

  WEBHOOKS_LOG(WEBHOOKS_TAG "Starting retrieving an access token since the 'device_code' is available\n");

  woauth_initialize_accesstoken_request(request);

  woauth_begin_http_request(request);
}

//  ------------------------------------------------------------------------------
//  Saves the device code (required to get the access token later)
//  and prints the user code so that it is visible by the user.
//  ------------------------------------------------------------------------------
void woauth_handle_devicecode_response
(
  async_http_request_t *request,
  http_transfer_data_t *data,
  char buffer[],
  size_t buffer_size
)
{
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),       //  Unused. Still required by rc_api_common.c.
    RC_JSON_NEW_FIELD("Error"),         //  Unused. Still required by rc_api_common.c.
    RC_JSON_NEW_FIELD("device_code"),
    RC_JSON_NEW_FIELD("user_code")
  };

  rc_api_response_t* api_response = (rc_api_response_t*)&oauth_code_response;

  rc_api_server_response_t response_obj;
  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = data->data;
  response_obj.body_length = rc_json_get_object_string_length(data->data);

  int result = rc_json_parse_server_response(api_response, &response_obj, fields, sizeof(fields) / sizeof(fields[0]));

  if (result != 0) {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Unable to read the OAuth response\n");
    is_pairing = true;
    return;
  }

  if (!rc_json_get_required_string(&oauth_code_response.device_code, api_response, &fields[2], "device_code")) {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Unable to read the 'device_code' from the response\n");
    is_pairing = true;
    return;
  }

  if (!rc_json_get_required_string(&oauth_code_response.user_code, api_response, &fields[3], "user_code")) {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Unable to read the 'user_code' from the response\n");
    is_pairing = true;
    return;
  }

  woauth_clear_usercode();

  //  Sets the device code in the configuration as well so that it is visible to the user.
  settings_t *settings = config_get_ptr();
  configuration_set_string(settings, settings->arrays.webhook_usercode, oauth_code_response.user_code);

  free(request);
  request = NULL;
      
  // Trigger the access_token retrieval.
  woauth_trigger_accesstoken_retrieval();
}

//  ------------------------------------------------------------------------------
//  Configures the request to retrieve the device code and user code.
//  ------------------------------------------------------------------------------
static void woauth_initialize_devicecode_request
(
  async_http_request_t* request
)
{
  rc_api_url_builder_t builder;
    
  const settings_t *settings = config_get_ptr();
    
  rc_url_builder_init(&builder, &(request->request.buffer), 48);
  rc_url_builder_append_str_param(&builder, "client_id", oauth_code_response.client_id);
  rc_url_builder_append_str_param(&builder, "scope", "scope-to-be-determined");
  
  request->handler = (async_http_handler)woauth_handle_devicecode_response;
  request->request.url = settings->arrays.webhook_code_url;
  request->request.post_data = rc_url_builder_finalize(&builder);
}

//  ------------------------------------------------------------------------------
//  Starts the OAuth Device Flow to first retrieve a device code and a user code.
//  ------------------------------------------------------------------------------
void woauth_schedule_devicecode_retrieval
(
  void
)
{
  async_http_request_t *request = (async_http_request_t*) calloc(1, sizeof(async_http_request_t));

  if (!request)
  {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Failed to allocate an OAuth device code request\n");
    return;
  }
  
  WEBHOOKS_LOG(WEBHOOKS_TAG "Starting OAuth Device Flow \n");

  woauth_initialize_devicecode_request(request);

  woauth_begin_http_request(request);
}

//  ------------------------------------------------------------------------------
//  Starts the process to associate the emulator with the Webhook server.
//  ------------------------------------------------------------------------------
void woauth_initiate_pairing
(
  void
)
{
  is_pairing = true;

  WEBHOOKS_LOG(WEBHOOKS_TAG "Starting pairing device to the server\n");

  strlcpy(oauth_code_response.client_id, DEFAULT_CLIENT_ID, sizeof(oauth_code_response.client_id));

  woauth_schedule_devicecode_retrieval();
}

//  ------------------------------------------------------------------------------
//
//  ------------------------------------------------------------------------------
bool woauth_is_pairing
(
  void
)
{
  return is_pairing;
}

//  ------------------------------------------------------------------------------
//
//  ------------------------------------------------------------------------------
void woauth_abort_pairing
(
  void
)
{
  WEBHOOKS_LOG(WEBHOOKS_TAG "Cancelling pairing device to the server\n");

  is_pairing = false;
}

//  ------------------------------------------------------------------------------
//  Returns an access token used to contact the Webhook server.
//  ------------------------------------------------------------------------------
const char* woauth_get_accesstoken
(
  void
)
{
  const settings_t *settings = config_get_ptr();
  const int EXPIRATION_WINDOW = 1000 * 10 * 5;

  strlcpy(oauth_code_response.client_id, DEFAULT_CLIENT_ID, sizeof(oauth_code_response.client_id));

  retro_time_t now = cpu_features_get_time_usec();

  char * e;

  retro_time_t expecting_refresh = (retro_time_t)strtoll(settings->arrays.webhook_expiresin, &e, 10);
    
  if (*e != 0) {
    //  The emulator has not been associated.
    //  Nothing can be done without any user intervention.
    WEBHOOKS_LOG(WEBHOOKS_TAG "Unable to read the expires_in from the configuration: the association must be established.\n");
    return NULL;
  }

  if (expecting_refresh == 0) {
    //  The emulator has not been associated.
    //  Nothing can be done without any user intervention.
    WEBHOOKS_LOG(WEBHOOKS_TAG "The value expires_in in the configuration is not set (0): the association must be established again\n");
    return NULL;
  }
  else if (now >= expecting_refresh) {
    if(!token_refresh_scheduled) {
      if (strlen(settings->arrays.webhook_refreshtoken) > 0) {
        //  Let's get an access token asap using the refresh token.
        token_refresh_scheduled = true;
        woauth_trigger_accesstoken_retrieval();
        return NULL;
      }
    }
    else {
      //  The refresh token as well as the device code is missing.
      //  Nothing can be done without any user intervention.
      WEBHOOKS_LOG(WEBHOOKS_TAG "The refresh_token is missing from the configuration: the association must be established again\n");
      return NULL;
    }
  }
  else if (now + EXPIRATION_WINDOW >= expecting_refresh) {
    if (!token_refresh_scheduled) {
      //  The access token expires within the next X minutes;
      //  Let's refresh it now.
      WEBHOOKS_LOG(WEBHOOKS_TAG "The access_token is about to expire. A new access_token is being retrieved using the refresh_token\n");

      token_refresh_scheduled = true;
      woauth_trigger_accesstoken_retrieval();
      return settings->arrays.webhook_accesstoken;
    }
    else {
      //  The refresh token as well as the device code is missing.
      //  Nothing can be done without any user intervention.
      WEBHOOKS_LOG(WEBHOOKS_TAG "A new access_token retrieval is already scheduled.\n");
      return NULL;
    }
  }
    
  //  The access token is not close to expire.
  return settings->arrays.webhook_accesstoken;
}
