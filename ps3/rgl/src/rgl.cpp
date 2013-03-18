/*  RetroArch - A frontend for libretro.
 *  RGL - An OpenGL subset wrapper library.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "rgl.h"
#include "rglp.h"

/*============================================================
  BUFFERS
  ============================================================ */

static rglBufferObject *rglCreateBufferObject (void)
{
   GLuint size = sizeof( rglBufferObject ) + rglpBufferObjectSize();
   rglBufferObject *buffer = (rglBufferObject*)malloc(size);

   if(!buffer )
      return NULL;

   memset( buffer, 0, size ); 
   buffer->refCount = 1;
   new( &buffer->textureReferences ) RGL::Vector<rglTexture *>();

   return buffer;
}

static void rglFreeBufferObject (void *data)
{
   rglBufferObject *buffer = (rglBufferObject*)data;

   if ( --buffer->refCount == 0 )
   {
      rglPlatformDestroyBufferObject( buffer );
      buffer->textureReferences.~Vector<rglTexture *>();
      free( buffer );
   }
}

static void rglUnbindBufferObject (void *data, GLuint name)
{
   RGLcontext *LContext = (RGLcontext*)data;

   if (LContext->ArrayBuffer == name)
      LContext->ArrayBuffer = 0;
   if (LContext->PixelUnpackBuffer == name)
      LContext->PixelUnpackBuffer = 0;

   for ( int i = 0;i < RGL_MAX_VERTEX_ATTRIBS;++i )
   {
      if ( LContext->attribs->attrib[i].arrayBuffer == name )
      {
         LContext->attribs->attrib[i].arrayBuffer = 0;
         LContext->attribs->HasVBOMask &= ~( 1 << i );
      }
   }
}

GLAPI void APIENTRY glBindBuffer( GLenum target, GLuint name )
{
   RGLcontext *LContext = _CurrentContext;

   if (name)
      rglTexNameSpaceCreateNameLazy( &LContext->bufferObjectNameSpace, name );

   switch ( target )
   {
      case GL_ARRAY_BUFFER:
         LContext->ArrayBuffer = name;
         break;
      case GL_PIXEL_UNPACK_BUFFER_ARB:
         LContext->PixelUnpackBuffer = name;
         break;
      case GL_TEXTURE_REFERENCE_BUFFER_SCE:
         LContext->TextureBuffer = name;
         break;
      default:
         break;
   }
}

GLAPI GLvoid* APIENTRY glMapBuffer( GLenum target, GLenum access )
{
   RGLcontext *LContext = _CurrentContext;
   GLuint name = 0;

   switch ( target )
   {
      case GL_ARRAY_BUFFER:
         name = LContext->ArrayBuffer;
         break;
      case GL_PIXEL_UNPACK_BUFFER_ARB:
         name = LContext->PixelUnpackBuffer;
         break;
      case GL_TEXTURE_REFERENCE_BUFFER_SCE:
         name = LContext->TextureBuffer;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return NULL;
   }
   rglBufferObject* bufferObject = (rglBufferObject*)LContext->bufferObjectNameSpace.data[name];

#ifndef HAVE_RGL_2D
   switch ( access )
   {
      case GL_READ_ONLY:
      case GL_WRITE_ONLY:
      case GL_READ_WRITE:
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return NULL;
   }
#else
   (void)0;
#endif

   bufferObject->mapped = GL_TRUE;
   void *result = rglPlatformBufferObjectMap( bufferObject, access );

   return result;
}

GLAPI GLboolean APIENTRY glUnmapBuffer( GLenum target )
{
   RGLcontext *LContext = _CurrentContext;
   GLuint name = 0;

   switch ( target )
   {
      case GL_ARRAY_BUFFER:
         name = LContext->ArrayBuffer;
         break;
      case GL_TEXTURE_REFERENCE_BUFFER_SCE:
         name = LContext->TextureBuffer;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return GL_FALSE;
   }
   rglBufferObject* bufferObject = (rglBufferObject*)LContext->bufferObjectNameSpace.data[name];

   bufferObject->mapped = GL_FALSE;

   GLboolean result = rglPlatformBufferObjectUnmap( bufferObject );

   return result;
}

GLAPI void APIENTRY glDeleteBuffers( GLsizei n, const GLuint *buffers )
{
   RGLcontext *LContext = _CurrentContext;
   for (int i = 0; i < n; ++i)
   {
      if(!rglTexNameSpaceIsName(&LContext->bufferObjectNameSpace, buffers[i]))
         continue;
      if (buffers[i])
         rglUnbindBufferObject( LContext, buffers[i] );
   }
   rglTexNameSpaceDeleteNames( &LContext->bufferObjectNameSpace, n, buffers );
}

GLAPI void APIENTRY glGenBuffers( GLsizei n, GLuint *buffers )
{
   RGLcontext *LContext = _CurrentContext;
   rglTexNameSpaceGenNames( &LContext->bufferObjectNameSpace, n, buffers );
}

GLAPI void APIENTRY glBufferData( GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage )
{
   RGLcontext *LContext = _CurrentContext;

   GLuint name = 0;

   switch ( target )
   {
      case GL_ARRAY_BUFFER:
         name = LContext->ArrayBuffer;
         break;
      case GL_PIXEL_UNPACK_BUFFER_ARB:
         name = LContext->PixelUnpackBuffer;
         break;
      case GL_TEXTURE_REFERENCE_BUFFER_SCE:
         name = LContext->TextureBuffer;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }
   rglBufferObject* bufferObject = (rglBufferObject*)LContext->bufferObjectNameSpace.data[name];

   if ( bufferObject->refCount > 1 )
   {
      rglTexNameSpaceDeleteNames( &LContext->bufferObjectNameSpace, 1, &name );
      rglTexNameSpaceCreateNameLazy( &LContext->bufferObjectNameSpace, name );

      bufferObject = (rglBufferObject*)LContext->bufferObjectNameSpace.data[name];
   }

   if (bufferObject->size > 0)
      rglPlatformDestroyBufferObject( bufferObject );

   bufferObject->size = size;
   bufferObject->width = 0;
   bufferObject->height = 0;
   bufferObject->internalFormat = GL_NONE;

   if ( size > 0 )
   {
      GLboolean created = rglpCreateBufferObject(bufferObject);
      if ( !created )
      {
         rglSetError( GL_OUT_OF_MEMORY );
         return;
      }
      if (data)
         rglPlatformBufferObjectSetData( bufferObject, 0, size, data, GL_TRUE );
   }
}

GLAPI void APIENTRY glBufferSubData( GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data )
{
   RGLcontext *LContext = _CurrentContext;
   GLuint name = 0;

   switch ( target )
   {
      case GL_ARRAY_BUFFER:
         name = LContext->ArrayBuffer;
         break;
      case GL_PIXEL_UNPACK_BUFFER_ARB:
         name = LContext->PixelUnpackBuffer;
         break;
      case GL_TEXTURE_REFERENCE_BUFFER_SCE:
         name = LContext->TextureBuffer;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }

   rglBufferObject* bufferObject = (rglBufferObject*)LContext->bufferObjectNameSpace.data[name];

   if ( bufferObject->refCount > 1 )
   {
      rglBufferObject* oldBufferObject = bufferObject;

      rglTexNameSpaceDeleteNames( &LContext->bufferObjectNameSpace, 1, &name );
      rglTexNameSpaceCreateNameLazy( &LContext->bufferObjectNameSpace, name );

      bufferObject = (rglBufferObject*)LContext->bufferObjectNameSpace.data[name];
      bufferObject->size = oldBufferObject->size;

      GLboolean created = rglpCreateBufferObject(bufferObject);
      if ( !created )
      {
         rglSetError( GL_OUT_OF_MEMORY );
         return;
      }
      rglPlatformBufferObjectCopyData( bufferObject, oldBufferObject );
   }

   rglPlatformBufferObjectSetData( bufferObject, offset, size, data, GL_FALSE );
}

/*============================================================
  FRAMEBUFFER
  ============================================================ */

GLAPI void APIENTRY glClear( GLbitfield mask )
{
   RGLcontext*	LContext = _CurrentContext;

   if ( LContext->needValidate & RGL_VALIDATE_FRAMEBUFFER )
      rglValidateFramebuffer();
   rglFBClear( mask );
}

GLAPI void APIENTRY glClearColor( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha )
{
}

GLAPI void APIENTRY glBlendColor( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha )
{
   RGLcontext*	LContext = _CurrentContext;
   LContext->BlendColor.R = rglClampf( red );
   LContext->BlendColor.G = rglClampf( green );
   LContext->BlendColor.B = rglClampf( blue );
   LContext->BlendColor.A = rglClampf( alpha );

   LContext->needValidate |= RGL_VALIDATE_BLENDING;
}

