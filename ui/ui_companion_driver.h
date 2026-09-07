/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __UI_COMPANION_DRIVER_H
#define __UI_COMPANION_DRIVER_H

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <lists/file_list.h>
#include <lists/string_list.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../command.h"

/* A desktop ("WIMP") companion backend is available in this build.
 * The Qt companion, the native Win32 companion (any desktop Windows
 * target) and the native Cocoa companion (macOS) all count. This
 * condition must be kept in sync with the guards in
 * settings/settings_def_desktop_menu.h, which cannot include this
 * header. */
#if defined(HAVE_QT) || defined(HAVE_COCOA) || \
      (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))
#define HAVE_COMPANION_WIMP 1
#endif

RETRO_BEGIN_DECLS

enum ui_msg_window_buttons
{
   UI_MSG_WINDOW_OK = 0,
   UI_MSG_WINDOW_OKCANCEL,
   UI_MSG_WINDOW_YESNO,
   UI_MSG_WINDOW_YESNOCANCEL
};

enum ui_msg_window_response
{
   UI_MSG_RESPONSE_NA = 0,
   UI_MSG_RESPONSE_OK,
   UI_MSG_RESPONSE_CANCEL,
   UI_MSG_RESPONSE_YES,
   UI_MSG_RESPONSE_NO
};

enum ui_msg_window_type
{
    UI_MSG_WINDOW_TYPE_ERROR = 0,
    UI_MSG_WINDOW_TYPE_INFORMATION,
    UI_MSG_WINDOW_TYPE_QUESTION,
    UI_MSG_WINDOW_TYPE_WARNING
};

enum uico_driver_state_flags
{
   UICO_ST_FLAG_WIMP_IS_INITED   = (1 << 0),
   UICO_ST_FLAG_IS_ON_FOREGROUND = (1 << 1)
};

typedef struct ui_msg_window_state
{
   enum ui_msg_window_buttons buttons;
   char *text;
   char *title;
   void *window;
} ui_msg_window_state;

typedef struct ui_browser_window_state
{
   void *window;
   char *filters;
   char *filters_title;
   char *startdir;
   char *path;
   char *title;
   char *result;
} ui_browser_window_state_t;

typedef struct ui_browser_window
{
   bool (*open)(ui_browser_window_state_t *state);
   bool (*save)(ui_browser_window_state_t *state);
   const char *ident;
} ui_browser_window_t;

typedef struct ui_msg_window
{
   enum ui_msg_window_response (*error      )(ui_msg_window_state *state);
   enum ui_msg_window_response (*information)(ui_msg_window_state *state);
   enum ui_msg_window_response (*question   )(ui_msg_window_state *state);
   enum ui_msg_window_response (*warning    )(ui_msg_window_state *state);
   const char *ident;
} ui_msg_window_t;

typedef struct ui_application
{
   void* (*initialize)(void);
   void (*process_events)(void);
   void (*quit)(void);
   bool exiting;
   const char *ident;
} ui_application_t;

typedef struct ui_window
{
   void* (*init)(void);
   void (*destroy)(void *data);
   void (*set_focused)(void *data);
   void (*set_visible)(void *data, bool visible);
   void (*set_title)(void *data, char *buf);
   void (*set_droppable)(void *data, bool droppable);
   bool (*focused)(void *data);
   const char *ident;
} ui_window_t;

typedef struct ui_companion_driver
{
   void *(*init)(void);
   void (*deinit)(void *data);
   void (*toggle)(void *data, bool force);
   /* Per-frame hook for desktop companion drivers that do not own the
    * platform event pump (the native Win32 / Cocoa companions). Called
    * once per runloop iteration while the driver is initialised. Must
    * be bounded and never block. */
   void (*iterate)(void *data);
   void (*event_command)(void *data, enum event_command action);
   void (*notify_refresh)(void *data);
   void (*msg_queue_push)(void *data, const char *msg, unsigned priority, unsigned duration, bool flush);
   void (*render_messagebox)(const char *msg);
   void *(*get_main_window)(void *data);
   void (*log_msg)(void *data, const char *msg);
   bool (*is_active)(void *data);
   struct string_list *(*get_app_icons)(void);
   void (*set_app_icon)(const char *icon);
   uintptr_t (*get_app_icon_texture)(const char *icon);
   ui_browser_window_t *browser_window;
   ui_msg_window_t     *msg_window;
   ui_window_t         *window;
   ui_application_t    *application;
   const char        *ident;
} ui_companion_driver_t;

