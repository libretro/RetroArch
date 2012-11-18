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
#include "../../screenshot.h"

#ifdef _XBOX
#include "../../xdk/xdk_d3d.h"
#endif

#include "../../gfx/gfx_context.h"

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
texture_image m_menuMainRomSelectPanel;
texture_image m_menuMainBG;

// Rom list coords
int m_menuMainRomListPos_x;
int m_menuMainRomListPos_y;
#endif

static void rmenu_ctx_xdk_clear(void)
{
   gfx_ctx_xdk_clear();
}

static void rmenu_ctx_xdk_blend(bool enable)
{
   gfx_ctx_xdk_set_blend(enable);
}

static void rmenu_ctx_xdk_free_textures(void)
{
#ifdef _XBOX1
   texture_image_free(&m_menuMainBG);
   texture_image_free(&m_menuMainRomSelectPanel);
#endif
}

static void rmenu_ctx_xdk_init_textures(void)
{
#ifdef _XBOX1
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
#endif
}

static void rmenu_ctx_xdk_render_selection_panel(rmenu_position_t *position)
{
#ifdef _XBOX1
   m_menuMainRomSelectPanel.x = position->x;
   m_menuMainRomSelectPanel.y = position->y;
   m_menuMainRomSelectPanel.width = ROM_PANEL_WIDTH;
   m_menuMainRomSelectPanel.height = ROM_PANEL_HEIGHT;
   texture_image_render(&m_menuMainRomSelectPanel);
#endif
}

static void rmenu_ctx_xdk_render_bg(rmenu_position_t *position)
{
#ifdef _XBOX1
   m_menuMainBG.x = 0;
   m_menuMainBG.y = 0;
   texture_image_render(&m_menuMainBG);
#endif
}

static void rmenu_ctx_xdk_swap_buffers(void)
{
   gfx_ctx_xdk_swap_buffers();
}

void rmenu_ctx_xdk_set_swap_interval(unsigned interval)
{
   gfx_ctx_xdk_set_swap_interval(interval);
}

static void rmenu_ctx_xdk_set_default_pos(rmenu_default_positions_t *position)
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

static void rmenu_ctx_xdk_render_msg(float xpos, float ypos, float scale, unsigned color, const char *msg, ...)
{
#ifdef _XBOX1
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   xfonts_render_msg_place(d3d, xpos, ypos, scale, msg);
#endif
}

static void rmenu_ctx_xdk_render_menu_enable(bool enable)
{
}

static void rmenu_ctx_xdk_screenshot_enable(bool enable)
{
}

static void rmenu_ctx_xdk_screenshot_dump(void *data)
{
   gfx_ctx_xdk_screenshot_dump(NULL);
}

static const char * rmenu_ctx_xdk_drive_mapping_previous(void)
{
   if(drive_mapping_idx > 0)
      drive_mapping_idx--;
   return drive_mappings[drive_mapping_idx];
}

static const char * rmenu_ctx_xdk_drive_mapping_next(void)
{
   if((drive_mapping_idx + 1) < DRIVE_MAPPING_SIZE)
      drive_mapping_idx++;
   return drive_mappings[drive_mapping_idx];
}

static void rmenu_ctx_xdk_set_filtering(unsigned index, bool set_smooth)
{
   gfx_ctx_xdk_set_filtering(index, set_smooth);
}

static void rmenu_ctx_xdk_set_aspect_ratio(unsigned aspectratio_index)
{
   driver.video->set_aspect_ratio(NULL, aspectratio_index);
}

static void rmenu_ctx_xdk_set_fbo_enable(unsigned i)
{
   gfx_ctx_xdk_set_fbo(i);
}


const rmenu_context_t rmenu_ctx_xdk = {
   rmenu_ctx_xdk_clear,
   rmenu_ctx_xdk_set_filtering,
   rmenu_ctx_xdk_set_aspect_ratio,
   rmenu_ctx_xdk_blend,
   rmenu_ctx_xdk_set_fbo_enable,
   rmenu_ctx_xdk_free_textures,
   rmenu_ctx_xdk_init_textures,
   rmenu_ctx_xdk_render_selection_panel,
   rmenu_ctx_xdk_render_bg,
   rmenu_ctx_xdk_render_menu_enable,
   rmenu_ctx_xdk_render_msg,
   rmenu_ctx_xdk_screenshot_enable,
   rmenu_ctx_xdk_screenshot_dump,
   rmenu_ctx_xdk_swap_buffers,
   rmenu_ctx_xdk_set_swap_interval,
   rmenu_ctx_xdk_set_default_pos,
   rmenu_ctx_xdk_drive_mapping_previous,
   rmenu_ctx_xdk_drive_mapping_next,
};
