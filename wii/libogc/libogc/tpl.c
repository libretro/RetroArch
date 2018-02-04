#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <gcutil.h>
#include <gccore.h>
#include "tpl.h"
#include "processor.h"

#define TPL_FILE_TYPE_DISC			0
#define TPL_FILE_TYPE_MEM			1

#define TPL_HDR_VERSION_FIELD		0
#define TPL_HDR_NTEXTURE_FIELD		4
#define TPL_HDR_HDRSIZE_FIELD		8
#define TPL_HDR_DESCR_FIELD		   12

// texture header
typedef struct _tplimgheader TPLImgHeader;

struct _tplimgheader {
	u16 height;
	u16 width;
	u32 fmt;
	void *data;
	u32 wraps;
	u32 wrapt;
	u32 minfilter;
	u32 magfilter;
	f32 lodbias;
	u8 edgelod;
	u8 minlod;
	u8 maxlod;
	u8 unpacked;
} ATTRIBUTE_PACKED;

// texture palette header
typedef struct _tplpalheader TPLPalHeader;

struct _tplpalheader {
	u16 nitems;
	u8 unpacked;
	u8 pad;
	u32 fmt;
	void *data;
} ATTRIBUTE_PACKED;

// texture descriptor
typedef struct _tpldesc TPLDescHeader;

struct _tpldesc {
	TPLImgHeader *imghead;
	TPLPalHeader *palhead;
} ATTRIBUTE_PACKED;

static u32 TPL_GetTextureSize(u32 width,u32 height,u32 fmt)
{
	u32 size = 0;

	switch(fmt) {
			case GX_TF_I4:
			case GX_TF_CI4:
			case GX_TF_CMPR:
				size = ((width+7)>>3)*((height+7)>>3)*32;
				break;
			case GX_TF_I8:
			case GX_TF_IA4:
			case GX_TF_CI8:
				size = ((width+7)>>3)*((height+7)>>2)*32;
				break;
			case GX_TF_IA8:
			case GX_TF_CI14:
			case GX_TF_RGB565:
			case GX_TF_RGB5A3:
				size = ((width+3)>>2)*((height+3)>>2)*32;
				break;
			case GX_TF_RGBA8:
				size = ((width+3)>>2)*((height+3)>>2)*32*2;
				break;
			default:
				break;
	}
	return size;
}

s32 TPL_OpenTPLFromFile(TPLFile* tdf, const char* file_name)
{
	u32 c;
	u32 version;
	FILE *f = NULL;
	TPLDescHeader *deschead = NULL;
	TPLImgHeader *imghead = NULL;
	TPLPalHeader *palhead = NULL;

	if(!file_name) return 0;

	f = fopen(file_name,"rb");
	if(!f) return -1;

	tdf->type = TPL_FILE_TYPE_DISC;
	tdf->tpl_file = (FHANDLE)f;

	fread(&version,sizeof(u32),1,f);
	fread(&tdf->ntextures,sizeof(u32),1,f);

	fseek(f,TPL_HDR_DESCR_FIELD,SEEK_SET);

	deschead = malloc(tdf->ntextures*sizeof(TPLDescHeader));
	if(deschead) {
		fread(deschead,sizeof(TPLDescHeader),tdf->ntextures,f);

		for(c=0;c<tdf->ntextures;c++) {
			imghead = deschead[c].imghead;
			palhead = deschead[c].palhead;

			//now read in the image data.
			fseek(f,(s32)imghead,SEEK_SET);
			imghead = malloc(sizeof(TPLImgHeader));
			if(!imghead) goto error_open;

			fread(imghead,sizeof(TPLImgHeader),1,f);
			deschead[c].imghead = imghead;

			if(palhead) {
				fseek(f,(s32)palhead,SEEK_SET);

				palhead = malloc(sizeof(TPLPalHeader));
				if(!palhead) goto error_open;

				fread(palhead,sizeof(TPLPalHeader),1,f);
				deschead[c].palhead = palhead;
			}
		}
		tdf->texdesc = deschead;

		return 1;
	}

error_open:
	if(deschead) free(deschead);
	if(palhead) free(palhead);

	fclose(f);
	return 0;
}

s32 TPL_OpenTPLFromMemory(TPLFile* tdf, void *memory,u32 len)
{
	u32 c,pos;
	const char *p = memory;
	TPLDescHeader *deschead = NULL;
	TPLImgHeader *imghead = NULL;
	TPLPalHeader *palhead = NULL;

	if(!memory || !len) return -1;		//TPL_ERR_INVALID

	tdf->type = TPL_FILE_TYPE_MEM;
	tdf->tpl_file = (FHANDLE)NULL;

	//version = *(u32*)(p + TPL_HDR_VERSION_FIELD);
	tdf->ntextures = *(u32*)(p + TPL_HDR_NTEXTURE_FIELD);

	deschead = (TPLDescHeader*)(p + TPL_HDR_DESCR_FIELD);
	for(c=0;c<tdf->ntextures;c++) {
		imghead = NULL;
		palhead = NULL;

		pos = (u32)deschead[c].imghead;
		imghead = (TPLImgHeader*)(p + pos);

		pos = (u32)imghead->data;
		imghead->data = (char*)(p + pos);

		pos = (u32)deschead[c].palhead;
		if(pos) {
			palhead = (TPLPalHeader*)(p + pos);

			pos = (u32)palhead->data;
			palhead->data = (char*)(p + pos);
		}
		deschead[c].imghead = imghead;
		deschead[c].palhead = palhead;
	}
	tdf->texdesc = deschead;

	return 1;
}

