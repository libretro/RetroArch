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

#include <stdlib.h>
#include <string.h>

#include "dbus_runtime.h"

#ifdef RARCH_HAVE_DBUS_RUNTIME

#include <dynamic/dylib.h>
#include <retro_atomic.h>

/* The table once loaded. Whoever loads first publishes theirs; a loser
 * of the race drops its own copy. Nothing waits on anyone. */
#ifdef RETRO_ATOMIC_HAS_PTR
static retro_atomic_ptr_t rdbus_published;
#else
static rdbus_t           *rdbus_published;
#endif

/* Function pointers of different types share one representation on
 * every target this builds for; copying the bytes keeps the store clear
 * of the aliasing rules a cast would break. */
#define RDBUS_SYM(field, name) \
   if (!(sym = dylib_proc(t->lib, name))) \
      goto fail; \
   memcpy(&t->field, &sym, sizeof(sym))

static rdbus_t *rdbus_load(void)
{
   function_t sym;
   rdbus_t *t = (rdbus_t*)calloc(1, sizeof(*t));

   if (!t)
      return NULL;
   if (!(t->lib = dylib_load("libdbus-1.so.3")))
   {
      free(t);
      return NULL;
   }

   RDBUS_SYM(error_init,                           "dbus_error_init");
   RDBUS_SYM(error_free,                           "dbus_error_free");
   RDBUS_SYM(bus_get_private,                      "dbus_bus_get_private");
   RDBUS_SYM(connection_set_exit_on_disconnect,    "dbus_connection_set_exit_on_disconnect");
   RDBUS_SYM(connection_close,                     "dbus_connection_close");
   RDBUS_SYM(connection_unref,                     "dbus_connection_unref");
   RDBUS_SYM(connection_send_with_reply_and_block, "dbus_connection_send_with_reply_and_block");
   RDBUS_SYM(message_new_method_call,              "dbus_message_new_method_call");
   RDBUS_SYM(message_append_args,                  "dbus_message_append_args");
   RDBUS_SYM(message_unref,                        "dbus_message_unref");
   RDBUS_SYM(message_iter_init,                    "dbus_message_iter_init");
   RDBUS_SYM(message_iter_get_arg_type,            "dbus_message_iter_get_arg_type");
   RDBUS_SYM(message_iter_recurse,                 "dbus_message_iter_recurse");
   RDBUS_SYM(message_iter_get_basic,               "dbus_message_iter_get_basic");
   RDBUS_SYM(threads_init_default,                 "dbus_threads_init_default");
   RDBUS_SYM(error_is_set,                         "dbus_error_is_set");
   RDBUS_SYM(bus_name_has_owner,                   "dbus_bus_name_has_owner");
   RDBUS_SYM(bus_add_match,                        "dbus_bus_add_match");
   RDBUS_SYM(connection_get_unix_fd,               "dbus_connection_get_unix_fd");
   RDBUS_SYM(connection_read_write,                "dbus_connection_read_write");
   RDBUS_SYM(connection_pop_message,               "dbus_connection_pop_message");
   RDBUS_SYM(message_is_signal,                    "dbus_message_is_signal");
   RDBUS_SYM(message_iter_next,                    "dbus_message_iter_next");
   RDBUS_SYM(message_iter_init_append,             "dbus_message_iter_init_append");
   RDBUS_SYM(message_iter_append_basic,            "dbus_message_iter_append_basic");
   RDBUS_SYM(message_iter_open_container,          "dbus_message_iter_open_container");
   RDBUS_SYM(message_iter_close_container,         "dbus_message_iter_close_container");

   /* Other threads of the process may be using libdbus through a
    * connection of their own. */
   t->threads_init_default();
   return t;

fail:
   dylib_close(t->lib);
   free(t);
   return NULL;
}

const rdbus_t *rdbus_get(void)
{
   rdbus_t *t;
#ifdef RETRO_ATOMIC_HAS_PTR
   if ((t = (rdbus_t*)retro_atomic_load_acquire_ptr(&rdbus_published)))
      return t;
   if (!(t = rdbus_load()))
      return NULL;
   if (!retro_atomic_cas_ptr(&rdbus_published, NULL, t))
   {
      /* Another thread published first; the library stays loaded
       * through its handle. */
      dylib_close(t->lib);
      free(t);
      return (const rdbus_t*)retro_atomic_load_acquire_ptr(&rdbus_published);
   }
   return t;
#else
   /* Without pointer atomics there are no threads to race. */
   if (!rdbus_published)
      rdbus_published = rdbus_load();
   return rdbus_published;
#endif
}

#endif
