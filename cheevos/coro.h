#ifndef CORO_H
#define CORO_H

/*
Released under the CC0: https://creativecommons.org/publicdomain/zero/1.0/
*/

/* Go to the x label */
#define CORO_GOTO( x ) \
  do { \
    coro->step = ( x ); \
    goto CORO_again; \
  } while ( 0 )

/* Go to a subroutine, execution continues until the subroutine returns via RET */
/* x is the subroutine label, y and z are the A and B arguments */
#define CORO_GOSUB( x ) \
  do { \
    coro->stack[ coro->sp++ ] = __LINE__; \
    coro->step = ( x ); \
    goto CORO_again; \
    case __LINE__: ; \
  } while ( 0 )

/* Returns from a subroutine */
#define CORO_RET() \
  do { \
    coro->step = coro->stack[ --coro->sp ]; \
    goto CORO_again; \
  } while ( 0 )

/* Yields to the caller, execution continues from this point when the coroutine is resumed */
#define CORO_YIELD() \
  do { \
    coro->step = __LINE__; \
    return 1; \
    case __LINE__: ; \
  } while ( 0 )

/* The coroutine entry point, never use 0 as a label */
#define CORO_BEGIN 0

/* Sets up a coroutine, x is a pointer to coro_t */
#define CORO_SETUP( x ) \
  do { \
    ( x )->step = CORO_BEGIN; \
    ( x )->sp   = 0; \
  } while ( 0 )

/* A coroutine */
typedef struct
{
   /* co-routine specific variables  */
   char badge_name[16];
   char url[256];
   char badge_basepath[PATH_MAX_LENGTH];
   char badge_fullpath[PATH_MAX_LENGTH];
   unsigned char hash[16];
   bool round;
   unsigned gameid;
   unsigned i;
   unsigned j;
   unsigned k;
   size_t bytes;
   size_t count;
   size_t offset;
   size_t len;
   size_t size;
   MD5_CTX md5;
   cheevos_nes_header_t header;
   retro_time_t t0;
   struct retro_system_info sysinfo;
   void *data;
   char *json;
   const char *path;
   const char *ext;
   intfstream_t *stream;
   cheevo_t *cheevo;
   settings_t *settings;
   struct http_connection_t *conn;
   struct http_t *http;
   const cheevo_t *cheevo_end;

   /* co-routine general variables  */
   int step, sp;
   int stack[ 8 ];
} coro_t;

#endif /* CORO_H */
