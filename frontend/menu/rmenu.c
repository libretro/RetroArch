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

#include "rmenu.h"
#include "utils/file_browser.h"

#if defined(__CELLOS_LV2__)
#include <sdk_version.h>

#if (CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif

#endif

#include "../../console/rarch_console.h"

#include "../../gfx/image.h"

#include "../../gfx/gfx_common.h"
#include "../../gfx/gfx_context.h"

#include "../../file.h"
#include "../../driver.h"
#include "../../general.h"

#define EXT_IMAGES "png|PNG|jpg|JPG|JPEG|jpeg"
#define EXT_SHADERS "cg|CG"
#define EXT_CGP_PRESETS "cgp|CGP"
#define EXT_INPUT_PRESETS "cfg|CFG"

#if defined(_XBOX1)
#define ROM_PANEL_WIDTH 510
#define ROM_PANEL_HEIGHT 20
// Rom list coordinates
int xpos, ypos;
unsigned m_menuMainRomListPos_x = 60;
unsigned m_menuMainRomListPos_y = 80;
#endif

static bool set_libretro_core_as_launch;

filebrowser_t *browser;
filebrowser_t *tmpBrowser;
unsigned currently_selected_controller_menu = 0;

static const struct retro_keybind _rmenu_nav_binds[] = {
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) | (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_UP), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) | (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_DOWN), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) | (1ULL << RARCH_ANALOG_LEFT_X_DPAD_LEFT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) | (1ULL << RARCH_ANALOG_LEFT_X_DPAD_RIGHT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_UP), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_DOWN), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_LEFT_X_DPAD_LEFT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_LEFT_X_DPAD_RIGHT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_UP), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_DOWN), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_LEFT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_RIGHT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_B), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_A), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_X), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_Y), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_START), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_L), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_R), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_L2), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_R2), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_L3), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_R3), 0 },
};

static const struct retro_keybind *rmenu_nav_binds[] = {
   _rmenu_nav_binds
};

enum
{
   RMENU_DEVICE_NAV_UP = 0,
   RMENU_DEVICE_NAV_DOWN,
   RMENU_DEVICE_NAV_LEFT,
   RMENU_DEVICE_NAV_RIGHT,
   RMENU_DEVICE_NAV_UP_ANALOG_L,
   RMENU_DEVICE_NAV_DOWN_ANALOG_L,
   RMENU_DEVICE_NAV_LEFT_ANALOG_L,
   RMENU_DEVICE_NAV_RIGHT_ANALOG_L,
   RMENU_DEVICE_NAV_UP_ANALOG_R,
   RMENU_DEVICE_NAV_DOWN_ANALOG_R,
   RMENU_DEVICE_NAV_LEFT_ANALOG_R,
   RMENU_DEVICE_NAV_RIGHT_ANALOG_R,
   RMENU_DEVICE_NAV_B,
   RMENU_DEVICE_NAV_A,
   RMENU_DEVICE_NAV_X,
   RMENU_DEVICE_NAV_Y,
   RMENU_DEVICE_NAV_START,
   RMENU_DEVICE_NAV_SELECT,
   RMENU_DEVICE_NAV_L1,
   RMENU_DEVICE_NAV_R1,
   RMENU_DEVICE_NAV_L2,
   RMENU_DEVICE_NAV_R2,
   RMENU_DEVICE_NAV_L3,
   RMENU_DEVICE_NAV_R3,
   RMENU_DEVICE_NAV_LAST
};

const char drive_mappings[][32] = {
#if defined(_XBOX1)
   "C:",
   "D:",
   "E:",
   "F:",
   "G:"
#elif defined(__CELLOS_LV2__)
   "/app_home/",
   "/dev_hdd0/",
   "/dev_hdd1/",
   "/host_root/"
#endif
};

#if defined__CELLOS_LV2__
unsigned char drive_mapping_idx = 1;
#elif defined(_XBOX1)
unsigned char drive_mapping_idx = 2;
#else
unsigned char drive_mapping_idx = 0;
#endif

static const char *menu_drive_mapping_previous(void)
{
   if (drive_mapping_idx > 0)
      drive_mapping_idx--;
   return drive_mappings[drive_mapping_idx];
}

static const char *menu_drive_mapping_next(void)
{
   size_t arr_size = sizeof(drive_mappings) / sizeof(drive_mappings[0]);

   if ((drive_mapping_idx + 1) < arr_size)
      drive_mapping_idx++;

   return drive_mappings[drive_mapping_idx];
}

#if defined(_XBOX1)
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
#elif defined(__CELLOS_LV2__)
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

#define NUM_ENTRY_PER_PAGE 15
#endif

static void menu_set_default_pos(rmenu_default_positions_t *position)
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
   position->current_path_y_position = CURRENT_PATH_Y_POSITION;
   position->entries_per_page = NUM_ENTRY_PER_PAGE;
#if defined(_XBOX1)
   position->current_path_font_size = CURRENT_PATH_FONT_SIZE;
   position->variable_font_size = FONT_SIZE;
   position->core_msg_x_position = position->x_position;
   position->core_msg_y_position = position->msg_prev_next_y_position + 0.01f;
   position->core_msg_font_size = position->font_size;
#elif defined(__CELLOS_LV2__)
   position->current_path_font_size = g_settings.video.font_size;
   position->variable_font_size = g_settings.video.font_size;
   position->core_msg_x_position = 0.3f;
   position->core_msg_y_position = 0.06f;
   position->core_msg_font_size = COMMENT_Y_POSITION;
#endif
}

/*============================================================
  RMENU GRAPHICS
  ============================================================ */

#ifdef HAVE_OPENGL
GLuint menu_texture_id;
#endif

static void texture_image_border_load(const char *path)
{
#ifdef HAVE_OPENGL
   gl_t *gl = driver.video_data;

   if (!gl)
      return;

   glGenTextures(1, &menu_texture_id);

   RARCH_LOG("Loading texture image for menu...\n");
   if (!texture_image_load(path, &g_extern.console.menu_texture))
   {
      RARCH_ERR("Failed to load texture image for menu.\n");
      return;
   }

   glBindTexture(GL_TEXTURE_2D, menu_texture_id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->border_type);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->border_type);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   glTexImage2D(GL_TEXTURE_2D, 0, RARCH_GL_INTERNAL_FORMAT32,
         g_extern.console.menu_texture.width, g_extern.console.menu_texture.height, 0,
         RARCH_GL_TEXTURE_TYPE32, RARCH_GL_FORMAT32, g_extern.console.menu_texture.pixels);

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   free(g_extern.console.menu_texture.pixels);
#endif
}

static void rmenu_gfx_init(void)
{
   if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_LOW_RAM_MODE_ENABLE))
      return;

#ifdef _XBOX1
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   strlcpy(g_extern.console.menu_texture_path,"D:\\Media\\main-menu_480p.png",
         sizeof(g_extern.console.menu_texture_path));

   texture_image_load(g_extern.console.menu_texture_path, &g_extern.console.menu_texture);
   texture_image_load("D:\\Media\\menuMainRomSelectPanel.png", &g_extern.console.menu_panel);

   //Display some text
   //Center the text (hardcoded)
   xpos = d3d->win_width == 640 ? 65 : 400;
   ypos = d3d->win_width == 640 ? 430 : 670;
#else
   texture_image_border_load(g_extern.console.menu_texture_path);
#endif
}

static void rmenu_gfx_draw_panel(rarch_position_t *position)
{
#ifdef _XBOX1
   g_extern.console.menu_panel.x = position->x;
   g_extern.console.menu_panel.y = position->y;
   g_extern.console.menu_panel.width = ROM_PANEL_WIDTH;
   g_extern.console.menu_panel.height = ROM_PANEL_HEIGHT;
   texture_image_render(&g_extern.console.menu_panel);
#endif
}

static void rmenu_gfx_draw_bg(rarch_position_t *position)
{
#ifdef _XBOX1
   g_extern.console.menu_texture.x = 0;
   g_extern.console.menu_texture.y = 0;
   texture_image_render(&g_extern.console.menu_texture);
#endif
}

static void rmenu_gfx_frame(void *data)
{
   (void)data;
#if defined(HAVE_OPENGL)
   gl_t *gl = (gl_t*)data;

   gl_shader_use(gl, RARCH_CG_MENU_SHADER_INDEX);
   gl_set_viewport(gl, gl->win_width, gl->win_height, true, false);

   if (gl->shader)
   {
      gl->shader->set_params(gl->win_width, gl->win_height, 
            gl->win_width, gl->win_height, 
            gl->win_width, gl->win_height, 
            g_extern.frame_count, NULL, NULL, NULL, 0);
   }

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, menu_texture_id);

   gl->coords.vertex = vertexes_flipped;

   gl_shader_set_coords(gl, &gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); 

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
#endif
}

static void rmenu_gfx_free(void)
{
#ifdef _XBOX1
   texture_image_free(&g_extern.console.menu_texture);
   texture_image_free(&g_extern.console.menu_panel);
#endif
}

/*============================================================
  MENU STACK
  ============================================================ */

typedef struct
{
#ifdef HAVE_OSKUTIL
   unsigned osk_param;
   bool (*osk_init)(void *data);
   bool (*osk_callback)(void *data);
#endif
} rmenu_state_t;

static rmenu_state_t rmenu_state;

static unsigned char menu_stack_enum_array[10];
static unsigned stack_idx = 0;

static void menu_stack_pop(void)
{
   if (stack_idx > 1)
      stack_idx--;
}

static void menu_stack_push(unsigned menu_id)
{
   menu_stack_enum_array[stack_idx] = menu_id;
   stack_idx++;
}

/*============================================================
  EVENT CALLBACKS (AND RELATED)
  ============================================================ */

static void populate_setting_item(void *data, unsigned input)
{
   item *current_item = (item*)data;
   char fname[PATH_MAX];
   (void)fname;

   unsigned currentsetting = input;
   current_item->enum_id = input;

   switch(currentsetting)
   {
#ifdef __CELLOS_LV2__
      case SETTING_CHANGE_RESOLUTION:
         {
            unsigned width = gfx_ctx_get_resolution_width(g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx]);
            unsigned height = gfx_ctx_get_resolution_height(g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx]);
            strlcpy(current_item->text, "Resolution", sizeof(current_item->text));
            strlcpy(current_item->comment, "INFO - Change the display resolution.", sizeof(current_item->comment));
            snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%dx%d", width, height);
         }
         break;
      case SETTING_PAL60_MODE:
         strlcpy(current_item->text, "PAL60 Mode", sizeof(current_item->text));
         if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
         {
            strlcpy(current_item->setting_text, "ON", sizeof(current_item->setting_text));
            strlcpy(current_item->comment, "INFO - [PAL60 Mode is set to 'ON'.\nconverts frames from 60Hz to 50Hz.", sizeof(current_item->comment));
         }
         else
         {
            strlcpy(current_item->setting_text, "OFF", sizeof(current_item->setting_text));
            strlcpy(current_item->comment, "INFO - [PAL60 Mode is set to 'OFF'.\nframes are not converted.", sizeof(current_item->comment));
         }
         break;
