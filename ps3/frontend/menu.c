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

#include <sdk_version.h>
#include <cell/sysmodule.h>
#include <sysutil/sysutil_screenshot.h>
#include <cell/dbgfont.h>

#if(CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif

#include "../ps3_input.h"
#include "../../console/fileio/file_browser.h"

#include "../../console/rarch_console.h"
#include "../../console/rarch_console_rom_ext.h"
#include "../../console/rarch_console_input.h"
#include "../../console/rarch_console_config.h"
#include "../../console/rarch_console_settings.h"
#include "../../console/rarch_console_rsound.h"
#include "../../console/rarch_console_video.h"

#ifdef HAVE_ZLIB
#include "../../console/rarch_console_rzlib.h"
#endif

#include "../../gfx/gl_common.h"
#include "../../gfx/gl_font.h"
#include "../../gfx/gfx_context.h"
#include "../../gfx/context/ps3_ctx.h"
#include "../../gfx/shader_cg.h"

#include "../../file.h"
#include "../../general.h"

#include "menu.h"
#include "menu-entries.h"

#define NUM_ENTRY_PER_PAGE 17
#define INPUT_SCALE 2
#define MENU_ITEM_SELECTED(index) (menuitem_colors[index])

menu menuStack[10];
int menuStackindex = 0;
static bool set_libretro_core_as_launch;

filebrowser_t browser;
filebrowser_t tmpBrowser;
unsigned set_shader = 0;
static unsigned currently_selected_controller_menu = 0;
static char strw_buffer[PATH_MAX];
char core_text[256];

typedef enum {
   SETTINGS_ACTION_DOWN,
   SETTINGS_ACTION_UP,
   SETTINGS_ACTION_TAB_PREVIOUS,
   SETTINGS_ACTION_TAB_NEXT,
   SETTINGS_ACTION_NOOP
} settings_action_t;

typedef enum {
   MENU_ROMSELECT_ACTION_OK,
   MENU_ROMSELECT_ACTION_GOTO_SETTINGS,
   MENU_ROMSELECT_ACTION_NOOP,
} menu_romselect_action_t;

static void set_setting_label_write_on_or_off(item *items, bool cond, unsigned currentsetting)
{
   if(cond)
      snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "ON");
   else
      snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "OFF");
}

static void set_setting_label_color(item *items, bool cond, unsigned currentsetting)
{
   if(cond)
      items[currentsetting].text_color = GREEN;
   else
      items[currentsetting].text_color = ORANGE;
}

static void set_setting_label(menu * current_menu, item *items, unsigned currentsetting)
{
   char fname[PATH_MAX];
   (void)fname;

   switch(currentsetting)
   {
	   case SETTING_CHANGE_RESOLUTION:
                   set_setting_label_color(items,g_console.initial_resolution_id == g_console.supported_resolutions[g_console.current_resolution_index], currentsetting);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), ps3_get_resolution_label(g_console.supported_resolutions[g_console.current_resolution_index]));
		   break;
	   case SETTING_SHADER_PRESETS:
                   set_setting_label_color(items,true, currentsetting);
		   fill_pathname_base(fname, g_console.cgp_path, sizeof(fname));
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), fname);
		   break;
	   case SETTING_SHADER:
		   fill_pathname_base(fname, g_settings.video.cg_shader_path, sizeof(fname));
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "%s", fname);
                   set_setting_label_color(items,strcmp(g_settings.video.cg_shader_path, default_paths.shader_file) == 0, 
                   currentsetting);
		   break;
	   case SETTING_SHADER_2:
		   fill_pathname_base(fname, g_settings.video.second_pass_shader, sizeof(fname));
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "%s", fname);
                   set_setting_label_color(items,strcmp(g_settings.video.second_pass_shader, default_paths.shader_file) == 0,
                   currentsetting);
		   break;
	   case SETTING_FONT_SIZE:
                   set_setting_label_color(items,g_console.menu_font_size == 1.0f, currentsetting);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "%f", g_console.menu_font_size);
		   break;
	   case SETTING_KEEP_ASPECT_RATIO:
                   set_setting_label_color(items,g_console.aspect_ratio_index == ASPECT_RATIO_4_3, currentsetting);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), aspectratio_lut[g_console.aspect_ratio_index].name);
		   break;
	   case SETTING_HW_TEXTURE_FILTER:
                   set_setting_label_color(items,g_settings.video.smooth, currentsetting);
		   if(g_settings.video.smooth)
                      snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "Linear interpolation");
		   else
                      snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "Point filtering");
		   break;
	   case SETTING_HW_TEXTURE_FILTER_2:
                   set_setting_label_color(items,g_settings.video.second_pass_smooth, currentsetting);
		   if(g_settings.video.second_pass_smooth)
                      snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "Linear interpolation");
		   else
                      snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "Point filtering");
		   break;
	   case SETTING_SCALE_ENABLED:
                   set_setting_label_write_on_or_off(items, g_console.fbo_enabled, currentsetting);
                   set_setting_label_color(items,g_console.fbo_enabled, currentsetting);
		   break;
	   case SETTING_SCALE_FACTOR:
                   set_setting_label_color(items,g_settings.video.fbo_scale_x == 2.0f, currentsetting);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "%fx (X) / %fx (Y)", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);
		   snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), "INFO - [Custom Scaling Factor] is set to: '%fx (X) / %fx (Y)'.", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);
		   break;
	   case SETTING_HW_OVERSCAN_AMOUNT:
                   set_setting_label_color(items,g_console.overscan_amount == 0.0f, currentsetting);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "%f", g_console.overscan_amount);
		   break;
	   case SETTING_THROTTLE_MODE:
                   set_setting_label_write_on_or_off(items, g_console.throttle_enable, currentsetting);
                   set_setting_label_color(items,g_console.throttle_enable, currentsetting);
		   break;
	   case SETTING_TRIPLE_BUFFERING:
                   set_setting_label_write_on_or_off(items, g_console.triple_buffering_enable, currentsetting);
                   set_setting_label_color(items,g_console.triple_buffering_enable, currentsetting);
		   break;
	   case SETTING_ENABLE_SCREENSHOTS:
                   set_setting_label_write_on_or_off(items, g_console.screenshots_enable, currentsetting);
                   set_setting_label_color(items,g_console.screenshots_enable, currentsetting);
		   break;
	   case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
	   case SETTING_DEFAULT_VIDEO_ALL:
		   break;
	   case SETTING_SOUND_MODE:
		   switch(g_console.sound_mode)
		   {
                      case SOUND_MODE_NORMAL:
                         snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), 
                         "INFO - [Sound Output] is set to 'Normal' - normal audio output will be\nused.");
			 snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "Normal");
			 items[currentsetting].text_color = GREEN;
			 break;
                      case SOUND_MODE_RSOUND:
                         snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), 
                         "INFO - [Sound Output] is set to 'RSound' - the sound will be streamed over the\n network to the RSound audio server." );
			 snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "RSound");
			 items[currentsetting].text_color = ORANGE;
			 break;
                      case SOUND_MODE_HEADSET:
                         snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), 
                         "INFO - [Sound Output] is set to 'USB/Bluetooth Headset' - sound will\n be output through the headset");
			 snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "USB/Bluetooth Headset");
			 items[currentsetting].text_color = ORANGE;
			 break;
                      default:
                         break;
		   }
		   break;
	   case SETTING_RSOUND_SERVER_IP_ADDRESS:
                   set_setting_label_color(items,strcmp(g_settings.audio.device,"0.0.0.0") == 0, currentsetting);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), g_settings.audio.device);
		   break;
	   case SETTING_DEFAULT_AUDIO_ALL:
		   break;
	   case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
                   set_setting_label_color(items,g_extern.state_slot == 0, currentsetting);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "%d", g_extern.state_slot);
		   break;
		   /* emu-specific */
	   case SETTING_EMU_SHOW_INFO_MSG:
                   set_setting_label_write_on_or_off(items, g_console.info_msg_enable, currentsetting);
                   set_setting_label_color(items,g_console.info_msg_enable, currentsetting);
		   break;
	   case SETTING_EMU_REWIND_ENABLED:
                   set_setting_label_write_on_or_off(items, g_settings.rewind_enable, currentsetting);
		   if(g_settings.rewind_enable)
		   {
                      items[currentsetting].text_color = ORANGE;
		      snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), "INFO - [Rewind] feature is set to 'ON'. You can rewind the game in real-time.");
		   }
		   else
		   {
                      items[currentsetting].text_color = GREEN;
		      snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), "INFO - [Rewind] feature is set to 'OFF'.");
		   }
		   break;
           case SETTING_ZIP_EXTRACT:
                  set_setting_label_color(items,g_console.zip_extract_mode == ZIP_EXTRACT_TO_CURRENT_DIR, currentsetting);
                  switch(g_console.zip_extract_mode)
                  {
                     case ZIP_EXTRACT_TO_CURRENT_DIR:
			snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "Current dir");
                        snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), "INFO - [ZIP Extract Mode] is set to 'Current dir'.\nZIP files are extracted to the current directory.");
                        break;
                     case ZIP_EXTRACT_TO_CURRENT_DIR_AND_LOAD_FIRST_FILE:
			snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "Current dir and load first file");
                        snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), "INFO - [ZIP Extract Mode] is set to 'Current dir and load first file'.\nZIP files are extracted to the current directory, and the first game is automatically loaded.");
                        break;
                     case ZIP_EXTRACT_TO_CACHE_DIR:
			snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "Cache dir");
                        snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), "INFO - [ZIP Extract Mode] is set to 'Cache dir'.\nZIP files are extracted to the cache directory (dev_hdd1).");
                        break;
                  }
                  break;
	   case SETTING_RARCH_DEFAULT_EMU:
                  fill_pathname_base(fname, g_settings.libretro, sizeof(fname));
		  snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "%s", fname);

		  items[currentsetting].text_color = GREEN;
		   break;
	   case SETTING_EMU_AUDIO_MUTE:
                   set_setting_label_write_on_or_off(items, g_extern.audio_data.mute, currentsetting);
                   set_setting_label_color(items,!g_extern.audio_data.mute, currentsetting);
		   if(g_extern.audio_data.mute)
                      snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), "INFO - [Audio Mute] feature is set to 'ON'. The game audio will be muted.");
		   else
                      snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), "INFO - [Audio Mute] feature is set to 'OFF'.");
		   break;
	   case SETTING_ENABLE_CUSTOM_BGM:
                   set_setting_label_write_on_or_off(items, g_console.custom_bgm_enable, currentsetting);
                   set_setting_label_color(items,g_console.custom_bgm_enable, currentsetting);
		   break;
	   case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
                   set_setting_label_color(items,!(strcmp(g_console.default_rom_startup_dir, "/")), currentsetting);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), g_console.default_rom_startup_dir);
		   break;
	   case SETTING_PATH_SAVESTATES_DIRECTORY:
                   set_setting_label_color(items,!(strcmp(g_console.default_savestate_dir, default_paths.port_dir)), currentsetting);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), g_console.default_savestate_dir);
		   break;
	   case SETTING_PATH_SRAM_DIRECTORY:
                   set_setting_label_color(items,!(strcmp(g_console.default_sram_dir, default_paths.port_dir)), currentsetting);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), g_console.default_sram_dir);
		   break;
	   case SETTING_PATH_CHEATS:
                   set_setting_label_color(items,!(strcmp(g_settings.cheat_database, default_paths.port_dir)), currentsetting);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), g_settings.cheat_database);
		   break;
	   case SETTING_PATH_SYSTEM:
                   set_setting_label_color(items,!(strcmp(g_settings.system_directory, default_paths.system_dir)), currentsetting);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), g_settings.system_directory);
		   break;
	   case SETTING_ENABLE_SRAM_PATH:
                   set_setting_label_write_on_or_off(items, g_console.default_sram_dir_enable, currentsetting);
                   set_setting_label_color(items,!g_console.default_sram_dir_enable, currentsetting);
		   break;
	   case SETTING_ENABLE_STATE_PATH:
                   set_setting_label_write_on_or_off(items, g_console.default_savestate_dir_enable, currentsetting);
                   set_setting_label_color(items,!g_console.default_savestate_dir_enable, currentsetting);
		   break;
	   case SETTING_CONTROLS_SCHEME:
                   set_setting_label_color(items,strcmp(g_console.input_cfg_path,"") == 0, currentsetting);
		   snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), "INFO - Input scheme preset [%s] is selected.", g_console.input_cfg_path);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), g_console.input_cfg_path);
		   break;
	   case SETTING_CONTROLS_NUMBER:
                   set_setting_label_color(items,currently_selected_controller_menu == 0, currentsetting);
		   snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), "Controller %d is currently selected.", currently_selected_controller_menu+1);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "%d", currently_selected_controller_menu+1);
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
                      set_setting_label_color(items,g_settings.input.binds[currently_selected_controller_menu][currentsetting-(FIRST_CONTROL_BIND)].joykey == rarch_default_keybind_lut[currentsetting-FIRST_CONTROL_BIND], currentsetting);
		      const char * value = rarch_input_find_platform_key_label(g_settings.input.binds[currently_selected_controller_menu][currentsetting-(FIRST_CONTROL_BIND)].joykey);
		      unsigned id = currentsetting - FIRST_CONTROL_BIND;
		      snprintf(items[currentsetting].text, sizeof(items[currentsetting].text), rarch_input_get_default_keybind_name(id));
		      snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), "INFO - [%s] on the PS3 controller is mapped to action:\n[%s].", items[currentsetting].text, value);
		      snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), value);
		   }
		   break;
	   case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
	   case SETTING_CONTROLS_DEFAULT_ALL:
	   case SETTING_EMU_VIDEO_DEFAULT_ALL:
	   case SETTING_EMU_AUDIO_DEFAULT_ALL:
	   case SETTING_PATH_DEFAULT_ALL:
	   case SETTING_EMU_DEFAULT_ALL:
	   case SETTING_SAVE_SHADER_PRESET:
                   set_setting_label_color(items, current_menu->selected == currentsetting, currentsetting);
		   break;
	   default:
		   break;
   }
}

