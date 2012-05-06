#ifndef _cg_common_h
#define _cg_common_h

#include <vector>
#include <string>

#include <Cg/cgGL.h>

#include "rgl.h"
#include "private.h"

#ifndef STL_NAMESPACE
#define STL_NAMESPACE std::
#endif

#include <string.h> 

#define RGL_BOOLEAN_REGISTERS_COUNT 32
#define VERTEX_PROFILE_INDEX 0
#define FRAGMENT_PROFILE_INDEX 1

#define CGF_OUTPUTFROMH0 0x01
#define CGF_DEPTHREPLACE 0x02
#define CGF_PIXELKILL 0x04

#define CGPV_MASK 0x03
#define CGPV_VARYING 0x00
#define CGPV_UNIFORM 0x01
#define CGPV_CONSTANT 0x02
#define CGPV_MIXED 0x03

#define CGPF_REFERENCED 0x10
#define CGPF_SHARED 0x20
#define CGPF_GLOBAL 0x40
#define CGP_INTERNAL 0x80

#define CGP_INTRINSIC  0x0000
#define CGP_STRUCTURE  0x100
#define CGP_ARRAY  0x200
#define CGP_TYPE_MASK  (CGP_STRUCTURE + CGP_ARRAY)

#define CGP_UNROLLED 0x400
#define CGP_UNPACKED 0x800
#define CGP_CONTIGUOUS 0x1000

#define CGP_NORMALIZE 0x2000
#define CGP_RTCREATED 0x4000

#define CGP_SCF_BOOL (CG_TYPE_START_ENUM + 1024)
#define CG_BINARY_FORMAT_REVISION   0x00000006

