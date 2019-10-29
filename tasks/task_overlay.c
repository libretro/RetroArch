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

#include <stdlib.h>

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <lists/string_list.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <rhash.h>

#include "tasks_internal.h"

#include "../input/input_overlay.h"
#include "../retroarch.h"
#include "../verbosity.h"

typedef struct overlay_loader overlay_loader_t;

struct overlay_loader
{
   enum overlay_status state;
   enum overlay_image_transfer_status loading_status;
   config_file_t *conf;
   char *overlay_path;
   struct overlay *overlays;
   struct overlay *active;
   bool overlay_enable;
   bool overlay_hide_in_menu;
   size_t resolve_pos;
   unsigned size;
   unsigned pos;
   unsigned pos_increment;
   float overlay_opacity;
   float overlay_scale;
   bool driver_rgba_support;
};

static void task_overlay_image_done(struct overlay *overlay)
{
   overlay->pos           = 0;
   /* Divide iteration steps by half of total descs if size is even,
    * otherwise default to 8 (arbitrary value for now to speed things up). */
   overlay->pos_increment = (overlay->size / 2) ? ((unsigned)(overlay->size / 2)) : 8;
}

static void task_overlay_load_desc_image(
      overlay_loader_t *loader,
      struct overlay_desc *desc,
      struct overlay *input_overlay,
      unsigned ol_idx, unsigned desc_idx)
{
   char overlay_desc_image_key[64];
   char image_path[PATH_MAX_LENGTH];
   config_file_t              *conf = loader->conf;

   overlay_desc_image_key[0]        = '\0';
   image_path[0]                    = '\0';

   snprintf(overlay_desc_image_key, sizeof(overlay_desc_image_key),
         "overlay%u_desc%u_overlay", ol_idx, desc_idx);

   if (config_get_path(conf, overlay_desc_image_key,
            image_path, sizeof(image_path)))
   {
      struct texture_image image_tex;
      char path[PATH_MAX_LENGTH];

      path[0] = '\0';

      fill_pathname_resolve_relative(path, loader->overlay_path,
            image_path, sizeof(path));

      image_tex.supports_rgba = loader->driver_rgba_support;

      if (image_texture_load(&image_tex, path))
      {
         input_overlay->load_images[input_overlay->load_images_size++] = image_tex;
         desc->image       = image_tex;
         desc->image_index = input_overlay->load_images_size - 1;
      }
   }

   input_overlay->pos ++;
}

