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

#ifndef _INPUT_DEVICE_PINS_H
#define _INPUT_DEVICE_PINS_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>

/* A port's keyboard - and its mouse - kept by what the device is and
 * not by where it stands in a list.
 *
 * A port is given a keyboard by its number in the list of keyboards
 * (Keyboard Index). The number is a place in a list that changes:
 * with two keyboards, each given to a port, unplugging the first
 * makes the second "keyboard 1" - and the port that had "2" is left
 * naming a keyboard that is not there, while the other port now reads
 * the wrong one.
 *
 * So what is kept for a port is the keyboard's identity - a "pin" -
 * and the number follows from it each time the list is made:
 *
 * - an identity is what the input driver knows a keyboard by that
 *   stays the same from one day to the next: its USB vendor and
 *   product id, or its name where it has none. Two keyboards of one
 *   model have the same; the second listed is told apart as
 *   "<identity>#2", the third "#3", which holds as long as they are
 *   listed in the same order;
 *
 * - a port whose pinned keyboard is listed reads that keyboard,
 *   whatever its number is now;
 *
 * - a port whose pinned keyboard is not there reads no keyboard while
 *   some other port has its own - its keys must not be taken from
 *   that one - and every keyboard while none has: a single player
 *   who pinned a keyboard that is now in a drawer still has keys.
 *
 * - a port with a number and no pin - a setting from before pins -
 *   reads that number's keyboard, and is pinned to it by the caller.
 *
 * A mouse is kept the same way (input_pins_resolve_mice()), with two
 * differences that come from what Mouse Index is. Every port has a
 * number from the start - its own, so that the first port reads the
 * first mouse - and there is no "every mouse as one". So a port is
 * pinned only when its mouse is chosen by hand, never from a number
 * it merely has; and a port whose pinned mouse is not there falls
 * back to the mouse its number names, unless another port has its
 * own mouse, when it reads none.
 *
 * No allocation and nothing of any system's:
 * samples/input/device_pins runs it on its own. */

#define INPUT_PIN_LEN 64

/* what a port reads */
#define INPUT_PIN_ALL    0   /* every keyboard, as one */
#define INPUT_PIN_NONE (-1)  /* none: its own is away, and others have theirs */
/* 1 and up: that keyboard in the list */

/* Gives each of @n listed devices its identity from what the driver
 * knows it by (@base): the first with a given one keeps it, the
 * second is "<base>#2", and so on. */
static INLINE void input_pins_identities(char (*out)[INPUT_PIN_LEN],
      const char (*base)[INPUT_PIN_LEN], unsigned n)
{
   unsigned i, j;
   for (i = 0; i < n; i++)
   {
      unsigned nth = 1;
      for (j = 0; j < i; j++)
         if (!strcmp(base[j], base[i]))
            nth++;
      if (nth > 1)
      {
         /* room is kept for the number, so that the second of a pair
          * with a long name is not the first */
         size_t len = strlen(base[i]);
         if (len > INPUT_PIN_LEN - 12)
            len = INPUT_PIN_LEN - 12;
         memcpy(out[i], base[i], len);
         snprintf(out[i] + len, INPUT_PIN_LEN - len, "#%u", nth);
      }
      else
      {
         strncpy(out[i], base[i], INPUT_PIN_LEN - 1);
         out[i][INPUT_PIN_LEN - 1] = '\0';
      }
   }
}

/* Says what each of @ports ports reads (@choice), from its pin and
 * its number (@index, 0 for none) and the identities of the @listed
 * devices. */
static INLINE void input_pins_resolve(int8_t *choice,
      const char (*pin)[INPUT_PIN_LEN], const unsigned *index,
      unsigned ports,
      const char (*ident)[INPUT_PIN_LEN], unsigned listed)
{
   unsigned p, k;
   bool one_has_its_own = false;

   for (p = 0; p < ports; p++)
   {
      choice[p] = INPUT_PIN_ALL;
      if (pin[p][0])
      {
         /* -2: pinned, and not there; settled below */
         choice[p] = -2;
         for (k = 0; k < listed && k < 127; k++)
            if (ident[k][0] && !strcmp(ident[k], pin[p]))
            {
               choice[p] = (int8_t)(k + 1);
               break;
            }
      }
      else if (index[p] >= 1 && index[p] <= listed && index[p] <= 127)
         choice[p] = (int8_t)index[p];
      if (choice[p] >= 1)
         one_has_its_own = true;
   }
   for (p = 0; p < ports; p++)
      if (choice[p] == -2)
         choice[p] = one_has_its_own ? INPUT_PIN_NONE : INPUT_PIN_ALL;
}

/* what a port reads, of the mice */
#define INPUT_PIN_NO_MOUSE (-1)
/* 0 and up: that mouse in the list */

/* Says which mouse each of @ports ports reads (@choice): the one its
 * pin names; with that one away, none while another port has its own
 * and the one its number (@index) names otherwise; with no pin, the
 * one its number names. */
static INLINE void input_pins_resolve_mice(int16_t *choice,
      const char (*pin)[INPUT_PIN_LEN], const unsigned *index,
      unsigned ports,
      const char (*ident)[INPUT_PIN_LEN], unsigned listed)
{
   unsigned p, k;
   bool one_has_its_own = false;

   for (p = 0; p < ports; p++)
   {
      choice[p] = (int16_t)(index[p] < 0x7FFF ? index[p] : 0x7FFF);
      if (!pin[p][0])
         continue;
      /* -2: pinned, and not there; settled below */
      choice[p] = -2;
      for (k = 0; k < listed && k < 0x7FFF; k++)
         if (ident[k][0] && !strcmp(ident[k], pin[p]))
         {
            choice[p]       = (int16_t)k;
            one_has_its_own = true;
            break;
         }
   }
   for (p = 0; p < ports; p++)
      if (choice[p] == -2)
         choice[p] = one_has_its_own
            ? INPUT_PIN_NO_MOUSE
            : (int16_t)(index[p] < 0x7FFF ? index[p] : 0x7FFF);
}

#endif
