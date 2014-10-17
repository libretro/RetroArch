/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include <string.h>

#import "RetroArch_Apple.h"
#include "../../frontend/frontend.h"
#include "../../file.h"
#include "../../general.h"

id<RetroArch_Platform> apple_platform;

void apple_content_loaded(const char *core_path, const char  *full_path)
{
   [apple_platform loadingCore:BOXSTRING(core_path) withFile:full_path];
}

void apple_rarch_exited(void)
{
   [apple_platform unloadingCore];
}

void apple_run_core(int argc, char **argv)
{
   static char config_path[PATH_MAX];

   strlcpy(config_path, g_defaults.config_path, sizeof(config_path));

   static const char* const argv_menu[] = { "retroarch", "-c", config_path, "--menu", 0 };

   if (argc == 0)
      argc = 4;
   if (!argv)
      argv = (char**)(argv_menu);

   if (rarch_main(argc, argv))
      apple_rarch_exited();
}
