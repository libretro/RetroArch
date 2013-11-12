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
#include "../rglp.h"

#include <sdk_version.h>

#include "../../altivec_mem.h"

#include "include/GmmAlloc.h"
#include "include/rgl-constants.h"
#include "include/rgl-typedefs.h"
#include "include/rgl-externs.h"
#include "include/rgl-inline.h"

#include <Cg/cg.h>
#include <Cg/CgCommon.h>
#include <Cg/cgBinary.h>

#include <ppu_intrinsics.h>

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
   RGLcontext*	LContext = _CurrentContext;

   rglAttribute* attrib = LContext->attribs->attrib + index;
   attrib->value[0] = f[0];
   attrib->value[1] = f[1];
   attrib->value[2] = f[2];
   attrib->value[3] = f[3];
   RGLBIT_TRUE( LContext->attribs->DirtyMask, index );
}

//here ec has been advanced and is already on top of the embedded constant count
template<int SIZE> inline static void swapandsetfp( int ucodeSize, unsigned int loadProgramId, unsigned int loadProgramOffset, unsigned short *ec, const unsigned int   * __restrict v )
{
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   rglGcmSetTransferLocation(thisContext, CELL_GCM_LOCATION_LOCAL );
   unsigned short count = *( ec++ );
   for ( unsigned long offsetIndex = 0; offsetIndex < count; ++offsetIndex )
   {
      void *pointer=NULL;
      const int paddedSIZE = (SIZE + 1) & ~1; // even width only	
      uint32_t var = gmmIdToOffset( loadProgramId ) + loadProgramOffset + *( ec++ );
      rglGcmSetInlineTransferPointer(thisContext, var, paddedSIZE, pointer);
      float *fp = (float*)pointer;
      float *src = (float*)v;
      for (uint32_t j=0; j<SIZE;j++)
      {
         *fp = cellGcmSwap16Float32(*src);
         fp++;src++;
      }
   }

}

template<int SIZE> static void setVectorTypefp( void *dat, const void* __restrict v )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)dat;
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

//  matrix uniforms
// note that Cg generated matrices are 1 row per binary param
// storage within the parameter is row major (so register setting is easier)

//tmp array tentative

#define ROW_MAJOR 0
#define COL_MAJOR 1

template <int SIZE, bool isIndex> static void setVectorTypevpIndex (void *data, const void* __restrict v, const int index )
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   RGLcontext * LContext = _CurrentContext;
   const float * __restrict f = ( const float* )v;
   float * __restrict dst;
   if (isIndex)
      dst = ( float* )( *(( unsigned int ** )ptr->pushBufferPointer + index ) );
   else
      dst = ( float* )ptr->pushBufferPointer;

   for ( long i = 0; i < SIZE; ++ i )
      dst[i] = f[i];
   LContext->needValidate |= RGL_VALIDATE_VERTEX_CONSTANTS;
}

template<int SIZE, bool isIndex> static void setVectorTypefpIndex (void *dat, const void *v, const int index)
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
   if (isIndex)
   {
      int arrayIndex = index;
      while ( arrayIndex ) //jump to the right index... this is slow
      {
         ec += (( *ec ) + 2 );//+1 for the register, +1 for the count, +count for the number of embedded consts
         arrayIndex--;
      }
   }
   if ( RGL_LIKELY( *ec ) )
      swapandsetfp<SIZE>( program->header.instructionCount*16, program->loadProgramId, program->loadProgramOffset, ec, ( unsigned int * )data );
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

static _cgSetArrayIndexFunction setVectorTypeIndex[2][1][2][4] =
{
  {
      {
         {&setVectorTypevpIndex<1, 0>, &setVectorTypevpIndex<2, 0>, &setVectorTypevpIndex<3, 0>, &setVectorTypevpIndex<4, 0>, },
         {&setVectorTypefpIndex<1, 0>, &setVectorTypefpIndex<2, 0>, &setVectorTypefpIndex<3, 0>, &setVectorTypefpIndex<4, 0>, }
      },
   },
   {
      {
         {&setVectorTypevpIndex<1, 1>, &setVectorTypevpIndex<2, 1>, &setVectorTypevpIndex<3, 1>, &setVectorTypevpIndex<4, 1>, },
         {&setVectorTypefpIndex<1, 1>, &setVectorTypefpIndex<2, 1>, &setVectorTypefpIndex<3, 1>, &setVectorTypefpIndex<4, 1>, }
      },
   },
};

