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

#ifndef __RARCH_GENERAL_H
#define __RARCH_GENERAL_H

#include <stdint.h>
#include <math.h>
#include <limits.h>

/* Platform-specific headers */

/* Windows */
#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#endif

#include <boolean.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_inline.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>

#include "configuration.h"
#include "driver.h"
#include "playlist.h"
#include "runloop.h"
#include "defaults.h"
#include "verbosity.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_COMMAND
#include "command.h"
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "1.3.0"
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum sound_mode_enums
{
   SOUND_MODE_NORMAL = 0,
#ifdef HAVE_RSOUND
   SOUND_MODE_RSOUND,
#endif
#ifdef HAVE_HEADSET
   SOUND_MODE_HEADSET,
#endif
   SOUND_MODE_LAST
};

#ifdef __cplusplus
}
#endif

/**
 * retro_fail:
 * @error_code  : Error code.
 * @error       : Error message to show.
 *
 * Sanely kills the program.
 **/
static INLINE void retro_fail(int error_code, const char *error)
{
   global_t *global = global_get_ptr();

   if (!global)
      return;

   /* We cannot longjmp unless we're in rarch_main_init().
    * If not, something went very wrong, and we should 
    * just exit right away. */
   retro_assert(global->inited.error);

   strlcpy(global->error_string, error, sizeof(global->error_string));
   longjmp(global->error_sjlj_context, error_code);
}

#endif


