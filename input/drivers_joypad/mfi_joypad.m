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

#include "../input_driver.h"
#include "../../tasks/tasks_internal.h"

#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>

#ifndef MAX_MFI_CONTROLLERS
#define MAX_MFI_CONTROLLERS 4
#endif
#ifndef MAX_MFI_AXES
#define MAX_MFI_AXES 6
#endif

#if TARGET_OS_IOS
#include "../../configuration.h"
#define IPHONE_RUMBLE_AVAIL API_AVAILABLE(ios(14.0))
static CHHapticEngine *deviceHapticEngine IPHONE_RUMBLE_AVAIL;
static id<CHHapticPatternPlayer> deviceWeakPlayer IPHONE_RUMBLE_AVAIL;
static id<CHHapticPatternPlayer> deviceStrongPlayer IPHONE_RUMBLE_AVAIL;
#endif

enum
{
    GCCONTROLLER_PLAYER_INDEX_UNSET = -1,
};

@class MFIRumbleController;

/* TODO/FIXME - static globals */
static uint32_t mfi_buttons[MAX_USERS];
static int16_t  mfi_axes[MAX_USERS][MAX_MFI_AXES];
static uint32_t mfi_controllers[MAX_MFI_CONTROLLERS];
static MFIRumbleController *mfi_rumblers[MAX_MFI_CONTROLLERS];
#define MFI_WEAK_RUMBLE 0.3f
static NSMutableArray *mfiControllers;
static bool mfi_inited;

static bool apple_gamecontroller_available(void)
{
#if defined(IOS)
    int major, minor;
    get_ios_version(&major, &minor);

    if (major <= 6)
        return false;
#endif

    return true;
}

static bool mfi_controller_is_siri_remote(GCController *controller)
{
   return controller.microGamepad && !controller.extendedGamepad && [@"Remote" isEqualToString:controller.vendorName];
}