GLAPI void APIENTRY glBlendFunc( GLenum sfactor, GLenum dfactor )
{
   RGLcontext*	LContext = _CurrentContext;

   LContext->BlendFactorSrcRGB = sfactor;
   LContext->BlendFactorSrcAlpha = sfactor;
   LContext->BlendFactorDestRGB = dfactor;
   LContext->BlendFactorDestAlpha = dfactor;
   LContext->needValidate |= RGL_VALIDATE_BLENDING;
}

/*============================================================
  FRAMEBUFFER OBJECTS
  ============================================================ */

void rglFramebufferGetAttachmentTexture(
      RGLcontext* LContext,
      const rglFramebufferAttachment* attachment,
      rglTexture** texture,
      GLuint* face )
{
   switch ( attachment->type )
   {
      case RGL_FRAMEBUFFER_ATTACHMENT_NONE:
         *texture = NULL;
         *face = 0;
         break;
      case RGL_FRAMEBUFFER_ATTACHMENT_RENDERBUFFER:
         break;
      case RGL_FRAMEBUFFER_ATTACHMENT_TEXTURE:
         *texture = rglGetTextureSafe( LContext, attachment->name );
         *face = 0;
         if ( *texture )
         {
            switch ( attachment->textureTarget )
            {
               case GL_TEXTURE_2D:
                  break;
               default:
                  *texture = NULL;
            }
         }
         break;
      default:
         *face = 0;
         *texture = NULL;
   }
}

rglFramebufferAttachment* rglFramebufferGetAttachment(void *data, GLenum attachment)
{
   rglFramebuffer *framebuffer = (rglFramebuffer*)data;

   switch ( attachment )
   {
      case GL_COLOR_ATTACHMENT0_EXT:
      case GL_COLOR_ATTACHMENT1_EXT:
      case GL_COLOR_ATTACHMENT2_EXT:
      case GL_COLOR_ATTACHMENT3_EXT:
         return &framebuffer->color[attachment - GL_COLOR_ATTACHMENT0_EXT];
      default:
         rglSetError( GL_INVALID_ENUM );
         return NULL;
   }
}

void rglGetFramebufferSize( GLuint* width, GLuint* height )
{
   RGLcontext*	LContext = _CurrentContext;

   *width = *height = 0;

   if ( LContext->framebuffer )
   {
      rglFramebuffer* framebuffer = rglGetFramebuffer( LContext, LContext->framebuffer );

      if (rglPlatformFramebufferCheckStatus(framebuffer) != GL_FRAMEBUFFER_COMPLETE_OES)
         return;

      for ( int i = 0; i < RGL_MAX_COLOR_ATTACHMENTS; ++i )
      {
         rglTexture* colorTexture = NULL;
         GLuint face = 0;
         rglFramebufferGetAttachmentTexture( LContext, &framebuffer->color[i], &colorTexture, &face );
         if (colorTexture == NULL)
            continue;

         unsigned texture_width = colorTexture->image->width;
         unsigned texture_height = colorTexture->image->height;
         *width = MIN( *width, texture_width);
         *height = MIN( *height, texture_height);
      }
   }
   else
   {
      RGLdevice *LDevice = _CurrentDevice;
      *width = LDevice->deviceParameters.width;
      *height = LDevice->deviceParameters.height;
   }
}

GLAPI GLboolean APIENTRY glIsFramebufferOES( GLuint framebuffer )
{
   RGLcontext* LContext = _CurrentContext;

   if ( !rglTexNameSpaceIsName( &LContext->framebufferNameSpace, framebuffer ) )
      return GL_FALSE;

   return GL_TRUE;
}

GLAPI void APIENTRY glBindFramebufferOES( GLenum target, GLuint framebuffer )
{
   RGLcontext* LContext = _CurrentContext;

   if ( framebuffer )
      rglTexNameSpaceCreateNameLazy( &LContext->framebufferNameSpace, framebuffer );

   LContext->framebuffer = framebuffer;
   LContext->needValidate |= RGL_VALIDATE_SCISSOR_BOX | RGL_VALIDATE_FRAMEBUFFER;
}

GLAPI void APIENTRY glDeleteFramebuffersOES( GLsizei n, const GLuint *framebuffers )
{
   RGLcontext *LContext = _CurrentContext;

   for ( int i = 0; i < n; ++i )
   {
      if ( framebuffers[i] && framebuffers[i] == LContext->framebuffer )
         glBindFramebufferOES( GL_FRAMEBUFFER_OES, 0 );
   }

   rglTexNameSpaceDeleteNames( &LContext->framebufferNameSpace, n, framebuffers );
}

GLAPI void APIENTRY glGenFramebuffersOES( GLsizei n, GLuint *framebuffers )
{
   RGLcontext *LContext = _CurrentContext;
   rglTexNameSpaceGenNames( &LContext->framebufferNameSpace, n, framebuffers );
}

GLAPI GLenum APIENTRY glCheckFramebufferStatusOES( GLenum target )
{
   RGLcontext* LContext = _CurrentContext;

   if (LContext->framebuffer)
   {
      rglFramebuffer* framebuffer = rglGetFramebuffer( LContext, LContext->framebuffer );

      return rglPlatformFramebufferCheckStatus( framebuffer );
   }

   return GL_FRAMEBUFFER_COMPLETE_OES;
}

GLAPI void APIENTRY glFramebufferTexture2DOES( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level )
{
   RGLcontext* LContext = _CurrentContext;

   rglFramebuffer* framebuffer = rglGetFramebuffer( LContext, LContext->framebuffer );
   rglFramebufferAttachment* attach = rglFramebufferGetAttachment( framebuffer, attachment );

   if (!attach)
      return;

   rglTexture *textureObject = NULL;
   GLuint face;
   rglFramebufferGetAttachmentTexture( LContext, attach, &textureObject, &face );

   if (textureObject)
      textureObject->framebuffers.removeElement( framebuffer );

   if (texture)
   {
      attach->type = RGL_FRAMEBUFFER_ATTACHMENT_TEXTURE;
      textureObject = rglGetTexture( LContext, texture );
      textureObject->framebuffers.pushBack( framebuffer );
   }
   else
      attach->type = RGL_FRAMEBUFFER_ATTACHMENT_NONE;
   attach->name = texture;
   attach->textureTarget = textarget;

   framebuffer->needValidate = GL_TRUE;
   LContext->needValidate |= RGL_VALIDATE_SCISSOR_BOX | RGL_VALIDATE_FRAMEBUFFER;
}


/*============================================================
  IMAGE CONVERSION
  ============================================================ */

#define GL_UNSIGNED_INT_24_8      GL_UNSIGNED_INT_24_8_SCE
#define GL_UNSIGNED_INT_8_24_REV  GL_UNSIGNED_INT_8_24_REV_SCE

#define GL_UNSIGNED_SHORT_8_8		GL_UNSIGNED_SHORT_8_8_SCE
#define GL_UNSIGNED_SHORT_8_8_REV	GL_UNSIGNED_SHORT_8_8_REV_SCE
#define GL_UNSIGNED_INT_16_16		GL_UNSIGNED_INT_16_16_SCE
#define GL_UNSIGNED_INT_16_16_REV	GL_UNSIGNED_INT_16_16_REV_SCE

#define DECLARE_C_TYPES \
   DECLARE_TYPE(GL_BYTE,GLbyte,127.f) \
DECLARE_TYPE(GL_UNSIGNED_BYTE,GLubyte,255.f) \
DECLARE_TYPE(GL_SHORT,GLshort,32767.f) \
DECLARE_TYPE(GL_UNSIGNED_SHORT,GLushort,65535.f) \
DECLARE_TYPE(GL_INT,GLint,2147483647.f) \
DECLARE_TYPE(GL_UNSIGNED_INT,GLuint,4294967295.0) \
DECLARE_TYPE(GL_FIXED,GLfixed,65535.f)

#define DECLARE_UNPACKED_TYPES \
   DECLARE_UNPACKED_TYPE(GL_BYTE) \
DECLARE_UNPACKED_TYPE(GL_UNSIGNED_BYTE) \
DECLARE_UNPACKED_TYPE(GL_SHORT) \
DECLARE_UNPACKED_TYPE(GL_UNSIGNED_SHORT) \
DECLARE_UNPACKED_TYPE(GL_INT) \
DECLARE_UNPACKED_TYPE(GL_UNSIGNED_INT) \
DECLARE_UNPACKED_TYPE(GL_HALF_FLOAT_ARB) \
DECLARE_UNPACKED_TYPE(GL_FLOAT) \
DECLARE_UNPACKED_TYPE(GL_FIXED)


#define DECLARE_PACKED_TYPES \
   DECLARE_PACKED_TYPE_AND_REV_2(UNSIGNED_BYTE,4,4) \
