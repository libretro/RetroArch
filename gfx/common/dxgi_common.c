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
#include <string/stdstring.h>
#include <retro_environment.h>

#include <assert.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dxgi_common.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../ui/ui_companion_driver.h"
#include "../../retroarch.h"
#include "../frontend/frontend_driver.h"
#include "win32_common.h"

#if defined(HAVE_DYNAMIC) && !defined(__WINRT__)
#include <dynamic/dylib.h>

HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** ppFactory)
{
   static HRESULT(WINAPI * fp)(REFIID, void**);

   static dylib_t dxgi_dll;

   if (!dxgi_dll)
      dxgi_dll = dylib_load("dxgi.dll");

   if (!dxgi_dll)
      return TYPE_E_CANTLOADLIBRARY;

   if (!fp)
      fp = (HRESULT(WINAPI*)(REFIID, void**))dylib_proc(dxgi_dll, "CreateDXGIFactory1");

   if (!fp)
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

#define FORMAT_PROCESS_( \
      src_type, src_rb, src_gb, src_bb, src_ab, src_rs, src_gs, src_bs, src_as, dst_type, dst_rb, \
      dst_gb, dst_bb, dst_ab, dst_rs, dst_gs, dst_bs, dst_as) \
   do \
   { \
      if ((sizeof(src_type) == sizeof(dst_type)) && \
          ((src_rs == dst_rs && src_rb == dst_rb) || !dst_rb) && \
          ((src_gs == dst_gs && src_gb == dst_gb) || !dst_gb) && \
          ((src_bs == dst_bs && src_bb == dst_bb) || !dst_bb) && \
          ((src_as == dst_as && src_ab == dst_ab) || !dst_ab)) \
      { \
         const UINT8* in  = (const UINT8*)src_data; \
         UINT8*       out = (UINT8*)dst_data; \
         for (i = 0; i < height; i++) \
         { \
            memcpy(out, in, width * sizeof(src_type)); \
            in += src_pitch ? src_pitch : width * sizeof(src_type); \
            out += dst_pitch ? dst_pitch : width * sizeof(dst_type); \
         } \
      } \
      else \
      { \
         const src_type* src_ptr = (const src_type*)src_data; \
         dst_type*       dst_ptr = (dst_type*)dst_data; \
         if (src_pitch) \
            src_pitch -= width * sizeof(*src_ptr); \
         if (dst_pitch) \
            dst_pitch -= width * sizeof(*dst_ptr); \
         for (i = 0; i < height; i++) \
         { \
            for (j = 0; j < width; j++) \
            { \
               unsigned r, g, b, a; \
               src_type src_val = *src_ptr++; \
               if (src_rb) \
               { \
                  r = (src_val >> src_rs) & ((1 << src_rb) - 1); \
                  r = (src_rb < dst_rb) \
                            ? (r << (dst_rb - src_rb)) | \
                                    (r >> ((2 * src_rb > dst_rb) ? 2 * src_rb - dst_rb : 0)) \
                            : r >> (src_rb - dst_rb); \
               } \
               if (src_gb) \
               { \
                  g = (src_val >> src_gs) & ((1 << src_gb) - 1); \
                  g = (src_gb < dst_gb) \
                            ? (g << (dst_gb - src_gb)) | \
                                    (g >> ((2 * src_gb > dst_gb) ? 2 * src_gb - dst_gb : 0)) \
                            : g >> (src_gb - dst_gb); \
               } \
               if (src_bb) \
               { \
                  b = (src_val >> src_bs) & ((1 << src_bb) - 1); \
                  b = (src_bb < dst_bb) \
                            ? (b << (dst_bb - src_bb)) | \
                                    (b >> ((2 * src_bb > dst_bb) ? 2 * src_bb - dst_bb : 0)) \
                            : b >> (src_bb - dst_bb); \
               } \
               if (src_ab) \
               { \
                  a = (src_val >> src_as) & ((1 << src_ab) - 1); \
                  a = (src_ab < dst_ab) \
                            ? (a << (dst_ab - src_ab)) | \
                                    (a >> ((2 * src_ab > dst_ab) ? 2 * src_ab - dst_ab : 0)) \
                            : a >> (src_ab - dst_ab); \
               } \
               *dst_ptr++ = ((src_rb ? r : 0) << dst_rs) | ((src_gb ? g : 0) << dst_gs) | \
                            ((src_bb ? b : 0) << dst_bs) | \
                            ((src_ab ? a : ((1 << dst_ab) - 1)) << dst_as); \
            } \
            src_ptr = (src_type*)((UINT8*)src_ptr + src_pitch); \
            dst_ptr = (dst_type*)((UINT8*)dst_ptr + dst_pitch); \
         } \
      } \
   } while (0)

#define FORMAT_PROCESS(args) FORMAT_PROCESS_ args

