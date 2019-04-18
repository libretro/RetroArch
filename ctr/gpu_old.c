/* originally from from https://github.com/smealum/ctrulib */

/*
  gpu-old.c _ Legacy GPU commands.
*/

#include <stdlib.h>
#include <string.h>
#include <3ds/types.h>
#include <3ds/gpu/gpu.h>
#include <3ds/gpu/gx.h>
#include <3ds/gpu/shbin.h>

#include "gpu_old.h"

void GPU_Init(Handle *gsphandle)
{
	gpuCmdBuf=NULL;
	gpuCmdBufSize=0;
	gpuCmdBufOffset=0;
}

void GPU_Reset(u32* gxbuf, u32* gpuBuf, u32 gpuBufSize)
{
	GPUCMD_SetBuffer(gpuBuf, gpuBufSize, 0);
}

void GPU_SetFloatUniform(GPU_SHADER_TYPE type, u32 startreg, u32* data, u32 numreg)
{
	if(!data)return;

	int regOffset=(type==GPU_GEOMETRY_SHADER)?(-0x30):(0x0);

	GPUCMD_AddWrite(GPUREG_VSH_FLOATUNIFORM_CONFIG+regOffset, 0x80000000|startreg);
	GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA+regOffset, data, numreg*4);
}

//takes PAs as arguments
void GPU_SetViewport(u32* depthBuffer, u32* colorBuffer, u32 x, u32 y, u32 w, u32 h)
{
	u32 param[0x4];
	float fw=(float)w;
	float fh=(float)h;

	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 0x00000001);
	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_INVALIDATE, 0x00000001);

	u32 f116e=0x01000000|(((h-1)&0xFFF)<<12)|(w&0xFFF);

	param[0x0]=((u32)depthBuffer)>>3;
	param[0x1]=((u32)colorBuffer)>>3;
	param[0x2]=f116e;
	GPUCMD_AddIncrementalWrites(GPUREG_DEPTHBUFFER_LOC, param, 0x00000003);

	GPUCMD_AddWrite(GPUREG_RENDERBUF_DIM, f116e);
	GPUCMD_AddWrite(GPUREG_DEPTHBUFFER_FORMAT, 0x00000003); //depth buffer format
	GPUCMD_AddWrite(GPUREG_COLORBUFFER_FORMAT, 0x00000002); //color buffer format
	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_BLOCK32, 0x00000000); //?

	param[0x0]=f32tof24(fw/2);
	param[0x1]=f32tof31(2.0f / fw) << 1;
	param[0x2]=f32tof24(fh/2);
	param[0x3]=f32tof31(2.0f / fh) << 1;
	GPUCMD_AddIncrementalWrites(GPUREG_VIEWPORT_WIDTH, param, 0x00000004);

	GPUCMD_AddWrite(GPUREG_VIEWPORT_XY, (y<<16)|(x&0xFFFF));

	param[0x0]=0x00000000;
	param[0x1]=0x00000000;
	param[0x2]=((h-1)<<16)|((w-1)&0xFFFF);
	GPUCMD_AddIncrementalWrites(GPUREG_SCISSORTEST_MODE, param, 0x00000003);

	//enable depth buffer
	param[0x0]=0x0000000F;
	param[0x1]=0x0000000F;
	param[0x2]=0x00000002;
	param[0x3]=0x00000002;
	GPUCMD_AddIncrementalWrites(GPUREG_COLORBUFFER_READ, param, 0x00000004);
}

void GPU_SetScissorTest(GPU_SCISSORMODE mode, u32 left, u32 bottom, u32 right, u32 top)
{
	u32 param[3];

	param[0x0] = mode;
	param[0x1] = (bottom<<16)|(left&0xFFFF);
	param[0x2] = ((top-1)<<16)|((right-1)&0xFFFF);
	GPUCMD_AddIncrementalWrites(GPUREG_SCISSORTEST_MODE, param, 0x00000003);
}