#endif
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
      case SETTING_SHADER_PRESETS:
         strlcpy(current_item->text, "Shader Presets (CGP)", sizeof(current_item->text));
         fill_pathname_base(fname, g_extern.file_state.cgp_path, sizeof(fname));
         strlcpy(current_item->comment, "INFO - Select a [CG Preset] script.", sizeof(current_item->comment));
         strlcpy(current_item->setting_text, fname, sizeof(current_item->setting_text));
         break;
      case SETTING_SHADER:
         fill_pathname_base(fname, g_settings.video.cg_shader_path, sizeof(fname));
         strlcpy(current_item->text, "Shader #1", sizeof(current_item->text));
         strlcpy(current_item->setting_text, fname, sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Select a shader as [Shader #1].", sizeof(current_item->comment));
         break;
#endif
      case SETTING_EMU_SKIN:
         fill_pathname_base(fname, g_extern.console.menu_texture_path, sizeof(fname));
         strlcpy(current_item->text, "Menu Skin", sizeof(current_item->text));
         strlcpy(current_item->setting_text, fname, sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Select a skin for the menu.", sizeof(current_item->comment));
         break;
      case SETTING_EMU_LOW_RAM_MODE_ENABLE:
         strlcpy(current_item->text, "Low RAM Mode", sizeof(current_item->text));
         if (g_extern.lifecycle_mode_state & (1ULL <<MODE_MENU_LOW_RAM_MODE_ENABLE) ||
               g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_LOW_RAM_MODE_ENABLE_PENDING))
         {
            strlcpy(current_item->setting_text, "ON", sizeof(current_item->setting_text));
            strlcpy(current_item->comment, "INFO - Will load skin at startup.", sizeof(current_item->comment));
         }
         else
         {
            strlcpy(current_item->setting_text, "OFF", sizeof(current_item->setting_text));
            strlcpy(current_item->comment, "INFO - Will not load skin at startup to save up on RAM.", sizeof(current_item->comment));
         }
         break;
      case SETTING_FONT_SIZE:
         strlcpy(current_item->text, "Font Size", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%f", g_settings.video.font_size);
         strlcpy(current_item->comment, "INFO - Increase or decrease the [Font Size].", sizeof(current_item->comment));
         break;
      case SETTING_KEEP_ASPECT_RATIO:
         strlcpy(current_item->text, "Aspect Ratio", sizeof(current_item->text));
         strlcpy(current_item->setting_text, aspectratio_lut[g_settings.video.aspect_ratio_idx].name, sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Select an [Aspect Ratio].", sizeof(current_item->comment));
         break;
      case SETTING_HW_TEXTURE_FILTER:
         strlcpy(current_item->text, "Hardware filtering #1", sizeof(current_item->text));
         if (g_settings.video.smooth)
         {
            strlcpy(current_item->setting_text, "Bilinear", sizeof(current_item->setting_text));
            strlcpy(current_item->comment, "INFO - Hardware filtering #1 is set to Bilinear.",
                  sizeof(current_item->comment));
         }
         else
         {
            strlcpy(current_item->setting_text, "Point", sizeof(current_item->setting_text));
            strlcpy(current_item->comment, "INFO - Hardware filtering #1 is set to Point.",
                  sizeof(current_item->comment));
         }
         break;
#ifdef _XBOX1
      case SETTING_FLICKER_FILTER:
         strlcpy(current_item->text, "Flicker Filter", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%d", g_extern.console.screen.flicker_filter_index);
         strlcpy(current_item->comment, "INFO - Toggle the [Flicker Filter].", sizeof(current_item->comment));
         break;
      case SETTING_SOFT_DISPLAY_FILTER:
         strlcpy(current_item->text, "Soft Display Filter", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
         strlcpy(current_item->comment, "INFO - Toggle the [Soft Display Filter].", sizeof(current_item->comment));
         break;
#endif
      case SETTING_HW_OVERSCAN_AMOUNT:
         strlcpy(current_item->text, "Overscan", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%f", g_extern.console.screen.overscan_amount);
         strlcpy(current_item->comment, "INFO - Adjust or decrease [Overscan]. Set this to higher than 0.000\nif the screen doesn't fit on your TV/monitor.", sizeof(current_item->comment));
         break;
      case SETTING_REFRESH_RATE:
         strlcpy(current_item->text, "Refresh rate", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%fHz", g_settings.video.refresh_rate);
         strlcpy(current_item->comment, "INFO - Adjust or decrease [Refresh Rate].", sizeof(current_item->comment));
         break;
      case SETTING_THROTTLE_MODE:
         strlcpy(current_item->text, "Throttle Mode", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_THROTTLE_ENABLE)) ? "ON" : "OFF");
         snprintf(current_item->comment, sizeof(current_item->comment), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_THROTTLE_ENABLE)) ? "INFO - [Throttle Mode] is 'ON' - Vsync is enabled." : "INFO - [Throttle Mode] is 'OFF' - Vsync is disabled.");
         break;
      case SETTING_TRIPLE_BUFFERING:
         strlcpy(current_item->text, "Triple Buffering", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE)) ? "ON" : "OFF");
         snprintf(current_item->comment, sizeof(current_item->comment), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE)) ? "INFO - [Triple Buffering] is set to 'ON'." : "INFO - [Triple Buffering] is set to 'OFF'.");
         break;
      case SETTING_ENABLE_SCREENSHOTS:
         strlcpy(current_item->text, "Screenshot Option", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SCREENSHOTS_ENABLE)) ? "ON" : "OFF");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Screenshots feature is set to '%s'.", (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SCREENSHOTS_ENABLE)) ? "ON" : "OFF");
         break;
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
      case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
         strlcpy(current_item->text, "APPLY SHADER PRESET ON STARTUP", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Automatically load the currently selected [CG Preset] file on startup.", sizeof(current_item->comment));
         break;
#endif
      case SETTING_DEFAULT_VIDEO_ALL:
         strlcpy(current_item->text, "DEFAULTS", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Set all [General Video Settings] back to their 'DEFAULT' values.", sizeof(current_item->comment));
         break;
      case SETTING_SOUND_MODE:
         strlcpy(current_item->text, "Sound Output", sizeof(current_item->text));
         switch(g_extern.console.sound.mode)
         {
            case SOUND_MODE_NORMAL:
               strlcpy(current_item->comment, "INFO - [Sound Output] is set to 'Normal'.", sizeof(current_item->comment));
               strlcpy(current_item->setting_text, "Normal", sizeof(current_item->setting_text));
               break;
#ifdef HAVE_RSOUND
            case SOUND_MODE_RSOUND:
               strlcpy(current_item->comment, "INFO - [Sound Output] is set to 'RSound'.", sizeof(current_item->comment));
               strlcpy(current_item->setting_text, "RSound", sizeof(current_item->setting_text));
               break;
#endif
#ifdef HAVE_HEADSET
            case SOUND_MODE_HEADSET:
               strlcpy(current_item->comment, "INFO - [Sound Output] is set to USB/Bluetooth Headset.", sizeof(current_item->comment));
               strlcpy(current_item->setting_text, "USB/Bluetooth Headset", sizeof(current_item->setting_text));
               break;
#endif
            default:
               break;
         }
         break;
#ifdef HAVE_RSOUND
      case SETTING_RSOUND_SERVER_IP_ADDRESS:
         strlcpy(current_item->text, "RSound Server IP Address", sizeof(current_item->text));
         strlcpy(current_item->setting_text, g_settings.audio.device, sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Enter the IP Address of the [RSound Audio Server]. IP address\nmust be an IPv4 32-bits address, eg: '192.168.1.7'.", sizeof(current_item->comment));
         break;
#endif
      case SETTING_DEFAULT_AUDIO_ALL:
         strlcpy(current_item->text, "DEFAULTS", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Set all [General Audio Settings] back to their 'DEFAULT' values.", sizeof(current_item->comment));
         break;
      case SETTING_RESAMPLER_TYPE:
         strlcpy(current_item->text, "Sound resampler", sizeof(current_item->text));
#ifdef HAVE_SINC
         if (strstr(g_settings.audio.resampler, "sinc"))
         {
            strlcpy(current_item->setting_text, "Sinc", sizeof(current_item->setting_text));
            strlcpy(current_item->comment, "INFO - [Sinc resampler] - slightly slower but better sound quality at high frequencies.", sizeof(current_item->comment));
         }
         else
#endif
         {
            strlcpy(current_item->setting_text, "Hermite", sizeof(current_item->setting_text));
            strlcpy(current_item->comment, "INFO - [Hermite resampler] - faster but less accurate at high frequencies.", sizeof(current_item->comment));
         }
         break;
      case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
         strlcpy(current_item->text, "Current save state slot", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%d", g_extern.state_slot);
         strlcpy(current_item->comment, "INFO - Set the currently selected savestate slot.", sizeof(current_item->comment));
         break;
         /* emu-specific */
      case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
         strlcpy(current_item->text, "Debug info messages", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW)) ? "ON" : "OFF");
         strlcpy(current_item->comment, "INFO - Show onscreen debug messages.", sizeof(current_item->comment));
         break;
      case SETTING_EMU_SHOW_INFO_MSG:
         strlcpy(current_item->text, "Info messages", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW)) ? "ON" : "OFF");
         strlcpy(current_item->comment, "INFO - Show onscreen info messages in the menu.", sizeof(current_item->comment));
         break;
      case SETTING_EMU_REWIND_ENABLED:
         strlcpy(current_item->text, "Rewind option", sizeof(current_item->text));
         if (g_settings.rewind_enable)
         {
            strlcpy(current_item->setting_text, "ON", sizeof(current_item->setting_text));
            strlcpy(current_item->comment, "INFO - [Rewind] feature is set to 'ON'.",
                  sizeof(current_item->comment));
         }
         else
         {
            strlcpy(current_item->setting_text, "OFF", sizeof(current_item->setting_text));
            strlcpy(current_item->comment, "INFO - [Rewind] feature is set to 'OFF'.",
                  sizeof(current_item->comment));
         }
         break;
      case SETTING_EMU_REWIND_GRANULARITY:
         strlcpy(current_item->text, "Rewind granularity", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%d", g_settings.rewind_granularity);
         strlcpy(current_item->comment, "INFO - Set the amount of frames to 'rewind'.\nIncrease this to lower CPU usage.", sizeof(current_item->comment));
         break;
      case SETTING_RARCH_DEFAULT_EMU:
         strlcpy(current_item->text, "Default libretro core", sizeof(current_item->text));
         fill_pathname_base(fname, g_settings.libretro, sizeof(fname));
         strlcpy(current_item->setting_text, fname, sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Select a default libretro core to launch at start-up.", sizeof(current_item->comment));
         break;
      case SETTING_QUIT_RARCH:
         strlcpy(current_item->text, "Quit RetroArch and save settings ", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Quits RetroArch and saves the settings.", sizeof(current_item->comment));
         break;
      case SETTING_EMU_AUDIO_MUTE:
         strlcpy(current_item->text, "Mute Audio", sizeof(current_item->text));
         if (g_extern.audio_data.mute)
         {
            strlcpy(current_item->comment, "INFO - [Audio Mute] is set to 'ON'. The game audio will be muted.", sizeof(current_item->comment));
            strlcpy(current_item->setting_text, "ON", sizeof(current_item->setting_text));
         }
         else
         {
            strlcpy(current_item->comment, "INFO - [Audio Mute] is set to 'OFF'.", sizeof(current_item->comment));
            strlcpy(current_item->setting_text, "OFF", sizeof(current_item->setting_text));
         }
         break;
#ifdef _XBOX1
      case SETTING_EMU_AUDIO_SOUND_VOLUME_LEVEL:
         strlcpy(current_item->text, "Volume Level", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), g_extern.console.sound.volume_level ? "Loud" : "Normal");
         if (g_extern.audio_data.mute)
            strlcpy(current_item->comment, "INFO - Volume level is set to Loud.", sizeof(current_item->comment));
         else
            strlcpy(current_item->comment, "INFO - Volume level is set to Normal.", sizeof(current_item->comment));
         break;
#endif
      case SETTING_ENABLE_CUSTOM_BGM:
         strlcpy(current_item->text, "Custom BGM Option", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE)) ? "ON" : "OFF");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Custom BGM] is set to '%s'.", (g_extern.lifecycle_mode_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE)) ? "ON" : "OFF");
         break;
      case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
         strlcpy(current_item->text, "Startup ROM Directory", sizeof(current_item->text));
         strlcpy(current_item->setting_text, g_extern.console.main_wrap.default_rom_startup_dir, sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Set the default Startup ROM directory path.", sizeof(current_item->comment));
         break;
      case SETTING_PATH_SAVESTATES_DIRECTORY:
         strlcpy(current_item->text, "Savestate Directory", sizeof(current_item->text));
         strlcpy(current_item->setting_text, g_extern.console.main_wrap.default_savestate_dir, sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Set the default path where all the savestate files will be saved to.", sizeof(current_item->comment));
         break;
      case SETTING_PATH_SRAM_DIRECTORY:
         strlcpy(current_item->text, "SRAM Directory", sizeof(current_item->text));
         strlcpy(current_item->setting_text, g_extern.console.main_wrap.default_sram_dir, sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Set the default SaveRAM directory path.", sizeof(current_item->comment));
         break;
#ifdef HAVE_XML
      case SETTING_PATH_CHEATS:
         strlcpy(current_item->text, "Cheatfile Directory", sizeof(current_item->text));
         strlcpy(current_item->setting_text, g_settings.cheat_database, sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Set the default Cheatfile directory path.", sizeof(current_item->comment));
         break;
#endif
      case SETTING_PATH_SYSTEM:
         strlcpy(current_item->text, "System Directory", sizeof(current_item->text));
         strlcpy(current_item->setting_text, g_settings.system_directory, sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Set the default [System directory] path. System files like\nBIOS files, etc. will be stored here.", sizeof(current_item->comment));
         break;
      case SETTING_ENABLE_SRAM_PATH:
         snprintf(current_item->text, sizeof(current_item->text), "Custom SRAM Dir Enable");
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE)) ? "ON" : "OFF");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Custom SRAM Dir Path] is set to '%s'.", (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE)) ? "ON" : "OFF");
         break;
      case SETTING_ENABLE_STATE_PATH:
         snprintf(current_item->text, sizeof(current_item->text), "Custom Savestate Dir Enable");
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE)) ? "ON" : "OFF");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Custom Savestate Dir Path] is set to '%s'.", (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE)) ? "ON" : "OFF");
         break;
      case SETTING_CONTROLS_SCHEME:
         strlcpy(current_item->text, "Control Scheme Preset", sizeof(current_item->text));
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Input scheme preset [%s] is selected.", g_extern.file_state.input_cfg_path);
         strlcpy(current_item->setting_text, g_extern.file_state.input_cfg_path, sizeof(current_item->setting_text));
         break;
      case SETTING_CONTROLS_NUMBER:
         strlcpy(current_item->text, "Controller No", sizeof(current_item->text));
         snprintf(current_item->comment, sizeof(current_item->comment), "Controller %d is currently selected.", currently_selected_controller_menu+1);
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%d", currently_selected_controller_menu+1);
         break;
      case SETTING_DPAD_EMULATION:
         strlcpy(current_item->text, "D-Pad Emulation", sizeof(current_item->text));
         switch(g_settings.input.dpad_emulation[currently_selected_controller_menu])
         {
            case ANALOG_DPAD_NONE:
               snprintf(current_item->comment, sizeof(current_item->comment), "[%s] from Controller %d is mapped to D-pad.", "None", currently_selected_controller_menu+1);
               strlcpy(current_item->setting_text, "None", sizeof(current_item->setting_text));
               break;
            case ANALOG_DPAD_LSTICK:
               snprintf(current_item->comment, sizeof(current_item->comment), "[%s] from Controller %d is mapped to D-pad.", "Left Stick", currently_selected_controller_menu+1);
               strlcpy(current_item->setting_text, "Left Stick", sizeof(current_item->setting_text));
               break;
            case ANALOG_DPAD_RSTICK:
               snprintf(current_item->comment, sizeof(current_item->comment), "[%s] from Controller %d is mapped to D-pad.", "Right Stick", currently_selected_controller_menu+1);
               strlcpy(current_item->setting_text, "Right Stick", sizeof(current_item->setting_text));
               break;
         }
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_Y:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_SELECT:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_START:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_UP:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_DOWN:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_LEFT:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_RIGHT:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_A:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_X:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L2:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R2:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L3:
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R3:
         {
            unsigned id = currentsetting - FIRST_CONTROL_BIND;
            struct platform_bind key_label;
            strlcpy(key_label.desc, "Unknown", sizeof(key_label.desc));
            key_label.joykey = g_settings.input.binds[currently_selected_controller_menu][id].joykey;

            if (driver.input->set_keybinds)
               driver.input->set_keybinds(&key_label, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
            strlcpy(current_item->text, g_settings.input.binds[currently_selected_controller_menu][id].desc, sizeof(current_item->text));
            snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [%s] is mapped to action:\n[%s].", current_item->text, key_label.desc);
            strlcpy(current_item->setting_text, key_label.desc, sizeof(current_item->setting_text));
         }
         break;
      case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
         strlcpy(current_item->text, "SAVE CUSTOM CONTROLS", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Save the [Custom Controls] settings to file.",  sizeof(current_item->comment));
         break;
      case SETTING_CONTROLS_DEFAULT_ALL:
         strlcpy(current_item->text, "DEFAULTS", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Set all Controls settings to defaults.", sizeof(current_item->comment));
         break;
      case SETTING_EMU_VIDEO_DEFAULT_ALL:
         strlcpy(current_item->text, "DEFAULTS", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Set all Video settings to defaults.", sizeof(current_item->comment));
         break;
      case SETTING_EMU_AUDIO_DEFAULT_ALL:
         strlcpy(current_item->text, "DEFAULTS", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Set all Audio settings to defaults.", sizeof(current_item->comment));
         break;
      case SETTING_PATH_DEFAULT_ALL:
         strlcpy(current_item->text, "DEFAULTS", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Set all Path settings to defaults.", sizeof(current_item->comment));
         break;
      case SETTING_EMU_DEFAULT_ALL:
         strlcpy(current_item->text, "DEFAULTS", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Set all RetroArch settings to defaults.", sizeof(current_item->comment));
         break;
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
      case SETTING_SAVE_SHADER_PRESET:
         strlcpy(current_item->text, "SAVE SETTINGS AS CGP PRESET", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "INFO - Save the current video settings to a [CG Preset] (CGP) file.", sizeof(current_item->comment));
         break;
#endif
      case INGAME_MENU_LOAD_STATE:
         strlcpy(current_item->text, "Load State", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%d", g_extern.state_slot);
         strlcpy(current_item->comment, "Load from current state slot.", sizeof(current_item->comment));
         break;
      case INGAME_MENU_SAVE_STATE:
         strlcpy(current_item->text, "Save State", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%d", g_extern.state_slot);
         strlcpy(current_item->comment, "Save to current state slot.", sizeof(current_item->comment));
         break;
      case INGAME_MENU_ASPECT_RATIO:
         strlcpy(current_item->text, "Aspect Ratio", sizeof(current_item->text));
         strlcpy(current_item->setting_text, aspectratio_lut[g_settings.video.aspect_ratio_idx].name, sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "Change the aspect ratio of the screen.", sizeof(current_item->comment));
         break;
      case INGAME_MENU_OVERSCAN:
         strlcpy(current_item->text, "Overscan", sizeof(current_item->text));
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%f", g_extern.console.screen.overscan_amount);
         strlcpy(current_item->comment, "Change overscan correction.", sizeof(current_item->comment));
         break;
      case INGAME_MENU_ROTATION:
         strlcpy(current_item->text, "Rotation", sizeof(current_item->text));
         strlcpy(current_item->setting_text, rotation_lut[g_extern.console.screen.orientation], sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "Change orientation of the screen.", sizeof(current_item->comment));
         break;
      case INGAME_MENU_RESIZE_MODE:
         strlcpy(current_item->text, "Resize Mode", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "Allows you to resize the screen.", sizeof(current_item->comment));
         break;
      case INGAME_MENU_FRAME_ADVANCE:
         strlcpy(current_item->text, "Frame Advance", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "Press a button to step one frame.", sizeof(current_item->comment));
         break;
      case INGAME_MENU_SCREENSHOT_MODE:
         strlcpy(current_item->text, "Screenshot Mode", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "Take a screenshot.", sizeof(current_item->comment));
         break;
      case INGAME_MENU_RESET:
         strlcpy(current_item->text, "Reset", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "Reset the game.", sizeof(current_item->comment));
         break;
      case INGAME_MENU_RETURN_TO_GAME:
         strlcpy(current_item->text, "Return to Game", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "Change the currently loaded game.", sizeof(current_item->comment));
         break;
      case INGAME_MENU_CHANGE_GAME:
         strlcpy(current_item->text, "Change Game", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "Select another game.", sizeof(current_item->comment));
         break;
      case INGAME_MENU_CHANGE_LIBRETRO_CORE:
         strlcpy(current_item->text, "Change libretro core", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "Choose another libretro core.", sizeof(current_item->comment));
         break;
#ifdef HAVE_MULTIMAN
      case INGAME_MENU_RETURN_TO_MULTIMAN:
         strlcpy(current_item->text, "Return to multiMAN", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "Quit RetroArch and return to multiMAN.", sizeof(current_item->comment));
         break;
#endif
      case INGAME_MENU_QUIT_RETROARCH:
         strlcpy(current_item->text, "Quit RetroArch", sizeof(current_item->text));
         strlcpy(current_item->setting_text, "", sizeof(current_item->setting_text));
         strlcpy(current_item->comment, "Quit RetroArch.", sizeof(current_item->comment));
         break;
      default:
         break;
   }
}

static void display_menubar(uint8_t menu_type)
{
   char title[32];
   char msg[128];
   font_params_t font_parms = {0};

   filebrowser_t *fb = browser;

   rmenu_default_positions_t default_pos;
   menu_set_default_pos(&default_pos);

   font_parms.x = default_pos.x_position;
   font_parms.y = default_pos.current_path_y_position;
   font_parms.scale = default_pos.current_path_font_size;
   font_parms.color = WHITE;

   struct platform_bind key_label_r = {0};
   struct platform_bind key_label_l = {0};
   strlcpy(key_label_r.desc, "Unknown", sizeof(key_label_r.desc));
   key_label_r.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_R;

   strlcpy(key_label_l.desc, "Unknown", sizeof(key_label_l.desc));
   key_label_l.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_L;

   switch(menu_type)
   {
      case GENERAL_VIDEO_MENU:
         if (driver.input->set_keybinds)
            driver.input->set_keybinds(&key_label_r, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));

         snprintf(msg, sizeof(msg), "NEXT -> [%s]", key_label_r.desc);

         if (driver.video_poke->set_osd_msg)
            driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);
         break;
      case GENERAL_AUDIO_MENU:
      case EMU_GENERAL_MENU:
      case EMU_VIDEO_MENU:
      case EMU_AUDIO_MENU:
      case PATH_MENU:
         if (driver.input->set_keybinds)
         {
            driver.input->set_keybinds(&key_label_r, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
            driver.input->set_keybinds(&key_label_l, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         }
         snprintf(msg, sizeof(msg), "[%s] <- PREV | NEXT -> [%s]", key_label_l.desc, key_label_r.desc);

         if (driver.video_poke->set_osd_msg)
            driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);
         break;
      case CONTROLS_MENU:
      case INGAME_MENU_RESIZE:
         if (driver.input->set_keybinds)
            driver.input->set_keybinds(&key_label_l, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));

         snprintf(msg, sizeof(msg), "[%s] <- PREV", key_label_l.desc);

         if (driver.video_poke->set_osd_msg)
            driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);
         break;
      default:
         break;
   }

   switch(menu_type)
   {
      case SHADER_CHOICE:
         strlcpy(title, "Shaders", sizeof(title));
         break;
      case PRESET_CHOICE:
         strlcpy(title, "Shader", sizeof(title));
         break;
      case BORDER_CHOICE:
         strlcpy(title, "Borders", sizeof(title));
         break;
      case LIBRETRO_CHOICE:
         strlcpy(title, "Libretro", sizeof(title));
         break;
      case INPUT_PRESET_CHOICE:
         strlcpy(title, "Input", sizeof(title));
         break;
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_DEFAULT_ROM_DIR_CHOICE:
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SRAM_DIR_CHOICE:
      case PATH_SYSTEM_DIR_CHOICE:
         strlcpy(title, "Path", sizeof(title));
         break;
      case INGAME_MENU:
         strlcpy(title, "Ingame Menu", sizeof(title));
         break;
      case INGAME_MENU_RESIZE:
         strlcpy(title, "Resize Menu", sizeof(title));
         break;
      case INGAME_MENU_SCREENSHOT:
         strlcpy(title, "Ingame Menu", sizeof(title));
         break;
      case FILE_BROWSER_MENU:
         strlcpy(title, "Filebrowser", sizeof(title));
         break;
      case GENERAL_VIDEO_MENU:
         strlcpy(title, "Video", sizeof(title));
         break;
      case GENERAL_AUDIO_MENU:
         strlcpy(title, "Audio", sizeof(title));
         break;
      case EMU_GENERAL_MENU:
         strlcpy(title, "Retro", sizeof(title));
         break;
      case EMU_VIDEO_MENU:
         strlcpy(title, "Retro Video", sizeof(title));
         break;
      case EMU_AUDIO_MENU:
         strlcpy(title, "Retro Audio", sizeof(title));
         break;
      case PATH_MENU:
         strlcpy(title, "Path", sizeof(title));
         break;
      case CONTROLS_MENU:
         strlcpy(title, "Controls", sizeof(title));
         break;
   }

   switch(menu_type)
   {
      case SHADER_CHOICE:
      case PRESET_CHOICE:
      case BORDER_CHOICE:
      case LIBRETRO_CHOICE:
      case INPUT_PRESET_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_DEFAULT_ROM_DIR_CHOICE:
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SRAM_DIR_CHOICE:
      case PATH_SYSTEM_DIR_CHOICE:
         fb = tmpBrowser;
      case FILE_BROWSER_MENU:
         snprintf(msg, sizeof(msg), "PATH: %s", fb->directory_path);

         if (driver.video_poke->set_osd_msg)
            driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);
         break;
   }

   rarch_position_t position = {0};
   rmenu_gfx_draw_bg(&position);
   
   font_parms.x = default_pos.core_msg_x_position;
   font_parms.y = default_pos.core_msg_y_position;
   font_parms.scale = default_pos.core_msg_font_size;
   font_parms.color = WHITE;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, g_extern.title_buf, &font_parms);
#ifdef __CELLOS_LV2__

   font_parms.x = default_pos.x_position;
   font_parms.y = 0.05f;
   font_parms.scale = 1.4f;
   font_parms.color = WHITE;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, title, &font_parms);

   font_parms.x = 0.80f;
   font_parms.y = 0.015f;
   font_parms.scale = 0.82f;
   font_parms.color = WHITE;
   snprintf(msg, sizeof(msg), "v%s", PACKAGE_VERSION);

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);
#endif
}

