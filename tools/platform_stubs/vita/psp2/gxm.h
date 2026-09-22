/* Compile-only Vita stub for the matrix's gxm video lane.
 *
 * gxm_gfx.c reaches a compiler only inside the Vita toolchain, so
 * a change to a struct or a signature it touches is green
 * everywhere else until that job runs. This carries the SceGxm
 * declarations the driver names - and only those - each in the
 * shape the real header gives it, so the lane rejects what the
 * SDK would reject. The enums keep the SDK's values; what nothing
 * here names is left out, which is why a format constant may be
 * missing. Add it from the real header rather than inventing a
 * value. */
#ifndef STUB_PSP2_GXM
#define STUB_PSP2_GXM
#include <stdint.h>
#include <psp2/types.h>

#define SCE_GXM_COLOR_SURFACE_ALIGNMENT        4U
#define SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE       (512 * 1024)
#define SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE  (16 * 1024)
#define SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE           (16 * 1024 * 1024)
#define SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE            (128 * 1024)
#define SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE         (2 * 1024 * 1024)
#define SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT 16U
#define SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE           (2 * 1024)
#define SCE_GXM_PALETTE_ALIGNMENT              64U
#define SCE_GXM_TEXTURE_ALIGNMENT              16U
#define SCE_GXM_TILE_SHIFTX 5U
#define SCE_GXM_TILE_SHIFTY 5U

typedef enum SceGxmAttributeFormat {
	SCE_GXM_ATTRIBUTE_FORMAT_U8,
	SCE_GXM_ATTRIBUTE_FORMAT_S8,
	SCE_GXM_ATTRIBUTE_FORMAT_U16,
	SCE_GXM_ATTRIBUTE_FORMAT_S16,
	SCE_GXM_ATTRIBUTE_FORMAT_U8N,
	SCE_GXM_ATTRIBUTE_FORMAT_S8N,
	SCE_GXM_ATTRIBUTE_FORMAT_U16N,
	SCE_GXM_ATTRIBUTE_FORMAT_S16N,
	SCE_GXM_ATTRIBUTE_FORMAT_F16,
	SCE_GXM_ATTRIBUTE_FORMAT_F32,
	SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED
} SceGxmAttributeFormat;

typedef enum SceGxmBlendFactor {
	SCE_GXM_BLEND_FACTOR_ZERO,
	SCE_GXM_BLEND_FACTOR_ONE,
	SCE_GXM_BLEND_FACTOR_SRC_COLOR,
	SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
	SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
	SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	SCE_GXM_BLEND_FACTOR_DST_COLOR,
	SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
	SCE_GXM_BLEND_FACTOR_DST_ALPHA,
	SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
	SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE,
	SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE
} SceGxmBlendFactor;

typedef enum SceGxmBlendFunc {
	SCE_GXM_BLEND_FUNC_NONE,
	SCE_GXM_BLEND_FUNC_ADD,
	SCE_GXM_BLEND_FUNC_SUBTRACT,
	SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT,
	SCE_GXM_BLEND_FUNC_MIN,
	SCE_GXM_BLEND_FUNC_MAX
} SceGxmBlendFunc;

typedef struct SceGxmBlendInfo {
	uint8_t colorMask;
	uint8_t colorFunc : 4;
	uint8_t alphaFunc : 4;
	uint8_t colorSrc : 4;
	uint8_t colorDst : 4;
	uint8_t alphaSrc : 4;
	uint8_t alphaDst : 4;
} SceGxmBlendInfo;

