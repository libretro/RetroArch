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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include <clamping.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include "../configuration.h"

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

#include "../verbosity.h"
#include "../gfx/video_driver.h"
#include "input_overlay.h"

#define OVERLAY_GET_KEY(state, key) (((state)->keys[(key) / 32] >> ((key) % 32)) & 1)
#define OVERLAY_SET_KEY(state, key) (state)->keys[(key) / 32] |= 1 << ((key) % 32)

typedef struct input_overlay_state
{
   /* This is a bitmask of (1 << key_bind_id). */
   uint64_t buttons;
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog[4]; 

   uint32_t keys[RETROK_LAST / 32 + 1];
} input_overlay_state_t;

struct input_overlay
{
   void *iface_data;
   const video_overlay_interface_t *iface;
   input_overlay_state_t overlay_state;
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

input_overlay_t *overlay_ptr = NULL;

static bool input_overlay_add_inputs(input_overlay_t *ol,
      unsigned port, unsigned analog_dpad_mode);
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
      struct overlay_desc *desc = &ol->descs[i];
      float scale_w             = ol->mod_w * desc->range_x;
      float scale_h             = ol->mod_h * desc->range_y;
      float adj_center_x        = ol->mod_x + desc->x * ol->mod_w;
      float adj_center_y        = ol->mod_y + desc->y * ol->mod_h;

      desc->mod_w               = 2.0f * scale_w;
      desc->mod_h               = 2.0f * scale_h;
      desc->mod_x               = adj_center_x - scale_w;
      desc->mod_y               = adj_center_y - scale_h;
   }
}

static void input_overlay_set_vertex_geom(input_overlay_t *ol)
{
   size_t i;

   if (ol->active->image.pixels)
      ol->iface->vertex_geom(ol->iface_data, 0,
            ol->active->mod_x, ol->active->mod_y,
            ol->active->mod_w, ol->active->mod_h);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!desc->image.pixels)
         continue;

      if (ol->iface->vertex_geom)
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
void input_overlay_set_scale_factor(input_overlay_t *ol, float scale)
{
   size_t i;
    
   if (!ol)
      return;

   for (i = 0; i < ol->size; i++)
      input_overlay_scale(&ol->overlays[i], scale);

   input_overlay_set_vertex_geom(ol);
}

