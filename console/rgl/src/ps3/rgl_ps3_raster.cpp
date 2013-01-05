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

#include "../rgl.h"

#include "include/GmmAlloc.h"
#include "include/rgl-constants.h"
#include "include/rgl-typedefs.h"
#include "include/rgl-externs.h"
#include "include/rgl-inline.h"

#include <Cg/cg.h>
#include <Cg/CgCommon.h>
#include <Cg/cgBinary.h>

#include <RGL/platform.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

using namespace cell::Gcm;

/*============================================================
  MEMORY MANAGER
  ============================================================ */

#if RGL_ENDIAN == RGL_BIG_ENDIAN
#define ENDIAN_32(X, F) ((F) ? endianSwapWord(X) : (X))
#else
#define ENDIAN_32(X, F) (X)
#endif

int _parameterAlloc = 0;
int _ucodeAlloc = 0;

int rglGetTypeResource( _CGprogram* program, unsigned short typeIndex, short *resourceIndex );
int rglGetTypeResourceID( _CGprogram* program, unsigned short typeIndex );
int rglGetTypeResourceRegisterCountVP( _CGprogram* program, short resourceIndex, int resourceCount, unsigned short *resource );

static void setAttribConstantIndex (void *data, const void* __restrict v, const int ) // index
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   _CGprogram *program = ptr->program;
   const CgParameterResource *parameterResource = rglGetParameterResource( program, ptr->parameterEntry );
   GLuint index = parameterResource->resource - CG_ATTR0;
   float * f = ( float* ) v;
   rglVertexAttrib4fNV( index, f[0], f[1], f[2], f[3] );
}

void rglPlatformSetVertexRegister4fv (unsigned int reg, const float * __restrict v)
{
   // save to shared memory for context restore after flip
   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;
   __builtin_memcpy(driver->sharedVPConstants + reg*4*sizeof( float ), v, 4*sizeof( float ) );

   GCM_FUNC( cellGcmSetVertexProgramParameterBlock, reg, 1, v ); 
}

// endian swapping of the fragment uniforms, if necessary
#if RGL_ENDIAN == RGL_BIG_ENDIAN
#define SWAP_IF_BIG_ENDIAN(arg) endianSwapWordByHalf(arg)
#elif RGL_ENDIAN == RGL_LITTLE_ENDIAN
#define SWAP_IF_BIG_ENDIAN(arg) arg
#else
#error include missing for endianness
#endif

//here ec has been advanced and is already on top of the embedded constant count
template<int SIZE> inline static void swapandsetfp( int ucodeSize, unsigned int loadProgramId, unsigned int loadProgramOffset, unsigned short *ec, const unsigned int   * __restrict v )
{
   GCM_FUNC( cellGcmSetTransferLocation, CELL_GCM_LOCATION_LOCAL );
   unsigned short count = *( ec++ );
   for ( unsigned long offsetIndex = 0; offsetIndex < count; ++offsetIndex )
   {
      void *pointer=NULL;
      const int paddedSIZE = (SIZE + 1) & ~1; // even width only	
      GCM_FUNC( cellGcmSetInlineTransferPointer, gmmIdToOffset( loadProgramId ) + loadProgramOffset + *( ec++ ), paddedSIZE, &pointer);
      float *fp = (float*)pointer;
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
   float * __restrict  data = ( float* )ptr->pushBufferPointer;/*(float*)ptr->offset*;*/
   for ( long i = 0; i < SIZE; ++i ) //TODO: ced: find out if this loop for the get or for the reset in a future use of the same shader or just for the alignment???
      data[i] = f[i];
   _CGprogram *program = ptr->program;

   CgParameterResource *parameterResource = rglGetParameterResource( ptr->program, ptr->parameterEntry );
   unsigned short resource = parameterResource->resource;
   unsigned short *ec = ( unsigned short * )( ptr->program->resources ) + resource + 1;//+1 to skip the register
   if ( RGL_LIKELY( *ec ) )
   {
      swapandsetfp<SIZE>( program->header.instructionCount*16, program->loadProgramId, program->loadProgramOffset, ec, ( unsigned int * )data );
   }
}

template<int SIZE> static void setVectorTypeSharedfpIndex (void *data, const void* __restrict v, const int /*index*/ )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   RGLcontext * LContext = _CurrentContext;
   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;
   const CgParameterResource *parameterResource = rglGetParameterResource( ptr->program, ptr->parameterEntry );
   unsigned short resource = parameterResource->resource;
   unsigned short sharedResource = *(( unsigned short * )( ptr->program->resources ) + resource );
   const unsigned int * __restrict vi = ( const unsigned int* )v;

   GLuint dstVidOffset = gmmIdToOffset( driver->sharedFPConstantsId ) + sharedResource * 16;
   unsigned int values[4];
   values[0] = SWAP_IF_BIG_ENDIAN( vi[0] );
   values[1] = ( 1 < SIZE ) ? SWAP_IF_BIG_ENDIAN( vi[1] ) : 0;
   values[2] = ( 2 < SIZE ) ? SWAP_IF_BIG_ENDIAN( vi[2] ) : 0;
   values[3] = ( 3 < SIZE ) ? SWAP_IF_BIG_ENDIAN( vi[3] ) : 0;
   GCM_FUNC( cellGcmInlineTransfer, dstVidOffset, values, 4, 0 );

   LContext->needValidate |= RGL_VALIDATE_FRAGMENT_SHARED_CONSTANTS;
   // XXX we don't care about 32bit wrapping, do we ?
   ++LContext->LastFPConstantModification;
}

template<int SIZE> static void setVectorTypeSharedfpIndexArray (void *data, const void* __restrict v, const int index )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   RGLcontext * LContext = _CurrentContext;
   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;
   const CgParameterResource *parameterResource = rglGetParameterResource( ptr->program, ptr->parameterEntry );
   unsigned short resource = parameterResource->resource;

   //slow... skip the indices
   unsigned short *sharedResourcePtr = (( unsigned short * )( ptr->program->resources ) + resource );//no +1 here, we want the register
   int arrayIndex = index;
   while ( arrayIndex ) //jump to the right index... this is slow
   {
      sharedResourcePtr += (( *sharedResourcePtr ) + 2 );////+1 for the register, +1 for the count, +count for the number of embedded consts
      arrayIndex--;
   }
   unsigned short sharedResource = *sharedResourcePtr;

   const unsigned int * __restrict vi = ( const unsigned int* )v;

   GLuint dstVidOffset = gmmIdToOffset( driver->sharedFPConstantsId ) + sharedResource * 16;
   unsigned int values[4];
   values[0] = SWAP_IF_BIG_ENDIAN( vi[0] );
   values[1] = ( 1 < SIZE ) ? SWAP_IF_BIG_ENDIAN( vi[1] ) : 0;
   values[2] = ( 2 < SIZE ) ? SWAP_IF_BIG_ENDIAN( vi[2] ) : 0;
   values[3] = ( 3 < SIZE ) ? SWAP_IF_BIG_ENDIAN( vi[3] ) : 0;
   GCM_FUNC( cellGcmInlineTransfer, dstVidOffset, values, 4, 0 ); 

   LContext->needValidate |= RGL_VALIDATE_FRAGMENT_SHARED_CONSTANTS;
   // XXX we don't care about 32bit wrapping, do we ?
   ++LContext->LastFPConstantModification;
}
template<int SIZE> static void setVectorTypeSharedvpIndex (void *data, const void* __restrict v, const int /*index*/ )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   const float * __restrict f = ( const float * __restrict )v;
   const CgParameterResource *parameterResource = rglGetParameterResource( ptr->program, ptr->parameterEntry );
   unsigned short resource = parameterResource->resource;
   float * __restrict dst = ( float * __restrict )ptr->pushBufferPointer;
   for ( long i = 0; i < SIZE; ++ i )
      dst[i] = f[i];
   rglPlatformSetVertexRegister4fv( resource, dst );
}

template<int SIZE> static void setVectorTypeSharedvpIndexArray (void *data, const void* __restrict v, const int index )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   const float * __restrict f = ( const float * __restrict )v;
   const CgParameterResource *parameterResource = rglGetParameterResource( ptr->program, ptr->parameterEntry );
   unsigned short resource = parameterResource->resource + index; ///TODO: assume contiguous here , right ?
   float * __restrict dst = ( float * __restrict )ptr->pushBufferPointer;
   for ( long i = 0; i < SIZE; ++ i )
      dst[i] = f[i];
   rglPlatformSetVertexRegister4fv( resource, dst );
}


//  matrix uniforms
// note that Cg generated matrices are 1 row per binary param
// storage within the parameter is row major (so register setting is easier)

//tmp array tentative

#define ROW_MAJOR 0
#define COL_MAJOR 1

template <int SIZE> static void setVectorTypevpIndex (void *data, const void* __restrict v, const int /*index*/ )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   RGLcontext * LContext = _CurrentContext;
   const float * __restrict f = ( const float* )v;
   float * __restrict dst = ( float* )ptr->pushBufferPointer;
   for ( long i = 0; i < SIZE; ++ i )
      dst[i] = f[i];
   LContext->needValidate |= RGL_VALIDATE_VERTEX_CONSTANTS;
}
template <int SIZE> static void setVectorTypevpIndexArray (void *data, const void* __restrict v, const int index )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   RGLcontext * LContext = _CurrentContext;
   const float * __restrict f = ( const float* )v;
   float *  __restrict dst = ( float* )( *(( unsigned int ** )ptr->pushBufferPointer + index ) );
   for ( long i = 0; i < SIZE; ++ i )
      dst[i] = f[i];
   LContext->needValidate |= RGL_VALIDATE_VERTEX_CONSTANTS;
}

template<int SIZE> static void setVectorTypefpIndex (void *dat, const void *v, const int /*index*/ )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)dat;
   float * __restrict  f = (float*)v;
   float * __restrict  data = (float*)ptr->pushBufferPointer;/*(float*)ptr->offset*;*/
   for ( long i = 0; i < SIZE; ++i ) //TODO: ced: find out if this loop for the get or for the reset in a future use of the same shader or just for the alignment???
      data[i] = f[i];
   _CGprogram *program = ptr->program;

   const CgParameterResource *parameterResource = rglGetParameterResource( program, ptr->parameterEntry );
   unsigned short resource = parameterResource->resource;
   unsigned short *ec = ( unsigned short * )( ptr->program->resources ) + resource + 1;
   if ( RGL_LIKELY( *ec ) )
   {
      swapandsetfp<SIZE>( program->header.instructionCount*16, program->loadProgramId, program->loadProgramOffset, ec, ( unsigned int * )data );
   }
}

template<int SIZE> static void setVectorTypefpIndexArray (void *dat, const void* __restrict v, const int index )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)dat;
   float * __restrict  f = ( float* )v;
   float * __restrict  data = ( float* )ptr->pushBufferPointer;/*(float*)ptr->offset*;*/
   for ( long i = 0; i < SIZE; ++i ) //TODO: ced: find out if this loop for the get or for the reset in a future use of the same shader or just for the alignment???
      data[i] = f[i];
   _CGprogram *program = ptr->program;

   const CgParameterResource *parameterResource = rglGetParameterResource( program, ptr->parameterEntry );
   unsigned short resource = parameterResource->resource;
   unsigned short *ec = ( unsigned short * )( program->resources ) + resource + 1;
   int arrayIndex = index;
   while ( arrayIndex ) //jump to the right index... this is slow
   {
      ec += (( *ec ) + 2 );//+1 for the register, +1 for the count, +count for the number of embedded consts
      arrayIndex--;
   }
   if ( RGL_LIKELY( *ec ) )
   {
      swapandsetfp<SIZE>( program->header.instructionCount*16, program->loadProgramId, program->loadProgramOffset, ec, ( unsigned int * )data );
   }
}

//matrices
template <int ROWS, int COLS, int ORDER> static void setMatrixvpIndex (void *data, const void* __restrict v, const int index )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   RGLcontext * LContext = _CurrentContext;
   float *  __restrict f = ( float* )v;
   float *  __restrict dst = ( float* )ptr->pushBufferPointer;
   for ( long row = 0; row < ROWS; ++row )
   {
      for ( long col = 0; col < COLS; ++col )
         dst[row * 4 + col] = ( ORDER == ROW_MAJOR ) ? f[row * COLS + col] : f[col * ROWS + row];
   }
   LContext->needValidate |= RGL_VALIDATE_VERTEX_CONSTANTS;
}

template <int ROWS, int COLS, int ORDER> static void setMatrixSharedvpIndex (void *data, const void*  __restrict v, const int /*index*/ )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   float * __restrict f = ( float* )v;
   float * __restrict dst = ( float* )ptr->pushBufferPointer;

   const CgParameterResource *parameterResource = rglGetParameterResource( ptr->program, ptr->parameterEntry );
   unsigned short resource = parameterResource->resource;

   float tmp[ROWS*4];
   for ( long row = 0; row < ROWS; ++row )
   {
      for ( long col = 0; col < COLS; ++col )
      {
         tmp[row*4 + col] = dst[row * 4 + col] = ( ORDER == ROW_MAJOR ) ? f[row * COLS + col] : f[col * ROWS + row];
      }
      for ( long col = COLS; col < 4; ++col ) tmp[row*4 + col] = dst[row*4+col];
   }

   GCM_FUNC( cellGcmSetVertexProgramParameterBlock, resource, ROWS, (const float*)tmp);
}

template <int ROWS, int COLS, int ORDER> static void setMatrixSharedvpIndexArray (void *data, const void*  __restrict v, const int index )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   float * __restrict f = ( float* )v;
   float * __restrict dst = ( float* )ptr->pushBufferPointer;

   const CgParameterResource *parameterResource = rglGetParameterResource( ptr->program, ptr->parameterEntry );
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
   GCM_FUNC( cellGcmSetVertexProgramParameterBlock, resource, ROWS, tmp );
}

