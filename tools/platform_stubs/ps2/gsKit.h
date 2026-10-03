/* Compile-only PS2 stub for the matrix's ps2 video lane.
 *
 * ps2_gfx.c reaches a compiler only inside the PS2 toolchain, so
 * a change to a struct or a signature it touches is green
 * everywhere else until that job runs. This carries the gsKit
 * declarations the driver names - and only those - each in the
 * shape the real header gives it, so the lane rejects what the
 * SDK would reject. What nothing here names is left out, which is
 * why a mode or format constant may be missing. Add it from the
 * real header rather than inventing a value. */
#ifndef STUB_PS2_GSKIT
#define STUB_PS2_GSKIT
#include <tamtypes.h>
#include <dmaKit.h>

enum ETransferMode {
	ETM_INLINE = 0,
	ETM_DIRECT
};

#define GIF_TAG(NLOOP,EOP,PRE,PRIM,FLG,NREG) \
((u64)(NLOOP)	<< 0)	| \
((u64)(EOP)	<< 15)	| \
((u64)(PRE)	<< 46)	| \
((u64)(PRIM)	<< 47)	| \
((u64)(FLG)	<< 58)	| \
((u64)(NREG)	<< 60);

#define GSKIT_ALLOC_SYSBUFFER 0x00
#define GS_ASPECT_16_9 0x1
#define GS_ASPECT_4_3 0x0
#define GS_ATEST_OFF 0x03
#define GS_ATEST_ON 0x04
#define GS_DISPFB2	((volatile u64 *)(0x12000090))
#define GS_DISPLAY1        ((volatile u64 *)(0x12000080))
#define GS_DISPLAY2        ((volatile u64 *)(0x120000a0))
#define GS_FIELD 0x00
#define GS_FIELD_EVEN   0x03
#define GS_FIELD_NORMAL 0x00
#define GS_FIELD_ODD    0x02
#define GS_FILTER_LINEAR  0x01
#define GS_FILTER_NEAREST 0x00
#define GS_FRAME 0x01
#define GS_INTERLACED 0x01
#define GS_MODE_DTV_480P  0x50
#define GS_MODE_DTV_576P  0x53
#define GS_MODE_NTSC 0x02
#define GS_MODE_PAL  0x03
#define GS_NONINTERLACED 0x00
#define GS_ONESHOT 0x01
#define GS_OS_PER 0x01
#define GS_PER_OS 0x00
#define GS_PSMZ_16S 0x0A
#define GS_PSM_CT16 0x02
#define GS_PSM_CT32 0x00
#define GS_PSM_T8 0x13
#define GS_RENDER_QUEUE_OS_POOLSIZE 1024 * 1024
#define GS_RENDER_QUEUE_PER_POOLSIZE 1024 * 256

#define GS_SETREG_ALPHA(A, B, C, D, FIX) \
	((u64)(A)	<< 0)	| \
	((u64)(B)	<< 2)	| \
	((u64)(C)	<< 4)	| \
	((u64)(D)	<< 6)	| \
	((u64)(FIX)	<< 32)

#define GS_SETREG_RGBA(r, g, b, a) \
((u64)(r)        | ((u64)(g) << 8) | ((u64)(b) << 16) | ((u64)(a) << 24))

#define GS_SETREG_RGBAQ(r, g, b, a, q) \
((u64)(r)        | ((u64)(g) << 8) | ((u64)(b) << 16) | \
((u64)(a) << 24) | ((u64)(q) << 32))

#define GS_SETTING_OFF 0x00
#define GS_SETTING_ON 0x01
#define GS_ZTEST_OFF 0x01

struct gsBGColor
{
	u8 Red;
	u8 Green;
	u8 Blue;
};

struct gsClamp
{
	u8 WMS;
	u8 WMT;
	int MINU;
	int MAXU;
	int MINV;
	int MAXV;
};

int gsKit_add_vsync_handler(int (*vsync_callback)(int));
short int gsKit_check_rom(void);
void gsKit_finish(void);
void gsKit_remove_vsync_handler(int callback_id);
u32 gsKit_texture_size(int width, int height, int psm);
u32 gsKit_texture_size_ee(int width, int height, int psm);

struct gsQueue
{
	void *pool[2] __attribute__ ((aligned (64)));
	void *pool_cur;
	void *pool_max[2];
	int last_type;
	void *last_tag;
	int same_obj;
	void *dma_tag;
	u32 tag_size;
	u8 mode;
	int dbuf;
};

struct gsRegisters {
u64 SIGNAL:      1 __attribute__((packed));
u64 FINISH:      1 __attribute__((packed));
u64 HSINT:       1 __attribute__((packed));
u64 VSINT:       1 __attribute__((packed));
u64 reserved04:  3 __attribute__((packed));
u64 pad07:       1 __attribute__((packed));
u64 FLUSH:       1 __attribute__((packed));
u64 RESET:       1 __attribute__((packed));
u64 pad10:       2 __attribute__((packed));
u64 NFIELD:      1 __attribute__((packed));
u64 FIELD:       1 __attribute__((packed));
u64 FIFO:        2 __attribute__((packed));
u64 REV:         8 __attribute__((packed));
u64 ID:          8 __attribute__((packed));
u64 pad32:      32 __attribute__((packed));
};

struct gsTest
{
	u8 ATE;
	u8 ATST;
	u8 AREF;
	u8 AFAIL;
	u8 DATE;
	u8 DATM;
	u8 ZTE;
	u8 ZTST;
};

