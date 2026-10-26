#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   uint32_t core[3];
   uint32_t __unknown;
} OSRendezvous;

void OSInitRendezvous(OSRendezvous *rendezvous);
BOOL OSWaitRendezvous(OSRendezvous *rendezvous, uint32_t coreMask);
BOOL OSWaitRendezvousWithTimeout(OSRendezvous *rendezvous, uint32_t coreMask, OSTime timeout);

#ifdef __cplusplus
}
#endif
