/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include <string.h>
#include <time.h>
#include <locale.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <formats/image.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <encodings/utf.h>

#ifdef WIIU
#include <wiiu/os/energy.h>
#endif

#ifdef HAVE_DISCORD
#include "discord/discord.h"
#endif

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_THREADS
#include "../gfx/video_thread_wrapper.h"
#endif

#include "../gfx/video_driver.h"

#include "menu_animation.h"
#include "menu_driver.h"
#include "menu_cbs.h"
#include "menu_input.h"
#include "menu_entries.h"
#include "widgets/menu_dialog.h"
#include "menu_shader.h"

#include "../config.def.h"
#include "../content.h"
#include "../core.h"
#include "../configuration.h"
#include "../dynamic.h"
#include "../driver.h"
#include "../retroarch.h"
#include "../defaults.h"
#include "../frontend/frontend.h"
#include "../list_special.h"
#include "../tasks/tasks_internal.h"
#include "../ui/ui_companion_driver.h"
#include "../verbosity.h"

#define SCROLL_INDEX_SIZE          (2 * (26 + 2) + 1)

#define PARTICLES_COUNT            100

typedef struct menu_ctx_load_image
{
   void *data;
   enum menu_image_type type;
} menu_ctx_load_image_t;

/* Menu drivers */
static const menu_ctx_driver_t *menu_ctx_drivers[] = {
#if defined(HAVE_XUI)
   &menu_ctx_xui,
#endif
#if defined(HAVE_MATERIALUI)
   &menu_ctx_mui,
#endif
#if defined(HAVE_NUKLEAR)
   &menu_ctx_nuklear,
#endif
#if defined(HAVE_XMB)
   &menu_ctx_xmb,
#endif
#if defined(HAVE_STRIPES)
   &menu_ctx_stripes,
#endif
#if defined(HAVE_RGUI)
   &menu_ctx_rgui,
#endif
#if defined(HAVE_ZARCH)
   &menu_ctx_zarch,
#endif
   &menu_ctx_null,
   NULL
};

/* Menu display drivers */
static menu_display_ctx_driver_t *menu_display_ctx_drivers[] = {
#ifdef HAVE_D3D8
   &menu_display_ctx_d3d8,
#endif
#ifdef HAVE_D3D9
   &menu_display_ctx_d3d9,
#endif
#ifdef HAVE_D3D10
   &menu_display_ctx_d3d10,
#endif
#ifdef HAVE_D3D11
   &menu_display_ctx_d3d11,
#endif
#ifdef HAVE_D3D12
   &menu_display_ctx_d3d12,
#endif
#ifdef HAVE_OPENGL
   &menu_display_ctx_gl,
#endif
#ifdef HAVE_VULKAN
   &menu_display_ctx_vulkan,
#endif
#ifdef HAVE_METAL
   &menu_display_ctx_metal,
#endif
#ifdef HAVE_VITA2D
   &menu_display_ctx_vita2d,
#endif
#ifdef _3DS
   &menu_display_ctx_ctr,
#endif
#ifdef WIIU
   &menu_display_ctx_wiiu,
#endif
#if defined(_WIN32) && !defined(_XBOX)
   &menu_display_ctx_gdi,
#endif
#ifdef DJGPP
   &menu_display_ctx_vga,
#endif
#ifdef HAVE_SIXEL
   &menu_display_ctx_sixel,
#endif
#ifdef HAVE_CACA
   &menu_display_ctx_caca,
#endif
   &menu_display_ctx_null,
   NULL,
};

uintptr_t menu_display_white_texture;

static video_coord_array_t menu_disp_ca;

static enum
menu_toggle_reason menu_display_toggle_reason    = MENU_TOGGLE_REASON_NONE;

/* Width, height and pitch of the menu framebuffer */
static unsigned menu_display_framebuf_width      = 0;
static unsigned menu_display_framebuf_height     = 0;
static size_t menu_display_framebuf_pitch        = 0;

/* Height of the menu display header */
static unsigned menu_display_header_height       = 0;

static bool menu_display_has_windowed            = false;
static bool menu_display_msg_force               = false;
static bool menu_display_font_alloc_framebuf     = false;
static bool menu_display_framebuf_dirty          = false;
static const uint8_t *menu_display_font_framebuf = NULL;
static menu_display_ctx_driver_t *menu_disp      = NULL;

/* when enabled, on next iteration the 'Quick Menu' list will
 * be pushed onto the stack */
static bool menu_driver_pending_quick_menu      = false;

static bool menu_driver_prevent_populate        = false;

/* Is the menu driver still running? */
static bool menu_driver_alive                   = false;

/* A menu toggle has been requested; if the menu was running,
 * it will be closed; if the menu was not running, it will be opened */
static bool menu_driver_toggled                 = false;

/* The menu driver owns the userdata */
static bool menu_driver_data_own                = false;

/* The user has requested that the menu be shut down. This will
 * be enacted upon the next menu iteration */
static bool menu_driver_pending_quit            = false;

/* The user has requested that RetroArch be shut down. This will
 * be enacted upon the next menu iteration */
static bool menu_driver_pending_shutdown        = false;

/* Are we binding a button inside the menu? */
static bool menu_driver_is_binding              = false;

static menu_handle_t *menu_driver_data          = NULL;
static const menu_ctx_driver_t *menu_driver_ctx = NULL;
static void *menu_userdata                      = NULL;

/* Quick jumping indices with L/R.
 * Rebuilt when parsing directory. */
static size_t   scroll_index_list[SCROLL_INDEX_SIZE];
static unsigned scroll_index_size               = 0;
static unsigned scroll_acceleration             = 0;
static size_t menu_driver_selection_ptr         = 0;

/* Returns the OSK key at a given position */
int menu_display_osk_ptr_at_pos(void *data, int x, int y,
      unsigned width, unsigned height)
{
   unsigned i;
   int ptr_width  = width / 11;
   int ptr_height = height / 10;

   if (ptr_width >= ptr_height)
      ptr_width = ptr_height;

   for (i = 0; i < 44; i++)
   {
      int line_y    = (i / 11)*height/10.0;
      int ptr_x     = width/2.0 - (11*ptr_width)/2.0 + (i % 11) * ptr_width;
      int ptr_y     = height/2.0 + ptr_height*1.5 + line_y - ptr_height;

      if (x > ptr_x && x < ptr_x + ptr_width
       && y > ptr_y && y < ptr_y + ptr_height)
         return i;
   }

   return -1;
}

enum menu_toggle_reason menu_display_toggle_get_reason(void)
{
  return menu_display_toggle_reason;
}

void menu_display_toggle_set_reason(enum menu_toggle_reason reason)
{
  menu_display_toggle_reason = reason;
}

/* Check if the current menu driver is compatible
 * with your video driver. */
