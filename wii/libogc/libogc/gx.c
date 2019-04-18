#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "asm.h"
#include "processor.h"
#include "irq.h"
#include "lwp.h"
#include "system.h"
#include "video.h"
#include "video_types.h"
#include "lwp_watchdog.h"
#include "gx.h"
#include "gx_regdef.h"

#define TEXCACHE_TESTING

#define GX_FINISH		2

#if defined(HW_DOL)
	#define LARGE_NUMBER	(-1048576.0f)
#elif defined(HW_RVL)
	#define LARGE_NUMBER	(-1.0e+18f)
#endif

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

#define GX_LOAD_BP_REG(x)				\
	do {								\
		wgPipe->U8 = 0x61;				\
		asm volatile ("" ::: "memory" ); \
		wgPipe->U32 = (u32)(x);		\
		asm volatile ("" ::: "memory" ); \
	} while(0)

#define GX_LOAD_CP_REG(x, y)			\
	do {								\
		wgPipe->U8 = 0x08;				\
		asm volatile ("" ::: "memory" ); \
		wgPipe->U8 = (u8)(x);			\
		asm volatile ("" ::: "memory" ); \
		wgPipe->U32 = (u32)(y);		\
		asm volatile ("" ::: "memory" ); \
	} while(0)

#define GX_LOAD_XF_REG(x, y)			\
	do {								\
		wgPipe->U8 = 0x10;				\
		asm volatile ("" ::: "memory" ); \
		wgPipe->U32 = (u32)((x)&0xffff);		\
		asm volatile ("" ::: "memory" ); \
		wgPipe->U32 = (u32)(y);		\
		asm volatile ("" ::: "memory" ); \
	} while(0)

#define GX_LOAD_XF_REGS(x, n)			\
	do {								\
		wgPipe->U8 = 0x10;				\
		asm volatile ("" ::: "memory" ); \
		wgPipe->U32 = (u32)(((((n)&0xffff)-1)<<16)|((x)&0xffff));				\
		asm volatile ("" ::: "memory" ); \
	} while(0)

#define XY(x, y)   (((y) << 10) | (x))

#define GX_DEFAULT_BG	{64,64,64,255}
#define BLACK			{0,0,0,0}
#define WHITE			{255,255,255,255}

WGPipe* const wgPipe = (WGPipe*)0xCC008000;

static GXFifoObj _gpfifo;
static GXFifoObj _cpufifo;
static GXFifoObj _gxfifoobj;
static GXFifoObj _gx_dl_fifoobj;
static GXFifoObj _gx_old_cpufifo;
static void *_gxcurrbp = NULL;
static lwp_t _gxcurrentlwp = LWP_THREAD_NULL;

static u32 _gxcpufifoready = 0;
static u32 _gxgpfifoready = 0;
static u32 _cpgplinked = 0;
static u16 _gxgpstatus = 0;
static vu32 _gxoverflowsuspend = 0;
static vu32 _gxoverflowcount = 0;
static vu32 _gxfinished = 0;
static lwpq_t _gxwaitfinish;

static GXBreakPtCallback breakPtCB = NULL;
static GXDrawDoneCallback drawDoneCB = NULL;
static GXDrawSyncCallback tokenCB = NULL;

static GXTexRegionCallback regionCB = NULL;
static GXTlutRegionCallback tlut_regionCB = NULL;

static vu32* const _piReg = (u32*)0xCC003000;
static vu16* const _cpReg = (u16*)0xCC000000;
static vu16* const _peReg = (u16*)0xCC001000;
static vu16* const _memReg = (u16*)0xCC004000;

static u8 _gxtevcolid[9] = {0,1,0,1,0,1,7,5,6};
static u8 _gxtexmode0ids[8] = {0x80,0x81,0x82,0x83,0xA0,0xA1,0xA2,0xA3};
static u8 _gxtexmode1ids[8] = {0x84,0x85,0x86,0x87,0xA4,0xA5,0xA6,0xA7};
static u8 _gxteximg0ids[8] = {0x88,0x89,0x8A,0x8B,0xA8,0xA9,0xAA,0xAB};
static u8 _gxteximg1ids[8] = {0x8C,0x8D,0x8E,0x8F,0xAC,0xAD,0xAE,0xAF};
static u8 _gxteximg2ids[8] = {0x90,0x91,0x92,0x93,0xB0,0xB1,0xB2,0xB3};
static u8 _gxteximg3ids[8] = {0x94,0x95,0x96,0x97,0xB4,0xB5,0xB6,0xB7};
static u8 _gxtextlutids[8] = {0x98,0x99,0x9A,0x9B,0xB8,0xB9,0xBA,0xBB};

#if defined(HW_RVL)
static u32 _gxtexregionaddrtable[48] =
{
	0x00000000,0x00010000,0x00020000,0x00030000,
	0x00040000,0x00050000,0x00060000,0x00070000,
	0x00008000,0x00018000,0x00028000,0x00038000,
	0x00048000,0x00058000,0x00068000,0x00078000,
	0x00000000,0x00090000,0x00020000,0x000B0000,
	0x00040000,0x00098000,0x00060000,0x000B8000,
	0x00080000,0x00010000,0x000A0000,0x00030000,
	0x00088000,0x00050000,0x000A8000,0x00070000,
	0x00000000,0x00090000,0x00020000,0x000B0000,
	0x00040000,0x00090000,0x00060000,0x000B0000,
	0x00080000,0x00010000,0x000A0000,0x00030000,
	0x00080000,0x00050000,0x000A0000,0x00070000
};
#endif

extern u8 __gxregs[];
static struct __gx_regdef *__gx = (struct __gx_regdef*)__gxregs;
static u8 _gx_saved_data[STRUCT_REGDEF_SIZE] ATTRIBUTE_ALIGN(32);

static s32 __gx_onreset(s32 final);

static sys_resetinfo __gx_resetinfo = {
	{},
	__gx_onreset,
	127
};

static __inline__ BOOL IsWriteGatherBufferEmpty()
{
	return !(mfwpar()&1);
}

static __inline__ void DisableWriteGatherPipe()
{
	mthid2((mfhid2()&~0x40000000));
}

static __inline__ void EnableWriteGatherPipe()
{
	mtwpar(0x0C008000);
	mthid2((mfhid2()|0x40000000));
}

static __inline__ void __GX_ResetWriteGatherPipe()
{
	while(mfwpar()&1);
	mtwpar(0x0C008000);
}

static __inline__ void __GX_FifoLink(u8 enable)
{
	__gx->cpCRreg = ((__gx->cpCRreg&~0x10)|(_SHIFTL(enable,4,1)));
	_cpReg[1] = __gx->cpCRreg;
}

static __inline__ void __GX_WriteFifoIntReset(u8 inthi,u8 intlo)
{
	__gx->cpCLreg = ((__gx->cpCLreg&~0x03)|(_SHIFTL(intlo,1,1))|(inthi&1));
	_cpReg[2] = __gx->cpCLreg;
}

static __inline__ void __GX_WriteFifoIntEnable(u8 inthi, u8 intlo)
{
	__gx->cpCRreg = ((__gx->cpCRreg&~0x0C)|(_SHIFTL(intlo,3,1))|(_SHIFTL(inthi,2,1)));
	_cpReg[1] = __gx->cpCRreg;
}

static __inline__ void __GX_FifoReadEnable()
{
	__gx->cpCRreg = ((__gx->cpCRreg&~0x01)|1);
	_cpReg[1] = __gx->cpCRreg;
}

static __inline__ void __GX_FifoReadDisable()
{
	__gx->cpCRreg = ((__gx->cpCRreg&~0x01)|0);
	_cpReg[1] = __gx->cpCRreg;
}

static s32 __gx_onreset(s32 final)
{
	if(final==FALSE) {
		GX_Flush();
		GX_AbortFrame();
	}
	return 1;
}

static u32 __GX_IsGPFifoReady()
{
	return _gxgpfifoready;
}

static u32 __GX_CPGPLinkCheck()
{
	struct __gxfifo *gpfifo = (struct __gxfifo*)&_gpfifo;
	struct __gxfifo *cpufifo = (struct __gxfifo*)&_cpufifo;

	if(!_gxcpufifoready || !_gxgpfifoready) return 0;

	if((cpufifo->buf_start==gpfifo->buf_start)&&(cpufifo->buf_end==gpfifo->buf_end)) return 1;

	return 0;
}

static void __GX_InitRevBits()
{
	s32 i;

	i=0;
	while(i<8) {
		__gx->VAT0reg[i] = 0x40000000;
		__gx->VAT1reg[i] = 0x80000000;
		GX_LOAD_CP_REG((0x0080|i),__gx->VAT1reg[i]);
		i++;
	}

	GX_LOAD_XF_REG(0x1000,0x3f);
	GX_LOAD_XF_REG(0x1012,0x01);

	GX_LOAD_BP_REG(0x5800000f);

}

static void __GX_WaitAbort(u32 delay)
{
	u64 start,end;

	start = gettime();
	while(1) {
		end = gettime();
		if(diff_ticks(start,end)>=(u64)delay) break;
	};
}

#ifdef HW_RVL
static u32 __GX_ReadMemCounterU32(u32 reg)
{
	u16 lcnt,ucnt,tmp;

	tmp = _memReg[reg];
	do {
		ucnt = tmp;
		lcnt = _memReg[reg+1];
		tmp = _memReg[reg];
	} while(tmp!=ucnt);
	return (u32)((ucnt<<16)|lcnt);
}

static void __GX_WaitAbortPixelEngine()
{
	u32 cnt,tmp;

	cnt = __GX_ReadMemCounterU32(39);
	do {
		tmp = cnt;
		__GX_WaitAbort(8);
		cnt = __GX_ReadMemCounterU32(39);
	} while(cnt!=tmp);
}

static void __GX_Abort()
{
	if(__gx->gxFifoInited && __GX_IsGPFifoReady())
		__GX_WaitAbortPixelEngine();

	_piReg[6] = 1;
	__GX_WaitAbort(50);

	_piReg[6] = 0;
	__GX_WaitAbort(5);
}
#endif

static void __GX_SaveFifo()
{
	s32 rdwt_dst;
	u32 level,val;
	struct __gxfifo *cpufifo = (struct __gxfifo*)&_cpufifo;
	struct __gxfifo *gpfifo = (struct __gxfifo*)&_gpfifo;

	_CPU_ISR_Disable(level);

	if(_gxcpufifoready) {
		val = _piReg[0x05];
		cpufifo->wt_ptr = (u32)MEM_PHYSICAL_TO_K0((val&0x1FFFFFE0));
		cpufifo->fifo_wrap = ((val&0x20000000)==0x20000000);
	}

	if(_gxgpfifoready) {
		gpfifo->rd_ptr = (u32)MEM_PHYSICAL_TO_K0(_SHIFTL(_cpReg[29],16,16)|(_cpReg[28]&0xffff));
		gpfifo->rdwt_dst = (_SHIFTL(_cpReg[25],16,16)|(_cpReg[24]&0xffff));
	}

	if(_cpgplinked) {
		cpufifo->rd_ptr = gpfifo->rd_ptr;
		cpufifo->rdwt_dst = gpfifo->rdwt_dst;
		gpfifo->wt_ptr = cpufifo->wt_ptr;
	} else if(_gxcpufifoready) {
		rdwt_dst = (cpufifo->wt_ptr - cpufifo->rd_ptr);
		if(rdwt_dst<0) cpufifo->rdwt_dst = (cpufifo->rdwt_dst + cpufifo->size);
		else cpufifo->rdwt_dst = rdwt_dst;
	}

	_CPU_ISR_Restore(level);
}

static void __GX_CleanGPFifo()
{
	u32 level;
	struct __gxfifo *gpfifo = (struct __gxfifo*)&_gpfifo;
	struct __gxfifo *cpufifo = (struct __gxfifo*)&_cpufifo;

	if(!_gxgpfifoready) return;

	_CPU_ISR_Disable(level);
	__GX_FifoReadDisable();
	__GX_WriteFifoIntEnable(FALSE,FALSE);

	gpfifo->rd_ptr = gpfifo->wt_ptr;
	gpfifo->rdwt_dst = 0;

	/* setup rd<->wd dist */
	_cpReg[24] = _SHIFTL(gpfifo->rdwt_dst,0,16);
	_cpReg[25] = _SHIFTR(gpfifo->rdwt_dst,16,16);

	/* setup wt ptr */
	_cpReg[26] = _SHIFTL(MEM_VIRTUAL_TO_PHYSICAL(gpfifo->wt_ptr),0,16);
	_cpReg[27] = _SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(gpfifo->wt_ptr),16,16);

	/* setup rd ptr */
	_cpReg[28] = _SHIFTL(MEM_VIRTUAL_TO_PHYSICAL(gpfifo->rd_ptr),0,16);
	_cpReg[29] = _SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(gpfifo->rd_ptr),16,16);
	ppcsync();

	if(_cpgplinked) {
		cpufifo->rd_ptr = gpfifo->rd_ptr;
		cpufifo->wt_ptr = gpfifo->wt_ptr;
		cpufifo->rdwt_dst = gpfifo->rdwt_dst;

		_piReg[5] = (cpufifo->wt_ptr&0x1FFFFFE0);
		__GX_WriteFifoIntEnable(TRUE,FALSE);
		__GX_FifoLink(TRUE);
	}
	__gx->cpCRreg &= ~0x22;
	_cpReg[1] = __gx->cpCRreg;
	breakPtCB = NULL;

	__GX_WriteFifoIntReset(TRUE,TRUE);
	__GX_FifoReadEnable();
	_CPU_ISR_Restore(level);
}

static void __GXOverflowHandler()
{
	if(!_gxoverflowsuspend) {
		_gxoverflowsuspend = 1;
		_gxoverflowcount++;
		__GX_WriteFifoIntEnable(GX_DISABLE,GX_ENABLE);
		__GX_WriteFifoIntReset(GX_TRUE,GX_FALSE);
		LWP_SuspendThread(_gxcurrentlwp);
	}
}

static void __GXUnderflowHandler()
{
	if(_gxoverflowsuspend) {
		_gxoverflowsuspend = 0;
		LWP_ResumeThread(_gxcurrentlwp);
		__GX_WriteFifoIntReset(GX_TRUE,GX_TRUE);
		__GX_WriteFifoIntEnable(GX_ENABLE,GX_DISABLE);
	}
}

static void __GXCPInterruptHandler(u32 irq,void *ctx)
{
	__gx->cpSRreg = _cpReg[0];

	if((__gx->cpCRreg&0x08) && (__gx->cpSRreg&0x02))
		__GXUnderflowHandler();

	if((__gx->cpCRreg&0x04) && (__gx->cpSRreg&0x01))
		__GXOverflowHandler();

	if((__gx->cpCRreg&0x20) && (__gx->cpSRreg&0x10)) {
		__gx->cpCRreg &= ~0x20;
		_cpReg[1] = __gx->cpCRreg;
		if(breakPtCB)
			breakPtCB();
	}
}

static void __GXTokenInterruptHandler(u32 irq,void *ctx)
{
	u16 token = _peReg[7];

	if(tokenCB)
		tokenCB(token);

	_peReg[5] = (_peReg[5]&~0x04)|0x04;
}

static void __GXFinishInterruptHandler(u32 irq,void *ctx)
{
	_peReg[5] = (_peReg[5]&~0x08)|0x08;
	_gxfinished = 1;

	if(drawDoneCB)
		drawDoneCB();

	LWP_ThreadBroadcast(_gxwaitfinish);
}

static void __GX_PEInit()
{
	IRQ_Request(IRQ_PI_PETOKEN,__GXTokenInterruptHandler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_PI_PETOKEN));

	IRQ_Request(IRQ_PI_PEFINISH,__GXFinishInterruptHandler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_PI_PEFINISH));

	_peReg[5] = 0x0F;
}

static void __GX_FifoInit()
{
	IRQ_Request(IRQ_PI_CP,__GXCPInterruptHandler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_PI_CP));

	memset(&_cpufifo,0,sizeof(GXFifoObj));
	memset(&_gpfifo,0,sizeof(GXFifoObj));

	_gxcpufifoready = 0;
	_gxgpfifoready = 0;
	_cpgplinked = 0;
	_gxoverflowsuspend = 0;
	_gxcurrentlwp = LWP_GetSelf();
}

static void __GX_SetTmemConfig(u8 nr)
{
	if(nr==0) {
		//  Set_TextureImage0-3, GXTexMapID=0-3 tmem_offset=00000000, cache_width=32 kb, cache_height=32 kb, image_type=cached
		GX_LOAD_BP_REG(0x8c0d8000);
		GX_LOAD_BP_REG(0x900dc000);
		GX_LOAD_BP_REG(0x8d0d8400);
		GX_LOAD_BP_REG(0x910dc400);
		GX_LOAD_BP_REG(0x8e0d8800);
		GX_LOAD_BP_REG(0x920dc800);
		GX_LOAD_BP_REG(0x8f0d8c00);
		GX_LOAD_BP_REG(0x930dcc00);

		//  Set_TextureImage0-3, GXTexMapID=4-7 tmem_offset=00010000, cache_width=32 kb, cache_height=32 kb, image_type=cached
		GX_LOAD_BP_REG(0xac0d9000);
		GX_LOAD_BP_REG(0xb00dd000);
		GX_LOAD_BP_REG(0xad0d9400);
		GX_LOAD_BP_REG(0xb10dd400);
		GX_LOAD_BP_REG(0xae0d9800);
		GX_LOAD_BP_REG(0xb20dd800);
		GX_LOAD_BP_REG(0xaf0d9c00);
		GX_LOAD_BP_REG(0xb30ddc00);

		return;
	}

	if(nr==1) {
		//  Set_TextureImage0-3, GXTexMapID=0-3 tmem_offset=00000000, cache_width=32 kb, cache_height=32 kb, image_type=cached
		GX_LOAD_BP_REG(0x8c0d8000);
		GX_LOAD_BP_REG(0x900dc000);
		GX_LOAD_BP_REG(0x8d0d8800);
		GX_LOAD_BP_REG(0x910dc800);
		GX_LOAD_BP_REG(0x8e0d9000);
		GX_LOAD_BP_REG(0x920dd000);
		GX_LOAD_BP_REG(0x8f0d9800);
		GX_LOAD_BP_REG(0x930dd800);

		//  Set_TextureImage0-3, GXTexMapID=4-7 tmem_offset=00010000, cache_width=32 kb, cache_height=32 kb, image_type=cached
		GX_LOAD_BP_REG(0xac0da000);
		GX_LOAD_BP_REG(0xb00de000);
		GX_LOAD_BP_REG(0xad0da800);
		GX_LOAD_BP_REG(0xb10de800);
		GX_LOAD_BP_REG(0xae0db000);
		GX_LOAD_BP_REG(0xb20df000);
		GX_LOAD_BP_REG(0xaf0db800);
		GX_LOAD_BP_REG(0xb30df800);

		return;
	}

	if(nr==2) {
		//  Set_TextureImage0-3, GXTexMapID=0-3 tmem_offset=00000000, cache_width=32 kb, cache_height=32 kb, image_type=cached
		GX_LOAD_BP_REG(0x8c0d8000);
		GX_LOAD_BP_REG(0x900dc000);
		GX_LOAD_BP_REG(0x8d0d8800);
		GX_LOAD_BP_REG(0x910dc800);
		GX_LOAD_BP_REG(0x8e0d9000);
		GX_LOAD_BP_REG(0x920dd000);
		GX_LOAD_BP_REG(0x8f0d9800);
		GX_LOAD_BP_REG(0x930dd800);

		//  Set_TextureImage0-3, GXTexMapID=4-7 tmem_offset=00010000, cache_width=32 kb, cache_height=32 kb, image_type=cached
		GX_LOAD_BP_REG(0xac0da000);
		GX_LOAD_BP_REG(0xb00dc400);
		GX_LOAD_BP_REG(0xad0da800);
		GX_LOAD_BP_REG(0xb10dcc00);
		GX_LOAD_BP_REG(0xae0db000);
		GX_LOAD_BP_REG(0xb20dd400);
		GX_LOAD_BP_REG(0xaf0db800);
		GX_LOAD_BP_REG(0xb30ddc00);

		return;
	}
}

#if defined(HW_RVL)
static GXTexRegion* __GXDefTexRegionCallback(GXTexObj *obj,u8 mapid)
{
	u32 fmt,mipmap;
	GXTexRegion *ret = NULL;

	fmt = GX_GetTexObjFmt(obj);
	mipmap = GX_GetTexObjMipMap(obj);
	if(fmt>=GX_TF_CI4 && fmt<=GX_TF_CI14)
		return &__gx->texRegion[mapid];
	else if(fmt==GX_TF_CMPR)
		ret = &__gx->texRegion[mapid];
	else
		ret = &__gx->texRegion[mapid+8];

	if(mipmap) ret = &__gx->texRegion[mapid+16];

	return ret;
}
#else
static GXTexRegion* __GXDefTexRegionCallback(GXTexObj *obj,u8 mapid)
{
	u32 fmt;
	u32 idx;
	static u32 regionA = 0;
	static u32 regionB = 0;
	GXTexRegion *ret = NULL;

	fmt = GX_GetTexObjFmt(obj);
	if(fmt==0x0008 || fmt==0x0009 || fmt==0x000a) {
		idx = regionB++;
		ret = &__gx->texRegion[(idx&3)+8];
	} else {
		idx = regionA++;
		ret = &__gx->texRegion[(idx&7)];
	}
	return ret;
}
#endif

static GXTlutRegion* __GXDefTlutRegionCallback(u32 tlut_name)
{
	return &__gx->tlutRegion[tlut_name];
}

static void __GX_InitGX()
{
	s32 i;
	u32 flag;
	GXRModeObj *rmode;
	Mtx identity_matrix =
	{
		{1,0,0,0},
		{0,1,0,0},
		{0,0,1,0}
	};

	rmode = VIDEO_GetPreferredMode(NULL);

	GX_SetCopyClear((GXColor)BLACK,0xffffff);
	GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD1,GX_TG_MTX2x4,GX_TG_TEX1,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD2,GX_TG_MTX2x4,GX_TG_TEX2,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD3,GX_TG_MTX2x4,GX_TG_TEX3,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD4,GX_TG_MTX2x4,GX_TG_TEX4,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD5,GX_TG_MTX2x4,GX_TG_TEX5,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD6,GX_TG_MTX2x4,GX_TG_TEX6,GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD7,GX_TG_MTX2x4,GX_TG_TEX7,GX_IDENTITY);
	GX_SetNumTexGens(1);
	GX_ClearVtxDesc();
	GX_InvVtxCache();

	GX_SetLineWidth(6,GX_TO_ZERO);
	GX_SetPointSize(6,GX_TO_ZERO);

	GX_EnableTexOffsets(GX_TEXCOORD0,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD1,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD2,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD3,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD4,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD5,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD6,GX_DISABLE,GX_DISABLE);
	GX_EnableTexOffsets(GX_TEXCOORD7,GX_DISABLE,GX_DISABLE);

	GX_LoadPosMtxImm(identity_matrix,GX_PNMTX0);
	GX_LoadNrmMtxImm(identity_matrix,GX_PNMTX0);
	GX_SetCurrentMtx(GX_PNMTX0);
	GX_LoadTexMtxImm(identity_matrix,GX_IDENTITY,GX_MTX3x4);
	GX_LoadTexMtxImm(identity_matrix,GX_DTTIDENTITY,GX_MTX3x4);

	GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
	GX_SetCoPlanar(GX_DISABLE);
	GX_SetCullMode(GX_CULL_BACK);
	GX_SetClipMode(GX_CLIP_ENABLE);

	GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetScissorBoxOffset(0,0);

	GX_SetNumChans(0);

	GX_SetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHTNULL,GX_DF_NONE,GX_AF_NONE);
	GX_SetChanAmbColor(GX_COLOR0A0,(GXColor)BLACK);
	GX_SetChanMatColor(GX_COLOR0A0,(GXColor)WHITE);

	GX_SetChanCtrl(GX_COLOR1A1,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHTNULL,GX_DF_NONE,GX_AF_NONE);
	GX_SetChanAmbColor(GX_COLOR1A1,(GXColor)BLACK);
	GX_SetChanMatColor(GX_COLOR1A1,(GXColor)WHITE);

	GX_InvalidateTexAll();
	GX_SetTexRegionCallback(__GXDefTexRegionCallback);
	GX_SetTlutRegionCallback(__GXDefTlutRegionCallback);

	GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE1,GX_TEXCOORD1,GX_TEXMAP1,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE2,GX_TEXCOORD2,GX_TEXMAP2,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE3,GX_TEXCOORD3,GX_TEXMAP3,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE4,GX_TEXCOORD4,GX_TEXMAP4,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE5,GX_TEXCOORD5,GX_TEXMAP5,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE6,GX_TEXCOORD6,GX_TEXMAP6,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE7,GX_TEXCOORD7,GX_TEXMAP7,GX_COLOR0A0);
	GX_SetTevOrder(GX_TEVSTAGE8,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE9,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE10,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE11,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE12,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE13,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE14,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE15,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLORNULL);
	GX_SetNumTevStages(1);
	GX_SetTevOp(GX_TEVSTAGE0,GX_REPLACE);
	GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
	GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z8,0);
	for(i=0;i<GX_MAX_TEVSTAGE;i++) {
		GX_SetTevKColorSel(i,GX_TEV_KCSEL_1_4);
		GX_SetTevKAlphaSel(i,GX_TEV_KASEL_1);
		GX_SetTevSwapMode(i,GX_TEV_SWAP0,GX_TEV_SWAP0);
	}

	GX_SetTevSwapModeTable(GX_TEV_SWAP0,GX_CH_RED,GX_CH_GREEN,GX_CH_BLUE,GX_CH_ALPHA);
	GX_SetTevSwapModeTable(GX_TEV_SWAP1,GX_CH_RED,GX_CH_RED,GX_CH_RED,GX_CH_ALPHA);
	GX_SetTevSwapModeTable(GX_TEV_SWAP2,GX_CH_GREEN,GX_CH_GREEN,GX_CH_GREEN,GX_CH_ALPHA);
	GX_SetTevSwapModeTable(GX_TEV_SWAP3,GX_CH_BLUE,GX_CH_BLUE,GX_CH_BLUE,GX_CH_ALPHA);
	for(i=0;i<GX_MAX_TEVSTAGE;i++) {
		GX_SetTevDirect(i);
	}

	GX_SetNumIndStages(0);
	GX_SetIndTexCoordScale(GX_INDTEXSTAGE0,GX_ITS_1,GX_ITS_1);
	GX_SetIndTexCoordScale(GX_INDTEXSTAGE1,GX_ITS_1,GX_ITS_1);
	GX_SetIndTexCoordScale(GX_INDTEXSTAGE2,GX_ITS_1,GX_ITS_1);
	GX_SetIndTexCoordScale(GX_INDTEXSTAGE3,GX_ITS_1,GX_ITS_1);

	GX_SetFog(GX_FOG_NONE,0,1,0.1,1,(GXColor)BLACK);
	GX_SetFogRangeAdj(GX_DISABLE,0,NULL);

	GX_SetBlendMode(GX_BM_NONE,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_CLEAR);
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetAlphaUpdate(GX_ENABLE);
	GX_SetZMode(GX_ENABLE,GX_LEQUAL,GX_TRUE);
	GX_SetZCompLoc(GX_TRUE);
	GX_SetDither(GX_ENABLE);
	GX_SetDstAlpha(GX_DISABLE,0);
	GX_SetPixelFmt(GX_PF_RGB8_Z24,GX_ZC_LINEAR);

	GX_SetFieldMask(GX_ENABLE,GX_ENABLE);

	flag = 0;
	if(rmode->viHeight==(rmode->xfbHeight<<1)) flag = 1;
	GX_SetFieldMode(rmode->field_rendering,flag);

	GX_SetCopyClear((GXColor)GX_DEFAULT_BG,0x00ffffff);
	GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyYScale(1.0);
	GX_SetCopyClamp(GX_CLAMP_TOP|GX_CLAMP_BOTTOM);
	GX_SetCopyFilter(GX_FALSE,NULL,GX_FALSE,NULL);
	GX_SetDispCopyGamma(GX_GM_1_0);
	GX_SetDispCopyFrame2Field(GX_COPY_PROGRESSIVE);
	GX_ClearBoundingBox();

	GX_PokeColorUpdate(GX_TRUE);
	GX_PokeAlphaUpdate(GX_TRUE);
	GX_PokeDither(GX_FALSE);
	GX_PokeBlendMode(GX_BM_NONE,GX_BL_ZERO,GX_BL_ONE,GX_LO_SET);
	GX_PokeAlphaMode(GX_ALWAYS,0);
	GX_PokeAlphaRead(GX_READ_FF);
	GX_PokeDstAlpha(GX_DISABLE,0);
	GX_PokeZMode(GX_TRUE,GX_ALWAYS,GX_TRUE);

	GX_SetGPMetric(GX_PERF0_NONE,GX_PERF1_NONE);
	GX_ClearGPMetric();
}

