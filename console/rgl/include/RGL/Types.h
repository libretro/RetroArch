#ifndef	_RGL_TYPES_H
#define	_RGL_TYPES_H

#include <stdlib.h>
#include <float.h>

#include "../export/RGL/rgl.h"
#include "Base.h"

#include <Cg/cg.h>

struct rglFramebufferAttachment
{
   GLenum type;	// renderbuffer or texture
   GLuint name;

   // only valid for texture attachment
   GLenum textureTarget;
   rglFramebufferAttachment(): type( GL_NONE ), name( 0 ), textureTarget( GL_NONE )
   {};
};

#define RGL_MAX_COLOR_ATTACHMENTS 4

struct rglFramebuffer
{
   rglFramebufferAttachment color[RGL_MAX_COLOR_ATTACHMENTS];
   GLboolean needValidate;
   rglFramebuffer(): needValidate( GL_TRUE )
   {};
   virtual ~rglFramebuffer()
   {};
};


#ifdef __cplusplus
extern "C" {
#endif

#define RGLBIT_GET(f,n)			((f) & (1<<(n)))
#define RGLBIT_TRUE(f,n)			((f) |= (1<<(n)))
#define RGLBIT_FALSE(f,n)		((f) &= ~(1<<(n)))
#define RGLBIT_ASSIGN(f,n,val)	do { if(val) RGLBIT_TRUE(f,n); else RGLBIT_FALSE(f,n); } while(0)

#define ALIGN16 __attribute__((aligned (16)))
#define _RGL_RESTRICT __restrict

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
   }
   rglImage;

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
      GLuint baseLevel;
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
      GLuint		faceCount;
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
#define RGL_MAX_TEXTURE_IMAGE_UNITS	16
#define RGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS	4

#define RGL_MAX_VERTEX_ATTRIBS	16

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
   } ALIGN16 rglAttribute;

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
   } ALIGN16 rglAttributeState;

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
#define RGL_CONTEXT_GREEN_MASK				0x02
#define RGL_CONTEXT_BLUE_MASK				0x04
#define RGL_CONTEXT_ALPHA_MASK				0x08
#define RGL_CONTEXT_DEPTH_MASK				0x10
#define RGL_CONTEXT_COLOR_MASK				0x0F


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

#define rglELEMENTS_IN_MATRIX	16	// 4x4

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

   // There are 6 clock domains, each with a maximum of 4 experiments, plus 4 elapsed exp.
#define RGL_MAX_DPM_QUERIES       (4 * 6 + 4)

   struct RGLcontext
   {
      GLenum			error;

      rglViewPort		ViewPort;
      GLclampf		DepthNear;
      GLclampf		DepthFar;

      GLenum			PerspectiveCorrectHint;

      rglAttributeState defaultAttribs0;	// a default rglAttributeState, for bind = 0
      rglAttributeState *attribs;			// ptr to current rglAttributeState

      // Frame buffer-related fields
      //
      GLenum			DrawBuffer, ReadBuffer;

      GLboolean		ShaderSRGBRemap;

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

      // synchronization objects
      rglTexNameSpace	fenceObjectNameSpace;

      // framebuffer objects
      GLuint			framebuffer;	// GL_FRAMEBUFFER_OES binding
      rglTexNameSpace	framebufferNameSpace;

      GLboolean		VertexProgram;
      struct _CGprogram*	BoundVertexProgram;

      GLboolean		FragmentProgram;
      struct _CGprogram*	BoundFragmentProgram;
      unsigned int	LastFPConstantModification;

      GLboolean		AllowTXPDemotion;
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


#if defined(MSVC)
#pragma warning ( pop )
#endif

#ifdef __cplusplus
}
#endif

#endif
