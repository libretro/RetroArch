#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <float.h>

#include <cell/gcm.h>

#include <sys/sys_time.h>

#include "rgl.h"
#include "private.h"
#include <ppu_intrinsics.h>

#include "cg.h"
#include <Cg/cgc.h>
#include <Cg/cgGL.h>
#include "readelf.h"
#include "cgnv2rt.h"

#include <cell/sysmodule.h>

#include "../../../compat/strl.h"
#include "../../../general.h"

#define ENDIAN_32(X, F) ((F) ? endianSwapWord(X) : (X))
#define SWAP_IF_BIG_ENDIAN(arg) endianSwapWordByHalf(arg)

#define ROW_MAJOR 0
#define COL_MAJOR 1

#define pad(x, pad) (((x) + (pad) - 1 ) / (pad) * (pad))

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

static int _RGLInitCompleted = 0;
static char *_RGLVendorString = "Retro Arch";
static char *_RGLRendererString = "RGL";
static char *_RGLExtensionsString = "";
static char *_RGLVersionNumber = "1.0";

PSGLcontext* _CurrentContext = NULL;
RGLcontextHookFunction _RGLContextCreateHook = NULL;
RGLcontextHookFunction _RGLContextDestroyHook = NULL;

GmmAllocator *pGmmLocalAllocator = NULL;
GmmAllocator *pGmmMainAllocator = NULL;
static volatile uint32_t *pLock = NULL;
static uint32_t cachedLockValue = 0;
static GmmFixedAllocData *pGmmFixedAllocData = NULL;
GLuint nvFenceCounter = 0;

#define CAPACITY_INCR 16
#define NAME_INCREMENT 4

#define DECLARE_TYPE(TYPE,CTYPE,MAXVAL) \
typedef CTYPE type_##TYPE; \
static inline type_##TYPE _RGLFloatTo_##TYPE(float v) { return (type_##TYPE)((MAX(MIN(v, 1.f), 0.f)) * MAXVAL); } \
static inline float _RGLFloatFrom_##TYPE(type_##TYPE v) { return ((float)v)/MAXVAL; }
DECLARE_C_TYPES
#undef DECLARE_TYPE

typedef GLfloat type_GL_FLOAT;

static inline type_GL_FLOAT _RGLFloatTo_GL_FLOAT(float v)
{
   return v;
}

static inline float _RGLFloatFrom_GL_FLOAT(type_GL_FLOAT v)
{
   return v;
}

typedef GLhalfARB type_GL_HALF_FLOAT_ARB;

static const char *findSectionInPlace(const char* memory,unsigned int /*size*/,const char *name, size_t *sectionSize)
{
   const Elf32_Ehdr *ehdr = (const Elf32_Ehdr*)memory;
	
   const char *sectionHeaderStart = (const char*)ehdr + ehdr->e_shoff;
   const Elf32_Shdr *shstrtabHeader = (const Elf32_Shdr*)sectionHeaderStart + ehdr->e_shstrndx;
   const char *shstrtab = (const char*)ehdr + shstrtabHeader->sh_offset;

   size_t sectionCount = ehdr->e_shnum;

   for(size_t i=0; i < sectionCount; i++)
   {
      const Elf32_Shdr *sectionHeader = (const Elf32_Shdr *)sectionHeaderStart + i;
      const char *sectionName = shstrtab + sectionHeader->sh_name;
      if (!strcmp(name,sectionName))
      {
         *sectionSize = sectionHeader->sh_size;
	 return (const char*)ehdr + sectionHeader->sh_offset;
      }
   }
   return NULL;
}

static const char *findSymbolSectionInPlace(const char *memory, unsigned int /*size*/, size_t *symbolSize, size_t *symbolCount, const char **symbolstrtab)
{
   const Elf32_Ehdr *ehdr = (const Elf32_Ehdr*)memory;
	
   size_t sectionCount = ehdr->e_shnum;
   const char *sectionHeaderStart = (const char*)ehdr + ehdr->e_shoff;

   for(size_t i = 0; i<sectionCount; i++)
   {
      const Elf32_Shdr *sectionHeader = (const Elf32_Shdr *)sectionHeaderStart + i;
      if (sectionHeader->sh_type == SHT_SYMTAB)
      {
         *symbolSize = sectionHeader->sh_entsize;
	 *symbolCount = sectionHeader->sh_size / sectionHeader->sh_entsize;

	 const Elf32_Shdr *symbolStrHeader = (const Elf32_Shdr *)sectionHeaderStart + sectionHeader->sh_link;
	 *symbolstrtab = (const char*)ehdr + symbolStrHeader->sh_offset;
	 return (const char*)ehdr + sectionHeader->sh_offset;
      }
   }
   return NULL;
}

static int lookupSymbolValueInPlace(const char* symbolSection, size_t symbolSize, size_t symbolCount, const char *symbolstrtab, const char *name)
{
   for (size_t i = 0; i < symbolCount; i++)
   {
      Elf32_Sym* elf_sym = (Elf32_Sym*)symbolSection;

      if (!strcmp(symbolstrtab + elf_sym->st_name, name))
         return elf_sym->st_value;

      symbolSection += symbolSize;
   }
   return -1;
}

static const char *getSymbolByIndexInPlace(const char* symbolSection, size_t symbolSize, size_t symbolCount,  const char *symbolstrtab, int index)
{
   Elf32_Sym* elf_sym = (Elf32_Sym*)symbolSection + index;
   return symbolstrtab + elf_sym->st_name;
}

static inline type_GL_HALF_FLOAT_ARB _RGLFloatTo_GL_HALF_FLOAT_ARB( float x )
{
   jsIntAndFloat V = {f: x};
   unsigned int S = ( V.i >> 31 ) & 1;
   int E = (( V.i >> 23 ) & 0xff ) - 0x7f;
   unsigned int M = V.i & 0x007fffff;
   if (( E == 0x80 ) && ( M ) ) return 0x7fff;
   else if ( E >= 15 ) return( S << 15 ) | 0x7c00;
   else if ( E <= -14 ) return( S << 15 ) | (( 0x800000 + M ) >> ( -14 - E ) );
   else return( S << 15 ) | ((( E + 15 )&0x1f ) << 10 ) | ( M >> 13 );
}

