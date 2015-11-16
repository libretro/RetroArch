/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <boolean.h>

#include <AvailabilityMacros.h>
#import <GameController/GameController.h>

#include "mfi_hid.h"
#include "../drivers/cocoa_input.h"
#include "../connect/joypad_connection.h"

joypad_connection_t *slots;

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

   return true;
}

static void apple_gamecontroller_joypad_poll_internal(GCController *controller)
{
   uint32_t slot, pause;
   uint32_t *buttons;
   driver_t *driver          = driver_get_ptr();
   cocoa_input_data_t *apple = (cocoa_input_data_t*)driver->input_data;
   if (!apple || !controller)
      return;

   slot = (uint32_t)controller.playerIndex;
   /* retain the start (pause) value */
   pause = apple->mfi_buttons[slot] & (1 << RETRO_DEVICE_ID_JOYPAD_START);

   apple->mfi_buttons[slot] = 0;
   memset(apple->axes[slot], 0, sizeof(apple->axes[0]));

   apple->mfi_buttons[slot] |= pause;

   buttons = &apple->mfi_buttons[slot];

   if (controller.extendedGamepad)
   {
      GCExtendedGamepad *gp = (GCExtendedGamepad *)controller.extendedGamepad;

      *buttons |= gp.dpad.up.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
      *buttons |= gp.dpad.down.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
      *buttons |= gp.dpad.left.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
      *buttons |= gp.dpad.right.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
      *buttons |= gp.buttonA.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_B) : 0;
      *buttons |= gp.buttonB.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_A) : 0;
      *buttons |= gp.buttonX.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
      *buttons |= gp.buttonY.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_X) : 0;
      *buttons |= gp.leftShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_L) : 0;
      *buttons |= gp.rightShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_R) : 0;
      *buttons |= gp.leftTrigger.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
      *buttons |= gp.rightTrigger.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
      apple->axes[slot][0] = gp.leftThumbstick.xAxis.value * 32767.0f;
      apple->axes[slot][1] = gp.leftThumbstick.yAxis.value * 32767.0f;
      apple->axes[slot][2] = gp.rightThumbstick.xAxis.value * 32767.0f;
      apple->axes[slot][3] = gp.rightThumbstick.yAxis.value * 32767.0f;
      apple->axes[slot][4] = gp.rightThumbstick.yAxis.value * 32767.0f;
      apple->axes[slot][5] = gp.rightThumbstick.yAxis.value * 32767.0f;

   }
   else if (controller.gamepad)
   {
      GCGamepad *gp = (GCGamepad *)controller.gamepad;

      *buttons |= gp.dpad.up.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
      *buttons |= gp.dpad.down.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
      *buttons |= gp.dpad.left.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
      *buttons |= gp.dpad.right.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
      *buttons |= gp.buttonA.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_B) : 0;
      *buttons |= gp.buttonB.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_A) : 0;
      *buttons |= gp.buttonX.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
      *buttons |= gp.buttonY.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_X) : 0;
      *buttons |= gp.leftShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_L) : 0;
      *buttons |= gp.rightShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_R) : 0;
   }
}

static void apple_gamecontroller_joypad_poll(void)
{
   if (!apple_gamecontroller_available())
      return;

   for (GCController *controller in [GCController controllers])
      apple_gamecontroller_joypad_poll_internal(controller);
}

static void apple_gamecontroller_joypad_register(GCGamepad *gamepad)
{
   driver_t *driver = driver_get_ptr();
   cocoa_input_data_t *apple = (cocoa_input_data_t*)driver->input_data;
   gamepad.valueChangedHandler = ^(GCGamepad *updateGamepad, GCControllerElement *element) {
      apple_gamecontroller_poll(updateGamepad.controller);
   };

   gamepad.controller.controllerPausedHandler = ^(GCController *controller) {

      uint32_t slot = (uint32_t)controller.playerIndex;

      apple->mfi_buttons[slot] |= (1 << RETRO_DEVICE_ID_JOYPAD_START);

      dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            apple->mfi_buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_START);
            });

   };
}

static int32_t apple_gamecontroller_joypad_connect_gcapi(joypad_connection_t *joyconn)
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

