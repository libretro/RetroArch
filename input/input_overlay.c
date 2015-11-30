/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include <stddef.h>
#include <string.h>
#include <math.h>

#include <compat/posix_string.h>
#include <file/file_path.h>
#include <retro_assert.h>
#include <string/string_list.h>
#include <file/config_file.h>
#include <formats/image.h>
#include <clamping.h>
#include <rhash.h>

#include "input_overlay.h"
#include "input_keyboard.h"

#include "../configuration.h"
#include "../verbosity.h"
#include "../tasks/tasks.h"

struct input_overlay
{
   void *iface_data;
   const video_overlay_interface_t *iface;
   bool enable;

   bool blocked;
   bool alive;

   struct overlay *overlays;
   const struct overlay *active;
   size_t index;
   size_t size;

   unsigned next_index;
   enum overlay_status state;
};

typedef struct input_overlay_state
{
   /* This is a bitmask of (1 << key_bind_id). */
   uint64_t buttons;
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog[4]; 

   uint32_t keys[RETROK_LAST / 32 + 1];
} input_overlay_state_t;

static input_overlay_t *overlay_ptr;
static input_overlay_state_t overlay_st_ptr;

/**
 * input_overlay_scale:
 * @ol                    : Overlay handle.
 * @scale                 : Scaling factor.
 *
 * Scales overlay and all its associated descriptors
 * by a given scaling factor (@scale).
 **/
static void input_overlay_scale(struct overlay *ol, float scale)
{
   size_t i;

   if (!ol)
      return;

   if (ol->block_scale)
      scale = 1.0f;

   ol->scale = scale;
   ol->mod_w = ol->w * scale;
   ol->mod_h = ol->h * scale;
   ol->mod_x = ol->center_x +
      (ol->x - ol->center_x) * scale;
   ol->mod_y = ol->center_y +
      (ol->y - ol->center_y) * scale;

   for (i = 0; i < ol->size; i++)
   {
      float scale_w, scale_h, adj_center_x, adj_center_y;
      struct overlay_desc *desc = &ol->descs[i];

      if (!desc)
         continue;

      scale_w      = ol->mod_w * desc->range_x;
      scale_h      = ol->mod_h * desc->range_y;

      desc->mod_w  = 2.0f * scale_w;
      desc->mod_h  = 2.0f * scale_h;

      adj_center_x = ol->mod_x + desc->x * ol->mod_w;
      adj_center_y = ol->mod_y + desc->y * ol->mod_h;
      desc->mod_x  = adj_center_x - scale_w;
      desc->mod_y  = adj_center_y - scale_h;
   }
}

static void input_overlay_set_vertex_geom(void)
{
   size_t i;
   input_overlay_t *ol = overlay_ptr;

   if (!ol)
      return;
   if (ol->active->image.pixels)
      ol->iface->vertex_geom(ol->iface_data, 0,
            ol->active->mod_x, ol->active->mod_y,
            ol->active->mod_w, ol->active->mod_h);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!desc)
         continue;

      if (!desc->image.pixels)
         continue;

      if (ol->iface && ol->iface->vertex_geom)
         ol->iface->vertex_geom(ol->iface_data, desc->image_index,
               desc->mod_x, desc->mod_y, desc->mod_w, desc->mod_h);
   }
}

/**
 * input_overlay_set_scale_factor:
 * @ol                    : Overlay handle.
 * @scale                 : Factor of scale to apply.
 *
 * Scales the overlay by a factor of scale.
 **/
void input_overlay_set_scale_factor(float scale)
{
   size_t i;
   input_overlay_t *ol = overlay_ptr;

   if (!ol)
      return;

   for (i = 0; i < ol->size; i++)
      input_overlay_scale(&ol->overlays[i], scale);

   input_overlay_set_vertex_geom();
}

void input_overlay_free_overlay(struct overlay *overlay)
{
   size_t i;

   if (!overlay)
      return;

   for (i = 0; i < overlay->size; i++)
      texture_image_free(&overlay->descs[i].image);

   if (overlay->load_images)
      free(overlay->load_images);
   overlay->load_images = NULL;
   if (overlay->descs)
      free(overlay->descs);
   overlay->descs       = NULL;
   texture_image_free(&overlay->image);
}

static void input_overlay_free_overlays(input_overlay_t *ol)
{
   size_t i;

   if (!ol)
      return;

   for (i = 0; i < ol->size; i++)
      input_overlay_free_overlay(&ol->overlays[i]);

   if (ol->overlays)
      free(ol->overlays);
   ol->overlays = NULL;
}

