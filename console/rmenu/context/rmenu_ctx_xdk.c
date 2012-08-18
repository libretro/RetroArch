/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include "../rmenu.h"
#include "../../gfx/context/xdk_ctx.h"

#define ROM_PANEL_WIDTH 510
#define ROM_PANEL_HEIGHT 20

int xpos, ypos;
texture_image m_menuMainRomSelectPanel;
texture_image m_menuMainBG;

// Rom list coords
int m_menuMainRomListPos_x;
int m_menuMainRomListPos_y;

static void rmenu_ctx_xdk_clear(void)
{
   gfx_ctx_clear();
}

static void rmenu_ctx_xdk_blend(bool enable)
{
   gfx_ctx_set_blend(enable);
}

static void rmenu_ctx_xdk_free_textures(void)
{
   texture_image_free(&m_menuMainBG);
   texture_image_free(&m_menuMainRomSelectPanel);
}

static void rmenu_ctx_xdk_init_textures(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   int width  = d3d->d3dpp.BackBufferWidth;

   // Load background image
   if(width == 640)
   {
      texture_image_load("D:\\Media\\main-menu_480p.png", &m_menuMainBG);
      m_menuMainRomListPos_x = 60;
      m_menuMainRomListPos_y = 80;
   }
   else if(width == 1280)
   {
      texture_image_load("D:\\Media\\main-menu_720p.png", &m_menuMainBG);
      m_menuMainRomListPos_x = 360;
      m_menuMainRomListPos_y = 130;
   }

   // Load rom selector panel
   texture_image_load("D:\\Media\\menuMainRomSelectPanel.png", &m_menuMainRomSelectPanel);
   
   //Display some text
   //Center the text (hardcoded)
   xpos = width == 640 ? 65 : 400;
   ypos = width == 640 ? 430 : 670;
}

static void rmenu_ctx_xdk_render_selection_panel(rmenu_position_t *position)
{
   m_menuMainRomSelectPanel.x = position->x;
   m_menuMainRomSelectPanel.y = position->y;
   m_menuMainRomSelectPanel.width = ROM_PANEL_WIDTH;
   m_menuMainRomSelectPanel.height = ROM_PANEL_HEIGHT;
   texture_image_render(&m_menuMainRomSelectPanel);
}

static void rmenu_ctx_xdk_render_bg(rmenu_position_t *position)
{
   m_menuMainBG.x = 0;
   m_menuMainBG.y = 0;
   texture_image_render(&m_menuMainBG);
}

static void rmenu_ctx_xdk_swap_buffers(void)
{
   gfx_ctx_swap_buffers();
}

const rmenu_context_t rmenu_ctx_xdk = {
   rmenu_ctx_xdk_clear,
   rmenu_ctx_xdk_blend,
   rmenu_ctx_xdk_free_textures,
   rmenu_ctx_xdk_init_textures,
   rmenu_ctx_xdk_render_selection_panel,
   rmenu_ctx_xdk_render_bg,
   rmenu_ctx_xdk_swap_buffers,
};
