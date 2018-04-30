/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../../ui_companion_driver.h"

static enum ui_msg_window_response ui_msg_window_win32_response(ui_msg_window_state *state, UINT response)
{
	switch (response)
	{
	   case IDOK:
		return UI_MSG_RESPONSE_OK;
	   case IDCANCEL:
		return UI_MSG_RESPONSE_CANCEL;
	   case IDYES:
	    return UI_MSG_RESPONSE_YES;
	   case IDNO:
        return UI_MSG_RESPONSE_NO;
	   default:
		   break;
	}

	switch (state->buttons)
	{
	   case UI_MSG_WINDOW_OK:
		   return UI_MSG_RESPONSE_OK;
	   case UI_MSG_WINDOW_OKCANCEL:
		   return UI_MSG_RESPONSE_CANCEL;
	   case UI_MSG_WINDOW_YESNO:
		   return UI_MSG_RESPONSE_NO;
	   case UI_MSG_WINDOW_YESNOCANCEL:
		   return UI_MSG_RESPONSE_CANCEL;
	   default:
		   break;
	}

	return UI_MSG_RESPONSE_NA;
}

static UINT ui_msg_window_win32_buttons(ui_msg_window_state *state)
{
	switch (state->buttons)
	{
	   case UI_MSG_WINDOW_OK:
		   return MB_OK;
	   case UI_MSG_WINDOW_OKCANCEL:
		   return MB_OKCANCEL;
	   case UI_MSG_WINDOW_YESNO:
		   return MB_YESNO;
	   case UI_MSG_WINDOW_YESNOCANCEL:
		   return MB_YESNOCANCEL;
	}

	return 0;
}

static enum ui_msg_window_response ui_msg_window_win32_error(ui_msg_window_state *state)
{
   UINT flags = MB_ICONERROR | ui_msg_window_win32_buttons(state);
   return ui_msg_window_win32_response(state, MessageBoxA(NULL, (LPCSTR)state->text, (LPCSTR)state->title, flags));
}

static enum ui_msg_window_response ui_msg_window_win32_information(ui_msg_window_state *state)
{
   UINT flags = MB_ICONINFORMATION | ui_msg_window_win32_buttons(state);
   return ui_msg_window_win32_response(state, MessageBoxA(NULL, (LPCSTR)state->text, (LPCSTR)state->title, flags));
}

static enum ui_msg_window_response ui_msg_window_win32_question(ui_msg_window_state *state)
{
   UINT flags = MB_ICONQUESTION | ui_msg_window_win32_buttons(state);
   return ui_msg_window_win32_response(state, MessageBoxA(NULL, (LPCSTR)state->text, (LPCSTR)state->title, flags));
}

static enum ui_msg_window_response ui_msg_window_win32_warning(ui_msg_window_state *state)
{
   UINT flags = MB_ICONWARNING | ui_msg_window_win32_buttons(state);
   return ui_msg_window_win32_response(state, MessageBoxA(NULL, (LPCSTR)state->text, (LPCSTR)state->title, flags));
}

ui_msg_window_t ui_msg_window_win32 = {
   ui_msg_window_win32_error,
   ui_msg_window_win32_information,
   ui_msg_window_win32_question,
   ui_msg_window_win32_warning,
   "win32"
};
