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

#include "../configuration.h"
#include "../retroarch.h"
#include "../verbosity.h"

static const font_renderer_driver_t *font_backends[] = {
#ifdef HAVE_FREETYPE
   &freetype_font_renderer,
#endif
#if defined(__APPLE__) && defined(HAVE_CORETEXT)
   &coretext_font_renderer,
#endif
#ifdef HAVE_STB_FONT
#if defined(VITA) || defined(WIIU) || defined(ANDROID) || (defined(_WIN32) && !defined(_XBOX) && !defined(_MSC_VER) && _MSC_VER >= 1400) || (defined(_WIN32) && !defined(_XBOX) && defined(_MSC_VER)) || defined(__CELLOS_LV2__) || defined(HAVE_LIBNX) || defined(__linux__) || defined (HAVE_EMSCRIPTEN) || defined(__APPLE__)
   &stb_unicode_font_renderer,
#else
   &stb_font_renderer,
#endif
#endif
   &bitmap_font_renderer,
   NULL
};

static void *video_font_driver = NULL;

int font_renderer_create_default(
      const font_renderer_driver_t **drv,
      void **handle,
      const char *font_path, unsigned font_size)
{
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
         RARCH_LOG("[Font]: Using font rendering backend: %s.\n",
               font_backends[i]->ident);
         *drv = font_backends[i];
         return 1;
      }
      else
         RARCH_ERR("Failed to create rendering backend: %s.\n",
               font_backends[i]->ident);
   }

   *drv    = NULL;
   *handle = NULL;

   return 0;
}

#ifdef HAVE_D3D8
static const font_renderer_t *d3d8_font_backends[] = {
#if defined(_XBOX1)
   &d3d_xdk1_font,
#endif
   NULL
};

static bool d3d8_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
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
static const font_renderer_t *d3d9_font_backends[] = {
#if defined(_WIN32) && defined(HAVE_D3DX)
   &d3d_win32_font,
#endif
   NULL
};

static bool d3d9_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
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
static const font_renderer_t *gl1_font_backends[] = {
   &gl1_raster_font,
   NULL,
};

