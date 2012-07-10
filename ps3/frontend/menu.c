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

#include "../../console/retroarch_console.h"

#include "../../gfx/gl_common.h"
#include "../../gfx/gl_font.h"
#include "../../gfx/gfx_context.h"
#include "../../gfx/context/ps3_ctx.h"
#include "../../gfx/shader_cg.h"

#include "shared.h"
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

static void set_setting_label_write_on_or_off(bool cond, unsigned currentsetting)
{
   if(cond)
      snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "ON");
   else
      snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "OFF");
}

static void set_setting_label_color(bool cond, unsigned currentsetting)
{
   if(cond)
      items_generalsettings[currentsetting].text_color = GREEN;
   else
      items_generalsettings[currentsetting].text_color = ORANGE;
}

static void set_setting_label(menu * menu_obj, unsigned currentsetting)
{
   switch(currentsetting)
   {
	   case SETTING_CHANGE_RESOLUTION:
                   set_setting_label_color(g_console.initial_resolution_id == g_console.supported_resolutions[g_console.current_resolution_index], currentsetting);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), ps3_get_resolution_label(g_console.supported_resolutions[g_console.current_resolution_index]));
		   break;
	   case SETTING_SHADER_PRESETS:
		   {
                      char fname[PATH_MAX];
                      set_setting_label_color(g_console.cgp_path == DEFAULT_PRESET_FILE, currentsetting);
		      fill_pathname_base(fname, g_console.cgp_path, sizeof(fname));
		      snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), fname);
		   }
		   break;
	   case SETTING_SHADER:
		   {
                      char fname[PATH_MAX];
		      fill_pathname_base(fname, g_settings.video.cg_shader_path, sizeof(fname));
		      snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "%s", fname);
                      set_setting_label_color(strcmp(g_settings.video.cg_shader_path,DEFAULT_SHADER_FILE) == 0, 
                      currentsetting);
		   }
		   break;
	   case SETTING_SHADER_2:
		   {
                      char fname[PATH_MAX];
		      fill_pathname_base(fname, g_settings.video.second_pass_shader, sizeof(fname));
		      snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "%s", fname);
                      set_setting_label_color(strcmp(g_settings.video.second_pass_shader,DEFAULT_SHADER_FILE) == 0,
                      currentsetting);
		   }
		   break;
	   case SETTING_FONT_SIZE:
                   set_setting_label_color(g_console.menu_font_size == 1.0f, currentsetting);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "%f", g_console.menu_font_size);
		   break;
	   case SETTING_KEEP_ASPECT_RATIO:
                   set_setting_label_color(g_console.aspect_ratio_index == ASPECT_RATIO_4_3, currentsetting);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), aspectratio_lut[g_console.aspect_ratio_index].name);
		   break;
	   case SETTING_HW_TEXTURE_FILTER:
                   set_setting_label_color(g_settings.video.smooth, currentsetting);
		   if(g_settings.video.smooth)
                      snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "Linear interpolation");
		   else
                      snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "Point filtering");
		   break;
	   case SETTING_HW_TEXTURE_FILTER_2:
                   set_setting_label_color(g_settings.video.second_pass_smooth, currentsetting);
		   if(g_settings.video.second_pass_smooth)
                      snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "Linear interpolation");
		   else
                      snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "Point filtering");
		   break;
	   case SETTING_SCALE_ENABLED:
                   set_setting_label_write_on_or_off(g_console.fbo_enabled, currentsetting);
                   set_setting_label_color(g_console.fbo_enabled, currentsetting);
		   break;
	   case SETTING_SCALE_FACTOR:
                   set_setting_label_color(g_settings.video.fbo_scale_x == 2.0f, currentsetting);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "%fx (X) / %fx (Y)", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);
		   snprintf(items_generalsettings[currentsetting].comment, sizeof(items_generalsettings[currentsetting].comment), "INFO - [Custom Scaling Factor] is set to: '%fx (X) / %fx (Y)'.", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);
		   break;
	   case SETTING_HW_OVERSCAN_AMOUNT:
                   set_setting_label_color(g_console.overscan_amount == 0.0f, currentsetting);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "%f", g_console.overscan_amount);
		   break;
	   case SETTING_THROTTLE_MODE:
                   set_setting_label_write_on_or_off(g_console.throttle_enable, currentsetting);
                   set_setting_label_color(g_console.throttle_enable, currentsetting);
		   break;
	   case SETTING_TRIPLE_BUFFERING:
                   set_setting_label_write_on_or_off(g_console.triple_buffering_enable, currentsetting);
                   set_setting_label_color(g_console.triple_buffering_enable, currentsetting);
		   break;
	   case SETTING_ENABLE_SCREENSHOTS:
                   set_setting_label_write_on_or_off(g_console.screenshots_enable, currentsetting);
                   set_setting_label_color(g_console.screenshots_enable, currentsetting);
		   break;
	   case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
	   case SETTING_DEFAULT_VIDEO_ALL:
		   break;
	   case SETTING_SOUND_MODE:
		   switch(g_console.sound_mode)
		   {
                      case SOUND_MODE_NORMAL:
                         snprintf(items_generalsettings[currentsetting].comment, sizeof(items_generalsettings[currentsetting].comment), 
                         "INFO - [Sound Output] is set to 'Normal' - normal audio output will be\nused.");
			 snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "Normal");
			 items_generalsettings[currentsetting].text_color = GREEN;
			 break;
                      case SOUND_MODE_RSOUND:
                         snprintf(items_generalsettings[currentsetting].comment, sizeof(items_generalsettings[currentsetting].comment), 
                         "INFO - [Sound Output] is set to 'RSound' - the sound will be streamed over the\n network to the RSound audio server." );
			 snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "RSound");
			 items_generalsettings[currentsetting].text_color = ORANGE;
			 break;
                      case SOUND_MODE_HEADSET:
                         snprintf(items_generalsettings[currentsetting].comment, sizeof(items_generalsettings[currentsetting].comment), 
                         "INFO - [Sound Output] is set to 'USB/Bluetooth Headset' - sound will\n be output through the headset");
			 snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "USB/Bluetooth Headset");
			 items_generalsettings[currentsetting].text_color = ORANGE;
			 break;
                      default:
                         break;
		   }
		   break;
	   case SETTING_RSOUND_SERVER_IP_ADDRESS:
                   set_setting_label_color(strcmp(g_settings.audio.device,"0.0.0.0") == 0, currentsetting);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), g_settings.audio.device);
		   break;
	   case SETTING_DEFAULT_AUDIO_ALL:
		   break;
	   case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
                   set_setting_label_color(g_extern.state_slot == 0, currentsetting);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "%d", g_extern.state_slot);
		   break;
		   /* emu-specific */
	   case SETTING_EMU_SHOW_INFO_MSG:
                   set_setting_label_write_on_or_off(g_console.info_msg_enable, currentsetting);
                   set_setting_label_color(g_console.info_msg_enable, currentsetting);
		   break;
	   case SETTING_EMU_REWIND_ENABLED:
                   set_setting_label_write_on_or_off(g_settings.rewind_enable, currentsetting);
		   if(g_settings.rewind_enable)
		   {
			   items_generalsettings[currentsetting].text_color = ORANGE;
			   snprintf(items_generalsettings[currentsetting].comment, sizeof(items_generalsettings[currentsetting].comment), "INFO - [Rewind] feature is set to 'ON'. You can rewind the game in real-time.");
		   }
		   else
		   {
			   items_generalsettings[currentsetting].text_color = GREEN;
			   snprintf(items_generalsettings[currentsetting].comment, sizeof(items_generalsettings[currentsetting].comment), "INFO - [Rewind] feature is set to 'OFF'.");
		   }
		   break;
	   case SETTING_RARCH_DEFAULT_EMU:
		   {
			   char fname[PATH_MAX];
			   fill_pathname_base(fname, g_settings.libretro, sizeof(fname));
			   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "%s", fname);

			   items_generalsettings[currentsetting].text_color = GREEN;
		   }
		   break;
	   case SETTING_EMU_AUDIO_MUTE:
                   set_setting_label_write_on_or_off(g_extern.audio_data.mute, currentsetting);
                   set_setting_label_color(!g_extern.audio_data.mute, currentsetting);
		   if(g_extern.audio_data.mute)
                      snprintf(items_generalsettings[currentsetting].comment, sizeof(items_generalsettings[currentsetting].comment), "INFO - [Audio Mute] feature is set to 'ON'. The game audio will be muted.");
		   else
                      snprintf(items_generalsettings[currentsetting].comment, sizeof(items_generalsettings[currentsetting].comment), "INFO - [Audio Mute] feature is set to 'OFF'.");
		   break;
	   case SETTING_ENABLE_CUSTOM_BGM:
                   set_setting_label_write_on_or_off(g_console.custom_bgm_enable, currentsetting);
                   set_setting_label_color(g_console.custom_bgm_enable, currentsetting);
		   break;
	   case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
                   set_setting_label_color(!(strcmp(g_console.default_rom_startup_dir, "/")), currentsetting);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), g_console.default_rom_startup_dir);
		   break;
	   case SETTING_PATH_SAVESTATES_DIRECTORY:
                   set_setting_label_color(!(strcmp(g_console.default_savestate_dir, usrDirPath)), currentsetting);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), g_console.default_savestate_dir);
		   break;
	   case SETTING_PATH_SRAM_DIRECTORY:
                   set_setting_label_color(!(strcmp(g_console.default_sram_dir, usrDirPath)), currentsetting);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), g_console.default_sram_dir);
		   break;
	   case SETTING_PATH_CHEATS:
                   set_setting_label_color(!(strcmp(g_settings.cheat_database, usrDirPath)), currentsetting);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), g_settings.cheat_database);
		   break;
	   case SETTING_PATH_SYSTEM:
                   set_setting_label_color(!(strcmp(g_settings.system_directory, systemDirPath)), currentsetting);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), g_settings.cheat_database);
		   break;
	   case SETTING_ENABLE_SRAM_PATH:
                   set_setting_label_write_on_or_off(g_console.default_sram_dir_enable, currentsetting);
                   set_setting_label_color(!g_console.default_sram_dir_enable, currentsetting);
		   break;
	   case SETTING_ENABLE_STATE_PATH:
                   set_setting_label_write_on_or_off(g_console.default_savestate_dir_enable, currentsetting);
                   set_setting_label_color(!g_console.default_savestate_dir_enable, currentsetting);
		   break;
	   case SETTING_CONTROLS_SCHEME:
                   set_setting_label_color(strcmp(g_console.input_cfg_path,"") == 0, currentsetting);
		   snprintf(items_generalsettings[currentsetting].comment, sizeof(items_generalsettings[currentsetting].comment), "INFO - Input scheme preset [%s] is selected.", g_console.input_cfg_path);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), g_console.input_cfg_path);
		   break;
	   case SETTING_CONTROLS_NUMBER:
                   set_setting_label_color(currently_selected_controller_menu == 0, currentsetting);
		   snprintf(items_generalsettings[currentsetting].comment, sizeof(items_generalsettings[currentsetting].comment), "Controller %d is currently selected.", currently_selected_controller_menu+1);
		   snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), "%d", currently_selected_controller_menu+1);
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
                      set_setting_label_color(g_settings.input.binds[currently_selected_controller_menu][currentsetting-(FIRST_CONTROL_BIND)].joykey == rarch_default_keybind_lut[currentsetting-FIRST_CONTROL_BIND], currentsetting);
		      const char * value = rarch_input_find_platform_key_label(g_settings.input.binds[currently_selected_controller_menu][currentsetting-(FIRST_CONTROL_BIND)].joykey);
		      unsigned id = currentsetting - FIRST_CONTROL_BIND;
		      snprintf(items_generalsettings[currentsetting].text, sizeof(items_generalsettings[currentsetting].text), rarch_input_get_default_keybind_name(id));
		      snprintf(items_generalsettings[currentsetting].comment, sizeof(items_generalsettings[currentsetting].comment), "INFO - [%s] on the PS3 controller is mapped to action:\n[%s].", items_generalsettings[currentsetting].text, value);
		      snprintf(items_generalsettings[currentsetting].setting_text, sizeof(items_generalsettings[currentsetting].setting_text), value);
		   }
		   break;
	   case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
	   case SETTING_CONTROLS_DEFAULT_ALL:
	   case SETTING_EMU_VIDEO_DEFAULT_ALL:
	   case SETTING_EMU_AUDIO_DEFAULT_ALL:
	   case SETTING_PATH_DEFAULT_ALL:
	   case SETTING_EMU_DEFAULT_ALL:
	   case SETTING_SAVE_SHADER_PRESET:
                   set_setting_label_color(menu_obj->selected == currentsetting, currentsetting);
		   break;
	   default:
		   break;
   }
}

