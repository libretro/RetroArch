#ifndef _RGL_H
#define _RGL_H

#include "gl.h"
#include "glext.h"
#include "base.hpp"

#ifdef __PSL1GHT__
#include <rsx/rsx.h>
#include <rsx/resc.h>
#include <sysutil/video.h>
#include <unistd.h>
#include "../../../ps3/sdk_defines.h"
#define CGerror int
typedef void (* CGerrorCallbackFunc)(void);
typedef struct _CGcontext *CGcontext;
#else
#include <cell/resc.h>
#endif

#define _RGL_MAX_COLOR_ATTACHMENTS 4
#define RGL_SUBPIXEL_ADJUST (0.5/(1<<12))

#define gmmIdIsMain(id) (((GmmBaseBlock *)id)->isMain)

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum RGLEnum
{
   RGL_NONE                           = 0x0000,
   RGL_COLOR_BUFFER_BIT               = 0x4000,
   RGL_ZERO                           =      0,
   RGL_ONE                            =      1,
   RGL_SRC_COLOR                      = 0x0300,
   RGL_ONE_MINUS_SRC_COLOR            = 0x0301,
   RGL_SRC_ALPHA                      = 0x0302,
   RGL_ONE_MINUS_SRC_ALPHA            = 0x0303,
   RGL_DST_ALPHA                      = 0x0304,
   RGL_ONE_MINUS_DST_ALPHA            = 0x0305,
   RGL_DST_COLOR                      = 0x0306,
   RGL_ONE_MINUS_DST_COLOR            = 0x0307,
   RGL_SRC_ALPHA_SATURATE             = 0x0308,
   RGL_ONE_MINUS_CONSTANT_COLOR       = 0x8002,
   RGL_CONSTANT_ALPHA                 = 0x8003,
   RGL_ONE_MINUS_CONSTANT_ALPHA       = 0x8004,
   RGL_MIN                            = 0x8007,
   RGL_MAX                            = 0x8008,
   RGL_FUNC_SUBTRACT                  = 0x800A,
   RGL_FUNC_REVERSE_SUBTRACT          = 0x800B,
   RGL_LUMINANCE8                     = 0x8040,
   RGL_LUMINANCE16                    = 0x8042,
   RGL_ALPHA8                         = 0x803C,
   RGL_ALPHA16                        = 0x803E,
   RGL_INTENSITY8                     = 0x804B,
   RGL_INTENSITY16                    = 0x804D,
   RGL_LUMINANCE8_ALPHA8              = 0x8045,
   RGL_LUMINANCE16_ALPHA16            = 0x8048,
   RGL_HILO8                          = 0x885E,
   RGL_HILO16                         = 0x86F8,
   RGL_ARGB8                          = 0x6007,
   RGL_BGRA8                          = 0xff01,
   RGL_RGBA8                          = 0x8058,
   RGL_ABGR8                          = 0xff02,
   RGL_XBGR8                          = 0xff03,
   RGL_RGBX8                          = 0xff07,
   RGL_FLOAT_R32                      = 0x8885,
   RGL_FLOAT_RGBA16                   = 0x888A,
   RGL_FLOAT_RGBA32                   = 0x888B,
   RGL_FLOAT_RGBX16                   = 0xff04,
   RGL_FLOAT_RGBX32                   = 0xff05,
   RGL_LUMINANCE32F_ARB               = 0x8818,
   RGL_ALPHA_LUMINANCE16F_SCE         = 0x600B,
   RGL_RGB5_A1_SCE                    = 0x600C,
   RGL_RGB565_SCE                     = 0x600D,
   RGL_DITHER                         = 0x0bd0,
   RGL_PSHADER_SRGB_REMAPPING         = 0xff06,
   RGL_VERTEX_ATTRIB_ARRAY0           = 0x8650,
   RGL_VERTEX_ATTRIB_ARRAY1           = 0x8651,
   RGL_VERTEX_ATTRIB_ARRAY2           = 0x8652,
   RGL_VERTEX_ATTRIB_ARRAY3           = 0x8653,
   RGL_VERTEX_ATTRIB_ARRAY4           = 0x8654,
   RGL_VERTEX_ATTRIB_ARRAY5           = 0x8655,
   RGL_VERTEX_ATTRIB_ARRAY6           = 0x8656,
   RGL_VERTEX_ATTRIB_ARRAY7           = 0x8657,
   RGL_VERTEX_ATTRIB_ARRAY8           = 0x8658,
   RGL_VERTEX_ATTRIB_ARRAY9           = 0x8659,
   RGL_VERTEX_ATTRIB_ARRAY10          = 0x865a,
   RGL_VERTEX_ATTRIB_ARRAY11          = 0x865b,
   RGL_VERTEX_ATTRIB_ARRAY12          = 0x865c,
   RGL_VERTEX_ATTRIB_ARRAY13          = 0x865d,
   RGL_VERTEX_ATTRIB_ARRAY14          = 0x865e,
   RGL_VERTEX_ATTRIB_ARRAY15          = 0x865f,
   RGL_CLAMP                          = 0x2900,
   RGL_REPEAT                         = 0x2901,
   RGL_CLAMP_TO_EDGE                  = 0x812F,
   RGL_CLAMP_TO_BORDER                = 0x812D,
   RGL_MIRRORED_REPEAT                = 0x8370,
   RGL_MIRROR_CLAMP                   = 0x8742,
   RGL_MIRROR_CLAMP_TO_EDGE           = 0x8743,
   RGL_MIRROR_CLAMP_TO_BORDER         = 0x8912,
   RGL_GAMMA_REMAP_RED_BIT            = 0x0001,
   RGL_GAMMA_REMAP_GREEN_BIT          = 0x0002,
   RGL_GAMMA_REMAP_BLUE_BIT           = 0x0004,
   RGL_GAMMA_REMAP_ALPHA_BIT          = 0x0008,
   RGL_TEXTURE_WRAP_S                 = 0x2802,
   RGL_TEXTURE_WRAP_T                 = 0x2803,
   RGL_TEXTURE_WRAP_R                 = 0x8072,
   RGL_TEXTURE_MIN_FILTER             = 0x2801,
   RGL_TEXTURE_MAG_FILTER             = 0x2800,
   RGL_TEXTURE_MAX_ANISOTROPY         = 0x84FE,
   RGL_TEXTURE_COMPARE_FUNC           = 0x884D,
   RGL_TEXTURE_MIN_LOD                = 0x813A,
   RGL_TEXTURE_MAX_LOD                = 0x813B,
   RGL_TEXTURE_LOD_BIAS               = 0x8501,
   RGL_TEXTURE_BORDER_COLOR           = 0x1004,
   RGL_TEXTURE_GAMMA_REMAP            = 0xff30,
   RGL_VERTEX_PROGRAM                 = 0x8620,
   RGL_FRAGMENT_PROGRAM               = 0x8804,
   RGL_FLOAT                          = 0x1406,
   RGL_HALF_FLOAT                     = 0x140B,
   RGL_SHORT                          = 0x1402,
   RGL_UNSIGNED_BYTE                  = 0x1401,
   RGL_UNSIGNED_SHORT                 = 0x1403,
   RGL_UNSIGNED_INT                   = 0x1405,
   RGL_BYTE                           = 0x1400,
   RGL_INT                            = 0x1404,
   RGL_CMP                            = 0x6020,
} RGLEnum;

