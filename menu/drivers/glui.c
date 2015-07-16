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
#include <limits.h>

#include <retro_log.h>
#include <compat/posix_string.h>
#include <file/file_path.h>

#include "../menu.h"
#include "../menu_driver.h"
#include "../menu_hash.h"
#include "../menu_entry.h"
#include "../menu_display.h"
#include "../menu_video.h"

#include "../../gfx/video_thread_wrapper.h"
#include "../../gfx/font_driver.h"
#include "../../gfx/video_texture.h"

#include "../../runloop_data.h"

typedef struct glui_handle
{
   unsigned line_height;
   unsigned margin;
   unsigned glyph_width;
   char box_message[PATH_MAX_LENGTH];

   struct
   {
      struct
      {
         GRuint id;
         char path[PATH_MAX_LENGTH];
      } bg;
      GRuint white;
   } textures;

   gfx_font_raster_block_t list_block;
} glui_handle_t;

static void glui_blit_line(float x, float y,
      const char *message, uint32_t color, enum text_alignment text_align)
{
   unsigned width, height;
   glui_handle_t *glui       = NULL;
   struct font_params params = {0};
   menu_handle_t *menu       = menu_driver_get_ptr();
   menu_display_t *disp      = menu_display_get_ptr();

   if (!menu)
      return;

   video_driver_get_size(&width, &height);

   glui = (glui_handle_t*)menu->userdata;

   params.x           = x / width;
   params.y           = 1.0f - (y + glui->line_height/2 + disp->font.size/3) 
      / height;
   params.scale       = 1.0;
   params.color       = color;
   params.full_screen = true;
   params.text_align  = text_align;

   video_driver_set_osd_msg(message, &params, disp->font.buf);
}

static void glui_render_quad(gl_t *gl, int x, int y, int w, int h,
      float r, float g, float b, float a)
{
   unsigned width, height;
   struct gfx_coords coords;
   GRfloat color[16], tex_coord[8], vertex[8];
   menu_handle_t *menu = menu_driver_get_ptr();
   glui_handle_t *glui = (glui_handle_t*)menu->userdata;

   vertex[0] = 0;
   vertex[1] = 0;
   vertex[2] = 1;
   vertex[3] = 0;
   vertex[4] = 0;
   vertex[5] = 1;
   vertex[6] = 1;
   vertex[7] = 1;

   tex_coord[0] = 0;
   tex_coord[1] = 1;
   tex_coord[2] = 1;
   tex_coord[3] = 1;
   tex_coord[4] = 0;
   tex_coord[5] = 0;
   tex_coord[6] = 1;
   tex_coord[7] = 0;

   color[ 0]    = r;
   color[ 1]    = g;
   color[ 2]    = b;
   color[ 3]    = a;
   color[ 4]    = r;
   color[ 5]    = g;
   color[ 6]    = b;
   color[ 7]    = a;
   color[ 8]    = r;
   color[ 9]    = g;
   color[10]    = b;
   color[11]    = a;
   color[12]    = r;
   color[13]    = g;
   color[14]    = b;
   color[15]    = a;

   video_driver_get_size(&width, &height);

   glViewport(x, height - y - h, w, h);

   coords.vertices      = 4;
   coords.vertex        = vertex;
   coords.tex_coord     = tex_coord;
   coords.lut_tex_coord = tex_coord;

   coords.color = color;

   menu_video_draw_frame(gl->shader, &coords,
         &gl->mvp_no_rot, true, glui->textures.white);

   gl->coords.color = gl->white_color_ptr;
}

static void glui_draw_cursor(gl_t *gl, float x, float y)
{
   glui_render_quad(gl, x-5, y-5, 10, 10, 1, 1, 1, 1);
}

static void glui_draw_scrollbar(gl_t *gl)
{
   unsigned width, height;
   float content_height, total_height, scrollbar_height, y;
   int scrollbar_width  = 4;
   glui_handle_t *glui  = NULL;
   menu_handle_t *menu  = menu_driver_get_ptr();
   menu_display_t *disp = menu_display_get_ptr();

   if (!menu)
      return;

   video_driver_get_size(&width, &height);

   glui                 = (glui_handle_t*)menu->userdata;
   content_height       = menu_entries_get_end() * glui->line_height;
   total_height         = height - disp->header_height * 2;
   scrollbar_height     = total_height / (content_height / total_height);
   y                    = total_height * menu->scroll_y / content_height;

   if (content_height < total_height)
      return;

   glui_render_quad(gl,
         width - scrollbar_width,
         disp->header_height + y,
         scrollbar_width,
         scrollbar_height,
         1, 1, 1, 1);
}

