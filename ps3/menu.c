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

#include "ps3_input.h"
#include "../console/fileio/file_browser.h"

#include "../console/console_ext.h"

#include "ps3_video_psgl.h"

#include "shared.h"
#include "../file.h"
#include "../general.h"

#include "menu.h"
#include "menu-entries.h"

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define NUM_ENTRY_PER_PAGE 19

menu menuStack[25];
int menuStackindex = 0;
static bool set_initial_dir_tmpbrowser;

char special_action_msg[256];			/* message which should be overlaid on top of the screen */
filebrowser_t browser;				/* main file browser->for rom browser*/
filebrowser_t tmpBrowser;			/* tmp file browser->for everything else*/
uint32_t set_shader = 0;
static uint32_t currently_selected_controller_menu = 0;

static menu menu_filebrowser = {
   "FILE BROWSER |",		/* title*/
   FILE_BROWSER_MENU,		/* enum*/
   0,				/* selected item*/
   0,				/* page*/
   1,				/* maxpages */
   1,				/* refreshpage*/
   NULL				/* items*/
};

static menu menu_generalvideosettings = {
   "VIDEO |",			/* title*/
   GENERAL_VIDEO_MENU,		/* enum*/
   FIRST_VIDEO_SETTING,		/* selected item*/
   0,				/* page*/
   MAX_NO_OF_VIDEO_SETTINGS/NUM_ENTRY_PER_PAGE,	/* max pages */
   1,				/* refreshpage*/
   FIRST_VIDEO_SETTING,		/* first setting*/
   MAX_NO_OF_VIDEO_SETTINGS,	/* max no of path settings*/
   items_generalsettings		/* items*/
};

static menu menu_generalaudiosettings = {
   "AUDIO |",			/* title*/
   GENERAL_AUDIO_MENU,		/* enum*/
   FIRST_AUDIO_SETTING,		/* selected item*/
   0,				/* page*/
   MAX_NO_OF_AUDIO_SETTINGS/NUM_ENTRY_PER_PAGE,	/* max pages */
   1,				/* refreshpage*/
   FIRST_AUDIO_SETTING,		/* first setting*/
   MAX_NO_OF_AUDIO_SETTINGS,	/* max no of path settings*/
   items_generalsettings		/* items*/
};

static menu menu_emu_settings = {
   EMU_MENU_TITLE,					/* title*/
   EMU_GENERAL_MENU,					/* enum*/
   FIRST_EMU_SETTING,					/* selected item*/
   0,							/* page*/
   MAX_NO_OF_EMU_SETTINGS/NUM_ENTRY_PER_PAGE,		/* max pages*/
   1,                      				/* refreshpage*/
   FIRST_EMU_SETTING,					/* first setting*/
   MAX_NO_OF_EMU_SETTINGS,				/* max no of path settings*/
   items_generalsettings				/* items*/
};

static menu menu_emu_videosettings = {
   VIDEO_MENU_TITLE,					/* title*/
   EMU_VIDEO_MENU,					/* enum */
   FIRST_EMU_VIDEO_SETTING,				/* selected item*/
   0,							/* page*/
   MAX_NO_OF_EMU_VIDEO_SETTINGS/NUM_ENTRY_PER_PAGE,	/* max pages */
   1,							/* refreshpage*/
   FIRST_EMU_VIDEO_SETTING,				/* first setting*/
   MAX_NO_OF_EMU_VIDEO_SETTINGS,			/* max no of settings*/
   items_generalsettings				/* items*/
};

static menu menu_emu_audiosettings = {
   AUDIO_MENU_TITLE,					/* title*/
   EMU_AUDIO_MENU,					/* enum*/
   FIRST_EMU_AUDIO_SETTING,				/* selected item*/
   0,							/* page*/
   MAX_NO_OF_EMU_AUDIO_SETTINGS/NUM_ENTRY_PER_PAGE,	/* max pages*/
   1,							/* refreshpage*/
   FIRST_EMU_AUDIO_SETTING,				/* first setting*/
   MAX_NO_OF_EMU_AUDIO_SETTINGS,			/* max no of path settings*/
   items_generalsettings				/* items*/
};

static menu menu_pathsettings = {
   "PATH |",						/* title*/
   PATH_MENU,						/* enum*/
   FIRST_PATH_SETTING,					/* selected item*/
   0,							/* page*/
   MAX_NO_OF_PATH_SETTINGS/NUM_ENTRY_PER_PAGE,		/* max pages*/
   1,							/* refreshpage*/
   FIRST_PATH_SETTING,					/* first setting*/
   MAX_NO_OF_PATH_SETTINGS,				/* max no of path settings*/
   items_generalsettings				/* items*/
};

static menu menu_controlssettings = {
   "CONTROLS |",						/* title */
   CONTROLS_MENU,						/* enum */
   FIRST_CONTROLS_SETTING_PAGE_1,				/* selected item */
   0,								/* page */
   MAX_NO_OF_CONTROLS_SETTINGS/NUM_ENTRY_PER_PAGE,		/* max pages */
   1,								/* refreshpage */
   FIRST_CONTROLS_SETTING_PAGE_1,				/* first setting */
   MAX_NO_OF_CONTROLS_SETTINGS,					/* max no of path settings*/
   items_generalsettings					/* items */
};

static void display_menubar(uint32_t menu_enum)
{
   cellDbgFontPuts    (0.09f,  0.05f,  FONT_SIZE,  menu_enum == GENERAL_VIDEO_MENU ? RED : GREEN,   menu_generalvideosettings.title);
   cellDbgFontPuts    (0.19f,  0.05f,  FONT_SIZE,  menu_enum == GENERAL_AUDIO_MENU ? RED : GREEN,  menu_generalaudiosettings.title);
   cellDbgFontPuts    (0.29f,  0.05f,  FONT_SIZE,  menu_enum == EMU_GENERAL_MENU ? RED : GREEN,  menu_emu_settings.title);
   cellDbgFontPuts    (0.39f,  0.05f,  FONT_SIZE,  menu_enum == EMU_VIDEO_MENU ? RED : GREEN,   menu_emu_videosettings.title);
   cellDbgFontPuts    (0.57f,  0.05f,  FONT_SIZE,  menu_enum == EMU_AUDIO_MENU ? RED : GREEN,   menu_emu_audiosettings.title);
   cellDbgFontPuts    (0.09f,  0.09f,  FONT_SIZE,  menu_enum == PATH_MENU ? RED : GREEN,  menu_pathsettings.title);
   cellDbgFontPuts    (0.19f,  0.09f,  FONT_SIZE, menu_enum == CONTROLS_MENU ? RED : GREEN,  menu_controlssettings.title); 
   cellDbgFontPrintf (0.8f, 0.09f, 0.82f, WHITE, "v%s", EMULATOR_VERSION);
   cellDbgFontDraw();
}

enum
{
   DELAY_NONE,
   DELAY_SMALLEST,
   DELAY_SMALL,
   DELAY_MEDIUM,
   DELAY_LONG
};

static uint32_t set_delay = DELAY_NONE;
static uint64_t old_state = 0;

static void set_delay_speed(unsigned delaymode)
{
   unsigned speed;

   speed = 0;

   switch(delaymode)
   {
      case DELAY_NONE:
         break;
      case DELAY_SMALLEST:
	 speed = 4;
	 break;
      case DELAY_SMALL:
	 speed = 7;
	 break;
      case DELAY_MEDIUM:
	 speed = 14;
	 break;
      case DELAY_LONG:
	 speed = 30;
	 break;
   }

   strlcpy(special_action_msg, "", sizeof(special_action_msg));
   SET_TIMER_EXPIRATION(g_console.control_timer_expiration_frame_count, speed);
}

static void browser_update(filebrowser_t * b)
{
   uint64_t state, diff_state, button_was_pressed;

   state = cell_pad_input_poll_device(0);
   diff_state = old_state ^ state;
   button_was_pressed = old_state & diff_state;

   if(IS_TIMER_EXPIRED(g_console.control_timer_expiration_frame_count))
   {
      set_delay = DELAY_NONE;

      if (CTRL_LSTICK_DOWN(state))
      {
         if(b->currently_selected < b->file_count-1)
	 {
            FILEBROWSER_INCREMENT_ENTRY_POINTER(b);

	    if(g_console.emulator_initialized)
               set_delay = DELAY_SMALL;
	    else
               set_delay = DELAY_SMALLEST;
	 }
      }

      if (CTRL_DOWN(state))
      {
         if(b->currently_selected < b->file_count-1)
	 {
            FILEBROWSER_INCREMENT_ENTRY_POINTER(b);
	    if(g_console.emulator_initialized)
               set_delay = DELAY_SMALL;
	    else
               set_delay = DELAY_SMALLEST;
	 }
      }

      if (CTRL_LSTICK_UP(state))
      {
         if(b->currently_selected > 0)
	 {
            FILEBROWSER_DECREMENT_ENTRY_POINTER(b);
	    if(g_console.emulator_initialized)
               set_delay = DELAY_SMALL;
	    else
               set_delay = DELAY_SMALLEST;
	 }
      }

      if (CTRL_UP(state))
      {
         if(b->currently_selected > 0)
	 {
            FILEBROWSER_DECREMENT_ENTRY_POINTER(b);
	    if(g_console.emulator_initialized)
               set_delay = DELAY_SMALL;
	    else
               set_delay = DELAY_SMALLEST;
	 }
      }

      if (CTRL_RIGHT(state))
      {
         b->currently_selected = (MIN(b->currently_selected + 5, b->file_count-1));

	 if(g_console.emulator_initialized)
            set_delay = DELAY_MEDIUM;
	 else
            set_delay = DELAY_SMALL;
      }

      if (CTRL_LSTICK_RIGHT(state))
      {
         b->currently_selected = (MIN(b->currently_selected + 5, b->file_count-1));
	 if(g_console.emulator_initialized)
            set_delay = DELAY_SMALL;
	 else
            set_delay = DELAY_SMALLEST;
      }

      if (CTRL_LEFT(state))
      {
         if (b->currently_selected <= 5)
            b->currently_selected = 0;
	 else
            b->currently_selected -= 5;

	 if(g_console.emulator_initialized)
            set_delay = DELAY_MEDIUM;
	 else
            set_delay = DELAY_SMALL;
      }

      if (CTRL_LSTICK_LEFT(state))
      {
         if (b->currently_selected <= 5)
            b->currently_selected = 0;
	 else
            b->currently_selected -= 5;

	 if(g_console.emulator_initialized)
            set_delay = DELAY_SMALL;
	 else
            set_delay = DELAY_SMALLEST;
      }

      if (CTRL_R1(state))
      {
         b->currently_selected = (MIN(b->currently_selected + NUM_ENTRY_PER_PAGE, b->file_count-1));
	 set_delay = DELAY_MEDIUM;
      }

      if (CTRL_R2(state))
      {
         b->currently_selected = (MIN(b->currently_selected + 50, b->file_count-1));
	 if(!b->currently_selected)
            b->currently_selected = 0;
	 set_delay = DELAY_SMALL;
      }

      if (CTRL_L2(state))
      {
         if (b->currently_selected <= 50)
            b->currently_selected= 0;
	 else
            b->currently_selected -= 50;

	 set_delay = DELAY_SMALL;
      }

      if (CTRL_L1(state))
      {
         if (b->currently_selected <= NUM_ENTRY_PER_PAGE)
            b->currently_selected= 0;
	 else
            b->currently_selected -= NUM_ENTRY_PER_PAGE;

	 set_delay = DELAY_MEDIUM;
      }

      if (CTRL_CIRCLE(button_was_pressed))
      {
         filebrowser_pop_directory(b);
      }

      old_state = state;
   }
}

