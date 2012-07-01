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

#if defined(__CELLOS_LV2__)

#ifdef __PSL1GHT__
#include <ppu-asm.h>
#include <ppu-types.h>
#include <jpgdec/jpgdec.h>
#include <pngdec/pngdec.h>

#define pJpgDecCreate jpgDecCreate
#define pJpgDecMainHandle int
#define pPngDecMainHandle int
#define pJpgDecSubHandle int
#define pPngDecSubHandle int
#define pJpgDecThreadInParam jpgDecThreadInParam
#define pPngDecThreadInParam pngDecThreadInParam
#define pJpgDecThreadOutParam jpgDecThreadOutParam
#define pPngDecThreadOutParam pngDecThreadOutParam
#define pJpgDecSrc jpgDecSource
#define pPngDecSrc pngDecSource
#define pJpgDecOpnInfo uint32_t
#define pPngDecOpnInfo uint32_t
#define pJpgDecInfo jpgDecInfo
#define pPngDecInfo pngDecInfo
#define pJpgDecInParam jpgDecInParam
#define pPngDecInParam pngDecInParam
#define pJpgDecOutParam jpgDecOutParam
#define pPngDecOutParam pngDecOutParam
#define pJpgDecDataOutInfo jpgDecDataInfo
#define pPngDecDataOutInfo pngDecDataInfo
#define pJpgDecDataCtrlParam uint64_t
#define pPngDecDataCtrlParam uint64_t

#define spu_enable enable
#define stream_select stream
#define color_alpha alpha
#define color_space space
#define output_mode mode
#define output_bytes_per_line bytes_per_line
#define output_width width
#define output_height height

#define pJpgDecOpen jpgDecOpen
#define pJpgDecReadHeader jpgDecReadHeader
#define pJpgDecSetParameter jpgDecSetParameter
#define pJpgDecDecodeData jpgDecDecodeData
#define pJpgDecClose jpgDecClose
#define pJpgDecDestroy jpgDecDestroy

#define pPngDecCreate pngDecCreate
#define pPngDecOpen pngDecOpen
#define pPngDecReadHeader pngDecReadHeader
#define pPngDecSetParameter pngDecSetParameter
#define pPngDecDecodeData pngDecDecodeData
#define pPngDecClose pngDecClose
#define pPngDecDestroy pngDecDestroy

#define CELL_PNGDEC_SPU_THREAD_ENABLE 1
#define CELL_JPGDEC_SPU_THREAD_ENABLE 1
#define CELL_JPGDEC_FILE JPGDEC_FILE
#define CELL_PNGDEC_FILE PNGDEC_FILE
#define CELL_JPGDEC_SPU_THREAD_ENABLE 1
#define CELL_JPGDEC_FAST JPGDEC_LOW_QUALITY
#define CELL_JPGDEC_TOP_TO_BOTTOM JPGDEC_TOP_TO_BOTTOM
#define CELL_PNGDEC_TOP_TO_BOTTOM PNGDEC_TOP_TO_BOTTOM
#define CELL_JPG_ARGB JPGDEC_ARGB
#define CELL_PNGDEC_ARGB PNGDEC_ARGB
#define CELL_JPGDEC_DEC_STATUS_FINISH 0
#define CELL_PNGDEC_DEC_STATUS_FINISH 0
#define CELL_PNGDEC_1BYTE_PER_1PIXEL 1
#define CELL_PNGDEC_STREAM_ALPHA 1
#define CELL_OK 0
#define PTR_NULL 0

#else
#include <cell/codec.h>

#define pJpgDecCreate cellJpgDecCreate
#define pJpgDecMainHandle CellJpgDecMainHandle
#define pPngDecMainHandle CellPngDecMainHandle
#define pJpgDecSubHandle CellJpgDecSubHandle
#define pPngDecSubHandle CellPngDecSubHandle
#define pJpgDecThreadInParam CellJpgDecThreadInParam
#define pPngDecThreadInParam CellPngDecThreadInParam
#define pJpgDecThreadOutParam CellJpgDecThreadOutParam
#define pPngDecThreadOutParam CellPngDecThreadOutParam
#define pJpgDecSrc CellJpgDecSrc
#define pPngDecSrc CellPngDecSrc
#define pJpgDecOpnInfo CellJpgDecOpnInfo
#define pPngDecOpnInfo CellPngDecOpnInfo
#define pJpgDecInfo CellJpgDecInfo
#define pPngDecInfo CellPngDecInfo
#define pJpgDecInParam CellJpgDecInParam
#define pPngDecInParam CellPngDecInParam
#define pJpgDecOutParam CellJpgDecOutParam
#define pPngDecOutParam CellPngDecOutParam
#define pJpgDecDataOutInfo CellJpgDecDataOutInfo
#define pPngDecDataOutInfo CellPngDecDataOutInfo
#define pJpgDecDataCtrlParam CellJpgDecDataCtrlParam
#define pPngDecDataCtrlParam CellPngDecDataCtrlParam

#define spu_enable spuThreadEnable
#define ppu_prio ppuThreadPriority
#define spu_prio spuThreadPriority
#define malloc_func cbCtrlMallocFunc
#define malloc_arg cbCtrlMallocArg
#define free_func cbCtrlFreeFunc
#define free_arg cbCtrlFreeArg
#define stream_select srcSelect
#define file_name fileName
#define file_offset fileOffset
#define file_size fileSize
#define stream_ptr streamPtr
#define stream_size streamSize
#define down_scale downScale
#define color_alpha outputColorAlpha
#define color_space outputColorSpace
#define cmd_ptr commandPtr
#define quality method
#define output_mode outputMode
#define output_bytes_per_line outputBytesPerLine
#define output_width outputWidth
#define output_height outputHeight
#define bit_depth outputBitDepth
#define pack_flag outputPackFlag
#define alpha_select outputAlphaSelect

#define pJpgDecOpen cellJpgDecOpen
#define pJpgDecReadHeader cellJpgDecReadHeader
#define pJpgDecSetParameter cellJpgDecSetParameter
#define pJpgDecDecodeData cellJpgDecDecodeData
#define pJpgDecClose cellJpgDecClose
#define pJpgDecDestroy cellJpgDecDestroy

#define pPngDecCreate cellPngDecCreate
#define pPngDecOpen cellPngDecOpen
#define pPngDecReadHeader cellPngDecReadHeader
#define pPngDecSetParameter cellPngDecSetParameter
#define pPngDecDecodeData cellPngDecDecodeData
#define pPngDecClose cellPngDecClose
#define pPngDecDestroy cellPngDecDestroy

#define PTR_NULL NULL
#endif

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
