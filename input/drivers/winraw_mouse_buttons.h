#ifndef WINRAW_MOUSE_BUTTONS_H
#define WINRAW_MOUSE_BUTTONS_H

#include <retro_atomic.h>

/* The low half is what is held now; the high half latches every press
 * until the poll takes it, so a click let go before the poll still
 * reads as down for one frame. */
#define WINRAW_MOUSE_BUTTONS_LATCH_SHIFT 16

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
      now = (old & ~(int)up) | (int)down
          | (int)(down << WINRAW_MOUSE_BUTTONS_LATCH_SHIFT);
   } while (!retro_atomic_cas_int(buttons, old, now));
}

/* Held now, or pressed since the last poll; the latch is taken. */
static INLINE unsigned winraw_mouse_buttons_snapshot(
      retro_atomic_int_t *buttons, bool focused)
{
   int old;
   if (!focused)
   {
      retro_atomic_exchange_int(buttons, 0);
      return 0;
   }
   do
   {
      old = retro_atomic_load_relaxed_int(buttons);
      if (!((unsigned)old >> WINRAW_MOUSE_BUTTONS_LATCH_SHIFT))
         return (unsigned)old;
   } while (!retro_atomic_cas_int(buttons, old,
            old & ((1 << WINRAW_MOUSE_BUTTONS_LATCH_SHIFT) - 1)));
   return ((unsigned)old | ((unsigned)old >> WINRAW_MOUSE_BUTTONS_LATCH_SHIFT))
      & ((1u << WINRAW_MOUSE_BUTTONS_LATCH_SHIFT) - 1);
}

#endif