static void menu_stack_decrement(void)
{
   if(menuStackindex > 0)
      menuStackindex--;
}

menu *menu_stack_get_current_ptr (void)
{
   menu *current_menu = &menuStack[menuStackindex];
   return current_menu;
}

static void menu_stack_refresh (item *items, menu *current_menu)
{
   int page, i, j;
   float increment;

   page = 0;
   j = 0;
   increment = 0.16f;

   for(i = current_menu->first_setting; i < current_menu->max_settings; i++)
   {
      if(!(j < (NUM_ENTRY_PER_PAGE)))
      {
         j = 0;
	 increment = 0.16f;
	 page++;
      }

      items[i].text_xpos = 0.09f;
      items[i].text_ypos = increment; 
      items[i].page = page;
      set_setting_label(current_menu, items, i);
      increment += 0.03f;
      j++;
   }
}

static void menu_stack_push(item *items, unsigned menu_id)
{
   static bool first_push_do_not_increment = true;
   bool do_refresh = true;

   if(!first_push_do_not_increment)
      menuStackindex++;
   else
      first_push_do_not_increment = false;

   menu *current_menu = menu_stack_get_current_ptr();

   switch(menu_id)
   {
      case INGAME_MENU:
         strlcpy(current_menu->title, "Ingame Menu", sizeof(current_menu->title));
         current_menu->enum_id = menu_id;
         current_menu->selected = 0;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_INGAME_MENU;
         break;
      case INGAME_MENU_RESIZE:
         strlcpy(current_menu->title, "Resize Menu", sizeof(current_menu->title));
         current_menu->enum_id = INGAME_MENU_RESIZE;
         current_menu->selected = 0;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_INGAME_MENU;
         break;
      case INGAME_MENU_SCREENSHOT:
         strlcpy(current_menu->title, "Ingame Menu", sizeof(current_menu->title));
         current_menu->enum_id = menu_id;
         current_menu->selected = 0;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_INGAME_MENU;
         break;
      case FILE_BROWSER_MENU:
         strlcpy(current_menu->title, "File Browser", sizeof(current_menu->title));
         current_menu->enum_id = menu_id;
         current_menu->selected = 0;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         break;
      case LIBRETRO_CHOICE:
         strlcpy(current_menu->title, "Libretro cores", sizeof(current_menu->title));
         current_menu->enum_id = menu_id;
         current_menu->selected = 0;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         break;
      case PRESET_CHOICE:
         strlcpy(current_menu->title, "Shader presets", sizeof(current_menu->title));
         current_menu->enum_id = menu_id;
         current_menu->selected = 0;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         break;
      case INPUT_PRESET_CHOICE:
         strlcpy(current_menu->title, "Input presets", sizeof(current_menu->title));
         current_menu->enum_id = menu_id;
         current_menu->selected = 0;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         break;
      case SHADER_CHOICE:
         strlcpy(current_menu->title, "Shaders", sizeof(current_menu->title));
         current_menu->enum_id = menu_id;
         current_menu->selected = 0;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         break;
      case BORDER_CHOICE:
         strlcpy(current_menu->title, "Borders", sizeof(current_menu->title));
         current_menu->enum_id = menu_id;
         current_menu->selected = 0;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         break;
      case PATH_DEFAULT_ROM_DIR_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_SRAM_DIR_CHOICE:
      case PATH_CHEATS_DIR_CHOICE:
      case PATH_SYSTEM_DIR_CHOICE:
         strlcpy(current_menu->title, "Path Selection", sizeof(current_menu->title));
         current_menu->enum_id = menu_id;
         current_menu->selected = 0;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         break;
      case GENERAL_VIDEO_MENU:
         strlcpy(current_menu->title, "Video", sizeof(current_menu->title));
         current_menu->enum_id = GENERAL_VIDEO_MENU;
         current_menu->selected = FIRST_VIDEO_SETTING;
         current_menu->page = 0;
         current_menu->first_setting = FIRST_VIDEO_SETTING;
         current_menu->max_settings = MAX_NO_OF_VIDEO_SETTINGS;
         current_menu->category_id = CATEGORY_SETTINGS;
	 break;
      case GENERAL_AUDIO_MENU:
         strlcpy(current_menu->title, "Audio", sizeof(current_menu->title));
         current_menu->enum_id = GENERAL_AUDIO_MENU;
         current_menu->selected = FIRST_AUDIO_SETTING;
         current_menu->page = 0;
         current_menu->first_setting = FIRST_AUDIO_SETTING;
         current_menu->max_settings = MAX_NO_OF_AUDIO_SETTINGS;
         current_menu->category_id = CATEGORY_SETTINGS;
	 break;
      case EMU_GENERAL_MENU:
         strlcpy(current_menu->title, "Retro", sizeof(current_menu->title));
         current_menu->enum_id = EMU_GENERAL_MENU;
         current_menu->selected = FIRST_EMU_SETTING;
         current_menu->page = 0;
         current_menu->first_setting = FIRST_EMU_SETTING;
         current_menu->max_settings = MAX_NO_OF_EMU_SETTINGS;
         current_menu->category_id = CATEGORY_SETTINGS;
	 break;
      case EMU_VIDEO_MENU:
         strlcpy(current_menu->title, "Retro Video", sizeof(current_menu->title));
         current_menu->enum_id = EMU_VIDEO_MENU;
         current_menu->selected = FIRST_EMU_VIDEO_SETTING;
         current_menu->page = 0;
         current_menu->first_setting = FIRST_EMU_VIDEO_SETTING;
         current_menu->max_settings = MAX_NO_OF_EMU_VIDEO_SETTINGS;
         current_menu->category_id = CATEGORY_SETTINGS;
	 break;
      case EMU_AUDIO_MENU:
         strlcpy(current_menu->title, "Retro Audio", sizeof(current_menu->title));
         current_menu->enum_id = EMU_AUDIO_MENU;
         current_menu->selected = FIRST_EMU_AUDIO_SETTING;
         current_menu->page = 0;
         current_menu->first_setting = FIRST_EMU_AUDIO_SETTING;
         current_menu->max_settings = MAX_NO_OF_EMU_AUDIO_SETTINGS;
         current_menu->category_id = CATEGORY_SETTINGS;
	 break;
      case PATH_MENU:
         strlcpy(current_menu->title, "Path", sizeof(current_menu->title));
         current_menu->enum_id = PATH_MENU;
         current_menu->selected = FIRST_PATH_SETTING;
         current_menu->page = 0;
         current_menu->first_setting = FIRST_PATH_SETTING;
         current_menu->max_settings = MAX_NO_OF_PATH_SETTINGS;
         current_menu->category_id = CATEGORY_SETTINGS;
         break;
      case CONTROLS_MENU:
         strlcpy(current_menu->title, "Controls", sizeof(current_menu->title));
         current_menu->enum_id = CONTROLS_MENU;
         current_menu->selected = FIRST_CONTROLS_SETTING_PAGE_1;
         current_menu->page = 0;
         current_menu->first_setting = FIRST_CONTROLS_SETTING_PAGE_1;
         current_menu->max_settings = MAX_NO_OF_CONTROLS_SETTINGS;
         current_menu->category_id = CATEGORY_SETTINGS;
	 break;
       default:
         do_refresh = false;
         break;
   }

   if(do_refresh)
      menu_stack_refresh(items, current_menu);
}

