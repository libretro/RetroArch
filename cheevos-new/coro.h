#ifndef __RARCH_CHEEVOS_CORO_H
#define __RARCH_CHEEVOS_CORO_H

/*
Released under the CC0: https://creativecommons.org/publicdomain/zero/1.0/
*/

/* Use at the beginning of the coroutine, you must have declared a variable rcheevos_coro_t* coro */
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
  } } \
  do { return 0; } while ( 0 )

/* Go to the x label */
#define CORO_GOTO( x ) \
  do { \
    coro->step = ( x ); \
    goto CORO_again; \
  } while ( 0 )

/* Go to a subroutine, execution continues until the subroutine returns via RET */
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

/* Sets up the coroutine */
#define CORO_SETUP() \
  do { \
    coro->step = CORO_BEGIN; \
    coro->sp   = 0; \
  } while ( 0 )

#define CORO_STOP() \
  do { \
    return 0; \
  } while ( 0 )

/* Add this macro to your rcheevos_coro_t structure containing the variables for the coroutine */
#define CORO_FIELDS \
  int step, sp; \
  int stack[ 8 ];

#endif /* __RARCH_CHEEVOS_CORO_H */
