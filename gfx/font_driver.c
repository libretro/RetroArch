/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdlib.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "font_driver.h"
#include "video_thread_wrapper.h"

/* TODO/FIXME - global */
static void *video_font_driver = NULL;

int font_renderer_create_default(
      const font_renderer_driver_t **drv,
      void **handle,
      const char *font_path, unsigned font_size)
{
   static const font_renderer_driver_t *font_backends[] = {
#ifdef HAVE_FREETYPE
      &freetype_font_renderer,
#endif
#if defined(__APPLE__) && defined(HAVE_CORETEXT)
      &coretext_font_renderer,
#endif
#ifdef HAVE_STB_FONT
#if defined(VITA) || defined(ORBIS) || defined(WIIU) || defined(ANDROID) || (defined(_WIN32) && !defined(_XBOX) && !defined(_MSC_VER) && _MSC_VER >= 1400) || (defined(_WIN32) && !defined(_XBOX) && defined(_MSC_VER)) || defined(HAVE_LIBNX) || defined(__linux__) || defined (HAVE_EMSCRIPTEN) || defined(__APPLE__) || defined(HAVE_ODROIDGO2) || defined(__PS3__)
      &stb_unicode_font_renderer,
#else
      &stb_unicode_font_renderer,
#endif
#endif
      &bitmap_font_renderer,
      NULL
   };
   unsigned i;

   for (i = 0; font_backends[i]; i++)
   {
      const char *path = font_path;

      if (!path)
         path = font_backends[i]->get_default_font();
      if (!path)
         continue;

      *handle = font_backends[i]->init(path, font_size);
      if (*handle)
      {
         *drv = font_backends[i];
         return 1;
      }
   }

   *drv    = NULL;
   *handle = NULL;

   return 0;
}

#ifdef HAVE_D3D8
static bool d3d8_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   static const font_renderer_t *d3d8_font_backends[] = {
#if defined(_XBOX1)
      &d3d_xdk1_font,
#endif
      NULL
   };
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(d3d8_font_backends); i++)
   {
      void *data = d3d8_font_backends[i] ? d3d8_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded) : NULL;

      if (!data)
         continue;

      *font_driver = d3d8_font_backends[i];
      *font_handle = data;

      return true;
   }

   return false;
}
#endif

#ifdef HAVE_D3D9
static bool d3d9_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   static const font_renderer_t *d3d9_font_backends[] = {
#if defined(_WIN32) && defined(HAVE_D3DX)
      &d3d_win32_font,
#endif
      NULL
   };
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(d3d9_font_backends); i++)
   {
      void *data = d3d9_font_backends[i] ? d3d9_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded) : NULL;

      if (!data)
         continue;

      *font_driver = d3d9_font_backends[i];
      *font_handle = data;

      return true;
   }

   return false;
}
#endif

#ifdef HAVE_OPENGL1
static bool gl1_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = gl1_raster_font.init(
         video_data, font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &gl1_raster_font;
   *font_handle = data;
   return true;
}
#endif

