#ifndef _RGL_CONSTANTS_H
#define _RGL_CONSTANTS_H

#define RGLGCM_BIG_ENDIAN

#if RGL_ENDIAN == RGL_BIG_ENDIAN
#define ENDIAN_32(X, F) ((F) ? endianSwapWord(X) : (X))
#else
#define ENDIAN_32(X, F) (X)
#endif

#define FRAGMENT_PROFILE_INDEX 1

#define RGLP_MAX_TEXTURE_SIZE 4096

// GCM RESERVE and no RGLGCM post fifo SKID Check
#define GCM_RESERVE_NO_SKID

#ifdef GCM_RESERVE_NO_SKID


#define GCM_FUNC_RESERVE( count )\
   CELL_GCM_RESERVE(count);

#define GCM_FUNC_SAFE( GCM_FUNCTION, ...) \
{ \
   GCM_FUNCTION( (CellGcmContextData*)&rglGcmState_i.fifo, __VA_ARGS__ ); \
}

#define GCM_FUNC_SAFE_NO_ARGS( GCM_FUNCTION, ...) \
{ \
   GCM_FUNCTION( (CellGcmContextData*)&rglGcmState_i.fifo ); \
}

#define GCM_FUNC( GCM_FUNCTION, ...) \
{ \
   GCM_FUNCTION ## Inline( (CellGcmContextData*)&rglGcmState_i.fifo, __VA_ARGS__ ); \
}

#define GCM_FUNC_NO_ARGS( GCM_FUNCTION ) \
{ \
   GCM_FUNCTION ## Inline( (CellGcmContextData*)&rglGcmState_i.fifo ); \
}
#endif

