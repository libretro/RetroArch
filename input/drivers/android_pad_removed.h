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

#ifndef _ANDROID_PAD_REMOVED_H
#define _ANDROID_PAD_REMOVED_H

#include <limits.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_atomic.h>
#include <string/stdstring.h>

/* Input devices the OS has removed (input/drivers/android_input.c).
 *
 * Android reports a controller going away (switched off, out of range,
 * unplugged) to the activity's InputManager listener, on the UI thread.
 * No input event marks a removal, so that callback is the only way the
 * native side learns of it. Unheard, a pad stayed "connected" for as
 * long as the app ran: its port kept its device name (so an overlay
 * hidden while a controller is connected never came back), the buttons
 * it was holding stayed held, and pausing on disconnect could not
 * happen.
 *
 * Two parts, both here so samples/input/android_pad_removed can build
 * them on a host - nothing in this header takes an NDK type.
 *
 * The mailbox. The listener posts the removed device's id; the input
 * thread takes it out at its next poll. Each slot holds an id plus
 * one, zero meaning empty, and is claimed with a compare-and-swap, so
 * it does not matter which thread, or how many, the listener runs on.
 * A flag saves the poll from looking at the slots when nothing is
 * waiting: a producer fills a slot and then raises the flag, the poll
 * lowers the flag and then empties the slots, so an id that lands
 * behind the poll's back leaves the flag raised for the next one. A
 * full box refuses the id, which leaves that pad where every removed
 * pad was before this existed.
 *
 * The pad table. A removed pad's slot keeps its name and is marked
 * removed instead of being emptied, so the device finds its old port
 * again when it returns, under whatever id Android gives it. A removed
 * slot is matched before a live one of the same name: of two identical
 * controllers, the one that went away must not be taken for the one
 * still connected. */

#define ANDROID_REMOVED_SLOTS 8

/* What a pad slot's id holds once its device has been removed. No
 * device has this id (-1, the obvious choice, is the virtual
 * keyboard's). */
#define ANDROID_PAD_ID_REMOVED INT_MIN

typedef struct
{
   retro_atomic_int_t ids[ANDROID_REMOVED_SLOTS];
   retro_atomic_int_t pending;
} android_removed_box_t;

/* Any thread. Returns false if the id is not a physical device's or
 * the box is full. */
static INLINE bool android_removed_post(android_removed_box_t *box,
      int device_id)
{
   unsigned i;

   /* Ids below zero are not physical devices. */
   if (device_id < 0 || device_id == INT_MAX)
      return false;

   for (i = 0; i < ANDROID_REMOVED_SLOTS; i++)
   {
      if (retro_atomic_cas_int(&box->ids[i], 0, device_id + 1))
      {
         retro_atomic_store_release_int(&box->pending, 1);
         return true;
      }
   }

   return false;
}

/* The input thread. Writes the ids taken to @ids, which has room for
 * ANDROID_REMOVED_SLOTS of them, and returns how many. */
static INLINE unsigned android_removed_take(android_removed_box_t *box,
      int *ids)
{
   unsigned i;
   unsigned count = 0;

   if (!retro_atomic_load_acquire_int(&box->pending))
      return 0;

   retro_atomic_store_release_int(&box->pending, 0);

   for (i = 0; i < ANDROID_REMOVED_SLOTS; i++)
   {
      int v = retro_atomic_exchange_int(&box->ids[i], 0);
      if (v)
         ids[count++] = v - 1;
   }

   return count;
}

typedef struct state_device
{
   int id;
   int port;
   char name[256];
} state_device_t;

/* Mark the slot of device @id as removed. A slot's index is its port;
 * returns it, or -1 if no pad has that id. */
static INLINE int android_pad_mark_removed(state_device_t *pads,
      unsigned count, int id)
{
   unsigned i;

   for (i = 0; i < count; i++)
   {
      if (pads[i].id == id)
      {
         pads[i].id = ANDROID_PAD_ID_REMOVED;
         return (int)i;
      }
   }

   return -1;
}

/* The slot a returning device named @name left, or -1 if it left
 * none. */
static INLINE int android_pad_find_removed(const state_device_t *pads,
      unsigned count, const char *name)
{
   unsigned i;

   for (i = 0; i < count; i++)
   {
      if (     pads[i].id == ANDROID_PAD_ID_REMOVED
            && string_is_equal(name, pads[i].name))
         return (int)i;
   }

   return -1;
}

#endif