static void input_overlay_load_active(float opacity)
{
   input_overlay_t *ol = overlay_ptr;
   if (!ol)
      return;

   if (ol->iface && ol->iface->load)
      ol->iface->load(ol->iface_data, ol->active->load_images,
            ol->active->load_images_size);

   input_overlay_set_alpha_mod(opacity);
   input_overlay_set_vertex_geom();

   if (ol->iface && ol->iface->full_screen)
      ol->iface->full_screen(ol->iface_data, ol->active->full_screen);
}

/**
 * input_overlay_enable:
 * @enable                : Enable or disable the overlay
 *
 * Enable or disable the overlay.
 **/
static void input_overlay_enable(bool enable)
{
   input_overlay_t *ol = overlay_ptr;
   if (!ol)
      return;
   ol->enable = enable;

   if (ol->iface && ol->iface->enable)
      ol->iface->enable(ol->iface_data, enable);
}

/**
 * inside_hitbox:
 * @desc                  : Overlay descriptor handle.
 * @x                     : X coordinate value.
 * @y                     : Y coordinate value.
 *
 * Check whether the given @x and @y coordinates of the overlay
 * descriptor @desc is inside the overlay descriptor's hitbox.
 *
 * Returns: true (1) if X, Y coordinates are inside a hitbox, otherwise false (0). 
 **/
static bool inside_hitbox(const struct overlay_desc *desc, float x, float y)
{
   if (!desc)
      return false;

   switch (desc->hitbox)
   {
      case OVERLAY_HITBOX_RADIAL:
      {
         /* Ellipsis. */
         float x_dist  = (x - desc->x) / desc->range_x_mod;
         float y_dist  = (y - desc->y) / desc->range_y_mod;
         float sq_dist = x_dist * x_dist + y_dist * y_dist;
         return (sq_dist <= 1.0f);
      }

      case OVERLAY_HITBOX_RECT:
         return (fabs(x - desc->x) <= desc->range_x_mod) &&
            (fabs(y - desc->y) <= desc->range_y_mod);
   }

   return false;
}

/**
 * input_overlay_poll:
 * @out                   : Polled output data.
 * @norm_x                : Normalized X coordinate.
 * @norm_y                : Normalized Y coordinate.
 *
 * Polls input overlay.
 *
 * @norm_x and @norm_y are the result of
 * input_translate_coord_viewport().
 **/
static void input_overlay_poll(input_overlay_state_t *out,
      int16_t norm_x, int16_t norm_y)
{
   size_t i;
   float x, y;
   input_overlay_t *ol      = overlay_ptr;

   memset(out, 0, sizeof(*out));

   if (!ol->enable)
   {
      ol->blocked = false;
      return;
   }

   /* norm_x and norm_y is in [-0x7fff, 0x7fff] range,
    * like RETRO_DEVICE_POINTER. */
   x = (float)(norm_x + 0x7fff) / 0xffff;
   y = (float)(norm_y + 0x7fff) / 0xffff;

   x -= ol->active->mod_x;
   y -= ol->active->mod_y;
   x /= ol->active->mod_w;
   y /= ol->active->mod_h;

   for (i = 0; i < ol->active->size; i++)
   {
      float x_dist, y_dist;
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!desc)
         continue;
      if (!inside_hitbox(desc, x, y))
         continue;

      desc->updated = true;
      x_dist        = x - desc->x;
      y_dist        = y - desc->y;

      switch (desc->type)
      {
         case OVERLAY_TYPE_BUTTONS:
            {
               uint64_t mask = desc->key_mask;

               out->buttons |= mask;

               if (mask & (UINT64_C(1) << RARCH_OVERLAY_NEXT))
                  ol->next_index = desc->next_index;
            }
            break;
         case OVERLAY_TYPE_KEYBOARD:
            if (desc->key_mask < RETROK_LAST)
               OVERLAY_SET_KEY(out, desc->key_mask);
            break;
         default:
            {
               float x_val     = x_dist / desc->range_x;
               float y_val     = y_dist / desc->range_y;
               float x_val_sat = x_val / desc->analog_saturate_pct;
               float y_val_sat = y_val / desc->analog_saturate_pct;

               unsigned int base = (desc->type == OVERLAY_TYPE_ANALOG_RIGHT) ? 2 : 0;

               out->analog[base + 0] = clamp_float(x_val_sat, -1.0f, 1.0f) * 32767.0f;
               out->analog[base + 1] = clamp_float(y_val_sat, -1.0f, 1.0f) * 32767.0f;
            }
            break;
      }

      if (desc->movable)
      {
         desc->delta_x = clamp_float(x_dist, -desc->range_x, desc->range_x)
            * ol->active->mod_w;
         desc->delta_y = clamp_float(y_dist, -desc->range_y, desc->range_y)
            * ol->active->mod_h;
      }
   }

   if (!out->buttons)
      ol->blocked = false;
   else if (ol->blocked)
      memset(out, 0, sizeof(*out));
}

