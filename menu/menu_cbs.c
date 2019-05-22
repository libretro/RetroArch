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

#include <compat/strl.h>
#include <string/stdstring.h>

#include "menu_driver.h"
#include "menu_cbs.h"
#include "../verbosity.h"

#if 0
#define DEBUG_LOG
#endif

static void menu_cbs_init_log(const char *entry_label, const char *bind_label, const char *label)
{
#ifdef DEBUG_LOG
   if (!string_is_empty(label))
      RARCH_LOG("[%s]\t\t\tFound %s bind : [%s]\n", entry_label, bind_label, label);
#endif
}

struct key_desc key_descriptors[RARCH_MAX_KEYS] =
{
   {RETROK_FIRST,         "Unmapped"},
   {RETROK_BACKSPACE,     "Backspace"},
   {RETROK_TAB,           "Tab"},
   {RETROK_CLEAR,         "Clear"},
   {RETROK_RETURN,        "Return"},
   {RETROK_PAUSE,         "Pause"},
   {RETROK_ESCAPE,        "Escape"},
   {RETROK_SPACE,         "Space"},
   {RETROK_EXCLAIM,       "!"},
   {RETROK_QUOTEDBL,      "\""},
   {RETROK_HASH,          "#"},
   {RETROK_DOLLAR,        "$"},
   {RETROK_AMPERSAND,     "&"},
   {RETROK_QUOTE,         "\'"},
   {RETROK_LEFTPAREN,     "("},
   {RETROK_RIGHTPAREN,    ")"},
   {RETROK_ASTERISK,      "*"},
   {RETROK_PLUS,          "+"},
   {RETROK_COMMA,         ","},
   {RETROK_MINUS,         "-"},
   {RETROK_PERIOD,        "."},
   {RETROK_SLASH,         "/"},
   {RETROK_0,             "0"},
   {RETROK_1,             "1"},
   {RETROK_2,             "2"},
   {RETROK_3,             "3"},
   {RETROK_4,             "4"},
   {RETROK_5,             "5"},
   {RETROK_6,             "6"},
   {RETROK_7,             "7"},
   {RETROK_8,             "8"},
   {RETROK_9,             "9"},
   {RETROK_COLON,         ":"},
   {RETROK_SEMICOLON,     ";"},
   {RETROK_LESS,          "-"},
   {RETROK_EQUALS,        "="},
   {RETROK_GREATER,        ">"},
   {RETROK_QUESTION,      "?"},
   {RETROK_AT,            "@"},
   {RETROK_LEFTBRACKET,   "["},
   {RETROK_BACKSLASH,     "\\"},
   {RETROK_RIGHTBRACKET,  "]"},
   {RETROK_CARET,         "^"},
   {RETROK_UNDERSCORE,    "_"},
   {RETROK_BACKQUOTE,     "`"},
   {RETROK_a,             "a"},
   {RETROK_b,             "b"},
   {RETROK_c,             "c"},
   {RETROK_d,             "d"},
   {RETROK_e,             "e"},
   {RETROK_f,             "f"},
   {RETROK_g,             "g"},
   {RETROK_h,             "h"},
   {RETROK_i,             "i"},
   {RETROK_j,             "j"},
   {RETROK_k,             "k"},
   {RETROK_l,             "l"},
   {RETROK_m,             "m"},
   {RETROK_n,             "n"},
   {RETROK_o,             "o"},
   {RETROK_p,             "p"},
   {RETROK_q,             "q"},
   {RETROK_r,             "r"},
   {RETROK_s,             "s"},
   {RETROK_t,             "t"},
   {RETROK_u,             "u"},
   {RETROK_v,             "v"},
   {RETROK_w,             "w"},
   {RETROK_x,             "x"},
   {RETROK_y,             "y"},
   {RETROK_z,             "z"},
   {RETROK_DELETE,        "Delete"},

   {RETROK_KP0,           "Numpad 0"},
   {RETROK_KP1,           "Numpad 1"},
   {RETROK_KP2,           "Numpad 2"},
   {RETROK_KP3,           "Numpad 3"},
   {RETROK_KP4,           "Numpad 4"},
   {RETROK_KP5,           "Numpad 5"},
   {RETROK_KP6,           "Numpad 6"},
   {RETROK_KP7,           "Numpad 7"},
   {RETROK_KP8,           "Numpad 8"},
   {RETROK_KP9,           "Numpad 9"},
   {RETROK_KP_PERIOD,     "Numpad ."},
   {RETROK_KP_DIVIDE,     "Numpad /"},
   {RETROK_KP_MULTIPLY,   "Numpad *"},
   {RETROK_KP_MINUS,       "Numpad -"},
   {RETROK_KP_PLUS,        "Numpad +"},
   {RETROK_KP_ENTER,       "Numpad Enter"},
   {RETROK_KP_EQUALS,      "Numpad ="},

