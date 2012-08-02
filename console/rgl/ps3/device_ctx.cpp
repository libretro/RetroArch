#include <stdio.h>
#include "rgl.h"
#include "private.h"
#include <string.h>
#ifdef __PSL1GHT__
#include <rsx/resc.h>
#else
#include <sdk_version.h>
#include <cell/gcm.h>
#include <cell/resc.h>
#endif
#include "../../../ps3/sdk_defines.h"

#ifdef __PSL1GHT__
#include <sysutil/sysutil.h>
#else
#include <sysutil/sysutil_sysparam.h>
#include <sys/synchronization.h>
#endif

#include "../../../general.h"

#ifndef __PSL1GHT__
using namespace cell::Gcm;
#endif

#define MAX_TILED_REGIONS 15

#define TILED_BUFFER_ALIGNMENT 0x10000
#define TILED_BUFFER_HEIGHT_ALIGNMENT 64

#define FIFO_SIZE (65536)
#define _RGL_DMA_PUSH_BUFFER_PREFETCH_PADDING 0x1000
#define UTIL_LABEL_INDEX 253

extern void _RGLFifoGlSetRenderTarget( RGLRenderTargetEx const * const args );

typedef struct
{
   int id;
   GLuint offset;
   GLuint size;
   GLuint pitch;
   GLuint bank;
} jsTiledRegion;

typedef struct
{
   jsTiledRegion region[MAX_TILED_REGIONS];
} jsTiledMemoryManager;

PSGLdevice *_CurrentDevice = NULL;

static GLboolean _RGLDuringDestroyDevice = GL_FALSE;
static jsTiledMemoryManager _RGLTiledMemoryManager;
static RGLResource _RGLResource;
static sys_semaphore_t FlipSem;
static volatile uint32_t *labelAddress = NULL;
static const uint32_t WaitLabelIndex = 111;
extern GLuint nvFenceCounter;
RGLState _RGLState;

typedef struct
{
   int width;
   int height;
   unsigned char hwMode;
} VideoMode;

static const VideoMode sysutilModes[] =
{
   {720, 480, CELL_VIDEO_OUT_RESOLUTION_480},
   {720, 576, CELL_VIDEO_OUT_RESOLUTION_576},
   {1280, 720, CELL_VIDEO_OUT_RESOLUTION_720},
   {960, 1080, CELL_VIDEO_OUT_RESOLUTION_960x1080},
   {1280, 1080, CELL_VIDEO_OUT_RESOLUTION_1280x1080},
   {1440, 1080, CELL_VIDEO_OUT_RESOLUTION_1440x1080},
   {1600, 1080, CELL_VIDEO_OUT_RESOLUTION_1600x1080},
   {1920, 1080, CELL_VIDEO_OUT_RESOLUTION_1080},
};

static unsigned int validPitch[] =
{
   0x0200,
   0x0300,
   0x0400,
   0x0500,
   0x0600,
   0x0700,
   0x0800,
   0x0A00,
   0x0C00,
   0x0D00,
   0x0E00,
   0x1000,
   0x1400,
   0x1800,
   0x1A00,
   0x1C00,
   0x2000,
   0x2800,
   0x3000,
   0x3400,
   0x3800,
   0x4000,
   0x5000,
   0x6000,
   0x6800,
   0x7000,
   0x8000,
   0xA000,
   0xC000,
   0xD000,
   0xE000,
   0x10000,
};

static const unsigned int validPitchCount = sizeof( validPitch ) / sizeof( validPitch[0] );

static const int sysutilModeCount = sizeof( sysutilModes ) / sizeof( sysutilModes[0] );

static inline void _RGLPrintIt( unsigned int v )
{
    printf( "%02x %02x %02x %02x : ", ( v >> 24 )&0xff, ( v >> 16 )&0xff, ( v >> 8 )&0xff, v&0xff );

    for ( unsigned int mask = ( 0x1 << 31 ), i = 1; mask != 0; mask >>= 1, i++ )
        printf("%d%s", ( v & mask ) ? 1 : 0, ( i % 8 == 0 ) ? " " : "");
    printf( "\n" );
}

static inline void _RGLPrintFifoFromPut(unsigned int numWords) 
{
   for ( int i = -numWords; i <= -1; i++ )
      _RGLPrintIt((( uint32_t* )_RGLState.fifo.current )[i]);
}

static inline void _RGLPrintFifoFromGet(unsigned int numWords) 
{
   for ( int i = -numWords; i <= -1; i++)
      _RGLPrintIt((( uint32_t* )_RGLState.fifo.lastGetRead )[i]);
}

static GLuint _RGLFifoReadReference(RGLFifo *fifo)
{
   GLuint ref = *((volatile GLuint *) & fifo->dmaControl->Reference );
   fifo->lastHWReferenceRead = ref;
   return ref;
}

static GLboolean _RGLFifoReferenceInUse(RGLFifo *fifo, GLuint reference)
{
   if (!((fifo->lastHWReferenceRead - reference) & 0x80000000))
      return GL_FALSE;

   if ((fifo->lastSWReferenceFlushed - reference) & 0x80000000)
      _RGLFifoFlush( fifo );

   _RGLFifoReadReference( fifo );

   if (!((fifo->lastHWReferenceRead - reference) & 0x80000000))
      return GL_FALSE;

   return GL_TRUE;
}

static GLuint _RGLFifoPutReference(RGLFifo *fifo)
{
   fifo->lastSWReferenceWritten++;

   cellGcmSetReferenceCommandInline(&_RGLState.fifo, fifo->lastSWReferenceWritten);

   if ((fifo->lastSWReferenceWritten & 0x7fffffff) == 0)
      _RGLFifoFinish(fifo);

   return fifo->lastSWReferenceWritten;
}

void _RGLFifoFinish(RGLFifo *fifo)
{
   GLuint ref = _RGLFifoPutReference(fifo);

   _RGLFifoFlush( fifo );

   for (;;)
   {
      if (!_RGLFifoReferenceInUse(fifo, ref))
         break;
   }
}

