/* A stand-in for UPower on a private bus, for gamemode_test: answers
 * Properties.Get(OnBattery) and, on each SIGUSR1, flips OnBattery and
 * sends PropertiesChanged, as UPower does when the charger is plugged
 * in or pulled. Appends "battery 0|1" to the log file on every flip.
 *
 *   fake_upower <log file> ac|battery
 */
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "gfx/common/dbus_runtime.h"

typedef rdbus_connection_t DBusConnection;
typedef rdbus_message_t    DBusMessage;
typedef rdbus_error_t      DBusError;
typedef rdbus_iter_t       DBusMessageIter;

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
extern DBusMessage *dbus_message_new_method_return(DBusMessage *m);
extern DBusMessage *dbus_message_new_signal(const char *path,
      const char *iface, const char *name);
extern uint32_t     dbus_message_append_args(DBusMessage *m, int first, ...);
extern void         dbus_message_iter_init_append(DBusMessage *m,
      DBusMessageIter *it);
extern uint32_t     dbus_message_iter_open_container(DBusMessageIter *it,
      int type, const char *sig, DBusMessageIter *sub);
extern uint32_t     dbus_message_iter_close_container(DBusMessageIter *it,
      DBusMessageIter *sub);
extern uint32_t     dbus_message_iter_append_basic(DBusMessageIter *it,
      int type, const void *v);
extern void         dbus_message_unref(DBusMessage *m);

static volatile sig_atomic_t flips;
static void on_usr1(int sig) { (void)sig; flips++; }

int main(int argc, char **argv)
{
   DBusError err;
   DBusConnection *conn;
   uint32_t on_battery;
   int seen = 0;
   const char *iface = "org.freedesktop.UPower";

   if (argc < 3)
      return 2;
   on_battery = !strcmp(argv[2], "battery");
   signal(SIGUSR1, on_usr1);

   dbus_error_init(&err);
   if (!(conn = dbus_bus_get_private(RDBUS_BUS_SYSTEM, &err))
         || dbus_bus_request_name(conn, iface, 0, &err) != 1)
   {
      fprintf(stderr, "fake_upower: no bus or name\n");
      return 1;
   }
   {
      FILE *f = fopen(argv[1], "a");
      if (f) { fputs("upower ready\n", f); fclose(f); }
   }

   for (;;)
   {
      DBusMessage *msg;
      dbus_connection_read_write(conn, 20);

      while (seen != flips)
      {
         DBusMessage *sig;
         FILE *f;
         seen++;
         on_battery = !on_battery;
         if ((f = fopen(argv[1], "a")))
         {
            fprintf(f, "battery %u\n", (unsigned)on_battery);
            fclose(f);
         }
         /* The body is only the interface name: the client follows the
          * signal, not what it carries. */
         if ((sig = dbus_message_new_signal("/org/freedesktop/UPower",
                  "org.freedesktop.DBus.Properties", "PropertiesChanged")))
         {
            dbus_message_append_args(sig, RDBUS_TYPE_STRING, &iface,
                  RDBUS_TYPE_INVALID);
            dbus_connection_send(conn, sig, NULL);
            dbus_connection_flush(conn);
            dbus_message_unref(sig);
         }
      }

      while ((msg = dbus_connection_pop_message(conn)))
      {
         if (dbus_message_is_method_call(msg,
                  "org.freedesktop.DBus.Properties", "Get"))
         {
            DBusMessage *reply = dbus_message_new_method_return(msg);
            if (reply)
            {
               DBusMessageIter it, var;
               dbus_message_iter_init_append(reply, &it);
               dbus_message_iter_open_container(&it, RDBUS_TYPE_VARIANT,
                     "b", &var);
               dbus_message_iter_append_basic(&var, RDBUS_TYPE_BOOLEAN,
                     &on_battery);
               dbus_message_iter_close_container(&it, &var);
               dbus_connection_send(conn, reply, NULL);
               dbus_connection_flush(conn);
               dbus_message_unref(reply);
            }
         }
         dbus_message_unref(msg);
      }
   }
   return 0;
}
