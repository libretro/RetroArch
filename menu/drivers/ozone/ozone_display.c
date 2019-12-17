/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2018      - Alfredo Monclús
 *  Copyright (C) 2018      - natinusala
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
#include <features/features_cpu.h>

#include "../../menu_input.h"
#include "../../menu_animation.h"

#include "../../widgets/menu_osk.h"

static void ozone_cursor_animation_cb(void *userdata);

static void ozone_animate_cursor(ozone_handle_t *ozone, float *dst, float *target)
{
   menu_animation_ctx_entry_t entry;
   int i;

   entry.easing_enum = EASING_OUT_QUAD;
   entry.tag = (uintptr_t) &ozone_default_theme;
   entry.duration = ANIMATION_CURSOR_PULSE;
   entry.userdata = ozone;

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

      menu_animation_push(&entry);
   }
}

static void ozone_cursor_animation_cb(void *userdata)
{
   ozone_handle_t *ozone = (ozone_handle_t*) userdata;

   float *target = NULL;

   switch (ozone->theme_dynamic.cursor_state)
   {
      case 0:
         target = ozone->theme->cursor_border_1;
         break;
      case 1:
         target = ozone->theme->cursor_border_0;
         break;
   }

   ozone->theme_dynamic.cursor_state = (ozone->theme_dynamic.cursor_state + 1) % 2;

   ozone_animate_cursor(ozone, ozone->theme_dynamic.cursor_border, target);
}

void ozone_restart_cursor_animation(ozone_handle_t *ozone)
{
   menu_animation_ctx_tag tag = (uintptr_t) &ozone_default_theme;

   if (!ozone->has_all_assets)
      return;

   ozone->theme_dynamic.cursor_state = 1;
   memcpy(ozone->theme_dynamic.cursor_border, ozone->theme->cursor_border_0, sizeof(ozone->theme_dynamic.cursor_border));
   menu_animation_kill_by_tag(&tag);

   ozone_animate_cursor(ozone, ozone->theme_dynamic.cursor_border, ozone->theme->cursor_border_1);
}

void ozone_draw_text(
      video_frame_info_t *video_info,
      ozone_handle_t *ozone,
      const char *str, float x,
      float y,
      enum text_alignment text_align,
      unsigned width, unsigned height, font_data_t* font,
      uint32_t color,
      bool draw_outside)
{
   menu_display_draw_text(font, str, x, y,
         width, height, color, text_align, 1.0f,
         false,
         1.0, draw_outside);
}

static void ozone_draw_cursor_slice(ozone_handle_t *ozone,
      video_frame_info_t *video_info,
      int x_offset,
      unsigned width, unsigned height,
      size_t y, float alpha)
{
   menu_display_set_alpha(ozone->theme_dynamic.cursor_alpha, alpha);
   menu_display_set_alpha(ozone->theme_dynamic.cursor_border, alpha);

   menu_display_blend_begin(video_info);

   /* Cursor without border */
   menu_display_draw_texture_slice(
      video_info,
      x_offset - 14,
      (int)(y + 8),
      80, 80,
      width + 3 + 28 - 4,
      height + 20,
      video_info->width, video_info->height,
      ozone->theme_dynamic.cursor_alpha,
      20, 1.0,
      ozone->theme->textures[OZONE_THEME_TEXTURE_CURSOR_NO_BORDER]
   );

   /* Tainted border */
   menu_display_draw_texture_slice(
      video_info,
      x_offset - 14,
      (int)(y + 8),
      80, 80,
      width + 3 + 28 - 4,
      height + 20,
      video_info->width, video_info->height,
      ozone->theme_dynamic.cursor_border,
      20, 1.0,
      ozone->textures[OZONE_TEXTURE_CURSOR_BORDER]
   );

   menu_display_blend_end(video_info);
}

