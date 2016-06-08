/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../../ui_companion_driver.h"

static enum ui_msg_window_response ui_msg_window_null_error(ui_msg_window_state *state)
{
   return UI_MSG_RESPONSE_CANCEL;
}

static enum ui_msg_window_response ui_msg_window_null_information(ui_msg_window_state *state)
{
   return UI_MSG_RESPONSE_CANCEL;
}

static enum ui_msg_window_response ui_msg_window_null_question(ui_msg_window_state *state)
{
   return UI_MSG_RESPONSE_CANCEL;
}

static enum ui_msg_window_response ui_msg_window_null_warning(ui_msg_window_state *state)
{
   return UI_MSG_RESPONSE_CANCEL;
}

const ui_msg_window_t ui_msg_window_null = {
   ui_msg_window_null_error,
   ui_msg_window_null_information,
   ui_msg_window_null_question,
   ui_msg_window_null_warning,
   "null"
};
