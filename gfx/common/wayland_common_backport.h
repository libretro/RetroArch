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

#pragma once

#include <stdint.h>

#define WL_CLOSURE_MAX_ARGS 20

struct wl_interface;
struct wl_proxy;
struct wl_display;

extern uint32_t FALLBACK_wl_proxy_get_version(struct wl_proxy *proxy);

extern struct wl_proxy *FALLBACK_wl_proxy_marshal_constructor(
   struct wl_proxy *proxy,
   uint32_t opcode,
   const struct wl_interface *interface,
   ...);

extern struct wl_proxy *FALLBACK_wl_proxy_marshal_constructor_versioned(
   struct wl_proxy *proxy,
   uint32_t opcode,
   const struct wl_interface *interface,
   uint32_t version,
   ...);

extern int FALLBACK_wl_display_prepare_read(struct wl_display *display);
extern int FALLBACK_wl_display_read_events(struct wl_display *display);
extern void FALLBACK_wl_display_cancel_read(struct wl_display *display);

#define wl_proxy_get_version                   WRAPPER_wl_proxy_get_version
#define wl_proxy_marshal_constructor           WRAPPER_wl_proxy_marshal_constructor
#define wl_proxy_marshal_constructor_versioned WRAPPER_wl_proxy_marshal_constructor_versioned
#define wl_display_prepare_read                WRAPPER_wl_display_prepare_read
#define wl_display_read_events                 WRAPPER_wl_display_read_events
#define wl_display_cancel_read                 WRAPPER_wl_display_cancel_read

extern uint32_t WEBOS_wl_proxy_get_version(struct wl_proxy *proxy);
extern struct wl_proxy *WEBOS_wl_proxy_marshal_constructor(
   struct wl_proxy *proxy,
   uint32_t opcode,
   const struct wl_interface *interface,
   ...);

extern struct wl_proxy *WRAPPER__wl_proxy_marshal_constructor_versioned(
   struct wl_proxy *proxy,
   uint32_t opcode,
   const struct wl_interface *interface,
   uint32_t version,
   ...);

extern int WRAPPER_wl_display_prepare_read(struct wl_display *display);
extern int WRAPPER_wl_display_read_events(struct wl_display *display);
extern void WRAPPER_wl_display_cancel_read(struct wl_display *display);