static bool task_overlay_load_desc(
      overlay_loader_t *loader,
      struct overlay_desc *desc,
      struct overlay *input_overlay,
      unsigned ol_idx, unsigned desc_idx,
      unsigned width, unsigned height,
      bool normalized, float alpha_mod, float range_mod)
{
   float width_mod, height_mod;
   char overlay_desc_key[64];
   char conf_key[64];
   char overlay_desc_normalized_key[64];
   char overlay[256];
   float tmp_float                      = 0.0f;
   bool tmp_bool                        = false;
   bool ret                             = true;
   bool by_pixel                        = false;
   char *key                            = NULL;
   struct string_list *list             = NULL;
   const char *x                        = NULL;
   const char *y                        = NULL;
   const char *box                      = NULL;
   config_file_t *conf                  = loader->conf;

   overlay_desc_key[0] = conf_key[0] =
      overlay_desc_normalized_key[0] = overlay[0] = '\0';

   snprintf(overlay_desc_key, sizeof(overlay_desc_key),
         "overlay%u_desc%u", ol_idx, desc_idx);

   snprintf(overlay_desc_normalized_key, sizeof(overlay_desc_normalized_key),
         "overlay%u_desc%u_normalized", ol_idx, desc_idx);
   if (config_get_bool(conf, overlay_desc_normalized_key, &tmp_bool))
      normalized = tmp_bool;

   by_pixel = !normalized;

   if (by_pixel && (width == 0 || height == 0))
   {
      RARCH_ERR("[Overlay]: Base overlay is not set and not using normalized coordinates.\n");
      ret = false;
      goto end;
   }

   if (!config_get_array(conf, overlay_desc_key, overlay, sizeof(overlay)))
   {
      RARCH_ERR("[Overlay]: Didn't find key: %s.\n", overlay_desc_key);
      ret = false;
      goto end;
   }

   list = string_split(overlay, ", ");

   if (!list)
   {
      RARCH_ERR("[Overlay]: Failed to split overlay desc.\n");
      ret = false;
      goto end;
   }

   if (list->size < 6)
   {
      RARCH_ERR("[Overlay]: Overlay desc is invalid. Requires at least 6 tokens.\n");
      ret = false;
      goto end;
   }

   key            = list->elems[0].data;
   x              = list->elems[1].data;
   y              = list->elems[2].data;
   box            = list->elems[3].data;

   desc->retro_key_idx = 0;
   BIT256_CLEAR_ALL(desc->button_mask);

   if (string_is_equal(key, "analog_left"))
      desc->type          = OVERLAY_TYPE_ANALOG_LEFT;
   else if (string_is_equal(key, "analog_right"))
      desc->type          = OVERLAY_TYPE_ANALOG_RIGHT;
   else if (strstr(key, "retrok_") == key)
   {
      desc->type          = OVERLAY_TYPE_KEYBOARD;
      desc->retro_key_idx = input_config_translate_str_to_rk(key + 7);
   }
   else
   {
      char      *save = NULL;
      const char *tmp = strtok_r(key, "|", &save);

      desc->type = OVERLAY_TYPE_BUTTONS;

      for (; tmp; tmp = strtok_r(NULL, "|", &save))
      {
         if (!string_is_equal(tmp, "nul"))
            BIT256_SET(desc->button_mask, input_config_translate_str_to_bind_id(tmp));
      }

      if (BIT256_GET(desc->button_mask, RARCH_OVERLAY_NEXT))
      {
         char overlay_target_key[64];

         snprintf(overlay_target_key, sizeof(overlay_target_key),
               "overlay%u_desc%u_next_target", ol_idx, desc_idx);
         config_get_array(conf, overlay_target_key,
               desc->next_index_name, sizeof(desc->next_index_name));
      }
   }

   width_mod  = 1.0f;
   height_mod = 1.0f;

   if (by_pixel)
   {
      width_mod  /= width;
      height_mod /= height;
   }

   desc->x = (float)strtod(x, NULL) * width_mod;
   desc->y = (float)strtod(y, NULL) * height_mod;

   if (string_is_equal(box, "radial"))
      desc->hitbox = OVERLAY_HITBOX_RADIAL;
   else if (string_is_equal(box, "rect"))
      desc->hitbox = OVERLAY_HITBOX_RECT;
   else
   {
      RARCH_ERR("[Overlay]: Hitbox type (%s) is invalid. Use \"radial\" or \"rect\".\n", box);
      ret = false;
      goto end;
   }

   switch (desc->type)
   {
      case OVERLAY_TYPE_ANALOG_LEFT:
      case OVERLAY_TYPE_ANALOG_RIGHT:
         {
            char overlay_analog_saturate_key[64];

            overlay_analog_saturate_key[0] = '\0';

            if (desc->hitbox != OVERLAY_HITBOX_RADIAL)
            {
               RARCH_ERR("[Overlay]: Analog hitbox type must be \"radial\".\n");
               ret = false;
               goto end;
            }

            snprintf(overlay_analog_saturate_key,
                  sizeof(overlay_analog_saturate_key),
                  "overlay%u_desc%u_saturate_pct", ol_idx, desc_idx);
            if (config_get_float(conf, overlay_analog_saturate_key,
                     &tmp_float))
               desc->analog_saturate_pct = tmp_float;
            else
               desc->analog_saturate_pct = 1.0f;
         }
         break;
      default:
         /* OVERLAY_TYPE_BUTTONS  - unhandled */
         /* OVERLAY_TYPE_KEYBOARD - unhandled */
         break;
   }

   desc->range_x = (float)strtod(list->elems[4].data, NULL) * width_mod;
   desc->range_y = (float)strtod(list->elems[5].data, NULL) * height_mod;

   desc->mod_x   = desc->x - desc->range_x;
   desc->mod_w   = 2.0f * desc->range_x;
   desc->mod_y   = desc->y - desc->range_y;
   desc->mod_h   = 2.0f * desc->range_y;

   snprintf(conf_key, sizeof(conf_key),
         "overlay%u_desc%u_alpha_mod", ol_idx, desc_idx);
   desc->alpha_mod = alpha_mod;
   if (config_get_float(conf, conf_key, &tmp_float))
         desc->alpha_mod = tmp_float;

   snprintf(conf_key, sizeof(conf_key),
         "overlay%u_desc%u_range_mod", ol_idx, desc_idx);
   desc->range_mod = range_mod;
   if (config_get_float(conf, conf_key, &tmp_float))
      desc->range_mod = tmp_float;

   snprintf(conf_key, sizeof(conf_key),
         "overlay%u_desc%u_movable", ol_idx, desc_idx);
   desc->movable     = false;
   desc->delta_x     = 0.0f;
   desc->delta_y     = 0.0f;

   if (config_get_bool(conf, conf_key, &tmp_bool))
      desc->movable = tmp_bool;

   desc->range_x_mod = desc->range_x;
   desc->range_y_mod = desc->range_y;

   input_overlay->pos ++;

end:
   if (list)
      string_list_free(list);
   return ret;
}