static _cgSetArrayIndexFunction setMatrixTypeIndex[2][2][2][4][4][2] =
{
   {
      {
         {
            {
               { &setMatrixvpIndex<1, 1, 0>, &setMatrixvpIndex<1, 1, 1>},
               { &setMatrixvpIndex<1, 2, 0>, &setMatrixvpIndex<1, 2, 1>},
               { &setMatrixvpIndex<1, 3, 0>, &setMatrixvpIndex<1, 3, 1>},
               { &setMatrixvpIndex<1, 4, 0>, &setMatrixvpIndex<1, 4, 1>}
            },
            {
               { &setMatrixvpIndex<2, 1, 0>, &setMatrixvpIndex<2, 1, 1>},
               { &setMatrixvpIndex<2, 2, 0>, &setMatrixvpIndex<2, 2, 1>},
               { &setMatrixvpIndex<2, 3, 0>, &setMatrixvpIndex<2, 3, 1>},
               { &setMatrixvpIndex<2, 4, 0>, &setMatrixvpIndex<2, 4, 1>}
            },
            {
               { &setMatrixvpIndex<3, 1, 0>, &setMatrixvpIndex<3, 1, 1>},
               { &setMatrixvpIndex<3, 2, 0>, &setMatrixvpIndex<3, 2, 1>},
               { &setMatrixvpIndex<3, 3, 0>, &setMatrixvpIndex<3, 3, 1>},
               { &setMatrixvpIndex<3, 4, 0>, &setMatrixvpIndex<3, 4, 1>}
            },
            {
               { &setMatrixvpIndex<4, 1, 0>, &setMatrixvpIndex<4, 1, 1>},
               { &setMatrixvpIndex<4, 2, 0>, &setMatrixvpIndex<4, 2, 1>},
               { &setMatrixvpIndex<4, 3, 0>, &setMatrixvpIndex<4, 3, 1>},
               { &setMatrixvpIndex<4, 4, 0>, &setMatrixvpIndex<4, 4, 1>}
            },
         },
         {
            {{ &setMatrixfpIndex<1, 1, 0>, &setMatrixfpIndex<1, 1, 1>}, { &setMatrixfpIndex<1, 2, 0>, &setMatrixfpIndex<1, 2, 1>}, { &setMatrixfpIndex<1, 3, 0>, &setMatrixfpIndex<1, 3, 1>}, { &setMatrixfpIndex<1, 4, 0>, &setMatrixfpIndex<1, 4, 1>}},
            {{ &setMatrixfpIndex<2, 1, 0>, &setMatrixfpIndex<2, 1, 1>}, { &setMatrixfpIndex<2, 2, 0>, &setMatrixfpIndex<2, 2, 1>}, { &setMatrixfpIndex<2, 3, 0>, &setMatrixfpIndex<2, 3, 1>}, { &setMatrixfpIndex<2, 4, 0>, &setMatrixfpIndex<2, 4, 1>}},
            {{ &setMatrixfpIndex<3, 1, 0>, &setMatrixfpIndex<3, 1, 1>}, { &setMatrixfpIndex<3, 2, 0>, &setMatrixfpIndex<3, 2, 1>}, { &setMatrixfpIndex<3, 3, 0>, &setMatrixfpIndex<3, 3, 1>}, { &setMatrixfpIndex<3, 4, 0>, &setMatrixfpIndex<3, 4, 1>}},
            {{ &setMatrixfpIndex<4, 1, 0>, &setMatrixfpIndex<4, 1, 1>}, { &setMatrixfpIndex<4, 2, 0>, &setMatrixfpIndex<4, 2, 1>}, { &setMatrixfpIndex<4, 3, 0>, &setMatrixfpIndex<4, 3, 1>}, { &setMatrixfpIndex<4, 4, 0>, &setMatrixfpIndex<4, 4, 1>}},
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
   }
};

static void setSamplerfp (void *data, const void*v, int /* index */)
{
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   _CGprogram *program = (( CgRuntimeParameter* )ptr )->program;
   const CgParameterResource *parameterResource = rglGetParameterResource( program, (( CgRuntimeParameter* )ptr )->parameterEntry );

   // the value of v == NULL when it is called from cgGLEnableTextureParameter
   // the value of v == NULL when it is called from  cgGLSetTextureParameter
   // this may be called by a connected param to propagate its value
   // the spec says that the set should not cause the bind
   // so only do the bind when the call comes from cgGLEnableTextureParameter
   if (v)
      *(GLuint*)ptr->pushBufferPointer = *(GLuint*)v;
   else
   {
      rglTextureImageUnit *unit = _CurrentContext->TextureImageUnits + ( parameterResource->resource - CG_TEXUNIT0 );
      rglBindTextureInternal( unit, *(GLuint*)ptr->pushBufferPointer, ptr->glType );
   }
}

static void setSamplervp (void *data, const void*v, int /* index */)
{
   // the value of v == NULL when it is called from cgGLEnableTextureParameter
   // the value of v == NULL when it is called from  cgGLSetTextureParameter
   // this may be called by a connected param to propagate its value
   // the spec says that the set should not cause the bind
   // so only do the bind when the call comes from cgGLEnableTextureParameter
   CgRuntimeParameter *ptr = (CgRuntimeParameter*)data;
   if (v)
      *(GLuint*)ptr->pushBufferPointer = *( GLuint* )v;
}


#undef ROW_MAJOR
#undef COL_MAJOR

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
   int profileIndex = ( program->header.profile == CG_PROFILE_SCE_FP_TYPEB || 
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
            else //profileIndex == FRAGMENT_PROFILE_INDEX
            {
               int registerStride = isMatrix(( CGtype )parameterResource->type ) ? rglGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
                  //TODO: check this case
                  extraStorageInWords += 4 * arrayCount * registerStride;
            }
         }
      }
      arrayCount = 1;
   }

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

      rtParameter->samplerSetter = _cgIgnoreParamIndex;

      //tentative
      rtParameter->setterIndex = _cgIgnoreParamIndex;
      rtParameter->setterrIndex = _cgIgnoreParamIndex;
      rtParameter->settercIndex = _cgIgnoreParamIndex;

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
                  rtParameter->setterIndex = _cgIgnoreParamIndex;
                  rtParameter->samplerSetter = setSamplervp;
               }
               else
                  rtParameter->samplerSetter = setSamplerfp;
               rtParameter->glType = rglCgGetSamplerGLTypeFromCgType(( CGtype )( parameterResource->type ) );
            }
            else
            {
               if ( profileIndex == VERTEX_PROFILE_INDEX )
               {
                  int registerStride = isMatrix(( CGtype )parameterResource->type ) ? rglGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
                  int registerCount = arrayCount * registerStride;
                  if ( parameterEntry->flags & CGP_CONTIGUOUS )
                  {
                     memset( rglGcmCurrent, 0, 4*( 4*registerCount + 3 ) );
                     CellGcmContextData gcmContext;
                     gcmContext.current = (uint32_t*)rglGcmCurrent;
                     cellGcmSetVertexProgramParameterBlockUnsafeInline(&gcmContext, parameterResource->resource, registerCount, ( float* )rglGcmCurrent );
                     rglGcmCurrent = (typeof(rglGcmCurrent))gcmContext.current;

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
                           CellGcmContextData gcmContext;
                           gcmContext.current = (uint32_t*)rglGcmCurrent;
                           cellGcmSetVertexProgramParameterBlockUnsafeInline(&gcmContext, program->resources[resourceIndex], registerStride, ( float* )rglGcmCurrent );
                           rglGcmCurrent = (typeof(rglGcmCurrent))gcmContext.current;
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
               else //if (profileIndex == FRAGMENT_PROFILE_INDEX)
               {
                  int registerStride = isMatrix(( CGtype )parameterResource->type ) ? rglGetTypeRowCount(( CGtype )parameterResource->type ) : 1;
                  int registerCount = arrayCount * registerStride;
                  rtParameter->pushBufferPointer = currentStorage;
                  currentStorage += 4 * registerCount;
               }

               switch ( parameterResource->type )
               {
                  case CG_FLOAT:
                  case CG_FLOAT1: case CG_FLOAT2: case CG_FLOAT3: case CG_FLOAT4:
                     // if this gets updated, don't forget the halfs below
                     {
                        unsigned int floatCount = rglCountFloatsInCgType(( CGtype )parameterResource->type );
                        rtParameter->setterIndex = setVectorTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][0][profileIndex][floatCount - 1];
                     }
                     break;
                  case CG_FLOAT1x1: case CG_FLOAT1x2: case CG_FLOAT1x3: case CG_FLOAT1x4:
                  case CG_FLOAT2x1: case CG_FLOAT2x2: case CG_FLOAT2x3: case CG_FLOAT2x4:
                  case CG_FLOAT3x1: case CG_FLOAT3x2: case CG_FLOAT3x3: case CG_FLOAT3x4:
                  case CG_FLOAT4x1: case CG_FLOAT4x2: case CG_FLOAT4x3: case CG_FLOAT4x4:
                     // if this gets updated, don't forget the halfs below
                     rtParameter->setterrIndex = setMatrixTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][0][profileIndex][rglGetTypeRowCount(( CGtype )parameterResource->type ) - 1][rglGetTypeColCount(( CGtype )parameterResource->type ) - 1][ROW_MAJOR];
                     rtParameter->settercIndex = setMatrixTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][0][profileIndex][rglGetTypeRowCount(( CGtype )parameterResource->type ) - 1][rglGetTypeColCount(( CGtype )parameterResource->type ) - 1][COL_MAJOR];
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
                        rtParameter->setterIndex = setVectorTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][0][profileIndex][floatCount - 1];
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
                     rtParameter->setterrIndex = setMatrixTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][0][profileIndex][rglGetTypeRowCount(( CGtype )parameterResource->type ) - 1][rglGetTypeColCount(( CGtype )parameterResource->type ) - 1][ROW_MAJOR];
                     rtParameter->settercIndex = setMatrixTypeIndex[( containerEntry->flags&CGP_ARRAY ) ? 1 : 0][0][profileIndex][rglGetTypeRowCount(( CGtype )parameterResource->type ) - 1][rglGetTypeColCount(( CGtype )parameterResource->type ) - 1][COL_MAJOR];
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
                  rtParameter->setterIndex = _cgIgnoreParamIndex;
                  break;
               case CG_FLOAT1x1: case CG_FLOAT1x2: case CG_FLOAT1x3: case CG_FLOAT1x4:
               case CG_FLOAT2x1: case CG_FLOAT2x2: case CG_FLOAT2x3: case CG_FLOAT2x4:
               case CG_FLOAT3x1: case CG_FLOAT3x2: case CG_FLOAT3x3: case CG_FLOAT3x4:
               case CG_FLOAT4x1: case CG_FLOAT4x2: case CG_FLOAT4x3: case CG_FLOAT4x4:
                  rtParameter->setterrIndex = _cgIgnoreParamIndex;
                  rtParameter->settercIndex = _cgIgnoreParamIndex;
                  break;
               case CG_SAMPLER1D: case CG_SAMPLER2D: case CG_SAMPLER3D: case CG_SAMPLERRECT: case CG_SAMPLERCUBE:
                  rtParameter->samplerSetter = _cgIgnoreParamIndex;
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
                  rtParameter->setterIndex = _cgIgnoreParamIndex;
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
                  rtParameter->setterrIndex = _cgIgnoreParamIndex;
                  rtParameter->settercIndex = _cgIgnoreParamIndex;
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
      CellGcmContextData gcmContext;
      gcmContext.current = (uint32_t*)rglGcmCurrent;
      cellGcmSetNopCommandUnsafeInline(&gcmContext, nopCount);
      rglGcmCurrent = (typeof(rglGcmCurrent))gcmContext.current;
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
         0, rglBuffer->bufferSize);
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
      memcpy( gmmIdToAddress( rglBuffer->bufferId ) + offset, data, size );
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
      memcpy( gmmIdToAddress( rglBuffer->bufferId ), data, size );
   }
      else
      {
         if (tryImmediateCopy)
            memcpy( gmmIdToAddress( rglBuffer->bufferId ) + offset, data, size );
         else
         {
            // partial buffer write
            //  STREAM and DYNAMIC buffers get transfer via a bounce buffer.
            // copy via bounce buffer
            rglGcmSend( rglBuffer->bufferId, offset, rglBuffer->pitch, ( const char * )data, size );
         }
      }
}