static void browser_render(filebrowser_t * b)
{
   uint32_t file_count = b->file_count;
   int current_index, page_number, page_base, i;
   float currentX, currentY, ySpacing;

   current_index = b->currently_selected;
   page_number = current_index / NUM_ENTRY_PER_PAGE;
   page_base = page_number * NUM_ENTRY_PER_PAGE;

   currentX = 0.09f;
   currentY = 0.10f;
   ySpacing = 0.035f;

   for ( i = page_base; i < file_count && i < page_base + NUM_ENTRY_PER_PAGE; ++i)
   {
      currentY = currentY + ySpacing;
      cellDbgFontPuts(currentX, currentY, FONT_SIZE, i == current_index ? RED : b->cur[i].d_type == CELL_FS_TYPE_DIRECTORY ? GREEN : WHITE, b->cur[i].d_name);
      cellDbgFontDraw();
   }
   cellDbgFontDraw();
}

static void set_setting_label(menu * menu_obj, uint64_t currentsetting)
{
   switch(currentsetting)
   {
	   case SETTING_CHANGE_RESOLUTION:
		   if(g_console.initial_resolution_id == g_console.supported_resolutions[g_console.current_resolution_index])
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;

		   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), ps3_get_resolution_label(g_console.supported_resolutions[g_console.current_resolution_index]));
		   break;
	   case SETTING_SHADER_PRESETS:
		   {
			   char fname[MAX_PATH_LENGTH];
			   if(g_console.cgp_path == DEFAULT_PRESET_FILE)
				   menu_obj->items[currentsetting].text_color = GREEN;
			   else
				   menu_obj->items[currentsetting].text_color = ORANGE;
			   fill_pathname_base(fname, g_console.cgp_path, sizeof(fname));
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), fname);
		   }
		   break;
	   case SETTING_SHADER:
		   {
			   char fname[MAX_PATH_LENGTH];
			   fill_pathname_base(fname, g_settings.video.cg_shader_path, sizeof(fname));
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%s", fname);

			   if(strcmp(g_settings.video.cg_shader_path,DEFAULT_SHADER_FILE) == 0)
				   menu_obj->items[currentsetting].text_color = GREEN;
			   else
				   menu_obj->items[currentsetting].text_color = ORANGE;
		   }
		   break;
	   case SETTING_SHADER_2:
		   {
			   char fname[MAX_PATH_LENGTH];
			   fill_pathname_base(fname, g_settings.video.second_pass_shader, sizeof(fname));
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%s", fname);

			   if(strcmp(g_settings.video.second_pass_shader,DEFAULT_SHADER_FILE) == 0)
				   menu_obj->items[currentsetting].text_color = GREEN;
			   else
				   menu_obj->items[currentsetting].text_color = ORANGE;
		   }
		   break;
	   case SETTING_FONT_SIZE:
		   if(g_console.menu_font_size == 1.0f)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%f", g_console.menu_font_size);
		   break;
	   case SETTING_KEEP_ASPECT_RATIO:
		   if(g_console.aspect_ratio_index == ASPECT_RATIO_4_3)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), aspectratio_lut[g_console.aspect_ratio_index].name);
		   break;
	   case SETTING_HW_TEXTURE_FILTER:
		   if(g_settings.video.smooth)
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "Linear interpolation");
			   menu_obj->items[currentsetting].text_color = GREEN;
		   }
		   else
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "Point filtering");
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   }
		   break;
	   case SETTING_HW_TEXTURE_FILTER_2:
		   if(g_settings.video.second_pass_smooth)
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "Linear interpolation");
			   menu_obj->items[currentsetting].text_color = GREEN;
		   }
		   else
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "Point filtering");
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   }
		   break;
	   case SETTING_SCALE_ENABLED:
		   if(g_console.fbo_enabled)
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
			   menu_obj->items[currentsetting].text_color = GREEN;
		   }
		   else
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   }
		   break;
	   case SETTING_SCALE_FACTOR:
		   if(g_settings.video.fbo_scale_x == 2.0f)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;

		   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%fx (X) / %fx (Y)", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);
		   snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [Custom Scaling Factor] is set to: '%fx (X) / %fx (Y)'.", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);
		   break;
	   case SETTING_HW_OVERSCAN_AMOUNT:
		   if(g_console.overscan_amount == 0.0f)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;

		   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%f", g_console.overscan_amount);
		   break;
	   case SETTING_THROTTLE_MODE:
		   if(g_console.throttle_enable)
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
			   menu_obj->items[currentsetting].text_color = GREEN;
		   }
		   else
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   }
		   break;
	   case SETTING_TRIPLE_BUFFERING:
		   if(g_console.triple_buffering_enable)
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
			   menu_obj->items[currentsetting].text_color = GREEN;
		   }
		   else
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   }
		   break;
	   case SETTING_ENABLE_SCREENSHOTS:
		   if(g_console.screenshots_enable)
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
			   menu_obj->items[currentsetting].text_color = GREEN;
		   }
		   else
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   }
		   break;
	   case SETTING_SAVE_SHADER_PRESET:
		   if(menu_obj->selected == currentsetting)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   break;
	   case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
		   break;
	   case SETTING_DEFAULT_VIDEO_ALL:
		   break;
	   case SETTING_SOUND_MODE:
		   switch(g_console.sound_mode)
		   {
			   case SOUND_MODE_NORMAL:
				   snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [Sound Output] is set to 'Normal' - normal audio output will be\nused.");
				   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "Normal");
				   menu_obj->items[currentsetting].text_color = GREEN;
				   break;
			   case SOUND_MODE_RSOUND:
				   snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [Sound Output] is set to 'RSound' - the sound will be streamed over the\n network to the RSound audio server." );
				   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "RSound");
				   menu_obj->items[currentsetting].text_color = ORANGE;
				   break;
			   case SOUND_MODE_HEADSET:
				   snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [Sound Output] is set to 'USB/Bluetooth Headset' - sound will\n be output through the headset");
				   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "USB/Bluetooth Headset");
				   menu_obj->items[currentsetting].text_color = ORANGE;
				   break;
		   }
		   break;
	   case SETTING_RSOUND_SERVER_IP_ADDRESS:
		   if(strcmp(g_console.rsound_ip_address,"0.0.0.0") == 0)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;

		   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), g_console.rsound_ip_address);
		   break;
	   case SETTING_DEFAULT_AUDIO_ALL:
		   break;
	   case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
		   if(g_extern.state_slot == 0)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;

		   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%d", g_extern.state_slot);
		   break;
		   /* emu-specific */
	   case SETTING_EMU_DEFAULT_ALL:
		   if(menu_obj->selected == currentsetting)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   break;
	   case SETTING_EMU_REWIND_ENABLED:
		   if(g_settings.rewind_enable)
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
			   menu_obj->items[currentsetting].text_color = ORANGE;
			   snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [Rewind] feature is set to 'ON'. You can rewind the game in real-time.");
		   }
		   else
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
			   menu_obj->items[currentsetting].text_color = GREEN;
			   snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [Rewind] feature is set to 'OFF'.");
		   }
		   break;
	   case SETTING_RARCH_DEFAULT_EMU:
		   {
			   char fname[MAX_PATH_LENGTH];
			   fill_pathname_base(fname, g_settings.libretro, sizeof(fname));
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%s", fname);

			   menu_obj->items[currentsetting].text_color = GREEN;
		   }
		   break;
	   case SETTING_EMU_AUDIO_MUTE:
		   if(g_extern.audio_data.mute)
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
			   menu_obj->items[currentsetting].text_color = ORANGE;
			   snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [Audio Mute] feature is set to 'ON'. The game audio will be muted.");
		   }
		   else
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
			   menu_obj->items[currentsetting].text_color = GREEN;
			   snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [Audio Mute] feature is set to 'OFF'.");
		   }
		   break;
	   case SETTING_ENABLE_CUSTOM_BGM:
		   if(g_console.custom_bgm_enable)
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
			   menu_obj->items[currentsetting].text_color = GREEN;
		   }
		   else
		   {
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   }
		   break;
	   case SETTING_EMU_VIDEO_DEFAULT_ALL:
		   if(menu_obj->selected == currentsetting)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   break;
	   case SETTING_EMU_AUDIO_DEFAULT_ALL:
		   if(menu_obj->selected == currentsetting)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   break;
	   case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
		   if(!(strcmp(g_console.default_rom_startup_dir, "/")))
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;

		   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), g_console.default_rom_startup_dir);
		   break;
	   case SETTING_PATH_SAVESTATES_DIRECTORY:
		   if(!(strcmp(g_console.default_savestate_dir, usrDirPath)))
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;

		   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), g_console.default_savestate_dir);
		   break;
	   case SETTING_PATH_SRAM_DIRECTORY:
		   if(!(strcmp(g_console.default_sram_dir, usrDirPath)))
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;

		   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), g_console.default_sram_dir);
		   break;
	   case SETTING_PATH_CHEATS:
		   if(!(strcmp(g_settings.cheat_database, usrDirPath)))
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;

		   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), g_settings.cheat_database);
		   break;
	   case SETTING_ENABLE_SRAM_PATH:
		   if(g_console.default_sram_dir_enable)
		   {
			   menu_obj->items[currentsetting].text_color = ORANGE;
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
		   }
		   else
		   {
			   menu_obj->items[currentsetting].text_color = GREEN;
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
		   }

		   break;
	   case SETTING_ENABLE_STATE_PATH:
		   if(g_console.default_savestate_dir_enable)
		   {
			   menu_obj->items[currentsetting].text_color = ORANGE;
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "ON");
		   }
		   else
		   {
			   menu_obj->items[currentsetting].text_color = GREEN;
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "OFF");
		   }
		   break;
	   case SETTING_PATH_DEFAULT_ALL:
		   if(menu_obj->selected == currentsetting)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   break;
	   case SETTING_CONTROLS_SCHEME:
		   if(strcmp(g_console.input_cfg_path,"") == 0)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;

		   snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - Input scheme preset [%s] is selected.", g_console.input_cfg_path);
		   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), g_console.input_cfg_path);
		   break;
	   case SETTING_CONTROLS_NUMBER:
		   if(currently_selected_controller_menu == 0)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;

		   snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "Controller %d is currently selected.", currently_selected_controller_menu+1);
		   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), "%d", currently_selected_controller_menu+1);
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
			   if(g_settings.input.binds[currently_selected_controller_menu][currentsetting-(FIRST_CONTROL_BIND)].joykey == rarch_default_keybind_lut[currentsetting-FIRST_CONTROL_BIND])
				   menu_obj->items[currentsetting].text_color = GREEN;
			   else
				   menu_obj->items[currentsetting].text_color = ORANGE;
			   const char * value = rarch_input_find_platform_key_label(g_settings.input.binds[currently_selected_controller_menu][currentsetting-(FIRST_CONTROL_BIND)].joykey);
			   snprintf(menu_obj->items[currentsetting].text, sizeof(menu_obj->items[currentsetting].text), rarch_default_libretro_keybind_name_lut[currentsetting-(FIRST_CONTROL_BIND)]);
			   snprintf(menu_obj->items[currentsetting].comment, sizeof(menu_obj->items[currentsetting].comment), "INFO - [%s] on the PS3 controller is mapped to action:\n[%s].", menu_obj->items[currentsetting].text, value);
			   snprintf(menu_obj->items[currentsetting].setting_text, sizeof(menu_obj->items[currentsetting].setting_text), value);
		   }
		   break;
	   case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
		   if(menu_obj->selected == currentsetting)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   break;
	   case SETTING_CONTROLS_DEFAULT_ALL:
		   if(menu_obj->selected == currentsetting)
			   menu_obj->items[currentsetting].text_color = GREEN;
		   else
			   menu_obj->items[currentsetting].text_color = ORANGE;
		   break;
	   default:
		   break;
   }
}