#if defined(HAVE_OPENGL)
static bool gl_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = gl2_raster_font.init(
         video_data, font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &gl2_raster_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef HAVE_OPENGL_CORE
static bool gl3_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = gl3_raster_font.init(
         video_data, font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &gl3_raster_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef HAVE_CACA
static bool caca_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = caca_font.init(
         video_data, font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &caca_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef HAVE_SIXEL
static bool sixel_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = sixel_font.init(
         video_data, font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &sixel_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef DJGPP
static bool vga_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = vga_font.init(
         video_data, font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &vga_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef HAVE_GDI
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
static bool gdi_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = gdi_font.init(
         video_data, font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &gdi_font;
   *font_handle = data;
   return true;
}
#endif
#endif

#ifdef HAVE_VULKAN

static bool vulkan_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = vulkan_raster_font.init(video_data,
         font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &vulkan_raster_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef HAVE_METAL
static bool metal_font_init_first(
   const void **font_driver, void **font_handle,
   void *video_data, const char *font_path,
   float font_size, bool is_threaded)
{
   void *data = metal_raster_font.init(video_data,
         font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &metal_raster_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef HAVE_D3D10
static bool d3d10_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = d3d10_font.init(video_data,
         font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &d3d10_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef HAVE_D3D11
static bool d3d11_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = d3d11_font.init(video_data,
         font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &d3d11_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef HAVE_D3D12
static bool d3d12_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = d3d12_font.init(video_data,
         font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &d3d12_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef PS2
static bool ps2_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = ps2_font.init(
         video_data, font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &ps2_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef HAVE_VITA2D
static bool vita2d_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = vita2d_vita_font.init(
         video_data, font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &vita2d_vita_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef _3DS
static bool ctr_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = ctr_font.init(
         video_data, font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &ctr_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef HAVE_LIBNX
static bool switch_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = switch_font.init(
         video_data, font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &switch_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef WIIU
static bool wiiu_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = wiiu_font.init(
         video_data, font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &wiiu_font;
   *font_handle = data;
   return true;
}
#endif

#ifdef __PSL1GHT__
static bool rsx_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   void *data = rsx_font.init(
         video_data, font_path, font_size,
         is_threaded);

   if (!data)
      return false;

   *font_driver = &rsx_font;
   *font_handle = data;
   return true;
}
#endif

static bool font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path, float font_size,
      enum font_driver_render_api api, bool is_threaded)
{
   if (font_path && !font_path[0])
      font_path = NULL;

   switch (api)
   {
#ifdef HAVE_OPENGL1
      case FONT_DRIVER_RENDER_OPENGL1_API:
         return gl1_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_OPENGL
      case FONT_DRIVER_RENDER_OPENGL_API:
         return gl_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_OPENGL_CORE
      case FONT_DRIVER_RENDER_OPENGL_CORE_API:
         return gl3_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_VULKAN
      case FONT_DRIVER_RENDER_VULKAN_API:
         return vulkan_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_METAL
   case FONT_DRIVER_RENDER_METAL_API:
      return metal_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_D3D8
      case FONT_DRIVER_RENDER_D3D8_API:
         return d3d8_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_D3D9
      case FONT_DRIVER_RENDER_D3D9_API:
         return d3d9_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_D3D10
      case FONT_DRIVER_RENDER_D3D10_API:
         return d3d10_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_D3D11
      case FONT_DRIVER_RENDER_D3D11_API:
         return d3d11_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_D3D12
      case FONT_DRIVER_RENDER_D3D12_API:
         return d3d12_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_VITA2D
      case FONT_DRIVER_RENDER_VITA2D:
         return vita2d_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef PS2
      case FONT_DRIVER_RENDER_PS2:
         return ps2_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef _3DS
      case FONT_DRIVER_RENDER_CTR:
         return ctr_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef WIIU
      case FONT_DRIVER_RENDER_WIIU:
         return wiiu_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_CACA
      case FONT_DRIVER_RENDER_CACA:
         return caca_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_SIXEL
      case FONT_DRIVER_RENDER_SIXEL:
         return sixel_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_LIBNX
      case FONT_DRIVER_RENDER_SWITCH:
         return switch_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef __PSL1GHT__
      case FONT_DRIVER_RENDER_RSX:
         return rsx_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_GDI
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
      case FONT_DRIVER_RENDER_GDI:
         return gdi_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#endif
#ifdef DJGPP
      case FONT_DRIVER_RENDER_VGA:
         return vga_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
      case FONT_DRIVER_RENDER_DONT_CARE:
         /* TODO/FIXME - lookup graphics driver's 'API' */
         break;
      default:
         break;
   }

   return false;
}

#ifdef HAVE_LANGEXTRA
/* ASCII:       0xxxxxxx  (c & 0x80) == 0x00
 * other start: 11xxxxxx  (c & 0xC0) == 0xC0
 * other cont:  10xxxxxx  (c & 0xC0) == 0x80
 * Neutral:
 * 0020 - 002F: 001xxxxx (c & 0xE0) == 0x20
 * misc. white space:
 * 2000 - 200D: 11100010 10000000 1000xxxx (c[2] < 0x8E) (3 bytes)
 * Hebrew:
 * 0591 - 05F4: 1101011x (c & 0xFE) == 0xD6 (2 bytes)
 * Arabic:
 * 0600 - 06FF: 110110xx (c & 0xFC) == 0xD8 (2 bytes)
 */

/* clang-format off */
#define IS_ASCII(p)        ((*(p)&0x80) == 0x00)
#define IS_MBSTART(p)      ((*(p)&0xC0) == 0xC0)
#define IS_MBCONT(p)       ((*(p)&0xC0) == 0x80)
#define IS_DIR_NEUTRAL(p)  ((*(p)&0xE0) == 0x20)
#define IS_HEBREW(p)       ((*(p)&0xFE) == 0xD6)
#define IS_ARABIC(p)       ((*(p)&0xFC) == 0xD8)
#define IS_RTL(p)          (IS_HEBREW(p) || IS_ARABIC(p))
#define GET_ID_ARABIC(p)   (((unsigned char)(p)[0] << 6) | ((unsigned char)(p)[1] & 0x3F))


/* Checks for miscellaneous whitespace characters in the range U+2000 to U+200D */
static INLINE unsigned is_misc_ws(const unsigned char* src)
{
   unsigned res = 0;
   if (*(src) == 0xE2) /* first byte */
   {
      src++;
      if (*(src) == 0x80) /* second byte */
      {
         src++;
         res = (*(src) < 0x8E); /* third byte */
      }
   }
   return res;
}

static INLINE unsigned font_get_arabic_replacement(
      const char* src, const char* start)
{
   /* 0x0620 to 0x064F */
   static const unsigned arabic_shape_map[0x100][0x4] = {
      { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0600 */
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },

      { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0610 */
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },

      { 0 },                               /* 0x0620 */
      { 0xFE80 },
      { 0xFE81, 0xFE82 },
      { 0xFE83, 0xFE84 },
      { 0xFE85, 0xFE86 },
      { 0xFE87, 0xFE88 },
      { 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C },
      { 0xFE8D, 0xFE8E },

      { 0xFE8F, 0xFE90, 0xFE91, 0xFE92 },
      { 0xFE93, 0xFE94 },
      { 0xFE95, 0xFE96, 0xFE97, 0xFE98 },
      { 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C },
      { 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0 },
      { 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4 },
      { 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8 },
      { 0xFEA9, 0xFEAA },

      { 0xFEAB, 0xFEAC },                  /* 0x0630 */
      { 0xFEAD, 0xFEAE },
      { 0xFEAF, 0xFEB0 },
      { 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4 },
      { 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8 },
      { 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC },
      { 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0 },
      { 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4 },

      { 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8 },
      { 0xFEC9, 0xFECA, 0xFECB, 0xFECC },
      { 0xFECD, 0xFECE, 0xFECF, 0xFED0 },
      { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },

      { 0 },                               /* 0x0640 */
      { 0xFED1, 0xFED2, 0xFED3, 0xFED4 },
      { 0xFED5, 0xFED6, 0xFED7, 0xFED8 },
      { 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC },
      { 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0 },
      { 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4 },
      { 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8 },
      { 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC },

      { 0xFEED, 0xFEEE },
      { 0xFEEF, 0xFEF0, 0xFBE8, 0xFBE9 },
      { 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4 },
      { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },

      { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0650 */
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },


      { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0660 */
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },


      { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0670 */
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },

      { 0 }, { 0 },
      { 0xFB56, 0xFB57, 0xFB58, 0xFB59 },
      { 0 },


      { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0680 */
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },


      { 0 }, { 0 }, { 0 }, { 0 },          /* 0x0690 */
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },


      { 0 }, { 0 }, { 0 }, { 0 },          /* 0x06A0 */
      { 0 }, { 0 }, { 0 }, { 0 },

      { 0 },
      { 0xFB8E, 0xFB8F, 0xFB90, 0xFB91 },
      { 0 }, { 0 },

      { 0 }, { 0 }, { 0 },
      { 0xFB92, 0xFB93, 0xFB94, 0xFB95 },


      { 0 }, { 0 }, { 0 }, { 0 },          /* 0x06B0 */
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },


      { 0 }, { 0 }, { 0 }, { 0 },          /* 0x06C0 */
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },

      { 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF },
      { 0 }, { 0 }, { 0 },


      { 0 }, { 0 }, { 0 }, { 0 },          /* 0x06D0 */
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },


      { 0 }, { 0 }, { 0 }, { 0 },          /* 0x06E0 */
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },


      { 0 }, { 0 }, { 0 }, { 0 },          /* 0x06F0 */
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },
      { 0 }, { 0 }, { 0 }, { 0 },
   };
   unsigned      result         = 0;
   bool          prev_connected = false;
   bool          next_connected = false;
   unsigned char id             = GET_ID_ARABIC(src);
   const char*   prev           = src - 2;
   const char*   next           = src + 2;

   if (IS_ARABIC(prev) && (prev >= start))
   {
      unsigned char prev_id = GET_ID_ARABIC(prev);

      /* nonspacing diacritics 0x4b -- 0x5f */
      while (prev_id > 0x4A && prev_id < 0x60)
      {
         prev -= 2;
         if ((prev >= start) && IS_ARABIC(prev))
            prev_id = GET_ID_ARABIC(prev);
         else
            break;
      }

      if (prev_id == 0x44) /* Arabic Letter Lam */
      {
         unsigned char prev2_id = 0;
         const char*   prev2    = prev - 2;

         if (prev2 >= start)
            prev2_id            = (prev2[0] << 6) | (prev2[1] & 0x3F);

         /* nonspacing diacritics 0x4b -- 0x5f */
         while (prev2_id > 0x4A && prev2_id < 0x60)
         {
            prev2 -= 2;
            if ((prev2 >= start) && IS_ARABIC(prev2))
               prev2_id = GET_ID_ARABIC(prev2);
            else
               break;
         }

         prev_connected = !!arabic_shape_map[prev2_id][2];

         switch (id)
         {
            case 0x22: /* Arabic Letter Alef with Madda Above */
               return 0xFEF5 + prev_connected;
            case 0x23: /* Arabic Letter Alef with Hamza Above */
               return 0xFEF7 + prev_connected;
            case 0x25: /* Arabic Letter Alef with Hamza Below */
               return 0xFEF9 + prev_connected;
            case 0x27: /* Arabic Letter Alef */
               return 0xFEFB + prev_connected;
         }
      }
      prev_connected = !!arabic_shape_map[prev_id][2];
   }

   if (IS_ARABIC(next))
   {
      unsigned char next_id = GET_ID_ARABIC(next);

      /* nonspacing diacritics 0x4b -- 0x5f */
      while (next_id > 0x4A && next_id < 0x60)
      {
         next += 2;
         if (!IS_ARABIC(next))
            break;
         next_id = GET_ID_ARABIC(next);
      }

      next_connected = !!arabic_shape_map[next_id][1];
   }

   if ((result = 
            arabic_shape_map[id][prev_connected | (next_connected <<
               1)]))
      return result;
   return arabic_shape_map[id][prev_connected];
}
/* clang-format on */

static char* font_driver_reshape_msg(const char* msg, unsigned char *buffer, size_t buffer_size)
{
   const unsigned char *src        = (const unsigned char*)msg;
   bool                 reverse    = false;
   size_t              msg_size    = (strlen(msg) * 2) + 1;
   /* Fallback to heap allocated buffer if the buffer is too small */
   /* worst case transformations are 2 bytes to 4 bytes -- aliaspider */
   unsigned char*       dst_buffer = (buffer_size < msg_size) 
                                   ? (unsigned char*)malloc(msg_size)
                                   : buffer;
   unsigned char *dst              = (unsigned char*)dst_buffer;

   while (*src || reverse)
   {
      if (reverse)
      {
         src--;
         while (src > (const unsigned char*)msg && IS_MBCONT(src))
            src--;

         if (src >= (const unsigned char*)msg && (IS_RTL(src) || IS_DIR_NEUTRAL(src) || is_misc_ws(src)))
         {
            if (IS_ARABIC(src))
            {
               unsigned replacement = font_get_arabic_replacement(
                     (const char*)src, msg);

               if (replacement)
               {
                  if (replacement < 0x80)
                     *dst++ = replacement;
                  else if (replacement < 0x800)
                  {
                     *dst++ = 0xC0 | (replacement >> 6);
                     *dst++ = 0x80 | (replacement & 0x3F);
                  }
                  else if (replacement < 0x10000)
                  {
                     /* merged glyphs */
                     if ((replacement >= 0xFEF5) && (replacement <= 0xFEFC))
                        src -= 2;

                     *dst++ = 0xE0 | ( replacement >> 12);
                     *dst++ = 0x80 | ((replacement >> 6) & 0x3F);
                     *dst++ = 0x80 | ( replacement       & 0x3F);
                  }
                  else
                  {
                     *dst++ = 0xF0 |  (replacement >> 18);
                     *dst++ = 0x80 | ((replacement >> 12) & 0x3F);
                     *dst++ = 0x80 | ((replacement >> 6)  & 0x3F);
                     *dst++ = 0x80 | ( replacement        & 0x3F);
                  }

                  continue;
               }
            }

            *dst++ = *src++;
            while (IS_MBCONT(src))
               *dst++ = *src++;
            src--;

            while (IS_MBCONT(src))
               src--;
         }
         else
         {
            reverse = false;
            src++;
            while (  IS_MBCONT(src) 
                  || IS_RTL(src) 
                  || IS_DIR_NEUTRAL(src) 
                  || is_misc_ws(src))
               src++;
         }
      }
      else
      {
         if (IS_RTL(src))
         {
            reverse = true;
            while (  IS_MBCONT(src) 
                  || IS_RTL(src)
                  || IS_DIR_NEUTRAL(src)
                  || is_misc_ws(src))
               src++;
         }
         else
            *dst++ = *src++;
      }
   }

   *dst = '\0';

   return (char*)dst_buffer;
}
#endif

void font_driver_render_msg(
      void *data,
      const char *msg,
      const void *_params,
      void *font_data)
{
   const struct font_params *params = (const struct font_params*)_params;
   font_data_t                *font = (font_data_t*)(font_data
         ? font_data : video_font_driver);

   if (msg && *msg && font && font->renderer && font->renderer->render_msg)
   {
#ifdef HAVE_LANGEXTRA
      unsigned char tmp_buffer[64];
      char *new_msg = font_driver_reshape_msg(msg, tmp_buffer, sizeof(tmp_buffer));
      font->renderer->render_msg(data,
            font->renderer_data, new_msg, params);
      if (new_msg != (char*)tmp_buffer)
         free(new_msg);
#else
      char *new_msg = (char*)msg;
      font->renderer->render_msg(data,
            font->renderer_data, new_msg, params);
#endif
   }
}

void font_driver_bind_block(void *font_data, void *block)
{
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);

   if (font && font->renderer && font->renderer->bind_block)
      font->renderer->bind_block(font->renderer_data, block);
}

void font_driver_flush(unsigned width, unsigned height, void *font_data)
{
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);
   if (font && font->renderer && font->renderer->flush)
      font->renderer->flush(width, height, font->renderer_data);
}

int font_driver_get_message_width(void *font_data,
      const char *msg, size_t len, float scale)
{
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);
   if (len == 0 && msg)
      len = strlen(msg);
   if (font && font->renderer && font->renderer->get_message_width)
      return font->renderer->get_message_width(font->renderer_data, msg, len, scale);
   return -1;
}

int font_driver_get_line_height(void *font_data, float scale)
{
   struct font_line_metrics *metrics = NULL;
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);

   /* First try the line metrics implementation */
   if (font && font->renderer && font->renderer->get_line_metrics)
      if ((font->renderer->get_line_metrics(
                  font->renderer_data, &metrics)))
         return (int)roundf(metrics->height * scale);

   /* Else return an approximation
    * (uses a fudge of standard font metrics - mostly garbage...)
    * > font_size = (width of 'a') / 0.6
    * > line_height = font_size * 1.7f */
   return (int)roundf(1.7f * (float)font_driver_get_message_width(font_data, "a", 1, scale) / 0.6f);
}

int font_driver_get_line_ascender(void *font_data, float scale)
{
   struct font_line_metrics *metrics = NULL;
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);

   /* First try the line metrics implementation */
   if (font && font->renderer && font->renderer->get_line_metrics)
      if ((font->renderer->get_line_metrics(font->renderer_data, &metrics)))
         return (int)roundf(metrics->ascender * scale);

   /* Else return an approximation
    * (uses a fudge of standard font metrics - mostly garbage...)
    * > font_size = (width of 'a') / 0.6
    * > ascender = 1.58 * font_size * 0.75 */
   return (int)roundf(1.58f * 0.75f * (float)font_driver_get_message_width(font_data, "a", 1, scale) / 0.6f);
}

int font_driver_get_line_descender(void *font_data, float scale)
{
   struct font_line_metrics *metrics = NULL;
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);

   /* First try the line metrics implementation */
   if (font && font->renderer && font->renderer->get_line_metrics)
      if ((font->renderer->get_line_metrics(font->renderer_data, &metrics)))
         return (int)roundf(metrics->descender * scale);

   /* Else return an approximation
    * (uses a fudge of standard font metrics - mostly garbage...)
    * > font_size = (width of 'a') / 0.6
    * > descender = 1.58 * font_size * 0.25 */
   return (int)roundf(1.58f * 0.25f * (float)font_driver_get_message_width(font_data, "a", 1, scale) / 0.6f);
}

int font_driver_get_line_centre_offset(void *font_data, float scale)
{
   struct font_line_metrics *metrics = NULL;
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);

   /* First try the line metrics implementation */
   if (font && font->renderer && font->renderer->get_line_metrics)
      if ((font->renderer->get_line_metrics(font->renderer_data, &metrics)))
         return (int)roundf((metrics->ascender - metrics->descender) * 0.5f * scale);

   /* Else return an approximation... */
   return (int)roundf((1.58f * 0.5f * (float)font_driver_get_message_width(font_data, "a", 1, scale) / 0.6f) / 2.0f);
}