static void menu_stack_decrement(void)
{
   menuStackindex--;
}

static void menu_stack_increment(void)
{
   menuStackindex++;
}

static void menu_stack_refresh (item *items, unsigned stack_idx)
{
   int page, i, j;
   float increment;
   menu *menu_obj = &menuStack[stack_idx];

   page = 0;
   j = 0;
   increment = 0.16f;

   for(i = menu_obj->first_setting; i < menu_obj->max_settings; i++)
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
      set_setting_label(menu_obj, i);
      increment += 0.03f;
      j++;
   }
}

static void menu_stack_push(unsigned stack_idx, unsigned menu_id)
{
   switch(menu_id)
   {
      case INGAME_MENU:
         strlcpy(menuStack[stack_idx].title, "Ingame Menu", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = menu_id;
         menuStack[stack_idx].selected = 0;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].category_id = CATEGORY_INGAME_MENU;
         menu_stack_refresh(ingame_menu_settings, stack_idx);
         break;
      case INGAME_MENU_RESIZE:
         strlcpy(menuStack[stack_idx].title, "Resize Menu", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = INGAME_MENU_RESIZE;
         menuStack[stack_idx].selected = 0;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].category_id = CATEGORY_INGAME_MENU;
         break;
      case INGAME_MENU_SCREENSHOT:
         strlcpy(menuStack[stack_idx].title, "Ingame Menu", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = menu_id;
         menuStack[stack_idx].selected = 0;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].category_id = CATEGORY_INGAME_MENU;
         break;
      case FILE_BROWSER_MENU:
         strlcpy(menuStack[stack_idx].title, "File Browser", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = menu_id;
         menuStack[stack_idx].selected = 0;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].category_id = CATEGORY_FILEBROWSER;
         menu_stack_refresh(items_generalsettings, stack_idx);
	 strlcpy(browser.extensions, rarch_console_get_rom_ext(), sizeof(browser.extensions));
	 filebrowser_set_root(&browser, "/");
	 filebrowser_iterate(&browser, FILEBROWSER_ACTION_RESET);
         break;
      case LIBRETRO_CHOICE:
         strlcpy(menuStack[stack_idx].title, "Libretro cores", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = menu_id;
         menuStack[stack_idx].selected = 0;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].category_id = CATEGORY_FILEBROWSER;
         menu_stack_refresh(items_generalsettings, stack_idx);
	 strlcpy(tmpBrowser.extensions, "self|SELF|bin|BIN", sizeof(tmpBrowser.extensions));
	 filebrowser_set_root(&tmpBrowser, LIBRETRO_DIR_PATH);
	 filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_RESET);
         break;
      case PRESET_CHOICE:
         strlcpy(menuStack[stack_idx].title, "Shader presets", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = menu_id;
         menuStack[stack_idx].selected = 0;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].category_id = CATEGORY_FILEBROWSER;
         menu_stack_refresh(items_generalsettings, stack_idx);
	 strlcpy(tmpBrowser.extensions, "cgp|CGP", sizeof(tmpBrowser.extensions));
	 filebrowser_set_root(&tmpBrowser, PRESETS_DIR_PATH);
	 filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_RESET);
         break;
      case INPUT_PRESET_CHOICE:
         strlcpy(menuStack[stack_idx].title, "Input presets", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = menu_id;
         menuStack[stack_idx].selected = 0;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].category_id = CATEGORY_FILEBROWSER;
         menu_stack_refresh(items_generalsettings, stack_idx);
	 strlcpy(tmpBrowser.extensions, "cfg|CFG", sizeof(tmpBrowser.extensions));
	 filebrowser_set_root(&tmpBrowser, INPUT_PRESETS_DIR_PATH);
	 filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_RESET);
         break;
      case SHADER_CHOICE:
         strlcpy(menuStack[stack_idx].title, "Shaders", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = menu_id;
         menuStack[stack_idx].selected = 0;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].category_id = CATEGORY_FILEBROWSER;
         menu_stack_refresh(items_generalsettings, stack_idx);
	 strlcpy(tmpBrowser.extensions, "cg|CG", sizeof(tmpBrowser.extensions));
	 filebrowser_set_root(&tmpBrowser, SHADERS_DIR_PATH);
	 filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_RESET);
         break;
      case BORDER_CHOICE:
         strlcpy(menuStack[stack_idx].title, "Borders", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = menu_id;
         menuStack[stack_idx].selected = 0;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].category_id = CATEGORY_FILEBROWSER;
         menu_stack_refresh(items_generalsettings, stack_idx);
	 strlcpy(tmpBrowser.extensions, "png|PNG|jpg|JPG|JPEG|jpeg", sizeof(tmpBrowser.extensions));
	 filebrowser_set_root(&tmpBrowser, BORDERS_DIR_PATH);
	 filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_RESET);
         break;
      case PATH_DEFAULT_ROM_DIR_CHOICE:
      case PATH_SAVESTATES_DIR_CHOICE:
      case PATH_SRAM_DIR_CHOICE:
      case PATH_CHEATS_DIR_CHOICE:
      case PATH_SYSTEM_DIR_CHOICE:
         strlcpy(menuStack[stack_idx].title, "Path Selection", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = menu_id;
         menuStack[stack_idx].selected = 0;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].category_id = CATEGORY_FILEBROWSER;
         menu_stack_refresh(items_generalsettings, stack_idx);
	 strlcpy(tmpBrowser.extensions, "empty", sizeof(tmpBrowser.extensions));
	 filebrowser_set_root(&tmpBrowser, "/");
	 filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_RESET);
         break;
      case GENERAL_VIDEO_MENU:
         strlcpy(menuStack[stack_idx].title, "Video", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = GENERAL_VIDEO_MENU;
         menuStack[stack_idx].selected = FIRST_VIDEO_SETTING;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].first_setting = FIRST_VIDEO_SETTING;
         menuStack[stack_idx].max_settings = MAX_NO_OF_VIDEO_SETTINGS;
         menuStack[stack_idx].category_id = CATEGORY_SETTINGS;
         menu_stack_refresh(items_generalsettings, stack_idx);
	 break;
      case GENERAL_AUDIO_MENU:
         strlcpy(menuStack[stack_idx].title, "Audio", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = GENERAL_AUDIO_MENU;
         menuStack[stack_idx].selected = FIRST_AUDIO_SETTING;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].first_setting = FIRST_AUDIO_SETTING;
         menuStack[stack_idx].max_settings = MAX_NO_OF_AUDIO_SETTINGS;
         menuStack[stack_idx].category_id = CATEGORY_SETTINGS;
         menu_stack_refresh(items_generalsettings, stack_idx);
	 break;
      case EMU_GENERAL_MENU:
         strlcpy(menuStack[stack_idx].title, "Retro", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = EMU_GENERAL_MENU;
         menuStack[stack_idx].selected = FIRST_EMU_SETTING;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].first_setting = FIRST_EMU_SETTING;
         menuStack[stack_idx].max_settings = MAX_NO_OF_EMU_SETTINGS;
         menuStack[stack_idx].category_id = CATEGORY_SETTINGS;
         menu_stack_refresh(items_generalsettings, stack_idx);
	 break;
      case EMU_VIDEO_MENU:
         strlcpy(menuStack[stack_idx].title, "Retro Video", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = EMU_VIDEO_MENU;
         menuStack[stack_idx].selected = FIRST_EMU_VIDEO_SETTING;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].first_setting = FIRST_EMU_VIDEO_SETTING;
         menuStack[stack_idx].max_settings = MAX_NO_OF_EMU_VIDEO_SETTINGS;
         menuStack[stack_idx].category_id = CATEGORY_SETTINGS;
         menu_stack_refresh(items_generalsettings, stack_idx);
	 break;
      case EMU_AUDIO_MENU:
         strlcpy(menuStack[stack_idx].title, "Retro Audio", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = EMU_AUDIO_MENU;
         menuStack[stack_idx].selected = FIRST_EMU_AUDIO_SETTING;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].first_setting = FIRST_EMU_AUDIO_SETTING;
         menuStack[stack_idx].max_settings = MAX_NO_OF_EMU_AUDIO_SETTINGS;
         menuStack[stack_idx].category_id = CATEGORY_SETTINGS;
         menu_stack_refresh(items_generalsettings, stack_idx);
	 break;
      case PATH_MENU:
         strlcpy(menuStack[stack_idx].title, "Path", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = PATH_MENU;
         menuStack[stack_idx].selected = FIRST_PATH_SETTING;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].first_setting = FIRST_PATH_SETTING;
         menuStack[stack_idx].max_settings = MAX_NO_OF_PATH_SETTINGS;
         menuStack[stack_idx].category_id = CATEGORY_SETTINGS;
         menu_stack_refresh(items_generalsettings, stack_idx);
         break;
      case CONTROLS_MENU:
         strlcpy(menuStack[stack_idx].title, "Controls", sizeof(menuStack[stack_idx].title));
         menuStack[stack_idx].enum_id = CONTROLS_MENU;
         menuStack[stack_idx].selected = FIRST_CONTROLS_SETTING_PAGE_1;
         menuStack[stack_idx].page = 0;
         menuStack[stack_idx].first_setting = FIRST_CONTROLS_SETTING_PAGE_1;
         menuStack[stack_idx].max_settings = MAX_NO_OF_CONTROLS_SETTINGS;
         menuStack[stack_idx].category_id = CATEGORY_SETTINGS;
         menu_stack_refresh(items_generalsettings, stack_idx);
	 break;
       default:
         break;
   }
}

