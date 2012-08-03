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

#include "RetroLaunch/IoSupport.h"
#include "RetroLaunch/Surface.h"

#include "../../ps3/frontend/menu.h"
#include "../../ps3/frontend/menu-entries.h"
#include "../../console/fileio/file_browser.h"

#include "../../console/rarch_console.h"

#ifdef _XBOX1
#include "../../gfx/fonts/xdk1_xfonts.h"

#define NUM_ENTRY_PER_PAGE 17

#define ROM_PANEL_WIDTH 440
#define ROM_PANEL_HEIGHT 20

#define MAIN_TITLE_X 305
#define MAIN_TITLE_Y 30
#define MAIN_TITLE_COLOR 0xFFFFFFFF

#define MENU_MAIN_BG_X 0
#define MENU_MAIN_BG_Y 0

int xpos, ypos;
// Rom selector panel with coords
d3d_surface_t m_menuMainRomSelectPanel;
// Background image with coords
d3d_surface_t m_menuMainBG;

// Rom list coords
int m_menuMainRomListPos_x;
int m_menuMainRomListPos_y;

// Backbuffer width, height
int width; 
int height;
#endif

menu menuStack[10];
int menuStackindex = 0;

filebrowser_t browser;
filebrowser_t tmpBrowser;
static unsigned currently_selected_controller_menu = 0;

char m_title[128];

static uint64_t old_state = 0;

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
#ifdef __CELLOS_LV2__
	   case SETTING_CHANGE_RESOLUTION:
         set_setting_label_color(items,g_console.initial_resolution_id == g_console.supported_resolutions[g_console.current_resolution_index], currentsetting);
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), ps3_get_resolution_label(g_console.supported_resolutions[g_console.current_resolution_index]));
		   break;
#endif
	   case SETTING_SHADER_PRESETS:
         set_setting_label_color(items,true, currentsetting);
		   fill_pathname_base(fname, g_console.cgp_path, sizeof(fname));
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), fname);
		   break;
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
	   case SETTING_SHADER:
		   fill_pathname_base(fname, g_settings.video.cg_shader_path, sizeof(fname));
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "%s", fname);
         set_setting_label_color(items,strcmp(g_settings.video.cg_shader_path, default_paths.shader_file) == 0, currentsetting);
		   break;
	   case SETTING_SHADER_2:
		   fill_pathname_base(fname, g_settings.video.second_pass_shader, sizeof(fname));
		   snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "%s", fname);
         set_setting_label_color(items,strcmp(g_settings.video.second_pass_shader, default_paths.shader_file) == 0,
            currentsetting);
		   break;
#endif
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
#ifdef HAVE_RSOUND
            case SOUND_MODE_RSOUND:
               snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), 
                  "INFO - [Sound Output] is set to 'RSound' - the sound will be streamed over the\n network to the RSound audio server." );
               snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "RSound");
               items[currentsetting].text_color = ORANGE;
               break;
#endif
#ifdef HAVE_HEADSET
            case SOUND_MODE_HEADSET:
               snprintf(items[currentsetting].comment, sizeof(items[currentsetting].comment), 
                  "INFO - [Sound Output] is set to 'USB/Bluetooth Headset' - sound will\n be output through the headset");
               snprintf(items[currentsetting].setting_text, sizeof(items[currentsetting].setting_text), "USB/Bluetooth Headset");
               items[currentsetting].text_color = ORANGE;
               break;
#endif
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
   float increment_step = 0.03f;
   float x_position = POSITION_X;

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

      items[i].text_xpos = x_position;
      items[i].text_ypos = increment; 
      items[i].page = page;
      set_setting_label(current_menu, items, i);
      increment += increment_step;
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