static void menu_init_settings_pages(menu * menu_obj)
{
   int page, i, j;
   float increment;

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

      menu_obj->items[i].text_xpos = 0.09f;
      menu_obj->items[i].text_ypos = increment; 
      menu_obj->items[i].page = page;
      set_setting_label(menu_obj, i);
      increment += 0.03f;
      j++;
   }
   menu_obj->refreshpage = 0;
}

static void menu_reinit_settings (void)
{
   menu_init_settings_pages(&menu_generalvideosettings);
   menu_init_settings_pages(&menu_generalaudiosettings);
   menu_init_settings_pages(&menu_emu_settings);
   menu_init_settings_pages(&menu_emu_videosettings);
   menu_init_settings_pages(&menu_emu_audiosettings);
   menu_init_settings_pages(&menu_pathsettings);
   menu_init_settings_pages(&menu_controlssettings);
}

#define INPUT_SCALE 2

static void apply_scaling (unsigned init_mode)
{
   switch(init_mode)
   {
      case FBO_DEINIT:
         gl_deinit_fbo(g_gl);
	 break;
      case FBO_INIT:
	 gl_init_fbo(g_gl, RARCH_SCALE_BASE * INPUT_SCALE,
			 RARCH_SCALE_BASE * INPUT_SCALE);
	 break;
      case FBO_REINIT:
	 gl_deinit_fbo(g_gl);
	 gl_init_fbo(g_gl, RARCH_SCALE_BASE * INPUT_SCALE,
			 RARCH_SCALE_BASE * INPUT_SCALE);
	 break;
   }
}

static void select_file(uint32_t menu_id)
{
   char extensions[256], title[256], object[256], comment[256], dir_path[MAX_PATH_LENGTH],
      path[MAX_PATH_LENGTH], *separatorslash;
   uint64_t state, diff_state, button_was_pressed;

   state = cell_pad_input_poll_device(0);
   diff_state = old_state ^ state;
   button_was_pressed = old_state & diff_state;

   switch(menu_id)
   {
      case SHADER_CHOICE:
         strncpy(dir_path, SHADERS_DIR_PATH, sizeof(dir_path));
	 strncpy(extensions, "cg|CG", sizeof(extensions));
	 strncpy(title, "SHADER SELECTION", sizeof(title));
	 strncpy(object, "Shader", sizeof(object));
	 strncpy(comment, "INFO - Select a shader from the menu by pressing the X button.", sizeof(comment));
	 break;
      case PRESET_CHOICE:
	 strncpy(dir_path, PRESETS_DIR_PATH, sizeof(dir_path));
	 strncpy(extensions, "cgp|CGP", sizeof(extensions));
	 strncpy(title, "SHADER PRESETS SELECTION", sizeof(title));
	 strncpy(object, "Shader", sizeof(object));
	 strncpy(object, "Shader preset", sizeof(object));
	 strncpy(comment, "INFO - Select a shader preset from the menu by pressing the X button.", sizeof(comment));
	 break;
      case INPUT_PRESET_CHOICE:
	 strncpy(dir_path, INPUT_PRESETS_DIR_PATH, sizeof(dir_path));
	 strncpy(extensions, "cfg|CFG", sizeof(extensions));
	 strncpy(title, "INPUT PRESETS SELECTION", sizeof(title));
	 strncpy(object, "Input", sizeof(object));
	 strncpy(object, "Input preset", sizeof(object));
	 strncpy(comment, "INFO - Select an input preset from the menu by pressing the X button.", sizeof(comment));
	 break;
      case BORDER_CHOICE:
	 strncpy(dir_path, BORDERS_DIR_PATH, sizeof(dir_path));
	 strncpy(extensions, "png|PNG|jpg|JPG|JPEG|jpeg", sizeof(extensions));
	 strncpy(title, "BORDER SELECTION", sizeof(title));
	 strncpy(object, "Border", sizeof(object));
	 strncpy(object, "Border image file", sizeof(object));
	 strncpy(comment, "INFO - Select a border image file from the menu by pressing the X button.", sizeof(comment));
	 break;
      case LIBRETRO_CHOICE:
	 strncpy(dir_path, LIBRETRO_DIR_PATH, sizeof(dir_path));
	 strncpy(extensions, "self|SELF|bin|BIN", sizeof(extensions));
	 strncpy(title, "LIBRETRO CORE SELECTION", sizeof(title));
	 strncpy(object, "Libretro", sizeof(object));
	 strncpy(object, "Libretro core library", sizeof(object));
	 strncpy(comment, "INFO - Select a Libretro core from the menu by pressing the X button.", sizeof(comment));
	 break;
   }

   if(set_initial_dir_tmpbrowser)
   {
      filebrowser_new(&tmpBrowser, dir_path, extensions);
      set_initial_dir_tmpbrowser = false;
   }

   browser_update(&tmpBrowser);

   if(IS_TIMER_EXPIRED(g_console.control_timer_expiration_frame_count))
   {
      if (CTRL_START(button_was_pressed))
         filebrowser_reset_start_directory(&tmpBrowser, "/", extensions);

      if (CTRL_CROSS(button_was_pressed))
      {
         if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(tmpBrowser))
	 {
            /*if 'filename' is in fact '..' - then pop back directory instead of 
	      adding '..' to filename path */
            if(tmpBrowser.currently_selected == 0)
	    {
               filebrowser_pop_directory(&tmpBrowser);
	    }
	    else
	    {
               separatorslash = (strcmp(FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser),"/") == 0) ? "" : "/";
	       snprintf(path, sizeof(path), "%s%s%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), separatorslash, FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
	       filebrowser_push_directory(&tmpBrowser, path, true);
	    }
	 }
	 else if (FILEBROWSER_IS_CURRENT_A_FILE(tmpBrowser))
	 {
            snprintf(path, sizeof(path), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
	    printf("path: %s\n", path);

	    switch(menu_id)
	    {
               case SHADER_CHOICE:
                  gl_cg_load_shader(set_shader+1, path);
		  switch(set_shader+1)
		  {
                     case 1:
                        strlcpy(g_settings.video.cg_shader_path, path, sizeof(g_settings.video.cg_shader_path));
			break;
		     case 2:
			strlcpy(g_settings.video.second_pass_shader, path, sizeof(g_settings.video.second_pass_shader));
			break;
		  }
		  menu_reinit_settings();
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
		  menu_reinit_settings();
		  break;
	       case BORDER_CHOICE:
		  break;
	       case LIBRETRO_CHOICE:
		  strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
		  strlcpy(g_console.launch_app_on_exit, path, sizeof(g_console.launch_app_on_exit));
		  g_console.return_to_launcher = true;
		  g_console.menu_enable = false;
		  g_console.mode_switch = MODE_EXIT;
		  break;
	    }

            menuStackindex--;
	 }
      }

      if (CTRL_TRIANGLE(button_was_pressed))
         menuStackindex--;
   }

   cellDbgFontPrintf(0.09f, 0.09f, FONT_SIZE, YELLOW, "PATH: %s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser));
   cellDbgFontPuts	(0.09f,	0.05f,	FONT_SIZE,	RED,	title);
   cellDbgFontPrintf(0.09f, 0.92f, 0.92, YELLOW, "X - Select %s  /\\ - return to settings  START - Reset Startdir", object);
   cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s", comment);
   cellDbgFontDraw();

   browser_render(&tmpBrowser);
   old_state = state;
}

static void select_directory(uint32_t menu_id)
{
   char path[1024], newpath[1024], *separatorslash;
   uint64_t state, diff_state, button_was_pressed;

   state = cell_pad_input_poll_device(0);
   diff_state = old_state ^ state;
   button_was_pressed = old_state & diff_state;

   if(set_initial_dir_tmpbrowser)
   {
	   filebrowser_new(&tmpBrowser, "/\0", "empty");
	   set_initial_dir_tmpbrowser = false;
   }

   browser_update(&tmpBrowser);

   if(IS_TIMER_EXPIRED(g_console.control_timer_expiration_frame_count))
   {
      if (CTRL_START(button_was_pressed))
         filebrowser_reset_start_directory(&tmpBrowser, "/","empty");

      if (CTRL_SQUARE(button_was_pressed))
      {
         if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(tmpBrowser))
	 {
            snprintf(path, sizeof(path), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
	    switch(menu_id)
	    {
               case PATH_SAVESTATES_DIR_CHOICE:
                  strcpy(g_console.default_savestate_dir, path);
		  break;
	       case PATH_SRAM_DIR_CHOICE:
		  strcpy(g_console.default_sram_dir, path);
		  break;
	       case PATH_DEFAULT_ROM_DIR_CHOICE:
		  strcpy(g_console.default_rom_startup_dir, path);
		  break;
	       case PATH_CHEATS_DIR_CHOICE:
		  strcpy(g_settings.cheat_database, path);
		  break;
	    }
	    menuStackindex--;
	 }
      }

      if (CTRL_TRIANGLE(button_was_pressed))
      {
         strcpy(path, usrDirPath);
	 switch(menu_id)
	 {
            case PATH_SAVESTATES_DIR_CHOICE:
               strcpy(g_console.default_savestate_dir, path);
	       break;
	    case PATH_SRAM_DIR_CHOICE:
	       strcpy(g_console.default_sram_dir, path);
	       break;
	    case PATH_DEFAULT_ROM_DIR_CHOICE:
	       strcpy(g_console.default_rom_startup_dir, path);
	       break;
	    case PATH_CHEATS_DIR_CHOICE:
	       strcpy(g_settings.cheat_database, path);
	       break;
	 }
	 menuStackindex--;
      }

      if (CTRL_CROSS(button_was_pressed))
      {
         if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(tmpBrowser))
	 {
            /* if 'filename' is in fact '..' - then pop back directory instead of 
             * adding '..' to filename path */

            if(tmpBrowser.currently_selected == 0)
	    {
               filebrowser_pop_directory(&tmpBrowser);
	    }
	    else
	    {
               separatorslash = (strcmp(FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser),"/") == 0) ? "" : "/";
	       snprintf(newpath, sizeof(newpath), "%s%s%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), separatorslash, FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
	       filebrowser_push_directory(&tmpBrowser, newpath, false);
	    }
	 }
      }
   }

   cellDbgFontPrintf (0.09f,  0.09f, FONT_SIZE, YELLOW, 
      "PATH: %s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser));
   cellDbgFontPuts (0.09f, 0.05f,  FONT_SIZE, RED,    "DIRECTORY SELECTION");
   cellDbgFontPuts(0.09f, 0.93f, 0.92f, YELLOW,
      "X - Enter dir  /\\ - return to settings  START - Reset Startdir");
   cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s",
      "INFO - Browse to a directory and assign it as the path by\npressing SQUARE button.");
   cellDbgFontDraw();

   browser_render(&tmpBrowser);
   old_state = state;
}

