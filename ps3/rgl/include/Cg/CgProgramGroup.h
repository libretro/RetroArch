#ifndef CGPROGRAMGROUP_HEADER
#define CGPROGRAMGROUP_HEADER

#include "Cg/cgBinary.h"

typedef struct _CGnamedProgram
{
   const char *name;
   CGprogram program;
   int refCount;
}
_CGnamedProgram;

typedef struct _CGprogramGroup
{
   struct _CGprogramGroup *next;
   CGcontext ctx;
   unsigned int *constantTable;
   unsigned int *stringTable;
   unsigned int programCount;
   _CGnamedProgram *programs;
   int refCount; //total number of program refCounted
   bool userCreated;
   char *filedata;
   char *name;
}
_CGprogramGroup;

/* Program groups */
typedef struct _CGprogramGroup *CGprogramGroup;

CGprogramGroup rglCgCreateProgramGroup( CGcontext ctx, const char *name, void *ptr, int size );
CGprogramGroup rglCgCreateProgramGroupFromFile( CGcontext ctx, const char *group_file );
void rglCgDestroyProgramGroup( CGprogramGroup group );

int rglCgGetProgramCount( CGprogramGroup group );
CGprogram rglCgGetProgram( CGprogramGroup group, const char *name );
int rglCgGetProgramIndex( CGprogramGroup group, const char *name );
CGprogram rglCgGetProgramAtIndex( CGprogramGroup group, unsigned int index );
const char *rglCgGetProgramGroupName( CGprogramGroup group );

#endif
