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

#if(CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif

#endif

#include "../../console/fileio/file_browser.h"

#include "../../console/rarch_console.h"
#include "../../console/rarch_console_rom_ext.h"
#include "../../console/rarch_console_input.h"
#include "../../console/rarch_console_config.h"
#include "../../console/rarch_console_settings.h"

#include "../../gfx/image.h"

#ifdef HAVE_RSOUND
#include "../../console/rarch_console_rsound.h"
#endif

#include "../../console/rarch_console_video.h"

#ifdef HAVE_ZLIB
#include "../../console/rarch_console_rzlib.h"
#endif

#if defined(HAVE_OPENGL)
#include "../../gfx/gl_common.h"
#include "../../gfx/gl_font.h"
#endif
#include "../../gfx/gfx_context.h"

#if defined(HAVE_CG)
#include "../../gfx/shader_cg.h"
#endif

#include "../../file.h"
#include "../../general.h"

#include "rmenu.h"
#include "rmenu_entries.h"

#define INPUT_SCALE 2
#define MENU_ITEM_SELECTED(index) (menuitem_colors[index])

menu menuStack[10];
int menuStackindex = 0;
static bool set_libretro_core_as_launch;

filebrowser_t browser;
filebrowser_t tmpBrowser;
unsigned set_shader = 0;
unsigned currently_selected_controller_menu = 0;
char m_title[256];

static const rmenu_context_t *context;

static uint64_t old_state = 0;

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

static void set_setting_label_write_on_or_off(char *setting_text, bool cond, unsigned currentsetting)
{
   if(cond)
      snprintf(setting_text, sizeof(setting_text), "ON");
   else
      snprintf(setting_text, sizeof(setting_text), "OFF");
}

static void set_setting_label(unsigned i, item *current_item)
{
   char fname[PATH_MAX];
   (void)fname;

   unsigned currentsetting = i;

   switch(currentsetting)
   {
#ifdef __CELLOS_LV2__
      case SETTING_CHANGE_RESOLUTION:
      {
         unsigned width = gfx_ctx_get_resolution_width(g_console.supported_resolutions[g_console.current_resolution_index]);
	 unsigned height = gfx_ctx_get_resolution_height(g_console.supported_resolutions[g_console.current_resolution_index]);
         snprintf(current_item->text, sizeof(current_item->text), "Resolution");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%dx%d", width, height);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Change the display resolution.");
      }
      break;
#endif
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
      case SETTING_SHADER_PRESETS:
         snprintf(current_item->text, sizeof(current_item->text), "Shader Presets (CGP)");
	 fill_pathname_base(fname, g_console.cgp_path, sizeof(fname));
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), fname);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Select a [CG Preset] script.");
	 break;
      case SETTING_SHADER:
	 fill_pathname_base(fname, g_settings.video.cg_shader_path, sizeof(fname));
         snprintf(current_item->text, sizeof(current_item->text), "Shader #1");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%s", fname);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Select a shader as [Shader #1]. NOTE: Some shaders might be\ntoo slow at 1080p. If you experience any slowdown, try another shader.");
	 break;
      case SETTING_SHADER_2:
	 fill_pathname_base(fname, g_settings.video.second_pass_shader, sizeof(fname));
         snprintf(current_item->text, sizeof(current_item->text), "Shader #2");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%s", fname);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Select a shader as [Shader #2]. NOTE: Some shaders might be\ntoo slow at 1080p. If you experience any slowdown, try another shader.");
	 break;
#endif
      case SETTING_FONT_SIZE:
         snprintf(current_item->text, sizeof(current_item->text), "Font Size");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%f", g_console.menu_font_size);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Increase or decrease the [Font Size].");
	 break;
      case SETTING_KEEP_ASPECT_RATIO:
         snprintf(current_item->text, sizeof(current_item->text), "Aspect Ratio");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), aspectratio_lut[g_console.aspect_ratio_index].name);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Select an [Aspect Ratio].");
	 break;
      case SETTING_HW_TEXTURE_FILTER:
         snprintf(current_item->text, sizeof(current_item->text), "Hardware filtering #1");
	 if(g_settings.video.smooth)
            snprintf(current_item->setting_text, sizeof(current_item->setting_text), "Bilinear");
	 else
            snprintf(current_item->setting_text, sizeof(current_item->setting_text), "Point");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Hardware filtering #1 is set to [%s].", current_item->setting_text);
	 break;
#ifdef HAVE_FBO
      case SETTING_HW_TEXTURE_FILTER_2:
         snprintf(current_item->text, sizeof(current_item->text), "Hardware filtering #2");
	 if(g_settings.video.second_pass_smooth)
            snprintf(current_item->setting_text, sizeof(current_item->setting_text), "Bilinear");
	 else
            snprintf(current_item->setting_text, sizeof(current_item->setting_text), "Point");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Hardware filtering #2 is set to [%s].", current_item->setting_text);
	 break;
      case SETTING_SCALE_ENABLED:
         snprintf(current_item->text, sizeof(current_item->text), "Custom Scaling/Dual Shaders");
	 set_setting_label_write_on_or_off(current_item->setting_text, g_console.fbo_enabled, currentsetting);
	 snprintf(current_item->comment, sizeof(current_item->comment), g_console.fbo_enabled ? "INFO - [Custom Scaling] is set to 'ON' - 2x shaders will look much\nbetter, and you can select a shader for [Shader #2]." : "INFO - [Custom Scaling] is set to 'OFF'.");
	 break;
      case SETTING_SCALE_FACTOR:
         snprintf(current_item->text, sizeof(current_item->text), "Custom Scaling Factor");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%fx (X) / %fx (Y)", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);
	 snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Custom Scaling Factor] is set to: '%fx (X) / %fx (Y)'.", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);
	 break;
#endif
#ifdef _XBOX1
      case SETTING_FLICKER_FILTER:
         snprintf(current_item->text, sizeof(current_item->text), "Flicker Filter");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%d", g_console.flicker_filter);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Toggle the [Flicker Filter].");
	 break;
      case SETTING_SOFT_DISPLAY_FILTER:
         snprintf(current_item->text, sizeof(current_item->text), "Soft Display Filter");
	 set_setting_label_write_on_or_off(current_item->setting_text, g_console.soft_display_filter_enable, currentsetting);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Toggle the [Soft Display Filter].");
	 break;
#endif
      case SETTING_HW_OVERSCAN_AMOUNT:
         snprintf(current_item->text, sizeof(current_item->text), "Overscan");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%f", g_console.overscan_amount);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Adjust or decrease [Overscan]. Set this to higher than 0.000\nif the screen doesn't fit on your TV/monitor.");
	 break;
      case SETTING_THROTTLE_MODE:
         snprintf(current_item->text, sizeof(current_item->text), "Throttle Mode");
	 set_setting_label_write_on_or_off(current_item->setting_text, g_console.throttle_enable, currentsetting);
         snprintf(current_item->comment, sizeof(current_item->comment), g_console.throttle_enable ? "INFO - [Throttle Mode] is 'ON' - Vsync is enabled." : "INFO - [Throttle Mode] is 'OFF' - Vsync is disabled.");
	 break;
      case SETTING_TRIPLE_BUFFERING:
         snprintf(current_item->text, sizeof(current_item->text), "Triple Buffering");
	 set_setting_label_write_on_or_off(current_item->setting_text, g_console.triple_buffering_enable, currentsetting);
         snprintf(current_item->comment, sizeof(current_item->comment), g_console.triple_buffering_enable ? "INFO - [Triple Buffering] is set to 'ON'." : "INFO - [Triple Buffering] is set to 'OFF'.");
	 break;
      case SETTING_ENABLE_SCREENSHOTS:
         snprintf(current_item->text, sizeof(current_item->text), "Screenshot Option");
	 set_setting_label_write_on_or_off(current_item->setting_text, g_console.screenshots_enable, currentsetting);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Screenshots feature is set to '%s'.", g_console.screenshots_enable ? "ON" : "OFF");
	 break;
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
      case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
         snprintf(current_item->text, sizeof(current_item->text), "APPLY SHADER PRESET ON STARTUP");
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Automatically load the currently selected [CG Preset] file on startup.");
         break;
#endif
      case SETTING_DEFAULT_VIDEO_ALL:
         snprintf(current_item->text, sizeof(current_item->text), "DEFAULTS");
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Set all [General Video Settings] back to their 'DEFAULT' values.");
	 break;
      case SETTING_SOUND_MODE:
         snprintf(current_item->text, sizeof(current_item->text), "Sound Output");
	 switch(g_console.sound_mode)
	 {
		 case SOUND_MODE_NORMAL:
			 snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Sound Output] is set to 'Normal'.");
			 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "Normal");
			 break;
#ifdef HAVE_RSOUND
		 case SOUND_MODE_RSOUND:
			 snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Sound Output] is set to 'RSound'." );
			 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "RSound");
			 break;
#endif
#ifdef HAVE_HEADSET
		 case SOUND_MODE_HEADSET:
			 snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Sound Output] is set to 'USB/Bluetooth Headset'.");
			 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "USB/Bluetooth Headset");
			 break;
#endif
		 default:
			 break;
	 }
	 break;
#ifdef HAVE_RSOUND
      case SETTING_RSOUND_SERVER_IP_ADDRESS:
         snprintf(current_item->text, sizeof(current_item->text), "RSound Server IP Address");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), g_settings.audio.device);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Enter the IP Address of the [RSound Audio Server]. IP address\nmust be an IPv4 32-bits address, eg: '192.168.1.7'.");
	 break;