static void glui_get_message(const char *message)
{
   glui_handle_t *glui = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   if (!message || !*message)
      return;

   glui = (glui_handle_t*)menu->userdata;

   if (glui)
      strlcpy(glui->box_message, message, sizeof(glui->box_message));
}

static void glui_render_messagebox(const char *message)
{
   unsigned i;
   unsigned width, height;
   uint32_t normal_color;
   int x, y;
   struct string_list *list = NULL;
   menu_handle_t *menu      = menu_driver_get_ptr();
   menu_display_t *disp     = menu_display_get_ptr();
   settings_t *settings     = config_get_ptr();

   if (!menu || !disp || !menu->userdata)
      return;

   list = (struct string_list*)string_split(message, "\n");

   if (!list)
      return;

   if (list->elems == 0)
      goto end;

   video_driver_get_size(&width, &height);

   x = width  / 2;
   y = height / 2 - list->size * disp->font.size / 2;

   normal_color = FONT_COLOR_ARGB_TO_RGBA(settings->menu.entry_normal_color);

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      if (msg)
         glui_blit_line(x, y + i * disp->font.size,
               msg, normal_color, TEXT_ALIGN_CENTER);
   }

end:
   string_list_free(list);
}

static void glui_render(void)
{
   int bottom;
   unsigned width, height;
   glui_handle_t *glui                = NULL;
   menu_display_t *disp               = menu_display_get_ptr();
   menu_framebuf_t *frame_buf         = menu_display_fb_get_ptr();
   menu_handle_t *menu                = menu_driver_get_ptr();
   menu_input_t *menu_input           = menu_input_get_ptr();
   settings_t *settings               = config_get_ptr();

   if (!menu || !menu->userdata)
      return;

   video_driver_get_size(&width, &height);

   glui = (glui_handle_t*)menu->userdata;

   menu_animation_update(disp->animation,
         disp->animation->delta_time / IDEAL_DT);

   /* TODO/FIXME - we don't use framebuffer at all
    * for GLUI, we should refactor this dependency
    * away. */
   frame_buf->width  = width;
   frame_buf->height = height;

   if (settings->menu.pointer.enable)
   {
      menu_input->pointer.ptr =
         (menu_input->pointer.y - glui->line_height + menu->scroll_y - 16)
         / glui->line_height;

      menu->scroll_y -= menu_input->pointer.accel / 60.0;
      menu_input->pointer.accel = menu_input->pointer.accel * 0.96;
   }

   if (settings->menu.mouse.enable)
   {
      if (menu_input->mouse.scrolldown)
         menu->scroll_y += 10;

      if (menu_input->mouse.scrollup)
         menu->scroll_y -= 10;

      menu_input->mouse.ptr =
         (menu_input->mouse.y - glui->line_height + menu->scroll_y - 16)
         / glui->line_height;
   }

   if (menu->scroll_y < 0)
      menu->scroll_y = 0;

   bottom = menu_entries_get_end() * glui->line_height
      - height + disp->header_height * 2;
   if (menu->scroll_y > bottom)
      menu->scroll_y = bottom;

   if (menu_entries_get_end() * glui->line_height
         < height - disp->header_height*2)
      menu->scroll_y = 0;

   if (menu_entries_get_end() < height / glui->line_height)
      menu_entries_set_start(0);
   else
      menu_entries_set_start(menu->scroll_y / glui->line_height);
}

static void glui_render_label_value(glui_handle_t *glui, int y, unsigned width,
    uint64_t index, uint32_t color, bool selected, const char *label, const char *value)
{
   char label_str[PATH_MAX_LENGTH];
   char value_str[PATH_MAX_LENGTH];
   int value_len   = strlen(value);
   int ticker_limit = 0;
   int usable_width = 0;

   label_str[0] = '\0';
   value_str[0] = '\0';

   usable_width = width - (glui->margin * 2);

   if (value_len * glui->glyph_width > usable_width / 2)
      value_len = (usable_width/2) / glui->glyph_width;

   ticker_limit = (usable_width / glui->glyph_width) - (value_len + 2);

   menu_animation_ticker_str(label_str, ticker_limit, index, label, selected);
   menu_animation_ticker_str(value_str, value_len,    index, value, selected);

   glui_blit_line(glui->margin, y, label_str, color, TEXT_ALIGN_LEFT);
   glui_blit_line(width - glui->margin, y, value_str, color, TEXT_ALIGN_RIGHT);
}

