/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2018      - Alfredo Monclús
 *  Copyright (C) 2018-2020 - natinusala
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

#include "ozone.h"
#include "ozone_display.h"
#include "ozone_theme.h"

#include <string/stdstring.h>
#include <file/file_path.h>
#include <encodings/utf.h>
#include <lists/string_list.h>

#include "../../../gfx/gfx_animation.h"

#include "../../../input/input_osk.h"

static void ozone_cursor_animation_cb(void *userdata);

static void ozone_animate_cursor(ozone_handle_t *ozone,
      float *dst, float *target)
{
   int i;
   gfx_animation_ctx_entry_t entry;

   entry.easing_enum = EASING_OUT_QUAD;
   entry.tag         = (uintptr_t)&ozone_default_theme;
   entry.duration    = ANIMATION_CURSOR_PULSE;
   entry.userdata    = ozone;

   for (i = 0; i < 16; i++)
   {
      if (i == 3 || i == 7 || i == 11 || i == 15)
         continue;

      if (i == 14)
         entry.cb = ozone_cursor_animation_cb;
      else
         entry.cb = NULL;

      entry.subject        = &dst[i];
      entry.target_value   = target[i];

      gfx_animation_push(&entry);
   }
}

static void ozone_cursor_animation_cb(void *userdata)
{
   float *target         = NULL;
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   switch (ozone->theme_dynamic_cursor_state)
   {
      case 0:
         target = ozone->theme->cursor_border_1;
         break;
      case 1:
         target = ozone->theme->cursor_border_0;
         break;
   }

   ozone->theme_dynamic_cursor_state = 
      (ozone->theme_dynamic_cursor_state + 1) % 2;

   ozone_animate_cursor(ozone, ozone->theme_dynamic.cursor_border, target);
}

static void ozone_draw_cursor_slice(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      int x_offset,
      unsigned width, unsigned height,
      size_t y, float alpha)
{
   float scale_factor    = ozone->last_scale_factor;
   int slice_x           = x_offset - 12 * scale_factor;
   int slice_y           = (int)y + 8 * scale_factor;
   unsigned slice_new_w  = width + (24 + 1) * scale_factor;
   unsigned slice_new_h  = height + 20 * scale_factor;
   gfx_display_ctx_driver_t 
      *dispctx           = p_disp->dispctx;
   static float 
      last_alpha         = 0.0f;

   if (alpha != last_alpha)
   {
      gfx_display_set_alpha(ozone->theme_dynamic.cursor_alpha, alpha);
      gfx_display_set_alpha(ozone->theme_dynamic.cursor_border, alpha);
      last_alpha = alpha;
   }

   if (dispctx && dispctx->blend_begin)
      dispctx->blend_begin(userdata);

   /* Cursor without border */
   gfx_display_draw_texture_slice(
         p_disp,
         userdata,
         video_width,
         video_height,
         slice_x,
         slice_y,
         80, 80,
         slice_new_w,
         slice_new_h,
         video_width, video_height,
         ozone->theme_dynamic.cursor_alpha,
         20, scale_factor,
         ozone->theme->textures[OZONE_THEME_TEXTURE_CURSOR_NO_BORDER]
         );

   /* Tainted border */
   gfx_display_draw_texture_slice(
         p_disp,
         userdata,
         video_width,
         video_height,
         slice_x,
         slice_y,
         80, 80,
         slice_new_w,
         slice_new_h,
         video_width, video_height,
         ozone->theme_dynamic.cursor_border,
         20, scale_factor,
         ozone->textures[OZONE_TEXTURE_CURSOR_BORDER]
         );

   if (dispctx && dispctx->blend_end)
      dispctx->blend_end(userdata);
}