static void apple_gamecontroller_joypad_connect(GCController *controller)
{
   int32_t slot = apple_gamecontroller_joypad_connect_gcapi(slots);

   controller.playerIndex = (slot >= 0 && slot < MAX_USERS) ? slot : GCCONTROLLER_PLAYER_INDEX_UNSET;

   if (controller.playerIndex == GCControllerPlayerIndexUnset)
      return;

   apple_gamecontroller_joypad_register(controller.gamepad);
}

static void apple_gamecontroller_joypad_disconnect(GCController* controller)
{
   unsigned pad = (uint32_t)controller.playerIndex;
   if (pad == GCCONTROLLER_PLAYER_INDEX_UNSET)
      return;

   pad_connection_pad_deinit(&slots[pad], pad);
}

static bool linuxraw_joypad_init(void *data)
{
   if (!apple_gamecontroller_available())
      return false;

#ifdef __IPHONE_7_0
   [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification
                                                     object:nil
                                                      queue:[NSOperationQueue mainQueue]
                                                 usingBlock:^(NSNotification *note) {
                                                    apple_gamecontroller_joypad_connect([note object]);
                                                 }];

                                                      [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification
                                                                                                        object:nil
                                                                                                         queue:[NSOperationQueue mainQueue]
                                                                                                    usingBlock:^(NSNotification *note) {
                                                                                                       apple_gamecontroller_joypad_disconnect([note object]);
                                                                                                    } ];
#endif

                                                                                                         return true;
}

static void linuxraw_joypad_destroy(void)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (linuxraw_pads[i].fd >= 0)
         close(linuxraw_pads[i].fd);
   }

   memset(linuxraw_pads, 0, sizeof(linuxraw_pads));

   for (i = 0; i < MAX_USERS; i++)
      linuxraw_pads[i].fd = -1;

   if (g_notify >= 0)
      close(g_notify);
   g_notify = -1;

   if (g_epoll >= 0)
      close(g_epoll);
   g_epoll = -1;

   g_hotplug = false;
}

static bool linuxraw_joypad_button(unsigned port, uint16_t joykey)
{
   const struct linuxraw_joypad *pad = (const struct linuxraw_joypad*)&linuxraw_pads[port];
   if (!pad)
      return false;
   return joykey < NUM_BUTTONS && BIT64_GET(pad->buttons, joykey);
}

static uint64_t linuxraw_joypad_get_buttons(unsigned port)
{
   const struct linuxraw_joypad *pad = (const struct linuxraw_joypad*)&linuxraw_pads[port];
   if (!pad)
      return 0;
   return pad->buttons;
}

static int16_t linuxraw_joypad_axis(unsigned port, uint32_t joyaxis)
{
   int16_t val = 0;
   const struct linuxraw_joypad *pad = NULL;

   if (joyaxis == AXIS_NONE)
      return 0;

   pad = (const struct linuxraw_joypad*)&linuxraw_pads[port];

   if (AXIS_NEG_GET(joyaxis) < NUM_AXES)
   {
      val = pad->axes[AXIS_NEG_GET(joyaxis)];
      if (val > 0)
         val = 0;
      /* Kernel returns values in range [-0x7fff, 0x7fff]. */
   }
   else if (AXIS_POS_GET(joyaxis) < NUM_AXES)
   {
      val = pad->axes[AXIS_POS_GET(joyaxis)];
      if (val < 0)
         val = 0;
   }

   return val;
}

static bool linuxraw_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS && linuxraw_pads[pad].fd >= 0;
}

static const char *linuxraw_joypad_name(unsigned pad)
{
   if (pad >= MAX_USERS)
      return NULL;

   return *linuxraw_pads[pad].ident ? linuxraw_pads[pad].ident : NULL;
}

input_device_driver_t linuxraw_mfi = {
   linuxraw_joypad_init,
   linuxraw_joypad_query_pad,
   linuxraw_joypad_destroy,
   linuxraw_joypad_button,
   linuxraw_joypad_get_buttons,
   linuxraw_joypad_axis,
   apple_gamecontroller_joypad_poll,
   NULL,
   linuxraw_joypad_name,
   "mfi",
};
