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

#ifndef __RARCH_DBUS_RUNTIME_H
#define __RARCH_DBUS_RUNTIME_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

/* libdbus, loaded when first needed instead of linked, so every build
 * on a desktop Unix can talk to the system and session buses without
 * the D-Bus headers at build time, and runs unchanged where the library
 * is absent. What is declared here is libdbus's public, ABI-stable
 * surface under names of our own, so it can share a translation unit
 * with <dbus/dbus.h>. */
#if defined(HAVE_DYLIB) && !defined(__ANDROID__) && !defined(__APPLE__) \
   && !defined(ORBIS) && !defined(__ORBIS__) \
   && (defined(__linux__) || defined(__FreeBSD__) \
      || defined(__OpenBSD__) || defined(__NetBSD__))
#define RARCH_HAVE_DBUS_RUNTIME 1
#endif

#ifdef RARCH_HAVE_DBUS_RUNTIME

RETRO_BEGIN_DECLS

#define RDBUS_BUS_SESSION                0
#define RDBUS_BUS_SYSTEM                 1

#define RDBUS_TYPE_INVALID               0
#define RDBUS_TYPE_INT32                 ((int)'i')
#define RDBUS_TYPE_UINT32                ((int)'u')
#define RDBUS_TYPE_INT64                 ((int)'x')
#define RDBUS_TYPE_UINT64                ((int)'t')
#define RDBUS_TYPE_STRING                ((int)'s')
#define RDBUS_TYPE_VARIANT               ((int)'v')
#define RDBUS_TYPE_BOOLEAN               ((int)'b')
#define RDBUS_TYPE_DOUBLE                ((int)'d')
#define RDBUS_TYPE_ARRAY                 ((int)'a')
#define RDBUS_TYPE_STRUCT                ((int)'r')
#define RDBUS_TYPE_DICT_ENTRY            ((int)'e')

#define RDBUS_TIMEOUT_USE_DEFAULT        (-1)

#define RDBUS_ERROR_SERVICE_UNKNOWN      "org.freedesktop.DBus.Error.ServiceUnknown"
#define RDBUS_ERROR_NAME_HAS_NO_OWNER    "org.freedesktop.DBus.Error.NameHasNoOwner"

typedef uint32_t                   rdbus_bool_t;
typedef struct rdbus_connection    rdbus_connection_t;
typedef struct rdbus_message       rdbus_message_t;

/* DBusError: two public fields, then space libdbus keeps to itself. */
typedef struct rdbus_error
{
   const char  *name;
   const char  *message;
   unsigned int dummy1 : 1;
   unsigned int dummy2 : 1;
   unsigned int dummy3 : 1;
   unsigned int dummy4 : 1;
   unsigned int dummy5 : 1;
   void        *padding1;
} rdbus_error_t;

/* DBusMessageIter: no public fields, only its size and alignment. */
typedef struct rdbus_iter
{
   void    *dummy1;
   void    *dummy2;
   uint32_t dummy3;
   int      dummy4;
   int      dummy5;
   int      dummy6;
   int      dummy7;
   int      dummy8;
   int      dummy9;
   int      dummy10;
   int      dummy11;
   int      pad1;
   void    *pad2;
   void    *pad3;
} rdbus_iter_t;

typedef struct rdbus
{
   void                (*error_init)(rdbus_error_t *err);
   void                (*error_free)(rdbus_error_t *err);
   rdbus_connection_t *(*bus_get_private)(int type, rdbus_error_t *err);
   void                (*connection_set_exit_on_disconnect)(
         rdbus_connection_t *conn, rdbus_bool_t exit_on_disconnect);
   void                (*connection_close)(rdbus_connection_t *conn);
   void                (*connection_unref)(rdbus_connection_t *conn);
   rdbus_message_t    *(*connection_send_with_reply_and_block)(
         rdbus_connection_t *conn, rdbus_message_t *msg,
         int timeout_ms, rdbus_error_t *err);
   rdbus_message_t    *(*message_new_method_call)(const char *dest,
         const char *path, const char *iface, const char *method);
   rdbus_bool_t        (*message_append_args)(rdbus_message_t *msg,
         int first_type, ...);
   void                (*message_unref)(rdbus_message_t *msg);
   rdbus_bool_t        (*message_iter_init)(rdbus_message_t *msg,
         rdbus_iter_t *iter);
   int                 (*message_iter_get_arg_type)(rdbus_iter_t *iter);
   void                (*message_iter_recurse)(rdbus_iter_t *iter,
         rdbus_iter_t *sub);
   void                (*message_iter_get_basic)(rdbus_iter_t *iter,
         void *value);
   rdbus_bool_t        (*threads_init_default)(void);
   rdbus_bool_t        (*error_is_set)(const rdbus_error_t *err);
   rdbus_bool_t        (*bus_name_has_owner)(rdbus_connection_t *conn,
         const char *name, rdbus_error_t *err);
   void                (*bus_add_match)(rdbus_connection_t *conn,
         const char *rule, rdbus_error_t *err);
   rdbus_bool_t        (*connection_get_unix_fd)(rdbus_connection_t *conn,
         int *fd);
   rdbus_bool_t        (*connection_read_write)(rdbus_connection_t *conn,
         int timeout_ms);
   rdbus_message_t    *(*connection_pop_message)(rdbus_connection_t *conn);
   rdbus_bool_t        (*message_is_signal)(rdbus_message_t *msg,
         const char *iface, const char *member);
   rdbus_bool_t        (*message_iter_next)(rdbus_iter_t *iter);
   void                (*message_iter_init_append)(rdbus_message_t *msg,
         rdbus_iter_t *iter);
   rdbus_bool_t        (*message_iter_append_basic)(rdbus_iter_t *iter,
         int type, const void *value);
   rdbus_bool_t        (*message_iter_open_container)(rdbus_iter_t *iter,
         int type, const char *signature, rdbus_iter_t *sub);
   rdbus_bool_t        (*message_iter_close_container)(rdbus_iter_t *iter,
         rdbus_iter_t *sub);
   void                *lib;
} rdbus_t;

/**
 * libdbus's entry points, loading the library the first time; NULL
 * where it is not installed or lacks one of them. Callable from any
 * thread. The first call goes through the dynamic loader, so it belongs
 * on a thread that may wait: a helper, never the main or audio thread.
 */
const rdbus_t *rdbus_get(void);

RETRO_END_DECLS

#endif

#endif
