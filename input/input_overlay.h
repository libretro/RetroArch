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

#ifndef INPUT_OVERLAY_H__
#define INPUT_OVERLAY_H__

#include <stdint.h>
#include <boolean.h>

#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <formats/image.h>
#include <queues/task_queue.h>

#include "input_driver.h"

RETRO_BEGIN_DECLS

/* Overlay driver acts as a medium between input drivers
 * and video driver.
 *
 * Coordinates are fetched from input driver, and an
 * overlay with pressable actions are displayed on-screen.
 *
 * This interface requires that the video driver has support
 * for the overlay interface.
 */

typedef struct video_overlay_interface
{
   void (*enable)(void *data, bool state);
   bool (*load)(void *data,
         const void *images, unsigned num_images);
   void (*tex_geom)(void *data, unsigned image,
         float x, float y, float w, float h);
   void (*vertex_geom)(void *data, unsigned image,
         float x, float y, float w, float h);
   void (*full_screen)(void *data, bool enable);
   void (*set_alpha)(void *data, unsigned image, float mod);
} video_overlay_interface_t;

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

enum overlay_status
{
   OVERLAY_STATUS_NONE = 0,
   OVERLAY_STATUS_DEFERRED_LOAD,
   OVERLAY_STATUS_DEFERRED_LOADING_IMAGE,
   OVERLAY_STATUS_DEFERRED_LOADING_IMAGE_PROCESS,
   OVERLAY_STATUS_DEFERRED_LOADING,
   OVERLAY_STATUS_DEFERRED_LOADING_RESOLVE,
   OVERLAY_STATUS_DEFERRED_DONE,
   OVERLAY_STATUS_DEFERRED_ERROR
};

enum overlay_image_transfer_status
{
   OVERLAY_IMAGE_TRANSFER_NONE = 0,
   OVERLAY_IMAGE_TRANSFER_BUSY,
   OVERLAY_IMAGE_TRANSFER_DONE,
   OVERLAY_IMAGE_TRANSFER_DESC_IMAGE_ITERATE,
   OVERLAY_IMAGE_TRANSFER_DESC_ITERATE,
   OVERLAY_IMAGE_TRANSFER_DESC_DONE,
   OVERLAY_IMAGE_TRANSFER_ERROR
};

enum overlay_visibility
{
   OVERLAY_VISIBILITY_DEFAULT = 0,
   OVERLAY_VISIBILITY_VISIBLE,
   OVERLAY_VISIBILITY_HIDDEN
};

enum overlay_orientation
{
   OVERLAY_ORIENTATION_NONE = 0,
   OVERLAY_ORIENTATION_LANDSCAPE,
   OVERLAY_ORIENTATION_PORTRAIT
};

enum overlay_show_input_type
{
   OVERLAY_SHOW_INPUT_NONE = 0,
   OVERLAY_SHOW_INPUT_TOUCHED,
   OVERLAY_SHOW_INPUT_PHYSICAL,
   OVERLAY_SHOW_INPUT_LAST
};

struct overlay
{
   struct overlay_desc *descs;
   struct texture_image *load_images;

   struct texture_image image;

   unsigned load_images_size;
   unsigned id;
   unsigned pos_increment;

   size_t size;
   size_t pos;

   float mod_x, mod_y, mod_w, mod_h;
   float x, y, w, h;
   float center_x, center_y;
   float aspect_ratio;

   struct
   {
      float alpha_mod;
      float range_mod;

      struct
      {
         unsigned size;
         char key[64];
      } descs;

      struct
      {
         char key[64];
         char path[PATH_MAX_LENGTH];
      } paths;

      struct
      {
         char key[64];
      } names;

      struct
      {
         char array[256];
         char key[64];
      } rect;

      bool normalized;
   } config;

   bool full_screen;
   bool block_scale;
   bool block_x_separation;
   bool block_y_separation;

   char name[64];
};

struct overlay_desc
{
   struct texture_image image;

   enum overlay_hitbox hitbox;
   enum overlay_type type;

   bool updated;
   bool movable;

   unsigned next_index;
   unsigned image_index;

   float alpha_mod;
   float range_mod;
   float analog_saturate_pct;
   float range_x, range_y;
   float range_x_mod, range_y_mod;
   float mod_x, mod_y, mod_w, mod_h;
   float delta_x, delta_y;
   float x;
   float y;
   /* These are 'raw' x/y values shifted
    * by a user-configured offset (c.f.
    * OVERLAY_X/Y_SEPARATION). Used to determine
    * correct hitbox locations. By default,
    * will be equal to x/y */
   float x_shift;
   float y_shift;

   /* This is a retro_key value for keyboards */
   unsigned retro_key_idx;

   /* This is a bit mask of all input binds to set with this overlay control */
   input_bits_t button_mask;

   char next_index_name[64];
};

/* Holds general layout information for an
 * overlay (overall scaling + positional
 * offset factors) */
typedef struct
{
   float scale_landscape;
   float aspect_adjust_landscape;
   float x_separation_landscape;
   float y_separation_landscape;
   float x_offset_landscape;
   float y_offset_landscape;
   float scale_portrait;
   float aspect_adjust_portrait;
   float x_separation_portrait;
   float y_separation_portrait;
   float x_offset_portrait;
   float y_offset_portrait;
   float touch_scale;
   bool auto_scale;
} overlay_layout_desc_t;

/* Holds derived overlay layout information
 * for a specific display orientation */
typedef struct
{
   float x_scale;
   float y_scale;
   float x_separation;
   float y_separation;
   float x_offset;
   float y_offset;
} overlay_layout_t;

typedef struct overlay_desc overlay_desc_t;

typedef struct input_overlay input_overlay_t;

typedef struct
{
   struct overlay *overlays;
   struct overlay *active;
   size_t size;
   float overlay_opacity;
   overlay_layout_desc_t layout_desc;
   bool overlay_enable;
   bool hide_in_menu;
   bool hide_when_gamepad_connected;
} overlay_task_data_t;

void input_overlay_free_overlay(struct overlay *overlay);

void input_overlay_set_visibility(int overlay_idx,enum overlay_visibility vis);

RETRO_END_DECLS

#endif
