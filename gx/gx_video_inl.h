#define STRUCT_REGDEF_SIZE		1440

extern u8 __gxregs[];

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

#if defined(HW_RVL)
static GXTexRegion* __GXDefTexRegionCallback(GXTexObj *obj,u8 mapid)
{
   struct __gx_regdef *__gx = (struct __gx_regdef*)__gxregs;
	u32 fmt;
	GXTexRegion *ret = NULL;

	fmt = ((struct __gx_texobj*)obj)->tex_fmt;
	if ((fmt >= GX_TF_CI4 && fmt<=GX_TF_CI14) || fmt==GX_TF_CMPR)
		ret = &__gx->texRegion[mapid];
	else
		ret = &__gx->texRegion[mapid+8];

	return ret;
}
#else
static GXTexRegion* __GXDefTexRegionCallback(GXTexObj *obj,u8 mapid)
{
   struct __gx_regdef *__gx = (struct __gx_regdef*)__gxregs;
	u32 fmt;
	u32 idx;
	static u32 regionA = 0;
	static u32 regionB = 0;
	GXTexRegion *ret = NULL;

	fmt = ((struct __gx_texobj*)obj)->tex_fmt;
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

#define GX_LOAD_BP_REG(x) \
 wgPipe->U8 = 0x61;				\
 asm volatile ("" ::: "memory" ); \
 wgPipe->U32 = (u32)(x);		\
 asm volatile ("" ::: "memory" )

#define GX_InvalidateTexAll() \
	GX_LOAD_BP_REG(__gx->tevIndMask); \
	GX_LOAD_BP_REG(0x66001000); \
	GX_LOAD_BP_REG(0x66001100); \
	GX_LOAD_BP_REG(__gx->tevIndMask)

#define GX_SetCurrentMtx(mtx) \
	__gx->mtxIdxLo = (__gx->mtxIdxLo&~0x3f)|(mtx&0x3f); \
	__gx->dirtyState |= 0x04000000

#if defined(HW_RVL)
#define GX_CopyDisp(dest,clear) \
	u8 clflag; \
	u32 val; \
	if(clear) { \
		val= (__gx->peZMode&~0xf)|0xf; \
		GX_LOAD_BP_REG(val); \
		val = (__gx->peCMode0&~0x3); \
		GX_LOAD_BP_REG(val); \
	} \
	clflag = 0; \
	if(clear || (__gx->peCntrl&0x7)==0x0003) { \
		if(__gx->peCntrl&0x40) { \
			clflag = 1; \
			val = (__gx->peCntrl&~0x40); \
			GX_LOAD_BP_REG(val); \
		} \
	} \
	GX_LOAD_BP_REG(__gx->dispCopyTL); \
	GX_LOAD_BP_REG(__gx->dispCopyWH); \
	GX_LOAD_BP_REG(__gx->dispCopyDst); \
	val = 0x4b000000|(_SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(dest),5,24)); \
	GX_LOAD_BP_REG(val); \
	__gx->dispCopyCntrl = (__gx->dispCopyCntrl&~0x800)|(_SHIFTL(clear,11,1)); \
	__gx->dispCopyCntrl = (__gx->dispCopyCntrl&~0x4000)|0x4000; \
	__gx->dispCopyCntrl = (__gx->dispCopyCntrl&~0xff000000)|(_SHIFTL(0x52,24,8)); \
	GX_LOAD_BP_REG(__gx->dispCopyCntrl); \
	if(clear) { \
		GX_LOAD_BP_REG(__gx->peZMode); \
		GX_LOAD_BP_REG(__gx->peCMode0); \
	} \
	if(clflag) GX_LOAD_BP_REG(__gx->peCntrl)
#endif

#define GX_LoadTexObj(obj,mapid) GX_LoadTexObjPreloaded(obj,(__GXDefTexRegionCallback(obj,mapid)),mapid)