GLAPI void APIENTRY glBufferSubData( GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data )
{
   RGLcontext *LContext = (RGLcontext*)_CurrentContext;
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
   rglPlatformBufferObjectSetData( bufferObject, offset, size, data, GL_FALSE );
}


char *rglPlatformBufferObjectMap (void *data, GLenum access)
{
   rglBufferObject *bufferObject = (rglBufferObject*)data;
   rglGcmFifo *fifo = (rglGcmFifo*)&rglGcmState_i.fifo;
   rglGcmBufferObject *rglBuffer = (rglGcmBufferObject*)bufferObject->platformBufferObject;
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   GLuint ref;

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
      {
         unsigned int offset_bytes = 0;
         // must wait in order to read
         rglGcmSetInvalidateVertexCache(thisContext);
         rglGcmFifoFinish(ref, offset_bytes);
      }

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

#ifdef __cplusplus
extern "C" {
#endif

static void rglPlatformBufferObjectSetDataTextureReference(void *buf_data, GLintptr offset, GLsizeiptr size, const GLvoid *data, GLboolean tryImmediateCopy)
{
   rglBufferObject *bufferObject = (rglBufferObject*)buf_data;
   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;
   rglGcmBufferObject *rglBuffer = ( rglGcmBufferObject * )bufferObject->platformBufferObject;

   if ( size >= bufferObject->size )
   {
      // reallocate the buffer
      //  To avoid waiting for the GPU to finish with the buffer, just
      //  allocate a whole new one.
      rglBuffer->bufferSize = rglPad( size, RGL_BUFFER_OBJECT_BLOCK_SIZE );
      rglpsAllocateBuffer( bufferObject );

      // copy directly to newly allocated memory
      //  TODO: For GPU destination, should we copy to system memory and
      //  pull from GPU?
      memset( gmmIdToAddress(rglBuffer->bufferId), 0, size );
   }
   else
   {
      // partial buffer write
      //  STREAM and DYNAMIC buffers get transfer via a bounce buffer.
      // copy via bounce buffer
      GLuint id = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo, 0, size);
      memset(gmmIdToAddress(id), 0, size);
      rglGcmTransferData(rglBuffer->bufferId, offset, rglBuffer->pitch, id, 0, size, size, 1);
      gmmFree(id);
   }
}

GLAPI void APIENTRY glBufferSubDataTextureReferenceRA( GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data )
{
   RGLcontext *LContext = (RGLcontext*)_CurrentContext;
   GLuint name = LContext->TextureBuffer;

   rglBufferObject* bufferObject = (rglBufferObject*)LContext->bufferObjectNameSpace.data[name];
   rglPlatformBufferObjectSetDataTextureReference( bufferObject, offset, size, data, GL_FALSE );
}

char *rglPlatformBufferObjectMapTextureReference(void *data, GLenum access)
{
   rglBufferObject *bufferObject = (rglBufferObject*)data;
   rglGcmFifo *fifo = (rglGcmFifo*)&rglGcmState_i.fifo;
   rglGcmBufferObject *rglBuffer = (rglGcmBufferObject*)bufferObject->platformBufferObject;
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   rglBuffer->mapAccess = access;

   // only need to pin the first time we map
   gmmPinId( rglBuffer->bufferId );

   return gmmIdToAddress( rglBuffer->bufferId );
}

GLboolean rglPlatformBufferObjectUnmapTextureReference (void *data)
{
   rglBufferObject *bufferObject = (rglBufferObject*)data;
   rglGcmBufferObject *rglBuffer = ( rglGcmBufferObject * )bufferObject->platformBufferObject;
   rglBuffer->mapAccess = GL_NONE;
   gmmUnpinId( rglBuffer->bufferId );
   return GL_TRUE;
}

#ifdef __cplusplus
}
#endif

/*============================================================
  PLATFORM FRAMEBUFFER
  ============================================================ */

GLAPI void APIENTRY glClear( GLbitfield mask )
{
   unsigned int offset_bytes = 0;
   RGLcontext*	LContext = _CurrentContext;
   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   rglGcmFifo * fifo = &rglGcmState_i.fifo;

   if ( LContext->needValidate & RGL_VALIDATE_FRAMEBUFFER )
   {
      // set render targets
   
      RGLdevice *LDevice = _CurrentDevice;
      rglGcmDevice *gcmDevice = ( rglGcmDevice * )LDevice->platformDevice;

      // reset buffer data
      driver->rtValid = GL_FALSE;
      // get buffer parameters
      //  This may come from a framebuffer_object or the default framebuffer.
      //
      driver->rt = gcmDevice->rt;

      if (LContext->framebuffer)
      {
         rglPlatformFramebuffer* framebuffer = (rglPlatformFramebuffer *)rglGetFramebuffer(LContext, LContext->framebuffer);

         if (framebuffer->needValidate)
            framebuffer->validate( LContext );

         driver->rt = framebuffer->rt;
      }

      driver->rtValid = GL_TRUE;

      // update GPU configuration
      rglGcmFifoGlSetRenderTarget( &driver->rt );

      LContext->needValidate &= ~RGL_VALIDATE_FRAMEBUFFER;
      LContext->needValidate |= RGL_VALIDATE_VIEWPORT;
   }

   if (!driver->rtValid)
      return;

   rglGcmSetClearColor(thisContext, 0 );
   rglGcmSetClearSurface(thisContext, CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | 
         CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A );
   rglGcmSetInvalidateVertexCache(thisContext);
   rglGcmFifoFlush(fifo, offset_bytes);
}