static void __GX_FlushTextureState()
{
	GX_LOAD_BP_REG(__gx->tevIndMask);
}

static void __GX_XfVtxSpecs()
{
	u32 xfvtxspecs = 0;
	u32 nrms,texs,cols;

	cols = 0;
	if(__gx->vcdLo&0x6000) cols++;
	if(__gx->vcdLo&0x18000) cols++;

	nrms = 0;
	if(__gx->vcdNrms==1) nrms = 1;
	else if(__gx->vcdNrms==2) nrms = 2;

	texs = 0;
	if(__gx->vcdHi&0x3) texs++;
	if(__gx->vcdHi&0xc) texs++;
	if(__gx->vcdHi&0x30) texs++;
	if(__gx->vcdHi&0xc0) texs++;
	if(__gx->vcdHi&0x300) texs++;
	if(__gx->vcdHi&0xc00) texs++;
	if(__gx->vcdHi&0x3000) texs++;
	if(__gx->vcdHi&0xc000) texs++;

	xfvtxspecs = (_SHIFTL(texs,4,4))|(_SHIFTL(nrms,2,2))|(cols&0x3);
	GX_LOAD_XF_REG(0x1008,xfvtxspecs);
}

static void __GX_SetMatrixIndex(u32 mtx)
{
	if(mtx<5) {
		GX_LOAD_CP_REG(0x30,__gx->mtxIdxLo);
		GX_LOAD_XF_REG(0x1018,__gx->mtxIdxLo);
	} else {
		GX_LOAD_CP_REG(0x40,__gx->mtxIdxHi);
		GX_LOAD_XF_REG(0x1019,__gx->mtxIdxHi);
	}
}

static void __GX_SendFlushPrim()
{
	u32 tmp,tmp2,cnt;

	tmp = (__gx->xfFlush*__gx->xfFlushExp);

	wgPipe->U8 = 0x98;
	wgPipe->U16 = __gx->xfFlush;

	tmp2 = (tmp+3)/4;
	if(tmp>0) {
		cnt = tmp2/8;
		while(cnt) {
			wgPipe->U32 = 0;
					wgPipe->U32 = 0;
					wgPipe->U32 = 0;
					wgPipe->U32 = 0;
					wgPipe->U32 = 0;
					wgPipe->U32 = 0;
					wgPipe->U32 = 0;
					wgPipe->U32 = 0;
					cnt--;
		}
		tmp2 &= 0x0007;
		if(tmp2) {
			while(tmp2) {
				wgPipe->U32 = 0;
							tmp2--;
			}
		}
	}
	__gx->xfFlush = 1;
}

static void __GX_SetVCD()
{
	GX_LOAD_CP_REG(0x50,__gx->vcdLo);
	GX_LOAD_CP_REG(0x60,__gx->vcdHi);
	__GX_XfVtxSpecs();
}

static void __GX_SetVAT()
{
	u8 setvtx = 0;
	s32 i;

	for(i=0;i<8;i++) {
		setvtx = (1<<i);
		if(__gx->VATTable&setvtx) {
			GX_LOAD_CP_REG((0x70+(i&7)),__gx->VAT0reg[i]);
			GX_LOAD_CP_REG((0x80+(i&7)),__gx->VAT1reg[i]);
			GX_LOAD_CP_REG((0x90+(i&7)),__gx->VAT2reg[i]);
		}
	}
	__gx->VATTable = 0;
}

static void __SetSURegs(u8 texmap,u8 texcoord)
{
	u32 reg;
	u16 wd,ht;
	u8 wrap_s,wrap_t;

	wd = __gx->texMapSize[texmap]&0x3ff;
	ht = _SHIFTR(__gx->texMapSize[texmap],10,10);
	wrap_s = __gx->texMapWrap[texmap]&3;
	wrap_t = _SHIFTR(__gx->texMapWrap[texmap],2,2);

	reg = (texcoord&0x7);
	__gx->suSsize[reg] = (__gx->suSsize[reg]&~0x0000ffff)|wd;
	__gx->suTsize[reg] = (__gx->suTsize[reg]&~0x0000ffff)|ht;
	__gx->suSsize[reg] = (__gx->suSsize[reg]&~0x00010000)|(_SHIFTL(wrap_s,16,1));
	__gx->suTsize[reg] = (__gx->suTsize[reg]&~0x00010000)|(_SHIFTL(wrap_t,16,1));

	GX_LOAD_BP_REG(__gx->suSsize[reg]);
	GX_LOAD_BP_REG(__gx->suTsize[reg]);
}

static void __GX_SetSUTexRegs()
{
	u32 i;
	u32 indtev,dirtev;
	u8 texcoord,texmap;
	u32 tevreg,tevm,texcm;

	dirtev = (_SHIFTR(__gx->genMode,10,4))+1;
	indtev = _SHIFTR(__gx->genMode,16,3);

	//indirect texture order
	for(i=0;i<indtev;i++) {
		switch(i) {
			case GX_INDTEXSTAGE0:
				texmap = __gx->tevRasOrder[2]&7;
				texcoord = _SHIFTR(__gx->tevRasOrder[2],3,3);
				break;
			case GX_INDTEXSTAGE1:
				texmap = _SHIFTR(__gx->tevRasOrder[2],6,3);
				texcoord = _SHIFTR(__gx->tevRasOrder[2],9,3);
				break;
			case GX_INDTEXSTAGE2:
				texmap = _SHIFTR(__gx->tevRasOrder[2],12,3);
				texcoord = _SHIFTR(__gx->tevRasOrder[2],15,3);
				break;
			case GX_INDTEXSTAGE3:
				texmap = _SHIFTR(__gx->tevRasOrder[2],18,3);
				texcoord = _SHIFTR(__gx->tevRasOrder[2],21,3);
				break;
			default:
				texmap = 0;
				texcoord = 0;
				break;
		}

		texcm = _SHIFTL(1,texcoord,1);
		if(!(__gx->texCoordManually&texcm))
			__SetSURegs(texmap,texcoord);
	}

	//direct texture order
	for(i=0;i<dirtev;i++) {
		tevreg = 3+(_SHIFTR(i,1,3));
		texmap = (__gx->tevTexMap[i]&0xff);

		if(i&1) texcoord = _SHIFTR(__gx->tevRasOrder[tevreg],15,3);
		else texcoord = _SHIFTR(__gx->tevRasOrder[tevreg],3,3);

		tevm = _SHIFTL(1,i,1);
		texcm = _SHIFTL(1,texcoord,1);
		if(texmap!=0xff && (__gx->tevTexCoordEnable&tevm) && !(__gx->texCoordManually&texcm)) {
			__SetSURegs(texmap,texcoord);
		}
	}
}

static void __GX_SetGenMode()
{
	GX_LOAD_BP_REG(__gx->genMode);
	__gx->xfFlush = 0;
}

static void __GX_UpdateBPMask()
{
#if defined(HW_DOL)
	u32 i;
	u32 nbmp,nres;
	u8 ntexmap;

	nbmp = _SHIFTR(__gx->genMode,16,3);

	nres = 0;
	for(i=0;i<nbmp;i++) {
		switch(i) {
			case GX_INDTEXSTAGE0:
				ntexmap = __gx->tevRasOrder[2]&7;
				break;
			case GX_INDTEXSTAGE1:
				ntexmap = _SHIFTR(__gx->tevRasOrder[2],6,3);
				break;
			case GX_INDTEXSTAGE2:
				ntexmap = _SHIFTR(__gx->tevRasOrder[2],12,3);
				break;
			case GX_INDTEXSTAGE3:
				ntexmap = _SHIFTR(__gx->tevRasOrder[2],18,3);
				break;
			default:
				ntexmap = 0;
				break;
		}
		nres |= (1<<ntexmap);
	}

	if((__gx->tevIndMask&0xff)!=nres) {
		__gx->tevIndMask = (__gx->tevIndMask&~0xff)|(nres&0xff);
		GX_LOAD_BP_REG(__gx->tevIndMask);
	}
#endif
}

static void __GX_SetIndirectMask(u32 mask)
{
	__gx->tevIndMask = ((__gx->tevIndMask&~0xff)|(mask&0xff));
	GX_LOAD_BP_REG(__gx->tevIndMask);
}

static void __GX_SetTexCoordGen()
{
	u32 i,mask;
	u32 texcoord;

	if(__gx->dirtyState&0x02000000) GX_LOAD_XF_REG(0x103f,(__gx->genMode&0xf));

	i = 0;
	texcoord = 0x1040;
	mask = _SHIFTR(__gx->dirtyState,16,8);
	while(mask) {
		if(mask&0x0001) {
			GX_LOAD_XF_REG(texcoord,__gx->texCoordGen[i]);
			GX_LOAD_XF_REG((texcoord+0x10),__gx->texCoordGen2[i]);
		}
		mask >>= 1;
		texcoord++;
		i++;
	}
}

static void __GX_SetChanColor()
{
	if(__gx->dirtyState&0x0100)
		GX_LOAD_XF_REG(0x100a,__gx->chnAmbColor[0]);
	if(__gx->dirtyState&0x0200)
		GX_LOAD_XF_REG(0x100b,__gx->chnAmbColor[1]);
	if(__gx->dirtyState&0x0400)
		GX_LOAD_XF_REG(0x100c,__gx->chnMatColor[0]);
	if(__gx->dirtyState&0x0800)
		GX_LOAD_XF_REG(0x100d,__gx->chnMatColor[1]);
}

static void __GX_SetChanCntrl()
{
	u32 i,chan,mask;

	if(__gx->dirtyState&0x01000000) GX_LOAD_XF_REG(0x1009,(_SHIFTR(__gx->genMode,4,3)));

	i = 0;
	chan = 0x100e;
	mask = _SHIFTR(__gx->dirtyState,12,4);
	while(mask) {
		if(mask&0x0001) GX_LOAD_XF_REG(chan,__gx->chnCntrl[i]);

		mask >>= 1;
		chan++;
		i++;
	}
}

static void __GX_SetDirtyState()
{
	if(__gx->dirtyState&0x0001) {
		__GX_SetSUTexRegs();
	}
	if(__gx->dirtyState&0x0002) {
		__GX_UpdateBPMask();
	}
	if(__gx->dirtyState&0x0004) {
		__GX_SetGenMode();
	}
	if(__gx->dirtyState&0x0008) {
		__GX_SetVCD();
	}
	if(__gx->dirtyState&0x0010) {
		__GX_SetVAT();
	}
	if(__gx->dirtyState&~0xff) {
		if(__gx->dirtyState&0x0f00) {
			__GX_SetChanColor();
		}
		if(__gx->dirtyState&0x0100f000) {
			__GX_SetChanCntrl();
		}
		if(__gx->dirtyState&0x02ff0000) {
			__GX_SetTexCoordGen();
		}
		if(__gx->dirtyState&0x04000000) {
			__GX_SetMatrixIndex(0);
			__GX_SetMatrixIndex(5);
		}
	}
	__gx->dirtyState = 0;
}

static u32 __GX_GetNumXfbLines(u16 efbHeight,u32 yscale)
{
	u32 tmp,tmp1;

	tmp = (((efbHeight-1)<<8)/yscale)+1;
	if(yscale>128 && yscale<256) {
		while(yscale&0x01) yscale >>= 1;
		tmp1 = yscale*(efbHeight/yscale);
		if(!(efbHeight-tmp1)) tmp++;
	}
	if(tmp>1024) tmp = 1024;

	return tmp;
}

GXFifoObj* GX_Init(void *base,u32 size)
{
	s32 i,re0,re1;
#if defined(HW_RVL)
	u32 tmem;
#else
	u32 tmem_even,tmem_odd;
#endif
	u32 divis,res;
	u32 divid = TB_BUS_CLOCK;
	GXTexRegion *region = NULL;
	GXTlutRegion *tregion = NULL;

	LWP_InitQueue(&_gxwaitfinish);
	SYS_RegisterResetFunc(&__gx_resetinfo);

	memset(__gxregs,0,STRUCT_REGDEF_SIZE);

	__GX_FifoInit();
	GX_InitFifoBase(&_gxfifoobj,base,size);
	GX_SetCPUFifo(&_gxfifoobj);
	GX_SetGPFifo(&_gxfifoobj);
	__GX_PEInit();
	EnableWriteGatherPipe();

	__gx->gxFifoInited = 1;

	__gx->tevIndMask = 0xff;
	__gx->tevIndMask = (__gx->tevIndMask&~0xff000000)|(_SHIFTL(0x0f,24,8));

	i=0;
	re0 = 0xc0;
	re1 = 0xc1;
	while(i<16) {
		__gx->tevColorEnv[i] = (__gx->tevColorEnv[i]&~0xff000000)|(_SHIFTL(re0,24,8));
		__gx->tevAlphaEnv[i] = (__gx->tevAlphaEnv[i]&~0xff000000)|(_SHIFTL(re1,24,8));
		re0 += 2; re1 += 2; i++;
	}

	__gx->texCoordManually = 0;
	__gx->dirtyState = 0;

	__gx->saveDLctx = 1;
	__gx->gxFifoUnlinked = 0;

	__gx->sciTLcorner = (__gx->sciTLcorner&~0xff000000)|(_SHIFTL(0x20,24,8));
	__gx->sciBRcorner = (__gx->sciBRcorner&~0xff000000)|(_SHIFTL(0x21,24,8));
	__gx->lpWidth = (__gx->lpWidth&~0xff000000)|(_SHIFTL(0x22,24,8));
	__gx->genMode = (__gx->genMode&~0xff000000)|(_SHIFTL(0x00,24,8));

	i=0;
	re0 = 0x30;
	re1 = 0x31;
	while(i<8) {
		__gx->suSsize[i] = (__gx->suSsize[i]&~0xff000000)|(_SHIFTL(re0,24,8));
		__gx->suTsize[i] = (__gx->suTsize[i]&~0xff000000)|(_SHIFTL(re1,24,8));
		re0 += 2; re1 += 2; i++;
	}

	__gx->peZMode = (__gx->peZMode&~0xff000000)|(_SHIFTL(0x40,24,8));
	__gx->peCMode0 = (__gx->peCMode0&~0xff000000)|(_SHIFTL(0x41,24,8));
	__gx->peCMode1 = (__gx->peCMode1&~0xff000000)|(_SHIFTL(0x42,24,8));
	__gx->peCntrl = (__gx->peCntrl&~0xff000000)|(_SHIFTL(0x43,24,8));

	i=0;
	re0 = 0x25;
	while(i<11) {
		__gx->tevRasOrder[i] = (__gx->tevRasOrder[i]&~0xff000000)|(_SHIFTL(re0,24,8));
		re0++; i++;
	}

	divis = 500;
	res = (u32)(divid/divis);
	__GX_FlushTextureState();
	GX_LOAD_BP_REG(0x69000000|((_SHIFTR(res,11,24))|0x0400));

	divis = 4224;
	res = (u32)(res/divis);
	__GX_FlushTextureState();
	GX_LOAD_BP_REG(0x46000000|(res|0x0200));

	i=0;
	re0 = 0xf6;
	while(i<8) {
		__gx->tevSwapModeTable[i] = (__gx->tevSwapModeTable[i]&~0xff000000)|(_SHIFTL(re0,24,8));
		re0++; i++;
	}

	__gx->tevTexCoordEnable = 0;
	__gx->perf0Mode = GX_PERF0_NONE;
	__gx->perf1Mode = GX_PERF1_NONE;
	__gx->cpPerfMode = 0;

	__GX_InitRevBits();

	i=0;
	while(i<16) {
		__gx->tevTexMap[i] = 0xff;
		i++;
	}

#if defined(HW_RVL)
	i = 0;
	while(i<8) {
		region = &__gx->texRegion[i];
		GX_InitTexCacheRegion(region,GX_FALSE,_gxtexregionaddrtable[i+0],GX_TEXCACHE_32K,_gxtexregionaddrtable[i+8],GX_TEXCACHE_32K);

		region = &__gx->texRegion[i+8];
		GX_InitTexCacheRegion(region,GX_FALSE,_gxtexregionaddrtable[i+16],GX_TEXCACHE_32K,_gxtexregionaddrtable[i+24],GX_TEXCACHE_32K);

		region = &__gx->texRegion[i+16];
		GX_InitTexCacheRegion(region,GX_TRUE,_gxtexregionaddrtable[i+32],GX_TEXCACHE_32K,_gxtexregionaddrtable[i+40],GX_TEXCACHE_32K);

		i++;
	}

	i=0;
	while(i<16) {
		tmem = 0x000C0000+(i<<13);
		tregion = &__gx->tlutRegion[i];
		GX_InitTlutRegion(tregion,tmem,GX_TLUT_256);
		i++;
	}

	i=0;
	while(i<4) {
		tmem = 0x000E0000+(i<<15);
		tregion = &__gx->tlutRegion[i+16];
		GX_InitTlutRegion(tregion,tmem,GX_TLUT_1K);
		i++;
	}
#else
	for(i=0;i<8;i++) {
		tmem_even = tmem_odd = (i<<15);
		region = &__gx->texRegion[i];
		GX_InitTexCacheRegion(region,GX_FALSE,tmem_even,GX_TEXCACHE_32K,(tmem_odd+0x00080000),GX_TEXCACHE_32K);
	}
	for(i=0;i<4;i++) {
		tmem_even = ((0x08+(i<<1))<<15);
		tmem_odd = ((0x09+(i<<1))<<15);
		region = &__gx->texRegion[i+8];
		GX_InitTexCacheRegion(region,GX_FALSE,tmem_even,GX_TEXCACHE_32K,tmem_odd,GX_TEXCACHE_32K);
	}
	for(i=0;i<16;i++) {
		tmem_even = (i<<13)+0x000C0000;
		tregion = &__gx->tlutRegion[i];
		GX_InitTlutRegion(tregion,tmem_even,GX_TLUT_256);
	}
	for(i=0;i<4;i++) {
		tmem_even = (i<<15)+0x000E0000;
		tregion = &__gx->tlutRegion[i+16];
		GX_InitTlutRegion(tregion,tmem_even,GX_TLUT_1K);
	}
#endif
	_cpReg[3] = 0;
	GX_LOAD_CP_REG(0x20,0x00000000);
	GX_LOAD_XF_REG(0x1006,0x0);

	GX_LOAD_BP_REG(0x23000000);
	GX_LOAD_BP_REG(0x24000000);
	GX_LOAD_BP_REG(0x67000000);

	__GX_SetIndirectMask(0);
#if defined(HW_RVL)
	__GX_SetTmemConfig(2);
#else
	__GX_SetTmemConfig(0);
#endif
	__GX_InitGX();

	return &_gxfifoobj;
}

void GX_InitFifoBase(GXFifoObj *fifo,void *base,u32 size)
{
	struct __gxfifo *ptr = (struct __gxfifo*)fifo;

	if(!ptr || size<GX_FIFO_MINSIZE) return;

	ptr->buf_start = (u32)base;
	ptr->buf_end = (u32)base + size - 4;
	ptr->size = size;
	ptr->rdwt_dst = 0;

	GX_InitFifoLimits(fifo,(size-GX_FIFO_HIWATERMARK),((size>>1)&0x7fffffe0));
	GX_InitFifoPtrs(fifo,base,base);
}

void GX_InitFifoLimits(GXFifoObj *fifo,u32 hiwatermark,u32 lowatermark)
{
	struct __gxfifo *ptr = (struct __gxfifo*)fifo;

	ptr->hi_mark = hiwatermark;
	ptr->lo_mark = lowatermark;
}

void GX_InitFifoPtrs(GXFifoObj *fifo,void *rd_ptr,void *wt_ptr)
{
	u32 level;
	s32 rdwt_dst;
	struct __gxfifo *ptr = (struct __gxfifo*)fifo;

	_CPU_ISR_Disable(level);
	rdwt_dst =  wt_ptr-rd_ptr;
	ptr->rd_ptr = (u32)rd_ptr;
	ptr->wt_ptr = (u32)wt_ptr;
	ptr->rdwt_dst = rdwt_dst;
	if(rdwt_dst<0) {
		rdwt_dst += ptr->size;
		ptr->rd_ptr = rdwt_dst;
	}
	_CPU_ISR_Restore(level);
}

void GX_GetFifoPtrs(GXFifoObj *fifo,void **rd_ptr,void **wt_ptr)
{
	struct __gxfifo *ptr = (struct __gxfifo*)fifo;
	*rd_ptr = (void*)ptr->rd_ptr;
	*wt_ptr = (void*)ptr->wt_ptr;
}

void GX_SetCPUFifo(GXFifoObj *fifo)
{
	u32 level;
	struct __gxfifo *ptr = (struct __gxfifo*)fifo;
	struct __gxfifo *cpufifo = (struct __gxfifo*)&_cpufifo;

	_CPU_ISR_Disable(level);
	if(!fifo) {
		_gxcpufifoready = 0;
		_cpgplinked = 0;
		cpufifo->gpfifo_ready = 0;
		cpufifo->cpufifo_ready = 0;
		_CPU_ISR_Restore(level);
		return;
	}

	cpufifo->buf_start = ptr->buf_start;
	cpufifo->buf_end = ptr->buf_end;
	cpufifo->size = ptr->size;
	cpufifo->hi_mark = ptr->hi_mark;
	cpufifo->lo_mark = ptr->lo_mark;
	cpufifo->rd_ptr = ptr->rd_ptr;
	cpufifo->wt_ptr = ptr->wt_ptr;
	cpufifo->rdwt_dst = ptr->rdwt_dst;
	cpufifo->fifo_wrap = ptr->fifo_wrap;
	cpufifo->gpfifo_ready = ptr->gpfifo_ready;
	cpufifo->cpufifo_ready = 1;

	_gxcpufifoready = 1;
	if(__GX_CPGPLinkCheck()) {
		_cpgplinked = 1;
		cpufifo->gpfifo_ready = 1;

		_piReg[3] = MEM_VIRTUAL_TO_PHYSICAL(cpufifo->buf_start);
		_piReg[4] = MEM_VIRTUAL_TO_PHYSICAL(cpufifo->buf_end);
		_piReg[5] = (cpufifo->wt_ptr&0x1FFFFFE0);

		__GX_WriteFifoIntReset(GX_TRUE,GX_TRUE);
		__GX_WriteFifoIntEnable(GX_ENABLE,GX_DISABLE);
		__GX_FifoLink(GX_TRUE);

		_CPU_ISR_Restore(level);
		return;
	}

	if(_cpgplinked) {
		__GX_FifoLink(GX_FALSE);
		_cpgplinked = 0;
	}

	__GX_WriteFifoIntEnable(GX_DISABLE,GX_DISABLE);

	_piReg[3] = MEM_VIRTUAL_TO_PHYSICAL(cpufifo->buf_start);
	_piReg[4] = MEM_VIRTUAL_TO_PHYSICAL(cpufifo->buf_end);
	_piReg[5] = (cpufifo->wt_ptr&0x1FFFFFE0);
	ppcsync();

	_CPU_ISR_Restore(level);
}