static void apple_gamecontroller_joypad_poll_internal(GCController *controller, uint32_t slot)
{
    uint32_t *buttons        = &mfi_buttons[slot];
    /* Retain the values from the paused controller handler and pass them through.
     * The menu button can be pressed/unpressed
     * like any other button in iOS 13,
     * so no need to passthrough anything */
    if (@available(iOS 13, *))
        *buttons             = 0;
    else
    {
       /* Use the paused controller handler for iOS versions below 13 */
       uint32_t pause        = *buttons & (1 << RETRO_DEVICE_ID_JOYPAD_START);
       uint32_t select       = *buttons & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT);
       uint32_t l3           = *buttons & (1 << RETRO_DEVICE_ID_JOYPAD_L3);
       uint32_t r3           = *buttons & (1 << RETRO_DEVICE_ID_JOYPAD_R3);
       *buttons              = 0 | pause | select | l3 | r3;
    }
    memset(mfi_axes[slot], 0, sizeof(mfi_axes[0]));

    if (@available(macOS 11, iOS 14, tvOS 14, *))
    {
        GCPhysicalInputProfile *profile = controller.physicalInputProfile;

        *buttons |= [[profile.dpads[GCInputDirectionPad] up] isPressed]       ? (1 << RETRO_DEVICE_ID_JOYPAD_UP)     : 0;
        *buttons |= [[profile.dpads[GCInputDirectionPad] down] isPressed]     ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN)   : 0;
        *buttons |= [[profile.dpads[GCInputDirectionPad] left] isPressed]     ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)   : 0;
        *buttons |= [[profile.dpads[GCInputDirectionPad] right] isPressed]    ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)  : 0;
        *buttons |= [profile.buttons[GCInputButtonA] isPressed]               ? (1 << RETRO_DEVICE_ID_JOYPAD_B)      : 0;
        *buttons |= [profile.buttons[GCInputButtonB] isPressed]               ? (1 << RETRO_DEVICE_ID_JOYPAD_A)      : 0;
        *buttons |= [profile.buttons[GCInputButtonX] isPressed]               ? (1 << RETRO_DEVICE_ID_JOYPAD_Y)      : 0;
        *buttons |= [profile.buttons[GCInputButtonY] isPressed]               ? (1 << RETRO_DEVICE_ID_JOYPAD_X)      : 0;
        *buttons |= [profile.buttons[GCInputLeftShoulder] isPressed]          ? (1 << RETRO_DEVICE_ID_JOYPAD_L)      : 0;
        *buttons |= [profile.buttons[GCInputRightShoulder] isPressed]         ? (1 << RETRO_DEVICE_ID_JOYPAD_R)      : 0;
        *buttons |= [profile.buttons[GCInputLeftTrigger] isPressed]           ? (1 << RETRO_DEVICE_ID_JOYPAD_L2)     : 0;
        *buttons |= [profile.buttons[GCInputRightTrigger] isPressed]          ? (1 << RETRO_DEVICE_ID_JOYPAD_R2)     : 0;
        *buttons |= [profile.buttons[GCInputLeftThumbstickButton] isPressed]  ? (1 << RETRO_DEVICE_ID_JOYPAD_L3)     : 0;
        *buttons |= [profile.buttons[GCInputRightThumbstickButton] isPressed] ? (1 << RETRO_DEVICE_ID_JOYPAD_R3)     : 0;
        *buttons |= [profile.buttons[GCInputButtonOptions] isPressed]         ? (1 << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
        *buttons |= [profile.buttons[GCInputButtonMenu] isPressed]            ? (1 << RETRO_DEVICE_ID_JOYPAD_START)  : 0;
        *buttons |= [profile.buttons[GCInputButtonHome] isPressed]            ? (1 << RARCH_FIRST_CUSTOM_BIND)       : 0;

        mfi_axes[slot][0] = [[profile.dpads[GCInputLeftThumbstick] xAxis] value]  * 32767.0f;
        mfi_axes[slot][1] = [[profile.dpads[GCInputLeftThumbstick] yAxis] value]  * 32767.0f;
        mfi_axes[slot][2] = [[profile.dpads[GCInputRightThumbstick] xAxis] value] * 32767.0f;
        mfi_axes[slot][3] = [[profile.dpads[GCInputRightThumbstick] yAxis] value] * 32767.0f;
        mfi_axes[slot][4] = [profile.buttons[GCInputLeftTrigger] value]           * 32767.0f;
        mfi_axes[slot][5] = [profile.buttons[GCInputRightTrigger] value]          * 32767.0f;
    }
    else if (controller.extendedGamepad)
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
#if OSX || __IPHONE_OS_VERSION_MAX_ALLOWED >= 120100 || __TV_OS_VERSION_MAX_ALLOWED >= 120100
        if (@available(iOS 12.1, macOS 10.15, tvOS 12.1, *))
        {
            *buttons         |= gp.leftThumbstickButton.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
            *buttons         |= gp.rightThumbstickButton.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
        }
#endif