#ifdef __cplusplus
extern "C"
{
#endif

typedef unsigned int                    CgBinaryOffset;
typedef CgBinaryOffset                  CgBinaryEmbeddedConstantOffset;
typedef CgBinaryOffset                  CgBinaryFloatOffset;
typedef CgBinaryOffset                  CgBinaryStringOffset;
typedef CgBinaryOffset                  CgBinaryParameterOffset;

typedef struct CgBinaryVertexProgram    CgBinaryVertexProgram;
typedef struct CgBinaryFragmentProgram  CgBinaryFragmentProgram;
typedef struct CgBinaryProgram          CgBinaryProgram;

typedef struct _CGnamedProgram
{
	const char *name;
	CGprogram program;
	int refCount;
} _CGnamedProgram;

typedef struct _CGprogramGroup
{
	struct _CGprogramGroup *next;
	CGcontext ctx;
	unsigned int *constantTable;
	unsigned int *stringTable;
	unsigned int programCount;
	_CGnamedProgram *programs;
	int refCount;
	bool userCreated;
	char *filedata;
	char *name;
} _CGprogramGroup;

typedef struct _CGprogramGroup *CGprogramGroup;

CGprogramGroup _RGLCgCreateProgramGroup( CGcontext ctx, const char *name, void *ptr, int size );
CGprogramGroup _RGLCgCreateProgramGroupFromFile( CGcontext ctx, const char *group_file );
void _RGLCgDestroyProgramGroup( CGprogramGroup group );

int _RGLCgGetProgramCount( CGprogramGroup group );
CGprogram _RGLCgGetProgram( CGprogramGroup group, const char *name );
int _RGLCgGetProgramIndex( CGprogramGroup group, const char *name );
CGprogram _RGLCgGetProgramAtIndex( CGprogramGroup group, unsigned int index );
const char *_RGLCgGetProgramGroupName( CGprogramGroup group );

typedef struct _CgParameterTableHeader
{
	unsigned short entryCount;
	unsigned short resourceTableOffset;
	unsigned short defaultValueIndexTableOffset;
	unsigned short defaultValueIndexCount;
	unsigned short semanticIndexTableOffset;
	unsigned short semanticIndexCount;
} CgParameterTableHeader;

typedef struct _CgParameterEntry
{
	unsigned int nameOffset;
	unsigned short typeIndex;
	unsigned short flags;
} CgParameterEntry;

#ifdef MSVC
#pragma warning( push )
#pragma warning ( disable : 4200 )
#endif

typedef struct _CgParameterArray
{
	unsigned short arrayType;
	unsigned short dimensionCount;
	unsigned short dimensions[];
} CgParameterArray;

#ifdef MSVC
#pragma warning( pop )
#endif

typedef struct _CgParameterStructure
{
	unsigned short memberCount;
	unsigned short reserved;
}
CgParameterStructure;

typedef struct _CgParameterResource
{
	unsigned short type;
	unsigned short resource;
}
CgParameterResource;

typedef struct _CgParameterSemantic
{
	unsigned short entryIndex;
	unsigned short reserved;
	unsigned int semanticOffset;
}
CgParameterSemantic;

typedef struct _CgParameterDefaultValue
{
	unsigned short entryIndex;
	unsigned short defaultValueIndex;
}
CgParameterDefaultValue;

typedef struct CgProgramHeader
{
	unsigned short profile;
	unsigned short compilerVersion;
	unsigned int instructionCount;
	unsigned int attributeInputMask;
	union
	{
		struct
		{
			unsigned int instructionSlot;
			unsigned int registerCount;
			unsigned int attributeOutputMask;
		} vertexProgram;
		struct
		{
			unsigned int partialTexType;
			unsigned short texcoordInputMask;
			unsigned short texcoord2d;
			unsigned short texcoordCentroid;
			unsigned char registerCount;
			unsigned char flags;
		} fragmentProgram;
	};
}
CgProgramHeader;

typedef void( *_cgSetFunction )( struct CgRuntimeParameter* _RGL_RESTRICT, const void* _RGL_RESTRICT );

typedef void( *_cgSetArrayIndexFunction )( struct CgRuntimeParameter* _RGL_RESTRICT, const void* _RGL_RESTRICT, const int index );

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
	struct _CGprogram*   next;
	CGprogram            id;
	struct _CGcontext*   parentContext;
	void*                parentEffect;
	bool		 inLocalMemory;
	unsigned int         constantPushBufferWordSize;
	unsigned int*        constantPushBuffer;
	void*     platformProgram;
	void*     platformProgramBinary;
	unsigned int         samplerCount;
	unsigned int *       samplerIndices;
	unsigned int *       samplerUnits;
	unsigned int         controlFlowBools;
	CgProgramHeader header;
	const char *name;
	const void *ucode;
	GLuint loadProgramId;
	GLuint loadProgramOffset; 
	int version;
	char *parameterResources;
	int rtParametersCount;
	CgRuntimeParameter *runtimeParameters;
	const CgParameterEntry *parametersEntries;
	unsigned short *resources;
	unsigned short *pushBufferPointers;
	int defaultValuesIndexCount;
	const CgParameterDefaultValue *defaultValuesIndices;
	int defaultValueCount;
	const float *defaultValues;
	const char *stringTable;
	unsigned int **constantPushBufferPointers;
	unsigned int *samplerValuesLocation;
	void *memoryBlock;
	_CGprogramGroup *programGroup;
	int programIndexInGroup;
	void *runtimeElf;
};

typedef struct _CGcontext
{
	struct _CGcontext* next;
	CGcontext          id;
	unsigned int       programCount;
	struct _CGprogram* programList;
	CGenum             compileType;
	unsigned int         controlFlowBoolsSharedMask;
	unsigned int         controlFlowBoolsShared;
	_CGprogram        defaultProgram;
	CGprogramGroup groupList;
	double currentParameterValue[16];
	char currentParameterName[128];
} _CGcontext;


void _RGLCgRaiseError( CGerror error );
extern void _RGLCgProgramDestroyAll( _CGcontext* c );
extern void _RGLCgDestroyContextParam( CgRuntimeParameter* p );
CgRuntimeParameter*_RGLCgCreateParameterInternal( _CGprogram *program, const char* name, CGtype type );
void _RGLCgProgramErase( _CGprogram* prog );