void GX_GetCPUFifo(GXFifoObj *fifo)
{
	struct __gxfifo* ptr = (struct __gxfifo*)fifo;
	struct __gxfifo* cpufifo = (struct __gxfifo*)&_cpufifo;

	if(!_gxcpufifoready) return;

	GX_Flush();
	__GX_SaveFifo();

	ptr->buf_start = cpufifo->buf_start;
	ptr->buf_end = cpufifo->buf_end;
	ptr->size = cpufifo->size;
	ptr->rd_ptr = cpufifo->rd_ptr;
	ptr->wt_ptr = cpufifo->wt_ptr;
	ptr->rdwt_dst = cpufifo->rdwt_dst;
	ptr->hi_mark = cpufifo->hi_mark;
	ptr->lo_mark = cpufifo->lo_mark;
	ptr->fifo_wrap = cpufifo->fifo_wrap;
	ptr->cpufifo_ready = cpufifo->cpufifo_ready;
	ptr->gpfifo_ready = cpufifo->gpfifo_ready;
}

void GX_SetGPFifo(GXFifoObj *fifo)
{
	u32 level;
	struct __gxfifo *ptr = (struct __gxfifo*)fifo;
	struct __gxfifo *gpfifo = (struct __gxfifo*)&_gpfifo;

	_CPU_ISR_Disable(level);
	__GX_FifoReadDisable();
	__GX_WriteFifoIntEnable(GX_DISABLE,GX_DISABLE);

	if(!fifo) {
		_gxgpfifoready = 0;
		_cpgplinked = 0;
		gpfifo->cpufifo_ready = 0;
		gpfifo->gpfifo_ready = 0;
		__GX_FifoLink(GX_FALSE);
		_CPU_ISR_Restore(level);
		return;
	}

	gpfifo->buf_start = ptr->buf_start;
	gpfifo->buf_end = ptr->buf_end;
	gpfifo->size = ptr->size;
	gpfifo->hi_mark = ptr->hi_mark;
	gpfifo->lo_mark = ptr->lo_mark;
	gpfifo->rd_ptr = ptr->rd_ptr;
	gpfifo->wt_ptr = ptr->wt_ptr;
	gpfifo->rdwt_dst = ptr->rdwt_dst;
	gpfifo->fifo_wrap = ptr->fifo_wrap;
	gpfifo->cpufifo_ready = ptr->cpufifo_ready;
	gpfifo->gpfifo_ready = 1;
	_gxgpfifoready = 1;

	/* setup fifo base */
	_cpReg[16] = _SHIFTL(MEM_VIRTUAL_TO_PHYSICAL(gpfifo->buf_start),0,16);
	_cpReg[17] = _SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(gpfifo->buf_start),16,16);

	/* setup fifo end */
	_cpReg[18] = _SHIFTL(MEM_VIRTUAL_TO_PHYSICAL(gpfifo->buf_end),0,16);
	_cpReg[19] = _SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(gpfifo->buf_end),16,16);

	/* setup hiwater mark */
	_cpReg[20] = _SHIFTL(gpfifo->hi_mark,0,16);
	_cpReg[21] = _SHIFTR(gpfifo->hi_mark,16,16);

	/* setup lowater mark */
	_cpReg[22] = _SHIFTL(gpfifo->lo_mark,0,16);
	_cpReg[23] = _SHIFTR(gpfifo->lo_mark,16,16);

	/* setup rd<->wd dist */
	_cpReg[24] = _SHIFTL(gpfifo->rdwt_dst,0,16);
	_cpReg[25] = _SHIFTR(gpfifo->rdwt_dst,16,16);

	/* setup wt ptr */
	_cpReg[26] = _SHIFTL(MEM_VIRTUAL_TO_PHYSICAL(gpfifo->wt_ptr),0,16);
	_cpReg[27] = _SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(gpfifo->wt_ptr),16,16);

	/* setup rd ptr */
	_cpReg[28] = _SHIFTL(MEM_VIRTUAL_TO_PHYSICAL(gpfifo->rd_ptr),0,16);
	_cpReg[29] = _SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(gpfifo->rd_ptr),16,16);
	ppcsync();

	if(__GX_CPGPLinkCheck()) {
		_cpgplinked = 1;
		gpfifo->cpufifo_ready = 1;
		__GX_WriteFifoIntEnable(GX_ENABLE,GX_DISABLE);
		__GX_FifoLink(GX_TRUE);
	} else {
		_cpgplinked = 0;
		gpfifo->cpufifo_ready = 0;
		__GX_WriteFifoIntEnable(GX_DISABLE,GX_DISABLE);
		__GX_FifoLink(GX_FALSE);
	}

	__GX_WriteFifoIntReset(GX_TRUE,GX_TRUE);
	__GX_FifoReadEnable();
	_CPU_ISR_Restore(level);
}

void GX_GetGPFifo(GXFifoObj *fifo)
{
	struct __gxfifo* ptr = (struct __gxfifo*)fifo;
	struct __gxfifo* gpfifo = (struct __gxfifo*)&_gpfifo;

	if(!_gxgpfifoready) return;

	__GX_SaveFifo();

	ptr->buf_start = gpfifo->buf_start;
	ptr->buf_end = gpfifo->buf_end;
	ptr->size = gpfifo->size;
	ptr->rd_ptr = gpfifo->rd_ptr;
	ptr->wt_ptr = gpfifo->wt_ptr;
	ptr->rdwt_dst = gpfifo->rdwt_dst;
	ptr->hi_mark = gpfifo->hi_mark;
	ptr->lo_mark = gpfifo->lo_mark;
	ptr->fifo_wrap = gpfifo->fifo_wrap;
	ptr->gpfifo_ready = gpfifo->gpfifo_ready;
	ptr->cpufifo_ready = gpfifo->cpufifo_ready;
}

void* GX_GetFifoBase(GXFifoObj *fifo)
{
	return (void*)((struct __gxfifo*)fifo)->buf_start;
}

u32 GX_GetFifoSize(GXFifoObj *fifo)
{
	return ((struct __gxfifo*)fifo)->size;
}

u32 GX_GetFifoCount(GXFifoObj *fifo)
{
	return ((struct __gxfifo*)fifo)->rdwt_dst;
}

u8 GX_GetFifoWrap(GXFifoObj *fifo)
{
	return ((struct __gxfifo*)fifo)->fifo_wrap;
}

u32 GX_GetOverflowCount()
{
	return _gxoverflowcount;
}

u32 GX_ResetOverflowCount()
{
	u32 ret = _gxoverflowcount;
	_gxoverflowcount = 0;
	return ret;
}

lwp_t GX_GetCurrentGXThread()
{
	return _gxcurrentlwp;
}

lwp_t GX_SetCurrentGXThread()
{
	u32 level;

	_CPU_ISR_Disable(level);
	lwp_t ret = _gxcurrentlwp;
	_gxcurrentlwp = LWP_GetSelf();
	_CPU_ISR_Restore(level);

	return ret;
}

volatile void* GX_RedirectWriteGatherPipe(void *ptr)
{
	u32 level;
	struct __gxfifo *cpufifo = (struct __gxfifo*)&_cpufifo;

	_CPU_ISR_Disable(level);
	GX_Flush();
	while(!IsWriteGatherBufferEmpty());

	mtwpar(0x0C008000);
	if(_cpgplinked) {
		__GX_FifoLink(GX_FALSE);
		__GX_WriteFifoIntEnable(GX_DISABLE,GX_DISABLE);
	}
	cpufifo->wt_ptr = (u32)MEM_PHYSICAL_TO_K0(_piReg[5]&~0x04000000);

	_piReg[3] = 0;
	_piReg[4] = 0x04000000;
	_piReg[5] = (((u32)ptr&0x3FFFFFE0)&~0x04000000);
	_sync();

	_CPU_ISR_Restore(level);

	return (volatile void*)0x0C008000;
}

void GX_RestoreWriteGatherPipe()
{
	u32 level;
	struct __gxfifo *cpufifo = (struct __gxfifo*)&_cpufifo;

	_CPU_ISR_Disable(level);

	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;

	ppcsync();
	while(!IsWriteGatherBufferEmpty());

	mtwpar(0x0C008000);
	_piReg[3] = MEM_VIRTUAL_TO_PHYSICAL(cpufifo->buf_start);
	_piReg[4] = MEM_VIRTUAL_TO_PHYSICAL(cpufifo->buf_end);
	_piReg[5] = (((u32)cpufifo->wt_ptr&0x3FFFFFE0)&~0x04000000);
	if(_cpgplinked) {
		__GX_WriteFifoIntReset(GX_TRUE,GX_TRUE);
		__GX_WriteFifoIntEnable(GX_ENABLE,GX_DISABLE);
		__GX_FifoLink(GX_TRUE);
	}
	_CPU_ISR_Restore(level);
}

void GX_Flush()
{
	if(__gx->dirtyState)
		__GX_SetDirtyState();

	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;

	ppcsync();
}

void GX_EnableBreakPt(void *break_pt)
{
	u32 level = 0;
	_CPU_ISR_Disable(level);
	__GX_FifoReadDisable();
	_cpReg[30] = _SHIFTL(MEM_VIRTUAL_TO_PHYSICAL(break_pt),0,16);
	_cpReg[31] = _SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(break_pt),16,16);
	__gx->cpCRreg = (__gx->cpCRreg&~0x22)|0x22;
	_cpReg[1] = __gx->cpCRreg;
	_gxcurrbp = break_pt;
	__GX_FifoReadEnable();
 	_CPU_ISR_Restore(level);
}

void GX_DisableBreakPt()
{
	u32 level = 0;
	_CPU_ISR_Disable(level);
	__gx->cpCRreg = (__gx->cpCRreg&~0x22);
	_cpReg[1] = __gx->cpCRreg;
	_gxcurrbp = NULL;
	_CPU_ISR_Restore(level);
}

#if defined(HW_DOL)
void GX_AbortFrame()
{
	_piReg[6] = 1;
	__GX_WaitAbort(50);
	_piReg[6] = 0;
	__GX_WaitAbort(5);

	if(__GX_IsGPFifoReady())
		__GX_CleanGPFifo();
}
#elif defined(HW_RVL)
void GX_AbortFrame()
{
	__GX_Abort();
	if(__GX_IsGPFifoReady()) {
		__GX_CleanGPFifo();
		__GX_InitRevBits();

		__gx->dirtyState = 0;
		GX_Flush();
	}
}
#endif

void GX_SetDrawSync(u16 token)
{
	u32 level = 0;
	_CPU_ISR_Disable(level);
	GX_LOAD_BP_REG(0x48000000 | token);
	GX_LOAD_BP_REG(0x47000000 | token);
	GX_Flush();
	_CPU_ISR_Restore(level);
}

u16 GX_GetDrawSync()
{
	return _peReg[7];
}

void GX_SetDrawDone()
{
	u32 level;
	_CPU_ISR_Disable(level);
	GX_LOAD_BP_REG(0x45000002); // set draw done!
	GX_Flush();
	_gxfinished = 0;
	_CPU_ISR_Restore(level);
}

void GX_WaitDrawDone()
{
	u32 level;
	_CPU_ISR_Disable(level);
	while(!_gxfinished)
		LWP_ThreadSleep(_gxwaitfinish);
	_CPU_ISR_Restore(level);
}

void GX_DrawDone()
{
	u32 level;

	_CPU_ISR_Disable(level);
	GX_LOAD_BP_REG(0x45000002); // set draw done!
	GX_Flush();

	_gxfinished = 0;
	_CPU_ISR_Flash(level);

	while(!_gxfinished)
		LWP_ThreadSleep(_gxwaitfinish);
	_CPU_ISR_Restore(level);
}

GXDrawDoneCallback GX_SetDrawDoneCallback(GXDrawDoneCallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	GXDrawDoneCallback ret = drawDoneCB;
	drawDoneCB = cb;
	_CPU_ISR_Restore(level);
	return ret;
}

GXDrawSyncCallback GX_SetDrawSyncCallback(GXDrawSyncCallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	GXDrawSyncCallback ret = tokenCB;
	tokenCB = cb;
	_CPU_ISR_Restore(level);
	return ret;
}

GXBreakPtCallback GX_SetBreakPtCallback(GXBreakPtCallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	GXBreakPtCallback ret = breakPtCB;
	breakPtCB = cb;
	_CPU_ISR_Restore(level);
	return ret;
}

void GX_PixModeSync()
{
	GX_LOAD_BP_REG(__gx->peCntrl);
}

void GX_TexModeSync()
{
	GX_LOAD_BP_REG(0x63000000);
}

void GX_SetMisc(u32 token,u32 value)
{
	u32 cnt;

	if(token==GX_MT_XF_FLUSH) {
		__gx->xfFlushSafe = value;
		cnt = cntlzw(__gx->xfFlushSafe);
		__gx->xfFlushExp = _SHIFTR(cnt,5,16);

		__gx->xfFlush = 1;
		if(!__gx->xfFlushSafe) return;

		__gx->dirtyState |= 0x0008;
	} else if(token==GX_MT_DL_SAVE_CTX) {
		__gx->saveDLctx = (value&0xff);
	}
	return;
}

void GX_SetViewportJitter(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ,u32 field)
{
	f32 x0,y0,x1,y1,n,f,z;
	static f32 Xfactor = 0.5;
	static f32 Yfactor = 342.0;
	static f32 Zfactor = 16777215.0;

	if(!field) yOrig -= Xfactor;

	x0 = wd*Xfactor;
	y0 = (-ht)*Xfactor;
	x1 = (xOrig+(wd*Xfactor))+Yfactor;
	y1 = (yOrig+(ht*Xfactor))+Yfactor;
	n = Zfactor*nearZ;
	f = Zfactor*farZ;
	z = f-n;

	GX_LOAD_XF_REGS(0x101a,6);
	wgPipe->F32 = x0;
	wgPipe->F32 = y0;
	wgPipe->F32 = z;
	wgPipe->F32 = x1;
	wgPipe->F32 = y1;
	wgPipe->F32 = f;
}

void GX_SetViewport(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ)
{
	GX_SetViewportJitter(xOrig,yOrig,wd,ht,nearZ,farZ,1);
}

void GX_LoadProjectionMtx(Mtx44 mt,u8 type)
{
	f32 tmp[7];

	((u32*)((void*)tmp))[6] = (u32)type;
	tmp[0] = mt[0][0];
	tmp[2] = mt[1][1];
	tmp[4] = mt[2][2];
	tmp[5] = mt[2][3];

	switch(type) {
		case GX_PERSPECTIVE:
			tmp[1] = mt[0][2];
			tmp[3] = mt[1][2];
			break;
		case GX_ORTHOGRAPHIC:
			tmp[1] = mt[0][3];
			tmp[3] = mt[1][3];
			break;
		default:
			tmp[1] = 0.0;
			tmp[3] = 0.0;
			break;
	}

	GX_LOAD_XF_REGS(0x1020,7);
	wgPipe->F32 = tmp[0];
	wgPipe->F32 = tmp[1];
	wgPipe->F32 = tmp[2];
	wgPipe->F32 = tmp[3];
	wgPipe->F32 = tmp[4];
	wgPipe->F32 = tmp[5];
	wgPipe->F32 = tmp[6];
}

static void __GetImageTileCount(u32 fmt,u16 wd,u16 ht,u32 *xtiles,u32 *ytiles,u32 *zplanes)
{
	u32 xshift,yshift,tile;

	switch(fmt) {
		case GX_TF_I4:
		case GX_TF_IA4:
		case GX_CTF_R4:
		case GX_CTF_RA4:
		case GX_CTF_Z4:
			xshift = 3;
			yshift = 3;
			break;
		case GX_TF_Z8:
		case GX_TF_I8:
		case GX_CTF_A8:
		case GX_CTF_R8:
		case GX_CTF_G8:
		case GX_CTF_B8:
		case GX_CTF_Z8M:
		case GX_CTF_Z8L:
			xshift = 3;
			yshift = 2;
			break;
		case GX_TF_IA8:
		case GX_CTF_RA8:
		case GX_CTF_RG8:
		case GX_CTF_GB8:
		case GX_TF_Z16:
		case GX_TF_Z24X8:
		case GX_CTF_Z16L:
		case GX_TF_RGB565:
		case GX_TF_RGB5A3:
		case GX_TF_RGBA8:
			xshift = 2;
			yshift = 2;
			break;
		default:
			xshift = 0;
			yshift = 0;
			break;
	}

	if(!(wd&0xffff)) wd = 1;
	if(!(ht&0xffff)) ht = 1;

	wd &= 0xffff;
	tile = (wd+((1<<xshift)-1))>>xshift;
	*xtiles = tile;

	ht &= 0xffff;
	tile = (ht+((1<<yshift)-1))>>yshift;
	*ytiles = tile;

	*zplanes = 1;
	if(fmt==GX_TF_RGBA8 || fmt==GX_TF_Z24X8) *zplanes = 2;
}

void GX_SetCopyClear(GXColor color,u32 zvalue)
{
	u32 val;

	val = (_SHIFTL(color.a,8,8))|(color.r&0xff);
	GX_LOAD_BP_REG(0x4f000000|val);

	val = (_SHIFTL(color.g,8,8))|(color.b&0xff);
	GX_LOAD_BP_REG(0x50000000|val);

	val = zvalue&0x00ffffff;
	GX_LOAD_BP_REG(0x51000000|val);
}

void GX_SetCopyClamp(u8 clamp)
{
	__gx->dispCopyCntrl = (__gx->dispCopyCntrl&~1)|(clamp&1);
	__gx->dispCopyCntrl = (__gx->dispCopyCntrl&~2)|(clamp&2);
}

void GX_SetDispCopyGamma(u8 gamma)
{
	__gx->dispCopyCntrl = (__gx->dispCopyCntrl&~0x180)|(_SHIFTL(gamma,7,2));
}

void GX_SetCopyFilter(u8 aa,u8 sample_pattern[12][2],u8 vf,u8 vfilter[7])
{
	u32 reg01=0,reg02=0,reg03=0,reg04=0,reg53=0,reg54=0;

	if(aa) {
		reg01 = sample_pattern[0][0]&0xf;
		reg01 = (reg01&~0xf0)|(_SHIFTL(sample_pattern[0][1],4,4));
		reg01 = (reg01&~0xf00)|(_SHIFTL(sample_pattern[1][0],8,4));
		reg01 = (reg01&~0xf000)|(_SHIFTL(sample_pattern[1][1],12,4));
		reg01 = (reg01&~0xf0000)|(_SHIFTL(sample_pattern[2][0],16,4));
		reg01 = (reg01&~0xf00000)|(_SHIFTL(sample_pattern[2][1],20,4));
		reg01 = (reg01&~0xff000000)|(_SHIFTL(0x01,24,8));

		reg02 = sample_pattern[3][0]&0xf;
		reg02 = (reg02&~0xf0)|(_SHIFTL(sample_pattern[3][1],4,4));
		reg02 = (reg02&~0xf00)|(_SHIFTL(sample_pattern[4][0],8,4));
		reg02 = (reg02&~0xf000)|(_SHIFTL(sample_pattern[4][1],12,4));
		reg02 = (reg02&~0xf0000)|(_SHIFTL(sample_pattern[5][0],16,4));
		reg02 = (reg02&~0xf00000)|(_SHIFTL(sample_pattern[5][1],20,4));
		reg02 = (reg02&~0xff000000)|(_SHIFTL(0x02,24,8));

		reg03 = sample_pattern[6][0]&0xf;
		reg03 = (reg03&~0xf0)|(_SHIFTL(sample_pattern[6][1],4,4));
		reg03 = (reg03&~0xf00)|(_SHIFTL(sample_pattern[7][0],8,4));
		reg03 = (reg03&~0xf000)|(_SHIFTL(sample_pattern[7][1],12,4));
		reg03 = (reg03&~0xf0000)|(_SHIFTL(sample_pattern[8][0],16,4));
		reg03 = (reg03&~0xf00000)|(_SHIFTL(sample_pattern[8][1],20,4));
		reg03 = (reg03&~0xff000000)|(_SHIFTL(0x03,24,8));

		reg04 = sample_pattern[9][0]&0xf;
		reg04 = (reg04&~0xf0)|(_SHIFTL(sample_pattern[9][1],4,4));
		reg04 = (reg04&~0xf00)|(_SHIFTL(sample_pattern[10][0],8,4));
		reg04 = (reg04&~0xf000)|(_SHIFTL(sample_pattern[10][1],12,4));
		reg04 = (reg04&~0xf0000)|(_SHIFTL(sample_pattern[11][0],16,4));
		reg04 = (reg04&~0xf00000)|(_SHIFTL(sample_pattern[11][1],20,4));
		reg04 = (reg04&~0xff000000)|(_SHIFTL(0x04,24,8));
	} else {
		reg01 = 0x01666666;
		reg02 = 0x02666666;
		reg03 = 0x03666666;
		reg04 = 0x04666666;
	}
	GX_LOAD_BP_REG(reg01);
	GX_LOAD_BP_REG(reg02);
	GX_LOAD_BP_REG(reg03);
	GX_LOAD_BP_REG(reg04);

	reg53 = 0x53595000;
	reg54 = 0x54000015;
	if(vf) {
		reg53 = 0x53000000|(vfilter[0]&0x3f);
		reg53 = (reg53&~0xfc0)|(_SHIFTL(vfilter[1],6,6));
		reg53 = (reg53&~0x3f000)|(_SHIFTL(vfilter[2],12,6));
		reg53 = (reg53&~0xfc0000)|(_SHIFTL(vfilter[3],18,6));

		reg54 = 0x54000000|(vfilter[4]&0x3f);
		reg54 = (reg54&~0xfc0)|(_SHIFTL(vfilter[5],6,6));
		reg54 = (reg54&~0x3f000)|(_SHIFTL(vfilter[6],12,6));
	}
	GX_LOAD_BP_REG(reg53);
	GX_LOAD_BP_REG(reg54);
}

void GX_SetDispCopyFrame2Field(u8 mode)
{
	__gx->dispCopyCntrl = (__gx->dispCopyCntrl&~0x3000)|(_SHIFTL(mode,12,2));
}

u32 GX_SetDispCopyYScale(f32 yscale)
{
	u32 ht,yScale = 0;

	yScale = ((u32)(256.0f/yscale))&0x1ff;
	GX_LOAD_BP_REG(0x4e000000|yScale);

	__gx->dispCopyCntrl = (__gx->dispCopyCntrl&~0x400)|(_SHIFTL(((256-yScale)>0),10,1));
	ht = _SHIFTR(__gx->dispCopyWH,12,10)+1;
	return __GX_GetNumXfbLines(ht,yScale);
}

void GX_SetDispCopyDst(u16 wd,u16 ht)
{
	__gx->dispCopyDst = (__gx->dispCopyDst&~0x3ff)|(_SHIFTR(wd,4,10));
	__gx->dispCopyDst = (__gx->dispCopyDst&~0xff000000)|(_SHIFTL(0x4d,24,8));
}

void GX_SetDispCopySrc(u16 left,u16 top,u16 wd,u16 ht)
{
	__gx->dispCopyTL = (__gx->dispCopyTL&~0x00ffffff)|XY(left,top);
	__gx->dispCopyTL = (__gx->dispCopyTL&~0xff000000)|(_SHIFTL(0x49,24,8));
	__gx->dispCopyWH = (__gx->dispCopyWH&~0x00ffffff)|XY((wd-1),(ht-1));
	__gx->dispCopyWH = (__gx->dispCopyWH&~0xff000000)|(_SHIFTL(0x4a,24,8));
}

void GX_CopyDisp(void *dest,u8 clear)
{
	u8 clflag;
	u32 val;

	if(clear) {
		val= (__gx->peZMode&~0xf)|0xf;
		GX_LOAD_BP_REG(val);
		val = (__gx->peCMode0&~0x3);
		GX_LOAD_BP_REG(val);
	}

	clflag = 0;
	if(clear || (__gx->peCntrl&0x7)==0x0003) {
		if(__gx->peCntrl&0x40) {
			clflag = 1;
			val = (__gx->peCntrl&~0x40);
			GX_LOAD_BP_REG(val);
		}
	}

	GX_LOAD_BP_REG(__gx->dispCopyTL);  // set source top
	GX_LOAD_BP_REG(__gx->dispCopyWH);

	GX_LOAD_BP_REG(__gx->dispCopyDst);

	val = 0x4b000000|(_SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(dest),5,24));
	GX_LOAD_BP_REG(val);

	__gx->dispCopyCntrl = (__gx->dispCopyCntrl&~0x800)|(_SHIFTL(clear,11,1));
	__gx->dispCopyCntrl = (__gx->dispCopyCntrl&~0x4000)|0x4000;
	__gx->dispCopyCntrl = (__gx->dispCopyCntrl&~0xff000000)|(_SHIFTL(0x52,24,8));

	GX_LOAD_BP_REG(__gx->dispCopyCntrl);

	if(clear) {
		GX_LOAD_BP_REG(__gx->peZMode);
		GX_LOAD_BP_REG(__gx->peCMode0);
	}
	if(clflag) GX_LOAD_BP_REG(__gx->peCntrl);
}

void GX_CopyTex(void *dest,u8 clear)
{
	u8 clflag;
	u32 val;

	if(clear) {
		val = (__gx->peZMode&~0xf)|0xf;
		GX_LOAD_BP_REG(val);
		val = (__gx->peCMode0&~0x3);
		GX_LOAD_BP_REG(val);
	}

	clflag = 0;
	val = __gx->peCntrl;
	if(__gx->texCopyZTex && (val&0x7)!=0x0003) {
		clflag = 1;
		val = (val&~0x7)|0x0003;
	}
	if(clear || (val&0x7)==0x0003) {
		if(val&0x40) {
			clflag = 1;
			val = (val&~0x40);
		}
	}
	if(clflag) GX_LOAD_BP_REG(val);

	val = 0x4b000000|(_SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(dest),5,24));

	GX_LOAD_BP_REG(__gx->texCopyTL);
	GX_LOAD_BP_REG(__gx->texCopyWH);
	GX_LOAD_BP_REG(__gx->texCopyDst);
	GX_LOAD_BP_REG(val);

	__gx->texCopyCntrl = (__gx->texCopyCntrl&~0x800)|(_SHIFTL(clear,11,1));
	__gx->texCopyCntrl = (__gx->texCopyCntrl&~0x4000);
	__gx->texCopyCntrl = (__gx->texCopyCntrl&~0xff000000)|(_SHIFTL(0x52,24,8));
	GX_LOAD_BP_REG(__gx->texCopyCntrl);

	if(clear) {
		GX_LOAD_BP_REG(__gx->peZMode);
		GX_LOAD_BP_REG(__gx->peCMode0);
	}
	if(clflag) GX_LOAD_BP_REG(__gx->peCntrl);
}

