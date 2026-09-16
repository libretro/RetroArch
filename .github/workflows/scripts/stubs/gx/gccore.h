/* Stand-in for devkitPro's libogc <gccore.h>, for the GX video driver.
 *
 * Prototypes and values follow libogc's, so the driver's calls are
 * checked as it makes them. As the note at the top of compile-matrix.sh
 * says, this does not prove the real SDK matches: it proves RetroArch's
 * own GX code is well formed and uses only what it has arranged to
 * have. Pinning the surface here rather than fetching the SDK is
 * deliberate - the 3DS driver no longer compiles against a current
 * libctru, whose gspSubmitGxCommand() changed shape under it, and a
 * lane that goes red when somebody else's headers move tells us
 * nothing about this tree.
 */
#ifndef LIBOGC_STUB_GCCORE_H
#define LIBOGC_STUB_GCCORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;
typedef volatile uint8_t  vu8;
typedef volatile uint16_t vu16;
typedef volatile uint32_t vu32;
typedef uint32_t lwpq_t;
typedef uint32_t lwp_t;
typedef uint32_t mutex_t;
typedef uint32_t syswd_t;
typedef struct { u32 dummy[16]; } GXFifoObj;
typedef void (*VIRetraceCallback)(u32 retraceCnt);
typedef void (*VIPositionCallback)(s16 x, s16 y);
typedef f32 Mtx[3][4];
typedef f32 Mtx44[4][4];

#define ATTRIBUTE_ALIGN(v) __attribute__((aligned(v)))
#define ATTRIBUTE_PACKED   __attribute__((packed))
#define LWP_TQUEUE_NULL   0xffffffff
#define SYS_BASE_CACHED   0x80000000
#define SYS_BASE_UNCACHED 0xc0000000
/* uintptr_t, not u32: on the console a pointer is 32 bits and libogc's
 * own cast is exact, while the host running this check is 64-bit and
 * would lose half of one. The arithmetic is what matters here. */
#define MEM_K0_TO_K1(x)   ((void*)((uintptr_t)(x) + (SYS_BASE_UNCACHED - SYS_BASE_CACHED)))
#define MEM_K1_TO_K0(x)   ((void*)((uintptr_t)(x) - (SYS_BASE_UNCACHED - SYS_BASE_CACHED)))
#define VIDEO_PadFramebufferWidth(w) ((u16)(((u16)(w) + 15) & ~15))
#define VI_TVMODE(fmt, mode) (((fmt) << 2) + (mode))

typedef struct { u8 r, g, b, a; } GXColor;
typedef struct { u32 dummy[8]; } GXTexObj;
typedef struct
{
   u32 viTVMode;
   u16 fbWidth, efbHeight, xfbHeight;
   u16 viXOrigin, viYOrigin, viWidth, viHeight;
   u32 xfbMode;
   u8  field_rendering, aa;
   u8  sample_pattern[12][2];
   u8  vfilter[7];
} GXRModeObj;

typedef enum { CONF_VIDEO_NTSC = 0, CONF_VIDEO_PAL, CONF_VIDEO_MPAL } CONFVideoMode;

