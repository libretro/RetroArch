/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#include <AvailabilityMacros.h>
#import <GameController/GameController.h>
#include <boolean.h>
#include "mfi_hid.h"
#include "../../input/drivers/cocoa_input.h"

enum
{
   GCCONTROLLER_PLAYER_INDEX_UNSET = -1,
};

static bool apple_gamecontroller_available(void)
{
   int major, minor;
   get_ios_version(&major, &minor);
    
   if (major <= 6)
      return false;
   /* by checking for extern symbols defined by the framework, we can check for its
    * existence at runtime. This is the Apple endorsed way of dealing with this */
#ifdef __IPHONE_7_0
   return (&GCControllerDidConnectNotification && &GCControllerDidDisconnectNotification);
#else
   return false;
#endif
}

static void apple_gamecontroller_poll(GCController *controller)
{
   uint32_t slot, pause;
   driver_t *driver = driver_get_ptr();
   cocoa_input_data_t *apple = (cocoa_input_data_t*)driver->input_data;
   if (!apple || !controller || controller.playerIndex == MAX_USERS)
      return;

   slot = (uint32_t)controller.playerIndex;
   /* retain the start (pause) value */
   pause = apple->buttons[slot] & (1 << RETRO_DEVICE_ID_JOYPAD_START);

   apple->buttons[slot] = 0;
   memset(apple->axes[slot], 0, sizeof(apple->axes[0]));

   apple->buttons[slot] |= pause;

   if (controller.extendedGamepad)
   {
      GCExtendedGamepad *gp = (GCExtendedGamepad *)controller.extendedGamepad;
      apple->buttons[slot] |= gp.dpad.up.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
      apple->buttons[slot] |= gp.dpad.down.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
      apple->buttons[slot] |= gp.dpad.left.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
      apple->buttons[slot] |= gp.dpad.right.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
      apple->buttons[slot] |= gp.buttonA.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_B) : 0;
      apple->buttons[slot] |= gp.buttonB.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_A) : 0;
      apple->buttons[slot] |= gp.buttonX.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
      apple->buttons[slot] |= gp.buttonY.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_X) : 0;
      apple->buttons[slot] |= gp.leftShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_L) : 0;
      apple->buttons[slot] |= gp.rightShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_R) : 0;
      apple->buttons[slot] |= gp.leftTrigger.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
      apple->buttons[slot] |= gp.rightTrigger.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
      apple->axes[slot][0] = gp.leftThumbstick.xAxis.value * 32767.0f;
      apple->axes[slot][1] = gp.leftThumbstick.yAxis.value * 32767.0f;
      apple->axes[slot][2] = gp.rightThumbstick.xAxis.value * 32767.0f;
      apple->axes[slot][3] = gp.rightThumbstick.yAxis.value * 32767.0f;
   }
   else if (controller.gamepad)
   {
      GCGamepad *gp = (GCGamepad *)controller.gamepad;
      apple->buttons[slot] |= gp.dpad.up.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
      apple->buttons[slot] |= gp.dpad.down.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
      apple->buttons[slot] |= gp.dpad.left.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
      apple->buttons[slot] |= gp.dpad.right.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
      apple->buttons[slot] |= gp.buttonA.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_B) : 0;
      apple->buttons[slot] |= gp.buttonB.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_A) : 0;
      apple->buttons[slot] |= gp.buttonX.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
      apple->buttons[slot] |= gp.buttonY.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_X) : 0;
      apple->buttons[slot] |= gp.leftShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_L) : 0;
      apple->buttons[slot] |= gp.rightShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_R) : 0;
   }
}

void apple_gamecontroller_poll_all(void)
{
   if (!apple_gamecontroller_available())
      return;

   for (GCController *controller in [GCController controllers])
      apple_gamecontroller_poll(controller);
}

static void apple_gamecontroller_register(GCGamepad *gamepad)
{
   driver_t *driver = driver_get_ptr();
   cocoa_input_data_t *apple = (cocoa_input_data_t*)driver->input_data;
   gamepad.valueChangedHandler = ^(GCGamepad *updateGamepad, GCControllerElement *element) {
      apple_gamecontroller_poll(updateGamepad.controller);
   };

   gamepad.controller.controllerPausedHandler = ^(GCController *controller) {

      uint32_t slot = (uint32_t)controller.playerIndex;

      apple->buttons[slot] |= (1 << RETRO_DEVICE_ID_JOYPAD_START);

      dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            apple->buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_START);
            });

   };

}

static int32_t apple_joypad_connect_gcapi(joypad_connection_t *joyconn)
{
   int pad = pad_connection_find_vacant_pad(joyconn);

   if (pad >= 0 && pad < MAX_USERS)
   {
      joypad_connection_t *s = (joypad_connection_t*)&joyconn[pad];

      if (s)
         s->connected = true;
   }

   return pad;
}

static void apple_gamecontroller_connect(GCController *controller)
{
   int32_t slot = apple_joypad_connect_gcapi(slots);

   controller.playerIndex = (slot >= 0 && slot < MAX_USERS) ? slot : GCCONTROLLER_PLAYER_INDEX_UNSET;

   if (controller.playerIndex == GCControllerPlayerIndexUnset)
      return;

   apple_gamecontroller_register(controller.gamepad);
}

static void apple_gamecontroller_disconnect(GCController* controller)
{
   unsigned pad = (uint32_t)controller.playerIndex;
   if (pad == GCCONTROLLER_PLAYER_INDEX_UNSET)
      return;

   pad_connection_pad_deinit(&slots[pad], pad);
}

void apple_gamecontroller_init(void)
{
   if (!apple_gamecontroller_available())
      return;
#ifdef __IPHONE_7_0
   [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification
                                                     object:nil
                                                      queue:[NSOperationQueue mainQueue]
                                                 usingBlock:^(NSNotification *note) {
                                                    apple_gamecontroller_connect([note object]);
                                                 }];

   [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification
                                                     object:nil
                                                      queue:[NSOperationQueue mainQueue]
                                                 usingBlock:^(NSNotification *note) {
                                                    apple_gamecontroller_disconnect([note object]);
                                                 } ];
#endif
}
