/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
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

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <string/stdstring.h>
#include <limits.h>

#include <retro_log.h>
#include <compat/posix_string.h>
#include <file/file_path.h>

#include "menu_generic.h"

#include "../menu.h"
#include "../menu_driver.h"
#include "../menu_animation.h"
#include "../menu_hash.h"
#include "../menu_display.h"

#include "../../runloop_data.h"

enum
{
   GLUI_TEXTURE_POINTER = 0,
   GLUI_TEXTURE_BACK,
   GLUI_TEXTURE_SWITCH_ON,
   GLUI_TEXTURE_SWITCH_OFF,
   GLUI_TEXTURE_TAB_MAIN_ACTIVE,
   GLUI_TEXTURE_TAB_PLAYLISTS_ACTIVE,
   GLUI_TEXTURE_TAB_SETTINGS_ACTIVE,
   GLUI_TEXTURE_TAB_MAIN_PASSIVE,
   GLUI_TEXTURE_TAB_PLAYLISTS_PASSIVE,
   GLUI_TEXTURE_TAB_SETTINGS_PASSIVE,
   GLUI_TEXTURE_LAST
};

enum
{
   GLUI_SYSTEM_TAB_MAIN = 0,
   GLUI_SYSTEM_TAB_PLAYLISTS,
   GLUI_SYSTEM_TAB_SETTINGS
};

#define GLUI_SYSTEM_TAB_END GLUI_SYSTEM_TAB_SETTINGS

struct materialui_texture_item
{
   GRuint id;
};

typedef struct materialui_handle
{
   unsigned tabs_height;
   unsigned line_height;
   unsigned icon_size;
   unsigned margin;
   unsigned glyph_width;
   char box_message[PATH_MAX_LENGTH];

   struct 
   {
      struct
      {
         float alpha;
      } arrow;

      struct materialui_texture_item bg;
      struct materialui_texture_item list[GLUI_TEXTURE_LAST];
      GRuint white;
   } textures;

   struct
   {
      struct
      {
         unsigned idx;
         unsigned idx_old;
      } active;

      float x_pos;
      size_t selection_ptr_old;
      size_t selection_ptr;
   } categories;

   gfx_font_raster_block_t list_block;
} materialui_handle_t;

static void materialui_context_reset_textures(materialui_handle_t *materialui, const char *iconpath)
{
   unsigned i;

   for (i = 0; i < GLUI_TEXTURE_LAST; i++)
   {
      struct texture_image ti     = {0};
      char path[PATH_MAX_LENGTH]  = {0};

      switch(i)
      {
         case GLUI_TEXTURE_POINTER:
            fill_pathname_join(path, iconpath, "pointer.png",   sizeof(path));
            break;
         case GLUI_TEXTURE_BACK:
            fill_pathname_join(path, iconpath, "back.png",   sizeof(path));
            break;
         case GLUI_TEXTURE_SWITCH_ON:
            fill_pathname_join(path, iconpath, "on.png",   sizeof(path));
            break;
         case GLUI_TEXTURE_SWITCH_OFF:
            fill_pathname_join(path, iconpath, "off.png",   sizeof(path));
            break;
         case GLUI_TEXTURE_TAB_MAIN_ACTIVE:
            fill_pathname_join(path, iconpath, "main_tab_active.png",   sizeof(path));
            break;
         case GLUI_TEXTURE_TAB_PLAYLISTS_ACTIVE:
            fill_pathname_join(path, iconpath, "playlists_tab_active.png",   sizeof(path));
            break;
         case GLUI_TEXTURE_TAB_SETTINGS_ACTIVE:
            fill_pathname_join(path, iconpath, "settings_tab_active.png",   sizeof(path));
            break;
         case GLUI_TEXTURE_TAB_MAIN_PASSIVE:
            fill_pathname_join(path, iconpath, "main_tab_passive.png",   sizeof(path));
            break;
         case GLUI_TEXTURE_TAB_PLAYLISTS_PASSIVE:
            fill_pathname_join(path, iconpath, "playlists_tab_passive.png",   sizeof(path));
            break;
         case GLUI_TEXTURE_TAB_SETTINGS_PASSIVE:
            fill_pathname_join(path, iconpath, "settings_tab_passive.png",   sizeof(path));
            break;
      }

      if (path[0] == '\0' || !path_file_exists(path))
         continue;

      texture_image_load(&ti, path);

      materialui->textures.list[i].id   = menu_display_texture_load(&ti,
            TEXTURE_FILTER_MIPMAP_LINEAR);

      texture_image_free(&ti);
   }
}

static void materialui_draw_icon(materialui_handle_t *materialui,
      GRuint texture,
      float x, float y,
      unsigned width, unsigned height,
      float rotation, float scale_factor,
      GRfloat *color)
{
   struct gfx_coords coords;
   math_matrix_4x4 mymat;

   menu_display_blend_begin();

   menu_display_matrix_4x4_rotate_z(&mymat, rotation, scale_factor, scale_factor, 1, true);

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   menu_display_draw_frame(
         x,
         height - y - materialui->icon_size,
         materialui->icon_size,
         materialui->icon_size,
         &coords, &mymat, texture,
         MENU_DISPLAY_PRIM_TRIANGLESTRIP);

   menu_display_blend_end();
}

