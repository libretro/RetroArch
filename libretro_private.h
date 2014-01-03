/* Copyright (C) 2010-2014 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro API header (libretro_private.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBRETRO_PRIVATE_H__
#define LIBRETRO_PRIVATE_H__

// Private additions to libretro. No API/ABI stability guaranteed.

#include "libretro.h"

#define RETRO_ENVIRONMENT_SET_LIBRETRO_PATH (RETRO_ENVIRONMENT_PRIVATE | 0)
                                           // const char * --
                                           // Sets the absolute path for the libretro core pointed to. RETRO_ENVIRONMENT_EXEC will use the last libretro core set with this call.
                                           // Returns false if file for absolute path could not be found.
#define RETRO_ENVIRONMENT_EXEC             (RETRO_ENVIRONMENT_PRIVATE | 1)
                                           // const char * --
                                           // Requests that this core is deinitialized, and a new core is loaded.
                                           // The libretro core used is set with SET_LIBRETRO_PATH, and path to game is passed in _EXEC. NULL means no game.
#define RETRO_ENVIRONMENT_EXEC_ESCAPE     (RETRO_ENVIRONMENT_PRIVATE | 2)
                                           // const char * --
                                           // Requests that this core is deinitialized, and a new core is loaded. It also escapes the main loop the core is currently
                                           // bound to.
                                           // The libretro core used is set with SET_LIBRETRO_PATH, and path to game is passed in _EXEC. NULL means no game.


#endif