   {RETROK_UP,             "Up"},
   {RETROK_DOWN,           "Down"},
   {RETROK_RIGHT,          "Right"},
   {RETROK_LEFT,           "Left"},
   {RETROK_INSERT,         "Insert"},
   {RETROK_HOME,           "Home"},
   {RETROK_END,            "End"},
   {RETROK_PAGEUP,         "Page Up"},
   {RETROK_PAGEDOWN,       "Page Down"},

   {RETROK_F1,             "F1"},
   {RETROK_F2,             "F2"},
   {RETROK_F3,             "F3"},
   {RETROK_F4,             "F4"},
   {RETROK_F5,             "F5"},
   {RETROK_F6,             "F6"},
   {RETROK_F7,             "F7"},
   {RETROK_F8,             "F8"},
   {RETROK_F9,             "F9"},
   {RETROK_F10,            "F10"},
   {RETROK_F11,            "F11"},
   {RETROK_F12,            "F12"},
   {RETROK_F13,            "F13"},
   {RETROK_F14,            "F14"},
   {RETROK_F15,            "F15"},

   {RETROK_NUMLOCK,        "Num Lock"},
   {RETROK_CAPSLOCK,       "Caps Lock"},
   {RETROK_SCROLLOCK,      "Scroll Lock"},
   {RETROK_RSHIFT,         "Right Shift"},
   {RETROK_LSHIFT,         "Left Shift"},
   {RETROK_RCTRL,          "Right Control"},
   {RETROK_LCTRL,          "Left Control"},
   {RETROK_RALT,           "Right Alt"},
   {RETROK_LALT,           "Left Alt"},
   {RETROK_RMETA,          "Right Meta"},
   {RETROK_LMETA,          "Left Meta"},
   {RETROK_LSUPER,         "Right Super"},
   {RETROK_RSUPER,         "Left Super"},
   {RETROK_MODE,           "Mode"},
   {RETROK_COMPOSE,        "Compose"},

   {RETROK_HELP,           "Help"},
   {RETROK_PRINT,          "Print"},
   {RETROK_SYSREQ,         "Sys Req"},
   {RETROK_BREAK,          "Break"},
   {RETROK_MENU,           "Menu"},
   {RETROK_POWER,          "Power"},
   {RETROK_EURO,           {-30, -126, -84, 0}}, /* "€" */
   {RETROK_UNDO,           "Undo"},
   {RETROK_OEM_102,        "OEM-102"}
};

/* This sets up all the callback functions for a menu entry.
 *
 * OK     : When we press the 'OK' button on an entry.
 * Cancel : When we press the 'Cancel' button on an entry.
 * Scan   : When we press the 'Scan' button on an entry.
 * Start  : When we press the 'Start' button on an entry.
 * Select : When we press the 'Select' button on an entry.
 * Info   : When we press the 'Info' button on an entry.
 * Content Switch   : ??? (TODO/FIXME - Kivutar should document this)
 * Up     : when we press 'Up' on the D-pad while this entry is selected.
 * Down   : when we press 'Down' on the D-pad while this entry is selected.
 * Left   : when we press 'Left' on the D-pad while this entry is selected.
 * Right  : when we press 'Right' on the D-pad while this entry is selected.
 * Deferred push : When pressing an entry results in spawning a new list, it waits until the next
 * frame to push this onto the stack. This function callback will then be invoked.
 * Refresh : What happens when the screen has to be refreshed. Does an entry have internal state
 * that needs to be rebuild?
 * Get value: Each entry has associated 'text', which we call the value. This function callback
 * lets us render that text.
 * Get title: Each entry can have a custom 'title'.
 * Label: Each entry has a label name. This function callback lets us render that label text.
 * Sublabel: each entry has a sublabel, which consists of one or more lines of additional information.
 * This function callback lets us render that text.
 */
