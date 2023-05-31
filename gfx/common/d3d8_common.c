/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#define CINTERFACE

/* For Xbox we will just link statically
 * to Direct3D libraries instead. */

#if !defined(_XBOX) && defined(HAVE_DYLIB)
#define HAVE_DYNAMIC_D3D
#endif

#ifdef HAVE_DYNAMIC_D3D
#include <dynamic/dylib.h>
#endif

#include "../../verbosity.h"

#include "d3d8_common.h"

/* TODO/FIXME - static globals */
#ifdef HAVE_DYNAMIC_D3D
static dylib_t g_d3d8_dll;
static bool dylib_initialized = false;
#endif

typedef IDirect3D8 *(__stdcall *D3DCreate_t)(UINT);

static D3DCreate_t D3DCreate;

void *d3d8_create(void)
{
#ifdef _XBOX
   UINT ver = 0;
#else
   UINT ver = 220;
#endif
   return D3DCreate(ver);
}

bool d3d8_initialize_symbols(enum gfx_ctx_api api)
{
#ifdef HAVE_DYNAMIC_D3D
   if (dylib_initialized)
      return true;

#if defined(DEBUG) || defined(_DEBUG)
   if (!(g_d3d8_dll = dylib_load("d3d8d.dll")))
#endif
      g_d3d8_dll  = dylib_load("d3d8.dll");

   if (!g_d3d8_dll)
      return false;
#endif

#ifdef HAVE_DYNAMIC_D3D
   D3DCreate                = (D3DCreate_t)dylib_proc(g_d3d8_dll, "Direct3DCreate8");
#else
   D3DCreate                = Direct3DCreate8;
#endif

   if (!D3DCreate)
      goto error;

#ifdef HAVE_DYNAMIC_D3D
   dylib_initialized = true;
#endif

   return true;

error:
   d3d8_deinitialize_symbols();
   return false;
}

void d3d8_deinitialize_symbols(void)
{
#ifdef HAVE_DYNAMIC_D3D
   if (g_d3d8_dll)
      dylib_close(g_d3d8_dll);
   g_d3d8_dll         = NULL;
   dylib_initialized = false;
#endif
}