static void materialui_blit_line(float x, float y, unsigned width, unsigned height,
      const char *message, uint32_t color, enum text_alignment text_align)
{
   int font_size;
   void *fb_buf              = NULL;
   materialui_handle_t *materialui       = NULL;
   struct font_params params = {0};
   menu_handle_t *menu       = menu_driver_get_ptr();

   if (!menu)
      return;

   materialui = (materialui_handle_t*)menu->userdata;

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_SIZE, &font_size);

   params.x           = x / width;
   params.y           = 1.0f - (y + materialui->line_height / 2 + font_size / 3) 
      / height;
   params.scale       = 1.0;
   params.color       = color;
   params.full_screen = true;
   params.text_align  = text_align;

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_BUF, &fb_buf);

   video_driver_set_osd_msg(message, &params, fb_buf);
}

static void materialui_render_quad(int x, int y, int w, int h,
      unsigned width, unsigned height,
      GRfloat *coord_color)
{
   struct gfx_coords coords;

   menu_handle_t *menu = menu_driver_get_ptr();
   materialui_handle_t *materialui = (materialui_handle_t*)menu->userdata;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = coord_color;

   menu_display_blend_begin();

   menu_display_draw_frame(
         x,
         height - y - h,
         w,
         h,
         &coords, NULL, materialui->textures.white,
         MENU_DISPLAY_PRIM_TRIANGLESTRIP);

   menu_display_blend_end();
}

static void materialui_draw_scrollbar(unsigned width, unsigned height, GRfloat *coord_color)
{
   unsigned header_height;
   float content_height, total_height, scrollbar_width, scrollbar_height, y;
   materialui_handle_t *materialui  = NULL;
   menu_handle_t *menu  = menu_driver_get_ptr();

   if (!menu)
      return;

   menu_display_ctl(MENU_DISPLAY_CTL_HEADER_HEIGHT, &header_height);

   materialui                 = (materialui_handle_t*)menu->userdata;
   content_height       = menu_entries_get_end() * materialui->line_height;
   total_height         = height - header_height - materialui->tabs_height;
   scrollbar_height     = total_height / (content_height / total_height) - (header_height / 6);
   y                    = total_height * menu->scroll_y / content_height;

   if (content_height >= total_height)
   {
      scrollbar_width  = (header_height / 12);
      if (scrollbar_height <= header_height / 12)
         scrollbar_height = header_height / 12;

      materialui_render_quad(
            width - scrollbar_width - (header_height / 12),
            header_height + y + (header_height / 12),
            scrollbar_width,
            scrollbar_height,
            width, height,
            coord_color);
   }
}

static void materialui_get_message(const char *message)
{
   materialui_handle_t *materialui = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   if (!message || !*message)
      return;

   materialui = (materialui_handle_t*)menu->userdata;

   if (materialui)
      strlcpy(materialui->box_message, message, sizeof(materialui->box_message));
}

static void materialui_render_messagebox(const char *message)
{
   unsigned i;
   unsigned width, height;
   uint32_t normal_color;
   int x, y, font_size;
   struct string_list *list = NULL;
   menu_handle_t *menu      = menu_driver_get_ptr();
   settings_t *settings     = config_get_ptr();

   if (!menu || !menu->userdata)
      return;

   list = (struct string_list*)string_split(message, "\n");

   if (!list)
      return;

   if (list->elems == 0)
      goto end;

   video_driver_get_size(&width, &height);

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_SIZE, &font_size);

   x = width  / 2;
   y = height / 2 - list->size * font_size / 2;

   normal_color = FONT_COLOR_ARGB_TO_RGBA(settings->menu.entry_normal_color);

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      if (msg)
         materialui_blit_line(x, y + i * font_size,
               width, height,
               msg, normal_color, TEXT_ALIGN_CENTER);
   }

end:
   string_list_free(list);
}

static void materialui_render(void)
{
   float delta_time, dt;
   int bottom;
   unsigned width, height, header_height;
   materialui_handle_t *materialui                = NULL;
   menu_handle_t *menu                = menu_driver_get_ptr();
   settings_t *settings               = config_get_ptr();

   if (!menu || !menu->userdata)
      return;

   video_driver_get_size(&width, &height);

   materialui = (materialui_handle_t*)menu->userdata;


   menu_animation_ctl(MENU_ANIMATION_CTL_DELTA_TIME, &delta_time);
   dt = delta_time / IDEAL_DT;
   menu_animation_ctl(MENU_ANIMATION_CTL_UPDATE, &dt);

   menu_display_ctl(MENU_DISPLAY_CTL_SET_WIDTH,  &width);
   menu_display_ctl(MENU_DISPLAY_CTL_SET_HEIGHT, &height);
   menu_display_ctl(MENU_DISPLAY_CTL_HEADER_HEIGHT, &header_height);

   if (settings->menu.pointer.enable)
   {
      int16_t pointer_y = menu_input_pointer_state(MENU_POINTER_Y_AXIS);
      float    old_accel_val, new_accel_val;
      unsigned new_pointer_val = 
         (pointer_y - materialui->line_height + menu->scroll_y - 16)
         / materialui->line_height;

      menu_input_ctl(MENU_INPUT_CTL_POINTER_ACCEL_READ, &old_accel_val);
      menu_input_ctl(MENU_INPUT_CTL_POINTER_PTR, &new_pointer_val);

      menu->scroll_y            -= old_accel_val / 60.0;

      new_accel_val = old_accel_val * 0.96;

      menu_input_ctl(MENU_INPUT_CTL_POINTER_ACCEL_WRITE, &new_accel_val);
   }

   if (settings->menu.mouse.enable)
   {
      int16_t mouse_y          = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

      unsigned new_pointer_val = 
         (mouse_y - materialui->line_height + menu->scroll_y - 16)
         / materialui->line_height;
      menu_input_ctl(MENU_INPUT_CTL_MOUSE_PTR, &new_pointer_val);
   }

   if (menu->scroll_y < 0)
      menu->scroll_y = 0;

   bottom = menu_entries_get_end() * materialui->line_height
      - height + header_height + materialui->tabs_height;
   if (menu->scroll_y > bottom)
      menu->scroll_y = bottom;

   if (menu_entries_get_end() * materialui->line_height
         < height - header_height - materialui->tabs_height)
      menu->scroll_y = 0;

   if (menu_entries_get_end() < height / materialui->line_height)
      menu_entries_set_start(0);
   else
      menu_entries_set_start(menu->scroll_y / materialui->line_height);
}

