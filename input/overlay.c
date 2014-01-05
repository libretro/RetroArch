/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "overlay.h"
#include "../general.h"
#include "../driver.h"
#include "../libretro.h"
#include "../gfx/image.h"
#include "../conf/config_file.h"
#include "../compat/posix_string.h"
#include "input_common.h"
#include "../file.h"
#include <stddef.h>
#include <math.h>

enum overlay_hitbox
{
   OVERLAY_HITBOX_RADIAL = 0,
   OVERLAY_HITBOX_RECT
};

enum overlay_type
{
   OVERLAY_TYPE_BUTTONS = 0,
   OVERLAY_TYPE_ANALOG_LEFT,
   OVERLAY_TYPE_ANALOG_RIGHT,
   OVERLAY_TYPE_KEYBOARD
};

struct overlay_desc
{
   float x;
   float y;

   enum overlay_hitbox hitbox;
   float range_x, range_y;
   float range_x_mod, range_y_mod;
   float mod_x, mod_y, mod_w, mod_h;
   float delta_x, delta_y;

   enum overlay_type type;
   uint64_t key_mask;
   float analog_saturate_pct;

   unsigned next_index;
   char next_index_name[64];

   struct texture_image image;
   unsigned image_index;

   float alpha_mod;
   float range_mod;

   bool updated;
   bool movable;
};

struct overlay
{
   struct overlay_desc *descs;
   size_t size;

   struct texture_image image;

   bool block_scale;
   float mod_x, mod_y, mod_w, mod_h;
   float x, y, w, h;
   float scale;
   float center_x, center_y;

   bool full_screen;

   char name[64];

   struct texture_image *load_images;
   unsigned load_images_size;
};

struct input_overlay
{
   void *iface_data;
   const video_overlay_interface_t *iface;
   bool enable;

   bool blocked;

   struct overlay *overlays;
   const struct overlay *active;
   size_t index;
   size_t size;

   unsigned next_index;
   char *overlay_path;
};

struct str_to_bind_map
{
   const char *str;
   unsigned bind;
};

static const struct str_to_bind_map str_to_bind[] = {
   { "b",                     RETRO_DEVICE_ID_JOYPAD_B },
   { "y",                     RETRO_DEVICE_ID_JOYPAD_Y },
   { "select",                RETRO_DEVICE_ID_JOYPAD_SELECT },
   { "start",                 RETRO_DEVICE_ID_JOYPAD_START },
   { "up",                    RETRO_DEVICE_ID_JOYPAD_UP },
   { "down",                  RETRO_DEVICE_ID_JOYPAD_DOWN },
   { "left",                  RETRO_DEVICE_ID_JOYPAD_LEFT },
   { "right",                 RETRO_DEVICE_ID_JOYPAD_RIGHT },
   { "a",                     RETRO_DEVICE_ID_JOYPAD_A },
   { "x",                     RETRO_DEVICE_ID_JOYPAD_X },
   { "l",                     RETRO_DEVICE_ID_JOYPAD_L },
   { "r",                     RETRO_DEVICE_ID_JOYPAD_R },
   { "l2",                    RETRO_DEVICE_ID_JOYPAD_L2 },
   { "r2",                    RETRO_DEVICE_ID_JOYPAD_R2 },
   { "l3",                    RETRO_DEVICE_ID_JOYPAD_L3 },
   { "r3",                    RETRO_DEVICE_ID_JOYPAD_R3 },
   { "turbo",                 RARCH_TURBO_ENABLE },
   { "l_x_plus",              RARCH_ANALOG_LEFT_X_PLUS },
   { "l_x_minus",             RARCH_ANALOG_LEFT_X_MINUS },
   { "l_y_plus",              RARCH_ANALOG_LEFT_Y_PLUS },
   { "l_y_minus",             RARCH_ANALOG_LEFT_Y_MINUS },
   { "r_x_plus",              RARCH_ANALOG_RIGHT_X_PLUS },
   { "r_x_minus",             RARCH_ANALOG_RIGHT_X_MINUS },
   { "r_y_plus",              RARCH_ANALOG_RIGHT_Y_PLUS },
   { "r_y_minus",             RARCH_ANALOG_RIGHT_Y_MINUS },
   { "toggle_fast_forward",   RARCH_FAST_FORWARD_KEY },
   { "hold_fast_forward",     RARCH_FAST_FORWARD_HOLD_KEY },
   { "load_state",            RARCH_LOAD_STATE_KEY },
   { "save_state",            RARCH_SAVE_STATE_KEY },
   { "toggle_fullscreen",     RARCH_FULLSCREEN_TOGGLE_KEY },
   { "exit_emulator",         RARCH_QUIT_KEY },
   { "state_slot_increase",   RARCH_STATE_SLOT_PLUS },
   { "state_slot_decrease",   RARCH_STATE_SLOT_MINUS },
   { "rewind",                RARCH_REWIND },
   { "movie_record_toggle",   RARCH_MOVIE_RECORD_TOGGLE },
   { "pause_toggle",          RARCH_PAUSE_TOGGLE },
   { "frame_advance",         RARCH_FRAMEADVANCE },
   { "reset",                 RARCH_RESET },
   { "shader_next",           RARCH_SHADER_NEXT },
   { "shader_prev",           RARCH_SHADER_PREV },
   { "cheat_index_plus",      RARCH_CHEAT_INDEX_PLUS },
   { "cheat_index_minus",     RARCH_CHEAT_INDEX_MINUS },
   { "cheat_toggle",          RARCH_CHEAT_TOGGLE },
   { "screenshot",            RARCH_SCREENSHOT },
   { "dsp_config",            RARCH_DSP_CONFIG },
   { "audio_mute",            RARCH_MUTE },
   { "netplay_flip_players",  RARCH_NETPLAY_FLIP },
   { "slowmotion",            RARCH_SLOWMOTION },
   { "enable_hotkey",         RARCH_ENABLE_HOTKEY },
   { "volume_up",             RARCH_VOLUME_UP },
   { "volume_down",           RARCH_VOLUME_DOWN },
   { "overlay_next",          RARCH_OVERLAY_NEXT },
   { "disk_eject_toggle",     RARCH_DISK_EJECT_TOGGLE },
   { "disk_next",             RARCH_DISK_NEXT },
   { "grab_mouse_toggle",     RARCH_GRAB_MOUSE_TOGGLE },
   { "menu_toggle",           RARCH_MENU_TOGGLE },
};

