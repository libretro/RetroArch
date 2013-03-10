/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include "../../driver.h"

#ifdef _XBOX
#include "../../xdk/xdk_d3d.h"
#endif

#include "../../console/rarch_console.h"

#include "../image.h"

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xgraphics.h>

#include "../../screenshot.h"

#include "../fonts/d3d_font.h"

#if defined(_XBOX1)
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTATIONINTERVAL
#elif defined(_XBOX360)
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTINTERVAL
#endif

#define ROM_PANEL_WIDTH 510
#define ROM_PANEL_HEIGHT 20

#define HARDCODE_FONT_SIZE 21

#define POSITION_X m_menuMainRomListPos_x
#define POSITION_X_CENTER (m_menuMainRomListPos_x + 350)
#define POSITION_Y_START m_menuMainRomListPos_y
#define POSITION_Y_BEGIN (POSITION_Y_START + POSITION_Y_INCREMENT)
#define POSITION_Y_INCREMENT 20
#define COMMENT_Y_POSITION (ypos - ((POSITION_Y_INCREMENT/2) * 3))
#define COMMENT_TWO_Y_POSITION (ypos - ((POSITION_Y_INCREMENT/2) * 1))

#define MSG_QUEUE_X_POSITION POSITION_X
#define MSG_QUEUE_Y_POSITION (ypos - ((POSITION_Y_INCREMENT/2) * 7) + 5)
#define MSG_QUEUE_FONT_SIZE HARDCODE_FONT_SIZE

#define MSG_PREV_NEXT_Y_POSITION 24

#define CURRENT_PATH_Y_POSITION (m_menuMainRomListPos_y - ((POSITION_Y_INCREMENT/2)))
#define CURRENT_PATH_FONT_SIZE 21

#define FONT_SIZE 21 

#define NUM_ENTRY_PER_PAGE 12

#define DRIVE_MAPPING_SIZE 5

const char drive_mappings[DRIVE_MAPPING_SIZE][32] = {
   "C:",
   "D:",
   "E:",
   "F:",
   "G:"
};

unsigned char drive_mapping_idx = 2;

int xpos, ypos;
#ifdef _XBOX1
// Rom list coordinates
unsigned m_menuMainRomListPos_x;
unsigned m_menuMainRomListPos_y;
#endif

static void gfx_ctx_xdk_set_blend(bool enable)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   if(enable)
   {
      d3d->d3d_render_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
      d3d->d3d_render_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   }
   d3d->d3d_render_device->SetRenderState(D3DRS_ALPHABLENDENABLE, enable);
}

static void gfx_ctx_xdk_set_swap_interval(unsigned interval)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->d3d_render_device;

   if (interval)
      d3dr->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_ONE);
   else
      d3dr->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_IMMEDIATE);
}

static void gfx_ctx_xdk_get_available_resolutions (void)
{
}


static void gfx_ctx_xdk_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   *quit = false;
   *resize = false;

   if (d3d->quitting)
      *quit = true;

   if (d3d->should_resize)
      *resize = true;
}

static void gfx_ctx_xdk_set_resize(unsigned width, unsigned height) { }

static bool gfx_ctx_xdk_menu_init(void)
{
#ifdef _XBOX1
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   int width  = d3d->win_width;

   // Load background image
   if(width == 640)
   {
      strlcpy(g_extern.console.menu_texture_path,"D:\\Media\\main-menu_480p.png",
            sizeof(g_extern.console.menu_texture_path));
      m_menuMainRomListPos_x = 60;
      m_menuMainRomListPos_y = 80;
   }
   else if(width == 1280)
   {
      strlcpy(g_extern.console.menu_texture_path, "D:\\Media\\main-menu_720p.png",
            sizeof(g_extern.console.menu_texture_path));
      m_menuMainRomListPos_x = 360;
      m_menuMainRomListPos_y = 130;
   }

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_LOW_RAM_MODE_ENABLE)) { }
   else
      texture_image_load(g_extern.console.menu_texture_path, &g_extern.console.menu_texture);

   // Load rom selector panel
   texture_image_load("D:\\Media\\menuMainRomSelectPanel.png", &g_extern.console.menu_panel);
   
   //Display some text
   //Center the text (hardcoded)
   xpos = width == 640 ? 65 : 400;
   ypos = width == 640 ? 430 : 670;
#endif

   return true;
}

static void gfx_ctx_xdk_menu_frame(void* data)
{
	(void)data;
}

static void gfx_ctx_xdk_menu_free(void)
{
#ifdef _XBOX1
   texture_image_free(&g_extern.console.menu_texture);
   texture_image_free(&g_extern.console.menu_panel);
#endif
}

static void gfx_ctx_xdk_swap_buffers(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = d3d->d3d_render_device;
#ifdef _XBOX1
   d3dr->EndScene();
#endif
   d3dr->Present(NULL, NULL, NULL, NULL);
}

