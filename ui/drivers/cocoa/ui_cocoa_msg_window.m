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

#include <string/stdstring.h>

#include "cocoa_defines.h"
#include "cocoa_common.h"

#if defined(HAVE_COCOA)
extern id apple_platform;
#endif

#include "../../ui_companion_driver.h"

static enum ui_msg_window_response ui_msg_window_cocoa_dialog(ui_msg_window_state *state, enum ui_msg_window_type type)
{
#if defined(HAVE_COCOA_METAL)
   NSModalResponse response;
   NSAlert *alert = [NSAlert new];
#elif defined(HAVE_COCOA)
   NSInteger response;
   NSAlert* alert = [[NSAlert new] autorelease];
#endif

   if (!string_is_empty(state->title))
      [alert setMessageText:BOXSTRING(state->title)];
   [alert setInformativeText:BOXSTRING(state->text)];

   switch (state->buttons)
   {
      case UI_MSG_WINDOW_OK:
         [alert addButtonWithTitle:BOXSTRING("OK")];
         break;
      case UI_MSG_WINDOW_YESNO:
         [alert addButtonWithTitle:BOXSTRING("Yes")];
         [alert addButtonWithTitle:BOXSTRING("No")];
         break;
      case UI_MSG_WINDOW_OKCANCEL:
         [alert addButtonWithTitle:BOXSTRING("OK")];
         [alert addButtonWithTitle:BOXSTRING("Cancel")];
         break;
      case UI_MSG_WINDOW_YESNOCANCEL:
         [alert addButtonWithTitle:BOXSTRING("Yes")];
         [alert addButtonWithTitle:BOXSTRING("No")];
         [alert addButtonWithTitle:BOXSTRING("Cancel")];
         break;
   }

   switch (type)
   {
      case UI_MSG_WINDOW_TYPE_ERROR:
         [alert setAlertStyle:NSAlertStyleCritical];
         break;
      case UI_MSG_WINDOW_TYPE_WARNING:
         [alert setAlertStyle:NSAlertStyleWarning];
         break;
      case UI_MSG_WINDOW_TYPE_QUESTION:
         [alert setAlertStyle:NSAlertStyleInformational];
         break;
      case UI_MSG_WINDOW_TYPE_INFORMATION:
         [alert setAlertStyle:NSAlertStyleInformational];
         break;
   }

#if defined(HAVE_COCOA_METAL)
   [alert beginSheetModalForWindow:(BRIDGE NSWindow *)ui_companion_driver_get_main_window()
                 completionHandler:^(NSModalResponse returnCode) {
                    [[NSApplication sharedApplication] stopModalWithCode:returnCode];
                 }];
   response = [alert runModal];
#elif defined(HAVE_COCOA)
    [alert beginSheetModalForWindow:ui_companion_driver_get_main_window()
                      modalDelegate:apple_platform
                     didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                        contextInfo:nil];
    response = [[NSApplication sharedApplication] runModalForWindow:[alert window]];
#endif

   switch (state->buttons)
   {
      case UI_MSG_WINDOW_OK:
         if (response == NSAlertFirstButtonReturn)
            return UI_MSG_RESPONSE_OK;
         break;
      case UI_MSG_WINDOW_OKCANCEL:
         if (response == NSAlertFirstButtonReturn)
            return UI_MSG_RESPONSE_OK;
         if (response == NSAlertSecondButtonReturn)
            return UI_MSG_RESPONSE_CANCEL;
         break;
      case UI_MSG_WINDOW_YESNO:
         if (response == NSAlertFirstButtonReturn)
            return UI_MSG_RESPONSE_YES;
         if (response == NSAlertSecondButtonReturn)
            return UI_MSG_RESPONSE_NO;
         break;
      case UI_MSG_WINDOW_YESNOCANCEL:
         if (response == NSAlertFirstButtonReturn)
            return UI_MSG_RESPONSE_YES;
         if (response == NSAlertSecondButtonReturn)
            return UI_MSG_RESPONSE_NO;
         if (response == NSAlertThirdButtonReturn)
            return UI_MSG_RESPONSE_CANCEL;
         break;
   }

   return UI_MSG_RESPONSE_NA;
}

static enum ui_msg_window_response ui_msg_window_cocoa_error(ui_msg_window_state *state)
{
   return ui_msg_window_cocoa_dialog(state, UI_MSG_WINDOW_TYPE_ERROR);
}

static enum ui_msg_window_response ui_msg_window_cocoa_information(ui_msg_window_state *state)
{
   return ui_msg_window_cocoa_dialog(state, UI_MSG_WINDOW_TYPE_INFORMATION);
}

static enum ui_msg_window_response ui_msg_window_cocoa_question(ui_msg_window_state *state)
{
   return ui_msg_window_cocoa_dialog(state, UI_MSG_WINDOW_TYPE_QUESTION);
}

static enum ui_msg_window_response ui_msg_window_cocoa_warning(ui_msg_window_state *state)
{
   return ui_msg_window_cocoa_dialog(state, UI_MSG_WINDOW_TYPE_WARNING);
}

ui_msg_window_t ui_msg_window_cocoa = {
   ui_msg_window_cocoa_error,
   ui_msg_window_cocoa_information,
   ui_msg_window_cocoa_question,
   ui_msg_window_cocoa_warning,
   "cocoa"
};