static unsigned input_str_to_bind(const char *str)
{
   unsigned i;
   for (i = 0; i < ARRAY_SIZE(str_to_bind); i++)
   {
      if (!strcmp(str_to_bind[i].str, str))
         return str_to_bind[i].bind;
   }

   return RARCH_BIND_LIST_END;
}

static const struct str_to_bind_map str_to_keyboard[] = {
   { "retrok_backspace",      RETROK_BACKSPACE },
   { "retrok_tab",            RETROK_TAB },
   { "retrok_clear",          RETROK_CLEAR },
   { "retrok_return",         RETROK_RETURN },
   { "retrok_pause",          RETROK_PAUSE },
   { "retrok_escape",         RETROK_ESCAPE },
   { "retrok_space",          RETROK_SPACE },
   { "retrok_exclaim",        RETROK_EXCLAIM },
   { "retrok_quotedbl",       RETROK_QUOTEDBL },
   { "retrok_hash",           RETROK_HASH },
   { "retrok_dollar",         RETROK_DOLLAR },
   { "retrok_ampersand",      RETROK_AMPERSAND },
   { "retrok_quote",          RETROK_QUOTE },
   { "retrok_leftparen",      RETROK_LEFTPAREN },
   { "retrok_rightparen",     RETROK_RIGHTPAREN },
   { "retrok_asterisk",       RETROK_ASTERISK },
   { "retrok_plus",           RETROK_PLUS },
   { "retrok_comma",          RETROK_COMMA },
   { "retrok_minus",          RETROK_MINUS },
   { "retrok_period",         RETROK_PERIOD },
   { "retrok_slash",          RETROK_SLASH },
   { "retrok_0",              RETROK_0 },
   { "retrok_1",              RETROK_1 },
   { "retrok_2",              RETROK_2 },
   { "retrok_3",              RETROK_3 },
   { "retrok_4",              RETROK_4 },
   { "retrok_5",              RETROK_5 },
   { "retrok_6",              RETROK_6 },
   { "retrok_7",              RETROK_7 },
   { "retrok_8",              RETROK_8 },
   { "retrok_9",              RETROK_9 },
   { "retrok_colon",          RETROK_COLON },
   { "retrok_semicolon",      RETROK_SEMICOLON },
   { "retrok_less",           RETROK_LESS },
   { "retrok_equals",         RETROK_EQUALS },
   { "retrok_greater",        RETROK_GREATER },
   { "retrok_question",       RETROK_QUESTION },
   { "retrok_at",             RETROK_AT },
   { "retrok_leftbracket",    RETROK_LEFTBRACKET },
   { "retrok_backslash",      RETROK_BACKSLASH },
   { "retrok_rightbracket",   RETROK_RIGHTBRACKET },
   { "retrok_caret",          RETROK_CARET },
   { "retrok_underscore",     RETROK_UNDERSCORE },
   { "retrok_backquote",      RETROK_BACKQUOTE },
   { "retrok_a",              RETROK_a },
   { "retrok_b",              RETROK_b },
   { "retrok_c",              RETROK_c },
   { "retrok_d",              RETROK_d },
   { "retrok_e",              RETROK_e },
   { "retrok_f",              RETROK_f },
   { "retrok_g",              RETROK_g },
   { "retrok_h",              RETROK_h },
   { "retrok_i",              RETROK_i },
   { "retrok_j",              RETROK_j },
   { "retrok_k",              RETROK_k },
   { "retrok_l",              RETROK_l },
   { "retrok_m",              RETROK_m },
   { "retrok_n",              RETROK_n },
   { "retrok_o",              RETROK_o },
   { "retrok_p",              RETROK_p },
   { "retrok_q",              RETROK_q },
   { "retrok_r",              RETROK_r },
   { "retrok_s",              RETROK_s },
   { "retrok_t",              RETROK_t },
   { "retrok_u",              RETROK_u },
   { "retrok_v",              RETROK_v },
   { "retrok_w",              RETROK_w },
   { "retrok_x",              RETROK_x },
   { "retrok_y",              RETROK_y },
   { "retrok_z",              RETROK_z },
   { "retrok_delete",         RETROK_DELETE },

   { "retrok_kp0",            RETROK_KP0 },
   { "retrok_kp1",            RETROK_KP1 },
   { "retrok_kp2",            RETROK_KP2 },
   { "retrok_kp3",            RETROK_KP3 },
   { "retrok_kp4",            RETROK_KP4 },
   { "retrok_kp5",            RETROK_KP5 },
   { "retrok_kp6",            RETROK_KP6 },
   { "retrok_kp7",            RETROK_KP7 },
   { "retrok_kp8",            RETROK_KP8 },
   { "retrok_kp9",            RETROK_KP9 },
   { "retrok_kp_period",      RETROK_KP_PERIOD },
   { "retrok_kp_divide",      RETROK_KP_DIVIDE },
   { "retrok_kp_multiply",    RETROK_KP_MULTIPLY },
   { "retrok_kp_minus",       RETROK_KP_MINUS },
   { "retrok_kp_plus",        RETROK_KP_PLUS },
   { "retrok_kp_enter",       RETROK_KP_ENTER },
   { "retrok_kp_equals",      RETROK_KP_EQUALS },

   { "retrok_up",             RETROK_UP },
   { "retrok_down",           RETROK_DOWN },
   { "retrok_right",          RETROK_RIGHT },
   { "retrok_left",           RETROK_LEFT },
   { "retrok_insert",         RETROK_INSERT },
   { "retrok_home",           RETROK_HOME },
   { "retrok_end",            RETROK_END },
   { "retrok_pageup",         RETROK_PAGEUP },
   { "retrok_pagedown",       RETROK_PAGEDOWN },

   { "retrok_f1",             RETROK_F1 },
   { "retrok_f2",             RETROK_F2 },
   { "retrok_f3",             RETROK_F3 },
   { "retrok_f4",             RETROK_F4 },
   { "retrok_f5",             RETROK_F5 },
   { "retrok_f6",             RETROK_F6 },
   { "retrok_f7",             RETROK_F7 },
   { "retrok_f8",             RETROK_F8 },
   { "retrok_f9",             RETROK_F9 },
   { "retrok_f10",            RETROK_F10 },
   { "retrok_f11",            RETROK_F11 },
   { "retrok_f12",            RETROK_F12 },
   { "retrok_f13",            RETROK_F13 },
   { "retrok_f14",            RETROK_F14 },
   { "retrok_f15",            RETROK_F15 },

   { "retrok_numlock",        RETROK_NUMLOCK },
   { "retrok_capslock",       RETROK_CAPSLOCK },
   { "retrok_scrollock",      RETROK_SCROLLOCK },
   { "retrok_rshift",         RETROK_RSHIFT },
   { "retrok_lshift",         RETROK_LSHIFT },
   { "retrok_rctrl",          RETROK_RCTRL },
   { "retrok_lctrl",          RETROK_LCTRL },
   { "retrok_ralt",           RETROK_RALT },
   { "retrok_lalt",           RETROK_LALT },
   { "retrok_rmeta",          RETROK_RMETA },
   { "retrok_lmeta",          RETROK_LMETA },
   { "retrok_lsuper",         RETROK_LSUPER },
   { "retrok_rsuper",         RETROK_RSUPER },
   { "retrok_mode",           RETROK_MODE },
   { "retrok_compose",        RETROK_COMPOSE },

   { "retrok_help",           RETROK_HELP },
   { "retrok_print",          RETROK_PRINT },
   { "retrok_sysreq",         RETROK_SYSREQ },
   { "retrok_break",          RETROK_BREAK },
   { "retrok_menu",           RETROK_MENU },
   { "retrok_power",          RETROK_POWER },
   { "retrok_euro",           RETROK_EURO },
   { "retrok_undo",           RETROK_UNDO },
};

