#ifndef WINRAW_MOUSE_PENDING_H
#define WINRAW_MOUSE_PENDING_H

#include <retro_atomic.h>

enum winraw_mouse_position_source
{
   WINRAW_MOUSE_DELTA,
   WINRAW_MOUSE_ABSOLUTE,
   WINRAW_MOUSE_CURSOR
};

static INLINE enum winraw_mouse_position_source winraw_mouse_take_position(
      retro_atomic_int_t *cursor, retro_atomic_int_t *absolute)
{
   if (retro_atomic_exchange_int(cursor, 0))
   {
      /* Discarding a superseded flag publishes no payload. */
      retro_atomic_store_relaxed_int(absolute, 0);
      return WINRAW_MOUSE_CURSOR;
   }
   if (retro_atomic_exchange_int(absolute, 0))
      return WINRAW_MOUSE_ABSOLUTE;
   return WINRAW_MOUSE_DELTA;
}

#endif