rglFramebuffer* rglCreateFramebuffer (void)
{
   return new rglPlatformFramebuffer();
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
   RGLcontext* LContext = (RGLcontext*)_CurrentContext;

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

static void rglPlatformValidateTextureResources (void *data);

void rglPlatformFramebuffer::validate (void *data)
{
   RGLcontext *LContext = (RGLcontext*)data;
   complete = (rglPlatformFramebufferCheckStatus(this) == GL_FRAMEBUFFER_COMPLETE_OES);

   if (!complete)
      return;

   GLuint width = RGLGCM_MAX_RT_DIMENSION;
   GLuint height = RGLGCM_MAX_RT_DIMENSION;

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

      if (colorTexture == NULL)
         continue;

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
      rt.colorIdOffset[i] = nvTexture->gpuAddressIdOffset;
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
   rt.width = width;
   rt.height = height;

   rt.yInverted = RGLGCM_FALSE;
   rt.xOffset = 0;
   rt.yOffset = 0;
   needValidate = GL_FALSE;
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
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   rglGcmFifo *fifo = (rglGcmFifo*)&rglGcmState_i.fifo;
   GLuint ref;
   unsigned int offset_bytes = 0;

   rglGcmSetInvalidateVertexCache(thisContext);
   rglGcmFifoFinish(ref, offset_bytes);

   rglGcmDriver *driver = (rglGcmDriver*)malloc(sizeof(rglGcmDriver));
   memset(driver, 0, sizeof(rglGcmDriver));

   driver->rt.yInverted = RGLGCM_TRUE;
   driver->invalidateVertexCache = GL_FALSE;
   driver->flushBufferCount = 0;

   // [YLIN] Make it 16 byte align

   return driver;
}

// Destroy the driver, and free all its used memory
void rglPlatformRasterExit (void *data)
{
   rglGcmDriver *driver = (rglGcmDriver*)data;

   if (driver)
      free(driver);
}

void rglDumpFifo (char *name);

// [YLIN] We are going to use gcm macro directly!
#include <cell/gcm/gcm_method_data.h>

#undef RGLGCM_REMAP_MODES

const uint32_t c_rounded_size_ofrglDrawParams = (sizeof(rglDrawParams)+0x7f)&~0x7f;
static uint8_t s_dparams_buff[ c_rounded_size_ofrglDrawParams ] __attribute__((aligned(128)));

// Fast rendering path called by several glDraw calls:
//   glDrawElements, glDrawRangeElements, glDrawArrays
// Slow rendering calls this function also, though it must also perform various
// memory setup operations first
GLAPI void APIENTRY glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
   RGLcontext*	LContext = _CurrentContext;
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;

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

   if ( LContext->needValidate )
   {
      GLuint mask = RGL_VALIDATE_ALL;
      LContext->needValidate &= mask;

      GLuint  needValidate = LContext->needValidate;

      if (RGL_UNLIKELY( needValidate & RGL_VALIDATE_TEXTURES_USED))
      {
         long unitInUseCount = LContext->BoundFragmentProgram->samplerCount;
         const GLuint* unitsInUse = LContext->BoundFragmentProgram->samplerUnits;
         for ( long i = 0; i < unitInUseCount; ++i )
         {
            long unit = unitsInUse[i];
            rglTexture* texture = LContext->TextureImageUnits[unit].currentTexture;

            if (!texture)
               continue;

            // Validate resources of a texture and set it to a unit

            if (RGL_UNLIKELY( texture->revalidate))
               rglPlatformValidateTextureResources(texture); // this updates the isComplete bit.

            GLboolean isCompleteCache = texture->isComplete;

            if (RGL_LIKELY(isCompleteCache))
            {
               // Set a texture to a gcm texture unit

               rglGcmTexture *platformTexture = ( rglGcmTexture * )texture->platformTexture;
               const GLuint imageOffset = gmmIdToOffset(platformTexture->gpuAddressId) +
                  platformTexture->gpuAddressIdOffset;
               platformTexture->gcmTexture.offset = imageOffset; 

               // set up the texture unit with the info for the current texture
               // bind texture , control 1,3,format and remap
               const CellGcmTexture *texture = (const CellGcmTexture*)&platformTexture->gcmTexture;

               rglGcmSetTextureBorder(thisContext, unit, texture, 0x1);

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
            else
            {
               // Validate incomplete texture by remapping
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

               rglGcmSetTextureControl(thisContext, unit, CELL_GCM_FALSE, 0, 0, 0 ); // disable control 0
               rglGcmSetTextureRemap(thisContext, unit, remap ); // set texture remap only
            }
         }
      }

      bool validate_vertex_consts = false;

      if (RGL_UNLIKELY(needValidate & RGL_VALIDATE_VERTEX_PROGRAM))
      {
         const _CGprogram *program = (const _CGprogram*)LContext->BoundVertexProgram;
         __dcbt(program->ucode);
         __dcbt(((uint8_t*)program->ucode)+128);
         __dcbt(((uint8_t*)program->ucode)+256);
         __dcbt(((uint8_t*)program->ucode)+384);

         CellCgbVertexProgramConfiguration conf;
         conf.instructionSlot = program->header.vertexProgram.instructionSlot;
         conf.instructionCount = program->header.instructionCount;
         conf.registerCount = program->header.vertexProgram.registerCount;
         conf.attributeInputMask = program->header.attributeInputMask;

         rglGcmFifo *fifo = (rglGcmFifo*)&rglGcmState_i.fifo;
         GLuint spaceInWords = 7 + 5 * conf.instructionCount;

         // Push a CG program onto the current command buffer

         // make sure there is space for the pushbuffer + any nops we need to add for alignment  
         if ( fifo->current + spaceInWords + 1024 > fifo->end )
            rglOutOfSpaceCallback( fifo, spaceInWords );

         rglGcmSetVertexProgramLoad(thisContext, &conf, program->ucode );
         rglGcmSetUserClipPlaneControl(thisContext, 0, 0, 0, 0, 0, 0 );

         rglGcmInterpolantState *s = &rglGcmState_i.state.interpolant;
         s->vertexProgramAttribMask = program->header.vertexProgram.attributeOutputMask;

         rglGcmSetVertexAttribOutputMask(thisContext, (( s->vertexProgramAttribMask) &
                  s->fragmentProgramAttribMask));

         int count = program->defaultValuesIndexCount;
         for ( int i = 0;i < count;i++ )
         {
            const CgParameterEntry *parameterEntry = program->parametersEntries + program->defaultValuesIndices[i].entryIndex;
            GLboolean cond = (( parameterEntry->flags & CGPF_REFERENCED )
                  && ( parameterEntry->flags & CGPV_MASK ) == CGPV_CONSTANT );

            if (!cond)
               continue;

            const float *value = program->defaultValues + 
               program->defaultValuesIndices[i].defaultValueIndex;

            const CgParameterResource *parameterResource = rglGetParameterResource( program, parameterEntry );

            GLboolean cond2 = parameterResource->resource != (unsigned short) - 1;

            if (!cond2)
               continue;

            switch ( parameterResource->type )
            {
               case CG_FLOAT:
               case CG_FLOAT1:
               case CG_FLOAT2:
               case CG_FLOAT3:
               case CG_FLOAT4:
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
                  GCM_FUNC( cellGcmSetVertexProgramParameterBlock, parameterResource->resource, 1, value ); // GCM_PORT_TESTED [Cedric]
                  break;
               case CG_FLOAT4x4:
               case CG_HALF4x4:
               case CG_INT4x4:
               case CG_BOOL4x4:
               case CG_FIXED4x4:
                  // set 4 consts
                  {
                     GLfloat v2[16];
                     v2[0] = value[0];
                     v2[1] = value[4];
                     v2[2] = value[8];
                     v2[3] = value[12];
                     v2[4] = value[1];
                     v2[5] = value[5];
                     v2[6] = value[9];
                     v2[7] = value[13];
                     v2[8] = value[2];
                     v2[9] = value[6];
                     v2[10] = value[10];
                     v2[11] = value[14];
                     v2[12] = value[3];
                     v2[13] = value[7];
                     v2[14] = value[11];
                     v2[15] = value[15];
                     GCM_FUNC( cellGcmSetVertexProgramParameterBlock, parameterResource->resource, 4, v2 ); // GCM_PORT_TESTED [Cedric]
                  }
                  break;
               case CG_FLOAT3x3:
               case CG_HALF3x3:
               case CG_INT3x3:
               case CG_BOOL3x3:
               case CG_FIXED3x3:
                  // set 3 consts
                  {
                     GLfloat v2[12];
                     v2[0] = value[0];
                     v2[1] = value[3];
                     v2[2] = value[6];
                     v2[3] = 0;
                     v2[4] = value[1];
                     v2[5] = value[4];
                     v2[6] = value[7];
                     v2[7] = 0;
                     v2[8] = value[2];
                     v2[9] = value[5];
                     v2[10] = value[8];
                     v2[11] = 0;
                     GCM_FUNC( cellGcmSetVertexProgramParameterBlock, parameterResource->resource, 3, v2 );
                  }
                  break;
            }
         }

         // Set all uniforms.
         if(!(LContext->needValidate & RGL_VALIDATE_VERTEX_CONSTANTS) && LContext->BoundVertexProgram->parentContext)
            validate_vertex_consts = true;
      }

      if (RGL_LIKELY(needValidate & RGL_VALIDATE_VERTEX_CONSTANTS) || validate_vertex_consts)
      {
         _CGprogram *cgprog = LContext->BoundVertexProgram;
         rglGcmFifo *fifo = (rglGcmFifo*)&rglGcmState_i.fifo;
         GLuint spaceInWords = cgprog->constantPushBufferWordSize + 4 + 32;

         // Push a CG program onto the current command buffer

         // make sure there is space for the pushbuffer + any nops we need to add for alignment  
         if ( fifo->current + spaceInWords + 1024 > fifo->end )
            rglOutOfSpaceCallback( fifo, spaceInWords );

         // first add nops to get us the next alligned position in the fifo 
         // [YLIN] Use VMX register to copy
         uint32_t padding_in_word = ( ( 0x10-(((uint32_t)rglGcmState_i.fifo.current)&0xf))&0xf )>>2;
         uint32_t padded_size = ( ((cgprog->constantPushBufferWordSize)<<2) + 0xf )&~0xf;

         unsigned i;
         rglGcmSetNopCommand(thisContext, i, padding_in_word );
         memcpy16(rglGcmState_i.fifo.current, cgprog->constantPushBuffer, padded_size);
         rglGcmState_i.fifo.current+=cgprog->constantPushBufferWordSize;
      }

      if (RGL_UNLIKELY(needValidate & RGL_VALIDATE_FRAGMENT_PROGRAM))
      {
         // Set up the current fragment program on hardware

         rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;
         _CGprogram *program = LContext->BoundFragmentProgram;

         // params are set directly in the GPU memory, so there is nothing to be done here.
         CellCgbFragmentProgramConfiguration conf;

         conf.offset = gmmIdToOffset(program->loadProgramId) + program->loadProgramOffset;

         rglGcmInterpolantState *s = &rglGcmState_i.state.interpolant;
         s->fragmentProgramAttribMask |= program->header.attributeInputMask | CELL_GCM_ATTRIB_OUTPUT_MASK_POINTSIZE;

         conf.attributeInputMask = ( s->vertexProgramAttribMask) &
            s->fragmentProgramAttribMask;

         conf.texCoordsInputMask = program->header.fragmentProgram.texcoordInputMask;
         conf.texCoords2D = program->header.fragmentProgram.texcoord2d;
         conf.texCoordsCentroid = program->header.fragmentProgram.texcoordCentroid;

         int fragmentControl = ( 1 << 15 ) | ( 1 << 10 );
         fragmentControl |= program->header.fragmentProgram.flags & CGF_DEPTHREPLACE ? 0xE : 0x0;
         fragmentControl |= program->header.fragmentProgram.flags & CGF_OUTPUTFROMH0 ? 0x00 : 0x40;
         fragmentControl |= program->header.fragmentProgram.flags & CGF_PIXELKILL ? 0x80 : 0x00;

         conf.fragmentControl  = fragmentControl;
         conf.registerCount = program->header.fragmentProgram.registerCount < 2 ? 2 : program->header.fragmentProgram.registerCount;

         conf.fragmentControl &= ~CELL_GCM_MASK_SET_SHADER_CONTROL_CONTROL_TXP; 
         /* TODO - look into this */
         conf.fragmentControl |= 0 << CELL_GCM_SHIFT_SET_SHADER_CONTROL_CONTROL_TXP; 

         rglGcmSetFragmentProgramLoad(thisContext, &conf, CELL_GCM_LOCATION_LOCAL);

         rglGcmSetZMinMaxControl(thisContext, ( program->header.fragmentProgram.flags & CGF_DEPTHREPLACE ) ? RGLGCM_FALSE : RGLGCM_TRUE, RGLGCM_FALSE, RGLGCM_FALSE );

         driver->fpLoadProgramId = program->loadProgramId;
         driver->fpLoadProgramOffset = program->loadProgramOffset;
      }

      if ( RGL_LIKELY(( needValidate & ~( RGL_VALIDATE_TEXTURES_USED |
                     RGL_VALIDATE_VERTEX_PROGRAM |
                     RGL_VALIDATE_VERTEX_CONSTANTS |
                     RGL_VALIDATE_FRAGMENT_PROGRAM ) ) == 0 ) )
      {
         LContext->needValidate = 0;
         goto beginning;
      }

      if ( RGL_UNLIKELY( needValidate & RGL_VALIDATE_VIEWPORT ) )
      {
         rglGcmViewportState *v = &rglGcmState_i.state.viewport;
         v->x = LContext->ViewPort.X;
         v->y = LContext->ViewPort.Y;
         v->w = LContext->ViewPort.XSize;
         v->h = LContext->ViewPort.YSize;

         rglGcmFifoGlViewport(v, LContext->DepthNear, LContext->DepthFar);
      }


      if (RGL_UNLIKELY(needValidate & RGL_VALIDATE_BLENDING ))
      {
         if ((LContext->Blending))
         {
            GCM_FUNC( cellGcmSetBlendEnable, LContext->Blending );

            rglGcmBlendState *blend = &rglGcmState_i.state.blend;
            GLuint hwColor;

            blend->r = LContext->BlendColor.R;
            blend->g = LContext->BlendColor.G;
            blend->b = LContext->BlendColor.B;
            blend->a = LContext->BlendColor.A;

            if (rglGcmState_i.renderTarget.colorFormat == RGLGCM_ARGB8)
            {
               RGLGCM_CALC_COLOR_LE_ARGB8( &hwColor, blend->r, blend->g, blend->b, blend->a );
               GCM_FUNC( cellGcmSetBlendColor, hwColor, hwColor );
            }

            GCM_FUNC( cellGcmSetBlendEquation, (rglGcmEnum)LContext->BlendEquationRGB,
                  (rglGcmEnum)LContext->BlendEquationAlpha );
            GCM_FUNC( cellGcmSetBlendFunc, (rglGcmEnum)LContext->BlendFactorSrcRGB,(rglGcmEnum)LContext->BlendFactorDestRGB,(rglGcmEnum)LContext->BlendFactorSrcAlpha,(rglGcmEnum)LContext->BlendFactorDestAlpha);
         }
      }

#if 0
      if ( RGL_UNLIKELY( needValidate & RGL_VALIDATE_SHADER_SRGB_REMAP ) )
      {
         GCM_FUNC( cellGcmSetFragmentProgramGammaEnable, LContext->ShaderSRGBRemap ? CELL_GCM_TRUE : CELL_GCM_FALSE); 
         LContext->needValidate &= ~RGL_VALIDATE_SHADER_SRGB_REMAP;
      }
#endif

      LContext->needValidate = 0;
   }

beginning:

   GLboolean slowPath = rglPlatformRequiresSlowPath( dparams, 0, 0);
   (void)slowPath;

   rglGcmDriver *driver = (rglGcmDriver*)_CurrentDevice->rasterDriver;

   if (RGL_UNLIKELY(!driver->rtValid))
      return;

   // check for any writable mapped buffers
   if ( driver->flushBufferCount != 0 )
      driver->invalidateVertexCache = GL_TRUE;

   GLuint gpuOffset = GMM_ERROR;

   uint32_t totalXfer = 0;
   for ( GLuint i = 0; i < RGL_MAX_VERTEX_ATTRIBS; ++i )
      totalXfer += dparams->attribXferSize[i];

   // validates attributes for specified draw paramaters
   // gpuOffset is pointer to index buffer
   rglAttributeState* as = LContext->attribs;

   // allocate upload transfer buffer if necessary
   //  The higher level bounce buffer allocator is used, which means that
   //  the buffer will automatically be freed after all RGLGCM calls up to
   //  the next allocation have finished.
   void* xferBuffer = NULL;
   GLuint xferId = GMM_ERROR;
   GLuint VBOId = GMM_ERROR;
   if ( RGL_UNLIKELY( dparams->xferTotalSize ) )
   {
      xferId = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo,
            0, dparams->xferTotalSize);
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
            GLsizei stride = attrib->clientStride;
            const GLuint freq = attrib->frequency;

            if ( RGL_UNLIKELY( dparams->attribXferSize[i] ) )
            {
               // attribute data is client side, need to transfer

               // don't transfer data that is not going to be used, from 0 to first*stride
               GLuint offset = ( dparams->firstVertex / freq ) * stride;

               char * b = (char*)xferBuffer + dparams->attribXferOffset[i];
               memcpy(b + offset, (char*)attrib->clientData + offset,
                     dparams->attribXferSize[i] - offset);

               // draw directly from bounce buffer
               gpuOffset = gmmIdToOffset(xferId) + (b - ( char * )xferBuffer);

            }
            else
            {
               // attribute data in VBO, clientData is offset.
               VBOId = rglGcmGetBufferObjectOrigin( attrib->arrayBuffer );
               gpuOffset = gmmIdToOffset(VBOId)
                  + (( const GLubyte* )attrib->clientData - ( const GLubyte* )NULL );
            }

            rglGcmEnum       type = (rglGcmEnum)attrib->clientType;
            GLint           size = attrib->clientSize;

            // syntax check
            switch ( size )
            {
               case 0: // disable
                  stride = 0;
                  attrib->normalized = 0;
                  type = RGLGCM_FLOAT;
                  gpuOffset = 0;
                  break;
               case 1:
               case 2:
               case 3:
               case 4:
                  // valid
                  break;
               default:
                  break;
            }

            // mapping to native types
            uint8_t gcmType = 0;
            switch ( type )
            {
               case RGLGCM_UNSIGNED_BYTE:
                  if (attrib->normalized)
                     gcmType = CELL_GCM_VERTEX_UB;
                  else
                     gcmType = CELL_GCM_VERTEX_UB256;
                  break;

               case RGLGCM_SHORT:
                  gcmType = attrib->normalized ? CELL_GCM_VERTEX_S1 : CELL_GCM_VERTEX_S32K;
                  break;

               case RGLGCM_FLOAT:
                  gcmType = CELL_GCM_VERTEX_F;
                  break;

               case RGLGCM_HALF_FLOAT:
                  gcmType = CELL_GCM_VERTEX_SF;
                  break;

               case RGLGCM_CMP:
                  size = 1;   // required for this format
                  gcmType = CELL_GCM_VERTEX_CMP;
                  break;

               default:
                  break;
            }

            rglGcmSetVertexDataArray(thisContext, i, freq, stride, size, gcmType, CELL_GCM_LOCATION_LOCAL, gpuOffset );
         }
         else
         {
            // attribute is disabled
            rglGcmSetVertexDataArray(thisContext, i, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
            rglGcmSetVertexData4f(thisContext, i, attrib->value );
         }
      }
      driver->invalidateVertexCache = GL_TRUE;
   }
   as->DirtyMask = 0;	// all attributes are now clean

   if ( xferId != GMM_ERROR )
      gmmFree( xferId );

   if ( driver->invalidateVertexCache )
   {
      driver->invalidateVertexCache = GL_FALSE;
      rglGcmSetInvalidateVertexCache(thisContext);
   }

   GCM_FUNC( cellGcmSetUpdateFragmentProgramParameter, 
         gmmIdToOffset( driver->fpLoadProgramId ) + driver->fpLoadProgramOffset );

   uint8_t gcmMode = 0;

   switch (dparams->mode)
   {
      case RGLGCM_POINTS:
         gcmMode = CELL_GCM_PRIMITIVE_POINTS;
         break;
      case RGLGCM_LINES:
         gcmMode = CELL_GCM_PRIMITIVE_LINES;
         break;
      case RGLGCM_LINE_LOOP:
         gcmMode = CELL_GCM_PRIMITIVE_LINE_LOOP;
         break;
      case RGLGCM_LINE_STRIP:
         gcmMode = CELL_GCM_PRIMITIVE_LINE_STRIP;
         break;
      case RGLGCM_TRIANGLES:
         gcmMode = CELL_GCM_PRIMITIVE_TRIANGLES;
         break;
      case RGLGCM_TRIANGLE_STRIP:
         gcmMode = CELL_GCM_PRIMITIVE_TRIANGLE_STRIP;
         break;
      case RGLGCM_TRIANGLE_FAN:
         gcmMode = CELL_GCM_PRIMITIVE_TRIANGLE_FAN;
         break;
      case RGLGCM_QUADS:
         gcmMode = CELL_GCM_PRIMITIVE_QUADS;
         break;
      case RGLGCM_QUAD_STRIP:
         gcmMode = CELL_GCM_PRIMITIVE_QUAD_STRIP;
         break;
      case RGLGCM_POLYGON:
         gcmMode = CELL_GCM_PRIMITIVE_POLYGON;
         break;
   }

   rglGcmSetDrawArrays(thisContext, gcmMode, dparams->firstVertex, dparams->vertexCount );
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

