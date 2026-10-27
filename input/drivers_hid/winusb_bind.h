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

#include <string.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>
#include <compat/strcasestr.h>

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) \
   && defined(HAVE_WINUSB) && defined(HAVE_THREADS)
#define HAVE_WINUSB_BIND 1
#endif

RETRO_BEGIN_DECLS

/* An original Xbox pad, whoever made it, from the device's compatible
 * ids (a double-NUL-terminated list) and its USB ids: its interface is
 * XID, class 0x58 subclass 0x42. The DVD remote's receiver and Steel
 * Battalion's controller are XID too, with other reports. */
static INLINE bool winusb_is_xid(const char *ids, unsigned vid, unsigned pid)
{
   if (     (vid == 0x045e && pid == 0x0284)
         || (vid == 0x0a7b && pid == 0xd000))
      return false;
   for (; *ids; ids += strlen(ids) + 1)
      if (compat_strcasestr(ids, "Class_58&SubClass_42"))
         return true;
   return false;
}

#ifdef HAVE_WINUSB_BIND
/* Command-line arguments the elevated copy of RetroArch is started
 * with; an installer may start it with one too. */
#define WINUSB_BIND_ARG     "--winusb-bind-ds3"
#define WINUSB_RESTORE_ARG  "--winusb-restore-ds3"
#define WINUSB_BIND_GCA_ARG "--winusb-bind-gca"
#define WINUSB_BIND_XID_ARG "--winusb-bind-xid"

/* What is moved: the DualShock 3s, or the Wii U GameCube adapters.
 * Windows' own driver cannot start either; it cannot run the adapter
 * at all, so it is only ever moved to WinUSB. */
enum winusb_bind_kind
{
   WINUSB_BIND_DS3 = 0,
   WINUSB_BIND_GCA,
   /* original Xbox pads: no driver of Windows' runs them at all */
   WINUSB_BIND_XID
};

/* A DualShock 3, by its USB ids. */
bool winusb_bind_is_ds3(unsigned vid, unsigned pid);

/* A Wii U GameCube adapter, by its USB ids. */
static INLINE bool winusb_bind_is_gca(unsigned vid, unsigned pid)
{
   return vid == 0x057e && pid == 0x0337;
}

/* @arg one of the three above: the pads of its kind plugged in moved
 * to WinUSB, or given back to Windows' own driver, with *ret the exit
 * code (how many were, or 255 when it could not be done). False for
 * any other argument. Needs administrator rights. */
bool winusb_bind_command(const char *arg, int *ret);

/* The same asked for from the menu: a copy of RetroArch is started
 * with administrator rights to do it, and the outcome is shown as a
 * message. Nothing waits on the main thread. False while one asked for
 * before is still going. */
bool winusb_bind_request(enum winusb_bind_kind kind, bool bind);

/* The WinUSB scan's word, at each scan, on whether a GameCube adapter
 * is plugged in that nothing could open - Windows' own driver cannot
 * run it - said on screen once when one comes; and the menu's ask.
 * Both on the frontend's thread. */
void winusb_bind_gca_seen(bool waiting);
bool winusb_bind_gca_waiting(void);
/* The same for an original Xbox pad */
void winusb_bind_xid_seen(bool waiting);
bool winusb_bind_xid_waiting(void);
#endif

RETRO_END_DECLS

#endif