static void materialui_render_label_value(materialui_handle_t *materialui,
      int y, unsigned width, unsigned height,
      uint64_t index, uint32_t color, bool selected, const char *label,
      const char *value, GRfloat *pure_white)
{
   char label_str[PATH_MAX_LENGTH];
   char value_str[PATH_MAX_LENGTH];
   int value_len   = strlen(value);
   int ticker_limit = 0;
   size_t usable_width = 0;
   GRuint texture_switch = 0;
   bool do_draw_text = false;
   uint32_t hash_value = 0;

   label_str[0] = '\0';
   value_str[0] = '\0';

   usable_width = width - (materialui->margin * 2);

   if (value_len * materialui->glyph_width > usable_width / 2)
      value_len = (usable_width/2) / materialui->glyph_width;

   ticker_limit = (usable_width / materialui->glyph_width) - (value_len + 2);

   menu_animation_ticker_str(label_str, ticker_limit, index, label, selected);
   menu_animation_ticker_str(value_str, value_len,    index, value, selected);

   materialui_blit_line(materialui->margin, y, width, height, label_str, color, TEXT_ALIGN_LEFT);

   hash_value = menu_hash_calculate(value);

   if (!strcmp(value, "disabled") ||
         !strcmp(value, "off"))
   {
      if (materialui->textures.list[GLUI_TEXTURE_SWITCH_OFF].id)
         texture_switch = materialui->textures.list[GLUI_TEXTURE_SWITCH_OFF].id;
      else
         do_draw_text = true;
   }
   else if (!strcmp(value, "enabled") ||
         !strcmp(value, "on"))
   {
      if (materialui->textures.list[GLUI_TEXTURE_SWITCH_ON].id)
         texture_switch = materialui->textures.list[GLUI_TEXTURE_SWITCH_ON].id;
      else
         do_draw_text = true;
   }
   else
   {
      switch (hash_value)
      {
         case MENU_VALUE_COMP:
            break;
         case MENU_VALUE_MORE:
            break;
         case MENU_VALUE_CORE:
            break;
         case MENU_VALUE_RDB:
            break;
         case MENU_VALUE_CURSOR:
            break;
         case MENU_VALUE_FILE:
            break;
         case MENU_VALUE_DIR:
            break;
         case MENU_VALUE_MUSIC:
            break;
         case MENU_VALUE_IMAGE:
            break;
         case MENU_VALUE_MOVIE:
            break;
         case MENU_VALUE_ON:
            if (materialui->textures.list[GLUI_TEXTURE_SWITCH_ON].id)
               texture_switch = materialui->textures.list[GLUI_TEXTURE_SWITCH_ON].id;
            else
               do_draw_text = true;
            break;
         case MENU_VALUE_OFF:
            if (materialui->textures.list[GLUI_TEXTURE_SWITCH_OFF].id)
               texture_switch = materialui->textures.list[GLUI_TEXTURE_SWITCH_OFF].id;
            else
               do_draw_text = true;
            break;
         default:
            do_draw_text = true;
            break;
      }
   }

   if (do_draw_text)
      materialui_blit_line(width - materialui->margin, y, width, height, value_str, color, TEXT_ALIGN_RIGHT);

   if (texture_switch)
      materialui_draw_icon(materialui, texture_switch,
            width - materialui->margin - materialui->icon_size, y, width, height, 0, 1, &pure_white[0]);

}

static void materialui_render_menu_list(materialui_handle_t *materialui,
      unsigned width, unsigned height,
      menu_handle_t *menu,
      uint32_t normal_color,
      uint32_t hover_color,
      GRfloat *pure_white)
{
   unsigned header_height;
   size_t i                = 0;
   uint64_t *frame_count   = video_driver_get_frame_count();
   size_t          end     = menu_entries_get_end();

   if (!menu_display_ctl(MENU_DISPLAY_CTL_UPDATE_PENDING, NULL))
      return;

   menu_display_ctl(MENU_DISPLAY_CTL_HEADER_HEIGHT, &header_height);

   materialui->list_block.carr.coords.vertices = 0;

   for (i = menu_entries_get_start(); i < end; i++)
   {
      int y;
      size_t selection;
      bool entry_selected;
      menu_entry_t entry;

      if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
         continue;

      y = header_height - menu->scroll_y + (materialui->line_height * i);

      if ((y - (int)materialui->line_height) > (int)height
            || ((y + (int)materialui->line_height) < 0))
         continue;

      menu_entry_get(&entry, 0, i, NULL, true);

      entry_selected = selection == i;

      materialui_render_label_value(materialui, y, width, height, *frame_count / 40,
         entry_selected ? hover_color : normal_color, entry_selected,
         entry.path, entry.value, pure_white);
   }
}