//forward decls
extern const char *ps3_get_resolution_label(unsigned resolution);

static void display_menubar(void)
{
   struct retro_system_info info;
   menu *menu_obj = &menuStack[menuStackindex];
   gl_t *gl = driver.video_data;

   switch(menu_obj->enum_id)
   {
      case GENERAL_VIDEO_MENU:
	 cellDbgFontPrintf(0.09f, 0.03f, 0.91f, WHITE, "NEXT ->");
         break;
      case GENERAL_AUDIO_MENU:
      case EMU_GENERAL_MENU:
      case EMU_VIDEO_MENU:
      case EMU_AUDIO_MENU:
      case PATH_MENU:
	 cellDbgFontPrintf(0.09f, 0.03f, 0.91f, WHITE, "<- PREV | NEXT ->");
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
	 cellDbgFontPrintf(0.09f, 0.03f, 0.91f, WHITE, "<- PREV");
         break;
      default:
         break;
   }

   retro_get_system_info(&info);
   const char *id = info.library_name ? info.library_name : "Unknown";

   filebrowser_t *fb = &browser;

   switch(menu_obj->enum_id)
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
         cellDbgFontPrintf (0.09f, 0.09f, FONT_SIZE, YELLOW, "PATH: %s", filebrowser_get_current_dir(fb));
         break;
      default:
         break;
   }

   cellDbgFontPrintf(0.09f, 0.05f, 1.4f, WHITE, menu_obj->title);
   cellDbgFontPrintf (0.4f, 0.06f, 0.82f, WHITE, "Libretro core: %s (v%s)", id, info.library_version);
   cellDbgFontPrintf (0.8f, 0.12f, 0.82f, WHITE, "v%s", PACKAGE_VERSION);
   gl_render_msg_post(gl);
}

uint64_t state, trigger_state, held_state;
static uint64_t old_state = 0;
static uint64_t older_state = 0;

static void browser_update(filebrowser_t * b, const char *extensions)
{
   filebrowser_action_t action = FILEBROWSER_ACTION_NOOP;

   if (CTRL_LSTICK_DOWN(trigger_state))
      action = FILEBROWSER_ACTION_DOWN;
   else if (CTRL_DOWN(trigger_state))
      action = FILEBROWSER_ACTION_DOWN;
   else if (CTRL_LSTICK_UP(trigger_state))
      action = FILEBROWSER_ACTION_UP;
   else if (CTRL_UP(trigger_state))
      action = FILEBROWSER_ACTION_UP;
   else if (CTRL_RIGHT(trigger_state))
      action = FILEBROWSER_ACTION_RIGHT;
   else if (CTRL_LSTICK_RIGHT(trigger_state))
      action = FILEBROWSER_ACTION_RIGHT;
   else if (CTRL_LEFT(trigger_state))
      action = FILEBROWSER_ACTION_LEFT;
   else if (CTRL_LSTICK_LEFT(trigger_state))
      action = FILEBROWSER_ACTION_LEFT;
   else if (CTRL_R1(trigger_state))
      action = FILEBROWSER_ACTION_SCROLL_DOWN;
   else if (CTRL_R2(trigger_state))
      action = FILEBROWSER_ACTION_SCROLL_DOWN_SMOOTH;
   else if (CTRL_L2(trigger_state))
      action = FILEBROWSER_ACTION_SCROLL_UP_SMOOTH;
   else if (CTRL_L1(trigger_state))
      action = FILEBROWSER_ACTION_SCROLL_UP;
   else if (CTRL_CIRCLE(trigger_state))
      action = FILEBROWSER_ACTION_CANCEL;
   else if (CTRL_START(trigger_state))
   {
      action = FILEBROWSER_ACTION_RESET;
      filebrowser_set_root(b, "/");
      strlcpy(b->extensions, extensions, sizeof(b->extensions));
   }

   if(action != FILEBROWSER_ACTION_NOOP)
      filebrowser_iterate(b, action);
}

static void browser_render(filebrowser_t * b)
{
   gl_t *gl = driver.video_data;
   unsigned file_count = b->current_dir.list->size;
   int current_index, page_number, page_base, i;
   float currentX, currentY, ySpacing;

   current_index = b->current_dir.ptr;
   page_number = current_index / NUM_ENTRY_PER_PAGE;
   page_base = page_number * NUM_ENTRY_PER_PAGE;

   currentX = 0.09f;
   currentY = 0.10f;
   ySpacing = 0.035f;

   for ( i = page_base; i < file_count && i < page_base + NUM_ENTRY_PER_PAGE; ++i)
   {
      char fname_tmp[256];
      fill_pathname_base(fname_tmp, b->current_dir.list->elems[i].data, sizeof(fname_tmp));
      currentY = currentY + ySpacing;
      cellDbgFontPuts(currentX, currentY, FONT_SIZE, i == current_index ? RED : b->current_dir.list->elems[i].attr.b ? GREEN : WHITE, fname_tmp);
      gl_render_msg_post(gl);
   }
   gl_render_msg_post(gl);
}



static void apply_scaling (unsigned init_mode)
{
   gl_t *gl = driver.video_data;

   switch(init_mode)
   {
      case FBO_DEINIT:
         gl_deinit_fbo(gl);
	 break;
      case FBO_INIT:
	 gl_init_fbo(gl, RARCH_SCALE_BASE * INPUT_SCALE,
			 RARCH_SCALE_BASE * INPUT_SCALE);
	 break;
      case FBO_REINIT:
	 gl_deinit_fbo(gl);
	 gl_init_fbo(gl, RARCH_SCALE_BASE * INPUT_SCALE,
			 RARCH_SCALE_BASE * INPUT_SCALE);
	 break;
   }
}