static void set_keybind_digital(uint64_t state, uint64_t default_retro_joypad_id)
{
   unsigned keybind_action = KEYBIND_NOACTION;

   if(CTRL_LEFT(state) | CTRL_LSTICK_LEFT(state))
      keybind_action = KEYBIND_DECREMENT;

   if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
      keybind_action = KEYBIND_INCREMENT;

   if(CTRL_START(state))
      keybind_action = KEYBIND_DEFAULT;

   rarch_input_set_keybind(currently_selected_controller_menu, keybind_action, default_retro_joypad_id);

   if(keybind_action != KEYBIND_NOACTION)
      set_delay = DELAY_MEDIUM;
}

static void rarch_filename_input_and_save (unsigned filename_type)
{
   bool filename_entered = false;
   char filename_tmp[256], filepath[MAX_PATH_LENGTH];
   oskutil_write_initial_message(&g_console.oskutil_handle, L"example");
   oskutil_write_message(&g_console.oskutil_handle, L"Enter filename for preset (with no file extension)");

   oskutil_start(&g_console.oskutil_handle);

   while(OSK_IS_RUNNING(g_console.oskutil_handle))
   {
      glClear(GL_COLOR_BUFFER_BIT);
      gl_frame_menu();
      video_gl.swap(NULL);
      cellSysutilCheckCallback();
   }

   if(g_console.oskutil_handle.text_can_be_fetched)
   {
      strncpy(filename_tmp, OUTPUT_TEXT_STRING(g_console.oskutil_handle), sizeof(filename_tmp));
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
         glClear(GL_COLOR_BUFFER_BIT);
	 video_gl.swap(NULL);
	 cellSysutilCheckCallback();
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

static void producesettingentry(menu * menu_obj, uint64_t switchvalue)
{
	uint64_t state;

	state = cell_pad_input_poll_device(0);

	switch(switchvalue)
	{
		case SETTING_CHANGE_RESOLUTION:
			if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) )
			{
				ps3_next_resolution();
				set_delay = DELAY_SMALL;
			}
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) )
			{
				ps3_previous_resolution();
				set_delay = DELAY_SMALL;
			}
			if(CTRL_CROSS(state))
			{
				if (g_console.supported_resolutions[g_console.current_resolution_index] == CELL_VIDEO_OUT_RESOLUTION_576)
				{
					if(ps3_check_resolution(CELL_VIDEO_OUT_RESOLUTION_576))
					{
						//ps3graphics_set_pal60hz(Settings.PS3PALTemporalMode60Hz);
						ps3graphics_video_reinit();
					}
				}
				else
				{
					//ps3graphics_set_pal60hz(0);
					ps3graphics_video_reinit();
				}
			}
			break;
			/*
			   case SETTING_PAL60_MODE:
			   if(CTRL_RIGHT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state) || CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
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
			if((CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state)))
			{
				if(g_console.emulator_initialized)
				{
					menuStackindex++;
					menuStack[menuStackindex] = menu_filebrowser;
					menuStack[menuStackindex].enum_id = PRESET_CHOICE;
					set_initial_dir_tmpbrowser = true;
					set_delay = DELAY_LONG;
				}
			}
			if(CTRL_START(state))
			{
				strlcpy(g_console.cgp_path, "", sizeof(g_console.cgp_path));
			}
			break;
		case SETTING_SHADER:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = SHADER_CHOICE;
				set_shader = 0;
				set_initial_dir_tmpbrowser = true;
				set_delay = DELAY_LONG;
			}
			if(CTRL_START(state))
			{
				gl_cg_load_shader(1, NULL);
				strlcpy(g_settings.video.cg_shader_path, DEFAULT_SHADER_FILE, sizeof(g_settings.video.cg_shader_path));
				menu_reinit_settings();
			}
			break;
		case SETTING_SHADER_2:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = SHADER_CHOICE;
				set_shader = 1;
				set_initial_dir_tmpbrowser = true;
				set_delay = DELAY_LONG;
			}
			if(CTRL_START(state))
			{
				gl_cg_load_shader(2, NULL);
				strlcpy(g_settings.video.second_pass_shader, DEFAULT_SHADER_FILE, sizeof(g_settings.video.second_pass_shader));
				menu_reinit_settings();
			}
			break;
		case SETTING_FONT_SIZE:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state))
			{
				if(g_console.menu_font_size > 0) 
				{
					g_console.menu_font_size -= 0.01f;
					set_delay = DELAY_MEDIUM;
				}
			}
			if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				if((g_console.menu_font_size < 2.0f))
				{
					g_console.menu_font_size += 0.01f;
					set_delay = DELAY_MEDIUM;
				}
			}
			if(CTRL_START(state))
				g_console.menu_font_size = 1.0f;
			break;
		case SETTING_KEEP_ASPECT_RATIO:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
			{
				if(g_console.aspect_ratio_index > 0)
				{
					g_console.aspect_ratio_index--;
					video_gl.set_aspect_ratio(NULL, g_console.aspect_ratio_index);
					set_delay = DELAY_SMALL;
				}
			}
			if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
			{
				g_console.aspect_ratio_index++;
				if(g_console.aspect_ratio_index < ASPECT_RATIO_END)
				{
					video_gl.set_aspect_ratio(NULL, g_console.aspect_ratio_index);
					set_delay = DELAY_SMALL;
				}
				else
					g_console.aspect_ratio_index = ASPECT_RATIO_END-1;
			}
			if(CTRL_START(state))
			{
				g_console.aspect_ratio_index = ASPECT_RATIO_4_3;
				video_gl.set_aspect_ratio(NULL, g_console.aspect_ratio_index);
			}
			break;
		case SETTING_HW_TEXTURE_FILTER:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_settings.video.smooth = !g_settings.video.smooth;
				ps3_set_filtering(1, g_settings.video.smooth);
				set_delay = DELAY_LONG;
			}
			if(CTRL_START(state))
			{
				g_settings.video.smooth = 1;
				ps3_set_filtering(1, g_settings.video.smooth);
			}
			break;
		case SETTING_HW_TEXTURE_FILTER_2:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_settings.video.second_pass_smooth = !g_settings.video.second_pass_smooth;
				ps3_set_filtering(2, g_settings.video.second_pass_smooth);
				set_delay = DELAY_LONG;
			}
			if(CTRL_START(state))
			{
				g_settings.video.second_pass_smooth = 1;
				ps3_set_filtering(2, g_settings.video.second_pass_smooth);
			}
			break;
		case SETTING_SCALE_ENABLED:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_console.fbo_enabled = !g_console.fbo_enabled;
				gl_set_fbo_enable(g_console.fbo_enabled);

				set_delay = DELAY_MEDIUM;

			}
			if(CTRL_START(state))
			{
				g_console.fbo_enabled = true;
				g_settings.video.fbo_scale_x = 2.0f;
				g_settings.video.fbo_scale_y = 2.0f;
				apply_scaling(FBO_DEINIT);
				apply_scaling(FBO_INIT);
			}
			break;
		case SETTING_SCALE_FACTOR:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
			{
				if(g_console.fbo_enabled)
				{
					if((g_settings.video.fbo_scale_x > MIN_SCALING_FACTOR))
					{
						g_settings.video.fbo_scale_x -= 1.0f;
						g_settings.video.fbo_scale_y -= 1.0f;
						apply_scaling(FBO_REINIT);
						set_delay = DELAY_MEDIUM;
					}
				}
			}
			if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				if(g_console.fbo_enabled)
				{
					if((g_settings.video.fbo_scale_x < MAX_SCALING_FACTOR))
					{
						g_settings.video.fbo_scale_x += 1.0f;
						g_settings.video.fbo_scale_y += 1.0f;
						apply_scaling(FBO_REINIT);
						set_delay = DELAY_MEDIUM;
					}
				}
			}
			if(CTRL_START(state))
			{
				g_settings.video.fbo_scale_x = 2.0f;
				g_settings.video.fbo_scale_y = 2.0f;
				apply_scaling(FBO_DEINIT);
				apply_scaling(FBO_INIT);
			}
			break;
		case SETTING_HW_OVERSCAN_AMOUNT:
			if(CTRL_LEFT(state)  ||  CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state))
			{
				g_console.overscan_amount -= 0.01f;
				g_console.overscan_enable = true;

				if(g_console.overscan_amount == 0.0f)
					g_console.overscan_enable = false;

				ps3graphics_set_overscan(g_console.overscan_enable, g_console.overscan_amount, 1);
				set_delay = DELAY_SMALLEST;
			}
			if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_console.overscan_amount += 0.01f;
				g_console.overscan_enable = true;

				if(g_console.overscan_amount == 0.0f)
					g_console.overscan_enable = 0;

				ps3graphics_set_overscan(g_console.overscan_enable, g_console.overscan_amount, 1);
				set_delay = DELAY_SMALLEST;
			}
			if(CTRL_START(state))
			{
				g_console.overscan_amount = 0.0f;
				g_console.overscan_enable = false;
				ps3graphics_set_overscan(g_console.overscan_enable, g_console.overscan_amount, 1);
			}
			break;
		case SETTING_THROTTLE_MODE:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
			{
				g_console.throttle_enable = !g_console.throttle_enable;
				ps3graphics_set_vsync(g_console.throttle_enable);
				set_delay = DELAY_MEDIUM;
			}
			if(CTRL_START(state))
			{
				g_console.throttle_enable = true;
				ps3graphics_set_vsync(g_console.throttle_enable);
				set_delay = DELAY_MEDIUM;
			}
			break;
		case SETTING_TRIPLE_BUFFERING:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
			{
				g_console.triple_buffering_enable = !g_console.triple_buffering_enable;
				ps3graphics_video_reinit();
				set_delay = DELAY_MEDIUM;
			}
			if(CTRL_START(state))
			{
				if(!g_console.triple_buffering_enable)
				{
					g_console.triple_buffering_enable = true;
					ps3graphics_video_reinit();
				}
			}
			break;
		case SETTING_ENABLE_SCREENSHOTS:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
			{
#if(CELL_SDK_VERSION > 0x340000)
				g_console.screenshots_enable = !g_console.screenshots_enable;
				if(g_console.screenshots_enable)
				{
					cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
					CellScreenShotSetParam screenshot_param = {0, 0, 0, 0};

					screenshot_param.photo_title = EMULATOR_NAME;
					screenshot_param.game_title = EMULATOR_NAME;
					cellScreenShotSetParameter (&screenshot_param);
					cellScreenShotEnable();
				}
				else
				{
					cellScreenShotDisable();
					cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
				}

				set_delay = DELAY_MEDIUM;
#endif
			}
			if(CTRL_START(state))
			{
#if(CELL_SDK_VERSION > 0x340000)
				g_console.screenshots_enable = true;
#endif
			}
			break;
		case SETTING_SAVE_SHADER_PRESET:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state)  || CTRL_RIGHT(state) | CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				rarch_filename_input_and_save(SHADER_PRESET_FILE);
			}
			break;
		case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
			break;
		case SETTING_DEFAULT_VIDEO_ALL:
			break;
		case SETTING_SOUND_MODE:
			if(CTRL_LEFT(state) ||  CTRL_LSTICK_LEFT(state))
			{
				if(g_console.sound_mode != SOUND_MODE_NORMAL)
				{
					g_console.sound_mode--;
					//emulator_toggle_sound(g_console.sound_mode);
					set_delay = DELAY_MEDIUM;
				}
			}
			if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				if(g_console.sound_mode < SOUND_MODE_HEADSET)
				{
					g_console.sound_mode++;
					//emulator_toggle_sound(g_console.sound_mode);
					set_delay = DELAY_MEDIUM;
				}
			}
			if(CTRL_START(state))
			{
				g_console.sound_mode = SOUND_MODE_NORMAL;
				//emulator_toggle_sound(g_console.sound_mode);
				set_delay = DELAY_MEDIUM;
			}
			break;
		case SETTING_RSOUND_SERVER_IP_ADDRESS:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_CROSS(state) | CTRL_LSTICK_RIGHT(state) )
			{
				oskutil_write_initial_message(&g_console.oskutil_handle, L"192.168.1.1");
				oskutil_write_message(&g_console.oskutil_handle, L"Enter IP address for the RSound Server.");
				oskutil_start(&g_console.oskutil_handle);
				while(OSK_IS_RUNNING(g_console.oskutil_handle))
				{
					glClear(GL_COLOR_BUFFER_BIT);
					video_gl.swap(NULL);
					cellSysutilCheckCallback();
				}

				if(g_console.oskutil_handle.text_can_be_fetched)
					strcpy(g_console.rsound_ip_address, OUTPUT_TEXT_STRING(g_console.oskutil_handle));
			}
			if(CTRL_START(state))
				strcpy(g_console.rsound_ip_address, "0.0.0.0");
			break;
		case SETTING_DEFAULT_AUDIO_ALL:
			break;
		case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state))
			{
				if(g_extern.state_slot != 0)
					g_extern.state_slot--;

				set_delay = DELAY_MEDIUM;
			}
			if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_extern.state_slot++;
				set_delay = DELAY_MEDIUM;
			}

			if(CTRL_START(state))
				g_extern.state_slot = 0;
			break;
		case SETTING_EMU_REWIND_ENABLED:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_settings.rewind_enable = !g_settings.rewind_enable;

				set_delay = DELAY_MEDIUM;
			}
			if(CTRL_START(state))
			{
				g_settings.rewind_enable = false;
			}
			break;
		case SETTING_RARCH_DEFAULT_EMU:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = LIBRETRO_CHOICE;
				set_initial_dir_tmpbrowser = true;
				set_delay = DELAY_LONG;
			}
			if(CTRL_START(state))
			{
			}
			break;
		case SETTING_EMU_AUDIO_MUTE:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				g_extern.audio_data.mute = !g_extern.audio_data.mute;

				set_delay = DELAY_MEDIUM;
			}
			if(CTRL_START(state))
			{
				g_extern.audio_data.mute = false;
			}
			break;
		case SETTING_ENABLE_CUSTOM_BGM:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
			{
#if(CELL_SDK_VERSION > 0x340000)
				g_console.custom_bgm_enable = !g_console.custom_bgm_enable;
				if(g_console.custom_bgm_enable)
					cellSysutilEnableBgmPlayback();
				else
					cellSysutilDisableBgmPlayback();

				set_delay = DELAY_MEDIUM;
#endif
			}
			if(CTRL_START(state))
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
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = PATH_DEFAULT_ROM_DIR_CHOICE;
				set_initial_dir_tmpbrowser = true;
				set_delay = DELAY_LONG;
			}

			if(CTRL_START(state))
				strcpy(g_console.default_rom_startup_dir, "/");
			break;
		case SETTING_PATH_SAVESTATES_DIRECTORY:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = PATH_SAVESTATES_DIR_CHOICE;
				set_initial_dir_tmpbrowser = true;
				set_delay = DELAY_LONG;
			}

			if(CTRL_START(state))
				strcpy(g_console.default_savestate_dir, usrDirPath);

			break;
		case SETTING_PATH_SRAM_DIRECTORY:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) ||  CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = PATH_SRAM_DIR_CHOICE;
				set_initial_dir_tmpbrowser = true;
				set_delay = DELAY_LONG;
			}

			if(CTRL_START(state))
				strcpy(g_console.default_sram_dir, "");
			break;
		case SETTING_PATH_CHEATS:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) ||  CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = PATH_CHEATS_DIR_CHOICE;
				set_initial_dir_tmpbrowser = true;
				set_delay = DELAY_LONG;
			}

			if(CTRL_START(state))
				strcpy(g_settings.cheat_database, usrDirPath);
			break;
		case SETTING_ENABLE_SRAM_PATH:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
			{
				g_console.default_sram_dir_enable = !g_console.default_sram_dir_enable;
				menu_reinit_settings();
				set_delay = DELAY_MEDIUM;
			}
			if(CTRL_START(state))
			{
				g_console.default_sram_dir_enable = true;
				menu_reinit_settings();
			}
			break;
		case SETTING_ENABLE_STATE_PATH:
			if(CTRL_LEFT(state)  || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
			{
				g_console.default_savestate_dir_enable = !g_console.default_savestate_dir_enable;
				menu_reinit_settings();
				set_delay = DELAY_MEDIUM;
			}
			if(CTRL_START(state))
			{
				g_console.default_savestate_dir_enable = true;
				menu_reinit_settings();
			}
			break;
		case SETTING_PATH_DEFAULT_ALL:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) ||  CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state) || CTRL_START(state))
			{
				strcpy(g_console.default_rom_startup_dir, "/");
				strcpy(g_console.default_savestate_dir, usrDirPath);
				strcpy(g_settings.cheat_database, usrDirPath);
				strcpy(g_console.default_sram_dir, "");

				menu_reinit_settings();
			}
			break;
		case SETTING_CONTROLS_SCHEME:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state) | CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = INPUT_PRESET_CHOICE;
				set_initial_dir_tmpbrowser = true;
				set_delay = DELAY_LONG;
			}
			if(CTRL_START(state))
			{
				menu_reinit_settings();
			}
			break;
		case SETTING_CONTROLS_NUMBER:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state))
			{
				if(currently_selected_controller_menu != 0)
					currently_selected_controller_menu--;
				menu_reinit_settings();
				set_delay = DELAY_MEDIUM;
			}

			if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
			{
				if(currently_selected_controller_menu < 6)
					currently_selected_controller_menu++;
				menu_reinit_settings();
				set_delay = DELAY_MEDIUM;
			}

			if(CTRL_START(state))
				currently_selected_controller_menu = 0;
			break; 
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_UP:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_UP);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_DOWN:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_DOWN);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_LEFT:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_LEFT);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_RIGHT:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_RIGHT);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_A:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_A);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_B);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_X:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_X);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_Y:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_Y);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_SELECT:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_SELECT);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_START:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_START);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_L);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_R);
			break;
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L2:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_L2);
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R2:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_R2);
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L3:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_L3);
		case SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R3:
			set_keybind_digital(state, RETRO_DEVICE_ID_JOYPAD_R3);
			break;
		case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) ||  CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state) || CTRL_START(state))
			{
				rarch_filename_input_and_save(INPUT_PRESET_FILE);
			}
			break;
		case SETTING_CONTROLS_DEFAULT_ALL:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_RIGHT(state) ||  CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state) || CTRL_START(state))
			{
				rarch_input_set_default_keybinds(currently_selected_controller_menu);
				menu_reinit_settings();
			}
			break;
	}

	set_setting_label(menu_obj, switchvalue);
}

static void select_setting(menu * menu_obj)
{
   uint64_t state, diff_state, button_was_pressed, i;

   state = cell_pad_input_poll_device(0);
   diff_state = old_state ^ state;
   button_was_pressed = old_state & diff_state;

   if(IS_TIMER_EXPIRED(g_console.control_timer_expiration_frame_count))
   {
      set_delay = DELAY_NONE;
      /* back to ROM menu if CIRCLE is pressed */
      if (CTRL_L1(button_was_pressed) || CTRL_CIRCLE(button_was_pressed))
         menuStackindex--;

      if (CTRL_R1(button_was_pressed))
      {
         switch(menu_obj->enum_id)
	 {
            case GENERAL_VIDEO_MENU:
               menuStackindex++;
	       menuStack[menuStackindex] = menu_generalaudiosettings;
	       break;
	    case GENERAL_AUDIO_MENU:
	       menuStackindex++;
	       menuStack[menuStackindex] = menu_emu_settings;
	       break;
	    case EMU_GENERAL_MENU:
	       menuStackindex++;
	       menuStack[menuStackindex] = menu_emu_videosettings;
	       break;
	    case EMU_VIDEO_MENU:
	       menuStackindex++;
	       menuStack[menuStackindex] = menu_emu_audiosettings;
	       break;
	    case EMU_AUDIO_MENU:
	       menuStackindex++;
	       menuStack[menuStackindex] = menu_pathsettings;
	       break;
	    case PATH_MENU:
	       menuStackindex++;
	       menuStack[menuStackindex] = menu_controlssettings;
	       break;
	    case CONTROLS_MENU:
	       break;
	 }
      }

      /* down to next setting */

      if (CTRL_DOWN(state) || CTRL_LSTICK_DOWN(state))
      {
         menu_obj->selected++;

	 if (menu_obj->selected >= menu_obj->max_settings)
            menu_obj->selected = menu_obj->first_setting; 

	 if (menu_obj->items[menu_obj->selected].page != menu_obj->page)
            menu_obj->page = menu_obj->items[menu_obj->selected].page;

	 set_delay = DELAY_MEDIUM;
      }

      /* up to previous setting */

      if (CTRL_UP(state) || CTRL_LSTICK_UP(state))
      {
         if (menu_obj->selected == menu_obj->first_setting)
            menu_obj->selected = menu_obj->max_settings-1;
	 else
            menu_obj->selected--;

	 if (menu_obj->items[menu_obj->selected].page != menu_obj->page)
            menu_obj->page = menu_obj->items[menu_obj->selected].page;

	 set_delay = DELAY_MEDIUM;
      }

      producesettingentry(menu_obj, menu_obj->selected);
   }

   display_menubar(menu_obj->enum_id);
   cellDbgFontDraw();

   for ( i = menu_obj->first_setting; i < menu_obj->max_settings; i++)
   {
      if(menu_obj->items[i].page == menu_obj->page)
      {
         cellDbgFontPuts(menu_obj->items[i].text_xpos, menu_obj->items[i].text_ypos, FONT_SIZE, menu_obj->selected == menu_obj->items[i].enum_id ? YELLOW : menu_obj->items[i].item_color, menu_obj->items[i].text);
	 cellDbgFontPuts(0.5f, menu_obj->items[i].text_ypos, FONT_SIZE, menu_obj->items[i].text_color, menu_obj->items[i].setting_text);
	 cellDbgFontDraw();
      }
   }

   cellDbgFontPuts(0.09f, menu_obj->items[menu_obj->selected].comment_ypos, 0.86f, LIGHTBLUE, menu_obj->items[menu_obj->selected].comment);

   cellDbgFontPuts(0.09f, 0.91f, FONT_SIZE, YELLOW, "UP/DOWN - select  L3+R3 - resume game   X/LEFT/RIGHT - change");
   cellDbgFontPuts(0.09f, 0.95f, FONT_SIZE, YELLOW, "START - default   L1/CIRCLE - go back   R1 - go forward");
   cellDbgFontDraw();
   old_state = state;
}