static bool gfx_ctx_xdk_window_has_focus(void)
{
   return true;
}

static void gfx_ctx_xdk_menu_draw_bg(rarch_position_t *position)
{
#ifdef _XBOX1
   g_extern.console.menu_texture.x = 0;
   g_extern.console.menu_texture.y = 0;
   texture_image_render(&g_extern.console.menu_texture);
#endif
}

static void gfx_ctx_xdk_menu_draw_panel(rarch_position_t *position)
{
#ifdef _XBOX1
   g_extern.console.menu_panel.x = position->x;
   g_extern.console.menu_panel.y = position->y;
   g_extern.console.menu_panel.width = ROM_PANEL_WIDTH;
   g_extern.console.menu_panel.height = ROM_PANEL_HEIGHT;
   texture_image_render(&g_extern.console.menu_panel);
#endif
}

static void gfx_ctx_xdk_menu_set_default_pos(rmenu_default_positions_t *position)
{
#ifdef _XBOX1
   position->x_position = POSITION_X;
   position->x_position_center = POSITION_X_CENTER;
   position->y_position = POSITION_Y_BEGIN;
   position->comment_y_position = COMMENT_Y_POSITION;
   position->y_position_increment = POSITION_Y_INCREMENT;
   position->starting_y_position = POSITION_Y_START;
   position->comment_two_y_position = COMMENT_TWO_Y_POSITION;
   position->font_size = HARDCODE_FONT_SIZE;
   position->msg_queue_x_position = MSG_QUEUE_X_POSITION;
   position->msg_queue_y_position = MSG_QUEUE_Y_POSITION;
   position->msg_queue_font_size= MSG_QUEUE_FONT_SIZE;
   position->msg_prev_next_y_position = MSG_PREV_NEXT_Y_POSITION;
   position->current_path_font_size = CURRENT_PATH_FONT_SIZE;
   position->current_path_y_position = CURRENT_PATH_Y_POSITION;
   position->variable_font_size = FONT_SIZE;
   position->entries_per_page = NUM_ENTRY_PER_PAGE;
   position->core_msg_x_position = position->x_position;
   position->core_msg_y_position = position->msg_prev_next_y_position + 0.01f;
   position->core_msg_font_size = position->font_size;
#endif
}

static void gfx_ctx_xdk_menu_screenshot_enable(bool enable)
{
}

static void gfx_ctx_xdk_menu_screenshot_dump(void *data)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   HRESULT ret = S_OK;
   char filename[PATH_MAX];
   char shotname[PATH_MAX];

   fill_dated_filename(shotname, "bmp", sizeof(shotname));
   snprintf(filename, sizeof(filename), "%s\\%s", default_paths.screenshots_dir, shotname);
   
#if defined(_XBOX1)
   D3DSurface *surf = NULL;
   d3d->d3d_render_device->GetBackBuffer(-1, D3DBACKBUFFER_TYPE_MONO, &surf);
   ret = XGWriteSurfaceToFile(surf, filename);
   surf->Release();
#elif defined(_XBOX360)
   ret = 1; //false
   //ret = D3DXSaveTextureToFile(filename, D3DXIFF_BMP, d3d->lpTexture, NULL);
#endif

   if(ret == S_OK)
   {
      RARCH_LOG("Screenshot saved: %s.\n", filename);
      msg_queue_push(g_extern.msg_queue, "Screenshot saved.", 1, 30);
   }
}

static const char *gfx_ctx_xdk_menu_drive_mapping_previous(void)
{
   if(drive_mapping_idx > 0)
      drive_mapping_idx--;
   return drive_mappings[drive_mapping_idx];
}

static const char *gfx_ctx_xdk_menu_drive_mapping_next(void)
{
   if((drive_mapping_idx + 1) < DRIVE_MAPPING_SIZE)
      drive_mapping_idx++;
   return drive_mappings[drive_mapping_idx];
}

static void gfx_ctx_xdk_update_window_title(bool reset) { }