static void select_file(void)
{
   unsigned menu_id = menuStack[menuStackindex].enum_id;
   char extensions[256], object[256], comment[256], path[PATH_MAX];
   gl_t * gl = driver.video_data;

   switch(menu_id)
   {
      case SHADER_CHOICE:
	 strlcpy(extensions, "cg|CG", sizeof(extensions));
	 strlcpy(object, "Shader", sizeof(object));
	 strlcpy(comment, "INFO - Select a shader from the menu by pressing the X button.", sizeof(comment));
	 break;
      case PRESET_CHOICE:
	 strlcpy(extensions, "cgp|CGP", sizeof(extensions));
	 strlcpy(object, "Shader preset", sizeof(object));
	 strlcpy(comment, "INFO - Select a shader preset from the menu by pressing the X button.", sizeof(comment));
	 break;
      case INPUT_PRESET_CHOICE:
	 strlcpy(extensions, "cfg|CFG", sizeof(extensions));
	 strlcpy(object, "Input preset", sizeof(object));
	 strlcpy(comment, "INFO - Select an input preset from the menu by pressing the X button.", sizeof(comment));
	 break;
      case BORDER_CHOICE:
	 strlcpy(extensions, "png|PNG|jpg|JPG|JPEG|jpeg", sizeof(extensions));
	 strlcpy(object, "Border image file", sizeof(object));
	 strlcpy(comment, "INFO - Select a border image file from the menu by pressing the X button.", sizeof(comment));
	 break;
      case LIBRETRO_CHOICE:
	 strlcpy(extensions, "self|SELF|bin|BIN", sizeof(extensions));
	 strlcpy(object, "Libretro core", sizeof(object));
	 strlcpy(comment, "INFO - Select a Libretro core from the menu by pressing the X button.", sizeof(comment));
	 break;
   }

   browser_update(&tmpBrowser, extensions);

      if (CTRL_CROSS(trigger_state))
      {
         if(filebrowser_get_current_path_isdir(&tmpBrowser))
	 {
            /*if 'filename' is in fact '..' - then pop back directory instead of 
	      adding '..' to filename path */
            if(tmpBrowser.current_dir.ptr == 0)
               filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_CANCEL);
	    else
               filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_OK);
	 }
	 else if (path_file_exists(filebrowser_get_current_path(&tmpBrowser)))
	 {
            snprintf(path, sizeof(path), filebrowser_get_current_path(&tmpBrowser));

	    switch(menu_id)
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
		  menu_stack_refresh(items_generalsettings, menuStackindex);
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
		  menu_stack_refresh(items_generalsettings, menuStackindex);
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
      else if (CTRL_TRIANGLE(trigger_state))
         menu_stack_decrement();

   display_menubar();

   cellDbgFontPrintf(0.09f, 0.92f, 0.92, YELLOW, "X - Select %s  /\\ - return to settings  START - Reset Startdir", object);
   cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s", comment);
   gl_render_msg_post(gl);
}

static void select_directory(void)
{
   unsigned menu_id = menuStack[menuStackindex].enum_id;
   char path[1024];
   gl_t * gl = driver.video_data;

   {
      browser_update(&tmpBrowser, "empty");

      if (CTRL_SQUARE(trigger_state))
      {
         if(filebrowser_get_current_path_isdir(&tmpBrowser))
	 {
            snprintf(path, sizeof(path), filebrowser_get_current_path(&tmpBrowser));
	    switch(menu_id)
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
      else if (CTRL_TRIANGLE(trigger_state))
      {
         strlcpy(path, usrDirPath, sizeof(path));
	 switch(menu_id)
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
      else if (CTRL_CROSS(trigger_state))
      {
         if(filebrowser_get_current_path_isdir(&tmpBrowser))
	 {
            /* if 'filename' is in fact '..' - then pop back directory instead of 
             * adding '..' to filename path */

            if(tmpBrowser.current_dir.ptr == 0)
               filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_CANCEL);
	    else
               filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_OK);
	 }
      }
   }

   display_menubar();

   cellDbgFontPuts(0.09f, 0.93f, 0.92f, YELLOW,
      "X - Enter dir  /\\ - return to settings  START - Reset Startdir");
   cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s",
      "INFO - Browse to a directory and assign it as the path by\npressing SQUARE button.");
   gl_render_msg_post(gl);
}