#endif
      case SETTING_DEFAULT_AUDIO_ALL:
         snprintf(current_item->text, sizeof(current_item->text), "DEFAULTS");
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Set all [General Audio Settings] back to their 'DEFAULT' values.");
	 break;
      case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
         snprintf(current_item->text, sizeof(current_item->text), "Current save state slot");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%d", g_extern.state_slot);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Set the currently selected savestate slot.");
	 break;
	 /* emu-specific */
      case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
         snprintf(current_item->text, sizeof(current_item->text), "Debug info messages");
	 set_setting_label_write_on_or_off(current_item->setting_text, g_console.fps_info_msg_enable, currentsetting);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Show onscreen debug messages.");
	 break;
      case SETTING_EMU_SHOW_INFO_MSG:
         snprintf(current_item->text, sizeof(current_item->text), "Info messages");
	 set_setting_label_write_on_or_off(current_item->setting_text, g_console.info_msg_enable, currentsetting);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Show onscreen info messages in the menu.");
	 break;
      case SETTING_EMU_REWIND_ENABLED:
         snprintf(current_item->text, sizeof(current_item->text), "Rewind option");
	 set_setting_label_write_on_or_off(current_item->setting_text, g_settings.rewind_enable, currentsetting);
	 if(g_settings.rewind_enable)
	    snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Rewind] feature is set to 'ON'.");
	 else
	    snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Rewind] feature is set to 'OFF'.");
	 break;
      case SETTING_ZIP_EXTRACT:
         snprintf(current_item->text, sizeof(current_item->text), "ZIP Extract Option");
	 switch(g_console.zip_extract_mode)
	 {
		 case ZIP_EXTRACT_TO_CURRENT_DIR:
			 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "Current dir");
			 snprintf(current_item->comment, sizeof(current_item->comment), "INFO - ZIP files are extracted to the current dir.");
			 break;
		 case ZIP_EXTRACT_TO_CURRENT_DIR_AND_LOAD_FIRST_FILE:
			 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "Current dir and load first file");
			 snprintf(current_item->comment, sizeof(current_item->comment), "NFO - ZIP files are extracted to the current dir, and the first game is automatically loaded.");
			 break;
		 case ZIP_EXTRACT_TO_CACHE_DIR:
			 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "Cache dir");
			 snprintf(current_item->comment, sizeof(current_item->comment), "INFO - ZIP files are extracted to the cache dir.");
			 break;
	 }
	 break;
      case SETTING_RARCH_DEFAULT_EMU:
         snprintf(current_item->text, sizeof(current_item->text), "Default libretro core");
	 fill_pathname_base(fname, g_settings.libretro, sizeof(fname));
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%s", fname);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Select a default libretro core to launch at start-up.");
	 break;
      case SETTING_EMU_AUDIO_MUTE:
         snprintf(current_item->text, sizeof(current_item->text), "Mute Audio");
	 set_setting_label_write_on_or_off(current_item->setting_text, g_extern.audio_data.mute, currentsetting);
	 if(g_extern.audio_data.mute)
		 snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Audio Mute] feature is set to 'ON'. The game audio will be muted.");
	 else
		 snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Audio Mute] feature is set to 'OFF'.");
	 break;
      case SETTING_ENABLE_CUSTOM_BGM:
         snprintf(current_item->text, sizeof(current_item->text), "Custom BGM Option");
	 set_setting_label_write_on_or_off(current_item->setting_text, g_console.custom_bgm_enable, currentsetting);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Custom BGM] is set to '%s'.", g_console.custom_bgm_enable ? "ON" : "OFF");
	 break;
      case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
         snprintf(current_item->text, sizeof(current_item->text), "Startup ROM Directory");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), g_console.default_rom_startup_dir);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Set the default [Startup ROM directory]. NOTE: You will have to\nrestart the emulator for this change to have any effect.");
	 break;
      case SETTING_PATH_SAVESTATES_DIRECTORY:
         snprintf(current_item->text, sizeof(current_item->text), "Savestate Directory");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), g_console.default_savestate_dir);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Set the default path where all the savestate files will be saved to.");
	 break;
      case SETTING_PATH_SRAM_DIRECTORY:
         snprintf(current_item->text, sizeof(current_item->text), "SRAM Directory");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), g_console.default_sram_dir);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Set the default SRAM (SaveRAM) directory path. All the\nbattery backup saves will be stored in this directory.");
	 break;
#ifdef HAVE_XML
      case SETTING_PATH_CHEATS:
         snprintf(current_item->text, sizeof(current_item->text), "Cheatfile Directory");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), g_settings.cheat_database);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Set the default [Cheatfile directory] path. All CHT (cheat) files\nwill be stored here.");
	 break;
#endif
      case SETTING_PATH_SYSTEM:
         snprintf(current_item->text, sizeof(current_item->text), "System Directory");
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), g_settings.system_directory);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Set the default [System directory] path. System files like\nBIOS files, etc. will be stored here.");
	 break;
      case SETTING_ENABLE_SRAM_PATH:
         snprintf(current_item->text, sizeof(current_item->text), "Custom SRAM Dir Enable");
	 set_setting_label_write_on_or_off(current_item->setting_text, g_console.default_sram_dir_enable, currentsetting);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Custom SRAM Dir Path] is set to '%s'.", g_console.default_sram_dir_enable ? "ON" : "OFF");
	 break;
      case SETTING_ENABLE_STATE_PATH:
         snprintf(current_item->text, sizeof(current_item->text), "Custom Savestate Dir Enable");
	 set_setting_label_write_on_or_off(current_item->setting_text, g_console.default_savestate_dir_enable, currentsetting);
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [Custom Savestate Dir Path] is set to '%s'.", g_console.default_savestate_dir_enable ? "ON" : "OFF");
	 break;
      case SETTING_CONTROLS_SCHEME:
         snprintf(current_item->text, sizeof(current_item->text), "Control Scheme Preset");
	 snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Input scheme preset [%s] is selected.", g_console.input_cfg_path);
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), g_console.input_cfg_path);
	 break;
      case SETTING_CONTROLS_NUMBER:
         snprintf(current_item->text, sizeof(current_item->text), "Controller No");
	 snprintf(current_item->comment, sizeof(current_item->comment), "Controller %d is currently selected.", currently_selected_controller_menu+1);
	 snprintf(current_item->setting_text, sizeof(current_item->setting_text), "%d", currently_selected_controller_menu+1);
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
	    const char * value = rarch_input_find_platform_key_label(g_settings.input.binds[currently_selected_controller_menu][currentsetting-(FIRST_CONTROL_BIND)].joykey);
	    unsigned id = currentsetting - FIRST_CONTROL_BIND;
	    snprintf(current_item->text, sizeof(current_item->text), rarch_input_get_default_keybind_name(id));
	    snprintf(current_item->comment, sizeof(current_item->comment), "INFO - [%s] is mapped to action:\n[%s].", current_item->text, value);
	    snprintf(current_item->setting_text, sizeof(current_item->setting_text), value);
	 }
	 break;
      case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
         snprintf(current_item->text, sizeof(current_item->text), "SAVE CUSTOM CONTROLS");
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Save the [Custom Controls] settings to file.");
         break;
      case SETTING_CONTROLS_DEFAULT_ALL:
         snprintf(current_item->text, sizeof(current_item->text), "DEFAULTS");
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "");
         snprintf(current_item->comment, sizeof(current_item->comment), "IFNO - Set all [Controls] back to their 'DEFAULT' values.");
         break;
      case SETTING_EMU_VIDEO_DEFAULT_ALL:
         snprintf(current_item->text, sizeof(current_item->text), "DEFAULTS");
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Set [all RetroArch Video settings] back to their 'DEFAULT' values.");
         break;
      case SETTING_EMU_AUDIO_DEFAULT_ALL:
         snprintf(current_item->text, sizeof(current_item->text), "DEFAULTS");
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Set all [RetroArch Audio settings] back to their 'DEFAULT' values.");
         break;
      case SETTING_PATH_DEFAULT_ALL:
         snprintf(current_item->text, sizeof(current_item->text), "DEFAULTS");
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Set all [Path settings] back to their 'DEFAULT' values.");
         break;
      case SETTING_EMU_DEFAULT_ALL:
         snprintf(current_item->text, sizeof(current_item->text), "DEFAULTS");
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Set [all RetroArch settings] back to their 'DEFAULT' values.");
         break;
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
      case SETTING_SAVE_SHADER_PRESET:
         snprintf(current_item->text, sizeof(current_item->text), "SAVE SETTINGS AS CGP PRESET");
         snprintf(current_item->setting_text, sizeof(current_item->setting_text), "");
         snprintf(current_item->comment, sizeof(current_item->comment), "INFO - Save the current video settings to a [CG Preset] (CGP) file.");
	 break;
