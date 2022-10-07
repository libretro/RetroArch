/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <boolean.h>

#include <AvailabilityMacros.h>

#include "../../tasks/tasks_internal.h"

#import <GameController/GameController.h>

#ifndef MAX_MFI_CONTROLLERS
#define MAX_MFI_CONTROLLERS 4
#endif

enum
{
    GCCONTROLLER_PLAYER_INDEX_UNSET = -1,
};

/* TODO/FIXME - static globals */
static uint32_t mfi_buttons[MAX_USERS];
static int16_t  mfi_axes[MAX_USERS][4];
static uint32_t mfi_controllers[MAX_MFI_CONTROLLERS];
static NSMutableArray *mfiControllers;

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
    uint32_t slot, pause, select, l3, r3;
    uint32_t *buttons;
    if (!controller)
        return;

    slot               = (uint32_t)controller.playerIndex;
    /* If we have not assigned a slot to this controller yet, ignore it. */
    if (slot >= MAX_USERS)
        return;
    buttons            = &mfi_buttons[slot];

    /* retain the values from the paused controller handler and pass them through */
    if (@available(iOS 13, *))
    {
        /* The menu button can be pressed/unpressed 
         * like any other button in iOS 13,
         * so no need to passthrough anything */
        *buttons = 0;
    }
    else
    {
        /* Use the paused controller handler for iOS versions below 13 */
        pause              = *buttons & (1 << RETRO_DEVICE_ID_JOYPAD_START);
        select             = *buttons & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT);
        l3                 = *buttons & (1 << RETRO_DEVICE_ID_JOYPAD_L3);
        r3                 = *buttons & (1 << RETRO_DEVICE_ID_JOYPAD_R3);
        *buttons           = 0 | pause | select | l3 | r3;
    }
    memset(mfi_axes[slot], 0, sizeof(mfi_axes[0]));

    if (controller.extendedGamepad)
    {
        GCExtendedGamepad *gp = (GCExtendedGamepad *)controller.extendedGamepad;

        *buttons             |= gp.dpad.up.pressed         ? (1 << RETRO_DEVICE_ID_JOYPAD_UP)    : 0;
        *buttons             |= gp.dpad.down.pressed       ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN)  : 0;
        *buttons             |= gp.dpad.left.pressed       ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)  : 0;
        *buttons             |= gp.dpad.right.pressed      ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
        *buttons             |= gp.buttonA.pressed         ? (1 << RETRO_DEVICE_ID_JOYPAD_B)     : 0;
        *buttons             |= gp.buttonB.pressed         ? (1 << RETRO_DEVICE_ID_JOYPAD_A)     : 0;
        *buttons             |= gp.buttonX.pressed         ? (1 << RETRO_DEVICE_ID_JOYPAD_Y)     : 0;
        *buttons             |= gp.buttonY.pressed         ? (1 << RETRO_DEVICE_ID_JOYPAD_X)     : 0;
        *buttons             |= gp.leftShoulder.pressed    ? (1 << RETRO_DEVICE_ID_JOYPAD_L)     : 0;
        *buttons             |= gp.rightShoulder.pressed   ? (1 << RETRO_DEVICE_ID_JOYPAD_R)     : 0;
        *buttons             |= gp.leftTrigger.pressed     ? (1 << RETRO_DEVICE_ID_JOYPAD_L2)    : 0;
        *buttons             |= gp.rightTrigger.pressed    ? (1 << RETRO_DEVICE_ID_JOYPAD_R2)    : 0;
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 120100 || __TV_OS_VERSION_MAX_ALLOWED >= 120100
        if (@available(iOS 12.1, *))
        {
            *buttons         |= gp.leftThumbstickButton.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
            *buttons         |= gp.rightThumbstickButton.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
        }
#endif

