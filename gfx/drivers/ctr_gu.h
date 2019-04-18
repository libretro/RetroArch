/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

/* this file contains mostly modified functions from the ctrulib sdk */

#ifndef CTR_GU_H
#define CTR_GU_H

#include <3ds.h>
#include <stdint.h>
#include <string.h>
#include <retro_inline.h>

#include "ctr/ctr_debug.h"

#define VIRT_TO_PHYS(vaddr) \
   (((u32)(vaddr)) >= 0x14000000 && ((u32)(vaddr)) < 0x1c000000)?(void*)((u32)(vaddr) + 0x0c000000):\
   (((u32)(vaddr)) >= 0x1F000000 && ((u32)(vaddr)) < 0x1F600000)?(void*)((u32)(vaddr) - 0x07000000):\
   (((u32)(vaddr)) >= 0x1FF00000 && ((u32)(vaddr)) < 0x1FF80000)?(void*)(vaddr):\
   (((u32)(vaddr)) >= 0x30000000 && ((u32)(vaddr)) < 0x40000000)?(void*)((u32)(vaddr) - 0x10000000):(void*)0

#define CTRGU_SIZE(W,H)            (((u32)(W)&0xFFFF)|((u32)(H)<<16))

/* DMA flags */
#define CTRGU_DMA_VFLIP            (1 << 0)
#define CTRGU_DMA_L_TO_T           (1 << 1)
#define CTRGU_DMA_T_TO_L           (0 << 1)
#define CTRGU_DMA_TRUNCATE         (1 << 2)
#define CTRGU_DMA_CONVERT_NONE     (1 << 3)

#define CTRGU_RGBA8     (0)
#define CTRGU_RGB8      (1)
#define CTRGU_RGB565    (2)
#define CTRGU_RGBA5551  (3)
#define CTRGU_RGBA4444  (4)

#define CTRGU_MULTISAMPLE_NONE      (0 << 24)
#define CTRGU_MULTISAMPLE_2x1       (1 << 24)
#define CTRGU_MULTISAMPLE_2x2       (2 << 24)

#define CTR_CPU_TICKS_PER_SECOND    268123480
#define CTR_CPU_TICKS_PER_FRAME     4481134

extern u32* gpuCmdBuf;
extern u32 gpuCmdBufOffset;
extern u32 __linear_heap_size;
extern u32 __linear_heap;

__attribute__((always_inline))
static INLINE Result ctr_set_parallax_layer(bool state)
{
   u32 reg_state = state? 0x00010001: 0x0;
   return GSPGPU_WriteHWRegs(0x202000, &reg_state, 4);
}

__attribute__((always_inline))
static INLINE void ctrGuSetTexture(GPU_TEXUNIT unit, u32* data,
      u16 width, u16 height, u32 param, GPU_TEXCOLOR colorType)
{
   switch (unit)
   {
   case GPU_TEXUNIT0:
      GPUCMD_AddWrite(GPUREG_TEXUNIT0_TYPE, colorType);
      GPUCMD_AddWrite(GPUREG_TEXUNIT0_ADDR1, ((u32)data)>>3);
      GPUCMD_AddWrite(GPUREG_TEXUNIT0_DIM, (height)|(width<<16));
      GPUCMD_AddWrite(GPUREG_TEXUNIT0_PARAM, param);
      break;

   case GPU_TEXUNIT1:
      GPUCMD_AddWrite(GPUREG_TEXUNIT1_TYPE, colorType);
      GPUCMD_AddWrite(GPUREG_TEXUNIT1_ADDR, ((u32)data)>>3);
      GPUCMD_AddWrite(GPUREG_TEXUNIT1_DIM, (height)|(width<<16));
      GPUCMD_AddWrite(GPUREG_TEXUNIT1_PARAM, param);
      break;

   case GPU_TEXUNIT2:
      GPUCMD_AddWrite(GPUREG_TEXUNIT2_TYPE, colorType);
      GPUCMD_AddWrite(GPUREG_TEXUNIT2_ADDR, ((u32)data)>>3);
      GPUCMD_AddWrite(GPUREG_TEXUNIT2_DIM, (height)|(width<<16));
      GPUCMD_AddWrite(GPUREG_TEXUNIT2_PARAM, param);
      break;
   }
}

