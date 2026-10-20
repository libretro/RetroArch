/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - Daniel De Matteis
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

#ifndef _WINUSB_BIND_H
#define _WINUSB_BIND_H

#include <boolean.h>
#include <retro_common_api.h>

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) \
   && defined(HAVE_WINUSB) && defined(HAVE_THREADS)
#define HAVE_WINUSB_BIND 1
#endif

RETRO_BEGIN_DECLS

#ifdef HAVE_WINUSB_BIND
/* Command-line arguments the elevated copy of RetroArch is started
 * with; an installer may start it with one too. */
#define WINUSB_BIND_ARG    "--winusb-bind-ds3"
#define WINUSB_RESTORE_ARG "--winusb-restore-ds3"

/* A DualShock 3, by its USB ids. */
bool winusb_bind_is_ds3(unsigned vid, unsigned pid);

/* @arg one of the two above: the DualShock 3s plugged in moved to
 * WinUSB, or given back to Windows' own driver, with *ret the exit
 * code (how many were, or 255 when it could not be done). False for
 * any other argument. Needs administrator rights. */
bool winusb_bind_command(const char *arg, int *ret);

/* The same asked for from the menu: a copy of RetroArch is started
 * with administrator rights to do it, and the outcome is shown as a
 * message. Nothing waits on the main thread. False while one asked for
 * before is still going. */
bool winusb_bind_request(bool bind);
#endif

RETRO_END_DECLS

#endif
