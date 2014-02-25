#ifndef	_RGL_TYPES_H
#define	_RGL_TYPES_H

#include <stdlib.h>
#include <string.h>

#include "../export/PSGL/psgl.h"
#include "Types.h"
#include "Base.h"

#include <Cg/cg.h>

#define RGL_MAX_COLOR_ATTACHMENTS 4
#define RGL_MAX_TEXTURE_IMAGE_UNITS	16
#define RGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS	4

#define RGL_MAX_VERTEX_ATTRIBS	16

#define RGLGCM_BIG_ENDIAN

#define ENDIAN_32(X, F) ((F) ? endianSwapWord(X) : (X))

#define FRAGMENT_PROFILE_INDEX 1
#define RGLP_MAX_TEXTURE_SIZE 4096

#define RGLGCM_LINEAR_BUFFER_ALIGNMENT 128
#define RGLGCM_HOST_BUFFER_ALIGNMENT 128

#define RGLGCM_TRANSIENT_MEMORY_DEFAULT		(32 << 20)
#define RGLGCM_PERSISTENT_MEMORY_DEFAULT	(160 << 20)
#define RGLGCM_FIFO_SIZE_DEFAULT			(256 * 1024)
#define RGLGCM_HOST_SIZE_DEFAULT			(0)
#define RGLGCM_TRANSIENT_ENTRIES_DEFAULT	64

// There are 6 clock domains, each with a maximum of 4 experiments, plus 4 elapsed exp.
#define RGL_MAX_DPM_QUERIES       (4 * 6 + 4)

// RSX semaphore allocation
//  64-191	events
//  192		fence implementation (independent of nv_glFence)
//  253     used in RGLGcmFifoUtils.h
#define RGLGCM_SEMA_NEVENTS	128
#define RGLGCM_SEMA_BASE	64	// libgcm uses 0-63
#define RGLGCM_SEMA_FENCE	192

// synchronization
//  rglGcmSync enables GPU waiting by sending nv_glAcquireSemaphore to the
//  GPU and returning a memory mapped pointer to the semaphore.  The GPU
//  will be released when 0 is written to the memory location.
//
//  rglGcm{Inc,Test,Finish}FenceRef are intended to be drop-in replacements
//  for the corresponding RGLGCM routines, using a semaphore instead of the
//  fence mechanism (so IncFence uses the 3D class).

#define RGLGCM_MAX_COLOR_SURFACES 4

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
#define RGLGCM_F0_DOT_0             12582912.0f

// some other useful push buf defines/commands
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
   RGLGCM_SURFACE_POOL_LINEAR,
};


#define RGLGCM_DEVICE_SYNC_FENCE 1
#define RGLGCM_DEVICE_SYNC_COND  2

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

#define RGLGCM_HAS_INVALIDATE_TILE

#define RGLGCM_TILED_BUFFER_ALIGNMENT 0x10000 // 64KB
#define RGLGCM_TILED_BUFFER_HEIGHT_ALIGNMENT 64

#define RGLGCM_MAX_TILED_REGIONS 15

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

struct rglFramebufferAttachment
{
   GLenum type;	// renderbuffer or texture
   GLuint name;

   // only valid for texture attachment
   GLenum textureTarget;
   rglFramebufferAttachment(): type( GL_NONE ), name( 0 ), textureTarget( GL_NONE )
   {};
};


// gleSetRenderTarget has enough arguments to define its own struct
typedef struct rglGcmRenderTargetEx rglGcmRenderTargetEx;
struct rglGcmRenderTargetEx
{
   // color buffers
   rglGcmEnum   colorFormat;
   GLuint      colorBufferCount;

   GLuint      colorId[RGLGCM_SETRENDERTARGET_MAXCOUNT];
   GLuint      colorIdOffset[RGLGCM_SETRENDERTARGET_MAXCOUNT];
   GLuint      colorPitch[RGLGCM_SETRENDERTARGET_MAXCOUNT];

   // (0,0) is in the lower left
   GLboolean   yInverted;

   // window offset
   GLuint      xOffset;
   GLuint      yOffset;

   // render dimensions
   GLuint      width;
   GLuint      height;
};

struct rglFramebuffer
{
   rglFramebufferAttachment color[RGL_MAX_COLOR_ATTACHMENTS];
   rglGcmRenderTargetEx rt;
   GLboolean complete;
   GLboolean needValidate;
};