#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 130000 || __TV_OS_VERSION_MAX_ALLOWED >= 130000
        if (@available(iOS 13, *))
        {
            /* Support "Options" button present in PS4 / XBox One controllers */
            *buttons         |= gp.buttonOptions.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
            
            /* Support buttons that aren't supported by older mFi controller via "hotkey" combinations:
             *
             * LS + Menu => Select
             * LT + Menu => L3
             * RT + Menu => R3
	     */
            if (gp.buttonMenu.pressed )
            {
                if (gp.leftShoulder.pressed)
                    *buttons     |= 1 << RETRO_DEVICE_ID_JOYPAD_SELECT;
                else if (gp.leftTrigger.pressed)
                    *buttons     |= 1 << RETRO_DEVICE_ID_JOYPAD_L3;
                else if (gp.rightTrigger.pressed)
                    *buttons     |= 1 << RETRO_DEVICE_ID_JOYPAD_R3;
                else
                    *buttons     |= 1 << RETRO_DEVICE_ID_JOYPAD_START;
            }
        }
#endif

        mfi_axes[slot][0]     = gp.leftThumbstick.xAxis.value * 32767.0f;
        mfi_axes[slot][1]     = gp.leftThumbstick.yAxis.value * 32767.0f;
        mfi_axes[slot][2]     = gp.rightThumbstick.xAxis.value * 32767.0f;
        mfi_axes[slot][3]     = gp.rightThumbstick.yAxis.value * 32767.0f;

    }

    /* GCGamepad is deprecated */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
    else if (controller.gamepad)
    {
        GCGamepad *gp = (GCGamepad *)controller.gamepad;

        *buttons |= gp.dpad.up.pressed       ? (1 << RETRO_DEVICE_ID_JOYPAD_UP)    : 0;
        *buttons |= gp.dpad.down.pressed     ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN)  : 0;
        *buttons |= gp.dpad.left.pressed     ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)  : 0;
        *buttons |= gp.dpad.right.pressed    ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
        *buttons |= gp.buttonA.pressed       ? (1 << RETRO_DEVICE_ID_JOYPAD_B)     : 0;
        *buttons |= gp.buttonB.pressed       ? (1 << RETRO_DEVICE_ID_JOYPAD_A)     : 0;
        *buttons |= gp.buttonX.pressed       ? (1 << RETRO_DEVICE_ID_JOYPAD_Y)     : 0;
        *buttons |= gp.buttonY.pressed       ? (1 << RETRO_DEVICE_ID_JOYPAD_X)     : 0;
        *buttons |= gp.leftShoulder.pressed  ? (1 << RETRO_DEVICE_ID_JOYPAD_L)     : 0;
        *buttons |= gp.rightShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_R)     : 0;
    }
#pragma clang diagnostic pop
}

static void apple_gamecontroller_joypad_poll(void)
{
    if (!apple_gamecontroller_available())
        return;

    for (GCController *controller in [GCController controllers])
        apple_gamecontroller_joypad_poll_internal(controller);
}

