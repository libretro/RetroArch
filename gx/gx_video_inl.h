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
   FIFO_PUTU32(((x)&0xffff)); \
   FIFO_PUTU32((y))

#define GX_LOAD_XF_REGS(x, n) \
   FIFO_PUTU8(0x10); \
   FIFO_PUTU32((((((n)&0xffff)-1)<<16)|((x)&0xffff)))

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

#define __GX_SetDispCopySrc(__gx, left, top, wd, ht) \
__gx->dispCopyTL = (__gx->dispCopyTL&~0x00ffffff)|XY(left,top); \
__gx->dispCopyTL = (__gx->dispCopyTL&~0xff000000)|(_SHIFTL(0x49,24,8)); \
__gx->dispCopyWH = (__gx->dispCopyWH&~0x00ffffff)|XY((wd-1),(ht-1)); \
__gx->dispCopyWH = (__gx->dispCopyWH&~0xff000000)|(_SHIFTL(0x4a,24,8))

#define __GX_SetDispCopyDst(__gx, wd, ht) \
__gx->dispCopyDst = (__gx->dispCopyDst&~0x3ff)|(_SHIFTR(wd,4,10)); \
__gx->dispCopyDst = (__gx->dispCopyDst&~0xff000000)|(_SHIFTL(0x4d,24,8))

static inline void __GX_CopyDisp(struct __gx_regdef *__gx, void *dest,u8 clear)
{
   u8 clflag;
   u32 val;

   if(clear)
   {
      val= (__gx->peZMode&~0xf)|0xf;
      GX_LOAD_BP_REG(val);
      val = (__gx->peCMode0&~0x3);
      GX_LOAD_BP_REG(val);
   }

   clflag = 0;
   if(clear || (__gx->peCntrl&0x7)==0x0003)
   {
      if(__gx->peCntrl&0x40)
      {
         clflag = 1;
         val = (__gx->peCntrl&~0x40);
         GX_LOAD_BP_REG(val);
      }
   }

   GX_LOAD_BP_REG(__gx->dispCopyTL); // set source top
   GX_LOAD_BP_REG(__gx->dispCopyWH);

   GX_LOAD_BP_REG(__gx->dispCopyDst);

   val = 0x4b000000|(_SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(dest),5,24));
   GX_LOAD_BP_REG(val);

   __gx->dispCopyCntrl = (__gx->dispCopyCntrl&~0x800)|(_SHIFTL(clear,11,1));
   __gx->dispCopyCntrl = (__gx->dispCopyCntrl&~0x4000)|0x4000;
   __gx->dispCopyCntrl = (__gx->dispCopyCntrl&~0xff000000)|(_SHIFTL(0x52,24,8));

   GX_LOAD_BP_REG(__gx->dispCopyCntrl);

   if(clear)
   {
      GX_LOAD_BP_REG(__gx->peZMode);
      GX_LOAD_BP_REG(__gx->peCMode0);
   }

   if(clflag)
   {
      GX_LOAD_BP_REG(__gx->peCntrl);
   }
}

#define XSHIFT 2
#define YSHIFT 2

#define __GX_InitTexObj(ptr, img_ptr, wd, ht, fmt, wrap_s, wrap_t, mipmap) \
   ptr->tex_filt = (ptr->tex_filt&~0x03)|(wrap_s&3); \
   ptr->tex_filt = (ptr->tex_filt&~0x0c)|(_SHIFTL(wrap_t,2,2)); \
   ptr->tex_filt = (ptr->tex_filt&~0x10)|0x10; \
   /* no mip-mapping */ \
   ptr->tex_filt= (ptr->tex_filt&~0xE0)|0x0080; \
   ptr->tex_fmt = fmt; \
   ptr->tex_size = (ptr->tex_size&~0x3ff)|((wd-1)&0x3ff); \
   ptr->tex_size = (ptr->tex_size&~0xFFC00)|(_SHIFTL((ht-1),10,10)); \
   ptr->tex_size = (ptr->tex_size&~0xF00000)|(_SHIFTL(fmt,20,4)); \
   ptr->tex_maddr = (ptr->tex_maddr&~0x01ffffff)|(_SHIFTR(MEM_VIRTUAL_TO_PHYSICAL(img_ptr),5,24)); \
   ptr->tex_tile_type = 2; \
   ptr->tex_tile_cnt = ((((wd+(1 << XSHIFT))-1) >> XSHIFT) * (((ht+(1 << YSHIFT))-1) >> YSHIFT)) & 0x7fff; \
   ptr->tex_flag |= 0x0002