//forward decls
extern const char *ps3_get_resolution_label(unsigned resolution);

static void display_menubar(menu *current_menu)
{
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;
   filebrowser_t *fb = &browser;
   char current_path[256], rarch_version[128];
   snprintf(current_path, sizeof(current_path), "PATH: %s", filebrowser_get_current_dir(fb));
   snprintf(rarch_version, sizeof(rarch_version), "v%s", PACKAGE_VERSION);

   switch(current_menu->enum_id)
   {
      case GENERAL_VIDEO_MENU:
	 render_msg_place_func(0.09f, 0.03f, 0.91f, WHITE, "NEXT ->");
         break;
      case GENERAL_AUDIO_MENU:
      case EMU_GENERAL_MENU:
      case EMU_VIDEO_MENU:
      case EMU_AUDIO_MENU:
      case PATH_MENU:
	 render_msg_place_func(0.09f, 0.03f, 0.91f, WHITE, "<- PREV | NEXT ->");
         break;
      case CONTROLS_MENU:
      case INGAME_MENU_RESIZE:
      case SHADER_CHOICE:
      case PRESET_CHOICE:
      case BORDER_CHOICE:
      case LIBRETRO_CHOICE:
      case INPUT_PRESET_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_DEFAULT_ROM_DIR_CHOICE:
      case PATH_CHEATS_DIR_CHOICE:
      case PATH_SRAM_DIR_CHOICE:
	 render_msg_place_func(0.09f, 0.03f, 0.91f, WHITE, "<- PREV");
         break;
      default:
         break;
   }

   switch(current_menu->enum_id)
   {
      case SHADER_CHOICE:
      case PRESET_CHOICE:
      case BORDER_CHOICE:
      case LIBRETRO_CHOICE:
      case INPUT_PRESET_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_DEFAULT_ROM_DIR_CHOICE:
      case PATH_CHEATS_DIR_CHOICE:
      case PATH_SRAM_DIR_CHOICE:
         fb = &tmpBrowser;
      case FILE_BROWSER_MENU:
         render_msg_place_func (0.09f, 0.09f, FONT_SIZE, YELLOW, current_path);
         break;
      default:
         break;
   }

   render_msg_place_func(0.09f, 0.05f, 1.4f, WHITE, current_menu->title);
   render_msg_place_func(0.3f, 0.06f, 0.82f, WHITE, core_text);
   render_msg_place_func(0.8f, 0.12f, 0.82f, WHITE, rarch_version);
   render_msg_post_func();
}

uint64_t state;
uint16_t input_st = 0;
static uint64_t old_state = 0;