#ifndef COLOR_BLACK
#define COLOR_BLACK (0x10801080)
#endif
#ifndef VI_DISPLAY_PIX_SZ
#define VI_DISPLAY_PIX_SZ 2
#endif
#ifndef VI_EURGB60
#define VI_EURGB60 5
#endif
#ifndef VI_INTERLACE
#define VI_INTERLACE 0
#endif
#ifndef VI_MAX_HEIGHT_EURGB60
#define VI_MAX_HEIGHT_EURGB60 VI_MAX_HEIGHT_NTSC
#endif
#ifndef VI_MAX_HEIGHT_MPAL
#define VI_MAX_HEIGHT_MPAL 480
#endif
#ifndef VI_MAX_HEIGHT_NTSC
#define VI_MAX_HEIGHT_NTSC 480
#endif
#ifndef VI_MAX_HEIGHT_PAL
#define VI_MAX_HEIGHT_PAL 576
#endif
#ifndef VI_MAX_WIDTH_EURGB60
#define VI_MAX_WIDTH_EURGB60 VI_MAX_WIDTH_NTSC
#endif
#ifndef VI_MAX_WIDTH_MPAL
#define VI_MAX_WIDTH_MPAL 720
#endif
#ifndef VI_MAX_WIDTH_NTSC
#define VI_MAX_WIDTH_NTSC 720
#endif
#ifndef VI_MAX_WIDTH_PAL
#define VI_MAX_WIDTH_PAL 720
#endif
#ifndef VI_MPAL
#define VI_MPAL 2
#endif
#ifndef VI_NON_INTERLACE
#define VI_NON_INTERLACE 1
#endif
#ifndef VI_NTSC
#define VI_NTSC 0
#endif
#ifndef VI_PAL
#define VI_PAL 1
#endif
#ifndef VI_PROGRESSIVE
#define VI_PROGRESSIVE 2
#endif
#ifndef VI_XFBMODE_DF
#define VI_XFBMODE_DF 1
#endif
#ifndef VI_XFBMODE_SF
#define VI_XFBMODE_SF 0
#endif
typedef struct { u32 dummy[8]; } syssram;

/* Drawing primitives libogc gives as inline functions */
static inline void GX_Position3f32(f32 x, f32 y, f32 z) { (void)x; (void)y; (void)z; }
static inline void GX_Position1x8(u8 i) { (void)i; }
static inline void GX_Color4u8(u8 r, u8 g, u8 b, u8 a) { (void)r; (void)g; (void)b; (void)a; }
static inline void GX_Color1x8(u8 i) { (void)i; }
static inline void GX_TexCoord2f32(f32 s, f32 t) { (void)s; (void)t; }
static inline void GX_TexCoord1x8(u8 i) { (void)i; }
static inline void GX_End(void) { }
void VIDEO_SetGamma(int gamma);

/* The matrix helpers the driver sets its projection up with */
void guOrtho(Mtx44 mt, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);
void guMtxIdentity(Mtx mt);
void c_guMtxConcat(Mtx a, Mtx b, Mtx ab);
void DCFlushRange(void *startaddress, u32 len);
void DCInvalidateRange(void *startaddress, u32 len);
void guMtxRotDeg(Mtx mt, const char axis, f32 deg);

/* The threading calls libretro-common's gx_defines.h maps OS* onto */
void LWP_InitQueue(lwpq_t *queue);
void LWP_CloseQueue(lwpq_t queue);
void LWP_ThreadSignal(lwpq_t queue);
void LWP_ThreadBroadcast(lwpq_t queue);
s32  LWP_ThreadSleep(lwpq_t queue);
s32  LWP_MutexInit(mutex_t *mutex, bool use_recursive);
s32  LWP_MutexLock(mutex_t mutex);
s32  LWP_MutexTryLock(mutex_t mutex);
s32  LWP_MutexUnlock(mutex_t mutex);
s32  LWP_MutexDestroy(mutex_t mutex);
s32  LWP_CondInit(lwpq_t *cond);
s32  LWP_CondWait(lwpq_t cond, mutex_t mutex);
s32  LWP_CondDestroy(lwpq_t cond);
s32  LWP_CreateThread(lwp_t *thethread, void *(*entry)(void *),
      void *arg, void *stackbase, u32 stack_size, u8 prio);
s32  LWP_JoinThread(lwp_t thethread, void **value_ptr);
void VIDEO_SetTrapFilter(bool enable);