static void materialui_draw_cursor(materialui_handle_t *materialui,
      GRfloat *color,
      float x, float y, unsigned width, unsigned height)
{
   struct gfx_coords coords;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   menu_display_blend_begin();

   menu_display_draw_frame(
         x - 32,
         height - y - 32,
         64,
         64,
         &coords, NULL,
         materialui->textures.list[GLUI_TEXTURE_POINTER].id,
         MENU_DISPLAY_PRIM_TRIANGLESTRIP);

   menu_display_blend_end();
}

static size_t materialui_list_get_size(void *data, menu_list_type_t type)
{
   size_t list_size        = 0;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         list_size  = menu_entries_get_stack_size(0);
         break;
      case MENU_LIST_TABS:
         list_size = GLUI_SYSTEM_TAB_END;
         break;
      default:
         break;
   }

   return list_size;
}

static void materialui_frame(void)
{
   unsigned header_height;
   bool display_kb;
   GRfloat black_bg[16] = {
      0, 0, 0, 0.75,
      0, 0, 0, 0.75,
      0, 0, 0, 0.75,
      0, 0, 0, 0.75,
   };
   GRfloat blue_bg[16] = {
      0.13, 0.59, 0.95, 1,
      0.13, 0.59, 0.95, 1,
      0.13, 0.59, 0.95, 1,
      0.13, 0.59, 0.95, 1,
   };
   GRfloat lightblue_bg[16] = {
      0.89, 0.95, 0.99, 1.00,
      0.89, 0.95, 0.99, 1.00,
      0.89, 0.95, 0.99, 1.00,
      0.89, 0.95, 0.99, 1.00,
   };
   GRfloat pure_white[16]=  {
      1, 1, 1, 1,
      1, 1, 1, 1,
      1, 1, 1, 1,
      1, 1, 1, 1,
   };
   GRfloat white_bg[16]=  {
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
      0.98, 0.98, 0.98, 1,
   };
   GRfloat white_transp_bg[16]=  {
      0.98, 0.98, 0.98, 0.90,
      0.98, 0.98, 0.98, 0.90,
      0.98, 0.98, 0.98, 0.90,
      0.98, 0.98, 0.98, 0.90,
   };
   GRfloat grey_bg[16]=  {
      0.78, 0.78, 0.78, 1,
      0.78, 0.78, 0.78, 1,
      0.78, 0.78, 0.78, 1,
      0.78, 0.78, 0.78, 1,
   };
   GRfloat shadow_bg[16]=  {
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0.2,
      0, 0, 0, 0.2,
   };
   unsigned width, height, ticker_limit, i, tab_width;
   char msg[PATH_MAX_LENGTH];
   char title[PATH_MAX_LENGTH];
   char title_buf[PATH_MAX_LENGTH];
   size_t selection;
   size_t title_margin;
   materialui_handle_t *materialui                     = NULL;
   driver_t *driver                        = driver_get_ptr();
   menu_handle_t *menu                     = menu_driver_get_ptr();
   settings_t *settings                    = config_get_ptr();
   uint64_t *frame_count                   = video_driver_get_frame_count();
   const uint32_t normal_color             = 0x212121ff;
   const uint32_t hover_color              = 0x212121ff;
   const uint32_t title_color              = 0xffffffff;
   const uint32_t activetab_color          = 0x0096f2ff;
   const uint32_t passivetab_color         = 0x9e9e9eff;
   bool background_rendered                = false;
   bool libretro_running                   = menu_display_ctl(MENU_DISPLAY_CTL_LIBRETRO_RUNNING, NULL);

   (void)passivetab_color;
   (void)activetab_color;

   if (!menu || !menu->userdata)
      return;

   materialui = (materialui_handle_t*)menu->userdata;

   msg[0]       = '\0';
   title[0]     = '\0';
   title_buf[0] = '\0';

   video_driver_get_size(&width, &height);

   menu_display_ctl(MENU_DISPLAY_CTL_SET_VIEWPORT, NULL);
   menu_display_ctl(MENU_DISPLAY_CTL_HEADER_HEIGHT, &header_height);


   if (libretro_running)
   {
      menu_display_frame_background(
            width, height,
            materialui->textures.white, 0.75f, false,
            &white_transp_bg[0],   &white_bg[0],
            NULL, NULL, 4,
            MENU_DISPLAY_PRIM_TRIANGLESTRIP);
   }
   else
   {
      menu_display_clear_color(1.0f, 1.0f, 1.0f, 0.75f);

      if (materialui->textures.bg.id)
      {
         background_rendered = true;

         /* Set new opacity for transposed white background */

         white_transp_bg[3]  = 0.30;
         white_transp_bg[7]  = 0.30;
         white_transp_bg[11] = 0.30;
         white_transp_bg[15] = 0.30;

         menu_display_frame_background(
               width, height,
               materialui->textures.bg.id, 0.75f, true,
               &white_transp_bg[0],   &white_bg[0],
               NULL, NULL, 4,
               MENU_DISPLAY_PRIM_TRIANGLESTRIP);

         /* Restore opacity of transposed white background */

         white_transp_bg[3]  = 0.90;
         white_transp_bg[7]  = 0.90;
         white_transp_bg[11] = 0.90;
         white_transp_bg[15] = 0.90;
      }
   }

   menu_entries_get_title(title, sizeof(title));

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   if (background_rendered || libretro_running)
   {
      lightblue_bg[3]  = 0.75;
      lightblue_bg[7]  = 0.75;
      lightblue_bg[11] = 0.75;
      lightblue_bg[15] = 0.75;
   }
   else
   {
      lightblue_bg[3]  = 1.00;
      lightblue_bg[7]  = 1.00;
      lightblue_bg[11] = 1.00;
      lightblue_bg[15] = 1.00;
   }
   /* highlighted entry */
   materialui_render_quad(0,
         header_height - menu->scroll_y + materialui->line_height *
         selection, width, materialui->line_height,
         width, height,
         &lightblue_bg[0]);

   menu_display_font_bind_block(menu, driver->font_osd_driver, &materialui->list_block);

   materialui_render_menu_list(materialui, width, height, menu, normal_color, hover_color, &pure_white[0]);

   menu_display_font_flush_block(menu, driver->font_osd_driver);

   menu_animation_ctl(MENU_ANIMATION_CTL_SET_ACTIVE, NULL);

   /* header */
   materialui_render_quad( 0, 0, width,
         header_height,
         width, height,
         &blue_bg[0]);

   /* display tabs if depth equal one, if not hide them */
   if (materialui_list_get_size(menu, MENU_LIST_PLAIN) == 1)
   {
      float scale_factor;
      menu_display_ctl(MENU_DISPLAY_CTL_GET_DPI, &scale_factor);

      materialui->tabs_height            = scale_factor / 3;

      /* tabs background */
      materialui_render_quad(0, height - materialui->tabs_height, width,
            materialui->tabs_height,
            width, height,
            &white_bg[0]);

      /* tabs separator */
      materialui_render_quad(0, height - materialui->tabs_height, width,
            1,
            width, height,
            &grey_bg[0]);

      for (i = 0; i <= GLUI_SYSTEM_TAB_END; i++)
      {
         unsigned tab_icon = GLUI_TEXTURE_TAB_MAIN_PASSIVE;
         switch (i)
         {
            case GLUI_SYSTEM_TAB_MAIN:
               tab_icon = (i == materialui->categories.selection_ptr)
                        ? GLUI_TEXTURE_TAB_MAIN_ACTIVE
                        : GLUI_TEXTURE_TAB_MAIN_PASSIVE;
               break;
            case GLUI_SYSTEM_TAB_PLAYLISTS:
               tab_icon = (i == materialui->categories.selection_ptr)
                        ? GLUI_TEXTURE_TAB_PLAYLISTS_ACTIVE
                        : GLUI_TEXTURE_TAB_PLAYLISTS_PASSIVE;
               break;
            case GLUI_SYSTEM_TAB_SETTINGS:
               tab_icon = (i == materialui->categories.selection_ptr)
                        ? GLUI_TEXTURE_TAB_SETTINGS_ACTIVE
                        : GLUI_TEXTURE_TAB_SETTINGS_PASSIVE;
               break;
         }

         materialui_draw_icon(materialui, materialui->textures.list[tab_icon].id,
               width / (GLUI_SYSTEM_TAB_END+1) * (i+0.5) - materialui->icon_size/2,
               height - materialui->tabs_height,
               width, height, 0, 1, &pure_white[0]);
      }

      /* active tab marker */
      tab_width = width / (GLUI_SYSTEM_TAB_END+1);
      materialui_render_quad(materialui->categories.selection_ptr * tab_width,
            height - (header_height/16),
            tab_width,
            header_height/16,
            width, height,
            &blue_bg[0]);
   }
   else
   {
      materialui->tabs_height = 0;
   }

   materialui_render_quad(0, header_height, width,
         header_height/12,
         width, height,
         &shadow_bg[0]);

   title_margin = materialui->margin;

   if (menu_entries_show_back())
   {
      title_margin = materialui->icon_size;
      materialui_draw_icon(materialui, materialui->textures.list[GLUI_TEXTURE_BACK].id,
         0, 0, width, height, 0, 1, &pure_white[0]);
   }

   ticker_limit = (width - materialui->margin*2) / materialui->glyph_width;
   menu_animation_ticker_str(title_buf, ticker_limit,
         *frame_count / 100, title, true);

   /* Title */
   materialui_blit_line(title_margin, 0, width, height, title_buf,
         title_color, TEXT_ALIGN_LEFT);

   materialui_draw_scrollbar(width, height, &grey_bg[0]);

   menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_DISPLAY, &display_kb);

   if (display_kb)
   {
      const char *str = NULL, *label = NULL;
      menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_BUFF_PTR, &str);
      menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_LABEL,    &label);

      if (!str)
         str = "";
      materialui_render_quad(0, 0, width, height, width, height, &black_bg[0]);
      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
      materialui_render_messagebox(msg);
   }

   if (materialui->box_message[0] != '\0')
   {
      materialui_render_quad(0, 0, width, height, width, height, &black_bg[0]);
      materialui_render_messagebox(materialui->box_message);
      materialui->box_message[0] = '\0';
   }

   if (settings->menu.mouse.enable && (settings->video.fullscreen || !video_driver_has_windowed()))
   {
      int16_t mouse_x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
      int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

      materialui_draw_cursor(materialui, &white_bg[0], mouse_x, mouse_y, width, height);
   }

   menu_display_restore_clear_color();
   menu_display_ctl(MENU_DISPLAY_CTL_UNSET_VIEWPORT, NULL);
}

