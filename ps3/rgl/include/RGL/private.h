#ifndef _RGL_PRIVATE_H
#define _RGL_PRIVATE_H

#include "../export/RGL/rgl.h"
#include "Types.h"
#include "Utils.h"

#ifndef OS_VERSION_NUMERIC
#define OS_VERSION_NUMERIC 0x160
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern RGL_EXPORT RGLcontext*	_CurrentContext;
extern RGLdevice*	_CurrentDevice;
extern RGL_EXPORT char*    rglVersion;

// only for internal purpose
#define GL_UNSIGNED_BYTE_4_4 0x4A00
#define GL_UNSIGNED_BYTE_4_4_REV 0x4A01
#define GL_UNSIGNED_BYTE_6_2 0x4A02
#define GL_UNSIGNED_BYTE_2_6_REV 0x4A03
#define GL_UNSIGNED_SHORT_12_4 0x4A04
#define GL_UNSIGNED_SHORT_4_12_REV 0x4A05
#define GL_UNSIGNED_BYTE_2_2_2_2 0x4A06
#define GL_UNSIGNED_BYTE_2_2_2_2_REV 0x4A07

#define GL_FLOAT_RGBA32 0x888B

typedef void( * RGLcontextHookFunction )( RGLcontext *context );
extern RGL_EXPORT RGLcontextHookFunction rglContextCreateHook;
extern RGL_EXPORT RGLcontextHookFunction rglContextDestroyHook;

extern RGLcontext*	rglContextCreate();
extern void		rglContextFree( RGLcontext* LContext );
extern void		rglSetError( GLenum error );
void rglDetachContext( RGLdevice *device, RGLcontext* context );
void rglSetFlipHandler(void (*handler)(const GLuint head), RGLdevice *device);
void rglSetVBlankHandler(void (*handler)(const GLuint head), RGLdevice *device);

//----------------------------------------
// Texture.c
//----------------------------------------
extern int	rglTextureInit( RGLcontext* context, GLuint name );
extern void	rglTextureDelete( RGLcontext* context, GLuint name );
extern GLboolean rglTextureHasValidLevels( const rglTexture *texture, int levels, int width, int height, int depth, GLenum format, GLenum type, GLenum internalFormat );
extern rglTexture *rglGetCurrentTexture (const void *data, GLenum target);
void rglReallocateImages (void *data, GLint level, GLsizei dimension);

static inline rglTexture* rglGetTextureSafe (RGLcontext *LContext, GLuint name)
{
   return rglTexNameSpaceIsName( &LContext->textureNameSpace, name ) ? ( rglTexture* )LContext->textureNameSpace.data[name] : NULL;
}

static inline rglFramebuffer *rglGetFramebuffer( RGLcontext *LContext, GLuint name );

static inline void rglTextureTouchFBOs (void *data)
{
   rglTexture *texture = (rglTexture*)data;
   RGLcontext *LContext = _CurrentContext;

   if (!LContext )
      return; // may be called in psglDestroyContext

   // check if bound to any framebuffer
   GLuint fbCount = texture->framebuffers.getCount();
   if ( fbCount > 0 )
   {
      rglFramebuffer *contextFramebuffer = LContext->framebuffer ? rglGetFramebuffer( LContext, LContext->framebuffer ) : NULL;
      for ( GLuint i = 0;i < fbCount;++i )
      {
         rglFramebuffer* framebuffer = texture->framebuffers[i];
         framebuffer->needValidate = GL_TRUE;
         if (RGL_UNLIKELY( framebuffer == contextFramebuffer))
            LContext->needValidate |= PSGL_VALIDATE_FRAMEBUFFER;
      }
   }
}

//----------------------------------------
// Image.c
//----------------------------------------
GLboolean rglIsType( GLenum type );
GLboolean rglIsFormat( GLenum format );
GLboolean rglIsValidPair( GLenum format, GLenum type );
void rglImageAllocCPUStorage (void *data);
void rglImageFreeCPUStorage (void *data);
extern void	rglSetImage(void *data, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLsizei alignment, GLenum format, GLenum type, const GLvoid* pixels );
extern void	rglSetSubImage( GLenum target, GLint level, rglTexture *texture, rglImage* image, GLint x, GLint y, GLint z, GLsizei width, GLsizei height, GLsizei depth, GLsizei alignment, GLenum format, GLenum type, const GLvoid* pixels );
extern int	rglGetPixelSize( GLenum format, GLenum type );

static inline int rglGetStorageSize( GLenum format, GLenum type, GLsizei width, GLsizei height, GLsizei depth )
{
   return rglGetPixelSize( format, type )*width*height*depth;
}