#endif
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
   rmenu_default_positions_t default_pos = {0};
   int page = 0;
   unsigned j = 0;
   int i;

   context->set_default_pos(&default_pos);

   for(i = current_menu->first_setting; i < current_menu->max_settings; i++)
   {
      if(!(j < default_pos.entries_per_page))
      {
         j = 0;
         default_pos.y_position = POSITION_Y_BEGIN;
         page++;
      }

      items[i].text_xpos = default_pos.x_position;
      items[i].text_ypos = default_pos.y_position; 
      items[i].page = page;
      default_pos.y_position += default_pos.y_position_increment;
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
         strlcpy(current_menu->title, "Filebrowser", sizeof(current_menu->title));
         current_menu->enum_id = menu_id;
         current_menu->selected = 0;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         break;
      case LIBRETRO_CHOICE:
         strlcpy(current_menu->title, "Libretro", sizeof(current_menu->title));
         current_menu->enum_id = menu_id;
         current_menu->selected = 0;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         break;
      case PRESET_CHOICE:
         strlcpy(current_menu->title, "Shader", sizeof(current_menu->title));
         current_menu->enum_id = menu_id;
         current_menu->selected = 0;
         current_menu->page = 0;
         current_menu->category_id = CATEGORY_FILEBROWSER;
         break;
      case INPUT_PRESET_CHOICE:
         strlcpy(current_menu->title, "Input", sizeof(current_menu->title));
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
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SYSTEM_DIR_CHOICE:
         strlcpy(current_menu->title, "Path", sizeof(current_menu->title));
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
   filebrowser_t *fb = &browser;
   char current_path[256], rarch_version[128];

   rmenu_default_positions_t default_pos;
   context->set_default_pos(&default_pos);

   snprintf(rarch_version, sizeof(rarch_version), "v%s", PACKAGE_VERSION);

   switch(current_menu->enum_id)
   {
      case GENERAL_VIDEO_MENU:
	 context->render_msg(default_pos.x_position, default_pos.msg_prev_next_y_position, default_pos.font_size, WHITE, "NEXT ->");
         break;
      case GENERAL_AUDIO_MENU:
      case EMU_GENERAL_MENU:
      case EMU_VIDEO_MENU:
      case EMU_AUDIO_MENU:
      case PATH_MENU:
	 context->render_msg(default_pos.x_position, default_pos.msg_prev_next_y_position, default_pos.font_size, WHITE, "<- PREV | NEXT ->");
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
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SRAM_DIR_CHOICE:
      case PATH_SYSTEM_DIR_CHOICE:
	 context->render_msg(default_pos.x_position, default_pos.msg_prev_next_y_position, default_pos.font_size, WHITE, "<- PREV");
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
#ifdef HAVE_XML
      case PATH_CHEATS_DIR_CHOICE:
#endif
      case PATH_SRAM_DIR_CHOICE:
      case PATH_SYSTEM_DIR_CHOICE:
         fb = &tmpBrowser;
      case FILE_BROWSER_MENU:
	 snprintf(current_path, sizeof(current_path), "PATH: %s", filebrowser_get_current_dir(fb));
         context->render_msg(default_pos.x_position, default_pos.current_path_y_position, default_pos.current_path_font_size, WHITE, current_path);
         break;
      default:
         break;
   }

   rmenu_position_t position = {0};
   context->render_bg(&position);

#ifdef __CELLOS_LV2__
   context->render_msg(default_pos.x_position, 0.05f, 1.4f, WHITE, current_menu->title);
   context->render_msg(0.3f, 0.06f, 0.82f, WHITE, m_title);
   context->render_msg(0.80f, 0.015f, 0.82f, WHITE, rarch_version);
#endif
}

static void browser_update(filebrowser_t * b, uint64_t input, const char *extensions)
{
   bool ret = true;
   filebrowser_action_t action = FILEBROWSER_ACTION_NOOP;

   if (input & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
      action = FILEBROWSER_ACTION_DOWN;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
      action = FILEBROWSER_ACTION_UP;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      action = FILEBROWSER_ACTION_RIGHT;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
      action = FILEBROWSER_ACTION_LEFT;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_R2))
      action = FILEBROWSER_ACTION_SCROLL_DOWN;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_L2))
      action = FILEBROWSER_ACTION_SCROLL_UP;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_A))
      action = FILEBROWSER_ACTION_CANCEL;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
   {
      action = FILEBROWSER_ACTION_RESET;
      filebrowser_set_root(b, default_paths.filesystem_root_dir);
      strlcpy(b->extensions, extensions, sizeof(b->extensions));
   }

   if(action != FILEBROWSER_ACTION_NOOP)
      ret = filebrowser_iterate(b, action);

   if(!ret)
      rarch_settings_msg(S_MSG_DIR_LOADING_ERROR, S_DELAY_180);
}

static void browser_render(filebrowser_t * b)
{
   unsigned file_count = b->current_dir.list->size;
   unsigned int current_index, page_number, page_base, i;

   rmenu_default_positions_t default_pos;
   context->set_default_pos(&default_pos);

   current_index = b->current_dir.ptr;
   page_number = current_index / default_pos.entries_per_page;
   page_base = page_number * default_pos.entries_per_page;

   for ( i = page_base; i < file_count && i < page_base + default_pos.entries_per_page; ++i)
   {
      char fname_tmp[256];
      fill_pathname_base(fname_tmp, b->current_dir.list->elems[i].data, sizeof(fname_tmp));
      default_pos.starting_y_position += default_pos.y_position_increment;

      //check if this is the currently selected file
      const char *current_pathname = filebrowser_get_current_path(b);
      if(strcmp(current_pathname, b->current_dir.list->elems[i].data) == 0)
      {
         rmenu_position_t position = {0};
         position.x = default_pos.x_position;
         position.y = default_pos.starting_y_position;
         context->render_selection_panel(&position);
      }

      context->render_msg(default_pos.x_position, default_pos.starting_y_position, default_pos.variable_font_size, i == current_index ? RED : b->current_dir.list->elems[i].attr.b ? GREEN : WHITE, fname_tmp);
   }
}

#ifdef __CELLOS_LV2__
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
#endif

static void select_file(item *items, menu *current_menu, uint64_t input)
{
   char extensions[256], object[256], comment[256], comment_two[256], path[PATH_MAX];
   bool ret = true;

   rmenu_default_positions_t default_pos;
   context->set_default_pos(&default_pos);

   switch(current_menu->enum_id)
   {
      case SHADER_CHOICE:
         strlcpy(extensions, EXT_SHADERS, sizeof(extensions));
         strlcpy(object, "Shader", sizeof(object));
         snprintf(comment, sizeof(comment), "INFO - Select a shader by pressing [%s].", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
         break;
      case PRESET_CHOICE:
         strlcpy(extensions, EXT_CGP_PRESETS, sizeof(extensions));
         strlcpy(object, "Shader preset", sizeof(object));
         snprintf(comment, sizeof(comment), "INFO - Select a shader preset by pressing [%s].", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
         break;
      case INPUT_PRESET_CHOICE:
         strlcpy(extensions, EXT_INPUT_PRESETS, sizeof(extensions));
         strlcpy(object, "Input preset", sizeof(object));
         snprintf(comment, sizeof(comment), "INFO - Select an input preset by pressing [%s].", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
         break;
      case BORDER_CHOICE:
         strlcpy(extensions, EXT_IMAGES, sizeof(extensions));
         strlcpy(object, "Border image file", sizeof(object));
         snprintf(comment, sizeof(comment), "INFO - Select a border image file by pressing [%s].", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
         break;
      case LIBRETRO_CHOICE:
         strlcpy(extensions, EXT_EXECUTABLES, sizeof(extensions));
         strlcpy(object, "Libretro core", sizeof(object));
         snprintf(comment, sizeof(comment), "INFO - Select a Libretro core by pressing [%s].", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
         break;
   }

   browser_update(&tmpBrowser, input, extensions);

   if (input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
   {
      bool is_dir = filebrowser_get_current_path_isdir(&tmpBrowser);
      if(is_dir)
         ret = filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_OK);
      else
      {
         snprintf(path, sizeof(path), filebrowser_get_current_path(&tmpBrowser));
         
         switch(current_menu->enum_id)
         {
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
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
#ifdef HAVE_OPENGL
               gl_cg_reinit(path);
#endif
               apply_scaling(FBO_INIT);
               break;
#endif
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

      if(!ret)
         rarch_settings_msg(S_MSG_DIR_LOADING_ERROR, S_DELAY_180);
   }
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_X))
      menu_stack_decrement();

   display_menubar(current_menu);

   snprintf(comment_two, sizeof(comment_two), "[%s] - return to settings [%s] - Reset Startdir", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_X), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_START));
   context->render_msg(default_pos.x_position, default_pos.comment_two_y_position, default_pos.font_size, YELLOW, comment_two);
   context->render_msg(default_pos.x_position, default_pos.comment_y_position, default_pos.font_size, WHITE, comment);
}

static void select_directory(item *items, menu *current_menu, uint64_t input)
{
   char path[1024], msg[256];
   bool ret = true;

   rmenu_default_positions_t default_pos;

   context->set_default_pos(&default_pos);

   bool is_dir = filebrowser_get_current_path_isdir(&tmpBrowser);
   browser_update(&tmpBrowser, input, "empty");

   if (input & (1 << RETRO_DEVICE_ID_JOYPAD_Y))
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
#ifdef HAVE_XML
            case PATH_CHEATS_DIR_CHOICE:
               strlcpy(g_settings.cheat_database, path, sizeof(g_settings.cheat_database));
               break;
#endif
            case PATH_SYSTEM_DIR_CHOICE:
               strlcpy(g_settings.system_directory, path, sizeof(g_settings.system_directory));
               break;
         }
         menu_stack_decrement();
      }
   }
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_X))
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
#ifdef HAVE_XML
         case PATH_CHEATS_DIR_CHOICE:
            strlcpy(g_settings.cheat_database, path, sizeof(g_settings.cheat_database));
            break;
