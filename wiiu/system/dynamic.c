#include <coreinit/dynload.h>
#include <coreinit/debug.h>

#define EXPORT(name) void* addr_##name
#include "../rpl/libcoreinit/exports.h"
#include "../rpl/libnsysnet/exports.h"
#include "../rpl/libgx2/exports.h"
#include "../rpl/libproc_ui/exports.h"
#include "../rpl/libsndcore2/exports.h"
#include "../rpl/libsysapp/exports.h"
#include "../rpl/libvpad/exports.h"

#undef EXPORT
#define EXPORT(name) do{if(OSDynLoad_FindExport(handle, 0, #name, &addr_##name) < 0)OSFatal("Function " # name " is NULL");} while(0)

void InitFunctionPointers(void)
{
   OSDynLoadModule handle;
   addr_OSDynLoad_Acquire = *(void**)0x00801500;
   addr_OSDynLoad_FindExport = *(void**)0x00801504;

   OSDynLoad_Acquire("coreinit.rpl", &handle);
   OSDynLoad_FindExport(handle, 0, "OSFatal", &addr_OSFatal);
   #include "../rpl/libcoreinit/exports.h"
   OSDynLoad_Release(handle);

   OSDynLoad_Acquire("nsysnet.rpl", &handle);
   #include "../rpl/libnsysnet/exports.h"
   OSDynLoad_Release(handle);

   OSDynLoad_Acquire("gx2.rpl", &handle);
   #include "../rpl/libgx2/exports.h"
   OSDynLoad_Release(handle);

   OSDynLoad_Acquire("proc_ui.rpl", &handle);
   #include "../rpl/libproc_ui/exports.h"
   OSDynLoad_Release(handle);

   OSDynLoad_Acquire("sndcore2.rpl", &handle);
   #include "../rpl/libsndcore2/exports.h"
   OSDynLoad_Release(handle);

   OSDynLoad_Acquire("sysapp.rpl", &handle);
   #include "../rpl/libsysapp/exports.h"
   OSDynLoad_Release(handle);

   OSDynLoad_Acquire("vpad.rpl", &handle);
   #include "../rpl/libvpad/exports.h"
   OSDynLoad_Release(handle);

}