static void control_update_wrap(uint64_t trigger_state)
{
   input_st = 0;
   input_ps3.poll(NULL);

   if (CTRL_LSTICK_DOWN(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);
   else if (CTRL_DOWN(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);
   else if (CTRL_LSTICK_UP(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_UP);
   else if (CTRL_UP(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_UP);
   else if (CTRL_RIGHT(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT);
   else if (CTRL_LSTICK_RIGHT(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT);
   else if (CTRL_LEFT(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_LEFT);
   else if (CTRL_LSTICK_LEFT(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_LEFT);
   else if (CTRL_R1(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_R);
   else if (CTRL_R2(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_R2);
   else if (CTRL_R3(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_R3);
   else if (CTRL_L1(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_L);
   else if (CTRL_L2(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_L2);
   else if (CTRL_L3(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_L3);
   else if (CTRL_SQUARE(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_Y);
   else if (CTRL_TRIANGLE(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_X);
   else if (CTRL_CIRCLE(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_A);
   else if (CTRL_CROSS(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_B);
   else if (CTRL_START(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_START);
   else if (CTRL_SELECT(trigger_state))
      input_st |= (1 << RETRO_DEVICE_ID_JOYPAD_SELECT);
}

static void browser_update(filebrowser_t * b, uint16_t inp_state, const char *extensions)
{
   filebrowser_action_t action = FILEBROWSER_ACTION_NOOP;

   if (inp_state & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
      action = FILEBROWSER_ACTION_DOWN;
   else if (inp_state & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
      action = FILEBROWSER_ACTION_UP;
   else if (inp_state & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      action = FILEBROWSER_ACTION_RIGHT;
   else if (inp_state & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
      action = FILEBROWSER_ACTION_LEFT;
   else if (inp_state & (1 << RETRO_DEVICE_ID_JOYPAD_R))
      action = FILEBROWSER_ACTION_SCROLL_DOWN;
   else if (inp_state & (1 << RETRO_DEVICE_ID_JOYPAD_R2))
      action = FILEBROWSER_ACTION_SCROLL_DOWN_SMOOTH;
   else if (inp_state & (1 << RETRO_DEVICE_ID_JOYPAD_L2))
      action = FILEBROWSER_ACTION_SCROLL_UP_SMOOTH;
   else if (inp_state & (1 << RETRO_DEVICE_ID_JOYPAD_L))
      action = FILEBROWSER_ACTION_SCROLL_UP;
   else if (inp_state & (1 << RETRO_DEVICE_ID_JOYPAD_A))
      action = FILEBROWSER_ACTION_CANCEL;
   else if (inp_state & (1 << RETRO_DEVICE_ID_JOYPAD_START))
   {
      action = FILEBROWSER_ACTION_RESET;
      filebrowser_set_root(b, "/");
      strlcpy(b->extensions, extensions, sizeof(b->extensions));
   }

   if(action != FILEBROWSER_ACTION_NOOP)
      filebrowser_iterate(b, action);
}

static void browser_render(filebrowser_t * b, float current_x, float current_y, float y_spacing)
{
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   unsigned file_count = b->current_dir.list->size;
   int current_index, page_number, page_base, i;
   float currentX, currentY, ySpacing;

   current_index = b->current_dir.ptr;
   page_number = current_index / NUM_ENTRY_PER_PAGE;
   page_base = page_number * NUM_ENTRY_PER_PAGE;

   currentX = current_x;
   currentY = current_y;
   ySpacing = y_spacing;

   for ( i = page_base; i < file_count && i < page_base + NUM_ENTRY_PER_PAGE; ++i)
   {
      char fname_tmp[256];
      fill_pathname_base(fname_tmp, b->current_dir.list->elems[i].data, sizeof(fname_tmp));
      currentY = currentY + ySpacing;
      render_msg_place_func(currentX, currentY, FONT_SIZE, i == current_index ? RED : b->current_dir.list->elems[i].attr.b ? GREEN : WHITE, fname_tmp);
      render_msg_post_func();
   }
   render_msg_post_func();
}

static void apply_scaling (unsigned init_mode)
{
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   switch(init_mode)
   {
      case FBO_DEINIT:
         gl_deinit_fbo(device_ptr);
	 break;
      case FBO_INIT:
	 gl_init_fbo(device_ptr, RARCH_SCALE_BASE * INPUT_SCALE,
			 RARCH_SCALE_BASE * INPUT_SCALE);
	 break;
      case FBO_REINIT:
	 gl_deinit_fbo(device_ptr);
	 gl_init_fbo(device_ptr, RARCH_SCALE_BASE * INPUT_SCALE,
			 RARCH_SCALE_BASE * INPUT_SCALE);
	 break;
   }
}

static void select_file(item *items, menu *current_menu)
{
   char extensions[256], object[256], comment[256], comment_two[256], path[PATH_MAX];
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   switch(current_menu->enum_id)
   {
      case SHADER_CHOICE:
	 strlcpy(extensions, EXT_SHADERS, sizeof(extensions));
	 strlcpy(object, "Shader", sizeof(object));
	 strlcpy(comment, "INFO - Select a shader from the menu by pressing the X button.", sizeof(comment));
	 break;
      case PRESET_CHOICE:
	 strlcpy(extensions, EXT_CGP_PRESETS, sizeof(extensions));
	 strlcpy(object, "Shader preset", sizeof(object));
	 strlcpy(comment, "INFO - Select a shader preset from the menu by pressing the X button.", sizeof(comment));
	 break;
      case INPUT_PRESET_CHOICE:
	 strlcpy(extensions, EXT_INPUT_PRESETS, sizeof(extensions));
	 strlcpy(object, "Input preset", sizeof(object));
	 strlcpy(comment, "INFO - Select an input preset from the menu by pressing the X button.", sizeof(comment));
	 break;
      case BORDER_CHOICE:
	 strlcpy(extensions, EXT_IMAGES, sizeof(extensions));
	 strlcpy(object, "Border image file", sizeof(object));
	 strlcpy(comment, "INFO - Select a border image file from the menu by pressing the X button.", sizeof(comment));
	 break;
      case LIBRETRO_CHOICE:
	 strlcpy(extensions, EXT_EXECUTABLES, sizeof(extensions));
	 strlcpy(object, "Libretro core", sizeof(object));
	 strlcpy(comment, "INFO - Select a Libretro core from the menu by pressing the X button.", sizeof(comment));
	 break;
   }

   browser_update(&tmpBrowser, input_st, extensions);

      if (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
      {
         bool is_dir = filebrowser_get_current_path_isdir(&tmpBrowser);
         if(is_dir)
            filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_OK);
	 else
	 {
            snprintf(path, sizeof(path), filebrowser_get_current_path(&tmpBrowser));

	    switch(current_menu->enum_id)
	    {
               case SHADER_CHOICE:
                  rarch_load_shader(set_shader+1, path);
		  switch(set_shader+1)
		  {
                     case 1:
                        strlcpy(g_settings.video.cg_shader_path, path, sizeof(g_settings.video.cg_shader_path));
			break;
		     case 2:
			strlcpy(g_settings.video.second_pass_shader, path, sizeof(g_settings.video.second_pass_shader));
			break;
		  }
		  menu_stack_refresh(items, current_menu);
		  break;
	       case PRESET_CHOICE:
		  strlcpy(g_console.cgp_path, path, sizeof(g_console.cgp_path));
		  apply_scaling(FBO_DEINIT);
		  gl_cg_reinit(path);
		  apply_scaling(FBO_INIT);
		  break;
	       case INPUT_PRESET_CHOICE:
		  strlcpy(g_console.input_cfg_path, path, sizeof(g_console.input_cfg_path));
		  config_read_keybinds(path);
		  menu_stack_refresh(items, current_menu);
		  break;
	       case BORDER_CHOICE:
		  break;
	       case LIBRETRO_CHOICE:
		  strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
		  if(set_libretro_core_as_launch)
		  {
		     strlcpy(g_console.launch_app_on_exit, path, sizeof(g_console.launch_app_on_exit));
                     set_libretro_core_as_launch = false;
                     rarch_settings_change(S_RETURN_TO_LAUNCHER);
		  }
		  break;
	    }

            menu_stack_decrement();
	 }
      }
      else if (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_X))
         menu_stack_decrement();

   display_menubar(current_menu);

   snprintf(comment_two, sizeof(comment_two), "X - Select %s  /\\ - return to settings  START - Reset Startdir", object);
   render_msg_place_func(0.09f, 0.92f, 0.92, YELLOW, comment_two);
   render_msg_place_func(0.09f, 0.83f, 0.91f, LIGHTBLUE, comment);
   render_msg_post_func();
}

static void select_directory(item *items, menu *current_menu)
{
   char path[1024];
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   {
      bool is_dir = filebrowser_get_current_path_isdir(&tmpBrowser);
      browser_update(&tmpBrowser, input_st, "empty");

      if (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_Y))
      {
         if(is_dir)
	 {
            snprintf(path, sizeof(path), filebrowser_get_current_path(&tmpBrowser));
	    switch(current_menu->enum_id)
	    {
               case PATH_SAVESTATES_DIR_CHOICE:
                  strlcpy(g_console.default_savestate_dir, path, sizeof(g_console.default_savestate_dir));
		  break;
	       case PATH_SRAM_DIR_CHOICE:
		  strlcpy(g_console.default_sram_dir, path, sizeof(g_console.default_sram_dir));
		  break;
	       case PATH_DEFAULT_ROM_DIR_CHOICE:
		  strlcpy(g_console.default_rom_startup_dir, path, sizeof(g_console.default_rom_startup_dir));
		  break;
	       case PATH_CHEATS_DIR_CHOICE:
		  strlcpy(g_settings.cheat_database, path, sizeof(g_settings.cheat_database));
		  break;
	    }
	    menu_stack_decrement();
	 }
      }
      else if (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_X))
      {
         strlcpy(path, default_paths.port_dir, sizeof(path));
	 switch(current_menu->enum_id)
	 {
            case PATH_SAVESTATES_DIR_CHOICE:
               strlcpy(g_console.default_savestate_dir, path, sizeof(g_console.default_savestate_dir));
	       break;
	    case PATH_SRAM_DIR_CHOICE:
	       strlcpy(g_console.default_sram_dir, path, sizeof(g_console.default_sram_dir));
	       break;
	    case PATH_DEFAULT_ROM_DIR_CHOICE:
	       strlcpy(g_console.default_rom_startup_dir, path, sizeof(g_console.default_rom_startup_dir));
	       break;
	    case PATH_CHEATS_DIR_CHOICE:
	       strlcpy(g_settings.cheat_database, path, sizeof(g_settings.cheat_database));
	       break;
	 }
	 menu_stack_decrement();
      }
      else if (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
      {
         if(is_dir)
            filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_OK);
      }
   }

   display_menubar(current_menu);

   render_msg_place_func(0.09f, 0.93f, 0.92f, YELLOW, "X - Enter dir  /\\ - return to settings  START - Reset Startdir");
   render_msg_place_func(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Browse to a directory and assign it as the path by\npressing SQUARE button.");
   render_msg_post_func();
}

static void set_keybind_digital(uint64_t default_retro_joypad_id)
{
   unsigned keybind_action = KEYBIND_NOACTION;

   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
      keybind_action = KEYBIND_DECREMENT;

   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
      keybind_action = KEYBIND_INCREMENT;

   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
      keybind_action = KEYBIND_DEFAULT;

   rarch_input_set_keybind(currently_selected_controller_menu, keybind_action, default_retro_joypad_id);
}

static void rarch_filename_input_and_save (unsigned filename_type)
{
   bool filename_entered = false;
   char filename_tmp[256], filepath[PATH_MAX];
   oskutil_write_initial_message(&g_console.oskutil_handle, L"example");
   oskutil_write_message(&g_console.oskutil_handle, L"Enter filename for preset (with no file extension)");

   oskutil_start(&g_console.oskutil_handle);

   while(OSK_IS_RUNNING(g_console.oskutil_handle))
   {
      gfx_ctx_clear();
      gfx_ctx_swap_buffers();
#ifdef HAVE_SYSUTILS
      cellSysutilCheckCallback();
#endif
   }

   if(g_console.oskutil_handle.text_can_be_fetched)
   {
      strlcpy(filename_tmp, OUTPUT_TEXT_STRING(g_console.oskutil_handle), sizeof(filename_tmp));
      switch(filename_type)
      {
         case CONFIG_FILE:
            break;
	 case SHADER_PRESET_FILE:
	    snprintf(filepath, sizeof(filepath), "%s/%s.cgp", default_paths.cgp_dir, filename_tmp);
	    break;
	 case INPUT_PRESET_FILE:
	    snprintf(filepath, sizeof(filepath), "%s/%s.cfg", default_paths.input_presets_dir, filename_tmp);
	    break;
      }

      filename_entered = true;
   }

   if(filename_entered)
   {
      char filetitle_tmp[512];
      oskutil_write_initial_message(&g_console.oskutil_handle, L"Example file title");
      oskutil_write_message(&g_console.oskutil_handle, L"Enter title for preset");
      oskutil_start(&g_console.oskutil_handle);

      while(OSK_IS_RUNNING(g_console.oskutil_handle))
      {
         /* OSK Util gets updated */
         gfx_ctx_clear();
	 gfx_ctx_swap_buffers();
#ifdef HAVE_SYSUTILS
         cellSysutilCheckCallback();
#endif
      }

      if(g_console.oskutil_handle.text_can_be_fetched)
         snprintf(filetitle_tmp, sizeof(filetitle_tmp), "%s", OUTPUT_TEXT_STRING(g_console.oskutil_handle));
      else
         snprintf(filetitle_tmp, sizeof(filetitle_tmp), "%s", "Custom");

      switch(filename_type)
      {
         case CONFIG_FILE:
            break;
	 case SHADER_PRESET_FILE:
	    {
               struct gl_cg_cgp_info current_settings;
	       current_settings.shader[0] = g_settings.video.cg_shader_path;
	       current_settings.shader[1] = g_settings.video.second_pass_shader;
	       current_settings.filter_linear[0] = g_settings.video.smooth;
	       current_settings.filter_linear[1] = g_settings.video.second_pass_smooth;
	       current_settings.render_to_texture = true;
	       current_settings.fbo_scale = g_settings.video.fbo_scale_x; //fbo_scale_x and y are the same anyway
	       gl_cg_save_cgp(filepath, &current_settings);
	    }
	    break;
	 case INPUT_PRESET_FILE:
	    config_save_keybinds(filepath);
	    break;
      }
   }
}

static void producesettingentry(menu *current_menu, item *items, unsigned switchvalue)
{
   switch(switchvalue)
   {
	   case SETTING_CHANGE_RESOLUTION:
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
			   rarch_settings_change(S_RESOLUTION_NEXT);
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
			   rarch_settings_change(S_RESOLUTION_PREVIOUS);
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
		   {
			   if (g_console.supported_resolutions[g_console.current_resolution_index] == CELL_VIDEO_OUT_RESOLUTION_576)
			   {
				   if(gfx_ctx_check_resolution(CELL_VIDEO_OUT_RESOLUTION_576))
				   {
					   //ps3graphics_set_pal60hz(Settings.PS3PALTemporalMode60Hz);
					   video_gl.restart();
				   }
			   }
			   else
			   {
				   //ps3graphics_set_pal60hz(0);
				   video_gl.restart();
			   }
		   }
		   break;
		   /*
		      case SETTING_PAL60_MODE:
		      if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		      {
		      if (Graphics->GetCurrentResolution() == CELL_VIDEO_OUT_RESOLUTION_576)
		      {
		      if(Graphics->CheckResolution(CELL_VIDEO_OUT_RESOLUTION_576))
		      {
		      Settings.PS3PALTemporalMode60Hz = !Settings.PS3PALTemporalMode60Hz;
		      Graphics->SetPAL60Hz(Settings.PS3PALTemporalMode60Hz);
		      Graphics->SwitchResolution(Graphics->GetCurrentResolution(), Settings.PS3PALTemporalMode60Hz, Settings.TripleBuffering);
		      }
		      }

		      }
		      break;
		      */
	   case SETTING_SHADER_PRESETS:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   if(g_console.emulator_initialized)
			   {
				   menu_stack_push(items, PRESET_CHOICE);
                                   filebrowser_set_root_and_ext(&tmpBrowser, EXT_CGP_PRESETS, default_paths.cgp_dir);
			   }
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   strlcpy(g_console.cgp_path, "", sizeof(g_console.cgp_path));
		   break;
	   case SETTING_SHADER:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   menu_stack_push(items, SHADER_CHOICE);
			   filebrowser_set_root_and_ext(&tmpBrowser, EXT_SHADERS, default_paths.shader_dir);
			   set_shader = 0;
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_load_shader(1, NULL);
			   strlcpy(g_settings.video.cg_shader_path, default_paths.shader_file, sizeof(g_settings.video.cg_shader_path));
			   menu_stack_refresh(items, current_menu);
		   }
		   break;
	   case SETTING_SHADER_2:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   menu_stack_push( items, SHADER_CHOICE);
			   filebrowser_set_root_and_ext(&tmpBrowser, EXT_SHADERS, default_paths.shader_dir);
			   set_shader = 1;
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_load_shader(2, NULL);
			   strlcpy(g_settings.video.second_pass_shader, default_paths.shader_file, sizeof(g_settings.video.second_pass_shader));
			   menu_stack_refresh(items, current_menu);
		   }
		   break;
	   case SETTING_FONT_SIZE:
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
		   {
			   if(g_console.menu_font_size > 0) 
				   g_console.menu_font_size -= 0.01f;
		   }
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   if((g_console.menu_font_size < 2.0f))
				   g_console.menu_font_size += 0.01f;
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   g_console.menu_font_size = 1.0f;
		   break;
	   case SETTING_KEEP_ASPECT_RATIO:
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
		   {
			   rarch_settings_change(S_ASPECT_RATIO_DECREMENT);
			   gfx_ctx_set_aspect_ratio(NULL, g_console.aspect_ratio_index);
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
		   {
			   rarch_settings_change(S_ASPECT_RATIO_INCREMENT);
			   gfx_ctx_set_aspect_ratio(NULL, g_console.aspect_ratio_index);
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_default(S_DEF_ASPECT_RATIO);
			   gfx_ctx_set_aspect_ratio(NULL, g_console.aspect_ratio_index);
		   }
		   break;
	   case SETTING_HW_TEXTURE_FILTER:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_HW_TEXTURE_FILTER);
			   gfx_ctx_set_filtering(1, g_settings.video.smooth);
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_change(S_DEF_HW_TEXTURE_FILTER);
			   gfx_ctx_set_filtering(1, g_settings.video.smooth);
		   }
		   break;
	   case SETTING_HW_TEXTURE_FILTER_2:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_HW_TEXTURE_FILTER_2);
			   gfx_ctx_set_filtering(2, g_settings.video.second_pass_smooth);
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_change(S_DEF_HW_TEXTURE_FILTER_2);
			   gfx_ctx_set_filtering(2, g_settings.video.second_pass_smooth);
		   }
		   break;
	   case SETTING_SCALE_ENABLED:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_SCALE_ENABLED);
			   gfx_ctx_set_fbo(g_console.fbo_enabled);
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_default(S_DEF_SCALE_ENABLED);
			   apply_scaling(FBO_DEINIT);
			   apply_scaling(FBO_INIT);
		   }
		   break;
	   case SETTING_SCALE_FACTOR:
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
		   {
			   if(g_console.fbo_enabled)
			   {
				   bool should_decrement = g_settings.video.fbo_scale_x > MIN_SCALING_FACTOR;
				   if(should_decrement)
				   {
					   rarch_settings_change(S_SCALE_FACTOR_DECREMENT);
					   apply_scaling(FBO_REINIT);
				   }
			   }
		   }
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   if(g_console.fbo_enabled)
			   {
				   bool should_increment = g_settings.video.fbo_scale_x < MAX_SCALING_FACTOR;
				   if(should_increment)
				   {
					   rarch_settings_change(S_SCALE_FACTOR_INCREMENT);
					   apply_scaling(FBO_REINIT);
				   }
			   }
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_default(S_DEF_SCALE_FACTOR);
			   apply_scaling(FBO_DEINIT);
			   apply_scaling(FBO_INIT);
		   }
		   break;
	   case SETTING_HW_OVERSCAN_AMOUNT:
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
		   {
			   rarch_settings_change(S_OVERSCAN_DECREMENT);
			   gfx_ctx_set_overscan();
		   }
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_OVERSCAN_INCREMENT);
			   gfx_ctx_set_overscan();
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_default(S_DEF_OVERSCAN);
			   gfx_ctx_set_overscan();
		   }
		   break;
	   case SETTING_THROTTLE_MODE:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_THROTTLE);
			   gfx_ctx_set_swap_interval(g_console.throttle_enable, true);
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_default(S_DEF_THROTTLE);
			   gfx_ctx_set_swap_interval(g_console.throttle_enable, true);
		   }
		   break;
	   case SETTING_TRIPLE_BUFFERING:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_TRIPLE_BUFFERING);
			   video_gl.restart();
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   bool old_buffer_input_st = g_console.triple_buffering_enable;
			   rarch_settings_default(S_DEF_TRIPLE_BUFFERING);

			   if(!old_buffer_input_st)
				   video_gl.restart();
		   }
		   break;
	   case SETTING_ENABLE_SCREENSHOTS:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
#if(CELL_SDK_VERSION > 0x340000)
			   g_console.screenshots_enable = !g_console.screenshots_enable;
			   if(g_console.screenshots_enable)
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
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
#if(CELL_SDK_VERSION > 0x340000)
			   g_console.screenshots_enable = true;
#endif
		   }
		   break;
	   case SETTING_SAVE_SHADER_PRESET:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
			   rarch_filename_input_and_save(SHADER_PRESET_FILE);
		   break;
	   case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
		   break;
	   case SETTING_DEFAULT_VIDEO_ALL:
		   break;
	   case SETTING_SOUND_MODE:
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
		   {
			   if(g_console.sound_mode != SOUND_MODE_NORMAL)
				   g_console.sound_mode--;
		   }
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   if(g_console.sound_mode < SOUND_MODE_HEADSET)
				   g_console.sound_mode++;
		   }
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_UP)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN)))
		   {
			   if(g_console.sound_mode != SOUND_MODE_RSOUND)
				   rarch_console_rsound_stop();
			   else
				   rarch_console_rsound_start(g_settings.audio.device);
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   g_console.sound_mode = SOUND_MODE_NORMAL;
			   rarch_console_rsound_stop();
		   }
		   break;
	   case SETTING_RSOUND_SERVER_IP_ADDRESS:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   oskutil_write_initial_message(&g_console.oskutil_handle, L"192.168.1.1");
			   oskutil_write_message(&g_console.oskutil_handle, L"Enter IP address for the RSound Server.");
			   oskutil_start(&g_console.oskutil_handle);
			   while(OSK_IS_RUNNING(g_console.oskutil_handle))
			   {
				   gfx_ctx_clear();
				   gfx_ctx_swap_buffers();
#ifdef HAVE_SYSUTILS
				   cellSysutilCheckCallback();
#endif
			   }

			   if(g_console.oskutil_handle.text_can_be_fetched)
				   strlcpy(g_settings.audio.device, OUTPUT_TEXT_STRING(g_console.oskutil_handle), sizeof(g_settings.audio.device));
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   strlcpy(g_settings.audio.device, "0.0.0.0", sizeof(g_settings.audio.device));
		   break;
	   case SETTING_DEFAULT_AUDIO_ALL:
		   break;
	   case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
			   rarch_settings_change(S_SAVESTATE_DECREMENT);
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
			   rarch_settings_change(S_SAVESTATE_INCREMENT);

		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   rarch_settings_default(S_DEF_SAVE_STATE);
		   break;
	   case SETTING_EMU_SHOW_INFO_MSG:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
			   g_console.info_msg_enable = !g_console.info_msg_enable;
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   g_console.info_msg_enable = true;
		   break;
	   case SETTING_EMU_REWIND_ENABLED:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_REWIND);

			   if(g_console.info_msg_enable)
				   rarch_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   g_settings.rewind_enable = false;
		   break;
	   case SETTING_ZIP_EXTRACT:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)))
		   {
			   if(g_console.zip_extract_mode > 0)
				   g_console.zip_extract_mode--;
		   }
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   if(g_console.zip_extract_mode < ZIP_EXTRACT_TO_CACHE_DIR)
				   g_console.zip_extract_mode++;
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   g_console.zip_extract_mode = ZIP_EXTRACT_TO_CURRENT_DIR;
		   }
		   break;
	   case SETTING_RARCH_DEFAULT_EMU:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   menu_stack_push(items, LIBRETRO_CHOICE);
			   filebrowser_set_root_and_ext(&tmpBrowser, EXT_EXECUTABLES, default_paths.core_dir);
			   set_libretro_core_as_launch = false;
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
		   }
		   break;
	   case SETTING_EMU_AUDIO_MUTE:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
			   rarch_settings_change(S_AUDIO_MUTE);

		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   rarch_settings_default(S_DEF_AUDIO_MUTE);
		   break;
	   case SETTING_ENABLE_CUSTOM_BGM:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
