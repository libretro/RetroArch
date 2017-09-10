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

struct key_desc key_descriptors[192] = 
{
   {RETROK_FIRST         , 0, "Unmapped"},
   {RETROK_BACKSPACE     , 8, "Backspace"},
   {RETROK_TAB           , 9, "Tab"},
   {RETROK_CLEAR         , 12, "Clear"},
   {RETROK_RETURN        , 13, "Return"},
   {RETROK_PAUSE         , 19, "Pause"},
   {RETROK_ESCAPE        , 27, "Escape"},
   {RETROK_SPACE         , 32, "Space"},
   {RETROK_EXCLAIM       , 33, "!"},
   {RETROK_QUOTEDBL      , 34, "\""},
   {RETROK_HASH          , 35, "#"},
   {RETROK_DOLLAR        , 36, "$"},
   {RETROK_AMPERSAND     , 38, "&"},
   {RETROK_QUOTE         , 39, "\'"},
   {RETROK_LEFTPAREN     , 40, ")"},
   {RETROK_RIGHTPAREN    , 41, ")"},
   {RETROK_ASTERISK      , 42, "*"},
   {RETROK_PLUS          , 43, "+"},
   {RETROK_COMMA         , 44, ","},
   {RETROK_MINUS         , 45, "-"},
   {RETROK_PERIOD        , 46, "."},
   {RETROK_SLASH         , 47, "/"},
   {RETROK_0             , 48, "0"},
   {RETROK_1             , 49, "1"},
   {RETROK_2             , 50, "2"},
   {RETROK_3             , 51, "3"},
   {RETROK_4             , 52, "4"},
   {RETROK_5             , 53, "5"},
   {RETROK_6             , 54, "6"},
   {RETROK_7             , 55, "7"},
   {RETROK_8             , 56, "8"},
   {RETROK_9             , 57, "9"},
   {RETROK_COLON         , 58, ":"},
   {RETROK_SEMICOLON     , 59, ";"},
   {RETROK_LESS          , 60, "-"},
   {RETROK_EQUALS        , 61, "="},
   {RETROK_GREATER       , 62, ">"},
   {RETROK_QUESTION      , 63, "?"},
   {RETROK_AT            , 64, "@"},
   {RETROK_LEFTBRACKET   , 91, "["},
   {RETROK_BACKSLASH     , 92, "\\"},
   {RETROK_RIGHTBRACKET  , 93, "]"},
   {RETROK_CARET         , 94, "^"},
   {RETROK_UNDERSCORE    , 95, "_"},
   {RETROK_BACKQUOTE     , 96, "`"},
   {RETROK_a             , 97, "a"},
   {RETROK_b             , 98, "b"},
   {RETROK_c             , 99, "c"},
   {RETROK_d             , 100, "d"},
   {RETROK_e             , 101, "e"},
   {RETROK_f             , 102, "f"},
   {RETROK_g             , 103, "g"},
   {RETROK_h             , 104, "h"},
   {RETROK_i             , 105, "i"},
   {RETROK_j             , 106, "j"},
   {RETROK_k             , 107, "k"},
   {RETROK_l             , 108, "l"},
   {RETROK_m             , 109, "m"},
   {RETROK_n             , 110, "n"},
   {RETROK_o             , 111, "o"},
   {RETROK_p             , 112, "p"},
   {RETROK_q             , 113, "q"},
   {RETROK_r             , 114, "r"},
   {RETROK_s             , 115, "s"},
   {RETROK_t             , 116, "t"},
   {RETROK_u             , 117, "u"},
   {RETROK_v             , 118, "v"},
   {RETROK_w             , 119, "w"},
   {RETROK_x             , 120, "x"},
   {RETROK_y             , 121, "y"},
   {RETROK_z             , 122, "z"},
   {RETROK_DELETE        , 127, "Delete"},
   
   {RETROK_KP0           , 256, "Numpad 0"},
   {RETROK_KP1           , 257, "Numpad 1"},
   {RETROK_KP2           , 258, "Numpad 2"},
   {RETROK_KP3           , 259, "Numpad 3"},
   {RETROK_KP4           , 260, "Numpad 4"},
   {RETROK_KP5           , 261, "Numpad 5"},
   {RETROK_KP6           , 262, "Numpad 6"},
   {RETROK_KP7           , 263, "Numpad 7"},
   {RETROK_KP8           , 264, "Numpad 8"},
   {RETROK_KP9           , 265, "Numpad 9"},
   {RETROK_KP_PERIOD     , 266, "Numpad ."},
   {RETROK_KP_DIVIDE     , 267, "Numpad /"},
   {RETROK_KP_MULTIPLY   , 268, "Numpad *"},
   {RETROK_KP_MINUS      , 269, "Numpad -"},
   {RETROK_KP_PLUS       , 270, "Numpad +"},
   {RETROK_KP_ENTER      , 271, "Numpad Enter"},
   {RETROK_KP_EQUALS     , 272, "Numpad ="},
   
   {RETROK_UP            , 273, "Up"},
   {RETROK_DOWN          , 274, "Down"},
   {RETROK_RIGHT         , 275, "Right"},
   {RETROK_LEFT          , 276, "Left"},
   {RETROK_INSERT        , 277, "Insert"},
   {RETROK_HOME          , 278, "Home"},
   {RETROK_END           , 279, "End"},
   {RETROK_PAGEUP        , 280, "Page Up"},
   {RETROK_PAGEDOWN      , 281, "Page Down"},
   
   {RETROK_F1            , 282, "F1"},
   {RETROK_F2            , 283, "F2"},
   {RETROK_F3            , 284, "F3"},
   {RETROK_F4            , 285, "F4"},
   {RETROK_F5            , 286, "F5"},
   {RETROK_F6            , 287, "F6"},
   {RETROK_F7            , 288, "F7"},
   {RETROK_F8            , 289, "F8"},
   {RETROK_F9            , 290, "F9"},
   {RETROK_F10           , 291, "F10"},
   {RETROK_F11           , 292, "F11"},
   {RETROK_F12           , 293, "F12"},
   {RETROK_F13           , 294, "F13"},
   {RETROK_F14           , 295, "F14"},
   {RETROK_F15           , 296, "F15"},
   
   {RETROK_NUMLOCK       , 300, "Num Lock"},
   {RETROK_CAPSLOCK      , 301, "Caps Lock"},
   {RETROK_SCROLLOCK     , 302, "Scroll Lock"},
   {RETROK_RSHIFT        , 303, "Right Shift"},
   {RETROK_LSHIFT        , 304, "Left Shift"},
   {RETROK_RCTRL         , 305, "Right Control"},
   {RETROK_LCTRL         , 306, "Left Control"},
   {RETROK_RALT          , 307, "Right Alt"},
   {RETROK_LALT          , 308, "Left Alt"},
   {RETROK_RMETA         , 309, "Right Meta"},
   {RETROK_LMETA         , 310, "Left Meta"},
   {RETROK_LSUPER        , 311, "Right Super"},
   {RETROK_RSUPER        , 312, "Left Super"},
   {RETROK_MODE          , 313, "Mode"},
   {RETROK_COMPOSE       , 314, "Compose"},
   
   {RETROK_HELP          , 315, "Help"},
   {RETROK_PRINT         , 316, "Print"},
   {RETROK_SYSREQ        , 317, "Sys Req"},
   {RETROK_BREAK         , 318, "Break"},
   {RETROK_MENU          , 319, "Menu"},
   {RETROK_POWER         , 320, "Power"},
   {RETROK_EURO          , 321, "€"},
   {RETROK_UNDO          , 322, "Undo"}
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