static void gfx_ctx_xdk_get_video_size(unsigned *width, unsigned *height)
{
   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;
#if defined(_XBOX360)
   XVIDEO_MODE video_mode;
   XGetVideoMode(&video_mode);

   *width  = video_mode.dwDisplayWidth;
   *height = video_mode.dwDisplayHeight;

   if(video_mode.fIsHiDef)
   {
      *width = 1280;
      *height = 720;
      g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_HD);
   }
   else
   {
	   *width = 640;
	   *height = 480;
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_HD);
   }

   if(video_mode.fIsWideScreen)
	   g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_WIDESCREEN);
   else
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_WIDESCREEN);
#elif defined(_XBOX1)
   DWORD video_mode = XGetVideoFlags();

    *width  = 640;
    *height = 480;

   // Only valid in PAL mode, not valid for HDTV modes!
   if(XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
   {
      // Check for 16:9 mode (PAL REGION)
      if(video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         if(video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
         {	//60 Hz, 720x480i
            *width = 720;
            *height = 480;
         }
         else
         {	//50 Hz, 720x576i
            *width = 720;
            *height = 576;
         }
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_WIDESCREEN);
      }
      else
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_WIDESCREEN);
   }
   else
   {
      // Check for 16:9 mode (NTSC REGIONS)
      if(video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         *width = 720;
         *height = 480;
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_WIDESCREEN);
      }
	  else
       g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_WIDESCREEN);
   }

   if(XGetAVPack() == XC_AV_PACK_HDTV)
   {
      if(video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      {
         *width	= 640;
         *height  = 480;
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_WIDESCREEN);
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_HD);
      }
	   else if(video_mode & XC_VIDEO_FLAGS_HDTV_720p)
      {
         *width	= 1280;
         *height  = 720;
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_WIDESCREEN);
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_HD);
      }
	   else if(video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
      {
         *width	= 1920;
         *height  = 1080;
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_WIDESCREEN);
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_HD);
      }
   }
#else
   /* TODO: implement */
   (void)width;
   (void)height;
#endif
}

static bool gfx_ctx_xdk_init(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   d3d->d3d_device = direct3d_create_ctx(D3D_SDK_VERSION);
   if (!d3d->d3d_device)
   {
      RARCH_ERR("Could not create Direct3D context.\n");
      free(d3d);
      return NULL;
   }

   return true;
}

static bool gfx_ctx_xdk_set_video_mode(
      unsigned width, unsigned height, bool fullscreen)
{
   /* TODO: implement */
   return true;
}

static void gfx_ctx_xdk_destroy(void)
{
   xdk_d3d_video_t * d3d = (xdk_d3d_video_t*)driver.video_data;

   if (d3d->d3d_render_device)
   {
      d3d->d3d_render_device->Release();
      d3d->d3d_render_device = 0;
   }

   if (d3d->d3d_device)
   {
      d3d->d3d_device->Release();
      d3d->d3d_device = 0;
   }
}

static void gfx_ctx_xdk_input_driver(const input_driver_t **input, void **input_data) { }



static bool gfx_ctx_xdk_bind_api(enum gfx_ctx_api api)
{
#if defined(_XBOX1)
   return api == GFX_CTX_DIRECT3D8_API;
#elif defined(_XBOX360)
   return api == GFX_CTX_DIRECT3D9_API;
#endif
}

/*============================================================
	MISC
        TODO: Refactor
============================================================ */

void gfx_ctx_set_overscan(void)
{
   /* TODO: implement */
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   if (!d3d)
      return;

   d3d->should_resize = true;
}

int gfx_ctx_xdk_check_resolution(unsigned resolution_id)
{
   /* TODO: implement */
   return 0;
}

static bool gfx_ctx_init_egl_image_buffer(const video_info_t *video)
{
   return false;
}

static bool gfx_ctx_write_egl_image(const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, unsigned index, void **image_handle)
{
   return false;
}

const gfx_ctx_driver_t gfx_ctx_xdk = {
   gfx_ctx_xdk_init,
   gfx_ctx_xdk_destroy,
   gfx_ctx_xdk_bind_api,
   gfx_ctx_xdk_set_swap_interval,
   gfx_ctx_xdk_set_video_mode,
   gfx_ctx_xdk_get_video_size,
   NULL,
   gfx_ctx_xdk_update_window_title,
   gfx_ctx_xdk_check_window,
   gfx_ctx_xdk_set_resize,
   gfx_ctx_xdk_window_has_focus,
   gfx_ctx_xdk_swap_buffers,
   gfx_ctx_xdk_input_driver,
   NULL,
   gfx_ctx_init_egl_image_buffer,
   gfx_ctx_write_egl_image,
   NULL,
   "xdk",

   // RARCH_CONSOLE stuff.
   gfx_ctx_xdk_set_blend,
   NULL,
   gfx_ctx_xdk_get_available_resolutions,
   gfx_ctx_xdk_check_resolution,
   NULL,

#if defined(HAVE_RMENU)
   gfx_ctx_xdk_menu_init,
   gfx_ctx_xdk_menu_frame,
   gfx_ctx_xdk_menu_free,
   gfx_ctx_xdk_menu_draw_bg,
   gfx_ctx_xdk_menu_draw_panel,
   gfx_ctx_xdk_menu_set_default_pos,
   gfx_ctx_xdk_menu_screenshot_enable,
   gfx_ctx_xdk_menu_screenshot_dump,
   gfx_ctx_xdk_menu_drive_mapping_previous,
   gfx_ctx_xdk_menu_drive_mapping_next
#endif
};
