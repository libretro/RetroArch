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
/* TODO/FIXME - static globals */
static DBusConnection* dbus_connection      = NULL;
static unsigned int dbus_screensaver_cookie = 0;
#endif

#include "../../verbosity.h"
#include <string.h>

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

   if (reply)
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

static bool
get_cursor_settings_from_env(char **theme, int *size)
{
	char *env_xtheme;
	char *env_xsize;

	env_xtheme = getenv("XCURSOR_THEME");
	if (env_xtheme != NULL)
		*theme = strdup(env_xtheme);

	env_xsize = getenv("XCURSOR_SIZE");
	if (env_xsize != NULL)
		*size = atoi(env_xsize);

	return env_xtheme != NULL && env_xsize != NULL;
}

#ifdef HAVE_DBUS
static DBusMessage *
get_setting_sync(DBusConnection *const connection,
      const char *key,
      const char *value)
{
   DBusError error;
   dbus_bool_t success;
   DBusMessage *message;
   DBusMessage *reply;

   message = dbus_message_new_method_call(
      "org.freedesktop.portal.Desktop",
      "/org/freedesktop/portal/desktop",
      "org.freedesktop.portal.Settings",
      "Read");

   success = dbus_message_append_args(message,
      DBUS_TYPE_STRING, &key,
      DBUS_TYPE_STRING, &value,
      DBUS_TYPE_INVALID);

   if (!success)
      return NULL;

   dbus_error_init(&error);

   reply = dbus_connection_send_with_reply_and_block(
         connection,
         message,
         DBUS_TIMEOUT_USE_DEFAULT,
         &error);

   dbus_message_unref(message);

   if (dbus_error_is_set(&error)) {
      dbus_error_free(&error);
      return NULL;
   }

   dbus_error_free(&error);
   return reply;
}

static bool
parse_type(DBusMessage *const reply,
      const int type,
      void *value)
{
   DBusMessageIter iter[3];

   dbus_message_iter_init(reply, &iter[0]);
   if (dbus_message_iter_get_arg_type(&iter[0]) != DBUS_TYPE_VARIANT)
      return false;

   dbus_message_iter_recurse(&iter[0], &iter[1]);
   if (dbus_message_iter_get_arg_type(&iter[1]) != DBUS_TYPE_VARIANT)
      return false;

   dbus_message_iter_recurse(&iter[1], &iter[2]);
   if (dbus_message_iter_get_arg_type(&iter[2]) != type)
      return false;

   dbus_message_iter_get_basic(&iter[2], value);

   return true;
}

bool
dbus_get_cursor_settings(char **theme, int *size)
{
   static const char name[] = "org.gnome.desktop.interface";
   static const char key_theme[] = "cursor-theme";
   static const char key_size[] = "cursor-size";

   DBusError error;
   DBusConnection *connection;
   DBusMessage *reply;
   const char *value_theme = NULL;

   dbus_error_init(&error);

   connection = dbus_bus_get(DBUS_BUS_SESSION, &error);

   if (dbus_error_is_set(&error))
      goto fallback;

   reply = get_setting_sync(connection, name, key_theme);
   if (!reply)
      goto fallback;

   if (!parse_type(reply, DBUS_TYPE_STRING, &value_theme)) {
      dbus_message_unref(reply);
      goto fallback;
   }

   *theme = strdup(value_theme);

   dbus_message_unref(reply);

   reply = get_setting_sync(connection, name, key_size);
   if (!reply)
      goto fallback;

   if (!parse_type(reply, DBUS_TYPE_INT32, size)) {
      dbus_message_unref(reply);
      goto fallback;
   }

   dbus_message_unref(reply);

   return true;

fallback:
   return get_cursor_settings_from_env(theme, size);
}
#else
bool
dbus_get_cursor_settings(char **theme, int *size)
{
   return get_cursor_settings_from_env(theme, size);
}
#endif