static void display_menubar(menu *current_menu)
{
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;
   filebrowser_t *fb = &browser;
   char current_path[256], rarch_version[128];

   float x_position = POSITION_X;
   float current_y_position = POSITION_Y_START;
   float font_size = m_menuMainRomListPos_y;

   snprintf(rarch_version, sizeof(rarch_version), "v%s", PACKAGE_VERSION);

   switch(current_menu->enum_id)
   {
      case GENERAL_VIDEO_MENU:
         render_msg_place_func(x_position, 0.03f, font_size, WHITE, "NEXT ->");
         break;
      case GENERAL_AUDIO_MENU:
      case EMU_GENERAL_MENU:
      case EMU_VIDEO_MENU:
      case EMU_AUDIO_MENU:
      case PATH_MENU:
         render_msg_place_func(x_position, 0.03f, font_size, WHITE, "<- PREV | NEXT ->");
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
         render_msg_place_func(x_position, 0.03f, font_size, WHITE, "<- PREV");
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
         snprintf(current_path, sizeof(current_path), "PATH: %s", filebrowser_get_current_dir(fb));
         render_msg_place_func(x_position, current_y_position, 0, 0, current_path);
         break;
      default:
         break;
   }

   //Render background image
   d3d_surface_render(&m_menuMainBG, MENU_MAIN_BG_X, MENU_MAIN_BG_Y,
   m_menuMainBG.m_imageInfo.Width, m_menuMainBG.m_imageInfo.Height);
}

static void browser_update(filebrowser_t * b, uint16_t input, const char *extensions)
{
   filebrowser_action_t action = FILEBROWSER_ACTION_NOOP;

   if (input & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
      action = FILEBROWSER_ACTION_DOWN;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
      action = FILEBROWSER_ACTION_UP;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      action = FILEBROWSER_ACTION_RIGHT;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
      action = FILEBROWSER_ACTION_LEFT;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_R))
      action = FILEBROWSER_ACTION_SCROLL_DOWN;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_L))
      action = FILEBROWSER_ACTION_SCROLL_UP;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_A))
      action = FILEBROWSER_ACTION_CANCEL;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
   {
      action = FILEBROWSER_ACTION_RESET;
      filebrowser_set_root(b, "/");
      strlcpy(b->extensions, extensions, sizeof(b->extensions));
   }

   if(action != FILEBROWSER_ACTION_NOOP)
      filebrowser_iterate(b, action);
}

static void browser_render(filebrowser_t *b, float current_x, float current_y, float y_spacing)
{
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   unsigned file_count = b->current_dir.list->size;
   unsigned current_index, page_number, page_base, i;
   float currentX, currentY, ySpacing;

   current_index = b->current_dir.ptr;
   page_number = current_index / NUM_ENTRY_PER_PAGE;
   page_base = page_number * NUM_ENTRY_PER_PAGE;

   currentX = current_x;
   currentY = current_y;
   ySpacing = y_spacing;

   for (i = page_base; i < file_count && i < page_base + NUM_ENTRY_PER_PAGE; ++i)
   {
      char fname_tmp[256];
      fill_pathname_base(fname_tmp, b->current_dir.list->elems[i].data, sizeof(fname_tmp));
      currentY = currentY + ySpacing;

      const char *rom_basename = fname_tmp;

      //check if this is the currently selected file
      const char *current_pathname = filebrowser_get_current_path(b);
      if(strcmp(current_pathname, b->current_dir.list->elems[i].data) == 0)
         d3d_surface_render(&m_menuMainRomSelectPanel, currentX, currentY, ROM_PANEL_WIDTH, ROM_PANEL_HEIGHT);

      render_msg_place_func(currentX, currentY, 0, 0, rom_basename);
   }
}

static void menu_romselect_iterate(filebrowser_t *filebrowser, menu_romselect_action_t action)
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
         break;
      default:
         break;
   }
}

static void select_rom(item *items, menu *current_menu, uint64_t input)
{
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   browser_update(&browser, input, rarch_console_get_rom_ext());
   
   menu_romselect_action_t action = MENU_ROMSELECT_ACTION_NOOP;
   
   if (input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
      action = MENU_ROMSELECT_ACTION_OK;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_R3))
   {
      LD_LAUNCH_DASHBOARD LaunchData = { XLD_LAUNCH_DASHBOARD_MAIN_MENU };
      XLaunchNewImage( NULL, (LAUNCH_DATA*)&LaunchData );
   }

   if (action != MENU_ROMSELECT_ACTION_NOOP)
      menu_romselect_iterate(&browser, action);

   display_menubar(current_menu);
   
   render_msg_place_func(xpos, ypos, 0, 0, m_title);
}

