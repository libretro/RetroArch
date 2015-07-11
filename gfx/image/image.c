/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <formats/image.h>
#ifdef HAVE_RPNG
#include <formats/rpng.h>
#endif
#include <formats/tga.h>
#ifdef _XBOX1
#include "../d3d/d3d_wrapper.h"
#endif
#include "../../file_ops.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <boolean.h>
#include "../../general.h"

#if defined(__CELLOS_LV2__) || defined(__PSLIGHT__)
#include "../../ps3/sdk_defines.h"

#ifdef __PSL1GHT__
#include <ppu-asm.h>
#include <ppu-types.h>
#include <pngdec/pngdec.h>
#else
#include <cell/codec.h>
#endif

#endif

bool texture_image_set_color_shifts(unsigned *r_shift, unsigned *g_shift,
      unsigned *b_shift, unsigned *a_shift)
{
   driver_t *driver     = driver_get_ptr();
   /* This interface "leak" is very ugly. FIXME: Fix this properly ... */
   bool use_rgba    = driver ? driver->gfx_use_rgba : false;
   *a_shift = 24;
   *r_shift = use_rgba ? 0 : 16;
   *g_shift = 8;
   *b_shift = use_rgba ? 16 : 0;

   return use_rgba;
}

bool texture_image_color_convert(unsigned r_shift,
      unsigned g_shift, unsigned b_shift, unsigned a_shift,
      struct texture_image *out_img)
{
   /* This is quite uncommon. */
   if (a_shift != 24 || r_shift != 16 || g_shift != 8 || b_shift != 0)
   {
      uint32_t i;
      uint32_t num_pixels = out_img->width * out_img->height;
      uint32_t *pixels    = (uint32_t*)out_img->pixels;

      for (i = 0; i < num_pixels; i++)
      {
         uint32_t col = pixels[i];
         uint8_t a = (uint8_t)(col >> 24);
         uint8_t r = (uint8_t)(col >> 16);
         uint8_t g = (uint8_t)(col >>  8);
         uint8_t b = (uint8_t)(col >>  0);
         pixels[i] = (a << a_shift) |
            (r << r_shift) | (g << g_shift) | (b << b_shift);
      }

      return true;
   }

   return false;
}

#ifdef HAVE_RPNG
static bool rpng_image_load_argb_shift(const char *path,
      struct texture_image *out_img,
      unsigned a_shift, unsigned r_shift,
      unsigned g_shift, unsigned b_shift)
{
   bool ret = rpng_load_image_argb(path,
         &out_img->pixels, &out_img->width, &out_img->height);

   if (!ret)
   {
      out_img->pixels = NULL;
      out_img->width  = out_img->height = 0;
      return false;
   }

   texture_image_color_convert(r_shift, g_shift, b_shift,
         a_shift, out_img);

   return true;
}
#endif

#ifdef GEKKO

#define GX_BLIT_LINE_32(off) \
{ \
   unsigned x; \
   const uint16_t *tmp_src = src; \
   uint16_t       *tmp_dst = dst; \
   for (x = 0; x < width2 >> 3; x++, tmp_src += 8, tmp_dst += 32) \
   { \
      tmp_dst[  0 + off] = tmp_src[0]; \
      tmp_dst[ 16 + off] = tmp_src[1]; \
      tmp_dst[  1 + off] = tmp_src[2]; \
      tmp_dst[ 17 + off] = tmp_src[3]; \
      tmp_dst[  2 + off] = tmp_src[4]; \
      tmp_dst[ 18 + off] = tmp_src[5]; \
      tmp_dst[  3 + off] = tmp_src[6]; \
      tmp_dst[ 19 + off] = tmp_src[7]; \
   } \
   src += tmp_pitch; \
}

static bool rpng_gx_convert_texture32(struct texture_image *image)
{
   unsigned tmp_pitch, width2, i;
   const uint16_t *src = NULL;
   uint16_t *dst       = NULL;
   /* Memory allocation in libogc is extremely primitive so try 
    * to avoid gaps in memory when converting by copying over to 
    * a temporary buffer first, then converting over into 
    * main buffer again. */
   void *tmp           = malloc(image->width * image->height * sizeof(uint32_t));

   if (!tmp)
   {
      RARCH_ERR("Failed to create temp buffer for conversion.\n");
      return false;
   }

   memcpy(tmp, image->pixels, image->width * image->height * sizeof(uint32_t));
   tmp_pitch = (image->width * sizeof(uint32_t)) >> 1;

   image->width       &= ~3;
   image->height      &= ~3;
   width2              = image->width << 1;
   src                 = (uint16_t*)tmp;
   dst                 = (uint16_t*)image->pixels;

   for (i = 0; i < image->height; i += 4, dst += 4 * width2)
   {
      GX_BLIT_LINE_32(0)
      GX_BLIT_LINE_32(4)
      GX_BLIT_LINE_32(8)
      GX_BLIT_LINE_32(12)
   }

   free(tmp);
   return true;
}

