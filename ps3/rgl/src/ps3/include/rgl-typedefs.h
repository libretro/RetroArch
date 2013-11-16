#include "../../../include/export/RGL/rgl.h"
#include "../../../include/RGL/Types.h"

using namespace cell::Gcm;

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
   GLuint    dwDotClock;      // In 10K Hertz
   GLushort  wHSyncPolarity;
   GLushort  wVSyncPolarity;
} MODESTRUC;

// descriptor for 2D data
typedef struct
{
   GLenum source;	// device, texture, renderbuffer
   GLuint width, height;
   GLuint bpp;		// bytes per pixel, derived from format
   GLuint pitch;	// 0 if swizzled

   rglGcmEnum format;	// e.g. RGLGCM_ARGB8

   GLenum pool;	// type of memory

   GLuint dataId;		// id to get address and offset
   GLuint dataIdOffset;
} rglGcmSurface;

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

typedef struct
{
   // TODO: get rid of this member
   rglGcmRenderTargetEx rt;
   // framebuffers
   rglGcmSurface color[3];
   // double/triple buffering management
   GLuint drawBuffer;	// 0, 1, or 2
   GLuint scanBuffer;	// 0, 1, or 2
   // resc buffers (maintain pointers for freeing in rglPlatformDestroyDevice)
   GLuint RescColorBuffersId;
   GLuint RescVertexArrayId;
   GLuint RescFragmentShaderId;
   const MODESTRUC *ms;
   GLboolean vsync;
   GLboolean skipFirstVsync;
   GLenum deviceType;
   GLenum TVStandard;
   GLenum TVFormat;
   GLuint swapFifoRef;
   GLuint swapFifoRef2; // Added for supporting Triple buffering [RSTENSON]
   GLboolean setOffset;
   GLboolean signal;
   GLuint semaValue;
   unsigned int syncMethod;
} rglGcmDevice;

typedef struct rglGcmDriver_
{
   rglGcmRenderTargetEx rt;
   GLboolean rtValid;
   GLboolean invalidateVertexCache;
   int xSuperSampling; // supersampling factor in X
   int ySuperSampling; // supersampling factor in Y

   GLuint flushBufferCount;	// # of mapped buffer objects in bounce buffer

   GLuint fpLoadProgramId; // address of the currently bound fragment program
   GLuint fpLoadProgramOffset;

   GLuint sharedFPConstantsId;
   char *sharedVPConstants;
} rglGcmDriver;

struct rglPlatformFramebuffer: public rglFramebuffer
{
   rglGcmRenderTargetEx rt;
   GLboolean complete;
   rglPlatformFramebuffer(): rglFramebuffer()
   {
      memset( &rt, 0, sizeof( rt ) );
   };
   virtual ~rglPlatformFramebuffer() {};
   void validate (void *data);
};

typedef struct
{
   GLuint SET_TEXTURE_CONTROL3; // pitch and depth
   GLuint SET_TEXTURE_OFFSET; // gpu addr (from dma ctx)
   GLuint SET_TEXTURE_FORMAT; // which dma ctx, [123]D, border source, mem layout, mip levels
   GLuint SET_TEXTURE_ADDRESS; // wrap, signed and unsigned remap control, gamma, zfunc
   GLuint SET_TEXTURE_CONTROL0; // enable, lod clamp, aniso, image field, alpha kill, colorkey
   GLuint SET_TEXTURE_CONTROL1; // remap and crossbar setup.
   GLuint SET_TEXTURE_FILTER; // lod bias, convol filter, min/mag filter, component signedness
   GLuint SET_TEXTURE_IMAGE_RECT; // texture width/height
} rglGcmTextureMethods;

// Gcm Specific function parameter mappings.
// for cellGcmSetControl
struct rglGcmTextureControl0
{
   GLuint minLOD;
   GLuint maxLOD;
   GLuint maxAniso;
};

// for cellGcmSetAddress
struct rglGcmTextureAddress
{
   GLuint wrapS;
   GLuint wrapT;
   GLuint wrapR;
   GLuint unsignedRemap;
   GLuint zfunc;
   GLuint gamma;
};

// for cellGcmSetTextureFilter
struct rglGcmTextureFilter
{
   GLuint 	min;
   GLuint 	mag;
   GLuint 	conv;
   GLint  bias;
};