static void ozone_draw_cursor_fallback(ozone_handle_t *ozone,
      video_frame_info_t *video_info,
      int x_offset,
      unsigned width, unsigned height,
      size_t y, float alpha)
{
   menu_display_set_alpha(ozone->theme_dynamic.selection_border, alpha);
   menu_display_set_alpha(ozone->theme_dynamic.selection, alpha);

   /* Fill */
   menu_display_draw_quad(video_info, x_offset, (int)y, width, height - 5, video_info->width, video_info->height, ozone->theme_dynamic.selection);

   /* Borders (can't do one single quad because of alpha) */

   /* Top */
   menu_display_draw_quad(video_info, x_offset - 3, (int)(y - 3), width + 6, 3, video_info->width, video_info->height, ozone->theme_dynamic.selection_border);

   /* Bottom */
   menu_display_draw_quad(video_info, x_offset - 3, (int)(y + height - 5), width + 6, 3, video_info->width, video_info->height, ozone->theme_dynamic.selection_border);

   /* Left */
   menu_display_draw_quad(video_info, (int)(x_offset - 3), (int)y, 3, height - 5, video_info->width, video_info->height, ozone->theme_dynamic.selection_border);

   /* Right */
   menu_display_draw_quad(video_info, x_offset + width, (int)y, 3, height - 5, video_info->width, video_info->height, ozone->theme_dynamic.selection_border);
}

void ozone_draw_cursor(ozone_handle_t *ozone,
      video_frame_info_t *video_info,
      int x_offset,
      unsigned width, unsigned height,
      size_t y, float alpha)
{
   if (ozone->has_all_assets)
      ozone_draw_cursor_slice(ozone, video_info, x_offset, width, height, y, alpha);
   else
      ozone_draw_cursor_fallback(ozone, video_info, x_offset, width, height, y, alpha);
}

void ozone_draw_icon(
      video_frame_info_t *video_info,
      unsigned icon_width,
      unsigned icon_height,
      uintptr_t texture,
      float x, float y,
      unsigned width, unsigned height,
      float rotation, float scale_factor,
      float *color)
{
   menu_display_ctx_rotate_draw_t rotate_draw;
   menu_display_ctx_draw_t draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = rotation;
   rotate_draw.scale_x      = scale_factor;
   rotate_draw.scale_y      = scale_factor;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw, video_info);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = color ? (const float*)color : ozone_pure_white;

   draw.x               = x;
   draw.y               = height - y - icon_height;
   draw.width           = icon_width;
   draw.height          = icon_height;
   draw.scale_factor    = scale_factor;
   draw.rotation        = rotation;
   draw.coords          = &coords;
   draw.matrix_data     = &mymat;
   draw.texture         = texture;
   draw.prim_type       = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id     = 0;

   menu_display_draw(&draw, video_info);
}

void ozone_draw_backdrop(video_frame_info_t *video_info, float alpha)
{
   /* TODO: Replace this backdrop by a blur shader on the whole screen if available */
   menu_display_set_alpha(ozone_backdrop, alpha);
   menu_display_draw_quad(video_info, 0, 0, video_info->width, video_info->height, video_info->width, video_info->height, ozone_backdrop);
}