void _cgRaiseInvalidParam( CgRuntimeParameter*p, const void*v );
void _cgRaiseNotMatrixParam( CgRuntimeParameter*p, const void*v );
void _cgIgnoreSetParam( CgRuntimeParameter*p, const void*v );
void _cgRaiseInvalidParamIndex( CgRuntimeParameter*p, const void*v, const int index );
void _cgRaiseNotMatrixParamIndex( CgRuntimeParameter*p, const void*v, const int index );
void _cgIgnoreSetParamIndex( CgRuntimeParameter*p, const void*v, const int index );

#define CG_IS_CONTEXT(_ctx) _RGLIsName(&_CurrentContext->cgContextNameSpace, (jsName)_ctx)
#define CG_IS_PROGRAM(_program) _RGLIsName(&_CurrentContext->cgProgramNameSpace, (jsName)_program)
#define CG_IS_PARAMETER(_param) _RGLIsName(&_CurrentContext->cgParameterNameSpace, (jsName)(((unsigned int)_param)&CG_PARAMETERMASK))

#define CG_PARAMETERSIZE 22 
#define CG_PARAMETERMASK ((1<<CG_PARAMETERSIZE)-1)
#define CG_GETINDEX(param) (int)((unsigned int)(param)>>CG_PARAMETERSIZE)

static inline bool isMatrix( CGtype type )
{
	if (( type >= CG_FLOAT1x1 && type <= CG_FLOAT4x4 ) ||
			( type >= CG_HALF1x1 && type <= CG_HALF4x4 ) ||
			( type >= CG_INT1x1 && type <= CG_INT4x4 ) ||
			( type >= CG_BOOL1x1 && type <= CG_BOOL4x4 ) ||
			( type >= CG_FIXED1x1 && type <= CG_FIXED4x4 ))
		return true;
	return false;
}

static inline bool isSampler( CGtype type )
{
	return ( type >= CG_SAMPLER1D && type <= CG_SAMPLERCUBE );
}


unsigned int _RGLCountFloatsInCgType( CGtype type );
CGbool _cgMatrixDimensions( CGtype type, unsigned int* nrows, unsigned int* ncols );

unsigned int _RGLGetTypeRowCount( CGtype parameterType );
unsigned int _RGLGetTypeColCount( CGtype parameterType );

inline static CgRuntimeParameter* _cgGetParamPtr( CGparameter p )
{
	return ( CgRuntimeParameter* )_RGLGetNamedValue( &_CurrentContext->cgParameterNameSpace, ( jsName )((( unsigned int )p )&CG_PARAMETERMASK ) );
}

inline static _CGprogram* _cgGetProgPtr( CGprogram p )
{
	return ( _CGprogram* )_RGLGetNamedValue( &_CurrentContext->cgProgramNameSpace, ( jsName )p );
}

inline static _CGcontext* _cgGetContextPtr( CGcontext c )
{
	return ( _CGcontext* )_RGLGetNamedValue( &_CurrentContext->cgContextNameSpace, ( jsName )c );
}

inline static CgRuntimeParameter* _RGLCgGLTestParameter( CGparameter param )
{
	return _cgGetParamPtr( param );
}

CgRuntimeParameter* _cgGLTestArrayParameter( CGparameter paramIn, long offset, long nelements );
CgRuntimeParameter* _cgGLTestTextureParameter( CGparameter param );

inline static int _RGLGetSizeofSubArray( const unsigned short *dimensions, unsigned short count )
{
	int res = 1;
	for ( int i = 0;i < count;i++ )
		res *= ( int )( *( dimensions++ ) );
	return res;
}

inline static CGresource _RGLGetBaseResource( CGresource resource )
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
			printf("RGL WARN: resource 0x%d is unknown here.\n", resource );
			return CG_UNDEFINED;
	}
}

CGprofile _RGLPlatformGetLatestProfile( CGGLenum profile_type );
int _RGLPlatformCopyProgram( _CGprogram* source, _CGprogram* destination );

void _RGLPlatformProgramErase( void* platformProgram );

int _RGLPlatformGenerateVertexProgram( _CGprogram *program, const CgProgramHeader *programHeader, const void *ucode, const CgParameterTableHeader *parameterHeader, const char *stringTable, const float *defaultValues );

