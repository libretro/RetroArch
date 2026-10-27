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

#ifndef _OVERLAY_CONTACT_H
#define _OVERLAY_CONTACT_H

#include <stdint.h>
#include <math.h>

#include <boolean.h>
#include <retro_inline.h>

/* A finger's contact on an overlay's D-pad or face-button area: the
 * ellipse the touchscreen reports, not one point. Each tip of its two
 * axes presses the way it lies from the area's middle, so a thumb laid
 * across up and right presses both, and one between two face buttons
 * presses the two. After RetroArch-Lite's contact-area input. Nothing
 * of the system's, so samples/input/overlay_contact checks it alone. */

enum overlay_contact_dir
{
   OVERLAY_CONTACT_UP    = 1 << 0,
   OVERLAY_CONTACT_DOWN  = 1 << 1,
   OVERLAY_CONTACT_LEFT  = 1 << 2,
   OVERLAY_CONTACT_RIGHT = 1 << 3
};

/* How a D-pad or face-button area reads a touch */
enum overlay_contact_method
{
   OVERLAY_CONTACT_METHOD_VECTOR = 0, /* the way its centre lies */
   OVERLAY_CONTACT_METHOD_AREA,       /* the ways its contact covers */
   OVERLAY_CONTACT_METHOD_BOTH,
   OVERLAY_CONTACT_METHOD_LAST
};

/* Within this of the middle, in half-widths, a tip presses no way: a
 * contact's narrow side a pixel either side of the middle is no press
 * left and right */
#define OVERLAY_CONTACT_DEAD 0.05f

/* The one way (x, y) lies from the middle, x right and y down: along
 * whichever is the longer */
static INLINE unsigned overlay_contact_fourway(float x, float y)
{
   if (fabsf(x) < OVERLAY_CONTACT_DEAD && fabsf(y) < OVERLAY_CONTACT_DEAD)
      return 0;
   if (fabsf(x) >= fabsf(y))
      return x > 0.0f ? OVERLAY_CONTACT_RIGHT : OVERLAY_CONTACT_LEFT;
   return y > 0.0f ? OVERLAY_CONTACT_DOWN : OVERLAY_CONTACT_UP;
}

/* The ways a contact covers. (@x, @y): its centre from the area's
 * middle, in the area's half-widths and half-heights, x right and y
 * down. @major_px, @minor_px: the ellipse's axes in pixels; @angle its
 * orientation in radians, as Android gives it - 0 with the major axis
 * upright, positive turned clockwise. @scale_x, @scale_y: a pixel in
 * the area's half-widths and half-heights; @magnify how large a
 * contact counts, 1 as reported. With no contact size (a mouse, a
 * pen), the way the centre lies. */
static INLINE unsigned overlay_contact_dirs(float x, float y,
      unsigned major_px, unsigned minor_px, float angle,
      float scale_x, float scale_y, float magnify)
{
   unsigned dirs = 0;
   float s, c, mx, my, nx, ny, rmaj, rmin;

   if (!major_px)
      return overlay_contact_fourway(x, y);

   rmaj = 0.5f * magnify * (float)major_px;
   rmin = 0.5f * magnify * (float)(minor_px ? minor_px : major_px);
   s    = (float)sin(angle);
   c    = (float)cos(angle);
   /* the major axis upright and turned clockwise: (sin, -cos) on a
    * screen whose y runs down; the minor across it */
   mx   =  rmaj * s * scale_x;
   my   = -rmaj * c * scale_y;
   nx   =  rmin * c * scale_x;
   ny   =  rmin * s * scale_y;

   dirs |= overlay_contact_fourway(x + mx, y + my);
   dirs |= overlay_contact_fourway(x - mx, y - my);
   dirs |= overlay_contact_fourway(x + nx, y + ny);
   dirs |= overlay_contact_fourway(x - nx, y - ny);
   return dirs;
}

/* A contact as a frontend keeps it: the axes packed as major << 16 |
 * minor, in pixels; the angle in ten-thousandths of a radian */
#define OVERLAY_CONTACT_PACK(major, minor) \
   ((uint32_t)(((major) > 0xffff ? 0xffff : (major)) << 16) \
    | (uint32_t)((minor) > 0xffff ? 0xffff : (minor)))
#define OVERLAY_CONTACT_MAJOR(c) ((unsigned)((c) >> 16))
#define OVERLAY_CONTACT_MINOR(c) ((unsigned)((c) & 0xffff))
#define OVERLAY_CONTACT_ANGLE(q) ((float)(q) / 10000.0f)
#define OVERLAY_CONTACT_ANGLE_Q(rad) ((int16_t)((rad) * 10000.0f))

#endif
