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

#ifndef _ANDROID_POLL_WAIT_H
#define _ANDROID_POLL_WAIT_H

#include <boolean.h>
#include <retro_inline.h>

/* How long the input poll (input/drivers/android_input.c) waits on the
 * looper, in milliseconds as ALooper_pollOnce takes them: @first for
 * the first event, @then after each one that came.
 *
 * In the foreground both are the input block timeout: a pad that sends
 * one press as several events has the rest land in the same poll, as
 * long as each follows the last within it. 0, the default, waits for
 * nothing. Backgrounded (@idle), the first wait is for whatever wakes
 * the app, however long, and the rest of a burst is taken without
 * waiting. */
static INLINE void android_poll_waits(int block_ms, bool idle,
      int *first, int *then)
{
   if (idle)
   {
      *first = -1;
      *then  = 0;
   }
   else
   {
      *first = block_ms;
      *then  = block_ms;
   }
}

#endif
