#ifndef __GX_REGDEF_H__
#define __GX_REGDEF_H__

#include <gctypes.h>

#define STRUCT_REGDEF_SIZE		1440

struct __gx_regdef
{
	u16 cpSRreg;
	u16 cpCRreg;
	u16 cpCLreg;
	u16 xfFlush;
	u16 xfFlushExp;
	u16 xfFlushSafe;
	u32 gxFifoInited;
	u32 vcdClear;
	u32 VATTable;
	u32 mtxIdxLo;
	u32 mtxIdxHi;
	u32 texCoordManually;
	u32 vcdLo;
	u32 vcdHi;
	u32 vcdNrms;
	u32 dirtyState;
	u32 perf0Mode;
	u32 perf1Mode;
	u32 cpPerfMode;
	u32 VAT0reg[8];
	u32 VAT1reg[8];
	u32 VAT2reg[8];
	u32 texMapSize[8];
	u32 texMapWrap[8];
	u32 sciTLcorner;
	u32 sciBRcorner;
	u32 lpWidth;
	u32 genMode;
	u32 suSsize[8];
	u32 suTsize[8];
	u32 tevTexMap[16];
	u32 tevColorEnv[16];
	u32 tevAlphaEnv[16];
	u32 tevSwapModeTable[8];
	u32 tevRasOrder[11];
	u32 tevTexCoordEnable;
	u32 tevIndMask;
	u32 texCoordGen[8];
	u32 texCoordGen2[8];
	u32 dispCopyCntrl;
	u32 dispCopyDst;
	u32 dispCopyTL;
	u32 dispCopyWH;
	u32 texCopyCntrl;
	u32 texCopyDst;
	u32 texCopyTL;
	u32 texCopyWH;
	u32 peZMode;
	u32 peCMode0;
	u32 peCMode1;
	u32 peCntrl;
	u32 chnAmbColor[2];
	u32 chnMatColor[2];
	u32 chnCntrl[4];
	GXTexRegion texRegion[24];
	GXTlutRegion tlutRegion[20];
	u8 saveDLctx;
	u8 gxFifoUnlinked;
	u8 texCopyZTex;
	u8 _pad;
} __attribute__((packed));

struct __gxfifo {
	vu32 buf_start;
	vu32 buf_end;
	vu32 size;
	vu32 hi_mark;
	vu32 lo_mark;
	vu32 rd_ptr;
	vu32 wt_ptr;
	vu32 rdwt_dst;
	vu8 fifo_wrap;
	vu8 cpufifo_ready;
	vu8 gpfifo_ready;
	u8 _pad[93];
} __attribute__((packed));

struct __gx_litobj
{
	u32 _pad[3];
	u32 col;
	f32 a0;
	f32 a1;
	f32 a2;
	f32 k0;
	f32 k1;
	f32 k2;
	f32 px;
	f32 py;
	f32 pz;
	f32 nx;
	f32 ny;
	f32 nz;
} __attribute__((packed));

struct __gx_texobj
{
	u32 tex_filt;
	u32 tex_lod;
	u32 tex_size;
	u32 tex_maddr;
	u32 usr_data;
	u32 tex_fmt;
	u32 tex_tlut;
	u16 tex_tile_cnt;
	u8 tex_tile_type;
	u8 tex_flag;
} __attribute__((packed));

struct __gx_tlutobj
{
	u32 tlut_fmt;
	u32 tlut_maddr;
	u16 tlut_nentries;
	u8 _pad[2];
} __attribute__((packed));

struct __gx_texregion
{
	u32 tmem_even;
	u32 tmem_odd;
	u16 size_even;
	u16 size_odd;
	u8 ismipmap;
	u8 iscached;
	u8 _pad[2];
} __attribute__((packed));

struct __gx_tlutregion
{
	u32 tmem_addr_conf;
	u32 tmem_addr_base;
	u32 tlut_maddr;
	u16 tlut_nentries;
	u8 _pad[2];
} __attribute__((packed));

#endif
