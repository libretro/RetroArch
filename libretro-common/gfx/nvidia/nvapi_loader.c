/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (nvapi_loader.c).
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

#include <stddef.h>
#include <boolean.h>

#include <gfx/nvidia/nvapi_loader.h>

#if defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)
#include <windows.h>

/* NvAPI_Initialize's interface id (nvapi_interface.h, MIT) */
#define NVAPI_ID_INITIALIZE 0x0150e828

typedef nvapi_proc_t (__cdecl *nvapi_query_interface_t)(unsigned id);
typedef int          (__cdecl *nvapi_initialize_t)(void);

static nvapi_query_interface_t nvapi_query_interface;
static bool nvapi_probed;

nvapi_proc_t nvapi_query(unsigned id)
{
   if (!nvapi_probed)
   {
      HMODULE dll;
      nvapi_query_interface_t query;
      nvapi_initialize_t init;
      nvapi_probed = true;
#ifdef _WIN64
      dll   = LoadLibraryA("nvapi64.dll");
#else
      dll   = LoadLibraryA("nvapi.dll");
#endif
      if (!dll)
         return NULL;
      query = (nvapi_query_interface_t)GetProcAddress(dll, "nvapi_QueryInterface");
      init  = query ? (nvapi_initialize_t)query(NVAPI_ID_INITIALIZE) : NULL;
      if (!init || init() != NVAPI_OK)
      {
         FreeLibrary(dll);
         return NULL;
      }
      nvapi_query_interface = query;
   }
   return nvapi_query_interface ? nvapi_query_interface(id) : NULL;
}

#else

nvapi_proc_t nvapi_query(unsigned id)
{
   (void)id;
   return NULL;
}

#endif