static void materialui_allocate_white_texture(materialui_handle_t *materialui)
{
   struct texture_image ti;
   static const uint8_t white_data[] = { 0xff, 0xff, 0xff, 0xff };

   ti.width  = 1;
   ti.height = 1;
   ti.pixels = (uint32_t*)&white_data;

   materialui->textures.white   = menu_display_texture_load(&ti,
         TEXTURE_FILTER_NEAREST);
}

static void materialui_font(menu_handle_t *menu)
{
   int font_size;
   char mediapath[PATH_MAX_LENGTH], fontpath[PATH_MAX_LENGTH];
   settings_t *settings = config_get_ptr();

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_SIZE, &font_size);

   fill_pathname_join(mediapath, settings->assets_directory, "glui", sizeof(mediapath));
   fill_pathname_join(fontpath, mediapath, "Roboto-Regular.ttf", sizeof(fontpath));

   if (!menu_display_init_main_font(menu, fontpath, font_size))
      RARCH_WARN("Failed to load font.");
}

static void materialui_layout(menu_handle_t *menu, materialui_handle_t *materialui)
{
   void *fb_buf;
   float scale_factor;
   int new_font_size;
   unsigned width, height, new_header_height;

   video_driver_get_size(&width, &height);

   /* Mobiles platforms may have very small display metrics coupled to a high
      resolution, so we should be dpi aware to ensure the entries hitboxes are big
      enough. On desktops, we just care about readability, with every widget size
      proportional to the display width. */
   menu_display_ctl(MENU_DISPLAY_CTL_GET_DPI, &scale_factor);

   new_header_height            = scale_factor / 3;
   new_font_size                = scale_factor / 9;

   materialui->tabs_height            = scale_factor / 3;
   materialui->line_height            = scale_factor / 3;
   materialui->margin                 = scale_factor / 9;
   materialui->icon_size              = scale_factor / 3;

   menu_display_ctl(MENU_DISPLAY_CTL_SET_HEADER_HEIGHT, &new_header_height);
   menu_display_ctl(MENU_DISPLAY_CTL_SET_FONT_SIZE,     &new_font_size);

   /* we assume the average glyph aspect ratio is close to 3:4 */
   materialui->glyph_width            = new_font_size * 3/4;

   materialui_font(menu);

   menu_display_ctl(MENU_DISPLAY_CTL_FONT_BUF, &fb_buf);

   if (fb_buf) /* calculate a more realistic ticker_limit */
   {
      driver_t *driver   = driver_get_ptr();
      int m_width = driver->font_osd_driver->get_message_width(fb_buf, "a", 1, 1);

      if (m_width)
         materialui->glyph_width = m_width;
   }
}