static bool gl1_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; gl1_font_backends[i]; i++)
   {
      void *data = gl1_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = gl1_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#if defined(HAVE_OPENGL)
static const font_renderer_t *gl_font_backends[] = {
   &gl_raster_font,
   NULL,
};

static bool gl_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; gl_font_backends[i]; i++)
   {
      void *data = gl_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = gl_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_OPENGL_CORE
static const font_renderer_t *gl_core_font_backends[] = {
   &gl_core_raster_font,
   NULL,
};

static bool gl_core_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; gl_core_font_backends[i]; i++)
   {
      void *data = gl_core_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = gl_core_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_CACA
static const font_renderer_t *caca_font_backends[] = {
   &caca_font,
   NULL,
};

static bool caca_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; caca_font_backends[i]; i++)
   {
      void *data = caca_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = caca_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_SIXEL
static const font_renderer_t *sixel_font_backends[] = {
   &sixel_font,
   NULL,
};

static bool sixel_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; sixel_font_backends[i]; i++)
   {
      void *data = sixel_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = sixel_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef DJGPP
static const font_renderer_t *vga_font_backends[] = {
   &vga_font,
   NULL,
};

static bool vga_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; vga_font_backends[i]; i++)
   {
      void *data = vga_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = vga_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_GDI
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
static const font_renderer_t *gdi_font_backends[] = {
   &gdi_font,
   NULL,
};

static bool gdi_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; gdi_font_backends[i]; i++)
   {
      void *data = gdi_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = gdi_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif
#endif

#ifdef HAVE_VULKAN
static const font_renderer_t *vulkan_font_backends[] = {
   &vulkan_raster_font,
   NULL,
};

static bool vulkan_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; vulkan_font_backends[i]; i++)
   {
      void *data = vulkan_font_backends[i]->init(video_data,
            font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = vulkan_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_METAL
static const font_renderer_t *metal_font_backends[] = {
   &metal_raster_font,
   NULL,
};

static bool metal_font_init_first(
   const void **font_driver, void **font_handle,
   void *video_data, const char *font_path,
   float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; metal_font_backends[i]; i++)
   {
      void *data = metal_font_backends[i]->init(video_data,
                                                 font_path, font_size,
                                                 is_threaded);

      if (!data)
         continue;

      *font_driver = metal_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_D3D10
static const font_renderer_t *d3d10_font_backends[] = {
   &d3d10_font,
   NULL,
};

static bool d3d10_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; d3d10_font_backends[i]; i++)
   {
      void *data = d3d10_font_backends[i]->init(video_data,
            font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = d3d10_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_D3D11
static const font_renderer_t *d3d11_font_backends[] = {
   &d3d11_font,
   NULL,
};

static bool d3d11_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; d3d11_font_backends[i]; i++)
   {
      void *data = d3d11_font_backends[i]->init(video_data,
            font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = d3d11_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_D3D12
static const font_renderer_t *d3d12_font_backends[] = {
   &d3d12_font,
   NULL,
};

static bool d3d12_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; d3d12_font_backends[i]; i++)
   {
      void *data = d3d12_font_backends[i]->init(video_data,
            font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = d3d12_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef PS2
static const font_renderer_t *ps2_font_backends[] = {
   &ps2_font
};

static bool ps2_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; ps2_font_backends[i]; i++)
   {
      void *data = ps2_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = ps2_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_VITA2D
static const font_renderer_t *vita2d_font_backends[] = {
   &vita2d_vita_font
};

static bool vita2d_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; vita2d_font_backends[i]; i++)
   {
      void *data = vita2d_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = vita2d_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef _3DS
static const font_renderer_t *ctr_font_backends[] = {
   &ctr_font
};

static bool ctr_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; ctr_font_backends[i]; i++)
   {
      void *data = ctr_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = ctr_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_LIBNX
static const font_renderer_t *switch_font_backends[] = {
   &switch_font,
   NULL
};

static bool switch_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; switch_font_backends[i]; i++)
   {
      void *data = switch_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = switch_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef WIIU
static const font_renderer_t *wiiu_font_backends[] = {
   &wiiu_font,
   NULL
};

static bool wiiu_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; wiiu_font_backends[i]; i++)
   {
      void *data = wiiu_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = wiiu_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
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
         return gl_core_font_init_first(font_driver, font_handle,
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

/* ACII:        0xxxxxxx  (c & 0x80) == 0x00
 * other start: 11xxxxxx  (c & 0xC0) == 0xC0
 * other cont:  10xxxxxx  (c & 0xC0) == 0x80
 * Neutral :
 * 0020 - 002F : 001xxxxx (c & 0xE0) == 0x20
 * Arabic:
 * 0600 - 07FF : 11011xxx (c & 0xF8) == 0xD8 (2 bytes)
 * 0800 - 08FF : 11100000 101000xx  c == 0xE0 && (c1 & 0xAC) == 0xA0 (3 bytes) */

/* clang-format off */
#define IS_ASCII(p)        ((*(p)&0x80) == 0x00)
#define IS_MBSTART(p)      ((*(p)&0xC0) == 0xC0)
#define IS_MBCONT(p)       ((*(p)&0xC0) == 0x80)
#define IS_DIR_NEUTRAL(p)  ((*(p)&0xE0) == 0x20)
#define IS_ARABIC0(p)      ((*(p)&0xF8) == 0xD8)
#define IS_ARABIC1(p)      ((*(p) == 0xE0) && ((*((p) + 1) & 0xAC) == 0xA0))
#define IS_ARABIC(p)       (IS_ARABIC0(p) || IS_ARABIC1(p))
#define IS_RTL(p)          IS_ARABIC(p)

/* 0x0620 to 0x064F */
static const unsigned arabic_shape_map[0x50 - 0x20][0x4] = {
   { 0 },                              /* 0x0620 */
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
   { 0 },
   { 0 },
   { 0 },
   { 0 },
   { 0 },

   { 0xFED1, 0xFED2, 0xFED3, 0xFED4 }, /* 0x0640 */
   { 0xFED5, 0xFED6, 0xFED7, 0xFED8 },
   { 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC },
   { 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0 },
   { 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4 },
   { 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8 },
   { 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC },
   { 0xFEED, 0xFEEE },

   { 0xFEEF, 0xFEF0, 0xFBE8, 0xFBE9 },
   { 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4 },
};
/* clang-format on */

static INLINE unsigned font_get_replacement(const char* src, const char* start)
{
   if ((*src & 0xFC) == 0xD8) /* 0x0600 to 0x06FF */
   {
      unsigned      result         = 0;
      bool          prev_connected = false;
      bool          next_connected = false;
      unsigned char id             = ((unsigned char)src[0] << 6) | ((unsigned char)src[1] & 0x3F);
      const char*   prev1          = src - 2;
      const char*   prev2          = src - 4;

      if (id < 0x21 || id > 0x4A)
         return 0;

      if (prev2 < start)
      {
         prev2 = NULL;
         if (prev1 < start)
            prev1 = NULL;
      }

      if (prev1 && (*prev1 & 0xFC) == 0xD8)
      {
         unsigned char prev1_id = 0;

         if (prev1)
            prev1_id = ((unsigned char)prev1[0] << 6) | ((unsigned char)prev1[1] & 0x3F);

         if (prev1_id == 0x44)
         {
            unsigned char prev2_id = 0;

            if (prev2)
               prev2_id = (prev2[0] << 6) | (prev2[1] & 0x3F);

            if (prev2_id > 0x20 || prev2_id < 0x50)
               prev_connected = !!arabic_shape_map[prev2_id - 0x20][2];

            switch (id)
            {
               case 0x22:
                  return 0xFEF5 + prev_connected;
               case 0x23:
                  return 0xFEF7 + prev_connected;
               case 0x25:
                  return 0xFEF9 + prev_connected;
               case 0x27:
                  return 0xFEFB + prev_connected;
            }
         }
         if (prev1_id > 0x20 || prev1_id < 0x50)
            prev_connected = !!arabic_shape_map[prev1_id - 0x20][2];
      }

      if ((src[2] & 0xFC) == 0xD8)
      {
         unsigned char next_id = ((unsigned char)src[2] << 6) | ((unsigned char)src[3] & 0x3F);

         if (next_id > 0x20 || next_id < 0x50)
            next_connected = true;
      }

      result = arabic_shape_map[id - 0x20][prev_connected | (next_connected << 1)];

      if (result)
         return result;

      return arabic_shape_map[id - 0x20][prev_connected];
   }

   return 0;
}

static char* font_driver_reshape_msg(const char* msg)
{
   /* worst case transformations are 2 bytes to 4 bytes */
   unsigned char*       buffer  = (unsigned char*)malloc((strlen(msg) * 2) + 1);
   const unsigned char* src     = (const unsigned char*)msg;
   unsigned char*       dst     = (unsigned char*)buffer;
   bool                 reverse = false;

   while (*src || reverse)
   {
      if (reverse)
      {
         src--;
         while (IS_MBCONT(src))
         {
            src--;

            if (src == (const unsigned char*)msg)
               goto end;
         }

         if (IS_RTL(src) || IS_DIR_NEUTRAL(src))
         {
            unsigned replacement = font_get_replacement((const char*)src, msg);
            if (replacement)
            {
               if (replacement < 0x80)
                  *dst++ = replacement;
               else if (replacement < 0x8000)
               {
                  *dst++ = 0xC0 | (replacement >> 6);
                  *dst++ = 0x80 | (replacement & 0x3F);
               }
               else if (replacement < 0x10000)
               {
                  /* merged glyphs */
                  if ((replacement >= 0xFEF5) && (replacement <= 0xFEFC))
                     src -= 2;

                  *dst++ = 0xE0 | (replacement >> 12);
                  *dst++ = 0x80 | ((replacement >> 6) & 0x3F);
                  *dst++ = 0x80 | (replacement & 0x3F);
               }
               else
               {
                  *dst++ = 0xF0 | (replacement >> 18);
                  *dst++ = 0x80 | ((replacement >> 12) & 0x3F);
                  *dst++ = 0x80 | ((replacement >> 6) & 0x3F);
                  *dst++ = 0x80 | (replacement & 0x3F);
               }

               continue;
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
            while (IS_MBCONT(src) || IS_RTL(src) || IS_DIR_NEUTRAL(src))
               src++;
         }
      }
      else
      {
         if (IS_RTL(src))
         {
            reverse = true;
            while (IS_MBCONT(src) || IS_RTL(src) || IS_DIR_NEUTRAL(src))
               src++;
         }
         else
            *dst++ = *src++;
      }
   }
end:
   *dst = '\0';

   return (char*)buffer;
}
#endif

void font_driver_render_msg(
      void *data,
      video_frame_info_t *video_info,
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
      char *new_msg = font_driver_reshape_msg(msg);
#else
      char *new_msg = (char*)msg;
#endif

      font->renderer->render_msg(video_info,
            font->renderer_data, new_msg, params);
#ifdef HAVE_LANGEXTRA
      free(new_msg);
#endif
   }
}

void font_driver_bind_block(void *font_data, void *block)
{
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);

   if (font && font->renderer && font->renderer->bind_block)
      font->renderer->bind_block(font->renderer_data, block);
}

void font_driver_flush(unsigned width, unsigned height, void *font_data,
      video_frame_info_t *video_info)
{
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);
   if (font && font->renderer && font->renderer->flush)
      font->renderer->flush(width, height, font->renderer_data, video_info);
}

int font_driver_get_message_width(void *font_data,
      const char *msg, unsigned len, float scale)
{
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);
   if (len == 0 && msg)
      len = (unsigned)strlen(msg);
   if (font && font->renderer && font->renderer->get_message_width)
      return font->renderer->get_message_width(font->renderer_data, msg, len, scale);
   return -1;
}

int font_driver_get_line_height(void *font_data, float scale)
{
   int line_height;
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);

   /* First try the line height implementation */
   if (font && font->renderer && font->renderer->get_line_height)
      if ((line_height = font->renderer->get_line_height(font->renderer_data)) != -1)
         return (int)(line_height * roundf(scale));

   /* Else return an approximation (width of 'a') */
   return font_driver_get_message_width(font_data, "a", 1, scale);
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
      font_data_t *font   = (font_data_t*)calloc(1, sizeof(*font));
      font->renderer      = (const font_renderer_t*)font_driver;
      font->renderer_data = font_handle;
      font->size          = font_size;
      return font;
   }

   return NULL;
}

void font_driver_init_osd(
      void *video_data,
      bool threading_hint,
      bool is_threaded,
      enum font_driver_render_api api)
{
   settings_t *settings = config_get_ptr();
   if (video_font_driver)
      return;

   video_font_driver = font_driver_init_first(video_data,
         *settings->paths.path_font ? settings->paths.path_font : NULL,
         settings->floats.video_font_size, threading_hint, is_threaded, api);

   if (!video_font_driver)
      RARCH_ERR("[font]: Failed to initialize OSD font.\n");
}

void font_driver_free_osd(void)
{
   if (video_font_driver)
      font_driver_free(video_font_driver);

   video_font_driver = NULL;
}
