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

/* The sensor snapshot the shader backends read each frame, from the
 * video thread: gyro, accelerometer and rest, nine floats to a slot.
 *
 * Three slots - the poll writes one, the reader reads one, the newest
 * waits in the middle - handed over by an exchange of @middle, so
 * neither side ever waits and a read never mixes two publications. One
 * writer and one reader. A header of its own so that a test can drive
 * the very same code.
 *
 * The indices are kept so that the zeroed state is a valid one: the
 * middle slot is the stored value ^ 1, the reader's ^ 2. */

#ifndef __INPUT_SENSOR_SNAP_H
#define __INPUT_SENSOR_SNAP_H

#include <string.h>
#include <stdint.h>
#include <retro_inline.h>
#include <retro_atomic.h>

#define INPUT_SENSOR_SNAP_FRESH 4

typedef struct input_sensor_snap
{
   float slot[3][9];
   retro_atomic_int_t middle;
   uint8_t write;        /* the writer's */
   uint8_t read;         /* the reader's */
   unsigned published;   /* the writer's: how many */
} input_sensor_snap_t;

static INLINE void input_sensor_snap_publish(input_sensor_snap_t *s,
      const float *gyro3, const float *accel3, const float *rest3)
{
   unsigned w = s->write;
   float *dst = s->slot[w];
   memcpy(dst,     gyro3,  3 * sizeof(float));
   memcpy(dst + 3, accel3, 3 * sizeof(float));
   memcpy(dst + 6, rest3,  3 * sizeof(float));
   s->write   = (uint8_t)(((unsigned)retro_atomic_exchange_int(&s->middle,
               (int)((w ^ 1u) | INPUT_SENSOR_SNAP_FRESH)) & 3u) ^ 1u);
   s->published++;
}

/* The newest published, taken in exchange for the slot last read and
 * read where it lies. */
static INLINE void input_sensor_snap_take(input_sensor_snap_t *s,
      float *gyro3, float *accel3, float *rest3)
{
   unsigned r = s->read ^ 2u;
   const float *v;
   if (retro_atomic_load_acquire_int(&s->middle) & INPUT_SENSOR_SNAP_FRESH)
   {
      r       = ((unsigned)retro_atomic_exchange_int(&s->middle,
               (int)(r ^ 1u)) & 3u) ^ 1u;
      s->read = (uint8_t)(r ^ 2u);
   }
   v = s->slot[r];
   memcpy(gyro3,  v,     3 * sizeof(float));
   memcpy(accel3, v + 3, 3 * sizeof(float));
   memcpy(rest3,  v + 6, 3 * sizeof(float));
}

#endif
