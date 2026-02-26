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

#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <dlfcn.h>

#include "wayland_common.h"
#include "wayland_common_backport.h"
#include "../../verbosity.h"

/*
   Backwards compatibility for older versions of libwayland-client that
   which do not have:
   wl_proxy_marshal_constructor_versioned
   wl_proxy_marshal_array_constructor_versioned
   wl_proxy_get_version
   wl_display_prepare_read
   wl_display_read_events
   wl_display_cancel_read
*/

/* Function pointers for dynamic dispatch */
static uint32_t (*real_wl_proxy_get_version)(struct wl_proxy *) = NULL;

static struct wl_proxy *(*real_wl_proxy_marshal_constructor)(
    struct wl_proxy *, uint32_t, const struct wl_interface *, ...) = NULL;

static struct wl_proxy *(*real_wl_proxy_marshal_constructor_versioned)(
    struct wl_proxy *, uint32_t, const struct wl_interface *, uint32_t, ...) = NULL;

static int (*real_wl_display_prepare_read)(struct wl_display *) = NULL;
static int (*real_wl_display_read_events)(struct wl_display *) = NULL;
static void (*real_wl_display_cancel_read)(struct wl_display *) = NULL;

static bool wayland_init_done = false;

static void wayland_init_fallbacks(void)
{
   void *wl_handle;

   if (wayland_init_done)
      return;

   wayland_init_done = true;

   /* Try to dynamically load the real functions */
   wl_handle = dlopen(NULL, RTLD_LAZY);
   if (wl_handle)
   {
      real_wl_proxy_get_version =
         dlsym(wl_handle, "wl_proxy_get_version");
      real_wl_proxy_marshal_constructor =
         dlsym(wl_handle, "wl_proxy_marshal_constructor");
      real_wl_proxy_marshal_constructor_versioned =
         dlsym(wl_handle, "wl_proxy_marshal_constructor_versioned");
      real_wl_display_prepare_read =
         dlsym(wl_handle, "wl_display_prepare_read");
      real_wl_display_read_events =
         dlsym(wl_handle, "wl_display_read_events");
      real_wl_display_cancel_read =
         dlsym(wl_handle, "wl_display_cancel_read");

      dlclose(wl_handle);
   }

   if (!real_wl_proxy_get_version)
      RARCH_LOG("[Wayland] Using fallback wl_proxy_get_version\n");

   if (!real_wl_proxy_marshal_constructor)
      RARCH_LOG("[Wayland] Using fallback wl_proxy_marshal_constructor\n");

   if (!real_wl_proxy_marshal_constructor_versioned)
      RARCH_LOG("[Wayland] Using fallback wl_proxy_marshal_constructor_versioned\n");

   if (!real_wl_display_prepare_read)
      RARCH_LOG("[Wayland] Using fallback wl_display_prepare_read\n");

   if (!real_wl_display_read_events)
      RARCH_LOG("[Wayland] Using fallback wl_display_read_events\n");

   if (!real_wl_display_cancel_read)
      RARCH_LOG("[Wayland] Using fallback wl_display_cancel_read\n");
}

uint32_t FALLBACK_wl_proxy_get_version(struct wl_proxy *proxy)
{
   (void)proxy;
   return 0;
}

/* Wrapper for wl_proxy_get_version */
uint32_t WRAPPER_wl_proxy_get_version(struct wl_proxy *proxy)
{
   uint32_t result;

   wayland_init_fallbacks();

   if (real_wl_proxy_get_version)
      result = real_wl_proxy_get_version(proxy);
   else
      result = FALLBACK_wl_proxy_get_version(proxy);

   return result;
}

/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
static int parse_msg_signature(const char *signature, int *new_id_index)
{
   int count = 0;
   for (; *signature; ++signature)
   {
      switch (*signature)
      {
      case 'n':
         *new_id_index = count;
         /* Intentional fallthrough */
      case 'i':
      case 'u':
      case 'f':
      case 's':
      case 'o':
      case 'a':
      case 'h':
         ++count;
         break;
      }
   }

   return count;
}

struct wl_proxy *FALLBACK_wl_proxy_marshal_constructor(
   struct wl_proxy *proxy,
   uint32_t opcode,
   const struct wl_interface *interface,
   ...)
{
   va_list ap;
   void *varargs[WL_CLOSURE_MAX_ARGS];
   int num_args;
   int new_id_index = -1;
   struct wl_interface *proxy_interface;
   struct wl_proxy *id;

   id = wl_proxy_create(proxy, interface);

   if (!id)
      return NULL;

   proxy_interface = (*(struct wl_interface **)proxy);
   if (opcode > proxy_interface->method_count)
      return NULL;

   num_args = parse_msg_signature(
      proxy_interface->methods[opcode].signature,
      &new_id_index);

   if (new_id_index < 0)
      return NULL;

   memset(varargs, 0, sizeof(varargs));
   va_start(ap, interface);
   for (int i = 0; i < num_args; i++)
      varargs[i] = va_arg(ap, void *);
   va_end(ap);

   varargs[new_id_index] = id;

   wl_proxy_marshal(proxy, opcode,
                    varargs[0], varargs[1], varargs[2], varargs[3],
                    varargs[4], varargs[5], varargs[6], varargs[7],
                    varargs[8], varargs[9], varargs[10], varargs[11],
                    varargs[12], varargs[13], varargs[14], varargs[15],
                    varargs[16], varargs[17], varargs[18], varargs[19]);

   return id;
}

