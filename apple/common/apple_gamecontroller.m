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

#include <Availability.h>
#include "RetroArch_Apple.h"
#import <GameController/GameController.h>
#include "../../input/apple_input.h"

static BOOL apple_gamecontroller_available(void)
{
    if (IOS_IS_VERSION_6_OR_LOWER())
        return false;
    /* by checking for extern symbols defined by the framework, we can check for its
     * existence at runtime. This is the Apple endorsed way of dealing with this */
    return (&GCControllerDidConnectNotification && &GCControllerDidDisconnectNotification);
}

static void apple_gamecontroller_poll(GCController *controller)
{
    if (!controller || controller.playerIndex == MAX_PLAYERS)
        return;
    
    uint32_t slot = (uint32_t)controller.playerIndex;
    
    /* retain the start (pause) value */
    uint32_t pause = g_current_input_data.pad_buttons[slot] & (1 << RETRO_DEVICE_ID_JOYPAD_START);
    
    g_current_input_data.pad_buttons[slot] = 0;
    memset(g_current_input_data.pad_axis[slot], 0, sizeof(g_current_input_data.pad_axis[0]));
    
    g_current_input_data.pad_buttons[slot] |= pause;
    
    if (controller.extendedGamepad)
    {
        GCExtendedGamepad *gp = (GCExtendedGamepad *)controller.extendedGamepad;
        g_current_input_data.pad_buttons[slot] |= gp.dpad.up.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.dpad.down.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.dpad.left.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.dpad.right.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.buttonA.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_B) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.buttonB.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_A) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.buttonX.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.buttonY.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_X) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.leftShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_L) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.rightShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_R) : 0;
        
        g_current_input_data.pad_buttons[slot] |= gp.leftTrigger.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.rightTrigger.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
        g_current_input_data.pad_axis[slot][0] = gp.leftThumbstick.xAxis.value * 32767.0f;
        g_current_input_data.pad_axis[slot][1] = gp.leftThumbstick.yAxis.value * 32767.0f;
        g_current_input_data.pad_axis[slot][2] = gp.rightThumbstick.xAxis.value * 32767.0f;
        g_current_input_data.pad_axis[slot][3] = gp.rightThumbstick.yAxis.value * 32767.0f;
    }
    else if (controller.gamepad)
    {
        GCGamepad *gp = (GCGamepad *)controller.gamepad;
        g_current_input_data.pad_buttons[slot] |= gp.dpad.up.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.dpad.down.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.dpad.left.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.dpad.right.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.buttonA.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_B) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.buttonB.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_A) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.buttonX.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.buttonY.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_X) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.leftShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_L) : 0;
        g_current_input_data.pad_buttons[slot] |= gp.rightShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_R) : 0;
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
    gamepad.valueChangedHandler = ^(GCGamepad *updateGamepad, GCControllerElement *element) {
        apple_gamecontroller_poll(updateGamepad.controller);
    };
    
    gamepad.controller.controllerPausedHandler = ^(GCController *controller) {
        
        uint32_t slot = (uint32_t)controller.playerIndex;
        
        g_current_input_data.pad_buttons[slot] |= (1 << RETRO_DEVICE_ID_JOYPAD_START);
        
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            g_current_input_data.pad_buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_START);
        });
        
    };
    
}

static void apple_gamecontroller_connect(GCController *controller)
{
    int32_t slot = apple_joypad_connect_gcapi();
    
    controller.playerIndex = (slot >= 0 && slot < MAX_PLAYERS) ? slot : GCControllerPlayerIndexUnset;
    
    if (controller.playerIndex == GCControllerPlayerIndexUnset)
        return;
    
    apple_gamecontroller_register(controller.gamepad);
}

static void apple_gamecontroller_disconnect(GCController* controller)
{
    if (controller.playerIndex == GCControllerPlayerIndexUnset)
        return;
    
    apple_joypad_disconnect((uint32_t)controller.playerIndex);
}

void apple_gamecontroller_init(void)
{
    if (!apple_gamecontroller_available())
        return;
    
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
}