static void glui_render_menu_list(glui_handle_t *glui,
      menu_handle_t *menu,
      uint32_t normal_color,
      uint32_t hover_color)
{
   unsigned width, height;
   size_t i                = 0;
   uint64_t frame_count    = video_driver_get_frame_count();
   size_t          end     = menu_entries_get_end();
   menu_display_t *disp    = menu_display_get_ptr();
   menu_entries_t *entries = menu_entries_get_ptr();

   if (!menu_display_update_pending())
      return;

   video_driver_get_size(&width, &height);

   glui->list_block.carr.coords.vertices = 0;

   for (i = menu_entries_get_start(); i < end; i++)
   {
      bool entry_selected;
      menu_entry_t entry;

      int y = disp->header_height - menu->scroll_y + (glui->line_height * i);

      if (y > (int)height || ((y + (int)glui->line_height) < 0))
         continue;

      menu_entries_get(i, &entry);

      entry_selected = entries->navigation.selection_ptr == i;

      glui_render_label_value(glui, y, width, frame_count / 40,
         entry_selected ? hover_color : normal_color, entry_selected,
         entry.path, entry.value);
   }
}

static void glui_frame(void)
{
   unsigned width, height, ticker_limit;
   char title[PATH_MAX_LENGTH];
   char title_buf[PATH_MAX_LENGTH];
   char title_msg[PATH_MAX_LENGTH];
   char timedate[PATH_MAX_LENGTH];
   gl_t *gl                                = NULL;
   glui_handle_t *glui                     = NULL;
   const struct font_renderer *font_driver = NULL;
   driver_t *driver                        = driver_get_ptr();
   menu_handle_t *menu                     = menu_driver_get_ptr();
   menu_animation_t *anim                  = menu_animation_get_ptr();
   menu_navigation_t *nav                  = menu_navigation_get_ptr();
   menu_display_t *disp                    = menu_display_get_ptr();
   settings_t *settings                    = config_get_ptr();
   menu_input_t *menu_input                = menu_input_get_ptr();
   uint64_t frame_count                    = video_driver_get_frame_count();
   const uint32_t normal_color             = FONT_COLOR_ARGB_TO_RGBA(
         settings->menu.entry_normal_color);
   const uint32_t hover_color              = FONT_COLOR_ARGB_TO_RGBA(
         settings->menu.entry_hover_color);
   const uint32_t title_color              = FONT_COLOR_ARGB_TO_RGBA(
         settings->menu.title_color);

   if (!menu || !menu->userdata)
      return;

   gl = (gl_t*)video_driver_get_ptr(NULL);

   if (!gl)
      return;

   glui = (glui_handle_t*)menu->userdata;

   title[0]     = '\0';
   title_buf[0] = '\0';
   title_msg[0] = '\0';
   timedate[0]  = '\0';

   video_driver_get_size(&width, &height);

   menu_display_set_viewport();

   menu_video_frame_background(menu, settings,
         gl, glui->textures.bg.id, 0.75f, 0.75f, false);

   menu_entries_get_title(title, sizeof(title));

   font_driver = driver->font_osd_driver;

   menu_display_font_bind_block(menu, font_driver, &glui->list_block);

   glui_render_menu_list(glui, menu, normal_color, hover_color);

   menu_display_font_flush_block(menu, font_driver);

   glui_render_quad(gl, 0,
         disp->header_height - menu->scroll_y + glui->line_height *
         nav->selection_ptr, width, glui->line_height, 1, 1, 1, 0.1);

   anim->is_active           = true;
   anim->label.is_updated    = false;

   glui_render_quad(gl, 0, 0, width,
         disp->header_height, 0.2, 0.2, 0.2, 1);

   ticker_limit = (width - glui->margin*2) / glui->glyph_width -
         strlen(menu_hash_to_str(MENU_VALUE_BACK)) * 2;
   menu_animation_ticker_str(title_buf, ticker_limit,
         frame_count / 100, title, true);
   glui_blit_line(width / 2, 0, title_buf,
         title_color, TEXT_ALIGN_CENTER);

   if (menu_entries_show_back())
      glui_blit_line(glui->margin, 0, menu_hash_to_str(MENU_VALUE_BACK),
            title_color, TEXT_ALIGN_LEFT);

   glui_render_quad(gl,
         0,
         height - disp->header_height,
         width,
         disp->header_height,
         0.2, 0.2, 0.2, 1);

   glui_draw_scrollbar(gl);

   if (settings->menu.core_enable)
   {
      menu_entries_get_core_title(title_msg, sizeof(title_msg));

      glui_blit_line(glui->margin,
            height - glui->line_height, title_msg,
            title_color, TEXT_ALIGN_LEFT);
   }

   if (settings->menu.timedate_enable)
   {
      menu_display_timedate(timedate, sizeof(timedate), 0);
      glui_blit_line(width - glui->margin,
            height - glui->line_height, timedate, hover_color,
            TEXT_ALIGN_RIGHT);
   }

   if (menu_input->keyboard.display)
   {
      char msg[PATH_MAX_LENGTH];
      const char           *str = *menu_input->keyboard.buffer;
      msg[0] = '\0';

      if (!str)
         str = "";
      glui_render_quad(gl, 0, 0, width, height, 0, 0, 0, 0.75);
      snprintf(msg, sizeof(msg), "%s\n%s", menu_input->keyboard.label, str);
      glui_render_messagebox(msg);
   }

   if (glui->box_message[0] != '\0')
   {
      glui_render_quad(gl, 0, 0, width, height, 0, 0, 0, 0.75);
      glui_render_messagebox(glui->box_message);
      glui->box_message[0] = '\0';
   }

   if (settings->menu.mouse.enable)
      glui_draw_cursor(gl, menu_input->mouse.x, menu_input->mouse.y);

   gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   menu_display_unset_viewport();
}