struct wl_proxy *FALLBACK_wl_proxy_marshal_constructor_versioned(
    struct wl_proxy *proxy,
    uint32_t opcode,
    const struct wl_interface *interface,
    uint32_t version,
    ...)
{
   va_list ap;
   void *varargs[WL_CLOSURE_MAX_ARGS];
   int num_args;
   int new_id_index = -1;
   struct wl_interface *proxy_interface;
   struct wl_proxy *id;

   (void)version; /* version parameter not available */

   id = wl_proxy_create(proxy, interface);

   if (!id)
      return NULL;

   proxy_interface = (*(struct wl_interface **)proxy);
   if (opcode > proxy_interface->method_count)
      return NULL;

   num_args = parse_msg_signature(
      proxy_interface->methods[opcode].signature,
      &new_id_index);

   if (new_id_index < 0)
      return NULL;

   memset(varargs, 0, sizeof(varargs));
   va_start(ap, version);
   for (int i = 0; i < num_args; i++)
      varargs[i] = va_arg(ap, void *);
   va_end(ap);

   varargs[new_id_index] = id;

   wl_proxy_marshal(proxy, opcode,
                    varargs[0], varargs[1], varargs[2], varargs[3],
                    varargs[4], varargs[5], varargs[6], varargs[7],
                    varargs[8], varargs[9], varargs[10], varargs[11],
                    varargs[12], varargs[13], varargs[14], varargs[15],
                    varargs[16], varargs[17], varargs[18], varargs[19]);

   return id;
}

int FALLBACK_wl_display_prepare_read(struct wl_display *display)
{
   (void)display;
   return 0;
}

int FALLBACK_wl_display_read_events(struct wl_display *display)
{
   /* Fallback to dispatch */
   return wl_display_dispatch(display);
}

void FALLBACK_wl_display_cancel_read(struct wl_display *display)
{
   (void)display;
   /* no-op */
}

struct wl_proxy *WRAPPER_wl_proxy_marshal_constructor(
   struct wl_proxy *proxy,
   uint32_t opcode,
   const struct wl_interface *interface,
   ...)
{
   va_list ap;
   struct wl_proxy *result;

   wayland_init_fallbacks();

   va_start(ap, interface);

   result = FALLBACK_wl_proxy_marshal_constructor(proxy, opcode, interface,
      va_arg(ap, void*), va_arg(ap, void*), va_arg(ap, void*),
      va_arg(ap, void*), va_arg(ap, void*), va_arg(ap, void*),
      va_arg(ap, void*), va_arg(ap, void*), va_arg(ap, void*),
      va_arg(ap, void*), va_arg(ap, void*), va_arg(ap, void*),
      va_arg(ap, void*), va_arg(ap, void*), va_arg(ap, void*),
      va_arg(ap, void*), va_arg(ap, void*), va_arg(ap, void*),
      va_arg(ap, void*), va_arg(ap, void*));

   va_end(ap);

   return result;
}

struct wl_proxy *WRAPPER_wl_proxy_marshal_constructor_versioned(
   struct wl_proxy *proxy,
   uint32_t opcode,
   const struct wl_interface *interface,
   uint32_t version,
   ...)
{
   va_list ap;
   struct wl_proxy *result;

   wayland_init_fallbacks();

   va_start(ap, version);

   result = FALLBACK_wl_proxy_marshal_constructor_versioned(proxy, opcode, interface, version,
      va_arg(ap, void*), va_arg(ap, void*), va_arg(ap, void*),
      va_arg(ap, void*), va_arg(ap, void*), va_arg(ap, void*),
      va_arg(ap, void*), va_arg(ap, void*), va_arg(ap, void*),
      va_arg(ap, void*), va_arg(ap, void*), va_arg(ap, void*),
      va_arg(ap, void*), va_arg(ap, void*), va_arg(ap, void*),
      va_arg(ap, void*), va_arg(ap, void*), va_arg(ap, void*),
      va_arg(ap, void*), va_arg(ap, void*));

   va_end(ap);

   return result;
}

int WRAPPER_wl_display_prepare_read(struct wl_display *display)
{
   int result;

   wayland_init_fallbacks();

   if (real_wl_display_prepare_read)
      result = real_wl_display_prepare_read(display);
   else
      result = FALLBACK_wl_display_prepare_read(display);

   return result;
}

int WRAPPER_wl_display_read_events(struct wl_display *display)
{
   int result;

   wayland_init_fallbacks();

   if (real_wl_display_read_events)
      result = real_wl_display_read_events(display);
   else
      result = FALLBACK_wl_display_read_events(display);

   return result;
}

void WRAPPER_wl_display_cancel_read(struct wl_display *display)
{
   wayland_init_fallbacks();

   if (real_wl_display_cancel_read)
      real_wl_display_cancel_read(display);
   else
      FALLBACK_wl_display_cancel_read(display);
}
