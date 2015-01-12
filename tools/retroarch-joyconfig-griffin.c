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

#include "retroarch-joyconfig.c"

#if defined(__linux) && !defined(ANDROID)
#include "../input/drivers/linuxraw_input.c"
#include "../input/drivers_joypad/linuxraw_joypad.c"
#endif

#if defined(HAVE_DINPUT)
#include "../input/drivers/dinput.c"
#endif

#if defined(HAVE_WINXINPUT)
#include "../input/drivers_joypad/winxinput_joypad.c"
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

#include "../libretro-sdk/file/config_file.c"
#include "../libretro-sdk/file/file_path.c"
#include "../libretro-sdk/string/string_list.c"
#include "../libretro-sdk/compat/compat.c"

#include "../input/drivers/nullinput.c"
#include "../input/drivers_joypad/nullinput_joypad.c"

#include "../input/input_context.c"
#include "../input/input_joypad.c"
#include "../input/input_common.c"
#include "../input/input_keymaps.c"

#include "../libretro-sdk/queues/message_queue.c"