/*============================================================
  PLATFORM TEXTURE
  ============================================================ */

// Calculate required size in bytes for given texture layout
static GLuint rglGetGcmTextureSize (void *data)
{
   rglGcmTextureLayout *layout = (rglGcmTextureLayout*)data;
   GLuint bytesNeeded = 0;
   GLuint faceAlign = layout->pitch ? 1 : 128;

   GLuint width = layout->baseWidth;
   GLuint height = layout->baseHeight;

   width = MAX(1U, width );
   height = MAX(1U, height );

   if ( !layout->pitch )
      bytesNeeded += layout->pixelBits * width * height / 8;
   else
      bytesNeeded += height * layout->pitch;

   return rglPad( bytesNeeded, faceAlign );
}

// Calculate pitch for a texture
// TransferVid2Vid needs 64byte pitch alignment
#define GET_TEXTURE_PITCH(texture) (rglPad(rglGetPixelSize(texture->image->format, texture->image->type) * texture->image->width, 64))

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

// Validate texture resources
static void rglPlatformValidateTextureResources (void *data)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
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
      // Reallocate texture based on usage, pool system, and strategy
      rglGcmTexture *gcmTexture = ( rglGcmTexture * )texture->platformTexture;

      // select the allocation strategy
      GLboolean done = GL_FALSE;
      GLuint size = 0;
      GLuint id = GMM_ERROR;

      if (texture->usage == GL_TEXTURE_LINEAR_SYSTEM_SCE ||
            texture->usage == GL_TEXTURE_SWIZZLED_SYSTEM_SCE)
         done = GL_TRUE;

      const rglGcmTextureLayout currentLayout = gcmTexture->gpuLayout;
      const GLuint currentSize = gcmTexture->gpuSize;

      if (!done)
      {
         rglGcmTextureLayout newLayout;

         // get layout and size compatible with this pool
         rglImage *image = texture->image;

         newLayout.levels = 1;
         newLayout.faces = 1;
         newLayout.baseWidth = image->width;
         newLayout.baseHeight = image->height;
         newLayout.baseDepth = image->depth;
         newLayout.internalFormat = ( rglGcmEnum )image->internalFormat;
         newLayout.pixelBits = rglPlatformGetBitsPerPixel( newLayout.internalFormat );
         newLayout.pitch = GET_TEXTURE_PITCH(texture);

         size = rglGetGcmTextureSize( &newLayout );

         if ( currentSize >= size && newLayout.pitch == currentLayout.pitch )
            gcmTexture->gpuLayout = newLayout;
         else
         {
            rglPlatformDropTexture( texture );

            // allocate in the specified pool
            id = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo, 
                  0, size);

            // set new
            gcmTexture->pool = RGLGCM_SURFACE_POOL_LINEAR;
            gcmTexture->gpuAddressId = id;
            gcmTexture->gpuAddressIdOffset = 0;
            gcmTexture->gpuSize = size;
            gcmTexture->gpuLayout = newLayout;

         }
      }
      rglTextureTouchFBOs( texture );

      // Upload texture from host memory to GPU memory
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
            dataId:      GMM_ERROR,
            dataIdOffset:0,
      };

      // use a bounce buffer to transfer to GPU
      GLuint bounceBufferId = GMM_ERROR;

      // check if upload is needed for this image
      rglImage *image = texture->image;

      if ( image->dataState == RGL_IMAGE_DATASTATE_HOST )
      {
         // lazy allocation of bounce buffer
         if ( bounceBufferId == GMM_ERROR && layout->baseDepth == 1 )
            bounceBufferId = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo,
                  0, gcmTexture->gpuSize);

         if ( bounceBufferId != GMM_ERROR )
         {
            // copy image to bounce buffer
            src.dataId = bounceBufferId;
            src.dataIdOffset = 0;

            // NPOT DXT
            memcpy( gmmIdToAddress( src.dataId ), image->data, image->storageSize );
         }

         // use surface copy functions
         src.width = image->width;
         src.height = image->height;
         src.pitch = pixelBytes * src.width;

         dst.width = src.width;
         dst.height = image->height;
         dst.dataId = gcmTexture->gpuAddressId;
         dst.dataIdOffset = gcmTexture->gpuAddressIdOffset;

         GLuint width = src.width;
         GLuint height = src.height;
         const GLuint srcPitch = src.pitch ? src.pitch : src.bpp * src.width;
         const GLuint dstPitch = dst.pitch ? dst.pitch : dst.bpp * dst.width;

         bool bpp_1_transferdata = src.bpp == 1 && 
            (!(( width % 2 ) == 0 ));

         if (( srcPitch >= 0x10000 ) || ( dstPitch >= 0x10000 ) || bpp_1_transferdata )
         {
            rglGcmTransferData( dst.dataId, dst.dataIdOffset, dstPitch,
                  src.dataId, src.dataIdOffset, srcPitch,
                  width * src.bpp, height );
         }
         else
         {
            switch ( src.bpp )
            {
               case 1:
                  width /= 2;
                  src.bpp = 2;
                  break;
               case 8:
               case 16:
                  src.bpp /= 4;
                  width *= 4;
                  break;
            }

            rglGcmSetTransferImage(gCellGcmCurrentContext, CELL_GCM_TRANSFER_LOCAL_TO_LOCAL, gmmIdToOffset(dst.dataId) + dst.dataIdOffset, dstPitch, 0, 0, gmmIdToOffset(src.dataId) + src.dataIdOffset, srcPitch, 0, 0, width, height, src.bpp);
         }

         // free CPU copy of data
         rglImageFreeCPUStorage( image );
         image->dataState |= RGL_IMAGE_DATASTATE_GPU;
      } // newer data on host

      if ( bounceBufferId != GMM_ERROR )
         gmmFree( bounceBufferId );

      rglGcmSetInvalidateTextureCache(thisContext, CELL_GCM_INVALIDATE_TEXTURE );
   }

   // gcmTexture method command
   // map RGL internal types to GCM
   rglGcmTexture *platformTexture = ( rglGcmTexture * )texture->platformTexture;
   rglGcmTextureLayout *layout = &platformTexture->gpuLayout;

   // XXX make sure that REVALIDATE_PARAMETERS is set if the format of the texture changes
   // revalidate the texture registers cache just to ensure we are in the correct filtering mode
   // based on the internal format.

   // -----------------------------------------------------------------------
   // map the SET_TEXTURE_FILTER method.
   platformTexture->gcmMethods.filter.min = rglGcmMapMinTextureFilter(texture->minFilter);
   platformTexture->gcmMethods.filter.mag = (texture->magFilter == GL_NEAREST) 
      ? CELL_GCM_TEXTURE_NEAREST : CELL_GCM_TEXTURE_LINEAR;
   platformTexture->gcmMethods.filter.conv = CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX;
   // We don't actually expose this, but still need to set it up properly incase we expose this later
   // hw expects a 5.8 twos-complement fixed-point // XXX  what is the - .26f ?
   platformTexture->gcmMethods.filter.bias = ( GLint )(( texture->lodBias - .26f ) * 256.0f );

   // -----------------------------------------------------------------------
   // set the SET_TEXTURE_CONTROL0 params
   platformTexture->gcmMethods.control0.maxAniso = CELL_GCM_TEXTURE_MAX_ANISO_1;
   const GLfloat minLOD = MAX( texture->minLod, 0);
   const GLfloat maxLOD = MIN( texture->maxLod, texture->maxLevel );
   platformTexture->gcmMethods.control0.minLOD = ( GLuint )( MAX( minLOD, 0 ) * 256.0f );
   platformTexture->gcmMethods.control0.maxLOD = ( GLuint )( MIN( maxLOD, layout->levels ) * 256.0f );

   // -----------------------------------------------------------------------
   // set the SET_TEXTURE_ADDRESS method params.
   platformTexture->gcmMethods.address.wrapS = rglGcmMapWrapMode( texture->wrapS );
   platformTexture->gcmMethods.address.wrapT = rglGcmMapWrapMode( texture->wrapT );
   platformTexture->gcmMethods.address.wrapR = rglGcmMapWrapMode( texture->wrapR );
   platformTexture->gcmMethods.address.unsignedRemap = CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL;