/**
 * input_overlay_update_desc_geom:
 * @ol                    : overlay handle.
 * @desc                  : overlay descriptors handle.
 * 
 * Update input overlay descriptors' vertex geometry.
 **/
static void input_overlay_update_desc_geom(input_overlay_t *ol,
      struct overlay_desc *desc)
{
   if (!desc || !desc->image.pixels)
      return;
   if (!desc->movable)
      return;

   if (ol->iface && ol->iface->vertex_geom)
      ol->iface->vertex_geom(ol->iface_data, desc->image_index,
            desc->mod_x + desc->delta_x, desc->mod_y + desc->delta_y,
            desc->mod_w, desc->mod_h);

   desc->delta_x = 0.0f;
   desc->delta_y = 0.0f;
}

/**
 * input_overlay_post_poll:
 *
 * Called after all the input_overlay_poll() calls to
 * update the range modifiers for pressed/unpressed regions
 * and alpha mods.
 **/
static void input_overlay_post_poll(float opacity)
{
   size_t i;
   input_overlay_t *ol      = overlay_ptr;

   if (!ol)
      return;

   input_overlay_set_alpha_mod(opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!desc)
         continue;

      desc->range_x_mod = desc->range_x;
      desc->range_y_mod = desc->range_y;

      if (desc->updated)
      {
         /* If pressed this frame, change the hitbox. */
         desc->range_x_mod *= desc->range_mod;
         desc->range_y_mod *= desc->range_mod;

         if (desc->image.pixels)
         {
            if (ol->iface && ol->iface->set_alpha)
               ol->iface->set_alpha(ol->iface_data, desc->image_index,
                     desc->alpha_mod * opacity);
         }
      }

      input_overlay_update_desc_geom(ol, desc);
      desc->updated = false;
   }
}

/**
 * input_overlay_poll_clear:
 * @ol                    : overlay handle
 *
 * Call when there is nothing to poll. Allows overlay to
 * clear certain state.
 **/
static void input_overlay_poll_clear(float opacity)
{
   size_t i;
   input_overlay_t *ol      = overlay_ptr;

   if (!ol)
      return;

   ol->blocked = false;

   input_overlay_set_alpha_mod(opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!desc)
         continue;

      desc->range_x_mod = desc->range_x;
      desc->range_y_mod = desc->range_y;
      desc->updated = false;

      desc->delta_x = 0.0f;
      desc->delta_y = 0.0f;
      input_overlay_update_desc_geom(ol, desc);
   }
}

/**
 * input_overlay_next:
 * @ol                    : Overlay handle.
 *
 * Switch to the next available overlay
 * screen.
 **/
void input_overlay_next(float opacity)
{
   input_overlay_t *ol      = overlay_ptr;
   if (!ol)
      return;

   ol->index      = ol->next_index;
   ol->active     = &ol->overlays[ol->index];

   input_overlay_load_active(opacity);

   ol->blocked    = true;
   ol->next_index = (ol->index + 1) % ol->size;
}

/**
 * input_overlay_full_screen:
 *
 * Checks if the overlay is fullscreen.
 *
 * Returns: true (1) if overlay is fullscreen, otherwise false (0).
 **/
static bool input_overlay_full_screen(void)
{
   input_overlay_t *ol      = overlay_ptr;
   if (!ol)
      return false;
   return ol->active->full_screen;
}

/**
 * input_overlay_free:
 * @ol                    : Overlay handle.
 *
 * Frees overlay handle.
 **/
void input_overlay_free(void)
{
   input_overlay_t *ol      = overlay_ptr;
   if (!ol)
      return;
   overlay_ptr = NULL;

   input_overlay_free_overlays(ol);

   if (ol->iface && ol->iface->enable)
      ol->iface->enable(ol->iface_data, false);

   free(ol);
}

