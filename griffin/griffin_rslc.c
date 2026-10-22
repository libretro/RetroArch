/*  RetroArch - A frontend for libretro.
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

/* rslc: the clean-room slang compiler from libretro-common, as one
 * translation unit for griffin builds.  The three units share only
 * rslc_share.h, so they amalgamate without renaming.  A Vulkan-only
 * build may define RSLC_NO_TEXT and leave rslc_emit.c out. */

#ifdef HAVE_RSLC
#include "../libretro-common/gfx/shaders/rslc_front.c"
#include "../libretro-common/gfx/shaders/rslc_ir.c"
#ifndef RSLC_NO_TEXT
#include "../libretro-common/gfx/shaders/rslc_emit.c"
#endif
#endif
