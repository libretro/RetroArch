#include <coreinit/dynload.h>
#include <coreinit/debug.h>

#define EXPORT(name) void* addr_##name
#define EXPORT_BEGIN(lib)
#define EXPORT_END()
#include "exports/all.h"

#undef EXPORT
#undef EXPORT_BEGIN
//#undef EXPORT_END

#define EXPORT(name)       do{if(OSDynLoad_FindExport(handle, 0, #name, &addr_##name) < 0)OSFatal("Function " # name " is NULL");} while(0)
#define EXPORT_BEGIN(lib)  OSDynLoad_Acquire(#lib, &handle)
//#define EXPORT_END()       OSDynLoad_Release(handle)

void InitFunctionPointers(void)
{
   OSDynLoadModule handle;
   addr_OSDynLoad_Acquire = *(void**)0x00801500;
   addr_OSDynLoad_FindExport = *(void**)0x00801504;

#include "exports/all.h"

}