#endif
         case PATH_SYSTEM_DIR_CHOICE:
            strlcpy(g_settings.system_directory, path, sizeof(g_settings.system_directory));
            break;
      }

      menu_stack_decrement();
   }
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
   {
      if(is_dir)
         ret = filebrowser_iterate(&tmpBrowser, FILEBROWSER_ACTION_OK);
   }

   if(!ret)
      rarch_settings_msg(S_MSG_DIR_LOADING_ERROR, S_DELAY_180);

   display_menubar(current_menu);

   snprintf(msg, sizeof(msg), "[%s] - Enter dir | [%s] - Go back", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_X));
   context->render_msg(default_pos.x_position, default_pos.comment_two_y_position, default_pos.font_size, YELLOW, msg);

   snprintf(msg, sizeof(msg), "[%s] - Reset to startdir", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_START));
   context->render_msg(default_pos.x_position, default_pos.comment_two_y_position + (default_pos.y_position_increment * 1), default_pos.font_size, YELLOW, msg);

   snprintf(msg, sizeof(msg), "INFO - Browse to a directory and assign it as the path by\npressing [%s].", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_Y));
   context->render_msg(default_pos.x_position, default_pos.comment_y_position, default_pos.font_size, WHITE, msg);
}

static void set_keybind_digital(uint64_t default_retro_joypad_id, uint64_t input)
{
   unsigned keybind_action = KEYBIND_NOACTION;

   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
      keybind_action = KEYBIND_DECREMENT;

   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
      keybind_action = KEYBIND_INCREMENT;

   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
      keybind_action = KEYBIND_DEFAULT;

   rarch_input_set_keybind(currently_selected_controller_menu, keybind_action, default_retro_joypad_id);
}

