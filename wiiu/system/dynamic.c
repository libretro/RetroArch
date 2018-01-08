#include <wiiu/os/dynload.h>
#include <wiiu/os/debug.h>

#define IMPORT(name) void* addr_##name
#define IMPORT_BEGIN(lib)
#define IMPORT_END()
#include "imports.h"

#undef IMPORT
#undef IMPORT_BEGIN
#undef IMPORT_END

#define IMPORT(name)       do{if(OSDynLoad_FindExport(handle, 0, #name, &addr_##name) < 0)OSFatal("Function " # name " is NULL");} while(0)
#define IMPORT_BEGIN(lib)  OSDynLoad_Acquire(#lib ".rpl", &handle)
/* #define IMPORT_END()       OSDynLoad_Release(handle) */
#define IMPORT_END()

void InitFunctionPointers(void)
{
   OSDynLoadModule handle;
   addr_OSDynLoad_Acquire = *(void**)0x00801500;
   addr_OSDynLoad_FindExport = *(void**)0x00801504;

#include "imports.h"

}