void GX_SetTexCopySrc(u16 left,u16 top,u16 wd,u16 ht)
{
	__gx->texCopyTL = (__gx->texCopyTL&~0x00ffffff)|XY(left,top);
	__gx->texCopyTL = (__gx->texCopyTL&~0xff000000)|(_SHIFTL(0x49,24,8));
	__gx->texCopyWH = (__gx->texCopyWH&~0x00ffffff)|XY((wd-1),(ht-1));
	__gx->texCopyWH = (__gx->texCopyWH&~0xff000000)|(_SHIFTL(0x4a,24,8));
}

void GX_SetTexCopyDst(u16 wd,u16 ht,u32 fmt,u8 mipmap)
{
	u8 lfmt = fmt&0xf;
	u32 xtiles,ytiles,zplanes;

	__GetImageTileCount(fmt,wd,ht,&xtiles,&ytiles,&zplanes);
	__gx->texCopyDst = (__gx->texCopyDst&~0x3ff)|((xtiles*zplanes)&0x3ff);

	if(fmt==GX_TF_Z16) lfmt = 11;
	if(fmt==GX_CTF_YUVA8 || (fmt>=GX_TF_I4 && fmt<GX_TF_RGB565)) __gx->texCopyCntrl = (__gx->texCopyCntrl&~0x18000)|0x18000;
	else __gx->texCopyCntrl = (__gx->texCopyCntrl&~0x18000)|0x10000;

	__gx->texCopyCntrl = (__gx->texCopyCntrl&~0x8)|(lfmt&0x8);
	__gx->texCopyCntrl = (__gx->texCopyCntrl&~0x200)|(_SHIFTL(mipmap,9,1));
	__gx->texCopyCntrl = (__gx->texCopyCntrl&~0x70)|(_SHIFTL(lfmt,4,3));

	__gx->texCopyDst = (__gx->texCopyDst&~0xff000000)|(_SHIFTL(0x4d,24,8));

	__gx->texCopyZTex = ((fmt&_GX_TF_ZTF)==_GX_TF_ZTF);
}

void GX_ClearBoundingBox()
{
	GX_LOAD_BP_REG(0x550003ff);
	GX_LOAD_BP_REG(0x560003ff);
}

void GX_BeginDispList(void *list,u32 size)
{
	struct __gxfifo *fifo;

	if(__gx->dirtyState)
		__GX_SetDirtyState();

	if(__gx->saveDLctx)
		memcpy(_gx_saved_data,__gxregs,STRUCT_REGDEF_SIZE);

	fifo = (struct __gxfifo*)&_gx_dl_fifoobj;
	fifo->buf_start = (u32)list;
	fifo->buf_end = (u32)list + size - 4;
	fifo->size = size;

	fifo->rd_ptr = (u32)list;
	fifo->wt_ptr = (u32)list;
	fifo->rdwt_dst = 0;

	__gx->gxFifoUnlinked = 1;

	GX_GetCPUFifo(&_gx_old_cpufifo);
	GX_SetCPUFifo(&_gx_dl_fifoobj);
	__GX_ResetWriteGatherPipe();
}

u32 GX_EndDispList()
{
	u32 level;
	u8 wrap = 0;

	GX_GetCPUFifo(&_gx_dl_fifoobj);
	GX_SetCPUFifo(&_gx_old_cpufifo);

	if(__gx->saveDLctx) {
		_CPU_ISR_Disable(level);
		memcpy(__gxregs,_gx_saved_data,STRUCT_REGDEF_SIZE);
		_CPU_ISR_Restore(level);
	}

	__gx->gxFifoUnlinked = 0;

	wrap = GX_GetFifoWrap(&_gx_dl_fifoobj);
	if(wrap) return 0;

	return GX_GetFifoCount(&_gx_dl_fifoobj);
}

void GX_CallDispList(void *list,u32 nbytes)
{
	if(__gx->dirtyState)
		__GX_SetDirtyState();

	if(!__gx->vcdClear)
		__GX_SendFlushPrim();

	wgPipe->U8 = 0x40;		//call displaylist
	wgPipe->U32 = MEM_VIRTUAL_TO_PHYSICAL(list);
	wgPipe->U32 = nbytes;
}

void GX_SetChanCtrl(s32 channel,u8 enable,u8 ambsrc,u8 matsrc,u8 litmask,u8 diff_fn,u8 attn_fn)
{
	u32 reg,difffn = (attn_fn==GX_AF_SPEC)?GX_DF_NONE:diff_fn;
	u32 val = (matsrc&1)|(_SHIFTL(enable,1,1))|(_SHIFTL(litmask,2,4))|(_SHIFTL(ambsrc,6,1))|(_SHIFTL(difffn,7,2))|(_SHIFTL(((GX_AF_NONE-attn_fn)>0),9,1))|(_SHIFTL((attn_fn>0),10,1))|(_SHIFTL((_SHIFTR(litmask,4,4)),11,4));

	reg = (channel&0x03);
	__gx->chnCntrl[reg] = val;
	__gx->dirtyState |= (0x1000<<reg);

	if(channel==GX_COLOR0A0) {
		__gx->chnCntrl[2] = val;
		__gx->dirtyState |= 0x5000;
	} else {
		__gx->chnCntrl[3] = val;
		__gx->dirtyState |= 0xa000;
	}
}

void GX_SetChanAmbColor(s32 channel,GXColor color)
{
	u32 reg,val = (_SHIFTL(color.r,24,8))|(_SHIFTL(color.g,16,8))|(_SHIFTL(color.b,8,8))|0x00;
	switch(channel) {
		case GX_COLOR0:
			reg = 0;
			val |= (__gx->chnAmbColor[0]&0xff);
			break;
		case GX_COLOR1:
			reg = 1;
			val |= (__gx->chnAmbColor[1]&0xff);
			break;
		case GX_ALPHA0:
			reg = 0;
			val = ((__gx->chnAmbColor[0]&~0xff)|(color.a&0xff));
			break;
		case GX_ALPHA1:
			reg = 1;
			val = ((__gx->chnAmbColor[1]&~0xff)|(color.a&0xff));
			break;
		case GX_COLOR0A0:
			reg = 0;
			val |= (color.a&0xFF);
			break;
		case GX_COLOR1A1:
			reg = 1;
			val |= (color.a&0xFF);
			break;
		default:
			return;
	}

	__gx->chnAmbColor[reg] = val;
	__gx->dirtyState |= (0x0100<<reg);
}

void GX_SetChanMatColor(s32 channel,GXColor color)
{
	u32 reg,val = (_SHIFTL(color.r,24,8))|(_SHIFTL(color.g,16,8))|(_SHIFTL(color.b,8,8))|0x00;
	switch(channel) {
		case GX_COLOR0:
			reg = 0;
			val |= (__gx->chnMatColor[0]&0xff);
			break;
		case GX_COLOR1:
			reg = 1;
			val |= (__gx->chnMatColor[1]&0xff);
			break;
		case GX_ALPHA0:
			reg = 0;
			val = ((__gx->chnMatColor[0]&~0xff)|(color.a&0xff));
			break;
		case GX_ALPHA1:
			reg = 1;
			val = ((__gx->chnMatColor[1]&~0xff)|(color.a&0xff));
			break;
		case GX_COLOR0A0:
			reg = 0;
			val |= (color.a&0xFF);
			break;
		case GX_COLOR1A1:
			reg = 1;
			val |= (color.a&0xFF);
			break;
		default:
			return;
	}

	__gx->chnMatColor[reg] = val;
	__gx->dirtyState |= (0x0400<<reg);
}

void GX_SetArray(u32 attr,void *ptr,u8 stride)
{
	u32 idx = 0;

	if(attr==GX_VA_NBT) attr = GX_VA_NRM;
	if(attr>=GX_VA_POS && attr<=GX_LIGHTARRAY) {
		idx = attr-GX_VA_POS;
		GX_LOAD_CP_REG((0xA0+idx),(u32)MEM_VIRTUAL_TO_PHYSICAL(ptr));
		GX_LOAD_CP_REG((0xB0+idx),(u32)stride);
	}
}

static __inline__ void __SETVCDATTR(u8 attr,u8 type)
{
	switch(attr) {
		case GX_VA_PTNMTXIDX:
			__gx->vcdLo = (__gx->vcdLo&~0x1)|(type&0x1);
			break;
		case GX_VA_TEX0MTXIDX:
			__gx->vcdLo = (__gx->vcdLo&~0x2)|(_SHIFTL(type,1,1));
			break;
		case GX_VA_TEX1MTXIDX:
			__gx->vcdLo = (__gx->vcdLo&~0x4)|(_SHIFTL(type,2,1));
			break;
		case GX_VA_TEX2MTXIDX:
			__gx->vcdLo = (__gx->vcdLo&~0x8)|(_SHIFTL(type,3,1));
			break;
		case GX_VA_TEX3MTXIDX:
			__gx->vcdLo = (__gx->vcdLo&~0x10)|(_SHIFTL(type,4,1));
			break;
		case GX_VA_TEX4MTXIDX:
			__gx->vcdLo = (__gx->vcdLo&~0x20)|(_SHIFTL(type,5,1));
			break;
		case GX_VA_TEX5MTXIDX:
			__gx->vcdLo = (__gx->vcdLo&~0x40)|(_SHIFTL(type,6,1));
			break;
		case GX_VA_TEX6MTXIDX:
			__gx->vcdLo = (__gx->vcdLo&~0x80)|(_SHIFTL(type,7,1));
			break;
		case GX_VA_TEX7MTXIDX:
			__gx->vcdLo = (__gx->vcdLo&~0x100)|(_SHIFTL(type,8,1));
			break;
		case GX_VA_POS:
			__gx->vcdLo = (__gx->vcdLo&~0x600)|(_SHIFTL(type,9,2));
			break;
		case GX_VA_NRM:
			__gx->vcdLo = (__gx->vcdLo&~0x1800)|(_SHIFTL(type,11,2));
			__gx->vcdNrms = 1;
			break;
		case GX_VA_NBT:
			__gx->vcdLo = (__gx->vcdLo&~0x1800)|(_SHIFTL(type,11,2));
			__gx->vcdNrms = 2;
			break;
		case GX_VA_CLR0:
			__gx->vcdLo = (__gx->vcdLo&~0x6000)|(_SHIFTL(type,13,2));
			break;
		case GX_VA_CLR1:
			__gx->vcdLo = (__gx->vcdLo&~0x18000)|(_SHIFTL(type,15,2));
			break;
		case GX_VA_TEX0:
			__gx->vcdHi = (__gx->vcdHi&~0x3)|(type&0x3);
			break;
		case GX_VA_TEX1:
			__gx->vcdHi = (__gx->vcdHi&~0xc)|(_SHIFTL(type,2,2));
			break;
		case GX_VA_TEX2:
			__gx->vcdHi = (__gx->vcdHi&~0x30)|(_SHIFTL(type,4,2));
			break;
		case GX_VA_TEX3:
			__gx->vcdHi = (__gx->vcdHi&~0xc0)|(_SHIFTL(type,6,2));
			break;
		case GX_VA_TEX4:
			__gx->vcdHi = (__gx->vcdHi&~0x300)|(_SHIFTL(type,8,2));
			break;
		case GX_VA_TEX5:
			__gx->vcdHi = (__gx->vcdHi&~0xc00)|(_SHIFTL(type,10,2));
			break;
		case GX_VA_TEX6:
			__gx->vcdHi = (__gx->vcdHi&~0x3000)|(_SHIFTL(type,12,2));
			break;
		case GX_VA_TEX7:
			__gx->vcdHi = (__gx->vcdHi&~0xc000)|(_SHIFTL(type,14,2));
			break;
	}
}

void GX_SetVtxDesc(u8 attr,u8 type)
{
	__SETVCDATTR(attr,type);
	__gx->dirtyState |= 0x0008;
}

void GX_SetVtxDescv(GXVtxDesc *attr_list)
{
	u32 i;

	if(!attr_list) return;

	for(i=0;i<GX_MAX_VTXDESC_LISTSIZE;i++){
		if(attr_list[i].attr==GX_VA_NULL) break;

		__SETVCDATTR(attr_list[i].attr,attr_list[i].type);
	}
	__gx->dirtyState |= 0x0008;
}

void GX_GetVtxDescv(GXVtxDesc *attr_list)
{
	u32 count;

	// Clear everything first
	for(count=0;count<GX_MAX_VTXDESC_LISTSIZE;count++) {
		attr_list[count].attr = GX_VA_NULL;
		attr_list[count].type = 0;
	}

	count = 0;
	if(__gx->vcdLo&0x1) {
		attr_list[count].attr = GX_VA_PTNMTXIDX;
		attr_list[count].type = __gx->vcdLo&0x1;
		count++;
	}

	if(__gx->vcdLo&0x2) {
		attr_list[count].attr = GX_VA_TEX0MTXIDX;
		attr_list[count].type = _SHIFTR(__gx->vcdLo&0x2,1,1);
		count++;
	}

	if(__gx->vcdLo&0x4) {
		attr_list[count].attr = GX_VA_TEX1MTXIDX;
		attr_list[count].type = _SHIFTR(__gx->vcdLo&0x4,2,1);
		count++;
	}

	if(__gx->vcdLo&0x8) {
		attr_list[count].attr = GX_VA_TEX2MTXIDX;
		attr_list[count].type = _SHIFTR(__gx->vcdLo&0x8,3,1);
		count++;
	}

	if(__gx->vcdLo&0x10) {
		attr_list[count].attr = GX_VA_TEX3MTXIDX;
		attr_list[count].type = _SHIFTR(__gx->vcdLo&0x10,4,1);
		count++;
	}

	if(__gx->vcdLo&0x20) {
		attr_list[count].attr = GX_VA_TEX4MTXIDX;
		attr_list[count].type = _SHIFTR(__gx->vcdLo&0x20,5,1);
		count++;
	}

	if(__gx->vcdLo&0x40) {
		attr_list[count].attr = GX_VA_TEX5MTXIDX;
		attr_list[count].type = _SHIFTR(__gx->vcdLo&0x40,6,1);
		count++;
	}

	if(__gx->vcdLo&0x80) {
		attr_list[count].attr = GX_VA_TEX6MTXIDX;
		attr_list[count].type = _SHIFTR(__gx->vcdLo&0x80,7,1);
		count++;
	}

	if(__gx->vcdLo&0x100) {
		attr_list[count].attr = GX_VA_TEX7MTXIDX;
		attr_list[count].type = _SHIFTR(__gx->vcdLo&0x100,8,1);
		count++;
	}

	if(__gx->vcdLo&0x600) {
		attr_list[count].attr = GX_VA_POS;
		attr_list[count].type = _SHIFTR(__gx->vcdLo&0x600,9,2);
		count++;
	}

	if(__gx->vcdLo&0x1800) {
		if(__gx->vcdNrms==1) {
			attr_list[count].attr = GX_VA_NRM;
			attr_list[count].type = _SHIFTR(__gx->vcdLo&0x1800,11,2);
			count++;
		} else if(__gx->vcdNrms==2){
			attr_list[count].attr = GX_VA_NBT;
			attr_list[count].type = _SHIFTR(__gx->vcdLo&0x1800,11,2);
			count++;
		}
	}

	if(__gx->vcdLo&0x6000) {
		attr_list[count].attr = GX_VA_CLR0;
		attr_list[count].type = _SHIFTR(__gx->vcdLo&0x6000,13,2);
		count++;
	}

	if(__gx->vcdLo&0x18000) {
		attr_list[count].attr = GX_VA_CLR1;
		attr_list[count].type = _SHIFTR(__gx->vcdLo&0x18000,15,2);
		count++;
	}

	if(__gx->vcdHi&0x3) {
		attr_list[count].attr = GX_VA_TEX0;
		attr_list[count].type = __gx->vcdHi&0x3;
		count++;
	}

	if(__gx->vcdHi&0xc) {
		attr_list[count].attr = GX_VA_TEX1;
		attr_list[count].type = _SHIFTR(__gx->vcdHi&0xc,2,2);
		count++;
	}

	if(__gx->vcdHi&0x30) {
		attr_list[count].attr = GX_VA_TEX2;
		attr_list[count].type = _SHIFTR(__gx->vcdHi&0x30,4,2);
		count++;
	}

	if(__gx->vcdHi&0xc0) {
		attr_list[count].attr = GX_VA_TEX3;
		attr_list[count].type = _SHIFTR(__gx->vcdHi&0xc0,6,2);
		count++;
	}

	if(__gx->vcdHi&0x300) {
		attr_list[count].attr = GX_VA_TEX4;
		attr_list[count].type = _SHIFTR(__gx->vcdHi&0x300,8,2);
		count++;
	}

	if(__gx->vcdHi&0xc00) {
		attr_list[count].attr = GX_VA_TEX5;
		attr_list[count].type = _SHIFTR(__gx->vcdHi&0xc00,10,2);
		count++;
	}

	if(__gx->vcdHi&0x3000) {
		attr_list[count].attr = GX_VA_TEX6;
		attr_list[count].type = _SHIFTR(__gx->vcdHi&0x3000,12,2);
		count++;
	}

	if(__gx->vcdHi&0xc000) {
		attr_list[count].attr = GX_VA_TEX7;
		attr_list[count].type = _SHIFTR(__gx->vcdHi&0xc000,14,2);
		count++;
	}
}

static __inline__ void __SETVCDFMT(u8 vtxfmt,u32 vtxattr,u32 comptype,u32 compsize,u32 frac)
{
	u8 vat = (vtxfmt&7);

	if(vtxattr==GX_VA_POS && (comptype==GX_POS_XY || comptype==GX_POS_XYZ)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x1)|(comptype&1);
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0xe)|(_SHIFTL(compsize,1,3));
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x1f0)|(_SHIFTL(frac,4,5));
		if(frac)
			__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_NRM && comptype==GX_NRM_XYZ
		&& (compsize==GX_S8 || compsize==GX_S16 || compsize==GX_F32)) {
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x200);
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x1C00)|(_SHIFTL(compsize,10,3));
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x80000000);
	} else if(vtxattr==GX_VA_NBT && (comptype==GX_NRM_NBT || comptype==GX_NRM_NBT3)
		&& (compsize==GX_S8 || compsize==GX_S16 || compsize==GX_F32)) {
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x200)|0x200;
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x1C00)|(_SHIFTL(compsize,10,3));
		if(comptype==GX_NRM_NBT3)
			__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x80000000)|0x80000000;
	} else if(vtxattr==GX_VA_CLR0 && (comptype==GX_CLR_RGB || comptype==GX_CLR_RGBA)
		&& (compsize>=GX_RGB565 && compsize<=GX_RGBA8)) {
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x2000)|(_SHIFTL(comptype,13,1));
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x1C000)|(_SHIFTL(compsize,14,3));
	} else if(vtxattr==GX_VA_CLR1 && (comptype==GX_CLR_RGB || comptype==GX_CLR_RGBA)
		&& (compsize>=GX_RGB565 && compsize<=GX_RGBA8)) {
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x20000)|(_SHIFTL(comptype,17,1));
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x1C0000)|(_SHIFTL(compsize,18,3));
	} else if(vtxattr==GX_VA_TEX0 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x200000)|(_SHIFTL(comptype,21,1));
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x1C00000)|(_SHIFTL(compsize,22,3));
		__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x3E000000)|(_SHIFTL(frac,25,5));
		if(frac)
			__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX1 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		__gx->VAT1reg[vat] = (__gx->VAT1reg[vat]&~0x1)|(comptype&1);
		__gx->VAT1reg[vat] = (__gx->VAT1reg[vat]&~0xe)|(_SHIFTL(compsize,1,3));
		__gx->VAT1reg[vat] = (__gx->VAT1reg[vat]&~0x1F0)|(_SHIFTL(frac,4,5));
		if(frac)
			__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX2 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		__gx->VAT1reg[vat] = (__gx->VAT1reg[vat]&~0x200)|(_SHIFTL(comptype,9,1));
		__gx->VAT1reg[vat] = (__gx->VAT1reg[vat]&~0x1C00)|(_SHIFTL(compsize,10,3));
		__gx->VAT1reg[vat] = (__gx->VAT1reg[vat]&~0x3E000)|(_SHIFTL(frac,13,5));
		if(frac)
			__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX3 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		__gx->VAT1reg[vat] = (__gx->VAT1reg[vat]&~0x40000)|(_SHIFTL(comptype,18,1));
		__gx->VAT1reg[vat] = (__gx->VAT1reg[vat]&~0x380000)|(_SHIFTL(compsize,19,3));
		__gx->VAT1reg[vat] = (__gx->VAT1reg[vat]&~0x7C00000)|(_SHIFTL(frac,22,5));
		if(frac)
			__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX4 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		__gx->VAT1reg[vat] = (__gx->VAT1reg[vat]&~0x8000000)|(_SHIFTL(comptype,27,1));
		__gx->VAT1reg[vat] = (__gx->VAT1reg[vat]&~0x70000000)|(_SHIFTL(compsize,28,3));
		__gx->VAT2reg[vat] = (__gx->VAT2reg[vat]&~0x1f)|(frac&0x1f);
		if(frac)
			__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX5 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		__gx->VAT2reg[vat] = (__gx->VAT2reg[vat]&~0x20)|(_SHIFTL(comptype,5,1));
		__gx->VAT2reg[vat] = (__gx->VAT2reg[vat]&~0x1C0)|(_SHIFTL(compsize,6,3));
		__gx->VAT2reg[vat] = (__gx->VAT2reg[vat]&~0x3E00)|(_SHIFTL(frac,9,5));
		if(frac)
			__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX6 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		__gx->VAT2reg[vat] = (__gx->VAT2reg[vat]&~0x4000)|(_SHIFTL(comptype,14,1));
		__gx->VAT2reg[vat] = (__gx->VAT2reg[vat]&~0x38000)|(_SHIFTL(compsize,15,3));
		__gx->VAT2reg[vat] = (__gx->VAT2reg[vat]&~0x7C0000)|(_SHIFTL(frac,18,5));
		if(frac)
			__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x40000000)|0x40000000;
	} else if(vtxattr==GX_VA_TEX7 && (comptype==GX_TEX_S || comptype==GX_TEX_ST)
		&& (compsize>=GX_U8 && compsize<=GX_F32)) {
		__gx->VAT2reg[vat] = (__gx->VAT2reg[vat]&~0x800000)|(_SHIFTL(comptype,23,1));
		__gx->VAT2reg[vat] = (__gx->VAT2reg[vat]&~0x7000000)|(_SHIFTL(compsize,24,3));
		__gx->VAT2reg[vat] = (__gx->VAT2reg[vat]&~0xF8000000)|(_SHIFTL(frac,27,5));
		if(frac)
			__gx->VAT0reg[vat] = (__gx->VAT0reg[vat]&~0x40000000)|0x40000000;
	}
}

void GX_SetVtxAttrFmt(u8 vtxfmt,u32 vtxattr,u32 comptype,u32 compsize,u32 frac)
{
	__SETVCDFMT(vtxfmt,vtxattr,comptype,compsize,frac);
	__gx->VATTable |= (1<<vtxfmt);
	__gx->dirtyState |= 0x0010;
}

void GX_SetVtxAttrFmtv(u8 vtxfmt,GXVtxAttrFmt *attr_list)
{
	u32 i;

	for(i=0;i<GX_MAX_VTXATTRFMT_LISTSIZE;i++) {
		if(attr_list[i].vtxattr==GX_VA_NULL) break;

		__SETVCDFMT(vtxfmt,attr_list[i].vtxattr,attr_list[i].comptype,attr_list[i].compsize,attr_list[i].frac);
	}
	__gx->VATTable |= (1<<vtxfmt);
	__gx->dirtyState |= 0x0010;
}

void GX_Begin(u8 primitve,u8 vtxfmt,u16 vtxcnt)
{
	u8 reg = primitve|(vtxfmt&7);

	if(__gx->dirtyState)
		__GX_SetDirtyState();

	wgPipe->U8 = reg;
	wgPipe->U16 = vtxcnt;
}

void GX_SetTexCoordGen(u16 texcoord,u32 tgen_typ,u32 tgen_src,u32 mtxsrc)
{
		GX_SetTexCoordGen2(texcoord,tgen_typ,tgen_src,mtxsrc,GX_FALSE,GX_DTTIDENTITY);
}