static void browser_update(void *data, uint64_t input, const char *extensions)
{
   filebrowser_t *b = (filebrowser_t*)data;
   filebrowser_action_t action = FILEBROWSER_ACTION_NOOP;
   bool ret = true;

   if (input & (1ULL << RMENU_DEVICE_NAV_DOWN))
      action = FILEBROWSER_ACTION_DOWN;
   else if (input & (1ULL << RMENU_DEVICE_NAV_UP))
      action = FILEBROWSER_ACTION_UP;
   else if (input & (1ULL << RMENU_DEVICE_NAV_RIGHT))
      action = FILEBROWSER_ACTION_RIGHT;
   else if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
      action = FILEBROWSER_ACTION_LEFT;
   else if (input & (1ULL << RMENU_DEVICE_NAV_R2))
      action = FILEBROWSER_ACTION_SCROLL_DOWN;
   else if (input & (1ULL << RMENU_DEVICE_NAV_L2))
      action = FILEBROWSER_ACTION_SCROLL_UP;
   else if (input & (1ULL << RMENU_DEVICE_NAV_A))
      action = FILEBROWSER_ACTION_CANCEL;
   else if (input & (1ULL << RMENU_DEVICE_NAV_START))
   {
      action = FILEBROWSER_ACTION_RESET;
      filebrowser_set_root_and_ext(b, NULL, default_paths.filesystem_root_dir);
      strlcpy(b->extensions, extensions, sizeof(b->extensions));
   }

   if (action != FILEBROWSER_ACTION_NOOP)
      ret = filebrowser_iterate(b, action);

   if (!ret)
      msg_queue_push(g_extern.msg_queue, "ERROR - Failed to open directory.", 1, 180);
}

static void browser_render(void *data)
{
   filebrowser_t *b = (filebrowser_t*)data;
   unsigned file_count = b->current_dir.list->size;
   unsigned current_index = 0;
   unsigned page_number = 0;
   unsigned page_base = 0;
   unsigned i;
   font_params_t font_parms = {0};
   rmenu_default_positions_t default_pos = {0};

   menu_set_default_pos(&default_pos);

   current_index = b->current_dir.ptr;
   page_number = current_index / default_pos.entries_per_page;
   page_base = page_number * default_pos.entries_per_page;

   font_parms.scale = default_pos.variable_font_size;

   for (i = page_base; i < file_count && i < page_base + default_pos.entries_per_page; ++i)
   {
      char fname_tmp[128];
      fill_pathname_base(fname_tmp, b->current_dir.list->elems[i].data, sizeof(fname_tmp));
      default_pos.starting_y_position += default_pos.y_position_increment;

      //check if this is the currently selected file
      const char *current_pathname = filebrowser_get_current_path(b);
      if (strcmp(current_pathname, b->current_dir.list->elems[i].data) == 0)
      {
         rarch_position_t position = {0};
         position.x = default_pos.x_position;
         position.y = default_pos.starting_y_position;
         rmenu_gfx_draw_panel(&position);
      }

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.starting_y_position;
      font_parms.color = i == current_index ? RED : b->current_dir.list->elems[i].attr.b ? GREEN : WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, fname_tmp, &font_parms);
   }
}

