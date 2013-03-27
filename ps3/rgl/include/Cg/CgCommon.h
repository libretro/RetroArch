#ifndef _cg_common_h
#define _cg_common_h

#include <Cg/cg.h>

#include "../export/RGL/rgl.h"
#include "../RGL/private.h"

#include <string>

#include "Cg/CgInternal.h"
#include "Cg/CgProgramGroup.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RGL_MAX_VP_SHARED_CONSTANTS 256
#define RGL_MAX_FP_SHARED_CONSTANTS 1024
#define RGL_BOOLEAN_REGISTERS_COUNT 32

// parameter setter, prototype of functions called when a uniform is set.
typedef void(*_cgSetFunction) (void *, const void* _RGL_RESTRICT);
typedef void(*_cgSetArrayIndexFunction) (void *, const void*, const int index);

typedef struct _CgUniform
{
   void *pushBufferPtr;
} _CgUniform;

typedef struct _CGprogram _CGprogram;

typedef struct CgRuntimeParameter
{
   _cgSetArrayIndexFunction samplerSetter;
   _cgSetArrayIndexFunction  setterIndex;
   _cgSetArrayIndexFunction settercIndex;
   _cgSetArrayIndexFunction setterrIndex;
   void *pushBufferPointer;
   const CgParameterEntry *parameterEntry;
   _CGprogram *program;
   int glType;
   CGparameter id;
} CgRuntimeParameter;

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

   // default program, fake owner of the context parameters
   _CGprogram        defaultProgram;

   //groups
   CGprogramGroup groupList;

   //"static" variable used to store the values of the last parameter for which getParameterValue has been called
   double currentParameterValue[16];
   char currentParameterName[128];
} _CGcontext;


// internal error handling
RGL_EXPORT void rglCgRaiseError( CGerror error );
// interface between object types
extern void rglCgProgramDestroyAll( _CGcontext* c );
extern void rglCgDestroyContextParam( CgRuntimeParameter* p );
RGL_EXPORT void rglCgProgramErase( _CGprogram* prog );

// default setters
void _cgRaiseInvalidParam( void *data, const void*v );
void _cgRaiseNotMatrixParam( void *data, const void*v );
void _cgIgnoreSetParam( void *dat, const void*v );
void _cgIgnoreParamIndex( void *dat, const void*v, const int index );

// cg helpers

// Is macros
#define CG_IS_CONTEXT(_ctx) rglIsName(&_CurrentContext->cgContextNameSpace, (unsigned int)_ctx)
#define CG_IS_PROGRAM(_program) rglIsName(&_CurrentContext->cgProgramNameSpace, (unsigned int)_program)
#define CG_IS_PARAMETER(_param) rglIsName(&_CurrentContext->cgParameterNameSpace, (unsigned int)(((unsigned int)_param)&CG_PARAMETERMASK))

//array indices
#define CG_PARAMETERSIZE 22 //22 bits == 4 millions parameters
#define CG_PARAMETERMASK ((1<<CG_PARAMETERSIZE)-1)
#define CG_GETINDEX(param) (int)((unsigned int)(param)>>CG_PARAMETERSIZE)


static inline bool isMatrix (CGtype type)
{
   if (( type >= CG_FLOAT1x1 && type <= CG_FLOAT4x4 ) ||
         ( type >= CG_HALF1x1 && type <= CG_HALF4x4 ) ||
         ( type >= CG_INT1x1 && type <= CG_INT4x4 ) ||
         ( type >= CG_BOOL1x1 && type <= CG_BOOL4x4 ) ||
         ( type >= CG_FIXED1x1 && type <= CG_FIXED4x4 ))
      return true;
   return false;
}

static inline bool isSampler (CGtype type)
{
   return ( type >= CG_SAMPLER1D && type <= CG_SAMPLERCUBE );
}


unsigned int rglCountFloatsInCgType( CGtype type );

unsigned int rglGetTypeRowCount( CGtype parameterType );
unsigned int rglGetTypeColCount( CGtype parameterType );

// the internal cg conversions
inline static CgRuntimeParameter* _cgGetParamPtr (CGparameter p)
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

static inline int rglGetSizeofSubArray( const unsigned short *dimensions, unsigned short count )
{
   int res = 1;
   for (int i = 0;i < count;i++)
      res *= ( int )(*(dimensions++));
   return res;
}

// platform API
CGprofile rglPlatformGetLatestProfile( CGGLenum profile_type );
int rglPlatformCopyProgram( _CGprogram* source, _CGprogram* destination );

void rglPlatformProgramErase (void *data);

int rglPlatformGenerateVertexProgram (void *data, const CgProgramHeader *programHeader, const void *ucode, const CgParameterTableHeader *parameterHeader, const char *stringTable, const float *defaultValues );
CGbool rglPlatformSupportsVertexProgram ( CGprofile p );

int rglPlatformGenerateFragmentProgram (void *data, const CgProgramHeader *programHeader, const void *ucode, const CgParameterTableHeader *parameterHeader, const char *stringTable, const float *defaultValues );
CGbool rglPlatformSupportsFragmentProgram ( CGprofile p );

void rglPlatformSetVertexRegister4fv (unsigned int reg, const float * _RGL_RESTRICT v );
void rglPlatformSetVertexRegisterBlock (unsigned int reg, unsigned int count, const float * _RGL_RESTRICT v );
void rglPlatformSetFragmentRegister4fv (unsigned int reg, const float * _RGL_RESTRICT v );
void rglPlatformSetFragmentRegisterBlock (unsigned int reg, unsigned int count, const float * _RGL_RESTRICT v );

void rglPlatformSetBoolVertexRegisters (unsigned int values );

// names API

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

struct rglNameSpace;

#define VERTEX_PROFILE_INDEX 0
#define FRAGMENT_PROFILE_INDEX 1

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

typedef int (*cgRTCgcCompileHookFunction) (const char*, const char *, const char*, const char**, char**);
typedef void(*cgRTCgcFreeHookFunction) (char*);
extern RGL_EXPORT cgRTCgcCompileHookFunction _cgRTCgcCompileProgramHook;
extern RGL_EXPORT cgRTCgcFreeHookFunction _cgRTCgcFreeCompiledProgramHook;


//-----------------------------------------------
//inlined helper functions
static inline const CgParameterResource *rglGetParameterResource (const void *data, const CgParameterEntry *entry )
{
   const _CGprogram *program = (const _CGprogram*)data;
   return (CgParameterResource *)(program->parameterResources + entry->typeIndex);
}

static inline CGtype rglGetParameterCGtype (const void *data, const CgParameterEntry *entry)
{
   const _CGprogram *program = (const _CGprogram*)data;

   if (entry->flags & CGP_RTCREATED)
      return (CGtype)entry->typeIndex;
   else
   {
      const CgParameterResource *parameterResource = rglGetParameterResource(program, entry);
      if (parameterResource)
         return (CGtype)parameterResource->type;
   }
   return CG_UNKNOWN_TYPE;
}

static inline const CgParameterArray *rglGetParameterArray (const void *data, const CgParameterEntry *entry)
{
   const _CGprogram *program = (const _CGprogram*)data;
   return (CgParameterArray*)(program->parameterResources + entry->typeIndex);
}

static inline const CgParameterStructure *rglGetParameterStructure (const void *data, const CgParameterEntry *entry )
{
   const _CGprogram *program = (const _CGprogram*)data;
   return (CgParameterStructure*)(program->parameterResources + entry->typeIndex);
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
}
#endif

#endif
