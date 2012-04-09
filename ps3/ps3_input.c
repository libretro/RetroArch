/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdlib.h>

#include <cell/pad.h>
#include <sdk_version.h>
#include <sys/memory.h>
#include <sysutil/sysutil_oskdialog.h>
#include <sysutil/sysutil_common.h>

#include "ps3_input.h"
#include "ps3_video_psgl.h"
#include "../driver.h"
#include "../console/console_ext_input.h"
#include "../libretro.h"
#include "../general.h"
#include "shared.h"

/*============================================================
	PS3 PAD
============================================================ */

#define MAP(x) (x & 0xFF)

static uint64_t state[MAX_PADS];

void cell_pad_input_deinit(void)
{
   cellPadEnd();
}

uint32_t cell_pad_input_pads_connected(void)
{
#if(CELL_SDK_VERSION > 0x340000)
   CellPadInfo2 pad_info;
   cellPadGetInfo2(&pad_info);
#else
   CellPadInfo pad_info;
   cellPadGetInfo(&pad_info);
#endif
   return pad_info.now_connect;
}

uint64_t cell_pad_input_poll_device(uint32_t id)
{
   CellPadData pad_data;
   static uint64_t ret[MAX_PADS];

   // Get new pad data
   cellPadGetData(id, &pad_data);

   if (pad_data.len == 0)
      return ret[id];
   else
   {
      ret[id] = 0;

      // Build the return value.
      ret[id] |= (uint64_t)MAP(pad_data.button[LOWER_BUTTONS]);
      ret[id] |= (uint64_t)MAP(pad_data.button[HIGHER_BUTTONS]) << 8;
      ret[id] |= (uint64_t)MAP(pad_data.button[RSTICK_X]) << 32;
      ret[id] |= (uint64_t)MAP(pad_data.button[RSTICK_Y]) << 40;
      ret[id] |= (uint64_t)MAP(pad_data.button[LSTICK_X]) << 16;
      ret[id] |= (uint64_t)MAP(pad_data.button[LSTICK_Y]) << 24;

      ret[id] |= (uint64_t)(PRESSED_LEFT_LSTICK(ret[id])) << LSTICK_LEFT_SHIFT;
      ret[id] |= (uint64_t)(PRESSED_RIGHT_LSTICK(ret[id])) << LSTICK_RIGHT_SHIFT;
      ret[id] |= (uint64_t)(PRESSED_UP_LSTICK(ret[id])) << LSTICK_UP_SHIFT;
      ret[id] |= (uint64_t)(PRESSED_DOWN_LSTICK(ret[id])) << LSTICK_DOWN_SHIFT;

      ret[id] |= (uint64_t)(PRESSED_LEFT_RSTICK(ret[id])) << RSTICK_LEFT_SHIFT;
      ret[id] |= (uint64_t)(PRESSED_RIGHT_RSTICK(ret[id])) << RSTICK_RIGHT_SHIFT;
      ret[id] |= (uint64_t)(PRESSED_UP_RSTICK(ret[id])) << RSTICK_UP_SHIFT;
      ret[id] |= (uint64_t)(PRESSED_DOWN_RSTICK(ret[id])) << RSTICK_DOWN_SHIFT;
      return ret[id];
   }
}
static void ps3_input_poll(void *data)
{
   (void)data;
   for (unsigned i = 0; i < MAX_PADS; i++)
      state[i] = cell_pad_input_poll_device(i);
}

static int16_t ps3_input_state(void *data, const struct snes_keybind **binds,
      bool port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;

   unsigned pads_connected, player;
   uint64_t button;

   player = 0;
   pads_connected = cell_pad_input_pads_connected(); 

   if (device != RETRO_DEVICE_JOYPAD)
      return 0;

   if (port == 1)
   {
	   if(pads_connected < 2)
		   return 0;

	   player = 1;

	   if(device == RETRO_DEVICE_MULTITAP)
	      player += index;
   }

   button = binds[player][id].joykey;

   return CTRL_MASK(state[player], button) ? 1 : 0;
}

/*============================================================
	ON-SCREEN KEYBOARD UTILITY
============================================================ */