static ssize_t task_overlay_find_index(const struct overlay *ol,
      const char *name, size_t size)
{
   size_t i;

   if (!ol)
      return -1;

   for (i = 0; i < size; i++)
   {
      if (string_is_equal(ol[i].name, name))
         return i;
   }

   return -1;
}

static bool task_overlay_resolve_targets(struct overlay *ol,
      size_t idx, size_t size)
{
   unsigned i;
   struct overlay *current = (struct overlay*)&ol[idx];

   for (i = 0; i < current->size; i++)
   {
      struct overlay_desc *desc = (struct overlay_desc*)&current->descs[i];
      const char *next          = desc->next_index_name;
      ssize_t         next_idx  = (idx + 1) & size;

      if (!string_is_empty(next))
      {
         next_idx = task_overlay_find_index(ol, next, size);

         if (next_idx < 0)
         {
            RARCH_ERR("[Overlay]: Couldn't find overlay called: \"%s\".\n",
                  next);
            return false;
         }
      }

      desc->next_index = (unsigned)next_idx;
   }

   return true;
}

static void task_overlay_resolve_iterate(retro_task_t *task)
{
   overlay_loader_t *loader  = (overlay_loader_t*)task->state;
   bool             not_done = loader->resolve_pos < loader->size;

   if (!not_done)
   {
      loader->state = OVERLAY_STATUS_DEFERRED_DONE;
      return;
   }

   if (!task_overlay_resolve_targets(loader->overlays,
            loader->resolve_pos, loader->size))
   {
      RARCH_ERR("[Overlay]: Failed to resolve next targets.\n");
      task_set_cancelled(task, true);
      loader->state   = OVERLAY_STATUS_DEFERRED_ERROR;
      return;
   }

   if (loader->resolve_pos == 0)
   {
      loader->active = &loader->overlays[0];

#if 0
      /* TODO: MOVE TO MAIN THREAD / CALLBACK */
      input_overlay_load_active(loader->deferred.opacity);
      input_overlay_enable(loader->deferred.enable);
#endif
   }

   loader->resolve_pos += 1;
}