void input_overlay_free_overlay(struct overlay *overlay)
{
   size_t i;

   if (!overlay)
      return;

   for (i = 0; i < overlay->size; i++)
      image_texture_free(&overlay->descs[i].image);

   if (overlay->load_images)
      free(overlay->load_images);
   overlay->load_images = NULL;
   if (overlay->descs)
      free(overlay->descs);
   overlay->descs       = NULL;
   image_texture_free(&overlay->image);

   if (overlay_ptr)
      free(overlay_ptr);
   overlay_ptr = NULL;
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

static void input_overlay_load_active(input_overlay_t *ol, float opacity)
{
   if (!ol)
      return;

   if (ol->iface->load)
      ol->iface->load(ol->iface_data, ol->active->load_images,
            ol->active->load_images_size);

   input_overlay_set_alpha_mod(ol, opacity);
   input_overlay_set_vertex_geom(ol);

   if (ol->iface->full_screen)
      ol->iface->full_screen(ol->iface_data, ol->active->full_screen);
}

/**
 * input_overlay_enable:
 * @enable                : Enable or disable the overlay
 *
 * Enable or disable the overlay.
 **/
static void input_overlay_enable(input_overlay_t *ol, bool enable)
{
   ol->enable = enable;

   if (ol->iface->enable)
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
         return 
            (fabs(x - desc->x) <= desc->range_x_mod) &&
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
static void input_overlay_poll(
      input_overlay_t *ol,
      input_overlay_state_t *out,
      int16_t norm_x, int16_t norm_y)
{
   size_t i;

   /* norm_x and norm_y is in [-0x7fff, 0x7fff] range,
    * like RETRO_DEVICE_POINTER. */
   float x = (float)(norm_x + 0x7fff) / 0xffff;
   float y = (float)(norm_y + 0x7fff) / 0xffff;

   x -= ol->active->mod_x;
   y -= ol->active->mod_y;
   x /= ol->active->mod_w;
   y /= ol->active->mod_h;

   for (i = 0; i < ol->active->size; i++)
   {
      float x_dist, y_dist;
      struct overlay_desc *desc = &ol->active->descs[i];

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
               float x_val       = x_dist / desc->range_x;
               float y_val       = y_dist / desc->range_y;
               float x_val_sat   = x_val / desc->analog_saturate_pct;
               float y_val_sat   = y_val / desc->analog_saturate_pct;

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

   if (ol->iface->vertex_geom)
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
static void input_overlay_post_poll(input_overlay_t *ol, float opacity)
{
   size_t i;

   input_overlay_set_alpha_mod(ol, opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      desc->range_x_mod = desc->range_x;
      desc->range_y_mod = desc->range_y;

      if (desc->updated)
      {
         /* If pressed this frame, change the hitbox. */
         desc->range_x_mod *= desc->range_mod;
         desc->range_y_mod *= desc->range_mod;

         if (desc->image.pixels)
         {
            if (ol->iface->set_alpha)
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
static void input_overlay_poll_clear(input_overlay_t *ol, float opacity)
{
   size_t i;

   ol->blocked = false;

   input_overlay_set_alpha_mod(ol, opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      desc->range_x_mod = desc->range_x;
      desc->range_y_mod = desc->range_y;
      desc->updated     = false;

      desc->delta_x     = 0.0f;
      desc->delta_y     = 0.0f;
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
void input_overlay_next(input_overlay_t *ol, float opacity)
{
   if (!ol)
      return;

   ol->index      = ol->next_index;
   ol->active     = &ol->overlays[ol->index];

   input_overlay_load_active(ol, opacity);

   ol->blocked    = true;
   ol->next_index = (unsigned)((ol->index + 1) % ol->size);
}

/**
 * input_overlay_free:
 * @ol                    : Overlay handle.
 *
 * Frees overlay handle.
 **/
void input_overlay_free(input_overlay_t *ol)
{
   if (!ol)
      return;
   overlay_ptr = NULL;

   input_overlay_free_overlays(ol);

   if (ol->iface->enable)
      ol->iface->enable(ol->iface_data, false);

   free(ol);
}

/* task_data = overlay_task_data_t* */
void input_overlay_loaded(void *task_data, void *user_data, const char *err)
{
   size_t i;
   overlay_task_data_t              *data = (overlay_task_data_t*)task_data;
   input_overlay_t                    *ol = NULL;
   const video_overlay_interface_t *iface = NULL;

   if (err)
      return;

#ifdef HAVE_MENU
   /* We can't display when the menu is up */
   if (data->hide_in_menu && menu_driver_is_alive())
   {
      if (data->overlay_enable)
         goto abort_load;
   }
#endif

   if (!data->overlay_enable || !video_driver_overlay_interface(&iface) || !iface)
   {
      RARCH_ERR("Overlay interface is not present in video driver, or not enabled.\n");
      goto abort_load;
   }

   ol             = (input_overlay_t*)calloc(1, sizeof(*ol));
   ol->overlays   = data->overlays;
   ol->size       = data->size;
   ol->active     = data->active;
   ol->iface      = iface; ol->iface_data = video_driver_get_ptr(true);

   input_overlay_load_active(ol, data->overlay_opacity);
   input_overlay_enable(ol, data->overlay_enable);

   input_overlay_set_scale_factor(ol, data->overlay_scale);

   ol->next_index = (unsigned)((ol->index + 1) % ol->size);
   ol->state      = OVERLAY_STATUS_NONE;
   ol->alive      = true;
   overlay_ptr    = ol;

   free(data);
   return;

abort_load:
   for (i = 0; i < data->size; i++)
      input_overlay_free_overlay(&data->overlays[i]);

   free(data->overlays);
   free(data);
}

/**
 * input_overlay_set_alpha_mod:
 * @ol                    : Overlay handle.
 * @mod                   : New modulating factor to apply.
 *
 * Sets a modulating factor for alpha channel. Default is 1.0.
 * The alpha factor is applied for all overlays.
 **/
void input_overlay_set_alpha_mod(input_overlay_t *ol, float mod)
{
   unsigned i;

   if (!ol)
      return;

   for (i = 0; i < ol->active->load_images_size; i++)
      ol->iface->set_alpha(ol->iface_data, i, mod);
}

bool input_overlay_is_alive(input_overlay_t *ol)
{
   if (ol)
      return ol->alive;
   return false;
}

bool input_overlay_key_pressed(input_overlay_t *ol, int key)
{
   input_overlay_state_t *ol_state  = ol ? &ol->overlay_state : NULL;
   if (!ol)
      return false;
   return (ol_state->buttons & (UINT64_C(1) << key));
}

/*
 * input_poll_overlay:
 *
 * Poll pressed buttons/keys on currently active overlay.
 **/
void input_poll_overlay(input_overlay_t *ol, float opacity, unsigned analog_dpad_mode,
      float axis_threshold)
{
   rarch_joypad_info_t joypad_info;
   input_overlay_state_t old_key_state;
   unsigned i, j, device;
   settings_t *settings            = config_get_ptr();
   uint16_t key_mod                = 0;
   bool polled                     = false;
   bool button_pressed             = false;
   input_overlay_state_t *ol_state = &ol->overlay_state;

   if (!ol_state)
      return;

   joypad_info.joy_idx             = 0;
   joypad_info.auto_binds          = NULL;
   joypad_info.axis_threshold      = 0.0f;

   memcpy(old_key_state.keys, ol_state->keys,
         sizeof(ol_state->keys));
   memset(ol_state, 0, sizeof(*ol_state));

   device = ol->active->full_screen ?
      RARCH_DEVICE_POINTER_SCREEN : RETRO_DEVICE_POINTER;

   for (i = 0;
         current_input->input_state(current_input_data, joypad_info,
            NULL,
            0, device, i, RETRO_DEVICE_ID_POINTER_PRESSED);
         i++)
   {
      input_overlay_state_t polled_data;
      int16_t x = current_input->input_state(current_input_data, joypad_info,
            NULL,
            0, device, i, RETRO_DEVICE_ID_POINTER_X);
      int16_t y = current_input->input_state(current_input_data, joypad_info,
            NULL,
            0, device, i, RETRO_DEVICE_ID_POINTER_Y);

      memset(&polled_data, 0, sizeof(struct input_overlay_state));

      if (ol->enable)
         input_overlay_poll(ol, &polled_data, x, y);
      else
         ol->blocked = false;

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

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LSHIFT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RSHIFT))
      key_mod |= RETROKMOD_SHIFT;

   if (OVERLAY_GET_KEY(ol_state, RETROK_LCTRL) ||
       OVERLAY_GET_KEY(ol_state, RETROK_RCTRL))
      key_mod |= RETROKMOD_CTRL;

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LALT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RALT))
      key_mod |= RETROKMOD_ALT;

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LMETA) ||
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

      if (input_overlay_key_pressed(ol, bind_plus))
         ol_state->analog[j] += 0x7fff;
      if (input_overlay_key_pressed(ol, bind_minus))
         ol_state->analog[j] -= 0x7fff;
   }

   /* Check for analog_dpad_mode.
    * Map analogs to d-pad buttons when configured. */
   switch (analog_dpad_mode)
   {
      case ANALOG_DPAD_LSTICK:
      case ANALOG_DPAD_RSTICK:
      {
         float analog_x, analog_y;
         unsigned analog_base = 2;

         if (analog_dpad_mode == ANALOG_DPAD_LSTICK)
            analog_base = 0;

         analog_x = (float)ol_state->analog[analog_base + 0] / 0x7fff;
         analog_y = (float)ol_state->analog[analog_base + 1] / 0x7fff;

         if (analog_x <= -axis_threshold)
            BIT32_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_LEFT);
         if (analog_x >=  axis_threshold)
            BIT32_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_RIGHT);
         if (analog_y <= -axis_threshold)
            BIT32_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_UP);
         if (analog_y >=  axis_threshold)
            BIT32_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_DOWN);
         break;
      }

      default:
         break;
   }

   if(settings->bools.input_overlay_show_physical_inputs)
   {
      button_pressed = input_overlay_add_inputs(ol, settings->uints.input_overlay_show_physical_inputs_port, analog_dpad_mode);
   }
   if (button_pressed || polled)
      input_overlay_post_poll(ol, opacity);
   else
      input_overlay_poll_clear(ol, opacity);
}

void input_state_overlay(input_overlay_t *ol, int16_t *ret,
      unsigned port, unsigned device, unsigned idx,
      unsigned id)
{
   input_overlay_state_t *ol_state = ol ? &ol->overlay_state : NULL;

   if (!ol || port != 0)
      return;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (input_overlay_key_pressed(ol, id))
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
/**
 * input_overlay_add_inputs:
 * @ol : pointer to overlay
 * @port : the user to show the inputs of
 *
 * Adds inputs from current_input to the overlay, so it's displayed
 * returns true if an input that is pressed will change the overlay 
 */
static bool input_overlay_add_inputs(input_overlay_t *ol,
      unsigned port, unsigned analog_dpad_mode)
{
   unsigned i;
   uint64_t mask;
   int id;
   bool button_pressed             = false;
   bool current_button_pressed     = false;
   input_overlay_state_t *ol_state = &ol->overlay_state;

   if(!ol_state)
      return false;

   for(i = 0; i < ol->active->size; i++)
   {
      overlay_desc_t *desc = &(ol->active->descs[i]);
      switch(desc->type)
      {
         case OVERLAY_TYPE_BUTTONS:
            mask = desc->key_mask;
            id = RETRO_DEVICE_ID_JOYPAD_B;
            /* Need to check all bits in the mask, 
             * multiple ones can be pressed */
            current_button_pressed = false;
            while(mask)
            {
               /* Get the next button ID */
               while(mask && (mask & 1) == 0)
               {
                  id+=1;
                  mask = mask >> 1;
               }
               /* Light up the button if pressed */
               if(input_state(port, RETRO_DEVICE_JOYPAD, 0, id))
               {
                  current_button_pressed = true;
                  desc->updated = true;

                  id+=1;
                  mask = mask >> 1;
               }
               else
               {
                  /* One of the buttons not pressed */
                  current_button_pressed = false;
                  desc->updated = false;
                  break;
               }
            }
            button_pressed = button_pressed || current_button_pressed;
            break;
         case OVERLAY_TYPE_ANALOG_LEFT:
         case OVERLAY_TYPE_ANALOG_RIGHT:
            {
               float analog_x, analog_y;
               float dx, dy;
               unsigned int index = (desc->type == OVERLAY_TYPE_ANALOG_RIGHT) ? 
                  RETRO_DEVICE_INDEX_ANALOG_RIGHT : RETRO_DEVICE_INDEX_ANALOG_LEFT;

               analog_x = input_state(port, RETRO_DEVICE_ANALOG, index, RETRO_DEVICE_ID_ANALOG_X);
               analog_y = input_state(port, RETRO_DEVICE_ANALOG, index, RETRO_DEVICE_ID_ANALOG_Y);
               dx = (analog_x/0x8000)*(desc->range_x/2);
               dy = (analog_y/0x8000)*(desc->range_y/2);

               desc->delta_x = dx;
               desc->delta_y = dy;
               /*Maybe use some option here instead of 0, only display
                 changes greater than some magnitude.
                 */
               if((dx*dx) > 0 || (dy*dy) > 0)
                  button_pressed = true;
            }
            break;
         case OVERLAY_TYPE_KEYBOARD:
            if(input_state(port, RETRO_DEVICE_KEYBOARD, 0, (unsigned)desc->key_mask))
            {
               desc->updated  = true;
               button_pressed = true;
            }
            break;
         default:
            break;
      }
   }

   return button_pressed;
}
