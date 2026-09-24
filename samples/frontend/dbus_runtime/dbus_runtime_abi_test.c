/* gfx/common/dbus_runtime.h against libdbus's own headers. libdbus
 * writes into DBusError and DBusMessageIter where the caller allocated
 * them, and takes its type codes and bus numbers by value, so a mirror
 * that drifts in size, alignment, a field offset or a constant would
 * corrupt the stack or ask for the wrong thing. */

#include <stdio.h>
#include <stddef.h>

#include <dbus/dbus.h>

#include "gfx/common/dbus_runtime.h"

#ifndef RARCH_HAVE_DBUS_RUNTIME
#error "dbus_runtime.h is gated off on this target"
#endif

struct align_error { char c; DBusError     v; };
struct align_rerr  { char c; rdbus_error_t v; };
struct align_iter  { char c; DBusMessageIter v; };
struct align_riter { char c; rdbus_iter_t    v; };

static int failures;

static void same(const char *what, long a, long b)
{
   if (a == b)
      printf("   ok   %-34s %ld\n", what, a);
   else
   {
      printf("   FAIL %-34s libdbus %ld, mirror %ld\n", what, a, b);
      failures++;
   }
}

int main(void)
{
   printf("DBusError\n");
   same("size", (long)sizeof(DBusError), (long)sizeof(rdbus_error_t));
   same("alignment", (long)offsetof(struct align_error, v),
         (long)offsetof(struct align_rerr, v));
   same("offset of name", (long)offsetof(DBusError, name),
         (long)offsetof(rdbus_error_t, name));
   same("offset of message", (long)offsetof(DBusError, message),
         (long)offsetof(rdbus_error_t, message));
   same("offset of padding1", (long)offsetof(DBusError, padding1),
         (long)offsetof(rdbus_error_t, padding1));

   printf("DBusMessageIter\n");
   same("size", (long)sizeof(DBusMessageIter), (long)sizeof(rdbus_iter_t));
   same("alignment", (long)offsetof(struct align_iter, v),
         (long)offsetof(struct align_riter, v));
   same("offset of pad3", (long)offsetof(DBusMessageIter, pad3),
         (long)offsetof(rdbus_iter_t, pad3));

   printf("constants\n");
   same("bool size", (long)sizeof(dbus_bool_t), (long)sizeof(rdbus_bool_t));
   same("DBUS_BUS_SESSION", (long)DBUS_BUS_SESSION, (long)RDBUS_BUS_SESSION);
   same("DBUS_BUS_SYSTEM", (long)DBUS_BUS_SYSTEM, (long)RDBUS_BUS_SYSTEM);
   same("DBUS_TYPE_INVALID", (long)DBUS_TYPE_INVALID, (long)RDBUS_TYPE_INVALID);
   same("DBUS_TYPE_INT32", (long)DBUS_TYPE_INT32, (long)RDBUS_TYPE_INT32);
   same("DBUS_TYPE_UINT32", (long)DBUS_TYPE_UINT32, (long)RDBUS_TYPE_UINT32);
   same("DBUS_TYPE_INT64", (long)DBUS_TYPE_INT64, (long)RDBUS_TYPE_INT64);
   same("DBUS_TYPE_UINT64", (long)DBUS_TYPE_UINT64, (long)RDBUS_TYPE_UINT64);
   same("DBUS_TYPE_STRING", (long)DBUS_TYPE_STRING, (long)RDBUS_TYPE_STRING);
   same("DBUS_TYPE_VARIANT", (long)DBUS_TYPE_VARIANT, (long)RDBUS_TYPE_VARIANT);
   same("DBUS_TYPE_BOOLEAN", (long)DBUS_TYPE_BOOLEAN, (long)RDBUS_TYPE_BOOLEAN);
   same("DBUS_TYPE_DOUBLE", (long)DBUS_TYPE_DOUBLE, (long)RDBUS_TYPE_DOUBLE);
   same("DBUS_TYPE_ARRAY", (long)DBUS_TYPE_ARRAY, (long)RDBUS_TYPE_ARRAY);
   same("DBUS_TYPE_STRUCT", (long)DBUS_TYPE_STRUCT, (long)RDBUS_TYPE_STRUCT);
   same("DBUS_TYPE_DICT_ENTRY", (long)DBUS_TYPE_DICT_ENTRY, (long)RDBUS_TYPE_DICT_ENTRY);
   same("DBUS_TIMEOUT_USE_DEFAULT", (long)DBUS_TIMEOUT_USE_DEFAULT,
         (long)RDBUS_TIMEOUT_USE_DEFAULT);

   if (failures)
   {
      printf("dbus_runtime: %d mismatch(es) with libdbus\n", failures);
      return 1;
   }
   printf("dbus_runtime: the mirror matches libdbus\n");
   return 0;
}