static void glui_allocate_white_texture(glui_handle_t *glui)
{
   struct texture_image ti;
   static const uint8_t white_data[] = { 0xff, 0xff, 0xff, 0xff };

   ti.width  = 1;
   ti.height = 1;
   ti.pixels = (uint32_t*)&white_data;

   glui->textures.white   = video_texture_load(&ti,
         TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_NEAREST);
}

static void glui_font(menu_handle_t *menu)
{
   settings_t *settings  = config_get_ptr();
   const char *font_path = NULL;

   font_path = settings->video.font_enable ? settings->video.font_path : NULL;

   if (!menu_display_init_main_font(menu, font_path, menu->display.font.size))
      RARCH_ERR("Failed to load font.");
}

static void glui_layout(menu_handle_t *menu, glui_handle_t *glui)
{
   menu_display_t *disp = menu_display_get_ptr();
   float scale_factor;
   unsigned width, height;
   video_driver_get_size(&width, &height);

   /* Mobiles platforms may have very small display metrics coupled to a high
      resolution, so we should be dpi aware to ensure the entries hitboxes are big
      enough. On desktops, we just care about readability, with every widget size
      proportional to the display width. */
   scale_factor = menu_display_get_dpi();

   glui->line_height            = scale_factor / 3;
   glui->margin                 = scale_factor / 6;
   menu->display.header_height  = scale_factor / 3;
   menu->display.font.size      = scale_factor / 8;
   /* we assume the average glyph aspect ratio is close to 3:4 */
   glui->glyph_width            = menu->display.font.size * 3/4;

   glui_font(menu);

   if (disp && disp->font.buf) /* calculate a more realistic ticker_limit */
   {
      driver_t *driver   = driver_get_ptr();
      int m_width = driver->font_osd_driver->get_message_width(disp->font.buf, "M", 1, 1);

      if (m_width)
         glui->glyph_width = m_width;
   }
}

static void *glui_init(void)
{
   glui_handle_t *glui                     = NULL;
   const video_driver_t *video_driver      = NULL;
   menu_handle_t                     *menu = NULL;
   gl_t *gl                                = (gl_t*)
      video_driver_get_ptr(&video_driver);

   if (video_driver != &video_gl || !gl)
   {
      RARCH_ERR("Cannot initialize GLUI menu driver: gl video driver is not active.\n");
      return NULL;
   }

   menu                 = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      goto error;

   menu->userdata       = (glui_handle_t*)calloc(1, sizeof(glui_handle_t));

   if (!menu->userdata)
      goto error;

   glui                         = (glui_handle_t*)menu->userdata;

   glui_layout(menu, glui);
   glui_allocate_white_texture(glui);

   return menu;
error:
   if (menu)
      free(menu);
   return NULL;
}

static void glui_free(void *data)
{
   gl_t *gl                                = NULL;
   const struct font_renderer *font_driver = NULL;
   menu_handle_t *menu                     = (menu_handle_t*)data;
   driver_t      *driver                   = driver_get_ptr();
   glui_handle_t *glui                     = (glui_handle_t*)menu->userdata;

   if (!glui || !menu)
      return;

   gfx_coord_array_free(&glui->list_block.carr);

   gl = (gl_t*)video_driver_get_ptr(NULL);

   font_driver = gl ? (const struct font_renderer*)driver->font_osd_driver : NULL;

   if (font_driver && font_driver->bind_block)
      font_driver->bind_block(driver->font_osd_data, NULL);

   if (menu->userdata)
      free(menu->userdata);
   menu->userdata = NULL;
}