void GPU_DepthMap(float zScale, float zOffset)
{
	GPUCMD_AddWrite(GPUREG_DEPTHMAP_ENABLE, 0x00000001);
	GPUCMD_AddWrite(GPUREG_DEPTHMAP_SCALE, f32tof24(zScale));
	GPUCMD_AddWrite(GPUREG_DEPTHMAP_OFFSET, f32tof24(zOffset));
}

void GPU_SetAlphaTest(bool enable, GPU_TESTFUNC function, u8 ref)
{
	GPUCMD_AddWrite(GPUREG_FRAGOP_ALPHA_TEST, (enable&1)|((function&7)<<4)|(ref<<8));
}

void GPU_SetStencilTest(bool enable, GPU_TESTFUNC function, u8 ref, u8 input_mask, u8 write_mask)
{
	GPUCMD_AddWrite(GPUREG_STENCIL_TEST, (enable&1)|((function&7)<<4)|(write_mask<<8)|(ref<<16)|(input_mask<<24));
}

void GPU_SetStencilOp(GPU_STENCILOP sfail, GPU_STENCILOP dfail, GPU_STENCILOP pass)
{
	GPUCMD_AddWrite(GPUREG_STENCIL_OP, sfail | (dfail << 4) | (pass << 8));
}

void GPU_SetDepthTestAndWriteMask(bool enable, GPU_TESTFUNC function, GPU_WRITEMASK writemask)
{
	GPUCMD_AddWrite(GPUREG_DEPTH_COLOR_MASK, (enable&1)|((function&7)<<4)|(writemask<<8));
}

void GPU_SetAlphaBlending(GPU_BLENDEQUATION colorEquation, GPU_BLENDEQUATION alphaEquation,
	GPU_BLENDFACTOR colorSrc, GPU_BLENDFACTOR colorDst,
	GPU_BLENDFACTOR alphaSrc, GPU_BLENDFACTOR alphaDst)
{
	GPUCMD_AddWrite(GPUREG_BLEND_FUNC, colorEquation | (alphaEquation<<8) | (colorSrc<<16) | (colorDst<<20) | (alphaSrc<<24) | (alphaDst<<28));
	GPUCMD_AddMaskedWrite(GPUREG_COLOR_OPERATION, 0x2, 0x00000100);
}

void GPU_SetColorLogicOp(GPU_LOGICOP op)
{
	GPUCMD_AddWrite(GPUREG_LOGIC_OP, op);
	GPUCMD_AddMaskedWrite(GPUREG_COLOR_OPERATION, 0x2, 0x00000000);
}

void GPU_SetBlendingColor(u8 r, u8 g, u8 b, u8 a)
{
	GPUCMD_AddWrite(GPUREG_BLEND_COLOR, r | (g << 8) | (b << 16) | (a << 24));
}

void GPU_SetTextureEnable(GPU_TEXUNIT units)
{
	GPUCMD_AddMaskedWrite(GPUREG_SH_OUTATTR_CLOCK, 0x2, units<<8); // enables texcoord outputs
	GPUCMD_AddWrite(GPUREG_TEXUNIT_CONFIG, 0x00011000|units); // enables texture units
}

void GPU_SetTexture(GPU_TEXUNIT unit, u32* data, u16 width, u16 height, u32 param, GPU_TEXCOLOR colorType)
{
	switch (unit)
	{
	case GPU_TEXUNIT0:
		GPUCMD_AddWrite(GPUREG_TEXUNIT0_TYPE, colorType);
		GPUCMD_AddWrite(GPUREG_TEXUNIT0_ADDR1, ((u32)data)>>3);
		GPUCMD_AddWrite(GPUREG_TEXUNIT0_DIM, (width<<16)|height);
		GPUCMD_AddWrite(GPUREG_TEXUNIT0_PARAM, param);
		break;

	case GPU_TEXUNIT1:
		GPUCMD_AddWrite(GPUREG_TEXUNIT1_TYPE, colorType);
		GPUCMD_AddWrite(GPUREG_TEXUNIT1_ADDR, ((u32)data)>>3);
		GPUCMD_AddWrite(GPUREG_TEXUNIT1_DIM, (width<<16)|height);
		GPUCMD_AddWrite(GPUREG_TEXUNIT1_PARAM, param);
		break;

	case GPU_TEXUNIT2:
		GPUCMD_AddWrite(GPUREG_TEXUNIT2_TYPE, colorType);
		GPUCMD_AddWrite(GPUREG_TEXUNIT2_ADDR, ((u32)data)>>3);
		GPUCMD_AddWrite(GPUREG_TEXUNIT2_DIM, (width<<16)|height);
		GPUCMD_AddWrite(GPUREG_TEXUNIT2_PARAM, param);
		break;
	}
}

