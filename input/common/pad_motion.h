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

#ifndef _PAD_MOTION_H
#define _PAD_MOTION_H

#include <stdint.h>

#include <boolean.h>
#include <retro_inline.h>
#include <libretro.h>

/* What the pad handlers share for a pad's motion sensors.
 *
 * A handler turns its pad's own axes into SDL's - the PlayStation
 * pads', SDL's de facto standard - in g and radians a second, and
 * these give a core the value it asks for on libretro's, as the sdl3
 * joypad driver does: so a core sees a pad the same way whichever
 * driver reads it. */

/* bit 0 the accelerometer, 1 the gyroscope */
#define PAD_MOTION_ACCEL 1
#define PAD_MOTION_GYRO  2

/* A core's ask, into @want; false for an action that is neither. */
static INLINE bool pad_motion_want(uint8_t *want,
      enum retro_sensor_action action)
{
   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         *want |= PAD_MOTION_ACCEL;
         return true;
      case RETRO_SENSOR_GYROSCOPE_ENABLE:
         *want |= PAD_MOTION_GYRO;
         return true;
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         *want &= (uint8_t)~PAD_MOTION_ACCEL;
         return true;
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
         *want &= (uint8_t)~PAD_MOTION_GYRO;
         return true;
      default:
         break;
   }
   return false;
}

/* Sensor @id from @accel and @gyro on SDL's axes - @gyro NULL for a pad
 * with none - where @want has it; false otherwise. */
static INLINE bool pad_motion_value(const float *accel, const float *gyro,
      uint8_t want, unsigned id, float *value)
{
   switch (id)
   {
      case RETRO_SENSOR_ACCELEROMETER_X:
      case RETRO_SENSOR_ACCELEROMETER_Y:
      case RETRO_SENSOR_ACCELEROMETER_Z:
         if (!(want & PAD_MOTION_ACCEL))
            return false;
         *value = id == RETRO_SENSOR_ACCELEROMETER_X ? accel[0]
            : id == RETRO_SENSOR_ACCELEROMETER_Y ? accel[2] : accel[1];
         return true;
      case RETRO_SENSOR_GYROSCOPE_X:
      case RETRO_SENSOR_GYROSCOPE_Y:
      case RETRO_SENSOR_GYROSCOPE_Z:
         if (!gyro || !(want & PAD_MOTION_GYRO))
            return false;
         *value = id == RETRO_SENSOR_GYROSCOPE_X ? gyro[0]
            : id == RETRO_SENSOR_GYROSCOPE_Y ? -gyro[2] : gyro[1];
         return true;
      default:
         break;
   }
   return false;
}

#endif