template <int ROWS, int COLS, int ORDER> static void setMatrixSharedfpIndex (void *data, const void* __restrict v, const int /*index*/ )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;

   const CgParameterResource *parameterResource = rglGetParameterResource( ptr->program, ptr->parameterEntry );
   unsigned short resource = parameterResource->resource;
   unsigned short sharedResource = *(( unsigned short * )( ptr->program->resources ) + resource );

   GLuint dstVidOffset = gmmIdToOffset( driver->sharedFPConstantsId ) + sharedResource * 16;
   //we assume that the assignment is contiguous
   const unsigned int * __restrict u = ( const unsigned int* )v;

   unsigned int tmp[ROWS*4];
   for ( long row = 0; row < ROWS; ++row )
   {
      tmp[row*4 + 0] = (( ORDER == ROW_MAJOR ) ? u[row * COLS + 0] : u[0 * ROWS + row] );
      tmp[row*4 + 1] = (( 1 < COLS ) ? (( ORDER == ROW_MAJOR ) ? u[row * COLS + 1] : u[1 * ROWS + row] ) : 0 );
      tmp[row*4 + 2] = (( 2 < COLS ) ? (( ORDER == ROW_MAJOR ) ? u[row * COLS + 2] : u[2 * ROWS + row] ) : 0 );
      tmp[row*4 + 3] = (( 3 < COLS ) ? (( ORDER == ROW_MAJOR ) ? u[row * COLS + 3] : u[3 * ROWS + row] ) : 0 );
   }
   GCM_FUNC( cellGcmSetTransferLocation, CELL_GCM_LOCATION_LOCAL );
   void *pointer=NULL;
   GCM_FUNC( cellGcmSetInlineTransferPointer, dstVidOffset, 4*ROWS, &pointer);
   float *fp = (float*)pointer;
   float *src = (float*)tmp;
   for (uint32_t j=0; j<ROWS;j++)
   {
      fp[0] = cellGcmSwap16Float32(src[0]);
      fp[1] = cellGcmSwap16Float32(src[1]);
      fp[2] = cellGcmSwap16Float32(src[2]);
      fp[3] = cellGcmSwap16Float32(src[3]);
      fp+=4;src+=4;
   }

   RGLcontext * LContext = _CurrentContext;
   LContext->needValidate |= RGL_VALIDATE_FRAGMENT_SHARED_CONSTANTS;
   ++LContext->LastFPConstantModification;
}

template <int ROWS, int COLS, int ORDER> static void setMatrixSharedfpIndexArray (void *data, const void* __restrict v, const int index )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   //TODO: double check for the semi endian swap... not done here, is it done by the RSX ?
   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;

   const CgParameterResource *parameterResource = rglGetParameterResource( ptr->program, ptr->parameterEntry );
   unsigned short resource = parameterResource->resource;
   //slow... skip the indices
   unsigned short *sharedResourcePtr = (( unsigned short * )( ptr->program->resources ) + resource );
   int arrayIndex = index * ROWS;
   while ( arrayIndex ) //jump to the right index... this is slow
   {
      sharedResourcePtr += (( *sharedResourcePtr ) + 2 );//+1 for the register, +1 for the count, +count for the number of embedded consts
      arrayIndex--;
   }
   unsigned short sharedResource = *sharedResourcePtr;

   GLuint dstVidOffset = gmmIdToOffset( driver->sharedFPConstantsId ) + sharedResource * 16;
   //we assume that the assignment is contiguous
   const unsigned int * __restrict u = ( const unsigned int* )v;

   unsigned int tmp[ROWS*4];
   for ( long row = 0; row < ROWS; ++row )
   {
      tmp[row*4 + 0] = (( ORDER == ROW_MAJOR ) ? u[row * COLS + 0] : u[0 * ROWS + row] );
      tmp[row*4 + 1] = (( 1 < COLS ) ? (( ORDER == ROW_MAJOR ) ? u[row * COLS + 1] : u[1 * ROWS + row] ) : 0 );
      tmp[row*4 + 2] = (( 2 < COLS ) ? (( ORDER == ROW_MAJOR ) ? u[row * COLS + 2] : u[2 * ROWS + row] ) : 0 );
      tmp[row*4 + 3] = (( 3 < COLS ) ? (( ORDER == ROW_MAJOR ) ? u[row * COLS + 3] : u[3 * ROWS + row] ) : 0 );
   }

   GCM_FUNC( cellGcmSetTransferLocation, CELL_GCM_LOCATION_LOCAL );


   void *pointer=NULL;
   GCM_FUNC( cellGcmSetInlineTransferPointer, dstVidOffset, 4*ROWS, &pointer);
   float *fp = (float*)pointer;
   const float *src = (const float*)tmp;
   for (uint32_t j=0; j<4*ROWS;j++)
   {
      *fp = cellGcmSwap16Float32(*src);
      fp++;src++;
   }

   RGLcontext * LContext = _CurrentContext;
   LContext->needValidate |= RGL_VALIDATE_FRAGMENT_SHARED_CONSTANTS;
   ++LContext->LastFPConstantModification;
}

//TODO ?: check: //works only for the consecutive alloc...
template <int ROWS, int COLS, int ORDER> static void setMatrixvpIndexArray (void *data, const void* __restrict v, const int index )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   RGLcontext * LContext = _CurrentContext;
   float *  __restrict f = ( float* )v;
   float *  __restrict dst = ( float* )( *(( unsigned int ** )ptr->pushBufferPointer + index ) );
   for ( long row = 0; row < ROWS; ++row )
   {
      for ( long col = 0; col < COLS; ++col )
         dst[row * 4 + col] = ( ORDER == ROW_MAJOR ) ? f[row * COLS + col] : f[col * ROWS + row];
   }
   LContext->needValidate |= RGL_VALIDATE_VERTEX_CONSTANTS;
}
template <int ROWS, int COLS, int ORDER> static void setMatrixfpIndex (void *data, const void* __restrict v, const int /*index*/ )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   float *  __restrict f = ( float* )v;
   float *  __restrict dst = ( float* )ptr->pushBufferPointer;
   _CGprogram *program = (( CgRuntimeParameter* )ptr )->program;
   const CgParameterResource *parameterResource = rglGetParameterResource( program, ptr->parameterEntry );
   unsigned short resource = parameterResource->resource;
   unsigned short *ec = ( unsigned short * )program->resources + resource + 1; //+1 to skip the register
   for ( long row = 0; row < ROWS; ++row )
   {
      for ( long col = 0; col < COLS; ++col )
         dst[row * 4 + col] = ( ORDER == ROW_MAJOR ) ? f[row * COLS + col] : f[col * ROWS + row];
      int count = *ec;
      if ( RGL_LIKELY( count ) )
      {
         swapandsetfp<COLS>( program->header.instructionCount*16, program->loadProgramId, program->loadProgramOffset, ec, ( unsigned int * )dst + row * 4 );
      }
      ec += count + 2; //+1 for the register, +1 for the count, + count for the number of embedded consts
   }
}
template <int ROWS, int COLS, int ORDER> static void setMatrixfpIndexArray (void *data, const void* __restrict v, const int index )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   float *  __restrict f = ( float* )v;
   float *  __restrict dst = ( float* )ptr->pushBufferPointer;
   _CGprogram *program = ptr->program;
   const CgParameterResource *parameterResource = rglGetParameterResource( program, ptr->parameterEntry );
   unsigned short resource = parameterResource->resource;
   unsigned short *ec = ( unsigned short * )program->resources + resource + 1;//+1 to skip the register
   int arrayIndex = index * ROWS;
   while ( arrayIndex ) //jump to the right index... this is slow
   {
      unsigned short count = ( *ec );
      ec += ( count + 2 ); //+1 for the register, +1 for the count, +count for the number of embedded consts
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
      ec += count + 2;//+1 for the register, +1 for the count, +count for the number of embedded consts
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
         {&setVectorTypeSharedvpIndex<1>, &setVectorTypeSharedvpIndex<2>, &setVectorTypeSharedvpIndex<3>, &setVectorTypeSharedvpIndex<4>, }, //should be the shared
         {&setVectorTypeSharedfpIndex<1>, &setVectorTypeSharedfpIndex<2>, &setVectorTypeSharedfpIndex<3>, &setVectorTypeSharedfpIndex<4>, } //should be the shared
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

static void setSamplerfp (void *data, const void*v, int ) //index
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   _CGprogram *program = (( CgRuntimeParameter* )ptr )->program;
   const CgParameterResource *parameterResource = rglGetParameterResource( program, (( CgRuntimeParameter* )ptr )->parameterEntry );

   // the value of v == NULL when it is called from cgGLEnableTextureParameter
   // the value of v == NULL when it is called from  cgGLSetTextureParameter
   // this may be called by a connected param to propagate its value
   // the spec says that the set should not cause the bind
   // so only do the bind when the call comes from cgGLEnableTextureParameter
   if ( v )
      *( GLuint* )ptr->pushBufferPointer = *( GLuint* )v;
   else
   {
      rglTextureImageUnit *unit = _CurrentContext->TextureImageUnits + ( parameterResource->resource - CG_TEXUNIT0 );
      rglBindTextureInternal( unit, *( GLuint* )ptr->pushBufferPointer, ptr->glType );
   }
}

static void setSamplervp (void *data, const void*v, int ) //index
{
   // the value of v == NULL when it is called from cgGLEnableTextureParameter
   // the value of v == NULL when it is called from  cgGLSetTextureParameter
   // this may be called by a connected param to propagate its value
   // the spec says that the set should not cause the bind
   // so only do the bind when the call comes from cgGLEnableTextureParameter
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   if (v)
   {
      *(GLuint*)ptr->pushBufferPointer = *( GLuint* )v;
   }
}


#undef ROW_MAJOR
#undef COL_MAJOR
// Previously from Shader.cpp

//---------------------------------------------------------------------------------------------------------
#define ROW_MAJOR 0
#define COL_MAJOR 1