void GPU_SetTextureBorderColor(GPU_TEXUNIT unit,u32 borderColor)
{
	switch (unit)
	{
	case GPU_TEXUNIT0:
		GPUCMD_AddWrite(GPUREG_TEXUNIT0_BORDER_COLOR, borderColor);
		break;

	case GPU_TEXUNIT1:
		GPUCMD_AddWrite(GPUREG_TEXUNIT1_BORDER_COLOR, borderColor);
		break;

	case GPU_TEXUNIT2:
		GPUCMD_AddWrite(GPUREG_TEXUNIT2_BORDER_COLOR, borderColor);
		break;
	}
}

const u8 GPU_FORMATSIZE[4]={1,1,2,4};

void GPU_SetAttributeBuffers(u8 totalAttributes, u32* baseAddress, u64 attributeFormats, u16 attributeMask, u64 attributePermutation, u8 numBuffers, u32 bufferOffsets[], u64 bufferPermutations[], u8 bufferNumAttributes[])
{
	u32 param[0x28];

	memset(param, 0x00, 0x28*4);

	param[0x0]=((u32)baseAddress)>>3;
	param[0x1]=attributeFormats&0xFFFFFFFF;
	param[0x2]=((totalAttributes-1)<<28)|((attributeMask&0xFFF)<<16)|((attributeFormats>>32)&0xFFFF);

	int i, j;
	u8 sizeTable[0xC];
	for(i=0;i<totalAttributes;i++)
	{
		u8 v=attributeFormats&0xF;
		sizeTable[i]=GPU_FORMATSIZE[v&3]*((v>>2)+1);
		attributeFormats>>=4;
	}

	for(i=0;i<numBuffers;i++)
	{
		u16 stride=0;
		param[3*(i+1)+0]=bufferOffsets[i];
		param[3*(i+1)+1]=bufferPermutations[i]&0xFFFFFFFF;
		for(j=0;j<bufferNumAttributes[i];j++)stride+=sizeTable[(bufferPermutations[i]>>(4*j))&0xF];
		param[3*(i+1)+2]=(bufferNumAttributes[i]<<28)|((stride&0xFFF)<<16)|((bufferPermutations[i]>>32)&0xFFFF);
	}

	GPUCMD_AddIncrementalWrites(GPUREG_ATTRIBBUFFERS_LOC, param, 0x00000027);

	GPUCMD_AddMaskedWrite(GPUREG_VSH_INPUTBUFFER_CONFIG, 0xB, 0xA0000000|(totalAttributes-1));
	GPUCMD_AddWrite(GPUREG_VSH_NUM_ATTR, (totalAttributes-1));

	GPUCMD_AddIncrementalWrites(GPUREG_VSH_ATTRIBUTES_PERMUTATION_LOW, ((u32[]){attributePermutation&0xFFFFFFFF, (attributePermutation>>32)&0xFFFF}), 2);
}

void GPU_SetAttributeBuffersAddress(u32* baseAddress)
{
	GPUCMD_AddWrite(GPUREG_ATTRIBBUFFERS_LOC, ((u32)baseAddress)>>3);
}

void GPU_SetFaceCulling(GPU_CULLMODE mode)
{
	GPUCMD_AddWrite(GPUREG_FACECULLING_CONFIG, mode&0x3);
}

void GPU_SetCombinerBufferWrite(u8 rgb_config, u8 alpha_config)
{
    GPUCMD_AddMaskedWrite(GPUREG_TEXENV_UPDATE_BUFFER, 0x2, (rgb_config << 8) | (alpha_config << 12));
}