typedef enum rglGcmEnum
{
   // gleSetRenderTarget
   RGLGCM_NONE                           = 0x0000,

   // glDrawArrays, glDrawElements, glBegin
   RGLGCM_POINTS                         = 0x0000,
   RGLGCM_LINES                          = 0x0001,
   RGLGCM_LINE_LOOP                      = 0x0002,
   RGLGCM_LINE_STRIP                     = 0x0003,
   RGLGCM_TRIANGLES                      = 0x0004,
   RGLGCM_TRIANGLE_STRIP                 = 0x0005,
   RGLGCM_TRIANGLE_FAN                   = 0x0006,
   RGLGCM_QUADS                          = 0x0007,
   RGLGCM_QUAD_STRIP                     = 0x0008,
   RGLGCM_POLYGON                        = 0x0009,

   // glClear
   RGLGCM_DEPTH_BUFFER_BIT               = 0x0100,
   RGLGCM_STENCIL_BUFFER_BIT             = 0x0400,
   RGLGCM_COLOR_BUFFER_BIT               = 0x4000,

   // glBlendFunc, glStencilFunc
   RGLGCM_ZERO                           =      0,
   RGLGCM_ONE                            =      1,
   RGLGCM_SRC_COLOR                      = 0x0300,
   RGLGCM_ONE_MINUS_SRC_COLOR            = 0x0301,
   RGLGCM_SRC_ALPHA                      = 0x0302,
   RGLGCM_ONE_MINUS_SRC_ALPHA            = 0x0303,
   RGLGCM_DST_ALPHA                      = 0x0304,
   RGLGCM_ONE_MINUS_DST_ALPHA            = 0x0305,
   RGLGCM_DST_COLOR                      = 0x0306,
   RGLGCM_ONE_MINUS_DST_COLOR            = 0x0307,
   RGLGCM_SRC_ALPHA_SATURATE             = 0x0308,

   // glAlphaFunc, glDepthFunc, glStencilFunc
   RGLGCM_NEVER                          = 0x0200,
   RGLGCM_LESS                           = 0x0201,
   RGLGCM_EQUAL                          = 0x0202,
   RGLGCM_LEQUAL                         = 0x0203,
   RGLGCM_GREATER                        = 0x0204,
   RGLGCM_NOTEQUAL                       = 0x0205,
   RGLGCM_GEQUAL                         = 0x0206,
   RGLGCM_ALWAYS                         = 0x0207,

   // glLogicOp
   RGLGCM_CLEAR                          = 0x1500,
   RGLGCM_AND                            = 0x1501,
   RGLGCM_AND_REVERSE                    = 0x1502,
   RGLGCM_COPY                           = 0x1503,
   RGLGCM_AND_INVERTED                   = 0x1504,
   RGLGCM_NOOP                           = 0x1505,
   RGLGCM_XOR                            = 0x1506,
   RGLGCM_OR                             = 0x1507,
   RGLGCM_NOR                            = 0x1508,
   RGLGCM_EQUIV                          = 0x1509,
   RGLGCM_INVERT                         = 0x150A,
   RGLGCM_OR_REVERSE                     = 0x150B,
   RGLGCM_COPY_INVERTED                  = 0x150C,
   RGLGCM_OR_INVERTED                    = 0x150D,
   RGLGCM_NAND                           = 0x150E,
   RGLGCM_SET                            = 0x150F,

   // BlendFunc
   RGLGCM_CONSTANT_COLOR                 = 0x8001,
   RGLGCM_ONE_MINUS_CONSTANT_COLOR       = 0x8002,
   RGLGCM_CONSTANT_ALPHA                 = 0x8003,
   RGLGCM_ONE_MINUS_CONSTANT_ALPHA       = 0x8004,
   RGLGCM_BLEND_COLOR                    = 0x8005,
   RGLGCM_FUNC_ADD                       = 0x8006,
   RGLGCM_MIN                            = 0x8007,
   RGLGCM_MAX                            = 0x8008,
   RGLGCM_BLEND_EQUATION                 = 0x8009,
   RGLGCM_FUNC_SUBTRACT                  = 0x800A,
   RGLGCM_FUNC_REVERSE_SUBTRACT          = 0x800B,

   // glTexImage binary formats -- keep in sync with glTexImage tables!
   RGLGCM_ALPHA8                         = 0x803C,
   RGLGCM_ALPHA16                        = 0x803E,
   RGLGCM_HILO8                          = 0x885E,
   RGLGCM_HILO16                         = 0x86F8,
   RGLGCM_ARGB8                          = 0x6007, // does not exist in classic OpenGL
   RGLGCM_BGRA8                          = 0xff01, // does not exist in classic OpenGL
   RGLGCM_RGBA8                          = 0x8058,
   RGLGCM_ABGR8                          = 0xff02, // does not exist in classic OpenGL
   RGLGCM_XBGR8                          = 0xff03, // does not exist in classic OpenGL
   RGLGCM_RGBX8                          = 0xff07, // does not exist in classic OpenGL
   RGLGCM_COMPRESSED_RGB_S3TC_DXT1       = 0x83F0,
   RGLGCM_COMPRESSED_RGBA_S3TC_DXT1      = 0x83F1,
   RGLGCM_COMPRESSED_RGBA_S3TC_DXT3      = 0x83F2,
   RGLGCM_COMPRESSED_RGBA_S3TC_DXT5      = 0x83F3,
   RGLGCM_DEPTH_COMPONENT16              = 0x81A5,
   RGLGCM_DEPTH_COMPONENT24              = 0x81A6,
   RGLGCM_FLOAT_R32                      = 0x8885,
   RGLGCM_RGB5_A1_SCE                    = 0x600C,
   RGLGCM_RGB565_SCE                     = 0x600D,

   // glEnable/glDisable
   RGLGCM_BLEND                          = 0x0be0,
   RGLGCM_COLOR_LOGIC_OP                 = 0x0bf2,
   RGLGCM_DITHER                         = 0x0bd0,
   RGLGCM_PSHADER_SRGB_REMAPPING         = 0xff06,

   // glVertexAttribPointer
   RGLGCM_VERTEX_ATTRIB_ARRAY0           = 0x8650,
   RGLGCM_VERTEX_ATTRIB_ARRAY1           = 0x8651,
   RGLGCM_VERTEX_ATTRIB_ARRAY2           = 0x8652,
   RGLGCM_VERTEX_ATTRIB_ARRAY3           = 0x8653,
   RGLGCM_VERTEX_ATTRIB_ARRAY4           = 0x8654,
   RGLGCM_VERTEX_ATTRIB_ARRAY5           = 0x8655,
   RGLGCM_VERTEX_ATTRIB_ARRAY6           = 0x8656,
   RGLGCM_VERTEX_ATTRIB_ARRAY7           = 0x8657,
   RGLGCM_VERTEX_ATTRIB_ARRAY8           = 0x8658,
   RGLGCM_VERTEX_ATTRIB_ARRAY9           = 0x8659,
   RGLGCM_VERTEX_ATTRIB_ARRAY10          = 0x865a,
   RGLGCM_VERTEX_ATTRIB_ARRAY11          = 0x865b,
   RGLGCM_VERTEX_ATTRIB_ARRAY12          = 0x865c,
   RGLGCM_VERTEX_ATTRIB_ARRAY13          = 0x865d,
   RGLGCM_VERTEX_ATTRIB_ARRAY14          = 0x865e,
   RGLGCM_VERTEX_ATTRIB_ARRAY15          = 0x865f,

   // glTexImage
   RGLGCM_TEXTURE_3D                     = 0x806F,
   RGLGCM_TEXTURE_CUBE_MAP               = 0x8513,
   RGLGCM_TEXTURE_1D                     = 0x0DE0,
   RGLGCM_TEXTURE_2D                     = 0x0DE1,

   // glTexParameter/TextureMagFilter
   RGLGCM_NEAREST                        = 0x2600,
   RGLGCM_LINEAR                         = 0x2601,
   // glTexParameter/TextureMinFilter
   RGLGCM_NEAREST_MIPMAP_NEAREST         = 0x2700,
   RGLGCM_LINEAR_MIPMAP_NEAREST          = 0x2701,
   RGLGCM_NEAREST_MIPMAP_LINEAR          = 0x2702,
   RGLGCM_LINEAR_MIPMAP_LINEAR           = 0x2703,

   // glTexParameter/TextureWrapMode
   RGLGCM_CLAMP                          = 0x2900,
   RGLGCM_REPEAT                         = 0x2901,
   RGLGCM_CLAMP_TO_EDGE                  = 0x812F,
   RGLGCM_CLAMP_TO_BORDER                = 0x812D,
   RGLGCM_MIRRORED_REPEAT                = 0x8370,
   RGLGCM_MIRROR_CLAMP                   = 0x8742,
   RGLGCM_MIRROR_CLAMP_TO_EDGE           = 0x8743,
   RGLGCM_MIRROR_CLAMP_TO_BORDER         = 0x8912,

   // glTexParameter/GammaRemap
   RGLGCM_GAMMA_REMAP_RED_BIT            = 0x0001,
   RGLGCM_GAMMA_REMAP_GREEN_BIT          = 0x0002,
   RGLGCM_GAMMA_REMAP_BLUE_BIT           = 0x0004,
   RGLGCM_GAMMA_REMAP_ALPHA_BIT          = 0x0008,

   // glTexParameter
   RGLGCM_TEXTURE_WRAP_S                 = 0x2802,
   RGLGCM_TEXTURE_WRAP_T                 = 0x2803,
   RGLGCM_TEXTURE_WRAP_R                 = 0x8072,
   RGLGCM_TEXTURE_MIN_FILTER             = 0x2801,
   RGLGCM_TEXTURE_MAG_FILTER             = 0x2800,
   RGLGCM_TEXTURE_MAX_ANISOTROPY         = 0x84FE,
   RGLGCM_TEXTURE_COMPARE_FUNC           = 0x884D,
   RGLGCM_TEXTURE_MIN_LOD                = 0x813A,
   RGLGCM_TEXTURE_MAX_LOD                = 0x813B,
   RGLGCM_TEXTURE_LOD_BIAS               = 0x8501,
   RGLGCM_TEXTURE_BORDER_COLOR           = 0x1004,
   RGLGCM_TEXTURE_GAMMA_REMAP            = 0xff30,

   // ARB_vertex_program
   RGLGCM_VERTEX_PROGRAM                 = 0x8620,
   RGLGCM_FRAGMENT_PROGRAM               = 0x8804,

   // glVertexAttribPointer
   RGLGCM_FLOAT                          = 0x1406,
   RGLGCM_HALF_FLOAT                     = 0x140B,
   RGLGCM_SHORT                          = 0x1402,
   RGLGCM_UNSIGNED_BYTE                  = 0x1401,
   RGLGCM_UNSIGNED_SHORT                 = 0x1403,
   RGLGCM_UNSIGNED_INT                   = 0x1405,
   RGLGCM_BYTE                           = 0x1400,
   RGLGCM_INT                            = 0x1404,

   // query support
   RGLGCM_SAMPLES_PASSED                 = 0xff10,

   // semaphore support
   RGLGCM_SEMAPHORE_USING_GPU            = 0xff20,
   RGLGCM_SEMAPHORE_USING_CPU            = 0xff21,
   RGLGCM_SEMAPHORE_USING_GPU_NO_WRITE_FLUSH = 0xff22,

   // depth clamp
   RGLGCM_DEPTH_CLAMP                    = 0x864F,

   // 11/11/10 bit 3-component attributes
   RGLGCM_CMP                            = 0x6020,
} rglGcmEnum;

