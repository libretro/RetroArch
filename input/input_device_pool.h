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

#ifndef _INPUT_DEVICE_POOL_H
#define _INPUT_DEVICE_POOL_H

#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>

/* The device numbers controllers are known by - the Device Index of a
 * port - shared between the joypad driver and the sources beside it
 * (a pad bound to WinUSB, which no joypad driver sees).
 *
 * The joypad driver's pads keep the numbers they have always had: a
 * pad in its slot n is device n, and nothing it reads goes through
 * this. A source beside it is given numbers the driver is not using,
 * from the top down so that the two meet as late as they can; a pad
 * that comes back is given the number it had if that is free. Where
 * the driver's pad arrives on a number a pad beside it holds, that pad
 * gives the number up and is told to move.
 *
 * One thread: connects and disconnects are applied on the main thread.
 * No allocation and no driver, settings or libretro types, so that
 * samples/input/device_pool can build it on its own. */

#define INPUT_POOL_DEVICES 16
/* sources beside the joypad driver */
#define INPUT_POOL_SOURCES 2
#define INPUT_POOL_SLOTS   16

typedef struct input_device_pool
{
   uint16_t driver;  /* numbers the joypad driver's pads are on */
   uint16_t beside;  /* numbers pads beside it are on */
   /* each source's slot: its number, and the last it had; -1 none */
   int8_t   index[INPUT_POOL_SOURCES][INPUT_POOL_SLOTS];
   int8_t   last[INPUT_POOL_SOURCES][INPUT_POOL_SLOTS];
   /* each number held beside the driver: whose it is */
   uint8_t  source[INPUT_POOL_DEVICES];
   uint8_t  slot[INPUT_POOL_DEVICES];
} input_device_pool_t;

static INLINE void input_device_pool_init(input_device_pool_t *pool)
{
   memset(pool, 0, sizeof(*pool));
   memset(pool->index, -1, sizeof(pool->index));
   memset(pool->last,  -1, sizeof(pool->last));
}

static INLINE bool input_device_pool_valid(unsigned source, unsigned slot)
{
   return source < INPUT_POOL_SOURCES && slot < INPUT_POOL_SLOTS;
}

/* A number for @source's pad in @slot: the one it had, if free, else
 * the highest free; -1 when every number is in use. */
static INLINE int input_device_pool_claim(input_device_pool_t *pool,
      unsigned source, unsigned slot)
{
   int i;
   uint16_t used;

   if (!input_device_pool_valid(source, slot))
      return -1;
   if (pool->index[source][slot] >= 0)
      return pool->index[source][slot];
   used = pool->driver | pool->beside;
   i    = pool->last[source][slot];
   if (i < 0 || (used & (1u << i)))
      for (i = INPUT_POOL_DEVICES - 1; i >= 0; i--)
         if (!(used & (1u << i)))
            break;
   if (i < 0)
      return -1;
   pool->beside              |= (uint16_t)(1u << i);
   pool->source[i]            = (uint8_t)source;
   pool->slot[i]              = (uint8_t)slot;
   pool->index[source][slot]  = (int8_t)i;
   pool->last[source][slot]   = (int8_t)i;
   return i;
}

static INLINE void input_device_pool_release(input_device_pool_t *pool,
      unsigned source, unsigned slot)
{
   int i;
   if (     !input_device_pool_valid(source, slot)
         || (i = pool->index[source][slot]) < 0)
      return;
   pool->beside              &= (uint16_t)~(1u << i);
   pool->index[source][slot]  = -1;
}

/* The joypad driver's pad arrived on @index. True when a pad beside
 * the driver was on that number: it has given it up, and is named in
 * @source and @slot to be given another. */
static INLINE bool input_device_pool_driver_connect(input_device_pool_t *pool,
      unsigned index, unsigned *source, unsigned *slot)
{
   bool moved = false;

   if (index >= INPUT_POOL_DEVICES)
      return false;
   if (pool->beside & (1u << index))
   {
      *source = pool->source[index];
      *slot   = pool->slot[index];
      pool->beside                  &= (uint16_t)~(1u << index);
      pool->index[*source][*slot]    = -1;
      moved                          = true;
   }
   pool->driver |= (uint16_t)(1u << index);
   return moved;
}

static INLINE void input_device_pool_driver_disconnect(
      input_device_pool_t *pool, unsigned index)
{
   if (index < INPUT_POOL_DEVICES)
      pool->driver &= (uint16_t)~(1u << index);
}

/* Whose @index is beside the driver: true, with @source and @slot. */
static INLINE bool input_device_pool_owner(const input_device_pool_t *pool,
      unsigned index, unsigned *source, unsigned *slot)
{
   if (index >= INPUT_POOL_DEVICES || !(pool->beside & (1u << index)))
      return false;
   *source = pool->source[index];
   *slot   = pool->slot[index];
   return true;
}

#endif