typedef struct PSGLdevice PSGLdevice;
typedef struct PSGLcontext PSGLcontext;
typedef struct RGLViewportState RGLViewportState;
struct RGLState;
extern RGLState _RGLState;

struct RGLViewportState
{
   GLint x, y, w, h;
   GLfloat xScale, xCenter;
   GLfloat yScale, yCenter;
};

struct jsFramebufferAttachment
{
   GLenum type;
   GLuint name;
   GLenum textureTarget;
   jsFramebufferAttachment(): type( GL_NONE ), name( 0 ), textureTarget( GL_NONE ) {};
};


struct jsFramebuffer
{
   jsFramebufferAttachment color[_RGL_MAX_COLOR_ATTACHMENTS];
   GLboolean needValidate;
   jsFramebuffer(): needValidate( GL_TRUE ) {};
   virtual ~jsFramebuffer() {};
};

#define RGLBIT_GET(f,n)			((f) & (1<<(n)))
#define RGLBIT_TRUE(f,n)		((f) |= (1<<(n)))
#define RGLBIT_FALSE(f,n)		((f) &= ~(1<<(n)))
#define RGLBIT_ASSIGN(f,n,val)	do { if(val) RGLBIT_TRUE(f,n); else RGLBIT_FALSE(f,n); } while(0)

typedef struct
{
   GLfloat	X, Y , Z, W;
} jsPositionXYZW;

typedef struct
{
   GLfloat	X, Y , Z;
} jsPositionXYZ;

typedef struct
{
   GLfloat	R, G, B, A;
} jsColorRGBAf;

typedef struct
{
   GLfloat * MatrixStackf;
   int		MatrixStackPtr;
   GLboolean dirty;
} jsMatrixStack;

typedef struct
{
   int	X, Y, XSize, YSize;
} jsViewPort;

#define _RGL_IMAGE_STORAGE_RASTER	0
#define _RGL_IMAGE_STORAGE_BLOCK	1

enum {
   _RGL_IMAGE_DATASTATE_UNSET = 0x0,
   _RGL_IMAGE_DATASTATE_HOST  = 0x1,
   _RGL_IMAGE_DATASTATE_GPU   = 0x2
};

typedef struct jsImage_
{
   GLboolean isSet;

   GLenum internalFormat;
   GLenum format;
   GLenum type;
   GLsizei width;
   GLsizei height;
   GLsizei alignment;

   GLsizei storageSize;
   GLsizei xstride, ystride;
   GLuint xblk, yblk;

   char *data;
   char *mallocData;
   GLsizei mallocStorageSize;
   GLenum dataState;
} jsImage;

typedef struct
{
   GLenum format;
   GLenum type;
   GLsizei width;
   GLsizei height;
   GLsizei xstride;
   GLsizei ystride;
   void* data;
} jsRaster;

#define _RGL_TEXTURE_REVALIDATE_LAYOUT   	0x01
#define _RGL_TEXTURE_REVALIDATE_IMAGES		0x02
#define _RGL_TEXTURE_REVALIDATE_PARAMETERS	0x04

typedef struct jsBufferObject jsBufferObject;

typedef struct
{
   GLuint revalidate;
   GLuint target;

   GLuint minFilter;
   GLuint magFilter;
   GLuint gammaRemap;
   GLenum usage;

   GLboolean isRenderTarget;
   GLboolean   isComplete;

   jsBufferObject *referenceBuffer;
   GLintptr	offset;

   RGL::Vector<jsFramebuffer *> framebuffers;

   GLuint		imageCount;
   jsImage*	image;
   void * platformTexture[];
}
jsTexture;

#define _RGL_MAX_TEXTURE_COORDS	8
#define _RGL_MAX_TEXTURE_IMAGE_UNITS	16
#define _RGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS	4

#define _RGL_MAX_TEXTURE_UNITS 4