static void task_overlay_deferred_loading(retro_task_t *task)
{
   size_t i                  = 0;
   overlay_loader_t *loader  = (overlay_loader_t*)task->state;
   struct overlay *overlay   = &loader->overlays[loader->pos];
   bool not_done             = loader->pos < loader->size;

   if (!not_done)
   {
      loader->state = OVERLAY_STATUS_DEFERRED_LOADING_RESOLVE;
      return;
   }

   switch (loader->loading_status)
   {
      case OVERLAY_IMAGE_TRANSFER_NONE:
      case OVERLAY_IMAGE_TRANSFER_BUSY:
         loader->loading_status = OVERLAY_IMAGE_TRANSFER_DONE;
#if 0
         break;
#endif
      case OVERLAY_IMAGE_TRANSFER_DONE:
         task_overlay_image_done(&loader->overlays[loader->pos]);
         loader->loading_status = OVERLAY_IMAGE_TRANSFER_DESC_IMAGE_ITERATE;
         loader->overlays[loader->pos].pos = 0;
         break;
      case OVERLAY_IMAGE_TRANSFER_DESC_IMAGE_ITERATE:
         for (i = 0; i < overlay->pos_increment; i++)
         {
            if (overlay->pos < overlay->size)
            {
               task_overlay_load_desc_image(loader,
                     &overlay->descs[overlay->pos], overlay,
                     loader->pos, (unsigned)overlay->pos);
            }
            else
            {
               overlay->pos       = 0;
               loader->loading_status = OVERLAY_IMAGE_TRANSFER_DESC_ITERATE;
               break;
            }

         }
         break;
      case OVERLAY_IMAGE_TRANSFER_DESC_ITERATE:
         for (i = 0; i < overlay->pos_increment; i++)
         {
            if (overlay->pos < overlay->size)
            {
               if (!task_overlay_load_desc(loader,
                        &overlay->descs[overlay->pos], overlay,
                        loader->pos, (unsigned)overlay->pos,
                        overlay->image.width, overlay->image.height,
                        overlay->config.normalized,
                        overlay->config.alpha_mod, overlay->config.range_mod))
               {
                  RARCH_ERR("[Overlay]: Failed to load overlay descs for overlay #%u.\n",
                        (unsigned)overlay->pos);
                  task_set_cancelled(task, true);
                  loader->state   = OVERLAY_STATUS_DEFERRED_ERROR;
                  break;
               }
            }
            else
            {
               overlay->pos       = 0;
               loader->loading_status = OVERLAY_IMAGE_TRANSFER_DESC_DONE;
               break;
            }
         }
         break;
      case OVERLAY_IMAGE_TRANSFER_DESC_DONE:
         if (loader->pos == 0)
            task_overlay_resolve_iterate(task);

         loader->pos += 1;
         loader->loading_status = OVERLAY_IMAGE_TRANSFER_NONE;
         break;
      case OVERLAY_IMAGE_TRANSFER_ERROR:
         task_set_cancelled(task, true);
         loader->state   = OVERLAY_STATUS_DEFERRED_ERROR;
         break;
   }
}