static bool menu_display_check_compatibility(
      enum menu_display_driver_type type,
      bool video_is_threaded)
{
   const char *video_driver =
#ifdef HAVE_THREADS
      (video_is_threaded) ?
      video_thread_get_ident() :
#endif
      video_driver_get_ident();

   switch (type)
   {
      case MENU_VIDEO_DRIVER_GENERIC:
         return true;
      case MENU_VIDEO_DRIVER_OPENGL:
         if (string_is_equal(video_driver, "gl"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_VULKAN:
         if (string_is_equal(video_driver, "vulkan"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_METAL:
         if (string_is_equal(video_driver, "metal"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_DIRECT3D8:
         if (string_is_equal(video_driver, "d3d8"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_DIRECT3D9:
         if (string_is_equal(video_driver, "d3d9"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_DIRECT3D10:
         if (string_is_equal(video_driver, "d3d10"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_DIRECT3D11:
         if (string_is_equal(video_driver, "d3d11"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_DIRECT3D12:
         if (string_is_equal(video_driver, "d3d12"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_VITA2D:
         if (string_is_equal(video_driver, "vita2d"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_CTR:
         if (string_is_equal(video_driver, "ctr"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_WIIU:
         if (string_is_equal(video_driver, "gx2"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_SIXEL:
         if (string_is_equal(video_driver, "sixel"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_CACA:
         if (string_is_equal(video_driver, "caca"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_GDI:
         if (string_is_equal(video_driver, "gdi"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_VGA:
         if (string_is_equal(video_driver, "vga"))
            return true;
         break;
      case MENU_VIDEO_DRIVER_SWITCH:
         if (string_is_equal(video_driver, "switch"))
            return true;
         break;
   }

   return false;
}

/* Display the date and time - time_mode will influence how
 * the time representation will look like.
 * */
void menu_display_timedate(menu_display_ctx_datetime_t *datetime)
{
   time_t time_;

   if (!datetime)
      return;

   time(&time_);

   setlocale(LC_TIME, "");

   switch (datetime->time_mode)
   {
      case 0: /* Date and time */
         strftime(datetime->s, datetime->len,
               "%Y-%m-%d %H:%M:%S", localtime(&time_));
         break;
      case 1: /* YY-MM-DD HH:MM */
         strftime(datetime->s, datetime->len,
               "%Y-%m-%d %H:%M", localtime(&time_));
         break;
      case 2: /* MM-DD-YYYY HH:MM  */
         strftime(datetime->s, datetime->len,
               "%m-%d-%Y %H:%M", localtime(&time_));
         break;
      case 3: /* Time */
         strftime(datetime->s, datetime->len,
               "%H:%M:%S", localtime(&time_));
         break;
      case 4: /* Time (hours-minutes) */
         strftime(datetime->s, datetime->len,
               "%H:%M", localtime(&time_));
         break;
      case 5: /* Date and time, without year and seconds */
         strftime(datetime->s, datetime->len,
               "%d/%m %H:%M", localtime(&time_));
         break;
      case 6:
         strftime(datetime->s, datetime->len,
               "%m/%d %H:%M", localtime(&time_));
         break;
      case 7: /* Time (hours-minutes), in 12 hour AM-PM designation */
#if defined(__linux__) && !defined(ANDROID)
         strftime(datetime->s, datetime->len,
            "%r", localtime(&time_));
#else
         {
            char *local;

            strftime(datetime->s, datetime->len,

               "%I:%M:%S %p", localtime(&time_));
            local = local_to_utf8_string_alloc(datetime->s);

            if (local)
            {
               strlcpy(datetime->s, local, datetime->len);
               free(local);
            }
         }
#endif
   }
}

/* Begin blending operation */
void menu_display_blend_begin(video_frame_info_t *video_info)
{
   if (menu_disp && menu_disp->blend_begin)
      menu_disp->blend_begin(video_info);
}

/* End blending operation */
void menu_display_blend_end(video_frame_info_t *video_info)
{
   if (menu_disp && menu_disp->blend_end)
      menu_disp->blend_end(video_info);
}

/* Begin scissoring operation */
void menu_display_scissor_begin(video_frame_info_t *video_info, int x, int y, unsigned width, unsigned height)
{
   if (menu_disp && menu_disp->scissor_begin)
      menu_disp->scissor_begin(video_info, x, y, width, height);
}

/* End scissoring operation */
void menu_display_scissor_end(video_frame_info_t *video_info)
{
   if (menu_disp && menu_disp->scissor_end)
      menu_disp->scissor_end(video_info);
}

/* Teardown; deinitializes and frees all
 * fonts associated to the menu driver */
void menu_display_font_free(font_data_t *font)
{
   font_driver_free(font);
}

/* Setup: Initializes the font associated
 * to the menu driver */
font_data_t *menu_display_font(
      enum application_special_type type,
      float font_size,
      bool is_threaded)
{
   char fontpath[PATH_MAX_LENGTH];
   font_data_t *font_data = NULL;

   if (!menu_disp)
      return NULL;

   fontpath[0] = '\0';

   fill_pathname_application_special(
         fontpath, sizeof(fontpath), type);

   if (!menu_disp->font_init_first((void**)&font_data,
            video_driver_get_ptr(false),
            fontpath, font_size, is_threaded))
      return NULL;

   return font_data;
}

/* Reset the menu's coordinate array vertices.
 * NOTE: Not every menu driver uses this. */
void menu_display_coords_array_reset(void)
{
   menu_disp_ca.coords.vertices = 0;
}

video_coord_array_t *menu_display_get_coords_array(void)
{
   return &menu_disp_ca;
}

const uint8_t *menu_display_get_font_framebuffer(void)
{
   return menu_display_font_framebuf;
}

void menu_display_set_font_framebuffer(const uint8_t *buffer)
{
   menu_display_font_framebuf = buffer;
}

bool menu_display_libretro_running(
      bool rarch_is_inited,
      bool rarch_is_dummy_core)
{
   settings_t *settings = config_get_ptr();
   if (!settings->bools.menu_pause_libretro)
   {
      if (rarch_is_inited && !rarch_is_dummy_core)
         return true;
   }
   return false;
}

/* Display the libretro core's framebuffer onscreen. */
bool menu_display_libretro(bool is_idle,
      bool rarch_is_inited, bool rarch_is_dummy_core)
{
   video_driver_set_texture_enable(true, false);

   if (menu_display_libretro_running(
            rarch_is_inited, rarch_is_dummy_core))
   {
      if (!input_driver_is_libretro_input_blocked())
         input_driver_set_libretro_input_blocked();

      core_run();
      input_driver_unset_libretro_input_blocked();

      return true;
   }

   if (is_idle)
   {
#ifdef HAVE_DISCORD
      discord_userdata_t userdata;
      userdata.status = DISCORD_PRESENCE_GAME_PAUSED;

      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
#endif
      return true; /* Maybe return false here
                      for indication of idleness? */
   }
   return video_driver_cached_frame();
}

/* Get the menu framebuffer's size dimensions. */
void menu_display_get_fb_size(unsigned *fb_width,
      unsigned *fb_height, size_t *fb_pitch)
{
   *fb_width  = menu_display_framebuf_width;
   *fb_height = menu_display_framebuf_height;
   *fb_pitch  = menu_display_framebuf_pitch;
}

/* Set the menu framebuffer's width. */
void menu_display_set_width(unsigned width)
{
   menu_display_framebuf_width = width;
}

/* Set the menu framebuffer's height. */
void menu_display_set_height(unsigned height)
{
   menu_display_framebuf_height = height;
}

void menu_display_set_header_height(unsigned height)
{
   menu_display_header_height = height;
}

unsigned menu_display_get_header_height(void)
{
   return menu_display_header_height;
}

size_t menu_display_get_framebuffer_pitch(void)
{
   return menu_display_framebuf_pitch;
}

void menu_display_set_framebuffer_pitch(size_t pitch)
{
   menu_display_framebuf_pitch = pitch;
}

bool menu_display_get_msg_force(void)
{
   return menu_display_msg_force;
}

void menu_display_set_msg_force(bool state)
{
   menu_display_msg_force = state;
}

bool menu_display_get_font_data_init(void)
{
   return menu_display_font_alloc_framebuf;
}

void menu_display_set_font_data_init(bool state)
{
   menu_display_font_alloc_framebuf = state;
}

/* Returns true if an animation is still active or
 * when the menu framebuffer still is dirty and
 * therefore it still needs to be rendered onscreen.
 *
 * This function can be used for optimization purposes
 * so that we don't have to render the menu graphics per-frame
 * unless a change has happened.
 * */
bool menu_display_get_update_pending(void)
{
   if (menu_animation_is_active() || menu_display_framebuf_dirty)
      return true;
   return false;
}

void menu_display_set_viewport(unsigned width, unsigned height)
{
   video_driver_set_viewport(width, height, true, false);
}

void menu_display_unset_viewport(unsigned width, unsigned height)
{
   video_driver_set_viewport(width, height, false, true);
}

/* Checks if the menu framebuffer has its 'dirty flag' set. This
 * means that the current contents of the framebuffer has changed
 * and that it has to be rendered to the screen. */
bool menu_display_get_framebuffer_dirty_flag(void)
{
   return menu_display_framebuf_dirty;
}

/* Set the menu framebuffer's 'dirty flag'. */
void menu_display_set_framebuffer_dirty_flag(void)
{
   menu_display_framebuf_dirty = true;
}

/* Unset the menu framebufer's 'dirty flag'. */
void menu_display_unset_framebuffer_dirty_flag(void)
{
   menu_display_framebuf_dirty = false;
}

/* Get the preferred DPI at which to render the menu.
 * NOTE: Only MaterialUI menu driver so far uses this, neither
 * RGUI or XMB use this. */
float menu_display_get_dpi(void)
{
   unsigned width, height;
   settings_t *settings = config_get_ptr();
   float            dpi   = 0.0f;
   float diagonal         = 6.5f;

   video_driver_get_size(&width, &height);

   if (!settings)
      return true;

#ifdef RARCH_MOBILE
   diagonal                = 5.0f;
#endif

   /* Generic dpi calculation formula,
    * the divider is the screen diagonal in inches */
   dpi = sqrt((width * width) + (height * height)) / diagonal;

   if (settings->bools.menu_dpi_override_enable)
      return settings->uints.menu_dpi_override_value;

   return dpi;
}

bool menu_display_driver_exists(const char *s)
{
   unsigned i;
   for (i = 0; i < ARRAY_SIZE(menu_display_ctx_drivers); i++)
   {
      if (string_is_equal(s, menu_display_ctx_drivers[i]->ident))
         return true;
   }

   return false;
}

bool menu_display_init_first_driver(bool video_is_threaded)
{
   unsigned i;

   for (i = 0; menu_display_ctx_drivers[i]; i++)
   {
      if (!menu_display_check_compatibility(
               menu_display_ctx_drivers[i]->type,
               video_is_threaded))
         continue;

      RARCH_LOG("[Menu]: Found menu display driver: \"%s\".\n",
            menu_display_ctx_drivers[i]->ident);
      menu_disp = menu_display_ctx_drivers[i];
      return true;
   }
   return false;
}

bool menu_display_restore_clear_color(void)
{
   if (!menu_disp || !menu_disp->restore_clear_color)
      return false;
   menu_disp->restore_clear_color();
   return true;
}

void menu_display_clear_color(menu_display_ctx_clearcolor_t *color,
      video_frame_info_t *video_info)
{
   if (menu_disp && menu_disp->clear_color)
      menu_disp->clear_color(color, video_info);
}

void menu_display_draw(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   if (!menu_disp || !draw || !menu_disp->draw)
      return;

   /* TODO - edge case */
   if (draw->height <= 0)
      draw->height = 1;

   menu_disp->draw(draw, video_info);
}

void menu_display_draw_pipeline(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   if (menu_disp && draw && menu_disp->draw_pipeline)
      menu_disp->draw_pipeline(draw, video_info);
}

void menu_display_draw_bg(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info, bool add_opacity_to_wallpaper,
      float override_opacity)
{
   static struct video_coords coords;
   const float *new_vertex       = NULL;
   const float *new_tex_coord    = NULL;
   if (!menu_disp || !draw)
      return;

   new_vertex           = draw->vertex;
   new_tex_coord        = draw->tex_coord;

   if (!new_vertex)
      new_vertex        = menu_disp->get_default_vertices();
   if (!new_tex_coord)
      new_tex_coord     = menu_disp->get_default_tex_coords();

   coords.vertices      = (unsigned)draw->vertex_count;
   coords.vertex        = new_vertex;
   coords.tex_coord     = new_tex_coord;
   coords.lut_tex_coord = new_tex_coord;
   coords.color         = (const float*)draw->color;

   draw->coords         = &coords;
   draw->scale_factor   = 1.0f;
   draw->rotation       = 0.0f;

   if (draw->texture)
      add_opacity_to_wallpaper = true;

   if (add_opacity_to_wallpaper)
      menu_display_set_alpha(draw->color, override_opacity);

   if (!draw->texture)
      draw->texture     = menu_display_white_texture;

   draw->matrix_data = (math_matrix_4x4*)menu_disp->get_default_mvp(video_info);
}

void menu_display_draw_gradient(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   draw->texture       = 0;
   draw->x             = 0;
   draw->y             = 0;

   menu_display_draw_bg(draw, video_info, false,
         video_info->menu_wallpaper_opacity);
   menu_display_draw(draw, video_info);
}

void menu_display_draw_quad(
      video_frame_info_t *video_info,
      int x, int y, unsigned w, unsigned h,
      unsigned width, unsigned height,
      float *color)
{
   menu_display_ctx_draw_t draw;
   struct video_coords coords;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = color;

   if (menu_disp && menu_disp->blend_begin)
      menu_disp->blend_begin(video_info);

   draw.x            = x;
   draw.y            = (int)height - y - (int)h;
   draw.width        = w;
   draw.height       = h;
   draw.coords       = &coords;
   draw.matrix_data  = NULL;
   draw.texture      = menu_display_white_texture;
   draw.prim_type    = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id  = 0;
   draw.scale_factor = 1.0f;
   draw.rotation     = 0.0f;

   menu_display_draw(&draw, video_info);

   if (menu_disp && menu_disp->blend_end)
      menu_disp->blend_end(video_info);
}

void menu_display_draw_polygon(
      video_frame_info_t *video_info,
      int x1, int y1,
      int x2, int y2,
      int x3, int y3,
      int x4, int y4,
      unsigned width, unsigned height,
      float *color)
{
   menu_display_ctx_draw_t draw;
   struct video_coords coords;

   float vertex[8];

   vertex[0]             = x1 / (float)width;
   vertex[1]             = y1 / (float)height;
   vertex[2]             = x2 / (float)width;
   vertex[3]             = y2 / (float)height;
   vertex[4]             = x3 / (float)width;
   vertex[5]             = y3 / (float)height;
   vertex[6]             = x4 / (float)width;
   vertex[7]             = y4 / (float)height;

   coords.vertices      = 4;
   coords.vertex        = &vertex[0];
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = color;

   if (menu_disp && menu_disp->blend_begin)
      menu_disp->blend_begin(video_info);

   draw.x            = 0;
   draw.y            = 0;
   draw.width        = width;
   draw.height       = height;
   draw.coords       = &coords;
   draw.matrix_data  = NULL;
   draw.texture      = menu_display_white_texture;
   draw.prim_type    = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id  = 0;
   draw.scale_factor = 1.0f;
   draw.rotation     = 0.0f;

   menu_display_draw(&draw, video_info);

   if (menu_disp && menu_disp->blend_end)
      menu_disp->blend_end(video_info);
}

void menu_display_draw_texture(
      video_frame_info_t *video_info,
      int x, int y, unsigned w, unsigned h,
      unsigned width, unsigned height,
      float *color, uintptr_t texture)
{
   menu_display_ctx_draw_t draw;
   menu_display_ctx_rotate_draw_t rotate_draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;
   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = 0.0;
   rotate_draw.scale_x      = 1.0;
   rotate_draw.scale_y      = 1.0;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;
   coords.vertices          = 4;
   coords.vertex            = NULL;
   coords.tex_coord         = NULL;
   coords.lut_tex_coord     = NULL;
   draw.width               = w;
   draw.height              = h;
   draw.coords              = &coords;
   draw.matrix_data         = &mymat;
   draw.prim_type           = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id         = 0;
   coords.color             = (const float*)color;

   menu_display_rotate_z(&rotate_draw, video_info);

   draw.texture             = texture;
   draw.x                   = x;
   draw.y                   = height - y;
   menu_display_draw(&draw, video_info);
}

/* Draw the texture split into 9 sections, without scaling the corners.
 * The middle sections will only scale in the X axis, and the side
 * sections will only scale in the Y axis. */
void menu_display_draw_texture_slice(
      video_frame_info_t *video_info,
      int x, int y, unsigned w, unsigned h,
      unsigned new_w, unsigned new_h,
      unsigned width, unsigned height,
      float *color, unsigned offset, float scale_factor, uintptr_t texture)
{
   menu_display_ctx_draw_t draw;
   menu_display_ctx_rotate_draw_t rotate_draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;
   unsigned i;
   float V_BL[2], V_BR[2], V_TL[2], V_TR[2], T_BL[2], T_BR[2], T_TL[2], T_TR[2];

   /* need space for the coordinates of two triangles in a strip, so 8 vertices */
   float *tex_coord  = (float*)malloc(8 * sizeof(float));
   float *vert_coord = (float*)malloc(8 * sizeof(float));
   float *colors     = (float*)malloc(16 * sizeof(float));

   /* normalized width/height of the amount to offset from the corners,
    * for both the vertex and texture coordinates */
   float vert_woff   = (offset * scale_factor) / (float)width;
   float vert_hoff   = (offset * scale_factor) / (float)height;
   float tex_woff    = offset / (float)w;
   float tex_hoff    = offset / (float)h;

   /* the width/height of the middle sections of both the scaled and original image */
   float vert_scaled_mid_width  = (new_w - (offset * scale_factor * 2)) / (float)width;
   float vert_scaled_mid_height = (new_h - (offset * scale_factor * 2)) / (float)height;
   float tex_mid_width          = (w - (offset * 2)) / (float)w;
   float tex_mid_height         = (h - (offset * 2)) / (float)h;

   /* normalized coordinates for the start position of the image */
   float norm_x                 = x / (float)width;
   float norm_y                 = (height - y) / (float)height;

   /* the four vertices of the top-left corner of the image,
    * used as a starting point for all the other sections */
   V_BL[0] = norm_x;
   V_BL[1] = norm_y;
   V_BR[0] = norm_x + vert_woff;
   V_BR[1] = norm_y;
   V_TL[0] = norm_x;
   V_TL[1] = norm_y + vert_hoff;
   V_TR[0] = norm_x + vert_woff;
   V_TR[1] = norm_y + vert_hoff;
   T_BL[0] = 0.0f;
   T_BL[1] = tex_hoff;
   T_BR[0] = tex_woff;
   T_BR[1] = tex_hoff;
   T_TL[0] = 0.0f;
   T_TL[1] = 0.0f;
   T_TR[0] = tex_woff;
   T_TR[1] = 0.0f;

   for (i = 0; i < (16 * sizeof(float)) / sizeof(colors[0]); i++)
      colors[i] = 1.0f;

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = 0.0;
   rotate_draw.scale_x      = 1.0;
   rotate_draw.scale_y      = 1.0;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;
   coords.vertices          = 4;
   coords.vertex            = vert_coord;
   coords.tex_coord         = tex_coord;
   coords.lut_tex_coord     = NULL;
   draw.width               = width;
   draw.height              = height;
   draw.coords              = &coords;
   draw.matrix_data         = &mymat;
   draw.prim_type           = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id         = 0;
   coords.color             = (const float*)colors;

   menu_display_rotate_z(&rotate_draw, video_info);

   draw.texture             = texture;
   draw.x                   = 0;
   draw.y                   = 0;

   /* vertex coords are specfied bottom-up in this order: BL BR TL TR */
   /* texture coords are specfied top-down in this order: BL BR TL TR */

   /* If someone wants to change this to not draw several times, the
    * coordinates will need to be modified because of the triangle strip usage. */

   /* top-left corner */
   vert_coord[0] = V_BL[0];
   vert_coord[1] = V_BL[1];
   vert_coord[2] = V_BR[0];
   vert_coord[3] = V_BR[1];
   vert_coord[4] = V_TL[0];
   vert_coord[5] = V_TL[1];
   vert_coord[6] = V_TR[0];
   vert_coord[7] = V_TR[1];

   tex_coord[0] = T_BL[0];
   tex_coord[1] = T_BL[1];
   tex_coord[2] = T_BR[0];
   tex_coord[3] = T_BR[1];
   tex_coord[4] = T_TL[0];
   tex_coord[5] = T_TL[1];
   tex_coord[6] = T_TR[0];
   tex_coord[7] = T_TR[1];

   menu_display_draw(&draw, video_info);

   /* top-middle section */
   vert_coord[0] = V_BL[0] + vert_woff;
   vert_coord[1] = V_BL[1];
   vert_coord[2] = V_BR[0] + vert_scaled_mid_width;
   vert_coord[3] = V_BR[1];
   vert_coord[4] = V_TL[0] + vert_woff;
   vert_coord[5] = V_TL[1];
   vert_coord[6] = V_TR[0] + vert_scaled_mid_width;
   vert_coord[7] = V_TR[1];

   tex_coord[0] = T_BL[0] + tex_woff;
   tex_coord[1] = T_BL[1];
   tex_coord[2] = T_BR[0] + tex_mid_width;
   tex_coord[3] = T_BR[1];
   tex_coord[4] = T_TL[0] + tex_woff;
   tex_coord[5] = T_TL[1];
   tex_coord[6] = T_TR[0] + tex_mid_width;
   tex_coord[7] = T_TR[1];

   menu_display_draw(&draw, video_info);

   /* top-right corner */
   vert_coord[0] = V_BL[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[1] = V_BL[1];
   vert_coord[2] = V_BR[0] + vert_scaled_mid_width + vert_woff;
   vert_coord[3] = V_BR[1];
   vert_coord[4] = V_TL[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[5] = V_TL[1];
   vert_coord[6] = V_TR[0] + vert_scaled_mid_width + vert_woff;
   vert_coord[7] = V_TR[1];

   tex_coord[0] = T_BL[0] + tex_woff + tex_mid_width;
   tex_coord[1] = T_BL[1];
   tex_coord[2] = T_BR[0] + tex_mid_width + tex_woff;
   tex_coord[3] = T_BR[1];
   tex_coord[4] = T_TL[0] + tex_woff + tex_mid_width;
   tex_coord[5] = T_TL[1];
   tex_coord[6] = T_TR[0] + tex_mid_width + tex_woff;
   tex_coord[7] = T_TR[1];

   menu_display_draw(&draw, video_info);

   /* middle-left section */
   vert_coord[0] = V_BL[0];
   vert_coord[1] = V_BL[1] - vert_scaled_mid_height;
   vert_coord[2] = V_BR[0];
   vert_coord[3] = V_BR[1] - vert_scaled_mid_height;
   vert_coord[4] = V_TL[0];
   vert_coord[5] = V_TL[1] - vert_hoff;
   vert_coord[6] = V_TR[0];
   vert_coord[7] = V_TR[1] - vert_hoff;

   tex_coord[0] = T_BL[0];
   tex_coord[1] = T_BL[1] + tex_mid_height;
   tex_coord[2] = T_BR[0];
   tex_coord[3] = T_BR[1] + tex_mid_height;
   tex_coord[4] = T_TL[0];
   tex_coord[5] = T_TL[1] + tex_hoff;
   tex_coord[6] = T_TR[0];
   tex_coord[7] = T_TR[1] + tex_hoff;

   menu_display_draw(&draw, video_info);

   /* center section */
   vert_coord[0] = V_BL[0] + vert_woff;
   vert_coord[1] = V_BL[1] - vert_scaled_mid_height;
   vert_coord[2] = V_BR[0] + vert_scaled_mid_width;
   vert_coord[3] = V_BR[1] - vert_scaled_mid_height;
   vert_coord[4] = V_TL[0] + vert_woff;
   vert_coord[5] = V_TL[1] - vert_hoff;
   vert_coord[6] = V_TR[0] + vert_scaled_mid_width;
   vert_coord[7] = V_TR[1] - vert_hoff;

   tex_coord[0] = T_BL[0] + tex_woff;
   tex_coord[1] = T_BL[1] + tex_mid_height;
   tex_coord[2] = T_BR[0] + tex_mid_width;
   tex_coord[3] = T_BR[1] + tex_mid_height;
   tex_coord[4] = T_TL[0] + tex_woff;
   tex_coord[5] = T_TL[1] + tex_hoff;
   tex_coord[6] = T_TR[0] + tex_mid_width;
   tex_coord[7] = T_TR[1] + tex_hoff;

   menu_display_draw(&draw, video_info);

   /* middle-right section */
   vert_coord[0] = V_BL[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[1] = V_BL[1] - vert_scaled_mid_height;
   vert_coord[2] = V_BR[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[3] = V_BR[1] - vert_scaled_mid_height;
   vert_coord[4] = V_TL[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[5] = V_TL[1] - vert_hoff;
   vert_coord[6] = V_TR[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[7] = V_TR[1] - vert_hoff;

   tex_coord[0] = T_BL[0] + tex_woff + tex_mid_width;
   tex_coord[1] = T_BL[1] + tex_mid_height;
   tex_coord[2] = T_BR[0] + tex_woff + tex_mid_width;
   tex_coord[3] = T_BR[1] + tex_mid_height;
   tex_coord[4] = T_TL[0] + tex_woff + tex_mid_width;
   tex_coord[5] = T_TL[1] + tex_hoff;
   tex_coord[6] = T_TR[0] + tex_woff + tex_mid_width;
   tex_coord[7] = T_TR[1] + tex_hoff;

   menu_display_draw(&draw, video_info);

   /* bottom-left corner */
   vert_coord[0] = V_BL[0];
   vert_coord[1] = V_BL[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[2] = V_BR[0];
   vert_coord[3] = V_BR[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[4] = V_TL[0];
   vert_coord[5] = V_TL[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[6] = V_TR[0];
   vert_coord[7] = V_TR[1] - vert_hoff - vert_scaled_mid_height;

   tex_coord[0] = T_BL[0];
   tex_coord[1] = T_BL[1] + tex_hoff + tex_mid_height;
   tex_coord[2] = T_BR[0];
   tex_coord[3] = T_BR[1] + tex_hoff + tex_mid_height;
   tex_coord[4] = T_TL[0];
   tex_coord[5] = T_TL[1] + tex_hoff + tex_mid_height;
   tex_coord[6] = T_TR[0];
   tex_coord[7] = T_TR[1] + tex_hoff + tex_mid_height;

   menu_display_draw(&draw, video_info);

   /* bottom-middle section */
   vert_coord[0] = V_BL[0] + vert_woff;
   vert_coord[1] = V_BL[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[2] = V_BR[0] + vert_scaled_mid_width;
   vert_coord[3] = V_BR[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[4] = V_TL[0] + vert_woff;
   vert_coord[5] = V_TL[1] - vert_scaled_mid_height;
   vert_coord[6] = V_TR[0] + vert_scaled_mid_width;
   vert_coord[7] = V_TR[1] - vert_scaled_mid_height;

   tex_coord[0] = T_BL[0] + tex_woff;
   tex_coord[1] = T_BL[1] + tex_hoff + tex_mid_height;
   tex_coord[2] = T_BR[0] + tex_mid_width;
   tex_coord[3] = T_BR[1] + tex_hoff + tex_mid_height;
   tex_coord[4] = T_TL[0] + tex_woff;
   tex_coord[5] = T_TL[1] + tex_mid_height;
   tex_coord[6] = T_TR[0] + tex_mid_width;
   tex_coord[7] = T_TR[1] + tex_mid_height;

   menu_display_draw(&draw, video_info);

   /* bottom-right corner */
   vert_coord[0] = V_BL[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[1] = V_BL[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[2] = V_BR[0] + vert_scaled_mid_width + vert_woff;
   vert_coord[3] = V_BR[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[4] = V_TL[0] + vert_woff + vert_scaled_mid_width;
   vert_coord[5] = V_TL[1] - vert_hoff - vert_scaled_mid_height;
   vert_coord[6] = V_TR[0] + vert_scaled_mid_width + vert_woff;
   vert_coord[7] = V_TR[1] - vert_hoff - vert_scaled_mid_height;

   tex_coord[0] = T_BL[0] + tex_woff + tex_mid_width;
   tex_coord[1] = T_BL[1] + tex_hoff + tex_mid_height;
   tex_coord[2] = T_BR[0] + tex_woff + tex_mid_width;
   tex_coord[3] = T_BR[1] + tex_hoff + tex_mid_height;
   tex_coord[4] = T_TL[0] + tex_woff + tex_mid_width;
   tex_coord[5] = T_TL[1] + tex_hoff + tex_mid_height;
   tex_coord[6] = T_TR[0] + tex_woff + tex_mid_width;
   tex_coord[7] = T_TR[1] + tex_hoff + tex_mid_height;

   menu_display_draw(&draw, video_info);

   free(colors);
   free(vert_coord);
   free(tex_coord);
}

void menu_display_rotate_z(menu_display_ctx_rotate_draw_t *draw,
      video_frame_info_t *video_info)
{
   math_matrix_4x4 matrix_rotated, matrix_scaled;
   math_matrix_4x4 *b = NULL;

   if (
         !draw                       ||
         !menu_disp                  ||
         !menu_disp->get_default_mvp ||
         menu_disp->handles_transform
      )
      return;

   b = (math_matrix_4x4*)menu_disp->get_default_mvp(video_info);

   if (!b)
      return;

   matrix_4x4_rotate_z(matrix_rotated, draw->rotation);
   matrix_4x4_multiply(*draw->matrix, matrix_rotated, *b);

   if (!draw->scale_enable)
      return;

   matrix_4x4_scale(matrix_scaled,
         draw->scale_x, draw->scale_y, draw->scale_z);
   matrix_4x4_multiply(*draw->matrix, matrix_scaled, *draw->matrix);
}

bool menu_display_get_tex_coords(menu_display_ctx_coord_draw_t *draw)
{
   if (!draw)
      return false;

   if (!menu_disp || !menu_disp->get_default_tex_coords)
      return false;

   draw->ptr = menu_disp->get_default_tex_coords();
   return true;
}

static bool menu_driver_load_image(menu_ctx_load_image_t *load_image_info)
{
   if (menu_driver_ctx && menu_driver_ctx->load_image)
      return menu_driver_ctx->load_image(menu_userdata,
            load_image_info->data, load_image_info->type);
   return false;
}

void menu_display_handle_thumbnail_upload(void *task_data,
      void *user_data, const char *err)
{
   menu_ctx_load_image_t load_image_info;
   struct texture_image *img = (struct texture_image*)task_data;

   load_image_info.data = img;
   load_image_info.type = MENU_IMAGE_THUMBNAIL;

   menu_driver_load_image(&load_image_info);

   image_texture_free(img);
   free(img);
   free(user_data);
}

void menu_display_handle_left_thumbnail_upload(void *task_data,
      void *user_data, const char *err)
{
   menu_ctx_load_image_t load_image_info;
   struct texture_image *img = (struct texture_image*)task_data;

   load_image_info.data = img;
   load_image_info.type = MENU_IMAGE_LEFT_THUMBNAIL;

   menu_driver_load_image(&load_image_info);

   image_texture_free(img);
   free(img);
   free(user_data);
}

void menu_display_handle_savestate_thumbnail_upload(void *task_data,
      void *user_data, const char *err)
{
   menu_ctx_load_image_t load_image_info;
   struct texture_image *img = (struct texture_image*)task_data;

   load_image_info.data = img;
   load_image_info.type = MENU_IMAGE_SAVESTATE_THUMBNAIL;

   menu_driver_load_image(&load_image_info);

   image_texture_free(img);
   free(img);
   free(user_data);
}

/* Function that gets called when we want to load in a
 * new menu wallpaper.
 */
void menu_display_handle_wallpaper_upload(void *task_data,
      void *user_data, const char *err)
{
   menu_ctx_load_image_t load_image_info;
   struct texture_image *img = (struct texture_image*)task_data;

   load_image_info.data = img;
   load_image_info.type = MENU_IMAGE_WALLPAPER;

   menu_driver_load_image(&load_image_info);
   image_texture_free(img);
   free(img);
   free(user_data);
}

void menu_display_allocate_white_texture(void)
{
   struct texture_image ti;
   static const uint8_t white_data[] = { 0xff, 0xff, 0xff, 0xff };

   ti.width  = 1;
   ti.height = 1;
   ti.pixels = (uint32_t*)&white_data;

   if (menu_display_white_texture)
      video_driver_texture_unload(&menu_display_white_texture);

   video_driver_texture_load(&ti,
         TEXTURE_FILTER_NEAREST, &menu_display_white_texture);
}

/*
 * Draw a hardware cursor on top of the screen for the mouse.
 */
void menu_display_draw_cursor(
      video_frame_info_t *video_info,
      float *color, float cursor_size, uintptr_t texture,
      float x, float y, unsigned width, unsigned height)
{
   menu_display_ctx_draw_t draw;
   struct video_coords coords;
   settings_t *settings = config_get_ptr();
   bool cursor_visible  = settings->bools.video_fullscreen ||
       !menu_display_has_windowed;

   if (!settings->bools.menu_mouse_enable || !cursor_visible)
      return;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = (const float*)color;

   if (menu_disp && menu_disp->blend_begin)
      menu_disp->blend_begin(video_info);

   draw.x               = x - (cursor_size / 2);
   draw.y               = (int)height - y - (cursor_size / 2);
   draw.width           = cursor_size;
   draw.height          = cursor_size;
   draw.coords          = &coords;
   draw.matrix_data     = NULL;
   draw.texture         = texture;
   draw.prim_type       = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id     = 0;

   menu_display_draw(&draw, video_info);

   if (menu_disp && menu_disp->blend_end)
      menu_disp->blend_end(video_info);
}

static INLINE float menu_display_scalef(float val,
      float oldmin, float oldmax, float newmin, float newmax)
{
   return (((val - oldmin) * (newmax - newmin)) / (oldmax - oldmin)) + newmin;
}

static INLINE float menu_display_randf(float min, float max)
{
   return (rand() * ((max - min) / RAND_MAX)) + min;
}

void menu_display_push_quad(
      unsigned width, unsigned height,
      const float *colors, int x1, int y1,
      int x2, int y2)
{
   float vertex[8];
   video_coords_t coords;
   menu_display_ctx_coord_draw_t coord_draw;
   video_coord_array_t *ca = menu_display_get_coords_array();

   vertex[0]             = x1 / (float)width;
   vertex[1]             = y1 / (float)height;
   vertex[2]             = x2 / (float)width;
   vertex[3]             = y1 / (float)height;
   vertex[4]             = x1 / (float)width;
   vertex[5]             = y2 / (float)height;
   vertex[6]             = x2 / (float)width;
   vertex[7]             = y2 / (float)height;

   coord_draw.ptr        = NULL;

   menu_display_get_tex_coords(&coord_draw);

   coords.color          = colors;
   coords.vertex         = vertex;
   coords.tex_coord      = coord_draw.ptr;
   coords.lut_tex_coord  = coord_draw.ptr;
   coords.vertices       = 3;

   video_coord_array_append(ca, &coords, 3);

   coords.color         += 4;
   coords.vertex        += 2;
   coords.tex_coord     += 2;
   coords.lut_tex_coord += 2;

   video_coord_array_append(ca, &coords, 3);
}

void menu_display_snow(int width, int height)
{
   struct display_particle
   {
      float x, y;
      float xspeed, yspeed;
      float alpha;
      bool alive;
   };
   static struct display_particle particles[PARTICLES_COUNT] = {{0}};
   static int timeout      = 0;
   unsigned i, max_gen     = 2;

   for (i = 0; i < PARTICLES_COUNT; ++i)
   {
      struct display_particle *p = (struct display_particle*)&particles[i];

      if (p->alive)
      {
         int16_t mouse_x  = menu_input_mouse_state(
               MENU_MOUSE_X_AXIS);

         p->y            += p->yspeed;
         p->x            += menu_display_scalef(
               mouse_x, 0, width, -0.3, 0.3);
         p->x            += p->xspeed;

         p->alive         = p->y >= 0 && p->y < height
            && p->x >= 0 && p->x < width;
      }
      else if (max_gen > 0 && timeout <= 0)
      {
         p->xspeed = menu_display_randf(-0.2, 0.2);
         p->yspeed = menu_display_randf(1, 2);
         p->y      = 0;
         p->x      = rand() % width;
         p->alpha  = (float)rand() / (float)RAND_MAX;
         p->alive  = true;

         max_gen--;
      }
   }

   if (max_gen == 0)
      timeout = 3;
   else
      timeout--;

   for (i = 0; i < PARTICLES_COUNT; ++i)
   {
      unsigned j;
      float alpha, colors[16];
      struct display_particle *p = &particles[i];

      if (!p->alive)
         continue;

      alpha = menu_display_randf(0, 100) > 90 ?
         p->alpha/2 : p->alpha;

      for (j = 0; j < 16; j++)
      {
         colors[j] = 1;
         if (j == 3 || j == 7 || j == 11 || j == 15)
            colors[j] = alpha;
      }

      menu_display_push_quad(width, height,
            colors, p->x-2, p->y-2, p->x+2, p->y+2);

      j++;
   }
}

void menu_display_draw_keyboard(
      uintptr_t hover_texture,
      const font_data_t *font,
      video_frame_info_t *video_info,
      char *grid[], unsigned id)
{
   unsigned i;
   int ptr_width, ptr_height;
   unsigned width    = video_info->width;
   unsigned height   = video_info->height;
   float dark[16]    =  {
      0.00, 0.00, 0.00, 0.85,
      0.00, 0.00, 0.00, 0.85,
      0.00, 0.00, 0.00, 0.85,
      0.00, 0.00, 0.00, 0.85,
   };

   float white[16]=  {
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
      1.00, 1.00, 1.00, 1.00,
   };

   menu_display_draw_quad(
         video_info,
         0, height/2.0, width, height/2.0,
         width, height,
         &dark[0]);

   ptr_width  = width  / 11;
   ptr_height = height / 10;

   if (ptr_width >= ptr_height)
      ptr_width = ptr_height;

   for (i = 0; i < 44; i++)
   {
      int line_y = (i / 11) * height / 10.0;

      if (i == id)
      {
         menu_display_blend_begin(video_info);

         menu_display_draw_texture(
               video_info,
               width/2.0 - (11*ptr_width)/2.0 + (i % 11) * ptr_width,
               height/2.0 + ptr_height*1.5 + line_y,
               ptr_width, ptr_height,
               width, height,
               &white[0],
               hover_texture);

         menu_display_blend_end(video_info);
      }

      menu_display_draw_text(font, grid[i],
            width/2.0 - (11*ptr_width)/2.0 + (i % 11)
            * ptr_width + ptr_width/2.0,
            height/2.0 + ptr_height + line_y + font->size / 3,
            width, height, 0xffffffff, TEXT_ALIGN_CENTER, 1.0f,
            false, 0);
   }
}

/* Draw text on top of the screen.
 */
void menu_display_draw_text(
      const font_data_t *font, const char *text,
      float x, float y, int width, int height,
      uint32_t color, enum text_alignment text_align,
      float scale, bool shadows_enable, float shadow_offset)
{
   struct font_params params;

   /* Don't draw outside of the screen */
   if (     (x < -64 || x > width  + 64)
         || (y < -64 || y > height + 64)
      )
      return;

   params.x           = x / width;
   params.y           = 1.0f - y / height;
   params.scale       = scale;
   params.drop_mod    = 0.0f;
   params.drop_x      = 0.0f;
   params.drop_y      = 0.0f;
   params.color       = color;
   params.full_screen = true;
   params.text_align  = text_align;

   if (shadows_enable)
   {
      params.drop_x      = shadow_offset;
      params.drop_y      = -shadow_offset;
      params.drop_alpha  = 0.35f;
   }

   video_driver_set_osd_msg(text, &params, (void*)font);
}

void menu_display_reset_textures_list(
      const char *texture_path, const char *iconpath,
      uintptr_t *item, enum texture_filter_type filter_type)
{
   struct texture_image ti;
   char texpath[PATH_MAX_LENGTH] = {0};

   ti.width                      = 0;
   ti.height                     = 0;
   ti.pixels                     = NULL;
   ti.supports_rgba              = video_driver_supports_rgba();

   if (!string_is_empty(texture_path))
      fill_pathname_join(texpath, iconpath, texture_path, sizeof(texpath));

   if (string_is_empty(texpath) || !filestream_exists(texpath))
      return;

   if (!image_texture_load(&ti, texpath))
      return;

   video_driver_texture_load(&ti,
         filter_type, item);
   image_texture_free(&ti);
}

bool menu_driver_is_binding_state(void)
{
   return menu_driver_is_binding;
}

void menu_driver_set_binding_state(bool on)
{
   menu_driver_is_binding = on;
}

/**
 * menu_driver_find_handle:
 * @idx              : index of driver to get handle to.
 *
 * Returns: handle to menu driver at index. Can be NULL
 * if nothing found.
 **/
const void *menu_driver_find_handle(int idx)
{
   const void *drv = menu_ctx_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * menu_driver_find_ident:
 * @idx              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of menu driver at index.
 * Can be NULL if nothing found.
 **/
const char *menu_driver_find_ident(int idx)
{
   const menu_ctx_driver_t *drv = menu_ctx_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_menu_driver_options:
 *
 * Get an enumerated list of all menu driver names,
 * separated by '|'.
 *
 * Returns: string listing of all menu driver names,
 * separated by '|'.
 **/
const char *config_get_menu_driver_options(void)
{
   return char_list_new_special(STRING_LIST_MENU_DRIVERS, NULL);
}

#ifdef HAVE_COMPRESSION
/* This function gets called at first startup on Android/iOS
 * when we need to extract the APK contents/zip file. This
 * file contains assets which then get extracted to the
 * user's asset directories. */
static void bundle_decompressed(void *task_data,
      void *user_data, const char *err)
{
   settings_t      *settings   = config_get_ptr();
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;

   if (dec && !err)
      command_event(CMD_EVENT_REINIT, NULL);

   if (err)
      RARCH_ERR("%s", err);

   if (dec)
   {
      /* delete bundle? */
      free(dec->source_file);
      free(dec);
   }

   settings->uints.bundle_assets_extract_last_version =
      settings->uints.bundle_assets_extract_version_current;

   configuration_set_bool(settings, settings->bools.bundle_finished, true);

   command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
}
#endif

/**
 * menu_init:
 * @data                     : Menu context handle.
 *
 * Create and initialize menu handle.
 *
 * Returns: menu handle on success, otherwise NULL.
 **/
static bool menu_init(menu_handle_t *menu_data)
{
   settings_t *settings        = config_get_ptr();

   if (!menu_entries_ctl(MENU_ENTRIES_CTL_INIT, NULL))
      return false;

   if (settings->bools.menu_show_start_screen)
   {
      menu_dialog_push_pending(true, MENU_DIALOG_WELCOME);

      configuration_set_bool(settings,
            settings->bools.menu_show_start_screen, false);

      if (settings->bools.config_save_on_exit)
         command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
   }

   if (      settings->bools.bundle_assets_extract_enable
         && !string_is_empty(settings->arrays.bundle_assets_src)
         && !string_is_empty(settings->arrays.bundle_assets_dst)
#ifdef IOS
         && menu_dialog_is_push_pending()
#else
         && (settings->uints.bundle_assets_extract_version_current
            != settings->uints.bundle_assets_extract_last_version)
#endif
      )
   {
      menu_dialog_push_pending(true, MENU_DIALOG_HELP_EXTRACT);
#ifdef HAVE_COMPRESSION
      task_push_decompress(settings->arrays.bundle_assets_src,
            settings->arrays.bundle_assets_dst,
            NULL, settings->arrays.bundle_assets_dst_subdir,
            NULL, bundle_decompressed, NULL);
#endif
   }

   menu_shader_manager_init();

   menu_disp_ca.allocated    =  0;

   menu_display_has_windowed = video_driver_has_windowed();

   return true;
}

/* This callback gets triggered by the keyboard whenever
 * we press or release a keyboard key. When a keyboard
 * key is being pressed down, 'down' will be true. If it
 * is being released, 'down' will be false.
 */
static void menu_input_key_event(bool down, unsigned keycode,
      uint32_t character, uint16_t mod)
{
   (void)down;
   (void)keycode;
   (void)mod;

#if 0
   RARCH_LOG("down: %d, keycode: %d, mod: %d, character: %d\n",
         down, keycode, mod, character);
#endif

   menu_event_kb_set(down, (enum retro_key)keycode);
}

/* Gets called when we want to toggle the menu.
 * If the menu is already running, it will be turned off.
 * If the menu is off, then the menu will be started.
 */
static void menu_driver_toggle(bool on)
{
   retro_keyboard_event_t *key_event          = NULL;
   retro_keyboard_event_t *frontend_key_event = NULL;
   settings_t                 *settings       = config_get_ptr();
   bool pause_libretro                        = settings ?
      settings->bools.menu_pause_libretro : false;
   bool enable_menu_sound                     = settings ?
      settings->bools.audio_enable_menu : false;

   menu_driver_toggled = on;

   if (!on)
      menu_display_toggle_set_reason(MENU_TOGGLE_REASON_NONE);

   if (menu_driver_ctx && menu_driver_ctx->toggle)
      menu_driver_ctx->toggle(menu_userdata, on);

   if (on)
      menu_driver_alive = true;
   else
      menu_driver_alive = false;

   rarch_ctl(RARCH_CTL_FRONTEND_KEY_EVENT_GET, &frontend_key_event);
   rarch_ctl(RARCH_CTL_KEY_EVENT_GET,          &key_event);

   if (menu_driver_alive)
   {
      bool refresh = false;

#ifdef WIIU
      /* Enable burn-in protection menu is running */
      IMEnableDim();
#endif

      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);

      /* Menu should always run with vsync on. */
      command_event(CMD_EVENT_VIDEO_SET_BLOCKING_STATE, NULL);
      /* Stop all rumbling before entering the menu. */
      command_event(CMD_EVENT_RUMBLE_STOP, NULL);

      if (pause_libretro && !enable_menu_sound)
         command_event(CMD_EVENT_AUDIO_STOP, NULL);

      /* Override keyboard callback to redirect to menu instead.
       * We'll use this later for something ... */

      if (key_event && frontend_key_event)
      {
         *frontend_key_event        = *key_event;
         *key_event                 = menu_input_key_event;

         rarch_ctl(RARCH_CTL_SET_FRAME_TIME_LAST, NULL);
      }
   }
   else
   {
#ifdef WIIU
      /* Disable burn-in protection while core is running; this is needed
       * because HID inputs don't count for the purpose of Wii U
       * power-saving. */
      IMDisableDim();
#endif

      if (!rarch_ctl(RARCH_CTL_IS_SHUTDOWN, NULL))
         driver_set_nonblock_state();

      if (pause_libretro && !enable_menu_sound)
         command_event(CMD_EVENT_AUDIO_START, NULL);

      /* Restore libretro keyboard callback. */
      if (key_event && frontend_key_event)
         *key_event = *frontend_key_event;
   }
}

const char *menu_driver_ident(void)
{
   if (!menu_driver_alive)
      return NULL;
   if (!menu_driver_ctx || !menu_driver_ctx->ident)
      return NULL;
  return menu_driver_ctx->ident;
}

void menu_driver_frame(video_frame_info_t *video_info)
{
   if (menu_driver_alive && menu_driver_ctx->frame)
      menu_driver_ctx->frame(menu_userdata, video_info);
}

bool menu_driver_render(bool is_idle, bool rarch_is_inited,
      bool rarch_is_dummy_core)
{
   if (!menu_driver_data)
      return false;

   if (BIT64_GET(menu_driver_data->state, MENU_STATE_RENDER_FRAMEBUFFER)
         != BIT64_GET(menu_driver_data->state, MENU_STATE_RENDER_MESSAGEBOX))
      BIT64_SET(menu_driver_data->state, MENU_STATE_RENDER_FRAMEBUFFER);

   if (BIT64_GET(menu_driver_data->state, MENU_STATE_RENDER_FRAMEBUFFER))
      menu_display_framebuf_dirty = true;

   if (BIT64_GET(menu_driver_data->state, MENU_STATE_RENDER_MESSAGEBOX)
         && !string_is_empty(menu_driver_data->menu_state_msg))
   {
      if (menu_driver_ctx->render_messagebox)
         menu_driver_ctx->render_messagebox(menu_userdata,
               menu_driver_data->menu_state_msg);

      if (ui_companion_is_on_foreground())
      {
         const ui_companion_driver_t *ui = ui_companion_get_ptr();
         if (ui->render_messagebox)
            ui->render_messagebox(menu_driver_data->menu_state_msg);
      }
   }

   if (BIT64_GET(menu_driver_data->state, MENU_STATE_BLIT))
   {
      settings_t *settings = config_get_ptr();
      menu_animation_update_time(settings->bools.menu_timedate_enable);

      if (menu_driver_ctx->render)
         menu_driver_ctx->render(menu_userdata, is_idle);
   }

   if (menu_driver_alive && !is_idle)
      menu_display_libretro(is_idle, rarch_is_inited, rarch_is_dummy_core);

   if (menu_driver_ctx->set_texture)
      menu_driver_ctx->set_texture();

   menu_driver_data->state               = 0;

   return true;
}

/* Checks if the menu is still running */
bool menu_driver_is_alive(void)
{
   return menu_driver_alive;
}

/* Checks if the menu framebuffer is set.
 * This would usually only return true
 * for framebuffer-based menu drivers, like RGUI. */
bool menu_driver_is_texture_set(void)
{
   if (menu_driver_ctx && menu_driver_ctx->set_texture)
      return true;
   return false;
}

/* Iterate the menu driver for one frame. */
bool menu_driver_iterate(menu_ctx_iterate_t *iterate)
{
   /* If the user had requested that the Quick Menu
    * be spawned during the previous frame, do this now
    * and exit the function to go to the next frame.
    */
   if (menu_driver_pending_quick_menu)
   {
      menu_driver_pending_quick_menu = false;
      menu_entries_flush_stack(NULL, MENU_SETTINGS);
      menu_display_set_msg_force(true);

      generic_action_ok_displaylist_push("", NULL,
            "", 0, 0, 0, ACTION_OK_DL_CONTENT_SETTINGS);

      if (menu_driver_pending_quit)
      {
         menu_driver_pending_quit     = false;
         return false;
      }

      return true;
   }

   /* If the user had requested that the menu
    * be shutdown during the previous frame, do
    * this now. */
   if (menu_driver_pending_quit)
   {
      menu_driver_pending_quit     = false;
      return false;
   }

   /* if the user had requested that RetroArch
    * be shutdown during the previous frame, do
    * this now. */
   if (menu_driver_pending_shutdown)
   {
      menu_driver_pending_shutdown = false;
      if (!command_event(CMD_EVENT_QUIT, NULL))
         return false;
      return true;
   }

   if (!menu_driver_ctx || !menu_driver_ctx->iterate)
      return false;

   if (menu_driver_ctx->iterate(menu_driver_data,
            menu_userdata, iterate->action) == -1)
      return false;

   return true;
}

bool menu_driver_list_cache(menu_ctx_list_t *list)
{
   if (!list || !menu_driver_ctx || !menu_driver_ctx->list_cache)
      return false;

   menu_driver_ctx->list_cache(menu_userdata,
         list->type, list->action);
   return true;
}

/* Clear all the menu lists. */
bool menu_driver_list_clear(file_list_t *list)
{
   if (!list)
      return false;
   if (menu_driver_ctx->list_clear)
      menu_driver_ctx->list_clear(list);
   return true;
}

bool menu_driver_list_insert(menu_ctx_list_t *list)
{
   if (!list || !menu_driver_ctx || !menu_driver_ctx->list_insert)
      return false;
   menu_driver_ctx->list_insert(menu_userdata,
         list->list, list->path, list->fullpath,
         list->label, list->idx, list->entry_type);

   return true;
}

bool menu_driver_list_set_selection(file_list_t *list)
{
   if (!list)
      return false;

   if (!menu_driver_ctx || !menu_driver_ctx->list_set_selection)
      return false;

   menu_driver_ctx->list_set_selection(menu_userdata, list);
   return true;
}

static void menu_update_libretro_info(void)
{
   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
   command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
}

static bool menu_driver_init_internal(bool video_is_threaded)
{
   settings_t *settings           = config_get_ptr();
   menu_update_libretro_info();

   if (menu_driver_data)
      return true;

   menu_driver_data               = (menu_handle_t*)
      menu_driver_ctx->init(&menu_userdata, video_is_threaded);

   if (!menu_driver_data || !menu_init(menu_driver_data))
      goto error;

   strlcpy(settings->arrays.menu_driver, menu_driver_ctx->ident,
         sizeof(settings->arrays.menu_driver));

   if (menu_driver_ctx->lists_init)
      if (!menu_driver_ctx->lists_init(menu_driver_data))
         goto error;

   return true;

error:
   retroarch_fail(1, "init_menu()");
   return false;
}

static bool menu_driver_context_reset(bool video_is_threaded)
{
   if (!menu_driver_ctx || !menu_driver_ctx->context_reset)
      return false;
   menu_driver_ctx->context_reset(menu_userdata, video_is_threaded);
   return true;
}

bool menu_driver_init(bool video_is_threaded)
{
   if (menu_driver_init_internal(video_is_threaded))
      return menu_driver_context_reset(video_is_threaded);
   return false;
}

void menu_driver_navigation_set(bool scroll)
{
   if (menu_driver_ctx->navigation_set)
      menu_driver_ctx->navigation_set(menu_userdata, scroll);
}

void menu_driver_populate_entries(menu_displaylist_info_t *info)
{
   if (menu_driver_ctx && menu_driver_ctx->populate_entries)
      menu_driver_ctx->populate_entries(
            menu_userdata, info->path,
            info->label, info->type);
}

bool menu_driver_push_list(menu_ctx_displaylist_t *disp_list)
{
   if (menu_driver_ctx->list_push)
      if (menu_driver_ctx->list_push(menu_driver_data,
               menu_userdata, disp_list->info, disp_list->type) == 0)
         return true;
   return false;
}

void menu_driver_set_thumbnail_system(char *s, size_t len)
{
   if (menu_driver_ctx && menu_driver_ctx->set_thumbnail_system)
      menu_driver_ctx->set_thumbnail_system(menu_userdata, s, len);
}

void menu_driver_set_thumbnail_content(char *s, size_t len)
{
   if (menu_driver_ctx && menu_driver_ctx->set_thumbnail_content)
      menu_driver_ctx->set_thumbnail_content(menu_userdata, s, len);
}

/* Teardown function for the menu driver. */
void menu_driver_destroy(void)
{
   menu_driver_pending_quick_menu = false;
   menu_driver_pending_quit       = false;
   menu_driver_pending_shutdown   = false;
   menu_driver_prevent_populate   = false;
   menu_driver_alive              = false;
   menu_driver_data_own           = false;
   menu_driver_ctx                = NULL;
   menu_userdata                  = NULL;
}

bool menu_driver_list_get_entry(menu_ctx_list_t *list)
{
   if (!menu_driver_ctx || !menu_driver_ctx->list_get_entry)
   {
      list->entry = NULL;
      return false;
   }
   list->entry = menu_driver_ctx->list_get_entry(menu_userdata,
         list->type, (unsigned int)list->idx);
   return true;
}

bool menu_driver_list_get_selection(menu_ctx_list_t *list)
{
   if (!menu_driver_ctx || !menu_driver_ctx->list_get_selection)
   {
      list->selection = 0;
      return false;
   }
   list->selection = menu_driver_ctx->list_get_selection(menu_userdata);

   return true;
}

bool menu_driver_list_get_size(menu_ctx_list_t *list)
{
   if (!menu_driver_ctx || !menu_driver_ctx->list_get_size)
   {
      list->size = 0;
      return false;
   }
   list->size = menu_driver_ctx->list_get_size(menu_userdata, list->type);
   return true;
}

bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data)
{
   switch (state)
   {
      case RARCH_MENU_CTL_DRIVER_DATA_GET:
         {
            menu_handle_t **driver_data = (menu_handle_t**)data;
            if (!driver_data)
               return false;
            *driver_data = menu_driver_data;
         }
         break;
      case RARCH_MENU_CTL_SET_PENDING_QUICK_MENU:
         menu_driver_pending_quick_menu = true;
         break;
      case RARCH_MENU_CTL_SET_PENDING_QUIT:
         menu_driver_pending_quit     = true;
         break;
      case RARCH_MENU_CTL_SET_PENDING_SHUTDOWN:
         menu_driver_pending_shutdown = true;
         break;
      case RARCH_MENU_CTL_FIND_DRIVER:
         {
            int i;
            driver_ctx_info_t drv;
            settings_t *settings = config_get_ptr();

            drv.label = "menu_driver";
            drv.s     = settings->arrays.menu_driver;

            driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

            i = (int)drv.len;

            if (i >= 0)
               menu_driver_ctx = (const menu_ctx_driver_t*)
                  menu_driver_find_handle(i);
            else
            {
               if (verbosity_is_enabled())
               {
                  unsigned d;
                  RARCH_WARN("Couldn't find any menu driver named \"%s\"\n",
                        settings->arrays.menu_driver);
                  RARCH_LOG_OUTPUT("Available menu drivers are:\n");
                  for (d = 0; menu_driver_find_handle(d); d++)
                     RARCH_LOG_OUTPUT("\t%s\n", menu_driver_find_ident(d));
                  RARCH_WARN("Going to default to first menu driver...\n");
               }

               menu_driver_ctx = (const menu_ctx_driver_t*)
                  menu_driver_find_handle(0);

               if (!menu_driver_ctx)
               {
                  retroarch_fail(1, "find_menu_driver()");
                  return false;
               }
            }
         }
         break;
      case RARCH_MENU_CTL_SET_PREVENT_POPULATE:
         menu_driver_prevent_populate = true;
         break;
      case RARCH_MENU_CTL_UNSET_PREVENT_POPULATE:
         menu_driver_prevent_populate = false;
         break;
      case RARCH_MENU_CTL_IS_PREVENT_POPULATE:
         return menu_driver_prevent_populate;
      case RARCH_MENU_CTL_IS_TOGGLE:
         return menu_driver_toggled;
      case RARCH_MENU_CTL_SET_TOGGLE:
         menu_driver_toggle(true);
         break;
      case RARCH_MENU_CTL_UNSET_TOGGLE:
         menu_driver_toggle(false);
         break;
      case RARCH_MENU_CTL_SET_OWN_DRIVER:
         menu_driver_data_own = true;
         break;
      case RARCH_MENU_CTL_UNSET_OWN_DRIVER:
         menu_driver_data_own = false;
         break;
      case RARCH_MENU_CTL_OWNS_DRIVER:
         return menu_driver_data_own;
      case RARCH_MENU_CTL_DEINIT:
         if (menu_driver_ctx && menu_driver_ctx->context_destroy)
            menu_driver_ctx->context_destroy(menu_userdata);

         if (menu_driver_data_own)
            return true;

         playlist_free_cached();
         menu_shader_manager_free();

         if (menu_driver_data)
         {
            unsigned i;

            scroll_acceleration       = 0;
            menu_driver_selection_ptr = 0;
            scroll_index_size         = 0;

            for (i = 0; i < SCROLL_INDEX_SIZE; i++)
               scroll_index_list[i] = 0;

            menu_input_ctl(MENU_INPUT_CTL_DEINIT, NULL);

            if (menu_driver_ctx && menu_driver_ctx->free)
               menu_driver_ctx->free(menu_userdata);

            if (menu_userdata)
               free(menu_userdata);
            menu_userdata = NULL;

#ifndef HAVE_DYNAMIC
            if (frontend_driver_has_fork())
#endif
            {
               rarch_system_info_t *system = runloop_get_system_info();
               libretro_free_system_info(&system->info);
               memset(&system->info, 0, sizeof(struct retro_system_info));
            }

            video_coord_array_free(&menu_disp_ca);
            menu_display_msg_force       = false;
            menu_display_header_height   = 0;
            menu_disp                    = NULL;
            menu_display_has_windowed    = false;

            menu_animation_ctl(MENU_ANIMATION_CTL_DEINIT, NULL);

            menu_display_framebuf_width  = 0;
            menu_display_framebuf_height = 0;
            menu_display_framebuf_pitch  = 0;
            menu_entries_ctl(MENU_ENTRIES_CTL_DEINIT, NULL);

            command_event(CMD_EVENT_HISTORY_DEINIT, NULL);

            menu_dialog_reset();

            free(menu_driver_data);
         }
         menu_driver_data = NULL;
         break;
      case RARCH_MENU_CTL_LIST_FREE:
         {
            menu_ctx_list_t *list = (menu_ctx_list_t*)data;

            if (menu_driver_ctx)
            {
               if (menu_driver_ctx->list_free)
                  menu_driver_ctx->list_free(list->list, list->idx, list->list_size);
            }

            if (list->list)
            {
               file_list_free_userdata  (list->list, list->idx);
               file_list_free_actiondata(list->list, list->idx);
            }
         }
         break;
      case RARCH_MENU_CTL_ENVIRONMENT:
         {
            menu_ctx_environment_t *menu_environ =
               (menu_ctx_environment_t*)data;

            if (menu_driver_ctx->environ_cb)
            {
               if (menu_driver_ctx->environ_cb(menu_environ->type,
                        menu_environ->data, menu_userdata) == 0)
                  return true;
            }
         }
         return false;
      case RARCH_MENU_CTL_POINTER_TAP:
         {
            menu_ctx_pointer_t *point = (menu_ctx_pointer_t*)data;
            if (!menu_driver_ctx || !menu_driver_ctx->pointer_tap)
            {
               point->retcode = 0;
               return false;
            }
            point->retcode = menu_driver_ctx->pointer_tap(menu_userdata,
                  point->x, point->y, point->ptr,
                  point->cbs, point->entry, point->action);
         }
         break;
      case RARCH_MENU_CTL_POINTER_DOWN:
         {
            menu_ctx_pointer_t *point = (menu_ctx_pointer_t*)data;
            if (!menu_driver_ctx || !menu_driver_ctx->pointer_down)
            {
               point->retcode = 0;
               return false;
            }
            point->retcode = menu_driver_ctx->pointer_down(menu_userdata,
                  point->x, point->y, point->ptr,
                  point->cbs, point->entry, point->action);
         }
         break;
      case RARCH_MENU_CTL_POINTER_UP:
         {
            menu_ctx_pointer_t *point = (menu_ctx_pointer_t*)data;
            if (!menu_driver_ctx || !menu_driver_ctx->pointer_up)
            {
               point->retcode = 0;
               return false;
            }
            point->retcode = menu_driver_ctx->pointer_up(menu_userdata,
                  point->x, point->y, point->ptr,
                  point->cbs, point->entry, point->action);
         }
         break;
      case RARCH_MENU_CTL_OSK_PTR_AT_POS:
         {
            unsigned width            = 0;
            unsigned height           = 0;
            menu_ctx_pointer_t *point = (menu_ctx_pointer_t*)data;
            if (!menu_driver_ctx || !menu_driver_ctx->osk_ptr_at_pos)
            {
               point->retcode = 0;
               return false;
            }
            video_driver_get_size(&width, &height);
            point->retcode = menu_driver_ctx->osk_ptr_at_pos(menu_userdata,
                  point->x, point->y, width, height);
         }
         break;
      case RARCH_MENU_CTL_BIND_INIT:
         {
            menu_ctx_bind_t *bind = (menu_ctx_bind_t*)data;

            if (!menu_driver_ctx || !menu_driver_ctx->bind_init)
            {
               bind->retcode = 0;
               return false;
            }
            bind->retcode = menu_driver_ctx->bind_init(
                  bind->cbs,
                  bind->path,
                  bind->label,
                  bind->type,
                  bind->idx);
         }
         break;
      case RARCH_MENU_CTL_UPDATE_THUMBNAIL_PATH:
         {
            size_t selection = menu_navigation_get_selection();

            if (!menu_driver_ctx || !menu_driver_ctx->update_thumbnail_path)
               return false;
            menu_driver_ctx->update_thumbnail_path(menu_userdata, (unsigned)selection, 'L');
            menu_driver_ctx->update_thumbnail_path(menu_userdata, (unsigned)selection, 'R');
         }
         break;
      case RARCH_MENU_CTL_UPDATE_THUMBNAIL_IMAGE:
         {
            if (!menu_driver_ctx || !menu_driver_ctx->update_thumbnail_image)
               return false;
            menu_driver_ctx->update_thumbnail_image(menu_userdata);
         }
         break;
      case RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_PATH:
         {
            size_t selection = menu_navigation_get_selection();

            if (!menu_driver_ctx || !menu_driver_ctx->update_savestate_thumbnail_path)
               return false;
            menu_driver_ctx->update_savestate_thumbnail_path(menu_userdata, (unsigned)selection);
         }
         break;
      case RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_IMAGE:
         {
            if (!menu_driver_ctx || !menu_driver_ctx->update_savestate_thumbnail_image)
               return false;
            menu_driver_ctx->update_savestate_thumbnail_image(menu_userdata);
         }
         break;
      case MENU_NAVIGATION_CTL_CLEAR:
         {
            bool *pending_push = (bool*)data;

            menu_navigation_set_selection(0);
            menu_driver_navigation_set(true);

            if (pending_push)
               if (menu_driver_ctx->navigation_clear)
                  menu_driver_ctx->navigation_clear(
                        menu_userdata, *pending_push);
         }
         break;
      case MENU_NAVIGATION_CTL_INCREMENT:
         {
            settings_t *settings   = config_get_ptr();
            unsigned scroll_speed  = *((unsigned*)data);
            size_t  menu_list_size = menu_entries_get_size();
            bool wraparound_enable = settings->bools.menu_navigation_wraparound_enable;

            if (menu_driver_selection_ptr >= menu_list_size - 1
                  && !wraparound_enable)
               return false;

            if ((menu_driver_selection_ptr + scroll_speed) < menu_list_size)
            {
               size_t idx  = menu_driver_selection_ptr + scroll_speed;

               menu_navigation_set_selection(idx);
               menu_driver_navigation_set(true);
            }
            else
            {
               if (wraparound_enable)
               {
                  bool pending_push = false;
                  menu_driver_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
               }
               else if (menu_list_size > 0)
                  menu_driver_ctl(MENU_NAVIGATION_CTL_SET_LAST,  NULL);
            }

            if (menu_driver_ctx->navigation_increment)
               menu_driver_ctx->navigation_increment(menu_userdata);
         }
         break;
      case MENU_NAVIGATION_CTL_DECREMENT:
         {
            size_t idx             = 0;
            settings_t *settings   = config_get_ptr();
            unsigned scroll_speed  = *((unsigned*)data);
            size_t  menu_list_size = menu_entries_get_size();
            bool wraparound_enable = settings->bools.menu_navigation_wraparound_enable;

            if (menu_driver_selection_ptr == 0 && !wraparound_enable)
               return false;

            if (menu_driver_selection_ptr >= scroll_speed)
               idx = menu_driver_selection_ptr - scroll_speed;
            else
            {
               idx  = menu_list_size - 1;
               if (!wraparound_enable)
                  idx = 0;
            }

            menu_navigation_set_selection(idx);
            menu_driver_navigation_set(true);

            if (menu_driver_ctx->navigation_decrement)
               menu_driver_ctx->navigation_decrement(menu_userdata);
         }
         break;
      case MENU_NAVIGATION_CTL_SET_LAST:
         {
            size_t menu_list_size = menu_entries_get_size();
            size_t new_selection  = menu_list_size - 1;
            menu_navigation_set_selection(new_selection);

            if (menu_driver_ctx->navigation_set_last)
               menu_driver_ctx->navigation_set_last(menu_userdata);
         }
         break;
      case MENU_NAVIGATION_CTL_ASCEND_ALPHABET:
         {
            size_t i               = 0;
            size_t  menu_list_size = menu_entries_get_size();

            if (!scroll_index_size)
               return false;

            if (menu_driver_selection_ptr == scroll_index_list[scroll_index_size - 1])
               menu_driver_selection_ptr = menu_list_size - 1;
            else
            {
               while (i < scroll_index_size - 1
                     && scroll_index_list[i + 1] <= menu_driver_selection_ptr)
                  i++;
               menu_driver_selection_ptr = scroll_index_list[i + 1];

               if (menu_driver_selection_ptr >= menu_list_size)
                  menu_driver_selection_ptr = menu_list_size - 1;
            }

            if (menu_driver_ctx->navigation_ascend_alphabet)
               menu_driver_ctx->navigation_ascend_alphabet(
                     menu_userdata, &menu_driver_selection_ptr);
         }
         break;
      case MENU_NAVIGATION_CTL_DESCEND_ALPHABET:
         {
            size_t i        = 0;

            if (!scroll_index_size)
               return false;

            if (menu_driver_selection_ptr == 0)
               return false;

            i   = scroll_index_size - 1;

            while (i && scroll_index_list[i - 1] >= menu_driver_selection_ptr)
               i--;

            if (i > 0)
               menu_driver_selection_ptr = scroll_index_list[i - 1];

            if (menu_driver_ctx->navigation_descend_alphabet)
               menu_driver_ctx->navigation_descend_alphabet(
                     menu_userdata, &menu_driver_selection_ptr);
         }
         break;
      case MENU_NAVIGATION_CTL_CLEAR_SCROLL_INDICES:
         scroll_index_size = 0;
         break;
      case MENU_NAVIGATION_CTL_ADD_SCROLL_INDEX:
         {
            size_t *sel        = (size_t*)data;
            if (!sel)
               return false;

            scroll_index_list[scroll_index_size]   = *sel;

            if (!((scroll_index_size + 1) >= SCROLL_INDEX_SIZE))
               scroll_index_size++;
         }
         break;
      case MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL:
         {
            size_t *sel = (size_t*)data;
            if (!sel)
               return false;
            *sel = scroll_acceleration;
         }
         break;
      case MENU_NAVIGATION_CTL_SET_SCROLL_ACCEL:
         {
            size_t *sel = (size_t*)data;
            if (!sel)
               return false;
            scroll_acceleration = (unsigned)(*sel);
         }
         break;
      default:
      case RARCH_MENU_CTL_NONE:
         break;
   }

   return true;
}

size_t menu_navigation_get_selection(void)
{
   return menu_driver_selection_ptr;
}

void menu_navigation_set_selection(size_t val)
{
   menu_driver_selection_ptr = val;
}