#define _RGL_MAX_TEXTURE_SIZE_LOG2	12
#define _RGL_MAX_TEXTURE_SIZE	(4096)
#define _RGL_MAX_TEXTURE_SIZE_3D_LOG2	9
#define _RGL_MAX_TEXTURE_SIZE_3D	(512)
#define _RGL_MAX_MODELVIEW_STACK_DEPTH 16
#define _RGL_MAX_PROJECTION_STACK_DEPTH 2
#define _RGL_MAX_TEXTURE_STACK_DEPTH 2

#define _RGL_MAX_VERTEX_ATTRIBS	16

typedef struct
{
   GLuint		bound2D;

   jsTexture*	default2D;

   GLenum      fragmentTarget;

   GLenum		envMode;
   jsColorRGBAf	envColor;

   jsTexture* currentTexture;
} jsTextureImageUnit;

typedef struct
{
   GLuint		revalidate;
   jsMatrixStack	TextureMatrixStack;
} jsTextureCoordsUnit;

enum
{
   _RGL_FRAMEBUFFER_ATTACHMENT_NONE,
   _RGL_FRAMEBUFFER_ATTACHMENT_RENDERBUFFER,
   _RGL_FRAMEBUFFER_ATTACHMENT_TEXTURE,
};

typedef enum PSGLtvStandard
{
   PSGL_TV_STANDARD_NONE,
   PSGL_TV_STANDARD_NTSC_M,
   PSGL_TV_STANDARD_NTSC_J,
   PSGL_TV_STANDARD_PAL_M,
   PSGL_TV_STANDARD_PAL_B,
   PSGL_TV_STANDARD_PAL_D,
   PSGL_TV_STANDARD_PAL_G,
   PSGL_TV_STANDARD_PAL_H,
   PSGL_TV_STANDARD_PAL_I,
   PSGL_TV_STANDARD_PAL_N,
   PSGL_TV_STANDARD_PAL_NC,
   PSGL_TV_STANDARD_HD480I,
   PSGL_TV_STANDARD_HD480P,
   PSGL_TV_STANDARD_HD576I,
   PSGL_TV_STANDARD_HD576P,
   PSGL_TV_STANDARD_HD720P,
   PSGL_TV_STANDARD_HD1080I,
   PSGL_TV_STANDARD_HD1080P,
   PSGL_TV_STANDARD_1280x720_ON_VESA_1280x768 = 128,
   PSGL_TV_STANDARD_1280x720_ON_VESA_1280x1024,
   PSGL_TV_STANDARD_1920x1080_ON_VESA_1920x1200,
} PSGLtvStandard;

typedef enum PSGLdeviceConnector
{
   PSGL_DEVICE_CONNECTOR_NONE,
   PSGL_DEVICE_CONNECTOR_VGA,
   PSGL_DEVICE_CONNECTOR_DVI,
   PSGL_DEVICE_CONNECTOR_HDMI,
   PSGL_DEVICE_CONNECTOR_COMPOSITE,
   PSGL_DEVICE_CONNECTOR_SVIDEO,
   PSGL_DEVICE_CONNECTOR_COMPONENT,
} PSGLdeviceConnector;

typedef enum PSGLbufferingMode
{
   PSGL_BUFFERING_MODE_SINGLE = 1,
   PSGL_BUFFERING_MODE_DOUBLE = 2,
   PSGL_BUFFERING_MODE_TRIPLE = 3,
} PSGLbufferingMode;

typedef enum RescRatioMode
{
   RESC_RATIO_MODE_FULLSCREEN,
   RESC_RATIO_MODE_LETTERBOX,
   RESC_RATIO_MODE_PANSCAN,
} RescRatioMode;

typedef enum RescPalTemporalMode
{
   RESC_PAL_TEMPORAL_MODE_50_NONE,
   RESC_PAL_TEMPORAL_MODE_60_DROP,
   RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE,
   RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE_30_DROP,
   RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE_DROP_FLEXIBLE,
} RescPalTemporalMode;

typedef enum RescInterlaceMode
{
   RESC_INTERLACE_MODE_NORMAL_BILINEAR,
   RESC_INTERLACE_MODE_INTERLACE_FILTER,
} RescInterlaceMode;

typedef struct
{
   GLuint enable;
   GLenum colorFormat;
   GLenum depthFormat;
   GLenum multisamplingMode;
   PSGLtvStandard TVStandard;
   PSGLdeviceConnector connector;
   PSGLbufferingMode bufferingMode;
   GLuint width;
   GLuint height;
   GLuint renderWidth;
   GLuint renderHeight;
   RescRatioMode rescRatioMode;
   RescPalTemporalMode rescPalTemporalMode;
   RescInterlaceMode rescInterlaceMode;
   GLfloat horizontalScale;
   GLfloat verticalScale;
} PSGLdeviceParameters;

struct PSGLdevice
{
   PSGLdeviceParameters deviceParameters;
   GLvoid*		rasterDriver;
   char			platformDevice[];
};

typedef struct
{
   GLenum	mode;
   GLint	firstVertex;
   GLsizei	vertexCount;

   GLuint	xferTotalSize;
   GLuint	indexXferOffset;
   GLuint	indexXferSize;
   GLuint	attribXferTotalSize;
   GLuint	attribXferOffset[_RGL_MAX_VERTEX_ATTRIBS];
   GLuint	attribXferSize[_RGL_MAX_VERTEX_ATTRIBS];
} jsDrawParams;