// Structure to contain Gcm Function Parameters for setting later.
// Control1 and Control3 will be set by cellGcmSetTexture
struct rglGcmTextureMethodParams
{
   rglGcmTextureControl0 control0;
   rglGcmTextureAddress  address;
   rglGcmTextureFilter   filter;
   GLuint borderColor; // texture border color
};

typedef struct
{
   // These are enough to describe the GPU format
   GLuint 		faces;
   GLuint 		levels;
   GLuint 		baseWidth;
   GLuint 		baseHeight;
   GLuint 		baseDepth;
   rglGcmEnum 	internalFormat;
   GLuint 		pixelBits;
   GLuint 		pitch;
} rglGcmTextureLayout;

// GCM texture data structure
typedef struct
{
   GLenum pool;
   rglGcmTextureMethods 		methods; // [RSTENSON] soon to be legacy
   rglGcmTextureMethodParams 	gcmMethods;
   CellGcmTexture  		gcmTexture;
   GLuint gpuAddressId;
   GLuint gpuAddressIdOffset;
   GLuint gpuSize;
   rglGcmTextureLayout gpuLayout;
} rglGcmTexture;

/* pio flow control data structure */
typedef volatile struct
{
   GLuint Ignored00[0x010];
   GLuint Put;                     /* put offset, write only           0040-0043*/
   GLuint Get;                     /* get offset, read only            0044-0047*/
   GLuint Reference;               /* reference value, read only       0048-004b*/
   GLuint Ignored01[0x1];
   GLuint SetReference;            /* set reference value              0050-0053*/
   GLuint TopLevelGet;             /* top level get offset, read only  0054-0057*/
   GLuint Ignored02[0x2];
   GLuint SetContextDmaSemaphore;  /* set sema ctxdma, write only      0060-0063*/
   GLuint SetSemaphoreOffset;      /* set sema offset, write only      0064-0067*/
   GLuint SetSemaphoreAcquire;     /* set sema acquire, write only     0068-006b*/
   GLuint SetSemaphoreRelease;     /* set sema release, write only     006c-006f*/
   GLuint Ignored03[0x7e4];
} rglGcmControlDma;

// _only_ update lastGetRead if the Get is in our pushbuffer

#define fifoUpdateGetLastRead(fifo) \
   uint32_t* tmp = (uint32_t*)(( char* )fifo->dmaPushBufferBegin - fifo->dmaPushBufferOffset + ( *(( volatile GLuint* )&fifo->dmaControl->Get))); \
   if ((tmp >= fifo->ctx.begin) && (tmp <= fifo->ctx.end)) \
      fifo->lastGetRead = tmp;

// all fifo related data is kept here
struct rglGcmFifo
{
   CellGcmContextData ctx;
   // dmaControl for allocated channel
   rglGcmControlDma *dmaControl;

   // for storing the start of the push buffer
   // begin will be moving around
   uint32_t *dmaPushBufferBegin;
   uint32_t *dmaPushBufferEnd;

   // Fifo block size
   GLuint  fifoBlockSize;

   // pushbuffer location etc.
   // members around here were moved up
   unsigned long dmaPushBufferOffset;
   GLuint dmaPushBufferSizeInWords;

   // the last Put value we wrote
   uint32_t *lastPutWritten;

   // the last Get value we know of
   uint32_t *lastGetRead;

   // cached value of last reference write
   GLuint lastSWReferenceWritten;

   // cached value of lastSWReferenceWritten at most
   // recent fifo flush
   GLuint lastSWReferenceFlushed;

   // cached value of last reference read
   GLuint lastHWReferenceRead;
   uint32_t *dmaPushBufferGPU;
   int spuid;
};

// 16 byte aligned semaphores
struct  rglGcmSemaphore
{
   GLuint val;
   GLuint pad0;
   GLuint pad1;
   GLuint pad2;
};

// semaphore storage
struct rglGcmSemaphoreMemory
{
   rglGcmSemaphore userSemaphores[RGLGCM_MAX_USER_SEMAPHORES];
};

struct rglGcmResource
{
   char *localAddress;
   GLuint localSize;
   GLuint MemoryClock;
   GLuint GraphicsClock;