#if 0
   // now for gamma remap
   GLuint gamma = 0;
   GLuint remap = texture->gammaRemap;
   gamma |= ( remap & RGLGCM_GAMMA_REMAP_RED_BIT ) 		? CELL_GCM_TEXTURE_GAMMA_R : 0;
   gamma |= ( remap & RGLGCM_GAMMA_REMAP_GREEN_BIT )	? CELL_GCM_TEXTURE_GAMMA_G : 0;
   gamma |= ( remap & RGLGCM_GAMMA_REMAP_BLUE_BIT ) 	? CELL_GCM_TEXTURE_GAMMA_B : 0;
   gamma |= ( remap & RGLGCM_GAMMA_REMAP_ALPHA_BIT ) 	? CELL_GCM_TEXTURE_GAMMA_A : 0;

   platformTexture->gcmMethods.address.gamma = gamma;
#else
   platformTexture->gcmMethods.address.gamma = 0;
#endif

   // set border colors
   RGLGCM_CALC_COLOR_LE_ARGB8(&(platformTexture->gcmMethods.borderColor), 
         texture->borderColor.R, texture->borderColor.G, texture->borderColor.B, texture->borderColor.A); 

   // -----------------------------------------------------------------------
   // setup the GcmTexture
   // format, control1, control3, imagerect; setup for cellGcmSetTexture later

   // map RGL internal types to GCM
   // use color format for depth with no compare mode
   //  This hack is needed because the hardware will not read depth
   //  textures without performing a compare.  The depth value will need to
   //  be reconstructed in the shader from the color components.
   GLuint internalFormat = layout->internalFormat;

   // set the format and remap( control 1)
   uint8_t *gcmFormat = &platformTexture->gcmTexture.format;
   uint32_t *remap = &platformTexture->gcmTexture.remap;

   *gcmFormat = 0;

   switch (internalFormat)
   {
      case RGLGCM_ALPHA8:                 // in_rgba = xxAx, out_rgba = 000A
         {
            *gcmFormat =  CELL_GCM_TEXTURE_B8;
            *remap = CELL_GCM_REMAP_MODE(
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
      case RGLGCM_ALPHA16:                // in_rgba = xAAx, out_rgba = 000A
         {
            *gcmFormat =  CELL_GCM_TEXTURE_X16;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_ZERO,
                  CELL_GCM_TEXTURE_REMAP_ZERO,
                  CELL_GCM_TEXTURE_REMAP_ZERO );

         }
         break;
#if 0
      case RGLGCM_HILO8:                  // in_rgba = HLxx, out_rgba = HL11
         {
            *gcmFormat =  CELL_GCM_TEXTURE_COMPRESSED_HILO8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_ONE,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_ONE );

         }
         break;
      case RGLGCM_HILO16:                 // in_rgba = HLxx, out_rgba = HL11
         {
            *gcmFormat =  CELL_GCM_TEXTURE_Y16_X16;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_ONE,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_ONE );

         }
         break;