#define _RGL_ATTRIB_POSITION_INDEX			0
#define _RGL_ATTRIB_WEIGHT_INDEX			1
#define _RGL_ATTRIB_NORMAL_INDEX			2
#define _RGL_ATTRIB_PRIMARY_COLOR_INDEX			3
#define _RGL_ATTRIB_SECONDARY_COLOR_INDEX		4
#define _RGL_ATTRIB_FOG_COORD_INDEX			5
#define _RGL_ATTRIB_POINT_SIZE_INDEX			6
#define _RGL_ATTRIB_BLEND_INDICES_INDEX			7
#define _RGL_ATTRIB_TEX_COORD0_INDEX			8
#define _RGL_ATTRIB_TEX_COORD1_INDEX			9
#define _RGL_ATTRIB_TEX_COORD2_INDEX			10
#define _RGL_ATTRIB_TEX_COORD3_INDEX			11
#define _RGL_ATTRIB_TEX_COORD4_INDEX			12
#define _RGL_ATTRIB_TEX_COORD5_INDEX			13
#define _RGL_ATTRIB_TEX_COORD6_INDEX			14
#define _RGL_ATTRIB_TEX_COORD7_INDEX			15

typedef struct
{
   GLvoid*	clientData;
   GLuint	clientSize;
   GLenum	clientType;
   GLsizei	clientStride;
   GLuint  arrayBuffer;
   GLfloat	value[4];
   GLuint  frequency;
   GLboolean normalized;
} __attribute__((aligned (16))) jsAttribute;

typedef struct
{
   jsAttribute		attrib[_RGL_MAX_VERTEX_ATTRIBS];
   unsigned int DirtyMask;
   unsigned int EnabledMask;
   unsigned int NeedsConversionMask;
   unsigned int HasVBOMask;
   unsigned int ModuloMask;
} __attribute__((aligned (16))) jsAttributeState;

typedef struct
{
   jsAttributeState attribs;
   GLboolean	dirty;
   unsigned int	beenUpdatedMask;
   GLvoid*		cmdBuffer;
   GLuint		cmdNumWords;
} __attribute__((aligned (16))) jsAttribSet;

struct jsBufferObject
{
   GLuint refCount;
   GLsizeiptr size;
   GLenum usage;
   GLboolean mapped;
   GLenum internalFormat;
   GLuint width;
   GLuint height;
   RGL::Vector<jsTexture *> textureReferences;
   RGL::Vector<jsAttribSet *> attribSets;
   void *platformBufferObject[];
};

#define _RGL_CONTEXT_RED_MASK				0x01
#define _RGL_CONTEXT_GREEN_MASK				0x02
#define _RGL_CONTEXT_BLUE_MASK				0x04
#define _RGL_CONTEXT_ALPHA_MASK				0x08
#define _RGL_CONTEXT_DEPTH_MASK				0x10
#define _RGL_CONTEXT_COLOR_MASK				0x0F

#define ELEMENTS_IN_MATRIX	16

typedef struct jsNameSpace
{
   void** data;
   void** firstFree;
   unsigned long capacity;
} jsNameSpace;

typedef void *( *jsTexNameSpaceCreateFunction )( void );
typedef void( *jsTexNameSpaceDestroyFunction )( void * );

typedef struct jsTexNameSpace
{
   void** data;
   GLuint capacity;
   jsTexNameSpaceCreateFunction create;
   jsTexNameSpaceDestroyFunction destroy;
}
jsTexNameSpace;

struct PSGLcontext
{
   GLenum			error;
   int			MatrixMode;
   jsMatrixStack	ModelViewMatrixStack;
   jsMatrixStack	ProjectionMatrixStack;
   GLfloat			LocalToScreenMatrixf[ELEMENTS_IN_MATRIX];
   GLfloat 		InverseModelViewMatrixf[ELEMENTS_IN_MATRIX];
   GLboolean		InverseModelViewValid;
   GLfloat			ScalingFactor;
   jsViewPort		ViewPort;
   jsAttributeState defaultAttribs0;
   jsAttributeState *attribs;
   jsTexNameSpace	attribSetNameSpace;
   GLuint			attribSetName;
   GLboolean		attribSetDirty;
   jsColorRGBAf	ClearColor;
   jsColorRGBAf	AccumClearColor;
   GLboolean		ShaderSRGBRemap;
   GLboolean		Blending;
   GLboolean		BlendingMrt[3];
   GLenum			BlendEquationRGB;
   GLenum			BlendEquationAlpha;
   GLenum			BlendFactorSrcRGB;
   GLenum			BlendFactorDestRGB;
   GLenum			BlendFactorSrcAlpha;
   GLenum			BlendFactorDestAlpha;
   jsColorRGBAf	BlendColor;
   jsTexNameSpace textureNameSpace;
   GLuint			ActiveTexture;
   GLuint			CS_ActiveTexture;
   jsTextureImageUnit	TextureImageUnits[_RGL_MAX_TEXTURE_IMAGE_UNITS];
   jsTextureImageUnit* CurrentImageUnit;
   jsTextureCoordsUnit	TextureCoordsUnits[_RGL_MAX_TEXTURE_COORDS];
   jsTextureCoordsUnit* CurrentCoordsUnit;
   jsTexture *VertexTextureImages[_RGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS];
   GLsizei		packAlignment;
   GLsizei		unpackAlignment;
   jsTexNameSpace	bufferObjectNameSpace;
   GLuint	ArrayBuffer;
   GLuint	PixelUnpackBuffer;
   GLuint	TextureBuffer;
   GLuint			framebuffer;
   jsTexNameSpace	framebufferNameSpace;
   GLboolean		VertexProgram;
   struct _CGprogram*	BoundVertexProgram;
   GLboolean		FragmentProgram;
   struct _CGprogram*	BoundFragmentProgram;
   GLboolean		AllowTXPDemotion;
   GLboolean		VSync;
   GLuint			needValidate;
   GLboolean		everAttached;
   CGerror RGLcgLastError;
   CGerrorCallbackFunc RGLcgErrorCallbackFunction;
   CGcontext RGLcgContextHead;
   jsNameSpace  cgContextNameSpace;
   jsNameSpace  cgProgramNameSpace;
   jsNameSpace  cgParameterNameSpace;
};