static void *materialui_init(void)
{
   materialui_handle_t *materialui                     = NULL;
   menu_handle_t     *menu                 = (menu_handle_t*)
      calloc(1, sizeof(*menu));

   if (!menu)
      goto error;

   if (!menu_display_check_compatibility((enum menu_display_driver_type)menu_ctx_materialui.type))
      goto error;

   menu->userdata       = (materialui_handle_t*)calloc(1, sizeof(materialui_handle_t));

   if (!menu->userdata)
      goto error;

   materialui                         = (materialui_handle_t*)menu->userdata;

   materialui_layout(menu, materialui);
   materialui_allocate_white_texture(materialui);

   return menu;
error:
   if (menu)
      free(menu);
   return NULL;
}

static void materialui_free(void *data)
{
   menu_handle_t *menu                     = (menu_handle_t*)data;
   driver_t      *driver                   = driver_get_ptr();
   materialui_handle_t *materialui                     = (materialui_handle_t*)menu->userdata;
   const struct font_renderer *font_driver = driver ? (const struct font_renderer*)
      driver->font_osd_driver : NULL;

   if (!materialui || !menu)
      return;

   gfx_coord_array_free(&materialui->list_block.carr);

   if (font_driver && font_driver->bind_block)
      font_driver->bind_block(driver->font_osd_data, NULL);

   if (menu->userdata)
      free(menu->userdata);
   menu->userdata = NULL;
}

static void materialui_context_bg_destroy(materialui_handle_t *materialui)
{
   if (!materialui)
      return;

   menu_display_texture_unload((uintptr_t*)&materialui->textures.bg.id);
   menu_display_texture_unload((uintptr_t*)&materialui->textures.white);
}

static void materialui_context_destroy(void)
{
   unsigned i;
   materialui_handle_t *materialui   = NULL;
   menu_handle_t *menu   = menu_driver_get_ptr();

   if (!menu || !menu->userdata)
      return;

   materialui = (materialui_handle_t*)menu->userdata;

   if (!materialui)
      return;

   for (i = 0; i < GLUI_TEXTURE_LAST; i++)
      menu_display_texture_unload((uintptr_t*)&materialui->textures.list[i].id);

   menu_display_free_main_font();

   materialui_context_bg_destroy(materialui);
}

static bool materialui_load_image(void *data, menu_image_type_t type)
{
   materialui_handle_t *materialui = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu || !menu->userdata)
      return false;

   materialui = (materialui_handle_t*)menu->userdata;

   switch (type)
   {
      case MENU_IMAGE_NONE:
         break;
      case MENU_IMAGE_WALLPAPER:
         materialui_context_bg_destroy(materialui);

         materialui->textures.bg.id   = menu_display_texture_load(data,
               TEXTURE_FILTER_MIPMAP_LINEAR);
         materialui_allocate_white_texture(materialui);
         break;
      case MENU_IMAGE_BOXART:
         break;
   }

   return true;
}