#ifndef CONF_ASPECT_4_3
#define CONF_ASPECT_4_3 0
#endif
#ifndef GX_AF_NONE
#define GX_AF_NONE 2
#endif
#ifndef GX_ALWAYS
#define GX_ALWAYS 7
#endif
#ifndef GX_BL_INVSRCALPHA
#define GX_BL_INVSRCALPHA 5
#endif
#ifndef GX_BL_SRCALPHA
#define GX_BL_SRCALPHA 4
#endif
#ifndef GX_BM_BLEND
#define GX_BM_BLEND 1
#endif
#ifndef GX_CLAMP
#define GX_CLAMP 0
#endif
#ifndef GX_CLIP_DISABLE
#define GX_CLIP_DISABLE 1
#endif
#ifndef GX_CLR_RGBA
#define GX_CLR_RGBA 1
#endif
#ifndef GX_COLOR0A0
#define GX_COLOR0A0 4
#endif
#ifndef GX_CULL_NONE
#define GX_CULL_NONE 0
#endif
#ifndef GX_DF_NONE
#define GX_DF_NONE 0
#endif
#ifndef GX_DIRECT
#define GX_DIRECT 1
#endif
#ifndef GX_DISABLE
#define GX_DISABLE 0
#endif
#ifndef GX_ENABLE
#define GX_ENABLE 1
#endif
#ifndef GX_F32
#define GX_F32 4
#endif
#ifndef GX_FALSE
#define GX_FALSE 0
#endif
#ifndef GX_INDEX8
#define GX_INDEX8 2
#endif
#ifndef GX_LIGHTNULL
#define GX_LIGHTNULL 0x000
#endif
#ifndef GX_LINEAR
#define GX_LINEAR 1
#endif
#ifndef GX_LO_CLEAR
#define GX_LO_CLEAR 0
#endif
#ifndef GX_MAX_Z24
#define GX_MAX_Z24 0x00ffffff
#endif
#ifndef GX_MODULATE
#define GX_MODULATE 0
#endif
#ifndef GX_NEAR
#define GX_NEAR 0
#endif
#ifndef GX_ORTHOGRAPHIC
#define GX_ORTHOGRAPHIC 1
#endif
#ifndef GX_PF_RGB8_Z24
#define GX_PF_RGB8_Z24 0
#endif
#ifndef GX_PNMTX0
#define GX_PNMTX0 0
#endif
#ifndef GX_PNMTX1
#define GX_PNMTX1 3
#endif
#ifndef GX_POS_XYZ
#define GX_POS_XYZ 1
#endif
#ifndef GX_RGBA8
#define GX_RGBA8 5
#endif
#ifndef GX_SRC_REG
#define GX_SRC_REG 0
#endif
#ifndef GX_SRC_VTX
#define GX_SRC_VTX 1
#endif
#ifndef GX_TEVSTAGE0
#define GX_TEVSTAGE0 0
#endif
#ifndef GX_TEXCOORD0
#define GX_TEXCOORD0 0x0
#endif
#ifndef GX_TEXMAP0
#define GX_TEXMAP0 0
#endif
#ifndef GX_TEX_ST
#define GX_TEX_ST 1
#endif
#ifndef GX_TF_RGB565
#define GX_TF_RGB565 0x4
#endif
#ifndef GX_TF_RGB5A3
#define GX_TF_RGB5A3 0x5
#endif
#ifndef GX_TF_RGBA8
#define GX_TF_RGBA8 0x6
#endif
#ifndef GX_TRIANGLESTRIP
#define GX_TRIANGLESTRIP 0x98
#endif
#ifndef GX_TRUE
#define GX_TRUE 1
#endif
#ifndef GX_VA_CLR0
#define GX_VA_CLR0 11
#endif
#ifndef GX_VA_POS
#define GX_VA_POS 9
#endif
#ifndef GX_VA_TEX0
#define GX_VA_TEX0 13
#endif
#ifndef GX_VTXFMT0
#define GX_VTXFMT0 0
#endif
#ifndef GX_ZC_LINEAR
#define GX_ZC_LINEAR 0
#endif
#ifndef LWP_TQUEUE_NULL
#define LWP_TQUEUE_NULL 0xffffffff
#endif