typedef enum SceGxmColorBaseFormat {
   SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8     = 0x00000000,
   SCE_GXM_COLOR_BASE_FORMAT_U8U8U8       = 0x10000000,
   SCE_GXM_COLOR_BASE_FORMAT_U5U6U5       = 0x30000000,
   SCE_GXM_COLOR_BASE_FORMAT_U1U5U5U5     = 0x40000000,
   SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4     = 0x50000000,
   SCE_GXM_COLOR_BASE_FORMAT_U8U3U3U2     = 0x60000000,
   SCE_GXM_COLOR_BASE_FORMAT_F16          = 0xF0000000,
   SCE_GXM_COLOR_BASE_FORMAT_F16F16       = 0x00800000,
   SCE_GXM_COLOR_BASE_FORMAT_F32          = 0x10800000,
   SCE_GXM_COLOR_BASE_FORMAT_S16          = 0x20800000,
   SCE_GXM_COLOR_BASE_FORMAT_S16S16       = 0x30800000,
   SCE_GXM_COLOR_BASE_FORMAT_U16          = 0x40800000,
   SCE_GXM_COLOR_BASE_FORMAT_U16U16       = 0x50800000,
   SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10  = 0x60800000,
   SCE_GXM_COLOR_BASE_FORMAT_U8           = 0x80800000,
   SCE_GXM_COLOR_BASE_FORMAT_S8           = 0x90800000,
   SCE_GXM_COLOR_BASE_FORMAT_S5S5U6       = 0xA0800000,
   SCE_GXM_COLOR_BASE_FORMAT_U8U8         = 0xB0800000,
   SCE_GXM_COLOR_BASE_FORMAT_S8S8         = 0xC0800000,
   SCE_GXM_COLOR_BASE_FORMAT_U8S8S8U8     = 0xD0800000,
   SCE_GXM_COLOR_BASE_FORMAT_S8S8S8S8     = 0xE0800000,
   SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16 = 0x01000000,
   SCE_GXM_COLOR_BASE_FORMAT_F32F32       = 0x11000000,
   SCE_GXM_COLOR_BASE_FORMAT_F11F11F10    = 0x21000000,
   SCE_GXM_COLOR_BASE_FORMAT_SE5M9M9M9    = 0x31000000,
   SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10  = 0x41000000
} SceGxmColorBaseFormat;

typedef enum SceGxmColorMask {
   SCE_GXM_COLOR_MASK_A    = (1 << 0),
   SCE_GXM_COLOR_MASK_R    = (1 << 1),
   SCE_GXM_COLOR_MASK_G    = (1 << 2),
   SCE_GXM_COLOR_MASK_B    = (1 << 3),
   SCE_GXM_COLOR_MASK_ALL  = (SCE_GXM_COLOR_MASK_A | SCE_GXM_COLOR_MASK_B | SCE_GXM_COLOR_MASK_G | SCE_GXM_COLOR_MASK_R)
} SceGxmColorMask;

typedef enum SceGxmColorSurfaceScaleMode {
   SCE_GXM_COLOR_SURFACE_SCALE_NONE           = 0x00000000u,
   SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE = 0x00000001u
} SceGxmColorSurfaceScaleMode;

typedef enum SceGxmColorSurfaceType {
   SCE_GXM_COLOR_SURFACE_LINEAR    = 0x00000000u
} SceGxmColorSurfaceType;

typedef enum SceGxmColorSwizzle1Mode {
   SCE_GXM_COLOR_SWIZZLE1_R = 0x00000000u,
   SCE_GXM_COLOR_SWIZZLE1_G = 0x00100000u,
   SCE_GXM_COLOR_SWIZZLE1_A = 0x00100000u
} SceGxmColorSwizzle1Mode;

typedef enum SceGxmColorSwizzle2Mode {
   SCE_GXM_COLOR_SWIZZLE2_GR = 0x00000000u,
   SCE_GXM_COLOR_SWIZZLE2_RG = 0x00100000u,
   SCE_GXM_COLOR_SWIZZLE2_RA = 0x00200000u,
   SCE_GXM_COLOR_SWIZZLE2_AR = 0x00300000u
} SceGxmColorSwizzle2Mode;

typedef enum SceGxmColorSwizzle3Mode {
   SCE_GXM_COLOR_SWIZZLE3_BGR = 0x00000000u,
   SCE_GXM_COLOR_SWIZZLE3_RGB = 0x00100000u
} SceGxmColorSwizzle3Mode;

typedef enum SceGxmColorSwizzle4Mode {
   SCE_GXM_COLOR_SWIZZLE4_ABGR	= 0x00000000u,
   SCE_GXM_COLOR_SWIZZLE4_ARGB	= 0x00100000u,
   SCE_GXM_COLOR_SWIZZLE4_RGBA	= 0x00200000u,
   SCE_GXM_COLOR_SWIZZLE4_BGRA	= 0x00300000u
} SceGxmColorSwizzle4Mode;

typedef struct SceGxmContext SceGxmContext;