static void set_keybind_digital(uint64_t control_state, uint64_t default_retro_joypad_id)
{
   unsigned keybind_action = KEYBIND_NOACTION;

   if(CTRL_LEFT(control_state) | CTRL_LSTICK_LEFT(control_state))
      keybind_action = KEYBIND_DECREMENT;

   if(CTRL_RIGHT(control_state)  || CTRL_LSTICK_RIGHT(control_state) || CTRL_CROSS(control_state))
      keybind_action = KEYBIND_INCREMENT;

   if(CTRL_START(control_state))
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
	    snprintf(filepath, sizeof(filepath), "%s/%s.cgp", PRESETS_DIR_PATH, filename_tmp);
	    break;
	 case INPUT_PRESET_FILE:
	    snprintf(filepath, sizeof(filepath), "%s/%s.cfg", INPUT_PRESETS_DIR_PATH, filename_tmp);
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

static void producesettingentry(menu * menu_obj, unsigned switchvalue)
{
	switch(switchvalue)
	{
		case SETTING_CHANGE_RESOLUTION:
			if(CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) )
                           rarch_settings_change(S_RESOLUTION_NEXT);
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) )
                           rarch_settings_change(S_RESOLUTION_PREVIOUS);
			if(CTRL_CROSS(trigger_state))
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
			   if(CTRL_RIGHT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_CROSS(trigger_state) || CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state))
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
			if((CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_CROSS(trigger_state)))
			{
                           if(g_console.emulator_initialized)
			   {
                              menu_stack_increment();
                              menu_stack_push(menuStackindex, PRESET_CHOICE);
			   }
			}
			if(CTRL_START(trigger_state))
                           strlcpy(g_console.cgp_path, "", sizeof(g_console.cgp_path));
			break;
		case SETTING_SHADER:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
			   menu_stack_increment();
			   menu_stack_push(menuStackindex, SHADER_CHOICE);
			   set_shader = 0;
			}
			if(CTRL_START(trigger_state))
			{
                           rarch_load_shader(1, NULL);
			   strlcpy(g_settings.video.cg_shader_path, DEFAULT_SHADER_FILE, sizeof(g_settings.video.cg_shader_path));
			   menu_stack_refresh(items_generalsettings, menuStackindex);
			}
			break;
		case SETTING_SHADER_2:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
			   menu_stack_increment();
			   menu_stack_push(menuStackindex, SHADER_CHOICE);
			   set_shader = 1;
			}
			if(CTRL_START(trigger_state))
			{
                           rarch_load_shader(2, NULL);
			   strlcpy(g_settings.video.second_pass_shader, DEFAULT_SHADER_FILE, sizeof(g_settings.video.second_pass_shader));
			   menu_stack_refresh(items_generalsettings, menuStackindex);
			}
			break;
		case SETTING_FONT_SIZE:
			if(CTRL_LEFT(trigger_state)  || CTRL_LSTICK_LEFT(trigger_state) || CTRL_CROSS(trigger_state))
			{
				if(g_console.menu_font_size > 0) 
                                   g_console.menu_font_size -= 0.01f;
			}
			if(CTRL_RIGHT(trigger_state)  || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
				if((g_console.menu_font_size < 2.0f))
                                   g_console.menu_font_size += 0.01f;
			}
			if(CTRL_START(trigger_state))
                           g_console.menu_font_size = 1.0f;
			break;
		case SETTING_KEEP_ASPECT_RATIO:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state))
			{
                           rarch_settings_change(S_ASPECT_RATIO_DECREMENT);
			   gfx_ctx_set_aspect_ratio(NULL, g_console.aspect_ratio_index);
			}
			if(CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state))
			{
                           rarch_settings_change(S_ASPECT_RATIO_INCREMENT);
			   gfx_ctx_set_aspect_ratio(NULL, g_console.aspect_ratio_index);
			}
			if(CTRL_START(trigger_state))
			{
                           rarch_settings_default(S_DEF_ASPECT_RATIO);
			   gfx_ctx_set_aspect_ratio(NULL, g_console.aspect_ratio_index);
			}
			break;
		case SETTING_HW_TEXTURE_FILTER:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
                           rarch_settings_change(S_HW_TEXTURE_FILTER);
			   gfx_ctx_set_filtering(1, g_settings.video.smooth);
			}
			if(CTRL_START(trigger_state))
			{
                           rarch_settings_change(S_DEF_HW_TEXTURE_FILTER);
			   gfx_ctx_set_filtering(1, g_settings.video.smooth);
			}
			break;
		case SETTING_HW_TEXTURE_FILTER_2:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
                                rarch_settings_change(S_HW_TEXTURE_FILTER_2);
				gfx_ctx_set_filtering(2, g_settings.video.second_pass_smooth);
			}
			if(CTRL_START(trigger_state))
			{
                                rarch_settings_change(S_DEF_HW_TEXTURE_FILTER_2);
				gfx_ctx_set_filtering(2, g_settings.video.second_pass_smooth);
			}
			break;
		case SETTING_SCALE_ENABLED:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
                                rarch_settings_change(S_SCALE_ENABLED);
				gfx_ctx_set_fbo(g_console.fbo_enabled);
			}
			if(CTRL_START(trigger_state))
			{
                                rarch_settings_default(S_DEF_SCALE_ENABLED);
				apply_scaling(FBO_DEINIT);
				apply_scaling(FBO_INIT);
			}
			break;
		case SETTING_SCALE_FACTOR:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state))
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
			if(CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
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
			if(CTRL_START(trigger_state))
			{
                           rarch_settings_default(S_DEF_SCALE_FACTOR);
			   apply_scaling(FBO_DEINIT);
			   apply_scaling(FBO_INIT);
			}
			break;
		case SETTING_HW_OVERSCAN_AMOUNT:
			if(CTRL_LEFT(trigger_state)  ||  CTRL_LSTICK_LEFT(trigger_state) || CTRL_CROSS(trigger_state))
			{
				rarch_settings_change(S_OVERSCAN_DECREMENT);
				gfx_ctx_set_overscan();
			}
			if(CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
				rarch_settings_change(S_OVERSCAN_INCREMENT);
				gfx_ctx_set_overscan();
			}
			if(CTRL_START(trigger_state))
			{
				rarch_settings_default(S_DEF_OVERSCAN);
				gfx_ctx_set_overscan();
			}
			break;
		case SETTING_THROTTLE_MODE:
			if(CTRL_LEFT(trigger_state)  || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state))
			{
				rarch_settings_change(S_THROTTLE);
				gfx_ctx_set_swap_interval(g_console.throttle_enable, true);
			}
			if(CTRL_START(trigger_state))
			{
				rarch_settings_default(S_DEF_THROTTLE);
				gfx_ctx_set_swap_interval(g_console.throttle_enable, true);
			}
			break;
		case SETTING_TRIPLE_BUFFERING:
			if(CTRL_LEFT(trigger_state)  || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state))
			{
				rarch_settings_change(S_TRIPLE_BUFFERING);
				video_gl.restart();
			}
			if(CTRL_START(trigger_state))
			{
				bool old_buffer_trigger_state = g_console.triple_buffering_enable;
				rarch_settings_default(S_DEF_TRIPLE_BUFFERING);

				if(!old_buffer_trigger_state)
                                   video_gl.restart();
			}
			break;
		case SETTING_ENABLE_SCREENSHOTS:
			if(CTRL_LEFT(trigger_state)  || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state))
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
			if(CTRL_START(trigger_state))
			{
#if(CELL_SDK_VERSION > 0x340000)
                           g_console.screenshots_enable = true;
#endif
			}
			break;
		case SETTING_SAVE_SHADER_PRESET:
			if(CTRL_LEFT(trigger_state)  || CTRL_LSTICK_LEFT(trigger_state)  || CTRL_RIGHT(trigger_state) | CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
                           rarch_filename_input_and_save(SHADER_PRESET_FILE);
			break;
		case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
			break;
		case SETTING_DEFAULT_VIDEO_ALL:
			break;
		case SETTING_SOUND_MODE:
			if(CTRL_LEFT(trigger_state) ||  CTRL_LSTICK_LEFT(trigger_state))
			{
                           if(g_console.sound_mode != SOUND_MODE_NORMAL)
                              g_console.sound_mode--;
			}
			if(CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
                           if(g_console.sound_mode < SOUND_MODE_HEADSET)
                              g_console.sound_mode++;
			}
			if(CTRL_UP(trigger_state) || CTRL_LSTICK_UP(trigger_state) || CTRL_DOWN(trigger_state) || CTRL_LSTICK_DOWN(trigger_state))
			{
                           if(g_console.sound_mode != SOUND_MODE_RSOUND)
                              rarch_console_rsound_stop();
			   else
                              rarch_console_rsound_start(g_settings.audio.device);
			}
			if(CTRL_START(trigger_state))
			{
                           g_console.sound_mode = SOUND_MODE_NORMAL;
			   rarch_console_rsound_stop();
			}
			break;
		case SETTING_RSOUND_SERVER_IP_ADDRESS:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_CROSS(trigger_state) | CTRL_LSTICK_RIGHT(trigger_state) )
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
			if(CTRL_START(trigger_state))
                           strlcpy(g_settings.audio.device, "0.0.0.0", sizeof(g_settings.audio.device));
			break;
		case SETTING_DEFAULT_AUDIO_ALL:
			break;
		case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_CROSS(trigger_state))
                           rarch_settings_change(S_SAVESTATE_DECREMENT);
			if(CTRL_RIGHT(trigger_state)  || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
                           rarch_settings_change(S_SAVESTATE_INCREMENT);

			if(CTRL_START(trigger_state))
                           rarch_settings_default(S_DEF_SAVE_STATE);
			break;
		case SETTING_EMU_SHOW_INFO_MSG:
			if(CTRL_LEFT(trigger_state)  || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state))
                           g_console.info_msg_enable = !g_console.info_msg_enable;
			if(CTRL_START(trigger_state))
                           g_console.info_msg_enable = true;
			break;
		case SETTING_EMU_REWIND_ENABLED:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
                           rarch_settings_change(S_REWIND);

			   if(g_console.info_msg_enable)
                              rarch_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
			}
			if(CTRL_START(trigger_state))
                           g_settings.rewind_enable = false;
			break;
		case SETTING_RARCH_DEFAULT_EMU:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
			   menu_stack_increment();
			   menu_stack_push(menuStackindex, LIBRETRO_CHOICE);
			   set_libretro_core_as_launch = false;
			}
			if(CTRL_START(trigger_state))
			{
			}
			break;
		case SETTING_EMU_AUDIO_MUTE:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
                           g_extern.audio_data.mute = !g_extern.audio_data.mute;
			}
			if(CTRL_START(trigger_state))
                           g_extern.audio_data.mute = false;
			break;
		case SETTING_ENABLE_CUSTOM_BGM:
			if(CTRL_LEFT(trigger_state)  || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state))
			{
#if(CELL_SDK_VERSION > 0x340000)
                           g_console.custom_bgm_enable = !g_console.custom_bgm_enable;
			   if(g_console.custom_bgm_enable)
                              cellSysutilEnableBgmPlayback();
			   else
                              cellSysutilDisableBgmPlayback();

#endif
			}
			if(CTRL_START(trigger_state))
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
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
			   menu_stack_increment();
			   menu_stack_push(menuStackindex, PATH_DEFAULT_ROM_DIR_CHOICE);
			}

			if(CTRL_START(trigger_state))
				strlcpy(g_console.default_rom_startup_dir, "/", sizeof(g_console.default_rom_startup_dir));
			break;
		case SETTING_PATH_SAVESTATES_DIRECTORY:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
			   menu_stack_increment();
                           menu_stack_push(menuStackindex, PATH_SAVESTATES_DIR_CHOICE);
			}

			if(CTRL_START(trigger_state))
                           strlcpy(g_console.default_savestate_dir, usrDirPath, sizeof(g_console.default_savestate_dir));

			break;
		case SETTING_PATH_SRAM_DIRECTORY:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) ||  CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
			   menu_stack_increment();
                           menu_stack_push(menuStackindex, PATH_SRAM_DIR_CHOICE);
			}

			if(CTRL_START(trigger_state))
                           strlcpy(g_console.default_sram_dir, "", sizeof(g_console.default_sram_dir));
			break;
		case SETTING_PATH_CHEATS:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) ||  CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
			   menu_stack_increment();
                           menu_stack_push(menuStackindex, PATH_CHEATS_DIR_CHOICE);
			}

			if(CTRL_START(trigger_state))
                           strlcpy(g_settings.cheat_database, usrDirPath, sizeof(g_settings.cheat_database));
			break;
		case SETTING_PATH_SYSTEM:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) ||  CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
			   menu_stack_increment();
                           menu_stack_push(menuStackindex, PATH_SYSTEM_DIR_CHOICE);
			}

			if(CTRL_START(trigger_state))
                           strlcpy(g_settings.system_directory, systemDirPath, sizeof(g_settings.system_directory));
			break;
		case SETTING_ENABLE_SRAM_PATH:
			if(CTRL_LEFT(trigger_state)  || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state))
			{
                           g_console.default_sram_dir_enable = !g_console.default_sram_dir_enable;
			   menu_stack_refresh(items_generalsettings, menuStackindex);
			}
			if(CTRL_START(trigger_state))
			{
                           g_console.default_sram_dir_enable = true;
			   menu_stack_refresh(items_generalsettings, menuStackindex);
			}
			break;
		case SETTING_ENABLE_STATE_PATH:
			if(CTRL_LEFT(trigger_state)  || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state))
			{
                           g_console.default_savestate_dir_enable = !g_console.default_savestate_dir_enable;
			   menu_stack_refresh(items_generalsettings, menuStackindex);
			}
			if(CTRL_START(trigger_state))
			{
                           g_console.default_savestate_dir_enable = true;
			   menu_stack_refresh(items_generalsettings, menuStackindex);
			}
			break;
		case SETTING_PATH_DEFAULT_ALL:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) ||  CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state) || CTRL_START(trigger_state))
			{
                           strlcpy(g_console.default_rom_startup_dir, "/", sizeof(g_console.default_rom_startup_dir));
			   strlcpy(g_console.default_savestate_dir, usrDirPath, sizeof(g_console.default_savestate_dir));
			   strlcpy(g_settings.cheat_database, usrDirPath, sizeof(g_settings.cheat_database));
			   strlcpy(g_console.default_sram_dir, "", sizeof(g_console.default_sram_dir));

			   menu_stack_refresh(items_generalsettings, menuStackindex);
			}
			break;
		case SETTING_CONTROLS_SCHEME:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_CROSS(trigger_state) | CTRL_RIGHT(trigger_state)  || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
                           menu_stack_increment();
                           menu_stack_push(menuStackindex, INPUT_PRESET_CHOICE);
			}
			if(CTRL_START(trigger_state))
			   menu_stack_refresh(items_generalsettings, menuStackindex);
			break;
		case SETTING_CONTROLS_NUMBER:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_CROSS(trigger_state))
			{
                           if(currently_selected_controller_menu != 0)
                              currently_selected_controller_menu--;
			   menu_stack_refresh(items_generalsettings, menuStackindex);
			}

			if(CTRL_RIGHT(trigger_state)  || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state))
			{
                           if(currently_selected_controller_menu < 6)
                              currently_selected_controller_menu++;
			   menu_stack_refresh(items_generalsettings, menuStackindex);
			}

			if(CTRL_START(trigger_state))
                           currently_selected_controller_menu = 0;
			break; 
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_UP:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_UP);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_DOWN:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_DOWN);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_LEFT:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_LEFT);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_RIGHT:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_RIGHT);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_A:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_A);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_B);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_X:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_X);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_Y:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_Y);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_SELECT:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_SELECT);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_START:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_START);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_L);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_R);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L2:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_L2);
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R2:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_R2);
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L3:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_L3);
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R3:
			set_keybind_digital(trigger_state, RETRO_DEVICE_ID_JOYPAD_R3);
			break;
		case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) ||  CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state) || CTRL_START(trigger_state))
                           rarch_filename_input_and_save(INPUT_PRESET_FILE);
			break;
		case SETTING_CONTROLS_DEFAULT_ALL:
			if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_RIGHT(trigger_state) ||  CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state) || CTRL_START(trigger_state))
			{
                           rarch_input_set_default_keybinds(currently_selected_controller_menu);
			   menu_stack_refresh(items_generalsettings, menuStackindex);
			}
			break;
	}

	set_setting_label(menu_obj, switchvalue);
}