#ifdef __CELLOS_LV2__
static void rarch_filename_input_and_save (unsigned filename_type)
{
   bool filename_entered = false;
   char filename_tmp[256], filepath[PATH_MAX];
   oskutil_write_initial_message(&g_console.oskutil_handle, L"example");
   oskutil_write_message(&g_console.oskutil_handle, L"Enter filename for preset (with no file extension)");

   oskutil_start(&g_console.oskutil_handle);

   while(OSK_IS_RUNNING(g_console.oskutil_handle))
   {
      context->clear();
      context->swap_buffers();
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
      char filetitle_tmp[256];
      oskutil_write_initial_message(&g_console.oskutil_handle, L"Example file title");
      oskutil_write_message(&g_console.oskutil_handle, L"Enter title for preset");
      oskutil_start(&g_console.oskutil_handle);

      while(OSK_IS_RUNNING(g_console.oskutil_handle))
      {
         /* OSK Util gets updated */
         context->clear();
	 context->swap_buffers();
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
#endif

static void producesettingentry(menu *current_menu, item *items, unsigned switchvalue, uint64_t input)
{
   switch(switchvalue)
   {
#ifdef __CELLOS_LV2__
	   case SETTING_CHANGE_RESOLUTION:
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
			   rarch_settings_change(S_RESOLUTION_NEXT);
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
			   rarch_settings_change(S_RESOLUTION_PREVIOUS);
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
		   {
			   if (g_console.supported_resolutions[g_console.current_resolution_index] == CELL_VIDEO_OUT_RESOLUTION_576)
			   {
				   if(gfx_ctx_check_resolution(CELL_VIDEO_OUT_RESOLUTION_576))
				   {
					   //ps3graphics_set_pal60hz(Settings.PS3PALTemporalMode60Hz);
					   video_ptr.restart();
				   }
			   }
			   else
			   {
				   //ps3graphics_set_pal60hz(0);
				   video_ptr.restart();
			   }
		   }
		   break;
		   /*
		      case SETTING_PAL60_MODE:
		      if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
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
#endif
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
	   case SETTING_SHADER_PRESETS:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   if(g_console.emulator_initialized)
			   {
				   menu_stack_push(items, PRESET_CHOICE);
               filebrowser_set_root_and_ext(&tmpBrowser, EXT_CGP_PRESETS, default_paths.cgp_dir);
			   }
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   strlcpy(g_console.cgp_path, "", sizeof(g_console.cgp_path));
		   break;
	   case SETTING_SHADER:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   menu_stack_push(items, SHADER_CHOICE);
			   filebrowser_set_root_and_ext(&tmpBrowser, EXT_SHADERS, default_paths.shader_dir);
			   set_shader = 0;
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_load_shader(1, NULL);
			   strlcpy(g_settings.video.cg_shader_path, default_paths.shader_file, sizeof(g_settings.video.cg_shader_path));
			   menu_stack_refresh(items, current_menu);
		   }
		   break;
	   case SETTING_SHADER_2:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   menu_stack_push( items, SHADER_CHOICE);
			   filebrowser_set_root_and_ext(&tmpBrowser, EXT_SHADERS, default_paths.shader_dir);
			   set_shader = 1;
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_load_shader(2, NULL);
			   strlcpy(g_settings.video.second_pass_shader, default_paths.shader_file, sizeof(g_settings.video.second_pass_shader));
			   menu_stack_refresh(items, current_menu);
		   }
		   break;
#endif
	   case SETTING_FONT_SIZE:
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
		   {
			   if(g_console.menu_font_size > 0) 
				   g_console.menu_font_size -= 0.01f;
		   }
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   if((g_console.menu_font_size < 2.0f))
				   g_console.menu_font_size += 0.01f;
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   g_console.menu_font_size = 1.0f;
		   break;
	   case SETTING_KEEP_ASPECT_RATIO:
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
		   {
			   rarch_settings_change(S_ASPECT_RATIO_DECREMENT);
			   gfx_ctx_set_aspect_ratio(NULL, g_console.aspect_ratio_index);
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
		   {
			   rarch_settings_change(S_ASPECT_RATIO_INCREMENT);
			   gfx_ctx_set_aspect_ratio(NULL, g_console.aspect_ratio_index);
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_default(S_DEF_ASPECT_RATIO);
			   gfx_ctx_set_aspect_ratio(NULL, g_console.aspect_ratio_index);
		   }
		   break;
	   case SETTING_HW_TEXTURE_FILTER:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_HW_TEXTURE_FILTER);
			   gfx_ctx_set_filtering(1, g_settings.video.smooth);
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_change(S_DEF_HW_TEXTURE_FILTER);
			   gfx_ctx_set_filtering(1, g_settings.video.smooth);
		   }
		   break;
#ifdef HAVE_FBO
	   case SETTING_HW_TEXTURE_FILTER_2:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_HW_TEXTURE_FILTER_2);
			   gfx_ctx_set_filtering(2, g_settings.video.second_pass_smooth);
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_change(S_DEF_HW_TEXTURE_FILTER_2);
			   gfx_ctx_set_filtering(2, g_settings.video.second_pass_smooth);
		   }
		   break;
	   case SETTING_SCALE_ENABLED:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_SCALE_ENABLED);
			   gfx_ctx_set_fbo(g_console.fbo_enabled);
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_default(S_DEF_SCALE_ENABLED);
			   apply_scaling(FBO_DEINIT);
			   apply_scaling(FBO_INIT);
		   }
		   break;
	   case SETTING_SCALE_FACTOR:
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
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
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
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
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_default(S_DEF_SCALE_FACTOR);
			   apply_scaling(FBO_DEINIT);
			   apply_scaling(FBO_INIT);
		   }
		   break;
#endif
#ifdef _XBOX1
      case SETTING_FLICKER_FILTER:
         if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
		   {
            if(g_console.flicker_filter > 0)
               g_console.flicker_filter--;
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
		   {
            if(g_console.flicker_filter < 5)
               g_console.flicker_filter++;
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   g_console.flicker_filter = 0;
		   }
         break;
      case SETTING_SOFT_DISPLAY_FILTER:
         if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   g_console.soft_display_filter_enable = !g_console.soft_display_filter_enable;
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
            g_console.soft_display_filter_enable = true;
		   }
         break;
#endif
	   case SETTING_HW_OVERSCAN_AMOUNT:
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
		   {
			   rarch_settings_change(S_OVERSCAN_DECREMENT);
			   gfx_ctx_set_overscan();
		   }
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_OVERSCAN_INCREMENT);
			   gfx_ctx_set_overscan();
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_default(S_DEF_OVERSCAN);
			   gfx_ctx_set_overscan();
		   }
		   break;
	   case SETTING_THROTTLE_MODE:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_THROTTLE);
			   gfx_ctx_set_swap_interval(g_console.throttle_enable, true);
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   rarch_settings_default(S_DEF_THROTTLE);
			   gfx_ctx_set_swap_interval(g_console.throttle_enable, true);
		   }
		   break;
	   case SETTING_TRIPLE_BUFFERING:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_TRIPLE_BUFFERING);
			   video_ptr.restart();
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   bool old_buffer_input = g_console.triple_buffering_enable;
			   rarch_settings_default(S_DEF_TRIPLE_BUFFERING);

			   if(!old_buffer_input)
				   video_ptr.restart();
		   }
		   break;
	   case SETTING_ENABLE_SCREENSHOTS:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
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
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
#if(CELL_SDK_VERSION > 0x340000)
			   g_console.screenshots_enable = true;
#endif
		   }
		   break;
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
	   case SETTING_SAVE_SHADER_PRESET:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
			   rarch_filename_input_and_save(SHADER_PRESET_FILE);
		   break;
	   case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
		   break;
#endif
	   case SETTING_DEFAULT_VIDEO_ALL:
		   break;
	   case SETTING_SOUND_MODE:
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
		   {
			   if(g_console.sound_mode != SOUND_MODE_NORMAL)
				   g_console.sound_mode--;
		   }
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   if(g_console.sound_mode < (SOUND_MODE_LAST-1))
				   g_console.sound_mode++;
		   }
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_UP)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN)))
		   {
#ifdef HAVE_RSOUND
			   if(g_console.sound_mode != SOUND_MODE_RSOUND)
				   rarch_console_rsound_stop();
			   else
				   rarch_console_rsound_start(g_settings.audio.device);
#endif
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   g_console.sound_mode = SOUND_MODE_NORMAL;
#ifdef HAVE_RSOUND
			   rarch_console_rsound_stop();
#endif
		   }
		   break;
#ifdef HAVE_RSOUND
	   case SETTING_RSOUND_SERVER_IP_ADDRESS:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   oskutil_write_initial_message(&g_console.oskutil_handle, L"192.168.1.1");
			   oskutil_write_message(&g_console.oskutil_handle, L"Enter IP address for the RSound Server.");
			   oskutil_start(&g_console.oskutil_handle);
			   while(OSK_IS_RUNNING(g_console.oskutil_handle))
			   {
				   context->clear();
				   context->swap_buffers();
			   }

			   if(g_console.oskutil_handle.text_can_be_fetched)
				   strlcpy(g_settings.audio.device, OUTPUT_TEXT_STRING(g_console.oskutil_handle), sizeof(g_settings.audio.device));
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   strlcpy(g_settings.audio.device, "0.0.0.0", sizeof(g_settings.audio.device));
		   break;
#endif
	   case SETTING_DEFAULT_AUDIO_ALL:
		   break;
	   case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
			   rarch_settings_change(S_SAVESTATE_DECREMENT);
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
			   rarch_settings_change(S_SAVESTATE_INCREMENT);

		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   rarch_settings_default(S_DEF_SAVE_STATE);
		   break;
	   case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
			   g_console.fps_info_msg_enable = !g_console.fps_info_msg_enable;
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   g_console.fps_info_msg_enable = false;
		   break;
	   case SETTING_EMU_SHOW_INFO_MSG:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
			   g_console.info_msg_enable = !g_console.info_msg_enable;
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   g_console.info_msg_enable = true;
		   break;
	   case SETTING_EMU_REWIND_ENABLED:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   rarch_settings_change(S_REWIND);

			   if(g_console.info_msg_enable)
				   rarch_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   g_settings.rewind_enable = false;
		   break;
	   case SETTING_ZIP_EXTRACT:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)))
		   {
			   if(g_console.zip_extract_mode > 0)
				   g_console.zip_extract_mode--;
		   }
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   if(g_console.zip_extract_mode < ZIP_EXTRACT_TO_CACHE_DIR)
				   g_console.zip_extract_mode++;
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
			   g_console.zip_extract_mode = ZIP_EXTRACT_TO_CURRENT_DIR;
		   }
		   break;
	   case SETTING_RARCH_DEFAULT_EMU:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
            menu_stack_push(items, LIBRETRO_CHOICE);
		      filebrowser_set_root_and_ext(&tmpBrowser, EXT_EXECUTABLES, default_paths.core_dir);
		      set_libretro_core_as_launch = false;
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
		   }
		   break;
	   case SETTING_EMU_AUDIO_MUTE:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
            rarch_settings_change(S_AUDIO_MUTE);

		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
            rarch_settings_default(S_DEF_AUDIO_MUTE);
		   break;
	   case SETTING_ENABLE_CUSTOM_BGM:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
#if(CELL_SDK_VERSION > 0x340000)
			   g_console.custom_bgm_enable = !g_console.custom_bgm_enable;
			   if(g_console.custom_bgm_enable)
				   cellSysutilEnableBgmPlayback();
			   else
				   cellSysutilDisableBgmPlayback();

#endif
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
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
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   menu_stack_push(items, PATH_DEFAULT_ROM_DIR_CHOICE);
            filebrowser_set_root_and_ext(&tmpBrowser, "empty", default_paths.filesystem_root_dir);
		   }

		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
            strlcpy(g_console.default_rom_startup_dir, default_paths.filesystem_root_dir, sizeof(g_console.default_rom_startup_dir));
		   break;
	   case SETTING_PATH_SAVESTATES_DIRECTORY:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   menu_stack_push(items, PATH_SAVESTATES_DIR_CHOICE);
            filebrowser_set_root_and_ext(&tmpBrowser, "empty", default_paths.filesystem_root_dir);
		   }

		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   strlcpy(g_console.default_savestate_dir, default_paths.savestate_dir, sizeof(g_console.default_savestate_dir));

		   break;
	   case SETTING_PATH_SRAM_DIRECTORY:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
			   menu_stack_push(items, PATH_SRAM_DIR_CHOICE);
            filebrowser_set_root_and_ext(&tmpBrowser, "empty", default_paths.filesystem_root_dir);
		   }

		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   strlcpy(g_console.default_sram_dir, default_paths.sram_dir, sizeof(g_console.default_sram_dir));
		   break;
#ifdef HAVE_XML
	   case SETTING_PATH_CHEATS:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
            menu_stack_push(items, PATH_CHEATS_DIR_CHOICE);
            filebrowser_set_root_and_ext(&tmpBrowser, "empty", default_paths.filesystem_root_dir);
		   }

		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
            strlcpy(g_settings.cheat_database, default_paths.port_dir, sizeof(g_settings.cheat_database));
		   break;
#endif
	   case SETTING_PATH_SYSTEM:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
            menu_stack_push(items, PATH_SYSTEM_DIR_CHOICE);
            filebrowser_set_root_and_ext(&tmpBrowser, "empty", default_paths.system_dir);
		   }

		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
            strlcpy(g_settings.system_directory, default_paths.system_dir, sizeof(g_settings.system_directory));
		   break;
	   case SETTING_ENABLE_SRAM_PATH:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)))
		   {
            g_console.default_sram_dir_enable = !g_console.default_sram_dir_enable;
		      menu_stack_refresh(items, current_menu);
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
            g_console.default_sram_dir_enable = true;
		      menu_stack_refresh(items, current_menu);
		   }
		   break;
	   case SETTING_ENABLE_STATE_PATH:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)))
		   {
            g_console.default_savestate_dir_enable = !g_console.default_savestate_dir_enable;
		      menu_stack_refresh(items, current_menu);
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
		   {
            g_console.default_savestate_dir_enable = true;
		      menu_stack_refresh(items, current_menu);
		   }
		   break;
	   case SETTING_PATH_DEFAULT_ALL:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_START)))
		   {
            strlcpy(g_console.default_rom_startup_dir, "/", sizeof(g_console.default_rom_startup_dir));
		      strlcpy(g_console.default_savestate_dir, default_paths.port_dir, sizeof(g_console.default_savestate_dir));
#ifdef HAVE_XML
		      strlcpy(g_settings.cheat_database, default_paths.port_dir, sizeof(g_settings.cheat_database));
#endif
		      strlcpy(g_console.default_sram_dir, "", sizeof(g_console.default_sram_dir));

		      menu_stack_refresh(items, current_menu);
		   }
		   break;
	   case SETTING_CONTROLS_SCHEME:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_START)))
		   {
            menu_stack_push(items, INPUT_PRESET_CHOICE);
		      filebrowser_set_root_and_ext(&tmpBrowser, EXT_INPUT_PRESETS, default_paths.input_presets_dir);
		   }
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   menu_stack_refresh(items, current_menu);
		   break;
	   case SETTING_CONTROLS_NUMBER:
		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
		   {
            if(currently_selected_controller_menu != 0)
               currently_selected_controller_menu--;
		      menu_stack_refresh(items, current_menu);
		   }

		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
		   {
            if(currently_selected_controller_menu < 6)
               currently_selected_controller_menu++;
		      menu_stack_refresh(items, current_menu);
		   }

		   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
			   currently_selected_controller_menu = 0;
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
#ifdef __CELLOS_LV2__
	   case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_START)))
            rarch_filename_input_and_save(INPUT_PRESET_FILE);
		   break;
#endif
	   case SETTING_CONTROLS_DEFAULT_ALL:
		   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_START)))
		   {
            rarch_input_set_default_keybinds(currently_selected_controller_menu);
		      menu_stack_refresh(items, current_menu);
		   }
		   break;
   }
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

static void select_setting(item *items, menu *current_menu, uint64_t input)
{
   unsigned i;
   char msg[256];

   rmenu_default_positions_t default_pos;

   context->set_default_pos(&default_pos);

   settings_action_t action = SETTINGS_ACTION_NOOP;

   /* back to ROM menu if CIRCLE is pressed */
   if ((input & (1 << RETRO_DEVICE_ID_JOYPAD_L)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_A)))
      action = SETTINGS_ACTION_TAB_PREVIOUS;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_R))
      action = SETTINGS_ACTION_TAB_NEXT;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
      action = SETTINGS_ACTION_DOWN;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
      action = SETTINGS_ACTION_UP;

   if(action != SETTINGS_ACTION_NOOP)
      settings_iterate(current_menu, items, action);

   producesettingentry(current_menu, items, current_menu->selected, input);

   display_menubar(current_menu);

   for(i = current_menu->first_setting; i < current_menu->max_settings; i++)
   {
      item current_item;
      set_setting_label(i, &current_item);

      if(items[i].page == current_menu->page)
      {
         default_pos.starting_y_position += default_pos.y_position_increment;
         context->render_msg(default_pos.x_position, default_pos.starting_y_position, default_pos.variable_font_size, current_menu->selected == items[i].enum_id ? YELLOW : WHITE, current_item.text);
         context->render_msg(default_pos.x_position_center, default_pos.starting_y_position, default_pos.variable_font_size, WHITE, current_item.setting_text);

         if(current_menu->selected == items[i].enum_id)
         {
            rmenu_position_t position = {0};
            position.x = default_pos.x_position;
            position.y = items[i].text_ypos;

            context->render_selection_panel(&position);
	    context->render_msg(default_pos.x_position, default_pos.comment_y_position, default_pos.font_size, WHITE, current_item.comment);
         }
      }
   }

   snprintf(msg, sizeof(msg), "[%s] + [%s] - resume game | [%s] - go forward", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_L3), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_R3), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_R));
   context->render_msg(default_pos.x_position, default_pos.comment_two_y_position, default_pos.font_size, YELLOW, msg);
   snprintf(msg, sizeof(msg), "[%s] - default | [%s]/[%s] - go back", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_START), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_L), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_A));
   context->render_msg(default_pos.x_position, default_pos.comment_two_y_position + (default_pos.y_position_increment * 1), default_pos.font_size, YELLOW, msg);
}