#define __GX_InvalidateTexAll(__gx) \
	GX_LOAD_BP_REG(__gx->tevIndMask); \
	GX_LOAD_BP_REG(0x66001000); \
	GX_LOAD_BP_REG(0x66001100); \
	GX_LOAD_BP_REG(__gx->tevIndMask)

#define __GX_SetCurrentMtx(__gx, mtx) \
	__gx->mtxIdxLo = (__gx->mtxIdxLo&~0x3f)|(mtx&0x3f); \
	__gx->dirtyState |= 0x04000000

#define __GX_SetVAT(__gx, setvtx) \
   for(s32 i = 0; i < 8;i++) \
   { \
      setvtx = (1<<i); \
      if(__gx->VATTable&setvtx) \
      { \
         GX_LOAD_CP_REG((0x70+(i&7)),__gx->VAT0reg[i]); \
         GX_LOAD_CP_REG((0x80+(i&7)),__gx->VAT1reg[i]); \
         GX_LOAD_CP_REG((0x90+(i&7)),__gx->VAT2reg[i]); \
      } \
   } \
   __gx->VATTable = 0

#define __GX_XfVtxSpecs(__gx) \
{ \
   u32 nrms,texs,cols; \
   cols = 0; \
   if(__gx->vcdLo&0x6000) cols++; \
   if(__gx->vcdLo&0x18000) cols++; \
   nrms = 0; \
   if(__gx->vcdNrms==1) nrms = 1; \
   else if(__gx->vcdNrms==2) nrms = 2; \
   texs = 0; \
   if(__gx->vcdHi & 0x3) texs++; \
   if(__gx->vcdHi & 0xc) texs++; \
   if(__gx->vcdHi&0x30) texs++; \
   if(__gx->vcdHi&0xc0) texs++; \
   if(__gx->vcdHi&0x300) texs++; \
   if(__gx->vcdHi&0xc00) texs++; \
   if(__gx->vcdHi&0x3000) texs++; \
   if(__gx->vcdHi&0xc000) texs++; \
   GX_LOAD_XF_REG(0x1008, ((_SHIFTL(texs,4,4))|(_SHIFTL(nrms,2,2))|(cols&0x3))); \
}

#define __GX_SetVCD(__gx) \
	GX_LOAD_CP_REG(0x50,__gx->vcdLo); \
   GX_LOAD_CP_REG(0x60,__gx->vcdHi); \
   __GX_XfVtxSpecs(__gx)


