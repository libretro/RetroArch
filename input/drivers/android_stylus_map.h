/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ANDROID_STYLUS_MAP_H
#define _ANDROID_STYLUS_MAP_H

#include <boolean.h>
#include <retro_inline.h>

/* How a pen maps onto the libretro devices the Android input driver
 * already reports (input/drivers/android_input.c). Nothing here is
 * pen-specific to a core:
 *
 * - Position: pointer 0. While hovering it moves without being
 *   pressed (input_stylus_hover_moves_pointer).
 * - Press: pointer 0 pressed, pointer count 1. The tip pressing
 *   harder than input_stylus_pressure_sensitivity allows is a press;
 *   with input_stylus_require_contact_for_click off, so is the barrel
 *   button, touching or hovering.
 * - Barrel button: the right mouse button.
 *
 * Pointer indices keep their meaning: index N is the Nth touch. A pen
 * is one touch, so it never occupies more than index 0. Reporting the
 * tip and the barrel button as further "touches" would read as a
 * two- and three-finger touch to everything that counts pointers,
 * the driver's own lightgun mapping included (two pointers are
 * turbo, three are reload).
 *
 * The driver turns a motion event into one of the events below and
 * applies the result; the decision itself is here, kept free of NDK
 * types so samples/input/android_stylus_map can build it on a host. */

enum android_stylus_event
{
   /* In range and not touching: hover enter or hover move. */
   ANDROID_STYLUS_HOVER = 0,
   /* Left the range of the digitizer. */
   ANDROID_STYLUS_HOVER_EXIT,
   /* Down or move. The tip touches when the reported distance is 0;
    * a down or move that still reports distance is a hover. */
   ANDROID_STYLUS_CONTACT,
   /* Lifted. */
   ANDROID_STYLUS_UP,
   /* The gesture was cancelled. */
   ANDROID_STYLUS_CANCEL
};

/* What the pen is holding, between events. */
typedef struct
{
   bool contact_active; /* The tip has touched since the last lift */
   bool press_active;   /* The pen holds pointer 0 pressed */
} android_stylus_state_t;

/* The user's settings. */
typedef struct
{
   bool     require_contact;      /* input_stylus_require_contact_for_click */
   bool     hover_moves_pointer;  /* input_stylus_hover_moves_pointer */
   unsigned pressure_sensitivity; /* input_stylus_pressure_sensitivity */
} android_stylus_cfg_t;

/* pointer_count is shared with the touchscreen, so most events leave
 * it alone. */
#define ANDROID_STYLUS_COUNT_KEEP (-1)

/* What an event does to the driver's pointer and mouse state. */
typedef struct
{
   bool set_position;  /* Write the event's position to pointer 0 */
   bool mouse_r;       /* The right mouse button */
   int  pointer_count; /* 0, 1 or ANDROID_STYLUS_COUNT_KEEP */
} android_stylus_result_t;

/* Press or release pointer 0 for the pen. A pressing pen takes
 * pointer_count, as a touchscreen event does, and says so on every
 * event so a finger lifting in between cannot leave it released. A
 * pen that is not pressing leaves it alone, apart from letting go of
 * its own press: a hover event must not release a finger. */
static INLINE int android_stylus_set_pressed(
      android_stylus_state_t *st, bool pressed)
{
   int count = ANDROID_STYLUS_COUNT_KEEP;

   if (pressed)
      count = 1;
   else if (st->press_active)
      count = 0;

   st->press_active = pressed;
   return count;
}

/* Sensitivity 1..100 to a pressure threshold of 0.02475..0.0: higher
 * is more sensitive, and at 100 any pressure above 0 is a click. */
static INLINE float android_stylus_pressure_threshold(unsigned sensitivity)
{
   if (sensitivity > 100)
      sensitivity = 100;
   return (float)(100 - (int)sensitivity) * 0.00025f;
}

static INLINE void android_stylus_map(
      android_stylus_state_t *st, const android_stylus_cfg_t *cfg,
      enum android_stylus_event event, bool side_pressed,
      float pressure, float distance,
      android_stylus_result_t *res)
{
   bool pressed       = false;

   res->set_position  = false;
   /* The barrel button is the pen's secondary button: it is reported
    * the way a mouse's is. */
   res->mouse_r       = side_pressed;
   res->pointer_count = ANDROID_STYLUS_COUNT_KEEP;

   switch (event)
   {
      case ANDROID_STYLUS_HOVER_EXIT:
         /* Out of range: nothing the pen held stays held. */
         res->mouse_r       = false;
         res->pointer_count = android_stylus_set_pressed(st, false);
         break;

      case ANDROID_STYLUS_HOVER:
         /* A hovering pen presses only when the user allowed a click
          * without contact and holds the barrel button. */
         pressed            = !cfg->require_contact && side_pressed;
         res->set_position  = pressed || cfg->hover_moves_pointer;
         res->pointer_count = android_stylus_set_pressed(st, pressed);
         break;

      case ANDROID_STYLUS_CANCEL:
         /* A lifted pen keeps reporting its barrel button through the
          * hover events that follow; a cancelled gesture has none
          * coming. */
         res->mouse_r       = false;
         /* fall-through */
      case ANDROID_STYLUS_UP:
         st->contact_active = false;
         res->pointer_count = android_stylus_set_pressed(st, false);
         break;

      case ANDROID_STYLUS_CONTACT:
      {
         /* Contact is distance-based (instant, no pressure needed);
          * a click is pressure-based, against the user's threshold. */
         bool tip_touching = (distance <= 0.0f);
         bool tip_down     = tip_touching
            && (pressure > android_stylus_pressure_threshold(
                     cfg->pressure_sensitivity));

         pressed = tip_down || (!cfg->require_contact && side_pressed);

         if (tip_touching)
            st->contact_active = true;

         /* Before the tip has touched, a down or move that still
          * reports distance is a hover: the position stays where the
          * last contact left it, so the cursor does not jump. */
         res->set_position  = st->contact_active || pressed;
         res->pointer_count = android_stylus_set_pressed(st, pressed);
         break;
      }
   }
}

#endif
