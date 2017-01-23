#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSCoroutine
{
   uint32_t nia;
   uint32_t cr;
   uint32_t ugqr1;
   uint32_t stack;
   uint32_t sda2Base;
   uint32_t sdaBase;
   uint32_t gpr[18];
   double fpr[18];
   double psr[18];
}OSCoroutine;

void OSInitCoroutine(OSCoroutine *coroutine, void *entry, void *stack);
uint32_t OSLoadCoroutine(OSCoroutine *coroutine, uint32_t result);
uint32_t OSSaveCoroutine(OSCoroutine *coroutine);
void OSSwitchCoroutine(OSCoroutine *from, OSCoroutine *to);

#ifdef __cplusplus
}
#endif