typedef struct
{
   /* Platform driver: OS glue (message pump, message boxes, file
    * browser, window handling). Always present; selected by platform. */
   const ui_companion_driver_t *drv;
   void *data;
#ifdef HAVE_COMPANION_WIMP
   /* Desktop companion ("WIMP") driver: the playlist / content browser
    * window. Selected by the ui_companion_driver setting, enabled by
    * desktop_menu_enable. Layered on top of the platform driver: with
    * the Qt companion on Windows / macOS the platform driver keeps
    * doing exactly what it does today. */
   const ui_companion_driver_t *wimp;
   void *wimp_data;
#endif
   uint8_t flags;
} uico_driver_state_t;

uint8_t ui_companion_get_flags(void);

void ui_companion_event_command(enum event_command action);

void ui_companion_driver_notify_refresh(void);

const ui_msg_window_t *ui_companion_driver_get_msg_window_ptr(void);

const ui_browser_window_t *ui_companion_driver_get_browser_window_ptr(void);

const ui_window_t *ui_companion_driver_get_window_ptr(void);

void ui_companion_driver_log_msg(const char *msg);

void *ui_companion_driver_get_main_window(void);

const char *ui_companion_driver_get_ident(void);

void ui_companion_driver_init_first(
      bool desktop_menu_enable,
      bool ui_companion_toggle,
      unsigned ui_companion_start_on_boot
      );

void ui_companion_driver_msg_queue_push(
      const char *msg, unsigned priority,
      unsigned duration, bool flush);

void ui_companion_driver_deinit(void);

/* Tear down just the desktop companion window (safe to call more than
 * once). Called at quit before the drivers go away, so the window is
 * gone while its own thread's message pump is still running - otherwise
 * a native companion window outlives the pump and the process hangs
 * with an undestroyed window. */
void ui_companion_driver_wimp_deinit(void);

void ui_companion_driver_toggle(
      bool desktop_menu_enable,
      bool ui_companion_toggle,
      bool force);

uico_driver_state_t *uico_state_get_ptr(void);

/* True when a desktop companion with a log view is open. verbosity.c
 * asks this before formatting a copy of a log line and only then calls
 * ui_companion_driver_log_msg(), which delivers without re-checking. */
bool ui_companion_driver_log_active(void);

/* Per-frame hook for the desktop companion driver; call once per
 * runloop iteration from the platform's main loop. */
void ui_companion_driver_wimp_iterate(void);

/* True when the active desktop companion's toolkit application has
 * asked the process to exit (Qt sets this when its last window closes
 * with quit-on-close; the native companions never do - they only hide).
 * The main loops treat it like runloop_iterate() returning -1. */
bool ui_companion_driver_wimp_exiting(void);

/* Tell the active desktop companion's toolkit application to quit
 * (Qt: QApplication::quit). No-op for companions without one. Called by
 * the main loops right before they break out on shutdown. */
void ui_companion_driver_wimp_quit(void);

/* Desktop companion driver selection (Settings -> Drivers ->
 * Companion UI). */
const ui_companion_driver_t *ui_companion_wimp_find_driver(const char *ident);
const char *ui_companion_wimp_find_ident(int idx);
/* Space-separated list of available desktop companion driver idents. */
const char *config_get_ui_companion_driver_options(void);
const char *config_get_default_ui_companion(void);

extern ui_companion_driver_t ui_companion_cocoa;
extern ui_companion_driver_t ui_companion_cocoatouch;
extern ui_companion_driver_t ui_companion_qt;
extern ui_companion_driver_t ui_companion_win32;
/* Native desktop companions (shared core + native controls). */
extern ui_companion_driver_t ui_companion_wimp_win32;
extern ui_companion_driver_t ui_companion_wimp_cocoa;

extern ui_msg_window_t ui_msg_window_win32;


RETRO_END_DECLS

#endif