#if OSX || __IPHONE_OS_VERSION_MAX_ALLOWED >= 130000 || __TV_OS_VERSION_MAX_ALLOWED >= 130000
        if (@available(iOS 13, tvOS 13, macOS 10.15, *))
        {
            *buttons             |= gp.buttonOptions.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
            *buttons             |= gp.buttonMenu.pressed    ? (1 << RETRO_DEVICE_ID_JOYPAD_START)  : 0;
            if (@available(iOS 14, tvOS 14, macOS 11, *))
                *buttons         |= gp.buttonHome.pressed    ? (1 << RARCH_FIRST_CUSTOM_BIND)       : 0;
            else
            {
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
        }
#endif

        mfi_axes[slot][0]         = gp.leftThumbstick.xAxis.value * 32767.0f;
        mfi_axes[slot][1]         = gp.leftThumbstick.yAxis.value * 32767.0f;
        mfi_axes[slot][2]         = gp.rightThumbstick.xAxis.value * 32767.0f;
        mfi_axes[slot][3]         = gp.rightThumbstick.yAxis.value * 32767.0f;
        mfi_axes[slot][4]         = gp.leftTrigger.value * 32767.0f;
        mfi_axes[slot][5]         = gp.rightTrigger.value * 32767.0f;

    }
    else if (controller.microGamepad)
    {
    }

    /* GCGamepad is deprecated */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
    else if (controller.gamepad)
    {
        GCGamepad *gp = (GCGamepad *)controller.gamepad;
        *buttons     |= gp.dpad.up.pressed       ? (1 << RETRO_DEVICE_ID_JOYPAD_UP)    : 0;
        *buttons     |= gp.dpad.down.pressed     ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN)  : 0;
        *buttons     |= gp.dpad.left.pressed     ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)  : 0;
        *buttons     |= gp.dpad.right.pressed    ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
        *buttons     |= gp.buttonA.pressed       ? (1 << RETRO_DEVICE_ID_JOYPAD_B)     : 0;
        *buttons     |= gp.buttonB.pressed       ? (1 << RETRO_DEVICE_ID_JOYPAD_A)     : 0;
        *buttons     |= gp.buttonX.pressed       ? (1 << RETRO_DEVICE_ID_JOYPAD_Y)     : 0;
        *buttons     |= gp.buttonY.pressed       ? (1 << RETRO_DEVICE_ID_JOYPAD_X)     : 0;
        *buttons     |= gp.leftShoulder.pressed  ? (1 << RETRO_DEVICE_ID_JOYPAD_L)     : 0;
        *buttons     |= gp.rightShoulder.pressed ? (1 << RETRO_DEVICE_ID_JOYPAD_R)     : 0;
    }
#pragma clang diagnostic pop
}

static void apple_gamecontroller_joypad_poll(void)
{
    if (!apple_gamecontroller_available())
        return;

    for (GCController *controller in [GCController controllers])
    {
       /* If we have not assigned a slot to this controller yet, ignore it. */
       if (  controller &&
             (controller.playerIndex >= 0) && (controller.playerIndex < MAX_USERS) &&
             !mfi_controller_is_siri_remote(controller))
          apple_gamecontroller_joypad_poll_internal(controller, (uint32_t)controller.playerIndex);
    }
}

static void apple_gamecontroller_joypad_register(GCController *controller)
{
#ifdef __IPHONE_14_0
    /* Don't let tvOS or iOS do anything with **our** buttons!!
     * iOS will start a screen recording if you hold or doubleclick
     * the OPTIONS button, we don't want that. */
    if (@available(iOS 14.0, tvOS 14.0, macOS 11, *))
    {
        GCExtendedGamepad *gp = (GCExtendedGamepad *)controller.extendedGamepad;
        gp.buttonOptions.preferredSystemGestureState = GCSystemGestureStateDisabled;
        gp.buttonMenu.preferredSystemGestureState    = GCSystemGestureStateDisabled;
        gp.buttonHome.preferredSystemGestureState    = GCSystemGestureStateDisabled;
    }
#endif

    /* controllerPausedHandler is deprecated in favor
     * of being able to deal with the menu
     * button as any other button */
    if (@available(iOS 13, *))
       return;

/* GCGamepad is deprecated */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
    {
        controller.controllerPausedHandler = ^(GCController *controller)

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
              mfi_buttons[slot]       &= ~(1 << RETRO_DEVICE_ID_JOYPAD_START);
              mfi_buttons[slot]       &= ~(1 << RETRO_DEVICE_ID_JOYPAD_L);
              mfi_buttons[slot]       |=  (1 << RETRO_DEVICE_ID_JOYPAD_SELECT);
              dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                    mfi_buttons[slot] &= ~(1 << RETRO_DEVICE_ID_JOYPAD_SELECT);
                    });
              return;
           }

           if (controller.extendedGamepad.leftTrigger.pressed )
           {
              mfi_buttons[slot]       &= ~(1 << RETRO_DEVICE_ID_JOYPAD_L2);
              mfi_buttons[slot]       &= ~(1 << RETRO_DEVICE_ID_JOYPAD_START);
              mfi_buttons[slot]       |=  (1 << RETRO_DEVICE_ID_JOYPAD_L3);
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
#pragma clang diagnostic pop
}

