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

#ifndef _WIN32_FOREGROUND_H
#define _WIN32_FOREGROUND_H

#include <stdint.h>
#include <windows.h>

#include <retro_inline.h>
#include <retro_atomic.h>

/* GetForegroundWindow() is a system call under the window manager's
 * global lock. The foreground can only reach or leave one of our
 * windows with that window being sent one of the messages below, so
 * the answer is kept and asked again only after one of them. Between
 * windows of other processes it may go stale, which tells the same:
 * not ours. Any thread. Zero-initialised state asks on first use. */
typedef struct
{
   retro_atomic_size_t hwnd;
   retro_atomic_int_t  moved;
   retro_atomic_int_t  asked; /* 'moved' + 1 as of 'hwnd' */
} win32_foreground_t;

/* From the window procedure of every window compared against it */
static INLINE void win32_foreground_message(win32_foreground_t *fg,
      UINT message)
{
   switch (message)
   {
      case WM_NCCREATE:
      case WM_NCDESTROY:
      case WM_ACTIVATEAPP:
      case WM_ACTIVATE:
      case WM_SETFOCUS:
      case WM_KILLFOCUS:
         retro_atomic_fetch_add_int(&fg->moved, 1);
         break;
      default:
         break;
   }
}

static INLINE HWND win32_foreground_get(win32_foreground_t *fg)
{
   HWND hwnd;
   int  moved = retro_atomic_load_acquire_int(&fg->moved);
   if (retro_atomic_load_acquire_int(&fg->asked) == moved + 1)
      return (HWND)(uintptr_t)retro_atomic_load_acquire_size(&fg->hwnd);
   hwnd = GetForegroundWindow();
   retro_atomic_store_release_size(&fg->hwnd, (size_t)(uintptr_t)hwnd);
   retro_atomic_store_release_int(&fg->asked, moved + 1);
   return hwnd;
}

#endif