typedef struct SceGxmContextParams {
	void *hostMem;
	SceSize hostMemSize;
	void *vdmRingBufferMem;
	SceSize vdmRingBufferMemSize;
	void *vertexRingBufferMem;
	SceSize vertexRingBufferMemSize;
	void *fragmentRingBufferMem;
	SceSize fragmentRingBufferMemSize;
	void *fragmentUsseRingBufferMem;
	SceSize fragmentUsseRingBufferMemSize;
	unsigned int fragmentUsseRingBufferOffset;
} SceGxmContextParams;

typedef enum SceGxmDepthStencilFormat {
   SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24    = 0x01266000u
} SceGxmDepthStencilFormat;

typedef struct SceGxmDepthStencilSurface {
	unsigned int zlsControl;
	void *depthData;
	void *stencilData;
	float backgroundDepth;
	unsigned int backgroundControl;
} SceGxmDepthStencilSurface;

typedef enum SceGxmDepthStencilSurfaceType {
   SCE_GXM_DEPTH_STENCIL_SURFACE_TILED   = 0x00011000u
} SceGxmDepthStencilSurfaceType;

typedef enum SceGxmDepthWriteMode {
   SCE_GXM_DEPTH_WRITE_DISABLED = 0x00100000u,
   SCE_GXM_DEPTH_WRITE_ENABLED  = 0x00000000u
} SceGxmDepthWriteMode;

typedef void (SceGxmDisplayQueueCallback)(const void *callbackData);
typedef struct SceGxmFragmentProgram SceGxmFragmentProgram;

typedef enum SceGxmIndexFormat {
   SCE_GXM_INDEX_FORMAT_U16   = 0x00000000u
} SceGxmIndexFormat;

typedef enum SceGxmIndexSource {
   SCE_GXM_INDEX_SOURCE_INDEX_16BIT    = 0x00000000u
} SceGxmIndexSource;

typedef enum SceGxmInitializeFlags {
   SCE_GXM_INITIALIZE_FLAG_DEFAULT                              = 0x00000000u,
   SCE_GXM_INITIALIZE_FLAG_PB_LPDDR                             = 0x00000001u,
   SCE_GXM_INITIALIZE_FLAG_SHARED_SYNC                          = 0x00000002u,
   SCE_GXM_INITIALIZE_FLAG_SHAREDPB_CREATE                      = 0x00000004u,
   SCE_GXM_INITIALIZE_FLAG_SHAREDPB_OPEN                        = 0x00000008u,
   SCE_GXM_INITIALIZE_FLAG_EXTENDED_FORMAT                      = 0x00000010u,
   SCE_GXM_INITIALIZE_FLAG_DISPLAY_QUEUE_THREAD_AFFINITY_CPU_1  = 0x00010000u,
   SCE_GXM_INITIALIZE_FLAG_DISPLAY_QUEUE_THREAD_AFFINITY_CPU_2  = 0x00020000u
} SceGxmInitializeFlags;

typedef enum SceGxmMemoryAttribFlags {
   SCE_GXM_MEMORY_ATTRIB_READ  = 1,
   SCE_GXM_MEMORY_ATTRIB_WRITE = 2
} SceGxmMemoryAttribFlags;

typedef enum SceGxmMultisampleMode {
	SCE_GXM_MULTISAMPLE_NONE,
	SCE_GXM_MULTISAMPLE_2X,
	SCE_GXM_MULTISAMPLE_4X
} SceGxmMultisampleMode;

typedef struct SceGxmNotification {
	volatile unsigned int *address;
	unsigned int value;
} SceGxmNotification;

typedef enum SceGxmOutputRegisterFormat {
	SCE_GXM_OUTPUT_REGISTER_FORMAT_DECLARED,
	SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
	SCE_GXM_OUTPUT_REGISTER_FORMAT_CHAR4,
	SCE_GXM_OUTPUT_REGISTER_FORMAT_USHORT2,
	SCE_GXM_OUTPUT_REGISTER_FORMAT_SHORT2,
	SCE_GXM_OUTPUT_REGISTER_FORMAT_HALF4,
	SCE_GXM_OUTPUT_REGISTER_FORMAT_HALF2,
	SCE_GXM_OUTPUT_REGISTER_FORMAT_FLOAT2,
	SCE_GXM_OUTPUT_REGISTER_FORMAT_FLOAT
} SceGxmOutputRegisterFormat;

typedef enum SceGxmOutputRegisterSize {
   SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT = 0x00000000u
} SceGxmOutputRegisterSize;