extern int rglGetTypeSize( GLenum type );
extern int	rglGetMaxBitSize( GLenum type );
extern int	rglGetStorageSize( GLenum format, GLenum type, GLsizei width, GLsizei height, GLsizei depth );

//----------------------------------------
// FramebufferObject.c
//----------------------------------------
rglFramebuffer *rglCreateFramebuffer( void );
void rglDestroyFramebuffer (void *data);

static inline rglFramebuffer *rglGetFramebuffer( RGLcontext *LContext, GLuint name )
{
   return ( rglFramebuffer * )LContext->framebufferNameSpace.data[name];
}

static inline rglFramebuffer *rglGetFramebufferSafe( RGLcontext *LContext, GLuint name )
{
   return rglTexNameSpaceIsName( &LContext->framebufferNameSpace, name ) ? ( rglFramebuffer * )LContext->framebufferNameSpace.data[name] : NULL;
}

void rglFramebufferGetAttachmentTexture( RGLcontext* LContext, const rglFramebufferAttachment* attachment, rglTexture** texture, GLuint* face );
GLenum rglPlatformFramebufferCheckStatus (void *data);
void rglPlatformFramebufferGetParameteriv( GLenum pname, GLint* params );

//----------------------------------------
// VertexArray.c
//----------------------------------------
void rglVertexAttribPointerNV( GLuint index, GLint fsize, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer );
void rglEnableVertexAttribArrayNV( GLuint index );
void rglDisableVertexAttribArrayNV( GLuint index );

//----------------------------------------
// Device/Device.c
//----------------------------------------
extern void rglDeviceInit (void *data);
extern void rglDeviceExit (void);
extern PSGLdeviceParameters * rglShadowDeviceParameters (void);


//----------------------------------------
// Device/.../PlatformDevice.c
//----------------------------------------
extern GLboolean rglPlatformDeviceInit (void *data);
extern void		rglPlatformDeviceExit (void);
extern int		rglPlatformDeviceSize (void);
extern int		rglPlatformCreateDevice (void *data);
extern void		rglPlatformDestroyDevice (void *data);
extern void		rglPlatformMakeCurrent (void *data);
extern void		rglPlatformSwapBuffers (void *data);
extern const GLvoid*	rglPlatformGetProcAddress (const char *funcName);

//----------------------------------------
// Raster/.../PlatformRaster.c
//----------------------------------------
void*	rglPlatformRasterInit (void);
void	rglPlatformRasterExit (void* data);
void	rglPlatformRasterDestroyResources (void);
GLboolean rglPlatformNeedsConversion (const rglAttributeState* as, GLuint index);
// [YLIN] Try to avoid LHS inside this function.
//   In oringinal implementation, indexType and indexCount will be stored right before this function
//   and since we will load them right after enter this function, there are LHS.
GLboolean rglPlatformRequiresSlowPath (void *data, const GLenum indexType, uint32_t indexCount);
void rglPlatformRasterGetIntegerv( GLenum pname, GLint* params );
void	rglInvalidateAttributes (void);

//----------------------------------------
// Raster/.../PlatformTexture.c
//----------------------------------------
extern void	rglPlatformCreateTexture (void *data);
extern void	rglPlatformDestroyTexture (void *data);
void rglPlatformValidateVertexTextures (void);
extern GLenum rglPlatformChooseInternalStorage (void *data, GLenum internalformat);
extern void rglPlatformCopyTexSubImage3D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height );
void rglPlatformGetImageData( GLenum target, GLint level, rglTexture *texture, rglImage *image );

//----------------------------------------
// Raster/.../PlatformFBops.c
//----------------------------------------
extern void rglPlatformReadPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLboolean flip, GLenum format, GLenum type, GLvoid *pixels );
extern GLboolean rglPlatformReadPBOPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLboolean flip, GLenum format, GLenum type, GLvoid *pixels );

//----------------------------------------
// Raster/.../PlatformBuffer.c
//----------------------------------------
int rglPlatformBufferObjectSize (void);
GLboolean rglPlatformCreateBufferObject( rglBufferObject* bufferObject );
void rglPlatformDestroyBufferObject (void *data);
void rglPlatformBufferObjectSetData (void *buf_data, GLintptr offset, GLsizeiptr size, const GLvoid *data, GLboolean tryImmediateCopy );
char *rglPlatformBufferObjectMap (void *data, GLenum access );
GLboolean rglPlatformBufferObjectUnmap (void *data);
void rglPlatformGetBufferParameteriv( rglBufferObject *bufferObject, GLenum pname, int *params );

// this is shared in glBindTexture and cgGL code
RGL_EXPORT void rglBindTextureInternal (void *data, GLuint name, GLenum target);

#ifdef __cplusplus
}
#endif

#endif