static inline float _RGLFloatFrom_GL_HALF_FLOAT_ARB( type_GL_HALF_FLOAT_ARB x )
{
    unsigned int S = x >> 15;
    unsigned int E = ( x & 0x7C00 ) >> 10;
    unsigned int M = x & 0x03ff;
    float f;
    if ( E == 31 )
    {
        if ( M == 0 ) f = _RGLInfinity.f;
        else f = _RGLNan.f;
    }
    else if ( E == 0 )
    {
        if ( M == 0 ) f = 0.f;
        else f = M * 1.f / ( 1 << 24 );
    }
    else f = ( 0x400 + M ) * 1.f / ( 1 << 25 ) * ( 1 << E );
    return S ? -f : f;
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

static void _RGLResetAttributeState( jsAttributeState* as )
{
    for(int i = 0; i < MAX_VERTEX_ATTRIBS; ++i)
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

    as->attrib[_RGL_ATTRIB_PRIMARY_COLOR_INDEX].value[0] = 1.0f;
    as->attrib[_RGL_ATTRIB_PRIMARY_COLOR_INDEX].value[1] = 1.0f;
    as->attrib[_RGL_ATTRIB_PRIMARY_COLOR_INDEX].value[2] = 1.0f;
    as->attrib[_RGL_ATTRIB_PRIMARY_COLOR_INDEX].value[3] = 1.0f;

    as->attrib[_RGL_ATTRIB_SECONDARY_COLOR_INDEX].value[0] = 1.0f;
    as->attrib[_RGL_ATTRIB_SECONDARY_COLOR_INDEX].value[1] = 1.0f;
    as->attrib[_RGL_ATTRIB_SECONDARY_COLOR_INDEX].value[2] = 1.0f;
    as->attrib[_RGL_ATTRIB_SECONDARY_COLOR_INDEX].value[3] = 1.0f;

    as->attrib[_RGL_ATTRIB_NORMAL_INDEX].value[0] = 0.f;
    as->attrib[_RGL_ATTRIB_NORMAL_INDEX].value[1] = 0.f;
    as->attrib[_RGL_ATTRIB_NORMAL_INDEX].value[2] = 1.f;

    as->DirtyMask = (1 << MAX_VERTEX_ATTRIBS) - 1;
    as->EnabledMask = 0;
    as->NeedsConversionMask = 0;
    as->HasVBOMask = 0;
    as->ModuloMask = 0;
}

static jsAttribSet* _RGLCreateAttribSet( void )
{
   jsAttribSet* attribSet = (jsAttribSet*)memalign(16, sizeof(jsAttribSet));

   _RGLResetAttributeState(&attribSet->attribs);

   attribSet->dirty = GL_TRUE;
   attribSet->beenUpdatedMask = 0;
   attribSet->cmdBuffer = NULL;
   attribSet->cmdNumWords = 0;

   return attribSet;
}

static void _RGLDestroyAttribSet( jsAttribSet* attribSet )
{
   if ( attribSet->cmdBuffer )
      free( attribSet->cmdBuffer );
   free( attribSet );
}

static void _RGLAttribSetDeleteBuffer( PSGLcontext *LContext, GLuint buffName )
{
   jsBufferObject *bufferObject = ( jsBufferObject * )LContext->bufferObjectNameSpace.data[buffName];
   GLuint attrSetCount = bufferObject->attribSets.getCount();
   if ( attrSetCount == 0 ) return;
   for ( unsigned int i = 0;i < attrSetCount;++i )
   {
      jsAttribSet *attribSet = bufferObject->attribSets[i];

      for(GLuint j = 0; j < MAX_VERTEX_ATTRIBS; ++j)
      {
         if(attribSet->attribs.attrib[j].arrayBuffer == buffName)
            attribSet->attribs.attrib[j].arrayBuffer = 0;
      }

      attribSet->dirty = GL_TRUE;
   }
   LContext->attribSetDirty = GL_TRUE;
   bufferObject->attribSets.clear();
}

static jsBufferObject *_RGLCreateBufferObject (void)
{
   GLuint size = sizeof(jsBufferObject) + sizeof(RGLBufferObject);
   jsBufferObject *buffer = (jsBufferObject*)malloc(size);

   if( !buffer )
      return NULL;

   memset(buffer, 0, size);

   buffer->refCount = 1;
   new( &buffer->textureReferences ) RGL::Vector<jsTexture *>();
   new( &buffer->attribSets ) RGL::Vector<jsAttribSet *>();

   return buffer;
}

static void _RGLPlatformDestroyBufferObject( jsBufferObject* bufferObject )
{
   RGLBufferObject *jsBuffer = (RGLBufferObject*)bufferObject->platformBufferObject;

   switch(jsBuffer->pool)
   {
      case SURFACE_POOL_SYSTEM:
      case SURFACE_POOL_LINEAR:
         gmmFree( jsBuffer->bufferId );
	 break;
      case SURFACE_POOL_NONE:
         break;
      default:
	 break;
   }

   jsBuffer->pool = SURFACE_POOL_NONE;
   jsBuffer->bufferId = GMM_ERROR;
}

static void _RGLFreeBufferObject( jsBufferObject *buffer )
{
   if(--buffer->refCount == 0)
   {
      _RGLPlatformDestroyBufferObject( buffer );
      buffer->textureReferences.~Vector<jsTexture *>();
      buffer->attribSets.~Vector<jsAttribSet *>();

      if(buffer != NULL)
         free( buffer );
   }
}

GLAPI void APIENTRY glBindBuffer( GLenum target, GLuint name )
{
    PSGLcontext *LContext = _CurrentContext;

    if(name)
       _RGLTexNameSpaceCreateNameLazy( &LContext->bufferObjectNameSpace, name );

    switch(target)
    {
        case GL_ARRAY_BUFFER:
            LContext->ArrayBuffer = name;
            break;
        case GL_ELEMENT_ARRAY_BUFFER:
            break;
        case GL_PIXEL_PACK_BUFFER_ARB:
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

GLAPI void APIENTRY glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
   PSGLcontext *LContext = _CurrentContext;
   for(int i = 0; i < n; ++i)
   {
      if(!_RGLTexNameSpaceIsName( &LContext->bufferObjectNameSpace, buffers[i] ) )
         continue;

      if(buffers[i] )
      {
         GLuint name = buffers[i];

	 if(LContext->ArrayBuffer == name)
            LContext->ArrayBuffer = 0;

	 if(LContext->PixelUnpackBuffer == name)
            LContext->PixelUnpackBuffer = 0;

	 for(int i = 0;i < MAX_VERTEX_ATTRIBS; ++i)
	 {
            if(LContext->attribs->attrib[i].arrayBuffer == name)
	    {
               LContext->attribs->attrib[i].arrayBuffer = 0;
	       LContext->attribs->HasVBOMask &= ~( 1 << i );
	    }
	 }
	 _RGLAttribSetDeleteBuffer( LContext, name );
      }
   }
   _RGLTexNameSpaceDeleteNames( &LContext->bufferObjectNameSpace, n, buffers );
}

GLAPI void APIENTRY glGenBuffers(GLsizei n, GLuint *buffers)
{
   PSGLcontext *LContext = _CurrentContext;
   _RGLTexNameSpaceGenNames( &LContext->bufferObjectNameSpace, n, buffers );
}

static inline void _RGLTextureTouchFBOs(jsTexture *texture)
{
   PSGLcontext *LContext = _CurrentContext;
   if(!LContext)
      return;

   GLuint fbCount = texture->framebuffers.getCount();

   if(fbCount > 0)
   {
      jsFramebuffer *contextFramebuffer = LContext->framebuffer ? (jsFramebuffer *)LContext->framebufferNameSpace.data[LContext->framebuffer] : NULL;

      for (GLuint i = 0; i < fbCount; ++i)
      {
         jsFramebuffer* framebuffer = texture->framebuffers[i];
	 framebuffer->needValidate = GL_TRUE;
	 if(RGL_UNLIKELY(framebuffer == contextFramebuffer))
            LContext->needValidate |= PSGL_VALIDATE_FRAMEBUFFER;
      }
   }
}

static void _RGLAllocateBuffer(jsBufferObject* bufferObject)
{
   RGLBufferObject *jsBuffer = (RGLBufferObject *)bufferObject->platformBufferObject;

   _RGLPlatformDestroyBufferObject(bufferObject);

   jsBuffer->pool = SURFACE_POOL_LINEAR;
   jsBuffer->bufferId = gmmAlloc(0, jsBuffer->bufferSize);
   jsBuffer->pitch = 0;

   if(jsBuffer->bufferId == GMM_ERROR)
      jsBuffer->pool = SURFACE_POOL_NONE;

   GLuint referenceCount = bufferObject->textureReferences.getCount();
   if(referenceCount > 0)
   {
      for (GLuint i = 0;i < referenceCount; ++i)
      {
         jsTexture *texture = bufferObject->textureReferences[i];
	 RGLTexture *gcmTexture = ( RGLTexture * )texture->platformTexture;
	 gcmTexture->gpuAddressId = jsBuffer->bufferId;
	 gcmTexture->gpuAddressIdOffset = texture->offset;
	 texture->revalidate |= TEXTURE_REVALIDATE_PARAMETERS;
	 _RGLTextureTouchFBOs( texture );
      }
      _CurrentContext->needValidate |= PSGL_VALIDATE_TEXTURES_USED | PSGL_VALIDATE_VERTEX_TEXTURES_USED ;
   }
}

static void _RGLMemcpy( const GLuint dstId, unsigned dstOffset, unsigned int pitch, const GLuint srcId, unsigned int size )
{
   pitch = pitch ? : 64;
   const GLuint dstOffsetAlign = dstOffset % pitch;
   GLuint srcOffset = 0;

   if ( dstOffsetAlign )
   {
      const GLuint firstBytes = MIN( pitch - dstOffsetAlign, size );

      transfer_params_t transfer_params;

      transfer_params.dst_id        = dstId;
      transfer_params.dst_id_offset = 0;
      transfer_params.dst_pitch     = pitch;
      transfer_params.dst_x         = dstOffsetAlign / 2;
      transfer_params.dst_y         = dstOffset / pitch;
      transfer_params.src_id        = srcId;
      transfer_params.src_id_offset = srcOffset;
      transfer_params.src_pitch     = pitch;
      transfer_params.src_x         = 0;
      transfer_params.src_y         = 0;
      transfer_params.width         = firstBytes / 2;
      transfer_params.height        = 1;
      transfer_params.bpp           = 2;
      transfer_params.fifo_ptr      = &_RGLState.fifo;

      TransferDataVidToVid(&transfer_params);

      dstOffset += firstBytes;
      srcOffset += firstBytes;
      size -= firstBytes;
   }

   const GLuint fullLines = size / pitch;
   const GLuint extraBytes = size % pitch;

   if ( fullLines )
   {
      transfer_params_t transfer_params;

      transfer_params.dst_id        = dstId;
      transfer_params.dst_id_offset = 0;
      transfer_params.dst_pitch     = pitch;
      transfer_params.dst_x         = 0;
      transfer_params.dst_y         = dstOffset / pitch;
      transfer_params.src_id        = srcId;
      transfer_params.src_id_offset = srcOffset;
      transfer_params.src_pitch     = pitch;
      transfer_params.src_x         = 0;
      transfer_params.src_y         = 0;
      transfer_params.width         = pitch / 2;
      transfer_params.height        = fullLines;
      transfer_params.bpp           = 2;
      transfer_params.fifo_ptr      = &_RGLState.fifo;

      TransferDataVidToVid(&transfer_params);
   }

   if ( extraBytes )
   {
      transfer_params_t transfer_params;

      transfer_params.dst_id        = dstId;
      transfer_params.dst_id_offset = 0;
      transfer_params.dst_pitch     = pitch;
      transfer_params.dst_x         = 0;
      transfer_params.dst_y         = fullLines + dstOffset / pitch;
      transfer_params.src_id        = srcId;
      transfer_params.src_id_offset = srcOffset;
      transfer_params.src_pitch     = pitch;
      transfer_params.src_x         = 0;
      transfer_params.src_y         = fullLines;
      transfer_params.width         = extraBytes / 2;
      transfer_params.height        = 1;
      transfer_params.bpp           = 2;
      transfer_params.fifo_ptr      = &_RGLState.fifo;

      TransferDataVidToVid(&transfer_params);
   }
}

static void _RGLPlatformBufferObjectSetData( jsBufferObject* bufferObject, GLintptr offset, GLsizeiptr size, const GLvoid *data, GLboolean tryImmediateCopy )
{
   RGLBufferObject *jsBuffer = ( RGLBufferObject * )bufferObject->platformBufferObject;

    if ( size == bufferObject->size && tryImmediateCopy )
        memcpy( gmmIdToAddress( jsBuffer->bufferId ) + offset, data, size );
    else
        if ( size >= bufferObject->size )
        {
            jsBuffer->bufferSize = _RGLPad( size, _RGL_BUFFER_OBJECT_BLOCK_SIZE );
            _RGLAllocateBuffer( bufferObject );

            switch ( jsBuffer->pool )
            {
                case SURFACE_POOL_NONE:
                    _RGLSetError( GL_OUT_OF_MEMORY );
                    return;
                default:
                    memcpy( gmmIdToAddress( jsBuffer->bufferId ), data, size );
                    break;
            }
        }
        else
        {
            if ( tryImmediateCopy )
                memcpy( gmmIdToAddress( jsBuffer->bufferId ) + offset, data, size );
            else
	    {
               unsigned int dstId = jsBuffer->bufferId;
	       unsigned int pitch = jsBuffer->pitch;
	       const char *src = (const char *)data;

	       switch ( bufferObject->usage )
	       {
                  case GL_STREAM_DRAW:
		  case GL_STREAM_READ:
		  case GL_STREAM_COPY:
		  case GL_DYNAMIC_DRAW:
		  case GL_DYNAMIC_READ:
		  case GL_DYNAMIC_COPY:
                     {
                        GLuint id = gmmAlloc(0, size);

		        memcpy( gmmIdToAddress(id), src, size );
		        _RGLMemcpy( dstId, offset, pitch, id, size );

		        gmmFree( id );
		     }
		     break;
		  default:
                     cellGcmSetInvalidateVertexCache( &_RGLState.fifo);
		     _RGLFifoFinish( &_RGLState.fifo );
		     memcpy( gmmIdToAddress( dstId ) + offset, src, size );
		     break;
	       };
	    }
        }

   ((RGLDriver *)_CurrentDevice->rasterDriver)->invalidateVertexCache = GL_TRUE;
}

static GLboolean _RGLPlatformCreateBufferObject( jsBufferObject* bufferObject )
{
   RGLBufferObject *jsBuffer = ( RGLBufferObject * )bufferObject->platformBufferObject;

   jsBuffer->pool = SURFACE_POOL_NONE;
   jsBuffer->bufferId = GMM_ERROR;
   jsBuffer->mapCount = 0;
   jsBuffer->mapAccess = GL_NONE;
   jsBuffer->bufferSize = _RGLPad( bufferObject->size, _RGL_BUFFER_OBJECT_BLOCK_SIZE );

   _RGLAllocateBuffer(bufferObject);

   return jsBuffer->bufferId != GMM_ERROR;
}

GLAPI void APIENTRY glBufferData( GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage )
{
   PSGLcontext *LContext = _CurrentContext;

   GLuint name = 0;
   switch(target)
   {
      case GL_ARRAY_BUFFER:
         name = LContext->ArrayBuffer;
	 break;
      case GL_ELEMENT_ARRAY_BUFFER:
	 break;
      case GL_PIXEL_PACK_BUFFER_ARB:
	 break;
      case GL_PIXEL_UNPACK_BUFFER_ARB:
	 name = LContext->PixelUnpackBuffer;
	 break;
      case GL_TEXTURE_REFERENCE_BUFFER_SCE:
	 name = LContext->TextureBuffer;
	 break;
      default:
	 _RGLSetError(GL_INVALID_ENUM);
	 return;
   }

   jsBufferObject* bufferObject = (jsBufferObject *)LContext->bufferObjectNameSpace.data[name];

   if(bufferObject->refCount > 1)
   {
      _RGLTexNameSpaceDeleteNames( &LContext->bufferObjectNameSpace, 1, &name );
      _RGLTexNameSpaceCreateNameLazy( &LContext->bufferObjectNameSpace, name );
      bufferObject = (jsBufferObject *)LContext->bufferObjectNameSpace.data[name];

   }

   if (bufferObject->size > 0)
      _RGLPlatformDestroyBufferObject( bufferObject );

   bufferObject->usage = usage;
   bufferObject->size = size;
   bufferObject->width = 0;
   bufferObject->height = 0;
   bufferObject->internalFormat = GL_NONE;

   if (size > 0)
   {
      GLboolean created = _RGLPlatformCreateBufferObject( bufferObject );
      if ( !created )
      {
         _RGLSetError( GL_OUT_OF_MEMORY );
	 return;
      }
      if ( data )
         _RGLPlatformBufferObjectSetData( bufferObject, 0, size, data, GL_TRUE );
   }

   GLuint attrSetCount = bufferObject->attribSets.getCount();
   if ( attrSetCount == 0 )
      return;

   for(unsigned int i = 0; i < attrSetCount; ++i)
   {
      jsAttribSet *attribSet = bufferObject->attribSets[i];
      attribSet->dirty = GL_TRUE;
   }

   LContext->attribSetDirty = GL_TRUE;
}

static GLvoid _RGLPlatformBufferObjectCopyData( jsBufferObject* bufferObjectDst,
    jsBufferObject* bufferObjectSrc )
{

    RGLBufferObject* dst = ( RGLBufferObject* )bufferObjectDst->platformBufferObject;
    RGLBufferObject* src = ( RGLBufferObject* )bufferObjectSrc->platformBufferObject;

    switch ( dst->pool )
    {
        case SURFACE_POOL_LINEAR:
            _RGLMemcpy( dst->bufferId, 0, dst->pitch, src->bufferId, src->bufferSize );
            break;
        case SURFACE_POOL_SYSTEM:
	    cellGcmSetInvalidateVertexCache( &_RGLState.fifo);
	    _RGLFifoFinish( &_RGLState.fifo );
            memcpy( gmmIdToAddress( dst->bufferId ), gmmIdToAddress( src->bufferId ),
            src->bufferSize );
            break;
    }

    ((RGLDriver *)_CurrentDevice->rasterDriver)->invalidateVertexCache = GL_TRUE;
}

GLAPI void APIENTRY glBufferSubData( GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data )
{
    PSGLcontext *LContext = _CurrentContext;
    GLuint name = 0;
    switch ( target )
    {
        case GL_ARRAY_BUFFER:
            name = LContext->ArrayBuffer;
            break;
        case GL_ELEMENT_ARRAY_BUFFER:
            break;
        case GL_PIXEL_PACK_BUFFER_ARB:
            break;
        case GL_PIXEL_UNPACK_BUFFER_ARB:
            name = LContext->PixelUnpackBuffer;
            break;
        case GL_TEXTURE_REFERENCE_BUFFER_SCE:
            name = LContext->TextureBuffer;
            break;
        default:
            _RGLSetError( GL_INVALID_ENUM );
            return;
    }

    jsBufferObject* bufferObject = (jsBufferObject *)LContext->bufferObjectNameSpace.data[name];



    if ( bufferObject->refCount > 1 )
    {
        jsBufferObject* oldBufferObject = bufferObject;

        _RGLTexNameSpaceDeleteNames( &LContext->bufferObjectNameSpace, 1, &name );
        _RGLTexNameSpaceCreateNameLazy( &LContext->bufferObjectNameSpace, name );

        bufferObject = (jsBufferObject *)LContext->bufferObjectNameSpace.data[name];
        bufferObject->size = oldBufferObject->size;
        bufferObject->usage = oldBufferObject->usage;

        GLboolean created = _RGLPlatformCreateBufferObject( bufferObject );
        if ( !created )
        {
            _RGLSetError( GL_OUT_OF_MEMORY );
            return;
        }
        _RGLPlatformBufferObjectCopyData( bufferObject, oldBufferObject );
    }
    _RGLPlatformBufferObjectSetData( bufferObject, offset, size, data, GL_FALSE );
}

static void _RGLFramebufferGetAttachmentTexture(
    const jsFramebufferAttachment* attachment,
    jsTexture** texture)
{
   PSGLcontext* LContext = _CurrentContext;

   switch ( attachment->type )
   {
      case FRAMEBUFFER_ATTACHMENT_NONE:
         *texture = NULL;
	 break;
      case FRAMEBUFFER_ATTACHMENT_RENDERBUFFER:
	 break;
      case FRAMEBUFFER_ATTACHMENT_TEXTURE:
	 *texture = _RGLTexNameSpaceIsName( &LContext->textureNameSpace, attachment->name ) ? ( jsTexture* )LContext->textureNameSpace.data[attachment->name] : NULL;
	 break;
      default:
	 *texture = NULL;
	 break;
   }
}

static GLboolean _RGLTextureIsValid(const jsTexture* texture)
{
   if(texture->imageCount < 1)
      return GL_FALSE;
   if ( !texture->image )
      return GL_FALSE;

   const jsImage* image = texture->image;

   int width = image->width;
   int height = image->height;

   GLenum format = image->format;
   GLenum type = image->type;
   GLenum internalFormat = image->internalFormat;

   if (( internalFormat == 0 ) || ( format == 0 ) || ( type == 0 ) )
      return GL_FALSE;

   if(!image->isSet)
      return GL_FALSE;
   if(width != image->width)
      return GL_FALSE;
   if(height != image->height)
      return GL_FALSE;
   if(format != image->format)
      return GL_FALSE;
   if(type != image->type)
      return GL_FALSE;
   if(internalFormat != image->internalFormat)
      return GL_FALSE;

   return GL_TRUE;
}

static GLenum _RGLPlatformFramebufferCheckStatus(jsFramebuffer* framebuffer)
{
   GLuint nBuffers = 0;

   jsImage* image[MAX_COLOR_ATTACHMENTS + 2] = {0};

   GLuint colorFormat = 0;
   for ( int i = 0; i < MAX_COLOR_ATTACHMENTS; ++i )
   {
      jsTexture* colorTexture = NULL;
      _RGLFramebufferGetAttachmentTexture(&framebuffer->color[i], &colorTexture);

      if ( colorTexture != NULL )
      {
         if ( !_RGLTextureIsValid( colorTexture ) )
	 {
            RARCH_ERR("Framebuffer color attachment texture is not complete.\n");
	    return GL_FRAMEBUFFER_UNSUPPORTED_OES;
	 }

         image[nBuffers] = colorTexture->image;

	 if ( colorFormat && colorFormat != image[nBuffers]->internalFormat )
	 {
            RARCH_ERR("Framebuffer attachments have inconsistent color formats.\n" );
	    return GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES;
	 }
	 colorFormat = image[nBuffers]->internalFormat;

	 ++nBuffers;
      }
   }

   if ( nBuffers && colorFormat != RGL_ARGB8)
   {
      RARCH_ERR("Color attachment to framebuffer must be a supported drawable format (GL_ARGB_SCE)\n");
      return GL_FRAMEBUFFER_UNSUPPORTED_OES;
   }

   if ( nBuffers == 0 )
      return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES;

   for ( GLuint i = 0; i < nBuffers; ++i )
      for ( GLuint j = i + 1; j < nBuffers; ++j )
         if ( image[i] == image[j] )
            return GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_OES;

   return GL_FRAMEBUFFER_COMPLETE_OES;
}

enum _RGLTextureStrategy
{
   TEXTURE_STRATEGY_END,
   TEXTURE_STRATEGY_UNTILED_ALLOC,
   TEXTURE_STRATEGY_UNTILED_CLEAR,
};

static enum _RGLTextureStrategy linearGPUStrategy[] =
{
   TEXTURE_STRATEGY_UNTILED_ALLOC,
   TEXTURE_STRATEGY_UNTILED_CLEAR,
   TEXTURE_STRATEGY_UNTILED_ALLOC,
   TEXTURE_STRATEGY_END,
};

static void _RGLImageAllocCPUStorage( jsImage *image )
{
   if((image->storageSize > image->mallocStorageSize) || (!image->mallocData))
   {
      if(image->mallocData)
         free( image->mallocData );

      image->mallocData = ( char * )malloc( image->storageSize + 128 );
      image->mallocStorageSize = image->storageSize;
   }

   intptr_t x = (intptr_t)image->mallocData;
   x = ( x + 128 - 1 ) / 128 * 128;
   image->data = (char*)x;

}

static inline int _RGLGetComponentCount( GLenum format )
{
    switch ( format )
    {
#define DECLARE_FORMAT(FORMAT,COUNT) \
		case FORMAT: \
			return COUNT;
            DECLARE_FORMATS
#undef DECLARE_FORMAT
        default:
            return 0;
    }
}

typedef void( GetComponentsFunction_t )( const unsigned char *bytes, GLfloat *values, int count );
typedef void( PutComponentsFunction_t )( unsigned char *bytes, GLfloat *values, int count );
typedef void( ColorConvertFunction_t )( jsColorRGBAf *color, GLfloat *values );


#define DECLARE_UNPACKED_TYPE(TYPE) \
static void _RGLGetComponents_##TYPE(const unsigned char *bytes, GLfloat *values, int count) \
{ \
	int i; \
	for (i=0;i<count;++i) \
	{ \
		const type_##TYPE data=*(const type_##TYPE *)bytes; \
		values[i]=_RGLFloatFrom_##TYPE(data); \
		bytes+=sizeof(type_##TYPE); \
	} \
}
DECLARE_UNPACKED_TYPES
#undef DECLARE_UNPACKED_TYPE

#define DECLARE_PACKED_TYPE(REALTYPE,TYPE,N,S1,S2,S3,S4,REV) \
static void _RGLGetComponents_##TYPE(const unsigned char *bytes, GLfloat *values, int count) \
{ \
	const type_##REALTYPE data=*(const type_##REALTYPE *)bytes; \
	GET_BITS(values[INDEX##REV(N,0)],data,S2+S3+S4,S1); \
	GET_BITS(values[INDEX##REV(N,1)],data,S3+S4,S2); \
	GET_BITS(values[INDEX##REV(N,2)],data,S4,S3); \
	GET_BITS(values[INDEX##REV(N,3)],data,0,S4); \
}
DECLARE_PACKED_TYPES
#undef DECLARE_PACKED_TYPE

#define DECLARE_UNPACKED_TYPE(TYPE) \
static void _RGLPutComponents_##TYPE(unsigned char *bytes, GLfloat *values, int count) \
{ \
	int i; \
	for (i=0;i<count;++i) \
	{ \
		type_##TYPE *data=(type_##TYPE *)bytes; \
		*data=_RGLFloatTo_##TYPE(values[i]); \
		bytes+=sizeof(type_##TYPE); \
	} \
}
DECLARE_UNPACKED_TYPES
#undef DECLARE_UNPACKED_TYPE

#define DECLARE_PACKED_TYPE(REALTYPE,TYPE,N,S1,S2,S3,S4,REV) \
static void _RGLPutComponents_##TYPE(unsigned char *bytes, GLfloat *values, int count) \
{ \
	type_##REALTYPE *data=(type_##REALTYPE *)bytes; \
	*data=0; \
	PUT_BITS(values[INDEX##REV(N,0)],*data,S2+S3+S4,S1); \
	PUT_BITS(values[INDEX##REV(N,1)],*data,S3+S4,S2); \
	PUT_BITS(values[INDEX##REV(N,2)],*data,S4,S3); \
	PUT_BITS(values[INDEX##REV(N,3)],*data,0,S4); \
}
DECLARE_PACKED_TYPES
#undef DECLARE_PACKED_TYPE

static inline GetComponentsFunction_t *_RGLFindGetComponentsFunction( GLenum type )
{
    switch ( type )
    {

#define DECLARE_UNPACKED_TYPE(TYPE) \
		case TYPE: \
			return &_RGLGetComponents_##TYPE;
            DECLARE_UNPACKED_TYPES
#undef DECLARE_UNPACKED_TYPE

#define DECLARE_PACKED_TYPE(REALTYPE,TYPE,N,S1,S2,S3,S4,REV) \
		case TYPE: \
			return &_RGLGetComponents_##TYPE;
            DECLARE_PACKED_TYPES
#undef DECLARE_PACKED_TYPE

        default:
            return NULL;
    }
}

static inline PutComponentsFunction_t *_RGLFindPutComponentsFunction( GLenum type )
{
    switch ( type )
    {
#define DECLARE_UNPACKED_TYPE(TYPE) \
		case TYPE: \
			return &_RGLPutComponents_##TYPE;
            DECLARE_UNPACKED_TYPES
#undef DECLARE_UNPACKED_TYPE

#define DECLARE_PACKED_TYPE(REALTYPE,TYPE,N,S1,S2,S3,S4,REV) \
		case TYPE: \
			return &_RGLPutComponents_##TYPE;
            DECLARE_PACKED_TYPES
#undef DECLARE_PACKED_TYPE

        default:
            return NULL;
    }
}

static void _RGLValuesToColor_GL_RGB( jsColorRGBAf *c, GLfloat *v ) { c->R = v[0]; c->G = v[1]; c->B = v[2]; c->A = 1.f; }
static void _RGLValuesToColor_GL_BGR( jsColorRGBAf *c, GLfloat *v ) { c->B = v[0]; c->G = v[1]; c->R = v[2]; c->A = 1.f; }
static void _RGLValuesToColor_GL_RGBA( jsColorRGBAf *c, GLfloat *v ) { c->R = v[0]; c->G = v[1]; c->B = v[2]; c->A = v[3]; }
static void _RGLValuesToColor_GL_BGRA( jsColorRGBAf *c, GLfloat *v ) { c->B = v[0]; c->G = v[1]; c->R = v[2]; c->A = v[3]; }
static void _RGLValuesToColor_GL_ABGR( jsColorRGBAf *c, GLfloat *v ) { c->A = v[0]; c->B = v[1]; c->G = v[2]; c->R = v[3]; }
static void _RGLValuesToColor_GL_ARGB_SCE( jsColorRGBAf *c, GLfloat *v ) { c->A = v[0]; c->R = v[1]; c->G = v[2]; c->B = v[3]; }
static void _RGLValuesToColor_GL_RED( jsColorRGBAf *c, GLfloat *v ) { c->R = v[0]; c->G = 0.f; c->B = 0.f; c->A = 1.f; }
static void _RGLValuesToColor_GL_GREEN( jsColorRGBAf *c, GLfloat *v ) { c->R = 0.f; c->G = v[0]; c->B = 0.f; c->A = 1.f; }
static void _RGLValuesToColor_GL_BLUE( jsColorRGBAf *c, GLfloat *v ) { c->R = 0.f; c->G = 0.f; c->B = v[0]; c->A = 1.f; }
static void _RGLValuesToColor_GL_ALPHA( jsColorRGBAf *c, GLfloat *v ) { c->R = 0.f; c->G = 0.f; c->B = 0.f; c->A = v[0]; }

static void _RGLColorToValues_GL_RGB( jsColorRGBAf *c, GLfloat *v ) { v[0] = c->R; v[1] = c->G; v[2] = c->B; }
static void _RGLColorToValues_GL_BGR( jsColorRGBAf *c, GLfloat *v ) { v[0] = c->B; v[1] = c->G; v[2] = c->R; }
static void _RGLColorToValues_GL_RGBA( jsColorRGBAf *c, GLfloat *v ) { v[0] = c->R; v[1] = c->G; v[2] = c->B; v[3] = c->A; }
static void _RGLColorToValues_GL_BGRA( jsColorRGBAf *c, GLfloat *v ) { v[0] = c->B; v[1] = c->G; v[2] = c->R; v[3] = c->A; }
static void _RGLColorToValues_GL_ABGR( jsColorRGBAf *c, GLfloat *v ) { v[0] = c->A; v[1] = c->B; v[2] = c->G; v[3] = c->R; }
static void _RGLColorToValues_GL_ARGB_SCE( jsColorRGBAf *c, GLfloat *v ) { v[0] = c->A; v[1] = c->R; v[2] = c->G; v[3] = c->B; }
static void _RGLColorToValues_GL_RED( jsColorRGBAf *c, GLfloat *v ) { v[0] = c->R; }
static void _RGLColorToValues_GL_GREEN( jsColorRGBAf *c, GLfloat *v ) { v[0] = c->G; }
static void _RGLColorToValues_GL_BLUE( jsColorRGBAf *c, GLfloat *v ) { v[0] = c->B; }
static void _RGLColorToValues_GL_ALPHA( jsColorRGBAf *c, GLfloat *v ) { v[0] = c->A; }

static inline ColorConvertFunction_t *_RGLFindValuesToColorFunction( GLenum format )
{
    switch ( format )
    {
#define DECLARE_FORMAT(FORMAT,COUNT) \
		case FORMAT: \
			return &_RGLValuesToColor_##FORMAT;
            DECLARE_FORMATS
#undef DECLARE_FORMAT
        default:
            return NULL;
    }
}

static inline ColorConvertFunction_t *_RGLFindColorToValuesFunction( GLenum format )
{
    switch ( format )
    {
#define DECLARE_FORMAT(FORMAT,COUNT) \
		case FORMAT: \
			return &_RGLColorToValues_##FORMAT;
            DECLARE_FORMATS
#undef DECLARE_FORMAT
        default:
            return NULL;
    }
}

static void _RGLRasterToImage(const jsRaster* raster, jsImage* image)
{
   const int srcComponents = _RGLGetComponentCount( raster->format );
   const int dstComponents = _RGLGetComponentCount( image->format );
   GetComponentsFunction_t* getComponents = _RGLFindGetComponentsFunction( raster->type );
   PutComponentsFunction_t* putComponents = _RGLFindPutComponentsFunction( image->type );
   ColorConvertFunction_t* valuesToColor = _RGLFindValuesToColorFunction( raster->format );
   ColorConvertFunction_t* colorToValues = _RGLFindColorToValuesFunction( image->format );

   for ( int j = 0; j < raster->height; ++j )
   {
      const unsigned char *src = ( const unsigned char * )raster->data + j * raster->ystride;
      unsigned char *dst = ( unsigned char * )image->data + (j) * image->ystride;

      for ( int k = 0; k < raster->width; ++k )
      {
         GLfloat values[4];
	 jsColorRGBAf color;
	 getComponents( src, values, srcComponents );
	 valuesToColor( &color, values );
	 colorToValues( &color, values );

	 if (image->type!=GL_FLOAT && image->type!=GL_HALF_FLOAT_ARB)
	 {
            values[0]= MAX( MIN( values[0], 1.f ), 0.f );
	    values[1]= MAX( MIN( values[1], 1.f ), 0.f );
	    values[2]= MAX( MIN( values[2], 1.f ), 0.f );
	    values[3]= MAX( MIN( values[3], 1.f ), 0.f );
	 }

	 putComponents( dst, values, dstComponents );

	 src += raster->xstride;
	 dst += image->xstride;
      }
   }
}

static void _RGLPlatformCopyGPUTexture( jsTexture* texture )
{
   RGLTexture *gcmTexture = ( RGLTexture * )texture->platformTexture;

   if ( gcmTexture->gpuAddressId == GMM_ERROR )
      return;

   RGLTextureLayout *layout = &gcmTexture->gpuLayout;

   jsImage* image = texture->image;

   if ( image->dataState == IMAGE_DATASTATE_GPU )
   {
      _RGLImageAllocCPUStorage( image );

      cellGcmSetInvalidateVertexCache( &_RGLState.fifo);
      _RGLFifoFinish( &_RGLState.fifo );

      char* gpuData = gmmIdToAddress(gcmTexture->gpuAddressId) + gcmTexture->gpuAddressIdOffset;

      jsRaster raster = 
      {
         format : image->format,
	 type   : image->type,
	 width  : image->width,
	 height : image->height,
	 xstride : image->xstride,
	 ystride : layout->pitch,
	 data    : gpuData
      };

      _RGLRasterToImage( &raster, image);

      image->dataState = IMAGE_DATASTATE_HOST;
   }
}

static void _RGLPlatformFreeGcmTexture(jsTexture* texture)
{
    RGLTexture *gcmTexture = (RGLTexture *)texture->platformTexture;

    switch (gcmTexture->pool)
    {
        case SURFACE_POOL_LINEAR:
        case SURFACE_POOL_SYSTEM:
            gmmFree(gcmTexture->gpuAddressId);
        case SURFACE_POOL_NONE:
            break;
        default:
            break;
    }

    gcmTexture->gpuAddressId = GMM_ERROR;
    gcmTexture->gpuAddressIdOffset = 0;
    gcmTexture->gpuSize = 0;
}

void _RGLPlatformDropTexture( jsTexture *texture )
{
   RGLTexture * gcmTexture = (RGLTexture *)texture->platformTexture;

   if(gcmTexture->pbo != NULL)
   {
      _RGLPlatformCopyGPUTexture(texture);
      _RGLFreeBufferObject(gcmTexture->pbo);
      gcmTexture->pbo = NULL;
      gcmTexture->gpuAddressId = GMM_ERROR;
      gcmTexture->gpuAddressIdOffset = 0;
      gcmTexture->pool = SURFACE_POOL_NONE;
      gcmTexture->gpuSize = 0;
   }

   if(gcmTexture->pool != SURFACE_POOL_NONE)
   {
      _RGLPlatformCopyGPUTexture( texture );
      _RGLPlatformFreeGcmTexture( texture );
   }

   gcmTexture->pool = SURFACE_POOL_NONE;
   gcmTexture->gpuAddressId = GMM_ERROR;
   gcmTexture->gpuAddressIdOffset = 0;
   gcmTexture->gpuSize = 0;
   texture->revalidate |= TEXTURE_REVALIDATE_IMAGES;
   _RGLTextureTouchFBOs(texture);
}

int _RGLGetPixelSize( GLenum format, GLenum type )
{
    int componentSize;

    switch(type)
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

   return _RGLGetComponentCount( format )*componentSize;
}

static void _RGLPlatformChooseGPUFormatAndLayout(
    const jsTexture* texture,
    GLboolean forceLinear,
    GLuint pitch,
    RGLTextureLayout* newLayout )
{
   jsImage *image = texture->image;

   newLayout->baseWidth = image->width;
   newLayout->baseHeight = image->height;
   newLayout->internalFormat = (RGLEnum)image->internalFormat;
   newLayout->pixelBits = _RGLPlatformGetBitsPerPixel( newLayout->internalFormat );
   newLayout->pitch = pitch ? pitch : _RGLPad( _RGLGetPixelSize( texture->image->format, texture->image->type )*texture->image->width, 64);
}

void _RGLPlatformDropUnboundTextures(GLenum pool)
{
   PSGLcontext* LContext = _CurrentContext;
   GLuint i, j;

   for (i = 0; i < LContext->textureNameSpace.capacity; ++i)
   {
      GLboolean bound = GL_FALSE;
      jsTexture *texture = ( jsTexture * )LContext->textureNameSpace.data[i];
      if(!texture || (texture->referenceBuffer != 0))
         continue;

      for (j = 0; j < MAX_VERTEX_TEXTURE_IMAGE_UNITS; ++j)
      {
         if (LContext->VertexTextureImages[j] == texture)
	 {
            bound = GL_TRUE;
	    break;
	 }
      }

      for ( j = 0; j < MAX_TEXTURE_IMAGE_UNITS; ++j)
      {
         jsTextureImageUnit *tu = LContext->TextureImageUnits + j;
	 if (tu->bound2D == i)
	 {
            bound = GL_TRUE;
	    break;
	 }
      }

      if ( bound )
         continue;

      RGLTexture *gcmTexture = ( RGLTexture * )texture->platformTexture;
      if ( gcmTexture->pbo != NULL && gcmTexture->pbo->refCount > 1 )
         continue;

      if ( pool != SURFACE_POOL_NONE && pool != gcmTexture->pool )
         continue;

      _RGLPlatformDropTexture( texture );
   }
}

static void _RGLPlatformReallocateGcmTexture( jsTexture* texture )
{
    RGLTexture *gcmTexture = ( RGLTexture * )texture->platformTexture;

    enum _RGLTextureStrategy *step = linearGPUStrategy;

    GLuint size = 0;
    GLuint id = GMM_ERROR;

    const RGLTextureLayout currentLayout = gcmTexture->gpuLayout;
    const GLuint currentSize = gcmTexture->gpuSize;

    // process strategy
    GLboolean done = GL_FALSE;
    while ( !done )
    {
        RGLTextureLayout newLayout;

        switch ( *step++ )
        {
            case TEXTURE_STRATEGY_UNTILED_ALLOC:
                _RGLPlatformChooseGPUFormatAndLayout( texture, GL_TRUE, 0, &newLayout );
		size = _RGLPad( newLayout.baseHeight * newLayout.pitch, 1);

                if ( gcmTexture->pool == SURFACE_POOL_LINEAR )
                {
                    if ( currentSize >= size && newLayout.pitch == currentLayout.pitch )
                    {
                        gcmTexture->gpuLayout = newLayout;
                        done = GL_TRUE;
                    }
                    else
                        _RGLPlatformDropTexture( texture );
                }

                if ( !done )
                {
		    id = gmmAlloc(0, size);
                    if ( id != GMM_ERROR )
                    {
                        if ( gcmTexture->pool != SURFACE_POOL_NONE )
                            _RGLPlatformDropTexture( texture );

                        gcmTexture->pool = SURFACE_POOL_LINEAR;
                        gcmTexture->gpuAddressId = id;
                        gcmTexture->gpuAddressIdOffset = 0;
                        gcmTexture->gpuSize = size;
                        gcmTexture->gpuLayout = newLayout;

                        done = GL_TRUE;
                    }
                }
                break;
            case TEXTURE_STRATEGY_UNTILED_CLEAR:
                _RGLPlatformDropUnboundTextures(SURFACE_POOL_LINEAR);
                break;
            case TEXTURE_STRATEGY_END:
                _RGLSetError( GL_OUT_OF_MEMORY );
                done = GL_TRUE;
                break;
            default:
	    	break;
        }
    }
    _RGLTextureTouchFBOs( texture );
}

static void _RGLImageFreeCPUStorage( jsImage *image )
{
   if (!image->mallocData)
      return;

   if (image->mallocData != NULL)
      free( image->mallocData );

   image->mallocStorageSize = 0;
   image->data = NULL;
   image->mallocData = NULL;
   image->dataState &= ~IMAGE_DATASTATE_HOST;
}

static void _RGLPlatformValidateTextureResources( jsTexture *texture )
{
    if ( RGL_UNLIKELY( !_RGLTextureIsValid( texture ) ) )
    {
        texture->isComplete = GL_FALSE;
        return;
    }
    texture->isComplete = GL_TRUE;

    if ( texture->revalidate & TEXTURE_REVALIDATE_IMAGES ||
    texture->revalidate & TEXTURE_REVALIDATE_LAYOUT )
    {
	    _RGLPlatformReallocateGcmTexture( texture );
	    RGLTexture *gcmTexture = ( RGLTexture * )texture->platformTexture;
	    RGLTextureLayout *layout = &gcmTexture->gpuLayout;

	    const GLuint pixelBytes = layout->pixelBits / 8;

	    RGLSurface src;
	    src.source             = SURFACE_SOURCE_TEMPORARY;
	    src.width              = 0;
	    src.height             = 0;
	    src.bpp                = pixelBytes;
	    src.pitch              = 0;
	    src.format             = layout->internalFormat;
	    src.pool               = SURFACE_POOL_LINEAR;
	    src.ppuData            = NULL;
	    src.dataId             = GMM_ERROR;
	    src.dataIdOffset       = 0;

	    RGLSurface dst;
	    dst.source             = SURFACE_SOURCE_TEXTURE;
	    dst.width              = 0;
	    dst.height             = 0;
	    dst.bpp                = pixelBytes;
	    dst.pitch              = layout->pitch;
	    dst.format             = layout->internalFormat;
	    dst.pool               = SURFACE_POOL_SYSTEM;
	    dst.ppuData            = NULL;
	    dst.dataId             = GMM_ERROR;
	    dst.dataIdOffset       = 0;

	    GLuint bounceBufferId = GMM_ERROR;

	    jsImage *image = texture->image;

	    if(image->dataState == IMAGE_DATASTATE_HOST)
	    {
		    src.ppuData = image->data;

		    if ( bounceBufferId == GMM_ERROR)
			    bounceBufferId = gmmAlloc(0, gcmTexture->gpuSize);

		    if ( bounceBufferId != GMM_ERROR )
		    {
			    src.dataId = bounceBufferId;
			    src.dataIdOffset = 0;

			    memcpy( gmmIdToAddress( src.dataId ), image->data, 
					    image->storageSize );
		    }

		    src.width = image->width;
		    src.height = image->height;
		    src.pitch = pixelBytes * src.width;

		    dst.width = src.width;
		    dst.height = image->height;
		    dst.dataId = gcmTexture->gpuAddressId;
		    dst.dataIdOffset = gcmTexture->gpuAddressIdOffset;

		    transfer_params_t transfer_params;

		    transfer_params.dst_id        = dst.dataId;
		    transfer_params.dst_id_offset = dst.dataIdOffset;
		    transfer_params.dst_pitch     = dst.pitch ? dst.pitch : (dst.bpp * dst.width);
		    transfer_params.dst_x         = 0;
		    transfer_params.dst_y         = 0;
		    transfer_params.src_id        = src.dataId;
		    transfer_params.src_id_offset = src.dataIdOffset;
		    transfer_params.src_pitch     = src.pitch ? src.pitch : (src.bpp * src.width);
		    transfer_params.src_x         = 0;
		    transfer_params.src_y         = 0;
		    transfer_params.width         = src.width;
		    transfer_params.height        = src.height;
		    transfer_params.bpp           = src.bpp;
		    transfer_params.fifo_ptr      = &_RGLState.fifo;

		    TransferDataVidToVid(&transfer_params);

		    _RGLImageFreeCPUStorage( image );
		    image->dataState |= IMAGE_DATASTATE_GPU;
	    }

	    if ( bounceBufferId != GMM_ERROR )
		    gmmFree( bounceBufferId );

	    cellGcmSetInvalidateTextureCacheInline( &_RGLState.fifo, CELL_GCM_INVALIDATE_TEXTURE);
    }

    RGLTexture *platformTexture = ( RGLTexture * )texture->platformTexture;
    RGLTextureLayout *layout = &platformTexture->gpuLayout;

    GLuint minFilter = texture->minFilter;
    GLuint magFilter = texture->magFilter;

    platformTexture->gcmMethods.filter.min = _RGLMapMinTextureFilter( minFilter );
    platformTexture->gcmMethods.filter.mag = _RGLMapMagTextureFilter( magFilter );
    platformTexture->gcmMethods.filter.bias = ( GLint )(( -.26f ) * 256.0f );

    GLuint gamma = 0;
    GLuint remap = texture->gammaRemap;
    gamma |= (remap & RGL_GAMMA_REMAP_RED_BIT) ? CELL_GCM_TEXTURE_GAMMA_R : 0;
    gamma |= (remap & RGL_GAMMA_REMAP_GREEN_BIT) ? CELL_GCM_TEXTURE_GAMMA_G : 0;
    gamma |= (remap & RGL_GAMMA_REMAP_BLUE_BIT) ? CELL_GCM_TEXTURE_GAMMA_B : 0;
    gamma |= (remap & RGL_GAMMA_REMAP_ALPHA_BIT) ? CELL_GCM_TEXTURE_GAMMA_A : 0;

    platformTexture->gcmMethods.address.gamma = gamma;

    GLuint internalFormat = layout->internalFormat;

    _RGLMapTextureFormat( internalFormat,
       platformTexture->gcmTexture.format, platformTexture->gcmTexture.remap );

    if ( layout->pitch )
        platformTexture->gcmTexture.format += 0x20;

    platformTexture->gcmTexture.width = layout->baseWidth;
    platformTexture->gcmTexture.height = layout->baseHeight;
    platformTexture->gcmTexture.depth = 1;
    platformTexture->gcmTexture.pitch = layout->pitch;
    platformTexture->gcmTexture.mipmap = 1;
    platformTexture->gcmTexture.cubemap = CELL_GCM_FALSE;
    platformTexture->gcmTexture.dimension = CELL_GCM_TEXTURE_DIMENSION_2;

    if(gmmIdIsMain(platformTexture->gpuAddressId))
        platformTexture->gcmTexture.location = CELL_GCM_LOCATION_MAIN;
    else
        platformTexture->gcmTexture.location = CELL_GCM_LOCATION_LOCAL;

    texture->revalidate = 0;
}

static void jsPlatformFramebuffer_validate( jsPlatformFramebuffer * fb, PSGLcontext *LContext )
{
   fb->complete = ( _RGLPlatformFramebufferCheckStatus(fb) == GL_FRAMEBUFFER_COMPLETE_OES );

   if(!fb->complete)
      return;

   GLuint width = CELL_GCM_MAX_RT_DIMENSION;
   GLuint height = CELL_GCM_MAX_RT_DIMENSION;

   fb->rt.colorBufferCount = 0;
   fb->rt.colorFormat = RGL_NONE;
   fb->colorBufferMask = 0x0;
   GLuint defaultPitch = 0;
   GLuint defaultId = GMM_ERROR;
   GLuint defaultIdOffset = 0;

   for(int i = 0; i < RGL_SETRENDERTARGET_MAXCOUNT; ++i)
   {
      jsTexture* colorTexture = NULL;
      _RGLFramebufferGetAttachmentTexture(&fb->color[i], &colorTexture);

      if(colorTexture == NULL)
         continue;

      RGLTexture* nvTexture = ( RGLTexture * )colorTexture->platformTexture;

      if ( !colorTexture->isRenderTarget )
      {
         colorTexture->isRenderTarget = GL_TRUE;
	 colorTexture->revalidate |= TEXTURE_REVALIDATE_LAYOUT;
      }
      _RGLPlatformValidateTextureResources( colorTexture );
      colorTexture->image->dataState = IMAGE_DATASTATE_GPU;

      fb->rt.colorId[i] = nvTexture->gpuAddressId;
      fb->rt.colorIdOffset[i] = nvTexture->gpuAddressIdOffset;
      fb->rt.colorPitch[i] = nvTexture->gpuLayout.pitch ? nvTexture->gpuLayout.pitch : nvTexture->gpuLayout.pixelBits * nvTexture->gpuLayout.baseWidth / 8;
      fb->colorBufferMask |= 1 << i;

      width = MIN( width, nvTexture->gpuLayout.baseWidth );
      height = MIN( height, nvTexture->gpuLayout.baseHeight );
      fb->rt.colorFormat = nvTexture->gpuLayout.internalFormat;
      fb->rt.colorBufferCount = i + 1;
      defaultId = fb->rt.colorId[i];
      defaultIdOffset = fb->rt.colorIdOffset[i];
      defaultPitch = fb->rt.colorPitch[i];

      if ( !( fb->colorBufferMask & ( 1 << i ) ) )
      {
         fb->rt.colorId[i] = defaultId;
	 fb->rt.colorIdOffset[i] = defaultIdOffset;
	 fb->rt.colorPitch[i] = defaultPitch;
      }
   }

    fb->rt.width = width;
    fb->rt.height = height;

    fb->rt.yInverted = CELL_GCM_FALSE;
    fb->rt.xOffset = 0;
    fb->rt.yOffset = 0;
    fb->needValidate = GL_FALSE;
}

static void _RGLValidateFramebuffer( void )
{
    PSGLdevice *LDevice = _CurrentDevice;
    RGLDevice *gcmDevice = ( RGLDevice * )LDevice->platformDevice;

    PSGLcontext* LContext = _CurrentContext;
    RGLDriver *driver= (RGLDriver *)_CurrentDevice->rasterDriver;

    driver->rtValid = GL_FALSE;

    if(LContext->framebuffer)
    {
        jsPlatformFramebuffer* framebuffer = static_cast<jsPlatformFramebuffer *>((jsFramebuffer *)LContext->framebufferNameSpace.data[LContext->framebuffer] );

        if ( framebuffer->needValidate )
           jsPlatformFramebuffer_validate( framebuffer, LContext );

	driver->rt = framebuffer->rt;
        driver->colorBufferMask = framebuffer->colorBufferMask;
    }
    else
    {
	driver->rt = gcmDevice->rt;
        driver->colorBufferMask = 0x1;
    }

    driver->rtValid = GL_TRUE;

    _RGLFifoGlSetRenderTarget( &driver->rt );

    LContext->needValidate &= ~PSGL_VALIDATE_FRAMEBUFFER;
    _RGLFifoGlViewport(LContext->ViewPort.X, LContext->ViewPort.Y, 
    LContext->ViewPort.XSize, LContext->ViewPort.YSize, 0.0f, 1.0f);
}

GLAPI void APIENTRY glClear( GLbitfield mask )
{
   PSGLcontext *LContext = _CurrentContext;
   RGLDriver *driver= (RGLDriver *)_CurrentDevice->rasterDriver;
   RGLFifo * fifo = &_RGLState.fifo;

   if ( LContext->needValidate & PSGL_VALIDATE_FRAMEBUFFER )
      _RGLValidateFramebuffer();

   if ( !driver->rtValid )
      return;

   GLbitfield newmask = 0;

   if(driver->rt.colorBufferCount)
      newmask |= RGL_COLOR_BUFFER_BIT;

   if(!newmask)
      return;

   GLbitfield clearMask = newmask;

   if(driver->rt.colorBufferCount > 1)
      clearMask &= ~RGL_COLOR_BUFFER_BIT;

   if (clearMask)
   {
      GLuint hwColor;
      RGL_CALC_COLOR_LE_ARGB8( &hwColor, RGL_CLAMPF_01(LContext->ClearColor.R), RGL_CLAMPF_01(LContext->ClearColor.G), RGL_CLAMPF_01(LContext->ClearColor.B), RGL_CLAMPF_01(LContext->ClearColor.A) );

      cellGcmSetClearColorInline( &_RGLState.fifo, hwColor);
      cellGcmSetClearSurfaceInline ( &_RGLState.fifo, CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
      newmask &= ~clearMask;
   }

   if ( newmask )
   {
      static float _RGLClearVertexBuffer[12] __attribute__((aligned(128))) =
      {
         -1.f, -1.f, 0.f,
	 -1.f, 1.f, 0.f,
	 1.f, -1.f, 0.f,
	 1.f, 1.f, 0.f,
      };

      _RGLClearVertexBuffer[2] = 2.f - 1.f;
      _RGLClearVertexBuffer[5] = 2.f - 1.f;
      _RGLClearVertexBuffer[8] = 2.f - 1.f;
      _RGLClearVertexBuffer[11] = 2.f - 1.f;

      GLuint bufferId = gmmAlloc(0, sizeof(_RGLClearVertexBuffer));
      memcpy( gmmIdToAddress(bufferId), _RGLClearVertexBuffer, sizeof( _RGLClearVertexBuffer ) );
      GmmBaseBlock *pBaseBlock = (GmmBaseBlock *)bufferId;

      cellGcmSetVertexDataArrayInline( &_RGLState.fifo, 0 /* index */, 1/* frequency */, 3 * sizeof(GLfloat)/*stride */, 3 /* size */, CELL_GCM_VERTEX_F /* gcmType */, CELL_GCM_LOCATION_LOCAL, gmmAddressToOffset(pBaseBlock->address, pBaseBlock->isMain)/* offset */);

      RGLBIT_TRUE( LContext->attribs->DirtyMask, 0 );

      for(int i = 1; i < MAX_VERTEX_ATTRIBS; ++i)
      {
	 cellGcmSetVertexDataArrayInline( &_RGLState.fifo, i/* index */, 0/* frequency */, 0/*stride */, 0/* size */, CELL_GCM_VERTEX_F /*gcmType */, CELL_GCM_LOCATION_LOCAL, 0/* offset */ );
	 RGLBIT_TRUE( LContext->attribs->DirtyMask, i );
      }
      cellGcmSetVertexData4fInline( &_RGLState.fifo, _RGL_ATTRIB_PRIMARY_COLOR_INDEX, (GLfloat*)&LContext->ClearColor);

      LContext->needValidate |= PSGL_VALIDATE_FRAGMENT_PROGRAM;

      gmmFree( bufferId );
   }

   cellGcmSetInvalidateVertexCache( &_RGLState.fifo);
   _RGLFifoFlush( fifo );
}

GLAPI void APIENTRY glClearColor( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha )
{
    PSGLcontext *LContext = _CurrentContext;

    LContext->ClearColor.R = MAX(MIN(red, 1.f), 0.f);
    LContext->ClearColor.G = MAX(MIN(green, 1.f), 0.f);
    LContext->ClearColor.B = MAX(MIN(blue, 1.f), 0.f);
    LContext->ClearColor.A = MAX(MIN(alpha, 1.f), 0.f);
}

GLAPI void APIENTRY glBlendEquation( GLenum mode )
{
    PSGLcontext *LContext = _CurrentContext;

    LContext->BlendEquationRGB = LContext->BlendEquationAlpha = mode;
    LContext->needValidate |= PSGL_VALIDATE_BLENDING;
}

GLAPI void APIENTRY glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    PSGLcontext*	LContext = _CurrentContext;
    LContext->BlendColor.R = MAX(MIN(red, 1.f), 0.f);
    LContext->BlendColor.G = MAX(MIN(green, 1.f), 0.f);
    LContext->BlendColor.B = MAX(MIN(blue, 1.f), 0.f);
    LContext->BlendColor.A = MAX(MIN(alpha, 1.f), 0.f);

    LContext->needValidate |= PSGL_VALIDATE_BLENDING;
}

GLAPI void APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor)
{
    PSGLcontext *LContext = _CurrentContext;

    LContext->BlendFactorSrcRGB = sfactor;
    LContext->BlendFactorSrcAlpha = sfactor;
    LContext->BlendFactorDestRGB = dfactor;
    LContext->BlendFactorDestAlpha = dfactor;
    LContext->needValidate |= PSGL_VALIDATE_BLENDING;
}


jsFramebufferAttachment* _RGLFramebufferGetAttachment( jsFramebuffer *framebuffer, GLenum attachment )
{
    switch ( attachment )
    {
        case GL_COLOR_ATTACHMENT0_EXT:
        case GL_COLOR_ATTACHMENT1_EXT:
        case GL_COLOR_ATTACHMENT2_EXT:
        case GL_COLOR_ATTACHMENT3_EXT:
            return &framebuffer->color[attachment - GL_COLOR_ATTACHMENT0_EXT];
        case GL_DEPTH_ATTACHMENT_OES:
        case GL_STENCIL_ATTACHMENT_OES:
        default:
            _RGLSetError( GL_INVALID_ENUM );
            return NULL;
    }
}

GLAPI void APIENTRY glBindFramebufferOES( GLenum target, GLuint framebuffer )
{
    PSGLcontext* LContext = _CurrentContext;

    if ( framebuffer )
        _RGLTexNameSpaceCreateNameLazy( &LContext->framebufferNameSpace, framebuffer );

    LContext->framebuffer = framebuffer;
    LContext->needValidate |= PSGL_VALIDATE_FRAMEBUFFER;
}

GLAPI void APIENTRY glDeleteFramebuffersOES( GLsizei n, const GLuint *framebuffers )
{

    PSGLcontext *LContext = _CurrentContext;

    for(int i = 0; i < n; ++i)
    {
        if ( framebuffers[i] && framebuffers[i] == LContext->framebuffer )
           glBindFramebufferOES( GL_FRAMEBUFFER_OES, 0 );
    }

    _RGLTexNameSpaceDeleteNames( &LContext->framebufferNameSpace, n, framebuffers );
}

GLAPI void APIENTRY glGenFramebuffersOES( GLsizei n, GLuint *framebuffers )
{
   PSGLcontext *LContext = _CurrentContext;
   _RGLTexNameSpaceGenNames( &LContext->framebufferNameSpace, n, framebuffers );
}


GLAPI GLenum APIENTRY glCheckFramebufferStatusOES( GLenum target )
{
    PSGLcontext *LContext = _CurrentContext;

    if ( LContext->framebuffer )
    {
        jsFramebuffer* framebuffer = (jsFramebuffer *)LContext->framebufferNameSpace.data[LContext->framebuffer];
        return _RGLPlatformFramebufferCheckStatus( framebuffer );
    }

    return GL_FRAMEBUFFER_COMPLETE_OES;
}

GLAPI void APIENTRY glFramebufferTexture2DOES( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level )
{
   PSGLcontext* LContext = _CurrentContext;

   jsFramebuffer* framebuffer = (jsFramebuffer *)LContext->framebufferNameSpace.data[LContext->framebuffer];

   jsFramebufferAttachment* attach = _RGLFramebufferGetAttachment( framebuffer, GL_COLOR_ATTACHMENT0_EXT );

   if ( !attach )
      return;

   jsTexture *textureObject = NULL;
   _RGLFramebufferGetAttachmentTexture(attach, &textureObject);

   if ( textureObject )
      textureObject->framebuffers.removeElement( framebuffer );

   if ( texture )
   {
      attach->type = FRAMEBUFFER_ATTACHMENT_TEXTURE;
      textureObject = ( jsTexture* )LContext->textureNameSpace.data[texture];
      textureObject->framebuffers.pushBack( framebuffer );
   }
   else
      attach->type = FRAMEBUFFER_ATTACHMENT_NONE;

   attach->name = texture;
   attach->textureTarget = GL_TEXTURE_2D;

   framebuffer->needValidate = GL_TRUE;
   LContext->needValidate |= PSGL_VALIDATE_FRAMEBUFFER;
}

void cgRTCgcInit( void )
{
    _cgRTCgcCompileProgramHook = &compile_program_from_string;
    _cgRTCgcFreeCompiledProgramHook = &free_compiled_program;
}

void cgRTCgcFree( void )
{
    _cgRTCgcCompileProgramHook = 0;
    _cgRTCgcFreeCompiledProgramHook = 0;
}

jsName _RGLCreateName(jsNameSpace * ns, void* object)
{
   if ( ns->firstFree == NULL )
   {
      int newCapacity = ns->capacity + NAME_INCREMENT;

      void** newData = ( void** )malloc( newCapacity * sizeof( void* ) );
      if ( newData == NULL )
      {
         _RGLCgRaiseError( CG_MEMORY_ALLOC_ERROR );
	 return 0;
      }
      memcpy( newData, ns->data, ns->capacity * sizeof( void* ) );

      if ( ns->data != NULL )
         free( ns->data );

      ns->data = newData;

      for ( int index = ns->capacity; index < newCapacity - 1; ++index )
         ns->data[index] = ns->data + index + 1;

      ns->data[newCapacity - 1] = NULL;
      ns->firstFree = ns->data + ns->capacity;
      ns->capacity = newCapacity;
   }
   jsName result = ns->firstFree - ns->data;

   ns->firstFree = ( void** ) * ns->firstFree;
   ns->data[result] = object;

   return result + 1;
}

unsigned int _RGLIsName(jsNameSpace* ns, jsName name)
{
   if ( RGL_UNLIKELY( name == 0 ) )
      return 0;

   --name;

   if ( RGL_UNLIKELY( name >= ns->capacity ) )
      return 0;

   void** value = ( void** )ns->data[name];

   if ( RGL_UNLIKELY( value == NULL ||
   (value >= ns->data && value < ns->data + ns->capacity)))
      return 0;

   return 1;
}

void _RGLEraseName(jsNameSpace* ns, jsName name)
{
   if(_RGLIsName( ns, name))
   {
      --name;
      ns->data[name] = ns->firstFree;
      ns->firstFree = ns->data + name;
   }
}

void _RGLTexNameSpaceInit( jsTexNameSpace *ns, jsTexNameSpaceCreateFunction create, jsTexNameSpaceDestroyFunction destroy )
{
    ns->capacity = CAPACITY_INCR; 
    ns->data = ( void ** )malloc( ns->capacity * sizeof( void* ) );
    memset( ns->data, 0, ns->capacity*sizeof( void* ) );
    ns->create = create;
    ns->destroy = destroy;
}

void _RGLTexNameSpaceFree( jsTexNameSpace *ns )
{
   for(GLuint i = 1; i < ns->capacity; ++i)
      if ( ns->data[i] ) ns->destroy( ns->data[i] );

   if(ns->data != NULL)
      free( ns->data );

   ns->data = NULL;
}

void _RGLTexNameSpaceResetNames( jsTexNameSpace *ns )
{
   for ( GLuint i = 1;i < ns->capacity;++i )
   {
      if ( ns->data[i] )
      {
         ns->destroy( ns->data[i] );
	 ns->data[i] = NULL;
      }
   }
}

GLuint _RGLTexNameSpaceGetFree( jsTexNameSpace *ns )
{
   GLuint i;

   for(i = 1; i < ns->capacity; ++i)
      if ( !ns->data[i] )
         break;

   return i;
}

GLboolean _RGLTexNameSpaceCreateNameLazy( jsTexNameSpace *ns, GLuint name )
{
    if ( name >= ns->capacity )
    {
        int newCapacity = name >= ns->capacity + CAPACITY_INCR ? name + 1 : ns->capacity + CAPACITY_INCR;
        void **newData = (void**)realloc(ns->data, newCapacity * sizeof(void*));
        memset( newData + ns->capacity, 0, (newCapacity - ns->capacity )*sizeof(void*));
        ns->data = newData;
        ns->capacity = newCapacity;
    }

    if (!ns->data[name])
    {
        ns->data[name] = ns->create();
        if(ns->data[name])
           return GL_TRUE;
    }
    return GL_FALSE;
}

GLboolean _RGLTexNameSpaceIsName( jsTexNameSpace *ns, GLuint name )
{
   if (( name > 0 ) && (name < ns->capacity))
      return( ns->data[name] != 0 );
   else
      return GL_FALSE;
}

void _RGLTexNameSpaceGenNames( jsTexNameSpace *ns, GLsizei n, GLuint *names )
{
    for (int i = 0; i < n; ++i)
    {
        GLuint name = _RGLTexNameSpaceGetFree( ns );
        names[i] = name;

        if(name)
           _RGLTexNameSpaceCreateNameLazy( ns, name );
    }
}

void _RGLTexNameSpaceDeleteNames( jsTexNameSpace *ns, GLsizei n, const GLuint *names )
{
   for ( int i = 0;i < n;++i )
   {
      GLuint name = names[i];

      if(!_RGLTexNameSpaceIsName(ns, name))
         continue;

      ns->destroy( ns->data[name] );
      ns->data[name] = NULL;
   }
}

static inline unsigned int endianSwapWordByHalf( unsigned int v )
{
   return (v & 0xffff ) << 16 | v >> 16;
}

static uint32_t gmmInitFixedAllocator (void)
{
   pGmmFixedAllocData = (GmmFixedAllocData *)malloc(sizeof(GmmFixedAllocData));

   if (pGmmFixedAllocData == NULL)
      return GMM_ERROR;

   memset(pGmmFixedAllocData, 0, sizeof(GmmFixedAllocData));

   for (int i = 0; i < 2; i++)
   {
      int blockCount = (i==0) ? GMM_BLOCK_COUNT : GMM_TILE_BLOCK_COUNT;
      int blockSize  = (i==0) ? sizeof(GmmBlock): sizeof(GmmTileBlock);

      pGmmFixedAllocData->ppBlockList[i] = (char **)malloc(sizeof(char *));

      if (pGmmFixedAllocData->ppBlockList[i] == NULL)
         return GMM_ERROR;

      pGmmFixedAllocData->ppBlockList[i][0] = (char *)malloc(blockSize * blockCount);

      if (pGmmFixedAllocData->ppBlockList[i][0] == NULL)
         return GMM_ERROR;

      pGmmFixedAllocData->ppFreeBlockList[i] = (uint16_t **)malloc(sizeof(uint16_t *));

      if (pGmmFixedAllocData->ppFreeBlockList[i] == NULL)
         return GMM_ERROR;

      pGmmFixedAllocData->ppFreeBlockList[i][0] = (uint16_t *)malloc(sizeof(uint16_t) * blockCount);

      if (pGmmFixedAllocData->ppFreeBlockList[i][0] == NULL)
         return GMM_ERROR;

      pGmmFixedAllocData->pBlocksUsed[i] = (uint16_t *)malloc(sizeof(uint16_t));

      if (pGmmFixedAllocData->pBlocksUsed[i] == NULL)
         return GMM_ERROR;

      for (int j=0; j<blockCount; j++)
         pGmmFixedAllocData->ppFreeBlockList[i][0][j] = j; 

      pGmmFixedAllocData->pBlocksUsed[i][0] = 0;
      pGmmFixedAllocData->BlockListCount[i] = 1;
   }

   return CELL_OK;
}

static uint8_t gmmSizeToFreeIndex(uint32_t size)
{
    if (size >= GMM_FREE_BIN_0 && size < GMM_FREE_BIN_1)
        return 0;
    else if (size >= GMM_FREE_BIN_1 && size < GMM_FREE_BIN_2)
        return 1;
    else if (size >= GMM_FREE_BIN_2 && size < GMM_FREE_BIN_3)
        return 2;
    else if (size >= GMM_FREE_BIN_3 && size < GMM_FREE_BIN_4)
        return 3;
    else if (size >= GMM_FREE_BIN_4 && size < GMM_FREE_BIN_5)
        return 4;
    else if (size >= GMM_FREE_BIN_5 && size < GMM_FREE_BIN_6)
        return 5;
    else if (size >= GMM_FREE_BIN_6 && size < GMM_FREE_BIN_7)
        return 6;
    else if (size >= GMM_FREE_BIN_7 && size < GMM_FREE_BIN_8)
        return 7;
    else if (size >= GMM_FREE_BIN_8 && size < GMM_FREE_BIN_9)
        return 8;
    else if (size >= GMM_FREE_BIN_9 && size < GMM_FREE_BIN_10)
        return 9;
    else if (size >= GMM_FREE_BIN_10 && size < GMM_FREE_BIN_11)
        return 10;
    else if (size >= GMM_FREE_BIN_11 && size < GMM_FREE_BIN_12)
        return 11;
    else if (size >= GMM_FREE_BIN_12 && size < GMM_FREE_BIN_13)
        return 12;
    else if (size >= GMM_FREE_BIN_13 && size < GMM_FREE_BIN_14)
        return 13;
    else if (size >= GMM_FREE_BIN_14 && size < GMM_FREE_BIN_15)
        return 14;
    else if (size >= GMM_FREE_BIN_15 && size < GMM_FREE_BIN_16)
        return 15;
    else if (size >= GMM_FREE_BIN_16 && size < GMM_FREE_BIN_17)
        return 16;
    else if (size >= GMM_FREE_BIN_17 && size < GMM_FREE_BIN_18)
        return 17;
    else if (size >= GMM_FREE_BIN_18 && size < GMM_FREE_BIN_19)
        return 18;
    else if (size >= GMM_FREE_BIN_19 && size < GMM_FREE_BIN_20)
        return 19;
    else if (size >= GMM_FREE_BIN_20 && size < GMM_FREE_BIN_21)
        return 20;
    else
        return 21;
}

void gmmAddFree(GmmAllocator *pAllocator, GmmBlock *pBlock)
{
    uint8_t freeIndex = gmmSizeToFreeIndex(pBlock->base.size);

    if (pAllocator->pFreeHead[freeIndex])
    {
        GmmBlock *pInsertBefore = pAllocator->pFreeHead[freeIndex];

        while (pInsertBefore && pInsertBefore->base.size < pBlock->base.size)
        {
            pInsertBefore = pInsertBefore->pNextFree;
        }

        if (pInsertBefore == NULL)
        {
            pBlock->pNextFree = NULL;
            pBlock->pPrevFree = pAllocator->pFreeTail[freeIndex];
            pAllocator->pFreeTail[freeIndex]->pNextFree = pBlock;
            pAllocator->pFreeTail[freeIndex] = pBlock;
        }
        else if (pInsertBefore == pAllocator->pFreeHead[freeIndex])
        {
            pBlock->pNextFree = pInsertBefore;
            pBlock->pPrevFree = pInsertBefore->pPrevFree;
            pInsertBefore->pPrevFree = pBlock;
            pAllocator->pFreeHead[freeIndex] = pBlock;
        }
        else
        {
            pBlock->pNextFree = pInsertBefore;
            pBlock->pPrevFree = pInsertBefore->pPrevFree;
            pInsertBefore->pPrevFree->pNextFree = pBlock;
            pInsertBefore->pPrevFree = pBlock;
        }
    }
    else
    {
        pBlock->pNextFree = NULL;
        pBlock->pPrevFree = NULL;
        pAllocator->pFreeHead[freeIndex] = pBlock;
        pAllocator->pFreeTail[freeIndex] = pBlock;
    }
}


static void *gmmAllocFixed(uint8_t isTile)
{
    int blockCount = isTile ? GMM_TILE_BLOCK_COUNT : GMM_BLOCK_COUNT;
    int blockSize  = isTile ? sizeof(GmmTileBlock) : sizeof(GmmBlock);
    int listCount  = pGmmFixedAllocData->BlockListCount[isTile];

    for (int i=0; i<listCount; i++)
    {
        if (pGmmFixedAllocData->pBlocksUsed[isTile][i] < blockCount)
        {
            return pGmmFixedAllocData->ppBlockList[isTile][i] + 
                   (pGmmFixedAllocData->ppFreeBlockList[isTile][i][pGmmFixedAllocData->pBlocksUsed[isTile][i]++] * 
                    blockSize);
        }
    }

    char **ppBlockList = 
        (char **)realloc(pGmmFixedAllocData->ppBlockList[isTile],
                         (listCount + 1) * sizeof(char *));
    if (ppBlockList == NULL)
        return NULL;

    pGmmFixedAllocData->ppBlockList[isTile] = ppBlockList;

    pGmmFixedAllocData->ppBlockList[isTile][listCount] = 
        (char *)malloc(blockSize * blockCount);
    if (pGmmFixedAllocData->ppBlockList[isTile][listCount] == NULL)
        return NULL;

    uint16_t **ppFreeBlockList = 
        (uint16_t **)realloc(pGmmFixedAllocData->ppFreeBlockList[isTile],
                            (listCount + 1) * sizeof(uint16_t *));
    if (ppFreeBlockList == NULL)
        return NULL;

    pGmmFixedAllocData->ppFreeBlockList[isTile] = ppFreeBlockList;

    pGmmFixedAllocData->ppFreeBlockList[isTile][listCount] = 
        (uint16_t *)malloc(sizeof(uint16_t) * blockCount);
    if (pGmmFixedAllocData->ppFreeBlockList[isTile][listCount] == NULL)
        return NULL;

    uint16_t *pBlocksUsed = 
        (uint16_t *)realloc(pGmmFixedAllocData->pBlocksUsed[isTile],
                            (listCount + 1) * sizeof(uint16_t));
    if (pBlocksUsed == NULL)
        return NULL;

    pGmmFixedAllocData->pBlocksUsed[isTile] = pBlocksUsed;
                                
    for (int i=0; i<blockCount; i++)
       pGmmFixedAllocData->ppFreeBlockList[isTile][listCount][i] = i; 

    pGmmFixedAllocData->pBlocksUsed[isTile][listCount] = 0;
    pGmmFixedAllocData->BlockListCount[isTile]++;

    return pGmmFixedAllocData->ppBlockList[isTile][listCount] + 
           (pGmmFixedAllocData->ppFreeBlockList[isTile][listCount][pGmmFixedAllocData->pBlocksUsed[isTile][listCount]++] * 
            blockSize);
}

static void gmmFreeFixed(uint8_t isTile, void *pBlock)
{
   int blockCount = isTile ? GMM_TILE_BLOCK_COUNT : GMM_BLOCK_COUNT;
   int blockSize  = isTile ? sizeof(GmmTileBlock) : sizeof(GmmBlock);

   for (int i=0; i<pGmmFixedAllocData->BlockListCount[isTile]; i++)
   {
      if (pBlock >= pGmmFixedAllocData->ppBlockList[isTile][i] &&
      pBlock < (pGmmFixedAllocData->ppBlockList[isTile][i] + blockSize * blockCount))
      {
         int index = ((char *)pBlock - pGmmFixedAllocData->ppBlockList[isTile][i]) / blockSize;
	 pGmmFixedAllocData->ppFreeBlockList[isTile][i][--pGmmFixedAllocData->pBlocksUsed[isTile][i]] = index;
      }
   }
}

uint32_t gmmInit(
    const void *localMemoryBase,
    const void *localStartAddress,
    const uint32_t localSize,
    const void *mainMemoryBase,
    const void *mainStartAddress,
    const uint32_t mainSize
)
{
    GmmAllocator *pAllocator[2];
    uint32_t alignedLocalSize, alignedMainSize;
    uint32_t localEndAddress = (uint32_t)localStartAddress + localSize;
    uint32_t mainEndAddress = (uint32_t)mainStartAddress + mainSize;

    localEndAddress = (localEndAddress / GMM_TILE_ALIGNMENT) * GMM_TILE_ALIGNMENT;
    mainEndAddress = (mainEndAddress / GMM_TILE_ALIGNMENT) * GMM_TILE_ALIGNMENT;

    alignedLocalSize = localEndAddress - (uint32_t)localStartAddress;
    alignedMainSize = mainEndAddress - (uint32_t)mainStartAddress;


    pAllocator[0] = (GmmAllocator *)malloc(2*sizeof(GmmAllocator));
    pAllocator[1] = pAllocator[0] + 1;

    if (pAllocator[0] == NULL)
        return GMM_ERROR;

    memset(pAllocator[0], 0, 2*sizeof(GmmAllocator));

    if (pAllocator[0])
    {
        pAllocator[0]->memoryBase = (uint32_t)localMemoryBase;
        pAllocator[1]->memoryBase = (uint32_t)mainMemoryBase;
        pAllocator[0]->startAddress = (uint32_t)localStartAddress;
        pAllocator[1]->startAddress = (uint32_t)mainStartAddress;
        pAllocator[0]->size = alignedLocalSize;
        pAllocator[1]->size = alignedMainSize;
        pAllocator[0]->freeAddress = pAllocator[0]->startAddress;
        pAllocator[1]->freeAddress = pAllocator[1]->startAddress;
        pAllocator[0]->tileStartAddress = ((uint32_t)localStartAddress) + alignedLocalSize;
        pAllocator[1]->tileStartAddress = ((uint32_t)mainStartAddress) + alignedMainSize;
        pAllocator[0]->totalSize = alignedLocalSize;
        pAllocator[1]->totalSize = alignedMainSize;

        pGmmLocalAllocator = pAllocator[0];
        pGmmMainAllocator = pAllocator[1];
    }
    else
        return GMM_ERROR;

    pLock = cellGcmGetLabelAddress(GMM_PPU_WAIT_INDEX);
    *pLock = 0;
    cachedLockValue = 0;

    return gmmInitFixedAllocator();
}

void gmmSetTileAttrib(const uint32_t id, const uint32_t tag, void *pData)
{
   GmmTileBlock *pTileBlock = (GmmTileBlock *)id;

   pTileBlock->tileTag = tag;
   pTileBlock->pData = pData;
}

uint32_t gmmIdToOffset(const uint32_t id)
{
   GmmBaseBlock *pBaseBlock = (GmmBaseBlock *)id;
   return gmmAddressToOffset(pBaseBlock->address, pBaseBlock->isMain);
}

char *gmmIdToAddress(const uint32_t id)
{
    GmmBaseBlock *pBaseBlock = (GmmBaseBlock *)id;

    do
    {
       if (cachedLockValue == 0)
          break;

       cachedLockValue = *pLock;

       if (cachedLockValue == 0)
          break;
    }while(1);

    return (char *)pBaseBlock->address;
}

static void _RGLGetTileRegionInfo(void* data, GLuint *address, GLuint *size)
{
   jsTiledRegion* region = ( jsTiledRegion* )data;

   *address = region->offset;
   *size = region->size;
}

static GmmBlock *gmmAllocBlock(GmmAllocator *pAllocator, uint32_t size)
{
    uint32_t    address;
    GmmBlock    *pNewBlock = NULL;
    GmmBlock    *pBlock = pAllocator->pTail;

    address = pAllocator->freeAddress;

    if (UINT_MAX - address >= size && 
        address + size <= pAllocator->startAddress + pAllocator->size)
    {
        pNewBlock = (GmmBlock *)gmmAllocFixed(0);
        if (pNewBlock == NULL)
            return NULL;

        memset(pNewBlock, 0, sizeof(GmmBlock));

        pNewBlock->base.address = address;
        pNewBlock->base.isMain = (pAllocator == pGmmMainAllocator);
        pNewBlock->base.size = size;
        pAllocator->freeAddress = address + size;

        if (pBlock)
        {
            pNewBlock->pPrev = pBlock;
            pBlock->pNext = pNewBlock;
            pAllocator->pTail = pNewBlock;
        }
        else
        {
            pAllocator->pHead = pNewBlock;
            pAllocator->pTail = pNewBlock;
        }
    }

    return pNewBlock;
}

static GmmTileBlock *gmmFindFreeTileBlock(GmmAllocator *pAllocator, const uint32_t size)
{
    GmmTileBlock    *pBlock = pAllocator->pTileHead;
    GmmTileBlock    *pBestAfterBlock = NULL;
    GmmTileBlock    *pNewBlock = NULL;
    uint32_t        bestSize = 0;
    uint32_t        freeSize = 0;

    while (pBlock && pBlock->pNext)
    {
        freeSize = pBlock->pNext->base.address - pBlock->base.address - pBlock->base.size;

        if (freeSize >= size &&
            (pBestAfterBlock == NULL || freeSize < bestSize) &&
            (pBlock->pNext == NULL ||
             pBlock->pData != pBlock->pNext->pData))
        {
            pBestAfterBlock = pBlock;
            bestSize = freeSize;
        }

        pBlock = pBlock->pNext;
    }

    if (pBestAfterBlock)
    {
        pNewBlock = (GmmTileBlock *)gmmAllocFixed(1);

        if (pNewBlock == NULL)
            return NULL;

        memset(pNewBlock, 0, sizeof(GmmTileBlock));

        pNewBlock->base.address = pBestAfterBlock->base.address + pBestAfterBlock->base.size;
        pNewBlock->base.isMain = (pAllocator == pGmmMainAllocator);
        pNewBlock->base.isTile = 1;
        pNewBlock->base.size = size;

        pNewBlock->pNext = pBestAfterBlock->pNext;
        pNewBlock->pPrev = pBestAfterBlock;
        pNewBlock->pPrev->pNext = pNewBlock;
        pNewBlock->pNext->pPrev = pNewBlock;

        return pNewBlock;
    }
    else
        return NULL;
}

static GmmTileBlock *gmmCreateTileBlock(GmmAllocator *pAllocator, const uint32_t size)
{
    GmmTileBlock    *pNewBlock;
    uint32_t        address;

    address = pAllocator->tileStartAddress - size;

    if (address > pAllocator->startAddress + pAllocator->size)
        return NULL;

    if (pAllocator->pTail &&
        pAllocator->pTail->base.address + pAllocator->pTail->base.size > address)
        return NULL;

    pAllocator->size = address - pAllocator->startAddress;
    pAllocator->tileSize = pAllocator->tileStartAddress + pAllocator->tileSize - address;
    pAllocator->tileStartAddress = address;

    pNewBlock = (GmmTileBlock *)gmmAllocFixed(1);
    if (pNewBlock == NULL)
        return NULL;

    memset(pNewBlock, 0, sizeof(GmmTileBlock));

    pNewBlock->base.address = address;
    pNewBlock->base.isMain = (pAllocator == pGmmMainAllocator);
    pNewBlock->base.isTile = 1;
    pNewBlock->base.size = size;
    pNewBlock->pNext = pAllocator->pTileHead;

    if (pAllocator->pTileHead)
        pAllocator->pTileHead->pPrev = pNewBlock;
    else
        pAllocator->pTileTail = pNewBlock;

    pAllocator->pTileHead = pNewBlock;

    return pNewBlock;
}

static void gmmFreeTileBlock(GmmTileBlock *pTileBlock)
{
    GmmAllocator    *pAllocator;

    if (pTileBlock->pPrev)
        pTileBlock->pPrev->pNext = pTileBlock->pNext;

    if (pTileBlock->pNext)
        pTileBlock->pNext->pPrev = pTileBlock->pPrev;

    if (pTileBlock->base.isMain)
        pAllocator = pGmmMainAllocator;
    else
        pAllocator = pGmmLocalAllocator;

    if (pAllocator->pTileHead == pTileBlock)
    {
        pAllocator->pTileHead = pTileBlock->pNext;

        if (pAllocator->pTileHead)
            pAllocator->pTileHead->pPrev = NULL;

        pAllocator->size = pAllocator->pTileHead ? 
                           pAllocator->pTileHead->base.address - pAllocator->startAddress :
                           pAllocator->totalSize;
        pAllocator->tileSize = pAllocator->totalSize - pAllocator->size;
        pAllocator->tileStartAddress = pAllocator->pTileHead ?
                                       pAllocator->pTileHead->base.address :
                                       pAllocator->startAddress + pAllocator->size;
    }

    if (pAllocator->pTileTail == pTileBlock)
    {
        pAllocator->pTileTail = pTileBlock->pPrev;
        
        if (pAllocator->pTileTail)
            pAllocator->pTileTail->pNext = NULL;
    }

    gmmFreeFixed(1, pTileBlock);
}

uint32_t gmmAllocExtendedTileBlock(const uint32_t size, const uint32_t tag)
{
    GmmAllocator    *pAllocator =  pGmmLocalAllocator;
    uint32_t        retId = 0;
    uint32_t        newSize;
    uint8_t         resizeSucceed = 1;

    newSize = pad(size, GMM_TILE_ALIGNMENT);

    GmmTileBlock    *pBlock = pAllocator->pTileTail;

    while (pBlock)
    {
        if (pBlock->tileTag == tag)
        {
            GLuint address, tileSize;
            _RGLGetTileRegionInfo(pBlock->pData, &address, &tileSize);

            if ((pBlock->pNext && pBlock->pNext->base.address-pBlock->base.address-pBlock->base.size >= newSize) ||
                (pBlock->pPrev && pBlock->base.address-pBlock->pPrev->base.address-pBlock->pPrev->base.size >= newSize))
            {
                GmmTileBlock *pNewBlock = (GmmTileBlock *)gmmAllocFixed(1);
                if (pNewBlock == NULL)
                    break;

                retId = (uint32_t)pNewBlock;

                memset(pNewBlock, 0, sizeof(GmmTileBlock));

                pNewBlock->base.isMain = (pAllocator == pGmmMainAllocator);
                pNewBlock->base.isTile = 1;
                pNewBlock->base.size = newSize;

                if (pBlock->pNext && pBlock->pNext->base.address-pBlock->base.address-pBlock->base.size >= newSize)
                {
                    pNewBlock->base.address = pBlock->base.address+pBlock->base.size;
                    pNewBlock->pNext = pBlock->pNext;
                    pNewBlock->pPrev = pBlock;
                    pBlock->pNext->pPrev = pNewBlock;
                    pBlock->pNext = pNewBlock;

                    if (pNewBlock->pPrev->pData != pNewBlock->pNext->pData)
                    {
                        resizeSucceed = _RGLTryResizeTileRegion( address, tileSize+newSize, pBlock->pData );
                    }
                }
                else
                {
                    pNewBlock->base.address = pBlock->base.address-newSize;
                    pNewBlock->pNext = pBlock;
                    pNewBlock->pPrev = pBlock->pPrev;
                    pBlock->pPrev->pNext = pNewBlock;
                    pBlock->pPrev = pNewBlock;

                    if (pNewBlock->pPrev->pData != pNewBlock->pNext->pData)
                    {
                        GmmBaseBlock *pBaseBlock = (GmmBaseBlock *)(uint32_t)(pNewBlock);
                        resizeSucceed = _RGLTryResizeTileRegion((GLuint)gmmAddressToOffset(pBaseBlock->address, pBaseBlock->isMain), tileSize+newSize, pBlock->pData);
                    }
                }
                gmmSetTileAttrib( retId, tag, pBlock->pData );
                break;
            }

            if (pBlock == pAllocator->pTileHead)
            {
                retId = (uint32_t)gmmCreateTileBlock(pAllocator, newSize); 
                if (retId == 0)
                    break;

                resizeSucceed = _RGLTryResizeTileRegion( (GLuint)gmmIdToOffset(retId), tileSize+newSize, pBlock->pData );
                gmmSetTileAttrib( retId, tag, pBlock->pData );
                break;
            }
        }

        pBlock = pBlock->pPrev;
    }

    if (retId == 0)
        return GMM_ERROR;

    if (!resizeSucceed)
    {
        gmmFreeTileBlock((GmmTileBlock *)retId);
        return GMM_ERROR;
    }

    return retId;
}

static GmmTileBlock *gmmAllocTileBlock(
    GmmAllocator *pAllocator,
    const uint32_t size
)
{
    GmmTileBlock    *pBlock = gmmFindFreeTileBlock(pAllocator, size); 

    if (pBlock == NULL)
        pBlock = gmmCreateTileBlock(pAllocator, size);

    return pBlock;
}

static void gmmFreeBlock(GmmBlock *pBlock)
{
    GmmAllocator    *pAllocator;

    if (pBlock->pPrev)
        pBlock->pPrev->pNext = pBlock->pNext;

    if (pBlock->pNext)
        pBlock->pNext->pPrev = pBlock->pPrev;
   
    if (pBlock->base.isMain)
        pAllocator = pGmmMainAllocator;
    else
        pAllocator = pGmmLocalAllocator;

    if (pAllocator->pHead == pBlock)
    {
        pAllocator->pHead = pBlock->pNext;

        if (pAllocator->pHead)
            pAllocator->pHead->pPrev = NULL;
    }

    if (pAllocator->pTail == pBlock)
    {
        pAllocator->pTail = pBlock->pPrev;
        
        if (pAllocator->pTail)
            pAllocator->pTail->pNext = NULL;
    }

    if (pBlock->pPrev == NULL)
        pAllocator->pSweepHead = pAllocator->pHead;
    else if (pBlock->pPrev &&
             (pAllocator->pSweepHead == NULL || 
              (pAllocator->pSweepHead &&
               pAllocator->pSweepHead->base.address > pBlock->pPrev->base.address)))
    {
        pAllocator->pSweepHead = pBlock->pPrev;
    }

    pAllocator->freedSinceSweep += pBlock->base.size;
    
    gmmFreeFixed(0, pBlock);
}

static void gmmAddPendingFree(GmmBlock *pBlock)
{
    GmmAllocator    *pAllocator;

    if (pBlock->base.isMain)
        pAllocator = pGmmMainAllocator;
    else
        pAllocator = pGmmLocalAllocator;

    if (pAllocator->pPendingFreeTail)
    {
        pBlock->pNextFree = NULL;
        pBlock->pPrevFree = pAllocator->pPendingFreeTail;
        pAllocator->pPendingFreeTail->pNextFree = pBlock;
        pAllocator->pPendingFreeTail = pBlock;
    }
    else
    {
        pBlock->pNextFree = NULL;
        pBlock->pPrevFree = NULL;
        pAllocator->pPendingFreeHead = pBlock;
        pAllocator->pPendingFreeTail = pBlock;
    }

    pBlock->isPinned = 0;

    GLuint* ref = &pBlock->fence;
    ++nvFenceCounter;
    cellGcmSetWriteBackEndLabelInline( &_RGLState.fifo, SEMA_FENCE, nvFenceCounter);
    *ref = nvFenceCounter;
}



uint32_t gmmFree(const uint32_t freeId)
{
    GmmBaseBlock    *pBaseBlock = (GmmBaseBlock *)freeId;
    
    if (pBaseBlock->isTile)
    {
        GmmTileBlock *pTileBlock = (GmmTileBlock *)pBaseBlock;

        if (pTileBlock->pPrev &&
            pTileBlock->pNext &&
            pTileBlock->pPrev->pData == pTileBlock->pNext->pData)
        {
        }
        else if (pTileBlock->pPrev && pTileBlock->pPrev->pData == pTileBlock->pData)
        {
            GLuint address, size;

            _RGLGetTileRegionInfo(pTileBlock->pData, &address, &size);
            if ( !_RGLTryResizeTileRegion(address, (size-pTileBlock->base.size), pTileBlock->pData) )
            {
                _RGLTryResizeTileRegion(address, 0, pTileBlock->pData);
                if ( !_RGLTryResizeTileRegion(address, (size-pTileBlock->base.size), pTileBlock->pData) )
                {
                }
            }
        }
        else if (pTileBlock->pNext && pTileBlock->pNext->pData == pTileBlock->pData)
        {
            GLuint address, size;

            _RGLGetTileRegionInfo(pTileBlock->pData, &address, &size);
            if ( !_RGLTryResizeTileRegion((address+pTileBlock->base.size), (size-pTileBlock->base.size), pTileBlock->pData) )
            {
                _RGLTryResizeTileRegion(address, 0, pTileBlock->pData);
                if ( !_RGLTryResizeTileRegion((address+pTileBlock->base.size), (size-pTileBlock->base.size), pTileBlock->pData) )
                {
                }
            }
        }
        else
        {
            if ( !_RGLTryResizeTileRegion( (GLuint)gmmIdToOffset(freeId), 0, ((GmmTileBlock *)freeId)->pData ) )
            {
            }
        }

        gmmFreeTileBlock(pTileBlock);
    }
    else
    {
        GmmBlock *pBlock = (GmmBlock *)pBaseBlock;

        gmmAddPendingFree(pBlock);
    }

    return CELL_OK;
}

static inline void gmmLocalMemcpy(
    const uint32_t dstOffset,
    const uint32_t srcOffset,
    const uint32_t moveSize
)
{
   CellGcmContextData *thisContext = &_RGLState.fifo;
   int32_t offset = 0;
   int32_t sizeLeft = moveSize;
   int32_t dimension = 4096;

   while (sizeLeft)
   {
      while(sizeLeft >= dimension*dimension*4)
      {
         cellGcmSetTransferImage(thisContext,
			 CELL_GCM_TRANSFER_LOCAL_TO_LOCAL,
			 dstOffset+offset,
			 dimension*4,
			 0,
			 0,
			 srcOffset+offset,
			 dimension*4,
			 0,
			 0,
			 dimension,
			 dimension,
			 4);

	 offset = offset + dimension*dimension*4;
	 sizeLeft = sizeLeft - (dimension*dimension*4);
      }

      dimension = dimension >> 1;

      if (dimension == 32)
         break;
   }

   if (sizeLeft)
      cellGcmSetTransferImage(thisContext, 
			   CELL_GCM_TRANSFER_LOCAL_TO_LOCAL,
			   dstOffset+offset,
			   sizeLeft,
			   0,
			   0,
			   srcOffset+offset,
			   sizeLeft,
			   0,
			   0,
			   sizeLeft/4,
			   1,
			   4);
}

static uint8_t gmmInternalSweep (void)
{
    GmmAllocator    *pAllocator  = pGmmLocalAllocator;
    GmmBlock        *pBlock;       
    GmmBlock        *pSrcBlock;
    GmmBlock        *pTempBlock;
    GmmBlock        *pTempBlockNext;
    uint32_t        dstAddress, srcAddress;
    uint32_t        srcOffset, dstOffset;
    uint32_t        prevEndAddress;
    uint32_t        moveSize, moveDistance;
    uint8_t         ret = 0;
    uint32_t        totalMoveSize = 0;

    pBlock = pAllocator->pSweepHead;
    srcAddress = 0;
    dstAddress = 0;
    prevEndAddress = 0;
    pSrcBlock = pBlock;

    while (pBlock != NULL)
    {
        if (pBlock->isPinned == 0)
        {
            if (pBlock->pPrev)
                prevEndAddress = pBlock->pPrev->base.address + pBlock->pPrev->base.size;
            else
                prevEndAddress = pAllocator->startAddress;

            if (pBlock->base.address > prevEndAddress)
            {
                dstAddress = prevEndAddress;
                srcAddress = pBlock->base.address;
                pSrcBlock = pBlock;
            }

            moveSize = pBlock->base.address + pBlock->base.size - srcAddress;

            if (srcAddress > dstAddress &&
                (pBlock->pNext == NULL ||
                 pBlock->pNext->base.address > pBlock->base.address + pBlock->base.size ||
                 pBlock->pNext->isPinned))
            {
                dstOffset = gmmAddressToOffset(dstAddress, 0);
                srcOffset = gmmAddressToOffset(srcAddress, 0);

                totalMoveSize += moveSize;

		if (dstOffset + moveSize <= srcOffset)
		   gmmLocalMemcpy(dstOffset, srcOffset, moveSize);
		else
		{
                   uint32_t moveBlockSize = srcOffset-dstOffset;
		   uint32_t iterations = (moveSize+moveBlockSize-1)/moveBlockSize;

		   for (uint32_t i = 0; i < iterations; i++)
                      gmmLocalMemcpy(dstOffset+(i*moveBlockSize), srcOffset+(i*moveBlockSize), moveBlockSize);
		}

                pTempBlock = pSrcBlock;

                moveDistance = srcOffset - dstOffset;

                while (pTempBlock != pBlock->pNext)
                {
                    pTempBlock->base.address -= moveDistance;
                    pTempBlock = pTempBlock->pNext;
                }
            }
        }
        else
        {
            uint32_t availableSize;

            srcAddress = 0;
            dstAddress = 0;

            if (pBlock->pPrev == NULL)
                availableSize = pBlock->base.address - pAllocator->startAddress;
            else
                availableSize = pBlock->base.address - (pBlock->pPrev->base.address + pBlock->pPrev->base.size);

            pTempBlock = pBlock->pNext;

            while (availableSize >= GMM_ALIGNMENT && pTempBlock)
            {
                pTempBlockNext = pTempBlock->pNext;

                if (pTempBlock->isPinned == 0 && pTempBlock->base.size <= availableSize)
                {
                    uint32_t pinDstAddress = (pBlock->pPrev == NULL) ?
                                             pAllocator->startAddress :
                                             pBlock->pPrev->base.address + pBlock->pPrev->base.size;
                    uint32_t pinSrcAddress = pTempBlock->base.address;
                    uint32_t moveSize = pTempBlock->base.size;

                    dstOffset = gmmAddressToOffset(pinDstAddress, 0);
                    srcOffset = gmmAddressToOffset(pinSrcAddress, 0);

                    totalMoveSize += moveSize;

		    if (dstOffset + moveSize <= srcOffset)
		       gmmLocalMemcpy(dstOffset, srcOffset, moveSize);
		    else
		    {
                       uint32_t moveBlockSize = srcOffset-dstOffset;
		       uint32_t iterations = (moveSize+moveBlockSize-1)/moveBlockSize;

		       for (uint32_t i = 0; i < iterations; i++)
                          gmmLocalMemcpy(dstOffset+(i*moveBlockSize), srcOffset+(i*moveBlockSize), moveBlockSize);
		    }

                    pTempBlock->base.address = pinDstAddress;

                    if (pTempBlock == pAllocator->pTail)
                    {
                        if (pTempBlock->pNext)
                            pAllocator->pTail = pTempBlock->pNext;
                        else
                            pAllocator->pTail = pTempBlock->pPrev;
                    }

                    if (pTempBlock->pNext)
                        pTempBlock->pNext->pPrev = pTempBlock->pPrev;

                    if (pTempBlock->pPrev)
                        pTempBlock->pPrev->pNext = pTempBlock->pNext;

                    if (pBlock->pPrev)
                        pBlock->pPrev->pNext = pTempBlock;
                    else
                        pAllocator->pHead = pTempBlock;

                    pTempBlock->pPrev = pBlock->pPrev;
                    pTempBlock->pNext = pBlock;
                    pBlock->pPrev = pTempBlock;
                }

                if (pBlock->pPrev)
                    availableSize = pBlock->base.address - (pBlock->pPrev->base.address + pBlock->pPrev->base.size);

                pTempBlock = pTempBlockNext;
            }

            if (availableSize)
            {
                GmmBlock *pNewBlock = (GmmBlock *)gmmAllocFixed(0);

                if (pNewBlock)
                {
                    memset(pNewBlock, 0, sizeof(GmmBlock));
                    pNewBlock->base.address = pBlock->base.address - availableSize;
                    pNewBlock->base.isMain = pBlock->base.isMain;
                    pNewBlock->base.size = availableSize;
                    pNewBlock->pNext = pBlock;
                    pNewBlock->pPrev = pBlock->pPrev;

                    if (pBlock->pPrev)
                        pBlock->pPrev->pNext = pNewBlock;

                    pBlock->pPrev = pNewBlock;

                    if (pBlock == pAllocator->pHead)
                        pAllocator->pHead = pNewBlock;

                    gmmAddFree(pAllocator, pNewBlock);

                    ret = 1;
                }
            }
        }

        pBlock = pBlock->pNext;
    }

    uint32_t newFreeAddress = pAllocator->pTail ? 
                              pAllocator->pTail->base.address + pAllocator->pTail->base.size :
                              pAllocator->startAddress;

    if (pAllocator->freeAddress != newFreeAddress)
    {
        pAllocator->freeAddress = newFreeAddress;
        ret = 1;
    }

    pAllocator->freedSinceSweep = 0;
    pAllocator->pSweepHead = NULL;


    return ret;
}


static void gmmFreeAll (void)
{
    GmmAllocator    *pAllocator = pGmmLocalAllocator;
    GmmBlock        *pBlock;
    GmmBlock        *pTemp;

    pBlock = pAllocator->pPendingFreeHead;
    while (pBlock)
    {
        pTemp = pBlock->pNextFree;
        gmmFreeBlock(pBlock);
        pBlock = pTemp;
    }
    pAllocator->pPendingFreeHead = NULL;
    pAllocator->pPendingFreeTail = NULL;

    for (int i=0; i<GMM_NUM_FREE_BINS; i++)
    {
        pBlock = pAllocator->pFreeHead[i];
        while (pBlock)
        {
            pTemp = pBlock->pNextFree;
            gmmFreeBlock(pBlock);
            pBlock = pTemp;
        }
        pAllocator->pFreeHead[i] = NULL;
        pAllocator->pFreeTail[i] = NULL;
    }
}

static uint32_t gmmFindFreeBlock(
    GmmAllocator    *pAllocator,
    uint32_t        size
)
{
    uint32_t        retId = GMM_ERROR;
    GmmBlock        *pBlock;
    uint8_t         found = 0;
    uint8_t         freeIndex = gmmSizeToFreeIndex(size);

    pBlock = pAllocator->pFreeHead[freeIndex];

    while (freeIndex < GMM_NUM_FREE_BINS)
    {
        if (pBlock)
        {
            if (pBlock->base.size >= size)
            {
                found = 1;
                break;
            }
            pBlock = pBlock->pNextFree;
        }
        else if (++freeIndex < GMM_NUM_FREE_BINS)
            pBlock = pAllocator->pFreeHead[freeIndex];
    }

    if (found)
    {
        if (pBlock->base.size != size)
        {
            // create a new block here
            GmmBlock *pNewBlock = (GmmBlock *)gmmAllocFixed(0);
            if (pNewBlock == NULL)
                return GMM_ERROR;

            memset(pNewBlock, 0, sizeof(GmmBlock));
            pNewBlock->base.address = pBlock->base.address + size;
            pNewBlock->base.isMain = pBlock->base.isMain;
            pNewBlock->base.size = pBlock->base.size - size;
            pNewBlock->pNext = pBlock->pNext;
            pNewBlock->pPrev = pBlock;

            if (pBlock->pNext)
                pBlock->pNext->pPrev = pNewBlock;

            pBlock->pNext = pNewBlock;

            if (pBlock == pAllocator->pTail)
                pAllocator->pTail = pNewBlock;

            gmmAddFree(pAllocator, pNewBlock);
        }
        pBlock->base.size = size;

        if (pBlock == pAllocator->pFreeHead[freeIndex])
           pAllocator->pFreeHead[freeIndex] = pBlock->pNextFree;
        if (pBlock == pAllocator->pFreeTail[freeIndex])
           pAllocator->pFreeTail[freeIndex] = pBlock->pPrevFree;
        if (pBlock->pNextFree)
           pBlock->pNextFree->pPrevFree = pBlock->pPrevFree;
        if (pBlock->pPrevFree)
           pBlock->pPrevFree->pNextFree = pBlock->pNextFree;

        retId = (uint32_t)pBlock;
    }

    return retId;
}

uint32_t gmmAlloc(const uint8_t isTile, const uint32_t size)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)&_RGLState.fifo;
   GmmAllocator    *pAllocator;
   uint32_t        retId;
   uint32_t        newSize;

   if (__builtin_expect((size == 0),0))
      return GMM_ERROR;

   pAllocator = pGmmLocalAllocator;

   if (!isTile)
   {
      newSize = pad(size, GMM_ALIGNMENT);
      retId = gmmFindFreeBlock(pAllocator, newSize);
   }
   else
   {
      newSize = pad(size, GMM_TILE_ALIGNMENT);
      retId = GMM_ERROR;
   }

   if (retId == GMM_ERROR)
   {
      if (isTile)
         retId = (uint32_t)gmmAllocTileBlock(pAllocator, newSize);
      else
         retId = (uint32_t)gmmAllocBlock(pAllocator, newSize);

      if (retId == MEMORY_ALLOC_ERROR)
      {
         gmmFreeAll();

	 if (gmmInternalSweep())
	 {
            *pLock = 1;
	    cachedLockValue = 1;
	    cellGcmSetWriteBackEndLabel(thisContext, GMM_PPU_WAIT_INDEX, 0);

	    cellGcmFlush(thisContext);
	 }

	 if (isTile)
            retId = (uint32_t)gmmAllocTileBlock(pAllocator, newSize);
	 else
            retId = (uint32_t)gmmAllocBlock(pAllocator, newSize);

	 if (!isTile && retId == MEMORY_ALLOC_ERROR)
            retId = gmmFindFreeBlock(pAllocator, newSize);
      }
   }

   return retId;
}

static int _RGLLoadFPShader( _CGprogram *program )
{
   unsigned int ucodeSize = program->header.instructionCount * 16;

   if ( program->loadProgramId == GMM_ERROR )
   {
      program->loadProgramId = gmmAlloc(0, ucodeSize);
      program->loadProgramOffset = 0;
   }

   unsigned int dstId = program->loadProgramId;
   unsigned dstOffset = program->loadProgramOffset;
   const char *src = (char*)program->ucode;

   GLuint id = gmmAlloc(0, ucodeSize);

   memcpy( gmmIdToAddress(id), src, ucodeSize );
   _RGLMemcpy( dstId, dstOffset, 0, id, ucodeSize );

   gmmFree( id );
   return GL_TRUE;
}

void _RGLPlatformSetVertexRegister4fv( unsigned int reg, const float * __restrict v ) {}
void _RGLPlatformSetVertexRegisterBlock( unsigned int reg, unsigned int count, const float * __restrict v ) {}
void _RGLPlatformSetFragmentRegister4fv( unsigned int reg, const float * __restrict v ) {}
void _RGLPlatformSetFragmentRegisterBlock( unsigned int reg, unsigned int count, const float * __restrict v ) {}

template<int SIZE> inline static void swapandsetfp( int ucodeSize, unsigned int loadProgramId, unsigned int loadProgramOffset, unsigned short *ec, const unsigned int   * __restrict v )
{
   cellGcmSetTransferLocationInline ( &_RGLState.fifo, CELL_GCM_LOCATION_LOCAL);
   unsigned short count = *( ec++ );
   for ( unsigned long offsetIndex = 0; offsetIndex < count; ++offsetIndex )
   {
      void *ptr = NULL;
      const int paddedSIZE = (SIZE + 1) & ~1;
      cellGcmSetInlineTransferPointerInline( &_RGLState.fifo, gmmIdToOffset( loadProgramId ) + loadProgramOffset + *(ec++), paddedSIZE, &ptr);
      float *fp = (float*)ptr;
      float *src = (float*)v;
      for (uint32_t j=0; j<SIZE;j++)
      {
         *fp = cellGcmSwap16Float32(*src);
	 fp++;src++;
      }
   }
}

template<int SIZE> static void setVectorTypefp( CgRuntimeParameter* __restrict ptr, const void* __restrict v )
{
   float * __restrict  f = ( float* )v;
   float * __restrict  data = ( float* )ptr->pushBufferPointer;

   for ( long i = 0; i < SIZE; ++i )
      data[i] = f[i];

   _CGprogram *program = ptr->program;

   CgParameterResource *parameterResource = _RGLGetParameterResource( ptr->program, ptr->parameterEntry );
   unsigned short resource = parameterResource->resource;
   unsigned short *ec = ( unsigned short * )( ptr->program->resources ) + resource + 1;

   if ( RGL_LIKELY( *ec ) )
   {
      swapandsetfp<SIZE>(program->header.instructionCount*16, program->loadProgramId, program->loadProgramOffset, ec, (unsigned int *)data);
   }
}

template<int SIZE> static void setVectorTypeSharedfpIndex( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int ) {}

template<int SIZE> static void setVectorTypeSharedfpIndexArray( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int index ) {}

template<int SIZE> static void setVectorTypeSharedvpIndex( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int )
{
    const float * __restrict f = ( const float * __restrict )v;
    const CgParameterResource *parameterResource = _RGLGetParameterResource( ptr->program, ptr->parameterEntry );
    unsigned short resource = parameterResource->resource;
    float * __restrict dst = (float * __restrict)ptr->pushBufferPointer;
    for (long i = 0; i < SIZE; ++i)
        dst[i] = f[i];
    _RGLPlatformSetVertexRegister4fv( resource, dst );
}

template<int SIZE> static void setVectorTypeSharedvpIndexArray( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int index )
{
    const float * __restrict f = ( const float * __restrict )v;
    const CgParameterResource *parameterResource = _RGLGetParameterResource( ptr->program, ptr->parameterEntry );
    unsigned short resource = parameterResource->resource + index;
    float * __restrict dst = ( float * __restrict )ptr->pushBufferPointer;
    for ( long i = 0; i < SIZE; ++ i )
        dst[i] = f[i];
    _RGLPlatformSetVertexRegister4fv( resource, dst );
}

template <int SIZE> static void setVectorTypevpIndex( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int )
{
    PSGLcontext * LContext = _CurrentContext;
    const float * __restrict f = ( const float* )v;
    float * __restrict dst = ( float* )ptr->pushBufferPointer;
    for ( long i = 0; i < SIZE; ++ i )
        dst[i] = f[i];
    LContext->needValidate |= PSGL_VALIDATE_VERTEX_CONSTANTS;
}
template <int SIZE> static void setVectorTypevpIndexArray( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int index )
{
    PSGLcontext * LContext = _CurrentContext;
    const float * __restrict f = ( const float* )v;
    float *  __restrict dst = ( float* )( *(( unsigned int ** )ptr->pushBufferPointer + index ) );
    for ( long i = 0; i < SIZE; ++ i )
        dst[i] = f[i];
    LContext->needValidate |= PSGL_VALIDATE_VERTEX_CONSTANTS;
}
template<int SIZE> static void setVectorTypefpIndex( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int )
{
    float * __restrict  f = ( float* )v;
    float * __restrict  data = ( float* )ptr->pushBufferPointer;
    for ( long i = 0; i < SIZE; ++i )
        data[i] = f[i];
    _CGprogram *program = ptr->program;

    const CgParameterResource *parameterResource = _RGLGetParameterResource( program, ptr->parameterEntry );
    unsigned short resource = parameterResource->resource;
    unsigned short *ec = ( unsigned short * )( ptr->program->resources ) + resource + 1;
    if ( RGL_LIKELY( *ec ) )
    {
        swapandsetfp<SIZE>( program->header.instructionCount*16, program->loadProgramId, program->loadProgramOffset, ec, ( unsigned int * )data );
    }
}
template<int SIZE> static void setVectorTypefpIndexArray( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int index )
{
    float * __restrict  f = ( float* )v;
    float * __restrict  data = ( float* )ptr->pushBufferPointer;
    for ( long i = 0; i < SIZE; ++i )
        data[i] = f[i];
    _CGprogram *program = ptr->program;

    const CgParameterResource *parameterResource = _RGLGetParameterResource( program, ptr->parameterEntry );
    unsigned short resource = parameterResource->resource;
    unsigned short *ec = ( unsigned short * )( program->resources ) + resource + 1;
    int arrayIndex = index;
    while ( arrayIndex )
    {
        ec += (( *ec ) + 2 );
        arrayIndex--;
    }
    if ( RGL_LIKELY( *ec ) )
    {
        swapandsetfp<SIZE>( program->header.instructionCount*16, program->loadProgramId, program->loadProgramOffset, ec, ( unsigned int * )data );
    }
}

template <int ROWS, int COLS, int ORDER> static void setMatrixvpIndex( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int index )
{
    PSGLcontext * LContext = _CurrentContext;
    float *  __restrict f = ( float* )v;
    float *  __restrict dst = ( float* )ptr->pushBufferPointer;
    for ( long row = 0; row < ROWS; ++row )
    {
        for ( long col = 0; col < COLS; ++col )
            dst[row * 4 + col] = ( ORDER == ROW_MAJOR ) ? f[row * COLS + col] : f[col * ROWS + row];
    }
    LContext->needValidate |= PSGL_VALIDATE_VERTEX_CONSTANTS;
}

template <int ROWS, int COLS, int ORDER> static void setMatrixSharedvpIndex( CgRuntimeParameter*  __restrict ptr, const void*  __restrict v, const int /*index*/ )
{
    float * __restrict f = ( float* )v;
    float * __restrict dst = ( float* )ptr->pushBufferPointer;

    const CgParameterResource *parameterResource = _RGLGetParameterResource( ptr->program, ptr->parameterEntry );
    unsigned short resource = parameterResource->resource;

    float tmp[ROWS*4];
    for ( long row = 0; row < ROWS; ++row )
    {
        for(long col = 0; col < COLS; ++col)
           tmp[row*4 + col] = dst[row * 4 + col] = ( ORDER == ROW_MAJOR ) ? f[row * COLS + col] : f[col * ROWS + row];
        for(long col = COLS; col < 4; ++col)
           tmp[row*4 + col] = dst[row*4+col];
    }

    cellGcmSetVertexProgramParameterBlockInline( &_RGLState.fifo, resource, ROWS, (const float*)tmp);
}

template <int ROWS, int COLS, int ORDER> static void setMatrixSharedvpIndexArray( CgRuntimeParameter*  __restrict ptr, const void*  __restrict v, const int index )
{
    float * __restrict f = ( float* )v;
    float * __restrict dst = ( float* )ptr->pushBufferPointer;

    const CgParameterResource *parameterResource = _RGLGetParameterResource( ptr->program, ptr->parameterEntry );
    unsigned short resource = parameterResource->resource + index * ROWS;

    float tmp[ROWS*4];
    for ( long row = 0; row < ROWS; ++row )
    {
        for ( long col = 0; col < COLS; ++col )
        {
            tmp[row*4 + col] = dst[row * 4 + col] = ( ORDER == ROW_MAJOR ) ? f[row * COLS + col] : f[col * ROWS + row];
        }
        for ( long col = COLS; col < 4; ++col ) tmp[row*4 + col] = dst[row*4+col];
    }
    cellGcmSetVertexProgramParameterBlockInline( &_RGLState.fifo, resource, ROWS, (const float*)tmp);
}

template <int ROWS, int COLS, int ORDER> static void setMatrixSharedfpIndex( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int /*index*/ )
{
}

template <int ROWS, int COLS, int ORDER> static void setMatrixSharedfpIndexArray( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int index )
{
}

template <int ROWS, int COLS, int ORDER> static void setMatrixvpIndexArray( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int index )
{
    PSGLcontext * LContext = _CurrentContext;
    float *  __restrict f = ( float* )v;
    float *  __restrict dst = ( float* )( *(( unsigned int ** )ptr->pushBufferPointer + index ) );
    for ( long row = 0; row < ROWS; ++row )
    {
        for ( long col = 0; col < COLS; ++col )
            dst[row * 4 + col] = ( ORDER == ROW_MAJOR ) ? f[row * COLS + col] : f[col * ROWS + row];
    }
    LContext->needValidate |= PSGL_VALIDATE_VERTEX_CONSTANTS;
}
template <int ROWS, int COLS, int ORDER> static void setMatrixfpIndex( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int /*index*/ )
{
    float *  __restrict f = ( float* )v;
    float *  __restrict dst = ( float* )ptr->pushBufferPointer;
    _CGprogram *program = (( CgRuntimeParameter* )ptr )->program;
    const CgParameterResource *parameterResource = _RGLGetParameterResource( program, ptr->parameterEntry );
    unsigned short resource = parameterResource->resource;
    unsigned short *ec = ( unsigned short * )program->resources + resource + 1;
    for ( long row = 0; row < ROWS; ++row )
    {
        for ( long col = 0; col < COLS; ++col )
            dst[row * 4 + col] = ( ORDER == ROW_MAJOR ) ? f[row * COLS + col] : f[col * ROWS + row];
        int count = *ec;
        if ( RGL_LIKELY( count ) )
        {
            swapandsetfp<COLS>( program->header.instructionCount*16, program->loadProgramId, program->loadProgramOffset, ec, ( unsigned int * )dst + row * 4 );
        }
        ec += count + 2;
    }
}
template <int ROWS, int COLS, int ORDER> static void setMatrixfpIndexArray( CgRuntimeParameter* __restrict ptr, const void* __restrict v, const int index )
{
    float *  __restrict f = ( float* )v;
    float *  __restrict dst = ( float* )ptr->pushBufferPointer;
    _CGprogram *program = ptr->program;
    const CgParameterResource *parameterResource = _RGLGetParameterResource( program, ptr->parameterEntry );
    unsigned short resource = parameterResource->resource;
    unsigned short *ec = ( unsigned short * )program->resources + resource + 1;
    int arrayIndex = index * ROWS;
    while ( arrayIndex ) 
    {
        unsigned short count = ( *ec );
        ec += ( count + 2 ); 
        arrayIndex--;
    }
    for ( long row = 0; row < ROWS; ++row )
    {
        for ( long col = 0; col < COLS; ++col )
            dst[row * 4 + col] = ( ORDER == ROW_MAJOR ) ? f[row * COLS + col] : f[col * ROWS + row];
        int count = *ec;
        if ( RGL_LIKELY( count ) )
        {
            swapandsetfp<COLS>( program->header.instructionCount*16, program->loadProgramId, program->loadProgramOffset, ec, ( unsigned int * )dst + row * 4 );
        }
        ec += count + 2;
    }
}

static _cgSetArrayIndexFunction setVectorTypeIndex[2][2][2][4] =
    {
        {
            {
                {&setVectorTypevpIndex<1>, &setVectorTypevpIndex<2>, &setVectorTypevpIndex<3>, &setVectorTypevpIndex<4>, },
                {&setVectorTypefpIndex<1>, &setVectorTypefpIndex<2>, &setVectorTypefpIndex<3>, &setVectorTypefpIndex<4>, }
            },
            {
                {&setVectorTypeSharedvpIndex<1>, &setVectorTypeSharedvpIndex<2>, &setVectorTypeSharedvpIndex<3>, &setVectorTypeSharedvpIndex<4>, }, 
                {&setVectorTypeSharedfpIndex<1>, &setVectorTypeSharedfpIndex<2>, &setVectorTypeSharedfpIndex<3>, &setVectorTypeSharedfpIndex<4>, } 
            },
        },
        {
            {
                {&setVectorTypevpIndexArray<1>, &setVectorTypevpIndexArray<2>, &setVectorTypevpIndexArray<3>, &setVectorTypevpIndexArray<4>, },
                {&setVectorTypefpIndexArray<1>, &setVectorTypefpIndexArray<2>, &setVectorTypefpIndexArray<3>, &setVectorTypefpIndexArray<4>, }
            },
            {
                {&setVectorTypeSharedvpIndexArray<1>, &setVectorTypeSharedvpIndexArray<2>, &setVectorTypeSharedvpIndexArray<3>, &setVectorTypeSharedvpIndexArray<4>, }, //should be the shared
                {&setVectorTypeSharedfpIndexArray<1>, &setVectorTypeSharedfpIndexArray<2>, &setVectorTypeSharedfpIndexArray<3>, &setVectorTypeSharedfpIndexArray<4>, } //should be the shared
            },
        },
    };

static _cgSetArrayIndexFunction setMatrixTypeIndex[2][2][2][4][4][2] =
    {
        {
            {
                {
                    {{ &setMatrixvpIndex<1, 1, 0>, &setMatrixvpIndex<1, 1, 1>}, { &setMatrixvpIndex<1, 2, 0>, &setMatrixvpIndex<1, 2, 1>}, { &setMatrixvpIndex<1, 3, 0>, &setMatrixvpIndex<1, 3, 1>}, { &setMatrixvpIndex<1, 4, 0>, &setMatrixvpIndex<1, 4, 1>}},
                    {{ &setMatrixvpIndex<2, 1, 0>, &setMatrixvpIndex<2, 1, 1>}, { &setMatrixvpIndex<2, 2, 0>, &setMatrixvpIndex<2, 2, 1>}, { &setMatrixvpIndex<2, 3, 0>, &setMatrixvpIndex<2, 3, 1>}, { &setMatrixvpIndex<2, 4, 0>, &setMatrixvpIndex<2, 4, 1>}},
                    {{ &setMatrixvpIndex<3, 1, 0>, &setMatrixvpIndex<3, 1, 1>}, { &setMatrixvpIndex<3, 2, 0>, &setMatrixvpIndex<3, 2, 1>}, { &setMatrixvpIndex<3, 3, 0>, &setMatrixvpIndex<3, 3, 1>}, { &setMatrixvpIndex<3, 4, 0>, &setMatrixvpIndex<3, 4, 1>}},
                    {{ &setMatrixvpIndex<4, 1, 0>, &setMatrixvpIndex<4, 1, 1>}, { &setMatrixvpIndex<4, 2, 0>, &setMatrixvpIndex<4, 2, 1>}, { &setMatrixvpIndex<4, 3, 0>, &setMatrixvpIndex<4, 3, 1>}, { &setMatrixvpIndex<4, 4, 0>, &setMatrixvpIndex<4, 4, 1>}},
                },
                {
                    {{ &setMatrixfpIndex<1, 1, 0>, &setMatrixfpIndex<1, 1, 1>}, { &setMatrixfpIndex<1, 2, 0>, &setMatrixfpIndex<1, 2, 1>}, { &setMatrixfpIndex<1, 3, 0>, &setMatrixfpIndex<1, 3, 1>}, { &setMatrixfpIndex<1, 4, 0>, &setMatrixfpIndex<1, 4, 1>}},
                    {{ &setMatrixfpIndex<2, 1, 0>, &setMatrixfpIndex<2, 1, 1>}, { &setMatrixfpIndex<2, 2, 0>, &setMatrixfpIndex<2, 2, 1>}, { &setMatrixfpIndex<2, 3, 0>, &setMatrixfpIndex<2, 3, 1>}, { &setMatrixfpIndex<2, 4, 0>, &setMatrixfpIndex<2, 4, 1>}},
                    {{ &setMatrixfpIndex<3, 1, 0>, &setMatrixfpIndex<3, 1, 1>}, { &setMatrixfpIndex<3, 2, 0>, &setMatrixfpIndex<3, 2, 1>}, { &setMatrixfpIndex<3, 3, 0>, &setMatrixfpIndex<3, 3, 1>}, { &setMatrixfpIndex<3, 4, 0>, &setMatrixfpIndex<3, 4, 1>}},
                    {{ &setMatrixfpIndex<4, 1, 0>, &setMatrixfpIndex<4, 1, 1>}, { &setMatrixfpIndex<4, 2, 0>, &setMatrixfpIndex<4, 2, 1>}, { &setMatrixfpIndex<4, 3, 0>, &setMatrixfpIndex<4, 3, 1>}, { &setMatrixfpIndex<4, 4, 0>, &setMatrixfpIndex<4, 4, 1>}},
                },
            },
            { //should be shared
                {
                    {{ &setMatrixSharedvpIndex<1, 1, 0>, &setMatrixSharedvpIndex<1, 1, 1>}, { &setMatrixSharedvpIndex<1, 2, 0>, &setMatrixSharedvpIndex<1, 2, 1>}, { &setMatrixSharedvpIndex<1, 3, 0>, &setMatrixSharedvpIndex<1, 3, 1>}, { &setMatrixSharedvpIndex<1, 4, 0>, &setMatrixSharedvpIndex<1, 4, 1>}},
                    {{ &setMatrixSharedvpIndex<2, 1, 0>, &setMatrixSharedvpIndex<2, 1, 1>}, { &setMatrixSharedvpIndex<2, 2, 0>, &setMatrixSharedvpIndex<2, 2, 1>}, { &setMatrixSharedvpIndex<2, 3, 0>, &setMatrixSharedvpIndex<2, 3, 1>}, { &setMatrixSharedvpIndex<2, 4, 0>, &setMatrixSharedvpIndex<2, 4, 1>}},
                    {{ &setMatrixSharedvpIndex<3, 1, 0>, &setMatrixSharedvpIndex<3, 1, 1>}, { &setMatrixSharedvpIndex<3, 2, 0>, &setMatrixSharedvpIndex<3, 2, 1>}, { &setMatrixSharedvpIndex<3, 3, 0>, &setMatrixSharedvpIndex<3, 3, 1>}, { &setMatrixSharedvpIndex<3, 4, 0>, &setMatrixSharedvpIndex<3, 4, 1>}},
                    {{ &setMatrixSharedvpIndex<4, 1, 0>, &setMatrixSharedvpIndex<4, 1, 1>}, { &setMatrixSharedvpIndex<4, 2, 0>, &setMatrixSharedvpIndex<4, 2, 1>}, { &setMatrixSharedvpIndex<4, 3, 0>, &setMatrixSharedvpIndex<4, 3, 1>}, { &setMatrixSharedvpIndex<4, 4, 0>, &setMatrixSharedvpIndex<4, 4, 1>}},
                },
                {
                    {{ &setMatrixSharedfpIndex<1, 1, 0>, &setMatrixSharedfpIndex<1, 1, 1>}, { &setMatrixSharedfpIndex<1, 2, 0>, &setMatrixSharedfpIndex<1, 2, 1>}, { &setMatrixSharedfpIndex<1, 3, 0>, &setMatrixSharedfpIndex<1, 3, 1>}, { &setMatrixSharedfpIndex<1, 4, 0>, &setMatrixSharedfpIndex<1, 4, 1>}},
                    {{ &setMatrixSharedfpIndex<2, 1, 0>, &setMatrixSharedfpIndex<2, 1, 1>}, { &setMatrixSharedfpIndex<2, 2, 0>, &setMatrixSharedfpIndex<2, 2, 1>}, { &setMatrixSharedfpIndex<2, 3, 0>, &setMatrixSharedfpIndex<2, 3, 1>}, { &setMatrixSharedfpIndex<2, 4, 0>, &setMatrixSharedfpIndex<2, 4, 1>}},
                    {{ &setMatrixSharedfpIndex<3, 1, 0>, &setMatrixSharedfpIndex<3, 1, 1>}, { &setMatrixSharedfpIndex<3, 2, 0>, &setMatrixSharedfpIndex<3, 2, 1>}, { &setMatrixSharedfpIndex<3, 3, 0>, &setMatrixSharedfpIndex<3, 3, 1>}, { &setMatrixSharedfpIndex<3, 4, 0>, &setMatrixSharedfpIndex<3, 4, 1>}},
                    {{ &setMatrixSharedfpIndex<4, 1, 0>, &setMatrixSharedfpIndex<4, 1, 1>}, { &setMatrixSharedfpIndex<4, 2, 0>, &setMatrixSharedfpIndex<4, 2, 1>}, { &setMatrixSharedfpIndex<4, 3, 0>, &setMatrixSharedfpIndex<4, 3, 1>}, { &setMatrixSharedfpIndex<4, 4, 0>, &setMatrixSharedfpIndex<4, 4, 1>}},
                },
            },
        },
        {
            {
                {
                    {{ &setMatrixvpIndexArray<1, 1, 0>, &setMatrixvpIndexArray<1, 1, 1>}, { &setMatrixvpIndexArray<1, 2, 0>, &setMatrixvpIndexArray<1, 2, 1>}, { &setMatrixvpIndexArray<1, 3, 0>, &setMatrixvpIndexArray<1, 3, 1>}, { &setMatrixvpIndexArray<1, 4, 0>, &setMatrixvpIndexArray<1, 4, 1>}},
                    {{ &setMatrixvpIndexArray<2, 1, 0>, &setMatrixvpIndexArray<2, 1, 1>}, { &setMatrixvpIndexArray<2, 2, 0>, &setMatrixvpIndexArray<2, 2, 1>}, { &setMatrixvpIndexArray<2, 3, 0>, &setMatrixvpIndexArray<2, 3, 1>}, { &setMatrixvpIndexArray<2, 4, 0>, &setMatrixvpIndexArray<2, 4, 1>}},
                    {{ &setMatrixvpIndexArray<3, 1, 0>, &setMatrixvpIndexArray<3, 1, 1>}, { &setMatrixvpIndexArray<3, 2, 0>, &setMatrixvpIndexArray<3, 2, 1>}, { &setMatrixvpIndexArray<3, 3, 0>, &setMatrixvpIndexArray<3, 3, 1>}, { &setMatrixvpIndexArray<3, 4, 0>, &setMatrixvpIndexArray<3, 4, 1>}},
                    {{ &setMatrixvpIndexArray<4, 1, 0>, &setMatrixvpIndexArray<4, 1, 1>}, { &setMatrixvpIndexArray<4, 2, 0>, &setMatrixvpIndexArray<4, 2, 1>}, { &setMatrixvpIndexArray<4, 3, 0>, &setMatrixvpIndexArray<4, 3, 1>}, { &setMatrixvpIndexArray<4, 4, 0>, &setMatrixvpIndexArray<4, 4, 1>}},
                },
                {
                    {{ &setMatrixfpIndexArray<1, 1, 0>, &setMatrixfpIndexArray<1, 1, 1>}, { &setMatrixfpIndexArray<1, 2, 0>, &setMatrixfpIndexArray<1, 2, 1>}, { &setMatrixfpIndexArray<1, 3, 0>, &setMatrixfpIndexArray<1, 3, 1>}, { &setMatrixfpIndexArray<1, 4, 0>, &setMatrixfpIndexArray<1, 4, 1>}},
                    {{ &setMatrixfpIndexArray<2, 1, 0>, &setMatrixfpIndexArray<2, 1, 1>}, { &setMatrixfpIndexArray<2, 2, 0>, &setMatrixfpIndexArray<2, 2, 1>}, { &setMatrixfpIndexArray<2, 3, 0>, &setMatrixfpIndexArray<2, 3, 1>}, { &setMatrixfpIndexArray<2, 4, 0>, &setMatrixfpIndexArray<2, 4, 1>}},
                    {{ &setMatrixfpIndexArray<3, 1, 0>, &setMatrixfpIndexArray<3, 1, 1>}, { &setMatrixfpIndexArray<3, 2, 0>, &setMatrixfpIndexArray<3, 2, 1>}, { &setMatrixfpIndexArray<3, 3, 0>, &setMatrixfpIndexArray<3, 3, 1>}, { &setMatrixfpIndexArray<3, 4, 0>, &setMatrixfpIndexArray<3, 4, 1>}},
                    {{ &setMatrixfpIndexArray<4, 1, 0>, &setMatrixfpIndexArray<4, 1, 1>}, { &setMatrixfpIndexArray<4, 2, 0>, &setMatrixfpIndexArray<4, 2, 1>}, { &setMatrixfpIndexArray<4, 3, 0>, &setMatrixfpIndexArray<4, 3, 1>}, { &setMatrixfpIndexArray<4, 4, 0>, &setMatrixfpIndexArray<4, 4, 1>}},
                },
            },
            { //should be shared
                {
                    {{ &setMatrixSharedvpIndexArray<1, 1, 0>, &setMatrixSharedvpIndexArray<1, 1, 1>}, { &setMatrixSharedvpIndexArray<1, 2, 0>, &setMatrixSharedvpIndexArray<1, 2, 1>}, { &setMatrixSharedvpIndexArray<1, 3, 0>, &setMatrixSharedvpIndexArray<1, 3, 1>}, { &setMatrixSharedvpIndexArray<1, 4, 0>, &setMatrixSharedvpIndexArray<1, 4, 1>}},
                    {{ &setMatrixSharedvpIndexArray<2, 1, 0>, &setMatrixSharedvpIndexArray<2, 1, 1>}, { &setMatrixSharedvpIndexArray<2, 2, 0>, &setMatrixSharedvpIndexArray<2, 2, 1>}, { &setMatrixSharedvpIndexArray<2, 3, 0>, &setMatrixSharedvpIndexArray<2, 3, 1>}, { &setMatrixSharedvpIndexArray<2, 4, 0>, &setMatrixSharedvpIndexArray<2, 4, 1>}},
                    {{ &setMatrixSharedvpIndexArray<3, 1, 0>, &setMatrixSharedvpIndexArray<3, 1, 1>}, { &setMatrixSharedvpIndexArray<3, 2, 0>, &setMatrixSharedvpIndexArray<3, 2, 1>}, { &setMatrixSharedvpIndexArray<3, 3, 0>, &setMatrixSharedvpIndexArray<3, 3, 1>}, { &setMatrixSharedvpIndexArray<3, 4, 0>, &setMatrixSharedvpIndexArray<3, 4, 1>}},
                    {{ &setMatrixSharedvpIndexArray<4, 1, 0>, &setMatrixSharedvpIndexArray<4, 1, 1>}, { &setMatrixSharedvpIndexArray<4, 2, 0>, &setMatrixSharedvpIndexArray<4, 2, 1>}, { &setMatrixSharedvpIndexArray<4, 3, 0>, &setMatrixSharedvpIndexArray<4, 3, 1>}, { &setMatrixSharedvpIndexArray<4, 4, 0>, &setMatrixSharedvpIndexArray<4, 4, 1>}},
                },
                {
                    {{ &setMatrixSharedfpIndexArray<1, 1, 0>, &setMatrixSharedfpIndexArray<1, 1, 1>}, { &setMatrixSharedfpIndexArray<1, 2, 0>, &setMatrixSharedfpIndexArray<1, 2, 1>}, { &setMatrixSharedfpIndexArray<1, 3, 0>, &setMatrixSharedfpIndexArray<1, 3, 1>}, { &setMatrixSharedfpIndexArray<1, 4, 0>, &setMatrixSharedfpIndexArray<1, 4, 1>}},
                    {{ &setMatrixSharedfpIndexArray<2, 1, 0>, &setMatrixSharedfpIndexArray<2, 1, 1>}, { &setMatrixSharedfpIndexArray<2, 2, 0>, &setMatrixSharedfpIndexArray<2, 2, 1>}, { &setMatrixSharedfpIndexArray<2, 3, 0>, &setMatrixSharedfpIndexArray<2, 3, 1>}, { &setMatrixSharedfpIndexArray<2, 4, 0>, &setMatrixSharedfpIndexArray<2, 4, 1>}},
                    {{ &setMatrixSharedfpIndexArray<3, 1, 0>, &setMatrixSharedfpIndexArray<3, 1, 1>}, { &setMatrixSharedfpIndexArray<3, 2, 0>, &setMatrixSharedfpIndexArray<3, 2, 1>}, { &setMatrixSharedfpIndexArray<3, 3, 0>, &setMatrixSharedfpIndexArray<3, 3, 1>}, { &setMatrixSharedfpIndexArray<3, 4, 0>, &setMatrixSharedfpIndexArray<3, 4, 1>}},
                    {{ &setMatrixSharedfpIndexArray<4, 1, 0>, &setMatrixSharedfpIndexArray<4, 1, 1>}, { &setMatrixSharedfpIndexArray<4, 2, 0>, &setMatrixSharedfpIndexArray<4, 2, 1>}, { &setMatrixSharedfpIndexArray<4, 3, 0>, &setMatrixSharedfpIndexArray<4, 3, 1>}, { &setMatrixSharedfpIndexArray<4, 4, 0>, &setMatrixSharedfpIndexArray<4, 4, 1>}},
                },
            },
        }
    };

_cgSetArrayIndexFunction getVectorTypeIndexSetterFunction( unsigned short a, unsigned short b, unsigned short c, unsigned short d )
{
    return setVectorTypeIndex[a][b][c][d];
}

_cgSetArrayIndexFunction getMatrixTypeIndexSetterFunction( unsigned short a, unsigned short b, unsigned short c, unsigned short d, unsigned short e, unsigned short f )
{
    return setMatrixTypeIndex[a][b][c][d][e][f];
}

static void _RGLBindTextureInternal( jsTextureImageUnit *unit, GLuint name)
{
   PSGLcontext*	LContext = _CurrentContext;
   jsTexture *texture = NULL;
   if ( name )
   {
      _RGLTexNameSpaceCreateNameLazy( &LContext->textureNameSpace, name );
      texture = ( jsTexture * )LContext->textureNameSpace.data[name];
      texture->target = GL_TEXTURE_2D;
   }

   unit->bound2D = name;

   unit->currentTexture = _RGLGetCurrentTexture( unit, GL_TEXTURE_2D );
   LContext->needValidate |= PSGL_VALIDATE_TEXTURES_USED;
}

static void setSamplerfp( CgRuntimeParameter*ptr, const void*v, int )
{
   _CGprogram *program = (( CgRuntimeParameter* )ptr )->program;
   const CgParameterResource *parameterResource = _RGLGetParameterResource( program, (( CgRuntimeParameter* )ptr )->parameterEntry );

   if ( v )
      *( GLuint* )ptr->pushBufferPointer = *( GLuint* )v;
   else
   {
      jsTextureImageUnit *unit = _CurrentContext->TextureImageUnits + ( parameterResource->resource - CG_TEXUNIT0 );
      _RGLBindTextureInternal( unit, *( GLuint* )ptr->pushBufferPointer);
   }
}

static void setSamplervp( CgRuntimeParameter*ptr, const void*v, int )
{
    _CGprogram *program = (( CgRuntimeParameter* )ptr )->program;
    const CgParameterResource *parameterResource = _RGLGetParameterResource( program, (( CgRuntimeParameter* )ptr )->parameterEntry );

    if ( v )
    {
        *( GLuint* )ptr->pushBufferPointer = *( GLuint* )v;
    }
    else
    {
	    GLuint unit = parameterResource->resource - CG_TEXUNIT0;
	    GLuint name = *( GLuint* )ptr->pushBufferPointer;
	    PSGLcontext *LContext = _CurrentContext;
	    jsTexture *texture = NULL;

	    if ( name && (name < LContext->textureNameSpace.capacity) )
		    texture = ( jsTexture * )LContext->textureNameSpace.data[name];

	    LContext->VertexTextureImages[unit] = texture;
	    LContext->needValidate |= PSGL_VALIDATE_VERTEX_TEXTURES_USED;
    }
}

static void _RGLCreatePushBuffer( _CGprogram *program )
{
    int bufferSize = 0;
    int programPushBufferPointersSize = 0;
    int extraStorageInWords = 0;
    int offsetCount = 0;
    int samplerCount = 0;
    int profileIndex = ( program->header.profile == CG_PROFILE_SCE_FP_TYPEB ||
                         program->header.profile == CG_PROFILE_SCE_FP_RSX ) ? FRAGMENT_PROFILE_INDEX : VERTEX_PROFILE_INDEX;

    bool hasSharedParams = false;
    int arrayCount = 1;
    for ( int i = 0;i < program->rtParametersCount;i++ )
    {
        const CgParameterEntry *parameterEntry = program->parametersEntries + i;

        if (( parameterEntry->flags & CGP_STRUCTURE ) || ( parameterEntry->flags & CGP_UNROLLED ) )
        {
            arrayCount = 1;
            continue;
        }

        if (( parameterEntry->flags & CGPF_REFERENCED ) )
        {
            if ( parameterEntry->flags & CGP_ARRAY )
            {
                const CgParameterArray *parameterArray = _RGLGetParameterArray( program, parameterEntry );
                arrayCount = _RGLGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount );
                continue;
            }
            if (( parameterEntry->flags & CGPV_MASK ) == CGPV_UNIFORM )
            {
                const CgParameterResource *parameterResource = _RGLGetParameterResource( program, parameterEntry );
                if ( parameterResource->type >= CG_SAMPLER1D &&  parameterResource->type <= CG_SAMPLERCUBE )
                {
                    offsetCount += arrayCount;
                    samplerCount += arrayCount;
                }
                else if ( profileIndex == VERTEX_PROFILE_INDEX )
                {
                    if ( parameterResource->type == CGP_SCF_BOOL )
                    {
                    }
                    else if ( !( parameterEntry->flags & CGPF_SHARED ) )
                    {
                        int registerStride = isMatrix(( CGtype )parameterResource->type ) ? _RGLGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
                        if ( parameterEntry->flags & CGP_CONTIGUOUS )
                            bufferSize += 3 + 4 * arrayCount * registerStride;
                        else
                        {
                            programPushBufferPointersSize += arrayCount;
                            int resourceIndex = parameterResource->resource;
                            int referencedSize = 3 + 4 * registerStride;
                            int notReferencedSize = 4 * registerStride;
                            for ( int j = 0;j < arrayCount;j++, resourceIndex += registerStride )
                            {
                                if ( program->resources[resourceIndex] != 0xffff )
                                    bufferSize += referencedSize;
                                else
                                    extraStorageInWords += notReferencedSize;
                            }
                        }
                    }
                    else
                    {
                        hasSharedParams = true;
                        if ( !( parameterEntry->flags & CGP_CONTIGUOUS ) )
                        {
                            programPushBufferPointersSize += arrayCount;
                        }
                    }
                }
                else
                {
                    int registerStride = isMatrix(( CGtype )parameterResource->type ) ? _RGLGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
                    if ( !( parameterEntry->flags & CGPF_SHARED ) )
                    {
                        extraStorageInWords += 4 * arrayCount * registerStride;
                    }
                    else
                    {
                        hasSharedParams = true;
                        unsigned short *resource = program->resources + parameterResource->resource;
                        for ( int j = 0;j < arrayCount*registerStride;j++ )
                        {
                            resource++;
                            unsigned short count = *resource++;
                            bufferSize += 24 * count;
                            resource += count;
                        }
                    }
                }
            }
        }
        arrayCount = 1;
    }

    if (( profileIndex == FRAGMENT_PROFILE_INDEX ) && ( hasSharedParams ) )
        bufferSize += 8 + 3 + 2;

    bufferSize = _RGLPad( bufferSize, 4 );

    unsigned int storageSizeInWords = bufferSize + extraStorageInWords;
    if ( storageSizeInWords )
        program->memoryBlock = ( unsigned int* )memalign( 16, storageSizeInWords * 4 );
    else
        program->memoryBlock = NULL;

    program->samplerCount = samplerCount;
    if ( samplerCount )
    {
        program->samplerValuesLocation = ( GLuint* )malloc( samplerCount * sizeof( GLuint ) );
        program->samplerIndices = ( GLuint* )malloc( samplerCount * sizeof( GLuint ) );
        program->samplerUnits = ( GLuint* )malloc( samplerCount * sizeof( GLuint ) );
    }
    else
    {
        program->samplerValuesLocation = NULL;
        program->samplerIndices = NULL;
        program->samplerUnits = NULL;
    }

    GLuint *samplerValuesLocation = program->samplerValuesLocation;
    GLuint *samplerIndices = program->samplerIndices;
    GLuint *samplerUnits = program->samplerUnits;

    if ( programPushBufferPointersSize )
        program->constantPushBufferPointers = ( unsigned int** )malloc( programPushBufferPointersSize * 4 );
    else
        program->constantPushBufferPointers = NULL;

    uint32_t *RGLCurrent = ( uint32_t * )program->memoryBlock;
    program->constantPushBuffer = ( bufferSize > 0 ) ? ( unsigned int * )RGLCurrent : NULL;
    unsigned int **programPushBuffer = program->constantPushBufferPointers;
    program->constantPushBufferWordSize = bufferSize;
    GLuint *currentStorage = ( GLuint * )( RGLCurrent + bufferSize );

    arrayCount = 1;
    const CgParameterEntry *containerEntry = NULL;
    for ( int i = 0;i < program->rtParametersCount;i++ )
    {
        CgRuntimeParameter *rtParameter = program->runtimeParameters + i;
        const CgParameterEntry *parameterEntry = program->parametersEntries + i;
        if ( containerEntry == NULL )
            containerEntry = parameterEntry;

        rtParameter->samplerSetter = _cgRaiseInvalidParamIndex;
        rtParameter->setterIndex = _cgRaiseInvalidParamIndex;
        rtParameter->setterrIndex = _cgRaiseNotMatrixParamIndex;
        rtParameter->settercIndex = _cgRaiseNotMatrixParamIndex;

        CGparameter id = ( CGparameter )_RGLCreateName( &_CurrentContext->cgParameterNameSpace, ( void* )rtParameter );
        if ( !id )
            break;

        rtParameter->id = id;
        rtParameter->parameterEntry = parameterEntry;
        rtParameter->program = program;

        if (( parameterEntry->flags & CGP_STRUCTURE ) || ( parameterEntry->flags & CGP_UNROLLED ) )
        {
            arrayCount = 1;
            containerEntry = NULL;
            continue;
        }

        if ( parameterEntry->flags & CGPF_REFERENCED )
        {
            if ( parameterEntry->flags & CGP_ARRAY )
            {
                const CgParameterArray *parameterArray = _RGLGetParameterArray( program, parameterEntry );
                arrayCount = _RGLGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount );
                continue;
            }
            if (( parameterEntry->flags & CGPV_MASK ) == CGPV_UNIFORM )
            {
                rtParameter->glType = GL_NONE;
                const CgParameterResource *parameterResource = _RGLGetParameterResource( program, parameterEntry );
                if ( parameterResource->type >= CG_SAMPLER1D && parameterResource->type <= CG_SAMPLERCUBE )
                {
                    rtParameter->pushBufferPointer = samplerValuesLocation;

                    *samplerValuesLocation = 0;
                    samplerValuesLocation++;

                    *samplerIndices = i;
                    samplerIndices++;
                    *samplerUnits = parameterResource->resource - CG_TEXUNIT0;
                    samplerUnits++;

                    if ( profileIndex == VERTEX_PROFILE_INDEX )
                    {
                        rtParameter->setterIndex = _cgIgnoreSetParamIndex;
                        rtParameter->samplerSetter = setSamplervp;
                    }
                    else
                    {
                        rtParameter->samplerSetter = setSamplerfp;
                    }
                    rtParameter->glType = _RGLCgGetSamplerGLTypeFromCgType(( CGtype )( parameterResource->type ) );
                }
                else
                {
                    if ( profileIndex == VERTEX_PROFILE_INDEX )
                    {
                        if ( parameterResource->type == CGP_SCF_BOOL )
                        {
                        }
                        else if ( !( parameterEntry->flags & CGPF_SHARED ) )
                        {
                            int registerStride = isMatrix(( CGtype )parameterResource->type ) ? _RGLGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
                            int registerCount = arrayCount * registerStride;
                            if ( parameterEntry->flags & CGP_CONTIGUOUS )
                            {
                                memset( RGLCurrent, 0, 4*( 4*registerCount + 3 ) );
                                GCM_FUNC_BUFFERED( cellGcmSetVertexProgramParameterBlock, RGLCurrent, parameterResource->resource, registerCount, ( float* )RGLCurrent );
                                rtParameter->pushBufferPointer = RGLCurrent - 4 * registerCount;
                            }
                            else
                            {
                                rtParameter->pushBufferPointer = programPushBuffer;
                                int resourceIndex = parameterResource->resource;
                                for ( int j = 0;j < arrayCount;j++, resourceIndex += registerStride )
                                {
                                    if ( program->resources[resourceIndex] != 0xffff )
                                    {
                                        memset( RGLCurrent, 0, 4*( 4*registerStride + 3 ) );
                                        GCM_FUNC_BUFFERED( cellGcmSetVertexProgramParameterBlock, RGLCurrent, program->resources[resourceIndex], registerStride, ( float* )RGLCurrent );
                                        *( programPushBuffer++ ) = ( unsigned int* )( RGLCurrent - 4 * registerStride );
                                    }
                                    else
                                    {
                                        *( programPushBuffer++ ) = ( unsigned int* )currentStorage;
                                        currentStorage += 4 * registerStride;
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        if ( parameterEntry->flags & CGPF_SHARED )
                        {
                            rtParameter->pushBufferPointer = NULL;
                        }
                        else
                        {
                            int registerStride = isMatrix(( CGtype )parameterResource->type ) ? _RGLGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
                            int registerCount = arrayCount * registerStride;
                            rtParameter->pushBufferPointer = currentStorage;
                            currentStorage += 4 * registerCount;
                        }
                    }

                    switch ( parameterResource->type )
                    {
                        case CG_FLOAT:
                        case CG_FLOAT1: case CG_FLOAT2: case CG_FLOAT3: case CG_FLOAT4:
                        {
                            unsigned int floatCount = _RGLCountFloatsInCgType(( CGtype )parameterResource->type );
                            rtParameter->setterIndex = setVectorTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][( containerEntry->flags&CGPF_SHARED ) ? 1 : 0][profileIndex][floatCount - 1];
                        }
                        break;
                        case CG_FLOAT1x1: case CG_FLOAT1x2: case CG_FLOAT1x3: case CG_FLOAT1x4:
                        case CG_FLOAT2x1: case CG_FLOAT2x2: case CG_FLOAT2x3: case CG_FLOAT2x4:
                        case CG_FLOAT3x1: case CG_FLOAT3x2: case CG_FLOAT3x3: case CG_FLOAT3x4:
                        case CG_FLOAT4x1: case CG_FLOAT4x2: case CG_FLOAT4x3: case CG_FLOAT4x4:
                            rtParameter->setterrIndex = setMatrixTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][( containerEntry->flags&CGPF_SHARED ) ? 1 : 0][profileIndex][_RGLGetTypeRowCount(( CGtype )parameterResource->type ) - 1][_RGLGetTypeColCount(( CGtype )parameterResource->type ) - 1][ROW_MAJOR];
                            rtParameter->settercIndex = setMatrixTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][( containerEntry->flags&CGPF_SHARED ) ? 1 : 0][profileIndex][_RGLGetTypeRowCount(( CGtype )parameterResource->type ) - 1][_RGLGetTypeColCount(( CGtype )parameterResource->type ) - 1][COL_MAJOR];
                            break;
                        case CG_SAMPLER1D: case CG_SAMPLER2D: case CG_SAMPLER3D: case CG_SAMPLERRECT: case CG_SAMPLERCUBE:
                            break;
                        case CGP_SCF_BOOL:
                            break;
                        case CG_HALF:
                        case CG_HALF1: case CG_HALF2: case CG_HALF3: case CG_HALF4:
                        case CG_INT:
                        case CG_INT1: case CG_INT2: case CG_INT3: case CG_INT4:
                        case CG_BOOL:
                        case CG_BOOL1: case CG_BOOL2: case CG_BOOL3: case CG_BOOL4:
                        case CG_FIXED:
                        case CG_FIXED1: case CG_FIXED2: case CG_FIXED3: case CG_FIXED4:
                        {
                            unsigned int floatCount = _RGLCountFloatsInCgType(( CGtype )parameterResource->type );
                            rtParameter->setterIndex = setVectorTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][( containerEntry->flags&CGPF_SHARED ) ? 1 : 0][profileIndex][floatCount - 1];
                        }
                        break;
                        case CG_HALF1x1: case CG_HALF1x2: case CG_HALF1x3: case CG_HALF1x4:
                        case CG_HALF2x1: case CG_HALF2x2: case CG_HALF2x3: case CG_HALF2x4:
                        case CG_HALF3x1: case CG_HALF3x2: case CG_HALF3x3: case CG_HALF3x4:
                        case CG_HALF4x1: case CG_HALF4x2: case CG_HALF4x3: case CG_HALF4x4:
                        case CG_INT1x1: case CG_INT1x2: case CG_INT1x3: case CG_INT1x4:
                        case CG_INT2x1: case CG_INT2x2: case CG_INT2x3: case CG_INT2x4:
                        case CG_INT3x1: case CG_INT3x2: case CG_INT3x3: case CG_INT3x4:
                        case CG_INT4x1: case CG_INT4x2: case CG_INT4x3: case CG_INT4x4:
                        case CG_BOOL1x1: case CG_BOOL1x2: case CG_BOOL1x3: case CG_BOOL1x4:
                        case CG_BOOL2x1: case CG_BOOL2x2: case CG_BOOL2x3: case CG_BOOL2x4:
                        case CG_BOOL3x1: case CG_BOOL3x2: case CG_BOOL3x3: case CG_BOOL3x4:
                        case CG_BOOL4x1: case CG_BOOL4x2: case CG_BOOL4x3: case CG_BOOL4x4:
                        case CG_FIXED1x1: case CG_FIXED1x2: case CG_FIXED1x3: case CG_FIXED1x4:
                        case CG_FIXED2x1: case CG_FIXED2x2: case CG_FIXED2x3: case CG_FIXED2x4:
                        case CG_FIXED3x1: case CG_FIXED3x2: case CG_FIXED3x3: case CG_FIXED3x4:
                        case CG_FIXED4x1: case CG_FIXED4x2: case CG_FIXED4x3: case CG_FIXED4x4:
                            rtParameter->setterrIndex = setMatrixTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][( containerEntry->flags&CGPF_SHARED ) ? 1 : 0][profileIndex][_RGLGetTypeRowCount(( CGtype )parameterResource->type ) - 1][_RGLGetTypeColCount(( CGtype )parameterResource->type ) - 1][ROW_MAJOR];
                            rtParameter->settercIndex = setMatrixTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][( containerEntry->flags&CGPF_SHARED ) ? 1 : 0][profileIndex][_RGLGetTypeRowCount(( CGtype )parameterResource->type ) - 1][_RGLGetTypeColCount(( CGtype )parameterResource->type ) - 1][COL_MAJOR];
                            break;
			case CG_STRING:
			    break;
                        default:
                            break;
                    }
                }
            }
        }
        else
        {
            if (( parameterEntry->flags & CGPV_MASK ) == CGPV_UNIFORM )
            {
                if ( parameterEntry->flags & CGP_ARRAY )
                    continue;

                const CgParameterResource *parameterResource = _RGLGetParameterResource( program, parameterEntry );
                switch ( parameterResource->type )
                {
                    case CG_FLOAT:
                    case CG_FLOAT1: case CG_FLOAT2: case CG_FLOAT3: case CG_FLOAT4:
                        rtParameter->setterIndex = _cgIgnoreSetParamIndex;
                        break;
                    case CG_FLOAT1x1: case CG_FLOAT1x2: case CG_FLOAT1x3: case CG_FLOAT1x4:
                    case CG_FLOAT2x1: case CG_FLOAT2x2: case CG_FLOAT2x3: case CG_FLOAT2x4:
                    case CG_FLOAT3x1: case CG_FLOAT3x2: case CG_FLOAT3x3: case CG_FLOAT3x4:
                    case CG_FLOAT4x1: case CG_FLOAT4x2: case CG_FLOAT4x3: case CG_FLOAT4x4:
                        rtParameter->setterrIndex = _cgIgnoreSetParamIndex;
                        rtParameter->settercIndex = _cgIgnoreSetParamIndex;
                        break;
                    case CG_SAMPLER1D: case CG_SAMPLER2D: case CG_SAMPLER3D: case CG_SAMPLERRECT: case CG_SAMPLERCUBE:
                        rtParameter->samplerSetter = _cgIgnoreSetParamIndex;
                        break;
                    case CGP_SCF_BOOL:
                        break;
		    case CG_HALF:
                    case CG_HALF1: case CG_HALF2: case CG_HALF3: case CG_HALF4:
		    case CG_INT:
                    case CG_INT1: case CG_INT2: case CG_INT3: case CG_INT4:
		    case CG_BOOL:
                    case CG_BOOL1: case CG_BOOL2: case CG_BOOL3: case CG_BOOL4:
		    case CG_FIXED:
                    case CG_FIXED1: case CG_FIXED2: case CG_FIXED3: case CG_FIXED4:
                        rtParameter->setterIndex = _cgIgnoreSetParamIndex;
                        break;
                    case CG_HALF1x1: case CG_HALF1x2: case CG_HALF1x3: case CG_HALF1x4:
                    case CG_HALF2x1: case CG_HALF2x2: case CG_HALF2x3: case CG_HALF2x4:
                    case CG_HALF3x1: case CG_HALF3x2: case CG_HALF3x3: case CG_HALF3x4:
                    case CG_HALF4x1: case CG_HALF4x2: case CG_HALF4x3: case CG_HALF4x4:
                    case CG_INT1x1: case CG_INT1x2: case CG_INT1x3: case CG_INT1x4:
                    case CG_INT2x1: case CG_INT2x2: case CG_INT2x3: case CG_INT2x4:
                    case CG_INT3x1: case CG_INT3x2: case CG_INT3x3: case CG_INT3x4:
                    case CG_INT4x1: case CG_INT4x2: case CG_INT4x3: case CG_INT4x4:
                    case CG_BOOL1x1: case CG_BOOL1x2: case CG_BOOL1x3: case CG_BOOL1x4:
                    case CG_BOOL2x1: case CG_BOOL2x2: case CG_BOOL2x3: case CG_BOOL2x4:
                    case CG_BOOL3x1: case CG_BOOL3x2: case CG_BOOL3x3: case CG_BOOL3x4:
                    case CG_BOOL4x1: case CG_BOOL4x2: case CG_BOOL4x3: case CG_BOOL4x4:
                    case CG_FIXED1x1: case CG_FIXED1x2: case CG_FIXED1x3: case CG_FIXED1x4:
                    case CG_FIXED2x1: case CG_FIXED2x2: case CG_FIXED2x3: case CG_FIXED2x4:
                    case CG_FIXED3x1: case CG_FIXED3x2: case CG_FIXED3x3: case CG_FIXED3x4:
                    case CG_FIXED4x1: case CG_FIXED4x2: case CG_FIXED4x3: case CG_FIXED4x4:
                        rtParameter->setterrIndex = _cgIgnoreSetParamIndex;
                        rtParameter->settercIndex = _cgIgnoreSetParamIndex;
                        break;
		    case CG_STRING:
			break;
                    default:
                        break;
                }
            }
        }
        arrayCount = 1;
        containerEntry = NULL;
    }

    if ( bufferSize > 0 )
    {
      int nopCount = ( program->constantPushBuffer + bufferSize ) - ( unsigned int * )RGLCurrent;
      GCM_FUNC_BUFFERED( cellGcmSetNopCommand, RGLCurrent, nopCount );
    }
}

static int _RGLGenerateProgram( _CGprogram *program, int profileIndex, const CgProgramHeader *programHeader, const void *ucode, const CgParameterTableHeader *parameterHeader,
                           const CgParameterEntry *parameterEntries, const char *stringTable, const float *defaultValues )
{
    CGprofile profile = ( CGprofile )programHeader->profile;

    int need_swapping = 0;

    if ( profile == ( CGprofile )7005 )
        profile = CG_PROFILE_SCE_VP_RSX;
    if ( profile == ( CGprofile )7006 )
        profile = CG_PROFILE_SCE_FP_RSX;

    if (( profile != CG_PROFILE_SCE_FP_TYPEB ) && ( profile != CG_PROFILE_SCE_VP_TYPEB ) &&
            ( profile != CG_PROFILE_SCE_FP_RSX ) && ( profile != CG_PROFILE_SCE_VP_RSX ) )
    {
        need_swapping = 1;
    }

    int invalidProfile = 0;
    switch ( ENDIAN_32( profile, need_swapping ) )
    {
        case CG_PROFILE_SCE_VP_TYPEB:
            if ( profileIndex != VERTEX_PROFILE_INDEX ) invalidProfile = 1;
            break;
        case CG_PROFILE_SCE_FP_TYPEB:
            if ( profileIndex != FRAGMENT_PROFILE_INDEX ) invalidProfile = 1;
            break;
        case CG_PROFILE_SCE_VP_RSX:
            if ( profileIndex != VERTEX_PROFILE_INDEX ) invalidProfile = 1;
            break;
        case CG_PROFILE_SCE_FP_RSX:
            if ( profileIndex != FRAGMENT_PROFILE_INDEX ) invalidProfile = 1;
            break;
        default:
            invalidProfile = 1;
            break;
    }
    if ( invalidProfile )
    {
        _RGLCgRaiseError( CG_UNKNOWN_PROFILE_ERROR );
        return 0;
    }

    memcpy( &program->header, programHeader, sizeof( program->header ) );

    program->ucode = ucode;
    program->loadProgramId = GMM_ERROR;
    program->loadProgramOffset = 0;

    size_t parameterSize = parameterHeader->entryCount * sizeof( CgRuntimeParameter );
    void *memoryBlock;
    if ( parameterSize )
        memoryBlock = memalign( 16, parameterSize );
    else
        memoryBlock = NULL;

    program->rtParametersCount = parameterHeader->entryCount;
    program->runtimeParameters = ( CgRuntimeParameter* )memoryBlock;

    if ( parameterEntries == NULL )
        parameterEntries = ( CgParameterEntry* )( parameterHeader + 1 );

    program->parametersEntries = parameterEntries;
    program->parameterResources = ( char* )( program->parametersEntries + program->rtParametersCount );
    program->resources = ( unsigned short* )(( char* )program->parametersEntries + ( parameterHeader->resourceTableOffset - sizeof( CgParameterTableHeader ) ) );
    program->defaultValuesIndexCount = parameterHeader->defaultValueIndexCount;
    program->defaultValuesIndices = ( CgParameterDefaultValue* )(( char* )program->parametersEntries + ( parameterHeader->defaultValueIndexTableOffset - sizeof( CgParameterTableHeader ) ) );

    program->defaultValues = NULL;

    memset( program->runtimeParameters, 0, parameterHeader->entryCount*sizeof( CgRuntimeParameter ) );

    program->stringTable = stringTable;
    program->defaultValues = defaultValues;

    _RGLCreatePushBuffer( program );

    int count = program->defaultValuesIndexCount;
    if ( profileIndex == FRAGMENT_PROFILE_INDEX)
    {
	    for ( int i = 0; i < count;i++ )
	    {
		    const void * __restrict pItemDefaultValues = program->defaultValues + program->defaultValuesIndices[i].defaultValueIndex;
		    const unsigned int * itemDefaultValues = ( const unsigned int * )pItemDefaultValues;
		    int index = ( int )program->defaultValuesIndices[i].entryIndex;

		    CgRuntimeParameter *rtParameter = program->runtimeParameters + index;
		    float *hostMemoryCopy = ( float * )rtParameter->pushBufferPointer;

		    if ( hostMemoryCopy )
		    {
			    const CgParameterEntry *parameterEntry = rtParameter->parameterEntry;
			    int arrayCount = 1;
			    if ( parameterEntry->flags & CGP_ARRAY )
			    {
				    const CgParameterArray *parameterArray = _RGLGetParameterArray( program, parameterEntry );
				    arrayCount = _RGLGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount );
				    i++;
				    parameterEntry++;
			    }
			    const CgParameterResource *parameterResource = _RGLGetParameterResource( program, parameterEntry );
			    unsigned short *resource = program->resources + parameterResource->resource + 1;
			    int registerStride = isMatrix(( CGtype )parameterResource->type ) ? _RGLGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
			    int registerCount = arrayCount * registerStride;
			    int j;
			    for ( j = 0;j < registerCount;j++ )
			    {
				    unsigned short embeddedConstCount = *( resource++ );
				    int k;
				    for ( k = 0;k < embeddedConstCount;k++ )
				    {
					    unsigned short ucodePatchOffset = *( resource )++;
					    unsigned int *dst = ( unsigned int* )(( char* )program->ucode + ucodePatchOffset );
					    dst[0] = SWAP_IF_BIG_ENDIAN( itemDefaultValues[0] );
					    dst[1] = SWAP_IF_BIG_ENDIAN( itemDefaultValues[1] );
					    dst[2] = SWAP_IF_BIG_ENDIAN( itemDefaultValues[2] );
					    dst[3] = SWAP_IF_BIG_ENDIAN( itemDefaultValues[3] );
				    }
				    memcpy(( void* )hostMemoryCopy, ( void* )itemDefaultValues, sizeof( float )*4 );
				    hostMemoryCopy += 4;
				    itemDefaultValues += 4;
				    resource++;
			    }
		    }
	    }
    }
    else
    {
	    for (int i = 0; i < count; i++)
	    {
		    int index = ( int )program->defaultValuesIndices[i].entryIndex;
		    CgRuntimeParameter *rtParameter = program->runtimeParameters + index;

		    int arrayCount = 1;
		    const CgParameterEntry *parameterEntry = rtParameter->parameterEntry;
		    bool isArray = false;
		    if ( parameterEntry->flags & CGP_ARRAY )
		    {
			    isArray = true;
			    const CgParameterArray *parameterArray = _RGLGetParameterArray( program, parameterEntry );
			    arrayCount = _RGLGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount );
			    parameterEntry++;
			    rtParameter++;
		    }

		    if ( rtParameter->pushBufferPointer )
		    {
			    const CgParameterResource *parameterResource = _RGLGetParameterResource( program, parameterEntry );
			    const float *itemDefaultValues = program->defaultValues + program->defaultValuesIndices[i].defaultValueIndex;
			    int registerStride = isMatrix(( CGtype )parameterResource->type ) ? _RGLGetTypeRowCount(( CGtype )parameterResource->type ) : 1;

			    if ( parameterEntry->flags & CGP_CONTIGUOUS )
                               memcpy( rtParameter->pushBufferPointer, itemDefaultValues, arrayCount * registerStride *4*sizeof( float ) );
			    else
			    {
				    unsigned int *pushBufferPointer = (( unsigned int * )rtParameter->pushBufferPointer );
				    for ( int j = 0;j < arrayCount;j++ )
				    {
                                       unsigned int *pushBufferAddress = isArray ? ( *( unsigned int** )pushBufferPointer ) : pushBufferPointer;
				       memcpy( pushBufferAddress, itemDefaultValues, registerStride*4*sizeof( float ) );
				       pushBufferPointer += isArray ? 1 : 3 + registerStride * 4;
				       itemDefaultValues += 4 * registerStride;
				    }
			    }
		    }
	    }
    }

    program->loadProgramId = GMM_ERROR;
    program->loadProgramOffset = 0;
    if ( profileIndex == FRAGMENT_PROFILE_INDEX )
    {
        int loaded = _RGLLoadFPShader( program );
        if ( ! loaded )
        {
            _RGLCgRaiseError( CG_MEMORY_ALLOC_ERROR );
            return 0;
        }
    }

    program->programGroup = NULL;
    program->programIndexInGroup = -1;

    return 1;
}

void _RGLPlatformVertexProgramErase( void* platformProgram )
{
    _CGprogram* program = ( _CGprogram* )platformProgram;
    if ( program->runtimeParameters )
        free( program->runtimeParameters );

    if ( program->memoryBlock )
        free( program->memoryBlock );

    if ( program->samplerIndices )
    {
        free( program->samplerValuesLocation );
        free( program->samplerIndices );
        free( program->samplerUnits );
    }

    if ( program->constantPushBufferPointers )
        free( program->constantPushBufferPointers );
}

void _RGLPlatformProgramErase( void* platformProgram )
{
    _CGprogram* program = ( _CGprogram* )platformProgram;

    if ( program->loadProgramId != GMM_ERROR )
    {
       if ( program->loadProgramId != GMM_ERROR )
       {
          gmmFree( program->loadProgramId );
	  program->loadProgramId = GMM_ERROR;
	  program->loadProgramOffset = 0;
       }
    }

    if ( program->runtimeParameters )
    {
        int i;
        int count = ( int )program->rtParametersCount;
        for ( i = 0;i < count;i++ )
        {
            _RGLEraseName( &_CurrentContext->cgParameterNameSpace, ( jsName )program->runtimeParameters[i].id );
        }
        free( program->runtimeParameters );
    }

    if ( program->memoryBlock )
        free( program->memoryBlock );

    if ( program->samplerIndices )
    {
        free( program->samplerValuesLocation );
        free( program->samplerIndices );
        free( program->samplerUnits );
    }

    if ( program->constantPushBufferPointers )
        free( program->constantPushBufferPointers );
}


CGbool _RGLPlatformSupportsFragmentProgram( CGprofile p )
{
    if ( p == CG_PROFILE_SCE_FP_TYPEB )
        return CG_TRUE;
    if ( CG_PROFILE_SCE_FP_RSX == p )
        return CG_TRUE;
    return CG_FALSE;
}

CGprofile _RGLPlatformGetLatestProfile( CGGLenum profile_type )
{
    switch ( profile_type )
    {
        case CG_GL_VERTEX:
            return CG_PROFILE_SCE_VP_RSX;
        case CG_GL_FRAGMENT:
            return CG_PROFILE_SCE_FP_RSX;
        default:
            break;
    }
    return CG_PROFILE_UNKNOWN;
}

int _RGLPlatformCopyProgram( _CGprogram* source, _CGprogram* destination )
{
    CgParameterTableHeader parameterHeader;
    parameterHeader.entryCount = source->rtParametersCount;
    parameterHeader.resourceTableOffset = ( uintptr_t )(( char* )source->resources - ( char* )source->parametersEntries + sizeof( CgParameterTableHeader ) );
    parameterHeader.defaultValueIndexCount = source->defaultValuesIndexCount;
    parameterHeader.defaultValueIndexTableOffset = ( uintptr_t )(( char* )source->defaultValuesIndices - ( char* )source->parametersEntries + sizeof( CgParameterTableHeader ) );

    int profileIndex;

    switch ( source->header.profile )
    {
        case CG_PROFILE_SCE_VP_TYPEB:
        case CG_PROFILE_SCE_VP_RSX:
            profileIndex = VERTEX_PROFILE_INDEX;
            break;

        case CG_PROFILE_SCE_FP_TYPEB:
        case CG_PROFILE_SCE_FP_RSX:
            profileIndex = FRAGMENT_PROFILE_INDEX;
            break;
        default:
            return 0;
    }
    return _RGLGenerateProgram( destination, profileIndex, &source->header, source->ucode, &parameterHeader, source->parametersEntries, source->stringTable, source->defaultValues );
}

static char *_RGLPlatformBufferObjectMap( jsBufferObject* bufferObject, GLenum access )
{
    RGLBufferObject *jsBuffer = ( RGLBufferObject * )bufferObject->platformBufferObject;

    if ( jsBuffer->mapCount++ == 0 )
    {
        if ( access == GL_WRITE_ONLY )
        {
            _RGLAllocateBuffer( bufferObject );
            if ( jsBuffer->pool == SURFACE_POOL_NONE )
            {
                _RGLSetError( GL_OUT_OF_MEMORY );
                return NULL;
            }
	}
	else
	{
		cellGcmSetInvalidateVertexCache( &_RGLState.fifo);
		_RGLFifoFinish( &_RGLState.fifo );
	}

        jsBuffer->mapAccess = access;

        if ( jsBuffer->mapAccess != GL_READ_ONLY )
	{
		RGLDriver *driver= (RGLDriver *)_CurrentDevice->rasterDriver;
		++driver->flushBufferCount;
	}

	GmmBaseBlock    *pBaseBlock = (GmmBaseBlock *)jsBuffer->bufferId;

	if (!pBaseBlock->isTile)
	{
		GmmBlock *pBlock = (GmmBlock *)jsBuffer->bufferId;

		pBlock->isPinned = 1;
	}
    }

    return gmmIdToAddress( jsBuffer->bufferId );
}

static jsFramebuffer* _RGLCreateFramebuffer( void )
{
   jsFramebuffer* framebuffer = new jsPlatformFramebuffer();
   return framebuffer;
}

static void _RGLDestroyFramebuffer( jsFramebuffer* framebuffer )
{
   delete framebuffer;
}

static void _RGLPlatformDestroyTexture( jsTexture* texture )
{
    if ( !texture->referenceBuffer )
    {
        RGLTexture *gcmTexture = ( RGLTexture * )texture->platformTexture;
        if ( gcmTexture->pbo != NULL )
        {
            _RGLFreeBufferObject( gcmTexture->pbo );
            gcmTexture->pbo = NULL;
            gcmTexture->pool = SURFACE_POOL_NONE;
            gcmTexture->gpuAddressId = GMM_ERROR;
            gcmTexture->gpuAddressIdOffset = 0;
            gcmTexture->gpuSize = 0;
        }

        _RGLPlatformFreeGcmTexture( texture );
    }
    _RGLTextureTouchFBOs( texture );
}

#ifndef __PSL1GHT__
#include <cell/gcm/gcm_method_data.h>
#endif

static void _RGLPlatformValidateTextureStage( int unit, jsTexture* texture )
{
    if ( RGL_UNLIKELY( texture->revalidate ) )
    {
        _RGLPlatformValidateTextureResources( texture );
    }

    GLboolean isCompleteCache = texture->isComplete;

    if ( RGL_LIKELY( isCompleteCache ) )
    {
        RGLTexture *platformTexture = ( RGLTexture * )texture->platformTexture;
        GmmBaseBlock *pBaseBlock = (GmmBaseBlock *)platformTexture->gpuAddressId;
	const GLuint imageOffset = gmmAddressToOffset(pBaseBlock->address, pBaseBlock->isMain) + platformTexture->gpuAddressIdOffset;
	platformTexture->gcmTexture.offset = imageOffset; 

	cellGcmSetTexture( &_RGLState.fifo, unit, &platformTexture->gcmTexture);
	CellGcmContextData *gcm_context = (CellGcmContextData*)&_RGLState.fifo;
	cellGcmReserveMethodSizeInline(gcm_context, 11);
	uint32_t *current = gcm_context->current;
	current[0] = CELL_GCM_METHOD_HEADER_TEXTURE_OFFSET(unit, 8);
	current[1] = CELL_GCM_METHOD_DATA_TEXTURE_OFFSET(platformTexture->gcmTexture.offset);
	current[2] = CELL_GCM_METHOD_DATA_TEXTURE_FORMAT(platformTexture->gcmTexture.location,
			CELL_GCM_FALSE, 
			CELL_GCM_TEXTURE_DIMENSION_2,
			platformTexture->gcmTexture.format,
			1);
	current[3] = CELL_GCM_METHOD_DATA_TEXTURE_ADDRESS(
			CELL_GCM_TEXTURE_BORDER, /* wrapS */
			CELL_GCM_TEXTURE_BORDER, /* wrapT */
			CELL_GCM_TEXTURE_BORDER, /* wrapR */
			CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, /* unsignedRemap */
			CELL_GCM_TEXTURE_ZFUNC_NEVER,
			platformTexture->gcmMethods.address.gamma,
			0);
	current[4] = CELL_GCM_METHOD_DATA_TEXTURE_CONTROL0(CELL_GCM_TRUE,
			0, /* minLOD */
			256000, /* maxLOD */
			CELL_GCM_TEXTURE_MAX_ANISO_1);
	current[5] = platformTexture->gcmTexture.remap;
	current[6] = CELL_GCM_METHOD_DATA_TEXTURE_FILTER(
			(platformTexture->gcmMethods.filter.bias & 0x1fff),
			platformTexture->gcmMethods.filter.min,
			platformTexture->gcmMethods.filter.mag,
			CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX); /* filter */
	current[7] = CELL_GCM_METHOD_DATA_TEXTURE_IMAGE_RECT(
			platformTexture->gcmTexture.height,
			platformTexture->gcmTexture.width);
	current[8] = 0;
	current[9] = CELL_GCM_METHOD_HEADER_TEXTURE_CONTROL3(unit,1);
	current[10] = CELL_GCM_METHOD_DATA_TEXTURE_CONTROL3(
			platformTexture->gcmTexture.pitch,
			1); /* depth */
	gcm_context->current = &current[11];
    }
    else
    {
        //printf("RGL WARN: Texture bound to unit %d is incomplete.\n", unit);
	GLuint remap = CELL_GCM_REMAP_MODE(
			CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
			CELL_GCM_TEXTURE_REMAP_FROM_A,
			CELL_GCM_TEXTURE_REMAP_FROM_R,
			CELL_GCM_TEXTURE_REMAP_FROM_G,
			CELL_GCM_TEXTURE_REMAP_FROM_B,
			CELL_GCM_TEXTURE_REMAP_ONE,
			CELL_GCM_TEXTURE_REMAP_ZERO,
			CELL_GCM_TEXTURE_REMAP_ZERO,
			CELL_GCM_TEXTURE_REMAP_ZERO );

	cellGcmSetTextureControlInline( &_RGLState.fifo, unit, CELL_GCM_FALSE, 0, 0, 0);
	cellGcmSetTextureRemapInline( &_RGLState.fifo, unit, remap);
    }


}

static GLenum _RGLPlatformChooseInternalFormat( GLenum internalFormat )
{
    switch ( internalFormat )
    {
        case GL_ALPHA:
        case GL_ALPHA4:
        case GL_ALPHA8:
            return RGL_ALPHA8;
        case GL_RGB10:
        case GL_RGB10_A2:
        case GL_RGB12:
        case GL_RGB16:
            return RGL_FLOAT_RGBX32;
        case GL_RGBA12:
        case GL_RGBA16:
            return RGL_FLOAT_RGBA32;
        case 3:
        case GL_R3_G3_B2:
        case GL_RGB4:
        case GL_RGB:
        case GL_RGB8:
        case RGL_RGBX8:
            return RGL_RGBX8;
        case 4:
        case GL_RGBA2:
        case GL_RGBA4:
        case GL_RGBA8:
        case GL_RGBA:
            return RGL_RGBA8;
        case GL_RGB5_A1:
            return RGL_RGB5_A1_SCE;
        case GL_RGB5:
            return RGL_RGB565_SCE;
        case GL_BGRA:
        case RGL_BGRA8:
            return RGL_BGRA8;
        case GL_ARGB_SCE:
            return RGL_ARGB8;
        default:
            return GL_INVALID_ENUM;
    }
    return GL_INVALID_ENUM;
}

static void _RGLPlatformExpandInternalFormat( GLenum internalFormat, GLenum *format, GLenum *type )
{
   switch ( internalFormat )
   {
      case RGL_ALPHA8:
         *format = GL_ALPHA;
	 *type = GL_UNSIGNED_BYTE;
	 break;
      case RGL_ARGB8:
	 *format = GL_BGRA;
	 *type = GL_UNSIGNED_INT_8_8_8_8_REV;
	 break;
      case RGL_RGB5_A1_SCE:
	 *format = GL_RGBA;
	 *type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	 break;
      case RGL_RGB565_SCE:
	 *format = GL_RGB;
	 *type = GL_UNSIGNED_SHORT_5_6_5_REV;
	 break;
      default:
	 return;
   }
}

static GLenum _RGLPlatformChooseInternalStorage( jsImage* image, GLenum internalFormat )
{
   image->storageSize = 0;

   GLenum platformInternalFormat = _RGLPlatformChooseInternalFormat( internalFormat );

   if ( platformInternalFormat == GL_INVALID_ENUM )
      return GL_INVALID_ENUM;

   image->internalFormat = platformInternalFormat;
   _RGLPlatformExpandInternalFormat( platformInternalFormat, &image->format, &image->type );

   image->storageSize = _RGLGetPixelSize(image->format, image->type) * image->width * image->height;

   return GL_NO_ERROR;
}

static inline GLuint _RGLGetBufferObjectOrigin(GLuint buffer)
{
    PSGLcontext*	LContext = _CurrentContext;
    jsBufferObject *bufferObject = (jsBufferObject *)LContext->bufferObjectNameSpace.data[buffer];

    RGLBufferObject *gcmBuffer = ( RGLBufferObject * ) & bufferObject->platformBufferObject;
    return gcmBuffer->bufferId;
}

static void _RGLSetImage( jsImage *image, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLsizei alignment, GLenum format, GLenum type, const GLvoid *pixels )
{
    image->width = width;
    image->height = height;
    image->alignment = alignment;

    image->xblk = 0;
    image->yblk = 0;

    image->xstride = 0;
    image->ystride = 0;

    image->format = 0;
    image->type = 0;
    image->internalFormat = 0;
    const GLenum status = _RGLPlatformChooseInternalStorage( image, internalFormat );
    (( void )status );

    image->data = NULL;
    image->mallocData = NULL;
    image->mallocStorageSize = 0;

    image->isSet = GL_TRUE;

    if ( image->xstride == 0 )
	    image->xstride = _RGLGetPixelSize( image->format, image->type );
    if ( image->ystride == 0 )
	    image->ystride = image->width * image->xstride;

    if ( pixels )
    {
        _RGLImageAllocCPUStorage( image );
        if ( !image->data )
            return;

        jsRaster raster;
        raster.format = format;
        raster.type = type;
        raster.width = width;
        raster.height = height;
        raster.data = ( void * )pixels;

        raster.xstride = _RGLGetPixelSize( raster.format, raster.type );
        raster.ystride = ( raster.width * raster.xstride + alignment - 1 ) / alignment * alignment;

        _RGLRasterToImage( &raster, image);
        image->dataState = IMAGE_DATASTATE_HOST;
    }
    else
        image->dataState = IMAGE_DATASTATE_UNSET;
}

static GLboolean _RGLPlatformTexturePBOImage(
    jsTexture* texture,
    jsImage* image,
    GLint internalFormat,
    GLsizei width, GLsizei height,
    GLenum format, GLenum type,
    const GLvoid *offset )
{
    PSGLcontext*	LContext = _CurrentContext;

    RGLTexture *gcmTexture = ( RGLTexture * )texture->platformTexture;

    image->dataState = IMAGE_DATASTATE_UNSET;
    if ( gcmTexture->pbo != NULL )
        _RGLPlatformDropTexture( texture );

    _RGLSetImage( image, internalFormat, width, height, 1 /* depth */, LContext->unpackAlignment, format, type, NULL );

    if ( LContext->PixelUnpackBuffer == 0 )
        return GL_FALSE;

    const GLuint pboPitch = _RGLPad(_RGLGetPixelSize(format, type) * width, LContext->unpackAlignment );
    if (( pboPitch&3 ) != 0 )
    {
        RARCH_WARN("PBO image pitch not a multiple of 4, using slow path.\n" );
        return GL_FALSE;
    }

    GLuint gpuId = _RGLGetBufferObjectOrigin( LContext->PixelUnpackBuffer );
    GLuint gpuIdOffset = (( GLubyte* )offset - ( GLubyte* )NULL );

    if ( gmmIdToOffset(gpuId)+gpuIdOffset & 63 )
    {
        RARCH_WARN("PBO offset not 64-byte aligned, using slow path.\n");
        return GL_FALSE;
    }

    GLboolean formatOK = GL_FALSE;
    switch ( internalFormat )
    {
        case 4:
        case GL_RGBA:
        case GL_RGBA8:
            if ( format == GL_RGBA && type == GL_UNSIGNED_INT_8_8_8_8 )
                formatOK = GL_TRUE;
            break;
        case GL_ALPHA:
        case GL_ALPHA8:
            if ( format == GL_ALPHA && type == GL_UNSIGNED_BYTE )
                formatOK = GL_TRUE;
            break;
        case GL_ARGB_SCE:
            if ( format == GL_BGRA && type == GL_UNSIGNED_INT_8_8_8_8_REV )
                formatOK = GL_TRUE;
            break;
        default:
            formatOK = GL_FALSE;
    }

    if (!formatOK )
    {
        RARCH_WARN("PBO format/type requires conversion to texture internal format, using slow path.\n");
        return GL_FALSE;
    }

    if (!_RGLTextureIsValid(texture))
    {
        RARCH_WARN("PBO transfering to incomplete texture, using slow path.\n");
        return GL_FALSE;
    }

    RGLTextureLayout newLayout;
    _RGLPlatformChooseGPUFormatAndLayout( texture, GL_TRUE, pboPitch, &newLayout );

    jsBufferObject* bufferObject = (jsBufferObject *)LContext->bufferObjectNameSpace.data[LContext->PixelUnpackBuffer];
    if ( newLayout.pitch != 0 && !bufferObject->mapped )
    {
        gcmTexture->gpuLayout = newLayout;
        if ( gcmTexture->gpuAddressId != GMM_ERROR && gcmTexture->pbo == NULL )
           _RGLPlatformFreeGcmTexture( texture );

        gcmTexture->pbo = bufferObject;
        gcmTexture->gpuAddressId = gpuId; 
        gcmTexture->gpuAddressIdOffset = gpuIdOffset;
        gcmTexture->pool = SURFACE_POOL_LINEAR;
        RGLTexture *gcmTexture = ( RGLTexture * )texture->platformTexture;
	// Get size of texture in GPU layout
        gcmTexture->gpuSize = _RGLPad( gcmTexture->gpuLayout.baseHeight * gcmTexture->gpuLayout.pitch, 1);
        ++bufferObject->refCount;
    }
    else
    {
        const GLuint bytesPerPixel = newLayout.pixelBits / 8;

        RGLSurface src;
        src.source             = SURFACE_SOURCE_PBO;
        src.width              = image->width;
        src.height             = image->height;
        src.bpp                = bytesPerPixel;
        src.pitch              = pboPitch;
        src.format             = newLayout.internalFormat;
        src.pool               = SURFACE_POOL_LINEAR;
        src.ppuData            = NULL;
        src.dataId             = gpuId;
        src.dataIdOffset       = gpuIdOffset;

        texture->revalidate |= TEXTURE_REVALIDATE_LAYOUT;
        _RGLPlatformValidateTextureResources( texture );

        RGLSurface dst;
        dst.source             = SURFACE_SOURCE_TEXTURE;
        dst.width              = image->width;
        dst.height             = image->height;
        dst.bpp                = bytesPerPixel;
        dst.pitch              = gcmTexture->gpuLayout.pitch;
        dst.format             = gcmTexture->gpuLayout.internalFormat;
        dst.pool               = gcmTexture->pool;
        dst.ppuData            = NULL;
        dst.dataId             = gcmTexture->gpuAddressId;
        dst.dataIdOffset       = gcmTexture->gpuAddressIdOffset;

	transfer_params_t transfer_params;

	transfer_params.dst_id        = dst.dataId;
	transfer_params.dst_id_offset = dst.dataIdOffset;
	transfer_params.dst_pitch     = dst.pitch ? dst.pitch : (dst.bpp * dst.width);
	transfer_params.dst_x         = 0;
	transfer_params.dst_y         = 0;
	transfer_params.src_id        = src.dataId;
	transfer_params.src_id_offset = src.dataIdOffset;
	transfer_params.src_pitch     = src.pitch ? src.pitch : (src.bpp * src.width);
	transfer_params.src_x         = 0;
	transfer_params.src_y         = 0;
	transfer_params.width         = width;
	transfer_params.height        = height;
	transfer_params.bpp           = src.bpp;
	transfer_params.fifo_ptr      = &_RGLState.fifo;

	TransferDataVidToVid(&transfer_params);
    }

    _RGLImageFreeCPUStorage( image );
    image->dataState = IMAGE_DATASTATE_GPU;

    texture->revalidate &= ~( TEXTURE_REVALIDATE_LAYOUT | TEXTURE_REVALIDATE_IMAGES );
    texture->revalidate |= TEXTURE_REVALIDATE_PARAMETERS;
    _RGLTextureTouchFBOs( texture );

    return GL_TRUE;
}

static GLboolean _RGLPlatformTextureReference( jsTexture *texture, GLuint pitch, jsBufferObject *bufferObject, GLintptr offset )
{
    RGLTexture *gcmTexture = (RGLTexture *)texture->platformTexture;

    RGLTextureLayout newLayout;
    _RGLPlatformChooseGPUFormatAndLayout(texture, GL_TRUE, pitch, &newLayout);

    texture->isRenderTarget = GL_TRUE;

    if(gcmTexture->gpuAddressId != GMM_ERROR)
        _RGLPlatformDestroyTexture( texture );

    RGLBufferObject *gcmBuffer = (RGLBufferObject *)& bufferObject->platformBufferObject;

    gcmTexture->gpuLayout = newLayout;
    gcmTexture->pool = gcmBuffer->pool;
    gcmTexture->gpuAddressId = gcmBuffer->bufferId;
    gcmTexture->gpuAddressIdOffset = offset;
    gcmTexture->gpuSize = _RGLPad( newLayout.baseHeight * newLayout.pitch, 1);

    texture->revalidate &= ~(TEXTURE_REVALIDATE_LAYOUT | TEXTURE_REVALIDATE_IMAGES);
    texture->revalidate |= TEXTURE_REVALIDATE_PARAMETERS;
    _RGLTextureTouchFBOs(texture);
    return GL_TRUE;
}

static inline void _RGLSetColorDepthBuffers( RGLRenderTarget *rt, RGLRenderTargetEx const * const args )
{
    CellGcmSurface *   grt = &rt->gcmRenderTarget;

    rt->colorBufferCount = args->colorBufferCount;

    GLuint oldHeight;
    GLuint oldyInverted;

    oldyInverted = rt->yInverted;
    oldHeight = rt->gcmRenderTarget.height;

    GLuint i;

    for ( i = 0; i < args->colorBufferCount; i++ )
    {
        if ( args->colorPitch[i] == 0 )
        {
            grt->colorOffset[i] = 0;
            grt->colorPitch[i] = 0x200;
            grt->colorLocation[i] = CELL_GCM_LOCATION_LOCAL;
        }
        else
        {
            if ( args->colorId[i] != GMM_ERROR )
            {
                if ( gmmIdIsMain(args->colorId[i]) )
                    grt->colorLocation[i] = CELL_GCM_LOCATION_MAIN;
                else
                    grt->colorLocation[i] = CELL_GCM_LOCATION_LOCAL;

                grt->colorOffset[i] = gmmIdToOffset(args->colorId[i]) + args->colorIdOffset[i];
                grt->colorPitch[i] = args->colorPitch[i];
            }
        }
    }

    for ( ; i < RGL_SETRENDERTARGET_MAXCOUNT; i++ )
    {
        grt->colorOffset[i] = grt->colorOffset[0];
        grt->colorPitch[i]   = grt->colorPitch[0];
        grt->colorLocation[i] = grt->colorLocation[0];
    }

    rt->yInverted = args->yInverted;
    grt->x        = args->xOffset; 
    grt->y        = args->yOffset; 
    grt->width    = args->width;
    grt->height   = args->height;

    if (( grt->height != oldHeight ) | ( rt->yInverted != oldyInverted ) )
    {
       RGLViewportState *v = &_RGLState.state.viewport;
       _RGLFifoGlViewport( v->x, v->y, v->w, v->h );
    }
}

void _RGLFifoGlSetRenderTarget( RGLRenderTargetEx const * const args )
{
   RGLRenderTarget *rt = &_RGLState.renderTarget;
   CellGcmSurface *   grt = &_RGLState.renderTarget.gcmRenderTarget;

   _RGLSetColorDepthBuffers( rt, args );

   //set color depth formats
   grt->colorFormat = CELL_GCM_SURFACE_A8R8G8B8;
   grt->depthFormat = CELL_GCM_SURFACE_Z24S8;
   grt->depthLocation = CELL_GCM_LOCATION_LOCAL;
   grt->depthOffset = 0;
   grt->depthPitch = 64;

   grt->antialias = CELL_GCM_SURFACE_CENTER_1;

   grt->type = CELL_GCM_SURFACE_PITCH;

   switch ( rt->colorBufferCount )
   {
      case 0:
         grt->colorTarget = CELL_GCM_SURFACE_TARGET_NONE;
	 break;
      case 1:
	 grt->colorTarget = CELL_GCM_SURFACE_TARGET_1;
	 break;
      case 2:
	 grt->colorTarget = CELL_GCM_SURFACE_TARGET_MRT1;
	 break;
      case 3:
	 grt->colorTarget = CELL_GCM_SURFACE_TARGET_MRT2;
	 break;
      case 4:
	 grt->colorTarget = CELL_GCM_SURFACE_TARGET_MRT3;
	 break;
      default:
	 break;
   }

   cellGcmSetSurfaceInline( &_RGLState.fifo, grt);
}

void _RGLSetError( GLenum error )
{
   RARCH_ERR("Error code: %d\n", error);
}

GLAPI GLenum APIENTRY glGetError(void)
{
    if ( !_CurrentContext )
	return GL_INVALID_OPERATION;
    else
    {
        GLenum error = _CurrentContext->error;

        _CurrentContext->error = GL_NO_ERROR;
        return error;
    }
}

static inline void _RGLPushProgramPushBuffer( _CGprogram * cgprog )
{
   RGLFifo *fifo = &_RGLState.fifo;
   GLuint spaceInWords = cgprog->constantPushBufferWordSize + 4 + 32;

   if ( fifo->current + spaceInWords + 1024 > fifo->end )
      _RGLOutOfSpaceCallback( fifo, spaceInWords );

   uint32_t padding_in_word = ( ( 0x10-(((uint32_t)_RGLState.fifo.current)&0xf))&0xf )>>2;
   uint32_t padded_size = ( ((cgprog->constantPushBufferWordSize)<<2) + 0xf )&~0xf;
   cellGcmSetNopCommandUnsafeInline( &_RGLState.fifo, padding_in_word);

   memcpy(_RGLState.fifo.current, cgprog->constantPushBuffer, padded_size);
   _RGLState.fifo.current+=cgprog->constantPushBufferWordSize;
}

static GLuint _RGLValidateStates( void )
{
    PSGLcontext* LContext = _CurrentContext;
    RGLDriver *driver= (RGLDriver *)_CurrentDevice->rasterDriver;

    LContext->needValidate &= PSGL_VALIDATE_ALL;
    GLuint  dirty = LContext->needValidate;
    GLuint  needValidate = LContext->needValidate;

    if ( RGL_UNLIKELY( needValidate & PSGL_VALIDATE_FRAMEBUFFER ) )
    {
        _RGLValidateFramebuffer();

        needValidate = LContext->needValidate;
    }

    if ( RGL_UNLIKELY( needValidate & PSGL_VALIDATE_TEXTURES_USED ) )
    {
        long unitInUseCount = LContext->BoundFragmentProgram->samplerCount;
        const GLuint* unitsInUse = LContext->BoundFragmentProgram->samplerUnits;

	for ( long i = 0; i < unitInUseCount; ++i )
	{
		long unit = unitsInUse[i];
		jsTexture* texture = LContext->TextureImageUnits[unit].currentTexture;

		_RGLPlatformValidateTextureStage( unit, texture );
	}
    }

    if ( RGL_UNLIKELY( needValidate & PSGL_VALIDATE_VERTEX_PROGRAM ) )
    {
	    const void *header = LContext->BoundVertexProgram;
	    const _CGprogram *vs = ( const _CGprogram* ) header;

	    __dcbt(vs->ucode);
	    __dcbt(((uint8_t*)vs->ucode)+128);
	    __dcbt(((uint8_t*)vs->ucode)+256);
	    __dcbt(((uint8_t*)vs->ucode)+384);

	    CellCgbVertexProgramConfiguration conf;
	    conf.instructionSlot = vs->header.vertexProgram.instructionSlot;
	    conf.instructionCount = vs->header.instructionCount;
	    conf.registerCount = vs->header.vertexProgram.registerCount;
	    conf.attributeInputMask = vs->header.attributeInputMask;

	    RGLFifo *fifo = &_RGLState.fifo;
	    GLuint spaceInWords = 7 + 5 * conf.instructionCount;

	    if ( fifo->current + spaceInWords + 1024 > fifo->end )
               _RGLOutOfSpaceCallback( fifo, spaceInWords );

	    cellGcmSetVertexProgramLoadInline( &_RGLState.fifo, &conf, vs->ucode);

	    RGLInterpolantState *s = &_RGLState.state.interpolant;
	    s->vertexProgramAttribMask = vs->header.vertexProgram.attributeOutputMask;

	    cellGcmSetVertexAttribOutputMaskInline( &_RGLState.fifo,  s->vertexProgramAttribMask & s->fragmentProgramAttribMask);

	    _CGprogram *program = ( _CGprogram* )vs;
	    int count = program->defaultValuesIndexCount;
	    for ( int i = 0; i < count; i++ )
	    {
		    const CgParameterEntry *parameterEntry = program->parametersEntries + program->defaultValuesIndices[i].entryIndex;
		    if (( parameterEntry->flags & CGPF_REFERENCED ) && ( parameterEntry->flags & CGPV_MASK ) == CGPV_CONSTANT )
		    {
			    const float *itemDefaultValues = program->defaultValues + program->defaultValuesIndices[i].defaultValueIndex;
			    const GLfloat *value = itemDefaultValues;
			    const CgParameterResource *parameterResource = _RGLGetParameterResource( program, parameterEntry );
			    if ( parameterResource->resource != ( unsigned short ) - 1 )
			    {
				    switch ( parameterResource->type )
				    {
					    case CG_FLOAT:
					    case CG_FLOAT1:
					    case CG_FLOAT2:
					    case CG_FLOAT3:
					    case CG_FLOAT4:
						    cellGcmSetVertexProgramParameterBlockInline( &_RGLState.fifo, parameterResource->resource, 1, value );
						    break;
					    case CG_FLOAT4x4:
						    {
							    GLfloat v2[16];
							    v2[0] = value[0];v2[1] = value[4];v2[2] = value[8];v2[3] = value[12];
							    v2[4] = value[1];v2[5] = value[5];v2[6] = value[9];v2[7] = value[13];
							    v2[8] = value[2];v2[9] = value[6];v2[10] = value[10];v2[11] = value[14];
							    v2[12] = value[3];v2[13] = value[7];v2[14] = value[11];v2[15] = value[15];
							    cellGcmSetVertexProgramParameterBlockInline( &_RGLState.fifo, parameterResource->resource, 4, v2 );
						    }
						    break;
					    case CG_FLOAT3x3:
						    {
							    GLfloat v2[12];
							    v2[0] = value[0];v2[1] = value[3];v2[2] = value[6];v2[3] = 0;
							    v2[4] = value[1];v2[5] = value[4];v2[6] = value[7];v2[7] = 0;
							    v2[8] = value[2];v2[9] = value[5];v2[10] = value[8];v2[11] = 0;
							    cellGcmSetVertexProgramParameterBlockInline( &_RGLState.fifo, parameterResource->resource, 3, v2 );
						    }
						    break;
					    case CG_HALF:
					    case CG_HALF1:
					    case CG_HALF2:
					    case CG_HALF3:
					    case CG_HALF4:
					    case CG_INT:
					    case CG_INT1:
					    case CG_INT2:
					    case CG_INT3:
					    case CG_INT4:
					    case CG_BOOL:
					    case CG_BOOL1:
					    case CG_BOOL2:
					    case CG_BOOL3:
					    case CG_BOOL4:
					    case CG_FIXED:
					    case CG_FIXED1:
					    case CG_FIXED2:
					    case CG_FIXED3:
					    case CG_FIXED4:
						    cellGcmSetVertexProgramParameterBlockInline( &_RGLState.fifo, parameterResource->resource, 1, value );
						    break;
					    case CG_HALF4x4:
					    case CG_INT4x4:
					    case CG_BOOL4x4:
					    case CG_FIXED4x4:
						    {
							    GLfloat v2[16];
							    v2[0] = value[0];v2[1] = value[4];v2[2] = value[8];v2[3] = value[12];
							    v2[4] = value[1];v2[5] = value[5];v2[6] = value[9];v2[7] = value[13];
							    v2[8] = value[2];v2[9] = value[6];v2[10] = value[10];v2[11] = value[14];
							    v2[12] = value[3];v2[13] = value[7];v2[14] = value[11];v2[15] = value[15];
							    cellGcmSetVertexProgramParameterBlockInline( &_RGLState.fifo, parameterResource->resource, 4, v2 );
						    }
						    break;
					    case CG_HALF3x3:
					    case CG_INT3x3:
					    case CG_BOOL3x3:
					    case CG_FIXED3x3:
						    {
							    GLfloat v2[12];
							    v2[0] = value[0];v2[1] = value[3];v2[2] = value[6];v2[3] = 0;
							    v2[4] = value[1];v2[5] = value[4];v2[6] = value[7];v2[7] = 0;
							    v2[8] = value[2];v2[9] = value[5];v2[10] = value[8];v2[11] = 0;
							    cellGcmSetVertexProgramParameterBlockInline( &_RGLState.fifo, parameterResource->resource, 3, v2 );
						    }
						    break;
					    default:
						    break;
				    }
			    }
		    }
	    }

	    if(!(LContext->needValidate & PSGL_VALIDATE_VERTEX_CONSTANTS) && LContext->BoundVertexProgram->parentContext)
	    {
		    cellGcmSetTransformBranchBitsInline( &_RGLState.fifo, LContext->BoundVertexProgram->controlFlowBools | LContext->BoundVertexProgram->parentContext->controlFlowBoolsShared );

		    _RGLPushProgramPushBuffer( LContext->BoundVertexProgram );
	    }
    }

    if ( RGL_LIKELY( needValidate & PSGL_VALIDATE_VERTEX_CONSTANTS ) && LContext->BoundVertexProgram->parentContext)
    {
	    cellGcmSetTransformBranchBitsInline( &_RGLState.fifo, LContext->BoundVertexProgram->controlFlowBools | LContext->BoundVertexProgram->parentContext->controlFlowBoolsShared );

	    _RGLPushProgramPushBuffer( LContext->BoundVertexProgram );
    }

    if ( RGL_UNLIKELY( needValidate & PSGL_VALIDATE_VERTEX_TEXTURES_USED ) )
    {
	    for ( int unit = 0; unit < MAX_VERTEX_TEXTURE_IMAGE_UNITS; ++unit )
	    {
		    jsTexture *texture = LContext->VertexTextureImages[unit];
		    if ( texture )
			    if ( RGL_UNLIKELY( texture->revalidate ) )
				    _RGLPlatformValidateTextureResources( texture );

		    cellGcmSetVertexTextureAddressInline( &_RGLState.fifo, unit, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP);
		    cellGcmSetVertexTextureControlInline( &_RGLState.fifo, unit, GL_FALSE, 0, 256);
		    cellGcmSetVertexTextureFilterInline( &_RGLState.fifo, unit, 0);
	    }
    }

    if ( RGL_UNLIKELY( needValidate & PSGL_VALIDATE_FRAGMENT_PROGRAM ) )
    {
	    _CGprogram *program = LContext->BoundFragmentProgram;

	    const GLvoid *header = program;
	    const _CGprogram *ps = ( const _CGprogram * )header;

	    CellCgbFragmentProgramConfiguration conf;

	    conf.offset = gmmIdToOffset(ps->loadProgramId) + ps->loadProgramOffset;

	    RGLInterpolantState *s = &_RGLState.state.interpolant;
	    s->fragmentProgramAttribMask |= ps->header.attributeInputMask | CELL_GCM_ATTRIB_OUTPUT_MASK_POINTSIZE;

	    conf.attributeInputMask = s->vertexProgramAttribMask & s->fragmentProgramAttribMask;
	    conf.texCoordsInputMask = ps->header.fragmentProgram.texcoordInputMask;
	    conf.texCoords2D = ps->header.fragmentProgram.texcoord2d;
	    conf.texCoordsCentroid = ps->header.fragmentProgram.texcoordCentroid;

	    int fragmentControl = ( 1 << 15 ) | ( 1 << 10 );
	    fragmentControl |= ps->header.fragmentProgram.flags & CGF_DEPTHREPLACE ? 0xE : 0x0;
	    fragmentControl |= ps->header.fragmentProgram.flags & CGF_OUTPUTFROMH0 ? 0x00 : 0x40;
	    fragmentControl |= ps->header.fragmentProgram.flags & CGF_PIXELKILL ? 0x80 : 0x00;

	    conf.fragmentControl  = fragmentControl;
	    conf.registerCount = ps->header.fragmentProgram.registerCount < 2 ? 2 : ps->header.fragmentProgram.registerCount;

	    uint32_t controlTxp = _CurrentContext->AllowTXPDemotion; 
	    conf.fragmentControl &= ~CELL_GCM_MASK_SET_SHADER_CONTROL_CONTROL_TXP; 
	    conf.fragmentControl |= controlTxp << CELL_GCM_SHIFT_SET_SHADER_CONTROL_CONTROL_TXP; 

	    cellGcmSetFragmentProgramLoadInline( &_RGLState.fifo, &conf);
	    cellGcmSetZMinMaxControlInline( &_RGLState.fifo, ( ps->header.fragmentProgram.flags & CGF_DEPTHREPLACE ) ? CELL_GCM_FALSE : CELL_GCM_TRUE, CELL_GCM_FALSE, CELL_GCM_FALSE );


	    driver->fpLoadProgramId = program->loadProgramId;
	    driver->fpLoadProgramOffset = program->loadProgramOffset;
    }
	
    if ( RGL_LIKELY(( needValidate & ~( PSGL_VALIDATE_TEXTURES_USED |
                                       PSGL_VALIDATE_VERTEX_PROGRAM |
                                       PSGL_VALIDATE_VERTEX_CONSTANTS |
                                       PSGL_VALIDATE_VERTEX_TEXTURES_USED |
                                       PSGL_VALIDATE_FRAGMENT_PROGRAM ) ) == 0 ) )
    {
        LContext->needValidate = 0;
        return dirty;
    }

    if ( RGL_UNLIKELY( needValidate & PSGL_VALIDATE_BLENDING ) )
    {
	if ((LContext->Blending || LContext->BlendingMrt[0] || LContext->BlendingMrt[1] || LContext->BlendingMrt[2]))
	{
		GLuint hwColor;
		cellGcmSetBlendEnableInline( &_RGLState.fifo, LContext->Blending);
		cellGcmSetBlendEnableMrtInline( &_RGLState.fifo, LContext->BlendingMrt[0], LContext->BlendingMrt[1], LContext->BlendingMrt[2] );

		RGL_CALC_COLOR_LE_ARGB8( &hwColor, RGL_CLAMPF_01(LContext->BlendColor.R), RGL_CLAMPF_01(LContext->BlendColor.G), RGL_CLAMPF_01(LContext->BlendColor.B), RGL_CLAMPF_01(LContext->BlendColor.A) );
		cellGcmSetBlendColorInline( &_RGLState.fifo, hwColor, hwColor);
		cellGcmSetBlendEquationInline( &_RGLState.fifo, (RGLEnum)LContext->BlendEquationRGB, (RGLEnum)LContext->BlendEquationAlpha );
		cellGcmSetBlendFuncInline( &_RGLState.fifo, (RGLEnum)LContext->BlendFactorSrcRGB, (RGLEnum)LContext->BlendFactorDestRGB, (RGLEnum)LContext->BlendFactorSrcAlpha, (RGLEnum)LContext->BlendFactorDestAlpha);
	}
	else 
	{
		cellGcmSetBlendEnableInline( &_RGLState.fifo, CELL_GCM_FALSE);
		cellGcmSetBlendEnableMrtInline( &_RGLState.fifo, CELL_GCM_FALSE, CELL_GCM_FALSE, CELL_GCM_FALSE );
	}
    }

    if ( RGL_UNLIKELY( needValidate & PSGL_VALIDATE_SHADER_SRGB_REMAP ) )
    {
	cellGcmSetFragmentProgramGammaEnableInline( &_RGLState.fifo, LContext->ShaderSRGBRemap ? CELL_GCM_TRUE : CELL_GCM_FALSE);

	LContext->needValidate &= ~PSGL_VALIDATE_SHADER_SRGB_REMAP;
    }

    LContext->needValidate = 0;
    return dirty;
}

PSGLcontext *psglGetCurrentContext()
{
   return _CurrentContext;
}

static void _RGLResetContext( PSGLcontext *LContext )
{
    _RGLTexNameSpaceResetNames( &LContext->textureNameSpace );
    _RGLTexNameSpaceResetNames( &LContext->bufferObjectNameSpace );
    _RGLTexNameSpaceResetNames( &LContext->framebufferNameSpace );
    _RGLTexNameSpaceResetNames( &LContext->attribSetNameSpace );

    LContext->ViewPort.X = 0;
    LContext->ViewPort.Y = 0;
    LContext->ViewPort.XSize = 0;
    LContext->ViewPort.YSize = 0;

    LContext->ClearColor.R = 0.f;
    LContext->ClearColor.G = 0.f;
    LContext->ClearColor.B = 0.f;
    LContext->ClearColor.A = 0.f;

    LContext->ShaderSRGBRemap = GL_FALSE;

    LContext->Blending = GL_FALSE;
    LContext->BlendingMrt[0] = GL_FALSE;
    LContext->BlendingMrt[1] = GL_FALSE;
    LContext->BlendingMrt[2] = GL_FALSE;
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

    for ( int i = 0;i < MAX_TEXTURE_IMAGE_UNITS;++i )
    {
        jsTextureImageUnit *tu = LContext->TextureImageUnits + i;
        tu->bound2D = 0;

        tu->fragmentTarget = 0;

        tu->envMode = GL_MODULATE;
        tu->envColor.R = 0.f;
        tu->envColor.G = 0.f;
        tu->envColor.B = 0.f;
        tu->envColor.A = 0.f;

        tu->currentTexture = NULL;
    }
    for(int i = 0; i < MAX_VERTEX_TEXTURE_IMAGE_UNITS; ++i)
        LContext->VertexTextureImages[i] = NULL;

    LContext->ActiveTexture = 0;
    LContext->CurrentImageUnit = LContext->TextureImageUnits;

    LContext->packAlignment = 4;
    LContext->unpackAlignment = 4;

    LContext->CS_ActiveTexture = 0;

    _RGLResetAttributeState( &LContext->defaultAttribs0 );
    LContext->attribs = &LContext->defaultAttribs0;
    LContext->attribSetName = 0;
    LContext->attribSetDirty = GL_FALSE;

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

static jsTexture *_RGLAllocateTexture (void)
{
    GLuint size = sizeof( jsTexture ) + sizeof( RGLTexture);
    jsTexture *texture = ( jsTexture * )malloc( size );
    memset( texture, 0, size );

    texture->target = 0;
    texture->minFilter = GL_NEAREST_MIPMAP_LINEAR;
    texture->magFilter = GL_LINEAR;
    texture->gammaRemap = 0;
    texture->usage = 0;
    texture->isRenderTarget = GL_FALSE;
    texture->image = NULL;
    texture->isComplete = GL_FALSE;
    texture->imageCount = 0;
    texture->revalidate = 0;
    texture->referenceBuffer = NULL;
    new( &texture->framebuffers ) RGL::Vector<jsFramebuffer *>();
    RGLTexture *gcmTexture = ( RGLTexture * )texture->platformTexture;
    memset( gcmTexture, 0, sizeof( RGLTexture ) );
    gcmTexture->gpuAddressId = GMM_ERROR;
    return texture;
}

static void _RGLFreeTexture( jsTexture *texture )
{
   _RGLTextureTouchFBOs( texture );
   texture->framebuffers.~Vector<jsFramebuffer *>();
   if ( texture->image )
   {
      for ( GLuint i = 0;i < texture->imageCount;++i )
      {
         jsImage *image = texture->image + i;
	 _RGLImageFreeCPUStorage( image );
      }
      if(texture->image != NULL)
         free( texture->image );
   }

   if ( texture->referenceBuffer )
      texture->referenceBuffer->textureReferences.removeElement( texture );

   _RGLPlatformDestroyTexture( texture );

   if(texture != NULL)
      free( texture );
}

PSGLcontext* psglCreateContext (void)
{
    PSGLcontext* LContext = ( PSGLcontext* )malloc( sizeof( PSGLcontext ) );
    if ( !LContext ) return NULL;

    memset( LContext, 0, sizeof( PSGLcontext ) );

    LContext->error = GL_NO_ERROR;
    _RGLTexNameSpaceInit( &LContext->textureNameSpace, ( jsTexNameSpaceCreateFunction )_RGLAllocateTexture, ( jsTexNameSpaceDestroyFunction )_RGLFreeTexture );

    for ( int i = 0;i < MAX_TEXTURE_IMAGE_UNITS;++i )
    {
        jsTextureImageUnit *tu = LContext->TextureImageUnits + i;

        tu->default2D = _RGLAllocateTexture();
        if ( !tu->default2D )
        {
            psglDestroyContext( LContext );
            return NULL;
        }
        tu->default2D->target = GL_TEXTURE_2D;
    }

    _RGLTexNameSpaceInit( &LContext->bufferObjectNameSpace, ( jsTexNameSpaceCreateFunction )_RGLCreateBufferObject, ( jsTexNameSpaceDestroyFunction )_RGLFreeBufferObject );
    _RGLTexNameSpaceInit( &LContext->framebufferNameSpace, ( jsTexNameSpaceCreateFunction )_RGLCreateFramebuffer, ( jsTexNameSpaceDestroyFunction )_RGLDestroyFramebuffer );
    _RGLTexNameSpaceInit( &LContext->attribSetNameSpace, ( jsTexNameSpaceCreateFunction )_RGLCreateAttribSet, ( jsTexNameSpaceDestroyFunction )_RGLDestroyAttribSet );

    LContext->needValidate = 0;
    LContext->everAttached = 0;

    LContext->RGLcgLastError = CG_NO_ERROR;
    LContext->RGLcgErrorCallbackFunction = NULL;
    LContext->RGLcgContextHead = ( CGcontext )NULL;
	
    LContext->cgProgramNameSpace.data = NULL;
    LContext->cgProgramNameSpace.firstFree = NULL;
    LContext->cgProgramNameSpace.capacity = 0;

    LContext->cgParameterNameSpace.data = NULL;
    LContext->cgParameterNameSpace.firstFree = NULL;
    LContext->cgParameterNameSpace.capacity = 0;

    LContext->cgContextNameSpace.data = NULL;
    LContext->cgContextNameSpace.firstFree = NULL;
    LContext->cgContextNameSpace.capacity = 0;

    _RGLResetContext( LContext );

    if ( _RGLContextCreateHook )
       _RGLContextCreateHook( LContext );

    return( LContext );
}

void  psglResetCurrentContext (void)
{
    PSGLcontext *context = _CurrentContext;
    _RGLResetContext( context );
    context->needValidate |= PSGL_VALIDATE_ALL;
}

static bool context_shutdown = false;

void  psglDestroyContext( PSGLcontext* LContext )
{
    context_shutdown = true;

    if ( _CurrentContext == LContext )
    {
	cellGcmSetInvalidateVertexCache( &_RGLState.fifo);
        _RGLFifoFinish( &_RGLState.fifo );
    }

    while ( LContext->RGLcgContextHead != ( CGcontext )NULL )
    {
        PSGLcontext* current = _CurrentContext;
        _CurrentContext = LContext;
        cgDestroyContext( LContext->RGLcgContextHead );
        _CurrentContext = current;
    }

    if (LContext->cgProgramNameSpace.data)
       free( LContext->cgProgramNameSpace.data );

    LContext->cgProgramNameSpace.data = NULL;
    LContext->cgProgramNameSpace.capacity = 0;
    LContext->cgProgramNameSpace.firstFree = NULL;

    if (LContext->cgParameterNameSpace.data)
       free( LContext->cgParameterNameSpace.data );

    LContext->cgParameterNameSpace.data = NULL;
    LContext->cgParameterNameSpace.capacity = 0;
    LContext->cgParameterNameSpace.firstFree = NULL;

    if (LContext->cgContextNameSpace.data)
       free( LContext->cgContextNameSpace.data );

    LContext->cgContextNameSpace.data = NULL;
    LContext->cgContextNameSpace.capacity = 0;
    LContext->cgContextNameSpace.firstFree = NULL;

    if ( _RGLContextDestroyHook ) _RGLContextDestroyHook( LContext );

    for ( int i = 0; i < MAX_TEXTURE_IMAGE_UNITS; ++i )
    {
        jsTextureImageUnit* tu = LContext->TextureImageUnits + i;
        if ( tu->default2D ) _RGLFreeTexture( tu->default2D );
    }

    _RGLTexNameSpaceFree( &LContext->textureNameSpace );
    _RGLTexNameSpaceFree( &LContext->bufferObjectNameSpace );
    _RGLTexNameSpaceFree( &LContext->framebufferNameSpace );
    _RGLTexNameSpaceFree( &LContext->attribSetNameSpace );

    if ( _CurrentContext == LContext )
        psglMakeCurrent( NULL, NULL );

    if(LContext != NULL)
       free( LContext );
}

void _RGLAttachContext( PSGLdevice *device, PSGLcontext* context )
{
   if ( !context->everAttached )
   {
      context->ViewPort.XSize = device->deviceParameters.width;
      context->ViewPort.YSize = device->deviceParameters.height;
      context->everAttached = GL_TRUE;
      _RGLFifoGlViewport(context->ViewPort.X, context->ViewPort.Y, 
      context->ViewPort.XSize, context->ViewPort.YSize, 0.0f, 1.0f);
   }
   context->needValidate = PSGL_VALIDATE_ALL;

   context->attribs->DirtyMask = ( 1 << MAX_VERTEX_ATTRIBS ) - 1;
}

GLAPI void APIENTRY glEnable( GLenum cap )
{
    PSGLcontext* LContext = _CurrentContext;

    switch (cap)
    {
        case GL_SHADER_SRGB_REMAP_SCE:
            LContext->ShaderSRGBRemap = GL_TRUE;
            LContext->needValidate |= PSGL_VALIDATE_SHADER_SRGB_REMAP;
            break;
        case GL_BLEND:
            LContext->Blending = GL_TRUE;
            LContext->BlendingMrt[0] = GL_TRUE;
            LContext->BlendingMrt[1] = GL_TRUE;
            LContext->BlendingMrt[2] = GL_TRUE;
            LContext->needValidate |= PSGL_VALIDATE_BLENDING;
            break;
        case GL_BLEND_MRT0_SCE:
            LContext->Blending = GL_TRUE;
            LContext->needValidate |= PSGL_VALIDATE_BLENDING;
            break;
        case GL_BLEND_MRT1_SCE:
            LContext->BlendingMrt[0] = GL_TRUE;
            LContext->needValidate |= PSGL_VALIDATE_BLENDING;
            break;
        case GL_BLEND_MRT2_SCE:
            LContext->BlendingMrt[1] = GL_TRUE;
            LContext->needValidate |= PSGL_VALIDATE_BLENDING;
            break;
        case GL_BLEND_MRT3_SCE:
            LContext->BlendingMrt[2] = GL_TRUE;
            LContext->needValidate |= PSGL_VALIDATE_BLENDING;
            break;
        case GL_POINT_SMOOTH:
            break;
        case GL_DITHER:
            break;
        case GL_POINT_SPRITE_OES:
        case GL_VERTEX_PROGRAM_POINT_SIZE_ARB:
            break;
        case GL_VSYNC_SCE:
            LContext->VSync = GL_TRUE;
            break;
	case GL_FRAGMENT_PROGRAM_CONTROL_CONTROLTXP_SCE:
	    LContext->AllowTXPDemotion = GL_TRUE;
	    LContext->needValidate |= PSGL_VALIDATE_FRAGMENT_PROGRAM;
	    break; 
        default:
            _RGLSetError( GL_INVALID_ENUM );
            return;
    }
}

GLAPI void APIENTRY glDisable(GLenum cap)
{
    PSGLcontext* LContext = _CurrentContext;

    switch ( cap )
    {
        case GL_SHADER_SRGB_REMAP_SCE:
            LContext->ShaderSRGBRemap = GL_FALSE;
            LContext->needValidate |= PSGL_VALIDATE_SHADER_SRGB_REMAP;
            break;
        case GL_BLEND:
            LContext->Blending = GL_FALSE;
            LContext->BlendingMrt[0] = GL_FALSE;
            LContext->BlendingMrt[1] = GL_FALSE;
            LContext->BlendingMrt[2] = GL_FALSE;
            LContext->needValidate |= PSGL_VALIDATE_BLENDING;
            break;

        case GL_BLEND_MRT0_SCE:
            LContext->Blending = GL_FALSE;
            LContext->needValidate |= PSGL_VALIDATE_BLENDING;
            break;

        case GL_BLEND_MRT1_SCE:
            LContext->BlendingMrt[0] = GL_FALSE;
            LContext->needValidate |= PSGL_VALIDATE_BLENDING;
            break;

        case GL_BLEND_MRT2_SCE:
            LContext->BlendingMrt[1] = GL_FALSE;
            LContext->needValidate |= PSGL_VALIDATE_BLENDING;
            break;

        case GL_BLEND_MRT3_SCE:
            LContext->BlendingMrt[2] = GL_FALSE;
            LContext->needValidate |= PSGL_VALIDATE_BLENDING;
            break;
        case GL_POINT_SMOOTH:
        case GL_LINE_SMOOTH:
            break;
        case GL_DITHER:
            break;
        case GL_POINT_SPRITE_OES:
        case GL_VERTEX_PROGRAM_POINT_SIZE_ARB:
            break;
        case GL_VSYNC_SCE:
            LContext->VSync = GL_FALSE;
            break;
	case GL_FRAGMENT_PROGRAM_CONTROL_CONTROLTXP_SCE:
	    LContext->AllowTXPDemotion = GL_FALSE;
	    LContext->needValidate |= PSGL_VALIDATE_FRAGMENT_PROGRAM;
	    break; 

        default:
            _RGLSetError( GL_INVALID_ENUM );
            return;
    }
}

GLAPI void APIENTRY glEnableClientState(GLenum array)
{
    PSGLcontext* LContext = _CurrentContext;

    switch(array)
    {
        case GL_VERTEX_ARRAY:
            _RGLEnableVertexAttribArrayNV( _RGL_ATTRIB_POSITION_INDEX );
            break;
        case GL_COLOR_ARRAY:
            _RGLEnableVertexAttribArrayNV( _RGL_ATTRIB_PRIMARY_COLOR_INDEX );
            break;
        case GL_NORMAL_ARRAY:
            _RGLEnableVertexAttribArrayNV( _RGL_ATTRIB_NORMAL_INDEX );
            break;
        case GL_TEXTURE_COORD_ARRAY:
            _RGLEnableVertexAttribArrayNV( _RGL_ATTRIB_TEX_COORD0_INDEX + LContext->CS_ActiveTexture );
            break;
        default:
            _RGLSetError( GL_INVALID_ENUM );
            return;
    }
}

GLAPI void APIENTRY glDisableClientState(GLenum array)
{
    PSGLcontext* LContext = _CurrentContext;

    switch ( array )
    {
        case GL_VERTEX_ARRAY:
            _RGLDisableVertexAttribArrayNV( _RGL_ATTRIB_POSITION_INDEX );
            break;
        case GL_COLOR_ARRAY:
            _RGLDisableVertexAttribArrayNV( _RGL_ATTRIB_PRIMARY_COLOR_INDEX );
            break;
        case GL_NORMAL_ARRAY:
            _RGLDisableVertexAttribArrayNV( _RGL_ATTRIB_NORMAL_INDEX );
            break;
        case GL_TEXTURE_COORD_ARRAY:
            _RGLDisableVertexAttribArrayNV( _RGL_ATTRIB_TEX_COORD0_INDEX + LContext->CS_ActiveTexture );
            break;
        default:
            _RGLSetError( GL_INVALID_ENUM );
            return;
    }
}

GLAPI void APIENTRY glFlush(void)
{
   PSGLcontext *LContext = _CurrentContext;
   RGLFifo *fifo = &_RGLState.fifo;

   if ( RGL_UNLIKELY( LContext->needValidate ) )
      _RGLValidateStates();

   cellGcmSetInvalidateVertexCache( &_RGLState.fifo);

   _RGLFifoFlush( fifo );
}

GLAPI void APIENTRY glFinish(void)
{
   glFlush();
   cellGcmSetInvalidateVertexCache( &_RGLState.fifo);
   _RGLFifoFinish( &_RGLState.fifo );
}

GLAPI const GLubyte* APIENTRY glGetString( GLenum name )
{
    switch ( name )
    {
        case GL_VENDOR:
            return(( GLubyte* )_RGLVendorString );
        case GL_RENDERER:
            return(( GLubyte* )_RGLRendererString );
        case GL_VERSION:
            return(( GLubyte* )_RGLVersionNumber );
        case GL_EXTENSIONS:
            return(( GLubyte* )_RGLExtensionsString );
        default:
        {
            _RGLSetError( GL_INVALID_ENUM );
            return(( GLubyte* )NULL );
        }
    }
}


void psglInit( PSGLinitOptions* options )
{
   if ( !_RGLInitCompleted )
   {
      int ret = cellSysmoduleLoadModule( CELL_SYSMODULE_GCM_SYS );
      ret = cellSysmoduleLoadModule( CELL_SYSMODULE_RESC );

      _RGLDeviceInit( options );
      _CurrentContext = NULL;
      _CurrentDevice = NULL;
   }

   _RGLInitCompleted = 1;
}

void psglExit (void)
{
   PSGLcontext* LContext = _CurrentContext;
   if ( LContext )
   {
      glFlush();
      cellGcmSetInvalidateVertexCache ( &_RGLState.fifo);
      _RGLFifoFinish( &_RGLState.fifo );

      psglMakeCurrent( NULL, NULL );
      _RGLDeviceExit();

      _CurrentContext = NULL; 

      _RGLInitCompleted = 0;
   }
}

#undef __STRICT_ANSI__

GLAPI void APIENTRY glVertexPointer( GLint size, GLenum type, GLsizei stride, const GLvoid* pointer )
{
   _RGLVertexAttribPointerNV( _RGL_ATTRIB_POSITION_INDEX, size, type, GL_FALSE, stride, pointer );
}

GLAPI void APIENTRY glTexCoordPointer( GLint size, GLenum type, GLsizei stride, const GLvoid* pointer )
{
    PSGLcontext*	LContext = _CurrentContext;

    _RGLVertexAttribPointerNV(
        _RGL_ATTRIB_TEX_COORD0_INDEX + LContext->CS_ActiveTexture,
        size,
        type,
        GL_FALSE,
        stride,
        pointer );
}

GLAPI void APIENTRY glColorPointer( GLint size, GLenum type, GLsizei stride, const GLvoid* pointer )
{
    _RGLVertexAttribPointerNV( _RGL_ATTRIB_PRIMARY_COLOR_INDEX, size, type, GL_TRUE, stride, pointer );
}

static GLboolean _RGLPlatformNeedsConversion( const jsAttributeState* as, GLuint index )
{
    const jsAttribute* attrib = as->attrib + index;

    switch ( attrib->clientType )
    {
        case GL_SHORT:
        case GL_HALF_FLOAT_ARB:
        case GL_FLOAT:
        case GL_FIXED_11_11_10_SCE:
            return GL_FALSE;
        case GL_UNSIGNED_BYTE:
            if ( attrib->normalized ||
                 attrib->clientSize == 4 ) 
                 return GL_FALSE; 
            break;
        default:
            break;
    }
    RARCH_WARN("Attribute %d needs conversion. Slow path ahead.\n", index);
    return GL_TRUE;
}

static int _RGLGetTypeSize( GLenum type )
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

void _RGLVertexAttribPointerNV(
    GLuint index,
    GLint fsize,
    GLenum type,
    GLboolean normalized,
    GLsizei stride,
    const GLvoid* pointer )
{
    PSGLcontext* LContext = _CurrentContext;

    GLsizei defaultStride = 0;
    switch ( type )
    {
        case GL_FLOAT:
        case GL_HALF_FLOAT_ARB:
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
        case GL_SHORT:
        case GL_FIXED:
            defaultStride = fsize * _RGLGetTypeSize( type );
            break;
        case GL_FIXED_11_11_10_SCE:
            defaultStride = 4;
            break;
        default:
            _RGLSetError( GL_INVALID_ENUM );
            return;
    }

    jsAttributeState* as = LContext->attribs;
    jsAttribute* attrib = as->attrib + index;
    attrib->clientSize = fsize;
    attrib->clientType = type;
    attrib->clientStride = stride ? stride : defaultStride;
    attrib->clientData = ( void* )pointer;
    GLuint oldArrayBuffer = attrib->arrayBuffer;
    attrib->arrayBuffer = LContext->ArrayBuffer;
    attrib->normalized = normalized;
    RGLBIT_ASSIGN( as->HasVBOMask, index, attrib->arrayBuffer != 0 );
    GLboolean needConvert = _RGLPlatformNeedsConversion( as, index );
    RGLBIT_ASSIGN( as->NeedsConversionMask, index, needConvert );

    RGLBIT_TRUE( as->DirtyMask, index );

    if ( LContext->attribSetName )
    {
        jsAttribSet* attribSet = (jsAttribSet*)LContext->attribSetNameSpace.data[LContext->attribSetName];

        if ( oldArrayBuffer )
        {
            int refcount = 0;
            for(unsigned int i = 0; i < MAX_VERTEX_ATTRIBS; ++i )
            {
                if ( attribSet->attribs.attrib[i].arrayBuffer == oldArrayBuffer ) ++refcount;
            }
            if(refcount == 1)
            {
               jsBufferObject *buffer = (jsBufferObject *)LContext->bufferObjectNameSpace.data[oldArrayBuffer];
               buffer->attribSets.removeElement(attribSet);
            }
        }

        if ( attrib->arrayBuffer )
        {
	    jsBufferObject *buffer = (jsBufferObject *)LContext->bufferObjectNameSpace.data[attrib->arrayBuffer];
	    buffer->attribSets.appendUnique(attribSet);
        }

	attribSet->dirty = GL_TRUE;
	LContext->attribSetDirty = GL_TRUE;
    }
}

void _RGLEnableVertexAttribArrayNV( GLuint index )
{
   PSGLcontext *LContext = _CurrentContext;
   jsAttribSet* attribSet = (jsAttribSet*)LContext->attribSetNameSpace.data[LContext->attribSetName];

    RGLBIT_TRUE( LContext->attribs->EnabledMask, index );
    RGLBIT_TRUE( LContext->attribs->DirtyMask, index );

    if ( LContext->attribSetName )
    {
       attribSet->dirty = GL_TRUE;
       LContext->attribSetDirty = GL_TRUE;
    }
}

void _RGLDisableVertexAttribArrayNV( GLuint index )
{
    PSGLcontext *LContext = _CurrentContext;
   jsAttribSet* attribSet = (jsAttribSet*)LContext->attribSetNameSpace.data[LContext->attribSetName];

    RGLBIT_FALSE( LContext->attribs->EnabledMask, index );
    RGLBIT_TRUE( LContext->attribs->DirtyMask, index );

    if ( LContext->attribSetName )
    {
       attribSet->dirty = GL_TRUE;
       LContext->attribSetDirty = GL_TRUE;
    }
}

static GLuint _RGLValidateAttributesSlow( jsDrawParams *dparams)
{
   PSGLcontext*	LContext = _CurrentContext;
   RGLDriver *driver= (RGLDriver *)_CurrentDevice->rasterDriver;
   jsAttributeState* as = LContext->attribs;

   void* xferBuffer = NULL;
   GLuint xferId = GMM_ERROR;
   GLuint VBOId = GMM_ERROR;
   GLuint gpuOffset;

   if(RGL_UNLIKELY( dparams->xferTotalSize))
   {
      xferId = gmmAlloc(0, dparams->xferTotalSize);
      xferBuffer = gmmIdToAddress(xferId);
   }

   unsigned int needsUpdateMask = (as->DirtyMask | (as->EnabledMask & ~as->HasVBOMask));

   LContext->attribSetDirty = GL_FALSE;

   if ( needsUpdateMask )
   {
      for ( GLuint i = 0; i < MAX_VERTEX_ATTRIBS; ++i )
      {
         if(!RGLBIT_GET(needsUpdateMask, i))
            continue;

	 jsAttribute* attrib = as->attrib + i;

	 if ( RGLBIT_GET( as->EnabledMask, i ) )
	 {
            GLsizei stride = attrib->clientStride;
	    const GLuint freq = attrib->frequency;

	    if ( RGL_UNLIKELY( dparams->attribXferSize[i] ) )
	    {
               GmmBaseBlock *pBaseBlock = (GmmBaseBlock *)xferId;

               GLuint maxElements = dparams->firstVertex + dparams->vertexCount;

	       GLuint offset;
	       if(RGLBIT_GET( as->ModuloMask, i))
                  offset = ( maxElements > freq ) ? 0 : dparams->firstVertex * stride;
	       else
                  offset = ( dparams->firstVertex / freq ) * stride;

	       char *b = (char *)xferBuffer + dparams->attribXferOffset[i];
	       memcpy( b + offset, ( char * )attrib->clientData + offset,
               dparams->attribXferSize[i] - offset );

	       gpuOffset = gmmAddressToOffset(pBaseBlock->address, pBaseBlock->isMain) + (b - ( char * )xferBuffer);
	    }
	    else
	    {
               VBOId = _RGLGetBufferObjectOrigin( attrib->arrayBuffer );
               GmmBaseBlock *pBaseBlock = (GmmBaseBlock *)VBOId;
	       gpuOffset = gmmAddressToOffset(pBaseBlock->address, pBaseBlock->isMain) + (( const GLubyte* )attrib->clientData - ( const GLubyte* )NULL );
	    }

	    if(attrib->clientSize == 0)
	    {
               stride = 0;
	       attrib->normalized = 0;
	       attrib->clientType = RGL_FLOAT;
	       gpuOffset = 0;
	    }

	    uint8_t gcmType = 0;

	    switch ( (RGLEnum)attrib->clientType )
	    {
		    case RGL_UNSIGNED_BYTE:
			    gcmType = attrib->normalized ? CELL_GCM_VERTEX_UB : CELL_GCM_VERTEX_UB256;
			    break;
		    case RGL_SHORT:
			    gcmType = attrib->normalized ? CELL_GCM_VERTEX_S1 : CELL_GCM_VERTEX_S32K;
			    break;
		    case RGL_FLOAT:
			    gcmType = CELL_GCM_VERTEX_F;
			    break;
		    case RGL_HALF_FLOAT:
			    gcmType = CELL_GCM_VERTEX_SF;
			    break;
		    case RGL_CMP:
			    attrib->clientSize = 1;
			    gcmType = CELL_GCM_VERTEX_CMP;
			    break;
		    default:
			    break;
	    }

	    cellGcmSetVertexDataArrayInline( &_RGLState.fifo, i, freq, stride, attrib->clientSize, gcmType, CELL_GCM_LOCATION_LOCAL, gpuOffset );
	 }
	 else
	 {
	    cellGcmSetVertexDataArrayInline( &_RGLState.fifo, i, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
	    cellGcmSetVertexData4fInline( &_RGLState.fifo, i,attrib->value);
	 }
      }
      cellGcmSetFrequencyDividerOperationInline( &_RGLState.fifo, as->ModuloMask);
      driver->invalidateVertexCache = GL_TRUE;
   }

   as->DirtyMask = 0;

   if ( xferId != GMM_ERROR )
      gmmFree( xferId );

   return 0;
}

GLAPI void APIENTRY glDrawArrays( GLenum mode, GLint first, GLsizei count )
{
   static uint8_t s_dparams_buff[ (sizeof(jsDrawParams) + 0x7f) & ~0x7f ] __attribute__((aligned(128)));

   PSGLcontext*	LContext = _CurrentContext;
   jsAttributeState* as = LContext->attribs;
   RGLDriver *driver= (RGLDriver *)_CurrentDevice->rasterDriver;
   const GLuint clientSideMask = as->EnabledMask & ~as->HasVBOMask;

   if ( RGL_UNLIKELY( ! RGLBIT_GET( LContext->attribs->EnabledMask, _RGL_ATTRIB_POSITION_INDEX ) ) ) return;

   uint32_t _tmp_clear_loop = ((sizeof(jsDrawParams) + 0x7f) & ~0x7f) >> 7;

   do{
      --_tmp_clear_loop;
      __dcbz(s_dparams_buff+(_tmp_clear_loop << 7));
   }while(_tmp_clear_loop);

   jsDrawParams *dparams = (jsDrawParams *)s_dparams_buff;
   dparams->mode = mode;
   dparams->firstVertex = first;
   dparams->vertexCount = count;
   GLuint maxElements = dparams->firstVertex + dparams->vertexCount;

   if ( LContext->needValidate )
      _RGLValidateStates();

   if ( RGL_UNLIKELY( clientSideMask ) )
   {
      for ( int i = 0; i < MAX_VERTEX_ATTRIBS; ++i )
      {
         dparams->attribXferOffset[i] = 0;
	 dparams->attribXferSize[i] = 0;

	 if(clientSideMask & (1 << i))
	 {
            jsAttribute* attrib = as->attrib + i;
	    const GLuint freq = attrib->frequency;
	    GLuint count;

	    if (RGLBIT_GET(as->ModuloMask, i))
               count = maxElements > freq ? freq : maxElements;
            else
               count = ( maxElements + freq - 1 ) / freq;

	    const GLuint numBytes = attrib->clientStride * count;
	    dparams->attribXferOffset[i] = dparams->xferTotalSize;
	    dparams->attribXferSize[i] = numBytes;

	    const GLuint numBytesPadded = _RGLPad( numBytes, 128 );
	    dparams->xferTotalSize += numBytesPadded;
	    dparams->attribXferTotalSize += numBytesPadded;
	 }
      }
   }

   if ( driver->flushBufferCount != 0 )
	   driver->invalidateVertexCache = GL_TRUE;

   uint32_t totalXfer = 0;

   for (GLuint i = 0; i < MAX_VERTEX_ATTRIBS; ++i)
      totalXfer += dparams->attribXferSize[i];

   GLuint gpuOffset = _RGLValidateAttributesSlow( dparams);
   (void)gpuOffset;

   if(driver->invalidateVertexCache)
   {
      driver->invalidateVertexCache = GL_FALSE;
      cellGcmSetInvalidateVertexCache ( &_RGLState.fifo);
   }

   GmmBaseBlock *pBaseBlock = (GmmBaseBlock *)driver->fpLoadProgramId;
   cellGcmSetUpdateFragmentProgramParameterInline( &_RGLState.fifo, gmmAddressToOffset(pBaseBlock->address, pBaseBlock->isMain) +driver->fpLoadProgramOffset );

   cellGcmSetDrawArraysInline( &_RGLState.fifo, CELL_GCM_PRIMITIVE_QUADS, dparams->firstVertex, dparams->vertexCount);
}

GLAPI void APIENTRY glGenTextures( GLsizei n, GLuint *textures )
{
   PSGLcontext*	LContext = _CurrentContext;
   _RGLTexNameSpaceGenNames( &LContext->textureNameSpace, n, textures );
}

static void _RGLTextureUnbind( PSGLcontext* context, GLuint name )
{
    int unit;
    for (unit = 0; unit < MAX_TEXTURE_IMAGE_UNITS; ++unit)
    {
        jsTextureImageUnit *tu = context->TextureImageUnits + unit;
        GLboolean dirty = GL_FALSE;
        if ( tu->bound2D == name )
        {
            tu->bound2D = 0;
            dirty = GL_TRUE;
        }
        if ( dirty )
        {
	    tu->currentTexture = _RGLGetCurrentTexture( tu, GL_TEXTURE_2D );
            context->needValidate |= PSGL_VALIDATE_TEXTURES_USED;
        }
    }
    if(_RGLTexNameSpaceIsName( &context->textureNameSpace, name))
    {
        jsTexture*texture = ( jsTexture * )context->textureNameSpace.data[name];
        for ( unit = 0;unit < MAX_VERTEX_TEXTURE_IMAGE_UNITS; ++unit )
        {
            if ( context->VertexTextureImages[unit] == texture )
            {
                context->VertexTextureImages[unit] = NULL;
                context->needValidate |= PSGL_VALIDATE_VERTEX_TEXTURES_USED;
            }
        }
    }
}

GLAPI void APIENTRY glDeleteTextures( GLsizei n, const GLuint *textures )
{
   PSGLcontext*	LContext = _CurrentContext;

   for(int i = 0;i < n; ++i)
      if(textures[i])
         _RGLTextureUnbind(LContext, textures[i]);

   _RGLTexNameSpaceDeleteNames( &LContext->textureNameSpace, n, textures );
} 

GLAPI void APIENTRY glTexParameteri( GLenum target, GLenum pname, GLint param )
{
   PSGLcontext*	LContext = _CurrentContext;
   jsTexture *texture = _RGLGetCurrentTexture( LContext->CurrentImageUnit, target );

   switch ( pname )
   {
	   case GL_TEXTURE_MIN_FILTER:
		   texture->minFilter = param;
		   if ( texture->referenceBuffer == 0 )
                      texture->revalidate |= TEXTURE_REVALIDATE_LAYOUT;
		   break;
	   case GL_TEXTURE_MAG_FILTER:
		   texture->magFilter = param;
		   break;
	   case GL_TEXTURE_MAX_LEVEL:
	   case GL_TEXTURE_WRAP_S:
	   case GL_TEXTURE_WRAP_T:
	   case GL_TEXTURE_WRAP_R:
	   case GL_TEXTURE_FROM_VERTEX_PROGRAM_SCE:
		   break;
	   case GL_TEXTURE_ALLOCATION_HINT_SCE:
		   texture->usage = param;
		   texture->revalidate |= TEXTURE_REVALIDATE_LAYOUT;
		   break;
	   case GL_TEXTURE_MIN_LOD:
	   case GL_TEXTURE_MAX_LOD:
	   case GL_TEXTURE_LOD_BIAS:
	   case GL_TEXTURE_MAX_ANISOTROPY_EXT:
	   case GL_DEPTH_TEXTURE_MODE_ARB:
	   case GL_TEXTURE_COMPARE_MODE_ARB:
	   case GL_TEXTURE_COMPARE_FUNC_ARB:
		   break;
	   case GL_TEXTURE_GAMMA_REMAP_R_SCE:
	   case GL_TEXTURE_GAMMA_REMAP_G_SCE:
	   case GL_TEXTURE_GAMMA_REMAP_B_SCE:
	   case GL_TEXTURE_GAMMA_REMAP_A_SCE:
		   {
			   GLuint bit = 1 << ( pname - GL_TEXTURE_GAMMA_REMAP_R_SCE );
			   if(param)
                              texture->gammaRemap |= bit;
			   else
                              texture->gammaRemap &= ~bit;
		   }
		   break;
	   default:
		   _RGLSetError( GL_INVALID_ENUM );
		   return;
   }

   texture->revalidate |= TEXTURE_REVALIDATE_PARAMETERS;
   LContext->needValidate |= PSGL_VALIDATE_TEXTURES_USED | PSGL_VALIDATE_VERTEX_TEXTURES_USED;
}

GLAPI void APIENTRY glBindTexture( GLenum target, GLuint name )
{
   PSGLcontext*	LContext = _CurrentContext;
   jsTextureImageUnit *unit = LContext->CurrentImageUnit;

   _RGLBindTextureInternal( unit, name);
}

static void _RGLReallocateImages( jsTexture *texture, GLsizei dimension )
{
   GLuint oldCount = texture->imageCount;

   if ( dimension <= 0 )
      dimension = 1;

   GLuint n = 1 + _RGLLog2( dimension );
   n = MAX( n, oldCount );

   jsImage *images = ( jsImage * )realloc( texture->image, n * sizeof( jsImage ) );

   memset( images + oldCount, 0, ( n - oldCount )*sizeof( jsImage ) );

   texture->image = images;
   texture->imageCount = n;
}

GLAPI void APIENTRY glTexImage2D( GLenum target, GLint level, GLint internalFormat,
GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels )
{
    PSGLcontext* LContext = _CurrentContext;
    jsTexture *texture;
    jsImage *image;

   jsTextureImageUnit *unit = LContext->CurrentImageUnit;

   jsTexture *tex = _RGLGetCurrentTexture(unit, GL_TEXTURE_2D);

   if(0 >= (int)tex->imageCount)
      _RGLReallocateImages(tex, MAX(width, height));

   image = tex->image;
   texture = tex;

    image->dataState = IMAGE_DATASTATE_UNSET;

    GLboolean directPBO = GL_FALSE;
    if ( LContext->PixelUnpackBuffer != 0 )
    {
        directPBO = _RGLPlatformTexturePBOImage(
                        texture,
                        image,
                        internalFormat,
                        width, height,
                        format, type,
                        pixels );
    }

    if ( !directPBO )
    {
        jsBufferObject* bufferObject = NULL;
        if ( LContext->PixelUnpackBuffer != 0 )
        {
            bufferObject = (jsBufferObject *)LContext->bufferObjectNameSpace.data[LContext->PixelUnpackBuffer];
            pixels = _RGLPlatformBufferObjectMap( bufferObject, GL_READ_ONLY ) +
                     (( const GLubyte* )pixels - ( const GLubyte* )NULL );
        }

        _RGLSetImage(image, internalFormat, width, height, 1, LContext->unpackAlignment,
        format, type, pixels);

        if ( LContext->PixelUnpackBuffer != 0 )
	{
           RGLBufferObject *jsBuffer = ( RGLBufferObject * )bufferObject->platformBufferObject;

	   if ( --jsBuffer->mapCount == 0 )
	   {
              if ( jsBuffer->mapAccess != GL_READ_ONLY )
	      {
                 RGLDriver *driver= (RGLDriver *)_CurrentDevice->rasterDriver;
		 --driver->flushBufferCount;

		 driver->invalidateVertexCache = GL_TRUE;
	      }

	      jsBuffer->mapAccess = GL_NONE;

	      GmmBaseBlock    *pBaseBlock = (GmmBaseBlock *)jsBuffer->bufferId;

	      if (!pBaseBlock->isTile)
	      {
                 GmmBlock *pBlock = (GmmBlock *)jsBuffer->bufferId;
		 pBlock->isPinned = 0;
	      }
	   }
	}

        texture->revalidate |= TEXTURE_REVALIDATE_IMAGES;
    }

    _RGLTextureTouchFBOs( texture );
    LContext->needValidate |= PSGL_VALIDATE_TEXTURES_USED | PSGL_VALIDATE_VERTEX_TEXTURES_USED;
}

GLAPI void APIENTRY glActiveTexture(GLenum texture)
{
   PSGLcontext* LContext = _CurrentContext;

   int unit = texture - GL_TEXTURE0;
   LContext->ActiveTexture = unit;
   LContext->CurrentImageUnit = unit < MAX_TEXTURE_IMAGE_UNITS ? LContext->TextureImageUnits + unit : NULL;
}

GLAPI void APIENTRY glClientActiveTexture(GLenum texture)
{
   PSGLcontext*	LContext = _CurrentContext;
   LContext->CS_ActiveTexture = texture - GL_TEXTURE0;
}

GLAPI void APIENTRY glPixelStorei(GLenum pname, GLint param)
{
   PSGLcontext* LContext = _CurrentContext;

   switch ( pname )
   {
      case GL_PACK_ALIGNMENT:
         LContext->packAlignment = param;
	 break;
      case GL_UNPACK_ALIGNMENT:
	 LContext->unpackAlignment = param;
	 break;
      default:
	 _RGLSetError( GL_INVALID_ENUM );
	 return;
   }
}


GLAPI void APIENTRY glTextureReferenceSCE( GLenum target, GLuint levels, GLuint baseWidth, GLuint baseHeight, GLuint baseDepth, GLenum internalFormat, GLuint pitch, GLintptr offset )
{
   PSGLcontext*	LContext = _CurrentContext;

   jsTexture *texture = _RGLGetCurrentTexture( LContext->CurrentImageUnit, GL_TEXTURE_2D);
   jsBufferObject *bufferObject = (jsBufferObject *)LContext->bufferObjectNameSpace.data[LContext->TextureBuffer];
   _RGLReallocateImages( texture, MAX( baseWidth, MAX( baseHeight, baseDepth ) ) );

   GLuint width = baseWidth;
   GLuint height = baseHeight;
   _RGLSetImage(texture->image, GL_RGB5_A1, width, height, 0, LContext->unpackAlignment,
   0, 0, NULL );
   width = MAX( 1U, width / 2 );
   height = MAX( 1U, height / 2 );
   texture->usage = GL_TEXTURE_LINEAR_GPU_SCE;

   GLboolean r = _RGLPlatformTextureReference( texture, pitch, bufferObject, offset );

   if(!r)
      return;

   bufferObject->textureReferences.pushBack( texture );
   texture->referenceBuffer = bufferObject;
   texture->offset = offset;
   _RGLTextureTouchFBOs( texture );
   LContext->needValidate |= PSGL_VALIDATE_TEXTURES_USED | PSGL_VALIDATE_VERTEX_TEXTURES_USED ;
}

GLAPI void APIENTRY glViewport( GLint x, GLint y, GLsizei width, GLsizei height )
{
   PSGLcontext*	LContext = _CurrentContext;

   LContext->ViewPort.X = x;
   LContext->ViewPort.Y = y;
   LContext->ViewPort.XSize = width;
   LContext->ViewPort.YSize = height;

   _RGLFifoGlViewport(LContext->ViewPort.X, LContext->ViewPort.Y, 
   LContext->ViewPort.XSize, LContext->ViewPort.YSize, 0.0f, 1.0f);
}

jsTexture *_RGLGetCurrentTexture( const jsTextureImageUnit *unit, GLenum target )
{
    PSGLcontext*	LContext = _CurrentContext;
    GLuint name = unit->bound2D;
    jsTexture *defaultTexture = unit->default2D;

    if ( name )
        return ( jsTexture * )LContext->textureNameSpace.data[name];
    else
    	return defaultTexture;
}

CgprogramHookFunction _cgProgramCreateHook = NULL;
CgprogramHookFunction _cgProgramDestroyHook = NULL;
CgprogramCopyHookFunction _cgProgramCopyHook = NULL;

cgRTCgcCompileHookFunction _cgRTCgcCompileProgramHook = NULL;
cgRTCgcFreeHookFunction _cgRTCgcFreeCompiledProgramHook;

CgcontextHookFunction _cgContextCreateHook = NULL;
CgcontextHookFunction _cgContextDestroyHook = NULL;

CgparameterHookFunction _cgParameterCreateHook = NULL;
CgparameterHookFunction _cgParameterDestroyHook = NULL;

typedef struct RGLcgProfileMapType
{
   CGprofile id;
   char* string;
   int is_vertex_program;
} RGLcgProfileMapType;

static void _RGLCgProgramPushFront( _CGcontext* ctx, _CGprogram* prog )
{
   prog->next = ctx->programList;
   ctx->programList = prog;
   prog->parentContext = ctx;
   ctx->programCount++;
}

static _CGprogram* _RGLCgProgramFindPrev( _CGcontext* ctx, _CGprogram* prog )
{
   _CGprogram* ptr = ctx->programList;

   while ( ptr != NULL && prog != ptr->next )
      ptr = ptr->next;

   return ptr;
}

void _RGLCgProgramErase( _CGprogram* prog )
{
   if ( _cgProgramDestroyHook ) _cgProgramDestroyHook( prog );

   switch ( prog->header.profile )
   {
      case CG_PROFILE_SCE_VP_TYPEB:
      case CG_PROFILE_SCE_VP_RSX:
      case CG_PROFILE_SCE_FP_TYPEB:
      case CG_PROFILE_SCE_FP_RSX:
         _RGLPlatformProgramErase( prog );
	 break;
      default:
         break;
   }

   if ( prog->id )
      _RGLEraseName( &_CurrentContext->cgProgramNameSpace, ( jsName )prog->id );

   if ( prog->runtimeElf )
      free( prog->runtimeElf );

   memset( prog, 0, sizeof( _CGprogram ) );
}

bool _RGLCgCreateProgramChecks( CGcontext ctx, CGprofile profile, CGenum program_type )
{
    if ( !CG_IS_CONTEXT( ctx ) )
    {
        _RGLCgRaiseError( CG_INVALID_CONTEXT_HANDLE_ERROR );
        return false;
    }

    switch ( profile )
    {
        case CG_PROFILE_SCE_VP_TYPEB:
        case CG_PROFILE_SCE_FP_TYPEB:
        case CG_PROFILE_SCE_VP_RSX:
        case CG_PROFILE_SCE_FP_RSX:
            break;
        default:
            _RGLCgRaiseError( CG_UNKNOWN_PROFILE_ERROR );
            return false;
    }

    switch ( program_type )
    {
        case CG_BINARY:
        case CG_SOURCE:
            break;
        default:
            _RGLCgRaiseError( CG_INVALID_ENUMERANT_ERROR );
            return false;
    }

    return true;
}

typedef struct
{
   const char* elfFile;
   size_t elfFileSize;

   const char *symtab;
   size_t symbolSize;
   size_t symbolCount;
   const char *symbolstrtab;

   const char* shadertab;
   size_t shadertabSize;
   const char* strtab;
   size_t strtabSize;
   const char* consttab;
   size_t consttabSize;
} CGELFBinary;

typedef struct
{
   const char *texttab;
   size_t texttabSize;
   const char *paramtab;
   size_t paramtabSize;
   int index;
} CGELFProgram;

static bool cgOpenElf( const void *ptr, size_t size, CGELFBinary *elfBinary )
{
    while(1)
    {
        size_t shadertabSize;
        size_t consttabSize;
        size_t strtabSize;
        size_t symbolSize;
        size_t symbolCount;
        const char *symbolstrtab;

        const char *symtab = findSymbolSectionInPlace(( const char * )ptr, size, &symbolSize, &symbolCount, &symbolstrtab );
        if ( !symtab )
            break;

        const char *shadertab = findSectionInPlace(( const char* )ptr, size, ".shadertab", &shadertabSize );

        if ( !shadertab )
            break;

        const char *strtab = findSectionInPlace(( const char* )ptr, size, ".strtab", &strtabSize );

        if ( !strtab )
            break;

        const char *consttab = findSectionInPlace(( const char* )ptr, size, ".const", &consttabSize );
        if ( !consttab )
            break;

        elfBinary->elfFile = ( const char* )ptr;
        elfBinary->elfFileSize = size;
        elfBinary->symtab = symtab;
        elfBinary->symbolSize = symbolSize;
        elfBinary->symbolCount = symbolCount;
        elfBinary->symbolstrtab = symbolstrtab;

        elfBinary->shadertab = shadertab;
        elfBinary->shadertabSize = shadertabSize;
        elfBinary->strtab = strtab;
        elfBinary->strtabSize = strtabSize;

        elfBinary->consttab = consttab;
        elfBinary->consttabSize = consttabSize;

        return true;
    }

    return false;
}

static bool cgGetElfProgramByIndex( CGELFBinary *elfBinary, int index, CGELFProgram *elfProgram )
{
    while(1)
    {
        char sectionName[64];
        snprintf( sectionName, sizeof(sectionName), ".text%04i", index );
        size_t texttabSize;
        const char *texttab = findSectionInPlace( elfBinary->elfFile, elfBinary->elfFileSize, sectionName, &texttabSize );
        if ( !texttab )
            break;
        snprintf( sectionName, sizeof(sectionName), ".paramtab%04i", index );
        size_t paramtabSize;
        const char *paramtab = findSectionInPlace( elfBinary->elfFile, elfBinary->elfFileSize, sectionName, &paramtabSize );
        if ( !paramtab )
            break;

        elfProgram->texttab = texttab;
        elfProgram->texttabSize = texttabSize;
        elfProgram->paramtab = paramtab;
        elfProgram->paramtabSize = paramtabSize;
        elfProgram->index = index;
        return true;
    }
    return false;
}

static bool cgGetElfProgramByName( CGELFBinary *elfBinary, const char *name, CGELFProgram *elfProgram )
{
   //if no name try to return the first program
   int ret;

   if ( name == NULL || name[0] == '\0' )
      ret = 0;
   else
      ret = lookupSymbolValueInPlace( elfBinary->symtab, elfBinary->symbolSize, elfBinary->symbolCount, elfBinary->symbolstrtab, name );

   if (ret != -1)
      return cgGetElfProgramByIndex( elfBinary, ret, elfProgram );
   else
      return false;
}

static CGprogram _RGLCgCreateProgram( CGcontext ctx, CGprofile profile, const CgProgramHeader *programHeader, const void *ucode, const CgParameterTableHeader *parameterHeader, const char *stringTable, const float *defaultValues )
{
    // Create the program structure.
    // all the structural data is filled in here,
    // as well as the profile.
    // The parameters and the actual program are generated from the ABI specific calls.

    _CGprogram* prog = ( _CGprogram* )malloc( sizeof( _CGprogram ) );
    if(prog == NULL)
    {
        _RGLCgRaiseError( CG_MEMORY_ALLOC_ERROR );
        return NULL;
    }

    // zero out the fields
    memset( prog, 0, sizeof( _CGprogram ) );

    // fill in the fields we know
    prog->parentContext = _cgGetContextPtr( ctx );
    prog->header.profile = profile;

    int success = 0;

    // create a name for the program and record it in the object
    CGprogram id = ( CGprogram )_RGLCreateName( &_CurrentContext->cgProgramNameSpace, prog );
    if(!id)
    {
        free(prog);
        _RGLCgRaiseError( CG_MEMORY_ALLOC_ERROR );
        return NULL;
    }
    prog->id = id;

    if ( profile == ( CGprofile )7005 )
        profile = CG_PROFILE_SCE_VP_RSX;
    if ( profile == ( CGprofile )7006 )
        profile = CG_PROFILE_SCE_FP_RSX;

    // load the binary into the program object
    switch ( profile )
    {
        case CG_PROFILE_SCE_VP_TYPEB:
            //case CG_PROFILE_SCE_VP_TYPEC:
        case CG_PROFILE_SCE_VP_RSX:
            // TODO ************** need to include the entry symbol too
	    success =  _RGLGenerateProgram( prog, VERTEX_PROFILE_INDEX, programHeader,
			    ucode, parameterHeader, NULL, stringTable, defaultValues );
            break;
        case CG_PROFILE_SCE_FP_TYPEB:
            //case CG_PROFILE_SCE_FP_TYPEC:
        case CG_PROFILE_SCE_FP_RSX:
            success = _RGLGenerateProgram( prog, FRAGMENT_PROFILE_INDEX, programHeader, ucode, parameterHeader, NULL, stringTable, defaultValues );
            break;
        default:
            // should never reach here
	    break;
    }

    // if the creation failed, free all resources.
    // the error was raised when the error was encoutered.
    if ( success == 0 )
    {
        // free the program object
        free( prog );
        // release the id too
        _RGLEraseName( &_CurrentContext->cgProgramNameSpace, ( jsName )id );
        return NULL;
    }

    // success! add the program to the program list in the context.
    _RGLCgProgramPushFront(prog->parentContext, prog);

    if(_cgProgramCreateHook)
       _cgProgramCreateHook(prog);

    // everything worked.
    return id;
}

static CGprogram _RGLCgUpdateProgramAtIndex( CGprogramGroup group, int index, int refcount );

CG_API CGprogram cgCreateProgram( CGcontext ctx,
                                  CGenum program_type,
                                  const char* program,
                                  CGprofile profile,
                                  const char* entry,
                                  const char** args )
{
    // Load a program from a memory pointer.

    if ( profile == ( CGprofile )7005 )
        profile = CG_PROFILE_SCE_VP_RSX;
    if ( profile == ( CGprofile )7006 )
        profile = CG_PROFILE_SCE_FP_RSX;

    if ( program_type == CG_ROW_MAJOR )
        program_type = CG_BINARY;

    if ( !_RGLCgCreateProgramChecks( ctx, profile, program_type ) )
        return NULL;

    //data to extract from the buffer passed:
    CgProgramHeader *programHeader = NULL;
    const void *ucode = NULL;
    CgParameterTableHeader *parameterHeader = NULL;
    const char *stringTable = NULL;
    const float *defaultValues = NULL;

    //first step, compile any source file
    const char *binaryBuffer = NULL;
    char* compiled_program = NULL;
    if ( program_type == CG_SOURCE )
    {
        if(_cgRTCgcCompileProgramHook)
        {
            _cgRTCgcCompileProgramHook( program, cgGetProfileString(profile), entry, args, &compiled_program );
            if(!compiled_program)
            {
                _RGLCgRaiseError( CG_COMPILER_ERROR );
                return NULL;
            }
            binaryBuffer = compiled_program;
        }
        else
        {
            RARCH_ERR("The CG runtime compiler hasn't been setup. cgRTCgcInit() should be called prior to this function.\n" );
            _RGLCgRaiseError( CG_INVALID_ENUMERANT_ERROR );
            return NULL;
        }
    }
    else
        binaryBuffer = program;

    bool bConvertedToElf = false;

    //At that point we have a binary file which is either any ELF or an NV format file
    const unsigned int ElfTag = 0x7F454C46; // == MAKEFOURCC(0x7F,'E','L','F');

    if (!(*( unsigned int* )binaryBuffer == ElfTag))
    {
        //convert NV file to the runtime format

        if ( program_type == CG_BINARY )
        {
            RARCH_WARN("A binary shader is being loaded using a deprecated binary format.  Please use the cgnv2elf tool to convert to the new, memory-saving, faster-loading format.\n");
        }

        //convert from NV format to the runtime format
        int compiled_program_size = 0;
        std::vector<char> stringTableArray;
        std::vector<float> defaultValuesArray;
        CgBinaryProgram* nvProgram = ( CgBinaryProgram* )binaryBuffer;
        char *runtimeElfShader = NULL;

        //check the endianness
        int totalSize;

        if (( nvProgram->profile != CG_PROFILE_SCE_FP_TYPEB ) && ( nvProgram->profile != CG_PROFILE_SCE_VP_TYPEB ) &&
                ( nvProgram->profile != ( CGprofile )7006 ) && ( nvProgram->profile != ( CGprofile )7005 ) &&
                ( nvProgram->profile != CG_PROFILE_SCE_FP_RSX ) && ( nvProgram->profile != CG_PROFILE_SCE_VP_RSX ) )
        {
            totalSize = endianSwapWord( nvProgram->totalSize );
        }
        else
            totalSize = nvProgram->totalSize;

        int res = convertNvToElfFromMemory( binaryBuffer, totalSize, 2, 0, ( void** )&runtimeElfShader, &compiled_program_size, stringTableArray, defaultValuesArray );
        if ( res != 0 )
        {
            RARCH_ERR("Invalid CG binary program.\n");
            _RGLCgRaiseError( CG_COMPILER_ERROR );
            if ( compiled_program )
                _cgRTCgcFreeCompiledProgramHook( compiled_program );
            return NULL;
        }

        if ( compiled_program )
            _cgRTCgcFreeCompiledProgramHook( compiled_program );

        size_t stringTableSize = stringTableArray.size() * sizeof( stringTable[0] );
        size_t defaultTableSize = defaultValuesArray.size() * sizeof( defaultValues[0] );
        int paddedSize = _RGLPad( compiled_program_size, 4 );

        char *runtimeElf = (char*)memalign( 16, paddedSize + stringTableSize + defaultTableSize );

        if ( !runtimeElf )
        {
            _RGLCgRaiseError( CG_MEMORY_ALLOC_ERROR );
            return NULL;
        }
        bConvertedToElf = true;
        memcpy( runtimeElf, runtimeElfShader, compiled_program_size );

        convertNvToElfFreeBinaryShader( runtimeElfShader );

        float* pDefaultValues = ( float* )(( char* )runtimeElf + paddedSize );
        defaultValues = pDefaultValues;

        if ( defaultTableSize )
            memcpy( pDefaultValues, &defaultValuesArray[0], defaultTableSize );

        char *pStringTable = ( char* )runtimeElf + paddedSize + defaultTableSize;
        stringTable = pStringTable;

        if ( stringTableSize )
            memcpy( pStringTable, &stringTableArray[0], stringTableSize );

        programHeader = ( CgProgramHeader* )runtimeElf;
        size_t elfUcodeSize = programHeader->instructionCount * 16;
        size_t ucodeOffset = _RGLPad( sizeof( CgProgramHeader ), 16 );
        size_t parameterOffset = _RGLPad( ucodeOffset + elfUcodeSize, 16 );
        ucode = ( char* )runtimeElf + ucodeOffset;
        parameterHeader = ( CgParameterTableHeader* )(( char* )runtimeElf + parameterOffset );
    }
    else
    {
        CGELFBinary elfBinary;
        CGELFProgram elfProgram;
        if ((( intptr_t )binaryBuffer ) & 15 )
        {
            RARCH_ERR("CG Binary not aligned on 16 bytes, needed for ucode section.\n");
            _RGLCgRaiseError( CG_PROGRAM_LOAD_ERROR );
            return NULL;
        }
        bool res = cgOpenElf( binaryBuffer, 0, &elfBinary );
        if(!res)
        {
            RARCH_ERR("Not a valid ELF.\n");
            _RGLCgRaiseError( CG_PROGRAM_LOAD_ERROR );
            return NULL;
        }
        if(!cgGetElfProgramByName( &elfBinary, entry, &elfProgram))
        {
            RARCH_ERR("Couldn't find the shader entry in the CG binary.\n");
            return NULL;
        }

        programHeader = ( CgProgramHeader* )elfBinary.shadertab + elfProgram.index;
        ucode = ( char* )elfProgram.texttab;
        parameterHeader = ( CgParameterTableHeader* )elfProgram.paramtab;
        stringTable = elfBinary.strtab;
        defaultValues = ( float* )elfBinary.consttab;
    }

    CGprogram prog = _RGLCgCreateProgram( ctx, profile, programHeader, ucode, parameterHeader, stringTable, defaultValues );

    if(bConvertedToElf)
    {
       _CGprogram* ptr = _cgGetProgPtr( prog );
       ptr->runtimeElf = programHeader;
    }

    return prog;
}

CG_API CGprogram cgCreateProgramFromFile( CGcontext ctx,
        CGenum program_type,
        const char* program_file,
        CGprofile profile,
        const char* entry,
        const char** args )
{
    if ( profile == ( CGprofile )7005 )
        profile = CG_PROFILE_SCE_VP_RSX;
    if ( profile == ( CGprofile )7006 )
        profile = CG_PROFILE_SCE_FP_RSX;

    if ( program_type == CG_ROW_MAJOR )
       program_type = CG_BINARY;

    if ( !_RGLCgCreateProgramChecks( ctx, profile, program_type ) )
       return NULL;

    FILE* fp = NULL;
    if ( RGL_LIKELY( program_type == CG_BINARY ) )
    {
        CGprogram ret = NULL;

        _CGcontext *context = _cgGetContextPtr( ctx );
        CGprogramGroup group = NULL;

        group = context->groupList;
        while ( group )
        {
            const char *groupName = _RGLCgGetProgramGroupName( group );
            if ( groupName && !strcmp( groupName, program_file ) )
            {
                int index;
                if ( entry == NULL )
                    index = 0;
                else
                    index = _RGLCgGetProgramIndex( group, entry );
                if ( index >= 0 )
                {
                    ret = _RGLCgUpdateProgramAtIndex( group, index, 1 );
                    break;
                }
                else
                {
                    return ( CGprogram )NULL;
                }
            }
            group = group->next;
        }

        if ( ret )
            return ret;
        else
        {
            fp = fopen( program_file, "rb");

            if ( fp == NULL )
            {
                _RGLCgRaiseError( CG_FILE_READ_ERROR );
                return (CGprogram)NULL;
            }

            unsigned int filetag = 0;
            int res = fread( &filetag, sizeof( filetag ), 1, fp );
            if (!res)
            {
                fclose(fp);
                _RGLCgRaiseError( CG_FILE_READ_ERROR );
                return ( CGprogram )NULL;
            }
            const unsigned int ElfTag = 0x7F454C46;
            if ( filetag == ElfTag )
            {
                fclose( fp );

                group = _RGLCgCreateProgramGroupFromFile( ctx, program_file );
                if ( group )
                {
                    _CGprogramGroup *_group = ( _CGprogramGroup * )group;
                    _group->userCreated = false;
                    if ( entry == NULL )
                    {
                        if ( group->programCount == 1 )
                            ret = _RGLCgUpdateProgramAtIndex( group, 0, 1 );
                    }
                    else
                    {
                        int index = _RGLCgGetProgramIndex( group, entry );
                        if ( index == -1 )
                        {
                            RARCH_ERR("Couldn't find the shader entry in the CG binary.\n");
                        }
                        else
                            ret = _RGLCgUpdateProgramAtIndex( group, index, 1 );
                    }
                }
                return ret;
            }
        }
    }

    if ( !fp )
    {
        fp = fopen( program_file, "rb" );
        if ( fp == NULL )
        {
            _RGLCgRaiseError( CG_FILE_READ_ERROR );
            return ( CGprogram )NULL;
        }
    }

    size_t file_size = 0;
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);

    char* ptr = (char*)malloc( file_size + 1 );
    if (ptr == NULL)
    {
        _RGLCgRaiseError( CG_MEMORY_ALLOC_ERROR );
        fclose( fp );
        return ( CGprogram )NULL;
    }

    fread( ptr, file_size, 1, fp );
    fclose( fp );

    if ( program_type == CG_SOURCE )
       ptr[file_size] = '\0';

    CGprogram ret = cgCreateProgram( ctx, program_type, ptr, profile, entry, args );

    free( ptr );

    return ret;
}

CG_API CGprogram cgCopyProgram( CGprogram program )
{
    if ( !CG_IS_PROGRAM( program ) )
    {
        _RGLCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
        return NULL;
    }
    _CGprogram* prog = _cgGetProgPtr( program );
    if ( prog == NULL )
    {
        _RGLCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
        return ( CGprogram )NULL;
    }
    
    _CGprogram* newprog;
    size_t paddedProgramSize = 0;
    size_t ucodeSize = 0;

    if (prog->header.profile == CG_PROFILE_SCE_FP_TYPEB || prog->header.profile == CG_PROFILE_SCE_FP_RSX)
    {
       paddedProgramSize = _RGLPad( sizeof( _CGprogram ), 16);
       ucodeSize = prog->header.instructionCount * 16;
       newprog = ( _CGprogram* )malloc(paddedProgramSize + ucodeSize);
    }
    else
    {
       newprog = (_CGprogram*)malloc(sizeof(_CGprogram));
    }

    if(newprog == NULL)
    {
        _RGLCgRaiseError( CG_MEMORY_ALLOC_ERROR );
        return ( CGprogram )NULL;
    }
    memset( newprog, 0, sizeof( _CGprogram ) );

    newprog->header.profile = prog->header.profile;
    newprog->parentContext = prog->parentContext;

    newprog->id = ( CGprogram )_RGLCreateName( &_CurrentContext->cgProgramNameSpace, newprog );

    int success = 0;
    switch ( prog->header.profile )
    {
        case CG_PROFILE_SCE_VP_TYPEB:
        case CG_PROFILE_SCE_VP_RSX:
        case CG_PROFILE_SCE_FP_TYPEB:
        case CG_PROFILE_SCE_FP_RSX:
            success = _RGLPlatformCopyProgram( prog, newprog );
            break;
        default:
            _RGLCgRaiseError( CG_UNKNOWN_PROFILE_ERROR );
            success = 0;
            break;
    }

    if ( success == 0 )
    {
        free( newprog );
        _RGLEraseName( &_CurrentContext->cgProgramNameSpace, ( jsName )newprog->id );
        return ( CGprogram )NULL;
    }

    if (prog->header.profile == CG_PROFILE_SCE_FP_TYPEB || prog->header.profile == CG_PROFILE_SCE_FP_RSX)
    {
       newprog->ucode = (char*)newprog + paddedProgramSize;
       memcpy((char*)newprog->ucode, (char*)prog->ucode, ucodeSize);
    }

    if ( prog->programGroup )
    {
        newprog->programGroup = prog->programGroup;
        newprog->programIndexInGroup = -1;
        _RGLCgUpdateProgramAtIndex( newprog->programGroup, -1, 1 );
    }

    _RGLCgProgramPushFront(newprog->parentContext, newprog);

    if(_cgProgramCopyHook)
       _cgProgramCopyHook(newprog, prog);

    return newprog->id;
}


CG_API void cgDestroyProgram( CGprogram program )
{
    if ( !CG_IS_PROGRAM( program ) )
    {
        _RGLCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
        return;
    }

    _CGprogram* ptr = _cgGetProgPtr( program );

    if ( ptr == NULL )
    {
        _RGLCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
        return;
    }

    if ( ptr->programGroup )
    {
        if ( !ptr->programGroup->userCreated )
        {
            if ( ptr->programIndexInGroup != -1 && ptr->programGroup->programs[ptr->programIndexInGroup].refCount == 0 )
            {
                _RGLCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
                return;
            }
            else
            {
                bool isGroupMember = ( ptr->programIndexInGroup != -1 );
                _RGLCgUpdateProgramAtIndex( ptr->programGroup, ptr->programIndexInGroup, -1 );
                if ( isGroupMember )
                    return;
            }
        }
    }

    _CGcontext* ctx = ptr->parentContext;

    if ( ptr == ctx->programList )
    {
        _CGprogram* p = ctx->programList;
        ctx->programList = p->next;
	_RGLCgProgramErase( p );
	if(p != NULL)
           free( p );
    }
    else
    {
        _CGprogram* p = _RGLCgProgramFindPrev( ctx, ptr );

	_CGprogram* next = p->next;
	if ( next )
	{
		p->next = next->next;
		_RGLCgProgramErase( next );
		if(next != NULL)
			free( next );
	}
    }
    return;
}

static CGprogram _RGLCgUpdateProgramAtIndex( CGprogramGroup group, int index, int refcount )
{
    if ( index < ( int )group->programCount )
    {
        if ( index >= 0 )
        {
            if ( refcount == 1 && group->programs[index].refCount == 1 )
            {
                CGprogram res = cgCopyProgram( group->programs[index].program );
                return res;
            }
            group->programs[index].refCount += refcount;
        }

        group->refCount += refcount;
        if ( refcount < 0 )
        {
            if ( group->refCount == 0 && !group->userCreated )
            {
                _RGLCgDestroyProgramGroup( group );
            }
            return NULL;
        }
        else
            return group->programs[index].program;
    }
    else
        return NULL;
}

static void _RGLCgAddGroup( CGcontext ctx, CGprogramGroup group )
{
    _CGcontext *context = _cgGetContextPtr( ctx );
    if ( !context->groupList )
        context->groupList = group;
    else
    {
        _CGprogramGroup *current = context->groupList;
        while ( current->next )
            current = current->next;
        current->next = group;
    }
}

static void _RGLCgRemoveGroup( CGcontext ctx, CGprogramGroup group )
{
    _CGcontext *context = _cgGetContextPtr( ctx );
    _CGprogramGroup *current = context->groupList;
    _CGprogramGroup *previous = NULL;
    while ( current && current != group )
    {
        previous = current;
        current = current->next;
    }
    if ( current )
    {
        if ( !previous )
            context->groupList = current->next;
        else
            previous->next = current->next;
    }
}

CGprogramGroup _RGLCgCreateProgramGroupFromFile( CGcontext ctx, const char *group_file )
{
    FILE *fp = fopen(group_file, "rb");

    if(fp == NULL)
    {
        _RGLCgRaiseError( CG_FILE_READ_ERROR );
        return ( CGprogramGroup )NULL;
    }

    size_t file_size = 0;
    fseek( fp, 0, SEEK_END );
    file_size = ftell( fp );
    rewind( fp );

    char *ptr = ( char* )malloc(file_size + 1);
    if(ptr == NULL)
    {
        _RGLCgRaiseError( CG_MEMORY_ALLOC_ERROR );
        return ( CGprogramGroup )NULL;
    }

    fread( ptr, file_size, 1, fp );
    fclose( fp );

    CGprogramGroup group = _RGLCgCreateProgramGroup( ctx, group_file, ptr, file_size );
    if ( !group )
        free( ptr );

    return group;
}

CGprogramGroup _RGLCgCreateProgramGroup( CGcontext ctx,  const char *name, void *ptr, int size )
{
    _CGprogramGroup *group = NULL;
    CGELFBinary elfBinary;
    elfBinary.elfFile = NULL;

    while ( 1 )
    {
        bool res = cgOpenElf( ptr, size, &elfBinary );
        if ( !res )
            break;

        size_t elfConstTableSize = ( size_t )elfBinary.consttabSize;
        size_t elfStringTableSize = ( size_t )elfBinary.strtabSize;

        int programCount = elfBinary.shadertabSize / sizeof( CgProgramHeader );
        int i;

        size_t nvProgramNamesOffset = _RGLPad( sizeof( _CGprogramGroup ), sizeof( _CGnamedProgram ) );
        size_t nvDefaultValuesTableOffset = _RGLPad( nvProgramNamesOffset + programCount * sizeof( _CGnamedProgram ), 16 );

        size_t nvStringTableOffset = nvDefaultValuesTableOffset + elfConstTableSize;
        size_t structureSize = nvStringTableOffset + elfStringTableSize;

        group = ( CGprogramGroup )malloc( structureSize );
        if ( !group )
            break;

        group->ctx = ctx;
        group->next = NULL;
        group->programCount = ( unsigned int )programCount;
        group->constantTable = ( unsigned int * )(( char* )group + nvDefaultValuesTableOffset );
        group->stringTable = ( unsigned int * )(( char* )group + nvStringTableOffset );
        group->programs = ( _CGnamedProgram * )(( char* )group + nvProgramNamesOffset );
        group->userCreated = true;
        group->refCount = 0;
        group->filedata = ( char* )ptr;
        if ( name )
        {
            int len = strlen( name );
            group->name = ( char* )malloc( len + 1 );
            if ( !group->name )
                break;
            strlcpy( group->name, name, sizeof(group->name));
        }
        else
            group->name = NULL;

        if ( elfConstTableSize )
            memcpy(( char* )group + nvDefaultValuesTableOffset, elfBinary.consttab, elfConstTableSize );
        if ( elfStringTableSize )
            memcpy(( char* )group + nvStringTableOffset, elfBinary.strtab, elfStringTableSize );

        _RGLCgAddGroup( ctx, group );

        for ( i = 0;i < ( int )group->programCount;i++ )
        {
            CgProgramHeader *cgShader = ( CgProgramHeader* )elfBinary.shadertab + i;

            if ( cgShader->profile == ( CGprofile )7005 )
                cgShader->profile = CG_PROFILE_SCE_VP_RSX;
            if ( cgShader->profile == ( CGprofile )7006 )
                cgShader->profile = CG_PROFILE_SCE_FP_RSX;

            CGELFProgram elfProgram;
            bool res = cgGetElfProgramByIndex( &elfBinary, i, &elfProgram );
            if ( !res )
                return false;

            CgProgramHeader *programHeader = cgShader;
            char *ucode = ( char * )elfProgram.texttab;
            CgParameterTableHeader *parameterHeader  = ( CgParameterTableHeader * )elfProgram.paramtab;

            const char *programName = getSymbolByIndexInPlace( elfBinary.symtab, elfBinary.symbolSize, elfBinary.symbolCount, elfBinary.symbolstrtab, i + 1 );
            group->programs[i].name = programName;
            group->programs[i].program = _RGLCgCreateProgram( ctx, ( CGprofile )cgShader->profile, programHeader, ucode, parameterHeader, ( const char* )group->stringTable, ( const float* )group->constantTable );
            _CGprogram *cgProgram = _cgGetProgPtr( group->programs[i].program );
            cgProgram->programGroup = group;
            cgProgram->programIndexInGroup = i;
            group->programs[i].refCount = 0;
        }
        break;
    }

    return group;
}

void _RGLCgDestroyProgramGroup( CGprogramGroup group )
{
    _CGprogramGroup *_group = ( _CGprogramGroup * )group;
    for ( int i = 0;i < ( int )_group->programCount;i++ )
    {
        _CGprogram *cgProgram = _cgGetProgPtr( group->programs[i].program );
        cgProgram->programGroup = NULL;
        cgDestroyProgram( _group->programs[i].program );
    }
    if(_group->filedata != NULL)
	    free( _group->filedata );
    if ( _group->name )
        free( _group->name );

    _RGLCgRemoveGroup( group->ctx, group );
    if(_group != NULL)
	    free( _group );
}

const char *_RGLCgGetProgramGroupName( CGprogramGroup group )
{
    _CGprogramGroup *_group = ( _CGprogramGroup * )group;
    return _group->name;
}

int _RGLCgGetProgramIndex( CGprogramGroup group, const char *name )
{
    int i;
    for ( i = 0;i < ( int )group->programCount;i++ )
    {
        if ( !strcmp( name, group->programs[i].name ) )
            return i;
    }
    return -1;
}

CGprogram _RGLCgGetProgramAtIndex( CGprogramGroup group, unsigned int index )
{
    return _RGLCgUpdateProgramAtIndex( group, index, 0 );
}

int _RGLCgGetProgramCount( CGprogramGroup group )
{
    return group->programCount;
}

static const RGLcgProfileMapType RGLcgProfileMap[] =
{
   {( CGprofile )6144, "CG_PROFILE_START", 1 },
   {( CGprofile )6145, "unknown", 1 },
#define CG_PROFILE_MACRO(name, compiler_id, compiler_id_caps, compiler_opt,int_id,vertex_profile) \
   {CG_PROFILE_ ## compiler_id_caps, compiler_opt, vertex_profile},
#include <Cg/cg_profiles.h>
   {( CGprofile )0, "", 0 }
};

CG_API const char* cgGetProfileString( CGprofile profile )
{
    const size_t arraysize = sizeof( RGLcgProfileMap ) / sizeof( RGLcgProfileMapType );
    unsigned int i = 0;
    while ( i < arraysize )
    {
        if ( profile == RGLcgProfileMap[i].id )
        {
            return RGLcgProfileMap[i].string;
        }
        ++i;
    }
    return "";
}

CG_API CGerror cgGetError(void)
{
   CGerror err = _CurrentContext->RGLcgLastError;
   _CurrentContext->RGLcgLastError = CG_NO_ERROR;
   return err;
}


CG_API const char* cgGetErrorString( CGerror error )
{
   static char strbuf[256];
   snprintf(strbuf, sizeof(strbuf), "%d", error);
   return strbuf;
}

void _RGLCgDestroyContextParam( CgRuntimeParameter* ptr )
{
   if ( _cgParameterDestroyHook ) _cgParameterDestroyHook( ptr );
      _RGLEraseName( &_CurrentContext->cgParameterNameSpace, ( jsName )( ptr->id ) );
   free( ptr );
}

static int _RGLGetSizeofSubArray( const short *dimensions, int count )
{
    int res = 1;
    for ( int i = 0;i < count;i++ )
        res *= ( int )( *( dimensions++ ) );
    return res;
}

static _CGparameter *_cgGetNamedParameter( _CGprogram* progPtr, const char* name, CGenum name_space, int *arrayIndex, const CgParameterEntry *_startEntry = NULL, int _entryCount = -1 )
{
    if ( name == NULL )
        return NULL;

    *arrayIndex = -1;
    int done = 0;
    const char *structureEnd;
    const char *structureStart = name;
    int itemIndex = -1;
    _CGprogram *program = progPtr;

    const CgParameterEntry *currentEntry;
    const CgParameterEntry *lastEntry;

    int containerCount = -2;
    if ( _startEntry && _entryCount != -1 )
    {
        currentEntry = _startEntry;
        containerCount = _entryCount;
    }
    else
    {
        currentEntry = program->parametersEntries;
    }
    lastEntry = program->parametersEntries + program->rtParametersCount;

    bool bWasUnrolled = false;
    const char *prevStructureStart = structureStart;

    while (( !done ) && ( *structureStart ) && ( containerCount != -1 ) )
    {
        structureEnd = strpbrk( structureStart, ".[" );
        if ( structureEnd == NULL )
        {
            structureEnd = structureStart + strlen( structureStart );
            done = 1;
        }

        if ( bWasUnrolled )
        {
            bWasUnrolled = false;
            structureStart = prevStructureStart;
        }
        char structName[256];
        int length = ( int )( structureEnd - structureStart );
        strncpy( structName, structureStart, length );
        structName[length] = '\0';
        prevStructureStart = structureStart;
        structureStart = structureEnd + 1;

        bool found = false;
        while ( !found && currentEntry < lastEntry && ( containerCount == -2 || containerCount > 0 ) )
        {
            if ( !strncmp( structName, program->stringTable + currentEntry->nameOffset, length )
                    && ( name_space == 0 || ( name_space == CG_GLOBAL && ( currentEntry->flags & CGPF_GLOBAL ) )
                         || ( name_space == CG_PROGRAM && !( currentEntry->flags & CGPF_GLOBAL ) ) ) )
            {
                if (( int )strlen( program->stringTable + currentEntry->nameOffset ) != length )
                {
                    if ( !strcmp( name, program->stringTable + currentEntry->nameOffset ) )
                    {
                        found = true;
                        done = 1;
                    }

                    if ( !strncmp( name, program->stringTable + currentEntry->nameOffset, length ) &&
                            !strcmp( "[0]", program->stringTable + currentEntry->nameOffset + length ) )
                    {
                        found = true;
                        done = 1;
                    }
                }
                else
                    found = true;
            }

            if ( !found )
            {
                int skipCount = 1;
                while ( skipCount && currentEntry < lastEntry )
                {
                    if ( currentEntry->flags & CGP_STRUCTURE )
                    {
                        const CgParameterStructure *parameterStructure = _RGLGetParameterStructure( program, currentEntry );
                        skipCount += parameterStructure->memberCount;
                    }
                    else if ( currentEntry->flags & CGP_ARRAY )
                    {
                        if ( currentEntry->flags & CGP_UNROLLED )
                        {
                            const CgParameterArray *parameterArray =  _RGLGetParameterArray( program, currentEntry );
                            skipCount += _RGLGetSizeofSubArray(( short* )parameterArray->dimensions, parameterArray->dimensionCount );
                        }
                        else
                            skipCount++;
                    }
                    currentEntry++;
                    skipCount--;
                }
            }
            if ( containerCount != -2 )
                containerCount--;
        }

        if ( found )
        {
            switch ( currentEntry->flags & CGP_TYPE_MASK )
            {
                case 0:
                    itemIndex = ( int )( currentEntry - program->parametersEntries );
                    break;
                case CGP_ARRAY:
                {

                    const CgParameterEntry *arrayEntry = currentEntry;
                    const CgParameterArray *parameterArray =  _RGLGetParameterArray( program, arrayEntry );

                    if ( *structureEnd == '\0' )
                    {
                        itemIndex = ( int )( currentEntry - program->parametersEntries );
                        break;
                    }

                    currentEntry++;
                    if ( currentEntry->flags &CGP_STRUCTURE )
                    {
                        bWasUnrolled = true;
                        containerCount = _RGLGetSizeofSubArray(( short* )parameterArray->dimensions, parameterArray->dimensionCount );
                        break;
                    }
                    else
                    {
                        const char *arrayStart = structureEnd;
                        const char *arrayEnd = structureEnd;

                        int dimensionCount = 0;
                        int arrayCellIndex = 0;
                        while ( *arrayStart == '[' && dimensionCount < parameterArray->dimensionCount )
                        {
                            arrayEnd = strchr( arrayStart + 1, ']' );
                            int length = ( int )( arrayEnd - arrayStart - 1 );
                            char indexString[16];
                            strncpy( indexString, arrayStart + 1, length );
                            indexString[length] = '\0';
                            int index = atoi( indexString );
                            int rowSize = parameterArray->dimensions[dimensionCount];
                            if ( index >= rowSize )
                            {
				return NULL;
                            }
                            arrayCellIndex += index * _RGLGetSizeofSubArray(( short* )parameterArray->dimensions + dimensionCount, parameterArray->dimensionCount - dimensionCount - 1 );

                            arrayStart = arrayEnd + 1;
                            dimensionCount++;
                        }
                        structureEnd = arrayStart;
                        if ( *structureEnd == '\0' )
                            done = 1;

                        if ( done )
                        {
                            ( *arrayIndex ) = arrayCellIndex;
                            itemIndex = ( int )( currentEntry - program->parametersEntries );
                        }
                    }
                }
                break;
                case CGP_STRUCTURE:
                    if ( done )
                        itemIndex = ( int )( currentEntry - program->parametersEntries );
                    else
                    {
                        const CgParameterStructure *parameterStructure = _RGLGetParameterStructure( program, currentEntry );
                        containerCount = parameterStructure->memberCount;
                    }
                    break;
                default:
                    break;
            }
        }
        if ( found )
        {
            if ( !bWasUnrolled )
                currentEntry++;
        }
        else
            break;
    }

    if ( itemIndex != -1 )
        return ( _CGparameter* )( program->runtimeParameters + itemIndex );
    else
        return NULL;
}

CG_API CGparameter cgGetNamedParameter( CGprogram prog, const char* name )
{
    if ( !CG_IS_PROGRAM( prog ) )
    {
        _RGLCgRaiseError( CG_INVALID_PROGRAM_HANDLE_ERROR );
        return ( CGparameter )NULL;
    }

    _CGprogram* progPtr = _cgGetProgPtr( prog );
    int arrayIndex = -1;
    CgRuntimeParameter *param = ( CgRuntimeParameter * )_cgGetNamedParameter( progPtr, name, ( CGenum )0, &arrayIndex );
    if ( param )
    {
        int ret = ( int )param->id;
        if ( arrayIndex != -1 )
            ret |= ( arrayIndex << CG_PARAMETERSIZE );
        return ( CGparameter )ret;
    }
    else
        return ( CGparameter )NULL;
}

static CGbool _RGLPlatformSupportsVertexProgram( CGprofile p )
{
    if ( p == CG_PROFILE_SCE_VP_TYPEB )
        return CG_TRUE;
    if ( p == CG_PROFILE_SCE_VP_TYPEC )
        return CG_TRUE;
    if ( p == CG_PROFILE_SCE_VP_RSX )
        return CG_TRUE;
    return CG_FALSE;
}

CGGL_API CGbool cgGLIsProfileSupported( CGprofile profile )
{
    if ( profile == ( CGprofile )7005 )
        profile = CG_PROFILE_SCE_VP_RSX;
    if ( profile == ( CGprofile )7006 )
        profile = CG_PROFILE_SCE_FP_RSX;

    switch ( profile )
    {
        case CG_PROFILE_SCE_VP_TYPEB:
        case CG_PROFILE_SCE_VP_RSX:
            return ( CGbool ) _RGLPlatformSupportsVertexProgram( profile );
        case CG_PROFILE_SCE_FP_TYPEB:
        case CG_PROFILE_SCE_FP_RSX:
            return ( CGbool ) _RGLPlatformSupportsFragmentProgram( profile );
        default:
            return CG_FALSE;
    }
}

CGGL_API void cgGLEnableProfile( CGprofile profile )
{
   if ( profile == ( CGprofile )7005 )
      profile = CG_PROFILE_SCE_VP_RSX;
   if ( profile == ( CGprofile )7006 )
      profile = CG_PROFILE_SCE_FP_RSX;

   PSGLcontext* LContext = _CurrentContext;
   struct _CGprogram* current = LContext->BoundFragmentProgram;
   switch ( profile )
   {
      case CG_PROFILE_SCE_VP_TYPEB:
      case CG_PROFILE_SCE_VP_RSX:
         LContext->VertexProgram = GL_TRUE;
	 LContext->needValidate |= PSGL_VALIDATE_VERTEX_PROGRAM | PSGL_VALIDATE_VERTEX_TEXTURES_USED;
	 break;
      case CG_PROFILE_SCE_FP_TYPEB:
      case CG_PROFILE_SCE_FP_RSX:
         LContext->FragmentProgram = GL_TRUE;
	 if ( current )
	 {
            for (GLuint i = 0; i < current->samplerCount; ++i)
	    {
               int unit = current->samplerUnits[i];
	       _CurrentContext->TextureImageUnits[unit].currentTexture = _RGLGetCurrentTexture( &_CurrentContext->TextureImageUnits[unit], GL_TEXTURE_2D );
	    }
	 }
	 LContext->needValidate |= PSGL_VALIDATE_FRAGMENT_PROGRAM | PSGL_VALIDATE_TEXTURES_USED;
	 break;
      default:
         _RGLCgRaiseError( CG_INVALID_PROFILE_ERROR );
	 break;
   }
}

CGGL_API void cgGLDisableProfile( CGprofile profile )
{
    if ( profile == ( CGprofile )7005 )
        profile = CG_PROFILE_SCE_VP_RSX;
    if ( profile == ( CGprofile )7006 )
        profile = CG_PROFILE_SCE_FP_RSX;

    PSGLcontext* LContext = _CurrentContext;
    switch ( profile )
    {
        case CG_PROFILE_SCE_VP_TYPEB:
        case CG_PROFILE_SCE_VP_RSX:
            LContext->VertexProgram = GL_FALSE;
            LContext->needValidate |= PSGL_VALIDATE_VERTEX_PROGRAM ;
            break;
        case CG_PROFILE_SCE_FP_TYPEB:
        case CG_PROFILE_SCE_FP_RSX:
            LContext->FragmentProgram = GL_FALSE;
	    for (GLuint unit = 0; unit < MAX_TEXTURE_UNITS; ++unit)
		    LContext->TextureImageUnits[unit].currentTexture = _RGLGetCurrentTexture( &LContext->TextureImageUnits[unit], GL_TEXTURE_2D );

	    LContext->needValidate |= PSGL_VALIDATE_FFX_FRAGMENT_PROGRAM | PSGL_VALIDATE_TEXTURES_USED;
            break;
        default:
            _RGLCgRaiseError( CG_INVALID_PROFILE_ERROR );
            break;
    }
}


CGGL_API CGprofile cgGLGetLatestProfile( CGGLenum profile_type )
{
    switch ( profile_type )
    {
        case CG_GL_VERTEX:
        case CG_GL_FRAGMENT:
            return _RGLPlatformGetLatestProfile( profile_type );
        default:
            _RGLCgRaiseError( CG_INVALID_ENUMERANT_ERROR );
            return CG_PROFILE_UNKNOWN;
    }
}

CGGL_API void cgGLSetOptimalOptions( CGprofile profile )
{
}

CGGL_API void cgGLLoadProgram( CGprogram program )
{
}

CGGL_API void cgGLBindProgram( CGprogram program )
{
    _CGprogram* ptr = _cgGetProgPtr( program );

    switch ( ptr->header.profile )
    {
	    case CG_PROFILE_SCE_VP_TYPEB:
	    case 7005:
	    case CG_PROFILE_SCE_VP_RSX:
		    _CurrentContext->BoundVertexProgram = ptr;
		    _CurrentContext->needValidate |= PSGL_VALIDATE_VERTEX_PROGRAM | PSGL_VALIDATE_VERTEX_TEXTURES_USED;
		    break;

	    case CG_PROFILE_SCE_FP_TYPEB:
	    case 7006:
	    case CG_PROFILE_SCE_FP_RSX:
		    _CurrentContext->BoundFragmentProgram = ptr;
		    _CurrentContext->needValidate |= PSGL_VALIDATE_FRAGMENT_PROGRAM | PSGL_VALIDATE_TEXTURES_USED;

		    for ( GLuint index = 0; index < ptr->samplerCount; ++index )
		    {
			    CgRuntimeParameter *rtParameter = ptr->runtimeParameters + ptr->samplerIndices[index];
			    CgParameterResource *parameter = ( CgParameterResource * )( ptr->parameterResources + rtParameter->parameterEntry->typeIndex );
			    unsigned int unit = parameter->resource - CG_TEXUNIT0;

			    _CurrentContext->TextureImageUnits[unit].fragmentTarget = rtParameter->glType;
			    _CurrentContext->TextureImageUnits[unit].currentTexture = _RGLGetCurrentTexture( &_CurrentContext->TextureImageUnits[unit], GL_TEXTURE_2D );
		    }
		    break;

	    default:
		    _RGLCgRaiseError( CG_INVALID_PROFILE_ERROR );
		    return;
    }

}

CGGL_API void cgGLUnbindProgram( CGprofile profile )
{
    switch ( profile )
    {
        case CG_PROFILE_SCE_VP_TYPEB:
        case CG_PROFILE_SCE_VP_RSX:
        case 7005:
		_CurrentContext->BoundVertexProgram = NULL;
		_CurrentContext->needValidate |= PSGL_VALIDATE_VERTEX_PROGRAM;
            break;
        case CG_PROFILE_SCE_FP_TYPEB:
        case CG_PROFILE_SCE_FP_RSX:
        case 7006:
	    _CurrentContext->BoundFragmentProgram = NULL;
	    for (GLuint unit = 0; unit < MAX_TEXTURE_UNITS; ++unit)
		    _CurrentContext->TextureImageUnits[unit].currentTexture = _RGLGetCurrentTexture( &_CurrentContext->TextureImageUnits[unit], GL_TEXTURE_2D );

	    _CurrentContext->needValidate |= PSGL_VALIDATE_FFX_FRAGMENT_PROGRAM | PSGL_VALIDATE_TEXTURES_USED;
            break;
        default:
            _RGLCgRaiseError( CG_INVALID_PROFILE_ERROR );
            return;
    }

}

CGGL_API void cgGLSetParameter1f( CGparameter param, float x )
{
   CgRuntimeParameter *ptr = _cgGetParamPtr( param );

   float v[4] = {x, x, x, x};
   ptr->setterIndex( ptr, v, CG_GETINDEX( param ) );
}

CGGL_API void cgGLSetParameter2f( CGparameter param, float x, float y )
{
   CgRuntimeParameter *ptr = _cgGetParamPtr( param );

   float v[4] = {x, y, y, y};
   ptr->setterIndex( ptr, v, CG_GETINDEX( param ) );
}

CGGL_API void cgGLSetParameterPointer
( CGparameter param,
  GLint fsize,
  GLenum type,
  GLsizei stride,
  const GLvoid *pointer )
{

   CgRuntimeParameter *_ptr = _cgGetParamPtr( param );

   const CgParameterResource *parameterResource = _RGLGetParameterResource( _ptr->program, _ptr->parameterEntry );
   GLuint index = ( GLuint )( parameterResource->resource - CG_ATTR0 );

   _RGLVertexAttribPointerNV(
		   index,
		   fsize,
		   type,
		   ( _ptr->parameterEntry->flags & CGP_NORMALIZE ) ? 1 : 0,
		   stride,
		   pointer );
}

CGGL_API void cgGLEnableClientState( CGparameter param )
{
   CgRuntimeParameter *_ptr = _cgGetParamPtr( param );

   const CgParameterResource *parameterResource = _RGLGetParameterResource( _ptr->program, _ptr->parameterEntry );

   GLuint index = ( GLuint )( parameterResource->resource - CG_ATTR0 );
   _RGLEnableVertexAttribArrayNV( index );
}

CGGL_API void cgGLDisableClientState( CGparameter param )
{
    CgRuntimeParameter *_ptr = _cgGetParamPtr( param );

    const CgParameterResource *parameterResource = _RGLGetParameterResource( _ptr->program, _ptr->parameterEntry );

    GLuint index = ( GLuint )( parameterResource->resource - CG_ATTR0 );
    _RGLDisableVertexAttribArrayNV( index );
}

CGGL_API void cgGLSetTextureParameter( CGparameter param, GLuint texobj )
{
    CgRuntimeParameter* ptr = _cgGetParamPtr(param);

    ptr->samplerSetter( ptr, &texobj, 0 );
}

CGGL_API void cgGLEnableTextureParameter( CGparameter param )
{
    CgRuntimeParameter* ptr = _cgGetParamPtr(param);
    ptr->samplerSetter( ptr, NULL, 0 );
}

static void _RGLCgContextPushFront(_CGcontext* ctx)
{
   if(_CurrentContext->RGLcgContextHead)
   {
      _CGcontext* head = _cgGetContextPtr( _CurrentContext->RGLcgContextHead );
      ctx->next = head;
   }
   _CurrentContext->RGLcgContextHead = ctx->id;
}

static void destroy_context(_CGcontext*ctx)
{
   if (_cgContextDestroyHook)
      _cgContextDestroyHook(ctx);

   _RGLEraseName( &_CurrentContext->cgContextNameSpace, ( jsName )ctx->id );
   memset(ctx, 0, sizeof( *ctx ) );
   ctx->compileType = CG_UNKNOWN;

   free( ctx );
}

CG_API CGcontext cgCreateContext(void)
{
    _CGcontext* ptr = NULL;

    ptr = ( _CGcontext* )malloc( sizeof( _CGcontext ) );
    if ( ptr == NULL )
    {
        _RGLCgRaiseError( CG_MEMORY_ALLOC_ERROR );
        return ( CGcontext )NULL;
    }

   memset( ptr, 0, sizeof( *ptr ) );
   ptr->compileType = CG_UNKNOWN;

    CGcontext result = ( CGcontext )_RGLCreateName( &_CurrentContext->cgContextNameSpace, ptr );
    if ( !result )
    {
       free( ptr );
       return NULL;
    }

    ptr->id = result;
    ptr->defaultProgram.parentContext = ptr;

    _RGLCgContextPushFront( ptr );

    if ( _cgContextCreateHook ) _cgContextCreateHook( ptr );

    return result;
}

CG_API void cgDestroyContext(CGcontext c)
{
    if(!CG_IS_CONTEXT(c))
    {
       _RGLCgRaiseError( CG_INVALID_CONTEXT_HANDLE_ERROR );
       return;
    }

    _CGcontext* ctx = _cgGetContextPtr( c );

    _RGLCgProgramErase( &ctx->defaultProgram );

    while ( ctx->programList )
    {
        _CGprogram * p = ctx->programList;
        ctx->programList = p->next;
	_RGLCgProgramErase( p );
	if(p != NULL)
		free( p );
    }

    _CGcontext * const head = _cgGetContextPtr( _CurrentContext->RGLcgContextHead );
    if ( head != ctx )
    {
        _CGcontext* ptr = head;
        while ( ptr->next != ctx ) ptr = ptr->next;
        ptr->next = ctx->next;
        destroy_context( ctx );
    }
    else
    {

        _CGcontext* second = head->next;
        destroy_context( head );

        if ( second )
            _CurrentContext->RGLcgContextHead = second->id;
        else
            _CurrentContext->RGLcgContextHead = 0;
    }
}

CG_API const char* cgGetLastListing( CGcontext c )
{
    if ( !CG_IS_CONTEXT( c ) )
    {
        _RGLCgRaiseError( CG_INVALID_CONTEXT_HANDLE_ERROR );
        return NULL;
    }

    return NULL;
}

void _RGLCgRaiseError( CGerror error )
{
    _CurrentContext->RGLcgLastError = error;

    if(!context_shutdown)
    {
       RARCH_ERR("Cg error: %d.\n", error);

       if ( _CurrentContext->RGLcgErrorCallbackFunction )
          _CurrentContext->RGLcgErrorCallbackFunction();
    }
}

unsigned int _RGLCountFloatsInCgType( CGtype type )
{
    int size = 0;
    switch ( type )
    {
        case CG_FLOAT:
        case CG_FLOAT1:
        case CG_FLOAT1x1:
            size = 1;
            break;
        case CG_FLOAT2:
        case CG_FLOAT2x1:
        case CG_FLOAT1x2:
            size = 2;
            break;
        case CG_FLOAT3:
        case CG_FLOAT3x1:
        case CG_FLOAT1x3:
            size = 3;
            break;
        case CG_FLOAT4:
        case CG_FLOAT4x1:
        case CG_FLOAT1x4:
        case CG_FLOAT2x2:
            size = 4;
            break;
        case CG_FLOAT2x3:
        case CG_FLOAT3x2:
            size = 6;
            break;
        case CG_FLOAT2x4:
        case CG_FLOAT4x2:
            size = 8;
            break;
        case CG_FLOAT3x3:
            size = 9;
            break;
        case CG_FLOAT3x4:
        case CG_FLOAT4x3:
            size = 12;
            break;
        case CG_FLOAT4x4:
            size = 16;
            break;
        case CG_SAMPLER1D:
        case CG_SAMPLER2D:
        case CG_SAMPLER3D:
        case CG_SAMPLERRECT:
        case CG_SAMPLERCUBE:
        case CG_BOOL:
        case CG_HALF:
        case CG_HALF1:
        case CG_HALF1x1:
            size = 1;
            break;
        case CG_HALF2:
        case CG_HALF2x1:
        case CG_HALF1x2:
            size = 2;
            break;
        case CG_HALF3:
        case CG_HALF3x1:
        case CG_HALF1x3:
            size = 3;
            break;
        case CG_HALF4:
        case CG_HALF4x1:
        case CG_HALF1x4:
        case CG_HALF2x2:
            size = 4;
            break;
        case CG_HALF2x3:
        case CG_HALF3x2:
            size = 6;
            break;
        case CG_HALF2x4:
        case CG_HALF4x2:
            size = 8;
            break;
        case CG_HALF3x3:
            size = 9;
            break;
        case CG_HALF3x4:
        case CG_HALF4x3:
            size = 12;
            break;
        case CG_HALF4x4:
            size = 16;
            break;
        case CG_INT:
        case CG_INT1:
        case CG_INT1x1:
            size = 1;
            break;
        case CG_INT2:
        case CG_INT2x1:
        case CG_INT1x2:
            size = 2;
            break;
        case CG_INT3:
        case CG_INT3x1:
        case CG_INT1x3:
            size = 3;
            break;
        case CG_INT4:
        case CG_INT4x1:
        case CG_INT1x4:
        case CG_INT2x2:
            size = 4;
            break;
        case CG_INT2x3:
        case CG_INT3x2:
            size = 6;
            break;
        case CG_INT2x4:
        case CG_INT4x2:
            size = 8;
            break;
        case CG_INT3x3:
            size = 9;
            break;
        case CG_INT3x4:
        case CG_INT4x3:
            size = 12;
            break;
        case CG_INT4x4:
            size = 16;
            break;
        case CG_BOOL1:
        case CG_BOOL1x1:
            size = 1;
            break;
        case CG_BOOL2:
        case CG_BOOL2x1:
        case CG_BOOL1x2:
            size = 2;
            break;
        case CG_BOOL3:
        case CG_BOOL3x1:
        case CG_BOOL1x3:
            size = 3;
            break;
        case CG_BOOL4:
        case CG_BOOL4x1:
        case CG_BOOL1x4:
        case CG_BOOL2x2:
            size = 4;
            break;
        case CG_BOOL2x3:
        case CG_BOOL3x2:
            size = 6;
            break;
        case CG_BOOL2x4:
        case CG_BOOL4x2:
            size = 8;
            break;
        case CG_BOOL3x3:
            size = 9;
            break;
        case CG_BOOL3x4:
        case CG_BOOL4x3:
            size = 12;
            break;
        case CG_BOOL4x4:
            size = 16;
            break;
        case CG_FIXED:
        case CG_FIXED1:
        case CG_FIXED1x1:
            size = 1;
            break;
        case CG_FIXED2:
        case CG_FIXED2x1:
        case CG_FIXED1x2:
            size = 2;
            break;
        case CG_FIXED3:
        case CG_FIXED3x1:
        case CG_FIXED1x3:
            size = 3;
            break;
        case CG_FIXED4:
        case CG_FIXED4x1:
        case CG_FIXED1x4:
        case CG_FIXED2x2:
            size = 4;
            break;
        case CG_FIXED2x3:
        case CG_FIXED3x2:
            size = 6;
            break;
        case CG_FIXED2x4:
        case CG_FIXED4x2:
            size = 8;
            break;
        case CG_FIXED3x3:
            size = 9;
            break;
        case CG_FIXED3x4:
        case CG_FIXED4x3:
            size = 12;
            break;
        case CG_FIXED4x4:
            size = 16;
            break;
        default:
            size = 0;
            break;
    }
    return size;
}

void _cgRaiseInvalidParam( CgRuntimeParameter*p, const void*v )
{
   _RGLCgRaiseError( CG_INVALID_PARAMETER_ERROR );
}
void _cgRaiseInvalidParamIndex( CgRuntimeParameter*p, const void*v, const int index )
{
   _RGLCgRaiseError( CG_INVALID_PARAMETER_ERROR );
}

void _cgRaiseNotMatrixParam( CgRuntimeParameter*p, const void*v )
{
   _RGLCgRaiseError( CG_NOT_MATRIX_PARAM_ERROR );
}

void _cgRaiseNotMatrixParamIndex( CgRuntimeParameter*p, const void*v, const int index )
{
   _RGLCgRaiseError( CG_NOT_MATRIX_PARAM_ERROR );
}

void _cgIgnoreSetParam( CgRuntimeParameter*p, const void*v ) {}
void _cgIgnoreSetParamIndex( CgRuntimeParameter*p, const void*v, const int index ) {}

#define CG_DATATYPE_MACRO(name, compiler_name, enum_name, base_enum, nrows, ncols,classname) \
	nrows ,
static int _typesRowCount[] =
    {
#include "Cg/cg_datatypes.h"
    };

#define CG_DATATYPE_MACRO(name, compiler_name, enum_name, base_enum, nrows, ncols,classname) \
	ncols ,
static int _typesColCount[] =
    {
#include "Cg/cg_datatypes.h"
    };

unsigned int _RGLGetTypeRowCount( CGtype parameterType )
{
    int typeIndex = parameterType - 1 - CG_TYPE_START_ENUM;
    return _typesRowCount[typeIndex];
}

unsigned int _RGLGetTypeColCount( CGtype parameterType )
{
    int typeIndex = parameterType - 1 - CG_TYPE_START_ENUM;
    return _typesColCount[typeIndex];
}

CGGL_API void cgGLSetMatrixParameterfc( CGparameter param, const float *matrix )
{
    CgRuntimeParameter* ptr = _cgGetParamPtr( param );
    ptr->settercIndex( ptr, matrix, CG_GETINDEX( param ) );
}