static void _RGLFifoInit(RGLFifo *fifo, void *dmaControl, unsigned long dmaPushBufferOffset, uint32_t *dmaPushBuffer, GLuint dmaPushBufferSize)
{
   fifo->fifoBlockSize  = DEFAULT_FIFO_BLOCK_SIZE;
   fifo->begin          = ( uint32_t * ) dmaPushBuffer;
   fifo->end            = fifo->begin + ( fifo->fifoBlockSize / sizeof( uint32_t ) ) - 1;
   fifo->current        = fifo->begin;
   fifo->lastGetRead    = fifo->current;
   fifo->lastPutWritten = fifo->current;

   fifo->dmaPushBufferBegin       = dmaPushBuffer;
   fifo->dmaPushBufferEnd         = ( uint32_t * )(( size_t )dmaPushBuffer + dmaPushBufferSize ) - 1;
   fifo->dmaControl               = ( RGLControlDma* )dmaControl;
   fifo->dmaPushBufferOffset      = dmaPushBufferOffset;
   fifo->dmaPushBufferSizeInWords = dmaPushBufferSize / sizeof( uint32_t );

   fifo->lastHWReferenceRead    = 0;
   fifo->lastSWReferenceWritten = 0;
   fifo->lastSWReferenceFlushed = 0;

   gCellGcmCurrentContext = fifo;
   gCellGcmCurrentContext->callback = ( CellGcmContextCallback )_RGLOutOfSpaceCallback;

   if ( _RGLFifoReadReference( fifo ) != 0 )
   {
      cellGcmSetReferenceCommandInline ( &_RGLState.fifo, 0);
      _RGLFifoFlush( fifo );

      for ( ;; )
      {
         if ( _RGLFifoReadReference( fifo ) == 0 )
            break;
      }
   }
   fifo->dmaPushBufferGPU = dmaPushBuffer;
   fifo->spuid = 0;
}

