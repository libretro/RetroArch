/* A stand-in for power-profiles-daemon on a private bus, for
 * gamemode_test. It owns the interface name given on the command line
 * and appends one line per call it gets to the log file:
 *
 *   hold <profile> <reason> <application id>
 *   release <cookie>
 *
 * With "refuse" as third argument every HoldProfile gets an error, as
 * ppd answers on a machine without a performance profile.
 *
 * libdbus is linked by soname and declared here by hand (its public ABI,
 * mirrored in gfx/common/dbus_runtime.h), so no D-Bus headers are
 * needed to build this.
 *
 *   fake_ppd <bus name> <log file> [refuse]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "gfx/common/dbus_runtime.h"

typedef rdbus_connection_t DBusConnection;
typedef rdbus_message_t    DBusMessage;
typedef rdbus_error_t      DBusError;

extern void         dbus_error_init(DBusError *e);
extern void         dbus_error_free(DBusError *e);
extern DBusConnection *dbus_bus_get_private(int type, DBusError *e);
extern int          dbus_bus_request_name(DBusConnection *c, const char *name,
      unsigned flags, DBusError *e);
extern uint32_t     dbus_connection_read_write(DBusConnection *c, int ms);
extern DBusMessage *dbus_connection_pop_message(DBusConnection *c);
extern uint32_t     dbus_connection_send(DBusConnection *c, DBusMessage *m,
      uint32_t *serial);
extern void         dbus_connection_flush(DBusConnection *c);
extern uint32_t     dbus_message_is_method_call(DBusMessage *m,
      const char *iface, const char *method);
extern uint32_t     dbus_message_get_args(DBusMessage *m, DBusError *e,
      int first, ...);
extern DBusMessage *dbus_message_new_method_return(DBusMessage *m);
extern DBusMessage *dbus_message_new_error(DBusMessage *m,
      const char *name, const char *text);
extern uint32_t     dbus_message_append_args(DBusMessage *m, int first, ...);
extern void         dbus_message_unref(DBusMessage *m);

#define DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER 1

static void log_line(const char *path, const char *line)
{
   FILE *f = fopen(path, "a");
   if (!f)
      return;
   fputs(line, f);
   fputc('\n', f);
   fclose(f);
}

int main(int argc, char **argv)
{
   DBusError err;
   DBusConnection *conn;
   const char *name, *logpath;
   int refuse;
   uint32_t next_cookie = 41;

   if (argc < 3)
   {
      fprintf(stderr, "usage: fake_ppd <bus name> <log file> [refuse]\n");
      return 2;
   }
   name    = argv[1];
   logpath = argv[2];
   refuse  = argc > 3 && !strcmp(argv[3], "refuse");

   dbus_error_init(&err);
   if (!(conn = dbus_bus_get_private(RDBUS_BUS_SYSTEM, &err)))
   {
      fprintf(stderr, "fake_ppd: no bus: %s\n", err.message ? err.message : "?");
      return 1;
   }
   if (dbus_bus_request_name(conn, name, 0, &err)
         != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
   {
      fprintf(stderr, "fake_ppd: could not own %s\n", name);
      return 1;
   }
   log_line(logpath, "ready");

   while (dbus_connection_read_write(conn, -1))
   {
      DBusMessage *msg;
      while ((msg = dbus_connection_pop_message(conn)))
      {
         DBusMessage *reply = NULL;
         char line[512];

         if (dbus_message_is_method_call(msg, name, "HoldProfile"))
         {
            const char *profile = "", *reason = "", *app = "";
            if (dbus_message_get_args(msg, &err,
                     RDBUS_TYPE_STRING, &profile,
                     RDBUS_TYPE_STRING, &reason,
                     RDBUS_TYPE_STRING, &app,
                     RDBUS_TYPE_INVALID))
            {
               snprintf(line, sizeof(line), "hold %s %s %s",
                     profile, reason, app);
               log_line(logpath, line);
               if (refuse)
                  reply = dbus_message_new_error(msg,
                        "org.freedesktop.UPower.PowerProfiles.Error.NotSupported",
                        "no performance profile");
               else if ((reply = dbus_message_new_method_return(msg)))
               {
                  uint32_t cookie = next_cookie++;
                  dbus_message_append_args(reply,
                        RDBUS_TYPE_UINT32, &cookie, RDBUS_TYPE_INVALID);
               }
            }
            dbus_error_free(&err);
         }
         else if (dbus_message_is_method_call(msg, name, "ReleaseProfile"))
         {
            uint32_t cookie = 0;
            if (dbus_message_get_args(msg, &err,
                     RDBUS_TYPE_UINT32, &cookie, RDBUS_TYPE_INVALID))
            {
               snprintf(line, sizeof(line), "release %u", (unsigned)cookie);
               log_line(logpath, line);
               reply = dbus_message_new_method_return(msg);
            }
            dbus_error_free(&err);
         }

         if (reply)
         {
            dbus_connection_send(conn, reply, NULL);
            dbus_connection_flush(conn);
            dbus_message_unref(reply);
         }
         dbus_message_unref(msg);
      }
   }
   return 0;
}