static void select_rom(void)
{
   char newpath[1024], *separatorslash;
   uint64_t state, diff_state, button_was_pressed;

   state = cell_pad_input_poll_device(0);
   diff_state = old_state ^ state;
   button_was_pressed = old_state & diff_state;

   browser_update(&browser);

   if(IS_TIMER_EXPIRED(g_console.control_timer_expiration_frame_count))
   {
      if (CTRL_SELECT(button_was_pressed))
      {
         menuStackindex++;
	 menuStack[menuStackindex] = menu_generalvideosettings;
      }

      if (CTRL_START(button_was_pressed))
         filebrowser_reset_start_directory(&browser, "/", rarch_console_get_rom_ext());

      if (CTRL_CROSS(button_was_pressed))
      {
         if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(browser))
	 {
            /*if 'filename' is in fact '..' - then pop back directory  instead of adding '..' to filename path */

            if(browser.currently_selected == 0)
	    {
               filebrowser_pop_directory(&browser);
	    }
	    else
	    {
               separatorslash = (strcmp(FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser),"/") == 0) ? "" : "/";
	       snprintf(newpath, sizeof(newpath), "%s%s%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), separatorslash, FILEBROWSER_GET_CURRENT_FILENAME(browser));
	       filebrowser_push_directory(&browser, newpath, true);
	    }
	 }
	 else if (FILEBROWSER_IS_CURRENT_A_FILE(browser))
	 {
            char rom_path_temp[MAX_PATH_LENGTH];
	    struct retro_system_info info;
	    retro_get_system_info(&info);
	    bool block_zip_extract  = info.block_extract;

	    snprintf(rom_path_temp, sizeof(rom_path_temp), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), FILEBROWSER_GET_CURRENT_FILENAME(browser));

	    if((strstr(rom_path_temp, ".zip") || strstr(rom_path_temp, ".ZIP")) && !block_zip_extract)
               rarch_extract_zipfile(rom_path_temp);
	    else
	    {
               g_console.menu_enable = false;
	       snprintf(g_console.rom_path, sizeof(g_console.rom_path), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), FILEBROWSER_GET_CURRENT_FILENAME(browser));
	       g_console.initialize_rarch_enable = 1;
	       g_console.mode_switch = MODE_EMULATION;
	    }
	 }
      }
   }

   if (FILEBROWSER_IS_CURRENT_A_DIRECTORY(browser))
   {
      if(!strcmp(FILEBROWSER_GET_CURRENT_FILENAME(browser),"app_home") || !strcmp(FILEBROWSER_GET_CURRENT_FILENAME(browser),"host_root"))
         cellDbgFontPrintf(0.09f, 0.83f, 0.91f, RED, "WARNING - This path only works on DEX PS3 systems. Do not attempt to open\n this directory on CEX PS3 systems, or you might have to restart.");
      else if(!strcmp(FILEBROWSER_GET_CURRENT_FILENAME(browser),".."))
         cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to go back to the previous directory.");
      else
         cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to enter the directory.");
   }

   if (FILEBROWSER_IS_CURRENT_A_FILE(browser))
      cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to load the game. ");

   struct retro_system_info info;
   retro_get_system_info(&info);
   const char *id = info.library_name ? info.library_name : "Unknown";

   cellDbgFontPuts	(0.09f,	0.05f,	FONT_SIZE,	RED,	"FILE BROWSER");
   cellDbgFontPrintf (0.3f, 0.05f, 0.82f, WHITE, "Libretro core: %s (v%s)", id, info.library_version);
   cellDbgFontPrintf (0.09f, 0.09f, FONT_SIZE, YELLOW,
		   "PATH: %s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser));
   cellDbgFontPuts   (0.09f, 0.93f, FONT_SIZE, YELLOW,
		   "L3 + R3 - resume game           SELECT - Settings screen");
   cellDbgFontDraw();

   browser_render(&browser);
   old_state = state;
}

