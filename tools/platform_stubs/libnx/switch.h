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
#include <stdbool.h>

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

/* Docked or handheld */
typedef enum
{
   AppletOperationMode_Handheld = 0,
   AppletOperationMode_Console  = 1
} AppletOperationMode;
AppletOperationMode appletGetOperationMode(void);

void armDCacheFlush(void *addr, size_t size);

/* The GPU channel the Switch video driver draws through */
typedef uint64_t iova_t;
typedef enum { NvKind_Pitch = 0x0, NvKind_Generic_16BX2 = 0xfe } NvKind;
typedef enum { NvChannelPriority_Low = 50, NvChannelPriority_Medium = 100,
   NvChannelPriority_High = 150 } NvChannelPriority;
typedef enum { NvColorFormat_A8B8G8R8 = 0x0000000100008D00ull } NvColorFormat;
typedef enum { NvLayout_Pitch = 1, NvLayout_Tiled = 2,
   NvLayout_BlockLinear = 3 } NvLayout;

typedef struct { u32 id; u32 value; } NvFence;
typedef struct { u32 num_fences; NvFence fences[4]; } NvMultiFence;
typedef struct { u32 handle; u32 id; u32 size; void *cpu_addr;
   NvKind kind; bool has_init; bool is_cpu_cacheable; } NvMap;
typedef struct { u32 fd; u32 page_size; bool has_init; } NvAddressSpace;
typedef struct { u32 id; u32 value; } NvGpuFenceState;
typedef struct
{
   u32 fd;
   bool has_init;
   NvFence fence;
   u32 fence_incr;
   u32 num_entries;
} NvGpuChannel;
typedef struct { u32 big_page_size; } nvioctl_gpu_characteristics;

#define GPFIFO_ENTRY_NOT_MAIN (1u << 9)

Result nvInitialize(void);
void   nvExit(void);
Result nvGpuInit(void);
void   nvGpuExit(void);
const nvioctl_gpu_characteristics *nvGpuGetCharacteristics(void);
Result nvFenceInit(void);
void   nvFenceExit(void);
Result nvFenceWait(NvFence *f, s32 timeout_us);
Result nvMapInit(void);
void   nvMapExit(void);
Result nvMapCreate(NvMap *m, void *cpu_addr, u32 size, u32 align,
      NvKind kind, bool is_cpu_cacheable);
void   nvMapClose(NvMap *m);
u32    nvMapGetHandle(NvMap *m);
u32    nvMapGetId(NvMap *m);
Result nvAddressSpaceCreate(NvAddressSpace *a, u32 page_size);
void   nvAddressSpaceClose(NvAddressSpace *a);
Result nvAddressSpaceMap(NvAddressSpace *a, u32 nvmap_handle,
      bool is_gpu_cacheable, NvKind kind, iova_t *iova_out);
Result nvAddressSpaceUnmap(NvAddressSpace *a, iova_t iova);
Result nvGpuChannelCreate(NvGpuChannel *c, NvAddressSpace *as,
      NvChannelPriority prio);
void   nvGpuChannelClose(NvGpuChannel *c);
Result nvGpuChannelAppendEntry(NvGpuChannel *c, iova_t start,
      size_t num_cmds, u32 flags, u32 flush_threshold);
Result nvGpuChannelKickoff(NvGpuChannel *c);
u32    nvGpuChannelGetSyncpointId(NvGpuChannel *c);
void   nvGpuChannelGetFence(NvGpuChannel *c, NvFence *fence_out);
void   nvGpuChannelIncrFence(NvGpuChannel *c);
void   nvMultiFenceCreate(NvMultiFence *mf, const NvFence *fence);

/* libnx's own: the window's buffers, as its framebuffer makes them */
typedef struct { int num_fds; int num_ints; } NativeHandle;
typedef struct
{
   u32 width;
   u32 height;
   NvColorFormat color_format;
   NvLayout layout;
   u32 pitch;
   u32 unused;
   u32 offset;
   NvKind kind;
   u32 block_height_log2;
   u32 scan;
   u32 second_field_offset;
   u64 flags;
   u64 size;
   u32 unk[6];
} NvSurface;
typedef struct
{
   NativeHandle header;
   s32 unk0;
   s32 nvmap_id;
   u32 unk2;
   u32 magic;
   u32 pid;
   u32 type;
   u32 usage;
   u32 format;
   u32 ext_format;
   u32 stride;
   u32 total_size;
   u32 num_planes;
   u32 unk12;
   NvSurface planes[3];
   u64 unused;
} NvGraphicBuffer;
#define GRALLOC_USAGE_HW_TEXTURE  0x00000100
#define GRALLOC_USAGE_HW_RENDER   0x00000200
#define GRALLOC_USAGE_HW_COMPOSER 0x00000800

