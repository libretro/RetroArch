#ifndef CORO_H
#define CORO_H

/*
Released under the CC0: https://creativecommons.org/publicdomain/zero/1.0/
*/

/* Use at the beginning of the coroutine, you must have declared a variable coro_t* coro */
#define CORO_ENTER() \
  { \
  CORO_again: ; \
  switch ( coro->step ) { \
  case CORO_BEGIN: ;

/* Use to define labels which are targets to GOTO and GOSUB */
#define CORO_SUB( x ) \
  case x: ;

/* Use at the end of the coroutine */
#define CORO_LEAVE() \
  } \
  } \
  do { return 0; } while ( 0 )

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

#define CORO_STOP() \
  do { \
    return 0; \
  } while ( 0 )

/* The coroutine entry point, never use 0 as a label */
#define CORO_BEGIN 0

/* Sets up a coroutine, x is a pointer to coro_t */
#define CORO_SETUP( x ) \
  do { \
    ( x )->step = CORO_BEGIN; \
    ( x )->sp = 0; \
  } while ( 0 )

#define CORO_VAR( x ) ( coro->x )

/* A coroutine */
typedef struct
{
  CORO_VARS
  int step, sp;
  int stack[ 8 ];
}
coro_t;

#endif /* CORO_H */
