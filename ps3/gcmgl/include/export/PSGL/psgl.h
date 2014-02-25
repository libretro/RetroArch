#ifndef _RGL_EXPORT_H
#define _RGL_EXPORT_H

#include <stdlib.h>
#include "export.h"
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <Cg/cg.h>
#include <Cg/cgGL.h>

#include <cell/cgb/cgb_struct.h>
#include <cell/resc.h>

#include <sdk_version.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PSGLdevice PSGLdevice;
typedef struct PSGLcontext PSGLcontext;

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

#define RGL_TV_STANDARD_NONE (PSGL_TV_STANDARD_NONE)
#define RGL_TV_STANDARD_NTSC_M 1
#define RGL_TV_STANDARD_NTSC_J 2
#define RGL_TV_STANDARD_PAL_M 3
#define RGL_TV_STANDARD_PAL_B 4
#define RGL_TV_STANDARD_PAL_D 5
#define RGL_TV_STANDARD_PAL_G 6
#define RGL_TV_STANDARD_PAL_H 7
#define RGL_TV_STANDARD_PAL_I 8
#define RGL_TV_STANDARD_PAL_N 9
#define RGL_TV_STANDARD_PAL_NC 10
#define RGL_TV_STANDARD_HD480I 11
#define RGL_TV_STANDARD_HD480P 12
#define RGL_TV_STANDARD_HD576I 13
#define RGL_TV_STANDARD_HD576P 14
#define RGL_TV_STANDARD_HD720P 15
#define RGL_TV_STANDARD_HD1080I 16
#define RGL_TV_STANDARD_HD1080P 17
#define RGL_TV_STANDARD_1280x720_ON_VESA_1280x768 128
#define RGL_TV_STANDARD_1280x720_ON_VESA_1280x1024 129
#define RGL_TV_STANDARD_1920x1080_ON_VESA_1920x1200 130

typedef enum PSGLbufferingMode
{
   PSGL_BUFFERING_MODE_SINGLE = 1,
   PSGL_BUFFERING_MODE_DOUBLE = 2,
   PSGL_BUFFERING_MODE_TRIPLE = 3,
} PSGLbufferingMode;

/* spoof as PSGL */
#define RGL_BUFFERING_MODE_SINGLE (PSGL_BUFFERING_MODE_SINGLE)
#define RGL_BUFFERING_MODE_DOUBLE (PSGL_BUFFERING_MODE_DOUBLE)
#define RGL_BUFFERING_MODE_TRIPLE (PSGL_BUFFERING_MODE_TRIPLE)

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

#define RGL_DEVICE_CONNECTOR_NONE (PSGL_DEVICE_CONNECTOR_NONE)
#define RGL_DEVICE_CONNECTOR_VGA 1
#define RGL_DEVICE_CONNECTOR_DVI 2
#define RGL_DEVICE_CONNECTOR_HDMI 3
#define RGL_DEVICE_CONNECTOR_COMPOSITE 4
#define RGL_DEVICE_CONNECTOR_SVIDEO 5
#define RGL_DEVICE_CONNECTOR_COMPONENT 6

typedef enum RescRatioMode
{
   RESC_RATIO_MODE_FULLSCREEN,
   RESC_RATIO_MODE_LETTERBOX,  // default
   RESC_RATIO_MODE_PANSCAN,
} RescRatioMode;