void GX_SetTexCoordGen2(u16 texcoord,u32 tgen_typ,u32 tgen_src,u32 mtxsrc,u32 normalize,u32 postmtx)
{
	u32 txc;
	u32 texcoords;
	u8 vtxrow,stq;

	if(texcoord>=GX_MAXCOORD) return;

	stq = 0;
	switch(tgen_src) {
		case GX_TG_POS:
			vtxrow = 0;
			stq = 1;
			break;
		case GX_TG_NRM:
			vtxrow = 1;
			stq = 1;
			break;
		case GX_TG_BINRM:
			vtxrow = 3;
			stq = 1;
			break;
		case GX_TG_TANGENT:
			vtxrow = 4;
			stq = 1;
			break;
		case GX_TG_COLOR0:
			vtxrow = 2;
			break;
		case GX_TG_COLOR1:
			vtxrow = 2;
			break;
		case GX_TG_TEX0:
			vtxrow = 5;
			break;
		case GX_TG_TEX1:
			vtxrow = 6;
			break;
		case GX_TG_TEX2:
			vtxrow = 7;
			break;
		case GX_TG_TEX3:
			vtxrow = 8;
			break;
		case GX_TG_TEX4:
			vtxrow = 9;
			break;
		case GX_TG_TEX5:
			vtxrow = 10;
			break;
		case GX_TG_TEX6:
			vtxrow = 11;
			break;
		case GX_TG_TEX7:
			vtxrow = 12;
			break;
		default:
			vtxrow = 5;
			break;
	}

	texcoords = 0;
	txc = (texcoord&7);
	if((tgen_typ==GX_TG_MTX3x4 || tgen_typ==GX_TG_MTX2x4))
	{
		if(tgen_typ==GX_TG_MTX3x4) texcoords = 0x02;

		texcoords |= (_SHIFTL(stq,2,1));
		texcoords |= (_SHIFTL(vtxrow,7,5));
	} else if((tgen_typ>=GX_TG_BUMP0 && tgen_typ<=GX_TG_BUMP7))
	{
		tgen_src -= GX_TG_TEXCOORD0;
		tgen_typ -= GX_TG_BUMP0;

		texcoords = 0x10;
		texcoords |= (_SHIFTL(stq,2,1));
		texcoords |= (_SHIFTL(vtxrow,7,5));
		texcoords |= (_SHIFTL(tgen_src,12,3));
		texcoords |= (_SHIFTL(tgen_typ,15,3));
	} else if(tgen_typ==GX_TG_SRTG) {
		if(tgen_src==GX_TG_COLOR0) texcoords = 0x20;
		else if(tgen_src==GX_TG_COLOR1) texcoords = 0x30;
		texcoords |= (_SHIFTL(stq,2,1));
		texcoords |= (_SHIFTL(2,7,5));
	}

	postmtx -= GX_DTTMTX0;
	__gx->texCoordGen[txc] = texcoords;
	__gx->texCoordGen2[txc] = ((_SHIFTL(normalize,8,1))|(postmtx&0x3f));

	switch(texcoord) {
		case GX_TEXCOORD0:
			__gx->mtxIdxLo = (__gx->mtxIdxLo&~0xfc0)|(_SHIFTL(mtxsrc,6,6));
			break;
		case GX_TEXCOORD1:
			__gx->mtxIdxLo = (__gx->mtxIdxLo&~0x3f000)|(_SHIFTL(mtxsrc,12,6));
			break;
		case GX_TEXCOORD2:
			__gx->mtxIdxLo = (__gx->mtxIdxLo&~0xfc0000)|(_SHIFTL(mtxsrc,18,6));
			break;
		case GX_TEXCOORD3:
			__gx->mtxIdxLo = (__gx->mtxIdxLo&~0x3f000000)|(_SHIFTL(mtxsrc,24,6));
			break;
		case GX_TEXCOORD4:
			__gx->mtxIdxHi = (__gx->mtxIdxHi&~0x3f)|(mtxsrc&0x3f);
			break;
		case GX_TEXCOORD5:
			__gx->mtxIdxHi = (__gx->mtxIdxHi&~0xfc0)|(_SHIFTL(mtxsrc,6,6));
			break;
		case GX_TEXCOORD6:
			__gx->mtxIdxHi = (__gx->mtxIdxHi&~0x3f000)|(_SHIFTL(mtxsrc,12,6));
			break;
		case GX_TEXCOORD7:
			__gx->mtxIdxHi = (__gx->mtxIdxHi&~0xfc0000)|(_SHIFTL(mtxsrc,18,6));
			break;
	}
	__gx->dirtyState |= (0x04000000|(0x00010000<<texcoord));
}

void GX_SetZTexture(u8 op,u8 fmt,u32 bias)
{
	u32 val = 0;

	if(fmt==GX_TF_Z8) fmt = 0;
	else if(fmt==GX_TF_Z16) fmt = 1;
	else fmt = 2;

	val = (u32)(_SHIFTL(op,2,2))|(fmt&3);
	GX_LOAD_BP_REG(0xF4000000|(bias&0x00FFFFFF));
	GX_LOAD_BP_REG(0xF5000000|(val&0x00FFFFFF));
}

static inline void WriteMtxPS4x3(register Mtx mt,register void *wgpipe)
{
	register f32 tmp0,tmp1,tmp2,tmp3,tmp4,tmp5;
	__asm__ __volatile__ (
		 "psq_l %0,0(%6),0,0\n\
		  psq_l %1,8(%6),0,0\n\
		  psq_l %2,16(%6),0,0\n\
		  psq_l %3,24(%6),0,0\n\
		  psq_l %4,32(%6),0,0\n\
		  psq_l %5,40(%6),0,0\n\
		  psq_st %0,0(%7),0,0\n\
		  psq_st %1,0(%7),0,0\n\
		  psq_st %2,0(%7),0,0\n\
		  psq_st %3,0(%7),0,0\n\
		  psq_st %4,0(%7),0,0\n\
		  psq_st %5,0(%7),0,0"
		  : "=&f"(tmp0),"=&f"(tmp1),"=&f"(tmp2),"=&f"(tmp3),"=&f"(tmp4),"=&f"(tmp5)
		  : "b"(mt), "b"(wgpipe)
		  : "memory"
	);
}

static inline void WriteMtxPS3x3from4x3(register Mtx mt,register void *wgpipe)
{
	register f32 tmp0,tmp1,tmp2,tmp3,tmp4,tmp5;
	__asm__ __volatile__
		("psq_l %0,0(%6),0,0\n\
		  lfs	%1,8(%6)\n\
		  psq_l %2,16(%6),0,0\n\
		  lfs	%3,24(%6)\n\
		  psq_l %4,32(%6),0,0\n\
		  lfs	%5,40(%6)\n\
		  psq_st %0,0(%7),0,0\n\
		  stfs	 %1,0(%7)\n\
		  psq_st %2,0(%7),0,0\n\
		  stfs	 %3,0(%7)\n\
		  psq_st %4,0(%7),0,0\n\
		  stfs	 %5,0(%7)"
		  : "=&f"(tmp0),"=&f"(tmp1),"=&f"(tmp2),"=&f"(tmp3),"=&f"(tmp4),"=&f"(tmp5)
		  : "b"(mt), "b"(wgpipe)
		  : "memory"
	);
}

static inline void WriteMtxPS3x3(register Mtx33 mt,register void *wgpipe)
{
	register f32 tmp0,tmp1,tmp2,tmp3,tmp4;
	__asm__ __volatile__
		("psq_l %0,0(%5),0,0\n\
		  psq_l %1,8(%5),0,0\n\
		  psq_l %2,16(%5),0,0\n\
		  psq_l %3,24(%5),0,0\n\
		  lfs	%4,32(%5)\n\
		  psq_st %0,0(%6),0,0\n\
		  psq_st %1,0(%6),0,0\n\
		  psq_st %2,0(%6),0,0\n\
		  psq_st %3,0(%6),0,0\n\
		  stfs	 %4,0(%6)"
		  : "=&f"(tmp0),"=&f"(tmp1),"=&f"(tmp2),"=&f"(tmp3),"=&f"(tmp4)
		  : "b"(mt), "b"(wgpipe)
		  : "memory"
	);
}

static inline void WriteMtxPS4x2(register Mtx mt,register void *wgpipe)
{
	register f32 tmp0,tmp1,tmp2,tmp3;
	__asm__ __volatile__
		("psq_l %0,0(%4),0,0\n\
		  psq_l %1,8(%4),0,0\n\
		  psq_l %2,16(%4),0,0\n\
		  psq_l %3,24(%4),0,0\n\
		  psq_st %0,0(%5),0,0\n\
		  psq_st %1,0(%5),0,0\n\
		  psq_st %2,0(%5),0,0\n\
		  psq_st %3,0(%5),0,0"
		  : "=&f"(tmp0),"=&f"(tmp1),"=&f"(tmp2),"=&f"(tmp3)
		  : "b"(mt), "b"(wgpipe)
		  : "memory"
	);
}

void GX_LoadPosMtxImm(Mtx mt,u32 pnidx)
{
	GX_LOAD_XF_REGS((0x0000|(_SHIFTL(pnidx,2,8))),12);
	WriteMtxPS4x3(mt,(void*)wgPipe);
}

void GX_LoadPosMtxIdx(u16 mtxidx,u32 pnidx)
{
	wgPipe->U8 = 0x20;
	wgPipe->U32 = ((_SHIFTL(mtxidx,16,16))|0xb000|(_SHIFTL(pnidx,2,8)));
}

void GX_LoadNrmMtxImm(Mtx mt,u32 pnidx)
{
	GX_LOAD_XF_REGS((0x0400|(pnidx*3)),9);
	WriteMtxPS3x3from4x3(mt,(void*)wgPipe);
}

void GX_LoadNrmMtxImm3x3(Mtx33 mt,u32 pnidx)
{
	GX_LOAD_XF_REGS((0x0400|(pnidx*3)),9);
	WriteMtxPS3x3(mt,(void*)wgPipe);
}

void GX_LoadNrmMtxIdx3x3(u16 mtxidx,u32 pnidx)
{
	wgPipe->U8 = 0x28;
	wgPipe->U32 = ((_SHIFTL(mtxidx,16,16))|0x8000|(0x0400|(pnidx*3)));
}

void GX_LoadTexMtxImm(Mtx mt,u32 texidx,u8 type)
{
	u32 addr = 0;
	u32 rows = (type==GX_MTX2x4)?2:3;

	if(texidx<GX_DTTMTX0) addr = (_SHIFTL(texidx,2,8));
	else {
		texidx -= GX_DTTMTX0;
		addr = 0x0500 + (_SHIFTL(texidx,2,8));
	}

	GX_LOAD_XF_REGS(addr,(rows*4));
	if(type==GX_MTX2x4)
		WriteMtxPS4x2(mt,(void*)wgPipe);
	else
		WriteMtxPS4x3(mt,(void*)wgPipe);
}

void GX_LoadTexMtxIdx(u16 mtxidx,u32 texidx,u8 type)
{
	u32 addr,size = (type==GX_MTX2x4)?7:11;

	if(texidx<GX_DTTMTX0) addr = 0x0000|(_SHIFTL(texidx,2,8));
	else addr = 0x0500|(_SHIFTL((texidx-GX_DTTMTX0),2,8));

	wgPipe->U8 = 0x30;
	wgPipe->U32 = ((_SHIFTL(mtxidx,16,16))|(_SHIFTL(size,12,4))|addr);
}

void GX_SetCurrentMtx(u32 mtx)
{
	__gx->mtxIdxLo = (__gx->mtxIdxLo&~0x3f)|(mtx&0x3f);
	__gx->dirtyState |= 0x04000000;
}

void GX_SetNumTexGens(u32 nr)
{
	__gx->genMode = (__gx->genMode&~0xf)|(nr&0xf);
	__gx->dirtyState |= 0x02000004;
}

void GX_InvVtxCache()
{
	wgPipe->U8 = 0x48; // vertex cache weg
}

void GX_SetZMode(u8 enable,u8 func,u8 update_enable)
{
	__gx->peZMode = (__gx->peZMode&~0x1)|(enable&1);
	__gx->peZMode = (__gx->peZMode&~0xe)|(_SHIFTL(func,1,3));
	__gx->peZMode = (__gx->peZMode&~0x10)|(_SHIFTL(update_enable,4,1));
	GX_LOAD_BP_REG(__gx->peZMode);
}

u32 GX_GetTexObjFmt(GXTexObj *obj)
{
	return ((struct __gx_texobj*)obj)->tex_fmt;
}

u32 GX_GetTexObjMipMap(GXTexObj *obj)
{
	return (((struct __gx_texobj*)obj)->tex_flag&0x01);
}
void* GX_GetTexObjData(GXTexObj *obj)
{
	return (void*)(_SHIFTL(((struct __gx_texobj*)obj)->tex_maddr & 0x00ffffff,5,24));
}

u8 GX_GetTexObjWrapS(GXTexObj* obj)
{
	return ((struct __gx_texobj*)obj)->tex_filt & 0x03;
}

u8 GX_GetTexObjWrapT(GXTexObj* obj)
{
	return _SHIFTR(((struct __gx_texobj*)obj)->tex_filt & 0x0c, 2, 2);
}

u16 GX_GetTexObjHeight(GXTexObj* obj)
{
	return _SHIFTR(((struct __gx_texobj*)obj)->tex_size & 0xffc00, 10, 10) + 1;
}

u16 GX_GetTexObjWidth(GXTexObj* obj)
{
	return (((struct __gx_texobj*)obj)->tex_size & 0x3ff) + 1;
}

void GX_GetTexObjAll(GXTexObj* obj, void** image_ptr, u16* width, u16* height,
                     u8* format, u8* wrap_s, u8* wrap_t, u8* mipmap)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;
	*image_ptr = (void*)(_SHIFTL(ptr->tex_maddr & 0x00ffffff,5,24));
	*width = (ptr->tex_size & 0x3ff) + 1;
	*height = _SHIFTR(ptr->tex_size & 0xffc00, 10, 10) + 1;
	*format = ptr->tex_fmt;
	*wrap_s = ptr->tex_filt & 0x03;
	*wrap_t = _SHIFTR(ptr->tex_filt & 0x0c, 2, 2);
	*mipmap = ptr->tex_flag & 0x01;
}
u32 GX_GetTexBufferSize(u16 wd,u16 ht,u32 fmt,u8 mipmap,u8 maxlod)
{
	u32 xshift,yshift,xtiles,ytiles,bitsize,size;

	switch(fmt) {
		case GX_TF_I4:
		case GX_TF_CMPR:
		case GX_CTF_R4:
		case GX_CTF_RA4:
		case GX_CTF_Z4:
			xshift = 3;
			yshift = 3;
			break;
		case GX_TF_Z8:
		case GX_TF_I8:
		case GX_TF_IA4:
		case GX_CTF_A8:
		case GX_CTF_R8:
		case GX_CTF_G8:
		case GX_CTF_B8:
		case GX_CTF_RG8:
		case GX_CTF_GB8:
		case GX_CTF_Z8M:
		case GX_CTF_Z8L:
			xshift = 3;
			yshift = 2;
			break;
		case GX_TF_IA8:
		case GX_TF_Z16:
		case GX_TF_Z24X8:
		case GX_TF_RGB565:
		case GX_TF_RGB5A3:
		case GX_TF_RGBA8:
		case GX_CTF_Z16L:
		case GX_CTF_RA8:
			xshift = 2;
			yshift = 2;
			break;
		default:
			xshift = 2;
			yshift = 2;
			break;
	}

	bitsize = 32;
	if(fmt==GX_TF_RGBA8 || fmt==GX_TF_Z24X8) bitsize = 64;

	size = 0;
	if(mipmap) {
		u32 cnt = (maxlod&0xff);
		while(cnt) {
			u32 w = wd&0xffff;
			u32 h = ht&0xffff;
			xtiles = ((w+(1<<xshift))-1)>>xshift;
			ytiles = ((h+(1<<yshift))-1)>>yshift;
			if(cnt==0) return size;

			size += ((xtiles*ytiles)*bitsize);
			if(w==0x0001 && h==0x0001) return size;
			if(wd>0x0001) wd = (w>>1);
			else wd = 0x0001;
			if(ht>0x0001) ht = (h>>1);
			else ht = 0x0001;

			--cnt;
		}
		return size;
	}

	wd &= 0xffff;
	xtiles = (wd+((1<<xshift)-1))>>xshift;

	ht &= 0xffff;
	ytiles = (ht+((1<<yshift)-1))>>yshift;

	size = ((xtiles*ytiles)*bitsize);

	return size;
}

void GX_InitTexCacheRegion(GXTexRegion *region,u8 is32bmipmap,u32 tmem_even,u8 size_even,u32 tmem_odd,u8 size_odd)
{
	u32 sze = 0;
	struct __gx_texregion *ptr = (struct __gx_texregion*)region;

	switch(size_even) {
		case GX_TEXCACHE_32K:
			sze = 3;
			break;
		case GX_TEXCACHE_128K:
			sze = 4;
			break;
		case GX_TEXCACHE_512K:
			sze = 5;
			break;
		default:
			sze = 3;
			break;
	}
	ptr->tmem_even = 0;
	ptr->tmem_even = (ptr->tmem_even&~0x7fff)|(_SHIFTR(tmem_even,5,15));
	ptr->tmem_even = (ptr->tmem_even&~0x38000)|(_SHIFTL(sze,15,3));
	ptr->tmem_even = (ptr->tmem_even&~0x1C0000)|(_SHIFTL(sze,18,3));

	switch(size_odd) {
		case GX_TEXCACHE_32K:
			sze = 3;
			break;
		case GX_TEXCACHE_128K:
			sze = 4;
			break;
		case GX_TEXCACHE_512K:
			sze = 5;
			break;
		default:
			sze = 3;
			break;
	}
	ptr->tmem_odd = 0;
	ptr->tmem_odd = (ptr->tmem_odd&~0x7fff)|(_SHIFTR(tmem_odd,5,15));
	ptr->tmem_odd = (ptr->tmem_odd&~0x38000)|(_SHIFTL(sze,15,3));
	ptr->tmem_odd = (ptr->tmem_odd&~0x1C0000)|(_SHIFTL(sze,18,3));

	ptr->ismipmap = is32bmipmap;
	ptr->iscached = 1;
}

void GX_InitTexPreloadRegion(GXTexRegion *region,u32 tmem_even,u32 size_even,u32 tmem_odd,u32 size_odd)
{
	struct __gx_texregion *ptr = (struct __gx_texregion*)region;

	ptr->tmem_even = 0;
	ptr->tmem_even = (ptr->tmem_even&~0x7FFF)|(_SHIFTR(tmem_even,5,15));
	ptr->tmem_even = (ptr->tmem_even&~0x200000)|0x200000;

	ptr->tmem_odd = 0;
	ptr->tmem_odd = (ptr->tmem_odd&~0x7FFF)|(_SHIFTR(tmem_odd,5,15));

	ptr->size_even = _SHIFTR(size_even,5,16);
	ptr->size_odd = _SHIFTR(size_odd,5,16);

	ptr->ismipmap = 0;
	ptr->iscached = 0;
}

void GX_InitTlutRegion(GXTlutRegion *region,u32 tmem_addr,u8 tlut_sz)
{
	struct __gx_tlutregion *ptr = (struct __gx_tlutregion*)region;

	tmem_addr -= 0x80000;

	ptr->tmem_addr_conf = 0;
	ptr->tmem_addr_conf = (ptr->tmem_addr_conf&~0x3ff)|(_SHIFTR(tmem_addr,9,10));
	ptr->tmem_addr_conf = (ptr->tmem_addr_conf&~0x1FFC00)|(_SHIFTL(tlut_sz,10,10));
	ptr->tmem_addr_conf = (ptr->tmem_addr_conf&~0xff000000)|(_SHIFTL(0x65,24,8));
}

void GX_InitTexObj(GXTexObj *obj,void *img_ptr,u16 wd,u16 ht,u8 fmt,u8 wrap_s,u8 wrap_t,u8 mipmap)
{
	u32 nwd,nht;
	u32 xshift,yshift;
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;

	if(!obj) return;

	memset(obj,0,sizeof(GXTexObj));

	ptr->tex_filt = (ptr->tex_filt&~0x03)|(wrap_s&3);
	ptr->tex_filt = (ptr->tex_filt&~0x0c)|(_SHIFTL(wrap_t,2,2));
	ptr->tex_filt = (ptr->tex_filt&~0x10)|0x10;

	if(mipmap) {
		ptr->tex_flag |= 0x01;
		if(fmt==GX_TF_CI4 || fmt==GX_TF_CI8 || fmt==GX_TF_CI14)
			ptr->tex_filt = (ptr->tex_filt&~0xe0)|0x00a0;
		else
			ptr->tex_filt = (ptr->tex_filt&~0xe0)|0x00c0;
	} else
		ptr->tex_filt= (ptr->tex_filt&~0xE0)|0x0080;

	ptr->tex_fmt = fmt;
	ptr->tex_size = (ptr->tex_size&~0x3ff)|((wd-1)&0x3ff);
	ptr->tex_size = (ptr->tex_size&~0xFFC00)|(_SHIFTL((ht-1),10,10));
	ptr->tex_size = (ptr->tex_size&~0xF00000)|(_SHIFTL(fmt,20,4));
	ptr->tex_maddr = (ptr->tex_maddr&~0x00ffffff)|(_SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(img_ptr),5,24));

	switch(fmt) {
		case GX_TF_I4:
		case GX_TF_CI4:
			xshift = 3;
			yshift = 3;
			ptr->tex_tile_type = 1;
			break;
		case GX_TF_I8:
		case GX_TF_IA4:
		case GX_TF_CI8:
			xshift = 3;
			yshift = 2;
			ptr->tex_tile_type = 2;
			break;
		case GX_TF_IA8:
		case GX_TF_RGB565:
		case GX_TF_RGB5A3:
		case GX_TF_RGBA8:
			xshift = 2;
			yshift = 2;
			ptr->tex_tile_type = 2;
			break;
		case GX_TF_CI14:
			xshift = 2;
			yshift = 2;
			ptr->tex_tile_type = 3;
			break;
		case GX_TF_CMPR:
			xshift = 3;
			yshift = 3;
			ptr->tex_tile_type = 0;
			break;
		default:
			xshift = 2;
			yshift = 2;
			ptr->tex_tile_type = 2;
			break;
	}

	nwd = ((wd+(1<<xshift))-1)>>xshift;
	nht = ((ht+(1<<yshift))-1)>>yshift;
	ptr->tex_tile_cnt = (nwd*nht)&0x7fff;

	ptr->tex_flag |= 0x0002;
}

void GX_InitTexObjCI(GXTexObj *obj,void *img_ptr,u16 wd,u16 ht,u8 fmt,u8 wrap_s,u8 wrap_t,u8 mipmap,u32 tlut_name)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;

	GX_InitTexObj(obj,img_ptr,wd,ht,fmt,wrap_s,wrap_t,mipmap);
	ptr->tex_flag &= ~0x02;
	ptr->tex_tlut = tlut_name;
}

void GX_InitTexObjLOD(GXTexObj *obj,u8 minfilt,u8 magfilt,f32 minlod,f32 maxlod,f32 lodbias,u8 biasclamp,u8 edgelod,u8 maxaniso)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;
	static u8 GX2HWFiltConv[] = {0x00,0x04,0x01,0x05,0x02,0x06,0x00,0x00};
	//static u8 HW2GXFiltConv[] = {0x00,0x02,0x04,0x00,0x01,0x03,0x05,0x00};

	if(lodbias<-4.0f) lodbias = -4.0f;
	else if(lodbias==4.0f) lodbias = 3.99f;

	ptr->tex_filt = (ptr->tex_filt&~0x1fe00)|(_SHIFTL(((u32)(32.0f*lodbias)),9,8));
	ptr->tex_filt = (ptr->tex_filt&~0x10)|(_SHIFTL((magfilt==GX_LINEAR?1:0),4,1));
	ptr->tex_filt = (ptr->tex_filt&~0xe0)|(_SHIFTL(GX2HWFiltConv[minfilt],5,3));
	ptr->tex_filt = (ptr->tex_filt&~0x100)|(_SHIFTL(!(edgelod&0xff),8,1));
	ptr->tex_filt = (ptr->tex_filt&~0x180000)|(_SHIFTL(maxaniso,19,2));
	ptr->tex_filt = (ptr->tex_filt&~0x200000)|(_SHIFTL(biasclamp,21,1));

	if(minlod<0.0f) minlod = 0.0f;
	else if(minlod>10.0f) minlod = 10.0f;

	if(maxlod<0.0f) maxlod = 0.0f;
	else if(maxlod>10.0f) maxlod = 10.0f;

	ptr->tex_lod = (ptr->tex_lod&~0xff)|(((u32)(16.0f*minlod))&0xff);
	ptr->tex_lod = (ptr->tex_lod&~0xff00)|(_SHIFTL(((u32)(16.0f*maxlod)),8,8));
}

void GX_InitTexObjData(GXTexObj *obj,void *img_ptr)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;
	ptr->tex_maddr = (ptr->tex_maddr&~0x00ffffff)|(_SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(img_ptr),5,24));
}

void GX_InitTexObjTlut(GXTexObj *obj,u32 tlut_name)
{
	((struct __gx_texobj*)obj)->tex_tlut = tlut_name;
}

void GX_InitTexObjWrapMode(GXTexObj *obj,u8 wrap_s,u8 wrap_t)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;

	ptr->tex_filt = (ptr->tex_filt&~0x03)|(wrap_s&3);
	ptr->tex_filt = (ptr->tex_filt&~0x0c)|(_SHIFTL(wrap_t,2,2));
}

void GX_InitTexObjFilterMode(GXTexObj *obj,u8 minfilt,u8 magfilt)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;
	static u8 GX2HWFiltConv[] = {0x00,0x04,0x01,0x05,0x02,0x06,0x00,0x00};

	ptr->tex_filt = (ptr->tex_filt&~0x10)|(_SHIFTL((magfilt==GX_LINEAR?1:0),4,1));
	ptr->tex_filt = (ptr->tex_filt&~0xe0)|(_SHIFTL(GX2HWFiltConv[minfilt],5,3));
}

void GX_InitTexObjMinLOD(GXTexObj *obj,f32 minlod)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;

	if(minlod<0.0f) minlod = 0.0f;
	else if(minlod>10.0f) minlod = 10.0f;

	ptr->tex_lod = (ptr->tex_lod&~0xff)|(((u32)(16.0f*minlod))&0xff);
}

void GX_InitTexObjMaxLOD(GXTexObj *obj,f32 maxlod)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;

	if(maxlod<0.0f) maxlod = 0.0f;
	else if(maxlod>10.0f) maxlod = 10.0f;

	ptr->tex_lod = (ptr->tex_lod&~0xff00)|(_SHIFTL(((u32)(16.0f*maxlod)),8,8));
}

void GX_InitTexObjLODBias(GXTexObj *obj,f32 lodbias)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;

	if(lodbias<-4.0f) lodbias = -4.0f;
	else if(lodbias==4.0f) lodbias = 3.99f;

	ptr->tex_filt = (ptr->tex_filt&~0x1fe00)|(_SHIFTL(((u32)(32.0f*lodbias)),9,8));
}