#define RGLGCM_LINEAR_BUFFER_ALIGNMENT 128
#define RGLGCM_HOST_BUFFER_ALIGNMENT 128

#define RGLGCM_TRANSIENT_MEMORY_DEFAULT		(32 << 20)
#define RGLGCM_PERSISTENT_MEMORY_DEFAULT	(160 << 20)
#define RGLGCM_FIFO_SIZE_DEFAULT			(256 * 1024)
#define RGLGCM_HOST_SIZE_DEFAULT			(0)
#define RGLGCM_TRANSIENT_ENTRIES_DEFAULT	64

// RSX semaphore allocation
//  64-191	events
//  192		fence implementation (independent of nv_glFence)
//  253     used in RGLGcmFifoUtils.h
#define RGLGCM_SEMA_NEVENTS	128
#define RGLGCM_SEMA_BASE	64	// libgcm uses 0-63
#define RGLGCM_SEMA_FENCE	(RGLGCM_SEMA_BASE+RGLGCM_SEMA_NEVENTS+0)

// synchronization
//  rglGcmSync enables GPU waiting by sending nv_glAcquireSemaphore to the
//  GPU and returning a memory mapped pointer to the semaphore.  The GPU
//  will be released when 0 is written to the memory location.
//
//  rglGcm{Inc,Test,Finish}FenceRef are intended to be drop-in replacements
//  for the corresponding RGLGCM routines, using a semaphore instead of the
//  fence mechanism (so IncFence uses the 3D class).

