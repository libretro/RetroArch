/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
   OVERLAY_TYPE_ANALOG_RIGHT
};

struct overlay_desc
{
   float x;
   float y;

   enum overlay_hitbox hitbox;
   float range_x, range_y;

   enum overlay_type type;
   uint64_t key_mask;
   float analog_saturate_pct;

   unsigned next_index;
   char next_index_name[64];
};

struct overlay
{
   struct overlay_desc *descs;
   size_t size;

   uint32_t *image;
   unsigned width;
   unsigned height;

   bool block_scale;
   float mod_x, mod_y, mod_w, mod_h;
   float x, y, w, h;
   float scale;
   float center_x, center_y;

   bool full_screen;

   char name[64];
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
   for (unsigned i = 0; i < ARRAY_SIZE(str_to_bind); i++)
   {
      if (!strcmp(str_to_bind[i].str, str))
         return str_to_bind[i].bind;
   }

   return RARCH_BIND_LIST_END;
}

static void input_overlay_scale(struct overlay *overlay, float scale)
{
   if (overlay->block_scale)
   {
      overlay->mod_x = overlay->x;
      overlay->mod_y = overlay->y;
      overlay->mod_w = overlay->w;
      overlay->mod_h = overlay->h;
   }
   else
   {
      overlay->scale = scale;
      overlay->mod_w = overlay->w * scale;
      overlay->mod_h = overlay->h * scale;
      overlay->mod_x = overlay->center_x + (overlay->x - overlay->center_x) * scale;
      overlay->mod_y = overlay->center_y + (overlay->y - overlay->center_y) * scale;
   }
}

void input_overlay_set_scale_factor(input_overlay_t *ol, float scale)
{
   for (size_t i = 0; i < ol->size; i++)
      input_overlay_scale(&ol->overlays[i], scale);

   ol->iface->vertex_geom(ol->iface_data, 0,
         ol->active->mod_x, ol->active->mod_y, ol->active->mod_w, ol->active->mod_h);
}

static void input_overlay_free_overlay(struct overlay *overlay)
{
   free(overlay->descs);
   free(overlay->image);
}

static void input_overlay_free_overlays(input_overlay_t *ol)
{
   for (size_t i = 0; i < ol->size; i++)
      input_overlay_free_overlay(&ol->overlays[i]);
   free(ol->overlays);
}

static bool input_overlay_load_desc(config_file_t *conf, struct overlay_desc *desc,
      unsigned ol_index, unsigned desc_index,
      unsigned width, unsigned height)
{
   bool ret = true;
   char overlay_desc_key[64];
   snprintf(overlay_desc_key, sizeof(overlay_desc_key), "overlay%u_desc%u", ol_index, desc_index);

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
   else
   {
      desc->type = OVERLAY_TYPE_BUTTONS;
      for (const char *tmp = strtok_r(key, "|", &save); tmp; tmp = strtok_r(NULL, "|", &save))
         desc->key_mask |= UINT64_C(1) << input_str_to_bind(tmp);

      if (desc->key_mask & (UINT64_C(1) << RARCH_OVERLAY_NEXT))
      {
         char overlay_target_key[64];
         snprintf(overlay_target_key, sizeof(overlay_target_key), "overlay%u_desc%u_next_target", ol_index, desc_index);
         config_get_array(conf, overlay_target_key, desc->next_index_name, sizeof(desc->next_index_name));
      }
   }

   desc->x = strtod(x, NULL) / width;
   desc->y = strtod(y, NULL) / height;

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

   if (desc->type != OVERLAY_TYPE_BUTTONS)
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

   desc->range_x = strtod(list->elems[4].data, NULL) / width;
   desc->range_y = strtod(list->elems[5].data, NULL) / height;

end:
   if (list)
      string_list_free(list);
   return ret;
}

static bool input_overlay_load_overlay(config_file_t *conf, const char *config_path,
      struct overlay *overlay, unsigned index)
{
   char overlay_path_key[64];
   char overlay_name_key[64];
   char overlay_path[PATH_MAX];
   char overlay_resolved_path[PATH_MAX];

   snprintf(overlay_path_key, sizeof(overlay_path_key), "overlay%u_overlay", index);
   if (!config_get_path(conf, overlay_path_key, overlay_path, sizeof(overlay_path)))
   {
      RARCH_ERR("[Overlay]: Config key: %s is not set.\n", overlay_path_key);
      return false;
   }

   snprintf(overlay_name_key, sizeof(overlay_name_key), "overlay%u_name", index);
   config_get_array(conf, overlay_name_key, overlay->name, sizeof(overlay->name));

   fill_pathname_resolve_relative(overlay_resolved_path, config_path,
         overlay_path, sizeof(overlay_resolved_path));

   struct texture_image img = {0};
   if (!texture_image_load(overlay_resolved_path, &img))
   {
      RARCH_ERR("Failed to load image: %s.\n", overlay_path);
      return false;
   }

   overlay->image  = img.pixels;
   overlay->width  = img.width;
   overlay->height = img.height;

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

