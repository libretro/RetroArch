/*  -- Cellframework Mk.II -  Open framework to abstract the common tasks related to
 *                            PS3 application development.
 *
 *  Copyright (C) 2010-2012
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
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

#include "sysutil/sysutil_oskdialog.h"
#include "sys/memory.h"

#include "oskutil.h"

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
	// Initial display psition of the OSK (On-Screen Keyboard) dialog [x, y]
	params->dialogParam.controlPoint.x = 0.0;
	params->dialogParam.controlPoint.y = 0.0;

	// Set standard position
	int32_t LayoutMode = CELL_OSKDIALOG_LAYOUTMODE_X_ALIGN_CENTER | CELL_OSKDIALOG_LAYOUTMODE_Y_ALIGN_TOP;
	cellOskDialogSetLayoutMode(LayoutMode);

	//Select panels to be used using flags
	// NOTE: We don't need CELL_OSKDIALOG_PANELMODE_JAPANESE_KATAKANA and \
	// CELL_OSKDIALOG_PANELMODE_JAPANESE obviously (and Korean), so I'm \
	// going to leave that all out	
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
	//reset text output state before beginning new session
	params->text_can_be_fetched = false;

	if (params->flags & OSK_IN_USE)
		return (true);

	int ret = sys_memory_container_create(&params->containerid, params->osk_memorycontainer);

	if(ret < 0)
		return (false);

	//Length limitation for input text
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
	params->outputInfo.result = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK; 	// Result onscreen keyboard dialog termination
	params->outputInfo.numCharsResultString = 256;			  	// Specify number of characters for returned text
	params->outputInfo.pResultString = (uint16_t *)params->osk_text_buffer;	// Buffer storing returned text

	cellOskDialogUnloadAsync(&params->outputInfo);

	int num;
	switch (params->outputInfo.result)
	{
		case CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK:
			//The text we get from the outputInfo is Unicode, needs to be converted
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