#define jsContextGetMatrixStack(mContext, mMatrixMode, mMatrixStack) do \
{\
	switch(mMatrixMode)\
	{\
		case GL_MODELVIEW:\
			mMatrixStack = &((mContext)->ModelViewMatrixStack);\
			break;\
		case GL_PROJECTION:\
			mMatrixStack = &((mContext)->ProjectionMatrixStack);\
			break;\
		case GL_TEXTURE:\
			if ((mContext)->CurrentCoordsUnit) mMatrixStack = &((mContext)->CurrentCoordsUnit->TextureMatrixStack);\
			else mMatrixStack=NULL; \
			break;\
		default: \
			break; \
	}\
} while(0)

#define jsContextGetMatrixf(mContext, mMatrixMode, mMatrixStack, mMatrix) do \
{\
	jsContextGetMatrixStack(mContext, mMatrixMode, mMatrixStack);\
	if (mMatrixStack) mMatrix = (mMatrixStack)->MatrixStackf+(mMatrixStack)->MatrixStackPtr*ELEMENTS_IN_MATRIX;\
} while (0)

#define MAX(A,B) ((A)>(B)?(A):(B))
#define MIN(A,B) ((A)<(B)?(A):(B))
#define RGL_LIKELY(COND) (COND)
#define RGL_UNLIKELY(COND) (COND)

#define _RGL_ALLOC_FIRST_FIT 0
#define _RGL_ALLOC_BEST_FIT 1

static inline float _RGLClampf( const float value )
{
   return MAX( MIN( value, 1.f ), 0.f );
}

static inline unsigned int endianSwapWord( unsigned int v )
{
   return ( v&0xff ) << 24 | ( v&0xff00 ) << 8 |
   ( v&0xff0000 ) >> 8 | ( v&0xff000000 ) >> 24;
}

static inline int _RGLLog2( unsigned int i )
{
   int l = 0;
   while ( i )
   {
      ++l;
      i >>= 1;
   }
   return l -1;
}

static inline unsigned long _RGLPad(unsigned long x, unsigned long pad)
{
   return ( x + pad - 1 ) / pad*pad;
}

static inline char* _RGLPadPtr(const char* p, unsigned int pad)
{
   intptr_t x = (intptr_t)p;
   x = ( x + pad - 1 ) / pad * pad;
   return ( char* )x;
}

typedef struct MemoryBlockManager_t_
{
   char *memory;
   GLuint size;
   GLuint minAlignment;
   GLenum method;
   GLuint *book;
   GLint bookSize;
   GLint bookAlloc;
} MemoryBlockManager_t;

typedef unsigned long jsName;

void _RGLInitNameSpace( struct jsNameSpace * name );
void _RGLFreeNameSpace( struct jsNameSpace * name );
jsName _RGLCreateName( struct jsNameSpace * ns, void* object );
unsigned int _RGLIsName( struct jsNameSpace* ns, jsName name );
void _RGLEraseName( struct jsNameSpace* ns, jsName name );

static inline void * _RGLGetNamedValue( struct jsNameSpace* ns, jsName name )
{
	return ns->data[name - 1];
}

void _RGLTexNameSpaceInit( jsTexNameSpace *ns, jsTexNameSpaceCreateFunction create, jsTexNameSpaceDestroyFunction destroy );
void _RGLTexNameSpaceFree( jsTexNameSpace *ns );
void _RGLTexNameSpaceResetNames( jsTexNameSpace *ns );
GLuint _RGLTexNameSpaceGetFree( jsTexNameSpace *ns );
GLboolean _RGLTexNameSpaceCreateNameLazy( jsTexNameSpace *ns, GLuint name );
GLboolean _RGLTexNameSpaceIsName( jsTexNameSpace *ns, GLuint name );
void _RGLTexNameSpaceGenNames( jsTexNameSpace *ns, GLsizei n, GLuint *names );
void _RGLTexNameSpaceDeleteNames( jsTexNameSpace *ns, GLsizei n, const GLuint *names );
void _RGLTexNameSpaceReinit( jsTexNameSpace * saved, jsTexNameSpace * active );

#define RGL_MEMORY_ALLOC_ERROR	0

#define GMM_ERROR                   0xFFFFFFFF
#define GMM_TILE_ALIGNMENT          0x10000
#define GMM_ALIGNMENT               128	
#define GMM_RSX_WAIT_INDEX          254
#define GMM_PPU_WAIT_INDEX          255	
#define GMM_BLOCK_COUNT             512
#define GMM_TILE_BLOCK_COUNT        16

#define GMM_NUM_FREE_BINS           22
#define GMM_FREE_BIN_0              0x80	// 0x00 - 0x80
#define GMM_FREE_BIN_1              0x100	// 0x80 - 0x100
#define GMM_FREE_BIN_2              0x180	// ...
#define GMM_FREE_BIN_3              0x200
#define GMM_FREE_BIN_4              0x280
#define GMM_FREE_BIN_5              0x300
#define GMM_FREE_BIN_6              0x380
#define GMM_FREE_BIN_7              0x400
#define GMM_FREE_BIN_8              0x800
#define GMM_FREE_BIN_9              0x1000
#define GMM_FREE_BIN_10             0x2000
#define GMM_FREE_BIN_11             0x4000
#define GMM_FREE_BIN_12             0x8000
#define GMM_FREE_BIN_13             0x10000
#define GMM_FREE_BIN_14             0x20000
#define GMM_FREE_BIN_15             0x40000
#define GMM_FREE_BIN_16             0x80000
#define GMM_FREE_BIN_17             0x100000
#define GMM_FREE_BIN_18             0x200000
#define GMM_FREE_BIN_19             0x400000
#define GMM_FREE_BIN_20             0x800000
#define GMM_FREE_BIN_21             0x1000000