#define MENU_ITEM_SELECTED(index) (menuitem_colors[index])

static void return_to_game (void)
{
   g_console.frame_advance_enable = false;
   g_console.ingame_menu_item = 0;
   g_console.menu_enable = false;
   g_console.mode_switch = MODE_EMULATION;
}

static void ingame_menu(uint32_t menu_id)
{
   char comment[256], msg_temp[256];
   static uint32_t menuitem_colors[MENU_ITEM_LAST];
   uint64_t state, stuck_in_loop;
   static uint64_t blocking;

   float x_position = 0.3f;
   float font_size = 1.1f;
   float ypos = 0.19f;
   float ypos_increment = 0.04f;

   for(int i = 0; i < MENU_ITEM_LAST; i++)
      menuitem_colors[i] = GREEN;

   menuitem_colors[g_console.ingame_menu_item] = RED;

   gl_t * gl = g_gl;

   state = cell_pad_input_poll_device(0);
   stuck_in_loop = 1;
   blocking = 0;

   if(IS_TIMER_EXPIRED(g_console.control_timer_expiration_frame_count) && blocking == false)
   {
      set_delay = DELAY_NONE;

      if(CTRL_CIRCLE(state))
         return_to_game();

      switch(g_console.ingame_menu_item)
      {
         case MENU_ITEM_LOAD_STATE:
            if(CTRL_CROSS(state))
	    {
               rarch_load_state();
	       return_to_game();
	    }
	    if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
	    {
               rarch_state_slot_decrease();
	       set_delay = DELAY_LONG;
	       blocking = 0;
	    }
	    if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
	    {
               rarch_state_slot_increase();
	       set_delay = DELAY_LONG;
	       blocking = 0;
	    }

	    strcpy(comment, "Press LEFT or RIGHT to change the current save state slot.\nPress CROSS to load the state from the currently selected save state slot.");
	    break;
	 case MENU_ITEM_SAVE_STATE:
	    if(CTRL_CROSS(state))
	    {
               rarch_save_state();
	       return_to_game();
	    }
	    if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
	    {
               rarch_state_slot_decrease();
	       set_delay = DELAY_LONG;
	       blocking = 0;
	    }
	    if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
	    {
               rarch_state_slot_increase();
	       set_delay = DELAY_LONG;
	       blocking = 0;
	    }

	    strcpy(comment, "Press LEFT or RIGHT to change the current save state slot.\nPress CROSS to save the state to the currently selected save state slot.");
	    break;
	 case MENU_ITEM_KEEP_ASPECT_RATIO:
	    if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
	    {
               if(g_console.aspect_ratio_index > 0)
	       {
                  g_console.aspect_ratio_index--;
		  video_gl.set_aspect_ratio(NULL, g_console.aspect_ratio_index);
		  set_delay = DELAY_LONG;
	       }
	    }
	    if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
	    {
               g_console.aspect_ratio_index++;
               if(g_console.aspect_ratio_index < ASPECT_RATIO_END)
	       {
		  video_gl.set_aspect_ratio(NULL, g_console.aspect_ratio_index);
		  set_delay = DELAY_LONG;
	       }
               else
                  g_console.aspect_ratio_index = ASPECT_RATIO_END-1;
	    }
	    if(CTRL_START(state))
	    {
               g_console.aspect_ratio_index = ASPECT_RATIO_4_3;
	       video_gl.set_aspect_ratio(NULL, g_console.aspect_ratio_index);
	    }
	    strcpy(comment, "Press LEFT or RIGHT to change the [Aspect Ratio].\nPress START to reset back to default values.");
	    break;
	 case MENU_ITEM_OVERSCAN_AMOUNT:
	    if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state) || CTRL_LSTICK_LEFT(state))
	    {
               g_console.overscan_amount -= 0.01f;
	       g_console.overscan_enable = true;

	       if(g_console.overscan_amount == 0.00f)
                  g_console.overscan_enable = false;

	       ps3graphics_set_overscan(g_console.overscan_enable, g_console.overscan_amount, 1);
	       set_delay = DELAY_SMALLEST;
	    }
	    if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state) || CTRL_LSTICK_RIGHT(state))
	    {
               g_console.overscan_amount += 0.01f;
	       g_console.overscan_enable = true;
	       if(g_console.overscan_amount == 0.0f)
                  g_console.overscan_amount = false;

	       ps3graphics_set_overscan(g_console.overscan_enable, g_console.overscan_amount, 1);
	       set_delay = DELAY_SMALLEST;
	    }
	    if(CTRL_START(state))
	    {
               g_console.overscan_amount = 0.0f;
	       g_console.overscan_enable = false;
	       ps3graphics_set_overscan(g_console.overscan_enable, g_console.overscan_amount, 1);
	    }
	    strcpy(comment, "Press LEFT or RIGHT to change the [Overscan] settings.\nPress START to reset back to default values.");
	    break;
	 case MENU_ITEM_ORIENTATION:
	    if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(state) || CTRL_LSTICK_LEFT(state))
	    {
               if(g_console.screen_orientation > ORIENTATION_NORMAL)
	       {
                  g_console.screen_orientation--;
		  video_gl.set_rotation(NULL, g_console.screen_orientation);
		  set_delay = DELAY_LONG;
	       }
	    }

	    if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state) || CTRL_LSTICK_RIGHT(state))
	    {
               if((g_console.screen_orientation+1) < ORIENTATION_END)
	       {
                  g_console.screen_orientation++;
		  video_gl.set_rotation(NULL, g_console.screen_orientation);
		  set_delay = DELAY_LONG;
	       }
	    }

	    if(CTRL_START(state))
	    {
               g_console.screen_orientation = ORIENTATION_NORMAL;
	       video_gl.set_rotation(NULL, g_console.screen_orientation);
	    }
	    strcpy(comment, "Press LEFT or RIGHT to change the [Orientation] settings.\nPress START to reset back to default values.");
	    break;
	 case MENU_ITEM_SCALE_FACTOR:
	    if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
	    {
               if(g_console.fbo_enabled)
	       {
                  if((g_settings.video.fbo_scale_x > MIN_SCALING_FACTOR))
		  {
                     g_settings.video.fbo_scale_x -= 1.0f;
		     g_settings.video.fbo_scale_y -= 1.0f;
		     apply_scaling(FBO_REINIT);
		     set_delay = DELAY_LONG;
		  }
	       }
	    }
	    if(CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(state))
	    {
               if(g_console.fbo_enabled)
	       {
                  if((g_settings.video.fbo_scale_x < MAX_SCALING_FACTOR))
		  {
                     g_settings.video.fbo_scale_x += 1.0f;
		     g_settings.video.fbo_scale_y += 1.0f;
		     apply_scaling(FBO_REINIT);
		     set_delay = DELAY_LONG;
		  }
	       }
	    }
	    if(CTRL_START(state))
	    {
               g_settings.video.fbo_scale_x = 2.0f;
	       g_settings.video.fbo_scale_y = 2.0f;
	       apply_scaling(FBO_REINIT);
	    }
	    strcpy(comment, "Press LEFT or RIGHT to change the [Scaling] settings.\nPress START to reset back to default values.");
	    break;
	 case MENU_ITEM_FRAME_ADVANCE:
	    if(CTRL_CROSS(state) || CTRL_R2(state) || CTRL_L2(state))
	    {
               g_console.frame_advance_enable = true;
	       g_console.ingame_menu_item = MENU_ITEM_FRAME_ADVANCE;
	       g_console.menu_enable = false;
	       g_console.mode_switch = MODE_EMULATION;
	    }
	    strcpy(comment, "Press 'CROSS', 'L2' or 'R2' button to step one frame. Pressing the button\nrapidly will advance the frame more slowly.");
	    break;
	 case MENU_ITEM_RESIZE_MODE:
	    if(CTRL_CROSS(state))
	    {
               g_console.aspect_ratio_index = ASPECT_RATIO_CUSTOM;
	       video_gl.set_aspect_ratio(NULL, g_console.aspect_ratio_index);
	       while(stuck_in_loop && g_console.ingame_menu_enable)
	       {
                  state = cell_pad_input_poll_device(0);

		  if(CTRL_SQUARE(~state))
		  {
                     glClear(GL_COLOR_BUFFER_BIT);
		     glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		     glEnable(GL_BLEND);
		  }

		  rarch_render_cached_frame();

		  if(CTRL_SQUARE(~state))
		  {
                     gl_frame_menu();
		  }

		  if(CTRL_LSTICK_LEFT(state) || CTRL_LEFT(state))
			  g_console.custom_viewport_x -= 1;
		  else if (CTRL_LSTICK_RIGHT(state) || CTRL_RIGHT(state))
			  g_console.custom_viewport_x += 1;

		  if (CTRL_LSTICK_UP(state) || CTRL_UP(state))
			  g_console.custom_viewport_y += 1;
		  else if (CTRL_LSTICK_DOWN(state) || CTRL_DOWN(state)) 
			  g_console.custom_viewport_y -= 1;

		  if (CTRL_RSTICK_LEFT(state) || CTRL_L1(state))
			  g_console.custom_viewport_width -= 1;
		  else if (CTRL_RSTICK_RIGHT(state) || CTRL_R1(state))
			  g_console.custom_viewport_width += 1;

		  if (CTRL_RSTICK_UP(state) || CTRL_L2(state))
			  g_console.custom_viewport_height += 1;
		  else if (CTRL_RSTICK_DOWN(state) || CTRL_R2(state))
			  g_console.custom_viewport_height -= 1;

		  if (CTRL_TRIANGLE(state))
		  {
			  g_console.custom_viewport_x = 0;
			  g_console.custom_viewport_y = 0;
			  g_console.custom_viewport_width = gl->win_width;
			  g_console.custom_viewport_height = gl->win_height;
		  }
		  if(CTRL_CIRCLE(state))
		  {
			  set_delay = DELAY_MEDIUM;
			  stuck_in_loop = 0;
		  }

		  if(CTRL_SQUARE(~state))
		  {
                     struct retro_system_info info;
		     retro_get_system_info(&info);
		     const char *id = info.library_name ? info.library_name : "Unknown";

		     cellDbgFontPuts (0.09f, 0.05f, FONT_SIZE, RED, "QUICK MENU");
		     cellDbgFontPrintf (0.3f, 0.05f, 0.82f, WHITE, "Libretro core: %s", id);
		     cellDbgFontPrintf (0.9f, 0.09f, 0.82f, WHITE, "v%s", EMULATOR_VERSION);
		     cellDbgFontPrintf(x_position, 0.14f, 1.4f, WHITE, "Resize Mode");
		     cellDbgFontPrintf(x_position,	ypos, font_size, GREEN,	"Viewport X: #%d", g_console.custom_viewport_x);

		     cellDbgFontPrintf(x_position,	ypos+(ypos_increment*1), font_size, GREEN, "Viewport Y: #%d", g_console.custom_viewport_y);

		     cellDbgFontPrintf(x_position,	ypos+(ypos_increment*2), font_size, GREEN, "Viewport Width: #%d", g_console.custom_viewport_width);

		     cellDbgFontPrintf(x_position,	ypos+(ypos_increment*3), font_size, GREEN, "Viewport Height: #%d", g_console.custom_viewport_height);

		     cellDbgFontPrintf (0.09f,   0.40f, font_size, LIGHTBLUE, "CONTROLS:");

		     cellDbgFontPrintf (0.09f,   0.46f, font_size,  LIGHTBLUE, "LEFT or LSTICK UP");
		     cellDbgFontPrintf (0.5f,   0.46f, font_size, LIGHTBLUE, "- Decrease Viewport X");

		     cellDbgFontDraw();

		     cellDbgFontPrintf (0.09f,   0.48f,   font_size,      LIGHTBLUE,           "RIGHT or LSTICK RIGHT");
		     cellDbgFontPrintf (0.5f,   0.48f,   font_size,      LIGHTBLUE,           "- Increase Viewport X");

		     cellDbgFontPrintf (0.09f,   0.50f,   font_size,      LIGHTBLUE,           "UP or LSTICK UP");
		     cellDbgFontPrintf (0.5f,   0.50f,   font_size,      LIGHTBLUE,           "- Increase Viewport Y");

		     cellDbgFontDraw();

		     cellDbgFontPrintf (0.09f,   0.52f,   font_size,      LIGHTBLUE,           "DOWN or LSTICK DOWN");
		     cellDbgFontPrintf (0.5f,   0.52f,   font_size,      LIGHTBLUE,           "- Decrease Viewport Y");

		     cellDbgFontPrintf (0.09f,   0.54f,   font_size,      LIGHTBLUE,           "L1 or RSTICK LEFT");
		     cellDbgFontPrintf (0.5f,   0.54f,   font_size,      LIGHTBLUE,           "- Decrease Viewport Width");

		     cellDbgFontDraw();

		     cellDbgFontPrintf (0.09f,   0.56f,   font_size,      LIGHTBLUE,           "R1 or RSTICK RIGHT");
		     cellDbgFontPrintf (0.5f,   0.56f,   font_size,      LIGHTBLUE,           "- Increase Viewport Width");

		     cellDbgFontPrintf (0.09f,   0.58f,   font_size,      LIGHTBLUE,           "L2 or  RSTICK UP");
		     cellDbgFontPrintf (0.5f,   0.58f,   font_size,      LIGHTBLUE,           "- Increase Viewport Height");

		     cellDbgFontDraw();

		     cellDbgFontPrintf (0.09f,   0.60f,   font_size,      LIGHTBLUE,           "R2 or RSTICK DOWN");
		     cellDbgFontPrintf (0.5f,   0.60f,   font_size,      LIGHTBLUE,           "- Decrease Viewport Height");

		     cellDbgFontPrintf (0.09f,   0.66f,   font_size,      LIGHTBLUE,           "TRIANGLE");
		     cellDbgFontPrintf (0.5f,   0.66f,   font_size,      LIGHTBLUE,           "- Reset To Defaults");

		     cellDbgFontPrintf (0.09f,   0.68f,   font_size,      LIGHTBLUE,           "SQUARE");
		     cellDbgFontPrintf (0.5f,   0.68f,   font_size,      LIGHTBLUE,           "- Show Game Screen");

		     cellDbgFontPrintf (0.09f,   0.70f,   font_size,      LIGHTBLUE,           "CIRCLE");
		     cellDbgFontPrintf (0.5f,   0.70f,   font_size,      LIGHTBLUE,           "- Return to Ingame Menu");

		     cellDbgFontDraw();

		     cellDbgFontPrintf (0.09f, 0.83f, 0.91f, LIGHTBLUE, "Allows you to resize the screen by moving around the two analog sticks.\nPress TRIANGLE to reset to default values, and CIRCLE to go back to the menu.");
		     cellDbgFontDraw();
		  }
		  video_gl.swap(NULL);
		  if(CTRL_SQUARE(~state))
		  {
                     glDisable(GL_BLEND);
		  }
	       }
	    }
	    strcpy(comment, "Allows you to resize the screen by moving around the two analog sticks.\nPress TRIANGLE to reset to default values, and CIRCLE to go back.");
	    break;
	 case MENU_ITEM_SCREENSHOT_MODE:
	    if(CTRL_CROSS(state))
	    {
               while(stuck_in_loop && g_console.ingame_menu_enable)
	       {
                  state = cell_pad_input_poll_device(0);
		  if(CTRL_CIRCLE(state))
		  {
                     set_delay = DELAY_MEDIUM;
		     stuck_in_loop = 0;
		  }

		  rarch_render_cached_frame();

		  video_gl.swap(NULL);
	       }
	    }

	    strcpy(comment, "Allows you to take a screenshot without any text clutter.\nPress CIRCLE to go back to the in-game menu while in 'Screenshot Mode'.");
	    break;
	 case MENU_ITEM_RETURN_TO_GAME:
	    if(CTRL_CROSS(state))
               return_to_game();

	    strcpy(comment, "Press 'CROSS' to return back to the game.");
	    break;
	 case MENU_ITEM_RESET:
	    if(CTRL_CROSS(state))
	    {
               return_to_game();
	       rarch_game_reset();
	    }
	    strcpy(comment, "Press 'CROSS' to reset the game.");
	    break;
	 case MENU_ITEM_RETURN_TO_MENU:
	    if(CTRL_CROSS(state))
	    {
               g_console.menu_enable = false;
	       g_console.ingame_menu_item = 0;
	       g_console.mode_switch = MODE_MENU;
	       set_delay = DELAY_LONG;
	    }
	    strcpy(comment, "Press 'CROSS' to return to the ROM Browser menu.");
	    break;
	 case MENU_ITEM_CHANGE_LIBRETRO:
	    if(CTRL_CROSS(state))
	    {
               menuStackindex++;
	       menuStack[menuStackindex] = menu_filebrowser;
	       menuStack[menuStackindex].enum_id = LIBRETRO_CHOICE;
	       set_initial_dir_tmpbrowser = true;
	       set_delay = DELAY_LONG;
	    }
	    strcpy(comment, "Press 'CROSS' to choose a different emulator core.");
	    break;
	 case MENU_ITEM_RETURN_TO_MULTIMAN:
	    if(CTRL_CROSS(state) && path_file_exists(MULTIMAN_EXECUTABLE))
	    {
               strlcpy(g_console.launch_app_on_exit, MULTIMAN_EXECUTABLE,
                  sizeof(g_console.launch_app_on_exit));

	       g_console.return_to_launcher = true;
	       g_console.menu_enable = false;
	       g_console.mode_switch = MODE_EXIT;
	    }
	    strcpy(comment, "Press 'CROSS' to quit the emulator and return to multiMAN.");
	    break;
	 case MENU_ITEM_RETURN_TO_XMB:
	    if(CTRL_CROSS(state))
	    {
               g_console.menu_enable = false;
	       g_console.mode_switch = MODE_EXIT;
	    }

	    strcpy(comment, "Press 'CROSS' to quit the emulator and return to the XMB.");
	    break;
      }

      if(CTRL_UP(state) || CTRL_LSTICK_UP(state))
      {
         if(g_console.ingame_menu_item > 0)
	 {
            g_console.ingame_menu_item--;
	    set_delay = DELAY_MEDIUM;
	 }
      }

      if(CTRL_DOWN(state) || CTRL_LSTICK_DOWN(state))
      {
         if(g_console.ingame_menu_item < (MENU_ITEM_LAST-1))
	 {
            g_console.ingame_menu_item++;
	    set_delay = DELAY_MEDIUM;
	 }
      }

      if(CTRL_L3(state) && CTRL_R3(state))
      {
         return_to_game();
      }
   }


   switch(g_console.screen_orientation)
   {
      case ORIENTATION_NORMAL:
         snprintf(msg_temp, sizeof(msg_temp), "Normal");
	 break;
      case ORIENTATION_VERTICAL:
	 snprintf(msg_temp, sizeof(msg_temp), "Vertical");
	 break;
      case ORIENTATION_FLIPPED:
	 snprintf(msg_temp, sizeof(msg_temp), "Flipped");
	 break;
      case ORIENTATION_FLIPPED_ROTATED:
	 snprintf(msg_temp, sizeof(msg_temp), "Flipped Rotated");
	 break;
   }

   cellDbgFontPrintf(x_position, 0.14f, 1.4f, WHITE, "Quick Menu");

   cellDbgFontPrintf(x_position, ypos, font_size, MENU_ITEM_SELECTED(MENU_ITEM_LOAD_STATE), "Load State #%d", g_extern.state_slot);

   cellDbgFontPrintf(x_position, ypos+(ypos_increment*MENU_ITEM_SAVE_STATE), font_size, MENU_ITEM_SELECTED(MENU_ITEM_SAVE_STATE), "Save State #%d", g_extern.state_slot);
   cellDbgFontDraw();

   cellDbgFontPrintf(x_position, (ypos+(ypos_increment*MENU_ITEM_KEEP_ASPECT_RATIO)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_KEEP_ASPECT_RATIO), "Aspect Ratio: %s", aspectratio_lut[g_console.aspect_ratio_index].name);

   cellDbgFontPrintf(x_position, (ypos+(ypos_increment*MENU_ITEM_OVERSCAN_AMOUNT)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_OVERSCAN_AMOUNT), "Overscan: %f", g_console.overscan_amount);

   cellDbgFontPrintf (x_position, (ypos+(ypos_increment*MENU_ITEM_ORIENTATION)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_ORIENTATION), "Orientation: %s", msg_temp);
   cellDbgFontDraw();

   cellDbgFontPrintf (x_position, (ypos+(ypos_increment*MENU_ITEM_SCALE_FACTOR)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_SCALE_FACTOR), "Scale Factor: %d", (int)(g_settings.video.fbo_scale_x));
   cellDbgFontDraw();

   cellDbgFontPrintf(x_position, (ypos+(ypos_increment*MENU_ITEM_RESIZE_MODE)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RESIZE_MODE), "Resize Mode");

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_FRAME_ADVANCE)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_FRAME_ADVANCE), "Frame Advance");

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_SCREENSHOT_MODE)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_SCREENSHOT_MODE), "Screenshot Mode");

   cellDbgFontDraw();

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RESET)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RESET), "Reset");

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_GAME)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_GAME), "Return to Game");
   cellDbgFontDraw();

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MENU)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_MENU), "Return to Menu");
   cellDbgFontDraw();

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_CHANGE_LIBRETRO)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_CHANGE_LIBRETRO), "Change libretro core");
   cellDbgFontDraw();

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_MULTIMAN)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_MULTIMAN), "Return to multiMAN");

   cellDbgFontPuts(x_position, (ypos+(ypos_increment*MENU_ITEM_RETURN_TO_XMB)), font_size, MENU_ITEM_SELECTED(MENU_ITEM_RETURN_TO_XMB), "Return to XMB");
   cellDbgFontDraw();

   struct retro_system_info info;
   retro_get_system_info(&info);
   const char *id = info.library_name ? info.library_name : "Unknown";

   cellDbgFontPuts(0.09f, 0.05f, FONT_SIZE, RED, "QUICK MENU");
   cellDbgFontPrintf (0.3f, 0.05f, 0.82f, WHITE, "Libretro core: %s", id);
   cellDbgFontPrintf (0.8f, 0.09f, 0.82f, WHITE, "v%s", EMULATOR_VERSION);
   cellDbgFontDraw();
   cellDbgFontPrintf (0.05f, 0.90f, 1.10f, WHITE, special_action_msg);
   cellDbgFontDraw();
   cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, comment);
   cellDbgFontDraw();

   old_state = state;
}