static void ozone_draw_cursor_fallback(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      int x_offset,
      unsigned width, unsigned height,
      size_t y, float alpha)
{
   static float last_alpha           = 0.0f;

   if (alpha != last_alpha)
   {
      gfx_display_set_alpha(ozone->theme_dynamic.selection_border, alpha);
      gfx_display_set_alpha(ozone->theme_dynamic.selection, alpha);
      last_alpha = alpha;
   }

   /* Fill */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         x_offset,
         (int)y,
         width,
         height - ozone->dimensions.spacer_3px,
         video_width,
         video_height,
         ozone->theme_dynamic.selection);

   /* Borders (can't do one single quad because of alpha) */

   /* Top */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         x_offset - ozone->dimensions.spacer_3px,
         (int)(y - ozone->dimensions.spacer_3px),
         width + ozone->dimensions.spacer_3px * 2,
         ozone->dimensions.spacer_3px,
         video_width,
         video_height,
         ozone->theme_dynamic.selection_border);

   /* Bottom */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         x_offset - ozone->dimensions.spacer_3px,
         (int)(y + height - ozone->dimensions.spacer_3px),
         width + ozone->dimensions.spacer_3px * 2,
         ozone->dimensions.spacer_3px,
         video_width,
         video_height,
         ozone->theme_dynamic.selection_border);

   /* Left */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         (int)(x_offset - ozone->dimensions.spacer_3px),
         (int)y,
         ozone->dimensions.spacer_3px,
         height - ozone->dimensions.spacer_3px,
         video_width,
         video_height,
         ozone->theme_dynamic.selection_border);

   /* Right */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         x_offset + width,
         (int)y,
         ozone->dimensions.spacer_3px,
         height - ozone->dimensions.spacer_3px,
         video_width,
         video_height,
         ozone->theme_dynamic.selection_border);
}



void ozone_restart_cursor_animation(ozone_handle_t *ozone)
{
   uintptr_t tag = (uintptr_t) &ozone_default_theme;

   if (!ozone->has_all_assets)
      return;

   ozone->theme_dynamic_cursor_state = 1;
   memcpy(ozone->theme_dynamic.cursor_border,
         ozone->theme->cursor_border_0,
         sizeof(ozone->theme_dynamic.cursor_border));
   gfx_animation_kill_by_tag(&tag);

   ozone_animate_cursor(ozone,
         ozone->theme_dynamic.cursor_border,
         ozone->theme->cursor_border_1);
}

void ozone_draw_cursor(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      int x_offset,
      unsigned width, unsigned height,
      size_t y, float alpha)
{
   int new_x    = x_offset;
   size_t new_y = y;

   /* Apply wiggle animation if needed */
   if (ozone->cursor_wiggle_state.wiggling)
      ozone_apply_cursor_wiggle_offset(ozone, &new_x, &new_y);

   /* Draw the cursor */
   if (ozone->has_all_assets)
      ozone_draw_cursor_slice(ozone, 
            p_disp,
            userdata,
            video_width, video_height,
            new_x, width, height, new_y, alpha);
   else
      ozone_draw_cursor_fallback(ozone,
            p_disp,
            userdata,
            video_width,
            video_height,
            new_x, width, height, new_y, alpha);
}

void ozone_draw_icon(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      unsigned icon_width,
      unsigned icon_height,
      uintptr_t texture,
      float x, float y,
      unsigned width, unsigned height,
      float rotation, float scale_factor,
      float *color)
{
   gfx_display_ctx_rotate_draw_t rotate_draw;
   gfx_display_ctx_draw_t draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;
   gfx_display_ctx_driver_t 
      *dispctx              = p_disp->dispctx;

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = rotation;
   rotate_draw.scale_x      = scale_factor;
   rotate_draw.scale_y      = scale_factor;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   gfx_display_rotate_z(p_disp, &rotate_draw, userdata);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   draw.x               = x;
   draw.y               = height - y - icon_height;
   draw.width           = icon_width;
   draw.height          = icon_height;
   draw.scale_factor    = scale_factor;
   draw.rotation        = rotation;
   draw.coords          = &coords;
   draw.matrix_data     = &mymat;
   draw.texture         = texture;
   draw.prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline_id     = 0;

   if (draw.height > 0 && draw.width > 0)
      dispctx->draw(&draw, userdata, video_width, video_height);
}