/* GCGamepad is deprecated */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
static void apple_gamecontroller_joypad_register(GCGamepad *gamepad)
{
#ifdef __IPHONE_14_0
    /* Don't let tvOS or iOS do anything with **our** buttons!!
     * iOS will start a screen recording if you hold or doubleclick  
     * the OPTIONS button, we don't want that. */
    if (@available(iOS 14.0, tvOS 14.0, *))
    {
        GCExtendedGamepad *gp = (GCExtendedGamepad *)gamepad.controller.extendedGamepad;
        gp.buttonOptions.preferredSystemGestureState = GCSystemGestureStateDisabled;
        gp.buttonMenu.preferredSystemGestureState    = GCSystemGestureStateDisabled;
        gp.buttonHome.preferredSystemGestureState    = GCSystemGestureStateDisabled;
    }
#endif
    
    gamepad.valueChangedHandler = ^(GCGamepad *updateGamepad, GCControllerElement *element)
    {
        apple_gamecontroller_joypad_poll_internal(updateGamepad.controller);
    };

    /* controllerPausedHandler is deprecated in favor 
     * of being able to deal with the menu
     * button as any other button */
    if (@available(iOS 13, *))
       return;

    {
        gamepad.controller.controllerPausedHandler = ^(GCController *controller)

        {
           uint32_t slot      = (uint32_t)controller.playerIndex;

           /* Support buttons that aren't supported by the mFi 
            * controller via "hotkey" combinations:
            *
            * LS + Menu => Select
            * LT + Menu => L3
            * RT + Menu => R3
            * Note that these are just button presses, and it 
            * does not simulate holding down the button
            */
           if (     controller.gamepad.leftShoulder.pressed 
                 || controller.extendedGamepad.leftShoulder.pressed )
           {
              mfi_buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_START);
              mfi_buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_L);
              mfi_buttons[slot] |= (1 << RETRO_DEVICE_ID_JOYPAD_SELECT);
              dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                    mfi_buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_SELECT);
                    });
              return;
           }

           if (controller.extendedGamepad.leftTrigger.pressed )
           {
              mfi_buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_L2);
              mfi_buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_START);
              mfi_buttons[slot] |= (1 << RETRO_DEVICE_ID_JOYPAD_L3);
              dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                    mfi_buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_L3);
                    });
              return;
           }

           if (controller.extendedGamepad.rightTrigger.pressed )
           {
              mfi_buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_R2);
              mfi_buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_START);
              mfi_buttons[slot] |= (1 << RETRO_DEVICE_ID_JOYPAD_R3);
              dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                    mfi_buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_R3);
                    });
              return;
           }

           mfi_buttons[slot] |= (1 << RETRO_DEVICE_ID_JOYPAD_START);

           dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                 mfi_buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_START);
                 });
        };
    }
}
#pragma clang diagnostic pop

static void mfi_joypad_autodetect_add(unsigned autoconf_pad)
{
    input_autoconfigure_connect("mFi Controller", NULL, mfi_joypad.ident, autoconf_pad, 0, 0);
}

static void apple_gamecontroller_joypad_connect(GCController *controller)
{
    signed desired_index = (int32_t)controller.playerIndex;
    desired_index        = (desired_index >= 0 && desired_index < MAX_MFI_CONTROLLERS)
    ? desired_index : 0;

    /* prevent same controller getting set twice */
    if ([mfiControllers containsObject:controller])
        return;

    if (mfi_controllers[desired_index] != (uint32_t)controller.hash)
    {
        /* desired slot is unused, take it */
        if (!mfi_controllers[desired_index])
        {
            controller.playerIndex = desired_index;
            mfi_controllers[desired_index] = (uint32_t)controller.hash;
        }
        else
        {
            /* find a new slot for this controller that's unused */
            unsigned i;

            for (i = 0; i < MAX_MFI_CONTROLLERS; ++i)
            {
                if (mfi_controllers[i])
                    continue;

                mfi_controllers[i] = (uint32_t)controller.hash;
                controller.playerIndex = i;
                break;
            }
        }

/* GCGamepad is deprecated */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
        [mfiControllers addObject:controller];
        /* Move any non-game controllers (like the siri remote) to the end */
        if (mfiControllers.count > 1)
        {
           int newPlayerIndex = 0;
           NSInteger connectedNonGameControllerIndex = NSNotFound;
           NSUInteger index = 0;

           for (GCController *connectedController in mfiControllers)
           {
              if (     connectedController.gamepad         == nil 
                    && connectedController.extendedGamepad == nil )
                 connectedNonGameControllerIndex = index;
              index++;
           }

           if (connectedNonGameControllerIndex != NSNotFound)
           {
              GCController *nonGameController = [mfiControllers objectAtIndex:connectedNonGameControllerIndex];
              [mfiControllers removeObjectAtIndex:connectedNonGameControllerIndex];
              [mfiControllers addObject:nonGameController];
           }
           for (GCController *gc in mfiControllers)
              gc.playerIndex = newPlayerIndex++;
        }

        apple_gamecontroller_joypad_register(controller.gamepad);
        mfi_joypad_autodetect_add((unsigned)controller.playerIndex);
    }
#pragma clang diagnostic pop
}

