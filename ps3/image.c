/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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
#include "../config.h"
#endif

#include "../gfx/image.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "../general.h"

#include "sdk_defines.h"

#ifdef __PSL1GHT__
#include <ppu-asm.h>
#include <ppu-types.h>
#include <jpgdec/jpgdec.h>
#include <pngdec/pngdec.h>
#else
#include <cell/codec.h>
#endif

/******************************************************************************* 
	Image decompression - structs
********************************************************************************/

typedef struct CtrlMallocArg
{
   uint32_t mallocCallCounts;
} CtrlMallocArg;

typedef struct CtrlFreeArg
{
   uint32_t freeCallCounts;
} CtrlFreeArg;

void *img_malloc(uint32_t size, void * a)
{
#ifndef __PSL1GHT__
   CtrlMallocArg *arg;
   arg = (CtrlMallocArg *) a;
   arg->mallocCallCounts++;
#endif

   return malloc(size);
}

static int img_free(void *ptr, void * a)
{
#ifndef __PSL1GHT__
   CtrlFreeArg *arg;
   arg = (CtrlFreeArg *) a;
   arg->freeCallCounts++;
#endif

   free(ptr);
   return 0;
}

/******************************************************************************* 
	Image decompression - libJPEG
********************************************************************************/

static bool ps3graphics_load_jpeg(const char * path, struct texture_image *out_img)
{
#ifndef __PSL1GHT__
   CtrlMallocArg              MallocArg;
   CtrlFreeArg                FreeArg;
   pJpgDecDataCtrlParam       dCtrlParam;
#endif
   pJpgDecMainHandle          mHandle = PTR_NULL;
   pJpgDecSubHandle           sHandle = PTR_NULL;
   pJpgDecThreadInParam       InParam;
   pJpgDecThreadOutParam      OutParam;
   pJpgDecSrc                 src;
   pJpgDecOpnInfo             opnInfo;
   pJpgDecInfo                info;
   pJpgDecInParam             inParam;
   pJpgDecOutParam            outParam;
   pJpgDecDataOutInfo         dOutInfo;

   InParam.spu_enable = CELL_JPGDEC_SPU_THREAD_ENABLE;
   InParam.ppu_prio = 1001;
   InParam.spu_prio = 250;
#ifdef __PSL1GHT__
   InParam.malloc_func = __get_addr32(__get_opd32(img_malloc));
   InParam.free_func = __get_addr32(__get_opd32(img_free));
   InParam.malloc_arg = 0;
   InParam.free_arg = 0;
#else
   MallocArg.mallocCallCounts = 0;
   FreeArg.freeCallCounts = 0;
   InParam.malloc_func = img_malloc;
   InParam.free_func = img_free;
   InParam.malloc_arg = &MallocArg;
   InParam.free_arg = &FreeArg;
#endif

   int ret_jpeg, ret = -1;
   ret_jpeg = pJpgDecCreate(&mHandle, &InParam, &OutParam);

   if (ret_jpeg != CELL_OK)
      goto error;

   memset(&src, 0, sizeof(pJpgDecSrc));
   src.stream_select    = CELL_JPGDEC_FILE;
#ifdef __PSL1GHT__
   src.file_name        = __get_addr32(path);
#else
   src.file_name        = path;
#endif
   src.file_offset      = 0;
   src.file_size        = 0;
   src.stream_ptr       = PTR_NULL;
   src.stream_size      = 0;

   src.spu_enable  = CELL_JPGDEC_SPU_THREAD_ENABLE;

   ret = pJpgDecOpen(mHandle, &sHandle, &src, &opnInfo);

   if (ret != CELL_OK)
      goto error;

   ret = pJpgDecReadHeader(mHandle, sHandle, &info);

   if (ret != CELL_OK)
      goto error;

   inParam.cmd_ptr            = PTR_NULL;
   inParam.quality            = CELL_JPGDEC_FAST;
   inParam.output_mode        = CELL_JPGDEC_TOP_TO_BOTTOM;
   inParam.color_space        = CELL_JPG_ARGB;
   inParam.down_scale         = 1;
   inParam.color_alpha        = 0xfe;
   ret = pJpgDecSetParameter(mHandle, sHandle, &inParam, &outParam);

   if (ret != CELL_OK)
      goto error;

#ifdef __PSL1GHT__
   uint64_t output_bytes_per_line = outParam.output_width * 4;
   ret = pJpgDecDecodeData(mHandle, sHandle, (uint8_t*)out_img->pixels, &output_bytes_per_line, &dOutInfo);
#else
   dCtrlParam.output_bytes_per_line = outParam.output_width * 4;
   ret = pJpgDecDecodeData(mHandle, sHandle, (uint8_t*)out_img->pixels, &dCtrlParam, &dOutInfo);
#endif

   if (ret != CELL_OK || dOutInfo.status != CELL_JPGDEC_DEC_STATUS_FINISH)
      goto error;

   out_img->width = outParam.output_width;
   out_img->height = outParam.output_height;

   pJpgDecClose(mHandle, sHandle);
   pJpgDecDestroy(mHandle);

   return true;

error:
   RARCH_ERR("ps3graphics_load_jpeg(): error.\n");
   if (mHandle && sHandle)
	   pJpgDecClose(mHandle, sHandle);
   if (mHandle)
	   pJpgDecDestroy(mHandle);
   return false;
}

