#ifndef RC_UTIL_H
#define RC_UTIL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A block of memory for variable length data (like strings and arrays).
 */
typedef struct rc_buffer_chunk_t {
  /* The current location where data is being written */
  char* write;
  /* The first byte past the end of data where writing cannot occur */
  char* end;
  /* The first byte of the data */
  char* start;
  /* The next block in the allocated memory chain */
  struct rc_buffer_chunk_t* next;
}
rc_buffer_chunk_t;

/**
 * A preallocated block of memory for variable length data (like strings and arrays).
 */
typedef struct rc_buffer_t {
  /* The chunk data (will point at the local data member) */
  struct rc_buffer_chunk_t chunk;
  /* Small chunk of memory pre-allocated for the chunk */
  char data[256];
}
rc_buffer_t;

void rc_buffer_init(rc_buffer_t* buffer);
void rc_buffer_destroy(rc_buffer_t* buffer);
char* rc_buffer_reserve(rc_buffer_t* buffer, size_t amount);
void rc_buffer_consume(rc_buffer_t* buffer, const char* start, char* end);
void* rc_buffer_alloc(rc_buffer_t* buffer, size_t amount);
char* rc_buffer_strcpy(rc_buffer_t* buffer, const char* src);
char* rc_buffer_strncpy(rc_buffer_t* buffer, const char* src, size_t len);

unsigned rc_djb2(const char* input);

void rc_format_md5(char checksum[33], const unsigned char digest[16]);

#ifdef __cplusplus
}
#endif

#endif /* RC_UTIL_H */