const u8 GPU_TEVID[]={0xC0,0xC8,0xD0,0xD8,0xF0,0xF8};

void GPU_SetTexEnv(u8 id, u16 rgbSources, u16 alphaSources, u16 rgbOperands, u16 alphaOperands, GPU_COMBINEFUNC rgbCombine, GPU_COMBINEFUNC alphaCombine, u32 constantColor)
{
	if(id>6)return;
	u32 param[0x5];
	memset(param, 0x00, 5*4);

	param[0x0]=(alphaSources<<16)|(rgbSources);
	param[0x1]=(alphaOperands<<12)|(rgbOperands);
	param[0x2]=(alphaCombine<<16)|(rgbCombine);
	param[0x3]=constantColor;
	param[0x4]=0x00000000; // ?

	GPUCMD_AddIncrementalWrites(GPUREG_0000|GPU_TEVID[id], param, 0x00000005);
}

void GPU_DrawArray(GPU_Primitive_t primitive, u32 first, u32 count)
{
	//set primitive type
	GPUCMD_AddMaskedWrite(GPUREG_PRIMITIVE_CONFIG, 0x2, primitive);
	GPUCMD_AddMaskedWrite(GPUREG_RESTART_PRIMITIVE, 0x2, 0x00000001);
	//index buffer address register should be cleared (except bit 31) before drawing
	GPUCMD_AddWrite(GPUREG_INDEXBUFFER_CONFIG, 0x80000000);
	//pass number of vertices
	GPUCMD_AddWrite(GPUREG_NUMVERTICES, count);
	//set first vertex
	GPUCMD_AddWrite(GPUREG_VERTEX_OFFSET, first);

	//all the following except 0x000F022E might be useless
	GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 0x1, 0x00000001);
	GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 0x1, 0x00000000);
	GPUCMD_AddWrite(GPUREG_DRAWARRAYS, 0x00000001);
	GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 0x1, 0x00000001);
	GPUCMD_AddWrite(GPUREG_VTX_FUNC, 0x00000001);
	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 0x00000001);
}

void GPU_DrawElements(GPU_Primitive_t primitive, u32* indexArray, u32 n)
{
	//set primitive type
	GPUCMD_AddMaskedWrite(GPUREG_PRIMITIVE_CONFIG, 0x2, primitive);
	GPUCMD_AddMaskedWrite(GPUREG_RESTART_PRIMITIVE, 0x2, 0x00000001);
	//index buffer (TODO : support multiple types)
	GPUCMD_AddWrite(GPUREG_INDEXBUFFER_CONFIG, 0x80000000|((u32)indexArray));
	//pass number of vertices
	GPUCMD_AddWrite(GPUREG_NUMVERTICES, n);

	GPUCMD_AddWrite(GPUREG_VERTEX_OFFSET, 0x00000000);

	GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG, 0x2, 0x00000100);
	GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 0x2, 0x00000100);

	GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 0x1, 0x00000000);
	GPUCMD_AddWrite(GPUREG_DRAWELEMENTS, 0x00000001);
	GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 0x1, 0x00000001);
	GPUCMD_AddWrite(GPUREG_VTX_FUNC, 0x00000001);

	// CHECKME: does this one also require GPUREG_FRAMEBUFFER_FLUSH at the end?
}

void GPU_FinishDrawing()
{
	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 0x00000001);
	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_INVALIDATE, 0x00000001);
	GPUCMD_AddWrite(GPUREG_EARLYDEPTH_CLEAR, 0x00000001);
}

void GPU_Finalize(void)
{
   GPUCMD_AddMaskedWrite(GPUREG_PRIMITIVE_CONFIG, 0x8, 0x00000000);
   GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 0x00000001);
   GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_INVALIDATE, 0x00000001);
#if 0
   GPUCMD_Split(NULL, NULL);
#else
   GPUCMD_AddWrite(GPUREG_FINALIZE, 0x12345678);
   //not the cleanest way of guaranteeing 0x10-byte size but whatever good enough for now
   GPUCMD_AddWrite(GPUREG_FINALIZE,0x12345678);
#endif
}