#define __GX_SetChanCntrl(__gx) \
{ \
   u32 i,chan,mask; \
   if(__gx->dirtyState&0x01000000) \
   { \
      GX_LOAD_XF_REG(0x1009,(_SHIFTR(__gx->genMode,4,3))); \
   } \
   i = 0; \
   chan = 0x100e; \
   mask = _SHIFTR(__gx->dirtyState,12,4); \
   while(mask) { \
      if(mask&0x0001) \
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

#ifdef HW_DOL
static inline void __GX_UpdateBPMask(struct __gx_regdef *__gx)
{
   u32 i;
   u32 nbmp,nres;
   u8 ntexmap;

   nbmp = _SHIFTR(_gx[0xac],16,3);

   nres = 0;
   for(i=0;i<nbmp;i++) {
      switch(i) {
         case GX_INDTEXSTAGE0:
            ntexmap = _gx[0xc2]&7;
            break;
         case GX_INDTEXSTAGE1:
            ntexmap = _SHIFTR(_gx[0xc2],6,3);
            break;
         case GX_INDTEXSTAGE2:
            ntexmap = _SHIFTR(_gx[0xc2],12,3);
            break;
         case GX_INDTEXSTAGE3:
            ntexmap = _SHIFTR(_gx[0xc2],18,3);
            break;
         default:
            ntexmap = 0;
            break;
      }
      nres |= (1<<ntexmap);
   }

   if((_gx[0xaf]&0xff)!=nres)
   {
      _gx[0xaf] = (_gx[0xaf]&~0xff)|(nres&0xff);
      GX_LOAD_BP_REG(_gx[0xaf]);
   }
}
#endif

#define __GX_SetChanColor(__gx) \
   if(__gx->dirtyState&0x0100) \
   { \
      GX_LOAD_XF_REG(0x100a,__gx->chnAmbColor[0]); \
   } \
   if(__gx->dirtyState&0x0200) \
   { \
      GX_LOAD_XF_REG(0x100b,__gx->chnAmbColor[1]); \
   } \
   if(__gx->dirtyState&0x0400) \
   { \
      GX_LOAD_XF_REG(0x100c,__gx->chnMatColor[0]); \
   } \
   if(__gx->dirtyState&0x0800) \
   { \
      GX_LOAD_XF_REG(0x100d,__gx->chnMatColor[1]); \
   }

static inline void __GX_SetTexCoordGen(struct __gx_regdef *__gx)
{
   u32 i,mask;
   u32 texcoord;

   if(__gx->dirtyState&0x02000000)
   {
      GX_LOAD_XF_REG(0x103f,(__gx->genMode&0xf));
   }

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

static void __SetSURegs(struct __gx_regdef *__gx, u8 texmap,u8 texcoord)
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

static inline void __GX_SetSUTexRegs(struct __gx_regdef *__gx)
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
         __SetSURegs(__gx, texmap,texcoord);
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
         __SetSURegs(__gx, texmap,texcoord);
      }
   }
}

#define __GX_SetGenMode(__gx) \
   GX_LOAD_BP_REG(__gx->genMode); \
   __gx->xfFlush = 0

static void __GX_SetDirtyState(struct __gx_regdef *__gx)
{
   if(__gx->dirtyState&0x0001)
   {
      __GX_SetSUTexRegs(__gx);
   }
#ifdef HW_DOL
   if(__gx->dirtyState&0x0002)
   {
      __GX_UpdateBPMask(__gx);
   }
#endif
   if(__gx->dirtyState&0x0004)
   {
      __GX_SetGenMode(__gx);
   }
   if(__gx->dirtyState&0x0008)
   {
      __GX_SetVCD(__gx);
   }
   if(__gx->dirtyState&0x0010)
   {
      u8 setvtx = 0;
      __GX_SetVAT(__gx, setvtx);
   }
   if(__gx->dirtyState&~0xff)
   {
      if(__gx->dirtyState&0x0f00)
      {
         __GX_SetChanColor(__gx);
      }
      if(__gx->dirtyState&0x0100f000)
      {
         __GX_SetChanCntrl(__gx);
      }
      if(__gx->dirtyState&0x02ff0000)
      {
         __GX_SetTexCoordGen(__gx);
      }
      if(__gx->dirtyState&0x04000000)
      {
         __GX_SetMatrixIndex(__gx, 0);
         __GX_SetMatrixIndex(__gx, 5);
      }
   }
   __gx->dirtyState = 0;
}

static void __GX_SendFlushPrim(struct __gx_regdef *__gx)
{
   u32 tmp,tmp2,cnt;

   tmp = (__gx->xfFlush*__gx->xfFlushExp);

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