#define RGLGCM_FALSE 0

#define RGLGCM_MAX_COLOR_SURFACES 4

#define RGLGCM_TRUE 1

// allocation unit for buffer objects
//  Each buffer object is allocated to a multiple of this block size.  This
//  must be at least 64 so that nv_glTransferDataVidToVid() can be used to
//  copy buffer objects within video memory.  This function performs a 2D
//  blit, and there is a 64-byte minimum pitch constraint.
//
//  Swizzled textures require 128-byte alignment, so this takes precedence.
#define RGL_BUFFER_OBJECT_BLOCK_SIZE 128

#define VERTEX_PROFILE_INDEX 0

// GCM can render to 4 color buffers at once.
#define RGLGCM_SETRENDERTARGET_MAXCOUNT  4

// max amount of semaphore we allocate space for
#define RGLGCM_MAX_USER_SEMAPHORES                   256
#define RGLGCM_PAGE_SIZE                             0x1000 // 4KB

#define RGLGCM_LM_MAX_TOTAL_QUERIES						          800
#define RGLGCM_LM_MAX_ZPASS_REPORTS						          (RGLGCM_LM_MAX_TOTAL_QUERIES - 10)
#define RGLGCM_LM_MAX_USER_QUERIES						          (RGLGCM_LM_MAX_ZPASS_REPORTS)

// For main memory query PSGL is going to enable 5000 at any given time 
// compared to the 800 currently for the Local memory queries 
// However, if you really need more than 5k then change the line below 
// and recompile [RSTENSON]
// Maximum value for RGLGCM_MM_MAX_TOTAL_QUERIES is 65,000 
#define RGLGCM_MM_MAX_TOTAL_QUERIES						          5000 // Should be plenty.  
#define RGLGCM_MM_MAX_ZPASS_REPORTS								  (RGLGCM_MM_MAX_TOTAL_QUERIES - 10)
#define RGLGCM_MM_MAX_USER_QUERIES						          (RGLGCM_MM_MAX_ZPASS_REPORTS)