void ozone_draw_backdrop(
      void *userdata,
      void *disp_data,
      unsigned video_width,
      unsigned video_height,
      float alpha)
{
   static float ozone_backdrop[16] = {
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
      0.00, 0.00, 0.00, 0.75,
   };
   static float last_alpha           = 0.0f;

   /* TODO: Replace this backdrop by a blur shader 
    * on the whole screen if available */
   if (alpha != last_alpha)
   {
      gfx_display_set_alpha(ozone_backdrop, alpha);
      last_alpha = alpha;
   }

   gfx_display_draw_quad(
         (gfx_display_t*)disp_data,
         userdata,
         video_width,
         video_height,
         0,
         0,
         video_width,
         video_height,
         video_width,
         video_height,
         ozone_backdrop);
}

void ozone_draw_osk(ozone_handle_t *ozone,
      void *userdata,
      void *disp_userdata,
      unsigned video_width,
      unsigned video_height,
      const char *label, const char *str)
{
   unsigned i;
   char message[2048];
   gfx_display_t *p_disp               = (gfx_display_t*)disp_userdata;
   const char *text                    = str;
   unsigned text_color                 = 0xffffffff;
   static float ozone_osk_backdrop[16] = {
      0.00, 0.00, 0.00, 0.15,
      0.00, 0.00, 0.00, 0.15,
      0.00, 0.00, 0.00, 0.15,
      0.00, 0.00, 0.00, 0.15,
   };
   static retro_time_t last_time  = 0;
   struct string_list list        = {0};
   float scale_factor             = ozone->last_scale_factor;
   unsigned margin                = 75 * scale_factor;
   unsigned padding               = 10 * scale_factor;
   unsigned bottom_end            = video_height / 2;
   unsigned y_offset              = 0;
   bool draw_placeholder          = string_is_empty(str);
   retro_time_t current_time      = menu_driver_get_current_time();

   if (current_time - last_time >= INTERVAL_OSK_CURSOR)
   {
      ozone->osk_cursor           = !ozone->osk_cursor;
      last_time                   = current_time;
   }

   /* Border */
   /* Top */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         margin,
         margin,
         video_width - margin*2,
         ozone->dimensions.spacer_1px,
         video_width,
         video_height,
         ozone->theme->entries_border);

   /* Bottom */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         margin,
         bottom_end - margin,
         video_width - margin*2,
         ozone->dimensions.spacer_1px,
         video_width,
         video_height,
         ozone->theme->entries_border);

   /* Left */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         margin,
         margin,
         ozone->dimensions.spacer_1px,
         bottom_end - margin*2,
         video_width,
         video_height,
         ozone->theme->entries_border);

   /* Right */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         video_width - margin,
         margin,
         ozone->dimensions.spacer_1px,
         bottom_end - margin*2,
         video_width,
         video_height,
         ozone->theme->entries_border);

   /* Backdrop */
   /* TODO: Remove the backdrop if blur shader is available */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         margin + ozone->dimensions.spacer_1px,
         margin + ozone->dimensions.spacer_1px,
         video_width - margin*2 - ozone->dimensions.spacer_2px,
         bottom_end - margin*2 - ozone->dimensions.spacer_2px,
         video_width,
         video_height,
         ozone_osk_backdrop);

   /* Placeholder & text*/
   if (draw_placeholder)
   {
      text        = label;
      text_color  = ozone_theme_light.text_sublabel_rgba;
   }

   (ozone->word_wrap)(message, sizeof(message), text,
         (video_width - margin*2 - padding*2) / ozone->fonts.entries_label.glyph_width,
         ozone->fonts.entries_label.wideglyph_width, 0);

   string_list_initialize(&list);
   string_split_noalloc(&list, message, "\n");

   for (i = 0; i < list.size; i++)
   {
      const char *msg = list.elems[i].data;

      gfx_display_draw_text(
            ozone->fonts.entries_label.font,
            msg,
            margin + padding * 2,       /* x */
            margin + padding + 
            ozone->fonts.entries_label.line_height 
            + y_offset,                /* y */
            video_width, video_height,
            text_color,
            TEXT_ALIGN_LEFT,
            1.0f,
            false,
            1.0f,
            false);

      /* Cursor */
      if (i == list.size - 1)
      {
         if (ozone->osk_cursor)
         {
            unsigned cursor_x = draw_placeholder 
               ? 0 
               : font_driver_get_message_width(
                     ozone->fonts.entries_label.font, msg,
                     (unsigned)strlen(msg), 1);
            gfx_display_draw_quad(
                  p_disp,
                  userdata,
                  video_width,
                  video_height,
                    margin 
                  + padding * 2 
                  + cursor_x,
                    margin 
                  + padding 
                  + y_offset 
                  + ozone->fonts.entries_label.line_height 
                  - ozone->fonts.entries_label.line_ascender 
                  + ozone->dimensions.spacer_3px,
                  ozone->dimensions.spacer_1px,
                  ozone->fonts.entries_label.line_ascender,
                  video_width,
                  video_height,
                  ozone->pure_white);
         }
      }
      else
         y_offset += 25 * scale_factor;
   }

   /* Keyboard */
   gfx_display_draw_keyboard(
         p_disp,
         userdata,
         video_width,
         video_height,
         ozone->theme->textures[OZONE_THEME_TEXTURE_CURSOR_STATIC],
         ozone->fonts.entries_label.font,
         input_event_get_osk_grid(),
         input_event_get_osk_ptr(),
         ozone->theme->text_rgba);

   string_list_deinitialize(&list);
}