/* task_data = overlay_task_data_t* */
static void input_overlay_loaded(void *task_data, void *user_data, const char *err)
{
   input_overlay_t       *ol;
   overlay_task_data_t *data = (overlay_task_data_t*)task_data;
   settings_t      *settings = config_get_ptr();

   if (err)
      return;

   /* We can't display when the menu is up */
   if (settings->input.overlay_hide_in_menu && menu_driver_alive())
   {
      if (!input_driver_ctl(RARCH_INPUT_CTL_IS_OSK_ENABLED, NULL)
            && settings->input.overlay_enable)
      {
         size_t i;
         for (i = 0; i < data->size; i++)
            input_overlay_free_overlay(&data->overlays[i]);

         free(data->overlays);
         free(data);
         return;
      }
   }

   ol = (input_overlay_t*)calloc(1, sizeof(*ol));
   ol->overlays = data->overlays;
   ol->size     = data->size;
   ol->active   = data->active;

   if (!video_driver_overlay_interface(&ol->iface))
   {
      RARCH_ERR("Overlay interface is not present in video driver.\n");
      goto error;
   }

   ol->iface_data            = video_driver_get_ptr(true);

   if (!ol->iface)
      goto error;

   overlay_ptr = ol;

   input_overlay_load_active(settings->input.overlay_opacity);
   input_overlay_enable(input_driver_ctl(RARCH_INPUT_CTL_IS_OSK_ENABLED, NULL) ? settings->osk.enable : settings->input.overlay_enable);
   input_overlay_set_scale_factor(settings->input.overlay_scale);

   ol->next_index = (ol->index + 1) % ol->size;
   ol->state      = OVERLAY_STATUS_NONE;
   ol->alive      = true;

   free(data);
   return;
error:
   input_overlay_free();
   free(data);
}

void input_overlay_init(void)
{
   input_overlay_free();
   rarch_task_push_overlay_load_default(input_overlay_loaded, NULL);
}

/**
 * input_overlay_set_alpha_mod:
 * @ol                    : Overlay handle.
 * @mod                   : New modulating factor to apply.
 *
 * Sets a modulating factor for alpha channel. Default is 1.0.
 * The alpha factor is applied for all overlays.
 **/
void input_overlay_set_alpha_mod(float mod)
{
   unsigned i;
   input_overlay_t *ol      = overlay_ptr;

   if (!ol)
      return;

   for (i = 0; i < ol->active->load_images_size; i++)
      ol->iface->set_alpha(ol->iface_data, i, mod);
}

bool input_overlay_is_alive(void)
{
   input_overlay_t *ol      = overlay_ptr;
   if (!ol)
      return false;
   return ol->alive;
}

bool input_overlay_key_pressed(int key)
{
   input_overlay_state_t *ol_state  = &overlay_st_ptr;

   if (!ol_state)
      return false;

   return (ol_state->buttons & (UINT64_C(1) << key));
}

/*
 * input_poll_overlay:
 *
 * Poll pressed buttons/keys on currently active overlay.
 **/