static void settings_iterate(menu * menu_obj, settings_action_t action)
{
   switch(action)
   {
      case SETTINGS_ACTION_DOWN:
         menu_obj->selected++;

	 if (menu_obj->selected >= menu_obj->max_settings)
            menu_obj->selected = menu_obj->first_setting; 

	 if (items_generalsettings[menu_obj->selected].page != menu_obj->page)
            menu_obj->page = items_generalsettings[menu_obj->selected].page;
         break;
      case SETTINGS_ACTION_UP:
         if (menu_obj->selected == menu_obj->first_setting)
            menu_obj->selected = menu_obj->max_settings-1;
	 else
            menu_obj->selected--;

	 if (items_generalsettings[menu_obj->selected].page != menu_obj->page)
            menu_obj->page = items_generalsettings[menu_obj->selected].page;
         break;
      case SETTINGS_ACTION_TAB_PREVIOUS:
	 menu_stack_decrement();
         break;
      case SETTINGS_ACTION_TAB_NEXT:
         switch(menu_obj->enum_id)
	 {
            case GENERAL_VIDEO_MENU:
	    case GENERAL_AUDIO_MENU:
	    case EMU_GENERAL_MENU:
	    case EMU_VIDEO_MENU:
	    case EMU_AUDIO_MENU:
	    case PATH_MENU:
	       menu_stack_increment();
               menu_stack_push(menuStackindex, menu_obj->enum_id+1);
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

static void select_setting(void)
{
   unsigned i;
   gl_t * gl = driver.video_data;
   menu *menu_obj = &menuStack[menuStackindex];

   settings_action_t action = SETTINGS_ACTION_NOOP;

   /* back to ROM menu if CIRCLE is pressed */
   if (CTRL_L1(trigger_state) || CTRL_CIRCLE(trigger_state))
      action = SETTINGS_ACTION_TAB_PREVIOUS;
   else if (CTRL_R1(trigger_state))
      action = SETTINGS_ACTION_TAB_NEXT;
   else if (CTRL_DOWN(trigger_state) || CTRL_LSTICK_DOWN(trigger_state))
      action = SETTINGS_ACTION_DOWN;
   else if (CTRL_UP(trigger_state) || CTRL_LSTICK_UP(trigger_state))
      action = SETTINGS_ACTION_UP;

   if(action != SETTINGS_ACTION_NOOP)
	   settings_iterate(menu_obj, action);

   producesettingentry(menu_obj, menu_obj->selected);

   display_menubar();
   gl_render_msg_post(gl);


   for (i = menu_obj->first_setting; i < menu_obj->max_settings; i++)
   {
      if(items_generalsettings[i].page == menu_obj->page)
      {
         cellDbgFontPuts(items_generalsettings[i].text_xpos, items_generalsettings[i].text_ypos, FONT_SIZE, menu_obj->selected == items_generalsettings[i].enum_id ? YELLOW : items_generalsettings[i].item_color, items_generalsettings[i].text);
	 cellDbgFontPuts(0.5f, items_generalsettings[i].text_ypos, FONT_SIZE, items_generalsettings[i].text_color, items_generalsettings[i].setting_text);
	 gl_render_msg_post(gl);
      }
   }

   cellDbgFontPuts(0.09f, items_generalsettings[menu_obj->selected].comment_ypos, 0.86f, LIGHTBLUE, items_generalsettings[menu_obj->selected].comment);

   cellDbgFontPuts(0.09f, 0.91f, FONT_SIZE, YELLOW, "UP/DOWN - select  L3+R3 - resume game   X/LEFT/RIGHT - change");
   cellDbgFontPuts(0.09f, 0.95f, FONT_SIZE, YELLOW, "START - default   L1/CIRCLE - go back   R1 - go forward");
   gl_render_msg_post(gl);
}

static void menu_romselect_iterate(filebrowser_t *filebrowser, menu_romselect_action_t action)
{
   switch(action)
   {
      case MENU_ROMSELECT_ACTION_OK:
         if(filebrowser_get_current_path_isdir(filebrowser))
	 {
            /*if 'filename' is in fact '..' - then pop back directory  instead of adding '..' to filename path */
            if(browser.current_dir.ptr == 0)
               filebrowser_iterate(filebrowser, FILEBROWSER_ACTION_CANCEL);
	    else
               filebrowser_iterate(filebrowser, FILEBROWSER_ACTION_OK);
	 }
	 else
	 {
            char rom_path_temp[PATH_MAX];
	    struct retro_system_info info;
	    retro_get_system_info(&info);
	    bool block_zip_extract  = info.block_extract;

	    snprintf(rom_path_temp, sizeof(rom_path_temp), filebrowser_get_current_path(filebrowser));

	    if((strstr(rom_path_temp, ".zip") || strstr(rom_path_temp, ".ZIP")) && !block_zip_extract)
               rarch_extract_zipfile(rom_path_temp);
	    else
	    {
               rarch_console_load_game(filebrowser_get_current_path(filebrowser));
	       rarch_settings_msg(S_MSG_LOADING_ROM, S_DELAY_45);
	    }
	 }
         break;
      case MENU_ROMSELECT_ACTION_GOTO_SETTINGS:
	 menu_stack_increment();
         menu_stack_push(menuStackindex, GENERAL_VIDEO_MENU);
         break;
      default:
         break;
   }
}

static void select_rom(void)
{
   gl_t * gl = driver.video_data;

   browser_update(&browser, rarch_console_get_rom_ext());
   menu_romselect_action_t action = MENU_ROMSELECT_ACTION_NOOP;

   if (CTRL_SELECT(trigger_state))
      action = MENU_ROMSELECT_ACTION_GOTO_SETTINGS;
   else if (CTRL_CROSS(trigger_state))
      action = MENU_ROMSELECT_ACTION_OK;

   if (action != MENU_ROMSELECT_ACTION_NOOP)
      menu_romselect_iterate(&browser, action);

   if (filebrowser_get_current_path_isdir(&browser))
   {
      if(!strcmp(filebrowser_get_current_path(&browser),"app_home") || !strcmp(filebrowser_get_current_path(&browser),"host_root"))
         cellDbgFontPrintf(0.09f, 0.83f, 0.91f, RED, "WARNING - This path only works on DEX PS3 systems. Do not attempt to open\n this directory on CEX PS3 systems, or you might have to restart.");
      else if(!strcmp(filebrowser_get_current_path(&browser),".."))
         cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to go back to the previous directory.");
      else
         cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to enter the directory.");
   }

   if (path_file_exists(filebrowser_get_current_path(&browser)))
      cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to load the game. ");

   display_menubar();

   cellDbgFontPuts   (0.09f, 0.91f, FONT_SIZE, YELLOW,
   "L3 + R3 - resume game           SELECT - Settings screen");
   gl_render_msg_post(gl);
}


static void ingame_menu_resize(void)
{
   gl_t * gl = driver.video_data;

   float x_position = 0.09f;
   float font_size = 1.1f;
   float ypos = 0.16f;
   float ypos_increment = 0.035f;

   g_console.aspect_ratio_index = ASPECT_RATIO_CUSTOM;
   gfx_ctx_set_aspect_ratio(NULL, g_console.aspect_ratio_index);

   if(CTRL_LSTICK_LEFT(state) || CTRL_LEFT(state))
      g_console.viewports.custom_vp.x -= 1;
   else if (CTRL_LSTICK_RIGHT(state) || CTRL_RIGHT(state))
      g_console.viewports.custom_vp.x += 1;

   if (CTRL_LSTICK_UP(state) || CTRL_UP(state))
      g_console.viewports.custom_vp.y += 1;
   else if (CTRL_LSTICK_DOWN(state) || CTRL_DOWN(state)) 
      g_console.viewports.custom_vp.y -= 1;

   if (CTRL_RSTICK_LEFT(state) || CTRL_L1(state))
      g_console.viewports.custom_vp.width -= 1;
   else if (CTRL_RSTICK_RIGHT(state) || CTRL_R1(state))
      g_console.viewports.custom_vp.width += 1;

   if (CTRL_RSTICK_UP(state) || CTRL_L2(state))
      g_console.viewports.custom_vp.height += 1;
   else if (CTRL_RSTICK_DOWN(state) || CTRL_R2(state))
      g_console.viewports.custom_vp.height -= 1;

   if (CTRL_TRIANGLE(trigger_state))
   {
      g_console.viewports.custom_vp.x = 0;
      g_console.viewports.custom_vp.y = 0;
      g_console.viewports.custom_vp.width = gl->win_width;
      g_console.viewports.custom_vp.height = gl->win_height;
   }

   if(CTRL_CIRCLE(trigger_state))
      menu_stack_decrement();

   if(CTRL_SQUARE(~trigger_state))
   {
      display_menubar();

      cellDbgFontPrintf(x_position, ypos, font_size, GREEN, "Viewport X: #%d", g_console.viewports.custom_vp.x);

      cellDbgFontPrintf(x_position, ypos+(ypos_increment*1), font_size, GREEN, "Viewport Y: #%d", g_console.viewports.custom_vp.y);

      cellDbgFontPrintf(x_position, ypos+(ypos_increment*2), font_size, GREEN, "Viewport Width: #%d", g_console.viewports.custom_vp.width);

      cellDbgFontPrintf(x_position, ypos+(ypos_increment*3), font_size, GREEN, "Viewport Height: #%d", g_console.viewports.custom_vp.height);

      cellDbgFontPrintf (0.09f, 0.40f, font_size, LIGHTBLUE, "CONTROLS:");

      cellDbgFontPrintf (0.09f, 0.46f, font_size,  LIGHTBLUE, "LEFT or LSTICK UP");
      cellDbgFontPrintf (0.5f, 0.46f, font_size, LIGHTBLUE, "- Decrease Viewport X");

      gl_render_msg_post(gl);

      cellDbgFontPrintf (0.09f, 0.48f, font_size, LIGHTBLUE, "RIGHT or LSTICK RIGHT");
      cellDbgFontPrintf (0.5f, 0.48f, font_size, LIGHTBLUE, "- Increase Viewport X");

      cellDbgFontPrintf (0.09f, 0.50f, font_size, LIGHTBLUE, "UP or LSTICK UP");
      cellDbgFontPrintf (0.5f, 0.50f, font_size, LIGHTBLUE, "- Increase Viewport Y");

      gl_render_msg_post(gl);

      cellDbgFontPrintf (0.09f, 0.52f, font_size, LIGHTBLUE, "DOWN or LSTICK DOWN");
      cellDbgFontPrintf (0.5f, 0.52f, font_size, LIGHTBLUE, "- Decrease Viewport Y");

      cellDbgFontPrintf (0.09f, 0.54f, font_size, LIGHTBLUE, "L1 or RSTICK LEFT");
      cellDbgFontPrintf (0.5f, 0.54f, font_size, LIGHTBLUE, "- Decrease Viewport Width");

      gl_render_msg_post(gl);

      cellDbgFontPrintf (0.09f, 0.56f, font_size, LIGHTBLUE, "R1 or RSTICK RIGHT");
      cellDbgFontPrintf (0.5f, 0.56f, font_size, LIGHTBLUE, "- Increase Viewport Width");

      cellDbgFontPrintf (0.09f, 0.58f, font_size, LIGHTBLUE, "L2 or  RSTICK UP");
      cellDbgFontPrintf (0.5f, 0.58f, font_size, LIGHTBLUE, "- Increase Viewport Height");

      gl_render_msg_post(gl);

      cellDbgFontPrintf (0.09f, 0.60f, font_size, LIGHTBLUE, "R2 or RSTICK DOWN");
      cellDbgFontPrintf (0.5f, 0.60f, font_size, LIGHTBLUE, "- Decrease Viewport Height");

      cellDbgFontPrintf (0.09f, 0.66f, font_size, LIGHTBLUE, "TRIANGLE");
      cellDbgFontPrintf (0.5f, 0.66f, font_size, LIGHTBLUE, "- Reset To Defaults");

      cellDbgFontPrintf (0.09f, 0.68f, font_size, LIGHTBLUE, "SQUARE");
      cellDbgFontPrintf (0.5f, 0.68f, font_size, LIGHTBLUE, "- Show Game Screen");

      cellDbgFontPrintf (0.09f, 0.70f, font_size, LIGHTBLUE, "CIRCLE");
      cellDbgFontPrintf (0.5f, 0.70f, font_size, LIGHTBLUE, "- Return to Ingame Menu");

      gl_render_msg_post(gl);

      cellDbgFontPrintf (0.09f, 0.83f, 0.91f, LIGHTBLUE, "Allows you to resize the screen by moving around the two analog sticks.\nPress TRIANGLE to reset to default values, and CIRCLE to go back to the menu.");
      gl_render_msg_post(gl);
   }
}

static void ingame_menu_screenshot(void)
{
   gl_t * gl = driver.video_data;

   if(g_console.ingame_menu_enable)
   {
      if(CTRL_CIRCLE(trigger_state))
      {
	 menu_stack_decrement();
	 gl->menu_render = true;
      }
   }
}

static void ingame_menu(void)
{
   char comment[256];
   static unsigned menuitem_colors[MENU_ITEM_LAST];
   gl_t * gl = driver.video_data;
   menu *menu_obj = &menuStack[menuStackindex];

   float x_position = 0.09f;
   float font_size = 1.1f;
   float ypos = 0.16f;
   float ypos_increment = 0.035f;

   for(int i = 0; i < MENU_ITEM_LAST; i++)
      menuitem_colors[i] = GREEN;

   menuitem_colors[g_console.ingame_menu_item] = RED;

   if(CTRL_CIRCLE(trigger_state))
      rarch_settings_change(S_RETURN_TO_GAME);

      switch(g_console.ingame_menu_item)
      {
         case MENU_ITEM_LOAD_STATE:
            if(CTRL_CROSS(trigger_state))
	    {
               rarch_load_state();
               rarch_settings_change(S_RETURN_TO_GAME);
	    }
	    if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state))
               rarch_state_slot_decrease();
	    if(CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state))
               rarch_state_slot_increase();

	    strlcpy(comment, "Press LEFT or RIGHT to change the current save state slot.\nPress CROSS to load the state from the currently selected save state slot.", sizeof(comment));
	    break;
	 case MENU_ITEM_SAVE_STATE:
	    if(CTRL_CROSS(trigger_state))
	    {
               rarch_save_state();
               rarch_settings_change(S_RETURN_TO_GAME);
	    }
	    if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state))
               rarch_state_slot_decrease();
	    if(CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state))
               rarch_state_slot_increase();

	    strlcpy(comment, "Press LEFT or RIGHT to change the current save state slot.\nPress CROSS to save the state to the currently selected save state slot.", sizeof(comment));
	    break;
	 case MENU_ITEM_KEEP_ASPECT_RATIO:
            producesettingentry(menu_obj, SETTING_KEEP_ASPECT_RATIO);
	    strlcpy(comment, "Press LEFT or RIGHT to change the [Aspect Ratio].\nPress START to reset back to default values.", sizeof(comment));
	    break;
	 case MENU_ITEM_OVERSCAN_AMOUNT:
            producesettingentry(menu_obj, SETTING_HW_OVERSCAN_AMOUNT);
	    strlcpy(comment, "Press LEFT or RIGHT to change the [Overscan] settings.\nPress START to reset back to default values.", sizeof(comment));
	    break;
	 case MENU_ITEM_ORIENTATION:
	    if(CTRL_LEFT(trigger_state) || CTRL_LSTICK_LEFT(trigger_state) || CTRL_CROSS(trigger_state) || CTRL_LSTICK_LEFT(trigger_state))
	    {
               rarch_settings_change(S_ROTATION_DECREMENT);
	       video_gl.set_rotation(NULL, g_console.screen_orientation);
	    }

	    if(CTRL_RIGHT(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state) || CTRL_CROSS(trigger_state) || CTRL_LSTICK_RIGHT(trigger_state))
	    {
               rarch_settings_change(S_ROTATION_INCREMENT);
	       video_gl.set_rotation(NULL, g_console.screen_orientation);
	    }

	    if(CTRL_START(trigger_state))
	    {
               rarch_settings_default(S_DEF_ROTATION);
	       video_gl.set_rotation(NULL, g_console.screen_orientation);
	    }
	    strlcpy(comment, "Press LEFT or RIGHT to change the [Orientation] settings.\nPress START to reset back to default values.", sizeof(comment));
	    break;
	 case MENU_ITEM_SCALE_FACTOR:
            producesettingentry(menu_obj, SETTING_SCALE_FACTOR);
	    strlcpy(comment, "Press LEFT or RIGHT to change the [Scaling] settings.\nPress START to reset back to default values.", sizeof(comment));
	    break;
	 case MENU_ITEM_FRAME_ADVANCE:
	    if(CTRL_CROSS(trigger_state) || CTRL_R2(trigger_state) || CTRL_L2(trigger_state))
	    {
               rarch_settings_change(S_FRAME_ADVANCE);
	       g_console.ingame_menu_item = MENU_ITEM_FRAME_ADVANCE;
	    }
	    strlcpy(comment, "Press 'CROSS', 'L2' or 'R2' button to step one frame. Pressing the button\nrapidly will advance the frame more slowly.", sizeof(comment));
	    break;
	 case MENU_ITEM_RESIZE_MODE:
	    if(CTRL_CROSS(trigger_state))
            {
               menu_stack_increment();
	       menu_stack_push(menuStackindex, INGAME_MENU_RESIZE);
            }
	    strlcpy(comment, "Allows you to resize the screen by moving around the two analog sticks.\nPress TRIANGLE to reset to default values, and CIRCLE to go back.", sizeof(comment));
	    break;
	 case MENU_ITEM_SCREENSHOT_MODE:
	    if(CTRL_CROSS(trigger_state))
	    {
               menu_stack_increment();
	       menu_stack_push(menuStackindex, INGAME_MENU_SCREENSHOT);
	    }
	    strlcpy(comment, "Allows you to take a screenshot without any text clutter.\nPress CIRCLE to go back to the in-game menu while in 'Screenshot Mode'.", sizeof(comment));
	    break;
	 case MENU_ITEM_RETURN_TO_GAME:
	    if(CTRL_CROSS(trigger_state))
               rarch_settings_change(S_RETURN_TO_GAME);

	    strlcpy(comment, "Press 'CROSS' to return back to the game.", sizeof(comment));
	    break;
	 case MENU_ITEM_RESET:
	    if(CTRL_CROSS(trigger_state))
	    {
               rarch_settings_change(S_RETURN_TO_GAME);
	       rarch_game_reset();
	    }
	    strlcpy(comment, "Press 'CROSS' to reset the game.", sizeof(comment));
	    break;
	 case MENU_ITEM_RETURN_TO_MENU:
	    if(CTRL_CROSS(trigger_state))
	    {
               rarch_settings_change(S_RETURN_TO_MENU);
	    }
	    strlcpy(comment, "Press 'CROSS' to return to the ROM Browser menu.", sizeof(comment));
	    break;
	 case MENU_ITEM_CHANGE_LIBRETRO:
	    if(CTRL_CROSS(trigger_state))
	    {
               menu_stack_increment();
	       menu_stack_push(menuStackindex, LIBRETRO_CHOICE);
	       set_libretro_core_as_launch = true;
	    }
	    strlcpy(comment, "Press 'CROSS' to choose a different emulator core.", sizeof(comment));
	    break;