void GX_InitTexObjBiasClamp(GXTexObj *obj,u8 biasclamp)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;
	ptr->tex_filt = (ptr->tex_filt&~0x200000)|(_SHIFTL(biasclamp,21,1));
}

void GX_InitTexObjEdgeLOD(GXTexObj *obj,u8 edgelod)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;
	ptr->tex_filt = (ptr->tex_filt&~0x100)|(_SHIFTL(!(edgelod&0xff),8,1));
}

void GX_InitTexObjMaxAniso(GXTexObj *obj,u8 maxaniso)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;
	ptr->tex_filt = (ptr->tex_filt&~0x180000)|(_SHIFTL(maxaniso,19,2));
}

void GX_InitTexObjUserData(GXTexObj *obj,void *userdata)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;
	ptr->usr_data = (u32)userdata;
}

void* GX_GetTexObjUserData(GXTexObj *obj)
{
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;
	return (void*)ptr->usr_data;
}

void GX_InitTlutObj(GXTlutObj *obj,void *lut,u8 fmt,u16 entries)
{
	struct __gx_tlutobj *ptr = (struct __gx_tlutobj*)obj;

	memset(obj,0,sizeof(GXTlutObj));

	ptr->tlut_fmt = _SHIFTL(fmt,10,2);
	ptr->tlut_maddr = (ptr->tlut_maddr&~0x00ffffff)|(_SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(lut),5,24));
	ptr->tlut_maddr = (ptr->tlut_maddr&~0xff000000)|(_SHIFTL(0x64,24,8));
	ptr->tlut_nentries = entries;
}

void GX_LoadTexObj(GXTexObj *obj,u8 mapid)
{
	GXTexRegion *region = NULL;

	if(regionCB)
		region = regionCB(obj,mapid);

	GX_LoadTexObjPreloaded(obj,region,mapid);
}

void GX_LoadTexObjPreloaded(GXTexObj *obj,GXTexRegion *region,u8 mapid)
{
	u8 type;
	struct __gx_tlutregion *tlut = NULL;
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;
	struct __gx_texregion *reg = (struct __gx_texregion*)region;

	ptr->tex_filt = (ptr->tex_filt&~0xff000000)|(_SHIFTL(_gxtexmode0ids[mapid],24,8));
	ptr->tex_lod = (ptr->tex_lod&~0xff000000)|(_SHIFTL(_gxtexmode1ids[mapid],24,8));
	ptr->tex_size = (ptr->tex_size&~0xff000000)|(_SHIFTL(_gxteximg0ids[mapid],24,8));
	ptr->tex_maddr = (ptr->tex_maddr&~0xff000000)|(_SHIFTL(_gxteximg3ids[mapid],24,8));

	reg->tmem_even = (reg->tmem_even&~0xff000000)|(_SHIFTL(_gxteximg1ids[mapid],24,8));
	reg->tmem_odd = (reg->tmem_odd&~0xff000000)|(_SHIFTL(_gxteximg2ids[mapid],24,8));

	GX_LOAD_BP_REG(ptr->tex_filt);
	GX_LOAD_BP_REG(ptr->tex_lod);
	GX_LOAD_BP_REG(ptr->tex_size);

	GX_LOAD_BP_REG(reg->tmem_even);
	GX_LOAD_BP_REG(reg->tmem_odd);

	GX_LOAD_BP_REG(ptr->tex_maddr);

	type = ptr->tex_flag;
	if(!(type&0x02)) {
		if(tlut_regionCB)
			tlut = (struct __gx_tlutregion*)tlut_regionCB(ptr->tex_tlut);
		tlut->tmem_addr_base = (tlut->tmem_addr_base&~0xff000000)|(_SHIFTL(_gxtextlutids[mapid],24,8));
		GX_LOAD_BP_REG(tlut->tmem_addr_base);
	}

	__gx->texMapSize[mapid] = ptr->tex_size;
	__gx->texMapWrap[mapid] = ptr->tex_filt;

	__gx->dirtyState |= 0x0001;
}

void GX_PreloadEntireTexture(GXTexObj *obj,GXTexRegion *region)
{
	u32 i,fmt;
	s32 wd,ht;
	u16 cnt = 0;
	u32 regA = 0;
	u32 regB = 0;
	u32 regC = 0;
	u32 regD = 0;
	struct __gx_texobj *ptr = (struct __gx_texobj*)obj;
	struct __gx_texregion *reg = (struct __gx_texregion*)region;

	regA = (regA&~0xff000000)|(_SHIFTL(0x60,24,8));
	regA = (regA&~0x00ffffff)|(ptr->tex_maddr&~0xff000000);

	regB = (regB&~0xff000000)|(_SHIFTL(0x61,24,8));
	regB = (regB&~0x00007fff)|(reg->tmem_even&0x00007fff);

	regC = (regC&~0xff000000)|(_SHIFTL(0x62,24,8));
	regC = (regC&~0x00007fff)|(reg->tmem_odd&0x00007fff);

	regD = (regD&~0xff000000)|(_SHIFTL(0x63,24,8));
	regD = (regD&~0x00007fff)|(ptr->tex_tile_cnt&0x00007fff);
	regD = (regD&~0x00018000)|(_SHIFTL(ptr->tex_tile_type,15,2));

	fmt = _SHIFTR(ptr->tex_size,20,4);

	__GX_FlushTextureState();
	GX_LOAD_BP_REG(regA);
	GX_LOAD_BP_REG(regB);
	GX_LOAD_BP_REG(regC);
	GX_LOAD_BP_REG(regD);

	if(ptr->tex_flag&0x01) {
		wd = (ptr->tex_size&0x3ff)+1;
		ht = _SHIFTR(ptr->tex_size,10,10)+1;
		if(wd>ht)
			cnt = (31 - (cntlzw(wd)));
		else
			cnt = (31 - (cntlzw(ht)));
	}

	if(cnt>0) {
		u32 tmem_even,tmem_odd,maddr;
		u32 tile_cnt = ptr->tex_tile_cnt;

		tmem_even = (reg->tmem_even&0xffff);
		tmem_odd = (reg->tmem_odd&0xffff);
		maddr = (ptr->tex_maddr&~0xff000000);

		i = 0;
		while(cnt) {
			u32 w,h;
			u32 te,to;
			u32 xshift,yshift;

			if(fmt==GX_TF_RGBA8) {
				tmem_even += tile_cnt;
				tmem_odd += tile_cnt;
				maddr += (tile_cnt<<1);
			} else {
				maddr += tile_cnt;
				if(i&1) tmem_odd += tile_cnt;
				else tmem_even += tile_cnt;
			}

			te = tmem_even;
			to = tmem_odd;
			if(i&1) {
				te = tmem_odd;
				to = tmem_even;
			}

			w = wd>>(i+1);
			h = wd>>(i+1);
			switch(ptr->tex_fmt) {
				case GX_TF_I4:
				case GX_TF_IA4:
				case GX_TF_CI4:
				case GX_TF_CMPR:
					xshift = 3;
					yshift = 3;
					break;
				case GX_TF_I8:
				case GX_TF_CI8:
					xshift = 3;
					yshift = 2;
					break;
				case GX_TF_IA8:
				case GX_TF_RGB5A3:
				case GX_TF_RGB565:
				case GX_TF_CI14:
					xshift = 2;
					yshift = 2;
					break;
				default:
					xshift = 0;
					yshift = 0;
					break;
			}

			if(!w) w = 1;
			if(!h) h = 1;

			regA = ((regA&~0x00ffffff)|(maddr&0x00ffffff));
			GX_LOAD_BP_REG(regA);

			regB = ((regB&~0x00007fff)|(te&0x00007fff));
			GX_LOAD_BP_REG(regB);

			regC = ((regC&~0x00007fff)|(to&0x00007fff));
			GX_LOAD_BP_REG(regC);

			tile_cnt = (((w+(1<<xshift))-1)>>xshift)*(((h+(1<<yshift))-1)>>yshift);
			regD = ((regD&~0x00007fff)|(tile_cnt&0x00007fff));
			GX_LOAD_BP_REG(regD);

			++i;
			--cnt;
		}
	}
	__GX_FlushTextureState();
}

void GX_InvalidateTexAll()
{
	__GX_FlushTextureState();
	GX_LOAD_BP_REG(0x66001000);
	GX_LOAD_BP_REG(0x66001100);
	__GX_FlushTextureState();
}

void GX_InvalidateTexRegion(GXTexRegion *region)
{
	u8 ismipmap;
	s32 cw_e,ch_e,cw_o,ch_o;
	u32 size,tmp,regvalA = 0,regvalB = 0;
	struct __gx_texregion *ptr = (struct __gx_texregion*)region;

	cw_e = (_SHIFTR(ptr->tmem_even,15,3))-1;
	ch_e = (_SHIFTR(ptr->tmem_even,18,3))-1;

	cw_o = (_SHIFTR(ptr->tmem_odd,15,3))-1;
	ch_o = (_SHIFTR(ptr->tmem_odd,18,3))-1;

	if(cw_e<0) cw_e = 0;
	if(ch_e<0) ch_e = 0;
	if(cw_o<0) cw_o = 0;
	if(ch_o<0) ch_o = 0;

	ismipmap = ptr->ismipmap;

	tmp = size = cw_e+ch_e;
	if(ismipmap) size = (tmp+cw_o+ch_o)-2;
	regvalA = _SHIFTR((ptr->tmem_even&0x7fff),6,9)|(_SHIFTL(size,9,4))|(_SHIFTL(0x66,24,8));

	if(cw_o!=0) {
		size = cw_o+ch_o;
		if(ismipmap) size += (tmp-2);
		regvalB = _SHIFTR((ptr->tmem_odd&0x7fff),6,9)|(_SHIFTL(size,9,4))|(_SHIFTL(0x66,24,8));
	}
	__GX_FlushTextureState();
	GX_LOAD_BP_REG(regvalA);
	if(cw_o!=0) GX_LOAD_BP_REG(regvalB);
	__GX_FlushTextureState();
}

void GX_LoadTlut(GXTlutObj *obj,u32 tlut_name)
{
	struct __gx_tlutregion *region = NULL;
	struct __gx_tlutobj *ptr = (struct __gx_tlutobj*)obj;

	if(tlut_regionCB)
		region = (struct __gx_tlutregion*)tlut_regionCB(tlut_name);

	__GX_FlushTextureState();
	GX_LOAD_BP_REG(ptr->tlut_maddr);
	GX_LOAD_BP_REG(region->tmem_addr_conf);
	__GX_FlushTextureState();

	region->tmem_addr_base = (ptr->tlut_fmt&~0x3ff)|(region->tmem_addr_conf&0x3ff);
	region->tlut_maddr = ptr->tlut_maddr;
	region->tlut_nentries = ptr->tlut_nentries;
}

void GX_SetTexCoordScaleManually(u8 texcoord,u8 enable,u16 ss,u16 ts)
{
	u32 reg;

	__gx->texCoordManually = (__gx->texCoordManually&~(_SHIFTL(1,texcoord,1)))|(_SHIFTL(enable,texcoord,1));
	if(!enable) return;

	reg = (texcoord&0x7);
	__gx->suSsize[reg] = (__gx->suSsize[reg]&~0xffff)|((ss-1)&0xffff);
	__gx->suTsize[reg] = (__gx->suTsize[reg]&~0xffff)|((ts-1)&0xffff);

	GX_LOAD_BP_REG(__gx->suSsize[reg]);
	GX_LOAD_BP_REG(__gx->suTsize[reg]);
}

void GX_SetTexCoordCylWrap(u8 texcoord,u8 s_enable,u8 t_enable)
{
	u32 reg;

	reg = (texcoord&0x7);
	__gx->suSsize[reg] = (__gx->suSsize[reg]&~0x20000)|(_SHIFTL(s_enable,17,1));
	__gx->suTsize[reg] = (__gx->suTsize[reg]&~0x20000)|(_SHIFTL(t_enable,17,1));

	if(!(__gx->texCoordManually&(_SHIFTL(1,texcoord,1)))) return;

	GX_LOAD_BP_REG(__gx->suSsize[reg]);
	GX_LOAD_BP_REG(__gx->suTsize[reg]);
}

void GX_SetTexCoordBias(u8 texcoord,u8 s_enable,u8 t_enable)
{
	u32 reg;

	reg = (texcoord&0x7);
	__gx->suSsize[reg] = (__gx->suSsize[reg]&~0x10000)|(_SHIFTL(s_enable,16,1));
	__gx->suTsize[reg] = (__gx->suTsize[reg]&~0x10000)|(_SHIFTL(t_enable,16,1));

	if(!(__gx->texCoordManually&(_SHIFTL(1,texcoord,1)))) return;

	GX_LOAD_BP_REG(__gx->suSsize[reg]);
	GX_LOAD_BP_REG(__gx->suTsize[reg]);
}

GXTexRegionCallback GX_SetTexRegionCallback(GXTexRegionCallback cb)
{
	u32 level;
	GXTexRegionCallback ret;

	_CPU_ISR_Disable(level);
	ret = regionCB;
	regionCB = cb;
	_CPU_ISR_Restore(level);

	return ret;
}

GXTlutRegionCallback GX_SetTlutRegionCallback(GXTlutRegionCallback cb)
{
	u32 level;
	GXTlutRegionCallback ret;

	_CPU_ISR_Disable(level);
	ret = tlut_regionCB;
	tlut_regionCB = cb;
	_CPU_ISR_Restore(level);

	return ret;
}

void GX_SetBlendMode(u8 type,u8 src_fact,u8 dst_fact,u8 op)
{
	__gx->peCMode0 = (__gx->peCMode0&~0x1);
	if(type==GX_BM_BLEND || type==GX_BM_SUBTRACT) __gx->peCMode0 |= 0x1;

	__gx->peCMode0 = (__gx->peCMode0&~0x800);
	if(type==GX_BM_SUBTRACT) __gx->peCMode0 |= 0x800;

	__gx->peCMode0 = (__gx->peCMode0&~0x2);
	if(type==GX_BM_LOGIC) __gx->peCMode0 |= 0x2;

	__gx->peCMode0 = (__gx->peCMode0&~0xF000)|(_SHIFTL(op,12,4));
	__gx->peCMode0 = (__gx->peCMode0&~0xE0)|(_SHIFTL(dst_fact,5,3));
	__gx->peCMode0 = (__gx->peCMode0&~0x700)|(_SHIFTL(src_fact,8,3));

	GX_LOAD_BP_REG(__gx->peCMode0);
}

void GX_ClearVtxDesc()
{
	__gx->vcdNrms = 0;
	__gx->vcdClear = ((__gx->vcdClear&~0x0600)|0x0200);
	__gx->vcdLo = __gx->vcdHi = 0;
	__gx->dirtyState |= 0x0008;
}

void GX_SetLineWidth(u8 width,u8 fmt)
{
	__gx->lpWidth = (__gx->lpWidth&~0xff)|(width&0xff);
	__gx->lpWidth = (__gx->lpWidth&~0x70000)|(_SHIFTL(fmt,16,3));
	GX_LOAD_BP_REG(__gx->lpWidth);
}

void GX_SetPointSize(u8 width,u8 fmt)
{
	__gx->lpWidth = (__gx->lpWidth&~0xFF00)|(_SHIFTL(width,8,8));
	__gx->lpWidth = (__gx->lpWidth&~0x380000)|(_SHIFTL(fmt,19,3));
	GX_LOAD_BP_REG(__gx->lpWidth);
}

void GX_SetTevColor(u8 tev_regid,GXColor color)
{
	u32 reg;

	reg = (_SHIFTL((0xe0+(tev_regid<<1)),24,8)|(_SHIFTL(color.a,12,8))|(color.r&0xff));
	GX_LOAD_BP_REG(reg);

	reg = (_SHIFTL((0xe1+(tev_regid<<1)),24,8)|(_SHIFTL(color.g,12,8))|(color.b&0xff));
	GX_LOAD_BP_REG(reg);

	//this two calls should obviously flush the Write Gather Pipe.
	GX_LOAD_BP_REG(reg);
	GX_LOAD_BP_REG(reg);
}

void GX_SetTevColorS10(u8 tev_regid,GXColorS10 color)
{
	u32 reg;

	reg = (_SHIFTL((0xe0+(tev_regid<<1)),24,8)|(_SHIFTL(color.a,12,11))|(color.r&0x7ff));
	GX_LOAD_BP_REG(reg);

	reg = (_SHIFTL((0xe1+(tev_regid<<1)),24,8)|(_SHIFTL(color.g,12,11))|(color.b&0x7ff));
	GX_LOAD_BP_REG(reg);

	//this two calls should obviously flush the Write Gather Pipe.
	GX_LOAD_BP_REG(reg);
	GX_LOAD_BP_REG(reg);
}

void GX_SetTevKColor(u8 tev_kregid,GXColor color)
{
	u32 reg;

	reg = (_SHIFTL((0xe0+(tev_kregid<<1)),24,8)|(_SHIFTL(1,23,1))|(_SHIFTL(color.a,12,8))|(color.r&0xff));
	GX_LOAD_BP_REG(reg);

	reg = (_SHIFTL((0xe1+(tev_kregid<<1)),24,8)|(_SHIFTL(1,23,1))|(_SHIFTL(color.g,12,8))|(color.b&0xff));
	GX_LOAD_BP_REG(reg);

	//this two calls should obviously flush the Write Gather Pipe.
	GX_LOAD_BP_REG(reg);
	GX_LOAD_BP_REG(reg);
}

void GX_SetTevKColorS10(u8 tev_kregid,GXColorS10 color)
{
	u32 reg;

	reg = (_SHIFTL((0xe0+(tev_kregid<<1)),24,8)|(_SHIFTL(1,23,1))|(_SHIFTL(color.a,12,11))|(color.r&0x7ff));
	GX_LOAD_BP_REG(reg);

	reg = (_SHIFTL((0xe1+(tev_kregid<<1)),24,8)|(_SHIFTL(1,23,1))|(_SHIFTL(color.g,12,11))|(color.b&0x7ff));
	GX_LOAD_BP_REG(reg);

	//this two calls should obviously flush the Write Gather Pipe.
	GX_LOAD_BP_REG(reg);
	GX_LOAD_BP_REG(reg);
}

void GX_SetTevOp(u8 tevstage,u8 mode)
{
	u8 defcolor = GX_CC_RASC;
	u8 defalpha = GX_CA_RASA;

	if(tevstage!=GX_TEVSTAGE0) {
		defcolor = GX_CC_CPREV;
		defalpha = GX_CA_APREV;
	}

	switch(mode) {
		case GX_MODULATE:
			GX_SetTevColorIn(tevstage,GX_CC_ZERO,GX_CC_TEXC,defcolor,GX_CC_ZERO);
			GX_SetTevAlphaIn(tevstage,GX_CA_ZERO,GX_CA_TEXA,defalpha,GX_CA_ZERO);
			break;
		case GX_DECAL:
			GX_SetTevColorIn(tevstage,defcolor,GX_CC_TEXC,GX_CC_TEXA,GX_CC_ZERO);
			GX_SetTevAlphaIn(tevstage,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,defalpha);
			break;
		case GX_BLEND:
			GX_SetTevColorIn(tevstage,defcolor,GX_CC_ONE,GX_CC_TEXC,GX_CC_ZERO);
			GX_SetTevAlphaIn(tevstage,GX_CA_ZERO,GX_CA_TEXA,defalpha,GX_CA_RASA);
			break;
		case GX_REPLACE:
			GX_SetTevColorIn(tevstage,GX_CC_ZERO,GX_CC_ZERO,GX_CC_ZERO,GX_CC_TEXC);
			GX_SetTevAlphaIn(tevstage,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_TEXA);
			break;
		case GX_PASSCLR:
			GX_SetTevColorIn(tevstage,GX_CC_ZERO,GX_CC_ZERO,GX_CC_ZERO,defcolor);
			GX_SetTevAlphaIn(tevstage,GX_CC_A2,GX_CC_A2,GX_CC_A2,defalpha);
			break;
	}
	GX_SetTevColorOp(tevstage,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
	GX_SetTevAlphaOp(tevstage,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
}

void GX_SetTevColorIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d)
{
	u32 reg = (tevstage&0xf);
	__gx->tevColorEnv[reg] = (__gx->tevColorEnv[reg]&~0xF000)|(_SHIFTL(a,12,4));
	__gx->tevColorEnv[reg] = (__gx->tevColorEnv[reg]&~0xF00)|(_SHIFTL(b,8,4));
	__gx->tevColorEnv[reg] = (__gx->tevColorEnv[reg]&~0xF0)|(_SHIFTL(c,4,4));
	__gx->tevColorEnv[reg] = (__gx->tevColorEnv[reg]&~0xf)|(d&0xf);

	GX_LOAD_BP_REG(__gx->tevColorEnv[reg]);
}

void GX_SetTevAlphaIn(u8 tevstage,u8 a,u8 b,u8 c,u8 d)
{
	u32 reg = (tevstage&0xf);
	__gx->tevAlphaEnv[reg] = (__gx->tevAlphaEnv[reg]&~0xE000)|(_SHIFTL(a,13,3));
	__gx->tevAlphaEnv[reg] = (__gx->tevAlphaEnv[reg]&~0x1C00)|(_SHIFTL(b,10,3));
	__gx->tevAlphaEnv[reg] = (__gx->tevAlphaEnv[reg]&~0x380)|(_SHIFTL(c,7,3));
	__gx->tevAlphaEnv[reg] = (__gx->tevAlphaEnv[reg]&~0x70)|(_SHIFTL(d,4,3));

	GX_LOAD_BP_REG(__gx->tevAlphaEnv[reg]);
}

void GX_SetTevColorOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid)
{
	/* set tev op add/sub*/
	u32 reg = (tevstage&0xf);
	__gx->tevColorEnv[reg] = (__gx->tevColorEnv[reg]&~0x40000)|(_SHIFTL(tevop,18,1));
	if(tevop<=GX_TEV_SUB) {
		__gx->tevColorEnv[reg] = (__gx->tevColorEnv[reg]&~0x300000)|(_SHIFTL(tevscale,20,2));
		__gx->tevColorEnv[reg] = (__gx->tevColorEnv[reg]&~0x30000)|(_SHIFTL(tevbias,16,2));
	} else {
		__gx->tevColorEnv[reg] = (__gx->tevColorEnv[reg]&~0x300000)|((_SHIFTL(tevop,19,4))&0x300000);
		__gx->tevColorEnv[reg] = (__gx->tevColorEnv[reg]&~0x30000)|0x30000;
	}
	__gx->tevColorEnv[reg] = (__gx->tevColorEnv[reg]&~0x80000)|(_SHIFTL(clamp,19,1));
	__gx->tevColorEnv[reg] = (__gx->tevColorEnv[reg]&~0xC00000)|(_SHIFTL(tevregid,22,2));

	GX_LOAD_BP_REG(__gx->tevColorEnv[reg]);
}

void GX_SetTevAlphaOp(u8 tevstage,u8 tevop,u8 tevbias,u8 tevscale,u8 clamp,u8 tevregid)
{
	/* set tev op add/sub*/
	u32 reg = (tevstage&0xf);
	__gx->tevAlphaEnv[reg] = (__gx->tevAlphaEnv[reg]&~0x40000)|(_SHIFTL(tevop,18,1));
	if(tevop<=GX_TEV_SUB) {
		__gx->tevAlphaEnv[reg] = (__gx->tevAlphaEnv[reg]&~0x300000)|(_SHIFTL(tevscale,20,2));
		__gx->tevAlphaEnv[reg] = (__gx->tevAlphaEnv[reg]&~0x30000)|(_SHIFTL(tevbias,16,2));
	} else {
		__gx->tevAlphaEnv[reg] = (__gx->tevAlphaEnv[reg]&~0x300000)|((_SHIFTL(tevop,19,4))&0x300000);
		__gx->tevAlphaEnv[reg] = (__gx->tevAlphaEnv[reg]&~0x30000)|0x30000;
	}
	__gx->tevAlphaEnv[reg] = (__gx->tevAlphaEnv[reg]&~0x80000)|(_SHIFTL(clamp,19,1));
	__gx->tevAlphaEnv[reg] = (__gx->tevAlphaEnv[reg]&~0xC00000)|(_SHIFTL(tevregid,22,2));

	GX_LOAD_BP_REG(__gx->tevAlphaEnv[reg]);
}

void GX_SetCullMode(u8 mode)
{
    static u8 cm2hw[] = { 0, 2, 1, 3 };

	__gx->genMode = (__gx->genMode&~0xC000)|(_SHIFTL(cm2hw[mode],14,2));
	__gx->dirtyState |= 0x0004;
}

void GX_SetCoPlanar(u8 enable)
{
	__gx->genMode = (__gx->genMode&~0x80000)|(_SHIFTL(enable,19,1));
	GX_LOAD_BP_REG(0xFE080000);
	GX_LOAD_BP_REG(__gx->genMode);
}

void GX_EnableTexOffsets(u8 coord,u8 line_enable,u8 point_enable)
{
	u32 reg = (coord&0x7);
	__gx->suSsize[reg] = (__gx->suSsize[reg]&~0x40000)|(_SHIFTL(line_enable,18,1));
	__gx->suSsize[reg] = (__gx->suSsize[reg]&~0x80000)|(_SHIFTL(point_enable,19,1));
	GX_LOAD_BP_REG(__gx->suSsize[reg]);
}

void GX_SetClipMode(u8 mode)
{
	GX_LOAD_XF_REG(0x1005,(mode&1));
}

void GX_SetScissor(u32 xOrigin,u32 yOrigin,u32 wd,u32 ht)
{
	u32 xo = xOrigin+0x156;
	u32 yo = yOrigin+0x156;
	u32 nwd = xo+(wd-1);
	u32 nht = yo+(ht-1);

	__gx->sciTLcorner = (__gx->sciTLcorner&~0x7ff)|(yo&0x7ff);
	__gx->sciTLcorner = (__gx->sciTLcorner&~0x7FF000)|(_SHIFTL(xo,12,11));

	__gx->sciBRcorner = (__gx->sciBRcorner&~0x7ff)|(nht&0xfff);
	__gx->sciBRcorner = (__gx->sciBRcorner&~0x7FF000)|(_SHIFTL(nwd,12,11));

	GX_LOAD_BP_REG(__gx->sciTLcorner);
	GX_LOAD_BP_REG(__gx->sciBRcorner);
}