void input_poll_overlay(float opacity)
{
   input_overlay_state_t old_key_state;
   unsigned i, j, device;
   uint16_t key_mod                = 0;
   bool polled                     = false;
   settings_t *settings            = config_get_ptr();
   input_overlay_state_t *ol_state = &overlay_st_ptr;

   if (!input_overlay_is_alive() || !ol_state)
      return;

   memcpy(old_key_state.keys, ol_state->keys,
         sizeof(ol_state->keys));
   memset(ol_state, 0, sizeof(*ol_state));

   device = input_overlay_full_screen() ?
      RARCH_DEVICE_POINTER_SCREEN : RETRO_DEVICE_POINTER;

   for (i = 0;
         input_driver_state(NULL, 0, device, i,
            RETRO_DEVICE_ID_POINTER_PRESSED);
         i++)
   {
      input_overlay_state_t polled_data;
      int16_t x = input_driver_state(NULL, 0,
            device, i, RETRO_DEVICE_ID_POINTER_X);
      int16_t y = input_driver_state(NULL, 0,
            device, i, RETRO_DEVICE_ID_POINTER_Y);

      input_overlay_poll(&polled_data, x, y);

      ol_state->buttons |= polled_data.buttons;

      for (j = 0; j < ARRAY_SIZE(ol_state->keys); j++)
         ol_state->keys[j] |= polled_data.keys[j];

      /* Fingers pressed later take prio and matched up
       * with overlay poll priorities. */
      for (j = 0; j < 4; j++)
         if (polled_data.analog[j])
            ol_state->analog[j] = polled_data.analog[j];

      polled = true;
   }

   if (OVERLAY_GET_KEY(ol_state, RETROK_LSHIFT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RSHIFT))
      key_mod |= RETROKMOD_SHIFT;

   if (OVERLAY_GET_KEY(ol_state, RETROK_LCTRL) ||
    OVERLAY_GET_KEY(ol_state, RETROK_RCTRL))
      key_mod |= RETROKMOD_CTRL;

   if (OVERLAY_GET_KEY(ol_state, RETROK_LALT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RALT))
      key_mod |= RETROKMOD_ALT;

   if (OVERLAY_GET_KEY(ol_state, RETROK_LMETA) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RMETA))
      key_mod |= RETROKMOD_META;

   /* CAPSLOCK SCROLLOCK NUMLOCK */
   for (i = 0; i < ARRAY_SIZE(ol_state->keys); i++)
   {
      if (ol_state->keys[i] != old_key_state.keys[i])
      {
         uint32_t orig_bits = old_key_state.keys[i];
         uint32_t new_bits  = ol_state->keys[i];

         for (j = 0; j < 32; j++)
            if ((orig_bits & (1 << j)) != (new_bits & (1 << j)))
               input_keyboard_event(new_bits & (1 << j),
                     i * 32 + j, 0, key_mod, RETRO_DEVICE_POINTER);
      }
   }

   /* Map "analog" buttons to analog axes like regular input drivers do. */
   for (j = 0; j < 4; j++)
   {
      unsigned bind_plus  = RARCH_ANALOG_LEFT_X_PLUS + 2 * j;
      unsigned bind_minus = bind_plus + 1;

      if (ol_state->analog[j])
         continue;

      if (input_overlay_key_pressed(bind_plus))
         ol_state->analog[j] += 0x7fff;
      if (input_overlay_key_pressed(bind_minus))
         ol_state->analog[j] -= 0x7fff;
   }

   /* Check for analog_dpad_mode.
    * Map analogs to d-pad buttons when configured. */
   switch (settings->input.analog_dpad_mode[0])
   {
      case ANALOG_DPAD_LSTICK:
      case ANALOG_DPAD_RSTICK:
      {
         float analog_x, analog_y;
         unsigned analog_base = 2;

         if (settings->input.analog_dpad_mode[0] == ANALOG_DPAD_LSTICK)
            analog_base = 0;

         analog_x = (float)ol_state->analog[analog_base + 0] / 0x7fff;
         analog_y = (float)ol_state->analog[analog_base + 1] / 0x7fff;

         if (analog_x <= -settings->input.axis_threshold)
            ol_state->buttons |= (1UL << RETRO_DEVICE_ID_JOYPAD_LEFT);
         if (analog_x >=  settings->input.axis_threshold)
            ol_state->buttons |= (1UL << RETRO_DEVICE_ID_JOYPAD_RIGHT);
         if (analog_y <= -settings->input.axis_threshold)
            ol_state->buttons |= (1UL << RETRO_DEVICE_ID_JOYPAD_UP);
         if (analog_y >=  settings->input.axis_threshold)
            ol_state->buttons |= (1UL << RETRO_DEVICE_ID_JOYPAD_DOWN);
         break;
      }

      default:
         break;
   }

   if (polled)
      input_overlay_post_poll(opacity);
   else
      input_overlay_poll_clear(opacity);
}

void input_state_overlay(int16_t *ret, unsigned port, unsigned device, unsigned idx,
      unsigned id)
{
   input_overlay_state_t *ol_state = &overlay_st_ptr;

   if (!ol_state)
      return;

   if (port != 0)
      return;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (input_overlay_key_pressed(id))
            *ret |= 1;
         break;
      case RETRO_DEVICE_KEYBOARD:
         if (id < RETROK_LAST)
         {
            if (OVERLAY_GET_KEY(ol_state, id))
               *ret |= 1;
         }
         break;
      case RETRO_DEVICE_ANALOG:
         {
            unsigned base = 0;

            if (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
               base = 2;
            if (id == RETRO_DEVICE_ID_ANALOG_Y)
               base += 1;
            if (ol_state && ol_state->analog[base])
               *ret = ol_state->analog[base];
         }
         break;
   }
}

