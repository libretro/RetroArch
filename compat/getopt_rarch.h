/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __RARCH_GETOPT_H
#define __RARCH_GETOPT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// Custom implementation of the GNU getopt_long for portability.
// Not designed to be fully compatible,
// but compatible with the features RetroArch uses.

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
// Avoid possible naming collisions during link since we prefer to use the actual name.
#define getopt_long(argc, argv, optstring, longopts, longindex) __getopt_long_rarch(argc, argv, optstring, longopts, longindex)

#ifdef __cplusplus
extern "C" {
#endif

struct option
{
   const char *name;
   int has_arg;
   int *flag;
   int val;
};

// argv[] is declared with char * const argv[] in GNU,
// but this makes no sense, as non-POSIX getopt_long mutates argv (non-opts are moved to the end).
int getopt_long(int argc, char *argv[],
      const char *optstring, const struct option *longopts, int *longindex);
extern char *optarg;
extern int optind, opterr, optopt;
#ifdef __cplusplus
}
#endif
#endif

#endif

