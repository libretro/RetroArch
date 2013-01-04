#ifndef _cg_common_h
#define _cg_common_h

#include <Cg/cg.h>

#include "../export/RGL/rgl.h"
#include "../RGL/private.h"

#include <vector>
#include <string>

#include "Cg/CgInternal.h"
#include "Cg/CgProgramGroup.h"

// [YLIN]
#include <string.h> // for memcpy16
#if (CELL_SDK_VERSION<=0x210001)
using _CSTD memcpy16; // fixed at SDK 220
#endif // (CELL_SDK_VERSION<=0x210001)

#ifdef __cplusplus
extern "C"
{
#endif


#define RGL_MAX_VP_SHARED_CONSTANTS 256
#define RGL_MAX_FP_SHARED_CONSTANTS 1024
#define RGL_BOOLEAN_REGISTERS_COUNT 32

   // parameter setter, prototype of functions called when a uniform is set.
   typedef void( *_cgSetFunction )( void *, const void* _RGL_RESTRICT );

   typedef void( *_cgSetArrayIndexFunction )( void *, const void*, const int index );

   typedef struct _CgUniform
   {
      void *pushBufferPtr;
   }
   _CgUniform;

   typedef struct _CGprogram _CGprogram;

   typedef struct CgRuntimeParameter
   {
      // sampler
      _cgSetArrayIndexFunction samplerSetter;
      /*cgSetFunction*/

      //vector
      _cgSetArrayIndexFunction  setterIndex;
      // matrices
      _cgSetArrayIndexFunction settercIndex;
      _cgSetArrayIndexFunction setterrIndex;

      void *pushBufferPointer;
      const CgParameterEntry *parameterEntry;
      _CGprogram *program;
      int glType; //tmp...
      CGparameter id;//tmp
   }
   CgRuntimeParameter;

   typedef struct
   {
      CgRuntimeParameter* child;
      CgRuntimeParameter* parent;
      CgRuntimeParameter* top;
      _cgSetArrayIndexFunction childOnBindSetter;
   }
   CgParameterConnection;

   typedef struct
   {
      CgRuntimeParameter* param;
      std::vector<char> semantic;
   }
   CgRuntimeSemantic;

   struct _CGprogram
   {
      struct _CGprogram*   next;          // link to next in NULL-terminated singly linked list of programs
      CGprogram            id;            // numerical id for this program object
      struct _CGcontext*   parentContext; // parent context for this program
      void*                parentEffect;  // parent effect for this program (only used for default program of an effect, containing effect parameters)
      bool				 inLocalMemory; // only pertains to fragment programs which can be in location local, the default, or location main via cgb interfaces 
      // parameters in the CG_PROGRAM namespace

      unsigned int         constantPushBufferWordSize;
      unsigned int*        constantPushBuffer;
      unsigned int         lastConstantUpdate;

      // executable program image
      void*     platformProgram;

      // entire binary program image from compiler
      void*     platformProgramBinary;

      // extra information to ease the coding of the rgl runtime
      // the following are used to know which parameters are samplers, so that we can walk them in a quick way.
      unsigned int         samplerCount;
      unsigned int *       samplerIndices;
      unsigned int *       samplerUnits;

      unsigned int         controlFlowBools;

      //binary format additions
      //info previously contained in platformProgram ( loadAddress + nvBinary )
      CgProgramHeader header;
      const char *name;
      const void *ucode;

      GLuint loadProgramId;
      // offset into the allocation id above, normally zero for internal use.
      // But for psglSetFragmentProgramConfiguration it's possible for 
      // users to manage sub-heap in allocation and put multiple 
      // program in each allocation.
      GLuint loadProgramOffset; 

      int version; //contained a boolean indicating if the structure pointers have been patched or not

      char *parameterResources;
      int rtParametersCount;
      CgRuntimeParameter *runtimeParameters;
      const CgParameterEntry *parametersEntries;

      unsigned short *resources;
      unsigned short *pushBufferPointers;

      int defaultValuesIndexCount;
      const CgParameterDefaultValue *defaultValuesIndices;

      int semanticCount;
      const CgParameterSemantic *semanticIndices;

      int defaultValueCount;
      const float *defaultValues;

      const char *stringTable;

      unsigned int **constantPushBufferPointers;

      //tmp
      unsigned int *samplerValuesLocation;
      void *memoryBlock;

      _CGprogramGroup *programGroup;
      int programIndexInGroup;

      // supports runtime created parameters
      std::vector<CgRuntimeParameter*> runtimeCreatedParameters;

      // supports parameter connections
      std::vector<CgParameterConnection> connectionTable;

      // supports runtime allocation of semantics
      std::vector<CgRuntimeSemantic> parameterSemantics;

      //runtime compilation / conversion
      void *runtimeElf;
   };

   typedef struct _CGcontext
   {
      struct _CGcontext* next;            // for global linked list of CGcontexts
      CGcontext          id;              // numerical handle for this object
      unsigned int       programCount;    // number of programs in the list
      struct _CGprogram* programList;     // head of singly linked list of programs

      CGenum             compileType;     // compile manual, immediate or lazy (unused so far)

      // implementation data for GL
      CGbool             GLmanageTextures;

      unsigned int         controlFlowBoolsSharedMask;
      unsigned int         controlFlowBoolsShared;

      // default program, fake owner of the context parameters
      _CGprogram        defaultProgram;

      //groups
      CGprogramGroup groupList;

      //"static" variable used to store the values of the last parameter for which getParameterValue has been called
      double currentParameterValue[16];
      char currentParameterName[128];
   }
   _CGcontext;


   // prototypes --------------------------------

   // internal error handling
   RGL_EXPORT void rglCgRaiseError( CGerror error );
   // interface between object types
   extern void rglCgProgramDestroyAll( _CGcontext* c );
   extern void rglCgDestroyContextParam( CgRuntimeParameter* p );
   RGL_EXPORT CgRuntimeParameter*rglCgCreateParameterInternal( _CGprogram *program, const char* name, CGtype type );
   RGL_EXPORT void rglCgProgramErase( _CGprogram* prog );

   // default setters
   void _cgRaiseInvalidParam( void *data, const void*v );
   void _cgRaiseNotMatrixParam( void *data, const void*v );
   void _cgIgnoreSetParam( void *dat, const void*v );
   void _cgRaiseInvalidParamIndex( void *dat, const void*v, const int index );
   void _cgRaiseNotMatrixParamIndex( void *dat, const void*v, const int index );
   void _cgIgnoreSetParamIndex( void *dat, const void*v, const int index );

   // cg helpers

   // Is macros
#define CG_IS_CONTEXT(_ctx) rglIsName(&_CurrentContext->cgContextNameSpace, (unsigned int)_ctx)
#define CG_IS_PROGRAM(_program) rglIsName(&_CurrentContext->cgProgramNameSpace, (unsigned int)_program)
#define CG_IS_PARAMETER(_param) rglIsName(&_CurrentContext->cgParameterNameSpace, (unsigned int)(((unsigned int)_param)&CG_PARAMETERMASK))

   //array indices
#define CG_PARAMETERSIZE 22 //22 bits == 4 millions parameters
#define CG_PARAMETERMASK ((1<<CG_PARAMETERSIZE)-1)
#define CG_GETINDEX(param) (int)((unsigned int)(param)>>CG_PARAMETERSIZE)


   inline static bool isMatrix( CGtype type )
   {
      if (( type >= CG_FLOAT1x1 && type <= CG_FLOAT4x4 ) ||
            ( type >= CG_HALF1x1 && type <= CG_HALF4x4 ) ||
            ( type >= CG_INT1x1 && type <= CG_INT4x4 ) ||
            ( type >= CG_BOOL1x1 && type <= CG_BOOL4x4 ) ||
            ( type >= CG_FIXED1x1 && type <= CG_FIXED4x4 ))
         return true;
      return false;
   }

   inline static bool isSampler( CGtype type )
   {
      return ( type >= CG_SAMPLER1D && type <= CG_SAMPLERCUBE );
   }


   unsigned int rglCountFloatsInCgType( CGtype type );
   CGbool _cgMatrixDimensions( CGtype type, unsigned int* nrows, unsigned int* ncols );

   unsigned int rglGetTypeRowCount( CGtype parameterType );
   unsigned int rglGetTypeColCount( CGtype parameterType );

   // the internal cg conversions
   inline static CgRuntimeParameter* _cgGetParamPtr( CGparameter p )
   {
      return ( CgRuntimeParameter* )rglGetNamedValue( &_CurrentContext->cgParameterNameSpace, ( unsigned int )((( unsigned int )p )&CG_PARAMETERMASK ) );
   }

   inline static _CGprogram* _cgGetProgPtr( CGprogram p )
   {
      return ( _CGprogram* )rglGetNamedValue( &_CurrentContext->cgProgramNameSpace, (unsigned int)p );
   }

   inline static _CGcontext* _cgGetContextPtr( CGcontext c )
   {
      return ( _CGcontext* )rglGetNamedValue( &_CurrentContext->cgContextNameSpace, (unsigned int)c );
   }

   inline static CgRuntimeParameter* rglCgGLTestParameter( CGparameter param )
   {
      return _cgGetParamPtr( param );
   }

   CgRuntimeParameter* _cgGLTestArrayParameter( CGparameter paramIn, long offset, long nelements );
   CgRuntimeParameter* _cgGLTestTextureParameter( CGparameter param );

#ifdef RGL_DEBUG
   void rglCgDumpState();
   void rglCgPrintSpaces( unsigned int n );
#endif

   inline static int rglGetSizeofSubArray( const unsigned short *dimensions, unsigned short count )
   {
      int res = 1;
      for ( int i = 0;i < count;i++ )
         res *= ( int )( *( dimensions++ ) );
      return res;
   }

   inline static CGresource rglGetBaseResource( CGresource resource )
   {
      switch ( resource )
      {
         case CG_ATTR0: case CG_ATTR1: case CG_ATTR2: case CG_ATTR3:
         case CG_ATTR4: case CG_ATTR5: case CG_ATTR6: case CG_ATTR7:
         case CG_ATTR8: case CG_ATTR9: case CG_ATTR10: case CG_ATTR11:
         case CG_ATTR12: case CG_ATTR13: case CG_ATTR14: case CG_ATTR15:
            return CG_ATTR0;
         case CG_HPOS:
            return CG_HPOS;
         case CG_COL0: case CG_COL1: case CG_COL2: case CG_COL3:
            return CG_COL0;
         case CG_TEXCOORD0: case CG_TEXCOORD1: case CG_TEXCOORD2: case CG_TEXCOORD3:
         case CG_TEXCOORD4: case CG_TEXCOORD5: case CG_TEXCOORD6: case CG_TEXCOORD7:
         case CG_TEXCOORD8: case CG_TEXCOORD9:
            return CG_TEXCOORD0;
         case CG_TEXUNIT0: case CG_TEXUNIT1: case CG_TEXUNIT2: case CG_TEXUNIT3:
         case CG_TEXUNIT4: case CG_TEXUNIT5: case CG_TEXUNIT6: case CG_TEXUNIT7:
         case CG_TEXUNIT8: case CG_TEXUNIT9: case CG_TEXUNIT10: case CG_TEXUNIT11:
         case CG_TEXUNIT12: case CG_TEXUNIT13: case CG_TEXUNIT14: case CG_TEXUNIT15:
            return CG_TEXUNIT0;
         case CG_FOGCOORD:
            return CG_FOGCOORD;
         case CG_PSIZ:
            return CG_PSIZ;
         case CG_WPOS:
            return CG_WPOS;
         case CG_COLOR0: case CG_COLOR1: case CG_COLOR2: case CG_COLOR3:
            return CG_COLOR0;
         case CG_DEPTH0:
            return CG_DEPTH0;
         case CG_C:
            return CG_C;
         case CG_B:
            return CG_B;
         case CG_CLP0: case CG_CLP1: case CG_CLP2: case CG_CLP3: case CG_CLP4: case CG_CLP5:
            return CG_CLP0;
         case CG_UNDEFINED:
            return CG_UNDEFINED;
         default:
            return CG_UNDEFINED;
      }
   }

   // platform API
   CGprofile rglPlatformGetLatestProfile( CGGLenum profile_type );
   int rglPlatformCopyProgram( _CGprogram* source, _CGprogram* destination );

   void rglPlatformProgramErase( void* platformProgram );

   int rglPlatformGenerateVertexProgram( _CGprogram *program, const CgProgramHeader *programHeader, const void *ucode, const CgParameterTableHeader *parameterHeader, const char *stringTable, const float *defaultValues );
   CGbool rglPlatformSupportsVertexProgram( CGprofile p );

   int rglPlatformGenerateFragmentProgram( _CGprogram *program, const CgProgramHeader *programHeader, const void *ucode, const CgParameterTableHeader *parameterHeader, const char *stringTable, const float *defaultValues );
   CGbool rglPlatformSupportsFragmentProgram( CGprofile p );



   void rglPlatformSetVertexRegister4fv( unsigned int reg, const float * _RGL_RESTRICT v );
   void rglPlatformSetVertexRegisterBlock( unsigned int reg, unsigned int count, const float * _RGL_RESTRICT v );
   void rglPlatformSetFragmentRegister4fv( unsigned int reg, const float * _RGL_RESTRICT v );
   void rglPlatformSetFragmentRegisterBlock( unsigned int reg, unsigned int count, const float * _RGL_RESTRICT v );

   void rglPlatformSetBoolVertexRegisters( unsigned int values );

   // names API
   RGL_EXPORT unsigned int _cgHashString( const char *str );

   inline static void _pullConnectedParameterValues( _CGprogram* ptr )
   {
      // we now use a pull method to get the data into the children parameters
      // when their program is bound they pull the data from their parents
      std::vector<CgParameterConnection>::iterator connectionIter = ptr->connectionTable.begin();
      while ( connectionIter != ptr->connectionTable.end() )
      {
         // pull data into connectionIter->child from connectionIter->top
         connectionIter->childOnBindSetter( connectionIter->child, connectionIter->top->pushBufferPointer, 0 );
         connectionIter++;
      }
   }

   inline static void _cgGLBindVertexProgram( _CGprogram* program )
   {
      // the program is a vertex program, just update the GL state
      _CurrentContext->BoundVertexProgram = program;

      // and inform the GL state to re-upload the vertex program
      _CurrentContext->needValidate |= PSGL_VALIDATE_VERTEX_PROGRAM;

      // This must happen before the sampler setters so texture parameters have the correct value in their push buffers for that routine
      _pullConnectedParameterValues( program );

      CGbool is_managed = program->parentContext->GLmanageTextures;
      // enable texture parameters if the managed flag is set.
      if ( is_managed )
      {
         for ( GLuint index = 0; index < program->samplerCount; ++index )
         {
            // walk the array of sampler parameters
            CgRuntimeParameter *rtParameter = program->runtimeParameters + program->samplerIndices[index];
            rtParameter->samplerSetter( rtParameter, NULL, 0 );
         }
      }
   }

   inline static void _cgGLBindFragmentProgram( _CGprogram* program )
   {
      _CurrentContext->BoundFragmentProgram = program;

      // need to revalidate the textures in order to update which targets to fetch from
      _CurrentContext->needValidate |= PSGL_VALIDATE_FRAGMENT_PROGRAM | PSGL_VALIDATE_TEXTURES_USED | PSGL_VALIDATE_FRAGMENT_SHARED_CONSTANTS;

      // This must happen before the sampler setters so texture parameters have the correct value in their push buffers for that routine
      _pullConnectedParameterValues( program );

      // TODO: push texture state
      //  Needs to be done per profile. Can't use glPushAttrib.

      CGbool is_managed = program->parentContext->GLmanageTextures;

      // deal with the texture parameters now.
      for ( GLuint index = 0; index < program->samplerCount; ++index )
      {
         // walk the array of sampler parameters
         CgRuntimeParameter *rtParameter = program->runtimeParameters + program->samplerIndices[index];
         CgParameterResource *parameter = ( CgParameterResource * )( program->parameterResources + rtParameter->parameterEntry->typeIndex );
         // find out which texture unit this parameter has been assigned to
         unsigned int unit = parameter->resource - CG_TEXUNIT0;

         _CurrentContext->TextureImageUnits[unit].fragmentTarget = rtParameter->glType;

         // enable texture parameters if the managed flag is set.
         if ( is_managed )
         {
            //tmp
            rtParameter->samplerSetter( rtParameter, NULL, 0 );
         }
         else
         {
            rglUpdateCurrentTextureCache( &_CurrentContext->TextureImageUnits[unit] );
         }
      }
   }

   inline static void _cgGLUnbindVertexProgram()
   {
      _CurrentContext->BoundVertexProgram = NULL;
      _CurrentContext->needValidate |= PSGL_VALIDATE_VERTEX_PROGRAM;
   }

   // XXX this should not be here.
   inline static void rglLeaveFFXFP( RGLcontext*LContext )
   {
      LContext->FragmentProgram = GL_TRUE;
      struct _CGprogram* current = LContext->BoundFragmentProgram;
      if ( current )
      {
         for ( GLuint i = 0; i < current->samplerCount; ++i )
         {
            int unit = current->samplerUnits[i];
            rglUpdateCurrentTextureCache( &_CurrentContext->TextureImageUnits[unit] );
         }
      }
      LContext->needValidate |= PSGL_VALIDATE_FRAGMENT_PROGRAM | PSGL_VALIDATE_TEXTURES_USED | PSGL_VALIDATE_FRAGMENT_SHARED_CONSTANTS;
   }

   inline static void _cgGLUnbindFragmentProgram()
   {
      _CurrentContext->BoundFragmentProgram = NULL;
   }

   static inline GLenum rglCgGetSamplerGLTypeFromCgType( CGtype type )
   {
      switch ( type )
      {
         case CG_SAMPLER1D:
         case CG_SAMPLER2D:
         case CG_SAMPLERRECT:
            return GL_TEXTURE_2D;
         case CG_SAMPLER3D:
            return GL_TEXTURE_3D;
         case CG_SAMPLERCUBE:
            return GL_TEXTURE_CUBE_MAP;
         default:
            return 0;
      }
   }

   static inline int is_created_param( CgRuntimeParameter* ptr )
   {
      if ( ptr->parameterEntry->flags & CGP_RTCREATED )
         return 1;
      return 0;
   }

   struct rglNameSpace;

#define VERTEX_PROFILE_INDEX 0
#define FRAGMENT_PROFILE_INDEX 1
   int rglNVGenerateProgram( _CGprogram *program, int profileIndex, const CgProgramHeader *programHeader, const void *ucode,
         const CgParameterTableHeader *parameterHeader, const CgParameterEntry *parameterEntries,
         const char *stringTable, const float *defaultValues );

   //
   // these functions return the statically allocated table of function pointers originally
   // written for NV unshared vertex parameter setters, but now also used by runtime
   // created parameters cause these setters just do straight copies into the pushbuffer memory
   //
   _cgSetArrayIndexFunction getVectorTypeIndexSetterFunction( unsigned short a, unsigned short b, unsigned short c, unsigned short d );
   _cgSetArrayIndexFunction getMatrixTypeIndexSetterFunction( unsigned short a, unsigned short b, unsigned short c, unsigned short d, unsigned short e, unsigned short f );

   // -------------------------------------------

   typedef void( * CgcontextHookFunction )( _CGcontext *context );
   extern RGL_EXPORT CgcontextHookFunction _cgContextCreateHook;
   extern RGL_EXPORT CgcontextHookFunction _cgContextDestroyHook;

   typedef void( * CgparameterHookFunction )( CgRuntimeParameter *parameter );
   extern RGL_EXPORT CgparameterHookFunction _cgParameterCreateHook;
   extern RGL_EXPORT CgparameterHookFunction _cgParameterDestroyHook;

   typedef void( * CgprogramHookFunction )( _CGprogram *program );
   typedef void( * CgprogramCopyHookFunction )( _CGprogram *newprogram, _CGprogram *oldprogram );
   extern RGL_EXPORT CgprogramHookFunction _cgProgramCreateHook;
   extern RGL_EXPORT CgprogramHookFunction _cgProgramDestroyHook;
   extern RGL_EXPORT CgprogramCopyHookFunction _cgProgramCopyHook;

   typedef int( * cgRTCgcCompileHookFunction )( const char*, const char *, const char*, const char**, char** );
   typedef void( * cgRTCgcFreeHookFunction )( char* );
   extern RGL_EXPORT cgRTCgcCompileHookFunction _cgRTCgcCompileProgramHook;
   extern RGL_EXPORT cgRTCgcFreeHookFunction _cgRTCgcFreeCompiledProgramHook;


   //-----------------------------------------------
   //inlined helper functions
   static inline int rglGetParameterType( const CGprogram *program, const CgParameterEntry *entry )
   {
      return ( entry->flags & CGP_TYPE_MASK );
   }

   static inline const CgParameterResource *rglGetParameterResource( const _CGprogram *program, const CgParameterEntry *entry )
   {
      return ( CgParameterResource * )( program->parameterResources + entry->typeIndex );
   }

   static inline CGtype rglGetParameterCGtype( const _CGprogram *program, const CgParameterEntry *entry )
   {
      if ( entry->flags & CGP_RTCREATED )
      {
         return ( CGtype )entry->typeIndex;
      }
      else
      {
         const CgParameterResource *parameterResource = rglGetParameterResource( program, entry );
         if ( parameterResource )
         {
            return ( CGtype )parameterResource->type;
         }
      }
      return CG_UNKNOWN_TYPE;
   }

   static inline const CgParameterArray *rglGetParameterArray( const _CGprogram *program, const CgParameterEntry *entry )
   {
      return ( CgParameterArray* )( program->parameterResources + entry->typeIndex );
   }

   static inline const CgParameterStructure *rglGetParameterStructure( const _CGprogram *program, const CgParameterEntry *entry )
   {
      return ( CgParameterStructure* )( program->parameterResources + entry->typeIndex );
   }

   inline int rglGetProgramProfileIndex( CGprofile profile )
   {
      if ( profile == CG_PROFILE_SCE_FP_TYPEB || profile == CG_PROFILE_SCE_FP_TYPEC || profile == CG_PROFILE_SCE_FP_RSX )
         return FRAGMENT_PROFILE_INDEX;
      else if ( profile == CG_PROFILE_SCE_VP_TYPEB || profile == CG_PROFILE_SCE_VP_TYPEC || profile == CG_PROFILE_SCE_VP_RSX )
         return VERTEX_PROFILE_INDEX;
      else
         return -1;
   }

#ifdef __cplusplus
}	// Close scope of 'extern "C"' declaration that encloses file.
#endif

#endif