static int select_file(uint8_t menu_type, uint64_t input)
{
   char extensions[128];
   char comment[128];
   char path[PATH_MAX];
   bool ret = true;
   font_params_t font_parms = {0};

   filebrowser_t *filebrowser = tmpBrowser;
   rmenu_default_positions_t default_pos;
   menu_set_default_pos(&default_pos);

   switch(menu_type)
   {
      case SHADER_CHOICE:
         strlcpy(extensions, EXT_SHADERS, sizeof(extensions));
         strlcpy(comment, "INFO - Select a shader.", sizeof(comment));
         break;
      case PRESET_CHOICE:
         strlcpy(extensions, EXT_CGP_PRESETS, sizeof(extensions));
         strlcpy(comment, "INFO - Select a shader preset.", sizeof(comment));
         break;
      case INPUT_PRESET_CHOICE:
         strlcpy(extensions, EXT_INPUT_PRESETS, sizeof(extensions));
         strlcpy(comment, "INFO - Select an input preset.", sizeof(comment));
         break;
      case BORDER_CHOICE:
         strlcpy(extensions, EXT_IMAGES, sizeof(extensions));
         strlcpy(comment, "INFO - Select a border image file.", sizeof(comment));
         break;
      case LIBRETRO_CHOICE:
         strlcpy(extensions, EXT_EXECUTABLES, sizeof(extensions));
         strlcpy(comment, "INFO - Select a Libretro core.", sizeof(comment));
         break;
   }

   browser_update(filebrowser, input, extensions);

   if (input & (1ULL << RMENU_DEVICE_NAV_B))
   {
      if (filebrowser_iterate(filebrowser, FILEBROWSER_ACTION_PATH_ISDIR))
         ret = filebrowser_iterate(filebrowser, FILEBROWSER_ACTION_OK);
      else
      {
         strlcpy(path, filebrowser_get_current_path(filebrowser), sizeof(path));

         switch(menu_type)
         {
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
            case SHADER_CHOICE:
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_FIRST_SHADER))
               {
                  strlcpy(g_settings.video.cg_shader_path, path, sizeof(g_settings.video.cg_shader_path));

                  if (g_settings.video.shader_type != RARCH_SHADER_NONE)
                  {
                     driver.video->set_shader(driver.video_data, (enum rarch_shader_type)g_settings.video.shader_type, path, RARCH_SHADER_INDEX_PASS0);
                     if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
                        msg_queue_push(g_extern.msg_queue, "INFO - Shader successfully loaded.", 1, 180);
                  }
                  else
                     RARCH_ERR("Shaders are unsupported on this platform.\n");

                  g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_FIRST_SHADER);
               }
               break;
            case PRESET_CHOICE:
               strlcpy(g_extern.file_state.cgp_path, path, sizeof(g_extern.file_state.cgp_path));
               break;
#endif
            case INPUT_PRESET_CHOICE:
               strlcpy(g_extern.file_state.input_cfg_path, path, sizeof(g_extern.file_state.input_cfg_path));
               config_read_keybinds(path);
               break;
            case BORDER_CHOICE:
#ifdef __CELLOS_LV2__
               texture_image_border_load(path);
               strlcpy(g_extern.console.menu_texture_path, path, sizeof(g_extern.console.menu_texture_path));
#endif
               break;
            case LIBRETRO_CHOICE:
               strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));

               if (set_libretro_core_as_launch)
               {
                  strlcpy(g_extern.fullpath, path, sizeof(g_extern.fullpath));
                  set_libretro_core_as_launch = false;
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN);
                  return -1;
               }
               else
               {
                  if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
                     msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch.", 1, 180);
               }
               break;
         }

         menu_stack_pop();
      }

      if (!ret)
         msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch.", 1, 180);
   }
   else if (input & (1ULL << RMENU_DEVICE_NAV_X))
      menu_stack_pop();

   display_menubar(menu_type);

   font_parms.x = default_pos.x_position;
   font_parms.y = default_pos.comment_y_position;
   font_parms.scale = default_pos.font_size;
   font_parms.color = WHITE;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, comment, &font_parms);

   struct platform_bind key_label_x = {0};
   struct platform_bind key_label_start = {0};
   strlcpy(key_label_x.desc, "Unknown", sizeof(key_label_x.desc));
   key_label_x.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_X;

   strlcpy(key_label_start.desc, "Unknown", sizeof(key_label_start.desc));
   key_label_start.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_START;

   if (driver.input->set_keybinds)
   {
      driver.input->set_keybinds(&key_label_x, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
      driver.input->set_keybinds(&key_label_start, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
   }

   snprintf(comment, sizeof(comment), "[%s] - return to settings [%s] - Reset Startdir", key_label_x.desc, key_label_start.desc);
   font_parms.y = default_pos.comment_two_y_position;
   font_parms.color = YELLOW;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, comment, &font_parms);

   browser_render(filebrowser);

   return 0;
}

static int select_directory(uint8_t menu_type, uint64_t input)
{
   font_params_t font_parms = {0};

   char path[PATH_MAX];
   char msg[256];
   bool ret = true;

   filebrowser_t *filebrowser = tmpBrowser;
   rmenu_default_positions_t default_pos;

   menu_set_default_pos(&default_pos);

   bool is_dir = filebrowser_iterate(filebrowser, FILEBROWSER_ACTION_PATH_ISDIR);
   browser_update(filebrowser, input, "empty");

   if (input & (1ULL << RMENU_DEVICE_NAV_Y))
   {
      if (is_dir)
      {
         strlcpy(path, filebrowser_get_current_path(filebrowser), sizeof(path));

         switch(menu_type)
         {
            case PATH_SAVESTATES_DIR_CHOICE:
               strlcpy(g_extern.console.main_wrap.default_savestate_dir, path, sizeof(g_extern.console.main_wrap.default_savestate_dir));
               break;
            case PATH_SRAM_DIR_CHOICE:
               strlcpy(g_extern.console.main_wrap.default_sram_dir, path, sizeof(g_extern.console.main_wrap.default_sram_dir));
               break;
            case PATH_DEFAULT_ROM_DIR_CHOICE:
               strlcpy(g_extern.console.main_wrap.default_rom_startup_dir, path, sizeof(g_extern.console.main_wrap.default_rom_startup_dir));
               break;
#ifdef HAVE_XML
            case PATH_CHEATS_DIR_CHOICE:
               strlcpy(g_settings.cheat_database, path, sizeof(g_settings.cheat_database));
               break;
#endif
            case PATH_SYSTEM_DIR_CHOICE:
               strlcpy(g_settings.system_directory, path, sizeof(g_settings.system_directory));
               break;
         }
         menu_stack_pop();
      }
   }
   else if (input & (1ULL << RMENU_DEVICE_NAV_X))
   {
      strlcpy(path, default_paths.port_dir, sizeof(path));
      switch(menu_type)
      {
         case PATH_SAVESTATES_DIR_CHOICE:
            strlcpy(g_extern.console.main_wrap.default_savestate_dir, path, sizeof(g_extern.console.main_wrap.default_savestate_dir));
            break;
         case PATH_SRAM_DIR_CHOICE:
            strlcpy(g_extern.console.main_wrap.default_sram_dir, path, sizeof(g_extern.console.main_wrap.default_sram_dir));
            break;
         case PATH_DEFAULT_ROM_DIR_CHOICE:
            strlcpy(g_extern.console.main_wrap.default_rom_startup_dir, path, sizeof(g_extern.console.main_wrap.default_rom_startup_dir));
            break;
#ifdef HAVE_XML
         case PATH_CHEATS_DIR_CHOICE:
            strlcpy(g_settings.cheat_database, path, sizeof(g_settings.cheat_database));
            break;
#endif
         case PATH_SYSTEM_DIR_CHOICE:
            strlcpy(g_settings.system_directory, path, sizeof(g_settings.system_directory));
            break;
      }

      menu_stack_pop();
   }
   else if (input & (1ULL << RMENU_DEVICE_NAV_B))
   {
      if (is_dir)
         ret = filebrowser_iterate(filebrowser, FILEBROWSER_ACTION_OK);
   }

   if (!ret)
      msg_queue_push(g_extern.msg_queue, "ERROR - Failed to open directory.", 1, 180);

   display_menubar(menu_type);

   struct platform_bind key_label_b = {0};
   struct platform_bind key_label_x = {0};
   struct platform_bind key_label_y = {0};
   struct platform_bind key_label_start = {0};

   strlcpy(key_label_b.desc, "Unknown", sizeof(key_label_b.desc));
   key_label_b.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_B;
   strlcpy(key_label_x.desc, "Unknown", sizeof(key_label_x.desc));
   key_label_x.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_X;
   strlcpy(key_label_y.desc, "Unknown", sizeof(key_label_y.desc));
   key_label_y.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_Y;
   strlcpy(key_label_start.desc, "Unknown", sizeof(key_label_start.desc));
   key_label_start.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_START;

   if (driver.input->set_keybinds)
   {
      driver.input->set_keybinds(&key_label_x, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
      driver.input->set_keybinds(&key_label_y, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
      driver.input->set_keybinds(&key_label_b, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
      driver.input->set_keybinds(&key_label_start, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
   }

   snprintf(msg, sizeof(msg), "[%s] - Enter dir | [%s] - Go back", key_label_b.desc, key_label_x.desc);

   font_parms.x = default_pos.x_position;
   font_parms.y = default_pos.comment_two_y_position;
   font_parms.scale = default_pos.font_size;
   font_parms.color = YELLOW;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   snprintf(msg, sizeof(msg), "[%s] - Reset to startdir", key_label_start.desc);

   font_parms.y = default_pos.comment_two_y_position + (default_pos.y_position_increment * 1);

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   snprintf(msg, sizeof(msg), "INFO - Browse to a directory and assign it as the path by\npressing [%s].", key_label_y.desc);

   font_parms.y = default_pos.comment_y_position;
   font_parms.color = WHITE;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   browser_render(filebrowser);

   return 0;
}

static void set_keybind_digital(unsigned default_retro_joypad_id, uint64_t input)
{
   if (!driver.input->set_keybinds)
      return;

   unsigned keybind_action = KEYBINDS_ACTION_NONE;

   if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
      keybind_action = (1ULL << KEYBINDS_ACTION_DECREMENT_BIND);

   if ((input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
      keybind_action = (1ULL << KEYBINDS_ACTION_INCREMENT_BIND);

   if (input & (1ULL << RMENU_DEVICE_NAV_START))
      keybind_action = (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BIND);


   if (keybind_action)
      driver.input->set_keybinds(driver.input_data, NULL, currently_selected_controller_menu,
            default_retro_joypad_id, keybind_action);
}

#if defined(HAVE_OSKUTIL)
#ifdef __CELLOS_LV2__

static bool osk_callback_enter_rsound(void *data)
{
   if (g_extern.lifecycle_mode_state & (1ULL << MODE_OSK_ENTRY_SUCCESS))
   {
      RARCH_LOG("OSK - Applying input data.\n");
      char tmp_str[256];
      int num = wcstombs(tmp_str, g_extern.console.misc.oskutil_handle.text_buf, sizeof(tmp_str));
      tmp_str[num] = 0;
      strlcpy(g_settings.audio.device, tmp_str, sizeof(g_settings.audio.device));
      goto do_exit;
   }
   else if (g_extern.lifecycle_mode_state & (1ULL << MODE_OSK_ENTRY_FAIL))
      goto do_exit;

   return false;

do_exit:
   g_extern.lifecycle_mode_state &= ~((1ULL << MODE_OSK_DRAW) | (1ULL << MODE_OSK_ENTRY_SUCCESS) |
         (1ULL << MODE_OSK_ENTRY_FAIL));
   return true;
}

static bool osk_callback_enter_rsound_init(void *data)
{
   oskutil_write_initial_message(&g_extern.console.misc.oskutil_handle, L"192.168.1.1");
   oskutil_write_message(&g_extern.console.misc.oskutil_handle, L"Enter IP address for the RSound Server.");
   oskutil_start(&g_extern.console.misc.oskutil_handle);

   return true;
}

static bool osk_callback_enter_filename(void *data)
{
   if (g_extern.lifecycle_mode_state & (1ULL << MODE_OSK_ENTRY_SUCCESS))
   {
      RARCH_LOG("OSK - Applying input data.\n");
      char tmp_str[256];
      char filepath[PATH_MAX];
      int num = wcstombs(tmp_str, g_extern.console.misc.oskutil_handle.text_buf, sizeof(tmp_str));
      tmp_str[num] = 0;

      switch(rmenu_state.osk_param)
      {
         case CONFIG_FILE:
            break;
         case SHADER_PRESET_FILE:
            snprintf(filepath, sizeof(filepath), "%s/%s.cgp", default_paths.cgp_dir, tmp_str);
            RARCH_LOG("[osk_callback_enter_filename]: filepath is: %s.\n", filepath);
            /* TODO - stub */
            break;
         case INPUT_PRESET_FILE:
            snprintf(filepath, sizeof(filepath), "%s/%s.cfg", default_paths.input_presets_dir, tmp_str);
            RARCH_LOG("[osk_callback_enter_filename]: filepath is: %s.\n", filepath);
            config_save_keybinds(filepath);
            break;
      }

      goto do_exit;
   }
   else if (g_extern.lifecycle_mode_state & (1ULL << MODE_OSK_ENTRY_FAIL))
      goto do_exit;

   return false;
do_exit:
   g_extern.lifecycle_mode_state &= ~((1ULL << MODE_OSK_DRAW) | (1ULL << MODE_OSK_ENTRY_SUCCESS) |
         (1ULL << MODE_OSK_ENTRY_FAIL));
   return true;
}

static bool osk_callback_enter_filename_init(void *data)
{
   oskutil_write_initial_message(&g_extern.console.misc.oskutil_handle, L"example");
   oskutil_write_message(&g_extern.console.misc.oskutil_handle, L"Enter filename for preset");
   oskutil_start(&g_extern.console.misc.oskutil_handle);

   return true;
}

#endif
#endif

static int set_setting_action(uint8_t menu_type, unsigned switchvalue, uint64_t input)
{
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;
   filebrowser_t *filebrowser = tmpBrowser;

   switch(switchvalue)
   {
#ifdef __CELLOS_LV2__
      case SETTING_CHANGE_RESOLUTION:
         if (input & (1ULL << RMENU_DEVICE_NAV_RIGHT))
            settings_set(1ULL << S_RESOLUTION_NEXT);
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
            settings_set(1ULL << S_RESOLUTION_PREVIOUS);
         if (input & (1ULL << RMENU_DEVICE_NAV_B))
         {
            if (g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx] == CELL_VIDEO_OUT_RESOLUTION_576)
            {
               if (g_extern.console.screen.pal_enable)
                  g_extern.lifecycle_mode_state |= (1ULL<< MODE_VIDEO_PAL_ENABLE);
            }
            else
            {
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_ENABLE);
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_VSYNC_BLOCK);
            }

            driver.video->restart();
         }
         break;
      case SETTING_PAL60_MODE:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_ENABLE))
            {
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
               {
                  g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
                  g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_VSYNC_BLOCK);
               }
               else
               {
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_PAL_VSYNC_BLOCK);
               }

               driver.video->restart();
            }
         }

         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_ENABLE))
            {
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_PAL_VSYNC_BLOCK);
               driver.video->restart();
            }
         }
         break;