#define OSK_IN_USE	(0x00000001)

void oskutil_init(oskutil_params *params, unsigned int containersize)
{
   params->flags = 0;
   params->is_running = false;
   if(containersize)
      params->osk_memorycontainer =  containersize; 
   else
      params->osk_memorycontainer =  1024*1024*7;
}

static bool oskutil_enable_key_layout()
{
   int ret = cellOskDialogSetKeyLayoutOption(CELL_OSKDIALOG_10KEY_PANEL | \
      CELL_OSKDIALOG_FULLKEY_PANEL);
   if (ret < 0)
      return (false);
   else
      return (true);
}

static void oskutil_create_activation_parameters(oskutil_params *params)
{
   params->dialogParam.controlPoint.x = 0.0;
   params->dialogParam.controlPoint.y = 0.0;

   int32_t LayoutMode = CELL_OSKDIALOG_LAYOUTMODE_X_ALIGN_CENTER | CELL_OSKDIALOG_LAYOUTMODE_Y_ALIGN_TOP;
   cellOskDialogSetLayoutMode(LayoutMode);

   params->dialogParam.allowOskPanelFlg = 
   CELL_OSKDIALOG_PANELMODE_ALPHABET |
   CELL_OSKDIALOG_PANELMODE_NUMERAL | 
   CELL_OSKDIALOG_PANELMODE_NUMERAL_FULL_WIDTH |
   CELL_OSKDIALOG_PANELMODE_ENGLISH;

   params->dialogParam.firstViewPanel = CELL_OSKDIALOG_PANELMODE_ENGLISH;
   params->dialogParam.prohibitFlgs = 0;
}

void oskutil_write_message(oskutil_params *params, const wchar_t* msg)
{
   params->inputFieldInfo.message = (uint16_t*)msg;
}

void oskutil_write_initial_message(oskutil_params *params, const wchar_t* msg)
{
   params->inputFieldInfo.init_text = (uint16_t*)msg;
}

bool oskutil_start(oskutil_params *params) 
{
   memset(params->osk_text_buffer, 0, sizeof(*params->osk_text_buffer));
   memset(params->osk_text_buffer_char, 0, 256);

   params->text_can_be_fetched = false;

   if (params->flags & OSK_IN_USE)
      return (true);

   int ret = sys_memory_container_create(&params->containerid, params->osk_memorycontainer);

   if(ret < 0)
      return (false);

   params->inputFieldInfo.limit_length = CELL_OSKDIALOG_STRING_SIZE;	

   oskutil_create_activation_parameters(params);

   if(!oskutil_enable_key_layout())
      return (false);

   ret = cellOskDialogLoadAsync(params->containerid, &params->dialogParam, &params->inputFieldInfo);
   if(ret < 0)
      return (false);

   params->flags |= OSK_IN_USE;
   params->is_running = true;

   return (true);
}

void oskutil_close(oskutil_params *params)
{
   cellOskDialogAbort();
}

void oskutil_finished(oskutil_params *params)
{
   int num;

   params->outputInfo.result = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK;
   params->outputInfo.numCharsResultString = 256;
   params->outputInfo.pResultString = (uint16_t *)params->osk_text_buffer;

   cellOskDialogUnloadAsync(&params->outputInfo);

   switch (params->outputInfo.result)
   {
      case CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK:
         num = wcstombs(params->osk_text_buffer_char, params->osk_text_buffer, 256);
         params->osk_text_buffer_char[num]=0;
         params->text_can_be_fetched = true;
         break;
      case CELL_OSKDIALOG_INPUT_FIELD_RESULT_CANCELED:
      case CELL_OSKDIALOG_INPUT_FIELD_RESULT_ABORT:
      case CELL_OSKDIALOG_INPUT_FIELD_RESULT_NO_INPUT_TEXT:
      default:
         params->osk_text_buffer_char[0]=0;
         params->text_can_be_fetched = false;
         break;
   }

   params->flags &= ~OSK_IN_USE;
}

void oskutil_unload(oskutil_params *params)
{
   sys_memory_container_destroy(params->containerid);
   params->is_running = false;
}

/*============================================================
	SSNES PS3 INPUT DRIVER 
============================================================ */