typedef enum SceGxmPrimitiveType {
   SCE_GXM_PRIMITIVE_TRIANGLE_STRIP  = 0x0C000000u
} SceGxmPrimitiveType;

typedef struct SceGxmProgram SceGxmProgram;
typedef struct SceGxmProgramParameter SceGxmProgramParameter;

typedef enum SceGxmRegionClipMode {
   SCE_GXM_REGION_CLIP_NONE    = 0x00000000u,
   SCE_GXM_REGION_CLIP_OUTSIDE = 0x80000000u
} SceGxmRegionClipMode;

typedef struct SceGxmRegisteredProgram SceGxmRegisteredProgram;
typedef struct SceGxmRenderTarget SceGxmRenderTarget;

typedef enum SceGxmRenderTargetFlags {
   SCE_GXM_RENDER_TARGET_CUSTOM_MULTISAMPLE_LOCATIONS = (1 << 0)
} SceGxmRenderTargetFlags;

typedef struct SceGxmRenderTargetParams {
	uint32_t flags;
	uint16_t width;
	uint16_t height;
	uint16_t scenesPerFrame;
	uint16_t multisampleMode;
	uint32_t multisampleLocations;
	SceUID driverMemBlock;
} SceGxmRenderTargetParams;

typedef struct SceGxmShaderPatcher SceGxmShaderPatcher;
typedef void *(SceGxmShaderPatcherBufferAllocCallback)(void *userData, SceSize size);
typedef void (SceGxmShaderPatcherBufferFreeCallback)(void *userData, void *mem);
typedef void *(SceGxmShaderPatcherHostAllocCallback)(void *userData, SceSize size);
typedef void (SceGxmShaderPatcherHostFreeCallback)(void *userData, void *mem);
typedef void *(SceGxmShaderPatcherUsseAllocCallback)(void *userData, SceSize size, unsigned int *usseOffset);
typedef void (SceGxmShaderPatcherUsseFreeCallback)(void *userData, void *mem);

typedef enum SceGxmStencilFunc {
   SCE_GXM_STENCIL_FUNC_NEVER          = 0x00000000u,
   SCE_GXM_STENCIL_FUNC_EQUAL          = 0x04000000u,
   SCE_GXM_STENCIL_FUNC_ALWAYS         = 0x0E000000u
} SceGxmStencilFunc;

typedef enum SceGxmStencilOp {
   SCE_GXM_STENCIL_OP_KEEP      = 0x00000000u,
   SCE_GXM_STENCIL_OP_ZERO      = 0x00000001u,
   SCE_GXM_STENCIL_OP_REPLACE   = 0x00000002u
} SceGxmStencilOp;

typedef struct SceGxmSyncObject SceGxmSyncObject;

typedef struct SceGxmTexture {
	union {
		struct {
			uint32_t unk0 : 1;
			uint32_t stride_ext : 2;
			uint32_t vaddr_mode : 3;
			uint32_t uaddr_mode : 3;
			uint32_t mip_filter : 1;
			uint32_t min_filter : 2;
			uint32_t mag_filter : 2;
			uint32_t unk1 : 3;
			uint32_t mip_count : 4;
			uint32_t lod_bias : 6;
			uint32_t gamma_mode : 2;
			uint32_t unk2 : 2;
			uint32_t format0 : 1;
		} generic;
		struct {
			uint32_t unk0 : 1;
			uint32_t stride_ext : 2;
			uint32_t vaddr_mode : 3;
			uint32_t uaddr_mode : 3;
			uint32_t stride_low : 3;
			uint32_t mag_filter : 2;
			uint32_t unk1 : 3;
			uint32_t stride : 10;
			uint32_t gamma_mode : 2;
			uint32_t unk2 : 2;
			uint32_t format0 : 1;
		} linear_strided;
	};
	union {
		struct {
			uint32_t height : 12;
			uint32_t width : 12;
			uint32_t base_format : 5;
			uint32_t type : 3;
		} generic2;
		struct {
			uint32_t height_pot : 4;
			uint32_t reserved0 : 12;
			uint32_t width_pot : 4;
			uint32_t reserved1 : 4;
			uint32_t base_format : 5;
			uint32_t type : 3;
		} swizzled_cube;
	};
	uint32_t lod_min0 : 2;
	uint32_t data_addr : 30;
	uint32_t palette_addr : 26;
	uint32_t lod_min1 : 2;
	uint32_t swizzle_format : 3;
	uint32_t normalize_mode : 1;
} SceGxmTexture;