Result nwindowConfigureBuffer(NWindow *nw, s32 slot, NvGraphicBuffer *buf);
Result nwindowDequeueBuffer(NWindow *nw, s32 *out_slot, NvMultiFence *out_fence);
Result nwindowQueueBuffer(NWindow *nw, s32 slot, const NvMultiFence *fence);
Result nwindowReleaseBuffers(NWindow *nw);
Result nwindowSetCrop(NWindow *nw, s32 left, s32 top, s32 right, s32 bottom);
Result nwindowSetSwapInterval(NWindow *nw, u32 swap_interval);

Result   framebufferCreate(Framebuffer *fb, NWindow *win,
      u32 width, u32 height, u32 format, u32 num_fbs);
Result   framebufferMakeLinear(Framebuffer *fb);
void    *framebufferBegin(Framebuffer *fb, u32 *out_stride);
void     framebufferEnd(Framebuffer *fb);
void     framebufferClose(Framebuffer *fb);


/* the kernel random source the crypto's random path draws on */
void randomGet(void *buf, size_t len);

/* switch_joypad.c: pads, Joy-Con assignment and rumble; libnx's pad
 * helpers are inline there, plain functions here */
typedef enum
{
   HidNpadIdType_No1      = 0,
   HidNpadIdType_Handheld = 0x20
} HidNpadIdType;

enum
{
   HidNpadStyleTag_NpadFullKey  = 1u << 0,
   HidNpadStyleTag_NpadHandheld = 1u << 1,
   HidNpadStyleTag_NpadJoyDual  = 1u << 2,
   HidNpadStyleTag_NpadJoyLeft  = 1u << 3,
   HidNpadStyleTag_NpadJoyRight = 1u << 4
};
#define HidNpadStyleSet_NpadStandard (HidNpadStyleTag_NpadFullKey \
   | HidNpadStyleTag_NpadHandheld | HidNpadStyleTag_NpadJoyDual   \
   | HidNpadStyleTag_NpadJoyLeft  | HidNpadStyleTag_NpadJoyRight)

#define HidNpadButton_A       (1ull << 0)
#define HidNpadButton_B       (1ull << 1)
#define HidNpadButton_X       (1ull << 2)
#define HidNpadButton_Y       (1ull << 3)
#define HidNpadButton_StickL  (1ull << 4)
#define HidNpadButton_StickR  (1ull << 5)
#define HidNpadButton_L       (1ull << 6)
#define HidNpadButton_R       (1ull << 7)
#define HidNpadButton_Plus    (1ull << 10)
#define HidNpadButton_Minus   (1ull << 11)
#define HidNpadButton_Left    (1ull << 12)
#define HidNpadButton_Up      (1ull << 13)
#define HidNpadButton_Right   (1ull << 14)
#define HidNpadButton_Down    (1ull << 15)
#define HidNpadButton_LeftSL  (1ull << 24)
#define HidNpadButton_LeftSR  (1ull << 25)
#define HidNpadButton_RightSL (1ull << 26)
#define HidNpadButton_RightSR (1ull << 27)

#define HidNpadJoyHoldType_Horizontal 1
#define HidNpadAttribute_IsConnected  (1u << 0)
#define HidDeviceTypeBits_JoyLeft     (1u << 4)
#define HidDeviceTypeBits_JoyRight    (1u << 5)

typedef struct HidAnalogStickState { s32 x; s32 y; } HidAnalogStickState;
typedef struct HidVibrationDeviceHandle { u32 type_value; } HidVibrationDeviceHandle;
typedef struct HidVibrationValue
{
   float amp_low;
   float freq_low;
   float amp_high;
   float freq_high;
} HidVibrationValue;

typedef struct
{
   u8                  id_mask;
   bool                read_handheld;
   u32                 attributes;
   u64                 buttons_cur;
   HidAnalogStickState sticks[2];
} PadState;

void padConfigureInput(u32 max_players, u32 style_set);
void padInitialize(PadState *pad, int id);
void padInitializeDefault(PadState *pad);
void padUpdate(PadState *pad);
bool padIsHandheld(const PadState *pad);
bool padIsNpadActive(const PadState *pad, HidNpadIdType id);
bool padIsConnected(const PadState *pad);
u64  padGetButtons(const PadState *pad);
HidAnalogStickState padGetStickPos(const PadState *pad, unsigned i);

void   hidSetNpadJoyHoldType(int type);
u32    hidGetNpadDeviceType(HidNpadIdType id);
Result hidSetNpadJoyAssignmentModeSingleByDefault(HidNpadIdType id);
Result hidSetNpadJoyAssignmentModeDual(HidNpadIdType id);
Result hidMergeSingleJoyAsDualJoy(HidNpadIdType id1, HidNpadIdType id2);
Result hidInitializeVibrationDevices(HidVibrationDeviceHandle *handles,
      s32 total, HidNpadIdType id, u32 style);
Result hidSendVibrationValues(const HidVibrationDeviceHandle *handles,
      const HidVibrationValue *values, s32 count);

#endif