__attribute__((always_inline))
static INLINE Result ctrGuSetCommandList_First(bool queued, u32* buf0a, u32 buf0s, u32* buf1a, u32 buf1s, u32* buf2a, u32 buf2s)
{
   u32 gxCommand[0x8];
   gxCommand[0]=0x05 | (queued? 0x01000000 : 0x0); //CommandID
   gxCommand[1]=(u32)buf0a; //buf0 address
   gxCommand[2]=(u32)buf0s; //buf0 size
   gxCommand[3]=(u32)buf1a; //buf1 address
   gxCommand[4]=(u32)buf1s; //buf1 size
   gxCommand[5]=(u32)buf2a; //buf2 address
   gxCommand[6]=(u32)buf2s; //buf2 size
   gxCommand[7]=0x0;

   return gspSubmitGxCommand(gxCmdBuf, gxCommand);
}

__attribute__((always_inline))
static INLINE Result ctrGuSetCommandList_Last(bool queued, u32* buf0a, u32 buf0s, u8 flags)
{
   u32 gxCommand[0x8];
   gxCommand[0]=0x01 | (queued? 0x01000000 : 0x0); //CommandID
   gxCommand[1]=(u32)buf0a; //buf0 address
   gxCommand[2]=(u32)buf0s; //buf0 size
   gxCommand[3]=flags&1; //written to GSP module state
   gxCommand[4]=gxCommand[5]=gxCommand[6]=0x0;
   gxCommand[7]=(flags>>1)&1; //when non-zero, call svcFlushProcessDataCache() with the specified buffer

   return gspSubmitGxCommand(gxCmdBuf, gxCommand);
}

__attribute__((always_inline))
static INLINE void ctrGuFlushAndRun(bool queued)
{
   //take advantage of GX_SetCommandList_First to flush gsp heap
   ctrGuSetCommandList_First(queued, gpuCmdBuf, gpuCmdBufOffset*4, (u32*)__linear_heap, __linear_heap_size, NULL, 0);
   ctrGuSetCommandList_Last(queued, gpuCmdBuf, gpuCmdBufOffset*4, 0x0);
}

__attribute__((always_inline))
static INLINE Result ctrGuSetMemoryFill(bool queued, u32* buf0a, u32 buf0v, u32* buf0e, u16 width0, u32* buf1a, u32 buf1v, u32* buf1e, u16 width1)
{
   u32 gxCommand[0x8];
   gxCommand[0]=0x02 | (queued? 0x01000000 : 0x0); //CommandID
   gxCommand[1]=(u32)buf0a; //buf0 address
   gxCommand[2]=buf0v; //buf0 value
   gxCommand[3]=(u32)buf0e; //buf0 end addr
   gxCommand[4]=(u32)buf1a; //buf1 address
   gxCommand[5]=buf1v; //buf1 value
   gxCommand[6]=(u32)buf1e; //buf1 end addr
   gxCommand[7]=(width0)|(width1<<16);

   return gspSubmitGxCommand(gxCmdBuf, gxCommand);
}

__attribute__((always_inline))
static INLINE Result ctrGuCopyImage
      (bool queued,
       const void* src, int src_w, int src_h, int src_fmt, bool src_is_tiled,
             void* dst, int dst_w,            int dst_fmt, bool dst_is_tiled)
{
   u32 gxCommand[0x8];
   gxCommand[0]=0x03 | (queued? 0x01000000 : 0x0); //CommandID
   gxCommand[1]=(u32)src;
   gxCommand[2]=(u32)dst;
   gxCommand[3]=dst_w&0xFF8;
   gxCommand[4]=CTRGU_SIZE(src_w, src_h);
   gxCommand[5]=(src_fmt << 8)|(dst_fmt << 12)
                | ((src_is_tiled == dst_is_tiled)? CTRGU_DMA_CONVERT_NONE
                     : src_is_tiled? CTRGU_DMA_T_TO_L
                     : CTRGU_DMA_L_TO_T)
                | ((dst_w > src_w) ? CTRGU_DMA_TRUNCATE : 0);
   gxCommand[6]=gxCommand[7]=0x0;

   return gspSubmitGxCommand(gxCmdBuf, gxCommand);

}