#ifdef HAVE_MULTIMAN
	 case MENU_ITEM_RETURN_TO_MULTIMAN:
	    if(CTRL_CROSS(trigger_state) && path_file_exists(MULTIMAN_EXECUTABLE))
	    {
               strlcpy(g_console.launch_app_on_exit, MULTIMAN_EXECUTABLE,
                  sizeof(g_console.launch_app_on_exit));

               rarch_settings_change(S_RETURN_TO_DASHBOARD);
	    }
	    strlcpy(comment, "Press 'CROSS' to quit the emulator and return to multiMAN.", sizeof(comment));
	    break;
#endif
	 case MENU_ITEM_RETURN_TO_DASHBOARD:
	    if(CTRL_CROSS(trigger_state))
               rarch_settings_change(S_RETURN_TO_DASHBOARD);

	    strlcpy(comment, "Press 'CROSS' to quit the emulator and return to the XMB.", sizeof(comment));
	    break;
      }

   if(CTRL_UP(trigger_state) || CTRL_LSTICK_UP(trigger_state))
   {
      if(g_console.ingame_menu_item > 0)
         g_console.ingame_menu_item--;
   }

   if(CTRL_DOWN(trigger_state) || CTRL_LSTICK_DOWN(trigger_state))
   {
      if(g_console.ingame_menu_item < (MENU_ITEM_LAST-1))
         g_console.ingame_menu_item++;
   }

   if(CTRL_L3(trigger_state) && CTRL_R3(trigger_state))
      rarch_settings_change(S_RETURN_TO_GAME);

   display_menubar();

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
   cellDbgFontPrintf(x_position, ypos, font_size, MENU_ITEM_SELECTED(MENU_ITEM_LOAD_STATE), strw_buffer);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
   cellDbgFontPrintf(x_position, ypos+(ypos_increment*MENU_ITEM_SAVE_STATE), font_size, MENU_ITEM_SELECTED(MENU_ITEM_SAVE_STATE), strw_buffer);
   gl_render_msg_post(gl);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
   cellDbgFontPrintf(x_position, (ypos+(ypos_increment*MENU_ITEM_KEEP_ASPECT_RATIO)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_KEEP_ASPECT_RATIO), strw_buffer);

   cellDbgFontPrintf(x_position, (ypos+(ypos_increment*MENU_ITEM_OVERSCAN_AMOUNT)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_OVERSCAN_AMOUNT), "Overscan: %f", g_console.overscan_amount);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
   cellDbgFontPrintf (x_position, (ypos+(ypos_increment*MENU_ITEM_ORIENTATION)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_ORIENTATION), strw_buffer);
   gl_render_msg_post(gl);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SCALE_FACTOR, sizeof(strw_buffer));
   cellDbgFontPrintf (x_position, (ypos+(ypos_increment*MENU_ITEM_SCALE_FACTOR)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_SCALE_FACTOR), strw_buffer);
   gl_render_msg_post(gl);

   cellDbgFontPrintf(x_position, (ypos+(ypos_increment*MENU_ITEM_RESIZE_MODE)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RESIZE_MODE), "Resize Mode");

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_FRAME_ADVANCE)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_FRAME_ADVANCE), "Frame Advance");

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_SCREENSHOT_MODE)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_SCREENSHOT_MODE), "Screenshot Mode");

   gl_render_msg_post(gl);

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RESET)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RESET), "Reset");

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_GAME)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_GAME), "Return to Game");
   gl_render_msg_post(gl);

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MENU)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_MENU), "Return to Menu");
   gl_render_msg_post(gl);

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_CHANGE_LIBRETRO)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_CHANGE_LIBRETRO), "Change libretro core");
   gl_render_msg_post(gl);