//This function creates the push buffer and the related structures
void rglCreatePushBuffer(void *data)
{
   _CGprogram *program = (_CGprogram*)data;

   //first pass to compute the space needed
   int bufferSize = 0;
   int programPushBufferPointersSize = 0;
   int extraStorageInWords = 0;
   int offsetCount = 0;
   int samplerCount = 0;
   int profileIndex = ( program->header.profile == CG_PROFILE_SCE_FP_TYPEB || //program->header.profile==CG_PROFILE_SCE_FP_TYPEC ||
         program->header.profile == CG_PROFILE_SCE_FP_RSX ) ? FRAGMENT_PROFILE_INDEX : VERTEX_PROFILE_INDEX;

   bool hasSharedParams = false;
   int arrayCount = 1;
   for ( int i = 0;i < program->rtParametersCount;i++ )
   {
      const CgParameterEntry *parameterEntry = program->parametersEntries + i;

      //skip the unrolled arrays and the structures
      if (( parameterEntry->flags & CGP_STRUCTURE ) || ( parameterEntry->flags & CGP_UNROLLED ) )
      {
         arrayCount = 1;
         continue;
      }

      if (( parameterEntry->flags & CGPF_REFERENCED ) )
      {
         if ( parameterEntry->flags & CGP_ARRAY )
         {
            const CgParameterArray *parameterArray = rglGetParameterArray( program, parameterEntry );
            arrayCount = rglGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount );
            continue;
         }
         if (( parameterEntry->flags & CGPV_MASK ) == CGPV_UNIFORM )
         {
            const CgParameterResource *parameterResource = rglGetParameterResource( program, parameterEntry );
            if ( parameterResource->type >= CG_SAMPLER1D &&  parameterResource->type <= CG_SAMPLERCUBE )
            {
               // store 1 sampler and 1 offset for texture samplers.
               offsetCount += arrayCount;
               samplerCount += arrayCount;
            }
            else if ( profileIndex == VERTEX_PROFILE_INDEX )
            {
               if ( parameterResource->type == CGP_SCF_BOOL )
               {
                  //do nothing
               }
               else if ( !( parameterEntry->flags & CGPF_SHARED ) )
               {
                  int registerStride = isMatrix(( CGtype )parameterResource->type ) ? rglGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
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
                        //I use the programPushBuffer pointer so it's valid to have element in an array without any affectation
                        if ( program->resources[resourceIndex] != 0xffff )
                           bufferSize += referencedSize; //referenced: push buffer
                        else
                           extraStorageInWords += notReferencedSize; //not referenced , extra storage location
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
            else //profileIndex == FRAGMENT_PROFILE_INDEX
            {
               int registerStride = isMatrix(( CGtype )parameterResource->type ) ? rglGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
               if ( !( parameterEntry->flags & CGPF_SHARED ) )
               {
                  //TODO: check this case
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

   if (( profileIndex == FRAGMENT_PROFILE_INDEX ) && (hasSharedParams))
      bufferSize += 8 + 3 + 2; // GCM_PORT_TESTED [CEDRIC] +3 for the channel switch that gcm does + 2 for the OUT end

   bufferSize = rglPad( bufferSize, 4 );

   //allocate the buffer(s)
   unsigned int storageSizeInWords = bufferSize + extraStorageInWords;
   if ( storageSizeInWords )
      program->memoryBlock = ( unsigned int* )memalign( 16, storageSizeInWords * 4 );
   else
      program->memoryBlock = NULL;

   //TODO: this is tmp
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

   uint32_t *rglGcmCurrent = (uint32_t*)program->memoryBlock;
   program->constantPushBuffer = ( bufferSize > 0 ) ? ( unsigned int * )rglGcmCurrent : NULL;
   unsigned int **programPushBuffer = program->constantPushBufferPointers;
   program->constantPushBufferWordSize = bufferSize;
   GLuint *currentStorage = ( GLuint * )( rglGcmCurrent + bufferSize );

   int outOfMemory = 0;
   //second pass to fill the buffer
   arrayCount = 1;
   const CgParameterEntry *containerEntry = NULL;
   for ( int i = 0;i < program->rtParametersCount;i++ )
   {
      CgRuntimeParameter *rtParameter = program->runtimeParameters + i;
      const CgParameterEntry *parameterEntry = program->parametersEntries + i;
      if ( containerEntry == NULL )
         containerEntry = parameterEntry;

      //rtParameter->setter = _cgRaiseInvalidParam;
      //rtParameter->setterr = _cgRaiseNotMatrixParam;
      //rtParameter->setterc = _cgRaiseNotMatrixParam;
      rtParameter->samplerSetter = _cgRaiseInvalidParamIndex;

      //tentative
      rtParameter->setterIndex = _cgRaiseInvalidParamIndex;
      rtParameter->setterrIndex = _cgRaiseNotMatrixParamIndex;
      rtParameter->settercIndex = _cgRaiseNotMatrixParamIndex;

      CGparameter id = ( CGparameter )rglCreateName( &_CurrentContext->cgParameterNameSpace, ( void* )rtParameter );
      if ( !id )
      {
         outOfMemory = 1;
         break;
      }

      rtParameter->id = id;
      rtParameter->parameterEntry = parameterEntry;
      rtParameter->program = program;

      //skip the unrolled arrays and the structures
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
            const CgParameterArray *parameterArray = rglGetParameterArray( program, parameterEntry );
            arrayCount = rglGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount );
            //continue to the next item
            continue;
         }
         if ((parameterEntry->flags & CGPV_MASK) == CGPV_UNIFORM)
         {
            //TODO: rtParameter->defaultNormalize = CG_FALSE;
            rtParameter->glType = GL_NONE;
            //TODO: needed ? rtParameter->flags = 0;
            const CgParameterResource *parameterResource = rglGetParameterResource( program, parameterEntry );
            if ( parameterResource->type >= CG_SAMPLER1D && parameterResource->type <= CG_SAMPLERCUBE )
            {
               //TODO
               rtParameter->pushBufferPointer = samplerValuesLocation;
               // initialize the texture name to zero, used by the setSamplerfp call to rglBindTextureInternal
               *samplerValuesLocation = 0;
               samplerValuesLocation++;

               // store the texture unit indices.
               *samplerIndices = i;
               samplerIndices++;
               *samplerUnits = parameterResource->resource - CG_TEXUNIT0;
               samplerUnits++;

               // XXX the setter is called when validating vertex programs.
               // this would cause a CG error.
               // the parameters should have a "validate" function instead
               if ( profileIndex == VERTEX_PROFILE_INDEX )
               {
                  rtParameter->setterIndex = _cgIgnoreSetParamIndex;
                  rtParameter->samplerSetter = setSamplervp;
               }
               else
               {
                  rtParameter->samplerSetter = setSamplerfp;
               }
               rtParameter->glType = rglCgGetSamplerGLTypeFromCgType(( CGtype )( parameterResource->type ) );
            }
            else
            {
               if ( profileIndex == VERTEX_PROFILE_INDEX )
               {
                  if ( parameterResource->type == CGP_SCF_BOOL )
                  {
                     //do nothing
                  }
                  else if ( !( parameterEntry->flags & CGPF_SHARED ) )
                  {
                     int registerStride = isMatrix(( CGtype )parameterResource->type ) ? rglGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
                     int registerCount = arrayCount * registerStride;
                     if ( parameterEntry->flags & CGP_CONTIGUOUS )
                     {
                        memset( rglGcmCurrent, 0, 4*( 4*registerCount + 3 ) );
                        GCM_FUNC_BUFFERED( cellGcmSetVertexProgramParameterBlock, rglGcmCurrent, parameterResource->resource, registerCount, ( float* )rglGcmCurrent );
                        rtParameter->pushBufferPointer = rglGcmCurrent - 4 * registerCount;
                     }
                     else
                     {
                        rtParameter->pushBufferPointer = programPushBuffer;
                        int resourceIndex = parameterResource->resource;
                        for ( int j = 0;j < arrayCount;j++, resourceIndex += registerStride )
                        {
                           //I use the programPushBuffer pointer so it's valid to have element in an array without any affectation
                           if ( program->resources[resourceIndex] != 0xffff )
                           {
                              memset( rglGcmCurrent, 0, 4*( 4*registerStride + 3 ) );
                              GCM_FUNC_BUFFERED( cellGcmSetVertexProgramParameterBlock, rglGcmCurrent, program->resources[resourceIndex], registerStride, ( float* )rglGcmCurrent ); // GCM_PORT_TESTED [KHOFF]
                              *( programPushBuffer++ ) = ( unsigned int* )( rglGcmCurrent - 4 * registerStride );
                           }
                           else
                           {
                              //This case is when there is an array item which is not referenced
                              //we still call tbe setter function, so we have to store the info somewhere...
                              //and we need to return the value previously set in case of the user asks for a get
                              *( programPushBuffer++ ) = ( unsigned int* )currentStorage;
                              currentStorage += 4 * registerStride;
                           }
                        }
                     }
                  }
                  else
                  {
                     rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;

                     if ( parameterEntry->flags & CGP_CONTIGUOUS )
                        rtParameter->pushBufferPointer = driver->sharedVPConstants + parameterResource->resource * 4 * sizeof( float );
                     else
                     {
                        int registerStride = isMatrix(( CGtype )parameterResource->type ) ? rglGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
                        int registerCount = arrayCount * registerStride;
                        for ( int j = 0;j < registerCount;j += registerStride )
                        {
                           *programPushBuffer = ( unsigned int* )driver->sharedVPConstants + program->resources[parameterResource->resource+j] * 4 * sizeof( float );
                           rtParameter->pushBufferPointer = programPushBuffer++;
                        }
                     }
                  }
               }
               else //if (profileIndex == FRAGMENT_PROFILE_INDEX)
               {
                  if ( parameterEntry->flags & CGPF_SHARED )
                  {
                     // XXX needs an offset for the get
                     rtParameter->pushBufferPointer = NULL;
                  }
                  else
                  {
                     int registerStride = isMatrix(( CGtype )parameterResource->type ) ? rglGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
                     int registerCount = arrayCount * registerStride;
                     rtParameter->pushBufferPointer = currentStorage;
                     currentStorage += 4 * registerCount;
                  }
               }

               switch ( parameterResource->type )
               {
                  case CG_FLOAT:
                  case CG_FLOAT1: case CG_FLOAT2: case CG_FLOAT3: case CG_FLOAT4:
                     // if this gets updated, don't forget the halfs below
                     {
                        unsigned int floatCount = rglCountFloatsInCgType(( CGtype )parameterResource->type );
                        rtParameter->setterIndex = setVectorTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][( containerEntry->flags&CGPF_SHARED ) ? 1 : 0][profileIndex][floatCount - 1];
                     }
                     break;
                  case CG_FLOAT1x1: case CG_FLOAT1x2: case CG_FLOAT1x3: case CG_FLOAT1x4:
                  case CG_FLOAT2x1: case CG_FLOAT2x2: case CG_FLOAT2x3: case CG_FLOAT2x4:
                  case CG_FLOAT3x1: case CG_FLOAT3x2: case CG_FLOAT3x3: case CG_FLOAT3x4:
                  case CG_FLOAT4x1: case CG_FLOAT4x2: case CG_FLOAT4x3: case CG_FLOAT4x4:
                     // if this gets updated, don't forget the halfs below
                     rtParameter->setterrIndex = setMatrixTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][( containerEntry->flags&CGPF_SHARED ) ? 1 : 0][profileIndex][rglGetTypeRowCount(( CGtype )parameterResource->type ) - 1][rglGetTypeColCount(( CGtype )parameterResource->type ) - 1][ROW_MAJOR];
                     rtParameter->settercIndex = setMatrixTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][( containerEntry->flags&CGPF_SHARED ) ? 1 : 0][profileIndex][rglGetTypeRowCount(( CGtype )parameterResource->type ) - 1][rglGetTypeColCount(( CGtype )parameterResource->type ) - 1][COL_MAJOR];
                     break;
                  case CG_SAMPLER1D: case CG_SAMPLER2D: case CG_SAMPLER3D: case CG_SAMPLERRECT: case CG_SAMPLERCUBE:
                     // A used sampler that does not have a TEXUNIT resource ?
                     // not sure if we ever go here.
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
                        unsigned int floatCount = rglCountFloatsInCgType(( CGtype )parameterResource->type );
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
                     rtParameter->setterrIndex = setMatrixTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][( containerEntry->flags&CGPF_SHARED ) ? 1 : 0][profileIndex][rglGetTypeRowCount(( CGtype )parameterResource->type ) - 1][rglGetTypeColCount(( CGtype )parameterResource->type ) - 1][ROW_MAJOR];
                     rtParameter->settercIndex = setMatrixTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][( containerEntry->flags&CGPF_SHARED ) ? 1 : 0][profileIndex][rglGetTypeRowCount(( CGtype )parameterResource->type ) - 1][rglGetTypeColCount(( CGtype )parameterResource->type ) - 1][COL_MAJOR];
                     break;
                     // addition to be compatible with cgc 2.0 
                  case CG_STRING:
                     break;
                  default:
                     break;
               }
            }
         }
         else if (( parameterEntry->flags & CGPV_MASK ) == CGPV_VARYING )
         {
            if (( parameterEntry->flags & CGPD_MASK ) == CGPD_IN && profileIndex == VERTEX_PROFILE_INDEX )
            {
               rtParameter->setterIndex = setAttribConstantIndex;
            }
         }
      }
      else
      {
         if (( parameterEntry->flags & CGPV_MASK ) == CGPV_UNIFORM )
         {
            if ( parameterEntry->flags & CGP_ARRAY )
               continue;

            const CgParameterResource *parameterResource = rglGetParameterResource( program, parameterEntry );
            // we silently ignore valid sets on unused parameters.
            switch ( parameterResource->type )
            {
               case CG_FLOAT:
               case CG_FLOAT1: case CG_FLOAT2: case CG_FLOAT3: case CG_FLOAT4:
                  //rtParameter->setter = _cgIgnoreSetParam;
                  rtParameter->setterIndex = _cgIgnoreSetParamIndex;
                  break;
               case CG_FLOAT1x1: case CG_FLOAT1x2: case CG_FLOAT1x3: case CG_FLOAT1x4:
               case CG_FLOAT2x1: case CG_FLOAT2x2: case CG_FLOAT2x3: case CG_FLOAT2x4:
               case CG_FLOAT3x1: case CG_FLOAT3x2: case CG_FLOAT3x3: case CG_FLOAT3x4:
               case CG_FLOAT4x1: case CG_FLOAT4x2: case CG_FLOAT4x3: case CG_FLOAT4x4:
                  //rtParameter->setterr = _cgIgnoreSetParam;
                  //rtParameter->setterc = _cgIgnoreSetParam;
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
                  //rtParameter->setter = _cgIgnoreSetParam;
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
                  //rtParameter->setterr = _cgIgnoreSetParam;
                  //rtParameter->setterc = _cgIgnoreSetParam;
                  rtParameter->setterrIndex = _cgIgnoreSetParamIndex;
                  rtParameter->settercIndex = _cgIgnoreSetParamIndex;
                  break;
                  // addition to be compatible with cgc 2.0 
               case CG_STRING:
                  break;
               default:
                  break;
            }
         }
         else if (( parameterEntry->flags & CGPV_MASK ) == CGPV_VARYING )
         {
            if (( parameterEntry->flags & CGPD_MASK ) == CGPD_IN && profileIndex == VERTEX_PROFILE_INDEX )
               rtParameter->setterIndex = setAttribConstantIndex;
         }
      }
      arrayCount = 1;
      containerEntry = NULL;
   }

   //add padding
   if ( bufferSize > 0 )
   {
      int nopCount = ( program->constantPushBuffer + bufferSize ) - ( unsigned int * )rglGcmCurrent;
      GCM_FUNC_BUFFERED( cellGcmSetNopCommand, rglGcmCurrent, nopCount ); // GCM_PORT_TESTED [KHOFF]
   }
}

//this function sets the embedded constant to their default value in the ucode of a fragment shader
//it's called at setup time right after loading the program. this function could be removed if the
//default values were already in the shader code
void rglSetDefaultValuesVP (void *data)
{
   _CGprogram *program = (_CGprogram*)data;
   int count = program->defaultValuesIndexCount;

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
         const CgParameterArray *parameterArray = rglGetParameterArray( program, parameterEntry );
         arrayCount = rglGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount );
         parameterEntry++;
         rtParameter++;
      }

      if ( rtParameter->pushBufferPointer ) //unreferenced might have default values
      {
         const CgParameterResource *parameterResource = rglGetParameterResource( program, parameterEntry );
         const float *itemDefaultValues = program->defaultValues + program->defaultValuesIndices[i].defaultValueIndex;
         int registerStride = isMatrix(( CGtype )parameterResource->type ) ? rglGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
         if ( parameterEntry->flags & CGP_CONTIGUOUS )
            __builtin_memcpy( rtParameter->pushBufferPointer, itemDefaultValues, arrayCount * registerStride *4*sizeof( float ) );
         else
         {
            unsigned int *pushBufferPointer = (( unsigned int * )rtParameter->pushBufferPointer );
            for ( int j = 0;j < arrayCount;j++ )
            {
               unsigned int *pushBufferAddress = isArray ? ( *( unsigned int** )pushBufferPointer ) : pushBufferPointer;
               __builtin_memcpy( pushBufferAddress, itemDefaultValues, registerStride*4*sizeof( float ) );
               pushBufferPointer += isArray ? 1 : 3 + registerStride * 4;
               itemDefaultValues += 4 * registerStride;
            }
         }
      }
   }
}

void rglSetDefaultValuesFP (void *data)
{
   _CGprogram *program = (_CGprogram*)data;
   int count = program->defaultValuesIndexCount;

   for ( int i = 0;i < count;i++ )
   {
      const void * __restrict pItemDefaultValues = program->defaultValues + program->defaultValuesIndices[i].defaultValueIndex;
      const unsigned int * itemDefaultValues = ( const unsigned int * )pItemDefaultValues;
      int index = ( int )program->defaultValuesIndices[i].entryIndex;

      CgRuntimeParameter *rtParameter = program->runtimeParameters + index;
      float *hostMemoryCopy = ( float * )rtParameter->pushBufferPointer;

      if ( hostMemoryCopy ) //certain parameter are not referenced but still have a default value.
      {
         const CgParameterEntry *parameterEntry = rtParameter->parameterEntry;
         int arrayCount = 1;
         if ( parameterEntry->flags & CGP_ARRAY )
         {
            const CgParameterArray *parameterArray = rglGetParameterArray( program, parameterEntry );
            arrayCount = rglGetSizeofSubArray( parameterArray->dimensions, parameterArray->dimensionCount );
            i++;
            parameterEntry++;
         }
         const CgParameterResource *parameterResource = rglGetParameterResource( program, parameterEntry );
         unsigned short *resource = program->resources + parameterResource->resource + 1; //+1 to skip the register
         int registerStride = isMatrix(( CGtype )parameterResource->type ) ? rglGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
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
            __builtin_memcpy(( void* )hostMemoryCopy, ( void* )itemDefaultValues, sizeof( float )*4 );
            hostMemoryCopy += 4;
            itemDefaultValues += 4;
            resource++; //skip the register of the next item
         }
      }
   }
}

/*============================================================
  PLATFORM BUFFER
  ============================================================ */

static void rglDeallocateBuffer (void *data)
{
   rglBufferObject *bufferObject = (rglBufferObject*)data;
   rglGcmBufferObject *rglBuffer = (rglGcmBufferObject*)bufferObject->platformBufferObject;

   switch ( rglBuffer->pool )
   {
      case RGLGCM_SURFACE_POOL_LINEAR:
         gmmFree( rglBuffer->bufferId );
         break;
      case RGLGCM_SURFACE_POOL_NONE:
         break;
      default:
         break;
   }

   rglBuffer->pool = RGLGCM_SURFACE_POOL_NONE;
   rglBuffer->bufferId = GMM_ERROR;
}