int menu_init(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   // Set libretro filename and version to variable
   struct retro_system_info info;
   retro_get_system_info(&info);
   const char *id = info.library_name ? info.library_name : "Unknown";
   snprintf(m_title, sizeof(m_title), "Libretro core: %s %s", id, info.library_version);


   // Set file cache size
   XSetFileCacheSize(8 * 1024 * 1024);

   // Mount drives
   xbox_io_mount("A:", "cdrom0");
   xbox_io_mount("E:", "Harddisk0\\Partition1");
   xbox_io_mount("Z:", "Harddisk0\\Partition2");
   xbox_io_mount("F:", "Harddisk0\\Partition6");
   xbox_io_mount("G:", "Harddisk0\\Partition7");

	strlcpy(browser.extensions, rarch_console_get_rom_ext(), sizeof(browser.extensions));
   menu_stack_push(menu_items, FILE_BROWSER_MENU);
   filebrowser_set_root_and_ext(&browser, rarch_console_get_rom_ext(), g_console.default_rom_startup_dir);
   filebrowser_set_root(&tmpBrowser, "/");
   
   width  = d3d->d3dpp.BackBufferWidth;

   // Quick hack to properly center the romlist in 720p, 
   // it might need more work though (font size and rom selector size -> needs more memory)
   // Init rom list coords
   // Load background image
   if(width == 640)
   {
      d3d_surface_new(&m_menuMainBG, "D:\\Media\\menuMainBG.png");
      m_menuMainRomListPos_x = 100;
      m_menuMainRomListPos_y = 100;
   }
   else if(width == 1280)
   {
      d3d_surface_new(&m_menuMainBG, "D:\\Media\\menuMainBG_720p.png");
      m_menuMainRomListPos_x = 400;
      m_menuMainRomListPos_y = 150;
   }

   // Load rom selector panel
   d3d_surface_new(&m_menuMainRomSelectPanel, "D:\\Media\\menuMainRomSelectPanel.png");
   
   //Display some text
   //Center the text (hardcoded)
   xpos = width == 640 ? 65 : 400;
   ypos = width == 640 ? 430 : 670;

   g_console.mode_switch = MODE_MENU;

   return 0;
}

