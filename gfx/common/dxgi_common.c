/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
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

#include <compat/strl.h>
#include <retro_environment.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "win32_common.h"
#include "dxgi_common.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../ui/ui_companion_driver.h"
#include "../../retroarch.h"
#include "../frontend/frontend_driver.h"

#ifdef __cplusplus
extern const GUID DECLSPEC_SELECTANY libretro_IID_IDXGIOutput6 = { 0x068346e8,0xaaec,
0x4b84, {0xad,0xd7,0x13,0x7f,0x51,0x3f,0x77,0xa1 } };
#else
const GUID DECLSPEC_SELECTANY libretro_IID_IDXGIOutput6 = { 0x068346e8,0xaaec,
0x4b84, {0xad,0xd7,0x13,0x7f,0x51,0x3f,0x77,0xa1 } };
#endif

#ifdef HAVE_DXGI_HDR
typedef enum hdr_root_constants
{
   HDR_ROOT_CONSTANTS_REFERENCE_WHITE_NITS = 0,
   HDR_ROOT_CONSTANTS_DISPLAY_CURVE,
   HDR_ROOT_CONSTANTS_COUNT
} hdr_root_constants_t;
#endif

#if defined(HAVE_DYLIB) && !defined(__WINRT__)
#include <dynamic/dylib.h>

HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** ppFactory)
{
   static HRESULT(WINAPI * fp)(REFIID, void**);
   static dylib_t dxgi_dll;
   if (!dxgi_dll)
      if (!(dxgi_dll = dylib_load("dxgi.dll")))
         return TYPE_E_CANTLOADLIBRARY;
   if (!fp)
      if (!(fp = (HRESULT(WINAPI*)(REFIID, void**))dylib_proc(dxgi_dll,
                  "CreateDXGIFactory1")))
         return TYPE_E_DLLFUNCTIONNOTFOUND;
   return fp(riid, ppFactory);
}
#endif

DXGI_FORMAT* dxgi_get_format_fallback_list(DXGI_FORMAT format)
{
   switch ((unsigned)format)
   {
      case DXGI_FORMAT_R32G32B32A32_FLOAT:
      {
         static DXGI_FORMAT formats[] = { DXGI_FORMAT_R32G32B32A32_FLOAT,
                                          DXGI_FORMAT_R16G16B16A16_FLOAT,
                                          DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
                                          DXGI_FORMAT_UNKNOWN };
         return formats;
      }
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
      {
         static DXGI_FORMAT formats[] = { DXGI_FORMAT_R16G16B16A16_FLOAT,
                                          DXGI_FORMAT_R32G32B32A32_FLOAT,
                                          DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT,
                                          DXGI_FORMAT_UNKNOWN };
         return formats;
      }
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      {
         static DXGI_FORMAT formats[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM,
                                          DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_UNKNOWN };
         return formats;
      }
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      {
         static DXGI_FORMAT formats[] = { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                                          DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM,
                                          DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_UNKNOWN };
         return formats;
      }
      case DXGI_FORMAT_B8G8R8A8_UNORM:
      {
         static DXGI_FORMAT formats[] = { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM,
                                          DXGI_FORMAT_UNKNOWN };
         return formats;
      }
      case DXGI_FORMAT_B8G8R8X8_UNORM:
      {
         static DXGI_FORMAT formats[] = { DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM,
                                          DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN };
         return formats;
      }
      case DXGI_FORMAT_B5G6R5_UNORM:
      {
         static DXGI_FORMAT formats[] = { DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM,
                                          DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM,
                                          DXGI_FORMAT_UNKNOWN };
         return formats;
      }
      case DXGI_FORMAT_EX_A4R4G4B4_UNORM:
      case DXGI_FORMAT_B4G4R4A4_UNORM:
      {
         static DXGI_FORMAT formats[] = { DXGI_FORMAT_B4G4R4A4_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM,
                                          DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN };
         return formats;
      }
      case DXGI_FORMAT_A8_UNORM:
      {
         static DXGI_FORMAT formats[] = { DXGI_FORMAT_A8_UNORM,       DXGI_FORMAT_R8_UNORM,
                                          DXGI_FORMAT_R8G8_UNORM,     DXGI_FORMAT_R8G8B8A8_UNORM,
                                          DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN };
         return formats;
      }
      case DXGI_FORMAT_R8_UNORM:
      {
         static DXGI_FORMAT formats[] = { DXGI_FORMAT_R8_UNORM,       DXGI_FORMAT_A8_UNORM,
                                          DXGI_FORMAT_R8G8_UNORM,     DXGI_FORMAT_R8G8B8A8_UNORM,
                                          DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN };
         return formats;
      }
      default:
         break;
   }
   return NULL;
}