static unsigned input_str_to_keyboard(const char *str)
{
   unsigned i;
   for (i = 0; i < ARRAY_SIZE(str_to_keyboard); i++)
   {
      if (!strcmp(str_to_keyboard[i].str, str))
         return str_to_keyboard[i].bind;
   }

   RARCH_ERR("[Overlay] Unknown keyboard key: %s.\n", str);
   return RETROK_UNKNOWN;
}

static void input_overlay_scale(struct overlay *overlay, float scale)
{
   size_t i;
   if (overlay->block_scale)
      scale = 1.0f;

   overlay->scale = scale;
   overlay->mod_w = overlay->w * scale;
   overlay->mod_h = overlay->h * scale;
   overlay->mod_x = overlay->center_x + (overlay->x - overlay->center_x) * scale;
   overlay->mod_y = overlay->center_y + (overlay->y - overlay->center_y) * scale;

   for (i = 0; i < overlay->size; i++)
   {
      struct overlay_desc *desc = &overlay->descs[i];

      float scale_w = overlay->mod_w * desc->range_x;
      float scale_h = overlay->mod_h * desc->range_y;

      desc->mod_w = 2.0f * scale_w;
      desc->mod_h = 2.0f * scale_h;

      float adj_center_x = overlay->mod_x + desc->x * overlay->mod_w;
      float adj_center_y = overlay->mod_y + desc->y * overlay->mod_h;
      desc->mod_x = adj_center_x - scale_w;
      desc->mod_y = adj_center_y - scale_h;
   }
}