static void apple_gamecontroller_joypad_disconnect(GCController* controller)
{
    signed pad = (int32_t)controller.playerIndex;

    if (pad == GCCONTROLLER_PLAYER_INDEX_UNSET)
        return;

    mfi_controllers[pad] = 0;
    if ([mfiControllers containsObject:controller])
    {
        [mfiControllers removeObject:controller];
        input_autoconfigure_disconnect(pad, mfi_joypad.ident);
    }
}

void *apple_gamecontroller_joypad_init(void *data)
{
   static bool inited = false;
   if (inited)
      return (void*)-1;
   if (!apple_gamecontroller_available())
      return NULL;
   mfiControllers = [[NSMutableArray alloc] initWithCapacity:MAX_MFI_CONTROLLERS];
#ifdef __IPHONE_7_0
   [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification
                                                     object:nil
                                                      queue:[NSOperationQueue mainQueue]
                                                 usingBlock:^(NSNotification *note)
                                                 {
                                                    apple_gamecontroller_joypad_connect([note object]);
                                                 }];
   [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification
                                                     object:nil
                                                      queue:[NSOperationQueue mainQueue]
                                                 usingBlock:^(NSNotification *note)
                                                 {
                                                    apple_gamecontroller_joypad_disconnect([note object]);
                                                 } ];
#endif

   return (void*)-1;
}

static void apple_gamecontroller_joypad_destroy(void) { }

static int32_t apple_gamecontroller_joypad_button(
      unsigned port, uint16_t joykey)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   /* Check hat. */
   else if (GET_HAT_DIR(joykey))
      return 0;
   else if (joykey < 32)
      return ((mfi_buttons[port] & (1 << joykey)) != 0);
   return 0;
}

static void apple_gamecontroller_joypad_get_buttons(unsigned port,
      input_bits_t *state)
{
    BITS_COPY16_PTR(state, mfi_buttons[port]);
}

static int16_t apple_gamecontroller_joypad_axis(
      unsigned port, uint32_t joyaxis)
{
    int16_t val  = 0;
    int16_t axis = -1;
    bool is_neg  = false;
    bool is_pos  = false;

    if (AXIS_NEG_GET(joyaxis) < 4)
    {
        axis     = AXIS_NEG_GET(joyaxis);
        is_neg   = true;
    }
    else if(AXIS_POS_GET(joyaxis) < 4)
    {
        axis     = AXIS_POS_GET(joyaxis);
        is_pos   = true;
    }
    else
       return 0;

    if (axis >= 0 && axis < 4)
       val  = mfi_axes[port][axis];
    if (is_neg && val > 0)
       return 0;
    else if (is_pos && val < 0)
       return 0;
    return val;
}

static int16_t apple_gamecontroller_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;

   if (port_idx >= DEFAULT_MAX_PADS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (     (uint16_t)joykey != NO_BTN 
            && !GET_HAT_DIR(i)
            && (i < 32)
            && ((mfi_buttons[port_idx] & (1 << i)) != 0)
         )
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(apple_gamecontroller_joypad_axis(port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static bool apple_gamecontroller_joypad_query_pad(unsigned pad)
{
    return pad < MAX_USERS;
}

static const char *apple_gamecontroller_joypad_name(unsigned pad)
{
    if (pad >= MAX_USERS)
        return NULL;

    return "mFi Controller";
}

input_device_driver_t mfi_joypad = {
    apple_gamecontroller_joypad_init,
    apple_gamecontroller_joypad_query_pad,
    apple_gamecontroller_joypad_destroy,
    apple_gamecontroller_joypad_button,
    apple_gamecontroller_joypad_state,
    apple_gamecontroller_joypad_get_buttons,
    apple_gamecontroller_joypad_axis,
    apple_gamecontroller_joypad_poll,
    NULL,
    NULL,
    apple_gamecontroller_joypad_name,
    "mfi",
};