#endif

void texture_image_free(struct texture_image *img)
{
#ifdef _XBOX1
   LPDIRECT3DTEXTURE d3dt = (LPDIRECT3DTEXTURE)img->texture_buf;
   LPDIRECT3DVERTEXBUFFER d3dv = (LPDIRECT3DVERTEXBUFFER)img->vertex_buf;
#endif
   if (!img)
      return;

#ifdef _XBOX1
   d3d_vertex_buffer_free(d3dv, NULL);
   d3d_texture_free(d3dt);
#endif
   if (img->pixels)
      free(img->pixels);
   memset(img, 0, sizeof(*img));
}

#ifdef _XBOX1
#include "../d3d/d3d.h"

bool texture_image_load(struct texture_image *out_img, const char *path)
{
   D3DXIMAGE_INFO m_imageInfo;
   driver_t *driver     = driver_get_ptr();
   d3d_video_t *d3d     = (d3d_video_t*)driver->video_data;
   LPDIRECT3DTEXTURE d3dt = (LPDIRECT3DTEXTURE)out_img->texture_buf;
   LPDIRECT3DVERTEXBUFFER d3dv = (LPDIRECT3DVERTEXBUFFER)out_img->vertex_buf;

   d3dv = NULL;

   d3dt = d3d_texture_new(d3d->dev, path,
         D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0,
         D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, D3DX_DEFAULT,
         D3DX_DEFAULT, 0, &m_imageInfo, NULL);

   if (!d3dt)
      return false;

   /* create a vertex buffer for the quad that will display the texture */
   d3dv = (LPDIRECT3DVERTEXBUFFER)d3d_vertex_buffer_new(
         d3d->dev, 4 * sizeof(Vertex), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX,
         D3DPOOL_MANAGED, NULL);

   if (!d3dv)
   {
      d3d_texture_free(d3dt);
      return false;
   }

   out_img->width       = m_imageInfo.Width;
   out_img->height      = m_imageInfo.Height;

   return true;
}
#elif defined(__CELLOS_LV2__)
typedef struct CtrlMallocArg
{
   uint32_t mallocCallCounts;
} CtrlMallocArg;

typedef struct CtrlFreeArg
{
   uint32_t freeCallCounts;
} CtrlFreeArg;

void *img_malloc(uint32_t size, void *a)
{
#ifndef __PSL1GHT__
   CtrlMallocArg *arg;
   arg = (CtrlMallocArg *) a;
   arg->mallocCallCounts++;
#endif

   return malloc(size);
}

static int img_free(void *ptr, void *a)
{
#ifndef __PSL1GHT__
   CtrlFreeArg *arg;
   arg = (CtrlFreeArg *) a;
   arg->freeCallCounts++;
#endif

   free(ptr);
   return 0;
}

