/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
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

#include <cell/codec.h>

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
	CtrlMallocArg *arg;
	arg = (CtrlMallocArg *) a;
	arg->mallocCallCounts++;
	return malloc(size);
}

static int img_free(void *ptr, void * a)
{
	CtrlFreeArg *arg;
	arg = (CtrlFreeArg *) a;
	arg->freeCallCounts++;
	free(ptr);
	return 0;
}

/******************************************************************************* 
	Image decompression - libJPEG
********************************************************************************/

static bool ps3graphics_load_jpeg(const char * path, struct texture_image *out_img)
{
	CtrlMallocArg              MallocArg;
	CtrlFreeArg                FreeArg;
	CellJpgDecMainHandle       mHandle = NULL;
	CellJpgDecSubHandle        sHandle = NULL;
	CellJpgDecThreadInParam    InParam;
	CellJpgDecThreadOutParam   OutParam;
	CellJpgDecSrc              src;
	CellJpgDecOpnInfo          opnInfo;
	CellJpgDecInfo             info;
	CellJpgDecInParam          inParam;
	CellJpgDecOutParam         outParam;
	CellJpgDecDataOutInfo      dOutInfo;
	CellJpgDecDataCtrlParam    dCtrlParam;

	MallocArg.mallocCallCounts = 0;
	FreeArg.freeCallCounts = 0;
	InParam.spuThreadEnable = CELL_JPGDEC_SPU_THREAD_ENABLE;
	InParam.ppuThreadPriority = 1001;
	InParam.spuThreadPriority = 250;
	InParam.cbCtrlMallocFunc = img_malloc;
	InParam.cbCtrlMallocArg = &MallocArg;
	InParam.cbCtrlFreeFunc = img_free;
	InParam.cbCtrlFreeArg = &FreeArg;

	int ret_jpeg, ret = -1;
	ret_jpeg = cellJpgDecCreate(&mHandle, &InParam, &OutParam);

	if (ret_jpeg != CELL_OK)
		goto error;

	memset(&src, 0, sizeof(CellJpgDecSrc));
	src.srcSelect        = CELL_JPGDEC_FILE;
	src.fileName         = path;
	src.fileOffset       = 0;
	src.fileSize         = 0;
	src.streamPtr        = NULL;
	src.streamSize       = 0;

	src.spuThreadEnable  = CELL_JPGDEC_SPU_THREAD_ENABLE;

	ret = cellJpgDecOpen(mHandle, &sHandle, &src, &opnInfo);

	if (ret != CELL_OK)
		goto error;

	ret = cellJpgDecReadHeader(mHandle, sHandle, &info);

	if (ret != CELL_OK)
		goto error;

	inParam.commandPtr         = NULL;
	inParam.method             = CELL_JPGDEC_FAST;
	inParam.outputMode         = CELL_JPGDEC_TOP_TO_BOTTOM;
	inParam.outputColorSpace   = CELL_JPG_ARGB;
	inParam.downScale          = 1;
	inParam.outputColorAlpha = 0xfe;
	ret = cellJpgDecSetParameter(mHandle, sHandle, &inParam, &outParam);

	if (ret != CELL_OK)
		goto error;

	dCtrlParam.outputBytesPerLine = outParam.outputWidth * 4;
	ret = cellJpgDecDecodeData(mHandle, sHandle, (uint8_t*)out_img->pixels, &dCtrlParam, &dOutInfo);

	if (ret != CELL_OK || dOutInfo.status != CELL_JPGDEC_DEC_STATUS_FINISH)
		goto error;

	out_img->width = outParam.outputWidth;
	out_img->height = outParam.outputHeight;

	cellJpgDecClose(mHandle, sHandle);
	cellJpgDecDestroy(mHandle);

	return true;

error:
	if (mHandle && sHandle)
		cellJpgDecClose(mHandle, sHandle);
	if (mHandle)
		cellJpgDecDestroy(mHandle);
	return false;
}