      overlay->x = strtod(list->elems[0].data, NULL);
      overlay->y = strtod(list->elems[1].data, NULL);
      overlay->w = strtod(list->elems[2].data, NULL);
      overlay->h = strtod(list->elems[3].data, NULL);
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

   for (size_t i = 0; i < overlay->size; i++)
   {
      if (!input_overlay_load_desc(conf, &overlay->descs[i], index, i, img.width, img.height))
      {
         RARCH_ERR("[Overlay]: Failed to load overlay descs for overlay #%u.\n", (unsigned)i);
         return false;
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
   for (size_t i = 0; i < size; i++)
   {
      if (strcmp(ol[i].name, name) == 0)
         return i;
   }

   return -1;
}

static bool input_overlay_resolve_targets(struct overlay *ol, size_t index, size_t size)
{
   struct overlay *current = &ol[index];

   for (size_t i = 0; i < current->size; i++)
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

   for (size_t i = 0; i < ol->size; i++)
   {
      if (!input_overlay_load_overlay(conf, path, &ol->overlays[i], i))
      {
         RARCH_ERR("[Overlay]: Failed to load overlay #%u.\n", (unsigned)i);
         ret = false;
         goto end;
      }
   }

   for (size_t i = 0; i < ol->size; i++)
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

input_overlay_t *input_overlay_new(const char *overlay)
{
   input_overlay_t *ol = (input_overlay_t*)calloc(1, sizeof(*ol));

   if (!ol)
      goto error;

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
   struct video_overlay_image image = {
      ol->active->image,
      ol->active->width,
      ol->active->height,
   };
   ol->iface->load(ol->iface_data, &image, 1);
   ol->iface->vertex_geom(ol->iface_data, 0,
         ol->active->mod_x, ol->active->mod_y, ol->active->mod_w, ol->active->mod_h);
   ol->iface->full_screen(ol->iface_data, ol->active->full_screen);

   ol->iface->enable(ol->iface_data, true);
   ol->enable = true;

   input_overlay_set_alpha_mod(ol, g_settings.input.overlay_opacity);
   input_overlay_set_scale_factor(ol, 1.0f);
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
         float x_dist = (x - desc->x) / desc->range_x;
         float y_dist = (y - desc->y) / desc->range_y;
         float sq_dist = x_dist * x_dist + y_dist * y_dist;
         return sq_dist <= 1.0f;
      }
      
      case OVERLAY_HITBOX_RECT:
         return (fabs(x - desc->x) <= desc->range_x) &&
            (fabs(y - desc->y) <= desc->range_y);

      default:
         return false;
   }
}

void input_overlay_poll(input_overlay_t *ol, input_overlay_state_t *out, int16_t norm_x, int16_t norm_y)
{
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

   for (size_t i = 0; i < ol->active->size; i++)
   {
      if (!inside_hitbox(&ol->active->descs[i], x, y))
         continue;

      if (ol->active->descs[i].type == OVERLAY_TYPE_BUTTONS)
      {
         uint64_t mask = ol->active->descs[i].key_mask;
         out->buttons |= mask;

         if (mask & (UINT64_C(1) << RARCH_OVERLAY_NEXT))
            ol->next_index = ol->active->descs[i].next_index;
      }
      else
      {
         float x_val = (x - ol->active->descs[i].x) / ol->active->descs[i].range_x / ol->active->descs[i].analog_saturate_pct;
         float y_val = (y - ol->active->descs[i].y) / ol->active->descs[i].range_y / ol->active->descs[i].analog_saturate_pct;

         if (fabs(x_val) > 1.0f)
            x_val = (x_val > 0.0f) ? 1.0f : -1.0f;

         if (fabs(y_val) > 1.0f)
            y_val = (y_val > 0.0f) ? 1.0f : -1.0f;

         unsigned int base = (ol->active->descs[i].type == OVERLAY_TYPE_ANALOG_RIGHT) ? 2 : 0;
         out->analog[base + 0] = x_val * 32767.0f;
         out->analog[base + 1] = y_val * 32767.0f;
      }
   }

   if (!out->buttons)
      ol->blocked = false;
   else if (ol->blocked)
      memset(out, 0, sizeof(*out));
}

void input_overlay_poll_clear(input_overlay_t *ol)
{
   ol->blocked = false;
}

void input_overlay_next(input_overlay_t *ol)
{
   ol->index = ol->next_index;
   ol->active = &ol->overlays[ol->index];

   struct video_overlay_image image = {
      ol->active->image,
      ol->active->width,
      ol->active->height,
   };
   ol->iface->load(ol->iface_data, &image, 1);
   ol->iface->vertex_geom(ol->iface_data, 0,
         ol->active->mod_x, ol->active->mod_y, ol->active->mod_w, ol->active->mod_h);
   ol->iface->full_screen(ol->iface_data, ol->active->full_screen);
   ol->iface->set_alpha(ol->iface_data, 0, g_settings.input.overlay_opacity);
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

   free(ol);
}

void input_overlay_set_alpha_mod(input_overlay_t *ol, float mod)
{
   ol->iface->set_alpha(ol->iface_data, 0, mod);
}