__attribute__((always_inline))
static INLINE Result ctrGuDisplayTransfer
     (bool queued,
      void* src, int src_w, int src_h, int src_fmt,
      void* dst, int dst_w,            int dst_fmt, int multisample_lvl)
{
   u32 gxCommand[0x8];
   gxCommand[0]=0x03 | (queued? 0x01000000 : 0x0); //CommandID
   gxCommand[1]=(u32)src;
   gxCommand[2]=(u32)dst;
   gxCommand[3]=CTRGU_SIZE(dst_w, 0);
   gxCommand[4]=CTRGU_SIZE(src_w, src_h);
   gxCommand[5]=(src_fmt << 8) | (dst_fmt << 12) | multisample_lvl;
   gxCommand[6]=gxCommand[7]=0x0;

   return gspSubmitGxCommand(gxCmdBuf, gxCommand);

}

__attribute__((always_inline))
static INLINE void ctrGuSetVertexShaderFloatUniform(int id, float* data, int count)
{
   GPUCMD_AddWrite(GPUREG_VSH_FLOATUNIFORM_CONFIG, 0x80000000|(u32)id);
   GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA, (u32*)data, (u32)count * 4);
}

#define CTRGU_ATTRIBFMT(f, n) ((((n)-1)<<2)|((f)&3))

__attribute__((always_inline))
static INLINE void ctrGuSetAttributeBuffers(u32 total_attributes,
      void* base_address, u64 attribute_formats, u32 buffer_size)
{
   u32 param[0x28];

   memset(param, 0x00, sizeof(param));

   param[0x0]=((u32)base_address)>>3;
   param[0x1]=attribute_formats & 0xFFFFFFFF;
   param[0x2]=((total_attributes-1)<<28)|0xFFF0000|((attribute_formats>>32)&0xFFFF);
   param[0x4]=0x76543210;
   param[0x5]=(total_attributes<<28)|((buffer_size&0xFFF)<<16)|0xBA98;

   GPUCMD_AddIncrementalWrites(GPUREG_ATTRIBBUFFERS_LOC, param, 0x00000027);
   GPUCMD_AddMaskedWrite(GPUREG_VSH_INPUTBUFFER_CONFIG, 0xB, 0xA0000000|(total_attributes-1));
   GPUCMD_AddWrite(GPUREG_VSH_NUM_ATTR, (total_attributes-1));
   GPUCMD_AddIncrementalWrites(GPUREG_VSH_ATTRIBUTES_PERMUTATION_LOW, ((u32[]){0x76543210, 0xBA98}), 2);
}

__attribute__((always_inline))
static INLINE void ctrGuSetAttributeBuffersAddress(u32* baseAddress)
{
   GPUCMD_AddWrite(GPUREG_ATTRIBBUFFERS_LOC, ((u32)baseAddress)>>3);
}

__attribute__((always_inline))
static INLINE void ctrGuSetVshGsh(shaderProgram_s* sp, DVLB_s* dvlb, u32 vsh_output_count, u32 gsh_input_count)
{
   dvlb->DVLE[0].outmapData[0] = vsh_output_count;
   dvlb->DVLE[0].outmapMask = (1 << vsh_output_count) - 1;
   shaderProgramInit(sp);
   shaderProgramSetVsh(sp, &dvlb->DVLE[0]);
   shaderProgramSetGsh(sp, &dvlb->DVLE[1], gsh_input_count);
}

__attribute__((always_inline))
static INLINE int ctrgu_swizzle_coords(int x, int y, int width)
{
   int pos = (x & 0x1) << 0 | ((x & 0x2) << 1) | ((x & 0x4) << 2) |
             (y & 0x1) << 1 | ((y & 0x2) << 2) | ((y & 0x4) << 3);

   return ((x >> 3) << 6) + ((y >> 3) * ((width >> 3) << 6)) + pos;

}

#endif // CTR_GU_H
