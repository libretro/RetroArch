#include <retro_inline.h>

#include "../../defines/gx_defines.h"

#define STRUCT_REGDEF_SIZE		1440
#define WGPIPE (0xCC008000)

#define FIFO_PUTU8(x) *(vu8*)WGPIPE = (u8)(x)
#define FIFO_PUTS8(x) *(vs8*)WGPIPE = (s8)(x)
#define FIFO_PUTU16(x) *(vu16*)WGPIPE = (u16)(x)
#define FIFO_PUTS16(x) *(vs16*)WGPIPE = (s16)(x)
#define FIFO_PUTU32(x) *(vu32*)WGPIPE = (u32)(x)
#define FIFO_PUTS32(x) *(vs32*)WGPIPE = (s32)(x)
#define FIFO_PUTF32(x) *(vf32*)WGPIPE = (f32)(x)

#define XY(x, y)   (((y) << 10) | (x))

#define GX_LOAD_BP_REG(x) \
   FIFO_PUTU8(0x61); \
   FIFO_PUTU32((x))

#define GX_LOAD_CP_REG(x, y) \
   FIFO_PUTU8(0x08); \
   FIFO_PUTU8((x)); \
   FIFO_PUTU32((y))

#define GX_LOAD_XF_REG(x, y) \
   FIFO_PUTU8(0x10); \
   FIFO_PUTU32(((x) & 0xffff)); \
   FIFO_PUTU32((y))

#define GX_LOAD_XF_REGS(x, n) \
   FIFO_PUTU8(0x10); \
   FIFO_PUTU32((((((n) & 0xffff)-1)<<16)|((x) & 0xffff)))

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

#define __GX_FlushTextureState(__gx) GX_LOAD_BP_REG(__gx->tevIndMask)

#define __GX_SetDispCopySrc(__gx, left, top, wd, ht) \
__gx->dispCopyTL = (__gx->dispCopyTL & ~0x00ffffff) | XY(left,top); \
__gx->dispCopyTL = (__gx->dispCopyTL & ~0xff000000) | (_SHIFTL(0x49,24,8)); \
__gx->dispCopyWH = (__gx->dispCopyWH & ~0x00ffffff) | XY((wd-1),(ht-1)); \
__gx->dispCopyWH = (__gx->dispCopyWH & ~0xff000000) | (_SHIFTL(0x4a,24,8))

#define __GX_SetDispCopyDst(__gx, wd, ht) \
__gx->dispCopyDst = (__gx->dispCopyDst & ~0x3ff) | (_SHIFTR(wd,4,10)); \
__gx->dispCopyDst = (__gx->dispCopyDst & ~0xff000000) | (_SHIFTL(0x4d,24,8))

#define __GX_SetClipMode(mode) GX_LOAD_XF_REG(0x1005,(mode & 1))

#define __GX_CopyDisp(__gx, dest, clear) \
{ \
   u8 clflag; \
   u32 val; \
   if(clear) \
   { \
      val= (__gx->peZMode & ~0xf) | 0xf; \
      GX_LOAD_BP_REG(val); \
      val = (__gx->peCMode0 & ~0x3); \
      GX_LOAD_BP_REG(val); \
   } \
   clflag = 0; \
   if (clear || (__gx->peCntrl & 0x7) == 0x0003) \
   { \
      if (__gx->peCntrl & 0x40) \
      { \
         clflag = 1; \
         val = (__gx->peCntrl & ~0x40); \
         GX_LOAD_BP_REG(val); \
      } \
   } \
   GX_LOAD_BP_REG(__gx->dispCopyTL); /* set source top */ \
   GX_LOAD_BP_REG(__gx->dispCopyWH); \
   GX_LOAD_BP_REG(__gx->dispCopyDst); \
   val = 0x4b000000 | (_SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(dest),5,24)); \
   GX_LOAD_BP_REG(val); \
   __gx->dispCopyCntrl = (__gx->dispCopyCntrl & ~0x800) | (_SHIFTL(clear,11,1)); \
   __gx->dispCopyCntrl = (__gx->dispCopyCntrl & ~0x4000) | 0x4000; \
   __gx->dispCopyCntrl = (__gx->dispCopyCntrl & ~0xff000000) | (_SHIFTL(0x52,24,8)); \
   GX_LOAD_BP_REG(__gx->dispCopyCntrl); \
   if (clear) \
   { \
      GX_LOAD_BP_REG(__gx->peZMode); \
      GX_LOAD_BP_REG(__gx->peCMode0); \
   } \
   if (clflag) \
   { \
      GX_LOAD_BP_REG(__gx->peCntrl); \
   } \
}