void menu_cbs_init(void *data,
      menu_file_list_cbs_t *cbs,
      const char *path, const char *label,
      unsigned type, size_t idx)
{
   menu_ctx_bind_t bind_info;
   const char *repr_label        = NULL;
   const char *menu_label        = NULL;
   uint32_t label_hash           = 0;
   uint32_t menu_label_hash      = 0;
   enum msg_hash_enums enum_idx  = MSG_UNKNOWN;
   file_list_t *list             = (file_list_t*)data;
   if (!list)
      return;

   menu_entries_get_last_stack(NULL, &menu_label, NULL, &enum_idx, NULL);

   if (!label || !menu_label)
      return;

   label_hash      = msg_hash_calculate(label);
   menu_label_hash = msg_hash_calculate(menu_label);

#ifdef DEBUG_LOG
   RARCH_LOG("\n");
#endif

   repr_label = (!string_is_empty(label)) ? label : path;

#ifdef DEBUG_LOG
   if (cbs && cbs->enum_idx != MSG_UNKNOWN)
      RARCH_LOG("\t\t\tenum_idx %d [%s]\n", cbs->enum_idx, msg_hash_to_str(cbs->enum_idx));
#endif

   /* It will try to find a corresponding callback function inside
    * menu_cbs_ok.c, then map this callback to the entry. */
   menu_cbs_init_bind_ok(cbs, path, label, type, idx, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "OK", cbs->action_ok_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_cancel.c, then map this callback to the entry. */
   menu_cbs_init_bind_cancel(cbs, path, label, type, idx);

   menu_cbs_init_log(repr_label, "CANCEL", cbs->action_cancel_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_scan.c, then map this callback to the entry. */
   menu_cbs_init_bind_scan(cbs, path, label, type, idx);

   menu_cbs_init_log(repr_label, "SCAN", cbs->action_scan_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_start.c, then map this callback to the entry. */
   menu_cbs_init_bind_start(cbs, path, label, type, idx);

   menu_cbs_init_log(repr_label, "START", cbs->action_start_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_select.c, then map this callback to the entry. */
   menu_cbs_init_bind_select(cbs, path, label, type, idx);

   menu_cbs_init_log(repr_label, "SELECT", cbs->action_select_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_info.c, then map this callback to the entry. */
   menu_cbs_init_bind_info(cbs, path, label, type, idx);

   menu_cbs_init_log(repr_label, "INFO", cbs->action_info_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_bind_content_list_switch.c, then map this callback to the entry. */
   menu_cbs_init_bind_content_list_switch(cbs, path, label, type, idx);

   menu_cbs_init_log(repr_label, "CONTENT SWITCH", cbs->action_content_list_switch_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_up.c, then map this callback to the entry. */
   menu_cbs_init_bind_up(cbs, path, label, type, idx);

   menu_cbs_init_log(repr_label, "UP", cbs->action_up_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_down.c, then map this callback to the entry. */
   menu_cbs_init_bind_down(cbs, path, label, type, idx);

   menu_cbs_init_log(repr_label, "DOWN", cbs->action_down_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_left.c, then map this callback to the entry. */
   menu_cbs_init_bind_left(cbs, path, label, type, idx, menu_label, label_hash);

   menu_cbs_init_log(repr_label, "LEFT", cbs->action_left_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_right.c, then map this callback to the entry. */
   menu_cbs_init_bind_right(cbs, path, label, type, idx, menu_label, label_hash);

   menu_cbs_init_log(repr_label, "RIGHT", cbs->action_right_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_deferred_push.c, then map this callback to the entry. */
   menu_cbs_init_bind_deferred_push(cbs, path, label, type, idx, label_hash);

   menu_cbs_init_log(repr_label, "DEFERRED PUSH", cbs->action_deferred_push_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_refresh.c, then map this callback to the entry. */
   menu_cbs_init_bind_refresh(cbs, path, label, type, idx);

   menu_cbs_init_log(repr_label, "REFRESH", cbs->action_refresh_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_get_string_representation.c, then map this callback to the entry. */
   menu_cbs_init_bind_get_string_representation(cbs, path, label, type, idx);

   menu_cbs_init_log(repr_label, "GET VALUE", cbs->action_get_value_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_title.c, then map this callback to the entry. */
   menu_cbs_init_bind_title(cbs, path, label, type, idx, label_hash);

   menu_cbs_init_log(repr_label, "GET TITLE", cbs->action_get_title_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_label.c, then map this callback to the entry. */
   menu_cbs_init_bind_label(cbs, path, label, type, idx);

   menu_cbs_init_log(repr_label, "LABEL", cbs->action_label_ident);

   /* It will try to find a corresponding callback function inside
    * menu_cbs_sublabel.c, then map this callback to the entry. */
   menu_cbs_init_bind_sublabel(cbs, path, label, type, idx);

   menu_cbs_init_log(repr_label, "SUBLABEL", cbs->action_sublabel_ident);

   bind_info.cbs             = cbs;
   bind_info.path            = path;
   bind_info.label           = label;
   bind_info.type            = type;
   bind_info.idx             = idx;
   bind_info.label_hash      = label_hash;

   menu_driver_ctl(RARCH_MENU_CTL_BIND_INIT, &bind_info);
}

/* Pretty much a stub function. TODO/FIXME - Might as well remove this. */
int menu_cbs_exit(void)
{
   return -1;
}