static void input_overlay_set_vertex_geom(input_overlay_t *ol)
{
   size_t i;
   if (ol->active->image.pixels)
      ol->iface->vertex_geom(ol->iface_data, 0,
            ol->active->mod_x, ol->active->mod_y, ol->active->mod_w, ol->active->mod_h);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];
      if (desc->image.pixels)
         ol->iface->vertex_geom(ol->iface_data, desc->image_index,
               desc->mod_x, desc->mod_y, desc->mod_w, desc->mod_h);
   }
}

void input_overlay_set_scale_factor(input_overlay_t *ol, float scale)
{
   size_t i;
   for (i = 0; i < ol->size; i++)
      input_overlay_scale(&ol->overlays[i], scale);

   input_overlay_set_vertex_geom(ol);
}

static void input_overlay_free_overlay(struct overlay *overlay)
{
   size_t i;
   for (i = 0; i < overlay->size; i++)
      texture_image_free(&overlay->descs[i].image);
   free(overlay->load_images);
   free(overlay->descs);
   texture_image_free(&overlay->image);
}

static void input_overlay_free_overlays(input_overlay_t *ol)
{
   size_t i;
   for (i = 0; i < ol->size; i++)
      input_overlay_free_overlay(&ol->overlays[i]);
   free(ol->overlays);
}