#if 0
static void __SETVCDATTR(struct __gx_regdef *__gx, u8 attr,u8 type)
{
   switch(attr)
   {
      case GX_VA_PTNMTXIDX:
         __gx->vcdLo = (__gx->vcdLo & ~0x1) | (type & 0x1);
         break;
      case GX_VA_TEX0MTXIDX:
         __gx->vcdLo = (__gx->vcdLo & ~0x2) | (_SHIFTL(type,1,1));
         break;
      case GX_VA_TEX1MTXIDX:
         __gx->vcdLo = (__gx->vcdLo & ~0x4)|(_SHIFTL(type,2,1));
         break;
      case GX_VA_TEX2MTXIDX:
         __gx->vcdLo = (__gx->vcdLo & ~0x8)|(_SHIFTL(type,3,1));
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
#endif

#define XSHIFT 2
#define YSHIFT 2

#define __GX_InitTexObj(ptr, img_ptr, wd, ht, fmt, wrap_s, wrap_t, mipmap) \
   ptr->tex_filt = (ptr->tex_filt & ~0x03)|(wrap_s & 3); \
   ptr->tex_filt = (ptr->tex_filt & ~0x0c)|(_SHIFTL(wrap_t,2,2)); \
   ptr->tex_filt = (ptr->tex_filt & ~0x10)|0x10; \
   /* no mip-mapping */ \
   ptr->tex_filt= (ptr->tex_filt & ~0xE0)|0x0080; \
   ptr->tex_fmt = fmt; \
   ptr->tex_size = (ptr->tex_size & ~0x3ff)|((wd-1) & 0x3ff); \
   ptr->tex_size = (ptr->tex_size & ~0xFFC00)|(_SHIFTL((ht-1),10,10)); \
   ptr->tex_size = (ptr->tex_size & ~0xF00000)|(_SHIFTL(fmt,20,4)); \
   ptr->tex_maddr = (ptr->tex_maddr & ~0x01ffffff)|(_SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(img_ptr),5,24)); \
   ptr->tex_tile_type = 2; \
   ptr->tex_tile_cnt = ((((wd+(1 << XSHIFT))-1) >> XSHIFT) * (((ht+(1 << YSHIFT))-1) >> YSHIFT)) & 0x7fff; \
   ptr->tex_flag |= 0x0002


#define __GX_InvalidateTexAll(__gx) \
	GX_LOAD_BP_REG(__gx->tevIndMask); \
	GX_LOAD_BP_REG(0x66001000); \
	GX_LOAD_BP_REG(0x66001100); \
	GX_LOAD_BP_REG(__gx->tevIndMask)

#define __GX_SetCurrentMtx(__gx, mtx) \
	__gx->mtxIdxLo = (__gx->mtxIdxLo & ~0x3f)|(mtx & 0x3f); \
	__gx->dirtyState |= 0x04000000

#define __GX_SetVAT(__gx, setvtx) \
   for(s32 i = 0; i < 8;i++) \
   { \
      setvtx = (1<<i); \
      if(__gx->VATTable & setvtx) \
      { \
         GX_LOAD_CP_REG((0x70+(i & 7)),__gx->VAT0reg[i]); \
         GX_LOAD_CP_REG((0x80+(i & 7)),__gx->VAT1reg[i]); \
         GX_LOAD_CP_REG((0x90+(i & 7)),__gx->VAT2reg[i]); \
      } \
   } \
   __gx->VATTable = 0

#define __GX_XfVtxSpecs(__gx) \
{ \
   u32 nrms,texs,cols; \
   cols = 0; \
   if(__gx->vcdLo & 0x6000) cols++; \
   if(__gx->vcdLo & 0x18000) cols++; \
   nrms = 0; \
   if(__gx->vcdNrms==1) nrms = 1; \
   else if(__gx->vcdNrms==2) nrms = 2; \
   texs = 0; \
   if(__gx->vcdHi & 0x3) texs++; \
   if(__gx->vcdHi & 0xc) texs++; \
   if(__gx->vcdHi & 0x30) texs++; \
   if(__gx->vcdHi & 0xc0) texs++; \
   if(__gx->vcdHi & 0x300) texs++; \
   if(__gx->vcdHi & 0xc00) texs++; \
   if(__gx->vcdHi & 0x3000) texs++; \
   if(__gx->vcdHi & 0xc000) texs++; \
   GX_LOAD_XF_REG(0x1008, ((_SHIFTL(texs,4,4))|(_SHIFTL(nrms,2,2))|(cols & 0x3))); \
}

#define __GX_SetVCD(__gx) \
	GX_LOAD_CP_REG(0x50,__gx->vcdLo); \
   GX_LOAD_CP_REG(0x60,__gx->vcdHi); \
   __GX_XfVtxSpecs(__gx)


#define __GX_SetChanCntrl(__gx) \
{ \
   u32 i,chan,mask; \
   if(__gx->dirtyState & 0x01000000) \
   { \
      GX_LOAD_XF_REG(0x1009,(_SHIFTR(__gx->genMode,4,3))); \
   } \
   i = 0; \
   chan = 0x100e; \
   mask = _SHIFTR(__gx->dirtyState,12,4); \
   while(mask) { \
      if(mask & 0x0001) \
      { \
         GX_LOAD_XF_REG(chan,__gx->chnCntrl[i]); \
      } \
      mask >>= 1; \
      chan++; \
      i++; \
   } \
}

#define __GX_SetMatrixIndex(__gx, mtx) \
   if(mtx<5) { \
      GX_LOAD_CP_REG(0x30,__gx->mtxIdxLo); \
      GX_LOAD_XF_REG(0x1018,__gx->mtxIdxLo); \
   } else { \
      GX_LOAD_CP_REG(0x40,__gx->mtxIdxHi); \
      GX_LOAD_XF_REG(0x1019,__gx->mtxIdxHi); \
   }

#define __GX_SetArray(__gx, attr, ptr, stride) \
   if(attr>=GX_VA_POS && attr<=GX_LIGHTARRAY) \
   { \
      GX_LOAD_CP_REG((0xA0 +((attr) - GX_VA_POS)),(u32)MEM_VIRTUAL_TO_PHYSICAL(ptr)); \
      GX_LOAD_CP_REG((0xB0 +((attr) - GX_VA_POS)),(u32)(stride)); \
   }

#define __GX_Begin(__vtx, primitive, vtxfmt, vtxcnt) \
   if(__gx->dirtyState) \
      __GX_SetDirtyState(__gx); \
   FIFO_PUTU8(primitive | (vtxfmt & 7)); \
   FIFO_PUTU16(vtxcnt)

#ifdef HW_DOL
static INLINE void __GX_UpdateBPMask(struct __gx_regdef *__gx)
{
   u32 i;
   u8 ntexmap;
   u32 nbmp = _SHIFTR(__gx->genMode,16,3);
   u32 nres = 0;

   for(i = 0; i < nbmp; i++)
   {
      switch (i)
      {
         case GX_INDTEXSTAGE0:
            ntexmap = __gx->tevRasOrder[2] & 7;
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

   if((__gx->tevIndMask & 0xff)!=nres)
   {
      __gx->tevIndMask = (__gx->tevIndMask & ~0xff)|(nres & 0xff);
      GX_LOAD_BP_REG(__gx->tevIndMask);
   }
}
#endif

#define __GX_SetChanColor(__gx) \
   if(__gx->dirtyState & 0x0100) \
   { \
      GX_LOAD_XF_REG(0x100a,__gx->chnAmbColor[0]); \
   } \
   if(__gx->dirtyState & 0x0200) \
   { \
      GX_LOAD_XF_REG(0x100b,__gx->chnAmbColor[1]); \
   } \
   if(__gx->dirtyState & 0x0400) \
   { \
      GX_LOAD_XF_REG(0x100c,__gx->chnMatColor[0]); \
   } \
   if(__gx->dirtyState & 0x0800) \
   { \
      GX_LOAD_XF_REG(0x100d,__gx->chnMatColor[1]); \
   }

static INLINE void __GX_SetTexCoordGen(struct __gx_regdef *__gx)
{
   u32 i,mask;
   u32 texcoord;

   if(__gx->dirtyState&0x02000000)
   {
      GX_LOAD_XF_REG(0x103f,(__gx->genMode&0xf));
   }

   i        = 0;
   texcoord = 0x1040;
   mask     = _SHIFTR(__gx->dirtyState,16,8);
   while(mask)
   {
      if(mask&0x0001)
      {
         GX_LOAD_XF_REG(texcoord,__gx->texCoordGen[i]);
         GX_LOAD_XF_REG((texcoord+0x10),__gx->texCoordGen2[i]);
      }
      mask >>= 1;
      texcoord++;
      i++;
   }
}

static void __SetSURegs(struct __gx_regdef *__gx, u8 texmap,u8 texcoord)
{
   u16 wd     = __gx->texMapSize[texmap] & 0x3ff;
   u16 ht     = _SHIFTR(__gx->texMapSize[texmap],10,10);
   u8  wrap_s = __gx->texMapWrap[texmap] & 3;
   u8  wrap_t = _SHIFTR(__gx->texMapWrap[texmap],2,2);
   u32 reg    = (texcoord & 0x7);

   __gx->suSsize[reg] = (__gx->suSsize[reg] & ~0x0000ffff)|wd;
   __gx->suTsize[reg] = (__gx->suTsize[reg]&~0x0000ffff)|ht;
   __gx->suSsize[reg] = (__gx->suSsize[reg]&~0x00010000)|(_SHIFTL(wrap_s,16,1));
   __gx->suTsize[reg] = (__gx->suTsize[reg]&~0x00010000)|(_SHIFTL(wrap_t,16,1));

   GX_LOAD_BP_REG(__gx->suSsize[reg]);
   GX_LOAD_BP_REG(__gx->suTsize[reg]);
}

static INLINE void __GX_SetSUTexRegs(struct __gx_regdef *__gx)
{
   u32 i, texcm;
   u8 texcoord, texmap;
   u32 dirtev = (_SHIFTR(__gx->genMode,10,4))+1;
   u32 indtev = _SHIFTR(__gx->genMode,16,3);

   /* Indirect texture order */
   for(i = 0; i < indtev; i++)
   {
      switch(i)
      {
         case GX_INDTEXSTAGE0:
            texmap = __gx->tevRasOrder[2] & 7;
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

      if(!(__gx->texCoordManually & texcm))
         __SetSURegs(__gx, texmap,texcoord);
   }

   /* Direct texture order */
   for(i = 0; i < dirtev; i++)
   {
      u32 tevm;
      u32 tevreg = 3+(_SHIFTR(i,1,3));
      texmap = (__gx->tevTexMap[i] & 0xff);

      if(i & 1)
         texcoord = _SHIFTR(__gx->tevRasOrder[tevreg],15,3);
      else
         texcoord = _SHIFTR(__gx->tevRasOrder[tevreg],3,3);

      tevm = _SHIFTL(1,i,1);
      texcm = _SHIFTL(1,texcoord,1);
      if(texmap!=0xff && (__gx->tevTexCoordEnable & tevm) && !(__gx->texCoordManually & texcm))
      {
         __SetSURegs(__gx, texmap,texcoord);
      }
   }
}

#define __GX_SetGenMode(__gx) \
   GX_LOAD_BP_REG(__gx->genMode); \
   __gx->xfFlush = 0

static void __GX_SetDirtyState(struct __gx_regdef *__gx)
{
   if(__gx->dirtyState & 0x0001)
   {
      __GX_SetSUTexRegs(__gx);
   }
#ifdef HW_DOL
   if(__gx->dirtyState & 0x0002)
   {
      __GX_UpdateBPMask(__gx);
   }
#endif
   if(__gx->dirtyState & 0x0004)
   {
      __GX_SetGenMode(__gx);
   }
   if(__gx->dirtyState & 0x0008)
   {
      __GX_SetVCD(__gx);
   }
   if(__gx->dirtyState & 0x0010)
   {
      u8 setvtx = 0;
      __GX_SetVAT(__gx, setvtx);
   }
   if(__gx->dirtyState & ~0xff)
   {
      if(__gx->dirtyState & 0x0f00)
      {
         __GX_SetChanColor(__gx);
      }
      if(__gx->dirtyState & 0x0100f000)
      {
         __GX_SetChanCntrl(__gx);
      }
      if(__gx->dirtyState & 0x02ff0000)
      {
         __GX_SetTexCoordGen(__gx);
      }
      if(__gx->dirtyState & 0x04000000)
      {
         __GX_SetMatrixIndex(__gx, 0);
         __GX_SetMatrixIndex(__gx, 5);
      }
   }
   __gx->dirtyState = 0;
}

static void __GX_SendFlushPrim(struct __gx_regdef *__gx)
{
   u32 tmp2,cnt;
   u32 tmp = (__gx->xfFlush*__gx->xfFlushExp);

   FIFO_PUTU8(0x98);
   FIFO_PUTU16(__gx->xfFlush);

   tmp2 = (tmp+3)/4;

   if(tmp > 0)
   {
      cnt = tmp2/8;
      while(cnt)
      {
         FIFO_PUTU32(0);
         FIFO_PUTU32(0);
         FIFO_PUTU32(0);
         FIFO_PUTU32(0);
         FIFO_PUTU32(0);
         FIFO_PUTU32(0);
         FIFO_PUTU32(0);
         FIFO_PUTU32(0);
         cnt--;
      }
      tmp2 &= 0x0007;
      if(tmp2)
      {
         while(tmp2)
         {
            FIFO_PUTU32(0);
            tmp2--;
         }
      }
   }
   __gx->xfFlush = 1;
}

#define __GX_InitTexObjFilterMode(ptr, minfilt, magfilt) \
{ \
   static u8 GX2HWFiltConv[] = {0x00,0x04,0x01,0x05,0x02,0x06,0x00,0x00}; \
   ptr->tex_filt = (ptr->tex_filt & ~0x10)|(_SHIFTL((magfilt==GX_LINEAR?1:0),4,1)); \
   ptr->tex_filt = (ptr->tex_filt & ~0xe0)|(_SHIFTL(GX2HWFiltConv[minfilt],5,3)); \
}

#define __GX_SetCullMode(__gx, mode) \
{ \
   static u8 cm2hw[] = { 0, 2, 1, 3 }; \
   __gx->genMode = (__gx->genMode & ~0xC000)|(_SHIFTL(cm2hw[mode],14,2)); \
   __gx->dirtyState |= 0x0004; \
}

#define __GX_CallDispList(__gx, list, nbytes) \
	if(__gx->dirtyState) \
   { \
      __GX_SetDirtyState(__gx);  \
   } \
   if(!__gx->vcdClear) \
   { \
      __GX_SendFlushPrim(__gx); \
   } \
   FIFO_PUTU8(0x40); /*call displaylist */ \
   FIFO_PUTU32(MEM_VIRTUAL_TO_PHYSICAL(list)); \
   FIFO_PUTU32(nbytes)

#define __GX_Flush(__gx) \
   if(__gx->dirtyState) \
      __GX_SetDirtyState(__gx); \
   FIFO_PUTU32(0); \
   FIFO_PUTU32(0); \
   FIFO_PUTU32(0); \
   FIFO_PUTU32(0); \
   FIFO_PUTU32(0); \
   FIFO_PUTU32(0); \
   FIFO_PUTU32(0); \
   FIFO_PUTU32(0); \
   ppcsync()

#define __GX_ClearVtxDesc(__gx) \
   __gx->vcdNrms = 0; \
   __gx->vcdClear = ((__gx->vcdClear & ~0x0600)|0x0200); \
   __gx->vcdLo = __gx->vcdHi = 0; \
   __gx->dirtyState |= 0x0008

#if 0
#define __GX_SetVtxDesc(__gx, attr, type) \
   __SETVCDATTR(__gx, attr,type); \
   __gx->dirtyState |= 0x0008
#endif

#define __GX_SetBlendMode(__gx, type, src_fact, dst_fact, op) \
   __gx->peCMode0 = (__gx->peCMode0 & ~0x1); \
   if(type==GX_BM_BLEND || type==GX_BM_SUBTRACT) __gx->peCMode0 |= 0x1; \
   __gx->peCMode0 = (__gx->peCMode0 & ~0x800); \
   if(type==GX_BM_SUBTRACT) __gx->peCMode0 |= 0x800; \
   __gx->peCMode0 = (__gx->peCMode0 & ~0x2); \
   if(type==GX_BM_LOGIC) __gx->peCMode0 |= 0x2; \
   __gx->peCMode0 = (__gx->peCMode0 & ~0xF000)|(_SHIFTL(op,12,4)); \
   __gx->peCMode0 = (__gx->peCMode0 & ~0xE0)|(_SHIFTL(dst_fact,5,3)); \
   __gx->peCMode0 = (__gx->peCMode0 & ~0x700)|(_SHIFTL(src_fact,8,3)); \
   GX_LOAD_BP_REG(__gx->peCMode0)

#define __GX_InvVtxCache() FIFO_PUTU8(0x48)

#define __GX_SetDispCopyGamma(__gx, gamma) __gx->dispCopyCntrl = (__gx->dispCopyCntrl & ~0x180) | (_SHIFTL(gamma,7,2))

#define __GX_SetColorUpdate(__gx, enable) \
   __gx->peCMode0 = (__gx->peCMode0 & ~0x8)|(_SHIFTL(enable,3,1)); \
   GX_LOAD_BP_REG(__gx->peCMode0)

#define __GX_SetAlphaUpdate(__gx, enable) \
   __gx->peCMode0 = (__gx->peCMode0 & ~0x10)|(_SHIFTL(enable,4,1)); \
   GX_LOAD_BP_REG(__gx->peCMode0)

#define __GX_SetNumChans(__gx, num) \
   __gx->genMode = (__gx->genMode & ~0x70)|(_SHIFTL(num,4,3)); \
   __gx->dirtyState |= 0x01000004

#define __GX_SetNumTexGens(__gx, nr) \
   __gx->genMode = (__gx->genMode & ~0xf)|(nr & 0xf); \
   __gx->dirtyState |= 0x02000004

#define __GX_PokeARGB(x, y, color) \
{ \
   u32 regval; \
   regval = 0xc8000000|(_SHIFTL(x,2,10)); \
   regval = (regval & ~0x3FF000)|(_SHIFTL(y,12,10)); \
   *(u32*)regval = _SHIFTL(color.a,24,8)|_SHIFTL(color.r,16,8)|_SHIFTL(color.g,8,8)|(color.b & 0xff); \
}

#define __GX_SetFieldMode(__gx, field_mode, half_aspect_ratio) \
   __gx->lpWidth = (__gx->lpWidth & ~0x400000)|(_SHIFTL(half_aspect_ratio,22,1)); \
   GX_LOAD_BP_REG(__gx->lpWidth); \
   __GX_FlushTextureState(__gx); \
   GX_LOAD_BP_REG(0x68000000|(field_mode & 1)); \
   __GX_FlushTextureState(__gx)

#define __GX_SetCopyClear(color, zvalue) \
{ \
   u32 val; \
   val = (_SHIFTL(color.a,8,8))|(color.r & 0xff); \
   GX_LOAD_BP_REG(0x4f000000|val); \
   val = (_SHIFTL(color.g,8,8))|(color.b & 0xff); \
   GX_LOAD_BP_REG(0x50000000|val); \
   val = zvalue & 0x00ffffff; \
   GX_LOAD_BP_REG(0x51000000|val); \
}

#define __GX_SetZMode(__gx, enable, func, update_enable) \
   __gx->peZMode = (__gx->peZMode&~0x1)|(enable&1); \
   __gx->peZMode = (__gx->peZMode&~0xe)|(_SHIFTL(func,1,3)); \
   __gx->peZMode = (__gx->peZMode&~0x10)|(_SHIFTL(update_enable,4,1)); \
   GX_LOAD_BP_REG(__gx->peZMode)

#define __GX_LoadTexObj(obj, mapid) \
{ \
   struct __gx_texobj *ptr = (struct __gx_texobj*)obj; \
   ptr->tex_filt = (ptr->tex_filt&~0xff000000)|(_SHIFTL(_gxtexmode0ids[mapid],24,8)); \
   ptr->tex_lod = (ptr->tex_lod&~0xff000000)|(_SHIFTL(_gxtexmode1ids[mapid],24,8)); \
   ptr->tex_size = (ptr->tex_size&~0xff000000)|(_SHIFTL(_gxteximg0ids[mapid],24,8)); \
   ptr->tex_maddr = (ptr->tex_maddr&~0xff000000)|(_SHIFTL(_gxteximg3ids[mapid],24,8)); \
   GX_LOAD_BP_REG(ptr->tex_filt); \
   GX_LOAD_BP_REG(ptr->tex_lod); \
   GX_LOAD_BP_REG(ptr->tex_size); \
   GX_LOAD_BP_REG(ptr->tex_maddr); \
   __gx->texMapSize[mapid] = ptr->tex_size; \
   __gx->texMapWrap[mapid] = ptr->tex_filt; \
   __gx->dirtyState |= 0x0001; \
}

#define X_FACTOR 0.5
#define Y_FACTOR 342.0
#define ZFACTOR 16777215.0

static INLINE void __GX_SetViewportJitter(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ,u32 field)
{
   f32 x0,y0,x1,y1,n,f,z;
   static f32 Xfactor = 0.5;
   static f32 Yfactor = 342.0;
   static f32 Zfactor = 16777215.0;

   if(!field)
      yOrig -= Xfactor;
   x0 = wd*Xfactor;
   y0 = (-ht)*Xfactor;
   x1 = (xOrig+(wd*Xfactor))+Yfactor;
   y1 = (yOrig+(ht*Xfactor))+Yfactor;
   n  = Zfactor*nearZ;
   f  = Zfactor*farZ;
   z  = f-n;
   GX_LOAD_XF_REGS(0x101a,6);
   FIFO_PUTF32(x0);
   FIFO_PUTF32(y0);
   FIFO_PUTF32(z);
   FIFO_PUTF32(x1);
   FIFO_PUTF32(y1);
   FIFO_PUTF32(f);
}

#define __GX_Position1x8(index) FIFO_PUTU8(index)
#define __GX_TexCoord1x8(index) FIFO_PUTU8(index)
#define __GX_Color1x8(index) FIFO_PUTU8(index)