static float materialui_get_scroll(void)
{
   size_t selection;
   unsigned width, height;
   int half = 0;
   materialui_handle_t *materialui    = NULL;
   menu_handle_t *menu    = menu_driver_get_ptr();

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return 0;

   if (!menu || !menu->userdata)
      return 0;

   video_driver_get_size(&width, &height);

   materialui = (materialui_handle_t*)menu->userdata;
   if (materialui->line_height)
      half = (height / materialui->line_height) / 2;

   if (selection < (unsigned)half)
      return 0;
   return ((selection + 2 - half) * materialui->line_height);
}

static void materialui_navigation_set(bool scroll)
{
   menu_handle_t *menu  = menu_driver_get_ptr();
   float     scroll_pos = materialui_get_scroll();

   if (!menu || !scroll)
      return;

   menu_animation_push(10, scroll_pos,
         &menu->scroll_y, EASING_IN_OUT_QUAD, -1, NULL);
}

static void  materialui_list_set_selection(file_list_t *list)
{
   materialui_navigation_set(true);
}

static void materialui_navigation_clear(bool pending_push)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return;

   menu_entries_set_start(0);
   menu->scroll_y      = 0;
}

static void materialui_navigation_set_last(void)
{
   materialui_navigation_set(true);
}

static void materialui_navigation_alphabet(size_t *unused)
{
   materialui_navigation_set(true);
}

static void materialui_populate_entries(const char *path,
      const char *label, unsigned i)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return;

   menu->scroll_y      = materialui_get_scroll();
}

static void materialui_context_reset(void)
{
   char iconpath[PATH_MAX_LENGTH]  = {0};
   materialui_handle_t *materialui   = NULL;
   menu_handle_t *menu   = menu_driver_get_ptr();
   settings_t *settings  = config_get_ptr();

   if (!menu || !menu->userdata || !settings)
      return;

   materialui      = (materialui_handle_t*)menu->userdata;
   if (!materialui)
      return;

   fill_pathname_join(iconpath, settings->assets_directory,
         "glui", sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

   materialui_layout(menu, materialui);
   materialui_context_bg_destroy(materialui);
   materialui_allocate_white_texture(materialui);
   materialui_context_reset_textures(materialui, iconpath);

   rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE,
         settings->menu.wallpaper, "cb_menu_wallpaper", 0, 1, true);
}

static int materialui_environ(menu_environ_cb_t type, void *data)
{
   switch (type)
   {
      case 0:
      default:
         break;
   }

   return -1;
}

static void materialui_preswitch_tabs(unsigned action)
{
   materialui_handle_t *materialui = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   materialui = (materialui_handle_t*)menu->userdata;

   if (!materialui)
      return;

   file_list_t *menu_stack = menu_entries_get_menu_stack_ptr(0);
   size_t stack_size = menu_stack->size;

   if (menu_stack->list[stack_size - 1].label)
      free(menu_stack->list[stack_size - 1].label);
   menu_stack->list[stack_size - 1].label = NULL;

   switch (materialui->categories.selection_ptr)
   {
      case GLUI_SYSTEM_TAB_MAIN:
         menu_stack->list[stack_size - 1].label = 
            strdup(menu_hash_to_str(MENU_VALUE_MAIN_MENU));
         menu_stack->list[stack_size - 1].type = 
            MENU_SETTINGS;
         break;
      case GLUI_SYSTEM_TAB_PLAYLISTS:
         menu_stack->list[stack_size - 1].label = 
            strdup(menu_hash_to_str(MENU_VALUE_PLAYLISTS_TAB));
         menu_stack->list[stack_size - 1].label = 
            strdup(menu_hash_to_str(MENU_VALUE_PLAYLISTS_TAB));
         menu_stack->list[stack_size - 1].type = 
            MENU_PLAYLISTS_TAB;
         break;
      case GLUI_SYSTEM_TAB_SETTINGS:
         menu_stack->list[stack_size - 1].label = 
            strdup(menu_hash_to_str(MENU_VALUE_SETTINGS_TAB));
         menu_stack->list[stack_size - 1].type = 
            MENU_SETTINGS;
         break;
   }
}

static void materialui_list_cache(menu_list_type_t type, unsigned action)
{
   size_t list_size;
   materialui_handle_t      *materialui = NULL;
   menu_handle_t      *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   materialui = (materialui_handle_t*)menu->userdata;

   if (!materialui)
      return;

   list_size = GLUI_SYSTEM_TAB_END;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         break;
      case MENU_LIST_HORIZONTAL:
         materialui->categories.selection_ptr_old = materialui->categories.selection_ptr;

         switch (action)
         {
            case MENU_ACTION_LEFT:
               if (materialui->categories.selection_ptr == 0)
               {
                  materialui->categories.selection_ptr = list_size;
                  materialui->categories.active.idx = list_size - 1;
               }
               else
                  materialui->categories.selection_ptr--;
               break;
            default:
               if (materialui->categories.selection_ptr == list_size)
               {
                  materialui->categories.selection_ptr = 0;
                  materialui->categories.active.idx = 1;
               }
               else
                  materialui->categories.selection_ptr++;
               break;
         }

         materialui_preswitch_tabs(action);
         break;
      default:
         break;
   }
}

