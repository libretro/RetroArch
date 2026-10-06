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

#ifndef __WIN32_IME_H
#define __WIN32_IME_H

#include <windows.h>
#include <boolean.h>
#include <retro_inline.h>
#include <retro_atomic.h>

/* Text entry and the IME.
 *
 * A window has an input context by default, and while it has one every
 * key that comes off its queue is first offered to the IME: on
 * Windows 8 and later that is the text services framework, for every
 * keyboard layout and not only the East Asian ones. RetroArch has a
 * use for what the IME produces in one place, a line of text being
 * typed in the menu. Everywhere else the offer is wasted work on the
 * thread that pumps the window, on each key down and up and each
 * repeat, and an IME left switched on puts its composition in front
 * of a game.
 *
 * So the window is without its input context except while a line of
 * text is open. The frontend says when one opens and closes
 * (win32_text_entry()); what it last said is kept here, and applied to
 * the window on the thread that owns it - when the window is made, and
 * on a posted message after that. Nothing waits for the message.
 *
 * Text from a layout that needs no IME is not touched by this: it
 * comes from TranslateMessage(). The other windows of the process -
 * the desktop menu, file dialogs - have input contexts of their own.
 *
 * ImmAssociateContextEx() is looked up when first wanted, as Windows
 * NT 4 and 95 do not have it; without it nothing changes. */
/* win32_common.c is this header's one user; it is a header so that a
 * test can drive the same code (samples/input/win32_ime). */
#define WIN32_WM_TEXT_ENTRY   (WM_APP + 0x7e)
#define WIN32_IACE_DEFAULT    0x0010

static retro_atomic_int_t win32_text_entry_on;

/* On the thread that owns @hwnd. */
static void win32_ime_apply(HWND hwnd, bool attach)
{
   typedef BOOL (WINAPI *imm_associate_ex_t)(HWND, HANDLE, DWORD);
   static imm_associate_ex_t associate_ex;
   static bool looked_up;

   if (!looked_up)
   {
      HMODULE imm = GetModuleHandleA("imm32.dll");
      if (!imm)
         imm       = LoadLibraryA("imm32.dll");
      if (imm)
         associate_ex = (imm_associate_ex_t)
            GetProcAddress(imm, "ImmAssociateContextEx");
      looked_up    = true;
   }

   if (associate_ex)
      associate_ex(hwnd, NULL, attach ? WIN32_IACE_DEFAULT : 0);
}

/* From any thread: a line of text has been opened, or closed. True if
 * that is a change, and the window is to be told (win32_ime_post()). */
static INLINE bool win32_ime_set(bool active)
{
   int now = active ? 1 : 0;
   if (retro_atomic_load_relaxed_int(&win32_text_entry_on) == now)
      return false;
   retro_atomic_store_release_int(&win32_text_entry_on, now);
   return true;
}

/* Tells the window, which does the work on its own thread. */
static INLINE void win32_ime_post(HWND hwnd)
{
   if (hwnd)
      PostMessage(hwnd, WIN32_WM_TEXT_ENTRY, 0, 0);
}

/* The window procedure's, for WIN32_WM_TEXT_ENTRY and WM_CREATE: applies
 * what the frontend says now, which is what the last posted message
 * stands for; a new window keeps its input context only if a line of
 * text is open already. */
static INLINE void win32_ime_sync(HWND hwnd)
{
   win32_ime_apply(hwnd,
         retro_atomic_load_acquire_int(&win32_text_entry_on) != 0);
}

#endif