#ifdef __cplusplus
extern "C" {
#endif

#define RGLBIT_GET(f,n)			((f) & (1<<(n)))
#define RGLBIT_TRUE(f,n)			((f) |= (1<<(n)))
#define RGLBIT_FALSE(f,n)		((f) &= ~(1<<(n)))
#define RGLBIT_ASSIGN(f,n,val)	do { if(val) RGLBIT_TRUE(f,n); else RGLBIT_FALSE(f,n); } while(0)

typedef struct
{
   GLfloat	R, G, B, A;
} rglColorRGBAf;

typedef struct
{
   int	X, Y, XSize, YSize;
} rglViewPort;

// image location flags
//  These are flag bits that indicate where the valid image data is.  Data
//  can be valid nowhere, on the host, on the GPU, or in both places.
enum {
   RGL_IMAGE_DATASTATE_UNSET = 0x0,	// not a flag, just a meaningful 0
   RGL_IMAGE_DATASTATE_HOST  = 0x1,
   RGL_IMAGE_DATASTATE_GPU   = 0x2
};

// Image data structure
typedef struct rglImage_
{
   // isSet indicates whether a gl*TexImage* call has been made on that image,
   // to know whether calling gl*TexSubImage* is valid or not.
   GLboolean isSet;

   GLenum internalFormat;
   GLenum format;
   GLenum type;
   GLsizei width;
   GLsizei height;
   GLsizei depth;
   GLsizei	alignment;

   // image storage
   //  For raster
   //  storage, the platform driver sets strides (in bytes) between
   //  lines and layers and the library takes care of the rest.

   //  These values are initially zero, but may be set by the platform
   //  rglPlatformChooseInternalStorage to specify custom storage
   //  (compressed, swizzled, etc.).  They should be considered
   //  read-only except by the platform driver.
   GLsizei storageSize;				// minimum allocation
   GLsizei xstride, ystride, zstride;	// strides
   GLuint xblk, yblk;			// block storage size

   char *data;
   char *mallocData;
   GLsizei mallocStorageSize;
   GLenum dataState;	// valid data location (see enum above)
} rglImage;

// Raster data structure
// This struct is used internally to define 3D raster data for writing
// to or reading from a rglImage.  The GL-level interface for pixel/texel
// level operations always uses a raster, even though the underlying
// platform-specific storage may not be a raster (e.g. compressed
// blocks).  The internal routines rglRasterToImage and rglImageToRaster
// convert between the two.
//
// A clean alternative would have been to use rglImage for everything and
// implement a single rglImageToImage copying function.  However, given
// that one side will always be a raster, the implementation cost was
// not seen as worth the generality.
typedef struct
{
   GLenum format;
   GLenum type;
   GLsizei width;
   GLsizei height;
   GLsizei depth;
   GLsizei xstride;
   GLsizei ystride;
   GLsizei zstride;
   void* data;
} rglRaster;

#define RGL_TEXTURE_REVALIDATE_LAYOUT   	0x01
#define RGL_TEXTURE_REVALIDATE_IMAGES		0x02
#define RGL_TEXTURE_REVALIDATE_PARAMETERS	0x04

typedef struct rglBufferObject rglBufferObject;

// Texture data structure
typedef struct
{
   GLuint revalidate;
   GLuint target;

   GLuint minFilter;
   GLuint magFilter;
   GLfloat minLod;
   GLfloat maxLod;
   GLuint maxLevel;
   GLuint wrapS;
   GLuint wrapT;
   GLuint wrapR;
   GLfloat lodBias;
   GLfloat maxAnisotropy;
   GLenum compareMode;
   GLenum compareFunc;
   GLuint gammaRemap;
   GLenum usage;

   rglColorRGBAf borderColor;

   GLboolean vertexEnable;
   GLboolean isRenderTarget;
   // this is valid when the revalidate bits do not have any resource bit set.
   // the validation of the resources update the bit.
   GLboolean   isComplete;

   rglBufferObject *referenceBuffer;
   intptr_t offset;

   RGL::Vector<rglFramebuffer *> framebuffers;

   GLuint		imageCount;
   rglImage*	image;
   void * platformTexture[]; // C99 flexible array member
} rglTexture;

typedef struct
{
   GLboolean isSet;
   void* platformFenceObject[];
} rglFenceObject;

// For now, we'll use a static array for lights
//

// Texture image unit data structure
typedef struct
{
   GLuint		bound2D;

   rglTexture*	default2D;

   GLboolean	enable2D;

   // the current fragment program's target for this unit, if in use.
   // this is invalid otherwise
   GLenum      fragmentTarget;
   GLfloat		lodBias;

   rglTexture* currentTexture;
} rglTextureImageUnit;

enum
{
   RGL_FRAMEBUFFER_ATTACHMENT_NONE,
   RGL_FRAMEBUFFER_ATTACHMENT_RENDERBUFFER,
   RGL_FRAMEBUFFER_ATTACHMENT_TEXTURE,
};

struct RGLdevice
{
   PSGLdeviceParameters deviceParameters;

   GLvoid*			rasterDriver;
   char			platformDevice[]; // C99 flexible array member
};

typedef struct
{
   // parameters to glDraw* calls
   GLenum	mode;
   GLint	firstVertex;
   GLsizei	vertexCount;
   GLuint	indexMin;
   GLuint	indexMax;		// 0==glDrawElements; 0!=glDrawRangeElements
   GLsizei	indexCount;

   // internal draw parameters (set by rglPlatformRequiresSlowPath):
   // client-side array transfer buffer params
   GLuint	xferTotalSize;
   GLuint	indexXferOffset;
   GLuint	indexXferSize;
   GLuint	attribXferTotalSize;
   GLuint	attribXferOffset[RGL_MAX_VERTEX_ATTRIBS];
   GLuint	attribXferSize[RGL_MAX_VERTEX_ATTRIBS];
} rglDrawParams;

// define mapping of vertex semantics to attributes
//  These indices specify the aliasing of vertex attributes with
//  conventional per-vertex parameters.  This mapping is the same as
//  specified in the NV_vertex_program extension.
#define RGL_ATTRIB_POSITION_INDEX			0
#define RGL_ATTRIB_WEIGHT_INDEX				1
#define RGL_ATTRIB_NORMAL_INDEX				2
#define RGL_ATTRIB_PRIMARY_COLOR_INDEX		3
#define RGL_ATTRIB_SECONDARY_COLOR_INDEX	4
#define RGL_ATTRIB_FOG_COORD_INDEX			5
#define RGL_ATTRIB_POINT_SIZE_INDEX			6
#define RGL_ATTRIB_BLEND_INDICES_INDEX		7
#define RGL_ATTRIB_TEX_COORD0_INDEX			8
#define RGL_ATTRIB_TEX_COORD1_INDEX			9
#define RGL_ATTRIB_TEX_COORD2_INDEX			10
#define RGL_ATTRIB_TEX_COORD3_INDEX			11
#define RGL_ATTRIB_TEX_COORD4_INDEX			12
#define RGL_ATTRIB_TEX_COORD5_INDEX			13
#define RGL_ATTRIB_TEX_COORD6_INDEX			14
#define RGL_ATTRIB_TEX_COORD7_INDEX			15

// per-attribute descriptor and data
typedef struct
{
   // GL state
   GLvoid*	clientData;   // client-side array pointer or VBO offset
   GLuint	clientSize;   // number of components 1-4
   GLenum	clientType;   // GL_SHORT, GL_INT, GL_FLOAT
   GLsizei	clientStride; // element-to-element distance [bytes]
   GLuint  arrayBuffer;  // name of buffer object; 0==none (ie client-side)
   GLfloat	value[4];     // constant attribute value
   GLuint  frequency;    // instancing divisor
   GLboolean normalized;
} __attribute__((aligned (16))) rglAttribute;

// state for the entire set of vertex attributes, plus
// other AttribSet-encapsulated state.
// (this is the block of state applied en mass during glBindAttribSetSCE())
typedef struct
{
   //  Vertex attribute descriptors and data are stored in this array.
   //  The fixed function attributes are aliased to the array via the
   //  indices defined by _RGL_ATTRIB_*_INDEX.
   rglAttribute		attrib[RGL_MAX_VERTEX_ATTRIBS];

   // bitfields corresponding to the attrib[] array elements:
   unsigned int DirtyMask; // 1 == attribute has changed & needs updating
   unsigned int EnabledMask; // 1 == attribute is enabled for drawing
   unsigned int HasVBOMask; // 1 == attribute is in a VBO (ie server-side)
} __attribute__((aligned (16))) rglAttributeState;

struct rglBufferObject
{
   GLuint refCount;
   GLsizeiptr size;
   GLboolean mapped;
   GLenum internalFormat;
   GLuint width;
   GLuint height;
   RGL::Vector<rglTexture *> textureReferences;
   void *platformBufferObject[];
};


#define RGL_CONTEXT_RED_MASK				0x01
#define RGL_CONTEXT_GREEN_MASK			0x02
#define RGL_CONTEXT_BLUE_MASK				0x04
#define RGL_CONTEXT_ALPHA_MASK			0x08
#define RGL_CONTEXT_DEPTH_MASK			0x10
#define RGL_CONTEXT_COLOR_MASK			0x0F

enum
{
   RGL_CONTEXT_ACTIVE_SURFACE_COLOR0,
   RGL_CONTEXT_ACTIVE_SURFACE_COLOR1,
   RGL_CONTEXT_ACTIVE_SURFACE_COLOR2,
   RGL_CONTEXT_ACTIVE_SURFACE_COLOR3,
   RGL_CONTEXT_ACTIVE_SURFACE_DEPTH,
   RGL_CONTEXT_ACTIVE_SURFACE_STENCIL,
   RGL_CONTEXT_ACTIVE_SURFACES
};

typedef struct rglNameSpace
{
   void** data;
   void** firstFree;
   unsigned long capacity;
} rglNameSpace;

typedef void *( *rglTexNameSpaceCreateFunction )( void );
typedef void( *rglTexNameSpaceDestroyFunction )( void * );

typedef struct rglTexNameSpace
{
   void** data;
   GLuint capacity;
   rglTexNameSpaceCreateFunction create;
   rglTexNameSpaceDestroyFunction destroy;
} rglTexNameSpace;

struct RGLcontext
{
   GLenum			error;

   rglViewPort		ViewPort;
   GLclampf		DepthNear;
   GLclampf		DepthFar;

   rglAttributeState defaultAttribs0;	// a default rglAttributeState, for bind = 0
   rglAttributeState *attribs;			// ptr to current rglAttributeState

   // Frame buffer-related fields
   //
   GLenum			DrawBuffer, ReadBuffer;

   GLboolean		Blending;       // enable for mrt color target 0
   GLenum			BlendEquationRGB;
   GLenum			BlendEquationAlpha;
   GLenum			BlendFactorSrcRGB;
   GLenum			BlendFactorDestRGB;
   GLenum			BlendFactorSrcAlpha;
   GLenum			BlendFactorDestAlpha;
   rglColorRGBAf	BlendColor;

   GLboolean		ColorLogicOp;
   GLenum			LogicOp;

   GLboolean		Dithering;

   GLuint	TexCoordReplaceMask;

   rglTexNameSpace textureNameSpace;
   GLuint			ActiveTexture;
   rglTextureImageUnit	TextureImageUnits[RGL_MAX_TEXTURE_IMAGE_UNITS];
   rglTextureImageUnit* CurrentImageUnit;

   GLsizei		packAlignment;
   GLsizei		unpackAlignment;

   rglTexNameSpace	bufferObjectNameSpace;
   GLuint	ArrayBuffer;
   GLuint	PixelUnpackBuffer;
   GLuint	TextureBuffer;

   // framebuffer objects
   GLuint			framebuffer;	// GL_FRAMEBUFFER_OES binding
   rglTexNameSpace	framebufferNameSpace;

   GLboolean		VertexProgram;
   struct _CGprogram*	BoundVertexProgram;

   GLboolean		FragmentProgram;
   struct _CGprogram*	BoundFragmentProgram;
   unsigned int	LastFPConstantModification;

   GLboolean		VSync;
   GLboolean       SkipFirstVSync;

   GLuint			needValidate;
   GLboolean		everAttached;

   CGerror RGLcgLastError;
   CGerrorCallbackFunc RGLcgErrorCallbackFunction;
   // Cg containers
   CGcontext RGLcgContextHead;
   rglNameSpace  cgContextNameSpace;
   rglNameSpace  cgProgramNameSpace;
   rglNameSpace  cgParameterNameSpace;
};

#ifdef __cplusplus
}
#endif

#endif