typedef enum SceGxmTextureBaseFormat {
   SCE_GXM_TEXTURE_BASE_FORMAT_U8           = 0x00000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_S8           = 0x01000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4     = 0x02000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2     = 0x03000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5     = 0x04000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5       = 0x05000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6       = 0x06000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_U8U8         = 0x07000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_S8S8         = 0x08000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8     = 0x0C000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8     = 0x0D000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_F32          = 0x12000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_U32          = 0x17000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_S32          = 0x18000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_UBC1         = 0x85000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_UBC2         = 0x86000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_UBC3         = 0x87000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_P8           = 0x95000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8       = 0x98000000,
   SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8       = 0x99000000
} SceGxmTextureBaseFormat;

typedef enum SceGxmTextureFilter {
   SCE_GXM_TEXTURE_FILTER_POINT           = 0x00000000u,
   SCE_GXM_TEXTURE_FILTER_LINEAR          = 0x00000001u
} SceGxmTextureFilter;

typedef enum SceGxmTextureSwizzle1Mode {
   SCE_GXM_TEXTURE_SWIZZLE1_R    = 0x00000000u,
   SCE_GXM_TEXTURE_SWIZZLE1_000R = 0x00001000u,
   SCE_GXM_TEXTURE_SWIZZLE1_111R = 0x00002000u,
   SCE_GXM_TEXTURE_SWIZZLE1_RRRR = 0x00003000u,
   SCE_GXM_TEXTURE_SWIZZLE1_0RRR = 0x00004000u,
   SCE_GXM_TEXTURE_SWIZZLE1_1RRR = 0x00005000u,
   SCE_GXM_TEXTURE_SWIZZLE1_R000 = 0x00006000u,
   SCE_GXM_TEXTURE_SWIZZLE1_R111 = 0x00007000u
} SceGxmTextureSwizzle1Mode;

typedef enum SceGxmTextureSwizzle2Mode {
   SCE_GXM_TEXTURE_SWIZZLE2_GR     = 0x00000000u,
   SCE_GXM_TEXTURE_SWIZZLE2_00GR   = 0x00001000u,
   SCE_GXM_TEXTURE_SWIZZLE2_GRRR   = 0x00002000u,
   SCE_GXM_TEXTURE_SWIZZLE2_RGGG   = 0x00003000u,
   SCE_GXM_TEXTURE_SWIZZLE2_GRGR   = 0x00004000u,
   SCE_GXM_TEXTURE_SWIZZLE2_00RG   = 0x00005000u
} SceGxmTextureSwizzle2Mode;

typedef enum SceGxmTextureSwizzle2ModeAlt {
   SCE_GXM_TEXTURE_SWIZZLE2_SD = 0x00000000u,
   SCE_GXM_TEXTURE_SWIZZLE2_DS = 0x00001000u
} SceGxmTextureSwizzle2ModeAlt;

typedef enum SceGxmTextureSwizzle3Mode {
   SCE_GXM_TEXTURE_SWIZZLE3_BGR   = 0x00000000u,
   SCE_GXM_TEXTURE_SWIZZLE3_RGB   = 0x00001000u
} SceGxmTextureSwizzle3Mode;

typedef enum SceGxmTextureSwizzle4Mode {
   SCE_GXM_TEXTURE_SWIZZLE4_ABGR   = 0x00000000u,
   SCE_GXM_TEXTURE_SWIZZLE4_ARGB   = 0x00001000u,
   SCE_GXM_TEXTURE_SWIZZLE4_RGBA   = 0x00002000u,
   SCE_GXM_TEXTURE_SWIZZLE4_BGRA   = 0x00003000u,
   SCE_GXM_TEXTURE_SWIZZLE4_1BGR   = 0x00004000u,
   SCE_GXM_TEXTURE_SWIZZLE4_1RGB   = 0x00005000u,
   SCE_GXM_TEXTURE_SWIZZLE4_RGB1   = 0x00006000u,
   SCE_GXM_TEXTURE_SWIZZLE4_BGR1   = 0x00007000u
} SceGxmTextureSwizzle4Mode;

