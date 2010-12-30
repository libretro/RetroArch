/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
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


#ifndef __FILTERS_H
#define __FILTERS_H

#ifdef HAVE_FILTER

#include "pastlib.h"
#include "grayscale.h"
#include "bleed.h"
#include "ntsc.h"

#define FILTER_HQ2X 1
#define FILTER_HQ4X 2
#define FILTER_GRAYSCALE 3
#define FILTER_BLEED 4
#define FILTER_NTSC 5
#define FILTER_HQ2X_STR "hq2x"
#define FILTER_HQ4X_STR "hq4x"
#define FILTER_GRAYSCALE_STR "grayscale"
#define FILTER_BLEED_STR "bleed"
#define FILTER_NTSC_STR "ntsc"
#endif

#endif