#ifdef HAVE_MULTIMAN
   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MULTIMAN)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_MULTIMAN), "Return to multiMAN");
#endif

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_DASHBOARD)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_DASHBOARD), "Return to XMB");
   gl_render_msg_post(gl);

   cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, comment);
   gl_render_msg_post(gl);
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
   menu_stack_push(0, FILE_BROWSER_MENU);
   filebrowser_set_root(&tmpBrowser, "/");
}

void menu_free (void)
{
   filebrowser_free(&browser);
   filebrowser_free(&tmpBrowser);
}

void menu_loop(void)
{
   gl_t * gl = driver.video_data;

   g_console.menu_enable = true;
   gl->block_swap = true;

   if(g_console.ingame_menu_enable)
   {
      menu_stack_increment();
      menu_stack_push(menuStackindex, INGAME_MENU);
   }

   do
   {
      static bool first_held = false;
      unsigned menu_id = menuStack[menuStackindex].enum_id;
      unsigned menu_category_id = menuStack[menuStackindex].category_id;

      state = cell_pad_input_poll_device(0);
      trigger_state = state & ~old_state;
      held_state = state & older_state;
      held_state = held_state & old_state;

      if(held_state)
      {
         bool analog_sticks_pressed = check_analog(held_state);
         bool shoulder_buttons_pressed = check_shoulder_buttons(held_state) && menu_category_id != CATEGORY_SETTINGS;
         bool do_held = analog_sticks_pressed || shoulder_buttons_pressed;

         if(do_held)
         {
            if(!first_held)
            {
               first_held = true;
	       SET_TIMER_EXPIRATION(gl, 7);
            }
            if(IS_TIMER_EXPIRED(gl))
            {
               first_held = false;
               trigger_state = held_state;
            }
         }
      }

      gfx_ctx_clear();

      if(menu_id == INGAME_MENU_RESIZE && CTRL_SQUARE(state) || menu_id == INGAME_MENU_SCREENSHOT)
	 gl->menu_render = false;
      else
      {
         gfx_ctx_set_blend(true);
	 gl->menu_render = true;
      }

      rarch_render_cached_frame();

      filebrowser_t * fb = &browser;

      switch(menu_id)
      {
         case FILE_BROWSER_MENU:
            select_rom();
            fb = &browser;
	    break;
	 case GENERAL_VIDEO_MENU:
	 case GENERAL_AUDIO_MENU:
	 case EMU_GENERAL_MENU:
	 case EMU_VIDEO_MENU:
	 case EMU_AUDIO_MENU:
	 case PATH_MENU:
	 case CONTROLS_MENU:
	    select_setting();
	    break;
	 case SHADER_CHOICE:
	 case PRESET_CHOICE:
	 case BORDER_CHOICE:
	 case LIBRETRO_CHOICE:
	 case INPUT_PRESET_CHOICE:
	    select_file();
            fb = &tmpBrowser;
	    break;
	 case PATH_SAVESTATES_DIR_CHOICE:
	 case PATH_DEFAULT_ROM_DIR_CHOICE:
	 case PATH_CHEATS_DIR_CHOICE:
	 case PATH_SRAM_DIR_CHOICE:
	    select_directory();
            fb = &tmpBrowser;
	    break;
	 case INGAME_MENU:
	    if(g_console.ingame_menu_enable)
               ingame_menu();
            break;
         case INGAME_MENU_RESIZE:
            ingame_menu_resize();
	    break;
         case INGAME_MENU_SCREENSHOT:
            ingame_menu_screenshot();
            break;
      }

      switch(menu_category_id)
      {
         case CATEGORY_FILEBROWSER:
            browser_render(fb);
            break;
         case CATEGORY_SETTINGS:
         case CATEGORY_INGAME_MENU:
         default:
            break;
      }

      older_state = old_state;
      old_state = state;

      if(IS_TIMER_EXPIRED(gl))
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
         SET_TIMER_EXPIRATION(gl, 30);
      }

      const char * message = msg_queue_pull(g_extern.msg_queue);

      if (message && g_console.info_msg_enable)
      {
         cellDbgFontPrintf(g_settings.video.msg_pos_x, 0.75f, 1.05f, WHITE, message);
         gl_render_msg_post(gl);
      }

      gfx_ctx_swap_buffers();
#ifdef HAVE_SYSUTILS
      cellSysutilCheckCallback();
#endif
      if(menu_id == INGAME_MENU_RESIZE && CTRL_SQUARE(state) || menu_id == INGAME_MENU_SCREENSHOT)
      { }
      else
         gfx_ctx_set_blend(false);

   }while (g_console.menu_enable);

   gl->menu_render = false;

   if(g_console.ingame_menu_enable)
      menu_stack_decrement();

   gl->block_swap = false;

   g_console.ingame_menu_enable = false;
}