static GLboolean _RGLInitFromRM( RGLResource *rmResource )
{
   RGLState *RGLSt = &_RGLState;
   RGLInterpolantState *s = &_RGLState.state.interpolant;
   RGLBlendState *blend = &_RGLState.state.blend;
   GLfloat ref;
   GLuint i, hwColor;

   ref = 0.0f;

   memset( RGLSt, 0, sizeof( *RGLSt ) );

   RGLSt->localAddress = rmResource->localAddress;
   RGLSt->hostMemoryBase = rmResource->hostMemoryBase;
   RGLSt->hostMemorySize = rmResource->hostMemorySize;

   RGLSt->semaphores = rmResource->semaphores;

   _RGLFifoInit( &RGLSt->fifo, rmResource->dmaControl, rmResource->dmaPushBufferOffset, ( uint32_t * )rmResource->dmaPushBuffer, rmResource->dmaPushBufferSize );

   _RGLFifoFinish( &RGLSt->fifo );

   RGL_CALC_COLOR_LE_ARGB8( &hwColor, RGL_CLAMPF_01(0.0f), RGL_CLAMPF_01(0.0f), RGL_CLAMPF_01(0.0f), RGL_CLAMPF_01(0.0f) );

   blend->alphaFunc = CELL_GCM_ALWAYS;
   blend->alphaRef = ref;

   ref = RGL_CLAMPF_01(ref);

   cellGcmSetAlphaFuncInline( &_RGLState.fifo, CELL_GCM_ALWAYS, RGL_QUICK_FLOAT2UINT( ref * 255.0f ));

   cellGcmSetBlendColorInline( &_RGLState.fifo, hwColor, hwColor);
   cellGcmSetBlendEquationInline( &_RGLState.fifo, CELL_GCM_FUNC_ADD, CELL_GCM_FUNC_ADD );
   cellGcmSetBlendFuncInline( &_RGLState.fifo, CELL_GCM_ONE, CELL_GCM_ZERO, CELL_GCM_ONE, CELL_GCM_ZERO );
   cellGcmSetClearColorInline( &_RGLState.fifo, hwColor);
   cellGcmSetAlphaTestEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetBlendEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetAlphaTestEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetBlendEnableMrtInline( &_RGLState.fifo, CELL_GCM_FALSE, CELL_GCM_FALSE, CELL_GCM_FALSE);
   cellGcmSetLogicOpEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetCullFaceEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetCullFaceEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetDepthBoundsTestEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetDepthTestEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetPolygonOffsetFillEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetPolygonOffsetLineEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetRestartIndexEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetFragmentProgramGammaEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetScissorInline( &_RGLState.fifo, 0, 0, 4095, 4095);
   cellGcmSetStencilTestEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetTwoSidedStencilTestEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetTwoSideLightEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetVertexAttribOutputMaskInline( &_RGLState.fifo, s->vertexProgramAttribMask & s->fragmentProgramAttribMask);

   cellGcmSetPointSpriteControlInline( &_RGLState.fifo, CELL_GCM_FALSE, 1, 0);
   cellGcmSetFrequencyDividerOperationInline( &_RGLState.fifo, 0);

   cellGcmSetRestartIndexInline( &_RGLState.fifo, 0);
   cellGcmSetShadeModeInline( &_RGLState.fifo, CELL_GCM_SMOOTH);

   for (i = 0; i < CELL_GCM_MAX_TEXIMAGE_COUNT; i++)
   {
      cellGcmSetTextureAddressInline( &_RGLState.fifo, i, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_NEVER, 0 );
      cellGcmSetTextureFilterInline( &_RGLState.fifo, i, 0, CELL_GCM_TEXTURE_NEAREST_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
      cellGcmSetTextureControlInline( &_RGLState.fifo, i, CELL_GCM_TRUE, 0, 12 << 8, CELL_GCM_TEXTURE_MAX_ANISO_1 );
   }

   _RGLFifoGlViewport( 0, 0, CELL_GCM_MAX_RT_DIMENSION, CELL_GCM_MAX_RT_DIMENSION, 0.0f, 1.0f );

   _RGLFifoFinish( &RGLSt->fifo );

   return GL_TRUE;
}

GLboolean _RGLInit(PSGLinitOptions* options, RGLResource *resource)
{
    if (!_RGLInitFromRM(resource))
    {
        RARCH_ERR("RGL GCM failed initialisation.\n");
        return GL_FALSE;
    }

    if ( gmmInit( resource->localAddress,
                  resource->localAddress,
                  resource->localSize,
                  resource->hostMemoryBase,
                  resource->hostMemoryBase + resource->hostMemoryReserved,
                  resource->hostMemorySize - resource->hostMemoryReserved ) == GMM_ERROR )
    {
        RARCH_ERR("Could not initialize GPU memory manager.\n");
        _RGLDestroy();
        return GL_FALSE;
    }

    _RGLState.semaphores->userSemaphores[SEMA_FENCE].val = nvFenceCounter;

    _RGLState.labelValue = 1; 

    return GL_TRUE;
}

void _RGLDestroy(void)
{
    RGLState *RGLSt = &_RGLState;
    memset( RGLSt, 0, sizeof( *RGLSt ) );
}

static inline int rescIsEnabled(PSGLdeviceParameters* params)
{
   return params->enable & ( PSGL_DEVICE_PARAMETERS_RESC_RENDER_WIDTH_HEIGHT |
   PSGL_DEVICE_PARAMETERS_RESC_RATIO_MODE | 
   PSGL_DEVICE_PARAMETERS_RESC_PAL_TEMPORAL_MODE |
   PSGL_DEVICE_PARAMETERS_RESC_INTERLACE_MODE |
   PSGL_DEVICE_PARAMETERS_RESC_ADJUST_ASPECT_RATIO );
}

static inline const VideoMode *findModeByResolution(int width, int height)
{
    for (int i = 0; i < sysutilModeCount; ++i)
    {
        const VideoMode *vm = sysutilModes + i;
        if (( vm->width == width ) && ( vm->height  == height ) ) return vm;
    }
    return NULL;
}

static const VideoMode *findModeByEnum( GLenum TVStandard )
{
    const VideoMode *vm = NULL;
    switch ( TVStandard )
    {
        case PSGL_TV_STANDARD_NTSC_M:
        case PSGL_TV_STANDARD_NTSC_J:
        case PSGL_TV_STANDARD_HD480P:
        case PSGL_TV_STANDARD_HD480I:
            vm = &(sysutilModes[0]);
            break;
        case PSGL_TV_STANDARD_PAL_M:
        case PSGL_TV_STANDARD_PAL_B:
        case PSGL_TV_STANDARD_PAL_D:
        case PSGL_TV_STANDARD_PAL_G:
        case PSGL_TV_STANDARD_PAL_H:
        case PSGL_TV_STANDARD_PAL_I:
        case PSGL_TV_STANDARD_PAL_N:
        case PSGL_TV_STANDARD_PAL_NC:
        case PSGL_TV_STANDARD_HD576I:
        case PSGL_TV_STANDARD_HD576P:
            vm = &(sysutilModes[1]);
            break;
        case PSGL_TV_STANDARD_HD720P:
        case PSGL_TV_STANDARD_1280x720_ON_VESA_1280x768:
        case PSGL_TV_STANDARD_1280x720_ON_VESA_1280x1024:
            vm = &(sysutilModes[2]);
            break;
        case PSGL_TV_STANDARD_HD1080I:
        case PSGL_TV_STANDARD_HD1080P:
        case PSGL_TV_STANDARD_1920x1080_ON_VESA_1920x1200:
            vm = &(sysutilModes[3]);
            break;
        default:
            vm = &(sysutilModes[2]);
            break;
    }

    return vm;
}

static VideoMode _sysutilDetectedVideoMode;

const VideoMode *_RGLDetectVideoMode (void)
{
    CellVideoOutState videoState;
    int ret = cellVideoOutGetState( CELL_VIDEO_OUT_PRIMARY, 0, &videoState );
    if ( ret < 0 )
    {
        RARCH_WARN("Couldn't read the video configuration, using a default 720p resolution.\n");
        videoState.displayMode.scanMode = CELL_VIDEO_OUT_SCAN_MODE_PROGRESSIVE;
        videoState.displayMode.resolutionId = CELL_VIDEO_OUT_RESOLUTION_720;
    }
    CellVideoOutResolution resolution;
    cellVideoOutGetResolution( videoState.displayMode.resolutionId, &resolution );

    _sysutilDetectedVideoMode.width = resolution.width;
    _sysutilDetectedVideoMode.height = resolution.height;
    _sysutilDetectedVideoMode.hwMode = videoState.displayMode.resolutionId;
    return &_sysutilDetectedVideoMode;
}

static void _RGLFlipCallbackFunction(const uint32_t head)
{
    int res = sys_semaphore_post(FlipSem,1);
    (void)res;
}

static void _RGLVblankCallbackFunction(const uint32_t head)
{   
    (void)head;
    int status = *labelAddress;

    switch(status)
    {
       case 2:
          if (cellGcmGetFlipStatus() == 0)
	  {
             cellGcmResetFlipStatus();
	     *labelAddress=1;
	  }
	  break;
       case 1:
          *labelAddress = 0;
	  break;
       default:
          break;
    }
}

static void _RGLRescVblankCallbackFunction(const uint32_t head)
{
   (void)head;
   int status = *labelAddress;
   switch(status)
   {
      case 2:
         if (cellRescGetFlipStatus() == 0)
	 {
            cellRescResetFlipStatus();
	    *labelAddress=1;
	 }
	 break;
      case 1:
         *labelAddress = 0;
	 break;
      default:
	 break;
   }
}

static unsigned int findValidPitch( unsigned int pitch )
{
    if ( pitch <= validPitch[0] ) return validPitch[0];
    else
    {
        for ( GLuint i = 0;i < validPitchCount - 1;++i )
            if (( pitch > validPitch[i] ) && ( pitch <= validPitch[i+1] ) ) return validPitch[i+1];

        return validPitch[validPitchCount-1];
    }
}

int32_t _RGLOutOfSpaceCallback( struct CellGcmContextData* fifoContext, uint32_t spaceInWords )
{
   uint32_t *nextbegin, *nextend, nextbeginoffset, nextendoffset;
   RGLFifo * fifo = &_RGLState.fifo;

   cellGcmFlushUnsafeInline((CellGcmContextData*)fifo);

   uint32_t * tmp = ( uint32_t * )(( char* )fifo->dmaPushBufferBegin - fifo->dmaPushBufferOffset + ( *(( volatile GLuint* ) & fifo->dmaControl->Get ) ) );
   if (( tmp >= fifo->begin ) && ( tmp <= fifo->end ) ) fifo->lastGetRead = tmp;

   if(fifo->end != fifo->dmaPushBufferEnd)
   {
      nextbegin = (uint32_t *)fifo->end + 1; 
      nextend = nextbegin + fifo->fifoBlockSize/sizeof(uint32_t) - 1;
   }
   else
   {
      nextbegin = (uint32_t *)fifo->dmaPushBufferBegin;
      nextend = nextbegin + (fifo->fifoBlockSize)/sizeof(uint32_t) - 1;
   }

   cellGcmAddressToOffset(nextbegin, &nextbeginoffset);
   cellGcmAddressToOffset(nextend, &nextendoffset);

   cellGcmSetJumpCommandUnsafeInline((CellGcmContextData*)fifo, nextbeginoffset);

   fifo->begin = nextbegin;
   fifo->current = nextbegin;
   fifo->end = nextend;

   const GLuint nopsAtBegin = 8;

   uint32_t get = fifo->dmaControl->Get;

   while(((get >= nextbeginoffset) && (get <= nextendoffset)) 
   || (get < fifo->dmaPushBufferOffset) || (get > fifo->dmaPushBufferOffset + 
   fifo->dmaPushBufferSizeInWords*sizeof(uint32_t))) 
   {
      get = fifo->dmaControl->Get;
   }

   for(GLuint i = 0; i < nopsAtBegin; i++)
   {
      fifo->current[0] = NOP;
      fifo->current++;
   }

   return CELL_OK;
};

void _RGLGraphicsHandler( const uint32_t head )
{
   RGLFifo *fifo = &_RGLState.fifo;

   uint32_t *tmp = ( uint32_t * )(( char* )fifo->dmaPushBufferBegin - fifo->dmaPushBufferOffset + ( *(( volatile GLuint* ) & fifo->dmaControl->Get ) ) );
   if (( tmp >= fifo->begin ) && ( tmp <= fifo->end ) ) fifo->lastGetRead = tmp;

   RARCH_ERR("Current PSGL FIFO info:\n" ); 
   RARCH_ERR("FIFO Begin %p End %p Current %p and Get %p \n", 
   _RGLState.fifo.begin, _RGLState.fifo.end, _RGLState.fifo.current,
   _RGLState.fifo.lastGetRead ); 

   RARCH_ERR("Last 10 words of the PSGL Fifo from the ppu put/current position \n" );  
   _RGLPrintFifoFromPut( 10 ); 

   RARCH_ERR("Last 10 words of the PSGL Fifo from the gpu get position \n" );  
   _RGLPrintFifoFromGet( 10 ); 
}

static int _RGLInitRM( RGLResource *gcmResource, unsigned int hostMemorySize, int inSysMem, unsigned int dmaPushBufferSize )
{
    memset(gcmResource, 0, sizeof(RGLResource));

    dmaPushBufferSize = _RGLPad(dmaPushBufferSize, HOST_BUFFER_ALIGNMENT);

    gcmResource->hostMemorySize = _RGLPad( FIFO_SIZE + hostMemorySize + dmaPushBufferSize + _RGL_DMA_PUSH_BUFFER_PREFETCH_PADDING  + (LM_MAX_TOTAL_QUERIES * sizeof( GLuint )), 1 << 20 );

    if ( gcmResource->hostMemorySize > 0 )
        gcmResource->hostMemoryBase = ( char * )memalign( 1 << 20, gcmResource->hostMemorySize  );

    if ( cellGcmInit(FIFO_SIZE, gcmResource->hostMemorySize, gcmResource->hostMemoryBase ) != 0 )
    {
        RARCH_ERR("RSXIF failed initialization.\n");
        return GL_FALSE;
    }
    
    cellGcmSetDebugOutputLevel( CELL_GCM_DEBUG_LEVEL2 );
    cellGcmSetGraphicsHandler( &_RGLGraphicsHandler );

    CellGcmConfig config;
    cellGcmGetConfiguration( &config );

    gcmResource->localAddress = ( char * )config.localAddress;
    gcmResource->localSize = config.localSize;
    gcmResource->MemoryClock = config.memoryFrequency;
    gcmResource->GraphicsClock = config.coreFrequency;

    gcmResource->semaphores = ( RGLSemaphoreMemory * )cellGcmGetLabelAddress( 0 );
    gcmResource->dmaControl = ( char* ) cellGcmGetControlRegister() - (( char * ) & (( RGLControlDma* )0 )->Put - ( char * )0 );

    cellGcmFinish(1);

    gcmResource->hostMemorySize -= dmaPushBufferSize + _RGL_DMA_PUSH_BUFFER_PREFETCH_PADDING;
    gcmResource->dmaPushBuffer = gcmResource->hostMemoryBase + gcmResource->hostMemorySize;
    gcmResource->dmaPushBufferOffset = ( char * )gcmResource->dmaPushBuffer - ( char * )gcmResource->hostMemoryBase;
    gcmResource->dmaPushBufferSize = dmaPushBufferSize;
    gcmResource->hostMemoryReserved = FIFO_SIZE;

    cellGcmSetJumpCommand(( char * )gcmResource->dmaPushBuffer - ( char * )gcmResource->hostMemoryBase );

    gCellGcmCurrentContext->callback = ( CellGcmContextCallback )_RGLOutOfSpaceCallback;

#ifdef LOG_VERBOSE
    RARCH_LOG("MClk: %f Mhz NVClk: %f Mhz.\n", ( float )gcmResource->MemoryClock / 1E6, ( float )gcmResource->GraphicsClock / 1E6 );
    RARCH_LOG("Video Memory: %i MB.\n", gcmResource->localSize / ( 1024*1024 ) );
    RARCH_LOG("Local address mapped at %p.\n", gcmResource->localAddress );
    RARCH_LOG("Push buffer at %p - %p (size = 0x%X), offset=0x%lx.\n",
    gcmResource->dmaPushBuffer, ( char* )gcmResource->dmaPushBuffer + gcmResource->dmaPushBufferSize, gcmResource->dmaPushBufferSize, gcmResource->dmaPushBufferOffset );
    RARCH_LOG("DMA control at %p.\n", gcmResource->dmaControl );
#endif
    
    return 1;
}

void _RGLDeviceInit( PSGLinitOptions* options )
{
    GLuint fifoSize = _RGL_FIFO_SIZE_DEFAULT;
    GLuint hostSize = _RGL_HOST_SIZE_DEFAULT;

    if ( options != NULL )
    {
        if ( options->enable & PSGL_INIT_FIFO_SIZE )
            fifoSize = options->fifoSize;
        if ( options->enable & PSGL_INIT_HOST_MEMORY_SIZE )
            hostSize = options->hostMemorySize;
    }

    if ( !_RGLInitRM( &_RGLResource, hostSize, 0, fifoSize ) )
    {
        RARCH_ERR("RM resource failed initialization.\n" );
        return;
    }

    bool retval = _RGLInit( options, &_RGLResource );
    (void)retval;
}

static void _RGLDestroyRM( RGLResource* gcmResource )
{
   if ( gcmResource->hostMemoryBase ) 
      free( gcmResource->hostMemoryBase );
	
    memset(( void* )gcmResource, 0, sizeof( RGLResource ) );

    return;
}

void _RGLDeviceExit()
{
    _RGLDestroy();
    _RGLDestroyRM( &_RGLResource );
}

static GLuint _RGLAllocCreateRegion(GLuint size, GLint tag, void* data )
{
   uint32_t id = gmmAlloc(1, size);

   if ( id != GMM_ERROR )
   {
      if ( _RGLTryResizeTileRegion( (GLuint)gmmIdToOffset(id), ((GmmBaseBlock *)id)->size, data ) )
         gmmSetTileAttrib( id, tag, data );
      else
      {
         gmmFree( id );
	 id = GMM_ERROR;
      }
   }

   return id; 
}

GLboolean _RGLAllocateColorSurface(
    GLuint width,
    GLuint height,
    GLuint bitsPerPixel,
    GLuint *id,
    GLuint *pitchAllocated,
    GLuint *bytesAllocated )
{
    jsTiledMemoryManager* mm = &_RGLTiledMemoryManager;

    const unsigned int pitch = width * bitsPerPixel / 8;
    const unsigned int tiledPitch = findValidPitch( pitch );
    if ( tiledPitch < pitch )
        *pitchAllocated = _RGLPad( pitch, tiledPitch );
    else
        *pitchAllocated = tiledPitch;

    GLuint padSize = TILED_BUFFER_ALIGNMENT;
    while (( padSize % ( tiledPitch*8 ) ) != 0 )
        padSize += TILED_BUFFER_ALIGNMENT;

    height = _RGLPad(height, TILED_BUFFER_HEIGHT_ALIGNMENT);
    *bytesAllocated = _RGLPad(( *pitchAllocated ) * height, padSize );

    const GLuint tag = *pitchAllocated | ( 0x0 );
   
    *id = gmmAllocExtendedTileBlock(*bytesAllocated, tag);

    if ( *id == GMM_ERROR )
    {
        for ( int i = 0; i < MAX_TILED_REGIONS; ++i )
        {
            if ( mm->region[i].size == 0 )
            {
                mm->region[i].id = i;
                mm->region[i].pitch = *pitchAllocated;
                mm->region[i].bank = 0x0;

                *id = _RGLAllocCreateRegion(*bytesAllocated, tag, &mm->region[i] );

                break;
            }
        }
    }

    if ( *id == GMM_ERROR )
    {
        *bytesAllocated = 0;
        *pitchAllocated = 0;
    }
    else
    {
        RARCH_LOG("Allocating GPU memory (tiled): %d bytes allocated at id 0x%08x.\n", *bytesAllocated, *id );
    }

    return *bytesAllocated > 0;
}

 PSGLdevice*	psglCreateDeviceAuto( GLenum colorFormat, GLenum depthFormat, GLenum multisamplingMode )
{
    PSGLdeviceParameters parameters;
    parameters.enable = PSGL_DEVICE_PARAMETERS_COLOR_FORMAT | PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT | PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE;
    parameters.colorFormat = colorFormat;
    parameters.depthFormat = GL_NONE;
    parameters.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;
    return psglCreateDeviceExtended( &parameters );
}

static void rescInit( const PSGLdeviceParameters* params, RGLDevice *gcmDevice )
{
   RARCH_WARN("RESC is enabled.\n");

   CellRescBufferMode dstBufferMode;
   if ( params->width == 720  && params->height == 480 )  dstBufferMode = CELL_RESC_720x480;
   else if ( params->width == 720  && params->height == 576 )  dstBufferMode = CELL_RESC_720x576;
   else if ( params->width == 1280 && params->height == 720 )  dstBufferMode = CELL_RESC_1280x720;
   else if ( params->width == 1920 && params->height == 1080 ) dstBufferMode = CELL_RESC_1920x1080;
   else
   {
      dstBufferMode = CELL_RESC_720x480;
      RARCH_ERR("Invalid display resolution for resolution conversion: %ux%u. Defaulting to 720x480...\n", params->width, params->height );
   }

   CellRescInitConfig conf;
   memset( &conf, 0, sizeof( CellRescInitConfig ) );
   conf.size            = sizeof( CellRescInitConfig );
   conf.resourcePolicy  = CELL_RESC_MINIMUM_GPU_LOAD | CELL_RESC_CONSTANT_VRAM;
   conf.supportModes    = CELL_RESC_720x480 | CELL_RESC_720x576 | CELL_RESC_1280x720 | CELL_RESC_1920x1080;
   conf.ratioMode       = ( params->rescRatioMode == RESC_RATIO_MODE_FULLSCREEN ) ? CELL_RESC_FULLSCREEN :
   ( params->rescRatioMode == RESC_RATIO_MODE_PANSCAN ) ? CELL_RESC_PANSCAN : CELL_RESC_LETTERBOX;
   conf.palTemporalMode = ( params->rescPalTemporalMode == RESC_PAL_TEMPORAL_MODE_60_DROP ) ? CELL_RESC_PAL_60_DROP :
   ( params->rescPalTemporalMode == RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE ) ? CELL_RESC_PAL_60_INTERPOLATE : 
   ( params->rescPalTemporalMode == RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE_30_DROP ) ? CELL_RESC_PAL_60_INTERPOLATE_30_DROP :
   ( params->rescPalTemporalMode == RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE_DROP_FLEXIBLE ) ? CELL_RESC_PAL_60_INTERPOLATE_DROP_FLEXIBLE : CELL_RESC_PAL_50;
   conf.interlaceMode   = ( params->rescInterlaceMode == RESC_INTERLACE_MODE_INTERLACE_FILTER ) ? CELL_RESC_INTERLACE_FILTER : CELL_RESC_NORMAL_BILINEAR;
   cellRescInit( &conf );

   GLuint size;
   GLuint colorBuffersPitch;
   uint32_t numColorBuffers = cellRescGetNumColorBuffers( dstBufferMode, ( CellRescPalTemporalMode )conf.palTemporalMode, 0 );

   _RGLAllocateColorSurface( params->width, params->height * numColorBuffers,
    4*8, &(gcmDevice->RescColorBuffersId), &colorBuffersPitch, &size );

   CellRescDsts dsts = { CELL_RESC_SURFACE_A8R8G8B8, colorBuffersPitch, 1 };
   cellRescSetDsts( dstBufferMode, &dsts );

   cellRescSetDisplayMode( dstBufferMode );

   int32_t colorBuffersSize, vertexArraySize, fragmentShaderSize;
   cellRescGetBufferSize( &colorBuffersSize, &vertexArraySize, &fragmentShaderSize );
   gcmDevice->RescVertexArrayId    = gmmAlloc(0, vertexArraySize);

   gcmDevice->RescFragmentShaderId = gmmAlloc(0, fragmentShaderSize);


   cellRescSetBufferAddress( gmmIdToAddress(gcmDevice->RescColorBuffersId),
		   gmmIdToAddress(gcmDevice->RescVertexArrayId),
		   gmmIdToAddress(gcmDevice->RescFragmentShaderId) );

   cellRescAdjustAspectRatio( params->horizontalScale, params->verticalScale );

   if ((params->enable & PSGL_DEVICE_PARAMETERS_RESC_INTERLACE_MODE) &&
		   (params->rescInterlaceMode == RESC_INTERLACE_MODE_INTERLACE_FILTER))
   {
      const unsigned int tableLength = 32;
      unsigned int tableSize = sizeof(uint16_t) * 4 * tableLength; 
      void *interlaceTable = gmmIdToAddress(gmmAlloc(0, tableSize));
      int32_t errorCode = cellRescCreateInterlaceTable(interlaceTable,params->renderHeight,CELL_RESC_ELEMENT_HALF,tableLength);
      (void)errorCode;
   }
}

static void _RGLSetDisplayMode( const VideoMode *vm, GLushort bitsPerPixel, GLuint pitch )
{
    CellVideoOutConfiguration videocfg;
    memset( &videocfg, 0, sizeof( videocfg ) );
    videocfg.resolutionId = vm->hwMode;
    videocfg.format = ( bitsPerPixel == 32 ) ? CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8 : CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_R16G16B16X16_FLOAT;
    videocfg.pitch = pitch;
    videocfg.aspect = CELL_VIDEO_OUT_ASPECT_AUTO;
    cellVideoOutConfigure( CELL_VIDEO_OUT_PRIMARY, &videocfg, NULL, 0 );
}

static int _RGLPlatformCreateDevice( PSGLdevice* device )
{
   RGLDevice *gcmDevice = ( RGLDevice * )device->platformDevice;
   PSGLdeviceParameters* params = &device->deviceParameters;
   jsTiledMemoryManager* mm = &_RGLTiledMemoryManager;

   _RGLDuringDestroyDevice = GL_FALSE;

   memset( mm->region, 0, sizeof( mm->region ) );
   for(int i = 0; i < MAX_TILED_REGIONS; ++i)
      cellGcmUnbindTile( i );


   const VideoMode *vm = NULL;
   if ( params->enable & PSGL_DEVICE_PARAMETERS_TV_STANDARD )
   {
      vm = findModeByEnum( params->TVStandard );

      if(!vm)
         return -1;

      params->width = vm->width;
      params->height = vm->height;
   }
   else if ( params->enable & PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT )
   {
      vm = findModeByResolution( params->width, params->height );

      if(!vm)
         return -1;

   }
   else
   {
      vm = _RGLDetectVideoMode();

      if(!vm)
         return -1;

      params->width = vm->width;
      params->height = vm->height;
   }

   if ( !(params->enable & PSGL_DEVICE_PARAMETERS_RESC_RENDER_WIDTH_HEIGHT) )
   {
      params->renderWidth = params->width;
      params->renderHeight = params->height;
   }

   if ( rescIsEnabled( params ) )
      rescInit( params, gcmDevice );

   gcmDevice->deviceType = 0;
   gcmDevice->TVStandard = params->TVStandard;

   gcmDevice->vsync = rescIsEnabled( params ) ? GL_TRUE : GL_FALSE;

   gcmDevice->ms = NULL;

   const GLuint width = params->renderWidth;
   const GLuint height = params->renderHeight;

   for ( int i = 0; i < params->bufferingMode; ++i )
   {
      gcmDevice->color[i].source = SURFACE_SOURCE_DEVICE;
      gcmDevice->color[i].width = width;
      gcmDevice->color[i].height = height;
      gcmDevice->color[i].bpp = 4;
      gcmDevice->color[i].format = RGL_ARGB8;
      gcmDevice->color[i].pool = SURFACE_POOL_LINEAR;

      GLuint size;
      _RGLAllocateColorSurface(width, height,
      gcmDevice->color[i].bpp*8, &gcmDevice->color[i].dataId,
      &gcmDevice->color[i].pitch, &size );
   }

   memset( &gcmDevice->rt, 0, sizeof( RGLRenderTargetEx ) );
   gcmDevice->rt.colorBufferCount = 1;
   gcmDevice->rt.yInverted = GL_TRUE;
   gcmDevice->rt.width = width;
   gcmDevice->rt.height = height;

   _RGLFifoGlViewport( 0, 0, width, height );

   GLuint hwColor;

   RGL_CALC_COLOR_LE_ARGB8( &hwColor, RGL_CLAMPF_01(0.0f), RGL_CLAMPF_01(0.0f), RGL_CLAMPF_01(0.0f), RGL_CLAMPF_01(0.0f) );

   cellGcmSetClearColorInline( &_RGLState.fifo, hwColor);

   gcmDevice->rt.colorFormat = RGL_ARGB8;

   for ( int i = 0; i < params->bufferingMode; ++i )
   {
      gcmDevice->rt.colorId[0] = gcmDevice->color[i].dataId;
      gcmDevice->rt.colorPitch[0] = gcmDevice->color[i].pitch;
      _RGLFifoGlSetRenderTarget( &gcmDevice->rt );
      cellGcmSetClearSurfaceInline ( &_RGLState.fifo, CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
   }

   gcmDevice->scanBuffer = 0;
   if ( params->bufferingMode == PSGL_BUFFERING_MODE_SINGLE )
      gcmDevice->drawBuffer = 0;
   else if ( params->bufferingMode == PSGL_BUFFERING_MODE_DOUBLE )
      gcmDevice->drawBuffer = 1;
   else if ( params->bufferingMode == PSGL_BUFFERING_MODE_TRIPLE )
      gcmDevice->drawBuffer = 2;

   sys_semaphore_attribute_t attr;
   sys_semaphore_attribute_initialize(attr);

   sys_semaphore_value_t initial_val = 0;
   sys_semaphore_value_t max_val = 1;
   switch (device->deviceParameters.bufferingMode)
   {
      case PSGL_BUFFERING_MODE_SINGLE:
         initial_val = 0;
	 max_val = 1;
	 break;
      case PSGL_BUFFERING_MODE_DOUBLE:
         initial_val = 1;
	 max_val = 2;
	 break;
      case PSGL_BUFFERING_MODE_TRIPLE:
         initial_val = 2;
	 max_val = 3;
	 break;
      default:
         break;
   }

   int res = sys_semaphore_create(&FlipSem, &attr, initial_val, max_val);
   (void)res;

   if(rescIsEnabled(params))
      cellRescSetFlipHandler(_RGLFlipCallbackFunction);
   else
      cellGcmSetFlipHandler(_RGLFlipCallbackFunction);

   labelAddress = (volatile uint32_t *)cellGcmGetLabelAddress(WaitLabelIndex);	
   *labelAddress = 0;

   if(rescIsEnabled(params))
	   cellRescSetVBlankHandler(_RGLRescVblankCallbackFunction);
   else
	   cellGcmSetVBlankHandler(_RGLVblankCallbackFunction);

   if (rescIsEnabled(params))
   {
      for (int i = 0; i < params->bufferingMode; ++i)
      {
         CellRescSrc rescSrc;
	 rescSrc.format = CELL_GCM_TEXTURE_A8R8G8B8 | CELL_GCM_TEXTURE_LN | CELL_GCM_TEXTURE_NR;
	 rescSrc.pitch = gcmDevice->color[i].pitch;
	 rescSrc.width = width;
	 rescSrc.height = height;
	 rescSrc.offset = gmmIdToOffset( gcmDevice->color[i].dataId );

	 if ( cellRescSetSrc( i, &rescSrc ) != CELL_OK )
	 {
            RARCH_ERR("Registering display buffer %d failed.\n", i );
	    return -1;
	 }
      }
   }
   else
   {
      _RGLSetDisplayMode(vm, gcmDevice->color[0].bpp*8, gcmDevice->color[0].pitch);

      cellGcmSetFlipMode(gcmDevice->vsync ? CELL_GCM_DISPLAY_VSYNC : CELL_GCM_DISPLAY_HSYNC);
      cellGcmSetInvalidateVertexCacheInline( &_RGLState.fifo);
      _RGLFifoFinish( &_RGLState.fifo );

      for (int i = 0; i < params->bufferingMode; ++i)
      {
         if (cellGcmSetDisplayBuffer( i, gmmIdToOffset( gcmDevice->color[i].dataId ), gcmDevice->color[i].pitch , width, height) != CELL_OK)
	 {
            RARCH_ERR("Registering display buffer %d failed.\n", i );
	    return -1;
	 }
      }
   }

   gcmDevice->swapFifoRef = _RGLFifoPutReference( &_RGLState.fifo );
   gcmDevice->swapFifoRef2 = gcmDevice->swapFifoRef;

   return 0;
}

PSGLdevice* psglCreateDeviceExtended(const PSGLdeviceParameters *parameters )
{
    PSGLdevice *device = (PSGLdevice *)malloc(sizeof(PSGLdevice) + sizeof(RGLDevice));

    if(!device)
    {
        _RGLSetError(GL_OUT_OF_MEMORY);
        return NULL;
    }
    memset(device, 0, sizeof(PSGLdevice) + sizeof(RGLDevice));

    PSGLdeviceParameters defaultParameters;

    defaultParameters.enable = 0;
    defaultParameters.colorFormat = GL_ARGB_SCE;
    defaultParameters.depthFormat = GL_NONE;
    defaultParameters.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;
    defaultParameters.TVStandard = PSGL_TV_STANDARD_NONE;
    defaultParameters.connector = PSGL_DEVICE_CONNECTOR_NONE;
    defaultParameters.bufferingMode = PSGL_BUFFERING_MODE_DOUBLE;
    defaultParameters.width = 0;
    defaultParameters.height = 0;
    defaultParameters.renderWidth = 0;
    defaultParameters.renderHeight = 0;
    defaultParameters.rescRatioMode = RESC_RATIO_MODE_LETTERBOX;
    defaultParameters.rescPalTemporalMode = RESC_PAL_TEMPORAL_MODE_50_NONE;
    defaultParameters.rescInterlaceMode = RESC_INTERLACE_MODE_NORMAL_BILINEAR;
    defaultParameters.horizontalScale = 1.0f;
    defaultParameters.verticalScale = 1.0f;

    memcpy(&device->deviceParameters, parameters, sizeof( PSGLdeviceParameters));

    if ((parameters->enable & PSGL_DEVICE_PARAMETERS_COLOR_FORMAT) == 0)
       device->deviceParameters.colorFormat = defaultParameters.colorFormat;

    if ((parameters->enable & PSGL_DEVICE_PARAMETERS_TV_STANDARD ) == 0)
       device->deviceParameters.TVStandard = defaultParameters.TVStandard;

    if ((parameters->enable & PSGL_DEVICE_PARAMETERS_CONNECTOR) == 0)
        device->deviceParameters.connector = defaultParameters.connector;

    if (( parameters->enable & PSGL_DEVICE_PARAMETERS_BUFFERING_MODE ) == 0 )
       device->deviceParameters.bufferingMode = defaultParameters.bufferingMode;

    if (( parameters->enable & PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT ) == 0 )
    {
       device->deviceParameters.width = defaultParameters.width;
       device->deviceParameters.height = defaultParameters.height;
    }

    if (( parameters->enable & PSGL_DEVICE_PARAMETERS_RESC_RENDER_WIDTH_HEIGHT ) == 0 )
    {
       device->deviceParameters.renderWidth = defaultParameters.renderWidth;
       device->deviceParameters.renderHeight = defaultParameters.renderHeight;
    }

    if (( parameters->enable & PSGL_DEVICE_PARAMETERS_RESC_RATIO_MODE ) == 0 )
       device->deviceParameters.rescRatioMode = defaultParameters.rescRatioMode;

    if (( parameters->enable & PSGL_DEVICE_PARAMETERS_RESC_PAL_TEMPORAL_MODE ) == 0 )
       device->deviceParameters.rescPalTemporalMode = defaultParameters.rescPalTemporalMode;

    if (( parameters->enable & PSGL_DEVICE_PARAMETERS_RESC_INTERLACE_MODE ) == 0 )
       device->deviceParameters.rescInterlaceMode = defaultParameters.rescInterlaceMode;

    if (( parameters->enable & PSGL_DEVICE_PARAMETERS_RESC_ADJUST_ASPECT_RATIO ) == 0 )
    {
       device->deviceParameters.horizontalScale = defaultParameters.horizontalScale;
       device->deviceParameters.verticalScale = defaultParameters.verticalScale;
    }

    device->rasterDriver = NULL;

    int result = _RGLPlatformCreateDevice( device );
    if ( result < 0 )
    {
       if(device != NULL)
          free( device );
       return NULL;
    }
    return device;
}

GLfloat psglGetDeviceAspectRatio(const PSGLdevice * device)
{
   CellVideoOutState videoState;
   cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &videoState);

   switch (videoState.displayMode.aspect)
   {
      case CELL_VIDEO_OUT_ASPECT_4_3:
         return 4.0f/3.0f;
      case CELL_VIDEO_OUT_ASPECT_16_9:
	 return 16.0f/9.0f;
   }

   return 16.0f/9.0f;
}

void psglGetDeviceDimensions(const PSGLdevice * device, GLuint *width, GLuint *height)
{
   *width = device->deviceParameters.width;
   *height = device->deviceParameters.height;
}

void psglDestroyDevice(PSGLdevice *device)
{
   if ( _CurrentDevice == device ) psglMakeCurrent( NULL, NULL );

   if ( device->rasterDriver )
      free( device->rasterDriver );

   RGLDevice *gcmDevice = ( RGLDevice * )device->platformDevice;
   PSGLdeviceParameters* params = &device->deviceParameters;

   cellGcmSetInvalidateVertexCacheInline( &_RGLState.fifo);
   _RGLFifoFinish( &_RGLState.fifo );

   if ( rescIsEnabled( params ) )
      cellRescSetFlipHandler(NULL);
   else
      cellGcmSetFlipHandler(NULL);

   if ( rescIsEnabled( &device->deviceParameters ) )
      cellRescSetVBlankHandler(NULL);
   else
      cellGcmSetVBlankHandler(NULL);

   int res = sys_semaphore_destroy(FlipSem);
   (void)res;

   if ( rescIsEnabled( params ) )
   {
      cellRescExit();
      gmmFree(gcmDevice->RescColorBuffersId);
      gmmFree(gcmDevice->RescVertexArrayId);
      gmmFree(gcmDevice->RescFragmentShaderId);
   }

   _RGLDuringDestroyDevice = GL_TRUE;
   for ( int i = 0; i < params->bufferingMode; ++i )
   {
      if (gcmDevice->color[i].pool != SURFACE_POOL_NONE)
         gmmFree( gcmDevice->color[i].dataId );
   }
   _RGLDuringDestroyDevice = GL_FALSE;

   if(device != NULL)
      free( device );
}

static void *_RGLPlatformRasterInit (void)
{
   RGLDriver *driver = (RGLDriver*)malloc(sizeof(RGLDriver));

   cellGcmSetInvalidateVertexCacheInline( &_RGLState.fifo);
   _RGLFifoFinish( &_RGLState.fifo );
   memset( driver, 0, sizeof( RGLDriver ) );
   driver->rt.yInverted = CELL_GCM_TRUE;
   driver->invalidateVertexCache = GL_FALSE;
   driver->flushBufferCount = 0;
   driver->colorBufferMask = 0x1;
   return driver;
}

void  psglMakeCurrent(PSGLcontext *context, PSGLdevice *device)
{
   if ( context && device )
   {
      _CurrentContext = context;
      _CurrentDevice = device;
      if ( !device->rasterDriver )
         device->rasterDriver = _RGLPlatformRasterInit();

      _RGLAttachContext( device, context );
   }
   else
   {
      _CurrentContext = NULL;
      _CurrentDevice = NULL;
   }
}

PSGLdevice *psglGetCurrentDevice(void)
{
   return _CurrentDevice;
}

extern void gmmUpdateFreeList (const uint8_t location);

GLAPI void psglSwap(void)
{
   PSGLcontext *LContext = _CurrentContext;
   PSGLdevice *device = _CurrentDevice;
   RGLFifo *fifo = &_RGLState.fifo;

   gmmUpdateFreeList(CELL_GCM_LOCATION_LOCAL);
   gmmUpdateFreeList(CELL_GCM_LOCATION_MAIN);

   RGLDevice *gcmDevice = ( RGLDevice * )device->platformDevice;

   const GLuint drawBuffer = gcmDevice->drawBuffer;

   GLboolean vsync = _CurrentContext->VSync;
   if(vsync != gcmDevice->vsync )
   {
      if (!rescIsEnabled( &device->deviceParameters))
      {
         cellGcmSetFlipMode( vsync ? CELL_GCM_DISPLAY_VSYNC : CELL_GCM_DISPLAY_HSYNC );
	 gcmDevice->vsync = vsync;
      }
   }

   if(device->deviceParameters.bufferingMode == PSGL_BUFFERING_MODE_TRIPLE )
   {
      if (rescIsEnabled( &device->deviceParameters))
         cellRescSetWaitFlip();
      else
         cellGcmSetWaitFlip();
   }

   if(rescIsEnabled( &device->deviceParameters))
   {
      int32_t res = cellRescSetConvertAndFlip((uint8_t)drawBuffer); 
      if ( res != CELL_OK )
      {
         RARCH_WARN("RESC cellRescSetConvertAndFlip returned error code %d.\n", res);

	 if(_CurrentContext)
            _CurrentContext->needValidate |= PSGL_VALIDATE_FRAMEBUFFER;

	 return;
      }
   }
   else
      cellGcmSetFlip(( uint8_t ) drawBuffer );

   if(device->deviceParameters.bufferingMode != PSGL_BUFFERING_MODE_TRIPLE )
   {
      if (rescIsEnabled( &device->deviceParameters))
         cellRescSetWaitFlip();
      else
         cellGcmSetWaitFlip();
   }

   cellGcmSetPolySmoothEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetLineStippleEnableInline( &_RGLState.fifo, CELL_GCM_FALSE );
   cellGcmSetPolygonStippleEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
   cellGcmSetDepthBoundsTestEnable( &_RGLState.fifo, CELL_GCM_FALSE);

   LContext->needValidate = PSGL_VALIDATE_ALL;

   for(int unit = 0; unit < MAX_TEXTURE_UNITS; unit++)
      LContext->TextureCoordsUnits[unit].TextureMatrixStack.dirty = GL_TRUE;

   LContext->ModelViewMatrixStack.dirty = GL_TRUE;
   LContext->ProjectionMatrixStack.dirty = GL_TRUE;
   LContext->attribs->DirtyMask = (1 << MAX_VERTEX_ATTRIBS) - 1;

   cellGcmSetInvalidateVertexCacheInline( &_RGLState.fifo);

   _RGLFifoFlush(fifo);

   while(sys_semaphore_wait(FlipSem, 1000) != CELL_OK);

   cellGcmSetInvalidateVertexCacheInline(&_RGLState.fifo);
   _RGLFifoFlush(fifo);

   if (device->deviceParameters.bufferingMode == PSGL_BUFFERING_MODE_DOUBLE)
   {
      gcmDevice->drawBuffer = gcmDevice->scanBuffer;
      gcmDevice->scanBuffer = drawBuffer;

      gcmDevice->rt.colorId[0] = gcmDevice->color[gcmDevice->drawBuffer].dataId;
      gcmDevice->rt.colorPitch[0] = gcmDevice->color[gcmDevice->drawBuffer].pitch;
   }
   else if(device->deviceParameters.bufferingMode == PSGL_BUFFERING_MODE_TRIPLE)
   {
      gcmDevice->drawBuffer = gcmDevice->scanBuffer;
      if (gcmDevice->scanBuffer == 2)
         gcmDevice->scanBuffer = 0;
      else
         gcmDevice->scanBuffer++;

      gcmDevice->rt.colorId[0] = gcmDevice->color[gcmDevice->drawBuffer].dataId;
      gcmDevice->rt.colorPitch[0] = gcmDevice->color[gcmDevice->drawBuffer].pitch;
   }
}

static inline void _RGLUtilWaitForIdle(void)
{
   cellGcmSetWriteBackEndLabelInline( &_RGLState.fifo, UTIL_LABEL_INDEX, _RGLState.labelValue);
   cellGcmSetWaitLabelInline( &_RGLState.fifo, UTIL_LABEL_INDEX, _RGLState.labelValue);

   _RGLState.labelValue++; 

   cellGcmSetWriteBackEndLabelInline( &_RGLState.fifo, UTIL_LABEL_INDEX, _RGLState.labelValue);
   cellGcmFlush();

   while( *(cellGcmGetLabelAddress(UTIL_LABEL_INDEX)) != _RGLState.labelValue);

   _RGLState.labelValue++;
}

GLboolean _RGLTryResizeTileRegion(GLuint address, GLuint size, void* data)
{
   jsTiledRegion* region = (jsTiledRegion*)data;

   if (size == 0)
   {
      region->offset = 0;
      region->size = 0;
      region->pitch = 0;

      if (!_RGLDuringDestroyDevice) 
      {
         _RGLUtilWaitForIdle(); 
	 cellGcmUnbindTile( region->id );
	 _RGLFifoFinish(&_RGLState.fifo);
      }
      return GL_TRUE;
   }
   region->offset = address;
   region->size = size;

   _RGLUtilWaitForIdle(); 

   cellGcmSetTileInfo(region->id, CELL_GCM_LOCATION_LOCAL,
   region->offset, region->size, region->pitch, CELL_GCM_COMPMODE_DISABLED, 0,
   region->bank );

   cellGcmBindTile( region->id ); 

   _RGLFifoFinish( &_RGLState.fifo );

   return GL_TRUE;
}

void _RGLGetTileRegionInfo(void* data, GLuint *address, GLuint *size)
{
   jsTiledRegion* region = ( jsTiledRegion* )data;

   *address = region->offset;
   *size = region->size;
}
