#ifndef WIIU_DBG_H
#define WIIU_DBG_H

#include <stdio.h>
#include <wiiu/types.h>
#ifdef __cplusplus
extern "C" {
#endif
//void wait_for_input(void);
//void dump_result_value(Result val);
#ifdef __cplusplus
}
#endif
void* OSGetSymbolName(u32 addr, char* out, u32 out_size);
void DisassemblePPCRange(void *start, void *end, void* printf_func, void* GetSymbolName_func, u32 flags);

#define DEBUG_DISASM(start, count) DisassemblePPCRange((void*)start, (u32*)(start) + (count), printf, OSGetSymbolName, 0)

//#define DEBUG_HOLD() do{printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);wait_for_input();}while(0)
#define DEBUG_LINE() do{printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);}while(0)
#define DEBUG_STR(X) printf( "%s: %s\n", #X, (char*)(X))
#define DEBUG_VAR(X) printf( "%-20s: 0x%08X\n", #X, (u32)(X))
#define DEBUG_VAR2(X) printf( "%-20s: 0x%08X (%i)\n", #X, (u32)(X), (int)(X))
#define DEBUG_INT(X) printf( "%-20s: %10i\n", #X, (s32)(X))
#define DEBUG_FLOAT(X) printf( "%-20s: %10.3f\n", #X, (float)(X))
#define DEBUG_VAR64(X) printf( #X"\r\t\t\t\t : 0x%016llX\n", (u64)(X))
//#define DEBUG_ERROR(X) do{if(X)dump_result_value(X);}while(0)
#define PRINTFPOS(X,Y) "\x1b["#X";"#Y"H"
#define PRINTFPOS_STR(X,Y) "\x1b[" X ";" Y "H"
#define PRINTF_LINE(X) "\x1b[" X ";0H"

#endif // WIIU_DBG_H
