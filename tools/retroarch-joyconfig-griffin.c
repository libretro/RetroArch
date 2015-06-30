/*  RetroArch JoyConfig.
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

#define IS_JOYCONFIG

#include <retro_environment.h>

#include "retroarch-joyconfig.c"

#include "../libretro-common/dynamic/dylib.c"

#if defined(__linux) && !defined(ANDROID)
#include "../input/drivers/linuxraw_input.c"
#include "../input/drivers_joypad/linuxraw_joypad.c"
#endif

#if defined(HAVE_DINPUT)
#include "../input/drivers/dinput.c"
#include "../input/drivers_joypad/dinput_joypad.c"
#endif

#if defined(HAVE_XINPUT)
#include "../input/drivers_joypad/xinput_joypad.c"
#endif

#if defined(HAVE_UDEV)
#include "../input/drivers_joypad/udev_joypad.c"
#endif

#if defined(HAVE_PARPORT)
#include "../input/drivers_joypad/parport_joypad.c"
#endif

#if defined(HAVE_SDL) || defined(HAVE_SDL2)
#include "../input/drivers_joypad/sdl_joypad.c"
#endif

#include "../libretro-common/queues/fifo_buffer.c"
#include "../libretro-common/file/config_file.c"
#include "../libretro-common/file/file_path.c"
#include "../libretro-common/hash/rhash.c"
#include "../file_path_special.c"
#include "../libretro-common/string/string_list.c"
#include "../libretro-common/compat/compat.c"

#include "../input/drivers/nullinput.c"
#include "../input/drivers_hid/null_hid.c"

#include "../libretro-common/rthreads/rthreads.c"

#ifndef __STDC_C89__
#ifdef HAVE_LIBUSB
#include "../input/drivers_hid/libusb_hid.c"

#ifndef HAVE_HID
#define HAVE_HID
#endif
#endif
#endif

#if defined(__APPLE__) && defined(HAVE_IOHIDMANAGER)
#include "../input/drivers_hid/iohidmanager_hid.c"

#ifndef HAVE_HID
#define HAVE_HID
#endif
#endif

#ifdef HAVE_HID
#include "../input/connect/joypad_connection.c"
#include "../input/connect/connect_ps3.c"
#include "../input/connect/connect_ps4.c"
#include "../input/connect/connect_wii.c"
#endif

#include "../input/drivers_joypad/hid_joypad.c"
#include "../input/drivers_joypad/null_joypad.c"

#include "../input/input_hid_driver.c"
#include "../input/input_joypad_driver.c"
#include "../input/input_joypad.c"
#include "../input/input_common.c"
#include "../input/input_keymaps.c"

#include "../libretro-common/queues/message_queue.c"