void ozone_draw_osk(ozone_handle_t *ozone,
      video_frame_info_t *video_info,
      const char *label, const char *str)
{
   unsigned i;
   const char *text;
   char message[2048];
   unsigned text_color;
   struct string_list *list;

   unsigned margin         = 75;
   unsigned padding        = 10;
   unsigned bottom_end     = video_info->height/2;
   unsigned y_offset       = 0;
   bool draw_placeholder   = string_is_empty(str);

   retro_time_t current_time      = cpu_features_get_time_usec();
   static retro_time_t last_time  = 0;

   if (current_time - last_time >= INTERVAL_OSK_CURSOR)
   {
      ozone->osk_cursor = !ozone->osk_cursor;
      last_time = current_time;
   }

   /* Border */
   /* Top */
   menu_display_draw_quad(video_info, margin, margin, video_info->width - margin*2, 1, video_info->width, video_info->height, ozone->theme->entries_border);

   /* Bottom */
   menu_display_draw_quad(video_info, margin, bottom_end - margin, video_info->width - margin*2, 1, video_info->width, video_info->height, ozone->theme->entries_border);

   /* Left */
   menu_display_draw_quad(video_info, margin, margin, 1, bottom_end - margin*2, video_info->width, video_info->height, ozone->theme->entries_border);

   /* Right */
   menu_display_draw_quad(video_info, video_info->width - margin, margin, 1, bottom_end - margin*2, video_info->width, video_info->height, ozone->theme->entries_border);

   /* Backdrop */
   /* TODO: Remove the backdrop if blur shader is available */
   menu_display_draw_quad(video_info, margin + 1, margin + 1, video_info->width - margin*2 - 2, bottom_end - margin*2 - 2, video_info->width, video_info->height, ozone_osk_backdrop);

   /* Placeholder & text*/
   if (!draw_placeholder)
   {
      text        = str;
      text_color  = 0xffffffff;
   }
   else
   {
      text        = label;
      text_color  = ozone_theme_light.text_sublabel_rgba;
   }

   word_wrap(message, text, (video_info->width - margin*2 - padding*2) / ozone->entry_font_glyph_width, true, 0);

   list = string_split(message, "\n");

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;

      ozone_draw_text(video_info, ozone, msg, margin + padding * 2, margin + padding + FONT_SIZE_ENTRIES_LABEL + y_offset, TEXT_ALIGN_LEFT, video_info->width, video_info->height, ozone->fonts.entries_label, text_color, false);

      /* Cursor */
      if (i == list->size - 1)
      {
         if (ozone->osk_cursor)
         {
            unsigned cursor_x = draw_placeholder ? 0 : font_driver_get_message_width(ozone->fonts.entries_label, msg, (unsigned)strlen(msg), 1);
            menu_display_draw_quad(video_info, margin + padding*2 + cursor_x, margin + padding + y_offset + 3, 1, 25, video_info->width, video_info->height, ozone_pure_white);
         }
      }
      else
      {
         y_offset += 25;
      }
   }

   /* Keyboard */
   menu_display_draw_keyboard(
            ozone->theme->textures[OZONE_THEME_TEXTURE_CURSOR_STATIC],
            ozone->fonts.entries_label,
            video_info,
            menu_event_get_osk_grid(),
            menu_event_get_osk_ptr(),
            ozone->theme->text_rgba);

   string_list_free(list);
}

void ozone_draw_messagebox(ozone_handle_t *ozone,
      video_frame_info_t *video_info,
      const char *message)
{
   unsigned i, y_position;
   int x, y, longest = 0, longest_width = 0;
   float line_height        = 0;
   unsigned width           = video_info->width;
   unsigned height          = video_info->height;
   struct string_list *list = !string_is_empty(message)
      ? string_split(message, "\n") : NULL;

   if (!list || !ozone || !ozone->fonts.footer)
   {
      if (list)
         string_list_free(list);
      return;
   }

   if (list->elems == 0)
      goto end;

   line_height      = 25;

   y_position       = height / 2;
   if (menu_input_dialog_get_display_kb())
      y_position    = height / 4;

   x                = width  / 2;
   y                = y_position - (list->size-1) * line_height / 2;

   /* find the longest line width */
   for (i = 0; i < list->size; i++)
   {
      const char *msg  = list->elems[i].data;
      int len          = (int)utf8len(msg);

      if (len > longest)
      {
         longest       = len;
         longest_width = font_driver_get_message_width(
               ozone->fonts.footer, msg, (unsigned)strlen(msg), 1);
      }
   }

   menu_display_set_alpha(ozone->theme_dynamic.message_background, ozone->animations.messagebox_alpha);

   menu_display_blend_begin(video_info);

   if (ozone->has_all_assets) /* avoid drawing a black box if there's no assets */
      menu_display_draw_texture_slice(
         video_info,
         x - longest_width/2 - 48,
         y + 16 - 48,
         256, 256,
         longest_width + 48 * 2,
         line_height * list->size + 48 * 2,
         width, height,
         ozone->theme_dynamic.message_background,
         16, 1.0,
         ozone->icons_textures[OZONE_ENTRIES_ICONS_TEXTURE_DIALOG_SLICE]
      );

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;

      if (msg)
         ozone_draw_text(video_info, ozone,
            msg,
            x - longest_width/2.0,
            y + (i+0.75) * line_height,
            TEXT_ALIGN_LEFT,
            width, height,
            ozone->fonts.footer,
            COLOR_TEXT_ALPHA(ozone->theme->text_rgba, (uint32_t)(ozone->animations.messagebox_alpha*255.0f)),
            false
         );
   }

