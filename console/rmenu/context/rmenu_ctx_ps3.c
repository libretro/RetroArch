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

#if defined(__CELLOS_LV2__)
#include <sdk_version.h>
#include <sysutil/sysutil_screenshot.h>
#endif

#ifdef HAVE_SYSUTILS
#include <cell/sysmodule.h>
#endif

#include "../rmenu.h"
#include "../../../gfx/gfx_context.h"

#define HARDCODE_FONT_SIZE 0.91f
#define POSITION_X 0.09f
#define POSITION_X_CENTER 0.5f
#define POSITION_Y_START 0.17f
#define POSITION_Y_INCREMENT 0.035f
#define POSITION_Y_BEGIN (POSITION_Y_START + POSITION_Y_INCREMENT)
#define COMMENT_TWO_Y_POSITION 0.91f
#define COMMENT_Y_POSITION 0.82f

#define MSG_QUEUE_X_POSITION g_settings.video.msg_pos_x
#define MSG_QUEUE_Y_POSITION 0.76f
#define MSG_QUEUE_FONT_SIZE 1.03f

#define MSG_PREV_NEXT_Y_POSITION 0.03f
#define CURRENT_PATH_Y_POSITION 0.15f
#define CURRENT_PATH_FONT_SIZE FONT_SIZE

#define FONT_SIZE (g_extern.console.rmenu.font_size)

#define NUM_ENTRY_PER_PAGE 15

#define DRIVE_MAPPING_SIZE 4

const char drive_mappings[DRIVE_MAPPING_SIZE][32] = {
   "/app_home/",
   "/dev_hdd0/",
   "/dev_hdd1/",
   "/host_root/"
};

unsigned char drive_mapping_idx = 1;

static void rmenu_ctx_ps3_clear(void)
{
   gfx_ctx_clear();
}

static void rmenu_ctx_ps3_blend(bool enable)
{
   gfx_ctx_set_blend(enable);
}

static void rmenu_ctx_ps3_init_textures(void)
{
}

static void rmenu_ctx_ps3_free_textures(void)
{
   gl_t *gl = driver.video_data;
   gl->draw_rmenu = false;
}

static void rmenu_ctx_ps3_render_selection_panel(rmenu_position_t *position)
{
   (void)position;
}

static void rmenu_ctx_ps3_render_bg(rmenu_position_t *position)
{
   (void)position;
}

static void rmenu_ctx_ps3_swap_buffers(void)
{
   gfx_ctx_swap_buffers();
#ifdef HAVE_SYSUTILS
      cellSysutilCheckCallback();
#endif
}

void rmenu_ctx_ps3_set_swap_interval(unsigned interval)
{
   gfx_ctx_set_swap_interval(interval);
}

static void rmenu_ctx_ps3_set_default_pos(rmenu_default_positions_t *position)
{
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
   position->core_msg_x_position = 0.3f;
   position->core_msg_y_position = 0.06f;
   position->core_msg_font_size = COMMENT_Y_POSITION;
}

static void rmenu_ctx_ps3_render_msg(float xpos, float ypos, float scale, unsigned color, const char *msg, ...)
{
   gl_t *gl = driver.video_data;

   gl_render_msg_place(gl, xpos, ypos, scale, color, msg);
}

static void rmenu_ctx_ps3_render_menu_enable(bool enable)
{
   gl_t *gl = driver.video_data;
   gl->draw_rmenu = enable;
}

static void rmenu_ctx_ps3_screenshot_enable(bool enable)
{
#if(CELL_SDK_VERSION > 0x340000)
   if(enable)
   {
      cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
      CellScreenShotSetParam screenshot_param = {0, 0, 0, 0};

      screenshot_param.photo_title = "RetroArch PS3";
      screenshot_param.game_title = "RetroArch PS3";
      cellScreenShotSetParameter (&screenshot_param);
      cellScreenShotEnable();
   }
   else
   {
      cellScreenShotDisable();
      cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
   }
#endif
}

static void rmenu_ctx_ps3_screenshot_dump(void *data)
{
   (void)data;
}

static const char * rmenu_ctx_ps3_drive_mapping_previous(void)
{
   if(drive_mapping_idx > 0)
      drive_mapping_idx--;
   return drive_mappings[drive_mapping_idx];
}

static const char * rmenu_ctx_ps3_drive_mapping_next(void)
{
   if((drive_mapping_idx + 1) < DRIVE_MAPPING_SIZE)
      drive_mapping_idx++;
   return drive_mappings[drive_mapping_idx];
}

static void rmenu_ctx_ps3_set_filtering(unsigned index, bool set_smooth)
{
   gfx_ctx_set_filtering(index, set_smooth);
}

static void rmenu_ctx_ps3_set_aspect_ratio(unsigned aspectratio_index)
{
   driver.video->set_aspect_ratio(NULL, aspectratio_index);
}

static void rmenu_ctx_ps3_set_fbo_enable(bool enable)
{
   gfx_ctx_set_fbo(enable);
}

static void rmenu_ctx_ps3_apply_fbo_state_changes(unsigned i)
{
   gfx_ctx_apply_fbo_state_changes(i);
}

const rmenu_context_t rmenu_ctx_ps3 = {
   .clear = rmenu_ctx_ps3_clear,
   .set_filtering = rmenu_ctx_ps3_set_filtering,
   .set_aspect_ratio = rmenu_ctx_ps3_set_aspect_ratio,
   .blend = rmenu_ctx_ps3_blend, 
   .set_fbo_enable = rmenu_ctx_ps3_set_fbo_enable,
   .apply_fbo_state_changes = rmenu_ctx_ps3_apply_fbo_state_changes,
   .free_textures = rmenu_ctx_ps3_free_textures,
   .init_textures = rmenu_ctx_ps3_init_textures,
   .render_selection_panel = rmenu_ctx_ps3_render_selection_panel,
   .render_bg = rmenu_ctx_ps3_render_bg,
   .render_menu_enable = rmenu_ctx_ps3_render_menu_enable,
   .render_msg = rmenu_ctx_ps3_render_msg,
    .screenshot_enable = rmenu_ctx_ps3_screenshot_enable,
    .screenshot_dump = rmenu_ctx_ps3_screenshot_dump,
   .swap_buffers = rmenu_ctx_ps3_swap_buffers,
   .set_swap_interval = rmenu_ctx_ps3_set_swap_interval,
   .set_default_pos = rmenu_ctx_ps3_set_default_pos,
   .drive_mapping_prev = rmenu_ctx_ps3_drive_mapping_previous,
   .drive_mapping_next = rmenu_ctx_ps3_drive_mapping_next,
};
