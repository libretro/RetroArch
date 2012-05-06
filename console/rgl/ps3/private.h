#ifndef _RGL_PRIVATE_H
#define _RGL_PRIVATE_H

#include "rgl.h"

#ifndef OS_VERSION_NUMERIC
#define OS_VERSION_NUMERIC 0x160
#endif

#ifdef __cplusplus
extern "C"
{
#endif


#ifdef __cplusplus
#define _RGL_EXTERN_C extern "C"
#else
#define _RGL_EXTERN_C
#endif

extern PSGLcontext *_CurrentContext;
extern PSGLdevice *_CurrentDevice;

#define GL_UNSIGNED_BYTE_4_4 0x4A00
#define GL_UNSIGNED_BYTE_4_4_REV 0x4A01
#define GL_UNSIGNED_BYTE_6_2 0x4A02
#define GL_UNSIGNED_BYTE_2_6_REV 0x4A03
#define GL_UNSIGNED_SHORT_12_4 0x4A04
#define GL_UNSIGNED_SHORT_4_12_REV 0x4A05
#define GL_UNSIGNED_BYTE_2_2_2_2 0x4A06
#define GL_UNSIGNED_BYTE_2_2_2_2_REV 0x4A07

#define GL_FLOAT_RGBA32 0x888B

#define RGL_SETRENDERTARGET_MAXCOUNT  4

#define RGL_NOP                     (0x00000000)
#define DEFAULT_FIFO_BLOCK_SIZE     (0x10000)

#define RGL_ATTRIB_COUNT                          16

#define RGL_F0_DOT_0		12582912.0f

#define RGL_CLAMPF_01(x)	((x) >= 0.0f ? ((x) > 1.0f ? 1.0f : (x)) : 0.0f)

#define ENDIAN_32(X, F) ((F) ? endianSwapWord(X) : (X))

#define VERTEX_PROFILE_INDEX 0
#define FRAGMENT_PROFILE_INDEX 1


typedef union
{
	unsigned int i;
	float f;
} jsIntAndFloat;

static const jsIntAndFloat _RGLNan = {i: 0x7fc00000U};
static const jsIntAndFloat _RGLInfinity = {i: 0x7f800000U};


typedef struct RGLRenderTargetEx RGLRenderTargetEx;
struct RGLRenderTargetEx
{
	RGLEnum   colorFormat;
	GLuint      colorBufferCount;
	GLuint      colorId[RGL_SETRENDERTARGET_MAXCOUNT];
	GLuint      colorIdOffset[RGL_SETRENDERTARGET_MAXCOUNT];
	GLuint      colorPitch[RGL_SETRENDERTARGET_MAXCOUNT];
	GLboolean   yInverted;
	GLuint      xOffset;
	GLuint      yOffset;
	GLuint      width;
	GLuint      height;
};

struct jsPlatformFramebuffer: public jsFramebuffer
{
    RGLRenderTargetEx rt;
    GLuint colorBufferMask;
    GLboolean complete;
    jsPlatformFramebuffer(): jsFramebuffer()
    {
        memset( &rt, 0, sizeof( rt ) );
    };
    virtual ~jsPlatformFramebuffer() {};
};

typedef struct _RGLDriver_
{
    RGLRenderTargetEx rt;
    GLuint colorBufferMask;
    GLboolean rtValid;
    GLboolean invalidateVertexCache;
    GLuint flushBufferCount;
    GLuint fpLoadProgramId;
    GLuint fpLoadProgramOffset;
}
RGLDriver;

typedef struct
{
    GLuint SET_TEXTURE_CONTROL3;
    GLuint SET_TEXTURE_OFFSET;
    GLuint SET_TEXTURE_FORMAT;
    GLuint SET_TEXTURE_ADDRESS;
    GLuint SET_TEXTURE_CONTROL0;
    GLuint SET_TEXTURE_CONTROL1;
    GLuint SET_TEXTURE_FILTER;
    GLuint SET_TEXTURE_IMAGE_RECT;
}
RGLTextureMethods;

struct RGLTextureAddress
{
    GLuint gamma;
};

struct RGLTextureFilter
{
    GLuint 	min;
    GLuint 	mag;
    GLint	bias;
};

struct RGLTextureMethodParams
{
    RGLTextureAddress  address;
    RGLTextureFilter   filter;
};


typedef struct
{
    GLuint 		baseWidth;
    GLuint 		baseHeight;
    RGLEnum		internalFormat;
    GLuint 		pixelBits;
    GLuint 		pitch;
} RGLTextureLayout;

typedef struct
{
    GLenum pool;
    RGLTextureMethodParams 	gcmMethods;
    CellGcmTexture		gcmTexture;
    GLuint 			gpuAddressId;
    GLuint                      gpuAddressIdOffset;
    GLuint 			gpuSize;
    RGLTextureLayout 		gpuLayout;
    jsBufferObject* pbo;
} RGLTexture;

typedef struct  _tagMODESTRUC
{
	GLushort  wHorizVisible;
	GLushort  wVertVisible;
	GLushort  wInterlacedMode;
	GLushort  wRefresh;
	GLushort  wHorizTotal;
	GLushort  wHorizBlankStart;
	GLushort  wHorizSyncStart;
	GLushort  wHorizSyncEnd;
	GLushort  wHorizBlankEnd;
	GLushort  wVertTotal;
	GLushort  wVertBlankStart;
	GLushort  wVertSyncStart;
	GLushort  wVertSyncEnd;
	GLushort  wVertBlankEnd;
	GLuint      dwDotClock;
	GLushort  wHSyncPolarity;
	GLushort  wVSyncPolarity;
}
MODESTRUC;

enum {
	_RGL_SURFACE_SOURCE_TEMPORARY,
	_RGL_SURFACE_SOURCE_DEVICE,
	_RGL_SURFACE_SOURCE_TEXTURE,
	_RGL_SURFACE_SOURCE_PBO,
};

enum {
	_RGL_SURFACE_POOL_NONE,
	_RGL_SURFACE_POOL_LINEAR,
	_RGL_SURFACE_POOL_SYSTEM,
};


typedef struct
{
	GLenum source;
	GLuint width, height;
	GLuint bpp;
	GLuint pitch;
	RGLEnum format;
	GLenum pool;
	char* ppuData;
	GLuint dataId;
	GLuint dataIdOffset;
} RGLSurface;


typedef struct
{
	RGLRenderTargetEx rt;
	RGLSurface color[3];
	GLuint drawBuffer;
	GLuint scanBuffer;
	GLuint RescColorBuffersId;
	GLuint RescVertexArrayId;
	GLuint RescFragmentShaderId;

	const MODESTRUC *ms;
	GLboolean vsync;
	GLenum deviceType;
	GLenum TVStandard;
	GLenum TVFormat;
	GLuint swapFifoRef;
	GLuint swapFifoRef2;
	GLboolean setOffset;
	GLboolean signal;
	GLuint semaValue;
	unsigned int syncMethod;
} RGLDevice;

int32_t _RGLOutOfSpaceCallback( struct CellGcmContextData *con, uint32_t space );

typedef struct _RGLShader_
{
    GLuint loadAddressId;
    CgBinaryProgram __attribute__(( aligned( 16 ) ) ) program;
} RGLShader;


void        _RGLFifoFinish( RGLFifo *fifo );

#define _RGLFifoFlush(fifo) \
{ \
    uint32_t offsetInBytes = 0; \
    cellGcmAddressToOffset( fifo->current, &offsetInBytes ); \
    cellGcmFlush( &_RGLState.fifo); \
    fifo->dmaControl->Put = offsetInBytes; \
    fifo->lastPutWritten = fifo->current; \
    fifo->lastSWReferenceFlushed = fifo->lastSWReferenceWritten; \
}

#define RGL_PAGE_SIZE                             0x1000
#define RGL_LM_MAX_TOTAL_QUERIES		    800



typedef struct RGLTextureState RGLTextureState;
struct RGLTextureState
{
	GLuint hwTexAddress;
	GLuint hwTexFilter;
	GLuint hwTexControl0;
};

void        _RGLDestroy( void );

typedef void( * RGLcontextHookFunction )( PSGLcontext *context );
extern RGLcontextHookFunction _RGLContextCreateHook;
extern RGLcontextHookFunction _RGLContextDestroyHook;

extern PSGLcontext*	_RGLContextCreate (void);
extern void		_RGLContextFree( PSGLcontext* LContext );
extern void		_RGLSetError( GLenum error );
void _RGLAttachContext( PSGLdevice *device, PSGLcontext* context );

extern jsTexture *_RGLGetCurrentTexture( const jsTextureImageUnit *unit, GLenum target );

void _RGLVertexAttribPointerNV( GLuint index, GLint fsize, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer );
void _RGLEnableVertexAttribArrayNV( GLuint index );
void _RGLDisableVertexAttribArrayNV( GLuint index );

void _RGLFifoGlSetRenderTarget( RGLRenderTargetEx const * const args );

extern void _RGLDeviceInit( PSGLinitOptions* options );
extern void _RGLDeviceExit (void);

static inline GLuint RGL_QUICK_FLOAT2UINT( const GLfloat f )
{
    union
    {
        GLfloat f;
        GLuint ui;
    } t;
    t.f = f + RGL_F0_DOT_0;
    return t.ui & 0xffff;
}

static inline void RGL_CALC_COLOR_LE_ARGB8( GLuint *color0, const GLfloat r, const GLfloat g, const GLfloat b, const GLfloat a )
{
    GLuint r2 = RGL_QUICK_FLOAT2UINT( r * 255.0f );
    GLuint g2 = RGL_QUICK_FLOAT2UINT( g * 255.0f );
    GLuint b2 = RGL_QUICK_FLOAT2UINT( b * 255.0f );
    GLuint a2 = RGL_QUICK_FLOAT2UINT( a * 255.0f );
    *color0 = ( a2 << 24 ) | ( r2 << 16 ) | ( g2 << 8 ) | ( b2 << 0 );
}

static inline GLuint _RGLMapMinTextureFilter( GLenum filter )
{
    switch ( filter )
    {
        case GL_NEAREST:
            return CELL_GCM_TEXTURE_NEAREST;
            break;
        case GL_LINEAR:
            return CELL_GCM_TEXTURE_LINEAR;
            break;
        case GL_NEAREST_MIPMAP_NEAREST:
            return CELL_GCM_TEXTURE_NEAREST_NEAREST;
            break;
        case GL_NEAREST_MIPMAP_LINEAR:
            return CELL_GCM_TEXTURE_NEAREST_LINEAR;
            break;
        case GL_LINEAR_MIPMAP_NEAREST:
            return CELL_GCM_TEXTURE_LINEAR_NEAREST;
            break;
        case GL_LINEAR_MIPMAP_LINEAR:
            return CELL_GCM_TEXTURE_LINEAR_LINEAR;
            break;
        default:
            return 0;
    }
    return filter;
}

static inline GLuint _RGLMapMagTextureFilter( GLenum filter )
{
    switch ( filter )
    {
        case GL_NEAREST:
            return CELL_GCM_TEXTURE_NEAREST;
            break;
        case GL_LINEAR:
            return CELL_GCM_TEXTURE_LINEAR;
            break;
        default:
            return 0;
    }
    return filter;
}

static inline void _RGLMapTextureFormat( GLuint internalFormat, uint8_t & gcmFormat, uint32_t & remap )
{
    gcmFormat = 0;

    switch ( internalFormat )
    {
	    case RGL_ALPHA8:                 // in_rgba = xxAx, out_rgba = 000A
		    {
			    gcmFormat =  CELL_GCM_TEXTURE_B8;
			    remap = CELL_GCM_REMAP_MODE(
					    CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
					    CELL_GCM_TEXTURE_REMAP_FROM_B,
					    CELL_GCM_TEXTURE_REMAP_FROM_R,
					    CELL_GCM_TEXTURE_REMAP_FROM_B,
					    CELL_GCM_TEXTURE_REMAP_FROM_B,
					    CELL_GCM_TEXTURE_REMAP_REMAP,
					    CELL_GCM_TEXTURE_REMAP_ZERO,
					    CELL_GCM_TEXTURE_REMAP_ZERO,
					    CELL_GCM_TEXTURE_REMAP_ZERO );

		    }
		    break;
	    case RGL_ARGB8:                  // in_rgba = RGBA, out_rgba = RGBA
		    {
			    gcmFormat =  CELL_GCM_TEXTURE_A8R8G8B8;
			    remap = CELL_GCM_REMAP_MODE(
					    CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
					    CELL_GCM_TEXTURE_REMAP_FROM_A,
					    CELL_GCM_TEXTURE_REMAP_FROM_R,
					    CELL_GCM_TEXTURE_REMAP_FROM_G,
					    CELL_GCM_TEXTURE_REMAP_FROM_B,
					    CELL_GCM_TEXTURE_REMAP_REMAP,
					    CELL_GCM_TEXTURE_REMAP_REMAP,
					    CELL_GCM_TEXTURE_REMAP_REMAP,
					    CELL_GCM_TEXTURE_REMAP_REMAP );

		    }
		    break;
	    case RGL_RGB5_A1_SCE:          // in_rgba = RGBA, out_rgba = RGBA
		    {
			    gcmFormat =  CELL_GCM_TEXTURE_A1R5G5B5;
			    remap = CELL_GCM_REMAP_MODE(
					    CELL_GCM_TEXTURE_REMAP_ORDER_XXXY,
					    CELL_GCM_TEXTURE_REMAP_FROM_A,
					    CELL_GCM_TEXTURE_REMAP_FROM_R,
					    CELL_GCM_TEXTURE_REMAP_FROM_G,
					    CELL_GCM_TEXTURE_REMAP_FROM_B,
					    CELL_GCM_TEXTURE_REMAP_REMAP,
					    CELL_GCM_TEXTURE_REMAP_REMAP,
					    CELL_GCM_TEXTURE_REMAP_REMAP,
					    CELL_GCM_TEXTURE_REMAP_REMAP );

		    }
		    break;
	    case RGL_RGB565_SCE:          // in_rgba = RGBA, out_rgba = RGBA
		    {
			    gcmFormat =  CELL_GCM_TEXTURE_R5G6B5;
			    remap = CELL_GCM_REMAP_MODE(
					    CELL_GCM_TEXTURE_REMAP_ORDER_XXXY,
					    CELL_GCM_TEXTURE_REMAP_FROM_A,
					    CELL_GCM_TEXTURE_REMAP_FROM_R,
					    CELL_GCM_TEXTURE_REMAP_FROM_G,
					    CELL_GCM_TEXTURE_REMAP_FROM_B,
					    CELL_GCM_TEXTURE_REMAP_ONE,
					    CELL_GCM_TEXTURE_REMAP_REMAP,
					    CELL_GCM_TEXTURE_REMAP_REMAP,
					    CELL_GCM_TEXTURE_REMAP_REMAP );

		    }
		    break;
	    default:
		    break;
    };

    return;
}

#ifdef __cplusplus
}
#endif

#endif