#if(CELL_SDK_VERSION > 0x340000)
			   g_console.custom_bgm_enable = !g_console.custom_bgm_enable;
			   if(g_console.custom_bgm_enable)
				   cellSysutilEnableBgmPlayback();
			   else
				   cellSysutilDisableBgmPlayback();

#endif
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
#if(CELL_SDK_VERSION > 0x340000)
			   g_console.custom_bgm_enable = true;
#endif
		   }
		   break;
	   case SETTING_EMU_VIDEO_DEFAULT_ALL:
		   break;
	   case SETTING_EMU_AUDIO_DEFAULT_ALL:
		   break;
	   case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   menu_stack_push(items, PATH_DEFAULT_ROM_DIR_CHOICE);
			   filebrowser_set_root_and_ext(&tmpBrowser, "empty", "/");
		   }

		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   strlcpy(g_console.default_rom_startup_dir, "/", sizeof(g_console.default_rom_startup_dir));
		   break;
	   case SETTING_PATH_SAVESTATES_DIRECTORY:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   menu_stack_push(items, PATH_SAVESTATES_DIR_CHOICE);
			   filebrowser_set_root_and_ext(&tmpBrowser, "empty", "/");
		   }

		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   strlcpy(g_console.default_savestate_dir, default_paths.savestate_dir, sizeof(g_console.default_savestate_dir));

		   break;
	   case SETTING_PATH_SRAM_DIRECTORY:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   menu_stack_push(items, PATH_SRAM_DIR_CHOICE);
			   filebrowser_set_root_and_ext(&tmpBrowser, "empty", "/");
		   }

		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   strlcpy(g_console.default_sram_dir, default_paths.sram_dir, sizeof(g_console.default_sram_dir));
		   break;
	   case SETTING_PATH_CHEATS:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   menu_stack_push(items, PATH_CHEATS_DIR_CHOICE);
			   filebrowser_set_root_and_ext(&tmpBrowser, "empty", "/");
		   }

		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   strlcpy(g_settings.cheat_database, default_paths.port_dir, sizeof(g_settings.cheat_database));
		   break;
	   case SETTING_PATH_SYSTEM:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   menu_stack_push(items, PATH_SYSTEM_DIR_CHOICE);
			   filebrowser_set_root_and_ext(&tmpBrowser, "empty", "/");
		   }

		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   strlcpy(g_settings.system_directory, default_paths.system_dir, sizeof(g_settings.system_directory));
		   break;
	   case SETTING_ENABLE_SRAM_PATH:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)))
		   {
			   g_console.default_sram_dir_enable = !g_console.default_sram_dir_enable;
			   menu_stack_refresh(items, current_menu);
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   g_console.default_sram_dir_enable = true;
			   menu_stack_refresh(items, current_menu);
		   }
		   break;
	   case SETTING_ENABLE_STATE_PATH:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)))
		   {
			   g_console.default_savestate_dir_enable = !g_console.default_savestate_dir_enable;
			   menu_stack_refresh(items, current_menu);
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   g_console.default_savestate_dir_enable = true;
			   menu_stack_refresh(items, current_menu);
		   }
		   break;
	   case SETTING_PATH_DEFAULT_ALL:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START)))
		   {
			   strlcpy(g_console.default_rom_startup_dir, "/", sizeof(g_console.default_rom_startup_dir));
			   strlcpy(g_console.default_savestate_dir, default_paths.port_dir, sizeof(g_console.default_savestate_dir));
			   strlcpy(g_settings.cheat_database, default_paths.port_dir, sizeof(g_settings.cheat_database));
			   strlcpy(g_console.default_sram_dir, "", sizeof(g_console.default_sram_dir));

			   menu_stack_refresh(items, current_menu);
		   }
		   break;
	   case SETTING_CONTROLS_SCHEME:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START)))
		   {
			   menu_stack_push(items, INPUT_PRESET_CHOICE);
			   filebrowser_set_root_and_ext(&tmpBrowser, EXT_INPUT_PRESETS, default_paths.input_presets_dir);
		   }
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   menu_stack_refresh(items, current_menu);
		   break;
	   case SETTING_CONTROLS_NUMBER:
		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
		   {
			   if(currently_selected_controller_menu != 0)
				   currently_selected_controller_menu--;
			   menu_stack_refresh(items, current_menu);
		   }

		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   if(currently_selected_controller_menu < 6)
				   currently_selected_controller_menu++;
			   menu_stack_refresh(items, current_menu);
		   }

		   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   currently_selected_controller_menu = 0;
		   break; 
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_UP:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_UP);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_DOWN:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_DOWN);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_LEFT:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_LEFT);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_RIGHT:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_RIGHT);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_A:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_A);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_B);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_X:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_X);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_Y:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_Y);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_SELECT:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_SELECT);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_START:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_START);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_L);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_R);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L2:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_L2);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R2:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_R2);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L3:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_L3);
		   break;
	   case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R3:
		   set_keybind_digital(RETRO_DEVICE_ID_JOYPAD_R3);
		   break;
	   case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START)))
			   rarch_filename_input_and_save(INPUT_PRESET_FILE);
		   break;
	   case SETTING_CONTROLS_DEFAULT_ALL:
		   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START)))
		   {
			   rarch_input_set_default_keybinds(currently_selected_controller_menu);
			   menu_stack_refresh(items, current_menu);
		   }
		   break;
   }

   set_setting_label(current_menu, items, switchvalue);
}

