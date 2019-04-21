/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <QtWidgets/QMessageBox>

#include "../../ui_companion_driver.h"

static enum ui_msg_window_response ui_msg_window_qt_response(ui_msg_window_state *state, QMessageBox::StandardButtons response)
{
	switch (response)
   {
      case QMessageBox::Ok:
         return UI_MSG_RESPONSE_OK;
      case QMessageBox::Cancel:
         return UI_MSG_RESPONSE_CANCEL;
      case QMessageBox::Yes:
         return UI_MSG_RESPONSE_YES;
      case QMessageBox::No:
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

static QFlags<QMessageBox::StandardButton> 
ui_msg_window_qt_buttons(ui_msg_window_state *state)
{
	switch (state->buttons)
	{
	   case UI_MSG_WINDOW_OK:
		   return QMessageBox::Ok;
	   case UI_MSG_WINDOW_OKCANCEL:
		   return QMessageBox::Cancel;
	   case UI_MSG_WINDOW_YESNO:
		   return (QMessageBox::Yes | QMessageBox::No);
	   case UI_MSG_WINDOW_YESNOCANCEL:
		   return (QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
	}

	return QMessageBox::NoButton;
}

static enum ui_msg_window_response 
ui_msg_window_qt_error(ui_msg_window_state *state)
{
   QFlags<QMessageBox::StandardButton> flags = ui_msg_window_qt_buttons(state);
   return ui_msg_window_qt_response(state, QMessageBox::critical((QWidget*)state->window, state->title, state->text, flags));
}

static enum ui_msg_window_response ui_msg_window_qt_information(ui_msg_window_state *state)
{
   QFlags<QMessageBox::StandardButton> flags = ui_msg_window_qt_buttons(state);
   return ui_msg_window_qt_response(state, QMessageBox::information((QWidget*)state->window, state->title, state->text, flags));
}

static enum ui_msg_window_response ui_msg_window_qt_question(ui_msg_window_state *state)
{
   QFlags<QMessageBox::StandardButton> flags = ui_msg_window_qt_buttons(state);
   return ui_msg_window_qt_response(state, QMessageBox::question((QWidget*)state->window, state->title, state->text, flags));
}

static enum ui_msg_window_response ui_msg_window_qt_warning(ui_msg_window_state *state)
{
   QFlags<QMessageBox::StandardButton> flags = ui_msg_window_qt_buttons(state);
   return ui_msg_window_qt_response(state, QMessageBox::warning((QWidget*)state->window, state->title, state->text, flags));
}

ui_msg_window_t ui_msg_window_qt = {
   ui_msg_window_qt_error,
   ui_msg_window_qt_information,
   ui_msg_window_qt_question,
   ui_msg_window_qt_warning,
   "qt"
};