static bool input_overlay_load_desc(input_overlay_t *ol, config_file_t *conf, struct overlay_desc *desc,
      unsigned ol_index, unsigned desc_index,
      unsigned width, unsigned height,
      bool normalized, float alpha_mod, float range_mod)
{
   bool ret = true;
   char overlay_desc_key[64];
   snprintf(overlay_desc_key, sizeof(overlay_desc_key), "overlay%u_desc%u", ol_index, desc_index);

   char overlay_desc_image_key[64];
   snprintf(overlay_desc_image_key, sizeof(overlay_desc_image_key),
         "overlay%u_desc%u_overlay", ol_index, desc_index);

   char image_path[PATH_MAX];
   if (config_get_path(conf, overlay_desc_image_key, image_path, sizeof(image_path)))
   {
      char path[PATH_MAX];
      fill_pathname_resolve_relative(path, ol->overlay_path, image_path, sizeof(path));

      struct texture_image img = {0};
      if (texture_image_load(path, &img))
         desc->image = img;
   }

   char overlay_desc_normalized_key[64];
   snprintf(overlay_desc_normalized_key, sizeof(overlay_desc_normalized_key),
         "overlay%u_desc%u_normalized", ol_index, desc_index);
   config_get_bool(conf, overlay_desc_normalized_key, &normalized);
   bool by_pixel = !normalized;

   if (by_pixel && (width == 0 || height == 0))
   {
      RARCH_ERR("[Overlay]: Base overlay is not set and not using normalized coordinates.\n");
      return false;
   }

   char overlay[256];
   if (!config_get_array(conf, overlay_desc_key, overlay, sizeof(overlay)))
   {
      RARCH_ERR("[Overlay]: Didn't find key: %s.\n", overlay_desc_key);
      return false;
   }

   struct string_list *list = string_split(overlay, ", ");
   if (!list)
   {
      RARCH_ERR("[Overlay]: Failed to split overlay desc.\n");
      return false;
   }

   if (list->size < 6)
   {
      string_list_free(list);
      RARCH_ERR("[Overlay]: Overlay desc is invalid. Requires at least 6 tokens.\n");
      return false;
   }

   const char *x   = list->elems[1].data;
   const char *y   = list->elems[2].data;
   const char *box = list->elems[3].data;

   char *key = list->elems[0].data;
   char *save;
   desc->key_mask = 0;

   if (strcmp(key, "analog_left") == 0)
      desc->type = OVERLAY_TYPE_ANALOG_LEFT;
   else if (strcmp(key, "analog_right") == 0)
      desc->type = OVERLAY_TYPE_ANALOG_RIGHT;
   else if (strstr(key, "retrok_") == key)
   {
      desc->type = OVERLAY_TYPE_KEYBOARD;
      desc->key_mask = input_str_to_keyboard(key);
   }
   else
   {
      const char *tmp;
      desc->type = OVERLAY_TYPE_BUTTONS;
      for (tmp = strtok_r(key, "|", &save); tmp; tmp = strtok_r(NULL, "|", &save))
      {
         if (strcmp(tmp, "nul") != 0)
            desc->key_mask |= UINT64_C(1) << input_str_to_bind(tmp);
      }

      if (desc->key_mask & (UINT64_C(1) << RARCH_OVERLAY_NEXT))
      {
         char overlay_target_key[64];
         snprintf(overlay_target_key, sizeof(overlay_target_key), "overlay%u_desc%u_next_target", ol_index, desc_index);
         config_get_array(conf, overlay_target_key, desc->next_index_name, sizeof(desc->next_index_name));
      }
   }

   float width_mod = by_pixel ? (1.0f / width) : 1.0f;
   float height_mod = by_pixel ? (1.0f / height) : 1.0f;

   desc->x = (float)strtod(x, NULL) * width_mod;
   desc->y = (float)strtod(y, NULL) * height_mod;

   if (!strcmp(box, "radial"))
      desc->hitbox = OVERLAY_HITBOX_RADIAL;
   else if (!strcmp(box, "rect"))
      desc->hitbox = OVERLAY_HITBOX_RECT;
   else
   {
      RARCH_ERR("[Overlay]: Hitbox type (%s) is invalid. Use \"radial\" or \"rect\".\n", box);
      ret = false;
      goto end;
   }

   if (desc->type == OVERLAY_TYPE_ANALOG_LEFT || desc->type == OVERLAY_TYPE_ANALOG_RIGHT)
   {
      if (desc->hitbox != OVERLAY_HITBOX_RADIAL)
      {
         RARCH_ERR("[Overlay]: Analog hitbox type must be \"radial\".\n");
         ret = false;
         goto end;
      }

      char overlay_analog_saturate_key[64];
      snprintf(overlay_analog_saturate_key, sizeof(overlay_analog_saturate_key), "overlay%u_desc%u_saturate_pct", ol_index, desc_index);
      if (!config_get_float(conf, overlay_analog_saturate_key, &desc->analog_saturate_pct))
         desc->analog_saturate_pct = 1.0f;
   }

   desc->range_x = (float)strtod(list->elems[4].data, NULL) * width_mod;
   desc->range_y = (float)strtod(list->elems[5].data, NULL) * height_mod;

   desc->mod_x = desc->x - desc->range_x;
   desc->mod_w = 2.0f * desc->range_x;
   desc->mod_y = desc->y - desc->range_y;
   desc->mod_h = 2.0f * desc->range_y;

   char conf_key[64];
   snprintf(conf_key, sizeof(conf_key), "overlay%u_desc%u_alpha_mod", ol_index, desc_index);
   desc->alpha_mod = alpha_mod;
   config_get_float(conf, conf_key, &desc->alpha_mod);

   snprintf(conf_key, sizeof(conf_key), "overlay%u_desc%u_range_mod", ol_index, desc_index);
   desc->range_mod = range_mod;
   config_get_float(conf, conf_key, &desc->range_mod);

   snprintf(conf_key, sizeof(conf_key), "overlay%u_desc%u_movable", ol_index, desc_index);
   desc->movable = false;
   desc->delta_x = 0.0f;
   desc->delta_y = 0.0f;
   config_get_bool(conf, conf_key, &desc->movable);

   desc->range_x_mod = desc->range_x;
   desc->range_y_mod = desc->range_y;

end:
   if (list)
      string_list_free(list);
   return ret;
}

static bool input_overlay_load_overlay(input_overlay_t *ol, config_file_t *conf, const char *config_path,
      struct overlay *overlay, unsigned index)
{
   size_t i;
   char overlay_path_key[64];
   char overlay_name_key[64];
   char overlay_path[PATH_MAX];
   char overlay_resolved_path[PATH_MAX];

