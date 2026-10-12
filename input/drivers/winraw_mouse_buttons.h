#ifndef WINRAW_MOUSE_BUTTONS_H
#define WINRAW_MOUSE_BUTTONS_H

#include <retro_atomic.h>

/* A report and a focus clear must not overwrite each other's update. */
static INLINE void winraw_mouse_buttons_update(retro_atomic_int_t *buttons,
      unsigned down, unsigned up)
{
   int old, now;
   if (!(down | up))
      return;
   do
   {
      old = retro_atomic_load_relaxed_int(buttons);
      now = (old & ~(int)up) | (int)down;
   } while (!retro_atomic_cas_int(buttons, old, now));
}

static INLINE unsigned winraw_mouse_buttons_snapshot(
      retro_atomic_int_t *buttons, bool focused)
{
   if (!focused)
   {
      retro_atomic_exchange_int(buttons, 0);
      return 0;
   }
   return (unsigned)retro_atomic_load_relaxed_int(buttons);
}

#endif