typedef enum SceGxmTextureSwizzleYUV420Mode {
   SCE_GXM_TEXTURE_SWIZZLE_YUV_CSC0 = 0x00000000u,
   SCE_GXM_TEXTURE_SWIZZLE_YVU_CSC0 = 0x00001000u,
   SCE_GXM_TEXTURE_SWIZZLE_YUV_CSC1 = 0x00002000u,
   SCE_GXM_TEXTURE_SWIZZLE_YVU_CSC1 = 0x00003000u
} SceGxmTextureSwizzleYUV420Mode;

typedef enum SceGxmTextureSwizzleYUV422Mode {
   SCE_GXM_TEXTURE_SWIZZLE_YUYV_CSC0 = 0x00000000u,
   SCE_GXM_TEXTURE_SWIZZLE_YVYU_CSC0 = 0x00001000u,
   SCE_GXM_TEXTURE_SWIZZLE_UYVY_CSC0 = 0x00002000u,
   SCE_GXM_TEXTURE_SWIZZLE_VYUY_CSC0 = 0x00003000u,
   SCE_GXM_TEXTURE_SWIZZLE_YUYV_CSC1 = 0x00004000u,
   SCE_GXM_TEXTURE_SWIZZLE_YVYU_CSC1 = 0x00005000u,
   SCE_GXM_TEXTURE_SWIZZLE_UYVY_CSC1 = 0x00006000u,
   SCE_GXM_TEXTURE_SWIZZLE_VYUY_CSC1 = 0x00007000u
} SceGxmTextureSwizzleYUV422Mode;

typedef struct SceGxmValidRegion {
	uint32_t xMax;
	uint32_t yMax;
} SceGxmValidRegion;

typedef struct SceGxmVertexAttribute {
	uint16_t streamIndex;
	uint16_t offset;
	uint8_t format;
	uint8_t componentCount;
	uint16_t regIndex;
} SceGxmVertexAttribute;

typedef struct SceGxmVertexProgram SceGxmVertexProgram;

typedef struct SceGxmVertexStream {
	uint16_t stride;
	uint16_t indexSource;
} SceGxmVertexStream;

int sceGxmDisplayQueueFinish(void);
int sceGxmMapFragmentUsseMemory(void *base, SceSize size, unsigned int *offset);
int sceGxmMapVertexUsseMemory(void *base, SceSize size, unsigned int *offset);
int sceGxmTerminate(void);
int sceGxmUnmapFragmentUsseMemory(void *base);
int sceGxmUnmapMemory(void *base);
int sceGxmUnmapVertexUsseMemory(void *base);
#define SCE_GXM_TILE_SIZEX  (1U << SCE_GXM_TILE_SHIFTX)
#define SCE_GXM_TILE_SIZEY  (1U << SCE_GXM_TILE_SHIFTY)

typedef enum SceGxmColorFormat {
   SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR = SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8 | SCE_GXM_COLOR_SWIZZLE4_ABGR,
   SCE_GXM_COLOR_FORMAT_A8B8G8R8 = SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR
} SceGxmColorFormat;

typedef struct SceGxmColorSurface {
	unsigned int pbeSidebandWord;
	unsigned int pbeEmitWords[6];
	unsigned int outputRegisterSize;
	SceGxmTexture backgroundTex;
} SceGxmColorSurface;

typedef struct SceGxmInitializeParams {
	unsigned int flags;
	unsigned int displayQueueMaxPendingCount;
	SceGxmDisplayQueueCallback *displayQueueCallback;
	unsigned int displayQueueCallbackDataSize;
	SceSize parameterBufferSize;
} SceGxmInitializeParams;

typedef SceGxmRegisteredProgram *SceGxmShaderPatcherId;

typedef struct SceGxmShaderPatcherParams {
	void *userData;
	SceGxmShaderPatcherHostAllocCallback *hostAllocCallback;
	SceGxmShaderPatcherHostFreeCallback *hostFreeCallback;
	SceGxmShaderPatcherBufferAllocCallback *bufferAllocCallback;
	SceGxmShaderPatcherBufferFreeCallback *bufferFreeCallback;
	void *bufferMem;
	SceSize bufferMemSize;
	SceGxmShaderPatcherUsseAllocCallback *vertexUsseAllocCallback;
	SceGxmShaderPatcherUsseFreeCallback *vertexUsseFreeCallback;
	void *vertexUsseMem;
	SceSize vertexUsseMemSize;
	unsigned int vertexUsseOffset;
	SceGxmShaderPatcherUsseAllocCallback *fragmentUsseAllocCallback;
	SceGxmShaderPatcherUsseFreeCallback *fragmentUsseFreeCallback;
	void *fragmentUsseMem;
	SceSize fragmentUsseMemSize;
	unsigned int fragmentUsseOffset;
} SceGxmShaderPatcherParams;