static void menu_romselect_iterate(filebrowser_t *filebrowser, item *items, menu_romselect_action_t action)
{
   bool ret = true;

   switch(action)
   {
      case MENU_ROMSELECT_ACTION_OK:
         if(filebrowser_get_current_path_isdir(filebrowser))
            ret = filebrowser_iterate(filebrowser, FILEBROWSER_ACTION_OK);
	 else
            rarch_console_load_game_wrap(filebrowser_get_current_path(filebrowser), g_console.zip_extract_mode, S_DELAY_45);
         break;
      case MENU_ROMSELECT_ACTION_GOTO_SETTINGS:
         menu_stack_push(items, GENERAL_VIDEO_MENU);
         break;
      default:
         break;
   }

   if(!ret)
      rarch_settings_msg(S_MSG_DIR_LOADING_ERROR, S_DELAY_180);

}

static void select_rom(item *items, menu *current_menu, uint64_t input)
{
   rmenu_default_positions_t default_pos;

   context->set_default_pos(&default_pos);

   browser_update(&browser, input, rarch_console_get_rom_ext());

   menu_romselect_action_t action = MENU_ROMSELECT_ACTION_NOOP;

   if (input & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT))
      action = MENU_ROMSELECT_ACTION_GOTO_SETTINGS;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
      action = MENU_ROMSELECT_ACTION_OK;

   if (action != MENU_ROMSELECT_ACTION_NOOP)
      menu_romselect_iterate(&browser, items, action);

   bool is_dir = filebrowser_get_current_path_isdir(&browser);

   char msg[128], msg2[128];

   if (is_dir)
   {
      const char *current_path = filebrowser_get_current_path(&browser);
      snprintf(msg, sizeof(msg), "INFO - Press [%s] to enter the directory.", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
   }
   else
      snprintf(msg, sizeof(msg), "INFO - Press [%s] to load the game.", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));

   context->render_msg(default_pos.x_position, default_pos.comment_y_position, default_pos.font_size, WHITE, msg);

   display_menubar(current_menu);

   snprintf(msg, sizeof(msg), "[%s] + [%s] - resume game", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_L3), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_R3));
   snprintf(msg2, sizeof(msg2), "[%s] - Settings", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_SELECT));

   context->render_msg(default_pos.x_position, default_pos.comment_two_y_position, default_pos.font_size, YELLOW, msg);
   context->render_msg(default_pos.x_position, default_pos.comment_two_y_position + (default_pos.y_position_increment * 1), default_pos.font_size, YELLOW, msg2);
}


static void ingame_menu_resize(item *items, menu *current_menu, uint64_t input)
{
   (void)items;

   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   rmenu_default_positions_t default_pos;
   context->set_default_pos(&default_pos);

   g_console.aspect_ratio_index = ASPECT_RATIO_CUSTOM;
   gfx_ctx_set_aspect_ratio(NULL, g_console.aspect_ratio_index);

   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_LEFT))
   {
#ifdef _XBOX
      if(g_console.viewports.custom_vp.x >= 4)
#endif
      {
         g_console.viewports.custom_vp.x -= 4;
      }
   }
   else if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
   {
#ifdef _XBOX
      if(g_console.viewports.custom_vp.x > 0)
#endif
      {
         g_console.viewports.custom_vp.x -= 1;
      }
   }

   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_RIGHT))
      g_console.viewports.custom_vp.x += 4;
   else if(input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      g_console.viewports.custom_vp.x += 1;

   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_UP))
      g_console.viewports.custom_vp.y += 4;
   else if(input & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
      g_console.viewports.custom_vp.y += 1;

   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_DOWN))
   {
#ifdef _XBOX
      if(g_console.viewports.custom_vp.y >= 4)
#endif
      {
         g_console.viewports.custom_vp.y -= 4;
      }
   }
   else if(input & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
   {
#ifdef _XBOX
      if(g_console.viewports.custom_vp.y > 0)
#endif
      {
         g_console.viewports.custom_vp.y -= 1;
      }
   }

   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_LEFT))
      g_console.viewports.custom_vp.width -= 4;
   else if(input & (1 << RETRO_DEVICE_ID_JOYPAD_L))
      g_console.viewports.custom_vp.width -= 1;

   if (input & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_RIGHT))
      g_console.viewports.custom_vp.width += 4;
   else if(input & (1 << RETRO_DEVICE_ID_JOYPAD_R))
      g_console.viewports.custom_vp.width += 1;

   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_UP))
      g_console.viewports.custom_vp.height += 4;
   else if(input & (1 << RETRO_DEVICE_ID_JOYPAD_L2))
      g_console.viewports.custom_vp.height += 1;

   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_DOWN))
      g_console.viewports.custom_vp.height -= 4;
   else if (input & (1 << RETRO_DEVICE_ID_JOYPAD_R2))
      g_console.viewports.custom_vp.height -= 1;

   if (input & (1 << RETRO_DEVICE_ID_JOYPAD_X))
   {
      g_console.viewports.custom_vp.x = 0;
      g_console.viewports.custom_vp.y = 0;
      g_console.viewports.custom_vp.width = device_ptr->win_width;
      g_console.viewports.custom_vp.height = device_ptr->win_height;
   }

   if (input & (1 << RETRO_DEVICE_ID_JOYPAD_A))
      menu_stack_decrement();

   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_Y)) && (old_state & (1 << RETRO_DEVICE_ID_JOYPAD_Y))) { }
   else
   {
      char viewport_x[64], viewport_y[64], viewport_w[64], viewport_h[64];
      char msg[256];
      display_menubar(current_menu);

      snprintf(viewport_x, sizeof(viewport_x), "Viewport X: #%d", g_console.viewports.custom_vp.x);
      snprintf(viewport_y, sizeof(viewport_y), "Viewport Y: #%d", g_console.viewports.custom_vp.y);
      snprintf(viewport_w, sizeof(viewport_w), "Viewport W: #%d", g_console.viewports.custom_vp.width);
      snprintf(viewport_h, sizeof(viewport_h), "Viewport H: #%d", g_console.viewports.custom_vp.height);

      context->render_msg(default_pos.x_position, default_pos.y_position, default_pos.font_size, GREEN, viewport_x);
      context->render_msg(default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*1), default_pos.font_size, GREEN, viewport_y);
      context->render_msg(default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*2), default_pos.font_size, GREEN, viewport_w);
      context->render_msg(default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*3), default_pos.font_size, GREEN, viewport_h);

      context->render_msg(default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*4), default_pos.font_size, WHITE, "CONTROLS:");

      snprintf(msg, sizeof(msg), "[%s] or [%s]", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_LEFT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_LEFT));
      context->render_msg (default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*5), default_pos.font_size,  WHITE, msg);
      context->render_msg (default_pos.x_position_center, default_pos.y_position+(default_pos.y_position_increment*5), default_pos.font_size, WHITE, "- Viewport X --");

      snprintf(msg, sizeof(msg), "[%s] or [%s]", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_RIGHT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_RIGHT));
      context->render_msg (default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*6), default_pos.font_size, WHITE, msg);
      context->render_msg (default_pos.x_position_center, default_pos.y_position+(default_pos.y_position_increment*6), default_pos.font_size, WHITE, "- Viewport X ++");

      snprintf(msg, sizeof(msg), "[%s] or [%s]", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_UP), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_UP));
      context->render_msg (default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*7), default_pos.font_size, WHITE, msg);
      context->render_msg (default_pos.x_position_center, default_pos.y_position+(default_pos.y_position_increment*7), default_pos.font_size, WHITE, "- Viewport Y ++");

      snprintf(msg, sizeof(msg), "[%s] or [%s]", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_DOWN), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_DOWN));
      context->render_msg (default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*8), default_pos.font_size, WHITE, msg);
      context->render_msg (default_pos.x_position_center, default_pos.y_position+(default_pos.y_position_increment*8), default_pos.font_size, WHITE, "- Viewport Y --");

      snprintf(msg, sizeof(msg), "[%s] or [%s]", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_L), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_LEFT));
      context->render_msg (default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*9), default_pos.font_size, WHITE, msg);
      context->render_msg (default_pos.x_position_center, default_pos.y_position+(default_pos.y_position_increment*9), default_pos.font_size, WHITE, "- Viewport W --");

      snprintf(msg, sizeof(msg), "[%s] or [%s]", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_R), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_RIGHT));
      context->render_msg (default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*10), default_pos.font_size, WHITE, msg);
      context->render_msg (default_pos.x_position_center, default_pos.y_position+(default_pos.y_position_increment*10), default_pos.font_size, WHITE, "- Viewport W ++");

      snprintf(msg, sizeof(msg), "[%s] or [%s]", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_L2), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_UP));
      context->render_msg (default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*11), default_pos.font_size, WHITE, msg);
      context->render_msg (default_pos.x_position_center, default_pos.y_position+(default_pos.y_position_increment*11), default_pos.font_size, WHITE, "- Viewport H ++");


      snprintf(msg, sizeof(msg), "[%s] or [%s]", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_R2), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_DOWN));
      context->render_msg (default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*12), default_pos.font_size, WHITE, msg);
      context->render_msg (default_pos.x_position_center, default_pos.y_position+(default_pos.y_position_increment*12), default_pos.font_size, WHITE, "- Viewport H --");

      snprintf(msg, sizeof(msg), "[%s]", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_X));
      context->render_msg (default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*13), default_pos.font_size, WHITE, msg);
      context->render_msg (default_pos.x_position_center, default_pos.y_position+(default_pos.y_position_increment*13), default_pos.font_size, WHITE, "- Reset To Defaults");

      snprintf(msg, sizeof(msg), "[%s]", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_Y));
      context->render_msg (default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*14), default_pos.font_size, WHITE, msg);
      context->render_msg (default_pos.x_position_center, default_pos.y_position+(default_pos.y_position_increment*14), default_pos.font_size, WHITE, "- Show Game");

      snprintf(msg, sizeof(msg), "[%s]", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_A));
      context->render_msg (default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*15), default_pos.font_size, WHITE, msg);
      context->render_msg (default_pos.x_position_center, default_pos.y_position+(default_pos.y_position_increment*15), default_pos.font_size, WHITE, "- Go back");

      snprintf(msg, sizeof(msg), "Allows you to resize the screen.\nPress [%s] to reset to defaults, and [%s] to go back.", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_X), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_A));
      context->render_msg (default_pos.x_position, default_pos.comment_y_position, default_pos.font_size, WHITE, msg);
   }
}