#define FORMAT_DST(st, dt) \
   case dt: \
   { \
      FORMAT_PROCESS((st##_DESCS, dt##_DESCS)); \
      break; \
   }

#define FORMAT_SRC(st) \
   case st: \
   { \
      switch ((unsigned)dst_format) \
      { \
         FORMAT_DST_LIST(st); \
         default: \
            assert(0); \
            break; \
      } \
      break; \
   }

/* clang-format off */
/*                                                        r, g, b, a,     r,  g,  b,  a */
#define DXGI_FORMAT_R8G8B8A8_UNORM_DESCS       UINT32,    8, 8, 8, 8,     0,  8, 16, 24
#define DXGI_FORMAT_B8G8R8X8_UNORM_DESCS       UINT32,    8, 8, 8, 0,    16,  8,  0,  0
#define DXGI_FORMAT_B8G8R8A8_UNORM_DESCS       UINT32,    8, 8, 8, 8,    16,  8,  0, 24
#define DXGI_FORMAT_A8_UNORM_DESCS             UINT8,     0, 0, 0, 8,     0,  0,  0,  0
#define DXGI_FORMAT_R8_UNORM_DESCS             UINT8,     8, 0, 0, 0,     0,  0,  0,  0
#define DXGI_FORMAT_B5G6R5_UNORM_DESCS         UINT16,    5, 6, 5, 0,    11,  5,  0,  0
#define DXGI_FORMAT_B5G5R5A1_UNORM_DESCS       UINT16,    5, 5, 5, 1,    10,  5,  0, 11
#define DXGI_FORMAT_B4G4R4A4_UNORM_DESCS       UINT16,    4, 4, 4, 4,     8,  4,  0, 12
#define DXGI_FORMAT_EX_A4R4G4B4_UNORM_DESCS    UINT16,    4, 4, 4, 4,     4,  8, 12,  0

#define FORMAT_SRC_LIST() \
   FORMAT_SRC(DXGI_FORMAT_R8G8B8A8_UNORM); \
   FORMAT_SRC(DXGI_FORMAT_B8G8R8X8_UNORM); \
   FORMAT_SRC(DXGI_FORMAT_A8_UNORM); \
   FORMAT_SRC(DXGI_FORMAT_R8_UNORM); \
   FORMAT_SRC(DXGI_FORMAT_B5G6R5_UNORM); \
   FORMAT_SRC(DXGI_FORMAT_B5G5R5A1_UNORM); \
   FORMAT_SRC(DXGI_FORMAT_B4G4R4A4_UNORM); \
   FORMAT_SRC(DXGI_FORMAT_B8G8R8A8_UNORM); \
   FORMAT_SRC(DXGI_FORMAT_EX_A4R4G4B4_UNORM)

#define FORMAT_DST_LIST(srcfmt) \
   FORMAT_DST(srcfmt, DXGI_FORMAT_R8G8B8A8_UNORM); \
   FORMAT_DST(srcfmt, DXGI_FORMAT_B8G8R8X8_UNORM); \
   FORMAT_DST(srcfmt, DXGI_FORMAT_A8_UNORM); \
   FORMAT_DST(srcfmt, DXGI_FORMAT_R8_UNORM); \
   FORMAT_DST(srcfmt, DXGI_FORMAT_B5G6R5_UNORM); \
   FORMAT_DST(srcfmt, DXGI_FORMAT_B5G5R5A1_UNORM); \
   FORMAT_DST(srcfmt, DXGI_FORMAT_B4G4R4A4_UNORM); \
   FORMAT_DST(srcfmt, DXGI_FORMAT_B8G8R8A8_UNORM); \
   FORMAT_DST(srcfmt, DXGI_FORMAT_EX_A4R4G4B4_UNORM)
   /* clang-format on */

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
#if defined(PERF_START) && defined(PERF_STOP)
   PERF_START();
#endif

   switch ((unsigned)src_format)
   {
      FORMAT_SRC_LIST();

      default:
         assert(0);
         break;
   }

#if defined(PERF_START) && defined(PERF_STOP)
   PERF_STOP();
#endif
}
#ifdef _MSC_VER
#pragma warning(default : 4293)
#endif

void dxgi_update_title(video_frame_info_t* video_info)
{
#ifndef __WINRT__
   const ui_window_t* window = ui_companion_driver_get_window_ptr();

   if (window)
   {
      char title[128];

      title[0] = '\0';

      video_driver_get_window_title(title, sizeof(title));

      if (title[0])
         window->set_title(&main_window, title);
   }
#endif
}

DXGI_FORMAT glslang_format_to_dxgi(glslang_format fmt)
{
#undef FMT_
#define FMT_(x)  case SLANG_FORMAT_##x: return DXGI_FORMAT_##x
#undef FMT2
#define FMT2(x,y) case SLANG_FORMAT_##x: return y

   switch (fmt)
   {
   FMT_(R8_UNORM);
   FMT_(R8_SINT);
   FMT_(R8_UINT);
   FMT_(R8G8_UNORM);
   FMT_(R8G8_SINT);
   FMT_(R8G8_UINT);
   FMT_(R8G8B8A8_UNORM);
   FMT_(R8G8B8A8_SINT);
   FMT_(R8G8B8A8_UINT);
   FMT2(R8G8B8A8_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

   FMT2(A2B10G10R10_UNORM_PACK32, DXGI_FORMAT_R10G10B10A2_UNORM);
   FMT2(A2B10G10R10_UINT_PACK32, DXGI_FORMAT_R10G10B10A2_UNORM);

   FMT_(R16_UINT);
   FMT_(R16_SINT);
   FMT2(R16_SFLOAT, DXGI_FORMAT_R16_FLOAT);
   FMT_(R16G16_UINT);
   FMT_(R16G16_SINT);
   FMT2(R16G16_SFLOAT, DXGI_FORMAT_R16G16_FLOAT);
   FMT_(R16G16B16A16_UINT);
   FMT_(R16G16B16A16_SINT);
   FMT2(R16G16B16A16_SFLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT);

   FMT_(R32_UINT);
   FMT_(R32_SINT);
   FMT2(R32_SFLOAT, DXGI_FORMAT_R32_FLOAT);
   FMT_(R32G32_UINT);
   FMT_(R32G32_SINT);
   FMT2(R32G32_SFLOAT, DXGI_FORMAT_R32G32_FLOAT);
   FMT_(R32G32B32A32_UINT);
   FMT_(R32G32B32A32_SINT);
   FMT2(R32G32B32A32_SFLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT);

   case SLANG_FORMAT_UNKNOWN:
   default:
      break;
   }

   return DXGI_FORMAT_UNKNOWN;
}
