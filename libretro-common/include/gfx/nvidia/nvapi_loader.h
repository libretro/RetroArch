/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (nvapi_loader.h).
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

#ifndef _LIBRETRO_NVAPI_LOADER_H
#define _LIBRETRO_NVAPI_LOADER_H

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* NVIDIA's NVAPI, from the driver's own nvapi64.dll (nvapi.dll on
 * 32-bit), loaded the first time it is asked for and kept: NVAPI is
 * not to be unloaded once initialized. Never linked, so no SDK is
 * needed to build and a machine without it, or with another vendor's
 * GPU, loses nothing. Functions are found by their published
 * interface ids (nvapi_interface.h in github.com/NVIDIA/nvapi, MIT);
 * the caller declares the structures it uses, as the SDK's nvapi.h
 * lays them out, with the version word sizeof | (revision << 16).
 * One loader serves a frontend and a core alike: each module that
 * links it loads and initializes NVAPI once for itself.
 *
 * Windows only: elsewhere every query answers NULL. */

/* NvAPI_Status's success */
#define NVAPI_OK 0

typedef void (*nvapi_proc_t)(void);

/* The function with interface id @id, NVAPI initialized, to cast to
 * its type: NULL where there is no NVAPI (no library, NvAPI_Initialize
 * failed) or the driver has no such function. */
nvapi_proc_t nvapi_query(unsigned id);

RETRO_END_DECLS

#endif