/******************************************************************************* 
	Image decompression - libPNG
********************************************************************************/

static bool ps3graphics_load_png(const char * path, struct texture_image *out_img)
{
	CtrlMallocArg              MallocArg;
	CtrlFreeArg                FreeArg;
	CellPngDecMainHandle       mHandle = NULL;
	CellPngDecSubHandle        sHandle = NULL;
	CellPngDecThreadInParam    InParam;
	CellPngDecThreadOutParam   OutParam;
	CellPngDecSrc              src;
	CellPngDecOpnInfo          opnInfo;
	CellPngDecInfo             info;
	CellPngDecInParam          inParam;
	CellPngDecOutParam         outParam;
	CellPngDecDataOutInfo      dOutInfo;
	CellPngDecDataCtrlParam    dCtrlParam;

	MallocArg.mallocCallCounts = 0;
	FreeArg.freeCallCounts = 0;
	InParam.spuThreadEnable = CELL_PNGDEC_SPU_THREAD_ENABLE;
	InParam.ppuThreadPriority = 512;
	InParam.spuThreadPriority = 200;
	InParam.cbCtrlMallocFunc = img_malloc;
	InParam.cbCtrlMallocArg = &MallocArg;
	InParam.cbCtrlFreeFunc = img_free;
	InParam.cbCtrlFreeArg = &FreeArg;

	int ret_png, ret = -1;
	ret_png = cellPngDecCreate(&mHandle, &InParam, &OutParam);

	if (ret_png != CELL_OK)
		goto error;

	memset(&src, 0, sizeof(CellPngDecSrc));
	src.srcSelect        = CELL_PNGDEC_FILE;
	src.fileName         = path;
	src.fileOffset       = 0;
	src.fileSize         = 0;
	src.streamPtr        = 0;
	src.streamSize       = 0;

	src.spuThreadEnable  = CELL_PNGDEC_SPU_THREAD_ENABLE;

	ret = cellPngDecOpen(mHandle, &sHandle, &src, &opnInfo);


	if (ret != CELL_OK)
		goto error;

	ret = cellPngDecReadHeader(mHandle, sHandle, &info);


	if (ret != CELL_OK)
		goto error;

	inParam.commandPtr         = NULL;
	inParam.outputMode         = CELL_PNGDEC_TOP_TO_BOTTOM;
	inParam.outputColorSpace   = CELL_PNGDEC_ARGB;
	inParam.outputBitDepth     = 8;
	inParam.outputPackFlag     = CELL_PNGDEC_1BYTE_PER_1PIXEL;
	inParam.outputAlphaSelect  = CELL_PNGDEC_STREAM_ALPHA;
	ret = cellPngDecSetParameter(mHandle, sHandle, &inParam, &outParam);

	if (ret != CELL_OK)
		goto error;

	dCtrlParam.outputBytesPerLine = outParam.outputWidth * 4;
	ret = cellPngDecDecodeData(mHandle, sHandle, (uint8_t*)out_img->pixels, &dCtrlParam, &dOutInfo);


	if (ret != CELL_OK || dOutInfo.status != CELL_PNGDEC_DEC_STATUS_FINISH)
		goto error;

	out_img->width = outParam.outputWidth;
	out_img->height = outParam.outputHeight;

	cellPngDecClose(mHandle, sHandle);
	cellPngDecDestroy(mHandle);

	return true;

error:
	if (mHandle && sHandle)
		cellPngDecClose(mHandle, sHandle);
	if (mHandle)
		cellPngDecDestroy(mHandle);
	return false;
}

bool texture_image_load(const char *path, struct texture_image *out_img)
{
	if(strstr(path, ".PNG") != NULL || strstr(path, ".png") != NULL)
	{
		if (!ps3graphics_load_png(path, out_img))
			return false;
	}
	else
	{
		if (!ps3graphics_load_jpeg(path, out_img))
			return false;
	}

	return true;
}
