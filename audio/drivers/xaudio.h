/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018 - Krzysztof Haładyn
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

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
/* XAudio 2.7 it part of the old DirectX SDKs. XAudio 2.8+ is part of the
 * Windows OS itself (starting from Windows 8). Since UWP lets you access
 * only libraries that are built-in to the OS, the headers had to be
 * upgraded to the newer version to get audio support working. */
#include "xaudio29.h"
#else
/* The old version still has to be there since XAudio 2.8 is not available
 * on Windows 7 and earlier */
#include "xaudio27.h"
#endif