int _RGLPlatformGenerateFragmentProgram( _CGprogram *program, const CgProgramHeader *programHeader, const void *ucode, const CgParameterTableHeader *parameterHeader, const char *stringTable, const float *defaultValues );
CGbool _RGLPlatformSupportsFragmentProgram( CGprofile p );



void _RGLPlatformSetVertexRegister4fv( unsigned int reg, const float * _RGL_RESTRICT v );
void _RGLPlatformSetVertexRegisterBlock( unsigned int reg, unsigned int count, const float * _RGL_RESTRICT v );
void _RGLPlatformSetFragmentRegister4fv( unsigned int reg, const float * _RGL_RESTRICT v );
void _RGLPlatformSetFragmentRegisterBlock( unsigned int reg, unsigned int count, const float * _RGL_RESTRICT v );

unsigned int _cgHashString( const char *str );

static inline GLenum _RGLCgGetSamplerGLTypeFromCgType( CGtype type )
{
	switch ( type )
	{
		case CG_SAMPLER1D:
		case CG_SAMPLER2D:
		case CG_SAMPLERRECT:
			return GL_TEXTURE_2D;
		default:
			return 0;
	}
}

struct jsNameSpace;

int _RGLNVGenerateProgram( _CGprogram *program, int profileIndex, const CgProgramHeader *programHeader, const void *ucode,
		const CgParameterTableHeader *parameterHeader, const CgParameterEntry *parameterEntries,
		const char *stringTable, const float *defaultValues );

_cgSetArrayIndexFunction getVectorTypeIndexSetterFunction( unsigned short a, unsigned short b, unsigned short c, unsigned short d );
_cgSetArrayIndexFunction getMatrixTypeIndexSetterFunction( unsigned short a, unsigned short b, unsigned short c, unsigned short d, unsigned short e, unsigned short f );

// -------------------------------------------

typedef void( * CgcontextHookFunction )( _CGcontext *context );
extern  CgcontextHookFunction _cgContextCreateHook;
extern  CgcontextHookFunction _cgContextDestroyHook;

typedef void( * CgparameterHookFunction )( CgRuntimeParameter *parameter );
extern  CgparameterHookFunction _cgParameterCreateHook;
extern  CgparameterHookFunction _cgParameterDestroyHook;

typedef void( * CgprogramHookFunction )( _CGprogram *program );
typedef void( * CgprogramCopyHookFunction )( _CGprogram *newprogram, _CGprogram *oldprogram );
extern  CgprogramHookFunction _cgProgramCreateHook;
extern  CgprogramHookFunction _cgProgramDestroyHook;
extern  CgprogramCopyHookFunction _cgProgramCopyHook;

typedef int( * cgRTCgcCompileHookFunction )( const char*, const char *, const char*, const char**, char** );
typedef void( * cgRTCgcFreeHookFunction )( char* );
extern  cgRTCgcCompileHookFunction _cgRTCgcCompileProgramHook;
extern  cgRTCgcFreeHookFunction _cgRTCgcFreeCompiledProgramHook;


static inline const CgParameterResource *_RGLGetParameterResource( const _CGprogram *program, const CgParameterEntry *entry )
{
	return ( CgParameterResource * )( program->parameterResources + entry->typeIndex );
}

static inline CGtype _RGLGetParameterCGtype( const _CGprogram *program, const CgParameterEntry *entry )
{
	if ( entry->flags & CGP_RTCREATED )
	{
		return ( CGtype )entry->typeIndex;
	}
	else
	{
		const CgParameterResource *parameterResource = _RGLGetParameterResource( program, entry );
		if ( parameterResource )
		{
			return ( CGtype )parameterResource->type;
		}
	}
	return CG_UNKNOWN_TYPE;
}

static inline const CgParameterArray *_RGLGetParameterArray( const _CGprogram *program, const CgParameterEntry *entry )
{
	return ( CgParameterArray* )( program->parameterResources + entry->typeIndex );
}

static inline const CgParameterStructure *_RGLGetParameterStructure( const _CGprogram *program, const CgParameterEntry *entry )
{
	return ( CgParameterStructure* )( program->parameterResources + entry->typeIndex );
}

inline int _RGLGetProgramProfileIndex( CGprofile profile )
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