static void ps3_free_input(void *data)
{
   (void)data;
}

static void* ps3_input_initialize(void)
{
   return (void*)-1;
}

void ps3_input_init(void)
{
   cellPadInit(MAX_PADS);
   for(unsigned i = 0; i < MAX_PADS; i++)
   	ps3_input_map_dpad_to_stick(g_settings.input.dpad_emulation[i], i);
}

void ps3_input_map_dpad_to_stick(uint32_t map_dpad_enum, uint32_t controller_id)
{
	switch(map_dpad_enum)
	{
		case DPAD_EMULATION_NONE:
			g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey		= ssnes_platform_keybind_lut[PS3_DEVICE_ID_JOYPAD_UP];
			g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_JOYPAD_DOWN];
			g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_JOYPAD_LEFT];
			g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_JOYPAD_RIGHT];
			break;
		case DPAD_EMULATION_LSTICK:
			g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey		= ssnes_platform_keybind_lut[PS3_DEVICE_ID_LSTICK_UP_DPAD];
			g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_LSTICK_DOWN_DPAD];
			g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_LSTICK_LEFT_DPAD];
			g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_LSTICK_RIGHT_DPAD];
			break;
		case DPAD_EMULATION_RSTICK:
			g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey		= ssnes_platform_keybind_lut[PS3_DEVICE_ID_RSTICK_UP_DPAD];
			g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_RSTICK_DOWN_DPAD];
			g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_RSTICK_LEFT_DPAD];
			g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_RSTICK_RIGHT_DPAD];
			break;
	}
}

static bool ps3_key_pressed(void *data, int key)
{
   (void)data;
   switch (key)
   {
      case SSNES_FAST_FORWARD_HOLD_KEY:
         return CTRL_RSTICK_DOWN(state[0]) && CTRL_R2(~state[0]);
      case SSNES_LOAD_STATE_KEY:
         return (CTRL_RSTICK_UP(state[0]) && CTRL_R2(state[0]));
      case SSNES_SAVE_STATE_KEY:
         return (CTRL_RSTICK_DOWN(state[0]) && CTRL_R2(state[0]));
      case SSNES_STATE_SLOT_PLUS:
         return (CTRL_RSTICK_RIGHT(state[0]) && CTRL_R2(state[0]));
      case SSNES_STATE_SLOT_MINUS:
         return (CTRL_RSTICK_LEFT(state[0]) && CTRL_R2(state[0]));
      case SSNES_FRAMEADVANCE:
      	if(g_console.frame_advance_enable)
	{
		g_console.menu_enable = true;
		g_console.ingame_menu_enable = true;
		g_console.mode_switch = MODE_MENU;
	}
	 return false;
      case SSNES_REWIND:
         return CTRL_RSTICK_UP(state[0]) && CTRL_R2(~state[0]);
      case SSNES_QUIT_KEY:
	 if(IS_TIMER_EXPIRED(g_console.timer_expiration_frame_count))
	 {
		 uint32_t r3_pressed = CTRL_R3(state[0]);
		 uint32_t l3_pressed = CTRL_L3(state[0]);
		 bool retval = false;
		 g_console.menu_enable = (r3_pressed && l3_pressed && IS_TIMER_EXPIRED(g_console.timer_expiration_frame_count));
		 g_console.ingame_menu_enable = r3_pressed && !l3_pressed;

		 if(g_console.menu_enable || (g_console.ingame_menu_enable && !g_console.menu_enable))
		 {
			 g_console.mode_switch = MODE_MENU;
			 SET_TIMER_EXPIRATION(g_console.control_timer_expiration_frame_count, 30);
			 retval = g_console.menu_enable;
		 }

		 retval = g_console.ingame_menu_enable ? g_console.ingame_menu_enable : g_console.menu_enable;
		 return retval;
	 }
      default:
         return false;
   }
}

const input_driver_t input_ps3 = {
   .init = ps3_input_initialize,
   .poll = ps3_input_poll,
   .input_state = ps3_input_state,
   .key_pressed = ps3_key_pressed,
   .free = ps3_free_input,
   .ident = "ps3",
};