end:
   string_list_free(list);
}

void ozone_draw_fullscreen_thumbnails(
      ozone_handle_t *ozone, video_frame_info_t *video_info)
{
   /* Check whether fullscreen thumbnails are visible */
   if (ozone->animations.fullscreen_thumbnail_alpha > 0.0f)
   {
      /* Note: right thumbnail is drawn at the top
       * in the sidebar, so it becomes the *left*
       * thumbnail when viewed fullscreen */
      menu_thumbnail_t *right_thumbnail = &ozone->thumbnails.left;
      menu_thumbnail_t *left_thumbnail  = &ozone->thumbnails.right;
      unsigned width                    = video_info->width;
      unsigned height                   = video_info->height;
      int view_width                    = (int)width;
      int view_height                   = (int)height - ozone->dimensions.header_height - ozone->dimensions.footer_height - 1;
      int thumbnail_margin              = ozone->dimensions.fullscreen_thumbnail_padding;
      bool show_right_thumbnail         = false;
      bool show_left_thumbnail          = false;
      unsigned num_thumbnails           = 0;
      float right_thumbnail_draw_width  = 0.0f;
      float right_thumbnail_draw_height = 0.0f;
      float left_thumbnail_draw_width   = 0.0f;
      float left_thumbnail_draw_height  = 0.0f;
      float background_alpha            = 0.85f;
      float background_color[16]        = {
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
      show_right_thumbnail = (right_thumbnail->status == MENU_THUMBNAIL_STATUS_AVAILABLE);
      show_left_thumbnail  = (left_thumbnail->status  == MENU_THUMBNAIL_STATUS_AVAILABLE);

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
      thumbnail_y          = ozone->dimensions.header_height + thumbnail_margin + 1;

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
         left_thumbnail_x    = thumbnail_margin;;
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
         menu_thumbnail_get_draw_dimensions(
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
         menu_thumbnail_get_draw_dimensions(
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
      menu_display_set_alpha(
            background_color,
            background_alpha * ozone->animations.fullscreen_thumbnail_alpha);

      /* > Separators */
      memcpy(separator_color, ozone->theme->header_footer_separator, sizeof(separator_color));
      menu_display_set_alpha(
            separator_color, ozone->animations.fullscreen_thumbnail_alpha);

      /* > Thumbnail frame */
      memcpy(frame_color, ozone->theme->sidebar_background, sizeof(frame_color));
      menu_display_set_alpha(
            frame_color, ozone->animations.fullscreen_thumbnail_alpha);

      /* Darken background */
      menu_display_draw_quad(
            video_info,
            0,
            ozone->dimensions.header_height + 1,
            width,
            (unsigned)view_height,
            width,
            height,
            background_color);

      /* Draw full-width separators */
      menu_display_draw_quad(
            video_info,
            0,
            ozone->dimensions.header_height,
            width,
            1,
            width,
            height,
            separator_color);

      menu_display_draw_quad(
            video_info,
            0,
            height - ozone->dimensions.footer_height,
            width,
            1,
            width,
            height,
            separator_color);

      /* Draw thumbnails */

      /* > Right */
      if (show_right_thumbnail)
      {
         /* Background */
         menu_display_draw_quad(
               video_info,
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
         menu_thumbnail_draw(
               video_info,
               right_thumbnail,
               right_thumbnail_x,
               thumbnail_y,
               (unsigned)thumbnail_box_width,
               (unsigned)thumbnail_box_height,
               MENU_THUMBNAIL_ALIGN_CENTRE,
               ozone->animations.fullscreen_thumbnail_alpha,
               1.0f,
               NULL);
      }

      /* > Left */
      if (show_left_thumbnail)
      {
         /* Background */
         menu_display_draw_quad(
               video_info,
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
         menu_thumbnail_draw(
               video_info,
               left_thumbnail,
               left_thumbnail_x,
               thumbnail_y,
               (unsigned)thumbnail_box_width,
               (unsigned)thumbnail_box_height,
               MENU_THUMBNAIL_ALIGN_CENTRE,
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
