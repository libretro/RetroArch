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
 * In the foreground, nothing: the poll takes what is queued and the
 * frame goes on. (The input block timeout used to be waited here, and
 * stalled every frame by it; it keeps presses back instead now,
 * android_press_group.h.) Backgrounded (@idle), the first wait is for
 * whatever wakes the app, however long, and the rest of a burst is
 * taken without waiting. */
static INLINE void android_poll_waits(bool idle, int *first, int *then)
{
   *first = idle ? -1 : 0;
   *then  = 0;
}

#endif