static void settings_iterate(menu *current_menu, item *items, settings_action_t action)
{
   switch(action)
   {
      case SETTINGS_ACTION_DOWN:
         current_menu->selected++;

	 if (current_menu->selected >= current_menu->max_settings)
            current_menu->selected = current_menu->first_setting; 

	 if (items[current_menu->selected].page != current_menu->page)
            current_menu->page = items[current_menu->selected].page;
         break;
      case SETTINGS_ACTION_UP:
         if (current_menu->selected == current_menu->first_setting)
            current_menu->selected = current_menu->max_settings-1;
	 else
            current_menu->selected--;

	 if (items[current_menu->selected].page != current_menu->page)
            current_menu->page = items[current_menu->selected].page;
         break;
      case SETTINGS_ACTION_TAB_PREVIOUS:
	 menu_stack_decrement();
         break;
      case SETTINGS_ACTION_TAB_NEXT:
         switch(current_menu->enum_id)
	 {
            case GENERAL_VIDEO_MENU:
	    case GENERAL_AUDIO_MENU:
	    case EMU_GENERAL_MENU:
	    case EMU_VIDEO_MENU:
	    case EMU_AUDIO_MENU:
	    case PATH_MENU:
               menu_stack_push(items, current_menu->enum_id + 1);
	       break;
	    case CONTROLS_MENU:
            default:
	       break;
	 }
         break;
      default:
         break;
   }
}

static void select_setting(item *items, menu *current_menu)
{
   unsigned i;
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   settings_action_t action = SETTINGS_ACTION_NOOP;

   /* back to ROM menu if CIRCLE is pressed */
   if ((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_L)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_A)))
      action = SETTINGS_ACTION_TAB_PREVIOUS;
   else if (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_R))
      action = SETTINGS_ACTION_TAB_NEXT;
   else if (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
      action = SETTINGS_ACTION_DOWN;
   else if (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
      action = SETTINGS_ACTION_UP;

   if(action != SETTINGS_ACTION_NOOP)
	   settings_iterate(current_menu, items, action);

   producesettingentry(current_menu, items, current_menu->selected);

   display_menubar(current_menu);
   render_msg_post_func();


   for(i = current_menu->first_setting; i < current_menu->max_settings; i++)
   {
      if(items[i].page == current_menu->page)
      {
         render_msg_place_func(items[i].text_xpos, items[i].text_ypos, FONT_SIZE, current_menu->selected == items[i].enum_id ? YELLOW : items[i].item_color, items[i].text);
	 render_msg_place_func(0.5f, items[i].text_ypos, FONT_SIZE, items[i].text_color, items[i].setting_text);
	 render_msg_post_func();
      }
   }

   render_msg_place_func(0.09f, COMMENT_YPOS, 0.86f, LIGHTBLUE, items[current_menu->selected].comment);

   render_msg_place_func(0.09f, 0.91f, FONT_SIZE, YELLOW, "UP/DOWN - select  L3+R3 - resume game   X/LEFT/RIGHT - change");
   render_msg_place_func(0.09f, 0.95f, FONT_SIZE, YELLOW, "START - default   L1/CIRCLE - go back   R1 - go forward");
   render_msg_post_func();
}

static void menu_romselect_iterate(filebrowser_t *filebrowser, item *items, menu_romselect_action_t action)
{
   switch(action)
   {
      case MENU_ROMSELECT_ACTION_OK:
         if(filebrowser_get_current_path_isdir(filebrowser))
            filebrowser_iterate(filebrowser, FILEBROWSER_ACTION_OK);
	 else
            rarch_console_load_game_wrap(filebrowser_get_current_path(filebrowser), g_console.zip_extract_mode, S_DELAY_45);
         break;
      case MENU_ROMSELECT_ACTION_GOTO_SETTINGS:
         menu_stack_push(items, GENERAL_VIDEO_MENU);
         break;
      default:
         break;
   }
}

static void select_rom(item *items, menu *current_menu)
{
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   browser_update(&browser, input_st, rarch_console_get_rom_ext());

   menu_romselect_action_t action = MENU_ROMSELECT_ACTION_NOOP;

   if (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT))
      action = MENU_ROMSELECT_ACTION_GOTO_SETTINGS;
   else if (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
      action = MENU_ROMSELECT_ACTION_OK;

   if (action != MENU_ROMSELECT_ACTION_NOOP)
      menu_romselect_iterate(&browser, items, action);

   bool is_dir = filebrowser_get_current_path_isdir(&browser);

   if (is_dir)
   {
      const char *current_path = filebrowser_get_current_path(&browser);
      if(!strcmp(current_path,"app_home") || !strcmp(current_path, "host_root"))
         render_msg_place_func(0.09f, 0.83f, 0.91f, RED, "WARNING - This path only works on DEX PS3 systems. Do not attempt to open\n this directory on CEX PS3 systems, or you might have to restart.");
      else
         render_msg_place_func(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to enter the directory.");
   }
   else
      render_msg_place_func(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to load the game. ");

   display_menubar(current_menu);

   render_msg_place_func   (0.09f, 0.91f, FONT_SIZE, YELLOW,
   "L3 + R3 - resume game           SELECT - Settings screen");
   render_msg_post_func();
}


static void ingame_menu_resize(item *items, menu *current_menu)
{
   (void)items;

   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   float x_position = 0.09f;
   float font_size = 1.1f;
   float ypos = 0.16f;
   float ypos_increment = 0.035f;

   g_console.aspect_ratio_index = ASPECT_RATIO_CUSTOM;
   gfx_ctx_set_aspect_ratio(NULL, g_console.aspect_ratio_index);

   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
      g_console.viewports.custom_vp.x -= 1;
   else if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      g_console.viewports.custom_vp.x += 1;

   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
      g_console.viewports.custom_vp.y += 1;
   else if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
      g_console.viewports.custom_vp.y -= 1;

   if (CTRL_RSTICK_LEFT(state) || CTRL_L1(state))
      g_console.viewports.custom_vp.width -= 1;
   else if (CTRL_RSTICK_RIGHT(state) || CTRL_R1(state))
      g_console.viewports.custom_vp.width += 1;

   if (CTRL_RSTICK_UP(state) || CTRL_L2(state))
      g_console.viewports.custom_vp.height += 1;
   else if (CTRL_RSTICK_DOWN(state) || CTRL_R2(state))
      g_console.viewports.custom_vp.height -= 1;

   if (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_X))
   {
      g_console.viewports.custom_vp.x = 0;
      g_console.viewports.custom_vp.y = 0;
      g_console.viewports.custom_vp.width = device_ptr->win_width;
      g_console.viewports.custom_vp.height = device_ptr->win_height;
   }

   if (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_A))
      menu_stack_decrement();

   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_Y)) { }
   else
   {
      char viewport_x[64], viewport_y[64], viewport_w[64], viewport_h[64];
      display_menubar(current_menu);

      snprintf(viewport_x, sizeof(viewport_x), "Viewport X: #%d", g_console.viewports.custom_vp.x);
      snprintf(viewport_y, sizeof(viewport_y), "Viewport Y: #%d", g_console.viewports.custom_vp.y);
      snprintf(viewport_w, sizeof(viewport_w), "Viewport Width: #%d", g_console.viewports.custom_vp.width);
      snprintf(viewport_h, sizeof(viewport_h), "Viewport Height: #%d", g_console.viewports.custom_vp.height);

      render_msg_place_func(x_position, ypos, font_size, GREEN, viewport_x);
      render_msg_place_func(x_position, ypos+(ypos_increment*1), font_size, GREEN, viewport_y);
      render_msg_place_func(x_position, ypos+(ypos_increment*2), font_size, GREEN, viewport_w);
      render_msg_place_func(x_position, ypos+(ypos_increment*3), font_size, GREEN, viewport_h);

      render_msg_place_func(0.09f, 0.40f, font_size, LIGHTBLUE, "CONTROLS:");

      render_msg_place_func (0.09f, 0.46f, font_size,  LIGHTBLUE, "LEFT or LSTICK UP");
      render_msg_place_func (0.5f, 0.46f, font_size, LIGHTBLUE, "- Decrease Viewport X");

      render_msg_post_func();

      render_msg_place_func (0.09f, 0.48f, font_size, LIGHTBLUE, "RIGHT or LSTICK RIGHT");
      render_msg_place_func (0.5f, 0.48f, font_size, LIGHTBLUE, "- Increase Viewport X");

      render_msg_place_func (0.09f, 0.50f, font_size, LIGHTBLUE, "UP or LSTICK UP");
      render_msg_place_func (0.5f, 0.50f, font_size, LIGHTBLUE, "- Increase Viewport Y");

      render_msg_post_func();

      render_msg_place_func (0.09f, 0.52f, font_size, LIGHTBLUE, "DOWN or LSTICK DOWN");
      render_msg_place_func (0.5f, 0.52f, font_size, LIGHTBLUE, "- Decrease Viewport Y");

      render_msg_place_func (0.09f, 0.54f, font_size, LIGHTBLUE, "L1 or RSTICK LEFT");
      render_msg_place_func (0.5f, 0.54f, font_size, LIGHTBLUE, "- Decrease Viewport Width");

      render_msg_post_func();

      render_msg_place_func (0.09f, 0.56f, font_size, LIGHTBLUE, "R1 or RSTICK RIGHT");
      render_msg_place_func (0.5f, 0.56f, font_size, LIGHTBLUE, "- Increase Viewport Width");

      render_msg_place_func (0.09f, 0.58f, font_size, LIGHTBLUE, "L2 or  RSTICK UP");
      render_msg_place_func (0.5f, 0.58f, font_size, LIGHTBLUE, "- Increase Viewport Height");

      render_msg_post_func();

      render_msg_place_func (0.09f, 0.60f, font_size, LIGHTBLUE, "R2 or RSTICK DOWN");
      render_msg_place_func (0.5f, 0.60f, font_size, LIGHTBLUE, "- Decrease Viewport Height");

      render_msg_place_func (0.09f, 0.66f, font_size, LIGHTBLUE, "TRIANGLE");
      render_msg_place_func (0.5f, 0.66f, font_size, LIGHTBLUE, "- Reset To Defaults");

      render_msg_place_func (0.09f, 0.68f, font_size, LIGHTBLUE, "SQUARE");
      render_msg_place_func (0.5f, 0.68f, font_size, LIGHTBLUE, "- Show Game Screen");

      render_msg_place_func (0.09f, 0.70f, font_size, LIGHTBLUE, "CIRCLE");
      render_msg_place_func (0.5f, 0.70f, font_size, LIGHTBLUE, "- Return to Ingame Menu");

      render_msg_post_func();

      render_msg_place_func (0.09f, 0.83f, 0.91f, LIGHTBLUE, "Allows you to resize the screen by moving around the two analog sticks.\nPress TRIANGLE to reset to default values, and CIRCLE to go back to the menu.");
      render_msg_post_func();
   }
}