DECLARE_PACKED_TYPE_AND_REV_2(UNSIGNED_BYTE,6,2) \
DECLARE_PACKED_TYPE_AND_REV_3(UNSIGNED_BYTE,3,3,2) \
DECLARE_PACKED_TYPE_AND_REV_4(UNSIGNED_BYTE,2,2,2,2) \
DECLARE_PACKED_TYPE_AND_REV_2(UNSIGNED_SHORT,12,4) \
DECLARE_PACKED_TYPE_AND_REV_2(UNSIGNED_SHORT,8,8) \
DECLARE_PACKED_TYPE_AND_REV_3(UNSIGNED_SHORT,5,6,5) \
DECLARE_PACKED_TYPE_AND_REV_4(UNSIGNED_SHORT,4,4,4,4) \
DECLARE_PACKED_TYPE_AND_REV_4(UNSIGNED_SHORT,5,5,5,1) \
DECLARE_PACKED_TYPE_AND_REV_2(UNSIGNED_INT,16,16) \
DECLARE_PACKED_TYPE_AND_REV_2(UNSIGNED_INT,24,8) \
DECLARE_PACKED_TYPE_AND_REV_4(UNSIGNED_INT,8,8,8,8) \
DECLARE_PACKED_TYPE_AND_REV_4(UNSIGNED_INT,10,10,10,2)

#define DECLARE_FORMATS \
   DECLARE_FORMAT(GL_RGB,3) \
DECLARE_FORMAT(GL_BGR,3) \
DECLARE_FORMAT(GL_RGBA,4) \
DECLARE_FORMAT(GL_BGRA,4) \
DECLARE_FORMAT(GL_ABGR,4) \
DECLARE_FORMAT(GL_ARGB_SCE,4) \
DECLARE_FORMAT(GL_RED,1) \
DECLARE_FORMAT(GL_GREEN,1) \
DECLARE_FORMAT(GL_BLUE,1) \
DECLARE_FORMAT(GL_ALPHA,1)

#define DECLARE_TYPE(TYPE,CTYPE,MAXVAL) \
   typedef CTYPE type_##TYPE; \
static inline type_##TYPE rglFloatTo_##TYPE(float v) { return (type_##TYPE)(rglClampf(v)*MAXVAL); } \
static inline float rglFloatFrom_##TYPE(type_##TYPE v) { return ((float)v)/MAXVAL; }
DECLARE_C_TYPES
#undef DECLARE_TYPE

typedef GLfloat type_GL_FLOAT;
typedef GLhalfARB type_GL_HALF_FLOAT_ARB;

static inline type_GL_FLOAT rglFloatTo_GL_FLOAT(float v)
{
   return v;
}

static inline float rglFloatFrom_GL_FLOAT(type_GL_FLOAT v)
{
   return v;
}

static inline type_GL_HALF_FLOAT_ARB rglFloatTo_GL_HALF_FLOAT_ARB(float x)
{
   return rglFloatToHalf(x);
}

static inline float rglFloatFrom_GL_HALF_FLOAT_ARB(type_GL_HALF_FLOAT_ARB x)
{
   return rglHalfToFloat(x);
}