static int materialui_list_push(menu_displaylist_info_t *info, unsigned type)
{
   int ret = -1;
   menu_handle_t *menu   = menu_driver_get_ptr();
   global_t    *global   = global_get_ptr();

   switch (type)
   {
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         menu_entries_clear(info->list);
         menu_entries_push(info->list,
               menu_hash_to_str(MENU_LABEL_VALUE_LOAD_CONTENT),
               menu_hash_to_str(MENU_LABEL_LOAD_CONTENT),
               MENU_SETTING_ACTION, 0, 0);

         if (core_info_list_num_info_files(global->core_info.list))
         {
            menu_entries_push(info->list,
                  menu_hash_to_str(MENU_LABEL_VALUE_DETECT_CORE_LIST),
                  menu_hash_to_str(MENU_LABEL_DETECT_CORE_LIST),
                  MENU_SETTING_ACTION, 0, 0);

            menu_entries_push(info->list,
                  menu_hash_to_str(MENU_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                  menu_hash_to_str(MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                  MENU_SETTING_ACTION, 0, 0);
         }

         info->need_push    = true;
         info->need_refresh = true;
         ret = 0;
         break;
      case DISPLAYLIST_MAIN_MENU:
         menu_entries_clear(info->list);

         if (global->inited.main && (global->inited.core.type != CORE_TYPE_DUMMY))
            menu_displaylist_parse_settings(menu, info,
                  menu_hash_to_str(MENU_LABEL_CONTENT_SETTINGS), PARSE_ACTION, false);

#if defined(HAVE_DYNAMIC) || defined(HAVE_LIBRETRO_MANAGEMENT)
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_CORE_LIST), PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_LOAD_CONTENT_LIST), PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_LOAD_CONTENT_HISTORY), PARSE_ACTION, false);
#if defined(HAVE_NETWORKING)
#if defined(HAVE_LIBRETRODB)
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_ADD_CONTENT_LIST), PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_ONLINE_UPDATER), PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_INFORMATION_LIST), PARSE_ACTION, false);
#ifndef HAVE_DYNAMIC
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_RESTART_RETROARCH), PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_CONFIGURATIONS), PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_SAVE_NEW_CONFIG), PARSE_ACTION, false);
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_HELP_LIST), PARSE_ACTION, false);
#if !defined(IOS)
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_QUIT_RETROARCH), PARSE_ACTION, false);
#endif
         menu_displaylist_parse_settings(menu, info,
               menu_hash_to_str(MENU_LABEL_SHUTDOWN), PARSE_ACTION, false);
         info->need_push    = true;
         ret = 0;
         break;
   }
   return ret;
}

static size_t materialui_list_get_selection(void *data)
{
   menu_handle_t *menu    = (menu_handle_t*)data;
   materialui_handle_t *materialui      = menu ? (materialui_handle_t*)menu->userdata : NULL;

   if (!materialui)
      return 0;

   return materialui->categories.selection_ptr;
}

static int materialui_pointer_tap(unsigned x, unsigned y, 
      unsigned ptr, menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   size_t selection, idx;
   unsigned header_height, width, height, i;
   bool scroll                = false;
   menu_handle_t *menu        = menu_driver_get_ptr();
   materialui_handle_t *materialui        = menu ? (materialui_handle_t*)menu->userdata : NULL;
   file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   if (!materialui)
      return 0;

   video_driver_get_size(&width, &height);

   menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);
   menu_display_ctl(MENU_DISPLAY_CTL_HEADER_HEIGHT, &header_height);

   if (y < header_height)
   {
      menu_entries_pop_stack(&selection, 0);
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &selection);
   }
   else if (y > height - materialui->tabs_height)
   {
      for (i = 0; i <= GLUI_SYSTEM_TAB_END; i++)
      {
         unsigned tab_width = width / (GLUI_SYSTEM_TAB_END + 1);
         unsigned start = tab_width * i;

         if ((x >= start) && (x < (start + tab_width)))
         {
            materialui->categories.selection_ptr = i;

            materialui_preswitch_tabs(action);

            if (cbs && cbs->action_content_list_switch)
               return cbs->action_content_list_switch(selection_buf, menu_stack,
                     "", "", 0);
         }
      }
   }
   else if (ptr <= (menu_entries_get_size() - 1))
   {
      if (ptr == selection && cbs && cbs->action_select)
         return menu_entry_action(entry, selection, MENU_ACTION_SELECT);

      idx  = ptr;

      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &idx);
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET, &scroll);
   }

   return 0;
}

menu_ctx_driver_t menu_ctx_materialui = {
   NULL,
   materialui_get_message,
   generic_menu_iterate,
   materialui_render,
   materialui_frame,
   materialui_init,
   materialui_free,
   materialui_context_reset,
   materialui_context_destroy,
   materialui_populate_entries,
   NULL,
   materialui_navigation_clear,
   NULL,
   NULL,
   materialui_navigation_set,
   materialui_navigation_set_last,
   materialui_navigation_alphabet,
   materialui_navigation_alphabet,
   generic_menu_init_list,
   NULL,
   NULL,
   NULL,
   materialui_list_cache,
   materialui_list_push,
   materialui_list_get_selection,
   materialui_list_get_size,
   NULL,
   materialui_list_set_selection,
   NULL,
   materialui_load_image,
   "glui",
   MENU_VIDEO_DRIVER_OPENGL,
   materialui_environ,
   materialui_pointer_tap,
};