void font_driver_free(void *font_data)
{
   font_data_t *font = (font_data_t*)font_data;

   if (font)
   {
      bool is_threaded        = false;
#ifdef HAVE_THREADS
      bool *is_threaded_tmp   = video_driver_get_threaded();
      is_threaded             = *is_threaded_tmp;
#endif

      if (font->renderer && font->renderer->free)
         font->renderer->free(font->renderer_data, is_threaded);

      font->renderer      = NULL;
      font->renderer_data = NULL;

      free(font);
   }
}

font_data_t *font_driver_init_first(
      void *video_data, const char *font_path, float font_size,
      bool threading_hint, bool is_threaded,
      enum font_driver_render_api api)
{
   const void *font_driver = NULL;
   void *font_handle       = NULL;
   bool ok                 = false;
#ifdef HAVE_THREADS
   if (     threading_hint
         && is_threaded
         && !video_driver_is_hw_context())
      ok = video_thread_font_init(&font_driver, &font_handle,
            video_data, font_path, font_size, api, font_init_first,
            is_threaded);
   else
#endif
   ok = font_init_first(&font_driver, &font_handle,
         video_data, font_path, font_size, api, is_threaded);

   if (ok)
   {
      font_data_t *font      = (font_data_t*)malloc(sizeof(*font));

      if (font)
      {
         font->renderer      = (const font_renderer_t*)font_driver;
         font->renderer_data = font_handle;
         font->size          = font_size;
         return font;
      }
   }

   return NULL;
}

void font_driver_init_osd(
      void *video_data,
      const void *video_info_data,
      bool threading_hint,
      bool is_threaded,
      enum font_driver_render_api api)
{
   const video_info_t *video_info = (const video_info_t*)video_info_data;
   if (!video_font_driver && video_info)
      video_font_driver = font_driver_init_first(video_data,
            *video_info->path_font ? video_info->path_font : NULL,
            video_info->font_size, threading_hint, is_threaded, api);
}

void font_driver_free_osd(void)
{
   if (video_font_driver)
      font_driver_free(video_font_driver);

   video_font_driver = NULL;
}