typedef struct GmmFixedAllocData
{
   char        **ppBlockList[2];
   uint16_t    **ppFreeBlockList[2];
   uint16_t    *pBlocksUsed[2];
   uint16_t    BlockListCount[2];
} GmmFixedAllocData;

typedef struct GmmBaseBlock
{
   uint8_t     isTile;
   uint8_t     isMain;
   uint32_t    address;
   uint32_t    size;
} GmmBaseBlock;

typedef struct GmmBlock
{
   GmmBaseBlock    base;
   struct GmmBlock *pPrev;
   struct GmmBlock *pNext;
   uint8_t     isPinned;
   struct GmmBlock *pPrevFree;
   struct GmmBlock *pNextFree;
   uint32_t    fence;
} GmmBlock;

typedef struct GmmTileBlock
{
   GmmBaseBlock        base;
   struct GmmTileBlock *pPrev;
   struct GmmTileBlock *pNext;

   uint32_t    tileTag;
   void        *pData;
} GmmTileBlock;

typedef struct GmmAllocator
{
   uint32_t    memoryBase;

   uint32_t    startAddress;
   uint32_t    size;
   uint32_t    freeAddress;

   GmmBlock    *pHead;
   GmmBlock    *pTail;
   GmmBlock    *pSweepHead;
   uint32_t    freedSinceSweep;
   uint32_t    tileStartAddress;
   uint32_t    tileSize;
   GmmTileBlock    *pTileHead;
   GmmTileBlock    *pTileTail;
   GmmBlock    *pPendingFreeHead;
   GmmBlock    *pPendingFreeTail;
   GmmBlock    *pFreeHead[GMM_NUM_FREE_BINS];
   GmmBlock    *pFreeTail[GMM_NUM_FREE_BINS];
   uint32_t    totalSize;
} GmmAllocator;

uint32_t gmmInit(
    const void *localMemoryBase,
    const void *localStartAddress,
    const uint32_t localSize,
    const void *mainMemoryBase,
    const void *mainStartAddress,
    const uint32_t mainSize
);

uint32_t gmmIdToOffset(const uint32_t id);
char *gmmIdToAddress(const uint32_t id);
uint32_t gmmFree(const uint32_t freeId);
uint32_t gmmAlloc(const uint8_t isTile, const uint32_t size);
uint32_t gmmAllocExtendedTileBlock(const uint32_t size, const uint32_t tag);

void gmmSetTileAttrib(
    const uint32_t id,
    const uint32_t tag,
    void *pData
);


#define GCM_FUNC_BUFFERED( GCM_FUNCTION, COMMAND_BUFFER, ...) \
					  { \
					  CellGcmContextData gcmContext; \
					  gcmContext.current = (uint32_t *)COMMAND_BUFFER; \
					  GCM_FUNCTION ## UnsafeInline( &gcmContext, __VA_ARGS__ ); \
					  COMMAND_BUFFER = (typeof(COMMAND_BUFFER))gcmContext.current;   \
					  }

#define GCM_FUNC_BUFFERED_NO_ARGS( GCM_FUNCTION, COMMAND_BUFFER ) \
					  { \
					  CellGcmContextData gcmContext; \
					  gcmContext.current = (uint32_t *)COMMAND_BUFFER; \
					  GCM_FUNCTION ## UnsafeInline( &gcmContext ); \
					  COMMAND_BUFFER = (typeof(COMMAND_BUFFER))gcmContext.current;   \
					  }

#define _RGLTransferDataVidToVid(dstId, dstIdOffset, dstPitch, dstX, dstY, srcId, srcIdOffset, srcPitch, srcX, srcY, width, height, bytesPerPixel) \
{ \
    GLuint dstOffset_tmp, srcOffset_tmp; \
    uint8_t mode; \
    dstOffset_tmp = gmmIdToOffset(dstId) + dstIdOffset; \
    srcOffset_tmp = gmmIdToOffset(srcId) + srcIdOffset; \
    mode = CELL_GCM_TRANSFER_LOCAL_TO_LOCAL; \
    if ( gmmIdIsMain(srcId) && gmmIdIsMain(dstId) ) \
        mode = CELL_GCM_TRANSFER_MAIN_TO_MAIN; \
    else if ( gmmIdIsMain(srcId) )	/* choose source DMA context */ \
        mode = CELL_GCM_TRANSFER_MAIN_TO_LOCAL; \
    else if ( gmmIdIsMain(dstId) )	/* choose destination DMA context */ \
        mode = CELL_GCM_TRANSFER_LOCAL_TO_MAIN; \
    cellGcmSetTransferImageInline( &_RGLState.fifo, mode, dstOffset_tmp, (dstPitch), (dstX), (dstY), (srcOffset_tmp), (srcPitch), (srcX), (srcY), (width), (height), (bytesPerPixel) ); \
}

#define RGL_BIG_ENDIAN

#define _RGL_LINEAR_BUFFER_ALIGNMENT 128
#define _RGL_HOST_BUFFER_ALIGNMENT 128

#define _RGL_TRANSIENT_MEMORY_DEFAULT		(32 << 20)
#define _RGL_PERSISTENT_MEMORY_DEFAULT	(160 << 20)
#define _RGL_FIFO_SIZE_DEFAULT		(256 * 1024)
#define _RGL_HOST_SIZE_DEFAULT		(0)
#define _RGL_TRANSIENT_ENTRIES_DEFAULT	64

#define _RGL_BUFFER_OBJECT_BLOCK_SIZE 128
#define SUBPIXEL_ADJUST (0.5/4096)