static void task_overlay_deferred_load(retro_task_t *task)
{
   unsigned i;
   overlay_loader_t *loader  = (overlay_loader_t*)task->state;
   config_file_t       *conf = loader->conf;

   for (i = 0; i < loader->pos_increment; i++, loader->pos++)
   {
      char tmp_str[PATH_MAX_LENGTH];
      char conf_key[64];
      char overlay_full_screen_key[64];
      float tmp_float                   = 0.0;
      bool tmp_bool                     = false;
      struct texture_image *texture_img = NULL;
      struct overlay_desc *overlay_desc = NULL;
      struct overlay          *overlay  = NULL;
      bool                     to_cont  = loader->pos < loader->size;

      if (!to_cont)
      {
         loader->pos   = 0;
         loader->state = OVERLAY_STATUS_DEFERRED_LOADING;
         break;
      }

      tmp_str[0] = conf_key[0] = overlay_full_screen_key[0] = '\0';

      overlay = &loader->overlays[loader->pos];

      snprintf(overlay->config.descs.key,
            sizeof(overlay->config.descs.key), "overlay%u_descs", loader->pos);

      if (!config_get_uint(conf, overlay->config.descs.key,
               &overlay->config.descs.size))
      {
         RARCH_ERR("[Overlay]: Failed to read number of descs from config key: %s.\n",
               overlay->config.descs.key);
         goto error;
      }

      overlay_desc = (struct overlay_desc*)
         calloc(overlay->config.descs.size, sizeof(*overlay->descs));

      if (!overlay_desc)
      {
         RARCH_ERR("[Overlay]: Failed to allocate descs.\n");
         goto error;
      }

      overlay->descs = overlay_desc;
      overlay->size  = overlay->config.descs.size;

      snprintf(overlay_full_screen_key, sizeof(overlay_full_screen_key),
            "overlay%u_full_screen", loader->pos);
      overlay->full_screen = false;
      if (config_get_bool(conf, overlay_full_screen_key, &tmp_bool))
         overlay->full_screen = tmp_bool;

      overlay->config.normalized = false;
      overlay->config.alpha_mod  = 1.0f;
      overlay->config.range_mod  = 1.0f;

      snprintf(conf_key, sizeof(conf_key),
            "overlay%u_normalized", loader->pos);

      if (config_get_bool(conf, conf_key, &tmp_bool))
         overlay->config.normalized = tmp_bool;

      snprintf(conf_key, sizeof(conf_key), "overlay%u_alpha_mod", loader->pos);
      if (config_get_float(conf, conf_key, &tmp_float))
         overlay->config.alpha_mod = tmp_float;

      snprintf(conf_key, sizeof(conf_key), "overlay%u_range_mod", loader->pos);
      if (config_get_float(conf, conf_key, &tmp_float))
         overlay->config.range_mod = tmp_float;

      /* Precache load image array for simplicity. */
      texture_img = (struct texture_image*)
         calloc(1 + overlay->size, sizeof(struct texture_image));

      if (!texture_img)
      {
         RARCH_ERR("[Overlay]: Failed to allocate load_images.\n");
         goto error;
      }

      overlay->load_images = texture_img;

      snprintf(overlay->config.paths.key, sizeof(overlay->config.paths.key),
            "overlay%u_overlay", loader->pos);

      if (config_get_path(conf, overlay->config.paths.key,
               tmp_str, sizeof(tmp_str)))
         strlcpy(overlay->config.paths.path,
               tmp_str, sizeof(overlay->config.paths.path));

      if (!string_is_empty(overlay->config.paths.path))
      {
         struct texture_image image_tex;
         char overlay_resolved_path[PATH_MAX_LENGTH];

         overlay_resolved_path[0] = '\0';

         fill_pathname_resolve_relative(overlay_resolved_path,
               loader->overlay_path,
               overlay->config.paths.path, sizeof(overlay_resolved_path));

         image_tex.supports_rgba = loader->driver_rgba_support;

         if (!image_texture_load(&image_tex, overlay_resolved_path))
         {
            RARCH_ERR("[Overlay]: Failed to load image: %s.\n",
                  overlay_resolved_path);
            loader->loading_status = OVERLAY_IMAGE_TRANSFER_ERROR;
            goto error;
         }

         overlay->load_images[overlay->load_images_size++] = image_tex;
         overlay->image = image_tex;
      }

      snprintf(overlay->config.names.key, sizeof(overlay->config.names.key),
            "overlay%u_name", loader->pos);
      config_get_array(conf, overlay->config.names.key,
            overlay->name, sizeof(overlay->name));

      /* By default, we stretch the overlay out in full. */
      overlay->x = overlay->y = 0.0f;
      overlay->w = overlay->h = 1.0f;

      snprintf(overlay->config.rect.key, sizeof(overlay->config.rect.key),
            "overlay%u_rect", loader->pos);

      if (config_get_array(conf, overlay->config.rect.key,
               overlay->config.rect.array, sizeof(overlay->config.rect.array)))
      {
         struct string_list *list = string_split(overlay->config.rect.array, ", ");

         if (!list || list->size < 4)
         {
            RARCH_ERR("[Overlay]: Failed to split rect \"%s\" into at least four tokens.\n",
                  overlay->config.rect.array);
            string_list_free(list);
            goto error;
         }

         overlay->x = (float)strtod(list->elems[0].data, NULL);
         overlay->y = (float)strtod(list->elems[1].data, NULL);
         overlay->w = (float)strtod(list->elems[2].data, NULL);
         overlay->h = (float)strtod(list->elems[3].data, NULL);
         string_list_free(list);
      }

      /* Assume for now that scaling center is in the middle.
       * TODO: Make this configurable. */
      overlay->block_scale = false;
      overlay->center_x    = overlay->x + 0.5f * overlay->w;
      overlay->center_y    = overlay->y + 0.5f * overlay->h;
   }

   return;

error:
   task_set_cancelled(task, true);
   loader->pos     = 0;
   loader->state   = OVERLAY_STATUS_DEFERRED_ERROR;
}

static void task_overlay_free(retro_task_t *task)
{
   unsigned i;
   overlay_loader_t *loader  = (overlay_loader_t*)task->state;
   struct overlay *overlay   = &loader->overlays[loader->pos];

   if (loader->overlay_path)
      free(loader->overlay_path);

   if (task_get_cancelled(task))
   {
      for (i = 0; i < overlay->load_images_size; i++)
      {
         struct texture_image *ti = &overlay->load_images[i];
         image_texture_free(ti);
      }

      for (i = 0; i < loader->size; i++)
         input_overlay_free_overlay(&loader->overlays[i]);

      free(loader->overlays);
   }

   if (loader->conf)
      config_file_free(loader->conf);

   free(loader);
}