void ozone_draw_messagebox(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      const char *message)
{
   unsigned i, y_position;
   char wrapped_message[MENU_SUBLABEL_MAX_LENGTH];
   int x, y, longest_width  = 0;
   int usable_width         = 0;
   struct string_list list  = {0};
   float scale_factor       = 0.0f;
   unsigned width           = video_width;
   unsigned height          = video_height;
   gfx_display_ctx_driver_t 
      *dispctx              = p_disp->dispctx;

   wrapped_message[0]       = '\0';

   /* Sanity check */
   if (string_is_empty(message) ||
       !ozone->fonts.footer.font)
      return;

   scale_factor = ozone->last_scale_factor;
   usable_width = (int)width - (48 * 8 * scale_factor);

   if (usable_width < 1)
      return;

   /* Split message into lines */
   (ozone->word_wrap)(
         wrapped_message, sizeof(wrapped_message), message,
         usable_width / (int)ozone->fonts.footer.glyph_width,
         ozone->fonts.footer.wideglyph_width, 0);

   string_list_initialize(&list);
   if (
            !string_split_noalloc(&list, wrapped_message, "\n")
         || list.elems == 0)
   {
      string_list_deinitialize(&list);
      return;
   }

   y_position       = height / 2;
   if (menu_input_dialog_get_display_kb())
      y_position    = height / 4;

   x                = width  / 2;
   y                = y_position - (list.size 
         * ozone->fonts.footer.line_height) / 2;

   /* find the longest line width */
   for (i = 0; i < list.size; i++)
   {
      const char *msg  = list.elems[i].data;

      if (!string_is_empty(msg))
      {
         int width = font_driver_get_message_width(
               ozone->fonts.footer.font, msg, (unsigned)strlen(msg), 1);

         if (width > longest_width)
            longest_width = width;
      }
   }

   gfx_display_set_alpha(ozone->theme_dynamic.message_background, ozone->animations.messagebox_alpha);

   if (dispctx && dispctx->blend_begin)
      dispctx->blend_begin(userdata);

   /* Avoid drawing a black box if there's no assets */
   if (ozone->has_all_assets)
   {
      /* Note: The fact that we use a texture slice here
       * makes things very messy
       * > The actual size and offset of a texture slice
       *   is quite 'loose', and depends upon source image
       *   size, draw size and scale factor... */
      unsigned slice_new_w = longest_width + 48 * 2 * scale_factor;
      unsigned slice_new_h = ozone->fonts.footer.line_height * (list.size + 2);
      int slice_x          = x - longest_width/2 - 48 * scale_factor;
      int slice_y          = y - ozone->fonts.footer.line_height +
            ((slice_new_h >= 256) 
             ? (16.0f * scale_factor) 
             : (16.0f * ((float)slice_new_h / 256.0f)));

      gfx_display_draw_texture_slice(
            p_disp,
            userdata,
            video_width,
            video_height,
            slice_x,
            slice_y,
            256, 256,
            slice_new_w,
            slice_new_h,
            width, height,
            ozone->theme_dynamic.message_background,
            16, scale_factor,
            ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DIALOG_SLICE]
            );
   }

   for (i = 0; i < list.size; i++)
   {
      const char *msg = list.elems[i].data;

      if (msg)
         gfx_display_draw_text(
               ozone->fonts.footer.font,
               msg,
               x - longest_width/2.0,
               y + (i * ozone->fonts.footer.line_height) + 
               ozone->fonts.footer.line_ascender,
               width,
               height,
               COLOR_TEXT_ALPHA(ozone->theme->text_rgba, (uint32_t)(ozone->animations.messagebox_alpha*255.0f)),
               TEXT_ALIGN_LEFT,
               1.0f,
               false,
               1.0f,
               false);
   }

   string_list_deinitialize(&list);
}