typedef enum SceGxmTextureFormat {
   SCE_GXM_TEXTURE_FORMAT_U8_R111 = SCE_GXM_TEXTURE_BASE_FORMAT_U8 | SCE_GXM_TEXTURE_SWIZZLE1_R111,
   SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_RGBA = SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4 | SCE_GXM_TEXTURE_SWIZZLE4_RGBA,
   SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB = SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5 | SCE_GXM_TEXTURE_SWIZZLE3_RGB,
   SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8 | SCE_GXM_TEXTURE_SWIZZLE4_ABGR,
   SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8 | SCE_GXM_TEXTURE_SWIZZLE4_ARGB,
   SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8 | SCE_GXM_TEXTURE_SWIZZLE4_1RGB,
   SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR = SCE_GXM_TEXTURE_BASE_FORMAT_UBC1 | SCE_GXM_TEXTURE_SWIZZLE4_ABGR,
   SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR = SCE_GXM_TEXTURE_BASE_FORMAT_UBC2 | SCE_GXM_TEXTURE_SWIZZLE4_ABGR,
   SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR = SCE_GXM_TEXTURE_BASE_FORMAT_UBC3 | SCE_GXM_TEXTURE_SWIZZLE4_ABGR,
   SCE_GXM_TEXTURE_FORMAT_R5G6B5 = SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB,
   SCE_GXM_TEXTURE_FORMAT_A8B8G8R8 = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR
} SceGxmTextureFormat;