   snprintf(overlay_path_key, sizeof(overlay_path_key), "overlay%u_overlay", index);
   if (config_get_path(conf, overlay_path_key, overlay_path, sizeof(overlay_path)))
   {
      fill_pathname_resolve_relative(overlay_resolved_path, config_path,
            overlay_path, sizeof(overlay_resolved_path));

      struct texture_image img = {0};
      if (texture_image_load(overlay_resolved_path, &img))
         overlay->image = img;
      else
      {
         RARCH_ERR("[Overlay]: Failed to load image: %s.\n", overlay_resolved_path);
         return false;
      }
   }

   snprintf(overlay_name_key, sizeof(overlay_name_key), "overlay%u_name", index);
   config_get_array(conf, overlay_name_key, overlay->name, sizeof(overlay->name));

   // By default, we stretch the overlay out in full.
   overlay->x = overlay->y = 0.0f;
   overlay->w = overlay->h = 1.0f;

   char overlay_rect_key[64];
   snprintf(overlay_rect_key, sizeof(overlay_rect_key), "overlay%u_rect", index);
   char overlay_rect[256];
   if (config_get_array(conf, overlay_rect_key, overlay_rect, sizeof(overlay_rect)))
   {
      struct string_list *list = string_split(overlay_rect, ", ");
      if (list->size < 4)
      {
         RARCH_ERR("[Overlay]: Failed to split rect \"%s\" into at least four tokens.\n", overlay_rect);
         return false;
      }

      overlay->x = (float)strtod(list->elems[0].data, NULL);
      overlay->y = (float)strtod(list->elems[1].data, NULL);
      overlay->w = (float)strtod(list->elems[2].data, NULL);
      overlay->h = (float)strtod(list->elems[3].data, NULL);
      string_list_free(list);
   }

   char overlay_full_screen_key[64];
   snprintf(overlay_full_screen_key, sizeof(overlay_full_screen_key),
         "overlay%u_full_screen", index);
   overlay->full_screen = false;
   config_get_bool(conf, overlay_full_screen_key, &overlay->full_screen);

   char overlay_descs_key[64];
   snprintf(overlay_descs_key, sizeof(overlay_descs_key), "overlay%u_descs", index);

   unsigned descs = 0;
   if (!config_get_uint(conf, overlay_descs_key, &descs))
   {
      RARCH_ERR("[Overlay]: Failed to read number of descs from config key: %s.\n", overlay_descs_key);
      return false;
   }

   overlay->descs = (struct overlay_desc*)calloc(descs, sizeof(*overlay->descs));
   if (!overlay->descs)
   {
      RARCH_ERR("[Overlay]: Failed to allocate descs.\n");
      return false;
   }

   overlay->size = descs;

   char conf_key[64];
   bool normalized = false;
   snprintf(conf_key, sizeof(conf_key),
         "overlay%u_normalized", index);
   config_get_bool(conf, conf_key, &normalized);

   float alpha_mod = 1.0f;
   snprintf(conf_key, sizeof(conf_key), "overlay%u_alpha_mod", index);
   config_get_float(conf, conf_key, &alpha_mod);

   float range_mod = 1.0f;
   snprintf(conf_key, sizeof(conf_key), "overlay%u_range_mod", index);
   config_get_float(conf, conf_key, &range_mod);

   for (i = 0; i < overlay->size; i++)
   {
      if (!input_overlay_load_desc(ol, conf, &overlay->descs[i], index, i,
               overlay->image.width, overlay->image.height,
               normalized, alpha_mod, range_mod))
      {
         RARCH_ERR("[Overlay]: Failed to load overlay descs for overlay #%u.\n", (unsigned)i);
         return false;
      }
   }

   // Precache load image array for simplicity.
   overlay->load_images = (struct texture_image*)calloc(1 + overlay->size, sizeof(struct texture_image));
   if (!overlay->load_images)
   {
      RARCH_ERR("[Overlay]: Failed to allocate load_images.\n");
      return false;
   }

   if (overlay->image.pixels)
      overlay->load_images[overlay->load_images_size++] = overlay->image;

   for (i = 0; i < overlay->size; i++)
   {
      if (overlay->descs[i].image.pixels)
      {
         overlay->descs[i].image_index = overlay->load_images_size;
         overlay->load_images[overlay->load_images_size++] = overlay->descs[i].image;
      }
   }

   // Assume for now that scaling center is in the middle.
   // TODO: Make this configurable.
   overlay->block_scale = false;
   overlay->center_x = overlay->x + 0.5f * overlay->w;
   overlay->center_y = overlay->y + 0.5f * overlay->h;

   return true;
}

static ssize_t input_overlay_find_index(const struct overlay *ol, const char *name, size_t size)
{
   size_t i;
   for (i = 0; i < size; i++)
   {
      if (strcmp(ol[i].name, name) == 0)
         return i;
   }

   return -1;
}