void menu_free(void)
{
   filebrowser_free(&browser);
   filebrowser_free(&tmpBrowser);

   d3d_surface_free(&m_menuMainBG);
   d3d_surface_free(&m_menuMainRomSelectPanel);
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
      //first button input frame
      uint64_t input_state_first_frame = 0;
      uint64_t input_state = 0;
      static bool first_held = false;
      menu *current_menu = menu_stack_get_current_ptr();

       input_ptr.poll(NULL);

      static const struct retro_keybind *binds[MAX_PLAYERS] = {
	      g_settings.input.binds[0],
	      g_settings.input.binds[1],
	      g_settings.input.binds[2],
	      g_settings.input.binds[3],
	      g_settings.input.binds[4],
	      g_settings.input.binds[5],
	      g_settings.input.binds[6],
	      g_settings.input.binds[7],
      };

      static const struct retro_keybind _analog_binds[] = {
	      { 0, 0, (enum retro_key)0, (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_LEFT), 0 },
	      { 0, 0, (enum retro_key)0, (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_RIGHT), 0 },
	      { 0, 0, (enum retro_key)0, (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_UP), 0 },
	      { 0, 0, (enum retro_key)0, (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_DOWN), 0 },
	      { 0, 0, (enum retro_key)0, (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_LEFT), 0 },
	      { 0, 0, (enum retro_key)0, (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_RIGHT), 0 },
	      { 0, 0, (enum retro_key)0, (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_UP), 0 },
	      { 0, 0, (enum retro_key)0, (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_DOWN), 0 },
      };

      const struct retro_keybind *analog_binds[] = {
	      _analog_binds
      };

      for (unsigned i = 0; i < RARCH_FIRST_META_KEY; i++)
      {
         input_state |= input_ptr.input_state(NULL, binds, false,
            RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
      }

      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 0) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_LEFT) : 0;
      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 1) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_RIGHT) : 0;
      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 2) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_UP) : 0;
      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 3) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_DOWN) : 0;

      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 4) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_LEFT) : 0;
      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 5) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_RIGHT) : 0;
      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 6) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_UP) : 0;
      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 7) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_DOWN) : 0;

      uint64_t trig_state = input_state & ~old_state; //set first button input frame as trigger
      input_state_first_frame = input_state;          //hold onto first button input frame

      //second button input frame
      input_state = 0;
      input_ptr.poll(NULL);


      for (unsigned i = 0; i < RARCH_FIRST_META_KEY; i++)
      {
         input_state |= input_ptr.input_state(NULL, binds, false,
            RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
      }

      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 0) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_LEFT) : 0;
      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 1) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_RIGHT) : 0;
      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 2) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_UP) : 0;
      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 3) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_DOWN) : 0;

      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 4) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_LEFT) : 0;
      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 5) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_RIGHT) : 0;
      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 6) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_UP) : 0;
      input_state |= input_ptr.input_state(NULL, analog_binds, false,
         RETRO_DEVICE_JOYPAD, 0, 7) ? (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_DOWN) : 0;

      bool analog_sticks_pressed = (input_state & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_LEFT)) || (input_state & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_RIGHT)) || (input_state & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_UP)) || (input_state & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_DOWN)) || (input_state & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_LEFT)) || (input_state & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_RIGHT)) || (input_state & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_UP)) || (input_state & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_DOWN));
      bool shoulder_buttons_pressed = ((input_state & (1 << RETRO_DEVICE_ID_JOYPAD_L2)) || (input_state & (1 << RETRO_DEVICE_ID_JOYPAD_R2))) /*&& current_menu->category_id != CATEGORY_SETTINGS*/;
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
            trig_state = input_state; //second input frame set as current frame
         }
      }
      
      device_ptr->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
         D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
      device_ptr->frame_count++;

      device_ptr->d3d_render_device->BeginScene();
      device_ptr->d3d_render_device->SetFlickerFilter(1);
      device_ptr->d3d_render_device->SetSoftDisplayFilter(1);
      
      filebrowser_t * fb = &browser;

      switch(current_menu->enum_id)
      {
         case FILE_BROWSER_MENU:
            select_rom(menu_items, current_menu, trig_state);
            fb = &browser;
            break;
         case GENERAL_VIDEO_MENU:
         case GENERAL_AUDIO_MENU:
         case EMU_GENERAL_MENU:
         case EMU_VIDEO_MENU:
         case EMU_AUDIO_MENU:
         case PATH_MENU:
         case CONTROLS_MENU:
            //select_setting(menu_items, current_menu, trig_state);
            break;
         case SHADER_CHOICE:
         case PRESET_CHOICE:
         case BORDER_CHOICE:
         case LIBRETRO_CHOICE:
         case INPUT_PRESET_CHOICE:
            //select_file(menu_items, current_menu, trig_state);
            fb = &tmpBrowser;
            break;
         case PATH_SAVESTATES_DIR_CHOICE:
         case PATH_DEFAULT_ROM_DIR_CHOICE:
         case PATH_CHEATS_DIR_CHOICE:
         case PATH_SRAM_DIR_CHOICE:
            //select_directory(menu_items, current_menu, trig_state);
            fb = &tmpBrowser;
            break;
         case INGAME_MENU:
            //if(g_console.ingame_menu_enable)
               //ingame_menu(menu_items, current_menu, trig_state);
            break;
         case INGAME_MENU_RESIZE:
            //ingame_menu_resize(menu_items, current_menu, trig_state);
            break;
         case INGAME_MENU_SCREENSHOT:
            //ingame_menu_screenshot(menu_items, current_menu, trig_state);
            break;
      }

      float x_position = POSITION_X;
      float starting_y_position = POSITION_Y_START;
      float y_position_increment = 20;

      switch(current_menu->category_id)
      {
         case CATEGORY_FILEBROWSER:
            browser_render(fb, x_position, starting_y_position, y_position_increment);
            break;
         case CATEGORY_SETTINGS:
         case CATEGORY_INGAME_MENU:
         default:
            break;
      }
      
      old_state = input_state_first_frame;

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
                  g_console.mode_switch = (((old_state & (1 << RETRO_DEVICE_ID_JOYPAD_L3)) && (old_state & (1 << RETRO_DEVICE_ID_JOYPAD_R3)) && g_console.emulator_initialized)) ? MODE_EMULATION : MODE_MENU;
            }
            else
            {
               g_console.menu_enable = !(((old_state & (1 << RETRO_DEVICE_ID_JOYPAD_L3)) && (old_state & (1 << RETRO_DEVICE_ID_JOYPAD_R3)) && g_console.emulator_initialized));
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

      gfx_ctx_swap_buffers();
   }while(g_console.menu_enable);

   if(g_console.ingame_menu_enable)
      menu_stack_decrement();

   device_ptr->block_swap = false;

   g_console.ingame_menu_enable = false;
}