typedef enum RescPalTemporalMode
{
   RESC_PAL_TEMPORAL_MODE_50_NONE,  // default - no conversion
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

/* spoof as PSGL */

#define RGL_DEVICE_PARAMETERS_COLOR_FORMAT             0x0001
#define RGL_DEVICE_PARAMETERS_DEPTH_FORMAT             0x0002
#define RGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE       0x0004
#define RGL_DEVICE_PARAMETERS_TV_STANDARD              0x0008
#define RGL_DEVICE_PARAMETERS_CONNECTOR                0x0010
#define RGL_DEVICE_PARAMETERS_BUFFERING_MODE           0x0020
#define RGL_DEVICE_PARAMETERS_WIDTH_HEIGHT             0x0040
#define RGL_DEVICE_PARAMETERS_RESC_RENDER_WIDTH_HEIGHT 0x0080
#define RGL_DEVICE_PARAMETERS_RESC_RATIO_MODE          0x0100
#define RGL_DEVICE_PARAMETERS_RESC_PAL_TEMPORAL_MODE   0x0200
#define RGL_DEVICE_PARAMETERS_RESC_INTERLACE_MODE      0x0400
#define RGL_DEVICE_PARAMETERS_RESC_ADJUST_ASPECT_RATIO 0x0800

// mask for validation
#define PSGL_VALIDATE_NONE                         0x00000000 
#define PSGL_VALIDATE_FRAMEBUFFER                  0x00000001             
#define PSGL_VALIDATE_TEXTURES_USED                0x00000002 
#define PSGL_VALIDATE_VERTEX_PROGRAM               0x00000004 
#define PSGL_VALIDATE_VERTEX_CONSTANTS 			   0x00000008
#define PSGL_VALIDATE_VERTEX_TEXTURES_USED 		   0x00000010   
#define PSGL_VALIDATE_FFX_VERTEX_PROGRAM 		   0x00000020
#define PSGL_VALIDATE_FRAGMENT_PROGRAM 			   0x00000040
#define PSGL_VALIDATE_FFX_FRAGMENT_PROGRAM 		   0x00000080
#define PSGL_VALIDATE_FRAGMENT_SHARED_CONSTANTS    0x00000100
#define PSGL_VALIDATE_VIEWPORT 					   0x00000200
#define PSGL_VALIDATE_DEPTH_TEST 				   0x00000800
#define PSGL_VALIDATE_WRITE_MASK 				   0x00001000
#define PSGL_VALIDATE_STENCIL_TEST 				   0x00002000
#define PSGL_VALIDATE_STENCIL_OP_AND_MASK 		   0x00004000
#define PSGL_VALIDATE_SCISSOR_BOX 				   0x00008000
#define PSGL_VALIDATE_FACE_CULL 				   0x00010000
#define PSGL_VALIDATE_BLENDING 					   0x00020000
#define PSGL_VALIDATE_POINT_RASTER 				   0x00040000
#define PSGL_VALIDATE_LINE_RASTER 				   0x00080000
#define PSGL_VALIDATE_POLYGON_OFFSET 			   0x00100000
#define PSGL_VALIDATE_SHADE_MODEL 				   0x00200000
#define PSGL_VALIDATE_LOGIC_OP 					   0x00400000
#define PSGL_VALIDATE_MULTISAMPLING 			   0x00800000
#define PSGL_VALIDATE_POLYGON_MODE 				   0x01000000
#define PSGL_VALIDATE_PRIMITIVE_RESTART 		   0x02000000
#define PSGL_VALIDATE_CLIP_PLANES 				   0x04000000
#define PSGL_VALIDATE_SHADER_SRGB_REMAP 		   0x08000000
#define PSGL_VALIDATE_POINT_SPRITE 				   0x10000000
#define PSGL_VALIDATE_TWO_SIDE_COLOR 			   0x20000000
#define PSGL_VALIDATE_ALL 						   0x3FFFFFFF

/* spoof as PSGL */
#define RGL_VALIDATE_NONE                         0x00000000 
#define RGL_VALIDATE_FRAMEBUFFER                  0x00000001             
#define RGL_VALIDATE_TEXTURES_USED                0x00000002 
#define RGL_VALIDATE_VERTEX_PROGRAM               0x00000004 
#define RGL_VALIDATE_VERTEX_CONSTANTS 			   0x00000008
#define RGL_VALIDATE_VERTEX_TEXTURES_USED 		   0x00000010   
#define RGL_VALIDATE_FFX_VERTEX_PROGRAM 		   0x00000020
#define RGL_VALIDATE_FRAGMENT_PROGRAM 			   0x00000040
#define RGL_VALIDATE_FFX_FRAGMENT_PROGRAM 		   0x00000080
#define RGL_VALIDATE_FRAGMENT_SHARED_CONSTANTS    0x00000100
#define RGL_VALIDATE_VIEWPORT 					   0x00000200
#define RGL_VALIDATE_DEPTH_TEST 				   0x00000800
#define RGL_VALIDATE_WRITE_MASK 				   0x00001000
#define RGL_VALIDATE_STENCIL_TEST 				   0x00002000
#define RGL_VALIDATE_STENCIL_OP_AND_MASK 		   0x00004000
#define RGL_VALIDATE_SCISSOR_BOX 				   0x00008000
#define RGL_VALIDATE_FACE_CULL 				   0x00010000
#define RGL_VALIDATE_BLENDING 					   0x00020000
#define RGL_VALIDATE_POINT_RASTER 				   0x00040000
#define RGL_VALIDATE_LINE_RASTER 				   0x00080000
#define RGL_VALIDATE_POLYGON_OFFSET 			   0x00100000
#define RGL_VALIDATE_SHADE_MODEL 				   0x00200000
#define RGL_VALIDATE_LOGIC_OP 					   0x00400000
#define RGL_VALIDATE_MULTISAMPLING 			   0x00800000
#define RGL_VALIDATE_POLYGON_MODE 				   0x01000000
#define RGL_VALIDATE_PRIMITIVE_RESTART 		   0x02000000
#define RGL_VALIDATE_CLIP_PLANES 				   0x04000000
#define RGL_VALIDATE_SHADER_SRGB_REMAP 		   0x08000000
#define RGL_VALIDATE_POINT_SPRITE 				   0x10000000
#define RGL_VALIDATE_TWO_SIDE_COLOR 			   0x20000000
#define RGL_VALIDATE_ALL 						   0x3FFFFFFF

#define RGLdevice PSGLdevice
#define RGLdeviceParameters PSGLdeviceParameters
#define RGLcontext PSGLcontext

typedef struct
{
   GLuint enable;
   GLenum colorFormat;
   GLenum depthFormat;
   GLenum multisamplingMode;
   PSGLtvStandard TVStandard;
   PSGLdeviceConnector connector;
   PSGLbufferingMode bufferingMode;
   GLuint width;   // dimensions of display device (scanout buffer)
   GLuint height;

   // dimensions of render buffer. Only set explicitly if the render target buffer
   // needs to be different size than display scanout buffer (resolution scaling required).
   // These can only be set if PSGL_DEVICE_PARAMETERS_RESC_RENDER_WIDTH_HEIGHT is set in the "enable" mask,
   // otherwise, render buffer dimensions are set to device dimensions (width/height).
   GLuint renderWidth;
   GLuint renderHeight;

   RescRatioMode rescRatioMode;             // RESC aspect ratio rescaling mode: full screen, letterbox, or pan & scan
   RescPalTemporalMode rescPalTemporalMode; // RESC pal frame rate conversion mode: none, drop frame, interpolate
   RescInterlaceMode rescInterlaceMode;     // RESC interlace filter mode: normal bilinear or use the anti-flicker interlace filter

   // horizontal and vertical scaling to adjust for the difference in overscan rates for each SD/HD mode or TV
   GLfloat horizontalScale;
   GLfloat verticalScale;
} PSGLdeviceParameters;


#define	PSGL_INIT_MAX_SPUS				0x0001
#define	PSGL_INIT_INITIALIZE_SPUS			0x0002
#define	PSGL_INIT_PERSISTENT_MEMORY_SIZE	0x0004
#define	PSGL_INIT_TRANSIENT_MEMORY_SIZE	0x0008
#define	PSGL_INIT_ERROR_CONSOLE			0x0010
#define	PSGL_INIT_FIFO_SIZE				0x0020
#define	PSGL_INIT_HOST_MEMORY_SIZE			0x0040
#define PSGL_INIT_USE_PMQUERIES           0x0080

   /* spoof as PSGL */
#define	RGL_INIT_MAX_SPUS				0x0001
#define	RGL_INIT_INITIALIZE_SPUS			0x0002
#define	RGL_INIT_PERSISTENT_MEMORY_SIZE	0x0004
#define	RGL_INIT_TRANSIENT_MEMORY_SIZE	0x0008
#define	RGL_INIT_ERROR_CONSOLE			0x0010
#define	RGL_INIT_FIFO_SIZE				0x0020
#define	RGL_INIT_HOST_MEMORY_SIZE			0x0040
#define RGL_INIT_USE_PMQUERIES           0x0080

typedef struct PSGLinitOptions
{
   GLuint			enable;	// bitfield of options to set
   GLuint 			maxSPUs;
   GLboolean		initializeSPUs;
   GLuint			persistentMemorySize;
   GLuint			transientMemorySize;
   int				errorConsole;
   GLuint			fifoSize;
   GLuint			hostMemorySize;
} PSGLinitOptions;

#define RGLinitOptions PSGLinitOptions

typedef void*( *PSGLmallocFunc )( size_t LSize );		// expected to return 16-byte aligned
typedef void*( *PSGLmemalignFunc )( size_t align, size_t LSize );
typedef void*( *PSGLreallocFunc )( void* LBlock, size_t LSize );
typedef void( *PSGLfreeFunc )( void* LBlock );

extern PSGL_EXPORT void	psglInit (void *data);
extern PSGL_EXPORT void	psglExit (void);

PSGL_EXPORT PSGLdevice*	psglCreateDeviceAuto( GLenum colorFormat, GLenum depthFormat, GLenum multisamplingMode );
PSGL_EXPORT PSGLdevice*	psglCreateDeviceExtended( const void *data);
PSGL_EXPORT GLfloat psglGetDeviceAspectRatio( const PSGLdevice * device );
PSGL_EXPORT void psglGetDeviceDimensions( const PSGLdevice * device, GLuint *width, GLuint *height );
PSGL_EXPORT void psglGetRenderBufferDimensions( const PSGLdevice * device, GLuint *width, GLuint *height );
PSGL_EXPORT void psglDestroyDevice (void *data);

PSGL_EXPORT void psglMakeCurrent( PSGLcontext* context, PSGLdevice* device );
PSGL_EXPORT PSGLcontext* psglCreateContext (void);
PSGL_EXPORT void psglDestroyContext (void *data);
PSGL_EXPORT void psglResetCurrentContext (void);
PSGL_EXPORT PSGLcontext* psglGetCurrentContext (void);
PSGL_EXPORT PSGLdevice* psglGetCurrentDevice (void);
PSGL_EXPORT void psglSwap (void);

static inline PSGL_EXPORT void psglRescAdjustAspectRatio( const float horizontalScale, const float verticalScale )
{
   cellRescAdjustAspectRatio( horizontalScale, verticalScale );
}

/*  hw cursor error code */
#define PSGL_HW_CURSOR_OK					CELL_OK		
#define PSGL_HW_CURSOR_ERROR_FAILURE		CELL_GCM_ERROR_FAILURE 
#define PSGL_HW_CURSOR_ERROR_INVALID_VALUE	CELL_GCM_ERROR_INVALID_VALUE 

#ifdef __cplusplus
}
#endif

#endif /* RGL_EXPORT_H */
