#ifndef STUB_WINDOWS_H
#define STUB_WINDOWS_H
#include <stdint.h>
typedef uint32_t DWORD;
typedef uint64_t DWORDLONG;
typedef struct { DWORD dwLength; DWORD dwMemoryLoad; DWORDLONG ullTotalPhys;
   DWORDLONG ullAvailPhys; } MEMORYSTATUSEX;
typedef struct { DWORD dwLength; DWORD dwMemoryLoad; DWORD dwTotalPhys;
   DWORD dwAvailPhys; } MEMORYSTATUS;
void GlobalMemoryStatusEx(MEMORYSTATUSEX *s);
void GlobalMemoryStatus(MEMORYSTATUS *s);
#endif
