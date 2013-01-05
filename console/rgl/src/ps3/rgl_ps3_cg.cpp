/*  RetroArch - A frontend for libretro.
 *  RGL - An OpenGL subset wrapper library.
 *  Copyright (C) 2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2012 - Daniel De Matteis
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

#include "../rgl_cg.h"

CGbool rglpSupportsVertexProgram( CGprofile p )
{
   if ( p == CG_PROFILE_SCE_VP_TYPEB )
      return CG_TRUE;
   if ( p == CG_PROFILE_SCE_VP_TYPEC )
      return CG_TRUE;
   if ( p == CG_PROFILE_SCE_VP_RSX )
      return CG_TRUE;
   return CG_FALSE;
}

CGbool rglpSupportsFragmentProgram( CGprofile p )
{
   if ( p == CG_PROFILE_SCE_FP_TYPEB )
      return CG_TRUE;
   if ( CG_PROFILE_SCE_FP_RSX == p )
      return CG_TRUE;
   return CG_FALSE;
}

CGprofile rglpGetLatestProfile( CGGLenum profile_type )
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

// uploads the given fp shader to gpu memory. Allocates if needed.
// This also builds the shared constants push buffer if needed, since it depends on the load address
static int rglpsLoadFPShader (void *data)
{
   _CGprogram *program = (_CGprogram*)data;
   unsigned int ucodeSize = program->header.instructionCount * 16;

   if ( program->loadProgramId == GMM_ERROR )
   {
      program->loadProgramId = gmmAlloc((CellGcmContextData*)&rglGcmState_i.fifo,
            CELL_GCM_LOCATION_LOCAL, 0, ucodeSize);
      program->loadProgramOffset = 0;
   }

   rglGcmSend( program->loadProgramId, program->loadProgramOffset, 0, ( char* )program->ucode, ucodeSize );
   return GL_TRUE;
}

static void rglpsUnloadFPShader (void *data)
{
   _CGprogram *program = (_CGprogram*)data;

   if ( program->loadProgramId != GMM_ERROR )
   {
      gmmFree( program->loadProgramId );
      program->loadProgramId = GMM_ERROR;
      program->loadProgramOffset = 0;
   }
}

//new binary addition

int rglGcmGenerateProgram (void *data, int profileIndex, const CgProgramHeader *programHeader, const void *ucode, const CgParameterTableHeader *parameterHeader,
      const CgParameterEntry *parameterEntries, const char *stringTable, const float *defaultValues )
{
   _CGprogram *program = (_CGprogram*)data;
   CGprofile profile = ( CGprofile )programHeader->profile;

   int need_swapping = 0;

   //hack to counter removal of TypeC during beta
   if ( profile == ( CGprofile )7005 )
      profile = CG_PROFILE_SCE_VP_RSX;
   if ( profile == ( CGprofile )7006 )
      profile = CG_PROFILE_SCE_FP_RSX;

   // if can't match a known profile, the data may be in wrong endianness
   if (( profile != CG_PROFILE_SCE_FP_TYPEB ) && ( profile != CG_PROFILE_SCE_VP_TYPEB ) &&
         ( profile != CG_PROFILE_SCE_FP_RSX ) && ( profile != CG_PROFILE_SCE_VP_RSX ) )
   {
      need_swapping = 1;
   }

   // check that this program block is of the right revision
   // i.e. that the cgBinary.h header hasn't changed since it was
   // compiled.

   // validate the profile
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
      rglCgRaiseError( CG_UNKNOWN_PROFILE_ERROR );
      return 0;
   }

   memcpy( &program->header, programHeader, sizeof( program->header ) );

   program->ucode = ucode;
   program->loadProgramId = GMM_ERROR;
   program->loadProgramOffset = 0;
   program->inLocalMemory = true; 


   size_t parameterSize = parameterHeader->entryCount * sizeof( CgRuntimeParameter );
   void *memoryBlock;
   if ( parameterSize )
      memoryBlock = memalign( 16, parameterSize );
   else
      memoryBlock = NULL;

   program->rtParametersCount = parameterHeader->entryCount;
   program->runtimeParameters = ( CgRuntimeParameter* )memoryBlock;

   if ( parameterEntries == NULL ) // the param entry can be supplied if not right after parameterHeader in memory, it happens when there's a program copy
      parameterEntries = ( CgParameterEntry* )( parameterHeader + 1 );

   program->parametersEntries = parameterEntries;
   program->parameterResources = ( char* )( program->parametersEntries + program->rtParametersCount );
   program->resources = ( unsigned short* )(( char* )program->parametersEntries + ( parameterHeader->resourceTableOffset - sizeof( CgParameterTableHeader ) ) );
   program->defaultValuesIndexCount = parameterHeader->defaultValueIndexCount;
   program->defaultValuesIndices = ( CgParameterDefaultValue* )(( char* )program->parametersEntries + ( parameterHeader->defaultValueIndexTableOffset - sizeof( CgParameterTableHeader ) ) );
   program->semanticCount = parameterHeader->semanticIndexCount;
   program->semanticIndices = ( CgParameterSemantic* )( program->defaultValuesIndices + program->defaultValuesIndexCount );

   program->defaultValues = NULL;

   memset( program->runtimeParameters, 0, parameterHeader->entryCount*sizeof( CgRuntimeParameter ) );

   //string table
   program->stringTable = stringTable;
   //default values
   program->defaultValues = defaultValues;

   rglCreatePushBuffer( program );
   if ( profileIndex == FRAGMENT_PROFILE_INDEX )
      rglSetDefaultValuesFP(program); // modifies the ucode
   else
      rglSetDefaultValuesVP(program); // modifies the push buffer

   // not loaded yet
   program->loadProgramId = GMM_ERROR;
   program->loadProgramOffset = 0;
   if ( profileIndex == FRAGMENT_PROFILE_INDEX )
   {
      // always load fragment shaders.
      int loaded = rglpsLoadFPShader( program );
      if ( ! loaded )
      {
         //TODO: what do we need to delete here ?
         rglCgRaiseError( CG_MEMORY_ALLOC_ERROR );
         return 0;
      }
   }

   program->programGroup = NULL;
   program->programIndexInGroup = -1;

   return 1;
}

CGprogram rglpCgUpdateProgramAtIndex( CGprogramGroup group, int index, int refcount )
{
   if (index < ( int )group->programCount)
   {
      //index can be < 0 , in that case refcount update on the group only, used when destroying a copied program
      if (index >= 0)
      {
         //if it has already been referenced duplicate instead of returning the same index, until the API offer a native support for
         //group of programs ( //fixed bug 13007 )
         if (refcount == 1 && group->programs[index].refCount == 1)
         {
            //it will handle the refcounting
            CGprogram res = cgCopyProgram( group->programs[index].program );
            return res;
         }
         group->programs[index].refCount += refcount;
      }

      group->refCount += refcount;
      if (refcount < 0)
      {
         if (group->refCount == 0 && !group->userCreated)
            rglCgDestroyProgramGroup( group );
         return NULL;
      }
      else
         return group->programs[index].program;
   }
   else
      return NULL;
}

//add the group to the context:
static void rglCgAddGroup( CGcontext ctx, CGprogramGroup group )
{
   _CGcontext *context = _cgGetContextPtr(ctx);
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

static void rglCgRemoveGroup( CGcontext ctx, CGprogramGroup group )
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

CGprogramGroup rglCgCreateProgramGroupFromFile( CGcontext ctx, const char *group_file )
{
   // check that file exists
   FILE* fp = fopen( group_file, "rb" );

   if ( NULL == fp )
   {
      rglCgRaiseError( CG_FILE_READ_ERROR );
      return ( CGprogramGroup )NULL;
   }

   // find the file length
   size_t file_size = 0;
   fseek( fp, 0, SEEK_END );
   file_size = ftell( fp );
   rewind( fp );

   // alloc memory for new binary program and read the data
   char* ptr = ( char* )malloc( file_size + 1 );
   if ( NULL == ptr )
   {
      rglCgRaiseError( CG_MEMORY_ALLOC_ERROR );
      return ( CGprogramGroup )NULL;
   }

   // read the entire file into memory then close the file
   // TODO ********* just loading the file is a bit lame really. We can do better.
   fread( ptr, file_size, 1, fp );
   fclose( fp );

   CGprogramGroup group = rglCgCreateProgramGroup( ctx, group_file, ptr, file_size );
   if ( !group )
      free( ptr );

   return group;
}

CGprogramGroup rglCgCreateProgramGroup( CGcontext ctx,  const char *name, void *ptr, int size )
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

      //first pass to get the size of each item ( could be faster if the embedded constants index table size was in the header
      int programCount = elfBinary.shadertabSize / sizeof( CgProgramHeader );
      int i;

      //structure of the memory buffer storing the group
      size_t nvProgramNamesOffset = rglPad( sizeof( _CGprogramGroup ), sizeof( _CGnamedProgram ) ); //program name offset
      size_t nvDefaultValuesTableOffset = rglPad( nvProgramNamesOffset + programCount * sizeof( _CGnamedProgram ), 16 );//shared default value table

      size_t nvStringTableOffset = nvDefaultValuesTableOffset + elfConstTableSize; //shared string table
      size_t structureSize = nvStringTableOffset + elfStringTableSize;//total structure size

      //create the program group
      group = ( CGprogramGroup )malloc( structureSize );
      if ( !group ) //out of memory
         break;

      //fill the group structure
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
         if ( !group->name )//out of memory
            break;
         strcpy( group->name, name );
      }
      else
         group->name = NULL;

      //copy the default values
      if ( elfConstTableSize )
         memcpy(( char* )group + nvDefaultValuesTableOffset, elfBinary.consttab, elfConstTableSize );
      //copy the string table
      if ( elfStringTableSize )
         memcpy(( char* )group + nvStringTableOffset, elfBinary.strtab, elfStringTableSize );

      //add the group to the context:
      rglCgAddGroup( ctx, group );

      //create all the shaders contained in the package and add them to the group
      for ( i = 0;i < ( int )group->programCount;i++ )
      {
         CgProgramHeader *cgShader = ( CgProgramHeader* )elfBinary.shadertab + i;

         //hack to counter removal of TypeC during beta
         if ( cgShader->profile == ( CGprofile )7005 )
            cgShader->profile = CG_PROFILE_SCE_VP_RSX;
         if ( cgShader->profile == ( CGprofile )7006 )
            cgShader->profile = CG_PROFILE_SCE_FP_RSX;

         CGELFProgram elfProgram;
         bool res = cgGetElfProgramByIndex( &elfBinary, i, &elfProgram );
         if ( !res )
            return false;

         //I reference the buffer passed as parameter here, so it will have to stay around
         CgProgramHeader *programHeader = cgShader;
         char *ucode = ( char * )elfProgram.texttab;
         CgParameterTableHeader *parameterHeader  = ( CgParameterTableHeader * )elfProgram.paramtab;

         const char *programName = getSymbolByIndexInPlace( elfBinary.symtab, elfBinary.symbolSize, elfBinary.symbolCount, elfBinary.symbolstrtab, i + 1 );
         group->programs[i].name = programName;
         group->programs[i].program = rglCgCreateProgram( ctx, ( CGprofile )cgShader->profile, programHeader, ucode, parameterHeader, ( const char* )group->stringTable, ( const float* )group->constantTable );
         _CGprogram *cgProgram = _cgGetProgPtr( group->programs[i].program );
         cgProgram->programGroup = group;
         cgProgram->programIndexInGroup = i;
         group->programs[i].refCount = 0;
      }
      break;
   }

   return group;
}

void rglCgDestroyProgramGroup( CGprogramGroup group )
{
   _CGprogramGroup *_group = ( _CGprogramGroup * )group;
   for ( int i = 0;i < ( int )_group->programCount;i++ )
   {
      //unlink the program
      _CGprogram *cgProgram = _cgGetProgPtr( group->programs[i].program );
      cgProgram->programGroup = NULL;
      cgDestroyProgram( _group->programs[i].program );
   }
   free( _group->filedata );
   if ( _group->name )
      free( _group->name );

   //remove the group from the group list
   rglCgRemoveGroup( group->ctx, group );
   free( _group );
}

const char *rglCgGetProgramGroupName (CGprogramGroup group)
{
   _CGprogramGroup *_group = ( _CGprogramGroup * )group;
   return _group->name;
}

int rglCgGetProgramIndex (CGprogramGroup group, const char *name)
{
   int i;
   for ( i = 0;i < ( int )group->programCount;i++ )
   {
      if ( !strcmp( name, group->programs[i].name ) )
         return i;
   }
   return -1;
}

void rglpProgramErase (void *data)
{
   _CGprogram* platformProgram = (_CGprogram*)data;
   _CGprogram* program = (_CGprogram*)platformProgram;

   if ( program->loadProgramId != GMM_ERROR )
      rglpsUnloadFPShader( program );

   //free the runtime parameters
   if ( program->runtimeParameters )
   {
      //need to erase all the program parameter "names"
      int i;
      int count = ( int )program->rtParametersCount;

      for ( i = 0; i < count;i++ )
         rglEraseName( &_CurrentContext->cgParameterNameSpace, (unsigned int)program->runtimeParameters[i].id );

      free( program->runtimeParameters );
   }

   //free the push buffer block
   if ( program->memoryBlock )
      free( program->memoryBlock );

   //free the samplers lookup tables
   if ( program->samplerIndices )
   {
      free( program->samplerValuesLocation );
      free( program->samplerIndices );
      free( program->samplerUnits );
   }

   //free the "pointers" on the push buffer used for fast access
   if ( program->constantPushBufferPointers )
      free( program->constantPushBufferPointers );
}

//TODO: use a ref mechanism for the string table or duplicate it !
int rglpCopyProgram (void *src_data, void *dst_data)
{
   _CGprogram *source = (_CGprogram*)src_data;
   _CGprogram *destination = (_CGprogram*)dst_data;
   //extract the layout of the parameter buffers from the source
   CgParameterTableHeader parameterHeader;
   parameterHeader.entryCount = source->rtParametersCount;
   parameterHeader.resourceTableOffset = ( uintptr_t )(( char* )source->resources - ( char* )source->parametersEntries + sizeof( CgParameterTableHeader ) );
   parameterHeader.defaultValueIndexCount = source->defaultValuesIndexCount;
   parameterHeader.defaultValueIndexTableOffset = ( uintptr_t )(( char* )source->defaultValuesIndices - ( char* )source->parametersEntries + sizeof( CgParameterTableHeader ) );
   parameterHeader.semanticIndexCount = source->semanticCount;
   parameterHeader.semanticIndexTableOffset = ( uintptr_t )(( char* )source->defaultValuesIndices - ( char* )source->parametersEntries + sizeof( CgParameterTableHeader ) );

   int profileIndex;

   //allocate the copy of the program
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
   return rglGcmGenerateProgram( destination, profileIndex, &source->header, source->ucode, &parameterHeader, source->parametersEntries, source->stringTable, source->defaultValues );
}

int rglpGenerateVertexProgram (void *data, const CgProgramHeader *programHeader,
      const void *ucode, const CgParameterTableHeader *parameterHeader, const char *stringTable,
      const float *defaultValues )
{
   _CGprogram *program = (_CGprogram*)data;
   return rglGcmGenerateProgram( program, VERTEX_PROFILE_INDEX, programHeader,
         ucode, parameterHeader, NULL, stringTable, defaultValues );

}

int rglpGenerateFragmentProgram (void *data, const CgProgramHeader *programHeader, const void *ucode,
      const CgParameterTableHeader *parameterHeader, const char *stringTable, const float *defaultValues )
{
   _CGprogram *program = (_CGprogram*)data;
   return rglGcmGenerateProgram( program, FRAGMENT_PROFILE_INDEX, programHeader, ucode, parameterHeader, NULL, stringTable, defaultValues );

}
