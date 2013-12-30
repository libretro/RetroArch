/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#include <Availability.h>
#include "RetroArch_Apple.h"
#ifdef IOS
#include <UIKit/UIDevice.h>
#endif

#if defined(__IPHONE_7_0) && !defined(OSX)

#define IS_PRESSED(x) (x.value > .01f)

#import <GameController/GameController.h>
#include "apple_input.h"

#ifdef IOS

#endif

static void apple_gamecontroller_poll(GCController* controller)
{
#ifdef IOS
    if (!(IOS_IS_VERSION_7_OR_HIGHER()))
        return;
#endif
   if (!controller || controller.playerIndex == MAX_PLAYERS)
      return;
 
   uint32_t slot = controller.playerIndex;
   g_current_input_data.pad_buttons[slot] = 0;
   memset(g_current_input_data.pad_axis[slot], 0, sizeof(g_current_input_data.pad_axis[0]));
 
   if (controller.extendedGamepad)
   {
      GCExtendedGamepad* gp = controller.extendedGamepad;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.dpad.up) ? 1 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.dpad.down) ? 2 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.dpad.left) ? 4 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.dpad.right) ? 8 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.buttonA) ? 16 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.buttonB) ? 32 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.buttonX) ? 64 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.buttonY) ? 128 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.leftShoulder) ? 256 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.rightShoulder) ? 512 : 0;

      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.leftTrigger) ? 1024 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.rightTrigger) ? 2048 : 0;
      g_current_input_data.pad_axis[slot][0] = gp.leftThumbstick.xAxis.value * 32767.0f;
      g_current_input_data.pad_axis[slot][1] = gp.leftThumbstick.yAxis.value * 32767.0f;
      g_current_input_data.pad_axis[slot][2] = gp.rightThumbstick.xAxis.value * 32767.0f;
      g_current_input_data.pad_axis[slot][3] = gp.rightThumbstick.yAxis.value * 32767.0f;
   }
   else if (controller.gamepad)
   {
      GCGamepad* gp = controller.gamepad;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.dpad.up) ? 1 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.dpad.down) ? 2 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.dpad.left) ? 4 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.dpad.right) ? 8 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.buttonA) ? 16 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.buttonB) ? 32 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.buttonX) ? 64 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.buttonY) ? 128 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.leftShoulder) ? 256 : 0;
      g_current_input_data.pad_buttons[slot] |= IS_PRESSED(gp.rightShoulder) ? 512 : 0;
   }
}

void apple_gamecontroller_poll_all(void)
{
#ifdef IOS
    if (!(IOS_IS_VERSION_7_OR_HIGHER()))
        return;
#endif
   NSArray* controllers = [GCController controllers];
   
   for (int i = 0; i != [controllers count]; i ++)
      apple_gamecontroller_poll([controllers objectAtIndex:i]);
}

void apple_gamecontroller_connect(GCController* controller)
{
#ifdef IOS
    if (!(IOS_IS_VERSION_7_OR_HIGHER()))
        return;
#endif
   int32_t slot = apple_joypad_connect_gcapi();
   controller.playerIndex = (slot >= 0 && slot < MAX_PLAYERS) ? slot : GCControllerPlayerIndexUnset;
   
/*
   if (controller.playerIndex == GCControllerPlayerIndexUnset)
      return;
   else if (controller.extendedGamepad)
      controller.extendedGamepad.valueChangedHandler =
      ^(GCExtendedGamepad *gamepad, GCControllerElement *element) { apple_gamecontroller_poll(gamepad.controller); };
   else if (controller.gamepad)
      controller.gamepad.valueChangedHandler =
      ^(GCGamepad *gamepad, GCControllerElement *element) { apple_gamecontroller_poll(gamepad.controller); };
*/
}

void apple_gamecontroller_disconnect(GCController* controller)
{
#ifdef IOS
    if (!(IOS_IS_VERSION_7_OR_HIGHER()))
        return;
#endif
   if (controller.playerIndex == GCControllerPlayerIndexUnset)
      return;
   
   apple_joypad_disconnect(controller.playerIndex);
}

void apple_gamecontroller_init(void)
{
#ifdef IOS
    if (!(IOS_IS_VERSION_7_OR_HIGHER()))
        return;
#endif
    
    [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification object:nil queue:[NSOperationQueue mainQueue]
      usingBlock:^(NSNotification *note) { apple_gamecontroller_connect([note object]); } ];

   [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification object:nil queue:[NSOperationQueue mainQueue]
      usingBlock:^(NSNotification *note) { apple_gamecontroller_disconnect([note object]); } ];
}

#else

void apple_gamecontroller_init(void)
{
   
}

void apple_gamecontroller_poll_all(void)
{
   
}

#endif