static void rglpsAllocateBuffer (void *data)
{
   rglBufferObject *bufferObject = (rglBufferObject*)data;
   rglGcmBufferObject *rglBuffer = (rglGcmBufferObject*)bufferObject->platformBufferObject;

   // free current buffer (if any)
   rglDeallocateBuffer( bufferObject );

   // allocate in GPU memory
   rglBuffer->pool = RGLGCM_SURFACE_POOL_LINEAR;
   rglBuffer->bufferId = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo,
         CELL_GCM_LOCATION_LOCAL, 0, rglBuffer->bufferSize);
   rglBuffer->pitch = 0;

   if ( rglBuffer->bufferId == GMM_ERROR )
      rglBuffer->pool = RGLGCM_SURFACE_POOL_NONE;

   GLuint referenceCount = bufferObject->textureReferences.getCount();
   if ( referenceCount > 0 )
   {
      for ( GLuint i = 0;i < referenceCount;++i )
      {
         rglTexture *texture = bufferObject->textureReferences[i];
         rglGcmTexture *gcmTexture = ( rglGcmTexture * )texture->platformTexture;
         gcmTexture->gpuAddressId = rglBuffer->bufferId;
         gcmTexture->gpuAddressIdOffset = texture->offset;
         texture->revalidate |= RGL_TEXTURE_REVALIDATE_PARAMETERS;
         rglTextureTouchFBOs( texture );
      }
      _CurrentContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
   }
}

int rglpBufferObjectSize(void)
{
   return sizeof(rglGcmBufferObject);
}

GLboolean rglpCreateBufferObject (void *data)
{
   rglBufferObject *bufferObject = (rglBufferObject*)data;
   rglGcmBufferObject *rglBuffer = ( rglGcmBufferObject * )bufferObject->platformBufferObject;

   rglBuffer->pool = RGLGCM_SURFACE_POOL_NONE;
   rglBuffer->bufferId = GMM_ERROR;
   rglBuffer->mapCount = 0;
   rglBuffer->mapAccess = GL_NONE;

   // allocate initial buffer
   rglBuffer->bufferSize = rglPad( bufferObject->size, RGL_BUFFER_OBJECT_BLOCK_SIZE );
   rglpsAllocateBuffer( bufferObject );

   return rglBuffer->bufferId != GMM_ERROR;
}

void rglPlatformDestroyBufferObject (void *data)
{
   rglBufferObject *bufferObject = (rglBufferObject*)data;
   rglDeallocateBuffer( bufferObject );
}

void rglPlatformBufferObjectSetData(void *buf_data, GLintptr offset, GLsizeiptr size, const GLvoid *data, GLboolean tryImmediateCopy)
{
   rglBufferObject *bufferObject = (rglBufferObject*)buf_data;
   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;
   rglGcmBufferObject *rglBuffer = ( rglGcmBufferObject * )bufferObject->platformBufferObject;

   if ( size == bufferObject->size && tryImmediateCopy )
      __builtin_memcpy( gmmIdToAddress( rglBuffer->bufferId ) + offset, data, size );
   else if ( size >= bufferObject->size )
   {
      // reallocate the buffer
      //  To avoid waiting for the GPU to finish with the buffer, just
      //  allocate a whole new one.
      rglBuffer->bufferSize = rglPad( size, RGL_BUFFER_OBJECT_BLOCK_SIZE );
      rglpsAllocateBuffer( bufferObject );

      // copy directly to newly allocated memory
      //  TODO: For GPU destination, should we copy to system memory and
      //  pull from GPU?
      switch ( rglBuffer->pool )
      {
         case RGLGCM_SURFACE_POOL_NONE:
            rglSetError( GL_OUT_OF_MEMORY );
            return;
         default:
            __builtin_memcpy( gmmIdToAddress( rglBuffer->bufferId ), data, size );
            break;
      }
   }
      else
      {
         if ( tryImmediateCopy )
            __builtin_memcpy( gmmIdToAddress( rglBuffer->bufferId ) + offset, data, size );
         else
         {
            // partial buffer write
            //  STREAM and DYNAMIC buffers get transfer via a bounce buffer.
            // copy via bounce buffer
            rglGcmSend( rglBuffer->bufferId, offset, rglBuffer->pitch, ( const char * )data, size );
         }
      }

   // be conservative here. Whenever we write to any Buffer Object, invalidate the vertex cache
   driver->invalidateVertexCache = GL_TRUE;
}

GLvoid rglPlatformBufferObjectCopyData(void *bufferObjectDst, void *bufferObjectSrc)
{
   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;

   rglBufferObject *in_dst = (rglBufferObject*)bufferObjectDst;
   rglBufferObject *in_src = (rglBufferObject*)bufferObjectSrc;
   rglGcmBufferObject* dst = (rglGcmBufferObject*)in_dst->platformBufferObject;
   rglGcmBufferObject* src = (rglGcmBufferObject*)in_src->platformBufferObject;

   rglGcmMemcpy( dst->bufferId, 0, dst->pitch, src->bufferId, 0, src->bufferSize );

   // be conservative here. Whenever we write to any Buffer Object, invalidate the vertex cache
   driver->invalidateVertexCache = GL_TRUE;
}

char *rglPlatformBufferObjectMap (void *data, GLenum access)
{
   rglBufferObject *bufferObject = (rglBufferObject*)data;
   rglGcmBufferObject *rglBuffer = (rglGcmBufferObject*)bufferObject->platformBufferObject;

   if (rglBuffer->mapCount++ == 0)
   {
      if (access == GL_WRITE_ONLY)
      {
         // replace entire buffer
         //  To avoid waiting for the GPU to finish using the buffer,
         //  just allocate a new one.
         rglpsAllocateBuffer( bufferObject );
         if ( rglBuffer->pool == RGLGCM_SURFACE_POOL_NONE )
         {
            rglSetError( GL_OUT_OF_MEMORY );
            return NULL;
         }
      }
      else
         rglpFifoGlFinish(); // must wait in order to read

      rglBuffer->mapAccess = access;

      // count writable mapped buffers
      //  If any buffers are left mapped when a draw is invoked, we must
      //  flush the vertex cache in case VBO data has been modified.
      if ( rglBuffer->mapAccess != GL_READ_ONLY )
      {
         rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;
         ++driver->flushBufferCount;
      }

      // only need to pin the first time we map
      gmmPinId( rglBuffer->bufferId );
   }

   return gmmIdToAddress( rglBuffer->bufferId );
}

GLboolean rglPlatformBufferObjectUnmap (void *data)
{
   rglBufferObject *bufferObject = (rglBufferObject*)data;
   rglGcmBufferObject *rglBuffer = ( rglGcmBufferObject * )bufferObject->platformBufferObject;
   // can't unmap if not mapped

   if ( --rglBuffer->mapCount == 0 )
   {
      // count writable mapped buffers
      //  If any buffers are left mapped when a draw is invoked, we must
      //  flush the vertex cache in case VBO data has been modified.
      if ( rglBuffer->mapAccess != GL_READ_ONLY )
      {
         rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;
         --driver->flushBufferCount;

         // make sure we flush for the next draw
         driver->invalidateVertexCache = GL_TRUE;
      }

      rglBuffer->mapAccess = GL_NONE;

      gmmUnpinId( rglBuffer->bufferId );
   }

   return GL_TRUE;
}

/*============================================================
  PLATFORM FRAME BUFFER OPERATIONS
  ============================================================ */

void rglFBClear( GLbitfield mask )
{
   RGLcontext*	LContext = _CurrentContext;
   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;

   if (!driver->rtValid)
      return;

   GLbitfield newmask = 0;
  
   if ((mask & GL_COLOR_BUFFER_BIT) && driver->rt.colorBufferCount)
      newmask |= RGLGCM_COLOR_BUFFER_BIT;

   if (!newmask)
      return;

   GLbitfield clearMask = newmask;
   if (driver->rt.colorFormat != RGLGCM_ARGB8)
      clearMask &= ~RGLGCM_COLOR_BUFFER_BIT;

   // always use quad clear for colors with MRT
   //  There is one global clear mask for all render targets.  This doesn't
   //  work nicely for all render target combinations, e.g. only the first
   //  and last targets enabled.  Quad clear works because color mask is
   //  per target.
   //
   //  TODO: Clear could be used if the enabled render targets are
   //  contiguous from 0, i.e. {0,1}, {0,1,2}, {0,1,2,3}.  If this is done,
   //  parallel changes need to made in rglValidateWriteMask because we
   //  bypass calling rglGcmFifoGlColorMask there and the mask used by nv_glClear
   //  is not updated.
   if ( driver->rt.colorBufferCount > 1 )
      clearMask &= ~RGLGCM_COLOR_BUFFER_BIT;

   if ( clearMask )
   {
      rglGcmFifoGlClearColor( 0, 0, 0, 0);

      rglGcmFifoGlClear( clearMask );
      newmask &= ~clearMask;
   }

   if ( newmask )
   {
      // draw a quad to erase everything.

      // disable/set up a lot of states
      //

      static float rglClearVertexBuffer[4*3] __attribute__(( aligned( RGL_ALIGN_FAST_TRANSFER ) ) ) =
      {
         -1.f, -1.f, 0.f,
         -1.f, 1.f, 0.f,
         1.f, -1.f, 0.f,
         1.f, 1.f, 0.f,
      };

      GLuint bufferId = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo, 
            CELL_GCM_LOCATION_LOCAL, 0, sizeof(rglClearVertexBuffer));

      __builtin_memcpy(gmmIdToAddress(bufferId), rglClearVertexBuffer, sizeof(rglClearVertexBuffer));
      rglGcmFifoGlVertexAttribPointer( 0, 3, RGLGCM_FLOAT, RGLGCM_FALSE, 3*sizeof( GLfloat ), 1, 0, gmmIdToOffset(bufferId) );
      RGLBIT_TRUE( LContext->attribs->DirtyMask, 0 );

      for (int i = 1; i < RGL_MAX_VERTEX_ATTRIBS; ++i)
      {
         rglGcmFifoGlVertexAttribPointer( i, 0, RGLGCM_FLOAT, 0, 0, 0, 0, 0 );
         RGLBIT_TRUE( LContext->attribs->DirtyMask, i );
      }
      int clearcolor = 0;
      rglGcmFifoGlVertexAttrib4fv( RGL_ATTRIB_PRIMARY_COLOR_INDEX, ( GLfloat* )&clearcolor );

      LContext->needValidate |= RGL_VALIDATE_WRITE_MASK | RGL_VALIDATE_FRAGMENT_PROGRAM;

      gmmFree( bufferId );
   }

   rglGcmFifoGlFlush();
}

/*============================================================
  PLATFORM FRAMEBUFFER
  ============================================================ */

rglFramebuffer* rglCreateFramebuffer (void)
{
   rglFramebuffer* framebuffer = new rglPlatformFramebuffer();
   return framebuffer;
}

void rglDestroyFramebuffer (void *data)
{
   rglFramebuffer *framebuffer = (rglFramebuffer*)data;

   if(framebuffer)
      delete framebuffer;
}

GLenum rglPlatformFramebufferCheckStatus (void *data)
{
   rglFramebuffer *framebuffer = (rglFramebuffer*)data;
   RGLcontext* LContext = _CurrentContext;

   GLuint nBuffers = 0;	// number of attached buffers
   int width = 0;
   int height = 0;
   GLboolean sizeMismatch = GL_FALSE;

   // record attached images
   //  We have to verify that no image is attached more than once.  The
   //  array is sized for color attachments plus depth and stencil.
   rglImage* image[RGL_MAX_COLOR_ATTACHMENTS + 2] = {0};

   // test colors
   GLuint colorFormat = 0;
   for ( int i = 0; i < RGL_MAX_COLOR_ATTACHMENTS; ++i )
   {
      rglTexture* colorTexture = NULL;
      GLuint colorFace = 0;
      rglFramebufferGetAttachmentTexture(
            LContext,
            &framebuffer->color[i],
            &colorTexture,
            &colorFace );
      // TODO: Complete texture may not be required.
      if (colorTexture != NULL)
      {
         if (colorTexture->referenceBuffer && !colorTexture->isRenderTarget)
            return GL_FRAMEBUFFER_UNSUPPORTED_OES;

         // all attachments must have the same dimensions
         image[nBuffers] = colorTexture->image;
         if (( width && width != image[nBuffers]->width ) ||
               ( height && height != image[nBuffers]->height ) )
            sizeMismatch = GL_TRUE;

         width = image[nBuffers]->width;
         height = image[nBuffers]->height;

         // all color attachments need the same format
         if ( colorFormat && colorFormat != image[nBuffers]->internalFormat )
            return GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES;

         colorFormat = image[nBuffers]->internalFormat;

         ++nBuffers;
      }
   }

   // check for supported color format
   if ( nBuffers )
   {
      if ( !rglIsDrawableColorFormat( colorFormat ) )
         return GL_FRAMEBUFFER_UNSUPPORTED_OES;

      switch ( colorFormat )
      {
         case RGLGCM_ARGB8:
         case RGLGCM_RGB5_A1_SCE:
         case RGLGCM_RGB565_SCE:
         case RGLGCM_FLOAT_R32:
            break;
         default:
            return GL_FRAMEBUFFER_UNSUPPORTED_OES;
      }
   }

   // at least once attachment is required
   if ( nBuffers == 0 )
      return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES;

   // verify no image is attached more than once
   //  This is an n-squared algorithm - n*log(n) is possible but
   //  probably not necessary (or even faster).
   for ( GLuint i = 0; i < nBuffers; ++i )
      for ( GLuint j = i + 1; j < nBuffers; ++j )
         if ( image[i] == image[j] )
            return GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_OES;

   return GL_FRAMEBUFFER_COMPLETE_OES;
}