struct gsTexture
{
	u32 Width;
	u32 Height;
	u8	PSM;
	u8	ClutPSM;
	u32	TBW;
	u32 *Mem;
	u32 *Clut;
	u32 Vram;
	u32 VramClut;
	u32 Filter;
	u8 ClutStorageMode;
	u8	Delayed;
};

typedef struct gsBGColor GSBGCOLOR;
typedef struct gsClamp GSCLAMP;
typedef struct gsQueue GSQUEUE;
typedef struct gsRegisters GSREG;
typedef struct gsTest GSTEST;
typedef struct gsTexture GSTEXTURE;

#define GS_SET_DISPFB2(FBP,FBW,PSM,DBX,DBY) \
*GS_DISPFB2 = \
((u64)(FBP)     << 0)   | \
((u64)(FBW)     << 9)   | \
((u64)(PSM)     << 15)  | \
((u64)(DBX)     << 32)  | \
((u64)(DBY)     << 43)

#define GS_SET_DISPLAY1(DX,DY,MAGH,MAGV,DW,DH) \
*GS_DISPLAY1 = \
((u64)(DX)      << 0)   | \
((u64)(DY)      << 12)  | \
((u64)(MAGH)    << 23)  | \
((u64)(MAGV)    << 27)  | \
((u64)(DW)      << 32)  | \
((u64)(DH)      << 44)

#define GS_SET_DISPLAY2(DX,DY,MAGH,MAGV,DW,DH) \
*GS_DISPLAY2 = \
((u64)(DX)      << 0)   | \
((u64)(DY)      << 12)  | \
((u64)(MAGH)    << 23)  | \
((u64)(MAGV)    << 27)  | \
((u64)(DW)      << 32)  | \
((u64)(DH)      << 44)

struct gsGlobal
{
	s16 Mode;
	s16 Interlace;
	s16 Field;
	u32 CurrentPointer;
	u32 TexturePointer;
	u8 Dithering;
	s8 DitherMatrix[16];
	u8 DoubleBuffering;
	u8 ZBuffering;
	u32 ScreenBuffer[2];
	u32 ZBuffer;
	u8 EvenOrOdd;
	u8 DrawOrder;
	u8 FirstFrame;
	u8 DrawField;
	u8 ActiveBuffer;
	volatile u8 LockBuffer;
	int Width;
	int Height;
	int Aspect;
	int OffsetX;
	int OffsetY;
	int StartX;
	int StartY;
	int StartXOffset;
	int StartYOffset;
	int MagH;
	int MagV;
	int DW;
int DH;
	GSBGCOLOR *BGColor;
	GSTEST *Test;
	GSCLAMP *Clamp;
	GSQUEUE *CurQueue;
	GSQUEUE *Per_Queue;
	GSQUEUE *Os_Queue;
	int Os_AllocSize;
	int Per_AllocSize;
	void *dma_misc __attribute__ ((aligned (64)));
	int PSM;
	int PSMZ;
	int PrimContext;
	int PrimFogEnable;
	int PrimAAEnable;
	int PrimAlphaEnable;
	u64 PrimAlpha;
	u8  PABE;
};

typedef struct gsGlobal GSGLOBAL;
unsigned int gsKit_TexManager_bind(GSGLOBAL * gsGlobal, GSTEXTURE * tex);
void gsKit_TexManager_invalidate(GSGLOBAL * gsGlobal, GSTEXTURE * tex);
void gsKit_TexManager_nextFrame(GSGLOBAL * gsGlobal);
void gsKit_TexManager_setmode(GSGLOBAL * gsGlobal, enum ETransferMode mode);
void gsKit_clear(GSGLOBAL *gsGlobal, u64 Color);
void gsKit_deinit_global(GSGLOBAL *gsGlobal);
void gsKit_display_buffer(GSGLOBAL *gsGlobal);
GSGLOBAL *gsKit_init_global_custom(int Os_AllocSize, int Per_AllocSize);
void gsKit_init_screen(GSGLOBAL *gsGlobal);
void gsKit_mode_switch(GSGLOBAL *gsGlobal, u8 mode);
void gsKit_prim_sprite_texture_3d(GSGLOBAL *gsGlobal, const GSTEXTURE *Texture, float x1, float y1, int iz1, float u1, float v1, float x2, float y2, int iz2, float u2, float v2, u64 color);
void gsKit_queue_exec(GSGLOBAL *gsGlobal);
void gsKit_set_display_offset(GSGLOBAL *gsGlobal, int x, int y);
void gsKit_set_primalpha(GSGLOBAL *gsGlobal, u64 AlphaMode, u8 PerPixel);
void gsKit_set_test(GSGLOBAL *gsGlobal, u8 Preset);
void gsKit_setactive(GSGLOBAL *gsGlobal);
u32 gsKit_vram_alloc(GSGLOBAL *gsGlobal, u32 size, u8 type);
void gsKit_vram_clear(GSGLOBAL *gsGlobal);

#define gsKit_init_global() \
		gsKit_init_global_custom(GS_RENDER_QUEUE_OS_POOLSIZE, GS_RENDER_QUEUE_PER_POOLSIZE);

#define gsKit_prim_sprite_texture(gsGlobal, Texture,	x1, y1, u1, v1,		\
							x2, y2, u2, v2,		\
							z, color)		\
	gsKit_prim_sprite_texture_3d(gsGlobal, Texture, x1, y1, z, u1, v1,	\
							x2, y2, z, u2, v2, color);

#endif
