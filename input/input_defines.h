/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef __INPUT_DEFINES__H
#define __INPUT_DEFINES__H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum analog_dpad_mode
{
   ANALOG_DPAD_NONE = 0,
   ANALOG_DPAD_LSTICK,
   ANALOG_DPAD_RSTICK,
   ANALOG_DPAD_LAST
};

/* Specialized _MOUSE that targets the full screen regardless of viewport.
 */
#define RARCH_DEVICE_MOUSE_SCREEN      (RETRO_DEVICE_MOUSE | 0x10000)

/* Specialized _POINTER that targets the full screen regardless of viewport.
 * Should not be used by a libretro implementation as coordinates returned
 * make no sense.
 *
 * It is only used internally for overlays. */
#define RARCH_DEVICE_POINTER_SCREEN    (RETRO_DEVICE_POINTER | 0x10000)

#define RARCH_DEVICE_ID_POINTER_BACK   (RETRO_DEVICE_ID_POINTER_PRESSED | 0x10000)

/* libretro has 16 buttons from 0-15 (libretro.h)
 * Analog binds use RETRO_DEVICE_ANALOG, but we follow the same scheme
 * internally in RetroArch for simplicity, so they are mapped into [16, 23].
 */
#define RARCH_FIRST_CUSTOM_BIND        16
#define RARCH_FIRST_META_KEY           RARCH_CUSTOM_BIND_LIST_END

#define AXIS_NEG(x)        (((uint32_t)(x) << 16) | UINT16_C(0xFFFF))
#define AXIS_POS(x)        ((uint32_t)(x) | UINT32_C(0xFFFF0000))
#define AXIS_NONE          UINT32_C(0xFFFFFFFF)
#define AXIS_DIR_NONE      UINT16_C(0xFFFF)

#define AXIS_NEG_GET(x)    (((uint32_t)(x) >> 16) & UINT16_C(0xFFFF))
#define AXIS_POS_GET(x)    ((uint32_t)(x) & UINT16_C(0xFFFF))

#define NO_BTN             UINT16_C(0xFFFF)

#define HAT_UP_SHIFT       15
#define HAT_DOWN_SHIFT     14
#define HAT_LEFT_SHIFT     13
#define HAT_RIGHT_SHIFT    12
#define HAT_UP_MASK        (1 << HAT_UP_SHIFT)
#define HAT_DOWN_MASK      (1 << HAT_DOWN_SHIFT)
#define HAT_LEFT_MASK      (1 << HAT_LEFT_SHIFT)
#define HAT_RIGHT_MASK     (1 << HAT_RIGHT_SHIFT)
#define HAT_MAP(x, hat)    ((x & ((1 << 12) - 1)) | hat)

#define HAT_MASK           (HAT_UP_MASK | HAT_DOWN_MASK | HAT_LEFT_MASK | HAT_RIGHT_MASK)
#define GET_HAT_DIR(x)     (x & HAT_MASK)
#define GET_HAT(x)         (x & (~HAT_MASK))


#ifdef __cplusplus
}
#endif

#endif