// Calculate byte offset for given texture layout, number of faces, and levels
static GLuint rglGetGcmImageOffset (void *data, GLuint face, GLuint level)
{
   rglGcmTextureLayout *layout = (rglGcmTextureLayout*)data;
   GLuint bytes = 0;
   GLuint faceAlign = layout->pitch ? 1 : 128;

   GLuint minWidth = 1;
   GLuint minHeight = 1;

   for (GLuint f = 0; f < layout->faces; ++f)
   {
      GLuint width = layout->baseWidth;
      GLuint height = layout->baseHeight;
      GLuint depth = layout->baseDepth;
      for ( GLuint i = 0;i < layout->levels; ++i )
      {
         if ((level == i) && (face == f))
            return bytes;

         width = MAX( minWidth, width );
         height = MAX( minHeight, height );
         depth = MAX( 1U, depth );

         if ( !layout->pitch ) 
            bytes += layout->pixelBits * width * height * depth / 8;
         else 
            bytes += height * depth * layout->pitch;

         width >>= 1;
         height >>= 1;
         depth >>= 1;
      }

      bytes = rglPad( bytes, faceAlign );
   }
   return 0;
}

void rglPlatformFramebuffer::validate (void *data)
{
   RGLcontext *LContext = (RGLcontext*)data;
   complete = (rglPlatformFramebufferCheckStatus(this) == GL_FRAMEBUFFER_COMPLETE_OES);

   if (!complete)
      return;

   GLuint width = RGLGCM_MAX_RT_DIMENSION;
   GLuint height = RGLGCM_MAX_RT_DIMENSION;

   GLuint xscale = 1;
   GLuint yscale = 1;

   // color
   rt.colorBufferCount = 0;
   rt.colorFormat = RGLGCM_NONE;
   GLuint defaultPitch = 0;
   GLuint defaultId = GMM_ERROR;
   GLuint defaultIdOffset = 0;

   for ( int i = 0; i < RGLGCM_SETRENDERTARGET_MAXCOUNT; ++i )
   {
      // get the texture and face
      rglTexture* colorTexture = NULL;
      GLuint face = 0;
      rglFramebufferGetAttachmentTexture( LContext, &color[i], &colorTexture, &face );
      if ( colorTexture == NULL ) continue;

      rglGcmTexture* nvTexture = ( rglGcmTexture * )colorTexture->platformTexture;

      // make sure texture is resident in a supported layout
      //  Some restrictions are added if a texture is used as a
      //  render target:
      //
      //  - no swizzled semifat or fat formats
      //  - no swizzled smaller than 16x16
      //  - no mipmapped cube maps in tiled memory
      //  - no cube maps with height not a multiple of 16 in tiled
      //    memory
      //
      //  We may need to reallocate the texture if any of these
      //  are true.
      //
      //  TODO: Measure time spent here and optimize if indicated.
      if ( !colorTexture->isRenderTarget )
      {
         colorTexture->isRenderTarget = GL_TRUE;
         colorTexture->revalidate |= RGL_TEXTURE_REVALIDATE_LAYOUT;
      }
      rglPlatformValidateTextureResources( colorTexture );
      colorTexture->image->dataState = RGL_IMAGE_DATASTATE_GPU;

      // set the render target
      rt.colorId[i] = nvTexture->gpuAddressId;
      rt.colorIdOffset[i] = nvTexture->gpuAddressIdOffset + rglGetGcmImageOffset( &nvTexture->gpuLayout, face, 0 );
      rt.colorPitch[i] = nvTexture->gpuLayout.pitch ? nvTexture->gpuLayout.pitch : nvTexture->gpuLayout.pixelBits * nvTexture->gpuLayout.baseWidth / 8;

      width = MIN( width, nvTexture->gpuLayout.baseWidth );
      height = MIN( height, nvTexture->gpuLayout.baseHeight );
      rt.colorFormat = nvTexture->gpuLayout.internalFormat;
      rt.colorBufferCount = i + 1;
      defaultId = rt.colorId[i];
      defaultIdOffset = rt.colorIdOffset[i];
      defaultPitch = rt.colorPitch[i];
   }

   // framebuffer dimensions are the intersection of attachments
   rt.width = width / xscale;
   rt.height = height / yscale;

   rt.yInverted = RGLGCM_FALSE;
   rt.xOffset = 0;
   rt.yOffset = 0;
   needValidate = GL_FALSE;
}

// set render targets
void rglValidateFramebuffer (void)
{
   RGLdevice *LDevice = _CurrentDevice;
   rglGcmDevice *gcmDevice = ( rglGcmDevice * )LDevice->platformDevice;

   RGLcontext* LContext = _CurrentContext;
   rglGcmDriver *gcmDriver = (rglGcmDriver*)_CurrentDevice->rasterDriver;

   // reset buffer data
   gcmDriver->rtValid = GL_FALSE;
   // get buffer parameters
   //  This may come from a framebuffer_object or the default framebuffer.

   if (LContext->framebuffer)
   {
      rglPlatformFramebuffer* framebuffer = static_cast<rglPlatformFramebuffer *>( rglGetFramebuffer( LContext, LContext->framebuffer ) );

      if (framebuffer->needValidate)
         framebuffer->validate( LContext );

      gcmDriver->rt = framebuffer->rt;
   }
   else	// use default framebuffer
      gcmDriver->rt = gcmDevice->rt;

   gcmDriver->rtValid = GL_TRUE;

   // update GPU configuration
   rglGcmFifoGlSetRenderTarget( &gcmDriver->rt );

   LContext->needValidate &= ~RGL_VALIDATE_FRAMEBUFFER;
   LContext->needValidate |= RGL_VALIDATE_VIEWPORT | RGL_VALIDATE_SCISSOR_BOX
      | RGL_VALIDATE_WRITE_MASK;
}

/*============================================================
  PLATFORM RASTER
  ============================================================ */

#define RGL_ATTRIB_BUFFER_ALIGNMENT 16

// maximum size for drawing data
#define RGLGCM_MAX_VERTEX_BUFFER_SIZE (2 << 20)
#define RGLGCM_MAX_INDEX_BUFFER_SIZE (1 << 20)

// Initialize the driver and setup the fixed function pipeline 
// shader and needed connections between GL state and the shader
void *rglPlatformRasterInit (void)
{
   rglpFifoGlFinish();
   rglGcmDriver *driver = ( rglGcmDriver * )malloc( sizeof( rglGcmDriver ) );
   memset( driver, 0, sizeof( rglGcmDriver ) );

   driver->rt.yInverted = RGLGCM_TRUE;

   driver->invalidateVertexCache = GL_FALSE;

   driver->flushBufferCount = 0;

   // [YLIN] Make it 16 byte align
   driver->sharedVPConstants = ( char * )memalign( 16, 4 * sizeof( float ) * RGL_MAX_VP_SHARED_CONSTANTS );
   driver->sharedFPConstantsId = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo,
         CELL_GCM_LOCATION_LOCAL, 0, 4 * sizeof(float) * RGL_MAX_FP_SHARED_CONSTANTS);
   return driver;
}

// Destroy the driver, and free all its used memory
void rglPlatformRasterExit (void *data)
{
   rglGcmDriver *driver = (rglGcmDriver*)data;

   gmmFree( driver->sharedFPConstantsId );
   free( driver->sharedVPConstants );

   if (driver)
      free(driver);
}

void rglDumpFifo (char *name);

// Fast rendering path called by several glDraw calls:
//   glDrawElements, glDrawRangeElements, glDrawArrays
// Slow rendering calls this function also, though it must also perform various
// memory setup operations first
void rglPlatformDraw (void *data)
{
   rglDrawParams *dparams = (rglDrawParams*)data;
   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;

   if (RGL_UNLIKELY(!driver->rtValid))
      return;

   // check for any writable mapped buffers
   if ( driver->flushBufferCount != 0 )
      driver->invalidateVertexCache = GL_TRUE;

   GLboolean isMain = 0;
   GLuint gpuOffset = GMM_ERROR;

   uint32_t totalXfer = 0;
   for ( GLuint i = 0; i < RGL_MAX_VERTEX_ATTRIBS; ++i )
      totalXfer += dparams->attribXferSize[i];

   gpuOffset = rglValidateAttributesSlow( dparams, &isMain );

   if ( driver->invalidateVertexCache )
   {
      driver->invalidateVertexCache = GL_FALSE;
      GCM_FUNC_NO_ARGS( cellGcmSetInvalidateVertexCache );
   }

   // if fpLoadProgramId is in main memory need set the CELL_GCM_LOCATION_MAIN
   if ( gmmIdIsMain(driver->fpLoadProgramId) )
   {
      GCM_FUNC( cellGcmSetUpdateFragmentProgramParameterLocation, 
            gmmIdToOffset( driver->fpLoadProgramId ) + driver->fpLoadProgramOffset, CELL_GCM_LOCATION_MAIN );
   }
   else
   {
      GCM_FUNC( cellGcmSetUpdateFragmentProgramParameter, 
            gmmIdToOffset( driver->fpLoadProgramId ) + driver->fpLoadProgramOffset );
   }

   rglGcmFifoGlDrawArrays(( rglGcmEnum )dparams->mode, dparams->firstVertex, dparams->vertexCount );
}


// Set up the current fragment program on hardware
void rglValidateFragmentProgram (void)
{
   RGLcontext*	LContext = _CurrentContext;
   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;
   _CGprogram *program = LContext->BoundFragmentProgram;

   // params are set directly in the GPU memory, so there is nothing to be done here.
   rglSetNativeCgFragmentProgram( program );
   driver->fpLoadProgramId = program->loadProgramId;
   driver->fpLoadProgramOffset = program->loadProgramOffset;
}

// must always call this before rglPlatformDraw() to setup rglDrawParams
GLboolean rglPlatformRequiresSlowPath (void *data, const GLenum indexType, uint32_t indexCount)
{
   rglDrawParams *dparams = (rglDrawParams*)data;
   RGLcontext* LContext = _CurrentContext;
   rglAttributeState* as = LContext->attribs;

   // are any enabled attributes on the client-side?
   const GLuint clientSideMask = as->EnabledMask & ~as->HasVBOMask;
   if ( RGL_UNLIKELY( clientSideMask ) )
   {
      // determine transfer buffer requirements for client-side attributes
      for ( int i = 0; i < RGL_MAX_VERTEX_ATTRIBS; ++i )
      {
         if ( clientSideMask & ( 1 << i ) )
         {
            rglAttribute* attrib = as->attrib + i;
            const GLuint freq = attrib->frequency;
            GLuint count = ( (dparams->firstVertex + dparams->vertexCount) + freq - 1 ) / freq;

            const GLuint numBytes = attrib->clientStride * count;
            dparams->attribXferOffset[i] = dparams->xferTotalSize;
            dparams->attribXferSize[i] = numBytes;

            const GLuint numBytesPadded = rglPad( numBytes, 128 );
            dparams->xferTotalSize += numBytesPadded;
            dparams->attribXferTotalSize += numBytesPadded;
         }
         else
         {
            dparams->attribXferOffset[i] = 0;
            dparams->attribXferSize[i] = 0;
         }
      }
   }

   return GL_FALSE;	// we are finally qualified for the fast path
}

void rglPlatformRasterFlush (void)
{
   rglGcmFifoGlFlush();
}

void rglpFifoGlFinish (void)
{
   GCM_FUNC_NO_ARGS( cellGcmSetInvalidateVertexCache );
   rglGcmFifoFinish( &rglGcmState_i.fifo );
}

// validates attributes for specified draw paramaters
// returns pointer to index buffer
GLuint rglValidateAttributesSlow (void *data, GLboolean *isMain)
{
   rglDrawParams *dparams = (rglDrawParams*)data;
   RGLcontext*	LContext = _CurrentContext;
   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;
   rglAttributeState* as = LContext->attribs;

   // allocate upload transfer buffer if necessary
   //  The higher level bounce buffer allocator is used, which means that
   //  the buffer will automatically be freed after all RGLGCM calls up to
   //  the next allocation have finished.
   void* xferBuffer = NULL;
   GLuint xferId = GMM_ERROR;
   GLuint VBOId = GMM_ERROR;
   GLuint gpuOffset;
   if ( RGL_UNLIKELY( dparams->xferTotalSize ) )
   {
      xferId = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo,
            CELL_GCM_LOCATION_LOCAL, 0, dparams->xferTotalSize);
      xferBuffer = gmmIdToAddress(xferId);
   }


   // which attributes are known to need updating?
   // (due to being dirty or enabled client-side arrays)
   unsigned int needsUpdateMask = (as->DirtyMask | (as->EnabledMask & ~as->HasVBOMask));

   // for any remaining attributes that need updating, do it now.
   if(needsUpdateMask)
   {
      for (GLuint i = 0; i < RGL_MAX_VERTEX_ATTRIBS; ++i)
      {
         // skip this attribute if not needing update
         if (!RGLBIT_GET( needsUpdateMask, i))
            continue;

         rglAttribute* attrib = as->attrib + i;
         if ( RGLBIT_GET( as->EnabledMask, i ) )
         {
            const GLsizei stride = attrib->clientStride;
            const GLuint freq = attrib->frequency;

            if ( RGL_UNLIKELY( dparams->attribXferSize[i] ) )
            {
               // attribute data is client side, need to transfer

               // don't transfer data that is not going to be used, from 0 to first*stride
               GLuint offset = ( dparams->firstVertex / freq ) * stride;

               char * b = ( char * )xferBuffer + dparams->attribXferOffset[i];
               __builtin_memcpy(b + offset,
                     ( char*)attrib->clientData + offset,
                     dparams->attribXferSize[i] - offset);

               // draw directly from bounce buffer
               *isMain = gmmIdIsMain(xferId);
               gpuOffset = gmmIdToOffset(xferId) + (b - ( char * )xferBuffer);

            }
            else
            {
               // attribute data in VBO, clientData is offset.
               VBOId = rglGcmGetBufferObjectOrigin( attrib->arrayBuffer );
               *isMain = gmmIdIsMain(VBOId);
               gpuOffset = gmmIdToOffset(VBOId)
                  + (( const GLubyte* )attrib->clientData - ( const GLubyte* )NULL );
            }

            rglGcmFifoGlVertexAttribPointer( i, attrib->clientSize,
                  ( rglGcmEnum )attrib->clientType, attrib->normalized,
                  stride, freq, *isMain, gpuOffset );
         }
         else
         {
            // attribute is disabled
            rglGcmFifoGlVertexAttribPointer( i, 0, RGLGCM_FLOAT, 0, 0, 0, 0, 0 );
            rglGcmFifoGlVertexAttrib4fv( i, attrib->value );
         }
      }
      driver->invalidateVertexCache = GL_TRUE;
   }
   as->DirtyMask = 0;	// all attributes are now clean

   // validate index buffer
   GLuint indexOffset = 0;

   if ( xferId != GMM_ERROR )
      gmmFree( xferId );

   return indexOffset;
}

/*============================================================
  PLATFORM TEXTURE
  ============================================================ */