static void mfi_joypad_autodetect_add(unsigned autoconf_pad, const char *display_name)
{
    input_autoconfigure_connect("mFi Controller", display_name, mfi_joypad.ident, autoconf_pad, 0, 0);
}

#define MFI_RUMBLE_AVAIL API_AVAILABLE(macos(11.0), ios(14.0), tvos(14.0))
@interface MFIRumbleController : NSObject
@property (nonatomic, strong, readonly) GCController *controller;
@property (nonatomic, strong) NSMutableSet<CHHapticEngine *> *engines MFI_RUMBLE_AVAIL;
@property (nonatomic, strong, readonly) id<CHHapticPatternPlayer> strongPlayer MFI_RUMBLE_AVAIL;
@property (nonatomic, strong, readonly) id<CHHapticPatternPlayer> weakPlayer MFI_RUMBLE_AVAIL;
@end

@implementation MFIRumbleController
@synthesize strongPlayer = _strongPlayer;
@synthesize weakPlayer   = _weakPlayer;

- (instancetype)initWithController:(GCController*)controller MFI_RUMBLE_AVAIL
{
    if (self = [super init])
    {
        if (!controller.haptics)
            return self;

        _controller = controller;
        _engines = [[NSMutableSet alloc] init];
    }
    return self;
}

- (id<CHHapticPatternPlayer>)createPlayerWithLocality:(GCHapticsLocality)locality andIntensity:(float)intensity MFI_RUMBLE_AVAIL
{
    NSError *error;
    if (!self.controller)
        return nil;

    if (![self.controller.haptics.supportedLocalities containsObject:locality])
        locality = GCHapticsLocalityDefault;
    CHHapticEngine *engine = [self.controller.haptics createEngineWithLocality:locality];
    [engine startAndReturnError:&error];
    if (error)
        return nil;

    [self.engines addObject:engine];

    __weak MFIRumbleController *weakSelf = self;
    engine.stoppedHandler = ^(CHHapticEngineStoppedReason stoppedReason)
    {
        MFIRumbleController *strongSelf = weakSelf;
        if (!strongSelf)
            return;

        [strongSelf shutdown];
    };
    engine.resetHandler = ^{
        MFIRumbleController *strongSelf = weakSelf;
        if (!strongSelf)
            return;

        for (CHHapticEngine *eng in strongSelf.engines)
            [eng startAndReturnError:nil];
    };

    CHHapticEventParameter *intense;
    CHHapticEvent *event;
    CHHapticPattern *pattern;

    intense = [[CHHapticEventParameter alloc]
               initWithParameterID:CHHapticEventParameterIDHapticIntensity
               value:intensity];
    event   = [[CHHapticEvent alloc]
             initWithEventType:CHHapticEventTypeHapticContinuous
             parameters:[NSArray arrayWithObjects:intense, nil]
             relativeTime:0
             duration:GCHapticDurationInfinite];
    pattern = [[CHHapticPattern alloc]
               initWithEvents:[NSArray arrayWithObject:event]
               parameters:[[NSArray alloc] init]
               error:&error];

    if (error)
        return nil;

    id<CHHapticPatternPlayer> player = [engine createPlayerWithPattern:pattern error:&error];
    if (error)
        return nil;
    [player stopAtTime:0 error:&error];
    return player;
}