static void ingame_menu_screenshot(item *items, menu *current_menu, uint64_t input)
{
   (void)items;
   (void)current_menu;

   if(g_console.ingame_menu_enable)
   {
      if(input & (1 << RETRO_DEVICE_ID_JOYPAD_A))
      {
         menu_stack_decrement();
         context->render_menu_enable(true);
      }
   }
}

static void ingame_menu(item *items, menu *current_menu, uint64_t input)
{
   char comment[256], overscan_msg[64];
   char strw_buffer[256];
   unsigned menuitem_colors[MENU_ITEM_LAST];

   rmenu_default_positions_t default_pos;
   context->set_default_pos(&default_pos);

   for(int i = 0; i < MENU_ITEM_LAST; i++)
      menuitem_colors[i] = GREEN;

   menuitem_colors[g_console.ingame_menu_item] = RED;

   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_A))
      rarch_settings_change(S_RETURN_TO_GAME);

      switch(g_console.ingame_menu_item)
      {
         case MENU_ITEM_LOAD_STATE:
            if(input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
            {
               rarch_load_state();
               rarch_settings_change(S_RETURN_TO_GAME);
            }
            if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
               rarch_state_slot_decrease();
            if(input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
               rarch_state_slot_increase();
            
            snprintf(comment, sizeof(comment), "Press [%s] / [%s] to change slots.\nPress [%s] to load the state from the current state slot.", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_LEFT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_RIGHT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
            break;
         case MENU_ITEM_SAVE_STATE:
            if(input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
            {
               rarch_save_state();
               rarch_settings_change(S_RETURN_TO_GAME);
            }
            
            if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
               rarch_state_slot_decrease();
            if(input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
               rarch_state_slot_increase();
            
            snprintf(comment, sizeof(comment), "Press [%s] / [%s] to change slots.\nPress [%s] to save the state to the current state slot.",rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_LEFT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_RIGHT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
            break;
         case MENU_ITEM_KEEP_ASPECT_RATIO:
            producesettingentry(current_menu, items, SETTING_KEEP_ASPECT_RATIO, input);
            snprintf(comment, sizeof(comment), "Press [%s] / [%s] to change the [Aspect Ratio].\nPress [%s] to reset back to default values.",rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_LEFT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_RIGHT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_START));
            break;
         case MENU_ITEM_OVERSCAN_AMOUNT:
            producesettingentry(current_menu, items, SETTING_HW_OVERSCAN_AMOUNT, input);
            snprintf(comment, sizeof(comment), "Press [%s] / [%s] to change the [Overscan] settings.\nPress [%s] to reset back to default values.",rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_LEFT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_RIGHT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_START));
            break;
         case MENU_ITEM_ORIENTATION:
            if(input & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
            {
               rarch_settings_change(S_ROTATION_DECREMENT);
               video_ptr.set_rotation(NULL, g_console.screen_orientation);
            }
            
            if((input & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_B)))
            {
               rarch_settings_change(S_ROTATION_INCREMENT);
               video_ptr.set_rotation(NULL, g_console.screen_orientation);
            }
            
            if(input & (1 << RETRO_DEVICE_ID_JOYPAD_START))
            {
               rarch_settings_default(S_DEF_ROTATION);
               video_ptr.set_rotation(NULL, g_console.screen_orientation);
            }
            snprintf(comment, sizeof(comment), "Press [%s] / [%s] to change the [Orientation] settings.\nPress [%s] to reset back to default values.",rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_LEFT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_RIGHT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_START));
            break;
#ifdef HAVE_FBO
	 case MENU_ITEM_SCALE_FACTOR:
       producesettingentry(current_menu, items, SETTING_SCALE_FACTOR, input);
	    snprintf(comment, sizeof(comment), "Press [%s] / [%s] to change the [Scaling] settings.\nPress [%s] to reset back to default values.",rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_LEFT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_RIGHT), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_START));
	    break;
#endif
	 case MENU_ITEM_FRAME_ADVANCE:
       if((input & (1 << RETRO_DEVICE_ID_JOYPAD_B)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_R2)) || (input & (1 << RETRO_DEVICE_ID_JOYPAD_L2)))
	    {
          rarch_settings_change(S_FRAME_ADVANCE);
	       g_console.ingame_menu_item = MENU_ITEM_FRAME_ADVANCE;
	    }
	    snprintf(comment, sizeof(comment), "Press [%s], [%s] or [%s] to step one frame.\nPressing the button rapidly will advance the frame more slowly.", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_L2), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_R2));
	    break;
	 case MENU_ITEM_RESIZE_MODE:
	    if(input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	       menu_stack_push(items, INGAME_MENU_RESIZE);
	    snprintf(comment, sizeof(comment), "Allows you to resize the screen.\nPress [%s] to reset to defaults, and [%s] to go back.", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_X), rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_A));
	    break;
	 case MENU_ITEM_SCREENSHOT_MODE:
	    if(input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	       menu_stack_push(items, INGAME_MENU_SCREENSHOT);
	    snprintf(comment, sizeof(comment), "Allows you to take a screenshot without any text.\nPress [%s] to go back to the in-game menu.", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_A));
	    break;
	 case MENU_ITEM_RETURN_TO_GAME:
	    if(input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
          rarch_settings_change(S_RETURN_TO_GAME);

	    snprintf(comment, sizeof(comment), "Press [%s] to return to the game.", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
	    break;
	 case MENU_ITEM_RESET:
	    if(input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	    {
          rarch_settings_change(S_RETURN_TO_GAME);
	       rarch_game_reset();
	    }
	    snprintf(comment, sizeof(comment), "Press [%s] to reset the game.", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
	    break;
	 case MENU_ITEM_RETURN_TO_MENU:
	    if(input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	    {
          rarch_settings_change(S_RETURN_TO_MENU);
	    }
	    snprintf(comment, sizeof(comment), "Press [%s] to return to the ROM Browser.", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
	    break;
	 case MENU_ITEM_CHANGE_LIBRETRO:
	    if(input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	    {
	       menu_stack_push(items, LIBRETRO_CHOICE);
	       filebrowser_set_root_and_ext(&tmpBrowser, EXT_EXECUTABLES, default_paths.core_dir);
	       set_libretro_core_as_launch = true;
	    }
	    snprintf(comment, sizeof(comment), "Press [%s] to choose another core.", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
	    break;
#ifdef HAVE_MULTIMAN
	 case MENU_ITEM_RETURN_TO_MULTIMAN:
	    if(input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
	    {
               RARCH_LOG("Boot Multiman: %s.\n", default_paths.multiman_self_file);
	       strlcpy(g_console.launch_app_on_exit, default_paths.multiman_self_file,
                 sizeof(g_console.launch_app_on_exit));
	       rarch_settings_change(S_RETURN_TO_LAUNCHER);
	    }
	    snprintf(comment, sizeof(comment), "Press [%s] to quit RetroArch and return to multiMAN.", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
	    break;
#endif
	 case MENU_ITEM_RETURN_TO_DASHBOARD:
	    if(input & (1 << RETRO_DEVICE_ID_JOYPAD_B))
          rarch_settings_change(S_RETURN_TO_DASHBOARD);

	    snprintf(comment, sizeof(comment), "Press [%s] to quit RetroArch.", rarch_input_find_platform_key_label(1 << RETRO_DEVICE_ID_JOYPAD_B));
	    break;
      }

   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
   {
      if(g_console.ingame_menu_item > 0)
         g_console.ingame_menu_item--;
   }

   if(input & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
   {
      if(g_console.ingame_menu_item < (MENU_ITEM_LAST-1))
         g_console.ingame_menu_item++;
   }

   if((input & (1 << RETRO_DEVICE_ID_JOYPAD_L3)) && (input & (1 << RETRO_DEVICE_ID_JOYPAD_R3)))
      rarch_settings_change(S_RETURN_TO_GAME);

   display_menubar(current_menu);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
   context->render_msg(default_pos.x_position, default_pos.y_position, default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_LOAD_STATE), strw_buffer);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
   context->render_msg(default_pos.x_position, default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_SAVE_STATE), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_SAVE_STATE), strw_buffer);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
   context->render_msg(default_pos.x_position, (default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_KEEP_ASPECT_RATIO)), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_KEEP_ASPECT_RATIO), strw_buffer);

   snprintf(overscan_msg, sizeof(overscan_msg), "Overscan: %f", g_console.overscan_amount);
   context->render_msg(default_pos.x_position, (default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_OVERSCAN_AMOUNT)), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_OVERSCAN_AMOUNT), overscan_msg);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
   context->render_msg (default_pos.x_position, (default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_ORIENTATION)), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_ORIENTATION), strw_buffer);

#ifdef HAVE_FBO
   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SCALE_FACTOR, sizeof(strw_buffer));
   context->render_msg (default_pos.x_position, (default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_SCALE_FACTOR)), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_SCALE_FACTOR), strw_buffer);
