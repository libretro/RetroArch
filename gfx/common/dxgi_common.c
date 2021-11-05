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
#include "../../config.h"
#endif

#include "dxgi_common.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../ui/ui_companion_driver.h"
#include "../../retroarch.h"
#include "../frontend/frontend_driver.h"
#include "win32_common.h"

const GUID DECLSPEC_SELECTANY libretro_IID_IDXGIOutput6 = { 0x068346e8,0xaaec,
0x4b84, {0xad,0xd7,0x13,0x7f,0x51,0x3f,0x77,0xa1 } };

#ifdef HAVE_DXGI_HDR
/* TODO/FIXME - globals */
static DXGI_HDR_METADATA_HDR10 g_hdr10_meta_data = {0};

typedef enum hdr_root_constants
{
   HDR_ROOT_CONSTANTS_REFERENCE_WHITE_NITS = 0,
   HDR_ROOT_CONSTANTS_DISPLAY_CURVE,
   HDR_ROOT_CONSTANTS_COUNT
} hdr_root_constants_t;
#endif

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

   switch ((unsigned)src_format)
   {
      FORMAT_SRC_LIST();

      default:
      assert(0);
      break;
   }
}

#ifdef _MSC_VER
#pragma warning(default : 4293)
#endif

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
bool dxgi_check_display_hdr_support(DXGIFactory factory, HWND hwnd)
#endif
{
   DXGIOutput6 output6       = NULL;
   DXGIOutput best_output    = NULL;
   DXGIOutput current_output = NULL;
   DXGIAdapter dxgi_adapter  = NULL;
   UINT i                    = 0;
   bool supported            = false;
   float best_intersect_area = -1;

#ifdef __WINRT__
   if (!DXGIIsCurrent2(factory))

   {
      if (FAILED(DXGICreateFactory2(&factory)))
      {
         RARCH_ERR("[DXGI]: Failed to create DXGI factory\n");
         return false;
      }
   }

   if (FAILED(DXGIEnumAdapters2(factory, 0, &dxgi_adapter)))
   {
      RARCH_ERR("[DXGI]: Failed to enumerate adapters\n");
      return false;
   }
#else
   if (!DXGIIsCurrent(factory))

   {
      if (FAILED(DXGICreateFactory(&factory)))
      {
         RARCH_ERR("[DXGI]: Failed to create DXGI factory\n");
         return false;
      }
   }

   if (FAILED(DXGIEnumAdapters(factory, 0, &dxgi_adapter)))
   {
      RARCH_ERR("[DXGI]: Failed to enumerate adapters\n");
      return false;
   }
#endif

   while (  DXGIEnumOutputs(dxgi_adapter, i, &current_output) 
         != DXGI_ERROR_NOT_FOUND)
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
      if (FAILED(DXGIGetOutputDesc(current_output, &desc)))
      {
         RARCH_ERR("[DXGI]: Failed to get DXGI output description\n");
         goto error;
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
         best_output         = current_output;
         AddRef(best_output);
         best_intersect_area = (float)intersect_area;
      }

      i++;
   }

   if (SUCCEEDED(best_output->lpVtbl->QueryInterface(
               best_output,
               &libretro_IID_IDXGIOutput6, (void**)&output6)))
   {
      DXGI_OUTPUT_DESC1 desc1;
      if (SUCCEEDED(DXGIGetOutputDesc1(output6, &desc1)))
      {
         supported = (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);

         if (supported)
            video_driver_set_hdr_support();
         else
         {
            settings_t*    settings          = config_get_ptr();
            settings->modified               = true;
            settings->bools.video_hdr_enable = false;

            video_driver_unset_hdr_support();
         }
      }
      else
      {
         RARCH_ERR("[DXGI]: Failed to get DXGI Output 6 description\n");
      }
      Release(output6);
   }
   else
   {
      RARCH_ERR("[DXGI]: Failed to get DXGI Output 6 from best output\n");
   }

error:
   Release(best_output);
   Release(current_output);
   Release(dxgi_adapter);

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
      if (SUCCEEDED(DXGICheckColorSpaceSupport(
                  chain_handle, color_space,
                  &color_space_support))
            && ((color_space_support & 
                  DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) 
               == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
      {
         if (FAILED(DXGISetColorSpace1(chain_handle, color_space)))
         {
            RARCH_ERR("[DXGI]: Failed to set DXGI swapchain colour space\n");
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
   static const display_chromaticities_t 
      display_chromaticity_list[]               =
   {
      { 0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, /* Rec709  */   
      { 0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f }, /* Rec2020 */  
   };
   const display_chromaticities_t* chroma       = NULL;
   DXGI_HDR_METADATA_HDR10 hdr10_meta_data      = {0};
   int selected_chroma                          = 0;
   
   if (!handle)
      return;

   /* Clear the hdr meta data if the monitor does not support HDR */
   if (!hdr_supported)
   {
      if (FAILED(DXGISetHDRMetaData(handle,
                  DXGI_HDR_METADATA_TYPE_NONE, 0, NULL)))
      {
         RARCH_ERR("[DXGI]: Failed to set HDR meta data to none\n");
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
      if (FAILED(DXGISetHDRMetaData(handle,
                  DXGI_HDR_METADATA_TYPE_NONE, 0, NULL)))
      {
         RARCH_ERR("[DXGI]: Failed to set HDR meta data to none\n");
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
      (UINT)(max_output_nits * 10000.0f);
   hdr10_meta_data.MinMasteringLuminance        = 
      (UINT)(min_output_nits * 10000.0f);
   hdr10_meta_data.MaxContentLightLevel         = 
      (UINT16)(max_cll);
   hdr10_meta_data.MaxFrameAverageLightLevel    = 
      (UINT16)(max_fall);

   if(g_hdr10_meta_data.RedPrimary                 != hdr10_meta_data.RedPrimary ||
      g_hdr10_meta_data.GreenPrimary               != hdr10_meta_data.GreenPrimary ||
      g_hdr10_meta_data.BluePrimary                != hdr10_meta_data.BluePrimary ||
      g_hdr10_meta_data.WhitePoint                 != hdr10_meta_data.WhitePoint ||
      g_hdr10_meta_data.MaxContentLightLevel       != hdr10_meta_data.MaxContentLightLevel ||
      g_hdr10_meta_data.MaxMasteringLuminance      != hdr10_meta_data.MaxMasteringLuminance ||
      g_hdr10_meta_data.MinMasteringLuminance      != hdr10_meta_data.MinMasteringLuminance ||
      g_hdr10_meta_data.MaxFrameAverageLightLevel  != hdr10_meta_data.MaxFrameAverageLightLevel)
   {
      if (FAILED(DXGISetHDRMetaData(handle,
                  DXGI_HDR_METADATA_TYPE_HDR10,
                  sizeof(DXGI_HDR_METADATA_HDR10), &hdr10_meta_data)))
      {
         RARCH_ERR("[DXGI]: Failed to set HDR meta data for HDR10\n");
         return;
      }
      g_hdr10_meta_data = hdr10_meta_data;
   }
}
#endif