- (id<CHHapticPatternPlayer>)strongPlayer
{
    _strongPlayer = _strongPlayer ?: [self createPlayerWithLocality:GCHapticsLocalityAll andIntensity:1.0];
    return _strongPlayer;
}

- (id<CHHapticPatternPlayer>)weakPlayer
{
    _weakPlayer = _weakPlayer ?: [self createPlayerWithLocality:GCHapticsLocalityTriggers andIntensity:MFI_WEAK_RUMBLE];
    return _weakPlayer;
}

- (void)shutdown
{
    if (@available(iOS 14, tvOS 14, macOS 11, *))
    {
        _weakPlayer   = nil;
        _strongPlayer = nil;
        [self.engines removeAllObjects];
    }
}

@end

static void apple_gamecontroller_joypad_setup_haptics(GCController *controller)
{
    if (@available(iOS 14, tvOS 14, macOS 11, *))
        mfi_rumblers[controller.playerIndex] = [[MFIRumbleController alloc] initWithController:controller];
}

static void apple_gamecontroller_joypad_connect(GCController *controller)
{
    signed desired_index = (int32_t)controller.playerIndex;
    if (!(desired_index >= 0 && desired_index < MAX_MFI_CONTROLLERS))
       desired_index     = 0;

    if (mfi_controller_is_siri_remote(controller))
    {
        RARCH_WARN("[mfi] ignoring siri remote as a controller\n");
        return;
    }

    /* Prevent same controller getting set twice */
    if ([mfiControllers containsObject:controller])
    {
        RARCH_DBG("[mfi] got connected notice for controller already connected\n");
        return;
    }

    if (@available(macOS 11, iOS 14, tvOS 14, *))
    {
        RARCH_DBG("[mfi] new controller connected:\n");
        RARCH_DBG("[mfi]    name: %s\n", [controller.vendorName UTF8String]);
        RARCH_DBG("[mfi]    category: %s\n", [controller.productCategory UTF8String]);
        RARCH_DBG("[mfi]    has battery info: %s\n", controller.battery != nil ? "yes" : "no");
        RARCH_DBG("[mfi]    has haptics: %s\n", controller.haptics != nil ? "yes" : "no");
        RARCH_DBG("[mfi]    has light: %s\n", controller.light != nil ? "yes" : "no");
        RARCH_DBG("[mfi]    has motion: %s\n", controller.motion != nil ? "yes" : "no");
        RARCH_DBG("[mfi]    has microGamepad: %s\n", controller.microGamepad != nil ? "yes" : "no");
        RARCH_DBG("[mfi]    has extendedGamepad: %s\n", controller.extendedGamepad != nil ? "yes" : "no");
        RARCH_DBG("[mfi]    input profile:\n");
        for (NSString *elem in controller.physicalInputProfile.elements.allKeys)
        {
            RARCH_DBG("[mfi]       %s\n", [elem UTF8String]);
            GCControllerElement *element = controller.physicalInputProfile.elements[elem];
            RARCH_DBG("[mfi]          analog: %s\n", element.analog ? "yes" : "no");
            RARCH_DBG("[mfi]          localizedName: %s\n", [element.localizedName UTF8String]);
        }
    }

    if (mfi_controllers[desired_index] != (uint32_t)controller.hash)
    {
        /* Desired slot is unused, take it */
        if (!mfi_controllers[desired_index])
        {
            RARCH_LOG("[mfi] controller given desired index %d\n", desired_index);
            controller.playerIndex = desired_index;
            mfi_controllers[desired_index] = (uint32_t)controller.hash;
        }
        else
        {
            /* Find a new slot for this controller that's unused */
            unsigned i;

            for (i = 0; i < MAX_MFI_CONTROLLERS; ++i)
            {
                if (mfi_controllers[i])
                    continue;

                RARCH_LOG("[mfi] controller reassigned from desired %d to %d\n", desired_index, i);
                mfi_controllers[i]     = (uint32_t)controller.hash;
                controller.playerIndex = i;
                break;
            }

            if (i == MAX_MFI_CONTROLLERS)
            {
                /* shouldn't ever get here, this is an Apple limit */
                RARCH_ERR("[mfi] too many connected controllers, ignoring\n");
                return;
            }
        }
    }

    [mfiControllers addObject:controller];

    RARCH_LOG("[mfi] controller connected, beginning setup and autodetect\n");
    apple_gamecontroller_joypad_register(controller);
    apple_gamecontroller_joypad_setup_haptics(controller);
    mfi_joypad_autodetect_add((unsigned)controller.playerIndex, [controller.vendorName cStringUsingEncoding:NSUTF8StringEncoding]);
}

