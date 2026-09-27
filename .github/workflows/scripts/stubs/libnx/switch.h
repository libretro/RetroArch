/* Stand-in for devkitPro's libnx <switch.h>.
 *
 * Only what the translation units checked here name, declared with the
 * shapes libnx gives them. As the note at the top of compile-matrix.sh
 * says, this does not prove the real SDK matches: it proves RetroArch's
 * own Switch code is well formed and that every symbol it uses is one it
 * has arranged to have. That is the class of breakage these lanes are
 * for - a driver naming a struct member or a setting that moved.
 */
#ifndef LIBNX_STUB_SWITCH_H
#define LIBNX_STUB_SWITCH_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  s32;
typedef uint32_t Result;

/* mem_stats.c */
typedef enum { InfoType_TotalMemorySize = 6, InfoType_UsedMemorySize = 7 } InfoType;
#define CUR_PROCESS_HANDLE 0xFFFF8001
#define R_SUCCEEDED(res) ((res) == 0)
#define R_FAILED(res)    ((res) != 0)
Result svcGetInfo(u64 *out, u32 id, u32 handle, u64 sub);

/* The window and framebuffer the Switch video driver draws through */
typedef struct NWindow NWindow;

typedef struct
{
   void *buf;
   u32   stride;
} Framebuffer;

typedef enum
{
   PIXEL_FORMAT_RGBA_8888 = 1
} PixelFormat;

NWindow *nwindowGetDefault(void);
Result   nwindowSetDimensions(NWindow *nw, u32 width, u32 height);

Result   framebufferCreate(Framebuffer *fb, NWindow *win,
      u32 width, u32 height, u32 format, u32 num_fbs);
Result   framebufferMakeLinear(Framebuffer *fb);
void    *framebufferBegin(Framebuffer *fb, u32 *out_stride);
void     framebufferEnd(Framebuffer *fb);
void     framebufferClose(Framebuffer *fb);

#endif
