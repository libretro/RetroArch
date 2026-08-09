#ifndef STUB_SWITCH_H
#define STUB_SWITCH_H
#include <stdint.h>
typedef uint32_t Result;
#define R_SUCCEEDED(r) ((r) == 0)
#define CUR_PROCESS_HANDLE 0
typedef enum { InfoType_TotalMemorySize = 6, InfoType_UsedMemorySize = 7 } InfoType;
Result svcGetInfo(uint64_t *out, uint32_t id, uint32_t handle, uint64_t sub);
#endif