#endif
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
      case SETTING_SHADER_PRESETS:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            if (g_extern.main_is_init)
            {
               menu_stack_push(PRESET_CHOICE);
               filebrowser_set_root_and_ext(filebrowser, EXT_CGP_PRESETS, default_paths.cgp_dir);
            }
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            strlcpy(g_extern.file_state.cgp_path, "", sizeof(g_extern.file_state.cgp_path));
         break;
      case SETTING_SHADER:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            menu_stack_push(SHADER_CHOICE);
            filebrowser_set_root_and_ext(filebrowser, EXT_SHADERS, default_paths.shader_dir);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_FIRST_SHADER);
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            strlcpy(g_settings.video.cg_shader_path, default_paths.shader_file, sizeof(g_settings.video.cg_shader_path));
            if (g_settings.video.shader_type != RARCH_SHADER_NONE)
            {
               driver.video->set_shader(driver.video_data, (enum rarch_shader_type)g_settings.video.shader_type, NULL, RARCH_SHADER_INDEX_PASS0);
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
                  msg_queue_push(g_extern.msg_queue, "INFO - Shader successfully loaded.", 1, 180);
            }
            else
               RARCH_ERR("Shaders are unsupported on this platform.\n");
         }
         break;
      case SETTING_EMU_SKIN:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            menu_stack_push(BORDER_CHOICE);
            filebrowser_set_root_and_ext(filebrowser, EXT_IMAGES, default_paths.border_dir);
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            if (!texture_image_load(default_paths.menu_border_file, &g_extern.console.menu_texture))
            {
               RARCH_ERR("Failed to load texture image for menu.\n");
               return false;
            }
         }
         break;
#endif
      case SETTING_EMU_LOW_RAM_MODE_ENABLE:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_LOW_RAM_MODE_ENABLE_PENDING)))
            {
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_LOW_RAM_MODE_ENABLE))
                  g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_LOW_RAM_MODE_ENABLE);
               else
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_LOW_RAM_MODE_ENABLE_PENDING);

               if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
                  msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch.", 1, 180);
            }
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_LOW_RAM_MODE_ENABLE)))
            {
               if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_LOW_RAM_MODE_ENABLE_PENDING)))
               {
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_LOW_RAM_MODE_ENABLE);
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_LOW_RAM_MODE_ENABLE_PENDING);

                  if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
                     msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch.", 1, 180);
               }
            }
         }
         break;
      case SETTING_FONT_SIZE:
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
         {
            if (g_settings.video.font_size > 0) 
               g_settings.video.font_size -= 0.01f;
         }
         if ((input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            if ((g_settings.video.font_size < 2.0f))
               g_settings.video.font_size += 0.01f;
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            g_settings.video.font_size = 1.0f;
         break;
      case INGAME_MENU_ASPECT_RATIO:
      case SETTING_KEEP_ASPECT_RATIO:
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
         {
            settings_set(1ULL << S_ASPECT_RATIO_DECREMENT);

            if (driver.video_poke->set_aspect_ratio)
               driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_RIGHT))
         {
            settings_set(1ULL << S_ASPECT_RATIO_INCREMENT);

            if (driver.video_poke->set_aspect_ratio)
               driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            settings_set(1ULL << S_DEF_ASPECT_RATIO);

            if (driver.video_poke->set_aspect_ratio)
               driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
         }
         break;
      case SETTING_HW_TEXTURE_FILTER:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            settings_set(1ULL << S_HW_TEXTURE_FILTER);

            if (driver.video_poke->set_filtering)
               driver.video_poke->set_filtering(driver.video_data, 1, g_settings.video.smooth);
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            settings_set(1ULL << S_DEF_HW_TEXTURE_FILTER);

            if (driver.video_poke->set_filtering)
               driver.video_poke->set_filtering(driver.video_data, 1, g_settings.video.smooth);
         }
         break;
#ifdef _XBOX1
      case SETTING_FLICKER_FILTER:
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
         {
            if (g_extern.console.screen.flicker_filter_index > 0)
               g_extern.console.screen.flicker_filter_index--;
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_RIGHT))
         {
            if (g_extern.console.screen.flicker_filter_index < 5)
               g_extern.console.screen.flicker_filter_index++;
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            g_extern.console.screen.flicker_filter_index = 0;
         break;
      case SETTING_SOFT_DISPLAY_FILTER:
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE))
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
            else
               g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
         break;
#endif
      case SETTING_HW_OVERSCAN_AMOUNT:
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
         {
            settings_set(1ULL << S_OVERSCAN_DECREMENT);

            if (driver.video_poke->apply_state_changes)
               driver.video_poke->apply_state_changes(driver.video_data);
         }
         if ((input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            settings_set(1ULL << S_OVERSCAN_INCREMENT);

            if (driver.video_poke->apply_state_changes)
               driver.video_poke->apply_state_changes(driver.video_data);
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            settings_set(1ULL << S_DEF_OVERSCAN);

            if (driver.video_poke->apply_state_changes)
               driver.video_poke->apply_state_changes(driver.video_data);
         }
         break;
      case SETTING_REFRESH_RATE:
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
         {
            settings_set(1ULL << S_REFRESH_RATE_DECREMENT);
            driver_set_monitor_refresh_rate(g_settings.video.refresh_rate);
         }
         if ((input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            settings_set(1ULL << S_REFRESH_RATE_INCREMENT);
            driver_set_monitor_refresh_rate(g_settings.video.refresh_rate);
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            settings_set(1ULL << S_DEF_REFRESH_RATE);
            driver_set_monitor_refresh_rate(g_settings.video.refresh_rate);
         }
         break;
      case SETTING_THROTTLE_MODE:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_VSYNC_BLOCK)))
            {
               settings_set(1ULL << S_THROTTLE);
               device_ptr->ctx_driver->swap_interval((g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_THROTTLE_ENABLE)) ? true : false);
            }
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_PAL_VSYNC_BLOCK)))
            {
               settings_set(1ULL << S_DEF_THROTTLE);
               device_ptr->ctx_driver->swap_interval((g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_THROTTLE_ENABLE)) ? true : false);
            }
         }
         break;
      case SETTING_TRIPLE_BUFFERING:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            settings_set(1ULL << S_TRIPLE_BUFFERING);
            driver.video->restart();
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            settings_set(1ULL << S_DEF_TRIPLE_BUFFERING);

            if (!(g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE)))
               driver.video->restart();
         }
         break;
      case SETTING_ENABLE_SCREENSHOTS:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SCREENSHOTS_ENABLE))
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_SCREENSHOTS_ENABLE);
            else
               g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_SCREENSHOTS_ENABLE);
            device_ptr->ctx_driver->rmenu_screenshot_enable((g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SCREENSHOTS_ENABLE)) ? true : false);
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_SCREENSHOTS_ENABLE);
            device_ptr->ctx_driver->rmenu_screenshot_enable((g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SCREENSHOTS_ENABLE)) ? true : false);
         }
         break;
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
      case SETTING_SAVE_SHADER_PRESET:
#ifdef HAVE_OSKUTIL
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            if (g_extern.main_is_init)
            {
               rmenu_state.osk_param = SHADER_PRESET_FILE;
               rmenu_state.osk_init = osk_callback_enter_filename_init;
               rmenu_state.osk_callback = osk_callback_enter_filename;
            }
         }
#endif
         break;
      case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
         break;
#endif
      case SETTING_DEFAULT_VIDEO_ALL:
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
            set_setting_action(NULL, SETTING_SHADER, 1ULL << RMENU_DEVICE_NAV_START);
#endif
         }
         break;
      case SETTING_SOUND_MODE:
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
         {
            if (g_extern.console.sound.mode != SOUND_MODE_NORMAL)
               g_extern.console.sound.mode--;
         }
         if ((input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            if (g_extern.console.sound.mode < (SOUND_MODE_LAST-1))
               g_extern.console.sound.mode++;
         }
         if ((input & (1ULL << RMENU_DEVICE_NAV_UP)) || (input & (1ULL << RMENU_DEVICE_NAV_DOWN)))
         {
#ifdef HAVE_RSOUND
            if (g_extern.console.sound.mode != SOUND_MODE_RSOUND)
               rarch_rsound_stop();
            else
               rarch_rsound_start(g_settings.audio.device);
#endif
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            g_extern.console.sound.mode = SOUND_MODE_NORMAL;
#ifdef HAVE_RSOUND
            rarch_rsound_stop();
#endif
         }
         break;
#ifdef HAVE_RSOUND
      case SETTING_RSOUND_SERVER_IP_ADDRESS:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
#ifdef HAVE_OSKUTIL
            rmenu_state.osk_init = osk_callback_enter_rsound_init;
            rmenu_state.osk_callback = osk_callback_enter_rsound;
#endif
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            strlcpy(g_settings.audio.device, "0.0.0.0", sizeof(g_settings.audio.device));
         break;
#endif
      case SETTING_DEFAULT_AUDIO_ALL:
         break;
      case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
            settings_set(1ULL << S_SAVESTATE_DECREMENT);
         if ((input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
            settings_set(1ULL << S_SAVESTATE_INCREMENT);
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            settings_set(1ULL << S_DEF_SAVE_STATE);
         break;
      case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
            settings_set(1ULL << S_INFO_DEBUG_MSG_TOGGLE);
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            settings_set(1ULL << S_DEF_INFO_DEBUG_MSG);
         break;
      case SETTING_EMU_SHOW_INFO_MSG:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
            settings_set(1ULL << S_INFO_MSG_TOGGLE);
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            settings_set(1ULL << S_DEF_INFO_MSG);
         break;
      case SETTING_EMU_REWIND_ENABLED:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            settings_set(1ULL << S_REWIND);

            if (g_settings.rewind_enable)
               rarch_init_rewind();
            else
               rarch_deinit_rewind();
         }

         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            if (g_settings.rewind_enable)
            {
               g_settings.rewind_enable = false;
               rarch_deinit_rewind();
            }
         }
         break;
      case SETTING_EMU_REWIND_GRANULARITY:
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
         {
            if (g_settings.rewind_granularity > 1)
               g_settings.rewind_granularity--;
         }
         if ((input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
            g_settings.rewind_granularity++;
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            g_settings.rewind_granularity = 1;
         break;
      case SETTING_RARCH_DEFAULT_EMU:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            menu_stack_push(LIBRETRO_CHOICE);
            filebrowser_set_root_and_ext(filebrowser, EXT_EXECUTABLES, default_paths.core_dir);
            set_libretro_core_as_launch = true;
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
         }
         break;
      case SETTING_QUIT_RARCH:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            g_extern.lifecycle_mode_state &= ~((1ULL << MODE_GAME));
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
            return -1;
         }
         break;
      case SETTING_RESAMPLER_TYPE:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
#ifdef HAVE_SINC
            if ( strstr(g_settings.audio.resampler, "hermite"))
               strlcpy(g_settings.audio.resampler, "sinc", sizeof(g_settings.audio.resampler));
            else
#endif
               strlcpy(g_settings.audio.resampler, "hermite", sizeof(g_settings.audio.resampler));

            if (g_extern.main_is_init)
            {
               if (!rarch_resampler_realloc(&g_extern.audio_data.resampler_data, &g_extern.audio_data.resampler,
                        g_settings.audio.resampler, g_extern.audio_data.orig_src_ratio == 0.0 ? 1.0 : g_extern.audio_data.orig_src_ratio))
               {
                  RARCH_ERR("Failed to initialize resampler \"%s\".\n", g_settings.audio.resampler);
                  g_extern.audio_active = false;
               }
            }

         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
#ifdef HAVE_SINC
            strlcpy(g_settings.audio.resampler, "sinc", sizeof(g_settings.audio.resampler));
#else
            strlcpy(g_settings.audio.resampler, "hermite", sizeof(g_settings.audio.resampler));
#endif

            if (g_extern.main_is_init)
            {
               if (!rarch_resampler_realloc(&g_extern.audio_data.resampler_data, &g_extern.audio_data.resampler,
                        g_settings.audio.resampler, g_extern.audio_data.orig_src_ratio == 0.0 ? 1.0 : g_extern.audio_data.orig_src_ratio))
               {
                  RARCH_ERR("Failed to initialize resampler \"%s\".\n", g_settings.audio.resampler);
                  g_extern.audio_active = false;
               }
            }
         }
         break;
      case SETTING_EMU_AUDIO_MUTE:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
            settings_set(1ULL << S_AUDIO_MUTE);

         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            settings_set(1ULL << S_DEF_AUDIO_MUTE);
         break;
#ifdef _XBOX1
      case SETTING_EMU_AUDIO_SOUND_VOLUME_LEVEL:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            g_extern.console.sound.volume_level = !g_extern.console.sound.volume_level;
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
               msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch.", 1, 180);
         }

         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            g_extern.console.sound.volume_level = 0;
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
               msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch.", 1, 180);
         }
         break;
#endif
      case SETTING_ENABLE_CUSTOM_BGM:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
#if (CELL_SDK_VERSION > 0x340000)
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE))
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE);
            else
               g_extern.lifecycle_mode_state |= (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE);
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE))
               cellSysutilEnableBgmPlayback();
            else
               cellSysutilDisableBgmPlayback();

#endif
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
#if (CELL_SDK_VERSION > 0x340000)
            g_extern.lifecycle_mode_state |= (1ULL << MODE_AUDIO_CUSTOM_BGM_ENABLE);
#endif
         }
         break;
      case SETTING_EMU_VIDEO_DEFAULT_ALL:
         break;
      case SETTING_EMU_AUDIO_DEFAULT_ALL:
         break;
      case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            menu_stack_push(PATH_DEFAULT_ROM_DIR_CHOICE);
            filebrowser_set_root_and_ext(filebrowser, "empty", default_paths.filesystem_root_dir);
         }

         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            strlcpy(g_extern.console.main_wrap.default_rom_startup_dir, default_paths.filesystem_root_dir, sizeof(g_extern.console.main_wrap.default_rom_startup_dir));
         break;
      case SETTING_PATH_SAVESTATES_DIRECTORY:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            menu_stack_push(PATH_SAVESTATES_DIR_CHOICE);
            filebrowser_set_root_and_ext(filebrowser, "empty", default_paths.filesystem_root_dir);
         }

         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            strlcpy(g_extern.console.main_wrap.default_savestate_dir, default_paths.savestate_dir, sizeof(g_extern.console.main_wrap.default_savestate_dir));

         break;
      case SETTING_PATH_SRAM_DIRECTORY:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            menu_stack_push(PATH_SRAM_DIR_CHOICE);
            filebrowser_set_root_and_ext(filebrowser, "empty", default_paths.filesystem_root_dir);
         }

         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            strlcpy(g_extern.console.main_wrap.default_sram_dir, default_paths.sram_dir, sizeof(g_extern.console.main_wrap.default_sram_dir));
         break;