   unsigned long long ioifMappings[32];

   char  * linearMemory;
   unsigned int persistentMemorySize;

   // host memory window the gpu can access
   char *  hostMemoryBase;
   GLuint  hostMemorySize;

   // offset of dmaPushBuffer relative to its DMA CONTEXT
   unsigned long dmaPushBufferOffset;
   char *  dmaPushBuffer;
   GLuint  dmaPushBufferSize;
   void*   dmaControl;

   // semaphores
   rglGcmSemaphoreMemory    *semaphores;
};

typedef struct
{
   GLuint fence;
}
rglGcmFenceObject;

typedef struct
{
   GLint sema;	// NV semaphore index
}
rglGcmEventObject;


typedef struct
{
   GLenum pool;		// LINEAR, SYSTEM, or NONE
   unsigned int bufferId;		// allocated Id
   unsigned int bufferSize;
   unsigned int pitch;

   GLuint mapCount;	// map reference count
   GLenum mapAccess;	// READ_ONLY, WRITE_ONLY, or READ_WRITE
}
rglGcmBufferObject;

typedef struct rglGcmShader_
{
   GLuint loadAddressId;
   CgBinaryProgram __attribute__(( aligned( 16 ) ) ) program;
} rglGcmShader;

// the current rendering surface/target
typedef struct rglGcmRenderTarget rglGcmRenderTarget;

struct rglGcmRenderTarget
{
   GLuint  colorFormat;
   GLuint  colorBufferCount;

   // (0,0) is in the lower left
   GLuint  yInverted;

   // gcm render target structure [RSTENSON]
   CellGcmSurface  gcmRenderTarget;
};

// cached state: texture
typedef struct rglGcmTextureState rglGcmTextureState;

struct rglGcmTextureState
{
   // unforunately to many pieces of state have been put into single
   // 32bit registers -- so we need to cache some of them...
   GLuint hwTexAddress;
   GLuint hwTexFilter;
   GLuint hwTexControl0;
   //GLuint hwTexCoordCtrl;

};

// cached state: viewport
typedef struct rglGcmViewportState rglGcmViewportState;
struct rglGcmViewportState
{
   // user values given as input to glViewport
   GLint x, y, w, h;
   // from glViewport
   GLfloat xScale, xCenter;
   GLfloat yScale, yCenter;
};

// cached state: blend
typedef struct rglGcmBlendState rglGcmBlendState;
struct rglGcmBlendState
{
   // current blend color
   GLfloat r, g, b, a;

   // alpha blend reference
   GLuint alphaFunc;
   GLfloat alphaRef;
};

typedef struct rglGcmInterpolantState rglGcmInterpolantState;
struct rglGcmInterpolantState
{
   // mask of inputs used by programs
   //  Uses bits from SET_VERTEX_ATTRIB_OUTPUT_MASK.
   GLuint vertexProgramAttribMask;
   GLuint fragmentProgramAttribMask;
};

// cached state (because no dedecated method exist)
typedef struct rglGcmCachedState rglGcmCachedState;
struct rglGcmCachedState
{
   // our hw<->ogl mapping is ...let's say strange...
   //rglGcmTextureState tex[RGLGCM_MAX_TEXIMAGE_COUNT];
   //[RSTENSON] Removing this above.  Texturing is all GCM now.

   // we need to track blending color, too
   rglGcmBlendState blend;

   // need to cache viewport values, because of yInverted
   rglGcmViewportState viewport;

   // all interpolants are enabled/disabled with a single mask
   rglGcmInterpolantState interpolant;
};

// ** the master instance representing a channel/context **
struct rglGcmState
{
   char *localAddress;

   // host memory window the gpu can access
   void 						*hostMemoryBase;
   GLuint 						hostMemorySize;

   // semaphores
   rglGcmSemaphoreMemory        *semaphores;

   // -- context state --

   // fifo
   rglGcmFifo               fifo;

   // rendering target
   rglGcmRenderTarget       renderTarget;

   // state
   rglGcmCachedState        state;

   // Cell Gcm Config
   CellGcmConfig config;

   // to use as the back end label value when syncing before cellGcmSetTile, SetZCull, and SetInvalidateTile. 
   GLuint			labelValue; 
};

extern rglGcmState rglGcmState_i;
