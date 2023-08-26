#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include <stdlib.h>
#include <net/net_http.h>

#include "webhooks_oauth.h"
#include "json_parser.h"

enum oauth_async_io_type
{
    DEVICE_CODE  = 0,
    ACCESS_TOKEN = 1
};

#define ACCESS_TOKEN_FREQUENCY (5 * 1000 * 1000) + cpu_features_get_time_usec();

const long MAX_DEVICECODE_LENGTH = 128;
const long MAX_USERCODE_LENGTH = 128;

const char* DEFAULT_CLIENT_ID = "retro_arch";

typedef void (*oauth2_client_callback)(void* userdata);

typedef void (*oauth2_async_handler)
(
  struct oauth2_async_io_request *request,
  http_transfer_data_t *data,
  char buffer[],
  size_t buffer_size
);

typedef struct oauth2_async_io_request
{
    rc_api_request_t request;
    oauth2_async_handler handler;
    int id;
    oauth2_client_callback callback;
    void* callback_data;
    int attempt_count;
    const char* success_message;
    const char* failure_message;
    const char* headers;
    char type;
} oauth2_async_io_request;

typedef struct oauth_t {
    char client_id[128];
    char device_code[MAX_DEVICECODE_LENGTH];
    char user_code[MAX_USERCODE_LENGTH];
} oauth_t;


oauth_t current_oauth;

bool token_refresh_scheduled;

static void woauth_schedule_accesstoken_retrieval();
static void woauth_trigger_accesstoken_retrieval();