#ifdef HAVE_XML
      case SETTING_PATH_CHEATS:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            menu_stack_push(PATH_CHEATS_DIR_CHOICE);
            filebrowser_set_root_and_ext(filebrowser, "empty", default_paths.filesystem_root_dir);
         }

         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            strlcpy(g_settings.cheat_database, default_paths.port_dir, sizeof(g_settings.cheat_database));
         break;
#endif
      case SETTING_PATH_SYSTEM:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            menu_stack_push(PATH_SYSTEM_DIR_CHOICE);
            filebrowser_set_root_and_ext(filebrowser, "empty", default_paths.system_dir);
         }

         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            strlcpy(g_settings.system_directory, default_paths.system_dir, sizeof(g_settings.system_directory));
         break;
      case SETTING_ENABLE_SRAM_PATH:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)))
         {
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE))
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
            else
               g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
         }

         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
         break;
      case SETTING_ENABLE_STATE_PATH:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)))
         {
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE))
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
            else
               g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
         break;
      case SETTING_PATH_DEFAULT_ALL:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)) || (input & (1ULL << RMENU_DEVICE_NAV_START)))
         {
            strlcpy(g_extern.console.main_wrap.default_rom_startup_dir, "/", sizeof(g_extern.console.main_wrap.default_rom_startup_dir));
            strlcpy(g_extern.console.main_wrap.default_savestate_dir, default_paths.port_dir, sizeof(g_extern.console.main_wrap.default_savestate_dir));
#ifdef HAVE_XML
            strlcpy(g_settings.cheat_database, default_paths.port_dir, sizeof(g_settings.cheat_database));
#endif
            strlcpy(g_extern.console.main_wrap.default_sram_dir, "", sizeof(g_extern.console.main_wrap.default_sram_dir));
         }
         break;
      case SETTING_CONTROLS_SCHEME:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)) || (input & (1ULL << RMENU_DEVICE_NAV_START)))
         {
            menu_stack_push(INPUT_PRESET_CHOICE);
            filebrowser_set_root_and_ext(filebrowser, EXT_INPUT_PRESETS, default_paths.input_presets_dir);
         }
         break;
      case SETTING_CONTROLS_NUMBER:
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
         {
            if (currently_selected_controller_menu != 0)
               currently_selected_controller_menu--;
         }

         if ((input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            if (currently_selected_controller_menu < 6)
               currently_selected_controller_menu++;
         }

         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            currently_selected_controller_menu = 0;
         break; 
      case SETTING_DPAD_EMULATION:
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
         {
            if (driver.input->set_keybinds)
            {
               unsigned keybind_action = 0;

               switch(g_settings.input.dpad_emulation[currently_selected_controller_menu])
               {
                  case ANALOG_DPAD_NONE:
                     break;
                  case ANALOG_DPAD_LSTICK:
                     keybind_action = (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_NONE);
                     break;
                  case ANALOG_DPAD_RSTICK:
                     keybind_action = (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_LSTICK);
                     break;
                  default:
                     break;
               }

               if (keybind_action)
                  driver.input->set_keybinds(driver.input_data, g_settings.input.device[currently_selected_controller_menu], currently_selected_controller_menu, 0, keybind_action);
            }
         }

         if ((input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            if (driver.input->set_keybinds)
            {
               unsigned keybind_action = 0;

               switch(g_settings.input.dpad_emulation[currently_selected_controller_menu])
               {
                  case ANALOG_DPAD_NONE:
                     keybind_action = (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_LSTICK);
                     break;
                  case ANALOG_DPAD_LSTICK:
                     keybind_action = (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_RSTICK);
                     break;
                  case ANALOG_DPAD_RSTICK:
                     break;
               }

               if (keybind_action)
                  driver.input->set_keybinds(driver.input_data, g_settings.input.device[currently_selected_controller_menu], currently_selected_controller_menu, 0, keybind_action);
            }
         }

         if (input & (1ULL << RMENU_DEVICE_NAV_START))
            if (driver.input->set_keybinds)
                  driver.input->set_keybinds(driver.input_data, g_settings.input.device[currently_selected_controller_menu], currently_selected_controller_menu, 0, (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_LSTICK));
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_UP:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_UP, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_DOWN:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_DOWN, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_LEFT:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_LEFT, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_RIGHT:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_RIGHT, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_A:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_A, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_B, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_X:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_X, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_Y:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_Y, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_SELECT:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_SELECT, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_START:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_START, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_L, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_R, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L2:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_L2, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R2:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_R2, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L3:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_L3, input);
         break;
      case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R3:
         set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_R3, input);
         break;
#ifdef HAVE_OSKUTIL
      case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)) || (input & (1ULL << RMENU_DEVICE_NAV_START)))
         {
            rmenu_state_t *rstate = (rmenu_state_t*)&rmenu_state;
            rstate->osk_param = INPUT_PRESET_FILE;
            rstate->osk_init = osk_callback_enter_filename_init;
            rstate->osk_callback = osk_callback_enter_filename;
         }
         break;
#endif
      case SETTING_CONTROLS_DEFAULT_ALL:
         if ((input & (1ULL << RMENU_DEVICE_NAV_LEFT)) || (input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)) || (input & (1ULL << RMENU_DEVICE_NAV_START)))
            if (driver.input->set_keybinds)
               driver.input->set_keybinds(driver.input_data, g_settings.input.device[currently_selected_controller_menu], currently_selected_controller_menu, 0,
                     (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS));
         break;
      case INGAME_MENU_LOAD_STATE:
         if (input & (1ULL << RMENU_DEVICE_NAV_B))
         {
            rarch_load_state();
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            return -1;
         }
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
            rarch_state_slot_decrease();
         if (input & (1ULL << RMENU_DEVICE_NAV_RIGHT))
            rarch_state_slot_increase();

         break;
      case INGAME_MENU_SAVE_STATE:
         if (input & (1ULL << RMENU_DEVICE_NAV_B))
         {
            rarch_save_state();
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            return -1;
         }

         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
            rarch_state_slot_decrease();
         if (input & (1ULL << RMENU_DEVICE_NAV_RIGHT))
            rarch_state_slot_increase();

         break;
      case INGAME_MENU_ROTATION:
         if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
         {
            settings_set(1ULL << S_ROTATION_DECREMENT);
            driver.video->set_rotation(NULL, g_extern.console.screen.orientation);
         }

         if ((input & (1ULL << RMENU_DEVICE_NAV_RIGHT)) || (input & (1ULL << RMENU_DEVICE_NAV_B)))
         {
            settings_set(1ULL << S_ROTATION_INCREMENT);
            driver.video->set_rotation(NULL, g_extern.console.screen.orientation);
         }

         if (input & (1ULL << RMENU_DEVICE_NAV_START))
         {
            settings_set(1ULL << S_DEF_ROTATION);
            driver.video->set_rotation(NULL, g_extern.console.screen.orientation);
         }
         break;
      case INGAME_MENU_FRAME_ADVANCE:
         if ((input & (1ULL << RMENU_DEVICE_NAV_B)) || (input & (1ULL << RMENU_DEVICE_NAV_R2)) || (input & (1ULL << RMENU_DEVICE_NAV_L2)))
         {
            g_extern.lifecycle_state |= (1ULL << RARCH_FRAMEADVANCE);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            settings_set(1ULL << S_FRAME_ADVANCE);
            return -1;
         }
         break;
      case INGAME_MENU_RESIZE_MODE:
         if (input & (1ULL << RMENU_DEVICE_NAV_B))
            menu_stack_push(INGAME_MENU_RESIZE);
         break;
      case INGAME_MENU_SCREENSHOT_MODE:
         if (input & (1ULL << RMENU_DEVICE_NAV_B))
            menu_stack_push(INGAME_MENU_SCREENSHOT);
         break;
      case INGAME_MENU_RETURN_TO_GAME:
         if (input & (1ULL << RMENU_DEVICE_NAV_B))
         {
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            return -1;
         }
         break;
      case INGAME_MENU_CHANGE_GAME:
         if (input & (1ULL << RMENU_DEVICE_NAV_B))
         {
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            return 0;
         }
         break;
      case INGAME_MENU_RESET:
         if (input & (1ULL << RMENU_DEVICE_NAV_B))
         {
            rarch_game_reset();
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            return -1;
         }
         break;
      case INGAME_MENU_CHANGE_LIBRETRO_CORE:
         if (input & (1ULL << RMENU_DEVICE_NAV_B))
         {
            menu_stack_push(LIBRETRO_CHOICE);
            filebrowser_set_root_and_ext(filebrowser, EXT_EXECUTABLES, default_paths.core_dir);
            set_libretro_core_as_launch = true;
         }
         break;
#ifdef HAVE_MULTIMAN
      case INGAME_MENU_RETURN_TO_MULTIMAN:
         if (input & (1ULL << RMENU_DEVICE_NAV_B))
         {
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN_MULTIMAN);
            return -1;
         }
         break;
#endif
      case INGAME_MENU_QUIT_RETROARCH:
         if (input & (1ULL << RMENU_DEVICE_NAV_B))
         {
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            return -1;
         }
         break;
   }

   return 0;
}

