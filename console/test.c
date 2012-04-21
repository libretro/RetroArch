/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

// Test module to check re-entrancy of libsnes implementations.
// Reruns RetroArch main loop with all roms defined on command-line
// to check if libsnes can load multiple roms after each other.

#include "../getopt_ssnes.h"
#include "../general.h"
#include <string.h>

int ssnes_main(int argc, char *argv[]);

#undef main
int main(int argc, char *argv[])
{
   while (ssnes_main(argc, argv) == 0)
   {
      if (optind + 1 >= argc)
         return 0;

      memmove(&argv[optind], &argv[optind + 1], (argc - optind - 1) * sizeof(char*));
      argc--;

      ssnes_main_clear_state();
   }
}