void ozone_draw_fullscreen_thumbnails(
      ozone_handle_t *ozone,
      void *userdata,
      void *disp_userdata,
      unsigned video_width,
      unsigned video_height)
{
   /* Check whether fullscreen thumbnails are visible */
   if (ozone->animations.fullscreen_thumbnail_alpha > 0.0f)
   {
      /* Note: right thumbnail is drawn at the top
       * in the sidebar, so it becomes the *left*
       * thumbnail when viewed fullscreen */
      gfx_thumbnail_t *right_thumbnail  = &ozone->thumbnails.left;
      gfx_thumbnail_t *left_thumbnail   = &ozone->thumbnails.right;
      unsigned width                    = video_width;
      unsigned height                   = video_height;
      int view_width                    = (int)width;
      gfx_display_t *p_disp             = (gfx_display_t*)disp_userdata;

      int view_height                   = (int)height - ozone->dimensions.header_height - ozone->dimensions.footer_height - ozone->dimensions.spacer_1px;
      int thumbnail_margin              = ozone->dimensions.fullscreen_thumbnail_padding;
      bool show_right_thumbnail         = false;
      bool show_left_thumbnail          = false;
      unsigned num_thumbnails           = 0;
      float right_thumbnail_draw_width  = 0.0f;
      float right_thumbnail_draw_height = 0.0f;
      float left_thumbnail_draw_width   = 0.0f;
      float left_thumbnail_draw_height  = 0.0f;
      float background_alpha            = 0.85f;
      static float background_color[16] = {
         0.0f, 0.0f, 0.0f, 1.0f,
         0.0f, 0.0f, 0.0f, 1.0f,
         0.0f, 0.0f, 0.0f, 1.0f,
         0.0f, 0.0f, 0.0f, 1.0f,
      };
      int frame_width                   = (int)((float)thumbnail_margin / 3.0f);
      float frame_color[16];
      float separator_color[16];
      int thumbnail_box_width;
      int thumbnail_box_height;
      int right_thumbnail_x;
      int left_thumbnail_x;
      int thumbnail_y;

      /* Sanity check: Return immediately if this is
       * a menu without thumbnails and we are not currently
       * 'fading out' the fullscreen thumbnail view */
      if (!ozone->fullscreen_thumbnails_available &&
          ozone->show_fullscreen_thumbnails)
         goto error;

      /* Safety check: ensure that current
       * selection matches the entry selected when
       * fullscreen thumbnails were enabled
       * > Note that we exclude this check if we are
       *   currently viewing the quick menu and the
       *   thumbnail view is fading out. This enables
       *   a smooth transition if the user presses
       *   RetroPad A or keyboard 'return' to enter the
       *   quick menu while fullscreen thumbnails are
       *   being displayed */
      if (((size_t)ozone->selection != ozone->fullscreen_thumbnail_selection) &&
          (!ozone->is_quick_menu || ozone->show_fullscreen_thumbnails))
         goto error;

      /* Sanity check: Return immediately if the view
       * width/height is < 1 */
      if ((view_width < 1) || (view_height < 1))
         goto error;

      /* Get number of 'active' thumbnails */
      show_right_thumbnail = (right_thumbnail->status == GFX_THUMBNAIL_STATUS_AVAILABLE);
      show_left_thumbnail  = (left_thumbnail->status  == GFX_THUMBNAIL_STATUS_AVAILABLE);

      if (show_right_thumbnail)
         num_thumbnails++;

      if (show_left_thumbnail)
         num_thumbnails++;

      /* Do nothing if both thumbnails are missing
       * > Note: Baring inexplicable internal errors, this
       *   can never happen... */
      if (num_thumbnails < 1)
         goto error;

      /* Get base thumbnail dimensions + draw positions */

      /* > Thumbnail bounding box height + y position
       *   are fixed */
      thumbnail_box_height = view_height - (thumbnail_margin * 2);
      thumbnail_y          = ozone->dimensions.header_height + thumbnail_margin + ozone->dimensions.spacer_1px;

      /* Thumbnail bounding box width and x position
       * depend upon number of active thumbnails */
      if (num_thumbnails == 2)
      {
         thumbnail_box_width = (view_width - (thumbnail_margin * 3) - frame_width) >> 1;
         left_thumbnail_x    = thumbnail_margin;
         right_thumbnail_x   = left_thumbnail_x + thumbnail_box_width + frame_width + thumbnail_margin;
      }
      else
      {
         thumbnail_box_width = view_width - (thumbnail_margin * 2);
         left_thumbnail_x    = thumbnail_margin;
         right_thumbnail_x   = left_thumbnail_x;
      }

      /* Sanity check */
      if ((thumbnail_box_width < 1) ||
          (thumbnail_box_height < 1))
         goto error;

      /* Get thumbnail draw dimensions
       * > Note: The following code is a bit awkward, since
       *   we have to do things in a very specific order
       *   - i.e. we cannot determine proper thumbnail
       *     layout until we have thumbnail draw dimensions.
       *     and we cannot get draw dimensions until we have
       *     the bounding box dimensions...  */
      if (show_right_thumbnail)
      {
         gfx_thumbnail_get_draw_dimensions(
               right_thumbnail,
               thumbnail_box_width, thumbnail_box_height, 1.0f,
               &right_thumbnail_draw_width, &right_thumbnail_draw_height);

         /* Sanity check */
         if ((right_thumbnail_draw_width <= 0.0f) ||
             (right_thumbnail_draw_height <= 0.0f))
            goto error;
      }

      if (show_left_thumbnail)
      {
         gfx_thumbnail_get_draw_dimensions(
               left_thumbnail,
               thumbnail_box_width, thumbnail_box_height, 1.0f,
               &left_thumbnail_draw_width, &left_thumbnail_draw_height);

         /* Sanity check */
         if ((left_thumbnail_draw_width <= 0.0f) ||
             (left_thumbnail_draw_height <= 0.0f))
            goto error;
      }

      /* Adjust thumbnail draw positions to achieve
       * uniform appearance (accounting for actual
       * draw dimensions...) */
      if (num_thumbnails == 2)
      {
         int left_padding  = (thumbnail_box_width - (int)left_thumbnail_draw_width)  >> 1;
         int right_padding = (thumbnail_box_width - (int)right_thumbnail_draw_width) >> 1;

         /* Move thumbnails as close together as possible,
          * and horizontally centre the resultant 'block'
          * of images */
         left_thumbnail_x  += right_padding;
         right_thumbnail_x -= left_padding;
      }

      /* Set colour values */

      /* > Background */
      gfx_display_set_alpha(
            background_color,
            background_alpha * ozone->animations.fullscreen_thumbnail_alpha);

      /* > Separators */
      memcpy(separator_color, ozone->theme->header_footer_separator, sizeof(separator_color));
      gfx_display_set_alpha(
            separator_color, ozone->animations.fullscreen_thumbnail_alpha);

      /* > Thumbnail frame */
      memcpy(frame_color, ozone->theme->sidebar_background, sizeof(frame_color));
      gfx_display_set_alpha(
            frame_color, ozone->animations.fullscreen_thumbnail_alpha);

      /* Darken background */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            0,
            ozone->dimensions.header_height + ozone->dimensions.spacer_1px,
            width,
            (unsigned)view_height,
            width,
            height,
            background_color);

      /* Draw full-width separators */
      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            0,
            ozone->dimensions.header_height,
            width,
            ozone->dimensions.spacer_1px,
            width,
            height,
            separator_color);

      gfx_display_draw_quad(
            p_disp,
            userdata,
            video_width,
            video_height,
            0,
            height - ozone->dimensions.footer_height,
            width,
            ozone->dimensions.spacer_1px,
            width,
            height,
            separator_color);

      /* Draw thumbnails */

      /* > Right */
      if (show_right_thumbnail)
      {
         /* Background */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               right_thumbnail_x - frame_width +
                     ((thumbnail_box_width - (int)right_thumbnail_draw_width) >> 1),
               thumbnail_y - frame_width +
                     ((thumbnail_box_height - (int)right_thumbnail_draw_height) >> 1),
               (unsigned)right_thumbnail_draw_width + (frame_width << 1),
               (unsigned)right_thumbnail_draw_height + (frame_width << 1),
               width,
               height,
               frame_color);

         /* Thumbnail */
         gfx_thumbnail_draw(
               userdata,
               video_width,
               video_height,
               right_thumbnail,
               right_thumbnail_x,
               thumbnail_y,
               (unsigned)thumbnail_box_width,
               (unsigned)thumbnail_box_height,
               GFX_THUMBNAIL_ALIGN_CENTRE,
               ozone->animations.fullscreen_thumbnail_alpha,
               1.0f,
               NULL);
      }

      /* > Left */
      if (show_left_thumbnail)
      {
         /* Background */
         gfx_display_draw_quad(
               p_disp,
               userdata,
               video_width,
               video_height,
               left_thumbnail_x - frame_width +
                     ((thumbnail_box_width - (int)left_thumbnail_draw_width) >> 1),
               thumbnail_y - frame_width +
                     ((thumbnail_box_height - (int)left_thumbnail_draw_height) >> 1),
               (unsigned)left_thumbnail_draw_width + (frame_width << 1),
               (unsigned)left_thumbnail_draw_height + (frame_width << 1),
               width,
               height,
               frame_color);

         /* Thumbnail */
         gfx_thumbnail_draw(
               userdata,
               video_width,
               video_height,
               left_thumbnail,
               left_thumbnail_x,
               thumbnail_y,
               (unsigned)thumbnail_box_width,
               (unsigned)thumbnail_box_height,
               GFX_THUMBNAIL_ALIGN_CENTRE,
               ozone->animations.fullscreen_thumbnail_alpha,
               1.0f,
               NULL);
      }
   }

   return;

error:
   /* If fullscreen thumbnails are enabled at
    * this point, must disable them immediately... */
   if (ozone->show_fullscreen_thumbnails)
      ozone_hide_fullscreen_thumbnails(ozone, false);
}
