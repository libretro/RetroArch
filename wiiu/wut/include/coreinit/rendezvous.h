#pragma once
#include <wut.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSRendezvous OSRendezvous;

struct OSRendezvous
{
   uint32_t core[3];
   uint32_t __unknown;
};

void OSInitRendezvous(OSRendezvous *rendezvous);
BOOL OSWaitRendezvous(OSRendezvous *rendezvous, uint32_t coreMask);
BOOL OSWaitRendezvousWithTimeout(OSRendezvous *rendezvous, uint32_t coreMask, OSTime timeout);

#ifdef __cplusplus
}
#endif