static void apple_gamecontroller_joypad_disconnect(GCController* controller)
{
    signed pad = (int32_t)controller.playerIndex;

    if (pad == GCCONTROLLER_PLAYER_INDEX_UNSET)
        return;

    mfi_rumblers[pad]    = nil;
    mfi_controllers[pad] = 0;
    if ([mfiControllers containsObject:controller])
    {
        [mfiControllers removeObject:controller];
        input_autoconfigure_disconnect(pad, mfi_joypad.ident);
    }
}

#if TARGET_OS_IOS
static void apple_gamecontroller_device_haptics_setup(void) IPHONE_RUMBLE_AVAIL
{
    if (!CHHapticEngine.capabilitiesForHardware.supportsHaptics)
        return;

    if (deviceHapticEngine)
        return;

    NSError *error;
    CHHapticEngine *engine = [[CHHapticEngine alloc] initAndReturnError:&error];
    if (error)
        return;
    [engine startAndReturnError:&error];
    if (error)
        return;
    deviceHapticEngine = engine;

    deviceHapticEngine.stoppedHandler = ^(CHHapticEngineStoppedReason reason)
    {
        deviceWeakPlayer = nil;
        deviceStrongPlayer = nil;
        deviceHapticEngine = nil;
    };
    deviceHapticEngine.resetHandler = ^{
        if (!deviceHapticEngine)
            return;
        [deviceHapticEngine startAndReturnError:nil];
    };
}

static id<CHHapticPatternPlayer> apple_gamecontroller_device_haptics_create_player(float intensity) IPHONE_RUMBLE_AVAIL
{
    if (!CHHapticEngine.capabilitiesForHardware.supportsHaptics)
        return nil;

    apple_gamecontroller_device_haptics_setup();
    if (!deviceHapticEngine)
        return nil;

    CHHapticEventParameter *intense;
    CHHapticEvent *event;
    CHHapticPattern *pattern;
    NSError *error;

    intense = [[CHHapticEventParameter alloc]
               initWithParameterID:CHHapticEventParameterIDHapticIntensity
               value:intensity];
    event   = [[CHHapticEvent alloc]
               initWithEventType:CHHapticEventTypeHapticContinuous
               parameters:[NSArray arrayWithObjects:intense, nil]
               relativeTime:0
               duration:GCHapticDurationInfinite];
    pattern = [[CHHapticPattern alloc]
               initWithEvents:[NSArray arrayWithObject:event]
               parameters:[[NSArray alloc] init]
               error:&error];

    if (error)
        return nil;

    id<CHHapticPatternPlayer> player = [deviceHapticEngine createPlayerWithPattern:pattern error:&error];
    if (error)
        return nil;
    [player stopAtTime:0 error:&error];
    return player;
}

static id<CHHapticPatternPlayer> apple_gamecontroller_device_haptics_strong_player(void) IPHONE_RUMBLE_AVAIL
{
    if (!deviceStrongPlayer)
        deviceStrongPlayer = apple_gamecontroller_device_haptics_create_player(1.0f);
    return deviceStrongPlayer;
}