typedef struct PSGLinitOptions
{
   GLuint			enable;
   int				errorConsole;
   GLuint			fifoSize;
   GLuint			hostMemorySize;
} PSGLinitOptions;

struct  RGLSemaphore
{
   GLuint val;
   GLuint pad0;
   GLuint pad1;
   GLuint pad2;
};

struct RGLSemaphoreMemory
{
   RGLSemaphore userSemaphores[256];
};

struct RGLResource
{
   char *localAddress;
   GLuint localSize;
   GLuint MemoryClock;
   GLuint GraphicsClock;
   char *  hostMemoryBase;
   GLuint  hostMemorySize;
   GLuint  hostMemoryReserved;
   unsigned long dmaPushBufferOffset;
   char *  dmaPushBuffer;
   GLuint  dmaPushBufferSize;
   void*   dmaControl;
   RGLSemaphoreMemory    *semaphores;
};

typedef volatile struct
{
   GLuint Ignored00[0x010];
   GLuint Put;
   GLuint Get;
   GLuint Reference;
   GLuint Ignored01[0x1];
   GLuint SetReference;
   GLuint TopLevelGet;
   GLuint Ignored02[0x2];
   GLuint SetContextDmaSemaphore;
   GLuint SetSemaphoreOffset;
   GLuint SetSemaphoreAcquire;
   GLuint SetSemaphoreRelease;
   GLuint Ignored03[0x7e4];
} RGLControlDma;

struct RGLFifo: public CellGcmContextData
{
   RGLControlDma *dmaControl;
   uint32_t *dmaPushBufferBegin;
   uint32_t *dmaPushBufferEnd;
   GLuint  fifoBlockSize;
   unsigned long dmaPushBufferOffset;
   GLuint dmaPushBufferSizeInWords;
   uint32_t *lastPutWritten;
   uint32_t *lastGetRead;
   GLuint lastSWReferenceWritten;
   GLuint lastSWReferenceFlushed;
   GLuint lastHWReferenceRead;
   uint32_t *dmaPushBufferGPU;
   int spuid;
};

typedef struct RGLRenderTarget RGLRenderTarget;
typedef struct RGLCachedState RGLCachedState;
typedef struct RGLBlendState RGLBlendState;
typedef struct RGLInterpolantState RGLInterpolantState;

struct RGLInterpolantState
{
   GLuint vertexProgramAttribMask;
   GLuint fragmentProgramAttribMask;
};

struct RGLBlendState
{
   GLuint alphaFunc;
   GLfloat alphaRef;
};

struct RGLRenderTarget
{
   GLuint  colorFormat;
   GLuint  colorBufferCount;
   GLuint  yInverted;
   CellGcmSurface  gcmRenderTarget;
};

struct RGLCachedState
{
   RGLBlendState blend;
   RGLViewportState viewport;
   RGLInterpolantState interpolant;
};

struct RGLState
{
   char			*localAddress;
   void 		*hostMemoryBase;
   GLuint		hostMemorySize;
   RGLSemaphoreMemory	*semaphores;
   RGLFifo		fifo;
   RGLRenderTarget	renderTarget;
   RGLCachedState	state;
   CellGcmConfig	config;
   GLuint		labelValue; 
};

GLboolean _RGLInit( PSGLinitOptions* options, RGLResource *resource );

#define _RGL_SEMA_NEVENTS	128
#define _RGL_SEMA_BASE	64
#define _RGL_SEMA_FENCE	(_RGL_SEMA_BASE+_RGL_SEMA_NEVENTS+0)

void _RGLIncFenceRef( GLuint* ref );

typedef struct
{
   GLenum pool;
   unsigned int bufferId;
   unsigned int bufferSize;
   unsigned int pitch;

   GLuint mapCount;
   GLenum mapAccess;
} RGLBufferObject;

void _RGLSetNativeCgVertexProgram( const void *header );
void _RGLSetNativeCgFragmentProgram( const void *header );

GLboolean _RGLTryResizeTileRegion( GLuint address, GLuint size, void* data );
void _RGLGetTileRegionInfo( void* data, GLuint *address, GLuint *size );

static inline GLuint _RGLPlatformGetBitsPerPixel( GLenum internalFormat )
{
   switch ( internalFormat )
   {
      case RGL_HILO8:
      case RGL_RGB5_A1_SCE:
      case RGL_RGB565_SCE:
         return 16;
      case RGL_ALPHA8:
	 return 8;
      case RGL_RGBX8:
      case RGL_RGBA8:
      case RGL_ABGR8:
      case RGL_ARGB8:
      case RGL_BGRA8:
	 return 32;
      default:
	 return 0;
   }
}


