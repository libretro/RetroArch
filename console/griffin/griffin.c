/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#if defined(_XBOX)
#include "../../msvc/msvc_compat.h"
#endif

/*============================================================
	CONSOLE EXTENSIONS
============================================================ */
#include "../console_ext.c"

/*============================================================
	COMPATIBILITY
============================================================ */
#include "../../compat/compat.c"

/*============================================================
	CONFIG FILE
============================================================ */
#ifdef _XBOX
#undef __SSNES_POSIX_STRING_H
#undef __SSNES_MSVC_COMPAT_H
#undef strcasecmp
#endif
#include "../../conf/config_file.c"

#include "func_hooks.h"

/*============================================================
	VIDEO
============================================================ */
#if defined(__CELLOS_LV2__)
#include "../../ps3/ps3_video_psgl.c"
#include "../../ps3/image.c"
#elif defined(_XBOX)
#include "../../360/xdk360_video.cpp"
#include "../../360/fonts.cpp"
#endif

/*============================================================
	INPUT
============================================================ */
#if defined(__CELLOS_LV2__)
#include "../../ps3/ps3_input.c"
#elif defined(_XBOX)
#include "../../360/xdk360_input.c"
#endif

/*============================================================
	SNES STATE
============================================================ */
#include "../../gfx/snes_state.c"

/*============================================================
	DRIVERS
============================================================ */
#include "../../driver.c"

/*============================================================
	FIFO BUFFER
============================================================ */
#include "../../fifo_buffer.c"

/*============================================================
	AUDIO HERMITE
============================================================ */
#include "../../audio/hermite.c"

/*============================================================
	RSOUND
============================================================ */
#ifdef __CELLOS_LV2__
#include "../../console/librsound/librsound.c"
#include "../../audio/rsound.c"
#endif

/*============================================================
	AUDIO UTILS
============================================================ */
#include "../../audio/utils.c"

/*============================================================
	AUDIO
============================================================ */
#if defined(__CELLOS_LV2__)
#include "../../ps3/ps3_audio.c"
#elif defined(_XBOX)
#include "../../360/xdk360_audio.cpp"
#endif

/*============================================================
	DYNAMIC
============================================================ */
#include "../../dynamic.c"

/*============================================================
	FILE
============================================================ */
#if defined(__CELLOS_LV2__)
#include "../../ps3/file_browser.c"
#elif defined(_XBOX)
#include "../../360/file_browser.c"
#endif
#include "../../file.c"

/*============================================================
	MESSAGE
============================================================ */
#include "../../message.c"

/*============================================================
	PATCH
============================================================ */
#include "../../patch.c"

/*============================================================
	SETTINGS
============================================================ */
#include "../../settings.c"

/*============================================================
	REWIND
============================================================ */
#include "../../rewind.c"



/*============================================================
	MAIN
============================================================ */
#if defined(__CELLOS_LV2__)
#include "../../ps3/main.c"
#elif defined(_XBOX)
#include "../../360/main.c"
#endif

/*============================================================
	SSNES
============================================================ */
#include "../../ssnes.c"

/*============================================================
	THREAD
============================================================ */
#include "../../thread.c"

/*============================================================
	NETPLAY
============================================================ */
#include "../../netplay.c"

/*============================================================
	MENU
============================================================ */
#if defined(__CELLOS_LV2__)
#include "../../ps3/menu.c"
#elif defined(_XBOX)
#include "../../360/menu.cpp"
#endif