s32 CONF_GetAspectRatio(void);
s32 CONF_GetDisplayOffsetH(s8 *offset);
s32 CONF_GetEuRGB60(void);
s32 CONF_GetProgressiveScan(void);
s32 CONF_GetVideo(void);
void GX_AbortFrame(void);
void GX_Begin(u8 primitve,u8 vtxfmt,u16 vtxcnt);
void GX_BeginDispList(void *list,u32 size);
void GX_CallDispList(void *list,u32 nbytes);
void GX_ClearVtxDesc(void);
void GX_CopyDisp(void *dest,u8 clear);
void GX_DrawDone(void);
u32 GX_EndDispList(void);
void GX_Flush(void);
f32 GX_GetYScaleFactor(u16 efbHeight,u16 xfbHeight);
GXFifoObj* GX_Init(void *base,u32 size);
void GX_InitTexObj(GXTexObj *obj,void *img_ptr,u16 wd,u16 ht,u8 fmt,u8 wrap_s,u8 wrap_t,u8 mipmap);
void GX_InitTexObjFilterMode(GXTexObj *obj,u8 minfilt,u8 magfilt);
void GX_InvVtxCache(void);
void GX_InvalidateTexAll(void);
void GX_LoadPosMtxImm(Mtx mt,u32 pnidx);
void GX_LoadProjectionMtx(Mtx44 mt,u8 type);
void GX_LoadTexObj(GXTexObj *obj,u8 mapid);
void GX_PeekARGB(u16 x,u16 y,GXColor *color);
void GX_PokeARGB(u16 x,u16 y,GXColor color);
void GX_SetAlphaUpdate(u8 enable);
void GX_SetArray(u32 attr,void *ptr,u8 stride);
void GX_SetBlendMode(u8 type,u8 src_fact,u8 dst_fact,u8 op);
void GX_SetChanCtrl(s32 channel,u8 enable,u8 ambsrc,u8 matsrc,u8 litmask,u8 diff_fn,u8 attn_fn);
void GX_SetClipMode(u8 mode);
void GX_SetColorUpdate(u8 enable);
void GX_SetCopyClear(GXColor color,u32 zvalue);
void GX_SetCopyFilter(u8 aa,u8 sample_pattern[12][2],u8 vf,u8 vfilter[7]);
void GX_SetCullMode(u8 mode);
void GX_SetCurrentMtx(u32 mtx);
void GX_SetDispCopyDst(u16 wd,u16 ht);
void GX_SetDispCopyGamma(u8 gamma);
void GX_SetDispCopySrc(u16 left,u16 top,u16 wd,u16 ht);
u32 GX_SetDispCopyYScale(f32 yscale);
void GX_SetFieldMode(u8 field_mode,u8 half_aspect_ratio);
void GX_SetNumChans(u8 num);
void GX_SetNumTexGens(u32 nr);
void GX_SetPixelFmt(u8 pix_fmt,u8 z_fmt);
void GX_SetTevOp(u8 tevstage,u8 mode);
void GX_SetTevOrder(u8 tevstage,u8 texcoord,u32 texmap,u8 color);
void GX_SetViewportJitter(f32 xOrig,f32 yOrig,f32 wd,f32 ht,f32 nearZ,f32 farZ,u32 field);
void GX_SetVtxAttrFmt(u8 vtxfmt,u32 vtxattr,u32 comptype,u32 compsize,u32 frac);
void GX_SetVtxDesc(u8 attr,u8 type);
void GX_SetZMode(u8 enable,u8 func,u8 update_enable);
u32 SYS_GetArena1Size(void);
void VIDEO_ClearFrameBuffer(GXRModeObj *rmode,void *fb,u32 color);
void VIDEO_Configure(GXRModeObj *rmode);
void VIDEO_Flush(void);
u32 VIDEO_GetCurrentTvMode(void);
u32 VIDEO_GetNextField(void);
GXRModeObj * VIDEO_GetPreferredMode(GXRModeObj *mode);
u32 VIDEO_HaveComponentCable(void);
void VIDEO_Init(void);
void VIDEO_SetBlack(bool black);
void VIDEO_SetNextFramebuffer(void *fb);
VIRetraceCallback VIDEO_SetPostRetraceCallback(VIRetraceCallback callback);
void VIDEO_WaitVSync(void);

#endif