// For 2.50 PSGL will use reports in main memory by default 
// To revert to reports in local memory comment out this define 
#define RGLGCM_USE_MAIN_MEMORY_REPORTS
#ifdef RGLGCM_USE_MAIN_MEMORY_REPORTS
#define RGLGCM_MAX_USER_QUERIES RGLGCM_MM_MAX_USER_QUERIES
#else
#define RGLGCM_MAX_USER_QUERIES RGLGCM_LM_MAX_USER_QUERIES
#endif 


#define RGLGCM_357C_NOTIFIERS_MAXCOUNT               11

enum
{
   // dma contexts
   RGLGCM_CHANNEL_DMA_SCRATCH_NOTIFIER,
   RGLGCM_CONTEXT_DMA_MEMORY_FRAME_BUFFER,
   RGLGCM_CONTEXT_DMA_TO_MEMORY_GET_REPORT,
   RGLGCM_CONTEXT_DMA_MEMORY_HOST_BUFFER,
   RGLGCM_CONTEXT_DMA_MEMORY_SEMAPHORE_RW,
   RGLGCM_CONTEXT_DMA_MEMORY_SEMAPHORE_RO,

   // classes
   RGLGCM_CURIE_PRIMITIVE,
   RGLGCM_MEM2MEM_HOST_TO_VIDEO,

   RGLGCM_IMAGEFROMCPU,
   RGLGCM_SCALEDIMAGE,
   RGLGCM_CONTEXT_2D_SURFACE,
   RGLGCM_CONTEXT_SWIZ_SURFACE,

   RGLGCM_HANDLE_COUNT
};

// For quick float->int conversions
#define RGLGCM_F0_DOT_0                              12582912.0f

// some other useful push buf defines/commands
#define RGLGCM_NON_INCREMENT        (0x40000000)
#define RGLGCM_NOP()                (0x00000000)
#define RGLGCM_JUMP(addr)           (0x20000000 | (addr))
#define RGLGCM_CALL(addr)           (0x00000002 | (addr))
#define RGLGCM_RETURN()             (0x00020000)
#define RGLGCM_MAX_METHOD_COUNT     (2047)
#define RGLGCM_COUNT_SHIFT          (18)
#define RGLGCM_SUBCHANNEL_SHIFT     (13)
#define RGLGCM_METHOD_SHIFT         (0)

#define DEFAULT_FIFO_BLOCK_SIZE     (0x10000)   // 64KB
#define FIFO_RESERVE_SIZE   8 // reserved words needed at the beginning of the fifo

#define BUFFER_HSYNC_NEGATIVE           0
#define BUFFER_HSYNC_POSITIVE           1
#define BUFFER_VSYNC_NEGATIVE           0
#define BUFFER_VSYNC_POSITIVE           1

// This is the format that the mode timing parameters are handed back

enum {
   RGLGCM_SURFACE_SOURCE_TEMPORARY,
   RGLGCM_SURFACE_SOURCE_DEVICE,
   RGLGCM_SURFACE_SOURCE_TEXTURE,
   RGLGCM_SURFACE_SOURCE_RENDERBUFFER,
   RGLGCM_SURFACE_SOURCE_PBO,
};

enum {
   RGLGCM_SURFACE_POOL_NONE,
   RGLGCM_SURFACE_POOL_TILED_COLOR,
   RGLGCM_SURFACE_POOL_TILED_DEPTH,
   RGLGCM_SURFACE_POOL_LINEAR,
   RGLGCM_SURFACE_POOL_SYSTEM,		// GPU accessible host memory
   RGLGCM_SURFACE_POOL_PPU,		// generic EA
   RGLGCM_SURFACE_POOL_SYSTEM_TILED_COLOR, // tiled color GPU accessible XDR 
   RGLGCM_SURFACE_POOL_SYSTEM_TILED_DEPTH, // tiled depth GPU accessible XDR 
};