#endif
      case RGLGCM_ARGB8:                  // in_rgba = RGBA, out_rgba = RGBA
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A8R8G8B8;
            *remap = CELL_GCM_REMAP_MODE(
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
#if 0
      case RGLGCM_BGRA8:                  // in_rgba = GRAB, out_rgba = RGBA ** NEEDS TO BE TESTED
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A8R8G8B8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP );

         }
         break;
      case RGLGCM_RGBA8:                  // in_rgba = GBAR, out_rgba = RGBA ** NEEDS TO BE TESTED
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A8R8G8B8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP );
         }
         break;
      case RGLGCM_ABGR8:                  // in_rgba = BGRA, out_rgba = RGBA  ** NEEDS TO BE TESTED
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A8R8G8B8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP );

         }
         break;
      case RGLGCM_RGBX8:                  // in_rgba = BGRA, out_rgba = RGB1  ** NEEDS TO BE TESTED
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A8R8G8B8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_ONE,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP );

         }
         break;
      case RGLGCM_XBGR8:                  // in_rgba = BGRA, out_rgba = RGB1  ** NEEDS TO BE TESTED
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A8R8G8B8;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_ONE,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_REMAP );

         }
         break;
      case RGLGCM_FLOAT_R32:              // in_rgba = Rxxx, out_rgba = R001
         {
            *gcmFormat =  CELL_GCM_TEXTURE_X32_FLOAT;
            *remap = CELL_GCM_REMAP_MODE(
                  CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
                  CELL_GCM_TEXTURE_REMAP_FROM_A,
                  CELL_GCM_TEXTURE_REMAP_FROM_R,
                  CELL_GCM_TEXTURE_REMAP_FROM_G,
                  CELL_GCM_TEXTURE_REMAP_FROM_B,
                  CELL_GCM_TEXTURE_REMAP_ONE,
                  CELL_GCM_TEXTURE_REMAP_REMAP,
                  CELL_GCM_TEXTURE_REMAP_ZERO,
                  CELL_GCM_TEXTURE_REMAP_ZERO );

         }
         break;
      case RGLGCM_RGB5_A1_SCE:          // in_rgba = RGBA, out_rgba = RGBA
         {
            *gcmFormat =  CELL_GCM_TEXTURE_A1R5G5B5;
            *remap = CELL_GCM_REMAP_MODE(
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
#endif
      case RGLGCM_RGB565_SCE:          // in_rgba = RGBA, out_rgba = RGBA
         {
            *gcmFormat =  CELL_GCM_TEXTURE_R5G6B5;
            *remap = CELL_GCM_REMAP_MODE(
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
   }

   // This is just to cover the conversion from swizzled to linear
   if(layout->pitch)
      platformTexture->gcmTexture.format += 0x20; // see class doc definitions for SZ_NR vs LN_NR...

   platformTexture->gcmTexture.width = layout->baseWidth;
   platformTexture->gcmTexture.height = layout->baseHeight;
   platformTexture->gcmTexture.depth = layout->baseDepth;
   platformTexture->gcmTexture.pitch = layout->pitch;
   platformTexture->gcmTexture.mipmap = layout->levels;
   platformTexture->gcmTexture.cubemap = CELL_GCM_FALSE;
   platformTexture->gcmTexture.dimension = CELL_GCM_TEXTURE_DIMENSION_2;
   platformTexture->gcmTexture.location = CELL_GCM_LOCATION_LOCAL;

   texture->revalidate = 0;
}

// Choose internal storage type and size, and set it to image, based on given format
GLenum rglPlatformChooseInternalStorage (void *data, GLenum internalFormat )
{
   rglImage *image = (rglImage*)data;

   // see note at bottom concerning storageSize
   image->storageSize = 0;

   GLenum *format = &image->format;
   GLenum *type = &image->type;

   // Choose internal format closest to given format
   // and extract right format, type

   switch (internalFormat)
   {
      case GL_ALPHA12:
      case GL_ALPHA16:
         internalFormat = RGLGCM_ALPHA16;
         *format = GL_ALPHA;
         *type = GL_UNSIGNED_SHORT;
         break;
      case GL_ALPHA:
      case GL_ALPHA4:
         internalFormat = RGLGCM_ALPHA8;
         *format = GL_ALPHA;
         *type = GL_UNSIGNED_BYTE;
         break;
      case GL_R3_G3_B2:
      case GL_RGB4:
      case GL_RGB:
      case GL_RGB8:
      case RGLGCM_RGBX8:
         internalFormat = RGLGCM_RGBX8;
         *format = GL_RGBA;
         *type = GL_UNSIGNED_INT_8_8_8_8;
         break;
      case GL_RGBA2:
      case GL_RGBA4:
      case GL_RGBA8:
      case GL_RGBA:
         internalFormat = RGLGCM_RGBA8;
         *format = GL_RGBA;
         *type = GL_UNSIGNED_INT_8_8_8_8;
         break;
      case GL_RGB5_A1:
         internalFormat = RGLGCM_RGB5_A1_SCE;
         *format = GL_RGBA;
         *type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
         break;
      case GL_RGB5:
         internalFormat = RGLGCM_RGB565_SCE;
         *format = GL_RGB;
         *type = GL_UNSIGNED_SHORT_5_6_5_REV;
         break;
      case GL_BGRA:
      case RGLGCM_BGRA8:
         internalFormat = RGLGCM_BGRA8;
         *format = GL_BGRA;
         *type = GL_UNSIGNED_INT_8_8_8_8;
         break;
      case GL_ARGB_SCE:
         internalFormat = RGLGCM_ARGB8;
         *format = GL_BGRA;
         *type = GL_UNSIGNED_INT_8_8_8_8_REV;
         break;
      case GL_RGBA32F_ARB:
      default:
         return GL_INVALID_ENUM;
   }

   image->internalFormat = internalFormat;

   // Note that it is critical to get the storageSize value correct because
   // this member is used to configure texture loads and unloads.  If this
   // value is wrong (e.g. contains unnecessary padding) it will corrupt
   // the GPU memory layout.
   image->storageSize = rglGetPixelSize(image->format, image->type) *
      image->width * image->height * image->depth;

   return GL_NO_ERROR;
}

GLAPI void APIENTRY glTextureReferenceSCE( GLenum target, GLuint levels,
      GLuint baseWidth, GLuint baseHeight, GLuint baseDepth, GLenum internalFormat, GLuint pitch, GLintptr offset )
{
   RGLcontext*	LContext = _CurrentContext;
   rglImage *image;

   rglTexture *texture = rglGetCurrentTexture( LContext->CurrentImageUnit, target );
   rglBufferObject *bufferObject = 
      (rglBufferObject*)LContext->bufferObjectNameSpace.data[LContext->TextureBuffer];
   rglReallocateImages( texture, 0, MAX( baseWidth, MAX( baseHeight, baseDepth ) ) );

   image = texture->image;

   image->width = baseWidth;
   image->height = baseHeight;
   image->depth = baseDepth;
   image->alignment = LContext->unpackAlignment;

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

   image->xstride = rglGetPixelSize( image->format, image->type );
   image->ystride = image->width * image->xstride;
   image->zstride = image->height * image->ystride;

   image->dataState = RGL_IMAGE_DATASTATE_UNSET;

   texture->maxLevel = 0;
   texture->usage = GL_TEXTURE_LINEAR_GPU_SCE;

   // Implementation of texture reference
   // Associate bufferObject to texture by assigning buffer's gpu address to the gcm texture 
   rglGcmTexture *gcmTexture = ( rglGcmTexture * )texture->platformTexture;

   // XXX check pitch restrictions ?

   rglGcmTextureLayout newLayout;

   newLayout.levels = 1;
   newLayout.faces = 1;
   newLayout.baseWidth = image->width;
   newLayout.baseHeight = image->height;
   newLayout.baseDepth = image->depth;
   newLayout.internalFormat = ( rglGcmEnum )image->internalFormat;
   newLayout.pixelBits = rglPlatformGetBitsPerPixel( newLayout.internalFormat );
   newLayout.pitch = pitch ? pitch : GET_TEXTURE_PITCH(texture);

   texture->isRenderTarget = GL_FALSE;

   if ( gcmTexture->gpuAddressId != GMM_ERROR )
      rglPlatformDestroyTexture( texture );

   rglGcmBufferObject *gcmBuffer = (rglGcmBufferObject*)&bufferObject->platformBufferObject;

   gcmTexture->gpuLayout = newLayout;
   gcmTexture->pool = gcmBuffer->pool;
   gcmTexture->gpuAddressId = gcmBuffer->bufferId;
   gcmTexture->gpuAddressIdOffset = offset;
   gcmTexture->gpuSize = rglGetGcmTextureSize( &newLayout );

   texture->revalidate &= ~(RGL_TEXTURE_REVALIDATE_LAYOUT | RGL_TEXTURE_REVALIDATE_IMAGES);
   texture->revalidate |= RGL_TEXTURE_REVALIDATE_PARAMETERS;
   rglTextureTouchFBOs( texture );
   
   bufferObject->textureReferences.pushBack( texture );
   texture->referenceBuffer = bufferObject;
   texture->offset = offset;
   rglTextureTouchFBOs( texture );
   LContext->needValidate |= RGL_VALIDATE_TEXTURES_USED;
}

// Set current render target to args
void rglGcmFifoGlSetRenderTarget (const void *data)
{
   CellGcmContextData *thisContext = (CellGcmContextData*)gCellGcmCurrentContext;
   rglGcmRenderTarget *rt = &rglGcmState_i.renderTarget;
   CellGcmSurface *grt = &rglGcmState_i.renderTarget.gcmRenderTarget;
   const rglGcmRenderTargetEx *args = (const rglGcmRenderTargetEx*)data;

   // GlSetRenderTarget implementation starts here

   // Render target rt's color and depth buffer parameters are updated with args
   // Fifo functions are called as required
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
      GLuint hwColor;

      rt->colorFormat  = args->colorFormat;

      if (rglGcmState_i.renderTarget.colorFormat == RGLGCM_ARGB8)
      {
         RGLGCM_CALC_COLOR_LE_ARGB8( &hwColor, blend->r, blend->g, blend->b, blend->a );
         GCM_FUNC( cellGcmSetBlendColor, hwColor, hwColor );
      }
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

   // Update rt's color and depth format with args
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

   // Update rt's AA and Swizzling parameters with args

   rglGcmSetAntiAliasingControl(thisContext,
         CELL_GCM_FALSE, 
         CELL_GCM_FALSE,
         CELL_GCM_FALSE, 
         0xFFFF);

   grt->type = CELL_GCM_SURFACE_PITCH;

   // Update rt's color targets
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

   // ensure if either width or height is 1 the other is one as well
   if (grt->width == 1)
      grt->height = 1; 
   else if (grt->height == 1)
      grt->width = 1; 

   uint32_t log2Width = 31 - ({__asm__("cntlzw %0,%1" : "=r" (log2Width) : "r" (grt->width)); log2Width;});
   uint32_t log2Height = 31 - ({__asm__("cntlzw %0,%1" : "=r" (log2Height) : "r" (grt->height)); log2Height;});
   rglGcmSetSurface(thisContext, grt, CELL_GCM_WINDOW_ORIGIN_BOTTOM, CELL_GCM_WINDOW_PIXEL_CENTER_HALF, log2Width, log2Height);
}

GLAPI void APIENTRY glPixelStorei( GLenum pname, GLint param )
{
   RGLcontext*	LContext = _CurrentContext;

    switch ( pname )
    {
        case GL_PACK_ALIGNMENT:
            LContext->packAlignment = param;
            break;
        case GL_UNPACK_ALIGNMENT:
            LContext->unpackAlignment = param;
            break;
    }
}