static void task_overlay_handler(retro_task_t *task)
{
   overlay_loader_t *loader  = (overlay_loader_t*)task->state;

   switch (loader->state)
   {
      case OVERLAY_STATUS_DEFERRED_LOADING:
         task_overlay_deferred_loading(task);
         break;
      case OVERLAY_STATUS_DEFERRED_LOAD:
         task_overlay_deferred_load(task);
         break;
      case OVERLAY_STATUS_DEFERRED_LOADING_RESOLVE:
         task_overlay_resolve_iterate(task);
         break;
      case OVERLAY_STATUS_DEFERRED_ERROR:
         task_set_cancelled(task, true);
         break;
      case OVERLAY_STATUS_DEFERRED_DONE:
      default:
      case OVERLAY_STATUS_NONE:
         task_set_finished(task, true);
         break;
   }

   if (task_get_finished(task) && !task_get_cancelled(task))
   {
      overlay_task_data_t *data = (overlay_task_data_t*)
         calloc(1, sizeof(*data));

      data->overlays        = loader->overlays;
      data->size            = loader->size;
      data->active          = loader->active;
      data->hide_in_menu    = loader->overlay_hide_in_menu;
      data->overlay_enable  = loader->overlay_enable;
      data->overlay_opacity = loader->overlay_opacity;
      data->overlay_scale   = loader->overlay_scale;

      task_set_data(task, data);
   }
}

static bool task_overlay_finder(retro_task_t *task, void *user_data)
{
   overlay_loader_t *loader = NULL;

   if (!task || (task->handler != task_overlay_handler))
      return false;

   if (!user_data)
      return false;

   loader = (overlay_loader_t*)task->state;
   if (!loader)
      return false;

   return string_is_equal(loader->overlay_path, (const char*)user_data);
}

bool task_push_overlay_load_default(
      retro_task_callback_t cb,
      const char *overlay_path,
      bool overlay_hide_in_menu,
      bool input_overlay_enable,
      float input_overlay_opacity,
      float input_overlay_scale,
      void *user_data)
{
   task_finder_data_t find_data;
   retro_task_t *t          = NULL;
   config_file_t *conf      = NULL;
   overlay_loader_t *loader = NULL;
   
   if (string_is_empty(overlay_path))
      return false;

   /* Prevent overlay from being loaded if it already is being loaded */
   find_data.func           = task_overlay_finder;
   find_data.userdata       = (void*)overlay_path;

   if (task_queue_find(&find_data))
      return false;

   loader                   = (overlay_loader_t*)calloc(1, sizeof(*loader));

   if (!loader)
      return false;

   if (!(conf = config_file_new_from_path_to_string(overlay_path)))
   {
      free(loader);
      return false;
   }

   if (!config_get_uint(conf, "overlays", &loader->size))
   {
      /* Error - overlays varaible not defined in config. */
      config_file_free(conf);
      free(loader);
      return false;
   }

   loader->overlays         = (struct overlay*)
      calloc(loader->size, sizeof(*loader->overlays));

   if (!loader->overlays)
   {
      config_file_free(conf);
      free(loader);
      return false;
   }

   loader->overlay_hide_in_menu = overlay_hide_in_menu;
   loader->overlay_enable       = input_overlay_enable;
   loader->overlay_opacity      = input_overlay_opacity;
   loader->overlay_scale        = input_overlay_scale;
   loader->conf                 = conf;
   loader->state                = OVERLAY_STATUS_DEFERRED_LOAD;
   loader->pos_increment        = (loader->size / 4) ? (loader->size / 4) : 4;
#ifdef RARCH_INTERNAL
   loader->driver_rgba_support  = video_driver_supports_rgba();
#endif
   t                            = task_init();

   if (!t)
   {
      config_file_free(conf);
      free(loader->overlays);
      free(loader);
      return false;
   }

   loader->overlay_path         = strdup(overlay_path);

   t->handler                   = task_overlay_handler;
   t->cleanup                   = task_overlay_free;
   t->state                     = loader;
   t->callback                  = cb;
   t->user_data                 = user_data;

   task_queue_push(t);

   return true;
}
