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

#ifdef HAVE_SYSUTILS
#include <cell/sysmodule.h>
#endif

#include "../rmenu.h"
#include "../../../gfx/context/ps3_ctx.h"

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
   gl->menu_render = false;
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
}

static void rmenu_ctx_ps3_render_msg(float xpos, float ypos, float scale, float color, const char *msg, ...)
{
   gl_t *gl = driver.video_data;
   gl_render_msg_place(gl, xpos, ypos, scale, color, msg)
}

static void rmenu_ctx_ps3_render_menu_enable(bool enable)
{
   gl_t *gl = driver.video_data;
   gl->menu_render = enable;
}

const rmenu_context_t rmenu_ctx_ps3 = {
   .clear = rmenu_ctx_ps3_clear,
   .blend = rmenu_ctx_ps3_blend, 
   .free_textures = rmenu_ctx_ps3_free_textures,
   .init_textures = rmenu_ctx_ps3_init_textures,
   .render_selection_panel = rmenu_ctx_ps3_render_selection_panel,
   .render_bg = rmenu_ctx_ps3_render_bg,
   .render_menu_enable = rmenu_ctx_ps3_render_menu_enable,
   .render_msg = rmenu_ctx_ps3_render_msg,
   .swap_buffers = rmenu_ctx_ps3_swap_buffers,
   .set_default_pos = rmenu_ctx_ps3_set_default_pos,
};