//  ------------------------------------------------------------------------------
//  Drops the Device Code from the memory structure.
//  ------------------------------------------------------------------------------
static void woauth_clear_devicecode()
{
  settings_t *settings = config_get_ptr();
  configuration_set_string(settings, settings->arrays.cheevos_webhook_usercode, "");
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
static void woauth_schedule_accesstoken_retrieval()
{
  retro_task_t* retry_task = task_init();
  retry_task->when = ACCESS_TOKEN_FREQUENCY;
  retry_task->handler = woauth_retry_accesstoken_retrieval;
  task_queue_push(retry_task);
}

//  ------------------------------------------------------------------------------
//  Clears the memory from the request.
//  ------------------------------------------------------------------------------
static void woauth_free_request
(
  oauth2_async_io_request* request
)
{
  rc_api_destroy_request(&request->request);

  if (request->callback)
    request->callback(request->callback_data);

  /* rich presence request will be reused on next ping - reset the attempt
   * counter. for all other request types, free the request object */
  free(request);
}

//  ------------------------------------------------------------------------------
//  Handles the HTTP response.
//  ------------------------------------------------------------------------------
static void woauth_end_oauth2_http_request
(
  retro_task_t* task,
  void* task_data,
  void* user_data,
  const char* error
)
{
  oauth2_async_io_request *request = (oauth2_async_io_request*)user_data;
  http_transfer_data_t      *data  = (http_transfer_data_t*)task_data;
  char buffer[224];

  if (error)
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

  // Check again a moment later.
  if (data != NULL && data->status != 200 && request->type == ACCESS_TOKEN) {
    woauth_schedule_accesstoken_retrieval();
  }
}

//  ------------------------------------------------------------------------------
//  Sends an HTTP request.
//  ------------------------------------------------------------------------------
static void woauth_begin_oauth2_http_request
(
  oauth2_async_io_request* request
)
{
  task_push_http_post_transfer_with_headers
  (
    request->request.url,
    request->request.post_data,
    true,
    "POST",
    NULL,
    woauth_end_oauth2_http_request,
    request
  );

}

//  ------------------------------------------------------------------------------
//  Retrieves the Access Token, Refresh Token and Expiry fields from the response.
//  ------------------------------------------------------------------------------
static void woauth_handle_accesstoken_response
(
  struct oauth2_async_io_request *request,
  http_transfer_data_t *data,
  char buffer[],
  size_t buffer_size
)
{
  json_field_t fields[] = {
    JSON_NEW_FIELD("access_token"),
    JSON_NEW_FIELD("refresh_token"),
    JSON_NEW_FIELD("expires_in"),
    //JSON_NEW_FIELD("id_token"),       // This is part of the standard but it is not used in the webhook.
  };

  const char* datacopy = data->data;
  int result = parse_json_object(&datacopy, fields, 3, NULL);

  if (result != JSON_OK) {
    free(request);
    request = NULL;
    return;
  }

  settings_t *settings = config_get_ptr();
      
  //  Extracts the Access Token from the parsed fields.
  long max_accesstoken_length = (long)sizeof(settings->arrays.cheevos_webhook_accesstoken);
  long accesstoken_length = fields[0].value_end -  fields[0].value_start - 1;
        
  if (accesstoken_length > max_accesstoken_length)
      accesstoken_length = max_accesstoken_length;
        
  char* webhook_accesstoken = malloc(accesstoken_length);
  strncpy(webhook_accesstoken, fields[0].value_start + 1, accesstoken_length - 1);
  webhook_accesstoken[accesstoken_length-1] = '\0';
    
  //  Extracts the Refresh Token from the parsed fields.
  long max_refreshtoken_length = (long)sizeof(settings->arrays.cheevos_webhook_refreshtoken);
  long refreshtoken_length = fields[1].value_end -  fields[1].value_start - 1;
      
  if (refreshtoken_length > max_refreshtoken_length)
      refreshtoken_length = max_refreshtoken_length;
      
  char* webhook_refreshtoken = malloc(refreshtoken_length);
  strncpy(webhook_refreshtoken, fields[1].value_start + 1, refreshtoken_length - 1);
  webhook_refreshtoken[refreshtoken_length-1] = '\0';

  //  Extracts the Expiration from the parsed fields.
  long expiresin_length = fields[2].value_end -  fields[2].value_start + 1;

  char* webhook_expiresin = malloc(expiresin_length);
  strncpy(webhook_expiresin, fields[2].value_start, expiresin_length);
  webhook_expiresin[expiresin_length-1] = '\0';

  char *end;
  long expires_in = strtol(webhook_expiresin, &end, 10);

  if (*end) {
    // conversion failed (non-integer in string), *end points to non-numeric character
    CHEEVOS_LOG(RCHEEVOS_TAG "Unable to read the expires_in value received from the OAuth server \n");
    woauth_schedule_accesstoken_retrieval();

    free(webhook_expiresin);
    webhook_expiresin = NULL;
    return;
  }

  //  Sets the device code in the configuration as well so that it is visible to the user.
  retro_time_t expiration_timestamp = cpu_features_get_time_usec() + 1000 * 1000 * expires_in;
  char expiration[64];
  sprintf(expiration, "%lld", expiration_timestamp);
  configuration_set_string(settings, settings->arrays.cheevos_webhook_expiresin, expiration);
  configuration_set_string(settings, settings->arrays.cheevos_webhook_accesstoken, webhook_accesstoken);
  configuration_set_string(settings, settings->arrays.cheevos_webhook_refreshtoken, webhook_refreshtoken);

  woauth_clear_devicecode();
  token_refresh_scheduled = false;
    
  free(request);
  request = NULL;
        
  free(webhook_accesstoken);
  webhook_accesstoken = NULL;
        
  free(webhook_refreshtoken);
  webhook_refreshtoken = NULL;

  free(webhook_expiresin);
  webhook_expiresin = NULL;
}

//  ------------------------------------------------------------------------------
//  Initializes the HTTP request used to get an access token.
//  ------------------------------------------------------------------------------
static void woauth_initialize_accesstoken_request
(
  oauth2_async_io_request* request
)
{
  rc_api_url_builder_t builder;
      
  const settings_t *settings = config_get_ptr();
      
  const char* client_id = current_oauth.client_id;
  const char* device_code = current_oauth.device_code;
      
  rc_url_builder_init(&builder, &(request->request.buffer), 48);
  rc_url_builder_append_str_param(&builder, "client_id", client_id);

  if (strlen(device_code) > 0)
    rc_url_builder_append_str_param(&builder, "device_code", device_code);
  else
    rc_url_builder_append_str_param(&builder, "refresh_token", settings->arrays.cheevos_webhook_refreshtoken);
    
  request->type = ACCESS_TOKEN;
  request->handler = (oauth2_async_handler)woauth_handle_accesstoken_response;
  request->request.url = settings->arrays.cheevos_webhook_token_url;
  request->request.post_data = rc_url_builder_finalize(&builder);

  rcheevos_log_post_url(request->request.url, request->request.post_data);
}

//  ------------------------------------------------------------------------------
//  Creates the HTTP request to get an access token and sends it.
//  ------------------------------------------------------------------------------
static void woauth_trigger_accesstoken_retrieval()
{
  oauth2_async_io_request *request = (oauth2_async_io_request*) calloc(1, sizeof(oauth2_async_io_request));

  if (!request)
  {
    CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate an OAuth2 request\n");
    return;
  }

  CHEEVOS_LOG(RCHEEVOS_TAG "Starting retrieving access token \n");

  woauth_initialize_accesstoken_request(request);

  woauth_begin_oauth2_http_request(request);
}

//  ------------------------------------------------------------------------------
//  Saves the device code (required to get the access token later)
//  and prints the user code so that it is visible by the user.
//  ------------------------------------------------------------------------------
void woauth_handle_devicecode_response
(
  struct oauth2_async_io_request *request,
  http_transfer_data_t *data,
  char buffer[],
  size_t buffer_size
)
{
  json_field_t fields[] = {
    JSON_NEW_FIELD("device_code"),
    JSON_NEW_FIELD("user_code")
  };

  const char* datacopy = data->data;
  int result = parse_json_object(&datacopy, fields, 2, NULL);

  if (result != JSON_OK) {
    free(request);
    request = NULL;
    return;
  }

  //  Extracts the Device Code from the parsed fields.
  long device_code_length = fields[0].value_end -  fields[0].value_start - 1;
    
  if (device_code_length > MAX_DEVICECODE_LENGTH)
    device_code_length = MAX_DEVICECODE_LENGTH;
    
  char* webhook_devicecode = malloc(device_code_length);
  strncpy(webhook_devicecode, fields[0].value_start + 1, device_code_length - 1);
  webhook_devicecode[device_code_length-1] = '\0';
    
  //  Extracts the User Code from the parsed fields.
  long user_code_length = fields[1].value_end -  fields[1].value_start - 1;
      
  if (user_code_length > MAX_USERCODE_LENGTH)
      user_code_length = MAX_USERCODE_LENGTH;
      
  char* webhook_usercode = malloc(user_code_length);
  strncpy(webhook_usercode, fields[1].value_start + 1, user_code_length - 1);
  webhook_usercode[user_code_length-1] = '\0';
    
  // Copies the Device Code and the User Code into the memory structure.
  strlcpy(current_oauth.device_code, webhook_devicecode, sizeof(current_oauth.device_code));
  strlcpy(current_oauth.user_code, webhook_usercode, sizeof(current_oauth.user_code));

  //  Sets the device code in the configuration as well so that it is visible to the user.
  settings_t *settings = config_get_ptr();
  configuration_set_string(settings, settings->arrays.cheevos_webhook_usercode, webhook_usercode);

  free(request);
  request = NULL;
    
  free(webhook_devicecode);
  webhook_devicecode = NULL;
    
  free(webhook_usercode);
  webhook_usercode = NULL;
      
  // Trigger the access_token retrieval.
  woauth_trigger_accesstoken_retrieval();
}

//  ------------------------------------------------------------------------------
//  Configures the request to retrieve the device code and user code.
//  ------------------------------------------------------------------------------
static void woauth_initialize_devicecode_request
(
  oauth2_async_io_request* request
)
{
  rc_api_url_builder_t builder;
    
  const settings_t *settings = config_get_ptr();
    
  rc_url_builder_init(&builder, &(request->request.buffer), 48);
  rc_url_builder_append_str_param(&builder, "client_id", current_oauth.client_id);
  rc_url_builder_append_str_param(&builder, "scope", "scope-to-be-determined");
  
  request->handler = (oauth2_async_handler)woauth_handle_devicecode_response;
  request->request.url = settings->arrays.cheevos_webhook_code_url;
  request->request.post_data = rc_url_builder_finalize(&builder);

  rcheevos_log_post_url(request->request.url, request->request.post_data);
}

//  ------------------------------------------------------------------------------
//  Starts the OAuth Device Flow to first retrieve a device code and a user code.
//  ------------------------------------------------------------------------------
void woauth_schedule_devicecode_retrieval()
{
  oauth2_async_io_request *request = (oauth2_async_io_request*) calloc(1, sizeof(oauth2_async_io_request));

  if (!request)
  {
    CHEEVOS_LOG(RCHEEVOS_TAG "Failed to allocate an OAuth device code request\n");
    return;
  }
  
  //retro_task_t* oauth_task = task_init();
  //task_set_title(oauth_task, "Starting association...");
  //task_queue_push(oauth_task);
      
  CHEEVOS_LOG(RCHEEVOS_TAG "Starting OAuth Device Flow \n");

  woauth_initialize_devicecode_request(request);

  woauth_begin_oauth2_http_request(request);
}

//  ------------------------------------------------------------------------------
//  Starts the process to associate the emulator with the Webhook server.
//  ------------------------------------------------------------------------------
void woauth_initiate()
{
  woauth_clear_devicecode();

  strlcpy(current_oauth.client_id, DEFAULT_CLIENT_ID, sizeof(current_oauth.client_id));
  woauth_schedule_devicecode_retrieval();
}

//  ------------------------------------------------------------------------------
//  Returns an access token used to contact the Webhook server.
//  ------------------------------------------------------------------------------
const char* woauth_get_accesstoken()
{
  const settings_t *settings = config_get_ptr();
  const int EXPIRATION_WINDOW = 1000 * 10 * 5;

  strncpy(&current_oauth.client_id, DEFAULT_CLIENT_ID, strlen(DEFAULT_CLIENT_ID));
    
  retro_time_t now = cpu_features_get_time_usec();

  char * e;
  errno = 0;

  retro_time_t expecting_refresh = (retro_time_t)strtoll(settings->arrays.cheevos_webhook_expiresin, &e, 10);
    
  if (*e != 0 || errno != 0) {
    //  The emulator has not been associated.
    //  Nothing can be done without any user intervention.
    return NULL;
  }

  if (expecting_refresh == 0) {
    //  The emulator has not been associated.
    //  Nothing can be done without any user intervention.
    return NULL;
  }
  else if (expecting_refresh <= now) {
    if(!token_refresh_scheduled) {
      if (strlen(settings->arrays.cheevos_webhook_refreshtoken) > 0) {
        //  Let's get an access token asap using the refresh token.
        token_refresh_scheduled = true;
        woauth_trigger_accesstoken_retrieval();
        return NULL;
      }
    }
    else {
      //  The refresh token as well as the device code is missing.
      //  Nothing can be done without any user intervention.
      return NULL;
    }
  }
  else if (now + EXPIRATION_WINDOW >= expecting_refresh) {
    if (!token_refresh_scheduled) {
      //  The access token expires within the next X minutes;
      //  Let's refresh it now.
      token_refresh_scheduled = true;
      woauth_trigger_accesstoken_retrieval();
      return settings->arrays.cheevos_webhook_accesstoken;
    }
    else {
      //  The refresh token as well as the device code is missing.
      //  Nothing can be done without any user intervention.
      return NULL;
    }
  }
    
  //  The access token is not close to expire.
  return settings->arrays.cheevos_webhook_accesstoken;
}