// Calculate required size in bytes for given texture layout
GLuint rglGetGcmTextureSize (void *data)
{
   rglGcmTextureLayout *layout = (rglGcmTextureLayout*)data;
   GLuint bytesNeeded = 0;
   GLuint faceAlign = layout->pitch ? 1 : 128;

   GLuint minWidth = 1;
   GLuint minHeight = 1;
   for ( GLuint face = 0;face < layout->faces;++face )
   {
      GLuint width = layout->baseWidth;
      GLuint height = layout->baseHeight;
      GLuint depth = layout->baseDepth;
      for ( GLuint i = 0;i < layout->levels; ++i )
      {
         width = MAX( minWidth, width );
         height = MAX( minHeight, height );
         depth = MAX( 1U, depth );

         if ( !layout->pitch )
            bytesNeeded += layout->pixelBits * width * height * depth / 8;
         else
            bytesNeeded += height * depth * layout->pitch;

         width >>= 1;
         height >>= 1;
         depth >>= 1;
      }

      bytesNeeded = rglPad( bytesNeeded, faceAlign );
   }

   return bytesNeeded;
}


// Get size of a texture
int rglPlatformTextureSize (void)
{
   return sizeof(rglGcmTexture);
}

// Calculate pitch for a texture
static GLuint _getTexturePitch (const void *data)
{
   const rglTexture *texture = (const rglTexture*)data;
   return rglPad( rglGetStorageSize( texture->image->format, texture->image->type, texture->image->width, 1, 1 ), 64 ); // TransferVid2Vid needs 64byte pitch alignment
}

// Return maximum number of texture image units
int rglPlatformTextureMaxUnits (void)
{
   return RGLGCM_MAX_TEXIMAGE_COUNT;
}

// Create a gcm texture by initializing memory to 0
void rglPlatformCreateTexture (void *data)
{
   rglTexture *texture = (rglTexture*)data;
   rglGcmTexture *gcmTexture = ( rglGcmTexture * )texture->platformTexture;
   memset( gcmTexture, 0, sizeof( rglGcmTexture ) );
   gcmTexture->gpuAddressId = GMM_ERROR;
}


void rglPlatformFreeGcmTexture (void *data);

// Destroy a texture by freeing a gcm texture and an associated PBO
void rglPlatformDestroyTexture (void *data)
{
   rglTexture *texture = (rglTexture*)data;

   if (!texture->referenceBuffer)
      rglPlatformFreeGcmTexture(texture);

   rglTextureTouchFBOs(texture);
}

// Get size of texture in GPU layout
static inline GLuint rglPlatformTextureGetGPUSize(const void *data)
{
   const rglTexture *texture = (const rglTexture*)data;
   rglGcmTexture *gcmTexture = ( rglGcmTexture * )texture->platformTexture;
   return rglGetGcmTextureSize( &gcmTexture->gpuLayout );
}

// Drop a texture from the GPU memory by detaching it from a PBO
void rglPlatformDropTexture (void *data)
{
   rglTexture *texture = (rglTexture*)data;
   rglGcmTexture *gcmTexture = ( rglGcmTexture * )texture->platformTexture;

   if (gcmTexture->pool != RGLGCM_SURFACE_POOL_NONE)
      rglPlatformFreeGcmTexture( texture );

   gcmTexture->pool = RGLGCM_SURFACE_POOL_NONE;
   gcmTexture->gpuAddressId = GMM_ERROR;
   gcmTexture->gpuAddressIdOffset = 0;
   gcmTexture->gpuSize = 0;
   texture->revalidate |= RGL_TEXTURE_REVALIDATE_IMAGES;
   rglTextureTouchFBOs( texture );
}

// Drop unbound textures from the GPU memory
// This is kind of slow, but we hit a slow path anyway.
//  If the pool argument is not RGLGCM_SURFACE_POOL_NONE, then only textures
//  in the specified pool will be dropped.
void rglPlatformDropUnboundTextures (GLenum pool)
{
   RGLcontext*	LContext = _CurrentContext;
   GLuint i, j;

   for (i = 0; i < LContext->textureNameSpace.capacity; ++i)
   {
      GLboolean bound = GL_FALSE;
      rglTexture *texture = ( rglTexture * )LContext->textureNameSpace.data[i];

      if (!texture || (texture->referenceBuffer != 0))
         continue;

      // check if bound
      for ( j = 0;j < RGL_MAX_TEXTURE_IMAGE_UNITS;++j )
      {
         rglTextureImageUnit *tu = LContext->TextureImageUnits + j;
         if ( tu->bound2D == i)
         {
            bound = GL_TRUE;
            break;
         }
      }
      if ( bound )
         continue;

      rglGcmTexture *gcmTexture = ( rglGcmTexture * )texture->platformTexture;

      // check pool
      if ( pool != RGLGCM_SURFACE_POOL_NONE &&
            pool != gcmTexture->pool )
         continue;

      rglPlatformDropTexture( texture );
   }
}

// Drop filitering mode for FP32 texture
static inline GLenum unFilter( GLenum filter )
{
   GLenum newFilter;
   switch ( filter )
   {
      case GL_NEAREST:
      case GL_LINEAR:
         newFilter = GL_NEAREST;
         break;
      case GL_NEAREST_MIPMAP_NEAREST:
      case GL_NEAREST_MIPMAP_LINEAR:
      case GL_LINEAR_MIPMAP_NEAREST:
      case GL_LINEAR_MIPMAP_LINEAR:
         newFilter = GL_NEAREST_MIPMAP_NEAREST;
         break;
      default:
         newFilter = GL_NEAREST;
   }
   return newFilter;
}

// Choose a texture layout and store it to newLayout, based on texture's filtering mode, swizzling, and size
void rglPlatformChooseGPUFormatAndLayout(
      const void *data, GLboolean forceLinear,
      GLuint pitch, rglGcmTextureLayout* newLayout )
{
   const rglTexture *texture = (const rglTexture*)data;
   rglImage *image = texture->image + texture->baseLevel;

#ifndef HAVE_RGL_2D
   GLuint levels = rglLog2( MAX( MAX( image->width, image->height ), image->depth ) ) + 1;
   levels = MIN( levels, texture->maxLevel + 1 );

   // if we don't want mipmapping, but still have valid mipmaps, load all the mipmaps.
   // This is to avoid a big cost when switching from mipmaps to non-mipmaps.
   if (( texture->minFilter == GL_LINEAR ) || ( texture->minFilter == GL_NEAREST ) )
      levels = 1;
#else
   GLuint levels = 1;
#endif

   newLayout->levels = levels;
   newLayout->faces = texture->faceCount;
   newLayout->baseWidth = image->width;
   newLayout->baseHeight = image->height;
   newLayout->baseDepth = image->depth;
   newLayout->internalFormat = ( rglGcmEnum )image->internalFormat;
   newLayout->pixelBits = rglPlatformGetBitsPerPixel( newLayout->internalFormat );
   newLayout->pitch = pitch ? pitch : _getTexturePitch(texture);
}

// texture strategy actions
//  A texture strategy is a sequence of actions represented by these tokens.
//  RGL_TEXTURE_STRATEGY_END must be the last token in any strategy.
enum rglTextureStrategy {
   RGL_TEXTURE_STRATEGY_END,			// allocation failed, give up
   RGL_TEXTURE_STRATEGY_FORCE_LINEAR,
   RGL_TEXTURE_STRATEGY_TILED_ALLOC,
   RGL_TEXTURE_STRATEGY_TILED_CLEAR,
   RGL_TEXTURE_STRATEGY_UNTILED_ALLOC,
   RGL_TEXTURE_STRATEGY_UNTILED_CLEAR,
   RGL_TEXTURE_STRATEGY_SYSTEM_ALLOC,
   RGL_TEXTURE_STRATEGY_SYSTEM_CLEAR,	// XXX probably not useful
};

static enum rglTextureStrategy tiledGPUStrategy[] =
{
   RGL_TEXTURE_STRATEGY_TILED_ALLOC,	// try tiled alloction
   RGL_TEXTURE_STRATEGY_FORCE_LINEAR,
   RGL_TEXTURE_STRATEGY_UNTILED_ALLOC,	// if failure, try linear allocation
   RGL_TEXTURE_STRATEGY_UNTILED_CLEAR,	// if failure, drop linear textures
   RGL_TEXTURE_STRATEGY_UNTILED_ALLOC,	// try linear again
   RGL_TEXTURE_STRATEGY_END,			// give up
};
static enum rglTextureStrategy linearGPUStrategy[] =
{
   RGL_TEXTURE_STRATEGY_FORCE_LINEAR,
   RGL_TEXTURE_STRATEGY_UNTILED_ALLOC,
   RGL_TEXTURE_STRATEGY_UNTILED_CLEAR,
   RGL_TEXTURE_STRATEGY_UNTILED_ALLOC,
   RGL_TEXTURE_STRATEGY_END,
};
static enum rglTextureStrategy swizzledGPUStrategy[] =
{
   RGL_TEXTURE_STRATEGY_UNTILED_ALLOC,
   RGL_TEXTURE_STRATEGY_UNTILED_CLEAR,
   RGL_TEXTURE_STRATEGY_UNTILED_ALLOC,
   RGL_TEXTURE_STRATEGY_END,
};
static enum rglTextureStrategy linearSystemStrategy[] =
{
   RGL_TEXTURE_STRATEGY_FORCE_LINEAR,
   RGL_TEXTURE_STRATEGY_SYSTEM_ALLOC,
   RGL_TEXTURE_STRATEGY_END,
};
static enum rglTextureStrategy swizzledSystemStrategy[] =
{
   RGL_TEXTURE_STRATEGY_SYSTEM_ALLOC,
   RGL_TEXTURE_STRATEGY_END,
};

// Reallocate texture based on usage, pool system, and strategy
void rglPlatformReallocateGcmTexture (void *data)
{
   rglTexture *texture = (rglTexture*)data;
   rglGcmTexture *gcmTexture = ( rglGcmTexture * )texture->platformTexture;

   // select the allocation strategy
   enum rglTextureStrategy *step = NULL;
   switch ( texture->usage )
   {
      case GL_TEXTURE_TILED_GPU_SCE:
         step = tiledGPUStrategy;
         break;
      case GL_TEXTURE_LINEAR_GPU_SCE:
         step = linearGPUStrategy;
         break;
      case GL_TEXTURE_SWIZZLED_GPU_SCE:
         step = swizzledGPUStrategy;
         break;
      case GL_TEXTURE_LINEAR_SYSTEM_SCE:
         step = linearSystemStrategy;
         break;
      case GL_TEXTURE_SWIZZLED_SYSTEM_SCE:
         step = swizzledSystemStrategy;
         break;
      default:
         step = swizzledGPUStrategy;
         break;
   }

   GLuint size = 0;
   GLuint id = GMM_ERROR;

   // allow swizzled format unless explicitly disallowed
   //  PBO textures cannot be swizzled.
   GLboolean forceLinear = GL_FALSE;

   const rglGcmTextureLayout currentLayout = gcmTexture->gpuLayout;
   const GLuint currentSize = gcmTexture->gpuSize;

   // process strategy
   GLboolean done = GL_FALSE;
   while ( !done )
   {
      rglGcmTextureLayout newLayout;

      switch ( *step++ )
      {
         case RGL_TEXTURE_STRATEGY_FORCE_LINEAR:
            forceLinear = GL_TRUE;
            break;
         case RGL_TEXTURE_STRATEGY_UNTILED_ALLOC:
            // get layout and size compatible with this pool
            rglPlatformChooseGPUFormatAndLayout( texture, forceLinear, 0, &newLayout );
            size = rglGetGcmTextureSize( &newLayout );

            // determine if current allocation already works
            //  If the current allocation has the right size and pool, we
            //  don't have to do anything.  If not, we only drop from the
            //  target pool because we may reuse the allocation from a
            //  different pool in a later step.
            if ( gcmTexture->pool == RGLGCM_SURFACE_POOL_LINEAR )
            {
               if ( currentSize >= size && newLayout.pitch == currentLayout.pitch )
               {
                  gcmTexture->gpuLayout = newLayout;
                  done = GL_TRUE;
               }
               else
                  rglPlatformDropTexture( texture );
            }

            if ( !done )
            {
               // allocate in the specified pool
               id = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo, 
                     CELL_GCM_LOCATION_LOCAL,
                     0,
                     size);
               if ( id != GMM_ERROR )
               {
                  // drop old allocation
                  if ( gcmTexture->pool != RGLGCM_SURFACE_POOL_NONE )
                     rglPlatformDropTexture( texture );

                  // set new
                  gcmTexture->pool = RGLGCM_SURFACE_POOL_LINEAR;
                  gcmTexture->gpuAddressId = id;
                  gcmTexture->gpuAddressIdOffset = 0;
                  gcmTexture->gpuSize = size;
                  gcmTexture->gpuLayout = newLayout;

                  done = GL_TRUE;
               }
            }
            break;
         case RGL_TEXTURE_STRATEGY_UNTILED_CLEAR:
            rglPlatformDropUnboundTextures( RGLGCM_SURFACE_POOL_LINEAR );
            break;
         case RGL_TEXTURE_STRATEGY_END:
            rglSetError( GL_OUT_OF_MEMORY );
            done = GL_TRUE;
            break;
         default:
            break;
      }
   } // while loop for allocation strategy steps
   rglTextureTouchFBOs( texture );
}

// Free memory pooled by a GCM texture
void rglPlatformFreeGcmTexture (void *data)
{
   rglTexture *texture = (rglTexture*)data;
   rglGcmTexture *gcmTexture = ( rglGcmTexture * )texture->platformTexture;
   switch ( gcmTexture->pool )
   {
      case RGLGCM_SURFACE_POOL_LINEAR:
         gmmFree( gcmTexture->gpuAddressId );
         break;
      case RGLGCM_SURFACE_POOL_SYSTEM:
         gmmFree( gcmTexture->gpuAddressId );
         break;
      case RGLGCM_SURFACE_POOL_TILED_COLOR:
         rglGcmFreeTiledSurface( gcmTexture->gpuAddressId );
         break;
      case RGLGCM_SURFACE_POOL_NONE:
         break;
   }


   gcmTexture->gpuAddressId = GMM_ERROR;
   gcmTexture->gpuAddressIdOffset = 0;
   gcmTexture->gpuSize = 0;
}