static id<CHHapticPatternPlayer> apple_gamecontroller_device_haptics_weak_player(void) IPHONE_RUMBLE_AVAIL
{
    if (!deviceWeakPlayer)
        deviceWeakPlayer = apple_gamecontroller_device_haptics_create_player(0.5f);
    return deviceWeakPlayer;
}
#endif

void *apple_gamecontroller_joypad_init(void *data)
{
   if (mfi_inited)
      return (void*)-1;

#if TARGET_OS_IOS
   if (@available(iOS 14, *))
      apple_gamecontroller_device_haptics_setup();
#endif

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
   mfi_inited = true;
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
    if (AXIS_NEG_GET(joyaxis) < MAX_MFI_AXES)
    {
       int16_t axis = AXIS_NEG_GET(joyaxis);
       int16_t val  = mfi_axes[port][axis];
       if (val < 0)
          return val;
    }
    else if (AXIS_POS_GET(joyaxis) < MAX_MFI_AXES)
    {
       int16_t axis = AXIS_POS_GET(joyaxis);
       int16_t val  = mfi_axes[port][axis];
       if (val > 0)
          return val;
    }
    return 0;
}

static int16_t apple_gamecontroller_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;

   if (port_idx < DEFAULT_MAX_PADS)
   {
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
   }

   return ret;
}

static bool apple_gamecontroller_joypad_set_rumble(unsigned pad,
      enum retro_rumble_effect type, uint16_t strength)
{
#if TARGET_OS_IOS
    settings_t *settings            = config_get_ptr();
    bool enable_device_vibration    = settings->bools.enable_device_vibration;

    if (@available(iOS 14, *))
    {
        if (enable_device_vibration && pad == 0)
        {
            NSError *error;
            id<CHHapticPatternPlayer> player = (type == RETRO_RUMBLE_STRONG ?
                                                apple_gamecontroller_device_haptics_strong_player() :
                                                apple_gamecontroller_device_haptics_weak_player());
            if (player)
            {
                if (strength == 0)
                    [player stopAtTime:0 error:&error];
                else
                {
                    float str = (float)strength / 65535.0f;
                    CHHapticDynamicParameter *param = [[CHHapticDynamicParameter alloc]
                                                       initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl
                                                       value:str
                                                       relativeTime:0];
                    [player sendParameters:[NSArray arrayWithObject:param] atTime:0 error:&error];
                    if (!error)
                        [player startAtTime:0 error:&error];
                }
            }
        }
    }
#endif

    if (pad < MAX_MFI_CONTROLLERS)
    {
       if (@available(iOS 14, tvOS 14, macOS 11, *))
       {
          MFIRumbleController *rumble = mfi_rumblers[pad];
          if (rumble)
          {
             NSError *error;
             id<CHHapticPatternPlayer> player = (type == RETRO_RUMBLE_STRONG ? rumble.strongPlayer : rumble.weakPlayer);
             if (player)
             {
                if (strength == 0)
                   [player stopAtTime:0 error:&error];
                else
                {
                   float str = (float)strength / 65535.0f;
                   if (type == RETRO_RUMBLE_WEAK) str *= MFI_WEAK_RUMBLE;
                   CHHapticDynamicParameter *param = [[CHHapticDynamicParameter alloc]
                      initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl
                                    value:str
                             relativeTime:0];
                   [player sendParameters:[NSArray arrayWithObject:param] atTime:0 error:&error];
                   if (!error)
                      [player startAtTime:0 error:&error];
                }
                return error;
             }
          }
       }
    }
    return false;
}

static bool apple_gamecontroller_joypad_query_pad(unsigned pad)
{
    return pad < MAX_USERS;
}

static const char *apple_gamecontroller_joypad_name(unsigned pad)
{
    if (pad < MAX_USERS)
       return "mFi Controller";
    return NULL;
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
    apple_gamecontroller_joypad_set_rumble,
    NULL,
    NULL,
    NULL,
    apple_gamecontroller_joypad_name,
    "mfi",
};