static int select_setting(uint8_t menu_type, uint64_t input)
{
   static uint8_t first_setting = FIRST_VIDEO_SETTING;
   static uint8_t selected = 0;
   static uint8_t page_number = 0;
   uint8_t items_pages[SETTING_LAST] = {0};
   uint8_t max_settings = 0;

   font_params_t font_parms = {0};

   int ret = 0;

   switch (menu_type)
   {
      case GENERAL_VIDEO_MENU:
         first_setting = FIRST_VIDEO_SETTING;
         max_settings = MAX_NO_OF_VIDEO_SETTINGS;
         break;
      case GENERAL_AUDIO_MENU:
         first_setting = FIRST_AUDIO_SETTING;
         max_settings = MAX_NO_OF_AUDIO_SETTINGS;
         break;
      case EMU_GENERAL_MENU:
         first_setting = FIRST_EMU_SETTING;
         max_settings = MAX_NO_OF_EMU_SETTINGS;
         break;
      case EMU_VIDEO_MENU:
         first_setting = FIRST_EMU_VIDEO_SETTING;
         max_settings = MAX_NO_OF_EMU_VIDEO_SETTINGS;
         break;
      case EMU_AUDIO_MENU:
         first_setting = FIRST_EMU_AUDIO_SETTING;
         max_settings = MAX_NO_OF_EMU_AUDIO_SETTINGS;
         break;
      case PATH_MENU:
         first_setting = FIRST_PATH_SETTING;
         max_settings = MAX_NO_OF_PATH_SETTINGS;
         break;
      case CONTROLS_MENU:
         first_setting = FIRST_CONTROLS_SETTING_PAGE_1;
         max_settings = MAX_NO_OF_CONTROLS_SETTINGS;
         break;
   }

   rmenu_default_positions_t default_pos;

   menu_set_default_pos(&default_pos);

   char msg[256];
   uint8_t i = 0;
   uint8_t j = 0;
   uint8_t item_page = 0;

   for(i = first_setting; i < max_settings; i++)
   {
      item item;
      populate_setting_item(&item, i);

      if (!(j < default_pos.entries_per_page))
      {
         j = 0;
         item_page++;
      }

      item.page = item_page;
      items_pages[i] = item_page;
      j++;

      if (item.page != page_number)
         continue;

      default_pos.starting_y_position += default_pos.y_position_increment;

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.starting_y_position;
      font_parms.scale = default_pos.variable_font_size;
      font_parms.color = selected == item.enum_id ? YELLOW : WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, item.text, &font_parms);

      font_parms.x = default_pos.x_position_center;
      font_parms.color = WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, item.setting_text, &font_parms);

      if (item.enum_id != selected)
         continue;

      rarch_position_t position = {0};
      position.x = default_pos.x_position;
      position.y = default_pos.starting_y_position;

      rmenu_gfx_draw_panel(&position);

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.comment_y_position;
      font_parms.scale = default_pos.font_size;
      font_parms.color = WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, item.comment, &font_parms);
   }

   if (input & (1ULL << RMENU_DEVICE_NAV_UP))
   {
      if (selected == first_setting)
         selected = max_settings-1;
      else
         selected--;

      if (items_pages[selected] != page_number)
         page_number = items_pages[selected];
   }
      
   if (input & (1ULL << RMENU_DEVICE_NAV_DOWN))
   {
      selected++;

      if (selected >= max_settings)
         selected = first_setting; 
      if (items_pages[selected] != page_number)
         page_number = items_pages[selected];
   }

   /* back to ROM menu if CIRCLE is pressed */
   if ((input & (1ULL << RMENU_DEVICE_NAV_L1)) || (input & (1ULL << RMENU_DEVICE_NAV_A)))
   {
      switch(menu_type)
      {
         case GENERAL_VIDEO_MENU:
            break;
         case GENERAL_AUDIO_MENU:
            selected = FIRST_VIDEO_SETTING;
            break;
         case EMU_GENERAL_MENU:
            selected = FIRST_AUDIO_SETTING;
            break;
         case EMU_VIDEO_MENU:
            selected = FIRST_EMU_SETTING;
            break;
         case EMU_AUDIO_MENU:
            selected = FIRST_EMU_VIDEO_SETTING;
            break;
         case PATH_MENU:
            selected = FIRST_EMU_AUDIO_SETTING;
            break;
         case CONTROLS_MENU:
            selected = FIRST_PATH_SETTING;
            break;
         default:
            break;
      }
      menu_stack_pop();
      page_number = 0;
   }
   else if (input & (1ULL << RMENU_DEVICE_NAV_R1))
   {
      switch(menu_type)
      {
         case GENERAL_VIDEO_MENU:
            if (menu_type == GENERAL_VIDEO_MENU)
               selected = FIRST_AUDIO_SETTING;
         case GENERAL_AUDIO_MENU:
            if (menu_type == GENERAL_AUDIO_MENU)
               selected = FIRST_EMU_SETTING;
         case EMU_GENERAL_MENU:
            if (menu_type == EMU_GENERAL_MENU)
               selected = FIRST_EMU_VIDEO_SETTING;
         case EMU_VIDEO_MENU:
            if (menu_type == EMU_VIDEO_MENU)
               selected = FIRST_EMU_AUDIO_SETTING;
         case EMU_AUDIO_MENU:
            if (menu_type == EMU_AUDIO_MENU)
               selected = FIRST_PATH_SETTING;
         case PATH_MENU:
            if (menu_type == PATH_MENU)
               selected = FIRST_CONTROLS_SETTING_PAGE_1;

            menu_stack_push(menu_type + 1);
            page_number = 0;
            break;
         case CONTROLS_MENU:
         default:
            break;
      }
   }

   ret = set_setting_action(menu_type, selected, input);

   if (ret != 0)
      return ret;

   display_menubar(menu_type);

   struct platform_bind key_label_l3 = {0};
   struct platform_bind key_label_r3 = {0};
   struct platform_bind key_label_start = {0};

   strlcpy(key_label_l3.desc, "Unknown", sizeof(key_label_l3.desc));
   key_label_l3.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_L3;
   strlcpy(key_label_r3.desc, "Unknown", sizeof(key_label_r3.desc));
   key_label_r3.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_R3;
   strlcpy(key_label_start.desc, "Unknown", sizeof(key_label_start.desc));
   key_label_start.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_START;

   if (driver.input->set_keybinds)
   {
      driver.input->set_keybinds(&key_label_l3, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
      driver.input->set_keybinds(&key_label_r3, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
      driver.input->set_keybinds(&key_label_start, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
   }

   snprintf(msg, sizeof(msg), "[%s] + [%s] - Resume game", key_label_l3.desc, key_label_r3.desc);

   font_parms.x = default_pos.x_position;
   font_parms.y = default_pos.comment_two_y_position;
   font_parms.scale = default_pos.font_size;
   font_parms.color = YELLOW;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   snprintf(msg, sizeof(msg), "[%s] - Reset to default", key_label_start.desc);
   font_parms.y = default_pos.comment_two_y_position + (default_pos.y_position_increment * 1);

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   return 0;
}

static int select_rom(uint8_t menu_type, uint64_t input)
{
   font_params_t font_parms = {0};
   char msg[128];
   rmenu_default_positions_t default_pos;
   filebrowser_t *filebrowser = browser;

   struct platform_bind key_label_b = {0};
   struct platform_bind key_label_l3 = {0};
   struct platform_bind key_label_r3 = {0};
   struct platform_bind key_label_select = {0};

   strlcpy(key_label_b.desc, "Unknown", sizeof(key_label_b.desc));
   key_label_b.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_B;
   strlcpy(key_label_l3.desc, "Unknown", sizeof(key_label_l3.desc));
   key_label_l3.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_L3;
   strlcpy(key_label_r3.desc, "Unknown", sizeof(key_label_r3.desc));
   key_label_r3.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_R3;
   strlcpy(key_label_select.desc, "Unknown", sizeof(key_label_select.desc));
   key_label_select.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT;

   if (driver.input->set_keybinds)
   {
      driver.input->set_keybinds(&key_label_l3, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
      driver.input->set_keybinds(&key_label_r3, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
      driver.input->set_keybinds(&key_label_select, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
      driver.input->set_keybinds(&key_label_b, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
   }

   menu_set_default_pos(&default_pos);

   browser_update(filebrowser, input, g_extern.system.valid_extensions);

   if (input & (1ULL << RMENU_DEVICE_NAV_SELECT))
      menu_stack_push(GENERAL_VIDEO_MENU);
   else if (input & (1ULL << RMENU_DEVICE_NAV_B))
   {
      if (filebrowser_iterate(filebrowser, FILEBROWSER_ACTION_PATH_ISDIR))
      {
         bool ret = filebrowser_iterate(filebrowser, FILEBROWSER_ACTION_OK);

         if (!ret)
            msg_queue_push(g_extern.msg_queue, "ERROR - Failed to open directory.", 1, 180);
      }
      else
      {
         strlcpy(g_extern.fullpath, filebrowser_get_current_path(filebrowser), sizeof(g_extern.fullpath));
         g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);
      }
   }
   else if (input & (1ULL << RMENU_DEVICE_NAV_L1))
   {
      const char * drive_map = menu_drive_mapping_previous();
      if (drive_map != NULL)
      {
         filebrowser_set_root_and_ext(filebrowser, g_extern.system.valid_extensions, drive_map);
         browser_update(filebrowser, 1ULL << RMENU_DEVICE_NAV_B, g_extern.system.valid_extensions);
      }
   }
   else if (input & (1ULL << RMENU_DEVICE_NAV_R1))
   {
      const char * drive_map = menu_drive_mapping_next();
      if (drive_map != NULL)
      {
         filebrowser_set_root_and_ext(filebrowser, g_extern.system.valid_extensions, drive_map);
         browser_update(filebrowser, 1ULL << RMENU_DEVICE_NAV_B, g_extern.system.valid_extensions);
      }
   }

   if (filebrowser_iterate(filebrowser, FILEBROWSER_ACTION_PATH_ISDIR))
      snprintf(msg, sizeof(msg), "INFO - Press [%s] to enter the directory.", key_label_b.desc);
   else
      snprintf(msg, sizeof(msg), "INFO - Press [%s] to load the game.", key_label_b.desc);

   font_parms.x = default_pos.x_position;
   font_parms.y = default_pos.comment_y_position;
   font_parms.scale = default_pos.font_size;
   font_parms.color = WHITE;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   display_menubar(menu_type);

   snprintf(msg, sizeof(msg), "[%s] + [%s] - resume game", key_label_l3.desc, key_label_r3.desc);

   font_parms.y = default_pos.comment_two_y_position;
   font_parms.color = YELLOW;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   snprintf(msg, sizeof(msg), "[%s] - Settings", key_label_select.desc);

   font_parms.y = default_pos.comment_two_y_position + (default_pos.y_position_increment * 1);

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

   browser_render(filebrowser);

   return 0;
}

static int ingame_menu_resize(uint8_t menu_type, uint64_t input)
{
   (void)menu_type;
   font_params_t font_parms = {0};

   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   rmenu_default_positions_t default_pos;
   menu_set_default_pos(&default_pos);

   g_settings.video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;
   
   if (driver.video_poke->set_aspect_ratio)
      driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);

   if (input & (1ULL << RMENU_DEVICE_NAV_LEFT_ANALOG_L))
   {
#ifdef _XBOX
      if (g_extern.console.screen.viewports.custom_vp.x >= 4)
#endif
         g_extern.console.screen.viewports.custom_vp.x -= 4;
   }
   else if (input & (1ULL << RMENU_DEVICE_NAV_LEFT) && (input & ~(1ULL << RMENU_DEVICE_NAV_LEFT_ANALOG_L)))
   {
#ifdef _XBOX
      if (g_extern.console.screen.viewports.custom_vp.x > 0)
#endif
         g_extern.console.screen.viewports.custom_vp.x -= 1;
   }

   if (input & (1ULL << RMENU_DEVICE_NAV_RIGHT_ANALOG_L))
      g_extern.console.screen.viewports.custom_vp.x += 4;
   else if (input & (1ULL << RMENU_DEVICE_NAV_RIGHT) && (input & ~(1ULL << RMENU_DEVICE_NAV_RIGHT_ANALOG_L)))
      g_extern.console.screen.viewports.custom_vp.x += 1;

   if (input & (1ULL << RMENU_DEVICE_NAV_UP_ANALOG_L))
      g_extern.console.screen.viewports.custom_vp.y += 4;
   else if (input & (1ULL << RMENU_DEVICE_NAV_UP) && (input & ~(1ULL << RMENU_DEVICE_NAV_UP_ANALOG_L)))
      g_extern.console.screen.viewports.custom_vp.y += 1;

   if (input & (1ULL << RMENU_DEVICE_NAV_DOWN_ANALOG_L))
   {
#ifdef _XBOX
      if (g_extern.console.screen.viewports.custom_vp.y >= 4)
#endif
         g_extern.console.screen.viewports.custom_vp.y -= 4;
   }
   else if (input & (1ULL << RMENU_DEVICE_NAV_DOWN) && (input & ~(1ULL << RMENU_DEVICE_NAV_DOWN_ANALOG_L)))
   {
#ifdef _XBOX
      if (g_extern.console.screen.viewports.custom_vp.y > 0)
#endif
         g_extern.console.screen.viewports.custom_vp.y -= 1;
   }

   if (input & (1ULL << RMENU_DEVICE_NAV_LEFT_ANALOG_R))
      g_extern.console.screen.viewports.custom_vp.width -= 4;
   else if (input & (1ULL << RMENU_DEVICE_NAV_L1) && (input && ~(1ULL << RMENU_DEVICE_NAV_LEFT_ANALOG_R)))
      g_extern.console.screen.viewports.custom_vp.width -= 1;

   if (input & (1ULL << RMENU_DEVICE_NAV_RIGHT_ANALOG_R))
      g_extern.console.screen.viewports.custom_vp.width += 4;
   else if (input & (1ULL << RMENU_DEVICE_NAV_R1) && (input & ~(1ULL << RMENU_DEVICE_NAV_RIGHT_ANALOG_R)))
      g_extern.console.screen.viewports.custom_vp.width += 1;

   if (input & (1ULL << RMENU_DEVICE_NAV_UP_ANALOG_R))
      g_extern.console.screen.viewports.custom_vp.height += 4;
   else if (input & (1ULL << RMENU_DEVICE_NAV_L2) && (input & ~(1ULL << RMENU_DEVICE_NAV_UP_ANALOG_R)))
      g_extern.console.screen.viewports.custom_vp.height += 1;

   if (input & (1ULL << RMENU_DEVICE_NAV_DOWN_ANALOG_R))
      g_extern.console.screen.viewports.custom_vp.height -= 4;
   else if (input & (1ULL << RMENU_DEVICE_NAV_R2) && (input & ~(1ULL << RMENU_DEVICE_NAV_DOWN_ANALOG_R)))
      g_extern.console.screen.viewports.custom_vp.height -= 1;

   if (input & (1ULL << RMENU_DEVICE_NAV_X))
   {
      g_extern.console.screen.viewports.custom_vp.x = 0;
      g_extern.console.screen.viewports.custom_vp.y = 0;
      g_extern.console.screen.viewports.custom_vp.width = device_ptr->win_width;
      g_extern.console.screen.viewports.custom_vp.height = device_ptr->win_height;
   }

   if (input & (1ULL << RMENU_DEVICE_NAV_A))
   {
      menu_stack_pop();
      g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_DRAW);
   }

   if ((input & (1ULL << RMENU_DEVICE_NAV_Y)))
   {
      if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_DRAW))
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_DRAW);
      else
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_DRAW);
   }

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_DRAW))
   {
      char viewport_x[32];
      char viewport_y[32];
      char viewport_w[32];
      char viewport_h[32];
      char msg[128];
      struct platform_bind key_label_b = {0};
      struct platform_bind key_label_a = {0};
      struct platform_bind key_label_y = {0};
      struct platform_bind key_label_x = {0};
      struct platform_bind key_label_l1 = {0};
      struct platform_bind key_label_l2 = {0};
      struct platform_bind key_label_r1 = {0};
      struct platform_bind key_label_r2 = {0};
      struct platform_bind key_label_select = {0};
      struct platform_bind key_label_dpad_left = {0};
      struct platform_bind key_label_dpad_right = {0};
      struct platform_bind key_label_dpad_up = {0};
      struct platform_bind key_label_dpad_down = {0};
      
      strlcpy(key_label_b.desc, "Unknown", sizeof(key_label_b.desc));
      key_label_b.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_B;
      strlcpy(key_label_a.desc, "Unknown", sizeof(key_label_a.desc));
      key_label_a.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_A;
      strlcpy(key_label_x.desc, "Unknown", sizeof(key_label_x.desc));
      key_label_x.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_X;
      strlcpy(key_label_y.desc, "Unknown", sizeof(key_label_y.desc));
      key_label_y.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_Y;
      strlcpy(key_label_l1.desc, "Unknown", sizeof(key_label_l1.desc));
      key_label_l1.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_L;
      strlcpy(key_label_r1.desc, "Unknown", sizeof(key_label_r1.desc));
      key_label_r1.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_R;
      strlcpy(key_label_l2.desc, "Unknown", sizeof(key_label_l2.desc));
      key_label_l2.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_L2;
      strlcpy(key_label_r2.desc, "Unknown", sizeof(key_label_r2.desc));
      key_label_r2.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_R2;
      strlcpy(key_label_select.desc, "Unknown", sizeof(key_label_select.desc));
      key_label_select.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT;
      strlcpy(key_label_dpad_left.desc, "Unknown", sizeof(key_label_dpad_left.desc));
      key_label_dpad_left.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT;
      strlcpy(key_label_dpad_right.desc, "Unknown", sizeof(key_label_dpad_left.desc));
      key_label_dpad_right.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT;
      strlcpy(key_label_dpad_up.desc, "Unknown", sizeof(key_label_dpad_up.desc));
      key_label_dpad_up.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_UP;
      strlcpy(key_label_dpad_down.desc, "Unknown", sizeof(key_label_dpad_down.desc));
      key_label_dpad_down.joykey = 1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN;

      if (driver.input->set_keybinds)
      {
         driver.input->set_keybinds(&key_label_l1, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_r1, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_l2, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_r2, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_select, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_b, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_a, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_y, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_x, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_dpad_left, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_dpad_right, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_dpad_up, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
         driver.input->set_keybinds(&key_label_dpad_down, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
      }

      display_menubar(menu_type);

      snprintf(viewport_x, sizeof(viewport_x), "Viewport X: #%d", g_extern.console.screen.viewports.custom_vp.x);
      snprintf(viewport_y, sizeof(viewport_y), "Viewport Y: #%d", g_extern.console.screen.viewports.custom_vp.y);
      snprintf(viewport_w, sizeof(viewport_w), "Viewport W: #%d", g_extern.console.screen.viewports.custom_vp.width);
      snprintf(viewport_h, sizeof(viewport_h), "Viewport H: #%d", g_extern.console.screen.viewports.custom_vp.height);

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.y_position;
      font_parms.scale = default_pos.font_size;
      font_parms.color = GREEN;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, viewport_x, &font_parms);

      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 1);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, viewport_y, &font_parms);

      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 2);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, viewport_w, &font_parms);

      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 3);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, viewport_h, &font_parms);

      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 4);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "CONTROLS:", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_dpad_left.desc);

      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 5);
      font_parms.color = WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = default_pos.x_position_center;
      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 5);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport X--", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_dpad_right.desc);

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 6);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = default_pos.x_position_center;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport X++", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_dpad_up.desc);
      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 7);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = default_pos.x_position_center;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport Y++", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_dpad_down.desc);

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 8);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = default_pos.x_position_center;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport Y--", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_l1.desc);

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 9);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = default_pos.x_position_center;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport W--", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_r1.desc);

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 10);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = default_pos.x_position_center;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport W++", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_l2.desc);

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 11);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = default_pos.x_position_center;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport H++", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_r2.desc);

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 12);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = default_pos.x_position_center;
      
      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Viewport H--", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_x.desc);

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 13);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = default_pos.x_position_center;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Reset To Defaults", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_y.desc);

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 14);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);

      font_parms.x = default_pos.x_position_center;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, "- Show Game", &font_parms);

      snprintf(msg, sizeof(msg), "[%s]", key_label_a.desc);
      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.y_position + (default_pos.y_position_increment * 15);

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(device_ptr, msg, &font_parms);

      font_parms.x = default_pos.x_position_center;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(device_ptr, "- Go back", &font_parms);

      snprintf(msg, sizeof(msg), "Press [%s] to reset to defaults.", key_label_x.desc);
      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.comment_y_position;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);
   }

   return 0;
}

