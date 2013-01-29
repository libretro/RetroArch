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

struct overlay_desc
{
   float x;
   float y;

   enum overlay_hitbox hitbox;
   float range_x, range_y;

   uint64_t key_mask;
};

struct overlay
{
   struct overlay_desc *descs;
   size_t size;

   uint32_t *image;
   unsigned width;
   unsigned height;

   float x, y, w, h;
   bool full_screen;
};

struct input_overlay
{
   void *iface_data;
   const video_overlay_interface_t *iface;
   bool enable;

   struct overlay *overlays;
   const struct overlay *active;
   size_t index;
   size_t size;
};

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
      return false;

   struct string_list *list = string_split(overlay, ", ");
   if (!list)
      return false;

   if (list->size < 6)
   {
      string_list_free(list);
      return false;
   }

   const char *x   = list->elems[1].data;
   const char *y   = list->elems[2].data;
   const char *box = list->elems[3].data;

   char *key = list->elems[0].data;
   char *save;
   desc->key_mask = 0;
   for (const char *tmp = strtok_r(key, "|", &save); tmp; tmp = strtok_r(NULL, "|", &save))
      desc->key_mask |= UINT64_C(1) << input_str_to_bind(tmp);

   desc->x        = strtod(x, NULL) / width;
   desc->y        = strtod(y, NULL) / height;

   if (!strcmp(box, "radial"))
      desc->hitbox = OVERLAY_HITBOX_RADIAL;
   else if (!strcmp(box, "rect"))
      desc->hitbox = OVERLAY_HITBOX_RECT;
   else
   {
      ret = false;
      goto end;
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
   char overlay_path[PATH_MAX];
   char overlay_resolved_path[PATH_MAX];

   snprintf(overlay_path_key, sizeof(overlay_path_key), "overlay%u_overlay", index);
   if (!config_get_path(conf, overlay_path_key, overlay_path, sizeof(overlay_path)))
      return false;

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
         return false;

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
      return false;

   overlay->descs = (struct overlay_desc*)calloc(descs, sizeof(*overlay->descs));
   if (!overlay->descs)
      return false;

   overlay->size = descs;

   for (size_t i = 0; i < overlay->size; i++)
   {
      if (!input_overlay_load_desc(conf, &overlay->descs[i], index, i, img.width, img.height))
         return false;
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
   ol->iface->load(ol->iface_data, ol->active->image, ol->active->width, ol->active->height);
   ol->iface->vertex_geom(ol->iface_data,
         ol->active->x, ol->active->y, ol->active->w, ol->active->h);
   ol->iface->full_screen(ol->iface_data, ol->active->full_screen);

   ol->iface->enable(ol->iface_data, true);
   ol->enable = true;

   input_overlay_set_alpha_mod(ol, 1.0f);

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

uint64_t input_overlay_poll(input_overlay_t *ol, int16_t norm_x, int16_t norm_y)
{
   if (!ol->enable)
      return 0;

   // norm_x and norm_y is in [-0x7fff, 0x7fff] range, like RETRO_DEVICE_POINTER.
   float x = (float)(norm_x + 0x7fff) / 0xffff;
   float y = (float)(norm_y + 0x7fff) / 0xffff;

   x -= ol->active->x;
   y -= ol->active->y;
   x /= ol->active->w;
   y /= ol->active->h;

   uint64_t state = 0;
   for (size_t i = 0; i < ol->active->size; i++)
   {
      if (inside_hitbox(&ol->active->descs[i], x, y))
         state |= ol->active->descs[i].key_mask;
   }

   return state;
}

void input_overlay_next(input_overlay_t *ol)
{
   ol->index = (ol->index + 1) % ol->size;
   ol->active = &ol->overlays[ol->index];

   ol->iface->load(ol->iface_data, ol->active->image, ol->active->width, ol->active->height);
   ol->iface->vertex_geom(ol->iface_data,
         ol->active->x, ol->active->y, ol->active->w, ol->active->h);
   ol->iface->full_screen(ol->iface_data, ol->active->full_screen);
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
   ol->iface->set_alpha(ol->iface_data, mod);
}


