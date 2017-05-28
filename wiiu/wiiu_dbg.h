#ifndef WIIU_DBG_H
#define WIIU_DBG_H

#include <stdio.h>
#include <stdint.h>

#ifdef WIIU
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

//void wait_for_input(void);
//void dump_result_value(Result val);
void* OSGetSymbolName(u32 addr, char* out, u32 out_size);
void DisassemblePPCRange(void *start, void *end, void* printf_func, void* GetSymbolName_func, u32 flags);

#ifdef __cplusplus
}
#endif

#define DEBUG_DISASM(start, count) DisassemblePPCRange((void*)start, (u32*)(start) + (count), printf, OSGetSymbolName, 0)
#endif /* WIIU */

//#define DEBUG_HOLD() do{printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);wait_for_input();}while(0)
#define DEBUG_LINE() do{printf("%s:%4d %s().\n", __FILE__, __LINE__, __FUNCTION__);fflush(stdout);}while(0)
#define DEBUG_STR(X) printf( "%s: %s\n", #X, (char*)(X))
#define DEBUG_VAR(X) printf( "%-20s: 0x%08X\n", #X, (uint32_t)(X))
#define DEBUG_VAR2(X) printf( "%-20s: 0x%08X (%i)\n", #X, (uint32_t)(X), (int)(X))
#define DEBUG_INT(X) printf( "%-20s: %10i\n", #X, (int32_t)(X))
#define DEBUG_FLOAT(X) printf( "%-20s: %10.3f\n", #X, (float)(X))
#define DEBUG_VAR64(X) printf( #X"\r\t\t\t\t : 0x%016llX\n", (uint64_t)(X))
//#define DEBUG_ERROR(X) do{if(X)dump_result_value(X);}while(0)
#define PRINTFPOS(X,Y) "\x1b["#X";"#Y"H"
#define PRINTFPOS_STR(X,Y) "\x1b[" X ";" Y "H"
#define PRINTF_LINE(X) "\x1b[" X ";0H"

#endif // WIIU_DBG_H
