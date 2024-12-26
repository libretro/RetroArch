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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../command.h"

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
   UICO_ST_FLAG_QT_IS_INITED     = (1 << 0),
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
   void (*event_command)(void *data, enum event_command action);
   void (*notify_refresh)(void *data);
   void (*msg_queue_push)(void *data, const char *msg, unsigned priority, unsigned duration, bool flush);
   void (*render_messagebox)(const char *msg);
   void *(*get_main_window)(void *data);
   void (*log_msg)(void *data, const char *msg);
   bool (*is_active)(void *data);
   ui_browser_window_t *browser_window;
   ui_msg_window_t     *msg_window;
   ui_window_t         *window;
   ui_application_t    *application;
   const char        *ident;
} ui_companion_driver_t;

typedef struct
{
   const ui_companion_driver_t *drv;
   void *data;
#ifdef HAVE_QT
   void *qt_data;
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

void ui_companion_driver_init_first(void);

void ui_companion_driver_msg_queue_push(
      const char *msg, unsigned priority,
      unsigned duration, bool flush);

void ui_companion_driver_deinit(void);

void ui_companion_driver_toggle(
      bool desktop_menu_enable,
      bool ui_companion_toggle,
      bool force);

uico_driver_state_t *uico_state_get_ptr(void);

extern ui_companion_driver_t ui_companion_cocoa;
extern ui_companion_driver_t ui_companion_qt;
extern ui_companion_driver_t ui_companion_win32;

extern ui_msg_window_t ui_msg_window_win32;


RETRO_END_DECLS

#endif