#ifdef _MSC_VER
#pragma warning(disable : 4293)
#endif
void dxgi_copy(
      int         width,
      int         height,
      DXGI_FORMAT src_format,
      int         src_pitch,
      const void* src_data,
      DXGI_FORMAT dst_format,
      int         dst_pitch,
      void*       dst_data)
{
   int i, j;

   switch ((unsigned)src_format)
   {
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      {
         switch ((unsigned)dst_format)
         {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            {
               {
                  const UINT8* in  = (const UINT8*)src_data;
                  UINT8*       out = (UINT8*)dst_data;
                  for (i = 0; i < height; i++)
                  {
                     memcpy(out, in, width * sizeof(UINT32));
                     in  += src_pitch ? (int)src_pitch  : (int)(width * sizeof(UINT32));
                     out += dst_pitch ? (int)dst_pitch  : (int)(width * sizeof(UINT32));
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8X8_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        g = (src_val >> 8) & 255;
                        b = (src_val >> 16) & 255;
                        a = (src_val >> 24) & 255;
                        a = a >> 8;
                        *dst_ptr++ = (r << 16) | (g << 8) | (b << 0);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_A8_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        r = r >> 8;
                        g = (src_val >> 8) & 255;
                        g = g >> 8;
                        b = (src_val >> 16) & 255;
                        b = b >> 8;
                        a = (src_val >> 24) & 255;
                        *dst_ptr++ = (a << 0);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_R8_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        g = (src_val >> 8) & 255;
                        g = g >> 8;
                        b = (src_val >> 16) & 255;
                        b = b >> 8;
                        a = (src_val >> 24) & 255;
                        a = a >> 8;
                        *dst_ptr++ = (r << 0);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G6R5_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        r = r >> 3;
                        g = (src_val >> 8) & 255;
                        g = g >> 2;
                        b = (src_val >> 16) & 255;
                        b = b >> 3;
                        a = (src_val >> 24) & 255;
                        a = a >> 8;
                        *dst_ptr++ = (r << 11) | (g << 5) | (b << 0);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G5R5A1_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        r = r >> 3;
                        g = (src_val >> 8) & 255;
                        g = g >> 3;
                        b = (src_val >> 16) & 255;
                        b = b >> 3;
                        a = (src_val >> 24) & 255;
                        a = a >> 7;
                        *dst_ptr++ = (r << 10) | (g << 5) | (b << 0) | (a << 11);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B4G4R4A4_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        r = r >> 4;
                        g = (src_val >> 8) & 255;
                        g = g >> 4;
                        b = (src_val >> 16) & 255;
                        b = b >> 4;
                        a = (src_val >> 24) & 255;
                        a = a >> 4;
                        *dst_ptr++ = (r << 8) | (g << 4) | (b << 0) | (a << 12);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        g = (src_val >> 8) & 255;
                        b = (src_val >> 16) & 255;
                        a = (src_val >> 24) & 255;
                        *dst_ptr++ = (r << 16) | (g << 8) | (b << 0) | (a << 24);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_EX_A4R4G4B4_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        r = r >> 4;
                        g = (src_val >> 8) & 255;
                        g = g >> 4;
                        b = (src_val >> 16) & 255;
                        b = b >> 4;
                        a = (src_val >> 24) & 255;
                        a = a >> 4;
                        *dst_ptr++ = (r << 4) | (g << 8) | (b << 12) | (a << 0);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            default:
               break;
         }
         break;
      }
      case DXGI_FORMAT_B8G8R8X8_UNORM:
      {
         switch ((unsigned)dst_format)
         {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        g = (src_val >> 8) & 255;
                        b = (src_val >> 0) & 255;
                        *dst_ptr++ = (r << 0) | (g << 8) | (b << 16) | (255 << 24);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8X8_UNORM:
            {
               {
                  const UINT8* in  = (const UINT8*)src_data;
                  UINT8*       out = (UINT8*)dst_data;
                  for (i = 0; i < height; i++)
                  {
                     memcpy(out, in, width * sizeof(UINT32));
                     in  += src_pitch ? (int)src_pitch  : (int)(width * sizeof(UINT32));
                     out += dst_pitch ? (int)dst_pitch  : (int)(width * sizeof(UINT32));
                  }
               }
               break;
            }
            case DXGI_FORMAT_A8_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        r = r >> 8;
                        g = (src_val >> 8) & 255;
                        g = g >> 8;
                        b = (src_val >> 0) & 255;
                        b = b >> 8;
                        *dst_ptr++ = (255 << 0);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_R8_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        g = (src_val >> 8) & 255;
                        g = g >> 8;
                        b = (src_val >> 0) & 255;
                        b = b >> 8;
                        *dst_ptr++ = (r << 0);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G6R5_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        r = r >> 3;
                        g = (src_val >> 8) & 255;
                        g = g >> 2;
                        b = (src_val >> 0) & 255;
                        b = b >> 3;
                        *dst_ptr++ = (r << 11) | (g << 5) | (b << 0);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G5R5A1_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        r = r >> 3;
                        g = (src_val >> 8) & 255;
                        g = g >> 3;
                        b = (src_val >> 0) & 255;
                        b = b >> 3;
                        *dst_ptr++ = (r << 10) | (g << 5) | (b << 0) | (1 << 11);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B4G4R4A4_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        r = r >> 4;
                        g = (src_val >> 8) & 255;
                        g = g >> 4;
                        b = (src_val >> 0) & 255;
                        b = b >> 4;
                        *dst_ptr++ = (r << 8) | (g << 4) | (b << 0) | (15 << 12);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        g = (src_val >> 8) & 255;
                        b = (src_val >> 0) & 255;
                        *dst_ptr++ = (r << 16) | (g << 8) | (b << 0) | (255 << 24);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_EX_A4R4G4B4_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        r = r >> 4;
                        g = (src_val >> 8) & 255;
                        g = g >> 4;
                        b = (src_val >> 0) & 255;
                        b = b >> 4;
                        *dst_ptr++ = (r << 4) | (g << 8) | (b << 12) | (15 << 0);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            default:
               break;
         }
         break;
      }
      case DXGI_FORMAT_A8_UNORM:
      {
         switch ((unsigned)dst_format)
         {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        a = (src_val >> 0) & 255;
                        *dst_ptr++ = (0 << 0) | (0 << 8) | (0 << 16) | (a << 24);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8X8_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        a = (src_val >> 0) & 255;
                        a = a >> 8;
                        *dst_ptr++ = (0 << 16) | (0 << 8) | (0 << 0);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_A8_UNORM:
            {
               {
                  const UINT8* in  = (const UINT8*)src_data;
                  UINT8*       out = (UINT8*)dst_data;
                  for (i = 0; i < height; i++)
                  {
                     memcpy(out, in, width * sizeof(UINT8));
                     in  += src_pitch ? (int)src_pitch  : (int)(width * sizeof(UINT8));
                     out += dst_pitch ? (int)dst_pitch  : (int)(width * sizeof(UINT8));
                  }
               }
               break;
            }
            case DXGI_FORMAT_R8_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        a = (src_val >> 0) & 255;
                        a = a >> 8;
                        *dst_ptr++ = (0 << 0);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G6R5_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        a = (src_val >> 0) & 255;
                        a = a >> 8;
                        *dst_ptr++ = (0 << 11) | (0 << 5) | (0 << 0);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G5R5A1_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        a = (src_val >> 0) & 255;
                        a = a >> 7;
                        *dst_ptr++ = (0 << 10) | (0 << 5) | (0 << 0) | (a << 11);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B4G4R4A4_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        a = (src_val >> 0) & 255;
                        a = a >> 4;
                        *dst_ptr++ = (0 << 8) | (0 << 4) | (0 << 0) | (a << 12);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        a = (src_val >> 0) & 255;
                        *dst_ptr++ = (0 << 16) | (0 << 8) | (0 << 0) | (a << 24);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_EX_A4R4G4B4_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        a = (src_val >> 0) & 255;
                        a = a >> 4;
                        *dst_ptr++ = (0 << 4) | (0 << 8) | (0 << 12) | (a << 0);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            default:
               break;
         }
         break;
      }
      case DXGI_FORMAT_R8_UNORM:
      {
         switch ((unsigned)dst_format)
         {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        *dst_ptr++ = (r << 0) | (0 << 8) | (0 << 16) | (255 << 24);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8X8_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        *dst_ptr++ = (r << 16) | (0 << 8) | (0 << 0);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_A8_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        r = r >> 8;
                        *dst_ptr++ = (255 << 0);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_R8_UNORM:
            {
               {
                  const UINT8* in  = (const UINT8*)src_data;
                  UINT8*       out = (UINT8*)dst_data;
                  for (i = 0; i < height; i++)
                  {
                     memcpy(out, in, width * sizeof(UINT8));
                     in  += src_pitch ? (int)src_pitch  : (int)(width * sizeof(UINT8));
                     out += dst_pitch ? (int)dst_pitch  : (int)(width * sizeof(UINT8));
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G6R5_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        r = r >> 3;
                        *dst_ptr++ = (r << 11) | (0 << 5) | (0 << 0);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G5R5A1_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        r = r >> 3;
                        *dst_ptr++ = (r << 10) | (0 << 5) | (0 << 0) | (1 << 11);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B4G4R4A4_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        r = r >> 4;
                        *dst_ptr++ = (r << 8) | (0 << 4) | (0 << 0) | (15 << 12);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        *dst_ptr++ = (r << 16) | (0 << 8) | (0 << 0) | (255 << 24);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_EX_A4R4G4B4_UNORM:
            {
               {
                  const UINT8* src_ptr = (const UINT8*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT8 src_val = *src_ptr++;
                        r = (src_val >> 0) & 255;
                        r = r >> 4;
                        *dst_ptr++ = (r << 4) | (0 << 8) | (0 << 12) | (15 << 0);
                     }
                     src_ptr = (UINT8*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            default:
               break;
         }
         break;
      }
      case DXGI_FORMAT_B5G6R5_UNORM:
      {
         switch ((unsigned)dst_format)
         {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 11) & 31;
                        r = (r << 3) | (r >> 2);
                        g = (src_val >> 5) & 63;
                        g = (g << 2) | (g >> 4);
                        b = (src_val >> 0) & 31;
                        b = (b << 3) | (b >> 2);
                        *dst_ptr++ = (r << 0) | (g << 8) | (b << 16) | (255 << 24);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8X8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 11) & 31;
                        r = (r << 3) | (r >> 2);
                        g = (src_val >> 5) & 63;
                        g = (g << 2) | (g >> 4);
                        b = (src_val >> 0) & 31;
                        b = (b << 3) | (b >> 2);
                        *dst_ptr++ = (r << 16) | (g << 8) | (b << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_A8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 11) & 31;
                        r = r >> 5;
                        g = (src_val >> 5) & 63;
                        g = g >> 6;
                        b = (src_val >> 0) & 31;
                        b = b >> 5;
                        *dst_ptr++ = (255 << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_R8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 11) & 31;
                        r = (r << 3) | (r >> 2);
                        g = (src_val >> 5) & 63;
                        g = g >> 6;
                        b = (src_val >> 0) & 31;
                        b = b >> 5;
                        *dst_ptr++ = (r << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G6R5_UNORM:
            {
               {
                  const UINT8* in  = (const UINT8*)src_data;
                  UINT8*       out = (UINT8*)dst_data;
                  for (i = 0; i < height; i++)
                  {
                     memcpy(out, in, width * sizeof(UINT16));
                     in  += src_pitch ? (int)src_pitch  : (int)(width * sizeof(UINT16));
                     out += dst_pitch ? (int)dst_pitch  : (int)(width * sizeof(UINT16));
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G5R5A1_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 11) & 31;
                        g = (src_val >> 5) & 63;
                        g = g >> 1;
                        b = (src_val >> 0) & 31;
                        *dst_ptr++ = (r << 10) | (g << 5) | (b << 0) | (1 << 11);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B4G4R4A4_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 11) & 31;
                        r = r >> 1;
                        g = (src_val >> 5) & 63;
                        g = g >> 2;
                        b = (src_val >> 0) & 31;
                        b = b >> 1;
                        *dst_ptr++ = (r << 8) | (g << 4) | (b << 0) | (15 << 12);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 11) & 31;
                        r = (r << 3) | (r >> 2);
                        g = (src_val >> 5) & 63;
                        g = (g << 2) | (g >> 4);
                        b = (src_val >> 0) & 31;
                        b = (b << 3) | (b >> 2);
                        *dst_ptr++ = (r << 16) | (g << 8) | (b << 0) | (255 << 24);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_EX_A4R4G4B4_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 11) & 31;
                        r = r >> 1;
                        g = (src_val >> 5) & 63;
                        g = g >> 2;
                        b = (src_val >> 0) & 31;
                        b = b >> 1;
                        *dst_ptr++ = (r << 4) | (g << 8) | (b << 12) | (15 << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            default:
               break;
         }
         break;
      }
      case DXGI_FORMAT_B5G5R5A1_UNORM:
      {
         switch ((unsigned)dst_format)
         {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 10) & 31;
                        r = (r << 3) | (r >> 2);
                        g = (src_val >> 5) & 31;
                        g = (g << 3) | (g >> 2);
                        b = (src_val >> 0) & 31;
                        b = (b << 3) | (b >> 2);
                        a = (src_val >> 11) & 1;
                        a = (a << 7) | (a >> 0);
                        *dst_ptr++ = (r << 0) | (g << 8) | (b << 16) | (a << 24);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8X8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 10) & 31;
                        r = (r << 3) | (r >> 2);
                        g = (src_val >> 5) & 31;
                        g = (g << 3) | (g >> 2);
                        b = (src_val >> 0) & 31;
                        b = (b << 3) | (b >> 2);
                        a = (src_val >> 11) & 1;
                        a = a >> 1;
                        *dst_ptr++ = (r << 16) | (g << 8) | (b << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_A8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 10) & 31;
                        r = r >> 5;
                        g = (src_val >> 5) & 31;
                        g = g >> 5;
                        b = (src_val >> 0) & 31;
                        b = b >> 5;
                        a = (src_val >> 11) & 1;
                        a = (a << 7) | (a >> 0);
                        *dst_ptr++ = (a << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_R8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 10) & 31;
                        r = (r << 3) | (r >> 2);
                        g = (src_val >> 5) & 31;
                        g = g >> 5;
                        b = (src_val >> 0) & 31;
                        b = b >> 5;
                        a = (src_val >> 11) & 1;
                        a = a >> 1;
                        *dst_ptr++ = (r << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G6R5_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 10) & 31;
                        g = (src_val >> 5) & 31;
                        g = (g << 1) | (g >> 4);
                        b = (src_val >> 0) & 31;
                        a = (src_val >> 11) & 1;
                        a = a >> 1;
                        *dst_ptr++ = (r << 11) | (g << 5) | (b << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G5R5A1_UNORM:
            {
               {
                  const UINT8* in  = (const UINT8*)src_data;
                  UINT8*       out = (UINT8*)dst_data;
                  for (i = 0; i < height; i++)
                  {
                     memcpy(out, in, width * sizeof(UINT16));
                     in  += src_pitch ? (int)src_pitch  : (int)(width * sizeof(UINT16));
                     out += dst_pitch ? (int)dst_pitch  : (int)(width * sizeof(UINT16));
                  }
               }
               break;
            }
            case DXGI_FORMAT_B4G4R4A4_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 10) & 31;
                        r = r >> 1;
                        g = (src_val >> 5) & 31;
                        g = g >> 1;
                        b = (src_val >> 0) & 31;
                        b = b >> 1;
                        a = (src_val >> 11) & 1;
                        a = (a << 3) | (a >> 0);
                        *dst_ptr++ = (r << 8) | (g << 4) | (b << 0) | (a << 12);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 10) & 31;
                        r = (r << 3) | (r >> 2);
                        g = (src_val >> 5) & 31;
                        g = (g << 3) | (g >> 2);
                        b = (src_val >> 0) & 31;
                        b = (b << 3) | (b >> 2);
                        a = (src_val >> 11) & 1;
                        a = (a << 7) | (a >> 0);
                        *dst_ptr++ = (r << 16) | (g << 8) | (b << 0) | (a << 24);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_EX_A4R4G4B4_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 10) & 31;
                        r = r >> 1;
                        g = (src_val >> 5) & 31;
                        g = g >> 1;
                        b = (src_val >> 0) & 31;
                        b = b >> 1;
                        a = (src_val >> 11) & 1;
                        a = (a << 3) | (a >> 0);
                        *dst_ptr++ = (r << 4) | (g << 8) | (b << 12) | (a << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            default:
               break;
         }
         break;
      }
      case DXGI_FORMAT_B4G4R4A4_UNORM:
      {
         switch ((unsigned)dst_format)
         {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 8) & 15;
                        r = (r << 4) | (r >> 0);
                        g = (src_val >> 4) & 15;
                        g = (g << 4) | (g >> 0);
                        b = (src_val >> 0) & 15;
                        b = (b << 4) | (b >> 0);
                        a = (src_val >> 12) & 15;
                        a = (a << 4) | (a >> 0);
                        *dst_ptr++ = (r << 0) | (g << 8) | (b << 16) | (a << 24);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8X8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 8) & 15;
                        r = (r << 4) | (r >> 0);
                        g = (src_val >> 4) & 15;
                        g = (g << 4) | (g >> 0);
                        b = (src_val >> 0) & 15;
                        b = (b << 4) | (b >> 0);
                        a = (src_val >> 12) & 15;
                        a = a >> 4;
                        *dst_ptr++ = (r << 16) | (g << 8) | (b << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_A8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 8) & 15;
                        r = r >> 4;
                        g = (src_val >> 4) & 15;
                        g = g >> 4;
                        b = (src_val >> 0) & 15;
                        b = b >> 4;
                        a = (src_val >> 12) & 15;
                        a = (a << 4) | (a >> 0);
                        *dst_ptr++ = (a << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_R8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 8) & 15;
                        r = (r << 4) | (r >> 0);
                        g = (src_val >> 4) & 15;
                        g = g >> 4;
                        b = (src_val >> 0) & 15;
                        b = b >> 4;
                        a = (src_val >> 12) & 15;
                        a = a >> 4;
                        *dst_ptr++ = (r << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G6R5_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 8) & 15;
                        r = (r << 1) | (r >> 3);
                        g = (src_val >> 4) & 15;
                        g = (g << 2) | (g >> 2);
                        b = (src_val >> 0) & 15;
                        b = (b << 1) | (b >> 3);
                        a = (src_val >> 12) & 15;
                        a = a >> 4;
                        *dst_ptr++ = (r << 11) | (g << 5) | (b << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G5R5A1_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 8) & 15;
                        r = (r << 1) | (r >> 3);
                        g = (src_val >> 4) & 15;
                        g = (g << 1) | (g >> 3);
                        b = (src_val >> 0) & 15;
                        b = (b << 1) | (b >> 3);
                        a = (src_val >> 12) & 15;
                        a = a >> 3;
                        *dst_ptr++ = (r << 10) | (g << 5) | (b << 0) | (a << 11);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B4G4R4A4_UNORM:
            {
               {
                  const UINT8* in  = (const UINT8*)src_data;
                  UINT8*       out = (UINT8*)dst_data;
                  for (i = 0; i < height; i++)
                  {
                     memcpy(out, in, width * sizeof(UINT16));
                     in  += src_pitch ? (int)src_pitch  : (int)(width * sizeof(UINT16));
                     out += dst_pitch ? (int)dst_pitch  : (int)(width * sizeof(UINT16));
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 8) & 15;
                        r = (r << 4) | (r >> 0);
                        g = (src_val >> 4) & 15;
                        g = (g << 4) | (g >> 0);
                        b = (src_val >> 0) & 15;
                        b = (b << 4) | (b >> 0);
                        a = (src_val >> 12) & 15;
                        a = (a << 4) | (a >> 0);
                        *dst_ptr++ = (r << 16) | (g << 8) | (b << 0) | (a << 24);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_EX_A4R4G4B4_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 8) & 15;
                        g = (src_val >> 4) & 15;
                        b = (src_val >> 0) & 15;
                        a = (src_val >> 12) & 15;
                        *dst_ptr++ = (r << 4) | (g << 8) | (b << 12) | (a << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            default:
               break;
         }
         break;
      }
      case DXGI_FORMAT_B8G8R8A8_UNORM:
      {
         switch ((unsigned)dst_format)
         {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        g = (src_val >> 8) & 255;
                        b = (src_val >> 0) & 255;
                        a = (src_val >> 24) & 255;
                        *dst_ptr++ = (r << 0) | (g << 8) | (b << 16) | (a << 24);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8X8_UNORM:
            {
               {
                  const UINT8* in  = (const UINT8*)src_data;
                  UINT8*       out = (UINT8*)dst_data;
                  for (i = 0; i < height; i++)
                  {
                     memcpy(out, in, width * sizeof(UINT32));
                     in  += src_pitch ? (int)src_pitch  : (int)(width * sizeof(UINT32));
                     out += dst_pitch ? (int)dst_pitch  : (int)(width * sizeof(UINT32));
                  }
               }
               break;
            }
            case DXGI_FORMAT_A8_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        r = r >> 8;
                        g = (src_val >> 8) & 255;
                        g = g >> 8;
                        b = (src_val >> 0) & 255;
                        b = b >> 8;
                        a = (src_val >> 24) & 255;
                        *dst_ptr++ = (a << 0);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_R8_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        g = (src_val >> 8) & 255;
                        g = g >> 8;
                        b = (src_val >> 0) & 255;
                        b = b >> 8;
                        a = (src_val >> 24) & 255;
                        a = a >> 8;
                        *dst_ptr++ = (r << 0);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G6R5_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        r = r >> 3;
                        g = (src_val >> 8) & 255;
                        g = g >> 2;
                        b = (src_val >> 0) & 255;
                        b = b >> 3;
                        a = (src_val >> 24) & 255;
                        a = a >> 8;
                        *dst_ptr++ = (r << 11) | (g << 5) | (b << 0);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G5R5A1_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        r = r >> 3;
                        g = (src_val >> 8) & 255;
                        g = g >> 3;
                        b = (src_val >> 0) & 255;
                        b = b >> 3;
                        a = (src_val >> 24) & 255;
                        a = a >> 7;
                        *dst_ptr++ = (r << 10) | (g << 5) | (b << 0) | (a << 11);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B4G4R4A4_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        r = r >> 4;
                        g = (src_val >> 8) & 255;
                        g = g >> 4;
                        b = (src_val >> 0) & 255;
                        b = b >> 4;
                        a = (src_val >> 24) & 255;
                        a = a >> 4;
                        *dst_ptr++ = (r << 8) | (g << 4) | (b << 0) | (a << 12);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            {
               {
                  const UINT8* in  = (const UINT8*)src_data;
                  UINT8*       out = (UINT8*)dst_data;
                  for (i = 0; i < height; i++)
                  {
                     memcpy(out, in, width * sizeof(UINT32));
                     in  += src_pitch ? (int)src_pitch  : (int)(width * sizeof(UINT32));
                     out += dst_pitch ? (int)dst_pitch  : (int)(width * sizeof(UINT32));
                  }
               }
               break;
            }
            case DXGI_FORMAT_EX_A4R4G4B4_UNORM:
            {
               {
                  const UINT32* src_ptr = (const UINT32*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT32 src_val = *src_ptr++;
                        r = (src_val >> 16) & 255;
                        r = r >> 4;
                        g = (src_val >> 8) & 255;
                        g = g >> 4;
                        b = (src_val >> 0) & 255;
                        b = b >> 4;
                        a = (src_val >> 24) & 255;
                        a = a >> 4;
                        *dst_ptr++ = (r << 4) | (g << 8) | (b << 12) | (a << 0);
                     }
                     src_ptr = (UINT32*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            default:
               break;
         }
         break;
      }
      case DXGI_FORMAT_EX_A4R4G4B4_UNORM:
      {
         switch ((unsigned)dst_format)
         {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 4) & 15;
                        r = (r << 4) | (r >> 0);
                        g = (src_val >> 8) & 15;
                        g = (g << 4) | (g >> 0);
                        b = (src_val >> 12) & 15;
                        b = (b << 4) | (b >> 0);
                        a = (src_val >> 0) & 15;
                        a = (a << 4) | (a >> 0);
                        *dst_ptr++ = (r << 0) | (g << 8) | (b << 16) | (a << 24);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8X8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 4) & 15;
                        r = (r << 4) | (r >> 0);
                        g = (src_val >> 8) & 15;
                        g = (g << 4) | (g >> 0);
                        b = (src_val >> 12) & 15;
                        b = (b << 4) | (b >> 0);
                        a = (src_val >> 0) & 15;
                        a = a >> 4;
                        *dst_ptr++ = (r << 16) | (g << 8) | (b << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_A8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 4) & 15;
                        r = r >> 4;
                        g = (src_val >> 8) & 15;
                        g = g >> 4;
                        b = (src_val >> 12) & 15;
                        b = b >> 4;
                        a = (src_val >> 0) & 15;
                        a = (a << 4) | (a >> 0);
                        *dst_ptr++ = (a << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_R8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT8*       dst_ptr = (UINT8*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 4) & 15;
                        r = (r << 4) | (r >> 0);
                        g = (src_val >> 8) & 15;
                        g = g >> 4;
                        b = (src_val >> 12) & 15;
                        b = b >> 4;
                        a = (src_val >> 0) & 15;
                        a = a >> 4;
                        *dst_ptr++ = (r << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT8*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G6R5_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 4) & 15;
                        r = (r << 1) | (r >> 3);
                        g = (src_val >> 8) & 15;
                        g = (g << 2) | (g >> 2);
                        b = (src_val >> 12) & 15;
                        b = (b << 1) | (b >> 3);
                        a = (src_val >> 0) & 15;
                        a = a >> 4;
                        *dst_ptr++ = (r << 11) | (g << 5) | (b << 0);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B5G5R5A1_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 4) & 15;
                        r = (r << 1) | (r >> 3);
                        g = (src_val >> 8) & 15;
                        g = (g << 1) | (g >> 3);
                        b = (src_val >> 12) & 15;
                        b = (b << 1) | (b >> 3);
                        a = (src_val >> 0) & 15;
                        a = a >> 3;
                        *dst_ptr++ = (r << 10) | (g << 5) | (b << 0) | (a << 11);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B4G4R4A4_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT16*       dst_ptr = (UINT16*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 4) & 15;
                        g = (src_val >> 8) & 15;
                        b = (src_val >> 12) & 15;
                        a = (src_val >> 0) & 15;
                        *dst_ptr++ = (r << 8) | (g << 4) | (b << 0) | (a << 12);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT16*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            {
               {
                  const UINT16* src_ptr = (const UINT16*)src_data;
                  UINT32*       dst_ptr = (UINT32*)dst_data;
                  int sp = src_pitch;
                  int dp = dst_pitch;
                  if (sp)
                     sp -= width * sizeof(*src_ptr);
                  if (dp)
                     dp -= width * sizeof(*dst_ptr);
                  for (i = 0; i < height; i++)
                  {
                     for (j = 0; j < width; j++)
                     {
                        unsigned r = 0, g = 0, b = 0, a = 0;
                        UINT16 src_val = *src_ptr++;
                        r = (src_val >> 4) & 15;
                        r = (r << 4) | (r >> 0);
                        g = (src_val >> 8) & 15;
                        g = (g << 4) | (g >> 0);
                        b = (src_val >> 12) & 15;
                        b = (b << 4) | (b >> 0);
                        a = (src_val >> 0) & 15;
                        a = (a << 4) | (a >> 0);
                        *dst_ptr++ = (r << 16) | (g << 8) | (b << 0) | (a << 24);
                     }
                     src_ptr = (UINT16*)((UINT8*)src_ptr + sp);
                     dst_ptr = (UINT32*)((UINT8*)dst_ptr + dp);
                  }
               }
               break;
            }
            case DXGI_FORMAT_EX_A4R4G4B4_UNORM:
            {
               {
                  const UINT8* in  = (const UINT8*)src_data;
                  UINT8*       out = (UINT8*)dst_data;
                  for (i = 0; i < height; i++)
                  {
                     memcpy(out, in, width * sizeof(UINT16));
                     in  += src_pitch ? (int)src_pitch  : (int)(width * sizeof(UINT16));
                     out += dst_pitch ? (int)dst_pitch  : (int)(width * sizeof(UINT16));
                  }
               }
               break;
            }
            default:
               break;
         }
         break;
      }

      default:
         break;
   }
}

#ifdef _MSC_VER
#pragma warning(default : 4293)
#endif

DXGI_FORMAT glslang_format_to_dxgi(glslang_format fmt)
{
   switch (fmt)
   {
      case SLANG_FORMAT_R8_UNORM:
         return DXGI_FORMAT_R8_UNORM;
      case SLANG_FORMAT_R8_SINT:
         return DXGI_FORMAT_R8_SINT;
      case SLANG_FORMAT_R8_UINT:
         return DXGI_FORMAT_R8_UINT;
      case SLANG_FORMAT_R8G8_UNORM:
         return DXGI_FORMAT_R8G8_UNORM;
      case SLANG_FORMAT_R8G8_SINT:
         return DXGI_FORMAT_R8G8_SINT;
      case SLANG_FORMAT_R8G8_UINT:
         return DXGI_FORMAT_R8G8_UINT;
      case SLANG_FORMAT_R8G8B8A8_UNORM:
         return DXGI_FORMAT_R8G8B8A8_UNORM;
      case SLANG_FORMAT_R8G8B8A8_SINT:
         return DXGI_FORMAT_R8G8B8A8_SINT;
      case SLANG_FORMAT_R8G8B8A8_UINT:
         return DXGI_FORMAT_R8G8B8A8_UINT;
      case SLANG_FORMAT_R8G8B8A8_SRGB:
         return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
      case SLANG_FORMAT_A2B10G10R10_UNORM_PACK32:
         return DXGI_FORMAT_R10G10B10A2_UNORM;
      case SLANG_FORMAT_A2B10G10R10_UINT_PACK32:
         return DXGI_FORMAT_R10G10B10A2_UNORM;
      case SLANG_FORMAT_R16_UINT:
         return DXGI_FORMAT_R16_UINT;
      case SLANG_FORMAT_R16_SINT:
         return DXGI_FORMAT_R16_SINT;
      case SLANG_FORMAT_R16_SFLOAT:
         return DXGI_FORMAT_R16_FLOAT;
      case SLANG_FORMAT_R16G16_UINT:
         return DXGI_FORMAT_R16G16_UINT;
      case SLANG_FORMAT_R16G16_SINT:
         return DXGI_FORMAT_R16G16_SINT;
      case SLANG_FORMAT_R16G16_SFLOAT:
         return DXGI_FORMAT_R16G16_FLOAT;
      case SLANG_FORMAT_R16G16B16A16_UINT:
         return DXGI_FORMAT_R16G16B16A16_UINT;
      case SLANG_FORMAT_R16G16B16A16_SINT:
         return DXGI_FORMAT_R16G16B16A16_SINT;
      case SLANG_FORMAT_R16G16B16A16_SFLOAT:
         return DXGI_FORMAT_R16G16B16A16_FLOAT;
      case SLANG_FORMAT_R32_UINT:
         return DXGI_FORMAT_R32_UINT;
      case SLANG_FORMAT_R32_SINT:
         return DXGI_FORMAT_R32_SINT;
      case SLANG_FORMAT_R32_SFLOAT:
         return DXGI_FORMAT_R32_FLOAT;
      case SLANG_FORMAT_R32G32_UINT:
         return DXGI_FORMAT_R32G32_UINT;
      case SLANG_FORMAT_R32G32_SINT:
         return DXGI_FORMAT_R32G32_SINT;
      case SLANG_FORMAT_R32G32_SFLOAT:
         return DXGI_FORMAT_R32G32_FLOAT;
      case SLANG_FORMAT_R32G32B32A32_UINT:
         return DXGI_FORMAT_R32G32B32A32_UINT;
      case SLANG_FORMAT_R32G32B32A32_SINT:
         return DXGI_FORMAT_R32G32B32A32_SINT;
      case SLANG_FORMAT_R32G32B32A32_SFLOAT:
         return DXGI_FORMAT_R32G32B32A32_FLOAT;
      case SLANG_FORMAT_UNKNOWN:
      default:
         break;
   }

   return DXGI_FORMAT_UNKNOWN;
}

#ifdef HAVE_DXGI_HDR
typedef struct display_chromaticities
{
   float red_x;
   float red_y;
   float green_x;
   float green_y;
   float blue_x;
   float blue_y;
   float white_x;
   float white_y;
} display_chromaticities_t;

inline static int dxgi_compute_intersection_area(
      int ax1, int ay1, int ax2, int ay2,
      int bx1, int by1, int bx2, int by2)
{
    return   MAX(0, MIN(ax2, bx2) -
             MAX(ax1, bx1))
           * MAX(0, MIN(ay2, by2) - MAX(ay1, by1));
}

#ifdef __WINRT__
bool dxgi_check_display_hdr_support(DXGIFactory2 factory, HWND hwnd)
#else
bool dxgi_check_display_hdr_support(DXGIFactory1 factory, HWND hwnd)
#endif
{
   DXGIOutput6 output6       = NULL;
   DXGIOutput best_output    = NULL;
   DXGIOutput current_output = NULL;
   DXGIAdapter dxgi_adapter  = NULL;
   UINT adapter_idx          = 0;
   bool supported            = false;
   float best_intersect_area = -1;

#ifdef __WINRT__
#ifdef __cplusplus
   if (!factory->IsCurrent())
#else
   if (!factory->lpVtbl->IsCurrent(factory))
#endif
   {
      if (FAILED(DXGICreateFactory2(&factory)))
      {
         RARCH_ERR("[DXGI] Failed to create DXGI factory.\n");
         return false;
      }
   }
#else
#ifdef __cplusplus
   if (!factory->IsCurrent())
#else
   if (!factory->lpVtbl->IsCurrent(factory))
#endif
   {
      if (FAILED(DXGICreateFactory1(&factory)))
      {
         RARCH_ERR("[DXGI] Failed to create DXGI factory.\n");
         return false;
      }
   }
#endif

   /* Enumerate ALL adapters to find the output the window is on.
    * On multi-GPU systems (e.g. Nvidia Optimus) the display may be
    * connected to an adapter other than index 0. */
#ifdef __cplusplus
   while (SUCCEEDED(factory->EnumAdapters1(adapter_idx, &dxgi_adapter)))
#else
   while (SUCCEEDED(factory->lpVtbl->EnumAdapters1(factory, adapter_idx, &dxgi_adapter)))
#endif
   {
      UINT i = 0;
#ifdef __cplusplus
      while (  dxgi_adapter->EnumOutputs(i, &current_output)
            != DXGI_ERROR_NOT_FOUND)
#else
      while (  dxgi_adapter->lpVtbl->EnumOutputs(dxgi_adapter, i, &current_output)
            != DXGI_ERROR_NOT_FOUND)
#endif
      {
         RECT r, rect;
         DXGI_OUTPUT_DESC desc;
         int intersect_area;
         int bx1, by1, bx2, by2;
         int ax1               = 0;
         int ay1               = 0;
         int ax2               = 0;
         int ay2               = 0;

         if (win32_get_client_rect(&rect))
         {
            ax1                = rect.left;
            ay1                = rect.top;
            ax2                = rect.right;
            ay2                = rect.bottom;
         }

         /* Get the rectangle bounds of current output */
#ifdef __cplusplus
         if (FAILED(current_output->GetDesc(&desc)))
#else
         if (FAILED(current_output->lpVtbl->GetDesc(current_output, &desc)))
#endif
         {
            RARCH_ERR("[DXGI] Failed to get DXGI output description.\n");
            i++;
            continue;
         }

         /* TODO/FIXME - DesktopCoordinates won't work for WinRT */
         r                      = desc.DesktopCoordinates;
         bx1                    = r.left;
         by1                    = r.top;
         bx2                    = r.right;
         by2                    = r.bottom;

         /* Compute the intersection */
         intersect_area         = dxgi_compute_intersection_area(
               ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);

         if (intersect_area > best_intersect_area)
         {
            if (best_output)
            {
#ifdef __cplusplus
               best_output->Release();
#else
               Release(best_output);
#endif
            }
            best_output         = current_output;
#ifdef __cplusplus
            best_output->AddRef();
#else
            AddRef(best_output);
#endif
            best_intersect_area = (float)intersect_area;
         }

         i++;
      }

      if (current_output)
      {
#ifdef __cplusplus
         current_output->Release();
#else
         Release(current_output);
#endif
         current_output = NULL;
      }
#ifdef __cplusplus
      dxgi_adapter->Release();
#else
      Release(dxgi_adapter);
#endif
      dxgi_adapter = NULL;
      adapter_idx++;
   }

   if (!best_output)
   {
      RARCH_ERR("[DXGI] No output found for HDR check.\n");
      return false;
   }

#ifdef __cplusplus
   if (SUCCEEDED(best_output->QueryInterface(
               libretro_IID_IDXGIOutput6, (void**)&output6)))
#else
   if (SUCCEEDED(best_output->lpVtbl->QueryInterface(
               best_output,
               &libretro_IID_IDXGIOutput6, (void**)&output6)))
#endif
   {
      DXGI_OUTPUT_DESC1 desc1;
#ifdef __cplusplus
      if (SUCCEEDED(output6->GetDesc1(&desc1)))
#else
      if (SUCCEEDED(output6->lpVtbl->GetDesc1(output6, &desc1)))
#endif
      {
         supported = (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);

         if (supported)
         {
            video_driver_set_hdr_support();
            video_driver_set_hdr10_support();
            /* When Windows reports HDR support (PQ/ST.2084),
             * scRGB (R16G16B16A16_FLOAT + G10_NONE_P709) is
             * always available — the Windows HDR compositor
             * guarantees both paths. */
            video_driver_set_scrgb_support();
         }
         else
         {
            settings_t*    settings           = config_get_ptr();
            settings->flags                  |= SETTINGS_FLG_MODIFIED;
            settings->uints.video_hdr_mode    = 0;

            video_driver_unset_hdr_support();
            video_driver_unset_hdr10_support();
            video_driver_unset_scrgb_support();
         }
      }
      else
      {
         RARCH_ERR("[DXGI] Failed to get DXGI Output 6 description.\n");
      }
#ifdef __cplusplus
      output6->Release();
#else
      Release(output6);
#endif
   }
   else
   {
      RARCH_ERR("[DXGI] Failed to get DXGI Output 6 from best output.\n");
   }

#ifdef __cplusplus
   best_output->Release();
#else
   Release(best_output);
#endif

   return supported;
}

void dxgi_swapchain_color_space(
      DXGISwapChain chain_handle,
      DXGI_COLOR_SPACE_TYPE *chain_color_space,
      DXGI_COLOR_SPACE_TYPE color_space)
{
   if (*chain_color_space != color_space)
   {
      UINT color_space_support = 0;
#ifdef __cplusplus
      if (SUCCEEDED(chain_handle->CheckColorSpaceSupport(
                  color_space, &color_space_support))
            && ((color_space_support &
                  DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)
               == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
#else
      if (SUCCEEDED(chain_handle->lpVtbl->CheckColorSpaceSupport(
                  chain_handle, color_space, &color_space_support))
            && ((color_space_support &
                  DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)
               == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
#endif
      {
#ifdef __cplusplus
         if (FAILED(chain_handle->SetColorSpace1(color_space)))
#else
         if (FAILED(chain_handle->lpVtbl->SetColorSpace1(chain_handle, color_space)))
#endif
         {
            RARCH_ERR("[DXGI] Failed to set DXGI swapchain colour space.\n");
            /* TODO/FIXME/CLARIFICATION: Was this fall-through intentional?
             * Should chain color space still be set even when this fails?
             * Going to assume this was wrong and early return instead
             */
            return;
         }

         *chain_color_space = color_space;
      }
   }
}

void dxgi_set_hdr_metadata(
      DXGISwapChain                 handle,
      bool                          hdr_supported,
      enum dxgi_swapchain_bit_depth chain_bit_depth,
      DXGI_COLOR_SPACE_TYPE         chain_color_space,
      float                         max_output_nits,
      float                         min_output_nits,
      float                         max_cll,
      float                         max_fall
)
{
   /* TODO/FIXME - static globals - not thread-safe */
   static DXGI_HDR_METADATA_HDR10 g_hdr10_meta_data = {0};
   static const display_chromaticities_t
      display_chromaticity_list[]                   =
   {
      { 0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, /* Rec709  */
      { 0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f }, /* Rec2020 */
   };
   const display_chromaticities_t* chroma           = NULL;
   DXGI_HDR_METADATA_HDR10 hdr10_meta_data          = {0};
   int selected_chroma                              = 0;

   if (!handle)
      return;

   /* Clear the hdr meta data if the monitor does not support HDR */
   if (!hdr_supported)
   {
#ifdef __cplusplus
      if (FAILED(handle->SetHDRMetaData(
                  DXGI_HDR_METADATA_TYPE_NONE, 0, NULL)))
#else
      if (FAILED(handle->lpVtbl->SetHDRMetaData(handle,
                  DXGI_HDR_METADATA_TYPE_NONE, 0, NULL)))
#endif
      {
         RARCH_ERR("[DXGI] Failed to set HDR meta data to none.\n");
      }
      return;
   }


   /* Now select the chromacity based on colour space */
   if (     chain_bit_depth   == DXGI_SWAPCHAIN_BIT_DEPTH_10
         && chain_color_space ==
         DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
      selected_chroma                           = 1;
   else
   {
#ifdef __cplusplus
      if (FAILED(handle->SetHDRMetaData(
                  DXGI_HDR_METADATA_TYPE_NONE, 0, NULL)))
#else
      if (FAILED(handle->lpVtbl->SetHDRMetaData(handle,
                  DXGI_HDR_METADATA_TYPE_NONE, 0, NULL)))
#endif
      {
         RARCH_ERR("[DXGI] Failed to set HDR meta data to none.\n");
      }
      return;
   }

   /* Set the HDR meta data */
   chroma                                       =
      &display_chromaticity_list[selected_chroma];
   hdr10_meta_data.RedPrimary[0]                =
      (UINT16)(chroma->red_x * 50000.0f);
   hdr10_meta_data.RedPrimary[1]                =
      (UINT16)(chroma->red_y * 50000.0f);
   hdr10_meta_data.GreenPrimary[0]              =
      (UINT16)(chroma->green_x * 50000.0f);
   hdr10_meta_data.GreenPrimary[1]              =
      (UINT16)(chroma->green_y * 50000.0f);
   hdr10_meta_data.BluePrimary[0]               =
      (UINT16)(chroma->blue_x * 50000.0f);
   hdr10_meta_data.BluePrimary[1]               =
      (UINT16)(chroma->blue_y * 50000.0f);
   hdr10_meta_data.WhitePoint[0]                =
      (UINT16)(chroma->white_x * 50000.0f);
   hdr10_meta_data.WhitePoint[1]                =
      (UINT16)(chroma->white_y * 50000.0f);
   hdr10_meta_data.MaxMasteringLuminance        =
      (UINT)(max_output_nits);              /* Units: 1 nit */
   hdr10_meta_data.MinMasteringLuminance        =
      (UINT)(min_output_nits * 10000.0f);   /* Units: 0.0001 nits */
   hdr10_meta_data.MaxContentLightLevel         =
      (UINT16)(max_cll);
   hdr10_meta_data.MaxFrameAverageLightLevel    =
      (UINT16)(max_fall);

   if (memcmp(g_hdr10_meta_data.RedPrimary,   hdr10_meta_data.RedPrimary,   sizeof(hdr10_meta_data.RedPrimary))   ||
       memcmp(g_hdr10_meta_data.GreenPrimary, hdr10_meta_data.GreenPrimary, sizeof(hdr10_meta_data.GreenPrimary)) ||
       memcmp(g_hdr10_meta_data.BluePrimary,  hdr10_meta_data.BluePrimary,  sizeof(hdr10_meta_data.BluePrimary))  ||
       memcmp(g_hdr10_meta_data.WhitePoint,   hdr10_meta_data.WhitePoint,   sizeof(hdr10_meta_data.WhitePoint))   ||
       g_hdr10_meta_data.MaxContentLightLevel       != hdr10_meta_data.MaxContentLightLevel  ||
       g_hdr10_meta_data.MaxMasteringLuminance      != hdr10_meta_data.MaxMasteringLuminance ||
       g_hdr10_meta_data.MinMasteringLuminance      != hdr10_meta_data.MinMasteringLuminance ||
       g_hdr10_meta_data.MaxFrameAverageLightLevel  != hdr10_meta_data.MaxFrameAverageLightLevel)
   {
#ifdef __cplusplus
      if (FAILED(handle->SetHDRMetaData(
                  DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &hdr10_meta_data)))
#else
      if (FAILED(handle->lpVtbl->SetHDRMetaData(handle,
                  DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &hdr10_meta_data)))
#endif
      {
         RARCH_ERR("[DXGI] Failed to set HDR meta data for HDR10.\n");
         return;
      }
      g_hdr10_meta_data = hdr10_meta_data;
   }
}
#endif