void GX_SetScissorBoxOffset(s32 xoffset,s32 yoffset)
{
	s32 xoff = _SHIFTR((xoffset+0x156),1,24);
	s32 yoff = _SHIFTR((yoffset+0x156),1,24);

	GX_LOAD_BP_REG((0x59000000|(_SHIFTL(yoff,10,10))|(xoff&0x3ff)));
}

void GX_SetNumChans(u8 num)
{
	__gx->genMode = (__gx->genMode&~0x70)|(_SHIFTL(num,4,3));
	__gx->dirtyState |= 0x01000004;
}

void GX_SetTevOrder(u8 tevstage,u8 texcoord,u32 texmap,u8 color)
{
	u8 colid;
	u32 texm,texc,tmp;
	u32 reg = 3+(_SHIFTR(tevstage,1,3));

	__gx->tevTexMap[(tevstage&0xf)] = texmap;

	texm = (texmap&~0x100);
	if(texm>=GX_MAX_TEXMAP) texm = 0;
	if(texcoord>=GX_MAXCOORD) {
		texc = 0;
		__gx->tevTexCoordEnable &= ~(_SHIFTL(1,tevstage,1));
	} else {
		texc = texcoord;
		__gx->tevTexCoordEnable |= (_SHIFTL(1,tevstage,1));
	}

	if(tevstage&1) {
		__gx->tevRasOrder[reg] = (__gx->tevRasOrder[reg]&~0x7000)|(_SHIFTL(texm,12,3));
		__gx->tevRasOrder[reg] = (__gx->tevRasOrder[reg]&~0x38000)|(_SHIFTL(texc,15,3));

		colid = GX_ALPHA_BUMP;
		if(color!=GX_COLORNULL) colid = _gxtevcolid[color];
		__gx->tevRasOrder[reg] = (__gx->tevRasOrder[reg]&~0x380000)|(_SHIFTL(colid,19,3));

		tmp = 1;
		if(texmap==GX_TEXMAP_NULL || texmap&0x100) tmp = 0;
		__gx->tevRasOrder[reg] = (__gx->tevRasOrder[reg]&~0x40000)|(_SHIFTL(tmp,18,1));
	} else {
		__gx->tevRasOrder[reg] = (__gx->tevRasOrder[reg]&~0x7)|(texm&0x7);
		__gx->tevRasOrder[reg] = (__gx->tevRasOrder[reg]&~0x38)|(_SHIFTL(texc,3,3));

		colid = GX_ALPHA_BUMP;
		if(color!=GX_COLORNULL) colid = _gxtevcolid[color];
		__gx->tevRasOrder[reg] = (__gx->tevRasOrder[reg]&~0x380)|(_SHIFTL(colid,7,3));

		tmp = 1;
		if(texmap==GX_TEXMAP_NULL || texmap&0x100) tmp = 0;
		__gx->tevRasOrder[reg] = (__gx->tevRasOrder[reg]&~0x40)|(_SHIFTL(tmp,6,1));
	}
	GX_LOAD_BP_REG(__gx->tevRasOrder[reg]);
	__gx->dirtyState |= 0x0001;
}

void GX_SetNumTevStages(u8 num)
{
	__gx->genMode = (__gx->genMode&~0x3C00)|(_SHIFTL((num-1),10,4));
	__gx->dirtyState |= 0x0004;
}

void GX_SetAlphaCompare(u8 comp0,u8 ref0,u8 aop,u8 comp1,u8 ref1)
{
	u32 val = 0;
	val = (_SHIFTL(aop,22,2))|(_SHIFTL(comp1,19,3))|(_SHIFTL(comp0,16,3))|(_SHIFTL(ref1,8,8))|(ref0&0xff);
	GX_LOAD_BP_REG(0xf3000000|val);
}

void GX_SetTevKColorSel(u8 tevstage,u8 sel)
{
	u32 reg = (_SHIFTR(tevstage,1,3));

	if(tevstage&1)
		__gx->tevSwapModeTable[reg] = (__gx->tevSwapModeTable[reg]&~0x7C000)|(_SHIFTL(sel,14,5));
	else
		__gx->tevSwapModeTable[reg] = (__gx->tevSwapModeTable[reg]&~0x1F0)|(_SHIFTL(sel,4,5));
	GX_LOAD_BP_REG(__gx->tevSwapModeTable[reg]);
}

void GX_SetTevKAlphaSel(u8 tevstage,u8 sel)
{
	u32 reg = (_SHIFTR(tevstage,1,3));

	if(tevstage&1)
		__gx->tevSwapModeTable[reg] = (__gx->tevSwapModeTable[reg]&~0xF80000)|(_SHIFTL(sel,19,5));
	else
		__gx->tevSwapModeTable[reg] = (__gx->tevSwapModeTable[reg]&~0x3E00)|(_SHIFTL(sel,9,5));
	GX_LOAD_BP_REG(__gx->tevSwapModeTable[reg]);
}

void GX_SetTevSwapMode(u8 tevstage,u8 ras_sel,u8 tex_sel)
{
	u32 reg = (tevstage&0xf);
	__gx->tevAlphaEnv[reg] = (__gx->tevAlphaEnv[reg]&~0x3)|(ras_sel&0x3);
	__gx->tevAlphaEnv[reg] = (__gx->tevAlphaEnv[reg]&~0xC)|(_SHIFTL(tex_sel,2,2));
	GX_LOAD_BP_REG(__gx->tevAlphaEnv[reg]);
}

void GX_SetTevSwapModeTable(u8 swapid,u8 r,u8 g,u8 b,u8 a)
{
	u32 regA = 0+(_SHIFTL(swapid,1,3));
	u32 regB = 1+(_SHIFTL(swapid,1,3));

	__gx->tevSwapModeTable[regA] = (__gx->tevSwapModeTable[regA]&~0x3)|(r&0x3);
	__gx->tevSwapModeTable[regA] = (__gx->tevSwapModeTable[regA]&~0xC)|(_SHIFTL(g,2,2));
	GX_LOAD_BP_REG(__gx->tevSwapModeTable[regA]);

	__gx->tevSwapModeTable[regB] = (__gx->tevSwapModeTable[regB]&~0x3)|(b&0x3);
	__gx->tevSwapModeTable[regB] = (__gx->tevSwapModeTable[regB]&~0xC)|(_SHIFTL(a,2,2));
	GX_LOAD_BP_REG(__gx->tevSwapModeTable[regB]);
}

void GX_SetTevIndirect(u8 tevstage,u8 indtexid,u8 format,u8 bias,u8 mtxid,u8 wrap_s,u8 wrap_t,u8 addprev,u8 utclod,u8 a)
{
	u32 val = (0x10000000|(_SHIFTL(tevstage,24,4)))|(indtexid&3)|(_SHIFTL(format,2,2))|(_SHIFTL(bias,4,3))|(_SHIFTL(a,7,2))|(_SHIFTL(mtxid,9,4))|(_SHIFTL(wrap_s,13,3))|(_SHIFTL(wrap_t,16,3))|(_SHIFTL(utclod,19,1))|(_SHIFTL(addprev,20,1));
	GX_LOAD_BP_REG(val);
}

void GX_SetTevDirect(u8 tevstage)
{
	GX_SetTevIndirect(tevstage,GX_INDTEXSTAGE0,GX_ITF_8,GX_ITB_NONE,GX_ITM_OFF,GX_ITW_OFF,GX_ITW_OFF,GX_FALSE,GX_FALSE,GX_ITBA_OFF);
}

void GX_SetNumIndStages(u8 nstages)
{
	__gx->genMode = (__gx->genMode&~0x70000)|(_SHIFTL(nstages,16,3));
	__gx->dirtyState |= 0x0006;
}

void GX_SetIndTexMatrix(u8 indtexmtx,f32 offset_mtx[2][3],s8 scale_exp)
{
	u32 ma,mb;
	u32 val,s,idx;

	if(indtexmtx>0x00 && indtexmtx<0x04) indtexmtx -= 0x01;
	else if(indtexmtx>0x04 && indtexmtx<0x08) indtexmtx -= 0x05;
	else if(indtexmtx>0x08 && indtexmtx<0x0C) indtexmtx -= 0x09;
	else indtexmtx = 0x00;

	s = (scale_exp+17);
	idx = ((indtexmtx<<2)-indtexmtx);

	ma = (u32)(offset_mtx[0][0]*1024.0F);
	mb = (u32)(offset_mtx[1][0]*1024.0F);
	val = (_SHIFTL((0x06+idx),24,8)|_SHIFTL(s,22,2)|_SHIFTL(mb,11,11)|_SHIFTL(ma,0,11));
	GX_LOAD_BP_REG(val);

	ma = (u32)(offset_mtx[0][1]*1024.0F);
	mb = (u32)(offset_mtx[1][1]*1024.0F);
	val = (_SHIFTL((0x07+idx),24,8)|_SHIFTL((s>>2),22,2)|_SHIFTL(mb,11,11)|_SHIFTL(ma,0,11));
	GX_LOAD_BP_REG(val);

	ma = (u32)(offset_mtx[0][2]*1024.0F);
	mb = (u32)(offset_mtx[1][2]*1024.0F);
	val = (_SHIFTL((0x08+idx),24,8)|_SHIFTL((s>>4),22,2)|_SHIFTL(mb,11,11)|_SHIFTL(ma,0,11));
	GX_LOAD_BP_REG(val);
}

void GX_SetTevIndBumpST(u8 tevstage,u8 indstage,u8 mtx_sel)
{
	u8 sel_s,sel_t;

	switch(mtx_sel) {
		case GX_ITM_0:
			sel_s = GX_ITM_S0;
			sel_t = GX_ITM_T0;
			break;
		case GX_ITM_1:
			sel_s = GX_ITM_S1;
			sel_t = GX_ITM_T1;
			break;
		case GX_ITM_2:
			sel_s = GX_ITM_S2;
			sel_t = GX_ITM_T2;
			break;
		default:
			sel_s = GX_ITM_OFF;
			sel_t = GX_ITM_OFF;
			break;
	}

	GX_SetTevIndirect((tevstage+0),indstage,GX_ITF_8,GX_ITB_ST,sel_s,GX_ITW_0,GX_ITW_0,GX_FALSE,GX_FALSE,GX_ITBA_OFF);
	GX_SetTevIndirect((tevstage+1),indstage,GX_ITF_8,GX_ITB_ST,sel_t,GX_ITW_0,GX_ITW_0,GX_TRUE,GX_FALSE,GX_ITBA_OFF);
	GX_SetTevIndirect((tevstage+2),indstage,GX_ITF_8,GX_ITB_NONE,GX_ITM_OFF,GX_ITW_OFF,GX_ITW_OFF,GX_TRUE,GX_FALSE,GX_ITBA_OFF);
}

void GX_SetTevIndBumpXYZ(u8 tevstage,u8 indstage,u8 mtx_sel)
{
	GX_SetTevIndirect(tevstage,indstage,GX_ITF_8,GX_ITB_STU,mtx_sel,GX_ITW_OFF,GX_ITW_OFF,GX_FALSE,GX_FALSE,GX_ITBA_OFF);
}

void GX_SetTevIndRepeat(u8 tevstage)
{
	GX_SetTevIndirect(tevstage,GX_INDTEXSTAGE0,GX_ITF_8,GX_ITB_NONE,GX_ITM_OFF,GX_ITW_0,GX_ITW_0,GX_TRUE,GX_FALSE,GX_ITBA_OFF);
}

void GX_SetIndTexCoordScale(u8 indtexid,u8 scale_s,u8 scale_t)
{
	switch(indtexid) {
		case GX_INDTEXSTAGE0:
			__gx->tevRasOrder[0] = (__gx->tevRasOrder[0]&~0x0f)|(scale_s&0x0f);
			__gx->tevRasOrder[0] = (__gx->tevRasOrder[0]&~0xF0)|(_SHIFTL(scale_t,4,4));
			GX_LOAD_BP_REG(__gx->tevRasOrder[0]);
			break;
		case GX_INDTEXSTAGE1:
			__gx->tevRasOrder[0] = (__gx->tevRasOrder[0]&~0xF00)|(_SHIFTL(scale_s,8,4));
			__gx->tevRasOrder[0] = (__gx->tevRasOrder[0]&~0xF000)|(_SHIFTL(scale_t,12,4));
			GX_LOAD_BP_REG(__gx->tevRasOrder[0]);
			break;
		case GX_INDTEXSTAGE2:
			__gx->tevRasOrder[1] = (__gx->tevRasOrder[1]&~0x0f)|(scale_s&0x0f);
			__gx->tevRasOrder[1] = (__gx->tevRasOrder[1]&~0xF0)|(_SHIFTL(scale_t,4,4));
			GX_LOAD_BP_REG(__gx->tevRasOrder[1]);
			break;
		case GX_INDTEXSTAGE3:
			__gx->tevRasOrder[1] = (__gx->tevRasOrder[1]&~0xF00)|(_SHIFTL(scale_s,8,4));
			__gx->tevRasOrder[1] = (__gx->tevRasOrder[1]&~0xF000)|(_SHIFTL(scale_t,12,4));
			GX_LOAD_BP_REG(__gx->tevRasOrder[1]);
			break;
	}
}

void GX_SetTevIndTile(u8 tevstage,u8 indtexid,u16 tilesize_x,u16 tilesize_y,u16 tilespacing_x,u16 tilespacing_y,u8 indtexfmt,u8 indtexmtx,u8 bias_sel,u8 alpha_sel)
{
	s32 wrap_s,wrap_t;
	f32 offset_mtx[2][3];
	f64 fdspace_x,fdspace_y;
	u32 fbuf_x[2] = { 0x43300000,tilespacing_x };
	u32 fbuf_y[2] = { 0x43300000,tilespacing_y };

	wrap_s = GX_ITW_OFF;
	if(tilesize_x==0x0010) wrap_s = GX_ITW_16;
	else if(tilesize_x==0x0020) wrap_s = GX_ITW_32;
	else if(tilesize_x==0x0040) wrap_s = GX_ITW_64;
	else if(tilesize_x==0x0080) wrap_s = GX_ITW_128;
	else if(tilesize_x==0x0100) wrap_s = GX_ITW_256;

	wrap_t = GX_ITW_OFF;
	if(tilesize_y==0x0010) wrap_t = GX_ITW_16;
	else if(tilesize_y==0x0020) wrap_t = GX_ITW_32;
	else if(tilesize_y==0x0040) wrap_t = GX_ITW_64;
	else if(tilesize_y==0x0080) wrap_t = GX_ITW_128;
	else if(tilesize_y==0x0100) wrap_t = GX_ITW_256;

	fdspace_x = *(f64*)((void*)fbuf_x);
	fdspace_y = *(f64*)((void*)fbuf_y);

	offset_mtx[0][0] = (f32)((fdspace_x - 4503599627370496.0F)*0.00097656250F);
	offset_mtx[0][1] = 0.0F;
	offset_mtx[0][2] = 0.0F;
	offset_mtx[1][0] = 0.0F;
	offset_mtx[1][1] = (f32)((fdspace_y - 4503599627370496.0F)*0.00097656250F);
	offset_mtx[1][2] = 0.0F;

	GX_SetIndTexMatrix(indtexmtx,offset_mtx,10);
	GX_SetTevIndirect(tevstage,indtexid,indtexfmt,bias_sel,indtexmtx,wrap_s,wrap_t,GX_FALSE,GX_TRUE,alpha_sel);
}

void GX_SetFog(u8 type,f32 startz,f32 endz,f32 nearz,f32 farz,GXColor col)
{
    f32 A, B, B_mant, C, A_f;
    u32 b_expn, b_m, a_hex, c_hex,val,proj = 0;
	union ieee32 { f32 f; u32 i; } v;

  	proj = _SHIFTR(type,3,1);

	// Calculate constants a, b, and c (TEV HW requirements).
	if(proj) { // Orthographic Fog Type
		if((farz==nearz) || (endz==startz)) {
			// take care of the odd-ball case.
			A_f = 0.0f;
			C = 0.0f;
		} else {
			A = 1.0f/(endz-startz);
			A_f = (farz-nearz) * A;
			C = (startz-nearz) * A;
		}

		b_expn	= 0;
		b_m		= 0;
	} else { // Perspective Fog Type
	  // Calculate constants a, b, and c (TEV HW requirements).
		if((farz==nearz) || (endz==startz)) {
			// take care of the odd-ball case.
			A = 0.0f;
			B = 0.5f;
			C = 0.0f;
		} else {
			A = (farz*nearz)/((farz-nearz)*(endz-startz));
			B = farz/(farz-nearz);
			C = startz/(endz-startz);
		}

		B_mant = B;
		b_expn = 1;
		while(B_mant>1.0f) {
			B_mant /= 2.0f;
			b_expn++;
		}

		while((B_mant>0.0f) && (B_mant<0.5f)) {
			B_mant *= 2.0f;
			b_expn--;
		}

		A_f   = A/(1<<(b_expn));
		b_m   = (u32)(B_mant * 8388638.0f);
	}
	v.f = A_f;
    a_hex = v.i;

	v.f = C;
    c_hex = v.i;

	val = 0xee000000|(_SHIFTR(a_hex,12,20));
	GX_LOAD_BP_REG(val);

	val = 0xef000000|(b_m&0x00ffffff);
	GX_LOAD_BP_REG(val);

	val = 0xf0000000|(b_expn&0x1f);
	GX_LOAD_BP_REG(val);

	val = 0xf1000000|(_SHIFTL(type,21,3))|(_SHIFTL(proj,20,1))|(_SHIFTR(c_hex,12,20));
	GX_LOAD_BP_REG(val);

	val = 0xf2000000|(_SHIFTL(col.r,16,8))|(_SHIFTL(col.g,8,8))|(col.b&0xff);
	GX_LOAD_BP_REG(val);
}

void GX_InitFogAdjTable(GXFogAdjTbl *table,u16 width,f32 projmtx[4][4])
{
	u32 i,val7;
	f32 val0,val1,val2,val4,val5,val6;

	if(projmtx[3][3]==0.0f) {
		val0 = projmtx[2][3]/(projmtx[2][2] - 1.0f);
		val1 = val0/projmtx[0][0];
	} else {
		val1 = 1.0f/projmtx[0][0];
		val0 = val1*1.7320499f;
	}

	val2 = val0*val0;
	val4 = 2.0f/(f32)width;
	for(i=0;i<10;i++) {
		val5 = (i+1)*32.0f;
		val5 *= val4;
		val5 *= val1;
		val5 *= val5;
		val5 /= val2;
		val6 = sqrtf(val5 + 1.0f);
		val7 = (u32)(val6*256.0f);
		table->r[i] = (val7&0x0fff);
	}
}

void GX_SetFogRangeAdj(u8 enable,u16 center,GXFogAdjTbl *table)
{
	u32 val;

	if(enable) {
		val = 0xe9000000|(_SHIFTL(table->r[1],12,12))|(table->r[0]&0x0fff);
		GX_LOAD_BP_REG(val);

		val = 0xea000000|(_SHIFTL(table->r[3],12,12))|(table->r[2]&0x0fff);
		GX_LOAD_BP_REG(val);

		val = 0xeb000000|(_SHIFTL(table->r[5],12,12))|(table->r[4]&0x0fff);
		GX_LOAD_BP_REG(val);

		val = 0xec000000|(_SHIFTL(table->r[7],12,12))|(table->r[6]&0x0fff);
		GX_LOAD_BP_REG(val);

		val = 0xed000000|(_SHIFTL(table->r[9],12,12))|(table->r[8]&0x0fff);
		GX_LOAD_BP_REG(val);
	}
	val = 0xe8000000|(_SHIFTL(enable,10,1))|((center + 342)&0x03ff);
	GX_LOAD_BP_REG(val);
}

void GX_SetFogColor(GXColor color)
{
	GX_LOAD_BP_REG(0xf2000000|(_SHIFTL(color.r,16,8)|_SHIFTL(color.g,8,8)|(color.b&0xff)));
}

void GX_SetColorUpdate(u8 enable)
{
	__gx->peCMode0 = (__gx->peCMode0&~0x8)|(_SHIFTL(enable,3,1));
	GX_LOAD_BP_REG(__gx->peCMode0);
}

void GX_SetAlphaUpdate(u8 enable)
{
	__gx->peCMode0 = (__gx->peCMode0&~0x10)|(_SHIFTL(enable,4,1));
	GX_LOAD_BP_REG(__gx->peCMode0);
}

void GX_SetZCompLoc(u8 before_tex)
{
	__gx->peCntrl = (__gx->peCntrl&~0x40)|(_SHIFTL(before_tex,6,1));
	GX_LOAD_BP_REG(__gx->peCntrl);
}

void GX_SetPixelFmt(u8 pix_fmt,u8 z_fmt)
{
	u8 ms_en = 0;
	u32 realfmt[8] = {0,1,2,3,4,4,4,5};

	__gx->peCntrl = (__gx->peCntrl&~0x7)|(realfmt[pix_fmt]&0x7);
	__gx->peCntrl = (__gx->peCntrl&~0x38)|(_SHIFTL(z_fmt,3,3));
	GX_LOAD_BP_REG(__gx->peCntrl);
	__gx->dirtyState |= 0x0004;

	if(pix_fmt==GX_PF_RGB565_Z16) ms_en = 1;
	__gx->genMode = (__gx->genMode&~0x200)|(_SHIFTL(ms_en,9,1));

	if(realfmt[pix_fmt]==GX_PF_Y8) {
		pix_fmt -= GX_PF_Y8;
		__gx->peCMode1 = (__gx->peCMode1&~0xC00)|(_SHIFTL(pix_fmt,10,2));
		GX_LOAD_BP_REG(__gx->peCMode1);
	}
}

void GX_SetDither(u8 dither)
{
	__gx->peCMode0 = (__gx->peCMode0&~0x4)|(_SHIFTL(dither,2,1));
	GX_LOAD_BP_REG(__gx->peCMode0);
}

void GX_SetDstAlpha(u8 enable,u8 a)
{
	__gx->peCMode1 = (__gx->peCMode1&~0xff)|(a&0xff);
	__gx->peCMode1 = (__gx->peCMode1&~0x100)|(_SHIFTL(enable,8,1));
	GX_LOAD_BP_REG(__gx->peCMode1);
}

void GX_SetFieldMask(u8 even_mask,u8 odd_mask)
{
	u32 val = 0;

	val = (_SHIFTL(even_mask,1,1))|(odd_mask&1);
	GX_LOAD_BP_REG(0x44000000|val);
}

void GX_SetFieldMode(u8 field_mode,u8 half_aspect_ratio)
{
	__gx->lpWidth = (__gx->lpWidth&~0x400000)|(_SHIFTL(half_aspect_ratio,22,1));
	GX_LOAD_BP_REG(__gx->lpWidth);

	__GX_FlushTextureState();
	GX_LOAD_BP_REG(0x68000000|(field_mode&1));
	__GX_FlushTextureState();
}

void GX_PokeAlphaMode(u8 func,u8 threshold)
{
	_peReg[3] = (_SHIFTL(func,8,8))|(threshold&0xFF);
}

void GX_PokeAlphaRead(u8 mode)
{
	_peReg[4] = (mode&~0x4)|0x4;
}

void GX_PokeDstAlpha(u8 enable,u8 a)
{
	_peReg[2] = (_SHIFTL(enable,8,1))|(a&0xff);
}

void GX_PokeAlphaUpdate(u8 update_enable)
{
	_peReg[1] = (_peReg[1]&~0x10)|(_SHIFTL(update_enable,4,1));
}

void GX_PokeColorUpdate(u8 update_enable)
{
	_peReg[1] = (_peReg[1]&~0x8)|(_SHIFTL(update_enable,3,1));
}

void GX_PokeDither(u8 dither)
{
	_peReg[1] = (_peReg[1]&~0x4)|(_SHIFTL(dither,2,1));
}

void GX_PokeBlendMode(u8 type,u8 src_fact,u8 dst_fact,u8 op)
{
	u32 regval = _peReg[1];

	regval = (regval&~0x1);
	if(type==GX_BM_BLEND || type==GX_BM_SUBTRACT) regval |= 0x1;

	regval = (regval&~0x800);
	if(type==GX_BM_SUBTRACT) regval |= 0x800;

	regval = (regval&~0x2);
	if(type==GX_BM_LOGIC) regval |= 0x2;

	regval = (regval&~0xF000)|(_SHIFTL(op,12,4));
	regval = (regval&~0xE0)|(_SHIFTL(dst_fact,5,3));
	regval = (regval&~0x700)|(_SHIFTL(src_fact,8,3));

	regval |= 0x41000000;
	_peReg[1] = (u16)regval;
}

void GX_PokeARGB(u16 x,u16 y,GXColor color)
{
	u32 regval;

	regval = 0xc8000000|(_SHIFTL(x,2,10));
	regval = (regval&~0x3FF000)|(_SHIFTL(y,12,10));
	*(u32*)regval = _SHIFTL(color.a,24,8)|_SHIFTL(color.r,16,8)|_SHIFTL(color.g,8,8)|(color.b&0xff);
}

void GX_PeekARGB(u16 x,u16 y,GXColor *color)
{
	u32 regval,val;

	regval = 0xc8000000|(_SHIFTL(x,2,10));
	regval = (regval&~0x3FF000)|(_SHIFTL(y,12,10));
	val = *(u32*)regval;
	color->a = _SHIFTR(val,24,8);
	color->r = _SHIFTR(val,16,8);
	color->g = _SHIFTR(val,8,8);
	color->b = val&0xff;
}