static bool input_overlay_resolve_targets(struct overlay *ol, size_t index, size_t size)
{
   size_t i;
   struct overlay *current = &ol[index];

   for (i = 0; i < current->size; i++)
   {
      const char *next = current->descs[i].next_index_name;
      if (*next)
      {
         ssize_t index = input_overlay_find_index(ol, next, size);
         if (index < 0)
         {
            RARCH_ERR("[Overlay]: Couldn't find overlay called: \"%s\".\n", next);
            return false;
         }

         current->descs[i].next_index = index;
      }
      else
         current->descs[i].next_index = (index + 1) % size;
   }

   return true;
}

static bool input_overlay_load_overlays(input_overlay_t *ol, const char *path)
{
   size_t i;
   bool ret = true;
   config_file_t *conf = config_file_new(path);
   if (!conf)
   {
      RARCH_ERR("Failed to load config file: %s.\n", path);
      return false;
   }

   unsigned overlays = 0;
   if (!config_get_uint(conf, "overlays", &overlays))
   {
      RARCH_ERR("overlays variable not defined in config.\n");
      ret = false;
      goto end;
   }

   if (!overlays)
   {
      ret = false;
      goto end;
   }

   ol->overlays = (struct overlay*)calloc(overlays, sizeof(*ol->overlays));
   if (!ol->overlays)
   {
      ret = false;
      goto end;
   }

   ol->size = overlays;

   for (i = 0; i < ol->size; i++)
   {
      if (!input_overlay_load_overlay(ol, conf, path, &ol->overlays[i], i))
      {
         RARCH_ERR("[Overlay]: Failed to load overlay #%u.\n", (unsigned)i);
         ret = false;
         goto end;
      }
   }

   for (i = 0; i < ol->size; i++)
   {
      if (!input_overlay_resolve_targets(ol->overlays, i, ol->size))
      {
         RARCH_ERR("[Overlay]: Failed to resolve next targets.\n");
         ret = false;
         goto end;
      }
   }

end:
   config_file_free(conf);
   return ret;
}

static void input_overlay_load_active(input_overlay_t *ol)
{
   ol->iface->load(ol->iface_data, ol->active->load_images, ol->active->load_images_size);

   input_overlay_set_alpha_mod(ol, g_settings.input.overlay_opacity);
   input_overlay_set_vertex_geom(ol);
   ol->iface->full_screen(ol->iface_data, ol->active->full_screen);
}

input_overlay_t *input_overlay_new(const char *overlay)
{
   input_overlay_t *ol = (input_overlay_t*)calloc(1, sizeof(*ol));

   if (!ol)
      goto error;

   ol->overlay_path = strdup(overlay);
   if (!ol->overlay_path)
   {
      free(ol);
      return NULL;
   }

   if (!driver.video->overlay_interface)
   {
      RARCH_ERR("Overlay interface is not present in video driver.\n");
      goto error;
   }

   video_overlay_interface_func(&ol->iface);
   ol->iface_data = driver.video_data;

   if (!ol->iface)
      goto error;

   if (!input_overlay_load_overlays(ol, overlay))
      goto error;

   ol->active = &ol->overlays[0];

   input_overlay_load_active(ol);
   ol->iface->enable(ol->iface_data, true);
   ol->enable = true;

   input_overlay_set_alpha_mod(ol, g_settings.input.overlay_opacity);
   input_overlay_set_scale_factor(ol, g_settings.input.overlay_scale);
   ol->next_index = (ol->index + 1) % ol->size;

   return ol;

error:
   input_overlay_free(ol);
   return NULL;
}

void input_overlay_enable(input_overlay_t *ol, bool enable)
{
   ol->enable = enable;
   ol->iface->enable(ol->iface_data, enable);
}

static bool inside_hitbox(const struct overlay_desc *desc, float x, float y)
{
   switch (desc->hitbox)
   {
      case OVERLAY_HITBOX_RADIAL:
      {
         // Ellipsis.
         float x_dist = (x - desc->x) / desc->range_x_mod;
         float y_dist = (y - desc->y) / desc->range_y_mod;
         float sq_dist = x_dist * x_dist + y_dist * y_dist;
         return sq_dist <= 1.0f;
      }
      
      case OVERLAY_HITBOX_RECT:
         return (fabs(x - desc->x) <= desc->range_x_mod) &&
            (fabs(y - desc->y) <= desc->range_y_mod);

      default:
         return false;
   }
}

static inline float clamp(float val, float lower, float upper)
{
   if (val < lower)
      return lower;
   else if (val > upper)
      return upper;
   else
      return val;
}