static bool ps3_load_png(const char *path, struct texture_image *out_img)
{
#ifdef __PSL1GHT__
   uint64_t output_bytes_per_line;
#else
   CtrlMallocArg                 MallocArg;
   CtrlFreeArg                   FreeArg;
   CellPngDecDataCtrlParam       dCtrlParam;
#endif
   CellPngDecThreadInParam       InParam;
   CellPngDecThreadOutParam      OutParam;
   CellPngDecSrc                 src;
   CellPngDecOpnInfo             opnInfo;
   CellPngDecInfo                info;
   CellPngDecInParam             inParam;
   CellPngDecOutParam            outParam;
   CellPngDecDataOutInfo         dOutInfo;
   size_t                        img_size;
   int                           ret_png, ret = -1;
   CellPngDecMainHandle          mHandle = PTR_NULL;
   CellPngDecSubHandle           sHandle = PTR_NULL;

   InParam.spu_enable         = CELL_PNGDEC_SPU_THREAD_ENABLE;
   InParam.ppu_prio           = 512;
   InParam.spu_prio           = 200;
#ifdef __PSL1GHT__
   InParam.malloc_func        = __get_addr32(__get_opd32(img_malloc));
   InParam.free_func          = __get_addr32(__get_opd32(img_free));
   InParam.malloc_arg         = 0;
   InParam.free_arg           = 0;
#else
   MallocArg.mallocCallCounts = 0;
   FreeArg.freeCallCounts     = 0;
   InParam.malloc_func        = img_malloc;
   InParam.malloc_arg         = &MallocArg;
   InParam.free_func          = img_free;
   InParam.free_arg           = &FreeArg;
#endif

   ret_png = cellPngDecCreate(&mHandle, &InParam, &OutParam);

   if (ret_png != CELL_OK)
      goto error;

   memset(&src, 0, sizeof(CellPngDecSrc));

   src.stream_select    = CELL_PNGDEC_FILE;
#ifdef __PSL1GHT__
   src.file_name        = __get_addr32(path);
#else
   src.file_name        = path;
#endif
   src.file_offset      = 0;
   src.file_size        = 0;
   src.stream_ptr       = 0;
   src.stream_size      = 0;
   src.spu_enable       = CELL_PNGDEC_SPU_THREAD_ENABLE;

   ret = cellPngDecOpen(mHandle, &sHandle, &src, &opnInfo);

   if (ret != CELL_OK)
      goto error;

   ret = cellPngDecReadHeader(mHandle, sHandle, &info);

   if (ret != CELL_OK)
      goto error;

   inParam.cmd_ptr            = PTR_NULL;
   inParam.output_mode        = CELL_PNGDEC_TOP_TO_BOTTOM;
   inParam.color_space        = CELL_PNGDEC_ARGB;
   inParam.bit_depth          = 8;
   inParam.pack_flag          = CELL_PNGDEC_1BYTE_PER_1PIXEL;
   inParam.alpha_select       = CELL_PNGDEC_STREAM_ALPHA;

   ret = cellPngDecSetParameter(mHandle, sHandle, &inParam, &outParam);

   if (ret != CELL_OK)
      goto error;

   img_size = outParam.output_width * 
      outParam.output_height * sizeof(uint32_t);
   out_img->pixels = (uint32_t*)malloc(img_size);

   if (!out_img->pixels)
      goto error;

   memset(out_img->pixels, 0, img_size);

#ifdef __PSL1GHT__
   output_bytes_per_line = outParam.output_width * 4;
   ret = cellPngDecDecodeData(mHandle, sHandle, (uint8_t*)
         out_img->pixels, &output_bytes_per_line, &dOutInfo);
#else
   dCtrlParam.output_bytes_per_line = outParam.output_width * 4;
   ret = cellPngDecDecodeData(mHandle, sHandle, (uint8_t*)
         out_img->pixels, &dCtrlParam, &dOutInfo);
#endif

   if (ret != CELL_OK || dOutInfo.status != CELL_PNGDEC_DEC_STATUS_FINISH)
      goto error;

   out_img->width = outParam.output_width;
   out_img->height = outParam.output_height;

   cellPngDecClose(mHandle, sHandle);
   cellPngDecDestroy(mHandle);

   return true;

error:
   if (out_img->pixels)
      free(out_img->pixels);
   out_img->pixels = 0;
   if (mHandle && sHandle)
      cellPngDecClose(mHandle, sHandle);
   if (mHandle)
      cellPngDecDestroy(mHandle);

   return false;
}


bool texture_image_load(struct texture_image *out_img, const char *path)
{
   if (!out_img)
      return false;

   if (!ps3_load_png(path, out_img))
      return false;

   return true;
}
#else
bool texture_image_load(struct texture_image *out_img, const char *path)
{
   bool ret = false;
   unsigned r_shift, g_shift, b_shift, a_shift;

   texture_image_set_color_shifts(&r_shift, &g_shift, &b_shift,
         &a_shift);

   (void)ret;

   if (strstr(path, ".tga"))
   {
      void *raw_buf = NULL;
      uint8_t *buf = NULL;
      ssize_t len;
      ret = read_file(path, &raw_buf, &len);

      if (!ret || len < 0)
      {
         RARCH_ERR("Failed to read image: %s.\n", path);
         return false;
      }

      buf = (uint8_t*)raw_buf;

      ret = rtga_image_load_shift(buf, out_img,
            a_shift, r_shift, g_shift, b_shift);

      if (buf)
         free(buf);
   }
#ifdef HAVE_RPNG
   else if (strstr(path, ".png"))
   {
      ret = rpng_image_load_argb_shift(path, out_img,
            a_shift, r_shift, g_shift, b_shift);
   }

#ifdef GEKKO
   if (ret)
   {
      if (!rpng_gx_convert_texture32(out_img))
      {
         texture_image_free(out_img);
         return false;
      }
   }
#endif

#endif

   return ret;
}
#endif
