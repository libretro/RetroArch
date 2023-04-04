#ifndef RC_API_REQUEST_H
#define RC_API_REQUEST_H

#include "rc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A block of memory for variable length data (like strings and arrays).
 */
typedef struct rc_api_buffer_chunk_t {
  /* The current location where data is being written */
  char* write;
  /* The first byte past the end of data where writing cannot occur */
  char* end;
  /* The first byte of the data */
  char* start;
  /* The next block in the allocated memory chain */
  struct rc_api_buffer_chunk_t* next;
}
rc_api_buffer_chunk_t;

/**
 * A preallocated block of memory for variable length data (like strings and arrays).
 */
typedef struct rc_api_buffer_t {
  /* The chunk data (will point at the local data member) */
  struct rc_api_buffer_chunk_t chunk;
  /* Small chunk of memory pre-allocated for the chunk */
  char data[256];
}
rc_api_buffer_t;

/**
 * A constructed request to send to the retroachievements server.
 */
typedef struct rc_api_request_t {
  /* The URL to send the request to (contains protocol, host, path, and query args) */
  const char* url;
  /* Additional query args that should be sent via a POST command. If null, GET may be used */
  const char* post_data;

  /* Storage for the url and post_data */
  rc_api_buffer_t buffer;
}
rc_api_request_t;

/**
 * Common attributes for all server responses.
 */
typedef struct rc_api_response_t {
  /* Server-provided success indicator (non-zero on success, zero on failure) */
  int succeeded;
  /* Server-provided message associated to the failure */
  const char* error_message;

  /* Storage for the response data */
  rc_api_buffer_t buffer;
}
rc_api_response_t;

void rc_api_destroy_request(rc_api_request_t* request);

void rc_api_set_host(const char* hostname);
void rc_api_set_image_host(const char* hostname);

#ifdef __cplusplus
}
#endif

#endif /* RC_API_REQUEST_H */