int sceGxmCreateContext(const SceGxmContextParams *params, SceGxmContext **context);
int sceGxmCreateRenderTarget(const SceGxmRenderTargetParams *params, SceGxmRenderTarget **renderTarget);
int sceGxmDepthStencilSurfaceInit(SceGxmDepthStencilSurface *surface, SceGxmDepthStencilFormat depthStencilFormat, SceGxmDepthStencilSurfaceType surfaceType, unsigned int strideInSamples, void *depthData, void *stencilData);
int sceGxmDestroyContext(SceGxmContext *context);
int sceGxmDestroyRenderTarget(SceGxmRenderTarget *renderTarget);
int sceGxmDisplayQueueAddEntry(SceGxmSyncObject *oldBuffer, SceGxmSyncObject *newBuffer, const void *callbackData);
int sceGxmDraw(SceGxmContext *context, SceGxmPrimitiveType primType, SceGxmIndexFormat indexType, const void *indexData, unsigned int indexCount);
int sceGxmEndScene(SceGxmContext *context, const SceGxmNotification *vertexNotification, const SceGxmNotification *fragmentNotification);
void sceGxmFinish(SceGxmContext *context);
int sceGxmMapMemory(void *base, SceSize size, SceGxmMemoryAttribFlags attr);
int sceGxmProgramCheck(const SceGxmProgram *program);
const SceGxmProgramParameter *sceGxmProgramFindParameterByName(const SceGxmProgram *program, const char *name);
unsigned int sceGxmProgramParameterGetResourceIndex(const SceGxmProgramParameter *parameter);
int sceGxmReserveVertexDefaultUniformBuffer(SceGxmContext *context, void **uniformBuffer);
void sceGxmSetFragmentProgram(SceGxmContext *context, const SceGxmFragmentProgram *fragmentProgram);
int sceGxmSetFragmentTexture(SceGxmContext *context, unsigned int textureIndex, const SceGxmTexture *texture);
void sceGxmSetFrontDepthWriteEnable(SceGxmContext *context, SceGxmDepthWriteMode enable);
void sceGxmSetFrontStencilFunc(SceGxmContext *context, SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail, SceGxmStencilOp depthPass, unsigned char compareMask, unsigned char writeMask);
void sceGxmSetFrontStencilRef(SceGxmContext *context, unsigned int sref);
void sceGxmSetRegionClip(SceGxmContext *context, SceGxmRegionClipMode mode, unsigned int xMin, unsigned int yMin, unsigned int xMax, unsigned int yMax);
int sceGxmSetUniformDataF(void *uniformBuffer, const SceGxmProgramParameter *parameter, unsigned int componentOffset, unsigned int componentCount, const float *sourceData);
void sceGxmSetVertexProgram(SceGxmContext *context, const SceGxmVertexProgram *vertexProgram);
int sceGxmSetVertexStream(SceGxmContext *context, unsigned int streamIndex, const void *streamData);
void sceGxmSetViewport(SceGxmContext *context, float xOffset, float xScale, float yOffset, float yScale, float zOffset, float zScale);
int sceGxmShaderPatcherDestroy(SceGxmShaderPatcher *shaderPatcher);
int sceGxmShaderPatcherReleaseFragmentProgram(SceGxmShaderPatcher *shaderPatcher, SceGxmFragmentProgram *fragmentProgram);
int sceGxmShaderPatcherReleaseVertexProgram(SceGxmShaderPatcher *shaderPatcher, SceGxmVertexProgram *vertexProgram);
int sceGxmSyncObjectCreate(SceGxmSyncObject **syncObject);
int sceGxmSyncObjectDestroy(SceGxmSyncObject *syncObject);
void *sceGxmTextureGetData(const SceGxmTexture *texture);
unsigned int sceGxmTextureGetHeight(const SceGxmTexture *texture);
unsigned int sceGxmTextureGetWidth(const SceGxmTexture *texture);
int sceGxmTextureSetMagFilter(SceGxmTexture *texture, SceGxmTextureFilter magFilter);
int sceGxmTextureSetMinFilter(SceGxmTexture *texture, SceGxmTextureFilter minFilter);
int sceGxmTextureSetPalette(SceGxmTexture *texture, const void *paletteData);
int sceGxmBeginScene(SceGxmContext *context, unsigned int flags, const SceGxmRenderTarget *renderTarget, const SceGxmValidRegion *validRegion, SceGxmSyncObject *vertexSyncObject, SceGxmSyncObject *fragmentSyncObject, const SceGxmColorSurface *colorSurface, const SceGxmDepthStencilSurface *depthStencil);
int sceGxmColorSurfaceInit(SceGxmColorSurface *surface, SceGxmColorFormat colorFormat, SceGxmColorSurfaceType surfaceType, SceGxmColorSurfaceScaleMode scaleMode, SceGxmOutputRegisterSize outputRegisterSize, unsigned int width, unsigned int height, unsigned int strideInPixels, void *data);
int sceGxmInitialize(const SceGxmInitializeParams *params);
int sceGxmPadHeartbeat(const SceGxmColorSurface *displaySurface, SceGxmSyncObject *displaySyncObject);
int sceGxmShaderPatcherCreate(const SceGxmShaderPatcherParams *params, SceGxmShaderPatcher **shaderPatcher);
int sceGxmShaderPatcherCreateFragmentProgram(SceGxmShaderPatcher *shaderPatcher, SceGxmShaderPatcherId programId, SceGxmOutputRegisterFormat outputFormat, SceGxmMultisampleMode multisampleMode, const SceGxmBlendInfo *blendInfo, const SceGxmProgram *vertexProgram, SceGxmFragmentProgram **fragmentProgram);
int sceGxmShaderPatcherCreateVertexProgram(SceGxmShaderPatcher *shaderPatcher, SceGxmShaderPatcherId programId, const SceGxmVertexAttribute *attributes, unsigned int attributeCount, const SceGxmVertexStream *streams, unsigned int streamCount, SceGxmVertexProgram **vertexProgram);
int sceGxmShaderPatcherRegisterProgram(SceGxmShaderPatcher *shaderPatcher, const SceGxmProgram *programHeader, SceGxmShaderPatcherId *programId);
int sceGxmShaderPatcherUnregisterProgram(SceGxmShaderPatcher *shaderPatcher, SceGxmShaderPatcherId programId);
SceGxmTextureFormat sceGxmTextureGetFormat(const SceGxmTexture *texture);
int sceGxmTextureInitLinear(SceGxmTexture *texture, const void *data, SceGxmTextureFormat texFormat, unsigned int width, unsigned int height, unsigned int mipCount);

#endif
