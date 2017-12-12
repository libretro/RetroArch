/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_DBUS
#include <dbus/dbus.h>
static DBusConnection* dbus_connection      = NULL;
static unsigned int dbus_screensaver_cookie = 0;
#endif

#include "../../verbosity.h"

void dbus_ensure_connection(void)
{
#ifdef HAVE_DBUS
    DBusError err;
    int ret;

    dbus_error_init(&err);

    dbus_connection = dbus_bus_get_private(DBUS_BUS_SESSION, &err);

    if (dbus_error_is_set(&err))
    {
        RARCH_LOG("[DBus]: Failed to get DBus connection. Screensaver will not be suspended via DBus.\n");
        dbus_error_free(&err);
    }

    if (dbus_connection)
        dbus_connection_set_exit_on_disconnect(dbus_connection, true);
#endif
}

void dbus_close_connection(void)
{
#ifdef HAVE_DBUS
   if (!dbus_connection)
      return;

   dbus_connection_close(dbus_connection);
   dbus_connection_unref(dbus_connection);
   dbus_connection = NULL;
#endif
}

bool dbus_screensaver_inhibit(void)
{
   bool ret           = false;
#ifdef HAVE_DBUS
   const char *app    = "RetroArch";
   const char *reason = "Playing a game";
   DBusMessage   *msg = NULL;
   DBusMessage *reply = NULL;

   if (!dbus_connection)
      return false; /* DBus connection was not obtained */

   if (dbus_screensaver_cookie > 0)
      return true; /* Already inhibited */

   msg = dbus_message_new_method_call("org.freedesktop.ScreenSaver",
         "/org/freedesktop/ScreenSaver",
         "org.freedesktop.ScreenSaver",
         "Inhibit");

   if (!msg)
      return false;

   if (!dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &app,
            DBUS_TYPE_STRING, &reason,
            DBUS_TYPE_INVALID))
   {
      dbus_message_unref(msg);
      return false;
   }

   reply = dbus_connection_send_with_reply_and_block(dbus_connection,
         msg, 300, NULL);

   if (reply != NULL)
   {
      if (!dbus_message_get_args(reply, NULL,
               DBUS_TYPE_UINT32, &dbus_screensaver_cookie,
               DBUS_TYPE_INVALID))
         dbus_screensaver_cookie = 0;
      else
         ret = true;

      dbus_message_unref(reply);
   }

   dbus_message_unref(msg);

   if (dbus_screensaver_cookie == 0)
   {
      RARCH_ERR("[DBus]: Failed to suspend screensaver via DBus.\n");
   }
   else
   {
      RARCH_LOG("[DBus]: Suspended screensaver via DBus.\n");
   }

#endif

   return ret;
}

void dbus_screensaver_uninhibit(void)
{
#ifdef HAVE_DBUS
   DBusMessage *msg = NULL;

   if (!dbus_connection)
      return;

   if (dbus_screensaver_cookie == 0)
      return;

   msg = dbus_message_new_method_call("org.freedesktop.ScreenSaver",
         "/org/freedesktop/ScreenSaver",
         "org.freedesktop.ScreenSaver",
         "UnInhibit");
   if (!msg)
       return;

   dbus_message_append_args(msg,
         DBUS_TYPE_UINT32, &dbus_screensaver_cookie,
         DBUS_TYPE_INVALID);

   if (dbus_connection_send(dbus_connection, msg, NULL))
      dbus_connection_flush(dbus_connection);
   dbus_message_unref(msg);

   dbus_screensaver_cookie = 0;
#endif
}

/* Returns false when fallback should be attempted */
bool dbus_suspend_screensaver(bool enable)
{
#ifdef HAVE_DBUS
   if (enable)
      return dbus_screensaver_inhibit();
   dbus_screensaver_uninhibit();
#endif
   return false;
}