// Upload texure from host memory to GPU memory
void rglPlatformUploadTexture (void *data)
{
   rglTexture *texture = (rglTexture*)data;
   rglGcmTexture *gcmTexture = ( rglGcmTexture * )texture->platformTexture;
   rglGcmTextureLayout *layout = &gcmTexture->gpuLayout;

   const GLuint pixelBytes = layout->pixelBits / 8;

   // host texture requires sync

   // create surface descriptors for image transfer
   rglGcmSurface src = {
            source:		RGLGCM_SURFACE_SOURCE_TEMPORARY,
            width:		0,		// replaced per image
            height:		0,		// replaced per image
            bpp:		pixelBytes,
            pitch:		0,		// replaced per image
            format:		layout->internalFormat,
            pool:		RGLGCM_SURFACE_POOL_LINEAR,		// via bounce buffer
            ppuData:     NULL,   // replaced per image
            dataId:      GMM_ERROR,
            dataIdOffset:0,
   };

   rglGcmSurface dst = {
            source:		RGLGCM_SURFACE_SOURCE_TEXTURE,
            width:		0,		// replaced per image
            height:		0,		// replaced per image
            bpp:		pixelBytes,
            pitch:		layout->pitch,
            format:		layout->internalFormat,
            pool:		gcmTexture->pool,
            ppuData:     NULL,   // replaced per image
            dataId:      GMM_ERROR,
            dataIdOffset:0,
   };

   // use a bounce buffer to transfer to GPU
   GLuint bounceBufferId = GMM_ERROR;

   // check if upload is needed for this image
   rglImage *image = texture->image;

   if ( image->dataState == RGL_IMAGE_DATASTATE_HOST )
   {
      // determine image offset from base address
      // TODO: compute all offsets at once for efficiency
      //  This is the offset in bytes for this face/image from the
      //  texture base address.
      const GLuint dataOffset = rglGetGcmImageOffset( layout, 0, 0 );

      // set source pixel buffer
      src.ppuData = image->data;

      // lazy allocation of bounce buffer
      if ( bounceBufferId == GMM_ERROR && layout->baseDepth == 1 )
         bounceBufferId = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo,
               CELL_GCM_LOCATION_LOCAL, 0, gcmTexture->gpuSize);

      if ( bounceBufferId != GMM_ERROR )
      {
         // copy image to bounce buffer
         src.dataId = bounceBufferId;
         src.dataIdOffset = dataOffset;

         // NPOT DXT
         __builtin_memcpy( gmmIdToAddress( src.dataId ) + dataOffset, 
               image->data, image->storageSize );
      }

      // use surface copy functions
      src.width = image->width;
      src.height = image->height;
      src.pitch = pixelBytes * src.width;

      dst.width = src.width;
      dst.height = image->height;
      dst.dataId = gcmTexture->gpuAddressId;
      dst.dataIdOffset = gcmTexture->gpuAddressIdOffset + dataOffset;

      GLuint offsetHeight = 0;

      if(dst.pitch)
      {
         // linear (not swizzled)
         //  The tiled linear format requires that render
         //  targets be aligned to 8*pitch from the start of
         //  the tiled region.
         offsetHeight = ( dataOffset / dst.pitch ) % 8;
         dst.height += offsetHeight;
         dst.dataIdOffset -= offsetHeight * dst.pitch;
      }

      rglGcmCopySurface(
            &src, 0, 0,
            &dst, 0, offsetHeight,
            src.width, src.height,
            GL_TRUE );	// don't bypass GPU pipeline

      // free CPU copy of data
      rglImageFreeCPUStorage( image );
      image->dataState |= RGL_IMAGE_DATASTATE_GPU;
   } // newer data on host

   if ( bounceBufferId != GMM_ERROR )
      gmmFree( bounceBufferId );

   GCM_FUNC( cellGcmSetInvalidateTextureCache, CELL_GCM_INVALIDATE_TEXTURE );
}

// map RGL internal types to GCM
static inline void rglGcmUpdateGcmTexture (void *data_tex, void *data_layout, void *data_plattex)
{
   rglTexture *texture = (rglTexture*)data_tex;
   rglGcmTexture *platformTexture = (rglGcmTexture*)data_plattex;
   rglGcmTextureLayout *layout = (rglGcmTextureLayout*)data_layout;

   // use color format for depth with no compare mode
   //  This hack is needed because the hardware will not read depth
   //  textures without performing a compare.  The depth value will need to
   //  be reconstructed in the shader from the color components.
   GLuint internalFormat = layout->internalFormat;

   // set the format and remap( control 1)
   rglGcmMapTextureFormat( internalFormat,
         &platformTexture->gcmTexture.format, &platformTexture->gcmTexture.remap );

   // This is just to cover the conversion from swizzled to linear
   if(layout->pitch)
      platformTexture->gcmTexture.format += 0x20; // see class doc definitions for SZ_NR vs LN_NR...

   platformTexture->gcmTexture.width = layout->baseWidth;
   platformTexture->gcmTexture.height = layout->baseHeight;
   platformTexture->gcmTexture.depth = layout->baseDepth;
   platformTexture->gcmTexture.pitch = layout->pitch;
   platformTexture->gcmTexture.mipmap = layout->levels;
   platformTexture->gcmTexture.cubemap = CELL_GCM_FALSE;
#ifndef HAVE_RGL_2D
   // set dimension, swizzled implies P2 width/height/depth
   switch ( texture->target )
   {
      case 0:
      case RGLGCM_TEXTURE_2D:
         platformTexture->gcmTexture.dimension = CELL_GCM_TEXTURE_DIMENSION_2;
         break;
   }
#else
   platformTexture->gcmTexture.dimension = CELL_GCM_TEXTURE_DIMENSION_2;
#endif
   platformTexture->gcmTexture.location = CELL_GCM_LOCATION_LOCAL;
}

// map RGL internal types to GCM
void rglGcmUpdateMethods (void *data)
{
   rglTexture *texture = (rglTexture*)data;
   rglGcmTexture *platformTexture = ( rglGcmTexture * )texture->platformTexture;
   rglGcmTextureLayout *layout = &platformTexture->gpuLayout;

   // max aniso
   // revalidate the texture registers cache.
   int maxAniso = ( int )texture->maxAnisotropy;
   GLuint minFilter = texture->minFilter;
   GLuint magFilter = texture->magFilter;

   // XXX make sure that REVALIDATE_PARAMETERS is set if the format of the texture changes
   // revalidate the texture registers cache just to ensure we are in the correct filtering mode
   // based on the internal format.
#ifndef HAVE_RGL_2D
   switch ( layout->internalFormat )
   {
      case RGLGCM_FLOAT_R32:
         // float 32 doesn't support filtering
         minFilter = unFilter( minFilter );
         magFilter = unFilter( magFilter );
         maxAniso = 1;
         break;
      default:
         break;
   }
#endif

   // -----------------------------------------------------------------------
   // map the SET_TEXTURE_FILTER method.
   platformTexture->gcmMethods.filter.min = rglGcmMapMinTextureFilter( minFilter );
   platformTexture->gcmMethods.filter.mag = rglGcmMapMagTextureFilter( magFilter );
   platformTexture->gcmMethods.filter.conv = CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX;
   // We don't actually expose this, but still need to set it up properly incase we expose this later
   // hw expects a 5.8 twos-complement fixed-point // XXX  what is the - .26f ?
   platformTexture->gcmMethods.filter.bias = ( GLint )(( texture->lodBias - .26f ) * 256.0f );

   // -----------------------------------------------------------------------
   // set the SET_TEXTURE_CONTROL0 params
   platformTexture->gcmMethods.control0.maxAniso = CELL_GCM_TEXTURE_MAX_ANISO_1;
   const GLfloat minLOD = MAX( texture->minLod, texture->baseLevel );
   const GLfloat maxLOD = MIN( texture->maxLod, texture->maxLevel );
   platformTexture->gcmMethods.control0.minLOD = ( GLuint )( MAX( minLOD, 0 ) * 256.0f );
   platformTexture->gcmMethods.control0.maxLOD = ( GLuint )( MIN( maxLOD, layout->levels ) * 256.0f );

   // -----------------------------------------------------------------------
   // set the SET_TEXTURE_ADDRESS method params.
   platformTexture->gcmMethods.address.wrapS = rglGcmMapWrapMode( texture->wrapS );
   platformTexture->gcmMethods.address.wrapT = rglGcmMapWrapMode( texture->wrapT );
   platformTexture->gcmMethods.address.wrapR = rglGcmMapWrapMode( texture->wrapR );
   platformTexture->gcmMethods.address.unsignedRemap = CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL;

   // now for gamma remap
   GLuint gamma = 0;
   GLuint remap = texture->gammaRemap;
   gamma |= ( remap & RGLGCM_GAMMA_REMAP_RED_BIT ) 		? CELL_GCM_TEXTURE_GAMMA_R : 0;
   gamma |= ( remap & RGLGCM_GAMMA_REMAP_GREEN_BIT )	? CELL_GCM_TEXTURE_GAMMA_G : 0;
   gamma |= ( remap & RGLGCM_GAMMA_REMAP_BLUE_BIT ) 	? CELL_GCM_TEXTURE_GAMMA_B : 0;
   gamma |= ( remap & RGLGCM_GAMMA_REMAP_ALPHA_BIT ) 	? CELL_GCM_TEXTURE_GAMMA_A : 0;

   platformTexture->gcmMethods.address.gamma = gamma;

   // set border colors
   RGLGCM_CALC_COLOR_LE_ARGB8(&(platformTexture->gcmMethods.borderColor), 
         texture->borderColor.R, texture->borderColor.G, texture->borderColor.B, texture->borderColor.A); 

   // -----------------------------------------------------------------------
   // setup the GcmTexture
   // format, control1, control3, imagerect; setup for cellGcmSetTexture later
   rglGcmUpdateGcmTexture( texture, layout, platformTexture );
}

// Validate texture resources
void rglPlatformValidateTextureResources (void *data)
{
   rglTexture *texture = (rglTexture*)data;
   texture->isComplete = GL_TRUE;

   //  We may need to reallocate the texture when the parameters are changed
   //  from non-mipmap to mipmap filtering, even though the images have not
   //  changed.
   //
   //  NOTE: If we ever support accessing mipmaps from a PBO, this code
   //  must be changed.  As it is, if the texture has a shared PBO and the
   //  mipmap flag then the slow path (copy back to host) is invoked.
   if ( texture->revalidate & RGL_TEXTURE_REVALIDATE_IMAGES || texture->revalidate & RGL_TEXTURE_REVALIDATE_LAYOUT )
   {
      // upload images
      rglPlatformReallocateGcmTexture( texture );
      rglPlatformUploadTexture( texture );
   }

   // gcmTexture method command
   rglGcmUpdateMethods( texture );

   texture->revalidate = 0;
}

// [YLIN] We are going to use gcm macro directly!
#include <cell/gcm/gcm_method_data.h>

// Set a texture to a gcm texture unit
static inline void rglGcmSetTextureUnit (GLuint unit, void *data)
{
   rglGcmTexture *platformTexture = (rglGcmTexture*)data;
   const GLuint imageOffset = gmmIdToOffset(platformTexture->gpuAddressId) + platformTexture->gpuAddressIdOffset;
   platformTexture->gcmTexture.offset = imageOffset; 

   // set up the texture unit with the info for the current texture
   // bind texture , control 1,3,format and remap
   // [YLIN] Contiguous GCM calls in fact will cause LHSs between them
   //  There is no walkaround for this but not to use GCM functions

   GCM_FUNC_SAFE( cellGcmSetTexture, unit, &platformTexture->gcmTexture );
   CellGcmContextData *gcm_context = (CellGcmContextData*)&rglGcmState_i.fifo;
   cellGcmReserveMethodSizeInline(gcm_context, 11);
   uint32_t *current = gcm_context->current;
   current[0] = CELL_GCM_METHOD_HEADER_TEXTURE_OFFSET(unit, 8);
   current[1] = CELL_GCM_METHOD_DATA_TEXTURE_OFFSET(platformTexture->gcmTexture.offset);
   current[2] = CELL_GCM_METHOD_DATA_TEXTURE_FORMAT(platformTexture->gcmTexture.location,
         platformTexture->gcmTexture.cubemap, 
         platformTexture->gcmTexture.dimension,
         platformTexture->gcmTexture.format,
         platformTexture->gcmTexture.mipmap);
   current[3] = CELL_GCM_METHOD_DATA_TEXTURE_ADDRESS( platformTexture->gcmMethods.address.wrapS,
         platformTexture->gcmMethods.address.wrapT,
         platformTexture->gcmMethods.address.wrapR,
         platformTexture->gcmMethods.address.unsignedRemap,
         platformTexture->gcmMethods.address.zfunc,
         platformTexture->gcmMethods.address.gamma,
         0);
   current[4] = CELL_GCM_METHOD_DATA_TEXTURE_CONTROL0(CELL_GCM_TRUE,
         platformTexture->gcmMethods.control0.minLOD,
         platformTexture->gcmMethods.control0.maxLOD,
         platformTexture->gcmMethods.control0.maxAniso);
   current[5] = platformTexture->gcmTexture.remap;
   current[6] = CELL_GCM_METHOD_DATA_TEXTURE_FILTER(
         (platformTexture->gcmMethods.filter.bias & 0x1fff),
         platformTexture->gcmMethods.filter.min,
         platformTexture->gcmMethods.filter.mag,
         platformTexture->gcmMethods.filter.conv);
   current[7] = CELL_GCM_METHOD_DATA_TEXTURE_IMAGE_RECT(
         platformTexture->gcmTexture.height,
         platformTexture->gcmTexture.width);
   current[8] = CELL_GCM_METHOD_DATA_TEXTURE_BORDER_COLOR(
         platformTexture->gcmMethods.borderColor);
   current[9] = CELL_GCM_METHOD_HEADER_TEXTURE_CONTROL3(unit,1);
   current[10] = CELL_GCM_METHOD_DATA_TEXTURE_CONTROL3(
         platformTexture->gcmTexture.pitch,
         platformTexture->gcmTexture.depth);
   gcm_context->current = &current[11];
}

// Validate incomplete texture by remapping
inline static void rglPlatformValidateIncompleteTexture( GLuint unit )
{
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

   // disable control 0
   GCM_FUNC( cellGcmSetTextureControl, unit, CELL_GCM_FALSE, 0, 0, 0 );
   // set texture remap only
   GCM_FUNC( cellGcmSetTextureRemap, unit, remap );
}

#undef RGLGCM_REMAP_MODES