/******************************************************************************* 
	Image decompression - libPNG
********************************************************************************/

static bool ps3graphics_load_png(const char * path, struct texture_image *out_img)
{
#ifndef __PSL1GHT__
   CtrlMallocArg              MallocArg;
   CtrlFreeArg                FreeArg;
   pPngDecDataCtrlParam       dCtrlParam;
#endif
   pPngDecMainHandle          mHandle = PTR_NULL;
   pPngDecSubHandle           sHandle = PTR_NULL;
   pPngDecThreadInParam       InParam;
   pPngDecThreadOutParam      OutParam;
   pPngDecSrc                 src;
   pPngDecOpnInfo             opnInfo;
   pPngDecInfo                info;
   pPngDecInParam             inParam;
   pPngDecOutParam            outParam;
   pPngDecDataOutInfo         dOutInfo;

   InParam.spu_enable         = CELL_PNGDEC_SPU_THREAD_ENABLE;
   InParam.ppu_prio           = 512;
   InParam.spu_prio           = 200;
#ifdef __PSL1GHT__
   InParam.malloc_func = __get_addr32(__get_opd32(img_malloc));
   InParam.free_func = __get_addr32(__get_opd32(img_free));
   InParam.malloc_arg = 0;
   InParam.free_arg = 0;
#else
   MallocArg.mallocCallCounts = 0;
   FreeArg.freeCallCounts     = 0;
   InParam.malloc_func        = img_malloc;
   InParam.malloc_arg         = &MallocArg;
   InParam.free_func          = img_free;
   InParam.free_arg           = &FreeArg;
#endif

   int ret_png, ret = -1;
   ret_png = pPngDecCreate(&mHandle, &InParam, &OutParam);

   if (ret_png != CELL_OK)
      goto error;

   memset(&src, 0, sizeof(pPngDecSrc));
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

   src.spu_enable  = CELL_PNGDEC_SPU_THREAD_ENABLE;

   ret = pPngDecOpen(mHandle, &sHandle, &src, &opnInfo);

   if (ret != CELL_OK)
      goto error;

   ret = pPngDecReadHeader(mHandle, sHandle, &info);

   if (ret != CELL_OK)
      goto error;

   inParam.cmd_ptr            = PTR_NULL;
   inParam.output_mode        = CELL_PNGDEC_TOP_TO_BOTTOM;
   inParam.color_space        = CELL_PNGDEC_ARGB;
   inParam.bit_depth          = 8;
   inParam.pack_flag          = CELL_PNGDEC_1BYTE_PER_1PIXEL;
   inParam.alpha_select       = CELL_PNGDEC_STREAM_ALPHA;
   ret = pPngDecSetParameter(mHandle, sHandle, &inParam, &outParam);

   if (ret != CELL_OK)
      goto error;

#ifdef __PSL1GHT__
   uint64_t output_bytes_per_line = outParam.output_width * 4;
   ret = pPngDecDecodeData(mHandle, sHandle, (uint8_t*)out_img->pixels, &output_bytes_per_line, &dOutInfo);
#else
   dCtrlParam.output_bytes_per_line = outParam.output_width * 4;
   ret = pPngDecDecodeData(mHandle, sHandle, (uint8_t*)out_img->pixels, &dCtrlParam, &dOutInfo);
#endif

   if (ret != CELL_OK || dOutInfo.status != CELL_PNGDEC_DEC_STATUS_FINISH)
      goto error;

   out_img->width = outParam.output_width;
   out_img->height = outParam.output_height;

   pPngDecClose(mHandle, sHandle);
   pPngDecDestroy(mHandle);

   return true;

error:
   RARCH_ERR("ps3graphics_load_png(): error.\n");

   if (mHandle && sHandle)
      pPngDecClose(mHandle, sHandle);
   if (mHandle)
      pPngDecDestroy(mHandle);

   return false;
}

bool texture_image_load(const char *path, struct texture_image *out_img)
{
   out_img->pixels = malloc(2048 * 2048 * 4);
   memset(out_img->pixels, 0, (2048 * 2048 * 4));
   if(strstr(path, ".PNG") != NULL || strstr(path, ".png") != NULL)
   {
      if (!ps3graphics_load_png(path, out_img))
      {
         free(out_img->pixels);
	 return false;
      }
   }
   else
   {
      if (!ps3graphics_load_jpeg(path, out_img))
      {
         free(out_img->pixels);
	 return false;
      }
   }

   return true;
}