static int ingame_menu_screenshot(uint8_t menu_type, uint64_t input)
{
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_DRAW);

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_INGAME))
   {
      if (input & (1ULL << RMENU_DEVICE_NAV_A))
      {
         menu_stack_pop();
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_DRAW);
      }

      if (input & (1ULL << RMENU_DEVICE_NAV_B))
         if (device_ptr->ctx_driver->rmenu_screenshot_dump)
            device_ptr->ctx_driver->rmenu_screenshot_dump(NULL);
   }

   return 0;
}

static int ingame_menu(uint8_t menu_type, uint64_t input)
{
   static uint8_t first_setting = FIRST_INGAME_MENU_SETTING;
   static uint8_t selected = FIRST_INGAME_MENU_SETTING;
   static uint8_t page_number = 0;
   uint8_t items_pages[SETTING_LAST] = {0};
   uint8_t max_settings = MAX_NO_OF_INGAME_MENU_SETTINGS;

   int ret = 0;
   font_params_t font_parms = {0};

   rmenu_default_positions_t default_pos;
   menu_set_default_pos(&default_pos);

   uint8_t i = 0;
   uint8_t j = 0;
   uint8_t item_page = 0;

   for(i = first_setting; i < max_settings; i++)
   {
      item item;
      populate_setting_item(&item, i);

      if (!(j < default_pos.entries_per_page))
      {
         j = 0;
         item_page++;
      }

      item.page = item_page;
      items_pages[i] = item_page;
      j++;

      if (item.page != page_number)
         continue;

      default_pos.starting_y_position += default_pos.y_position_increment;

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.starting_y_position;
      font_parms.scale = default_pos.variable_font_size;
      font_parms.color = selected == item.enum_id ? YELLOW : WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, item.text, &font_parms);

      font_parms.x = default_pos.x_position_center;
      font_parms.color = WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, item.setting_text, &font_parms);

      if (item.enum_id != selected)
         continue;

      rarch_position_t position = {0};
      position.x = default_pos.x_position;
      position.y = default_pos.starting_y_position;

      rmenu_gfx_draw_panel(&position);

      font_parms.x = default_pos.x_position;
      font_parms.y = default_pos.comment_y_position;
      font_parms.scale = default_pos.font_size;
      font_parms.color = WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, item.comment, &font_parms);
   }

   if (input & (1ULL << RMENU_DEVICE_NAV_A))
   {
      g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
      g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
      return -1;
   }

   if (input & (1ULL << RMENU_DEVICE_NAV_UP))
   {
      if (selected == first_setting)
         selected = max_settings-1;
      else
         selected--;

      if (items_pages[selected] != page_number)
         page_number = items_pages[selected];
   }
      
   if (input & (1ULL << RMENU_DEVICE_NAV_DOWN))
   {
      selected++;

      if (selected >= max_settings)
         selected = first_setting; 
      if (items_pages[selected] != page_number)
         page_number = items_pages[selected];
   }

   ret = set_setting_action(menu_type, selected, input);

   if (ret != 0)
      return ret;

   display_menubar(menu_type);

   return 0;
}

static int menu_input_process(uint8_t menu_type, uint64_t old_state)
{
   bool quit = false;
   bool resize = false;
   unsigned width;
   unsigned height;
   unsigned frame_count;

   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME))
   {
      if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
      {
         char tmp[PATH_MAX];
         char str[PATH_MAX];

         fill_pathname_base(tmp, g_extern.fullpath, sizeof(tmp));
         snprintf(str, sizeof(str), "INFO - Loading %s...", tmp);
         msg_queue_push(g_extern.msg_queue, str, 1, 1);
      }

      g_extern.lifecycle_mode_state |= (1ULL << MODE_INIT);
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME);
      return -1;
   }

   if (!(g_extern.frame_count < g_extern.delay_timer[0]))
   {
      bool return_to_game_enable = (((old_state & (1ULL << RMENU_DEVICE_NAV_L3)) && (old_state & (1ULL << RMENU_DEVICE_NAV_R3)) && g_extern.main_is_init));

      if (return_to_game_enable)
      {
         int ret = -1;
         if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_INGAME))
         {
            g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
            ret = 0;
         }

         g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
         return ret;
      }
   }

   frame_count = 0;
   device_ptr->ctx_driver->check_window(&quit, &resize, &width, &height, frame_count);

   if (quit)
   {
      g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
      return -1;
   }

   return 0;
}

/*============================================================
  RMENU API
  ============================================================ */


void menu_init(void)
{
   browser    = (filebrowser_t*)filebrowser_init(g_extern.console.main_wrap.default_rom_startup_dir, g_extern.system.valid_extensions);
   tmpBrowser = (filebrowser_t*)filebrowser_init(default_paths.filesystem_root_dir, "");

   filebrowser_set_root_and_ext(browser, g_extern.system.valid_extensions, g_extern.console.main_wrap.default_rom_startup_dir);
   filebrowser_set_root_and_ext(tmpBrowser, NULL, default_paths.filesystem_root_dir);

   menu_stack_push(FILE_BROWSER_MENU);

   rmenu_gfx_init();
}


void menu_free(void)
{
   rmenu_gfx_free();

   filebrowser_free(browser);
   filebrowser_free(tmpBrowser);
}

bool menu_iterate(void)
{
   const char *msg;
   static uint64_t input = 0;
   static uint64_t old_state = 0;
   font_params_t font_parms = {0};
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_PREINIT))
   {
      if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_INGAME))
         menu_stack_push(INGAME_MENU);

      g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_DRAW);

#ifndef __CELLOS_LV2__
      rmenu_gfx_init();
#endif

      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_PREINIT);
   }

   g_extern.frame_count++;


   rmenu_default_positions_t default_pos;
   menu_set_default_pos(&default_pos);

   if ((g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_LOW_RAM_MODE_ENABLE)))
   {
#if defined(HAVE_OPENGL)
      glClear(GL_COLOR_BUFFER_BIT);
#elif defined(HAVE_D3D8) || defined(HAVE_D3D9)
      xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
      LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->d3d_render_device;
      d3dr->Clear(0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
#endif
   }
   else
   {
      if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_DRAW))
      {
         if (driver.video_poke->set_blend)
            driver.video_poke->set_blend(driver.video_data, true);
      }

      rarch_render_cached_frame();
      rmenu_gfx_frame(driver.video_data);
   }

   //first button input frame
   uint64_t input_state_first_frame = 0;
   uint64_t input_state = 0;
   static bool first_held = false;

   driver.input->poll(NULL);

   for (unsigned i = 0; i < RMENU_DEVICE_NAV_LAST; i++)
      input_state |= driver.input->input_state(NULL, rmenu_nav_binds, 0,
            RETRO_DEVICE_JOYPAD, 0, i) ? (1ULL << i) : 0;

   //set first button input frame as trigger
   input = input_state & ~(old_state);
   //hold onto first button input frame
   input_state_first_frame = input_state;          

   //second button input frame
   input_state = 0;
   driver.input->poll(NULL);

   for (unsigned i = 0; i < RMENU_DEVICE_NAV_LAST; i++)
   {
      input_state |= driver.input->input_state(NULL, rmenu_nav_binds, 0,
            RETRO_DEVICE_JOYPAD, 0, i) ? (1ULL << i) : 0;
   }

   bool analog_sticks_pressed = (input_state & (1ULL << RMENU_DEVICE_NAV_LEFT_ANALOG_L)) || (input_state & (1ULL << RMENU_DEVICE_NAV_RIGHT_ANALOG_L)) || (input_state & (1ULL << RMENU_DEVICE_NAV_UP_ANALOG_L)) || (input_state & (1ULL << RMENU_DEVICE_NAV_DOWN_ANALOG_L)) || (input_state & (1ULL << RMENU_DEVICE_NAV_LEFT_ANALOG_R)) || (input_state & (1ULL << RMENU_DEVICE_NAV_RIGHT_ANALOG_R)) || (input_state & (1ULL << RMENU_DEVICE_NAV_UP_ANALOG_R)) || (input_state & (1ULL << RMENU_DEVICE_NAV_DOWN_ANALOG_R));
   bool shoulder_buttons_pressed = ((input_state & (1ULL << RMENU_DEVICE_NAV_L2)) || (input_state & (1ULL << RMENU_DEVICE_NAV_R2)));
   bool do_held = analog_sticks_pressed || shoulder_buttons_pressed;

   if (do_held)
   {
      if (!first_held)
      {
         first_held = true;
         g_extern.delay_timer[1] = g_extern.frame_count + 7;
      }

      if (!(g_extern.frame_count < g_extern.delay_timer[1]))
      {
         first_held = false;
         input = input_state; //second input frame set as current frame
      }
   }

   old_state = input_state_first_frame;

#ifdef HAVE_OSKUTIL
   if (rmenu_state.osk_init != NULL)
   {
      if (rmenu_state.osk_init(&rmenu_state))
         rmenu_state.osk_init = NULL;
   }

   if (rmenu_state.osk_callback != NULL)
   {
      if (rmenu_state.osk_callback(&rmenu_state))
         rmenu_state.osk_callback = NULL;
   }
#endif

   int input_entry_ret = 0;
   int input_process_ret = 0;

   unsigned menu_id = menu_stack_enum_array[stack_idx - 1];

   switch(menu_id)
   {
      case INGAME_MENU:
         input_entry_ret = ingame_menu(menu_id, input);
         input_process_ret = menu_input_process(menu_id, old_state);
         break;
      case INGAME_MENU_RESIZE:
         input_entry_ret = ingame_menu_resize(menu_id, input);
         input_process_ret = menu_input_process(menu_id, old_state);
         break;
      case INGAME_MENU_SCREENSHOT:
         input_entry_ret = ingame_menu_screenshot(menu_id, input);
         input_process_ret = menu_input_process(menu_id, old_state);
         break;
      case FILE_BROWSER_MENU:
         input_entry_ret = select_rom(menu_id, input);
         input_process_ret = menu_input_process(menu_id, old_state);
         break;
      case LIBRETRO_CHOICE:
      case PRESET_CHOICE:
      case INPUT_PRESET_CHOICE:
      case SHADER_CHOICE:
      case BORDER_CHOICE:
         input_entry_ret = select_file(menu_id, input);
         input_process_ret = menu_input_process(menu_id, old_state);
         break;
      case PATH_DEFAULT_ROM_DIR_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_SRAM_DIR_CHOICE:
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SYSTEM_DIR_CHOICE:
         input_entry_ret = select_directory(menu_id, input);
         input_process_ret = menu_input_process(menu_id, old_state);
         break;
      case GENERAL_VIDEO_MENU:
      case GENERAL_AUDIO_MENU:
      case EMU_GENERAL_MENU:
      case EMU_VIDEO_MENU:
      case EMU_AUDIO_MENU:
      case PATH_MENU:
      case CONTROLS_MENU:
         input_entry_ret = select_setting(menu_id, input);
         input_process_ret = menu_input_process(menu_id, old_state);
         break;
   }

   msg = msg_queue_pull(g_extern.msg_queue);

   font_parms.x = default_pos.msg_queue_x_position;
   font_parms.y = default_pos.msg_queue_y_position;
   font_parms.scale = default_pos.msg_queue_font_size;
   font_parms.color = WHITE;

   if (msg && (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW)))
   {
      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);
   }

   device_ptr->ctx_driver->swap_buffers();

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_DRAW))
   {
      if (driver.video_poke->set_blend)
         driver.video_poke->set_blend(driver.video_data, false);
   }

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_INGAME_EXIT) &&
         g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_INGAME))
   {
      menu_stack_pop();
      g_extern.lifecycle_mode_state &= ~((1ULL << MODE_MENU_INGAME) | (1ULL << MODE_MENU_INGAME_EXIT));
   }

   if (input_entry_ret != 0 || input_process_ret != 0)
      goto deinit;

   return true;

deinit:
   // set a timer delay so that we don't instantly switch back to the menu when
   // press and holding L3 + R3 in the emulation loop (lasts for 30 frame ticks)
   if (!(g_extern.lifecycle_state & (1ULL << RARCH_FRAMEADVANCE)))
      g_extern.delay_timer[0] = g_extern.frame_count + 30;

   g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_DRAW);

#ifdef _XBOX1
   rmenu_gfx_free();
#endif

   return false;
}