void static inline _RGLFifoGlViewport( GLint x, GLint y, GLsizei width, GLsizei height, GLclampf zNear = 0.0f, GLclampf zFar = 1.0f )
{
   RGLViewportState *vp = &_RGLState.state.viewport;
   RGLRenderTarget *rt = &_RGLState.renderTarget;
   GLint clipX0, clipX1, clipY0, clipY1;

   vp->x = x;
   vp->y = y;
   vp->w = width;
   vp->h = height;

   clipX0 = x;
   clipX1 = x + width;
   clipY0 = y;
   clipY1 = y + height;

   if ( rt->yInverted )
   {
      clipY0 = rt->gcmRenderTarget.height - ( y + height );
      clipY1 = rt->gcmRenderTarget.height - y;
   }

   if ( clipX0 < 0 )
      clipX0 = 0;

   if ( clipY0 < 0 )
      clipY0 = 0;

   if ( clipX1 >= CELL_GCM_MAX_RT_DIMENSION )
      clipX1 = CELL_GCM_MAX_RT_DIMENSION;

   if ( clipY1 >= CELL_GCM_MAX_RT_DIMENSION )
      clipY1 = CELL_GCM_MAX_RT_DIMENSION;

   if (( clipX1 <= clipX0 ) || ( clipY1 <= clipY0 ) )
      clipX0 = clipY0 = clipX1 = clipY1 = 0;

   vp->xScale = width * 0.5f;
   vp->xCenter = ( GLfloat )( x + vp->xScale + RGL_SUBPIXEL_ADJUST );
   vp->yScale = height;

   if ( rt->yInverted )
   {
      vp->yScale *= -0.5f;
      vp->yCenter = ( GLfloat )( rt->gcmRenderTarget.height - y +  vp->yScale + RGL_SUBPIXEL_ADJUST );
   }
   else
   {
      vp->yScale *= 0.5f;
      vp->yCenter = ( GLfloat )( y +  vp->yScale + RGL_SUBPIXEL_ADJUST );
   }

   float scale[4] = {  vp->xScale,  vp->yScale,  0.5f, 0.0f };
   float offset[4] = {   vp->xCenter,  vp->yCenter,  0.5f, 0.0f };

   cellGcmSetViewportInline( &_RGLState.fifo, clipX0, clipY0, clipX1 - clipX0,
		   clipY1 - clipY0, zNear, zFar, scale, offset );
}

#define PSGL_DEVICE_PARAMETERS_COLOR_FORMAT             0x0001
#define PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT             0x0002
#define PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE       0x0004
#define PSGL_DEVICE_PARAMETERS_TV_STANDARD              0x0008
#define PSGL_DEVICE_PARAMETERS_CONNECTOR                0x0010
#define PSGL_DEVICE_PARAMETERS_BUFFERING_MODE           0x0020
#define PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT             0x0040
#define PSGL_DEVICE_PARAMETERS_RESC_RENDER_WIDTH_HEIGHT 0x0080
#define PSGL_DEVICE_PARAMETERS_RESC_RATIO_MODE          0x0100
#define PSGL_DEVICE_PARAMETERS_RESC_PAL_TEMPORAL_MODE   0x0200
#define PSGL_DEVICE_PARAMETERS_RESC_INTERLACE_MODE      0x0400
#define PSGL_DEVICE_PARAMETERS_RESC_ADJUST_ASPECT_RATIO 0x0800

// mask for validation
#define PSGL_VALIDATE_NONE                         0x00000000 
#define PSGL_VALIDATE_FRAMEBUFFER                  0x00000001             
#define PSGL_VALIDATE_TEXTURES_USED                0x00000002 
#define PSGL_VALIDATE_VERTEX_PROGRAM               0x00000004 
#define PSGL_VALIDATE_VERTEX_CONSTANTS 		   0x00000008
#define PSGL_VALIDATE_VERTEX_TEXTURES_USED 	   0x00000010   
#define PSGL_VALIDATE_FFX_VERTEX_PROGRAM 	   0x00000020
#define PSGL_VALIDATE_FRAGMENT_PROGRAM 		   0x00000040
#define PSGL_VALIDATE_FFX_FRAGMENT_PROGRAM 	   0x00000080
#define PSGL_VALIDATE_VIEWPORT 			   0x00000200
#define PSGL_VALIDATE_FACE_CULL 		   0x00010000
#define PSGL_VALIDATE_BLENDING 			   0x00020000
#define PSGL_VALIDATE_POINT_RASTER 		   0x00040000
#define PSGL_VALIDATE_LINE_RASTER 		   0x00080000
#define PSGL_VALIDATE_POLYGON_OFFSET 		   0x00100000
#define PSGL_VALIDATE_SHADE_MODEL 		   0x00200000
#define PSGL_VALIDATE_LOGIC_OP 			   0x00400000
#define PSGL_VALIDATE_MULTISAMPLING 		   0x00800000
#define PSGL_VALIDATE_POLYGON_MODE 		   0x01000000
#define PSGL_VALIDATE_PRIMITIVE_RESTART 	   0x02000000
#define PSGL_VALIDATE_CLIP_PLANES 		   0x04000000
#define PSGL_VALIDATE_SHADER_SRGB_REMAP 	   0x08000000
#define PSGL_VALIDATE_POINT_SPRITE 		   0x10000000
#define PSGL_VALIDATE_TWO_SIDE_COLOR 		   0x20000000
#define PSGL_VALIDATE_ALL 			   0x3FFFFFFF
	

#define	PSGL_INIT_PERSISTENT_MEMORY_SIZE	0x0004
#define	PSGL_INIT_TRANSIENT_MEMORY_SIZE		0x0008
#define	PSGL_INIT_ERROR_CONSOLE			0x0010
#define	PSGL_INIT_FIFO_SIZE			0x0020
#define	PSGL_INIT_HOST_MEMORY_SIZE		0x0040
#define PSGL_INIT_USE_PMQUERIES			0x0080


extern void	psglInit( PSGLinitOptions* options );
extern void	psglExit();

PSGLdevice*	psglCreateDeviceAuto( GLenum colorFormat, GLenum depthFormat, GLenum multisamplingMode );
PSGLdevice*	psglCreateDeviceExtended( const PSGLdeviceParameters *parameters );
GLfloat psglGetDeviceAspectRatio( const PSGLdevice * device );
void psglGetDeviceDimensions( const PSGLdevice * device, GLuint *width, GLuint *height );
void psglDestroyDevice( PSGLdevice* device );

void psglMakeCurrent( PSGLcontext* context, PSGLdevice* device );
PSGLcontext* psglCreateContext();
void psglDestroyContext( PSGLcontext* LContext );
void psglResetCurrentContext();
PSGLcontext* psglGetCurrentContext();
PSGLdevice* psglGetCurrentDevice();
void psglSwap(void);

#ifdef __cplusplus
}
#endif

#endif
