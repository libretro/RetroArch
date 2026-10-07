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

#ifndef _SONY_PAD_MOTION_H
#define _SONY_PAD_MOTION_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_inline.h>
#include <libretro.h>

#include "sony_pad_output.h"

/* The motion sensors of a DualShock 4 or a DualSense, read out of the
 * pad's own input report.
 *
 * A joypad driver that reads these pads as plain HID - Windows raw
 * input does - gets their sticks and buttons from the report's HID
 * description. The gyroscope and the accelerometer are in the same
 * report, in a part the description calls vendor data and no HID
 * parser gives a meaning to. This reads them from where each pad
 * puts them, and turns them into what a libretro core is given: the
 * accelerometer in g, the gyroscope in radians a second, on the axes
 * the SDL joypad driver gives the same pads' sensors on, so that a
 * core sees the same pad the same way under either driver.
 *
 * Over USB only. Over Bluetooth both pads send a short report with no
 * sensors in it until they are switched to their full one - which has
 * no HID description at all, and would take the sticks and buttons
 * away from a driver that reads by the description.
 *
 * The scale is the pads' nominal one. Each pad also carries its own
 * calibration, which is not read here: the values are right to a few
 * percent, and a gyroscope at rest may read a little off zero.
 *
 * No allocation and nothing of any system's:
 * samples/input/sony_pad_motion checks it on its own. */

/* the report's six values, as the pad sends them */
typedef struct
{
   int16_t gyro[3];   /* the pad's own X, Y, Z */
   int16_t accel[3];
} sony_pad_motion_t;

/* accelerometer counts in one g, and gyroscope counts in one degree a
 * second */
#define SONY_PAD_ACCEL_PER_G   8192.0f
#define SONY_PAD_GYRO_PER_DPS  16.0f

static INLINE int16_t sony_pad_le16(const uint8_t *p)
{
   return (int16_t)(uint16_t)(p[0] | (p[1] << 8));
}

/* Reads the sensors out of a USB input report (@report, with its
 * report id first, @size bytes). False if this is not a report that
 * has them: another report id, or the short Bluetooth one. */
static INLINE bool sony_pad_motion_parse(sony_pad_motion_t *out,
      enum sony_pad_model model, const uint8_t *report, size_t size)
{
   size_t at;
   unsigned i;

   if (!report || size < 1 || report[0] != 0x01)
      return false;
   switch (model)
   {
      case SONY_PAD_DUALSENSE:
         /* sticks, triggers, a counter, four bytes of buttons and four
          * of a sequence number come first */
         at = 16;
         break;
      case SONY_PAD_DS4:
         /* sticks, three bytes of buttons, triggers, a timestamp and
          * a temperature come first */
         at = 13;
         break;
      default:
         return false;
   }
   if (size < at + 12)
      return false;
   for (i = 0; i < 3; i++)
   {
      out->gyro[i]  = sony_pad_le16(report + at + 2 * i);
      out->accel[i] = sony_pad_le16(report + at + 6 + 2 * i);
   }
   return true;
}

/* One libretro sensor value (@id, RETRO_SENSOR_ACCELEROMETER_X to
 * RETRO_SENSOR_GYROSCOPE_Z) from what was read. The pad's axes are X
 * to the right, Y up out of its face and Z towards the player;
 * libretro's Y and Z are the other way round, and its gyroscope's Y
 * turns the other way. False for an id that is neither sensor's. */
static INLINE bool sony_pad_motion_value(const sony_pad_motion_t *m,
      unsigned id, float *value)
{
   const float to_rad = 3.14159265358979323846f / 180.0f;
   switch (id)
   {
      case RETRO_SENSOR_ACCELEROMETER_X:
         *value = (float)m->accel[0] / SONY_PAD_ACCEL_PER_G;
         return true;
      case RETRO_SENSOR_ACCELEROMETER_Y:
         *value = (float)m->accel[2] / SONY_PAD_ACCEL_PER_G;
         return true;
      case RETRO_SENSOR_ACCELEROMETER_Z:
         *value = (float)m->accel[1] / SONY_PAD_ACCEL_PER_G;
         return true;
      case RETRO_SENSOR_GYROSCOPE_X:
         *value = (float)m->gyro[0] / SONY_PAD_GYRO_PER_DPS * to_rad;
         return true;
      case RETRO_SENSOR_GYROSCOPE_Y:
         *value = -(float)m->gyro[2] / SONY_PAD_GYRO_PER_DPS * to_rad;
         return true;
      case RETRO_SENSOR_GYROSCOPE_Z:
         *value = (float)m->gyro[1] / SONY_PAD_GYRO_PER_DPS * to_rad;
         return true;
      default:
         break;
   }
   return false;
}

#endif