static void ingame_menu_screenshot(item *items, menu *current_menu)
{
   (void)items;
   (void)current_menu;

   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   if(g_console.ingame_menu_enable)
   {
      if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_A))
      {
	 menu_stack_decrement();
	 device_ptr->menu_render = true;
      }
   }
}

static void ingame_menu(item *items, menu *current_menu)
{
   char comment[256], overscan_msg[64];
   static unsigned menuitem_colors[MENU_ITEM_LAST];
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   float x_position = 0.09f;
   float font_size = 1.1f;
   float ypos = 0.16f;
   float ypos_increment = 0.035f;

   for(int i = 0; i < MENU_ITEM_LAST; i++)
      menuitem_colors[i] = GREEN;

   menuitem_colors[g_console.ingame_menu_item] = RED;

   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_A))
      rarch_settings_change(S_RETURN_TO_GAME);

      switch(g_console.ingame_menu_item)
      {
         case MENU_ITEM_LOAD_STATE:
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	    {
               rarch_load_state();
               rarch_settings_change(S_RETURN_TO_GAME);
	    }
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
               rarch_state_slot_decrease();
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
               rarch_state_slot_increase();

	    strlcpy(comment, "Press LEFT or RIGHT to change the current save state slot.\nPress CROSS to load the state from the currently selected save state slot.", sizeof(comment));
	    break;
	 case MENU_ITEM_SAVE_STATE:
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	    {
               rarch_save_state();
               rarch_settings_change(S_RETURN_TO_GAME);
	    }
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
               rarch_state_slot_decrease();
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
               rarch_state_slot_increase();

	    strlcpy(comment, "Press LEFT or RIGHT to change the current save state slot.\nPress CROSS to save the state to the currently selected save state slot.", sizeof(comment));
	    break;
	 case MENU_ITEM_KEEP_ASPECT_RATIO:
            producesettingentry(current_menu, items, SETTING_KEEP_ASPECT_RATIO);
	    strlcpy(comment, "Press LEFT or RIGHT to change the [Aspect Ratio].\nPress START to reset back to default values.", sizeof(comment));
	    break;
	 case MENU_ITEM_OVERSCAN_AMOUNT:
            producesettingentry(current_menu, items, SETTING_HW_OVERSCAN_AMOUNT);
	    strlcpy(comment, "Press LEFT or RIGHT to change the [Overscan] settings.\nPress START to reset back to default values.", sizeof(comment));
	    break;
	 case MENU_ITEM_ORIENTATION:
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
	    {
               rarch_settings_change(S_ROTATION_DECREMENT);
	       video_gl.set_rotation(NULL, g_console.screen_orientation);
	    }

            if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
	    {
               rarch_settings_change(S_ROTATION_INCREMENT);
	       video_gl.set_rotation(NULL, g_console.screen_orientation);
	    }

            if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_START))
	    {
               rarch_settings_default(S_DEF_ROTATION);
	       video_gl.set_rotation(NULL, g_console.screen_orientation);
	    }
	    strlcpy(comment, "Press LEFT or RIGHT to change the [Orientation] settings.\nPress START to reset back to default values.", sizeof(comment));
	    break;
	 case MENU_ITEM_SCALE_FACTOR:
            producesettingentry(current_menu, items, SETTING_SCALE_FACTOR);
	    strlcpy(comment, "Press LEFT or RIGHT to change the [Scaling] settings.\nPress START to reset back to default values.", sizeof(comment));
	    break;
	 case MENU_ITEM_FRAME_ADVANCE:
            if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_R2)) || (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_L2)))
	    {
               rarch_settings_change(S_FRAME_ADVANCE);
	       g_console.ingame_menu_item = MENU_ITEM_FRAME_ADVANCE;
	    }
	    strlcpy(comment, "Press 'CROSS', 'L2' or 'R2' button to step one frame. Pressing the button\nrapidly will advance the frame more slowly.", sizeof(comment));
	    break;
	 case MENU_ITEM_RESIZE_MODE:
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	       menu_stack_push(items, INGAME_MENU_RESIZE);
	    strlcpy(comment, "Allows you to resize the screen by moving around the two analog sticks.\nPress TRIANGLE to reset to default values, and CIRCLE to go back.", sizeof(comment));
	    break;
	 case MENU_ITEM_SCREENSHOT_MODE:
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	       menu_stack_push(items, INGAME_MENU_SCREENSHOT);
	    strlcpy(comment, "Allows you to take a screenshot without any text clutter.\nPress CIRCLE to go back to the in-game menu while in 'Screenshot Mode'.", sizeof(comment));
	    break;
	 case MENU_ITEM_RETURN_TO_GAME:
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
               rarch_settings_change(S_RETURN_TO_GAME);

	    strlcpy(comment, "Press 'CROSS' to return back to the game.", sizeof(comment));
	    break;
	 case MENU_ITEM_RESET:
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	    {
               rarch_settings_change(S_RETURN_TO_GAME);
	       rarch_game_reset();
	    }
	    strlcpy(comment, "Press 'CROSS' to reset the game.", sizeof(comment));
	    break;
	 case MENU_ITEM_RETURN_TO_MENU:
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	    {
               rarch_settings_change(S_RETURN_TO_MENU);
	    }
	    strlcpy(comment, "Press 'CROSS' to return to the ROM Browser menu.", sizeof(comment));
	    break;
	 case MENU_ITEM_CHANGE_LIBRETRO:
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	    {
	       menu_stack_push(items, LIBRETRO_CHOICE);
	       filebrowser_set_root_and_ext(&tmpBrowser, EXT_EXECUTABLES, default_paths.core_dir);
	       set_libretro_core_as_launch = true;
	    }
	    strlcpy(comment, "Press 'CROSS' to choose a different emulator core.", sizeof(comment));
	    break;
#ifdef HAVE_MULTIMAN
	 case MENU_ITEM_RETURN_TO_MULTIMAN:
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	    {
               if(path_file_exists(default_paths.multiman_self_file))
               {
                  strlcpy(g_console.launch_app_on_exit, default_paths.multiman_self_file,
                  sizeof(g_console.launch_app_on_exit));

                  rarch_settings_change(S_RETURN_TO_DASHBOARD);
               }
	    }
	    strlcpy(comment, "Press 'CROSS' to quit the emulator and return to multiMAN.", sizeof(comment));
	    break;