// Valiate resources of a texture and set it to a unit
void rglPlatformValidateTextureStage( int unit, void *data)
{
   rglTexture *texture = (rglTexture*)data;

   if (RGL_UNLIKELY( texture->revalidate))
      rglPlatformValidateTextureResources(texture); // this updates the isComplete bit.

   GLboolean isCompleteCache = texture->isComplete;

   if (RGL_LIKELY(isCompleteCache))
   {
      rglGcmTexture *platformTexture = ( rglGcmTexture * )texture->platformTexture;
      rglGcmSetTextureUnit( unit, platformTexture );
   }
   else
   {
      //RGL_REPORT_EXTRA( RGL_REPORT_TEXTURE_INCOMPLETE, "Texture %d bound to unit %d(%s) is incomplete.", texture->name, unit, rglGetGLEnumName( texture->target ) );
      rglPlatformValidateIncompleteTexture( unit );
   }
}

// Choose internal format closest to given format
GLenum rglPlatformChooseInternalFormat (GLenum internalFormat)
{
   switch ( internalFormat )
   {
      case GL_ALPHA12:
      case GL_ALPHA16:
         return RGLGCM_ALPHA16;
      case GL_ALPHA:
      case GL_ALPHA4:
         return RGLGCM_ALPHA8;
      case GL_R3_G3_B2:
      case GL_RGB4:
      case GL_RGB:
      case GL_RGB8:
      case RGLGCM_RGBX8:
         return RGLGCM_RGBX8;
      case GL_RGBA2:
      case GL_RGBA4:
      case GL_RGBA8:
      case GL_RGBA:
         return RGLGCM_RGBA8;
      case GL_RGB5_A1:
         return RGLGCM_RGB5_A1_SCE;
      case GL_RGB5:
         return RGLGCM_RGB565_SCE;
      case GL_BGRA:
      case RGLGCM_BGRA8:
         return RGLGCM_BGRA8;
      case GL_ARGB_SCE:
         return RGLGCM_ARGB8;
      case GL_RGBA32F_ARB:
      default:
         return GL_INVALID_ENUM;
   }
   return GL_INVALID_ENUM;
}

// Expand internal format to format and type
void rglPlatformExpandInternalFormat( GLenum internalFormat, GLenum *format, GLenum *type )
{
   switch ( internalFormat )
   {
      case RGLGCM_ALPHA16:
         *format = GL_ALPHA;
         *type = GL_UNSIGNED_SHORT;
         break;
      case RGLGCM_ALPHA8:
         *format = GL_ALPHA;
         *type = GL_UNSIGNED_BYTE;
         break;
      case RGLGCM_RGBX8:
         *format = GL_RGBA;
         *type = GL_UNSIGNED_INT_8_8_8_8;
         break;
      case RGLGCM_RGBA8:
         *format = GL_RGBA;
         *type = GL_UNSIGNED_INT_8_8_8_8;
         break;
      case RGLGCM_ARGB8:
         *format = GL_BGRA;
         *type = GL_UNSIGNED_INT_8_8_8_8_REV;
         break;
      case RGLGCM_BGRA8:
         *format = GL_BGRA;
         *type = GL_UNSIGNED_INT_8_8_8_8;
         break;
      case RGLGCM_RGB5_A1_SCE:
         *format = GL_RGBA;
         *type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
         break;
      case RGLGCM_RGB565_SCE:
         *format = GL_RGB;
         *type = GL_UNSIGNED_SHORT_5_6_5_REV;
         break;
      default:
         return;
   }
}

// Choose internal storage type and size, and set it to image, based on given format
GLenum rglPlatformChooseInternalStorage (void *data, GLenum internalFormat )
{
   rglImage *image = (rglImage*)data;
   // see note at bottom concerning storageSize
   image->storageSize = 0;

   GLenum platformInternalFormat = rglPlatformChooseInternalFormat( internalFormat );

   if (platformInternalFormat == GL_INVALID_ENUM)
      return GL_INVALID_ENUM;

   image->internalFormat = platformInternalFormat;
   rglPlatformExpandInternalFormat( platformInternalFormat, &image->format, &image->type );

   // Note that it is critical to get the storageSize value correct because
   // this member is used to configure texture loads and unloads.  If this
   // value is wrong (e.g. contains unnecessary padding) it will corrupt
   // the GPU memory layout.
   image->storageSize = rglGetStorageSize(image->format, image->type,
         image->width, image->height, image->depth );

   return GL_NO_ERROR;
}

// Translate platform-specific format to GL enum
GLenum rglPlatformTranslateTextureFormat( GLenum internalFormat )
{
   switch ( internalFormat )
   {
      case RGLGCM_RGBX8:
         return GL_RGBA8;
      default:	// same as GL
         return internalFormat;
   }
}

// Implementation of texture reference
// Associate bufferObject to texture by assigning buffer's gpu address to the gcm texture 
GLboolean rglPlatformTextureReference (void *data, GLuint pitch, void *data_buf, GLintptr offset )
{
   rglBufferObject *bufferObject = (rglBufferObject*)data_buf;
   rglTexture *texture = (rglTexture*)data;
   rglGcmTexture *gcmTexture = ( rglGcmTexture * )texture->platformTexture;

   // XXX check pitch restrictions ?

   rglGcmTextureLayout newLayout;
   rglPlatformChooseGPUFormatAndLayout( texture, true, pitch, &newLayout );

   GLboolean isRenderTarget = GL_FALSE;
   GLboolean vertexEnable = GL_FALSE;
   // can usually be a render target, except for restrictions below
   if (rglIsDrawableColorFormat( newLayout.internalFormat))
      isRenderTarget = GL_TRUE;

#ifndef HAVE_RGL_2D
   switch (newLayout.internalFormat)
   {
      case GL_FLOAT_RGBA32:
      case GL_RGBA32F_ARB:
         vertexEnable = GL_TRUE;
         break;
      default:
         break;
   }
#endif

   texture->isRenderTarget = isRenderTarget;
   texture->vertexEnable = vertexEnable;

   if ( gcmTexture->gpuAddressId != GMM_ERROR )
      rglPlatformDestroyTexture( texture );

   rglGcmBufferObject *gcmBuffer = (rglGcmBufferObject*)&bufferObject->platformBufferObject;

   gcmTexture->gpuLayout = newLayout;
   gcmTexture->pool = gcmBuffer->pool;
   gcmTexture->gpuAddressId = gcmBuffer->bufferId;
   gcmTexture->gpuAddressIdOffset = offset;
   gcmTexture->gpuSize = rglGetGcmTextureSize( &newLayout );

   texture->revalidate &= ~( RGL_TEXTURE_REVALIDATE_LAYOUT | RGL_TEXTURE_REVALIDATE_IMAGES );
   texture->revalidate |= RGL_TEXTURE_REVALIDATE_PARAMETERS;
   rglTextureTouchFBOs( texture );
   return GL_TRUE;
}

// GlSetRenderTarget implementation starts here

// Render target rt's color and depth buffer parameters are updated with args
// Fifo functions are called as required
void static inline rglGcmSetColorDepthBuffers(void *data, const void *data_args)
{
   rglGcmRenderTarget *rt = (rglGcmRenderTarget*)data;
   CellGcmSurface *grt = &rt->gcmRenderTarget;
   const rglGcmRenderTargetEx* args = (const rglGcmRenderTargetEx*)data_args;

   rt->colorBufferCount = args->colorBufferCount;

   // remember rt for swap and clip related functions
   GLuint oldHeight;
   GLuint oldyInverted;

   oldyInverted = rt->yInverted;
   oldHeight = rt->gcmRenderTarget.height;

   if ( rt->colorFormat != ( GLuint )args->colorFormat )
   {
      // ARGB8 and FP16 interpret some registers differently
      rglGcmBlendState *blend = &rglGcmState_i.state.blend;
      rt->colorFormat  = args->colorFormat;
      rglGcmFifoGlBlendColor( blend->r, blend->g, blend->b, blend->a );
   }

   GLuint i = 0;

   for ( i = 0; i < args->colorBufferCount; i++ )
   {
      // choose context based on top bit of offset
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
            grt->colorLocation[i] = CELL_GCM_LOCATION_LOCAL;
            grt->colorOffset[i] = gmmIdToOffset(args->colorId[i]) + args->colorIdOffset[i];
            grt->colorPitch[i] = args->colorPitch[i];
         }
      }
   }

   // fill in the other render targets that haven't been set
   for ( ; i < RGLGCM_SETRENDERTARGET_MAXCOUNT; i++ )
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

   // scissor enabled/viewport and height changed ? obey yInverted
   if (( grt->height != oldHeight ) | ( rt->yInverted != oldyInverted ) )
   {
      rglGcmViewportState *v = &rglGcmState_i.state.viewport;
      rglGcmFifoGlViewport(v, 0.0f, 1.0f);
   }
}

// Update rt's color and depth format with args
void static inline rglGcmSetColorDepthFormats (void *data, const void *data_args)
{
   rglGcmRenderTarget *rt = (rglGcmRenderTarget*)data;
   CellGcmSurface *   grt = &rt->gcmRenderTarget;
   const rglGcmRenderTargetEx *args = (const rglGcmRenderTargetEx*)data_args;

   // set the color format
   switch ( args->colorFormat )
   {
      case RGLGCM_NONE:
      case RGLGCM_ARGB8:
         // choose a fake format
         // but choose a 16-bit format if depth is 16-bit
         grt->colorFormat = CELL_GCM_SURFACE_A8R8G8B8;
         break;
      case RGLGCM_FLOAT_R32:
         grt->colorFormat = CELL_GCM_SURFACE_F_X32;
         break;
      default:
         break;
   }

   // set the depth format
   // choose a fake format
   grt->depthFormat = CELL_GCM_SURFACE_Z24S8;
   grt->depthLocation = CELL_GCM_LOCATION_LOCAL;
   grt->depthOffset = 0;
   grt->depthPitch = 64;
}

// Update rt's color targets
static void inline rglGcmSetTarget (void *data, const void *data_args)
{
   rglGcmRenderTarget *rt = (rglGcmRenderTarget*)data;
   CellGcmSurface *   grt = &rt->gcmRenderTarget;
   const rglGcmRenderTargetEx *args = (const rglGcmRenderTargetEx*)data_args;

   // set target combo
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
   }
}

// Set current render target to args
void rglGcmFifoGlSetRenderTarget (const void *data)
{
   rglGcmRenderTarget *rt = &rglGcmState_i.renderTarget;
   CellGcmSurface *   grt = &rglGcmState_i.renderTarget.gcmRenderTarget;
   const rglGcmRenderTargetEx *args = (const rglGcmRenderTargetEx*)data;

   rglGcmSetColorDepthBuffers( rt, args );
   rglGcmSetColorDepthFormats( rt, args );

   // Update rt's AA and Swizzling parameters with args

   GCM_FUNC( cellGcmSetAntiAliasingControl, 
         CELL_GCM_FALSE, 
         CELL_GCM_FALSE,
         CELL_GCM_FALSE, 
         0xFFFF);

   grt->type = CELL_GCM_SURFACE_PITCH;
   rglGcmSetTarget( rt, args );

   // ensure if either width or height is 1 the other is one as well
   if (grt->width == 1)
      grt->height = 1; 
   else if (grt->height == 1)
      grt->width = 1; 

   GCM_FUNC( cellGcmSetSurface, grt );
}

/*============================================================
  PLATFORM TNL
  ============================================================ */

void rglValidateVertexProgram (void)
{
   // if validation is required, it means the program  has to be downloaded.
   RGLcontext*	LContext = _CurrentContext;

   rglSetNativeCgVertexProgram(LContext->BoundVertexProgram);

   // Set all uniforms.
   if(!(LContext->needValidate & RGL_VALIDATE_VERTEX_CONSTANTS) && LContext->BoundVertexProgram->parentContext)
      rglValidateVertexConstants();
}

void rglValidateVertexConstants (void)
{
   RGLcontext*	LContext = _CurrentContext;
   _CGprogram *cgprog = LContext->BoundVertexProgram;

   // Push a CG program onto the current command buffer

   // make sure there is space for the pushbuffer + any nops we need to add for alignment  
   rglGcmFifoWaitForFreeSpace( &rglGcmState_i.fifo,  cgprog->constantPushBufferWordSize + 4 + 32); 

   // first add nops to get us the next alligned position in the fifo 
   // [YLIN] Use VMX register to copy
   uint32_t padding_in_word = ( ( 0x10-(((uint32_t)rglGcmState_i.fifo.current)&0xf))&0xf )>>2;
   uint32_t padded_size = ( ((cgprog->constantPushBufferWordSize)<<2) + 0xf )&~0xf;

   GCM_FUNC( cellGcmSetNopCommandUnsafe, padding_in_word );
   memcpy16(rglGcmState_i.fifo.current, cgprog->constantPushBuffer, padded_size);
   rglGcmState_i.fifo.current+=cgprog->constantPushBufferWordSize;
}

/*============================================================
  UTILS
  ============================================================ */

void rglDrawUtilQuad( GLboolean useFixedVP, GLboolean useFixedFP, GLuint x, GLuint y, GLuint width, GLuint height )
{
   (void)useFixedVP;
   (void)useFixedFP;

   RGLcontext*	LContext = _CurrentContext;

   rglGcmFifoGlDisable( RGLGCM_BLEND );

   GCM_FUNC( cellGcmSetFrontPolygonMode, CELL_GCM_POLYGON_MODE_FILL );
   GCM_FUNC( cellGcmSetBackPolygonMode, CELL_GCM_POLYGON_MODE_FILL );
   GCM_FUNC( cellGcmSetUserClipPlaneControl,
         CELL_GCM_USER_CLIP_PLANE_DISABLE,
         CELL_GCM_USER_CLIP_PLANE_DISABLE,
         CELL_GCM_USER_CLIP_PLANE_DISABLE,
         CELL_GCM_USER_CLIP_PLANE_DISABLE,
         CELL_GCM_USER_CLIP_PLANE_DISABLE,
         CELL_GCM_USER_CLIP_PLANE_DISABLE );

   rglGcmViewportState *v = &rglGcmState_i.state.viewport;
   v->x = x;
   v->y = y;
   v->w = width;
   v->h = height;
   rglGcmFifoGlViewport(v, 0.0f, 1.0f);

   GCM_FUNC_NO_ARGS( cellGcmSetInvalidateVertexCache );
   rglGcmFifoGlDrawArrays( RGLGCM_TRIANGLE_STRIP, 0, 4 );

   LContext->needValidate |= RGL_VALIDATE_BLENDING | RGL_VALIDATE_VIEWPORT;
}
