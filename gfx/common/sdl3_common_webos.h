/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2019 - Daniel De Matteis
 *  Copyright (C)      2026 - Rob Loach
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

#ifndef SDL3_COMMON_WEBOS_H__
#define SDL3_COMMON_WEBOS_H__

/*
 * Defined in https://github.com/webosbrew/SDL-webOS/blob/webOS-3.4.x/include/SDL3/SDL_scancode.h and
 *            https://github.com/webosbrew/SDL-webOS/blob/webOS-3.4.x/include/SDL3/SDL_hints.h
 * 
 * Included for compiling against standard sdl3 headers with no hard dependency on webOS fork
*/

#ifndef SDL_SCANCODE_WEBOS_HOME
#define SDL_SCANCODE_WEBOS_HOME 364
#endif
#ifndef SDL_SCANCODE_WEBOS_CH_UP
#define SDL_SCANCODE_WEBOS_CH_UP 365
#endif
#ifndef SDL_SCANCODE_WEBOS_CH_DOWN
#define SDL_SCANCODE_WEBOS_CH_DOWN 366
#endif
#ifndef SDL_SCANCODE_WEBOS_BACK
#define SDL_SCANCODE_WEBOS_BACK 367
#endif
#ifndef SDL_SCANCODE_WEBOS_CURSOR_SHOW
#define SDL_SCANCODE_WEBOS_CURSOR_SHOW 368
#endif
#ifndef SDL_SCANCODE_WEBOS_CURSOR_HIDE
#define SDL_SCANCODE_WEBOS_CURSOR_HIDE 369
#endif
#ifndef SDL_SCANCODE_WEBOS_RED
#define SDL_SCANCODE_WEBOS_RED 370
#endif
#ifndef SDL_SCANCODE_WEBOS_GREEN
#define SDL_SCANCODE_WEBOS_GREEN 371
#endif
#ifndef SDL_SCANCODE_WEBOS_YELLOW
#define SDL_SCANCODE_WEBOS_YELLOW 372
#endif
#ifndef SDL_SCANCODE_WEBOS_BLUE
#define SDL_SCANCODE_WEBOS_BLUE 373
#endif
#ifndef SDL_SCANCODE_WEBOS_GUIDE
#define SDL_SCANCODE_WEBOS_GUIDE 374
#endif
#ifndef SDL_SCANCODE_WEBOS_EXIT
#define SDL_SCANCODE_WEBOS_EXIT 375
#endif

#ifndef SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_BACK
#define SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_BACK "SDL_WEBOS_ACCESS_POLICY_KEYS_BACK"
#endif
#ifndef SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_EXIT
#define SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_EXIT "SDL_WEBOS_ACCESS_POLICY_KEYS_EXIT"
#endif
#ifndef SDL_HINT_WEBOS_CURSOR_SLEEP_TIME
#define SDL_HINT_WEBOS_CURSOR_SLEEP_TIME "SDL_WEBOS_CURSOR_SLEEP_TIME"
#endif

#endif