#endif
	 case MENU_ITEM_RETURN_TO_DASHBOARD:
	    if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_B))
               rarch_settings_change(S_RETURN_TO_DASHBOARD);

	    strlcpy(comment, "Press 'CROSS' to quit the emulator and return to the XMB.", sizeof(comment));
	    break;
      }

   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
   {
      if(g_console.ingame_menu_item > 0)
         g_console.ingame_menu_item--;
   }

   if(input_st & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
   {
      if(g_console.ingame_menu_item < (MENU_ITEM_LAST-1))
         g_console.ingame_menu_item++;
   }

   if((input_st & (1 << RETRO_DEVICE_ID_JOYPAD_L3)) && (input_st & (1 << RETRO_DEVICE_ID_JOYPAD_R3)))
      rarch_settings_change(S_RETURN_TO_GAME);

   display_menubar(current_menu);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
   render_msg_place_func(x_position, ypos, font_size, MENU_ITEM_SELECTED(MENU_ITEM_LOAD_STATE), strw_buffer);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
   render_msg_place_func(x_position, ypos+(ypos_increment*MENU_ITEM_SAVE_STATE), font_size, MENU_ITEM_SELECTED(MENU_ITEM_SAVE_STATE), strw_buffer);
   render_msg_post_func();

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
   render_msg_place_func(x_position, (ypos+(ypos_increment*MENU_ITEM_KEEP_ASPECT_RATIO)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_KEEP_ASPECT_RATIO), strw_buffer);

   snprintf(overscan_msg, sizeof(overscan_msg), "Overscan: %f", g_console.overscan_amount);
   render_msg_place_func(x_position, (ypos+(ypos_increment*MENU_ITEM_OVERSCAN_AMOUNT)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_OVERSCAN_AMOUNT), overscan_msg);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
   render_msg_place_func (x_position, (ypos+(ypos_increment*MENU_ITEM_ORIENTATION)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_ORIENTATION), strw_buffer);
   render_msg_post_func();

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SCALE_FACTOR, sizeof(strw_buffer));
   render_msg_place_func (x_position, (ypos+(ypos_increment*MENU_ITEM_SCALE_FACTOR)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_SCALE_FACTOR), strw_buffer);
   render_msg_post_func();

   render_msg_place_func(x_position, (ypos+(ypos_increment*MENU_ITEM_RESIZE_MODE)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RESIZE_MODE), "Resize Mode");

   render_msg_place_func(x_position, (ypos+(ypos_increment*MENU_ITEM_FRAME_ADVANCE)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_FRAME_ADVANCE), "Frame Advance");

   render_msg_place_func(x_position, (ypos+(ypos_increment*MENU_ITEM_SCREENSHOT_MODE)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_SCREENSHOT_MODE), "Screenshot Mode");

   render_msg_post_func();

   render_msg_place_func(x_position, (ypos+(ypos_increment*MENU_ITEM_RESET)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RESET), "Reset");

   render_msg_place_func(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_GAME)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_GAME), "Return to Game");
   render_msg_post_func();

   render_msg_place_func(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MENU)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_MENU), "Return to Menu");
   render_msg_post_func();

   render_msg_place_func(x_position, (ypos+(ypos_increment*MENU_ITEM_CHANGE_LIBRETRO)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_CHANGE_LIBRETRO), "Change libretro core");
   render_msg_post_func();

#ifdef HAVE_MULTIMAN
   render_msg_place_func(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MULTIMAN)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_MULTIMAN), "Return to multiMAN");
#endif

   render_msg_place_func(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_DASHBOARD)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_DASHBOARD), "Return to XMB");
   render_msg_post_func();

   render_msg_place_func(0.09f, 0.83f, 0.91f, LIGHTBLUE, comment);
   render_msg_post_func();
}

static bool check_analog(uint64_t state_tmp)
{
  if(CTRL_LSTICK_UP(state_tmp) || CTRL_LSTICK_DOWN(state_tmp) || CTRL_LSTICK_RIGHT(state_tmp) || CTRL_LSTICK_LEFT(state_tmp))
     return true;
  else
     return false;
}

static bool check_shoulder_buttons(uint64_t state_tmp)
{
  if(CTRL_L1(state_tmp) || CTRL_L2(state_tmp) || CTRL_R1(state_tmp) || CTRL_R2(state_tmp))
     return true;
  else
     return false;
}

void menu_init (void)
{
   //Set libretro filename and version to variable
   struct retro_system_info info;
   retro_get_system_info(&info);
   const char *id = info.library_name ? info.library_name : "Unknown";
   snprintf(core_text, sizeof(core_text), "Libretro core: %s %s", id, info.library_version);

   menu_stack_push(menu_items, FILE_BROWSER_MENU);
   filebrowser_set_root_and_ext(&browser, rarch_console_get_rom_ext(), default_paths.filebrowser_startup_dir);
   filebrowser_set_root(&tmpBrowser, "/");
}

void menu_free (void)
{
   filebrowser_free(&browser);
   filebrowser_free(&tmpBrowser);
}

void menu_loop(void)
{
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   g_console.menu_enable = true;
   device_ptr->block_swap = true;

   if(g_console.ingame_menu_enable)
      menu_stack_push(ingame_menu_settings, INGAME_MENU);

   do
   {
      static bool first_held = false;
      menu *current_menu = menu_stack_get_current_ptr();

      state = cell_pad_input_poll_device(0);
      uint64_t trig_state = state & ~old_state;

      {
         //second button input
         uint64_t held_state = cell_pad_input_poll_device(0);
         bool analog_sticks_pressed = check_analog(held_state);
         bool shoulder_buttons_pressed = check_shoulder_buttons(held_state) && current_menu->category_id != CATEGORY_SETTINGS;
         bool do_held = analog_sticks_pressed || shoulder_buttons_pressed;

         if(do_held)
         {
            if(!first_held)
            {
               first_held = true;
	       SET_TIMER_EXPIRATION(device_ptr, 7);
            }
            if(IS_TIMER_EXPIRED(device_ptr))
            {
               first_held = false;
               trig_state = held_state;
            }
         }
      }

      control_update_wrap(trig_state);

      gfx_ctx_clear();

      if(current_menu->enum_id == INGAME_MENU_RESIZE && CTRL_SQUARE(state) || current_menu->enum_id == INGAME_MENU_SCREENSHOT)
	 device_ptr->menu_render = false;
      else
      {
         gfx_ctx_set_blend(true);
	 device_ptr->menu_render = true;
      }

      rarch_render_cached_frame();

      filebrowser_t * fb = &browser;

      switch(current_menu->enum_id)
      {
         case FILE_BROWSER_MENU:
            select_rom(menu_items, current_menu);
            fb = &browser;
	    break;
	 case GENERAL_VIDEO_MENU:
	 case GENERAL_AUDIO_MENU:
	 case EMU_GENERAL_MENU:
	 case EMU_VIDEO_MENU:
	 case EMU_AUDIO_MENU:
	 case PATH_MENU:
	 case CONTROLS_MENU:
	    select_setting(menu_items, current_menu);
	    break;
	 case SHADER_CHOICE:
	 case PRESET_CHOICE:
	 case BORDER_CHOICE:
	 case LIBRETRO_CHOICE:
	 case INPUT_PRESET_CHOICE:
	    select_file(menu_items, current_menu);
            fb = &tmpBrowser;
	    break;
	 case PATH_SAVESTATES_DIR_CHOICE:
	 case PATH_DEFAULT_ROM_DIR_CHOICE:
	 case PATH_CHEATS_DIR_CHOICE:
	 case PATH_SRAM_DIR_CHOICE:
	    select_directory(menu_items, current_menu);
            fb = &tmpBrowser;
	    break;
	 case INGAME_MENU:
	    if(g_console.ingame_menu_enable)
               ingame_menu(menu_items, current_menu);
            break;
         case INGAME_MENU_RESIZE:
            ingame_menu_resize(menu_items, current_menu);
	    break;
         case INGAME_MENU_SCREENSHOT:
            ingame_menu_screenshot(menu_items, current_menu);
            break;
      }

      switch(current_menu->category_id)
      {
         case CATEGORY_FILEBROWSER:
            browser_render(fb, 0.09f, 0.10f, 0.035f);
            break;
         case CATEGORY_SETTINGS:
         case CATEGORY_INGAME_MENU:
         default:
            break;
      }

      old_state = state;

      if(IS_TIMER_EXPIRED(device_ptr))
      {
         // if we want to force goto the emulation loop, skip this
         if(g_console.mode_switch != MODE_EMULATION)
	 {
            // for ingame menu, we need a different precondition because menu_enable
	    // can be set to false when going back from ingame menu to menu
            if(g_console.ingame_menu_enable == true)
	    {
               //we want to force exit when mode_switch is set to MODE_EXIT
               if(g_console.mode_switch != MODE_EXIT)
                  g_console.mode_switch = ((CTRL_L3(old_state) && CTRL_R3(old_state) && g_console.emulator_initialized)) ? MODE_EMULATION : MODE_MENU;
	    }
	    else
	    {
               g_console.menu_enable = !((CTRL_L3(old_state) && CTRL_R3(old_state) && g_console.emulator_initialized));
	       g_console.mode_switch = g_console.menu_enable ? MODE_MENU : MODE_EMULATION;
	    }
	 }
      }

      // set a timer delay so that we don't instantly switch back to the menu when
      // press and holding L3 + R3 in the emulation loop (lasts for 30 frame ticks)
      if(g_console.mode_switch == MODE_EMULATION && !g_console.frame_advance_enable)
      {
         SET_TIMER_EXPIRATION(device_ptr, 30);
      }

      const char * message = msg_queue_pull(g_extern.msg_queue);

      if (message && g_console.info_msg_enable)
      {
         render_msg_place_func(g_settings.video.msg_pos_x, 0.75f, 1.05f, WHITE, message);
         render_msg_post_func();
      }

      gfx_ctx_swap_buffers();
#ifdef HAVE_SYSUTILS
      cellSysutilCheckCallback();
#endif
      if(current_menu->enum_id == INGAME_MENU_RESIZE && CTRL_SQUARE(state) || current_menu->enum_id == INGAME_MENU_SCREENSHOT)
      { }
      else
         gfx_ctx_set_blend(false);

   }while (g_console.menu_enable);

   device_ptr->menu_render = false;

   if(g_console.ingame_menu_enable)
      menu_stack_decrement();

   device_ptr->block_swap = false;

   g_console.ingame_menu_enable = false;
}