void menu_init (void)
{
   filebrowser_new(&browser, g_console.default_rom_startup_dir, rarch_console_get_rom_ext());
}

void menu_loop(void)
{
   menuStack[0] = menu_filebrowser;
   menuStack[0].enum_id = FILE_BROWSER_MENU;

   g_console.menu_enable = true;

   menu_reinit_settings();

   if(g_console.emulator_initialized)
	   video_gl.set_swap_block_state(NULL, true);

   if(g_console.ingame_menu_enable)
   {
      menuStackindex++;
      menuStack[menuStackindex] = menu_filebrowser;
      menuStack[menuStackindex].enum_id = INGAME_MENU;
   }

   do
   {
      glClear(GL_COLOR_BUFFER_BIT);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      glEnable(GL_BLEND);
      if(g_console.emulator_initialized)
      {
         rarch_render_cached_frame();
      }

      gl_frame_menu();

      switch(menuStack[menuStackindex].enum_id)
      {
         case FILE_BROWSER_MENU:
            select_rom();
	    break;
	 case GENERAL_VIDEO_MENU:
	 case GENERAL_AUDIO_MENU:
	 case EMU_GENERAL_MENU:
	 case EMU_VIDEO_MENU:
	 case EMU_AUDIO_MENU:
	 case PATH_MENU:
	 case CONTROLS_MENU:
	    select_setting(&menuStack[menuStackindex]);
	    break;
	 case SHADER_CHOICE:
	 case PRESET_CHOICE:
	 case BORDER_CHOICE:
	 case LIBRETRO_CHOICE:
	 case INPUT_PRESET_CHOICE:
	    select_file(menuStack[menuStackindex].enum_id);
	    break;
	 case PATH_SAVESTATES_DIR_CHOICE:
	 case PATH_DEFAULT_ROM_DIR_CHOICE:
	 case PATH_CHEATS_DIR_CHOICE:
	 case PATH_SRAM_DIR_CHOICE:
	    select_directory(menuStack[menuStackindex].enum_id);
	    break;
	 case INGAME_MENU:
	    if(g_console.ingame_menu_enable)
		    ingame_menu(menuStack[menuStackindex].enum_id);
	    break;
      }


      if(IS_TIMER_EXPIRED(g_console.control_timer_expiration_frame_count))
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

	 //set new timer delay after previous one has expired
	 if(set_delay != DELAY_NONE)
            set_delay_speed(set_delay);
      }

      // set a timer delay so that we don't instantly switch back to the menu when
      // press and holding L3 + R3 in the emulation loop (lasts for 30 frame ticks)
      if(g_console.mode_switch == MODE_EMULATION && !g_console.frame_advance_enable)
      {
         SET_TIMER_EXPIRATION(g_console.timer_expiration_frame_count, 30);
      }

      video_gl.swap(NULL);
      glDisable(GL_BLEND);
   }while (g_console.menu_enable);

   if(g_console.ingame_menu_enable)
      menuStackindex--;		// pop ingame menu from stack

   if(g_console.emulator_initialized)
      video_gl.set_swap_block_state(NULL, false);

   g_console.ingame_menu_enable = false;
}