#define RGLGCM_DEVICE_SYNC_FENCE 1
#define RGLGCM_DEVICE_SYNC_COND  2

// max surface/scissor/viewport dimension
#define RGLGCM_MAX_RT_DIMENSION                      (CELL_GCM_MAX_RT_DIMENSION)

// a few texture consts
#define RGLGCM_MAX_SHADER_TEXCOORD_COUNT             (CELL_GCM_MAX_SHADER_TEXCOORD_COUNT)
#define RGLGCM_MAX_TEXIMAGE_COUNT                    (CELL_GCM_MAX_TEXIMAGE_COUNT)
#define RGLGCM_MAX_LOD_COUNT                         (CELL_GCM_MAX_LOD_COUNT)
#define RGLGCM_MAX_TEX_DIMENSION                     (CELL_GCM_MAX_TEX_DIMENSION)

// max attrib count
#define RGLGCM_ATTRIB_COUNT                          16

// Names for each of the vertex attributes
#define RGLGCM_ATTRIB_POSITION                       0
#define RGLGCM_ATTRIB_VERTEX_WEIGHT                  1
#define RGLGCM_ATTRIB_NORMAL                         2
#define RGLGCM_ATTRIB_COLOR                          3
#define RGLGCM_ATTRIB_SECONDARY_C OLOR               4
#define RGLGCM_ATTRIB_FOG_COORD                      5
#define RGLGCM_ATTRIB_PSIZE                          6
#define RGLGCM_ATTRIB_UNUSED1                        7
#define RGLGCM_ATTRIB_TEXCOORD0                      8
#define RGLGCM_ATTRIB_TEXCOORD1                      9
#define RGLGCM_ATTRIB_TEXCOORD2                      10
#define RGLGCM_ATTRIB_TEXCOORD3                      11
#define RGLGCM_ATTRIB_TEXCOORD4                      12
#define RGLGCM_ATTRIB_TEXCOORD5                      13
#define RGLGCM_ATTRIB_TEXCOORD6                      14
#define RGLGCM_ATTRIB_TEXCOORD7                      15

// Names for the vertex output components:
#define RGLGCM_ATTRIB_OUTPUT_HPOS                    0
#define RGLGCM_ATTRIB_OUTPUT_COL0                    1
#define RGLGCM_ATTRIB_OUTPUT_COL1                    2
#define RGLGCM_ATTRIB_OUTPUT_BFC0                    3
#define RGLGCM_ATTRIB_OUTPUT_BFC1                    4
#define RGLGCM_ATTRIB_OUTPUT_FOGC                    5
#define RGLGCM_ATTRIB_OUTPUT_PSIZ                    6
#define RGLGCM_ATTRIB_OUTPUT_TEX0                    7
#define RGLGCM_ATTRIB_OUTPUT_TEX1                    8
#define RGLGCM_ATTRIB_OUTPUT_TEX2                    9
#define RGLGCM_ATTRIB_OUTPUT_TEX3                    10
#define RGLGCM_ATTRIB_OUTPUT_TEX4                    11
#define RGLGCM_ATTRIB_OUTPUT_TEX5                    12
#define RGLGCM_ATTRIB_OUTPUT_TEX6                    13
#define RGLGCM_ATTRIB_OUTPUT_TEX7                    14

// viewport adjusting
#define RGLGCM_SUBPIXEL_ADJUST                       (0.5/(1<<12))
#define RGLGCM_VIEWPORT_EPSILON                      0.0f

// max vertex program constant slots
#define RGLGCM_VTXPRG_MAX_CONST                      (CELL_GCM_VTXPRG_MAX_CONST)
#define RGLGCM_VTXPRG_MAX_INST                       (CELL_GCM_VTXPRG_MAX_INST)

#define RGLGCM_HAS_INVALIDATE_TILE

#define RGLGCM_TILED_BUFFER_ALIGNMENT 0x10000 // 64KB
#define RGLGCM_TILED_BUFFER_HEIGHT_ALIGNMENT 64

#define RGLGCM_MAX_TILED_REGIONS 15

#define RGLP_BUFFER_OBJECT_SIZE() (sizeof(rglGcmBufferObject))

#endif