void input_overlay_poll(input_overlay_t *ol, input_overlay_state_t *out, int16_t norm_x, int16_t norm_y)
{
   size_t i;
   memset(out, 0, sizeof(*out));

   if (!ol->enable)
   {
      ol->blocked = false;
      return;
   }

   // norm_x and norm_y is in [-0x7fff, 0x7fff] range, like RETRO_DEVICE_POINTER.
   float x = (float)(norm_x + 0x7fff) / 0xffff;
   float y = (float)(norm_y + 0x7fff) / 0xffff;

   x -= ol->active->mod_x;
   y -= ol->active->mod_y;
   x /= ol->active->mod_w;
   y /= ol->active->mod_h;

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];
      if (!inside_hitbox(desc, x, y))
         continue;

      desc->updated = true;

      if (desc->type == OVERLAY_TYPE_BUTTONS)
      {
         uint64_t mask = desc->key_mask;
         out->buttons |= mask;

         if (mask & (UINT64_C(1) << RARCH_OVERLAY_NEXT))
            ol->next_index = desc->next_index;
      }
      else if (desc->type == OVERLAY_TYPE_KEYBOARD)
      {
         if (desc->key_mask < RETROK_LAST)
            OVERLAY_SET_KEY(out, desc->key_mask);
      }
      else
      {
         float x_dist = x - desc->x;
         float y_dist = y - desc->y;
         float x_val = x_dist / desc->range_x;
         float y_val = y_dist / desc->range_y;
         float x_val_sat = x_val / desc->analog_saturate_pct;
         float y_val_sat = y_val / desc->analog_saturate_pct;

         unsigned int base = (desc->type == OVERLAY_TYPE_ANALOG_RIGHT) ? 2 : 0;
         out->analog[base + 0] = clamp(x_val_sat, -1.0f, 1.0f) * 32767.0f;
         out->analog[base + 1] = clamp(y_val_sat, -1.0f, 1.0f) * 32767.0f;
      }

      if (desc->movable)
      {
         float x_dist = x - desc->x;
         float y_dist = y - desc->y;
         desc->delta_x = clamp(x_dist, -desc->range_x, desc->range_x) * ol->active->mod_w;
         desc->delta_y = clamp(y_dist, -desc->range_y, desc->range_y) * ol->active->mod_h;
      }
   }

   if (!out->buttons)
      ol->blocked = false;
   else if (ol->blocked)
      memset(out, 0, sizeof(*out));
}

static void input_overlay_update_desc_geom(input_overlay_t *ol, struct overlay_desc *desc)
{
   if (desc->image.pixels && desc->movable)
   {
      ol->iface->vertex_geom(ol->iface_data, desc->image_index,
            desc->mod_x + desc->delta_x, desc->mod_y + desc->delta_y,
            desc->mod_w, desc->mod_h);

      desc->delta_x = 0.0f;
      desc->delta_y = 0.0f;
   }
}

void input_overlay_post_poll(input_overlay_t *ol)
{
   size_t i;

   input_overlay_set_alpha_mod(ol, g_settings.input.overlay_opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      if (desc->updated)
      {
         // If pressed this frame, change the hitbox.
         desc->range_x_mod = desc->range_x * desc->range_mod;
         desc->range_y_mod = desc->range_y * desc->range_mod;

         if (desc->image.pixels)
            ol->iface->set_alpha(ol->iface_data, desc->image_index,
                  desc->alpha_mod * g_settings.input.overlay_opacity);
      }
      else
      {
         desc->range_x_mod = desc->range_x;
         desc->range_y_mod = desc->range_y;
      }

      input_overlay_update_desc_geom(ol, desc);
      desc->updated = false;
   }
}

void input_overlay_poll_clear(input_overlay_t *ol)
{
   size_t i;
   ol->blocked = false;
   input_overlay_set_alpha_mod(ol, g_settings.input.overlay_opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];
      desc->range_x_mod = desc->range_x;
      desc->range_y_mod = desc->range_y;
      desc->updated = false;

      desc->delta_x = 0.0f;
      desc->delta_y = 0.0f;
      input_overlay_update_desc_geom(ol, desc);
   }
}

void input_overlay_next(input_overlay_t *ol)
{
   ol->index = ol->next_index;
   ol->active = &ol->overlays[ol->index];

   input_overlay_load_active(ol);

   ol->blocked = true;
   ol->next_index = (ol->index + 1) % ol->size;
}

bool input_overlay_full_screen(input_overlay_t *ol)
{
   return ol->active->full_screen;
}

void input_overlay_free(input_overlay_t *ol)
{
   if (!ol)
      return;

   input_overlay_free_overlays(ol);

   if (ol->iface)
      ol->iface->enable(ol->iface_data, false);

   free(ol->overlay_path);
   free(ol);
}

void input_overlay_set_alpha_mod(input_overlay_t *ol, float mod)
{
   unsigned i;
   for (i = 0; i < ol->active->load_images_size; i++)
      ol->iface->set_alpha(ol->iface_data, i, g_settings.input.overlay_opacity);
}