void GX_PokeZ(u16 x,u16 y,u32 z)
{
	u32 regval;

	regval = 0xc8000000|(_SHIFTL(x,2,10));
	regval = (regval&~0x3FF000)|(_SHIFTL(y,12,10));
	regval = (regval&~0xC00000)|0x400000;
	*(u32*)regval = z;
}

void GX_PeekZ(u16 x,u16 y,u32 *z)
{
	u32 regval;

	regval = 0xc8000000|(_SHIFTL(x,2,10));
	regval = (regval&~0x3FF000)|(_SHIFTL(y,12,10));
	regval = (regval&~0xC00000)|0x400000;
	*z = *(u32*)regval;
}

void GX_PokeZMode(u8 comp_enable,u8 func,u8 update_enable)
{
	u16 regval;
	regval = comp_enable&0x1;
	regval = (regval&~0xE)|(_SHIFTL(func,1,3));
	regval = (regval&0x10)|(_SHIFTL(update_enable,4,1));
	_peReg[0] = regval;
}

void GX_SetIndTexOrder(u8 indtexstage,u8 texcoord,u8 texmap)
{
	switch(indtexstage) {
		case GX_INDTEXSTAGE0:
			__gx->tevRasOrder[2] = (__gx->tevRasOrder[2]&~0x7)|(texmap&0x7);
			__gx->tevRasOrder[2] = (__gx->tevRasOrder[2]&~0x38)|(_SHIFTL(texcoord,3,3));
			break;
		case GX_INDTEXSTAGE1:
			__gx->tevRasOrder[2] = (__gx->tevRasOrder[2]&~0x1C0)|(_SHIFTL(texmap,6,3));
			__gx->tevRasOrder[2] = (__gx->tevRasOrder[2]&~0xE00)|(_SHIFTL(texcoord,9,3));
			break;
		case GX_INDTEXSTAGE2:
			__gx->tevRasOrder[2] = (__gx->tevRasOrder[2]&~0x7000)|(_SHIFTL(texmap,12,3));
			__gx->tevRasOrder[2] = (__gx->tevRasOrder[2]&~0x38000)|(_SHIFTL(texcoord,15,3));
			break;
		case GX_INDTEXSTAGE3:
			__gx->tevRasOrder[2] = (__gx->tevRasOrder[2]&~0x1C0000)|(_SHIFTL(texmap,18,3));
			__gx->tevRasOrder[2] = (__gx->tevRasOrder[2]&~0xE00000)|(_SHIFTL(texcoord,21,3));
			break;
	}
	GX_LOAD_BP_REG(__gx->tevRasOrder[2]);
	__gx->dirtyState |= 0x0003;
}

void GX_InitLightPos(GXLightObj *lit_obj,f32 x,f32 y,f32 z)
{
	struct __gx_litobj *lit = (struct __gx_litobj*)lit_obj;

	lit->px = x;
	lit->py = y;
	lit->pz = z;
}

void GX_InitLightColor(GXLightObj *lit_obj,GXColor col)
{
	struct __gx_litobj *lit = (struct __gx_litobj*)lit_obj;
	lit->col = ((_SHIFTL(col.r,24,8))|(_SHIFTL(col.g,16,8))|(_SHIFTL(col.b,8,8))|(col.a&0xff));
}

void GX_LoadLightObj(GXLightObj *lit_obj,u8 lit_id)
{
	u32 id;
	u16 reg;
	struct __gx_litobj *lit = (struct __gx_litobj*)lit_obj;

	switch(lit_id) {
		case GX_LIGHT0:
			id = 0;
			break;
		case GX_LIGHT1:
			id = 1;
			break;
		case GX_LIGHT2:
			id = 2;
			break;
		case GX_LIGHT3:
			id = 3;
			break;
		case GX_LIGHT4:
			id = 4;
			break;
		case GX_LIGHT5:
			id = 5;
			break;
		case GX_LIGHT6:
			id = 6;
			break;
		case GX_LIGHT7:
			id = 7;
			break;
		default:
			id = 0;
			break;
	}

	reg = 0x600|(_SHIFTL(id,4,8));
	GX_LOAD_XF_REGS(reg,16);
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = 0;
	wgPipe->U32 = lit->col;
	wgPipe->F32 = lit->a0;
	wgPipe->F32 = lit->a1;
	wgPipe->F32 = lit->a2;
	wgPipe->F32 = lit->k0;
	wgPipe->F32 = lit->k1;
	wgPipe->F32 = lit->k2;
	wgPipe->F32 = lit->px;
	wgPipe->F32 = lit->py;
	wgPipe->F32 = lit->pz;
	wgPipe->F32 = lit->nx;
	wgPipe->F32 = lit->ny;
	wgPipe->F32 = lit->nz;
}

void GX_LoadLightObjIdx(u32 litobjidx,u8 litid)
{
	u32 reg;
	u32 idx = 0;

	switch(litid) {
		case GX_LIGHT0:
			idx = 0;
			break;
		case GX_LIGHT1:
			idx = 1;
			break;
		case GX_LIGHT2:
			idx = 2;
			break;
		case GX_LIGHT3:
			idx = 3;
			break;
		case GX_LIGHT4:
			idx = 4;
			break;
		case GX_LIGHT5:
			idx = 5;
			break;
		case GX_LIGHT6:
			idx = 6;
			break;
		case GX_LIGHT7:
			idx = 7;
			break;
		default:
			idx = 0;
			break;

	}

	reg = 0xf600|(_SHIFTL(idx,4,8));
	reg = (reg&~0xffff0000)|(_SHIFTL(litobjidx,16,16));

	wgPipe->U8 = 0x38;
	wgPipe->U32 = reg;
}

void GX_InitLightDir(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz)
{
	struct __gx_litobj *lit = (struct __gx_litobj*)lit_obj;

	lit->nx = -(nx);
	lit->ny = -(ny);
	lit->nz = -(nz);
}

void GX_InitLightDistAttn(GXLightObj *lit_obj,f32 ref_dist,f32 ref_brite,u8 dist_fn)
{
	f32 k0,k1,k2;
	struct __gx_litobj *lit = (struct __gx_litobj*)lit_obj;

	if(ref_dist<0.0f ||
		ref_brite<0.0f || ref_brite>=1.0f) dist_fn = GX_DA_OFF;

	switch(dist_fn) {
		case GX_DA_GENTLE:
			k0 = 1.0f;
			k1 = (1.0f-ref_brite)/(ref_brite*ref_dist);
			k2 = 0.0f;
			break;
		case GX_DA_MEDIUM:
			k0 = 1.0f;
			k1 = 0.5f*(1.0f-ref_brite)/(ref_brite*ref_dist);
			k2 = 0.5f*(1.0f-ref_brite)/(ref_brite*ref_dist*ref_dist);
			break;
		case GX_DA_STEEP:
			k0 = 1.0f;
			k1 = 0.0f;
			k2 = (1.0f-ref_brite)/(ref_brite*ref_dist*ref_dist);
			break;
		case GX_DA_OFF:
		default:
			k0 = 1.0f;
			k1 = 0.0f;
			k2 = 0.0f;
			break;
	}

	lit->k0 = k0;
	lit->k1 = k1;
	lit->k2 = k2;
}

void GX_InitLightAttn(GXLightObj *lit_obj,f32 a0,f32 a1,f32 a2,f32 k0,f32 k1,f32 k2)
{
	struct __gx_litobj *lit = (struct __gx_litobj*)lit_obj;

	lit->a0 = a0;
	lit->a1 = a1;
	lit->a2 = a2;
	lit->k0 = k0;
	lit->k1 = k1;
	lit->k2 = k2;
}

void GX_InitLightAttnA(GXLightObj *lit_obj,f32 a0,f32 a1,f32 a2)
{
	struct __gx_litobj *lit = (struct __gx_litobj*)lit_obj;

	lit->a0 = a0;
	lit->a1 = a1;
	lit->a2 = a2;
}

void GX_InitLightAttnK(GXLightObj *lit_obj,f32 k0,f32 k1,f32 k2)
{
	struct __gx_litobj *lit = (struct __gx_litobj*)lit_obj;

	lit->k0 = k0;
	lit->k1 = k1;
	lit->k2 = k2;
}

void GX_InitSpecularDirHA(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz,f32 hx,f32 hy,f32 hz)
{
    f32 px, py, pz;
	struct __gx_litobj *lit = (struct __gx_litobj*)lit_obj;

    px = (nx * LARGE_NUMBER);
    py = (ny * LARGE_NUMBER);
    pz = (nz * LARGE_NUMBER);

	lit->px = px;
	lit->py = py;
	lit->pz = pz;
	lit->nx = hx;
	lit->ny = hy;
	lit->nz = hz;
}

void GX_InitSpecularDir(GXLightObj *lit_obj,f32 nx,f32 ny,f32 nz)
{
    f32 px, py, pz;
    f32 hx, hy, hz, mag;
	struct __gx_litobj *lit = (struct __gx_litobj*)lit_obj;

    // Compute half-angle vector
    hx  = -nx;
    hy  = -ny;
    hz  = (-nz + 1.0f);
	mag = ((hx * hx) + (hy * hy) + (hz * hz));
	if(mag!=0.0f) mag = 1.0f / sqrtf(mag);

    hx *= mag;
    hy *= mag;
    hz *= mag;

    px  = (nx * LARGE_NUMBER);
    py  = (ny * LARGE_NUMBER);
    pz  = (nz * LARGE_NUMBER);

	lit->px = px;
	lit->py = py;
	lit->pz = pz;
	lit->nx = hx;
	lit->ny = hy;
	lit->nz = hz;
}

void GX_InitLightSpot(GXLightObj *lit_obj,f32 cut_off,u8 spotfn)
{
	f32 r,d,cr,a0,a1,a2;
	struct __gx_litobj *lit = (struct __gx_litobj*)lit_obj;

	if(cut_off<0.0f ||	cut_off>90.0f) spotfn = GX_SP_OFF;

	r = (cut_off*M_PI)/180.0f;
	cr = cosf(r);

	switch(spotfn) {
		case GX_SP_FLAT:
			a0 = -1000.0f*cr;
			a1 = 1000.0f;
			a2 = 0.0f;
			break;
		case GX_SP_COS:
			a0 = -cr/(1.0f-cr);
			a1 = 1.0f/(1.0f-cr);
			a2 = 0.0f;
			break;
		case GX_SP_COS2:
			a0 = 0.0f;
			a1 = -cr/(1.0f-cr);
			a2 = 1.0f/(1.0f-cr);
			break;
		case GX_SP_SHARP:
			d = (1.0f-cr)*(1.0f-cr);
			a0 = cr*(cr-2.0f);
			a1 = 2.0f/d;
			a2 = -1.0/d;
			break;
		case GX_SP_RING1:
			d = (1.0f-cr)*(1.0f-cr);
			a0 = -4.0f*cr/d;
			a1 = 4.0f*(1.0f+cr)/d;
			a2 = -4.0f/d;
			break;
		case GX_SP_RING2:
			d = (1.0f-cr)*(1.0f-cr);
			a0 = 1.0f-2.0f*cr*cr/d;
			a1 = 4.0f*cr/d;
			a2 = -2.0f/d;
			break;
		case GX_SP_OFF:
		default:
			a0 = 1.0f;
			a1 = 0.0f;
			a2 = 0.0f;
			break;
	}

	lit->a0 = a0;
	lit->a1 = a1;
	lit->a2 = a2;
}

void GX_SetGPMetric(u32 perf0,u32 perf1)
{
	// check last setted perf0 counters
	if(__gx->perf0Mode>=GX_PERF0_TRIANGLES && __gx->perf0Mode<GX_PERF0_QUAD_0CVG)
		GX_LOAD_BP_REG(0x23000000);
	else if(__gx->perf0Mode>=GX_PERF0_QUAD_0CVG && __gx->perf0Mode<GX_PERF0_CLOCKS)
		GX_LOAD_BP_REG(0x24000000);
	else if(__gx->perf0Mode>=GX_PERF0_VERTICES && __gx->perf0Mode<=GX_PERF0_CLOCKS)
		GX_LOAD_XF_REG(0x1006,0);

	// check last setted perf1 counters
	if(__gx->perf1Mode>=GX_PERF1_VC_ELEMQ_FULL && __gx->perf1Mode<GX_PERF1_FIFO_REQ) {
		__gx->cpPerfMode = (__gx->cpPerfMode&~0xf0);
		GX_LOAD_CP_REG(0x20,__gx->cpPerfMode);
	} else if(__gx->perf1Mode>=GX_PERF1_FIFO_REQ && __gx->perf1Mode<GX_PERF1_CLOCKS) {
		_cpReg[3] = 0;
	} else if(__gx->perf1Mode>=GX_PERF1_TEXELS && __gx->perf1Mode<=GX_PERF1_CLOCKS) {
		GX_LOAD_BP_REG(0x67000000);
	}

	__gx->perf0Mode = perf0;
	switch(__gx->perf0Mode) {
		case GX_PERF0_CLOCKS:
			GX_LOAD_XF_REG(0x1006,0x00000273);
			break;
		case GX_PERF0_VERTICES:
			GX_LOAD_XF_REG(0x1006,0x0000014a);
			break;
		case GX_PERF0_CLIP_VTX:
			GX_LOAD_XF_REG(0x1006,0x0000016b);
			break;
		case GX_PERF0_CLIP_CLKS:
			GX_LOAD_XF_REG(0x1006,0x00000084);
			break;
		case GX_PERF0_XF_WAIT_IN:
			GX_LOAD_XF_REG(0x1006,0x000000c6);
			break;
		case GX_PERF0_XF_WAIT_OUT:
			GX_LOAD_XF_REG(0x1006,0x00000210);
			break;
		case GX_PERF0_XF_XFRM_CLKS:
			GX_LOAD_XF_REG(0x1006,0x00000252);
			break;
		case GX_PERF0_XF_LIT_CLKS:
			GX_LOAD_XF_REG(0x1006,0x00000231);
			break;
		case GX_PERF0_XF_BOT_CLKS:
			GX_LOAD_XF_REG(0x1006,0x000001ad);
			break;
		case GX_PERF0_XF_REGLD_CLKS:
			GX_LOAD_XF_REG(0x1006,0x000001ce);
			break;
		case GX_PERF0_XF_REGRD_CLKS:
			GX_LOAD_XF_REG(0x1006,0x00000021);
			break;
		case GX_PERF0_CLIP_RATIO:
			GX_LOAD_XF_REG(0x1006,0x00000153);
			break;
		case GX_PERF0_TRIANGLES:
			GX_LOAD_BP_REG(0x2300AE7F);
			break;
		case GX_PERF0_TRIANGLES_CULLED:
			GX_LOAD_BP_REG(0x23008E7F);
			break;
		case GX_PERF0_TRIANGLES_PASSED:
			GX_LOAD_BP_REG(0x23009E7F);
			break;
		case GX_PERF0_TRIANGLES_SCISSORED:
			GX_LOAD_BP_REG(0x23001E7F);
			break;
		case GX_PERF0_TRIANGLES_0TEX:
			GX_LOAD_BP_REG(0x2300AC3F);
			break;
		case GX_PERF0_TRIANGLES_1TEX:
			GX_LOAD_BP_REG(0x2300AC7F);
			break;
		case GX_PERF0_TRIANGLES_2TEX:
			GX_LOAD_BP_REG(0x2300ACBF);
			break;
		case GX_PERF0_TRIANGLES_3TEX:
			GX_LOAD_BP_REG(0x2300ACFF);
			break;
		case GX_PERF0_TRIANGLES_4TEX:
			GX_LOAD_BP_REG(0x2300AD3F);
			break;
		case GX_PERF0_TRIANGLES_5TEX:
			GX_LOAD_BP_REG(0x2300AD7F);
			break;
		case GX_PERF0_TRIANGLES_6TEX:
			GX_LOAD_BP_REG(0x2300ADBF);
			break;
		case GX_PERF0_TRIANGLES_7TEX:
			GX_LOAD_BP_REG(0x2300ADFF);
			break;
		case GX_PERF0_TRIANGLES_8TEX:
			GX_LOAD_BP_REG(0x2300AE3F);
			break;
		case GX_PERF0_TRIANGLES_0CLR:
			GX_LOAD_BP_REG(0x2300A27F);
			break;
		case GX_PERF0_TRIANGLES_1CLR:
			GX_LOAD_BP_REG(0x2300A67F);
			break;
		case GX_PERF0_TRIANGLES_2CLR:
			GX_LOAD_BP_REG(0x2300AA7F);
			break;
		case GX_PERF0_QUAD_0CVG:
			GX_LOAD_BP_REG(0x2402C0C6);
			break;
		case GX_PERF0_QUAD_NON0CVG:
			GX_LOAD_BP_REG(0x2402C16B);
			break;
		case GX_PERF0_QUAD_1CVG:
			GX_LOAD_BP_REG(0x2402C0E7);
			break;
		case GX_PERF0_QUAD_2CVG:
			GX_LOAD_BP_REG(0x2402C108);
			break;
		case GX_PERF0_QUAD_3CVG:
			GX_LOAD_BP_REG(0x2402C129);
			break;
		case GX_PERF0_QUAD_4CVG:
			GX_LOAD_BP_REG(0x2402C14A);
			break;
		case GX_PERF0_AVG_QUAD_CNT:
			GX_LOAD_BP_REG(0x2402C1AD);
			break;
		case GX_PERF0_NONE:
			break;
	}

	__gx->perf1Mode = perf1;
	switch(__gx->perf1Mode) {
		case GX_PERF1_CLOCKS:
			GX_LOAD_BP_REG(0x67000042);
			break;
		case GX_PERF1_TEXELS:
			GX_LOAD_BP_REG(0x67000084);
			break;
		case GX_PERF1_TX_IDLE:
			GX_LOAD_BP_REG(0x67000063);
			break;
		case GX_PERF1_TX_REGS:
			GX_LOAD_BP_REG(0x67000129);
			break;
		case GX_PERF1_TX_MEMSTALL:
			GX_LOAD_BP_REG(0x67000252);
			break;
		case GX_PERF1_TC_CHECK1_2:
			GX_LOAD_BP_REG(0x67000021);
			break;
		case GX_PERF1_TC_CHECK3_4:
			GX_LOAD_BP_REG(0x6700014b);
			break;
		case GX_PERF1_TC_CHECK5_6:
			GX_LOAD_BP_REG(0x6700018d);
			break;
		case GX_PERF1_TC_CHECK7_8:
			GX_LOAD_BP_REG(0x670001cf);
			break;
		case GX_PERF1_TC_MISS:
			GX_LOAD_BP_REG(0x67000211);
			break;
		case GX_PERF1_VC_ELEMQ_FULL:
			__gx->cpPerfMode = (__gx->cpPerfMode&~0xf0)|0x20;
			GX_LOAD_CP_REG(0x20,__gx->cpPerfMode);
			break;
		case GX_PERF1_VC_MISSQ_FULL:
			__gx->cpPerfMode = (__gx->cpPerfMode&~0xf0)|0x30;
			GX_LOAD_CP_REG(0x20,__gx->cpPerfMode);
			break;
		case GX_PERF1_VC_MEMREQ_FULL:
			__gx->cpPerfMode = (__gx->cpPerfMode&~0xf0)|0x40;
			GX_LOAD_CP_REG(0x20,__gx->cpPerfMode);
			break;
		case GX_PERF1_VC_STATUS7:
			__gx->cpPerfMode = (__gx->cpPerfMode&~0xf0)|0x50;
			GX_LOAD_CP_REG(0x20,__gx->cpPerfMode);
			break;
		case GX_PERF1_VC_MISSREP_FULL:
			__gx->cpPerfMode = (__gx->cpPerfMode&~0xf0)|0x60;
			GX_LOAD_CP_REG(0x20,__gx->cpPerfMode);
			break;
		case GX_PERF1_VC_STREAMBUF_LOW:
			__gx->cpPerfMode = (__gx->cpPerfMode&~0xf0)|0x70;
			GX_LOAD_CP_REG(0x20,__gx->cpPerfMode);
			break;
		case GX_PERF1_VC_ALL_STALLS:
			__gx->cpPerfMode = (__gx->cpPerfMode&~0xf0)|0x90;
			GX_LOAD_CP_REG(0x20,__gx->cpPerfMode);
			break;
		case GX_PERF1_VERTICES:
			__gx->cpPerfMode = (__gx->cpPerfMode&~0xf0)|0x80;
			GX_LOAD_CP_REG(0x20,__gx->cpPerfMode);
			break;
		case GX_PERF1_FIFO_REQ:
			_cpReg[3] = 2;
			break;
		case GX_PERF1_CALL_REQ:
			_cpReg[3] = 3;
			break;
		case GX_PERF1_VC_MISS_REQ:
			_cpReg[3] = 4;
			break;
		case GX_PERF1_CP_ALL_REQ:
			_cpReg[3] = 5;
			break;
		case GX_PERF1_NONE:
			break;
	}

}

void GX_ClearGPMetric()
{
	_cpReg[2] = 4;
}

void GX_InitXfRasMetric()
{
	GX_LOAD_BP_REG(0x2402C022);
	GX_LOAD_XF_REG(0x1006,0x31000);
}

void GX_ReadXfRasMetric(u32 *xfwaitin,u32 *xfwaitout,u32 *rasbusy,u32 *clks)
{
	*rasbusy = _SHIFTL(_cpReg[33],16,16)|(_cpReg[32]&0xffff);
	*clks = _SHIFTL(_cpReg[35],16,16)|(_cpReg[34]&0xffff);
	*xfwaitin = _SHIFTL(_cpReg[37],16,16)|(_cpReg[36]&0xffff);
	*xfwaitout = _SHIFTL(_cpReg[39],16,16)|(_cpReg[38]&0xffff);
}

u32 GX_ReadClksPerVtx()
{
	GX_DrawDone();
	_cpReg[49] = 0x1007;
	_cpReg[48] = 0x1007;
	return (_cpReg[50]<<8);
}

void GX_ClearVCacheMetric()
{
	GX_LOAD_CP_REG(0,0);
}

void GX_ReadVCacheMetric(u32 *check,u32 *miss,u32 *stall)
{
	*check = _SHIFTL(_cpReg[41],16,16)|(_cpReg[40]&0xffff);
	*miss = _SHIFTL(_cpReg[43],16,16)|(_cpReg[42]&0xffff);
	*stall = _SHIFTL(_cpReg[45],16,16)|(_cpReg[44]&0xffff);
}

void GX_SetVCacheMetric(u32 attr)
{
}

void GX_GetGPStatus(u8 *overhi,u8 *underlow,u8 *readIdle,u8 *cmdIdle,u8 *brkpt)
{
	_gxgpstatus = _cpReg[0];
	*overhi = !!(_gxgpstatus&1);
	*underlow = !!(_gxgpstatus&2);
	*readIdle = !!(_gxgpstatus&4);
	*cmdIdle = !!(_gxgpstatus&8);
	*brkpt = !!(_gxgpstatus&16);
}

void GX_ReadGPMetric(u32 *cnt0,u32 *cnt1)
{
	u32 tmp,reg1,reg2;

	reg1 = (_SHIFTL(_cpReg[33],16,16))|(_cpReg[32]&0xffff);
	reg2 = (_SHIFTL(_cpReg[35],16,16))|(_cpReg[34]&0xffff);
	//reg3 = (_SHIFTL(_cpReg[37],16,16))|(_cpReg[36]&0xffff);
	//reg4 = (_SHIFTL(_cpReg[39],16,16))|(_cpReg[38]&0xffff);

	*cnt0 = 0;
	if(__gx->perf0Mode==GX_PERF0_CLIP_RATIO) {
		tmp = reg2*1000;
		*cnt0 = tmp/reg1;
	} else if(__gx->perf0Mode>=GX_PERF0_VERTICES && __gx->perf0Mode<GX_PERF0_NONE) *cnt0 = reg1;

	//further implementation needed.....
	// cnt1 fails....
}

void GX_AdjustForOverscan(GXRModeObj *rmin,GXRModeObj *rmout,u16 hor,u16 ver)
{
	if(rmin!=rmout) memcpy(rmout,rmin,sizeof(GXRModeObj));

	rmout->fbWidth = rmin->fbWidth-(hor<<1);
	rmout->efbHeight = rmin->efbHeight-((rmin->efbHeight*(ver<<1))/rmin->xfbHeight);
	if(rmin->xfbMode==VI_XFBMODE_SF && !(rmin->viTVMode&VI_PROGRESSIVE)) rmout->xfbHeight = rmin->xfbHeight-ver;
	else rmout->xfbHeight = rmin->xfbHeight-(ver<<1);

	rmout->viWidth = rmin->viWidth-(hor<<1);
	if(rmin->viTVMode&VI_PROGRESSIVE) rmout->viHeight = rmin->viHeight-(ver<<2);
	else rmout->viHeight = rmin->viHeight-(ver<<1);

	rmout->viXOrigin += hor;
	rmout->viYOrigin += ver;
}

f32 GX_GetYScaleFactor(u16 efbHeight,u16 xfbHeight)
{
	u32 yScale,xfblines,cnt;
	f32 yscale;

	yscale = (f32)efbHeight/(f32)xfbHeight;
	yScale = (u32)((f32)256.0/yscale)&0x1ff;

	cnt = xfbHeight;
	xfblines = __GX_GetNumXfbLines(efbHeight,yScale);
	while(xfblines>=xfbHeight) {
		yscale = (f32)(cnt--)/(f32)efbHeight;
		yScale = (u32)((f32)256.0/yscale)&0x1ff;
		xfblines = __GX_GetNumXfbLines(efbHeight,yScale);
	}

	while(xfblines<xfbHeight) {
		yscale = (f32)(cnt++)/(f32)efbHeight;
		yScale = (u32)((f32)256.0/yscale)&0x1ff;
		xfblines = __GX_GetNumXfbLines(efbHeight,yScale);
	}
	return yscale;
}

void GX_ReadBoundingBox(u16 *top,u16 *bottom,u16 *left,u16 *right)
{
	*left = _peReg[8];
	*right = _peReg[9];
	*top = _peReg[10];
	*bottom = _peReg[11];
}

#undef LARGE_NUMBER