static void glui_context_bg_destroy(glui_handle_t *glui)
{
   if (glui)
   {
      if (glui->textures.bg.id)
         glDeleteTextures(1, &glui->textures.bg.id);
      if (glui->textures.white)
         glDeleteTextures(1, &glui->textures.white);

      glui->textures.bg.id = 0;
      glui->textures.white = 0;
   }
}

static void glui_context_destroy(void)
{
   gl_t          *gl     = (gl_t*)video_driver_get_ptr(NULL);
   glui_handle_t *glui   = NULL;
   menu_handle_t *menu   = menu_driver_get_ptr();
   driver_t      *driver = driver_get_ptr();

   if (!menu || !menu->userdata || !gl || !driver)
      return;

   glui = (glui_handle_t*)menu->userdata;

   menu_display_free_main_font(menu);

   glui_context_bg_destroy(glui);
}

static bool glui_load_image(void *data, menu_image_type_t type)
{
   glui_handle_t *glui = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu || !menu->userdata)
      return false;

   glui = (glui_handle_t*)menu->userdata;

   switch (type)
   {
      case MENU_IMAGE_NONE:
         break;
      case MENU_IMAGE_WALLPAPER:
         glui_context_bg_destroy(glui);

         glui->textures.bg.id   = video_texture_load(data,
               TEXTURE_BACKEND_OPENGL, TEXTURE_FILTER_MIPMAP_LINEAR);
         glui_allocate_white_texture(glui);
         break;
      case MENU_IMAGE_BOXART:
         break;
   }

   return true;
}

static float glui_get_scroll(void)
{
   int half = 0;
   unsigned width, height;
   glui_handle_t *glui    = NULL;
   menu_handle_t *menu    = menu_driver_get_ptr();
   menu_navigation_t *nav = menu_navigation_get_ptr();

   if (!menu || !menu->userdata)
      return 0;

   video_driver_get_size(&width, &height);

   glui = (glui_handle_t*)menu->userdata;
   if (glui->line_height)
      half = (height / glui->line_height) / 2;

   if (nav->selection_ptr < (unsigned)half)
      return 0;
   return ((nav->selection_ptr + 2 - half) * glui->line_height);
}

static void glui_navigation_set(bool scroll)
{
   menu_display_t *disp = menu_display_get_ptr();
   menu_handle_t *menu  = menu_driver_get_ptr();
   float     scroll_pos = 0;

   if (!menu || !disp || !scroll)
      return;

   scroll_pos = glui_get_scroll();

   menu_animation_push(disp->animation, 10, scroll_pos,
         &menu->scroll_y, EASING_IN_OUT_QUAD, -1, NULL);
}

static void glui_navigation_clear(bool pending_push)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return;

   menu_entries_set_start(0);
   menu->scroll_y      = 0;
}

static void glui_navigation_set_last(void)
{
   glui_navigation_set(true);
}

static void glui_navigation_alphabet(size_t *unused)
{
   glui_navigation_set(true);
}

static void glui_populate_entries(const char *path,
      const char *label, unsigned i)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return;

   menu->scroll_y      = glui_get_scroll();
}

static void glui_context_reset(void)
{
   glui_handle_t *glui   = NULL;
   menu_handle_t *menu   = menu_driver_get_ptr();
   settings_t *settings  = config_get_ptr();

   if (!menu || !menu->userdata || !settings)
      return;

   glui      = (glui_handle_t*)menu->userdata;

   glui_layout(menu, glui);
   glui_context_bg_destroy(glui);
   glui_allocate_white_texture(glui);

   rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE,
         settings->menu.wallpaper, "cb_menu_wallpaper", 0, 1, true);
}

static int glui_environ(menu_environ_cb_t type, void *data)
{
   switch (type)
   {
      case 0:
      default:
         break;
   }

   return -1;
}

menu_ctx_driver_t menu_ctx_glui = {
   NULL,
   glui_get_message,
   glui_render,
   glui_frame,
   glui_init,
   glui_free,
   glui_context_reset,
   glui_context_destroy,
   glui_populate_entries,
   NULL,
   glui_navigation_clear,
   NULL,
   NULL,
   glui_navigation_set,
   glui_navigation_set_last,
   glui_navigation_alphabet,
   glui_navigation_alphabet,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   glui_load_image,
   "glui",
   glui_environ,
   NULL,
};
