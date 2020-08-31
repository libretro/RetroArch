/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2020 - Daniel De Matteis
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

#ifndef __XINPUT_JOYPAD_H
#define __XINPUT_JOYPAD_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#if defined(__WINRT__)
#include <Xinput.h>
#endif

/* Check if the definitions do not already exist.
 * Official and mingw xinput headers have different include guards.
 * Windows 10 API version doesn't have an include guard at all and just uses #pragma once instead
 */
#if ((!_XINPUT_H_) && (!__WINE_XINPUT_H)) && !defined(__WINRT__) && !defined(_XBOX)

#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

typedef struct
{
   uint16_t wButtons;
   uint8_t  bLeftTrigger;
   uint8_t  bRightTrigger;
   int16_t  sThumbLX;
   int16_t  sThumbLY;
   int16_t  sThumbRX;
   int16_t  sThumbRY;
} XINPUT_GAMEPAD;

typedef struct
{
   uint32_t       dwPacketNumber;
   XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE;

typedef struct
{
   uint16_t wLeftMotorSpeed;
   uint16_t wRightMotorSpeed;
} XINPUT_VIBRATION;

#endif

/* Guide constant is not officially documented. */
#define XINPUT_GAMEPAD_GUIDE 0x0400

#ifndef ERROR_DEVICE_NOT_CONNECTED
#define ERROR_DEVICE_NOT_CONNECTED 1167
#endif


RETRO_BEGIN_DECLS

RETRO_END_DECLS

#endif