s32 TPL_GetTextureInfo(TPLFile *tdf,s32 id,u32 *fmt,u16 *width,u16 *height)
{
	TPLDescHeader *deschead = NULL;
	TPLImgHeader *imghead = NULL;

	if(!tdf) return -1;
	if(id<0 || id>=tdf->ntextures) return -1;

	deschead = (TPLDescHeader*)tdf->texdesc;
	if(!deschead) return -1;

	imghead = deschead[id].imghead;
	if(!imghead) return -1;

	if(fmt) *fmt = imghead->fmt;
	if(width) *width = imghead->width;
	if(height) *height = imghead->height;

	return 0;
}

s32 TPL_GetTexture(TPLFile *tdf,s32 id,GXTexObj *texObj)
{
	s32 pos;
	u32 size;
	FILE *f = NULL;
	TPLDescHeader *deschead = NULL;
	TPLImgHeader *imghead = NULL;
	s32 bMipMap = 0;
	u8 biasclamp = GX_DISABLE;

	if(!tdf) return -1;
	if(!texObj) return -1;
	if(id<0 || id>=tdf->ntextures) return -1;

	deschead = (TPLDescHeader*)tdf->texdesc;
	if(!deschead) return -1;

	imghead = deschead[id].imghead;
	if(!imghead) return -1;

	size = TPL_GetTextureSize(imghead->width,imghead->height,imghead->fmt);
	if(tdf->type==TPL_FILE_TYPE_DISC) {
		f = (FILE*)tdf->tpl_file;
		pos = (s32)imghead->data;
		imghead->data = memalign(PPC_CACHE_ALIGNMENT,size);
		if(!imghead->data) return -1;

		fseek(f,pos,SEEK_SET);
		fread(imghead->data,1,size,f);
	}

	if(imghead->maxlod>0) bMipMap = 1;
	if(imghead->lodbias>0.0f) biasclamp = GX_ENABLE;

	DCFlushRange(imghead->data,size);
	GX_InitTexObj(texObj,imghead->data,imghead->width,imghead->height,imghead->fmt,imghead->wraps,imghead->wrapt,bMipMap);
	if(bMipMap) GX_InitTexObjLOD(texObj,imghead->minfilter,imghead->magfilter,imghead->minlod,imghead->maxlod,
								 imghead->lodbias,biasclamp,biasclamp,imghead->edgelod);

	return 0;
}

s32 TPL_GetTextureCI(TPLFile *tdf,s32 id,GXTexObj *texObj,GXTlutObj *tlutObj,u8 tluts)
{
	s32 pos;
	u32 size;
	FILE *f = NULL;
	TPLDescHeader *deschead = NULL;
	TPLImgHeader *imghead = NULL;
	TPLPalHeader *palhead = NULL;
	s32 bMipMap = 0;
	u8 biasclamp = GX_DISABLE;

	if(!tdf) return -1;
	if(!texObj) return -1;
	if(!tlutObj) return -1;
 	if(id<0 || id>=tdf->ntextures) return -1;

	deschead = (TPLDescHeader*)tdf->texdesc;
	if(!deschead) return -1;

	imghead = deschead[id].imghead;
	if(!imghead) return -1;

	palhead = deschead[id].palhead;
	if(!palhead) return -1;

	size = TPL_GetTextureSize(imghead->width,imghead->height,imghead->fmt);
	if(tdf->type==TPL_FILE_TYPE_DISC) {
		f = (FILE*)tdf->tpl_file;
		pos = (s32)imghead->data;
		imghead->data = memalign(PPC_CACHE_ALIGNMENT,size);
		if(!imghead->data) return -1;

		fseek(f,pos,SEEK_SET);
		fread(imghead->data,1,size,f);

		pos = (s32)palhead->data;
		palhead->data = memalign(PPC_CACHE_ALIGNMENT,(palhead->nitems*sizeof(u16)));
		if(!palhead->data) {
			free(imghead->data);
			return -1;
		}

		fseek(f,pos,SEEK_SET);
		fread(palhead->data,1,(palhead->nitems*sizeof(u16)),f);
	}

	if(imghead->maxlod>0) bMipMap = 1;
	if(imghead->lodbias>0.0f) biasclamp = GX_ENABLE;

	DCFlushRange(imghead->data,size);
	DCFlushRange(palhead->data,(palhead->nitems*sizeof(u16)));
	GX_InitTlutObj(tlutObj,palhead->data,palhead->fmt,palhead->nitems);
	GX_InitTexObjCI(texObj,imghead->data,imghead->width,imghead->height,imghead->fmt,imghead->wraps,imghead->wrapt,bMipMap,tluts);
	if(bMipMap) GX_InitTexObjLOD(texObj,imghead->minfilter,imghead->magfilter,imghead->minlod,imghead->maxlod,
							     imghead->lodbias,biasclamp,biasclamp,imghead->edgelod);

	return 0;
}

void TPL_CloseTPLFile(TPLFile *tdf)
{
	int i;
	FILE *f;
	TPLPalHeader *palhead;
	TPLImgHeader *imghead;
	TPLDescHeader *deschead;

	if(!tdf) return;

	if(tdf->type==TPL_FILE_TYPE_DISC) {
		f = (FILE*)tdf->tpl_file;
		if(f) fclose(f);

		deschead = (TPLDescHeader*)tdf->texdesc;
		if(!deschead) return;

		for(i=0;i<tdf->ntextures;i++) {
			imghead = deschead[i].imghead;
			palhead = deschead[i].palhead;
			if(imghead) {
				if(imghead->data) free(imghead->data);
				free(imghead);
			}
			if(palhead) {
				if(palhead->data) free(palhead->data);
				free(palhead);
			}
		}
		free(tdf->texdesc);
	}

	tdf->ntextures = 0;
	tdf->texdesc = NULL;
	tdf->tpl_file = NULL;
}