#define DECLARE_PACKED_TYPE_AND_REV_2(REALTYPE,S1,S2) \
   DECLARE_PACKED_TYPE(GL_##REALTYPE,GL_##REALTYPE##_##S1##_##S2,2,S1,S2,0,0,) \
DECLARE_PACKED_TYPE(GL_##REALTYPE,GL_##REALTYPE##_##S2##_##S1##_REV,2,S2,S1,0,0,_REV)

#define DECLARE_PACKED_TYPE_AND_REV_3(REALTYPE,S1,S2,S3) \
   DECLARE_PACKED_TYPE(GL_##REALTYPE,GL_##REALTYPE##_##S1##_##S2##_##S3,3,S1,S2,S3,0,) \
DECLARE_PACKED_TYPE(GL_##REALTYPE,GL_##REALTYPE##_##S3##_##S2##_##S1##_REV,3,S3,S2,S1,0,_REV)

#define DECLARE_PACKED_TYPE_AND_REV_4(REALTYPE,S1,S2,S3,S4) \
   DECLARE_PACKED_TYPE(GL_##REALTYPE,GL_##REALTYPE##_##S1##_##S2##_##S3##_##S4,4,S1,S2,S3,S4,) \
DECLARE_PACKED_TYPE(GL_##REALTYPE,GL_##REALTYPE##_##S4##_##S3##_##S2##_##S1##_REV,4,S4,S3,S2,S1,_REV)

#define DECLARE_PACKED_TYPE_AND_REALTYPE(REALTYPE,N,S1,S2,S3,S4,REV) \
   DECLARE_PACKED_TYPE(GL_##REALTYPE,PACKED_TYPE(REALTYPE,N,S1,S2,S3,S4,REV),N,S1,S2,S3,S4,REV)

#define INDEX(N,X) (X)
#define INDEX_REV(N,X) (N-1-X)

#define GET_BITS(to,from,first,count) if ((count)>0) to=((GLfloat)(((from)>>(first))&((1<<(count))-1)))/(GLfloat)((1<<((count==0)?1:count))-1)
#define PUT_BITS(from,to,first,count) if ((count)>0) to|=((unsigned int)((from)*((GLfloat)((1<<((count==0)?1:count))-1))))<<(first);

inline static int rglGetComponentCount( GLenum format )
{
   switch ( format )
   {
#define DECLARE_FORMAT(FORMAT,COUNT) \
      case FORMAT: \
                   return COUNT;
      DECLARE_FORMATS
#undef DECLARE_FORMAT
      case GL_PALETTE8_RGB8_OES:
      case GL_PALETTE8_RGBA8_OES:
      case GL_PALETTE8_R5_G6_B5_OES:
      case GL_PALETTE8_RGBA4_OES:
      case GL_PALETTE8_RGB5_A1_OES:
         return 1;
      default:
         return 0;
   }
}

int rglGetTypeSize( GLenum type )
{
   switch ( type )
   {

#define DECLARE_PACKED_TYPE(REALTYPE,TYPE,N,S1,S2,S3,S4,REV) \
      case TYPE: \
                 return sizeof(type_##REALTYPE);
      DECLARE_PACKED_TYPES
#undef DECLARE_PACKED_TYPE

#define DECLARE_UNPACKED_TYPE(TYPE) \
      case TYPE: \
                 return sizeof(type_##TYPE);
         DECLARE_UNPACKED_TYPES
#undef DECLARE_UNPACKED_TYPE

      default:
         return 0;
   }
}
int rglGetPixelSize( GLenum format, GLenum type )
{
   int componentSize;
   switch ( type )
   {

#define DECLARE_PACKED_TYPE(REALTYPE,TYPE,N,S1,S2,S3,S4,REV) \
      case TYPE: \
                 return sizeof(type_##REALTYPE);
      DECLARE_PACKED_TYPES
#undef DECLARE_PACKED_TYPE

#define DECLARE_UNPACKED_TYPE(TYPE) \
      case TYPE: \
                 componentSize=sizeof(type_##TYPE); \
         break;
         DECLARE_UNPACKED_TYPES
#undef DECLARE_UNPACKED_TYPE

      default:
         return 0;
   }
   return rglGetComponentCount( format )*componentSize;
}

void rglRawRasterToImage(const void *in_data,
      void *out_data, GLuint x, GLuint y, GLuint z )
{
   const rglRaster* raster = (const rglRaster*)in_data;
   rglImage *image = (rglImage*)out_data;
   const int pixelBits = rglGetPixelSize( image->format, image->type ) * 8;

   const GLuint size = pixelBits / 8;

   if ( raster->xstride == image->xstride &&
         raster->ystride == image->ystride &&
         raster->zstride == image->zstride )
   {
      memcpy((char*)image->data +
            x*image->xstride + y*image->ystride + z*image->zstride,
            raster->data, raster->depth*raster->zstride );

      return;
   }
   else if ( raster->xstride == image->xstride )
   {
      const GLuint lineBytes = raster->width * raster->xstride;
      for ( int i = 0; i < raster->depth; ++i )
      {
         for ( int j = 0; j < raster->height; ++j )
         {
            const char *src = ( const char * )raster->data +
               i * raster->zstride + j * raster->ystride;
            char *dst = ( char * )image->data +
               ( i + z ) * image->zstride +
               ( j + y ) * image->ystride +
               x * image->xstride;
            memcpy( dst, src, lineBytes );
         }
      }

      return;
   }

   for ( int i = 0; i < raster->depth; ++i )
   {
      for ( int j = 0; j < raster->height; ++j )
      {
         const char *src = ( const char * )raster->data +
            i * raster->zstride + j * raster->ystride;
         char *dst = ( char * )image->data +
            ( i + z ) * image->zstride +
            ( j + y ) * image->ystride +
            x * image->xstride;

         for ( int k = 0; k < raster->width; ++k )
         {
            memcpy( dst, src, size );

            src += raster->xstride;
            dst += image->xstride;
         }
      }
   }
}

void rglImageAllocCPUStorage (void *data)
{
   rglImage *image = (rglImage*)data;

   if (( image->storageSize > image->mallocStorageSize ) || ( !image->mallocData ) )
   {
      if (image->mallocData)
         free( image->mallocData );

      image->mallocData = (char *)malloc( image->storageSize + 128 );
      image->mallocStorageSize = image->storageSize;
   }
   image->data = rglPadPtr( image->mallocData, 128 );
}

void rglImageFreeCPUStorage(void *data)
{
   rglImage *image = (rglImage*)data;

   if (!image->mallocData)
      return;

   free( image->mallocData );
   image->mallocStorageSize = 0;
   image->data = NULL;
   image->mallocData = NULL;
   image->dataState &= ~RGL_IMAGE_DATASTATE_HOST;
}

static inline void rglSetImageTexRef(void *data, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLsizei alignment)
{
   rglImage *image = (rglImage*)data;

   image->width = width;
   image->height = height;
   image->depth = depth;
   image->alignment = alignment;

   image->xblk = 0;
   image->yblk = 0;

   image->xstride = 0;
   image->ystride = 0;
   image->zstride = 0;

   image->format = 0;
   image->type = 0;
   image->internalFormat = 0;
   const GLenum status = rglPlatformChooseInternalStorage( image, internalFormat );
   (( void )status );

   image->data = NULL;
   image->mallocData = NULL;
   image->mallocStorageSize = 0;

   image->isSet = GL_TRUE;

   if ( image->xstride == 0 )
      image->xstride = rglGetPixelSize( image->format, image->type );
   if ( image->ystride == 0 )
      image->ystride = image->width * image->xstride;
   if ( image->zstride == 0 )
      image->zstride = image->height * image->ystride;

   image->dataState = RGL_IMAGE_DATASTATE_UNSET;
}

void rglSetImage(void *data, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLsizei alignment, GLenum format, GLenum type, const void *pixels )
{
   rglImage *image = (rglImage*)data;

   image->width = width;
   image->height = height;
   image->depth = depth;
   image->alignment = alignment;

   image->xblk = 0;
   image->yblk = 0;

   image->xstride = 0;
   image->ystride = 0;
   image->zstride = 0;

   image->format = 0;
   image->type = 0;
   image->internalFormat = 0;
   const GLenum status = rglPlatformChooseInternalStorage( image, internalFormat );
   (( void )status );

   image->data = NULL;
   image->mallocData = NULL;
   image->mallocStorageSize = 0;

   image->isSet = GL_TRUE;

   {
      if ( image->xstride == 0 )
         image->xstride = rglGetPixelSize( image->format, image->type );
      if ( image->ystride == 0 )
         image->ystride = image->width * image->xstride;
      if ( image->zstride == 0 )
         image->zstride = image->height * image->ystride;
   }

   if (pixels)
   {
      rglImageAllocCPUStorage( image );
      if ( !image->data )
         return;

      rglRaster raster;
      raster.format = format;
      raster.type = type;
      raster.width = width;
      raster.height = height;
      raster.depth = depth;
      raster.data = (void*)pixels;

      raster.xstride = rglGetPixelSize( raster.format, raster.type );
      raster.ystride = (raster.width * raster.xstride + alignment - 1) / alignment * alignment;
      raster.zstride = raster.height * raster.ystride;

      rglRawRasterToImage( &raster, image, 0, 0, 0 );
      image->dataState = RGL_IMAGE_DATASTATE_HOST;
   }
   else
      image->dataState = RGL_IMAGE_DATASTATE_UNSET;
}

/*============================================================
  ENGINE
  ============================================================ */

static char* rglVendorString = "RetroArch";

static char* rglRendererString = "RGL";
static char* rglExtensionsString = "";

static char* rglVersionNumber = "2.00";
char* rglVersion = "2.00";

RGLcontext* _CurrentContext = NULL;

RGL_EXPORT RGLcontextHookFunction rglContextCreateHook = NULL;
RGL_EXPORT RGLcontextHookFunction rglContextDestroyHook = NULL;

void rglSetError( GLenum error )
{
}

GLAPI GLenum APIENTRY glGetError(void)
{
   if (!_CurrentContext )
      return GL_INVALID_OPERATION;
   else
   {
      GLenum error = _CurrentContext->error;

      _CurrentContext->error = GL_NO_ERROR;
      return error;
   }
}

static void rglGetTextureIntegerv( GLenum pname, GLint* params )
{
   switch ( pname )
   {
      case GL_MAX_TEXTURE_SIZE:
         params[0] = RGLP_MAX_TEXTURE_SIZE;
         break;
      default:
         fprintf(stderr, "rglGetTextureIntegerv: enum not supported.\n");
         return;
   }
}

GLAPI void APIENTRY glGetIntegerv(GLenum pname, GLint* params)
{
   switch (pname)
   {
      case GL_MAX_TEXTURE_SIZE:
         rglGetTextureIntegerv(pname, params);
         break;
      default:
         fprintf(stderr, "glGetIntegerv: enum not supported.\n");
         break;
   }
}

GLuint rglValidateStates (GLuint mask)
{
   RGLcontext* LContext = _CurrentContext;

   GLuint  dirty = LContext->needValidate & ~mask;
   LContext->needValidate &= mask;

   GLuint  needValidate = LContext->needValidate;

   if (RGL_UNLIKELY( needValidate & RGL_VALIDATE_FRAMEBUFFER))
   {
      rglValidateFramebuffer();
      needValidate = LContext->needValidate;
   }

   if (RGL_UNLIKELY( needValidate & RGL_VALIDATE_TEXTURES_USED))
   {
      long unitInUseCount = LContext->BoundFragmentProgram->samplerCount;
      const GLuint* unitsInUse = LContext->BoundFragmentProgram->samplerUnits;
      for ( long i = 0; i < unitInUseCount; ++i )
      {
         long unit = unitsInUse[i];
         rglTexture* texture = LContext->TextureImageUnits[unit].currentTexture;

         if (texture)
            rglPlatformValidateTextureStage( unit, texture );
      }
   }

   if (RGL_UNLIKELY(needValidate & RGL_VALIDATE_VERTEX_PROGRAM))
   {
      rglValidateVertexProgram();
   }

   if (RGL_LIKELY(needValidate & RGL_VALIDATE_VERTEX_CONSTANTS))
   {
      rglValidateVertexConstants();
   }

   if (RGL_UNLIKELY(needValidate & RGL_VALIDATE_FRAGMENT_PROGRAM))
   {
      rglValidateFragmentProgram();
   }

   if ( RGL_LIKELY(( needValidate & ~( RGL_VALIDATE_TEXTURES_USED |
                  RGL_VALIDATE_VERTEX_PROGRAM |
                  RGL_VALIDATE_VERTEX_CONSTANTS |
                  RGL_VALIDATE_FRAGMENT_PROGRAM ) ) == 0 ) )
   {
      LContext->needValidate = 0;
      return dirty;
   }

   if ( RGL_UNLIKELY( needValidate & RGL_VALIDATE_VIEWPORT ) )
      rglpValidateViewport();

   if ( RGL_UNLIKELY( needValidate & RGL_VALIDATE_BLENDING ) )
      rglpValidateBlending();

   if ( RGL_UNLIKELY( needValidate & RGL_VALIDATE_SHADER_SRGB_REMAP ) )
      rglpValidateShaderSRGBRemap();

   LContext->needValidate = 0;
   return dirty;
}

void rglResetAttributeState(void *data)
{
   rglAttributeState *as = (rglAttributeState*)data;

   for ( int i = 0; i < RGL_MAX_VERTEX_ATTRIBS; ++i )
   {
      as->attrib[i].clientSize = 4;
      as->attrib[i].clientType = GL_FLOAT;
      as->attrib[i].clientStride = 16;
      as->attrib[i].clientData = NULL;

      as->attrib[i].value[0] = 0.0f;
      as->attrib[i].value[1] = 0.0f;
      as->attrib[i].value[2] = 0.0f;
      as->attrib[i].value[3] = 1.0f;

      as->attrib[i].normalized = GL_FALSE;
      as->attrib[i].frequency = 1;

      as->attrib[i].arrayBuffer = 0;
   }

   as->attrib[RGL_ATTRIB_PRIMARY_COLOR_INDEX].value[0] = 1.0f;
   as->attrib[RGL_ATTRIB_PRIMARY_COLOR_INDEX].value[1] = 1.0f;
   as->attrib[RGL_ATTRIB_PRIMARY_COLOR_INDEX].value[2] = 1.0f;
   as->attrib[RGL_ATTRIB_PRIMARY_COLOR_INDEX].value[3] = 1.0f;

   as->attrib[RGL_ATTRIB_SECONDARY_COLOR_INDEX].value[0] = 1.0f;
   as->attrib[RGL_ATTRIB_SECONDARY_COLOR_INDEX].value[1] = 1.0f;
   as->attrib[RGL_ATTRIB_SECONDARY_COLOR_INDEX].value[2] = 1.0f;
   as->attrib[RGL_ATTRIB_SECONDARY_COLOR_INDEX].value[3] = 1.0f;

   as->attrib[RGL_ATTRIB_NORMAL_INDEX].value[0] = 0.f;
   as->attrib[RGL_ATTRIB_NORMAL_INDEX].value[1] = 0.f;
   as->attrib[RGL_ATTRIB_NORMAL_INDEX].value[2] = 1.f;

   as->DirtyMask = ( 1 << RGL_MAX_VERTEX_ATTRIBS ) - 1;
   as->EnabledMask = 0;
   as->HasVBOMask = 0;
}

static void rglResetContext (void *data)
{
   RGLcontext *LContext = (RGLcontext*)data;

   rglTexNameSpaceResetNames( &LContext->textureNameSpace );
   rglTexNameSpaceResetNames( &LContext->bufferObjectNameSpace );
   rglTexNameSpaceResetNames( &LContext->framebufferNameSpace );
   rglTexNameSpaceResetNames( &LContext->fenceObjectNameSpace );

   LContext->ViewPort.X = 0;
   LContext->ViewPort.Y = 0;
   LContext->ViewPort.XSize = 0;
   LContext->ViewPort.YSize = 0;

   LContext->PerspectiveCorrectHint = GL_DONT_CARE;

   LContext->DepthNear = 0.f;
   LContext->DepthFar = 1.f;

   LContext->DrawBuffer = LContext->ReadBuffer = GL_COLOR_ATTACHMENT0_EXT;

   LContext->ShaderSRGBRemap = GL_FALSE;

   LContext->Blending = GL_FALSE;
   LContext->BlendColor.R = 0.0f;
   LContext->BlendColor.G = 0.0f;
   LContext->BlendColor.B = 0.0f;
   LContext->BlendColor.A = 0.0f;
   LContext->BlendEquationRGB = GL_FUNC_ADD;
   LContext->BlendEquationAlpha = GL_FUNC_ADD;
   LContext->BlendFactorSrcRGB = GL_ONE;
   LContext->BlendFactorDestRGB = GL_ZERO;
   LContext->BlendFactorSrcAlpha = GL_ONE;
   LContext->BlendFactorDestAlpha = GL_ZERO;

   LContext->ColorLogicOp = GL_FALSE;
   LContext->LogicOp = GL_COPY;

   LContext->Dithering = GL_TRUE;

   LContext->TexCoordReplaceMask = 0;

   for ( int i = 0;i < RGL_MAX_TEXTURE_IMAGE_UNITS;++i )
   {
      rglTextureImageUnit *tu = LContext->TextureImageUnits + i;
      tu->bound2D = 0;

      tu->enable2D = GL_FALSE;
      tu->fragmentTarget = 0;

      tu->lodBias = 0.f;
      tu->currentTexture = NULL;
   }

   LContext->ActiveTexture = 0;
   LContext->CurrentImageUnit = LContext->TextureImageUnits;

   LContext->packAlignment = 4;
   LContext->unpackAlignment = 4;

   rglResetAttributeState( &LContext->defaultAttribs0 );
   LContext->attribs = &LContext->defaultAttribs0;

   LContext->framebuffer = 0;

   LContext->VertexProgram = GL_FALSE;
   LContext->BoundVertexProgram = 0;

   LContext->FragmentProgram = GL_FALSE;
   LContext->BoundFragmentProgram = 0;

   LContext->ArrayBuffer = 0;
   LContext->PixelUnpackBuffer = 0;
   LContext->TextureBuffer = 0;

   LContext->VSync = GL_FALSE;

   LContext->AllowTXPDemotion = GL_FALSE; 

}

RGLcontext* psglCreateContext(void)
{
   RGLcontext* LContext = (RGLcontext*)malloc(sizeof(RGLcontext));

   if (!LContext)
      return NULL;

   memset(LContext, 0, sizeof(RGLcontext));

   LContext->error = GL_NO_ERROR;

   rglTexNameSpaceInit( &LContext->textureNameSpace, ( rglTexNameSpaceCreateFunction )rglAllocateTexture, ( rglTexNameSpaceDestroyFunction )rglFreeTexture );

   for ( int i = 0;i < RGL_MAX_TEXTURE_IMAGE_UNITS;++i )
   {
      rglTextureImageUnit *tu = LContext->TextureImageUnits + i;

      tu->default2D = rglAllocateTexture();
      if ( !tu->default2D )
      {
         psglDestroyContext( LContext );
         return NULL;
      }
      tu->default2D->target = GL_TEXTURE_2D;
   }

   rglTexNameSpaceInit( &LContext->bufferObjectNameSpace, ( rglTexNameSpaceCreateFunction )rglCreateBufferObject, ( rglTexNameSpaceDestroyFunction )rglFreeBufferObject );
   rglTexNameSpaceInit( &LContext->framebufferNameSpace, ( rglTexNameSpaceCreateFunction )rglCreateFramebuffer, ( rglTexNameSpaceDestroyFunction )rglDestroyFramebuffer );

   LContext->needValidate = 0;
   LContext->everAttached = 0;

   LContext->RGLcgLastError = CG_NO_ERROR;
   LContext->RGLcgErrorCallbackFunction = NULL;
   LContext->RGLcgContextHead = ( CGcontext )NULL;

   rglInitNameSpace( &LContext->cgProgramNameSpace );
   rglInitNameSpace( &LContext->cgParameterNameSpace );
   rglInitNameSpace( &LContext->cgContextNameSpace );

   rglResetContext( LContext );

   if (rglContextCreateHook)
      rglContextCreateHook( LContext );

   return( LContext );
}

void RGL_EXPORT psglResetCurrentContext(void)
{
   RGLcontext *context = _CurrentContext;
   rglResetContext(context);
   context->needValidate |= RGL_VALIDATE_ALL;
}

RGLcontext *psglGetCurrentContext(void)
{
   return _CurrentContext;
}

void RGL_EXPORT psglDestroyContext (void *data)
{
   RGLcontext *LContext = (RGLcontext*)data;
   if ( _CurrentContext == LContext )
      rglpFifoGlFinish();

   while ( LContext->RGLcgContextHead != ( CGcontext )NULL )
   {
      RGLcontext* current = _CurrentContext;
      _CurrentContext = LContext;
      cgDestroyContext( LContext->RGLcgContextHead );
      _CurrentContext = current;
   }
   rglFreeNameSpace( &LContext->cgProgramNameSpace );
   rglFreeNameSpace( &LContext->cgParameterNameSpace );
   rglFreeNameSpace( &LContext->cgContextNameSpace );

   if ( rglContextDestroyHook ) rglContextDestroyHook( LContext );

   for ( int i = 0; i < RGL_MAX_TEXTURE_IMAGE_UNITS; ++i )
   {
      rglTextureImageUnit* tu = LContext->TextureImageUnits + i;
      if ( tu->default2D ) rglFreeTexture( tu->default2D );
   }

   rglTexNameSpaceFree( &LContext->textureNameSpace );
   rglTexNameSpaceFree( &LContext->bufferObjectNameSpace );
   rglTexNameSpaceFree( &LContext->fenceObjectNameSpace );
   rglTexNameSpaceFree( &LContext->framebufferNameSpace );

   if ( _CurrentContext == LContext )
      psglMakeCurrent( NULL, NULL );

   free( LContext );
}

void rglInvalidateAllStates (void *data)
{
   RGLcontext *context = (RGLcontext*)data;
   context->needValidate = RGL_VALIDATE_ALL;
   context->attribs->DirtyMask = ( 1 << RGL_MAX_VERTEX_ATTRIBS ) - 1;
}

void rglAttachContext (RGLdevice *device, RGLcontext* context)
{
   if (!context->everAttached)
   {
      context->ViewPort.XSize = device->deviceParameters.width;
      context->ViewPort.YSize = device->deviceParameters.height;
      context->needValidate |= RGL_VALIDATE_VIEWPORT | RGL_VALIDATE_SCISSOR_BOX;
      context->everAttached = GL_TRUE;
   }
   rglInvalidateAllStates( context );
}

GLAPI void APIENTRY glEnable( GLenum cap )
{
   RGLcontext* LContext = _CurrentContext;

   switch ( cap )
   {
      case GL_TEXTURE_2D:
         LContext->CurrentImageUnit->enable2D = GL_TRUE;
         rglUpdateCurrentTextureCache( LContext->CurrentImageUnit );
         LContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
         break;
      case GL_SHADER_SRGB_REMAP_SCE:
         LContext->ShaderSRGBRemap = GL_TRUE;
         LContext->needValidate |= RGL_VALIDATE_SHADER_SRGB_REMAP;
         break;
      case GL_BLEND:
         LContext->Blending = GL_TRUE;
         LContext->needValidate |= RGL_VALIDATE_BLENDING;
         break;
      case GL_DITHER:
         LContext->Dithering = GL_TRUE;
         break;
      case GL_VSYNC_SCE:
         LContext->VSync = GL_TRUE;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }
}

GLAPI void APIENTRY glDisable( GLenum cap )
{
   RGLcontext* LContext = _CurrentContext;

   switch ( cap )
   {
      case GL_TEXTURE_2D:
         LContext->CurrentImageUnit->enable2D = GL_FALSE;
         rglUpdateCurrentTextureCache( LContext->CurrentImageUnit );
         LContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
         break;
      case GL_SHADER_SRGB_REMAP_SCE:
         LContext->ShaderSRGBRemap = GL_FALSE;
         LContext->needValidate |= RGL_VALIDATE_SHADER_SRGB_REMAP;
         break;
      case GL_BLEND:
         LContext->Blending = GL_FALSE;
         LContext->needValidate |= RGL_VALIDATE_BLENDING;
         break;
      case GL_DITHER:
         LContext->Dithering = GL_FALSE;
         break;
      case GL_VSYNC_SCE:
         LContext->VSync = GL_FALSE;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }
}

GLAPI void APIENTRY glEnableClientState( GLenum array )
{
   switch ( array )
   {
      case GL_VERTEX_ARRAY:
         rglEnableVertexAttribArrayNV( RGL_ATTRIB_POSITION_INDEX );
         break;
      case GL_COLOR_ARRAY:
         rglEnableVertexAttribArrayNV( RGL_ATTRIB_PRIMARY_COLOR_INDEX );
         break;
      case GL_NORMAL_ARRAY:
         rglEnableVertexAttribArrayNV( RGL_ATTRIB_NORMAL_INDEX );
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }
}

GLAPI void APIENTRY glDisableClientState( GLenum array )
{
   switch ( array )
   {
      case GL_VERTEX_ARRAY:
         rglDisableVertexAttribArrayNV( RGL_ATTRIB_POSITION_INDEX );
         break;
      case GL_COLOR_ARRAY:
         rglDisableVertexAttribArrayNV( RGL_ATTRIB_PRIMARY_COLOR_INDEX );
         break;
      case GL_NORMAL_ARRAY:
         rglDisableVertexAttribArrayNV( RGL_ATTRIB_NORMAL_INDEX );
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }
}

GLAPI void APIENTRY glFlush(void)
{
   RGLcontext * LContext = _CurrentContext;

   if (RGL_UNLIKELY(LContext->needValidate))
      rglValidateStates( RGL_VALIDATE_ALL );

   rglPlatformRasterFlush();
}

GLAPI const GLubyte* APIENTRY glGetString( GLenum name )
{
   switch ( name )
   {
      case GL_VENDOR:
         return(( GLubyte* )rglVendorString );
      case GL_RENDERER:
         return(( GLubyte* )rglRendererString );
      case GL_VERSION:
         return(( GLubyte* )rglVersionNumber );
      case GL_EXTENSIONS:
         return(( GLubyte* )rglExtensionsString );
      default:
         {
            rglSetError( GL_INVALID_ENUM );
            return(( GLubyte* )NULL );
         }
   }
}

void psglInit (void *data)
{
   RGLinitOptions *options = (RGLinitOptions*)data;
   rglPsglPlatformInit(options);
}

void psglExit(void)
{
   rglPsglPlatformExit();
}

/*============================================================
  RASTER
  ============================================================ */

GLAPI void APIENTRY glViewport( GLint x, GLint y, GLsizei width, GLsizei height )
{
   RGLcontext*	LContext = _CurrentContext;

   LContext->ViewPort.X = x;
   LContext->ViewPort.Y = y;
   LContext->ViewPort.XSize = width;
   LContext->ViewPort.YSize = height;
   LContext->needValidate |= RGL_VALIDATE_VIEWPORT;
}

/*============================================================
  TEXTURES (INTERNAL)
  ============================================================ */

rglTexture *rglAllocateTexture(void)
{
   GLuint size = sizeof(rglTexture) + rglPlatformTextureSize();
   rglTexture *texture = (rglTexture*)malloc(size);
   memset( texture, 0, size );
   texture->target = 0;
   texture->minFilter = GL_NEAREST_MIPMAP_LINEAR;
   texture->magFilter = GL_LINEAR;
   texture->minLod = -1000.f;
   texture->maxLod = 1000.f;
   texture->baseLevel = 0;
   texture->maxLevel = 1000;
   texture->wrapS = GL_REPEAT;
   texture->wrapT = GL_REPEAT;
   texture->wrapR = GL_REPEAT;
   texture->lodBias = 0.f;
   texture->maxAnisotropy = 1.f;
   texture->compareMode = GL_NONE;
   texture->compareFunc = GL_LEQUAL;
   texture->gammaRemap = 0;
   texture->vertexEnable = GL_FALSE;
   texture->usage = 0;
   texture->isRenderTarget = GL_FALSE;
   texture->image = NULL;
   texture->isComplete = GL_FALSE;
   texture->imageCount = 0;
   texture->faceCount = 1;
   texture->revalidate = 0;
   texture->referenceBuffer = NULL;
   new( &texture->framebuffers ) RGL::Vector<rglFramebuffer *>();
   rglPlatformCreateTexture( texture );
   return texture;
}

void rglFreeTexture (void *data)
{
   rglTexture *texture = (rglTexture*)data;
   rglTextureTouchFBOs(texture);
   texture->framebuffers.~Vector<rglFramebuffer *>();

   if ( texture->image )
   {
      rglImage *image = texture->image;
      rglImageFreeCPUStorage( image );
      free( texture->image );
   }
   if ( texture->referenceBuffer )
      texture->referenceBuffer->textureReferences.removeElement( texture );
   rglPlatformDestroyTexture( texture );
   free( texture );
}

void rglTextureUnbind (void *data, GLuint name )
{
   RGLcontext *context = (RGLcontext*)data;
   int unit;

   for ( unit = 0; unit < RGL_MAX_TEXTURE_IMAGE_UNITS; ++unit)
   {
      rglTextureImageUnit *tu = context->TextureImageUnits + unit;
      GLboolean dirty = GL_FALSE;
      if ( tu->bound2D == name )
      {
         tu->bound2D = 0;
         dirty = GL_TRUE;
      }
      if ( dirty )
      {
         rglUpdateCurrentTextureCache( tu );
         context->needValidate |= RGL_VALIDATE_TEXTURES_USED;
      }
   }
}

GLboolean rglTextureIsValid (const void *data)
{
   const rglTexture *texture = (const rglTexture*)data;

   if (texture->imageCount < 1 + texture->baseLevel)
      return GL_FALSE;
   if ( !texture->image )
      return GL_FALSE;

   const rglImage* image = texture->image + texture->baseLevel;

   GLenum format = image->format;
   GLenum type = image->type;
   GLenum internalFormat = image->internalFormat;

   if (( texture->vertexEnable ) && ( internalFormat != GL_FLOAT_RGBA32 )
         && ( internalFormat != GL_RGBA32F_ARB ))
      return GL_FALSE;
   if (( internalFormat == 0 ) || ( format == 0 ) || ( type == 0 ) )
      return GL_FALSE;

   // don't need more than max level
   if ( texture->imageCount < 1 )
      return GL_FALSE;

   return GL_TRUE;
}

// Reallocate images held by a texture
void rglReallocateImages (void *data, GLint level, GLsizei dimension )
{
   rglTexture *texture = (rglTexture*)data;
   GLuint oldCount = texture->imageCount;

   if (dimension <= 0)
      dimension = 1;

   GLuint n = level + 1 + rglLog2( dimension );
   n = MAX( n, oldCount );

   rglImage *image = ( rglImage * )realloc( texture->image, n * sizeof( rglImage ) );
   memset( image + oldCount, 0, ( n - oldCount )*sizeof( rglImage ) );

   texture->image = image;
   texture->imageCount = n;
}

// Get an enabled texture mode of a texture image unit
GLenum rglGetEnabledTextureMode (const void *data)
{
   const rglTextureImageUnit *unit = (const rglTextureImageUnit*)data;
   // here, if fragment program is enabled and a valid program is set, get the enabled
   // units from the program instead of the texture units.
   if ( _CurrentContext->BoundFragmentProgram != NULL && _CurrentContext->FragmentProgram != GL_FALSE)
      return unit->fragmentTarget;
   else if ( unit->enable2D )
      return GL_TEXTURE_2D;

   return 0;
}

rglTexture *rglGetCurrentTexture (const void *data, GLenum target)
{
   const rglTextureImageUnit *unit = (const rglTextureImageUnit*)data;
   RGLcontext*	LContext = _CurrentContext;
   GLuint name = 0;
   rglTexture *defaultTexture = NULL;
   switch ( target )
   {
      case GL_TEXTURE_2D:
         name = unit->bound2D;
         defaultTexture = unit->default2D;
         break;
      default:
         return NULL;
   }
   if (name)
      return ( rglTexture * )LContext->textureNameSpace.data[name];
   else
      return defaultTexture;
}

void rglUpdateCurrentTextureCache (void *data)
{
   rglTextureImageUnit *unit = (rglTextureImageUnit*)data;
   GLenum target = rglGetEnabledTextureMode( unit );
   unit->currentTexture = rglGetCurrentTexture( unit, target );
}

int rglGetImage( GLenum target, GLint level, rglTexture **texture, rglImage **image, GLsizei reallocateSize )
{
   RGLcontext*	LContext = _CurrentContext;
   rglTextureImageUnit *unit = LContext->CurrentImageUnit;

   GLenum expectedTarget = 0;
   switch ( target )
   {
      case GL_TEXTURE_2D:
         expectedTarget = GL_TEXTURE_2D;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return -1;
   }

   rglTexture *tex = rglGetCurrentTexture( unit, expectedTarget );

   if (level >= ( int )tex->imageCount)
      rglReallocateImages( tex, level, reallocateSize );

   *image = tex->image + level;
   *texture = tex;
   return 0;
}

void rglBindTextureInternal (void *data, GLuint name, GLenum target )
{
   rglTextureImageUnit *unit = (rglTextureImageUnit*)data;
   RGLcontext*	LContext = _CurrentContext;
   rglTexture *texture = NULL;

   if (name)
   {
      rglTexNameSpaceCreateNameLazy( &LContext->textureNameSpace, name );
      texture = ( rglTexture * )LContext->textureNameSpace.data[name];

      if (!texture->target)
      {
         texture->target = target;
         texture->faceCount = 1;
      }
   }

#ifndef HAVE_RGL_2D
   switch ( target )
   {
      case GL_TEXTURE_2D:
         unit->bound2D = name;
         break;
      default:
         break;
   }
#else
   unit->bound2D = name;
#endif

   rglUpdateCurrentTextureCache( unit );
   LContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
}

/*============================================================
  TEXTURES
  ============================================================ */

GLAPI void APIENTRY glGenTextures( GLsizei n, GLuint *textures )
{
   RGLcontext*	LContext = _CurrentContext;
   rglTexNameSpaceGenNames( &LContext->textureNameSpace, n, textures );
}

GLAPI void APIENTRY glDeleteTextures( GLsizei n, const GLuint *textures )
{
   RGLcontext*	LContext = _CurrentContext;

   for ( int i = 0;i < n;++i )
   {
      if (textures[i])
         rglTextureUnbind( LContext, textures[i] );
   }

   rglTexNameSpaceDeleteNames( &LContext->textureNameSpace, n, textures );
}

GLAPI void APIENTRY glTexParameteri( GLenum target, GLenum pname, GLint param )
{
   RGLcontext*	LContext = _CurrentContext;
   rglTexture *texture = rglGetCurrentTexture( LContext->CurrentImageUnit, target );
   switch ( pname )
   {
      case GL_TEXTURE_MIN_FILTER:
         texture->minFilter = param;
         if ( texture->referenceBuffer == 0 )
            texture->revalidate |= RGL_TEXTURE_REVALIDATE_LAYOUT;
         break;
      case GL_TEXTURE_MAG_FILTER:
         texture->magFilter = param;
         break;
      case GL_TEXTURE_MAX_LEVEL:
         texture->maxLevel = param;
         break;
      case GL_TEXTURE_WRAP_S:
         texture->wrapS = param;
         break;
      case GL_TEXTURE_WRAP_T:
         texture->wrapT = param;
         break;
      case GL_TEXTURE_WRAP_R:
         texture->wrapR = param;
         break;
      case GL_TEXTURE_FROM_VERTEX_PROGRAM_SCE:
         if ( param != 0 )
            texture->vertexEnable = GL_TRUE;
         else
            texture->vertexEnable = GL_FALSE;
         texture->revalidate |= RGL_TEXTURE_REVALIDATE_LAYOUT;
         break;
      case GL_TEXTURE_ALLOCATION_HINT_SCE:
         texture->usage = param;
         texture->revalidate |= RGL_TEXTURE_REVALIDATE_LAYOUT;
         break;
      case GL_TEXTURE_COMPARE_MODE_ARB:
         texture->compareMode = param;
         break;
      case GL_TEXTURE_COMPARE_FUNC_ARB:
         texture->compareFunc = param;
         break;
      case GL_TEXTURE_GAMMA_REMAP_R_SCE:
      case GL_TEXTURE_GAMMA_REMAP_G_SCE:
      case GL_TEXTURE_GAMMA_REMAP_B_SCE:
      case GL_TEXTURE_GAMMA_REMAP_A_SCE:
         {
            GLuint bit = 1 << ( pname - GL_TEXTURE_GAMMA_REMAP_R_SCE );
            if ( param ) texture->gammaRemap |= bit;
            else texture->gammaRemap &= ~bit;
         }
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }
   texture->revalidate |= RGL_TEXTURE_REVALIDATE_PARAMETERS;
   LContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
}

GLAPI void APIENTRY glBindTexture( GLenum target, GLuint name )
{
   RGLcontext*	LContext = _CurrentContext;
   rglTextureImageUnit *unit = LContext->CurrentImageUnit;

   rglBindTextureInternal( unit, name, target );
}

GLAPI void APIENTRY glTexImage2D( GLenum target, GLint level, GLint internalFormat,
      GLsizei width, GLsizei height, GLint border, GLenum format,
      GLenum type, const GLvoid *pixels )
{
   RGLcontext*	LContext = _CurrentContext;

   rglTexture *texture;
   rglImage *image;
   if (rglGetImage(target, level, &texture, &image, MAX(width, height)))
      return;

   image->dataState = RGL_IMAGE_DATASTATE_UNSET;

   rglBufferObject* bufferObject = NULL;
   if ( LContext->PixelUnpackBuffer != 0 )
   {
      bufferObject = 
         (rglBufferObject*)LContext->bufferObjectNameSpace.data[LContext->PixelUnpackBuffer];
      pixels = rglPlatformBufferObjectMap( bufferObject, GL_READ_ONLY ) +
         (( const GLubyte* )pixels - ( const GLubyte* )NULL );
   }

   rglSetImage(
         image,
         internalFormat,
         width, height, 1,
         LContext->unpackAlignment,
         format, type,
         pixels );


   if ( LContext->PixelUnpackBuffer != 0 )
      rglPlatformBufferObjectUnmap( bufferObject );

   texture->revalidate |= RGL_TEXTURE_REVALIDATE_IMAGES;

   rglTextureTouchFBOs( texture );

   LContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
}

GLAPI void APIENTRY glActiveTexture( GLenum texture )
{
   RGLcontext*	LContext = _CurrentContext;

   int unit = texture - GL_TEXTURE0;
   LContext->ActiveTexture = unit;

   if(unit < RGL_MAX_TEXTURE_IMAGE_UNITS)
      LContext->CurrentImageUnit = LContext->TextureImageUnits + unit;
   else
      LContext->CurrentImageUnit = NULL;
}

GLAPI void APIENTRY glTextureReferenceSCE( GLenum target, GLuint levels,
      GLuint baseWidth, GLuint baseHeight, GLuint baseDepth, GLenum internalFormat, GLuint pitch, GLintptr offset )
{
   RGLcontext*	LContext = _CurrentContext;

   rglTexture *texture = rglGetCurrentTexture( LContext->CurrentImageUnit, target );
   rglBufferObject *bufferObject = 
      (rglBufferObject*)LContext->bufferObjectNameSpace.data[LContext->TextureBuffer];
   rglReallocateImages( texture, 0, MAX( baseWidth, MAX( baseHeight, baseDepth ) ) );

   rglSetImageTexRef(texture->image, internalFormat, baseWidth, baseHeight,
         baseDepth, LContext->unpackAlignment);
   texture->maxLevel = 0;
   texture->usage = GL_TEXTURE_LINEAR_GPU_SCE;

   GLboolean r = rglPlatformTextureReference( texture, pitch, bufferObject, offset );
   if ( !r ) return;
   bufferObject->textureReferences.pushBack( texture );
   texture->referenceBuffer = bufferObject;
   texture->offset = offset;
   rglTextureTouchFBOs( texture );
   LContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
}

/*============================================================
  VERTEX ARRAYS
  ============================================================ */

#include <ppu_intrinsics.h> /* TODO: move to platform-specific code */

const uint32_t c_rounded_size_ofrglDrawParams = (sizeof(rglDrawParams)+0x7f)&~0x7f;
static uint8_t s_dparams_buff[ c_rounded_size_ofrglDrawParams ] __attribute__((aligned(128)));

GLAPI void APIENTRY glVertexPointer( GLint size, GLenum type, GLsizei stride, const GLvoid* pointer )
{
   rglVertexAttribPointerNV( RGL_ATTRIB_POSITION_INDEX, size, type, GL_FALSE, stride, pointer );
}

GLAPI void APIENTRY glNormalPointer( GLenum type, GLsizei stride, const GLvoid* pointer )
{
   rglVertexAttribPointerNV( RGL_ATTRIB_NORMAL_INDEX, 3, type, GL_TRUE, stride, pointer );
}

GLAPI void APIENTRY glColor4f( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha )
{
   rglVertexAttrib4fNV( RGL_ATTRIB_PRIMARY_COLOR_INDEX, red, green, blue, alpha );
}

GLAPI void APIENTRY glColor4ub( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha )
{
   const float f = 1.f / 255.f;
   rglVertexAttrib4fNV( RGL_ATTRIB_PRIMARY_COLOR_INDEX, f*red, f*green, f*blue, f*alpha );
}

GLAPI void APIENTRY glColor4fv( const GLfloat *v )
{
   rglVertexAttrib4fvNV( RGL_ATTRIB_PRIMARY_COLOR_INDEX, v );
}

void rglVertexAttribPointerNV(
      GLuint index,
      GLint fsize,
      GLenum type,
      GLboolean normalized,
      GLsizei stride,
      const void* pointer)
{
   RGLcontext*	LContext = _CurrentContext;

   GLsizei defaultStride = 0;
   switch (type)
   {
      case GL_FLOAT:
      case GL_HALF_FLOAT_ARB:
      case GL_BYTE:
      case GL_UNSIGNED_BYTE:
      case GL_SHORT:
      case GL_FIXED:
         defaultStride = fsize * rglGetTypeSize( type );
         break;
      case GL_FIXED_11_11_10_SCE:
         defaultStride = 4;
         break;
      default:
         rglSetError( GL_INVALID_ENUM );
         return;
   }

   rglAttributeState* as = LContext->attribs;
   rglAttribute* attrib = as->attrib + index;
   attrib->clientSize = fsize;
   attrib->clientType = type;
   attrib->clientStride = stride ? stride : defaultStride;
   attrib->clientData = (void*)pointer;
   attrib->arrayBuffer = LContext->ArrayBuffer;
   attrib->normalized = normalized;
   RGLBIT_ASSIGN( as->HasVBOMask, index, attrib->arrayBuffer != 0 );

   RGLBIT_TRUE( as->DirtyMask, index );
}

void rglEnableVertexAttribArrayNV (GLuint index)
{
   RGLcontext *LContext = _CurrentContext;

   RGLBIT_TRUE( LContext->attribs->EnabledMask, index );
   RGLBIT_TRUE( LContext->attribs->DirtyMask, index );
}

void rglDisableVertexAttribArrayNV (GLuint index)
{
   RGLcontext *LContext = _CurrentContext;

   RGLBIT_FALSE( LContext->attribs->EnabledMask, index );
   RGLBIT_TRUE( LContext->attribs->DirtyMask, index );
}

void rglVertexAttrib1fNV (GLuint index, GLfloat x)
{
   RGLcontext*	LContext = _CurrentContext;

   rglAttribute* attrib = LContext->attribs->attrib + index;
   attrib->value[0] = x;
   attrib->value[1] = 0.0f;
   attrib->value[2] = 0.0f;
   attrib->value[3] = 1.0f;
   RGLBIT_TRUE( LContext->attribs->DirtyMask, index );
}

void rglVertexAttrib2fNV (GLuint index, GLfloat x, GLfloat y)
{
   RGLcontext*	LContext = _CurrentContext;

   rglAttribute* attrib = LContext->attribs->attrib + index;
   attrib->value[0] = x;
   attrib->value[1] = y;
   attrib->value[2] = 0.0f;
   attrib->value[3] = 1.0f;
   RGLBIT_TRUE( LContext->attribs->DirtyMask, index );
}

void rglVertexAttrib3fNV( GLuint index, GLfloat x, GLfloat y, GLfloat z )
{
   RGLcontext*	LContext = _CurrentContext;

   rglAttribute* attrib = LContext->attribs->attrib + index;
   attrib->value[0] = x;
   attrib->value[1] = y;
   attrib->value[2] = z;
   attrib->value[3] = 1.0f;
   RGLBIT_TRUE( LContext->attribs->DirtyMask, index );

}

void rglVertexAttrib4fNV( GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w )
{
   RGLcontext*	LContext = _CurrentContext;

   rglAttribute* attrib = LContext->attribs->attrib + index;
   attrib->value[0] = x;
   attrib->value[1] = y;
   attrib->value[2] = z;
   attrib->value[3] = w;
   RGLBIT_TRUE( LContext->attribs->DirtyMask, index );
}

void rglVertexAttrib1fvNV( GLuint index, const GLfloat* v )
{
   rglVertexAttrib1fNV( index, v[0] );
}

void rglVertexAttrib2fvNV( GLuint index, const GLfloat* v )
{
   rglVertexAttrib2fNV( index, v[0], v[1] );
}

void rglVertexAttrib3fvNV( GLuint index, const GLfloat* v )
{
   rglVertexAttrib3fNV( index, v[0], v[1], v[2] );
}

void rglVertexAttrib4fvNV( GLuint index, const GLfloat* v )
{
   rglVertexAttrib4fNV( index, v[0], v[1], v[2], v[3] );
}

GLAPI void APIENTRY glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
   RGLcontext*	LContext = _CurrentContext;

   if (RGL_UNLIKELY(!RGLBIT_GET(LContext->attribs->EnabledMask, RGL_ATTRIB_POSITION_INDEX)))
      return;

   uint32_t _tmp_clear_loop = c_rounded_size_ofrglDrawParams>>7;
   do{
      --_tmp_clear_loop;
      __dcbz(s_dparams_buff+(_tmp_clear_loop<<7));
   }while(_tmp_clear_loop);

   rglDrawParams *dparams = (rglDrawParams *)s_dparams_buff;
   dparams->mode = mode;
   dparams->firstVertex = first;
   dparams->vertexCount = count;

   if ( LContext->needValidate ) rglValidateStates( RGL_VALIDATE_ALL );

   GLboolean slowPath = rglPlatformRequiresSlowPath( dparams, 0, 0);
   (void)slowPath;

   rglPlatformDraw( dparams );
}

/*============================================================
  DEVICE CONTEXT CREATION
  ============================================================ */

RGLdevice *_CurrentDevice = NULL;

void rglDeviceInit (void *data)
{
   RGLinitOptions *options = (RGLinitOptions*)data;
   rglPlatformDeviceInit(options);
}

void rglDeviceExit(void)
{
   rglPlatformDeviceExit();
}

RGL_EXPORT RGLdevice*	psglCreateDeviceAuto( GLenum colorFormat, GLenum depthFormat, GLenum multisamplingMode )
{
   return rglPlatformCreateDeviceAuto(colorFormat, depthFormat, multisamplingMode);
}

RGL_EXPORT RGLdevice*	psglCreateDeviceExtended (const void *data)
{
   const RGLdeviceParameters *parameters = (const RGLdeviceParameters*)data;
   return rglPlatformCreateDeviceExtended(parameters);
}

RGL_EXPORT GLfloat psglGetDeviceAspectRatio (const void *data)
{
   const RGLdevice *device = (const RGLdevice*)data;
   return rglPlatformGetDeviceAspectRatio(device);
}

RGL_EXPORT void psglGetDeviceDimensions (const RGLdevice * device, GLuint *width, GLuint *height)
{
   *width = device->deviceParameters.width;
   *height = device->deviceParameters.height;
}

RGL_EXPORT void psglGetRenderBufferDimensions (const RGLdevice * device, GLuint *width, GLuint *height)
{
   *width = device->deviceParameters.renderWidth;
   *height = device->deviceParameters.renderHeight;
}

RGL_EXPORT void psglDestroyDevice (void *data)
{
   RGLdevice *device = (RGLdevice*)data;
   if (_CurrentDevice == device)
      psglMakeCurrent( NULL, NULL );

   if (device->rasterDriver)
      rglPlatformRasterExit( device->rasterDriver );

   rglPlatformDestroyDevice( device );

   free( device );
}

void RGL_EXPORT psglMakeCurrent (RGLcontext *context, RGLdevice *device)
{
   if ( context && device )
   {
      rglPlatformMakeCurrent( device->platformDevice );
      _CurrentContext = context;
      _CurrentDevice = device;
      if ( !device->rasterDriver )
      {
         device->rasterDriver = rglPlatformRasterInit();
      }
      rglAttachContext( device, context );
   }
   else
   {
      rglPlatformMakeCurrent( NULL );
      _CurrentContext = NULL;
      _CurrentDevice = NULL;
   }
}

RGLdevice *psglGetCurrentDevice (void)
{
   return _CurrentDevice;
}

GLAPI void RGL_EXPORT psglSwap (void)
{
#ifndef HAVE_RGL_2D
   if ( _CurrentDevice != NULL)
#endif
      rglPlatformSwapBuffers( _CurrentDevice );
}