#endif

   context->render_msg(default_pos.x_position, (default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_RESIZE_MODE)), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_RESIZE_MODE), "Resize Mode");

   context->render_msg(default_pos.x_position, (default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_FRAME_ADVANCE)), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_FRAME_ADVANCE), "Frame Advance");

   context->render_msg(default_pos.x_position, (default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_SCREENSHOT_MODE)), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_SCREENSHOT_MODE), "Screenshot Mode");

   context->render_msg(default_pos.x_position, (default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_RESET)), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_RESET), "Reset");

   context->render_msg(default_pos.x_position, (default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_RETURN_TO_GAME)), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_GAME), "Return to Game");

   context->render_msg(default_pos.x_position, (default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_RETURN_TO_MENU)), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_MENU), "Return to Menu");

   context->render_msg(default_pos.x_position, (default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_CHANGE_LIBRETRO)), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_CHANGE_LIBRETRO), "Change libretro core");

#ifdef HAVE_MULTIMAN
   context->render_msg(default_pos.x_position, (default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_RETURN_TO_MULTIMAN)), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_MULTIMAN), "Return to multiMAN");
#endif

   context->render_msg(default_pos.x_position, (default_pos.y_position+(default_pos.y_position_increment*MENU_ITEM_RETURN_TO_DASHBOARD)), default_pos.font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_DASHBOARD), "Return to Dashboard");

   context->render_msg(default_pos.x_position, default_pos.comment_y_position, default_pos.font_size, WHITE, comment);

   rmenu_position_t position = {0};
   position.x = default_pos.x_position;
   position.y = (default_pos.y_position+(default_pos.y_position_increment*g_console.ingame_menu_item));
   context->render_selection_panel(&position);
}

static void rmenu_filebrowser_init(void)
{
   menu_stack_push(rmenu_items, FILE_BROWSER_MENU);
   filebrowser_set_root_and_ext(&browser, rarch_console_get_rom_ext(), default_paths.filebrowser_startup_dir);
   filebrowser_set_root(&tmpBrowser, default_paths.filesystem_root_dir);
}

static void rmenu_filebrowser_free(void)
{
   filebrowser_free(&browser);
   filebrowser_free(&tmpBrowser);
}

void menu_init (void)
{
#if defined(__CELLOS_LV2__)
   context = &rmenu_ctx_ps3;
#elif defined(_XBOX1)
   context = (rmenu_context_t*)&rmenu_ctx_xdk;
#endif

   //Set libretro filename and version to variable
   struct retro_system_info info;
   retro_get_system_info(&info);
   const char *id = info.library_name ? info.library_name : "Unknown";
   snprintf(m_title, sizeof(m_title), "Libretro core: %s %s", id, info.library_version);

   rmenu_filebrowser_init();
}

void menu_free (void)
{
   rmenu_filebrowser_free();
}

void menu_loop(void)
{
   DEVICE_CAST device_ptr = (DEVICE_CAST)driver.video_data;

   g_console.menu_enable = true;
   device_ptr->block_swap = true;


   if(g_console.ingame_menu_enable)
      menu_stack_push(ingame_menu_settings, INGAME_MENU);

   context->init_textures();

   RARCH_LOG("rmenu items: %d\n", sizeof(rmenu_items));
   RARCH_LOG("rmenu stack: %d\n", sizeof(menuStack));

   do
   {
      //first button input frame
      uint64_t input_state_first_frame = 0;
      uint64_t input_state = 0;
      static bool first_held = false;
      menu *current_menu = menu_stack_get_current_ptr();

      rmenu_default_positions_t default_pos;
      context->set_default_pos(&default_pos);

      input_ptr.poll(NULL);

      const struct retro_keybind *binds[MAX_PLAYERS] = {
	      g_settings.input.binds[0],
	      g_settings.input.binds[1],
	      g_settings.input.binds[2],
	      g_settings.input.binds[3],
	      g_settings.input.binds[4],
	      g_settings.input.binds[5],
	      g_settings.input.binds[6],
	      g_settings.input.binds[7],
      };

      const struct retro_keybind _analog_binds[] = {
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
      bool shoulder_buttons_pressed = ((input_state & (1 << RETRO_DEVICE_ID_JOYPAD_L2)) || (input_state & (1 << RETRO_DEVICE_ID_JOYPAD_R2))) && current_menu->category_id != CATEGORY_SETTINGS;
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
      
      context->clear();

      if(current_menu->enum_id == INGAME_MENU_RESIZE && (trig_state & RETRO_DEVICE_ID_JOYPAD_Y) || current_menu->enum_id == INGAME_MENU_SCREENSHOT)
      {
         context->render_menu_enable(false);
      }
      else
      {
         context->blend(true);
         context->render_menu_enable(true);
      }

      rarch_render_cached_frame();

      filebrowser_t * fb = &browser;

      switch(current_menu->enum_id)
      {
	      case FILE_BROWSER_MENU:
		      select_rom(rmenu_items, current_menu, trig_state);
		      fb = &browser;
		      break;
	      case GENERAL_VIDEO_MENU:
	      case GENERAL_AUDIO_MENU:
	      case EMU_GENERAL_MENU:
	      case EMU_VIDEO_MENU:
	      case EMU_AUDIO_MENU:
	      case PATH_MENU:
	      case CONTROLS_MENU:
		      select_setting(rmenu_items, current_menu, trig_state);
		      break;
	      case SHADER_CHOICE:
	      case PRESET_CHOICE:
	      case BORDER_CHOICE:
	      case LIBRETRO_CHOICE:
	      case INPUT_PRESET_CHOICE:
		      select_file(rmenu_items, current_menu, trig_state);
		      fb = &tmpBrowser;
		      break;
	      case PATH_SAVESTATES_DIR_CHOICE:
	      case PATH_DEFAULT_ROM_DIR_CHOICE:
#ifdef HAVE_XML
	      case PATH_CHEATS_DIR_CHOICE:
#endif
         case PATH_SRAM_DIR_CHOICE:
         case PATH_SYSTEM_DIR_CHOICE:
		      select_directory(rmenu_items, current_menu, trig_state);
		      fb = &tmpBrowser;
		      break;
	      case INGAME_MENU:
		      if(g_console.ingame_menu_enable)
			      ingame_menu(rmenu_items, current_menu, trig_state);
		      break;
	      case INGAME_MENU_RESIZE:
		      ingame_menu_resize(rmenu_items, current_menu, trig_state);
		      break;
	      case INGAME_MENU_SCREENSHOT:
		      ingame_menu_screenshot(rmenu_items, current_menu, trig_state);
		      break;
      }

      switch(current_menu->category_id)
      {
         case CATEGORY_FILEBROWSER:
            browser_render(fb);
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

      const char * message = msg_queue_pull(g_extern.msg_queue);

      if (message && g_console.info_msg_enable)
      {
         context->render_msg(default_pos.msg_queue_x_position, default_pos.msg_queue_y_position, default_pos.msg_queue_font_size, WHITE, message);
      }

      context->swap_buffers();

      if(current_menu->enum_id == INGAME_MENU_RESIZE && (old_state & (1 << RETRO_DEVICE_ID_JOYPAD_Y)) || current_menu->enum_id == INGAME_MENU_SCREENSHOT)
      { }
      else
         context->blend(false);
   }while(g_console.menu_enable);

   context->free_textures();

   if(g_console.ingame_menu_enable)
      menu_stack_decrement();

   device_ptr->block_swap = false;

   g_console.ingame_menu_enable = false;
}
