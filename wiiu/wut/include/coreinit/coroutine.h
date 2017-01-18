#pragma once
#include <wut.h>

/**
 * \defgroup coreinit_coroutine Coroutines
 * \ingroup coreinit
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSCoroutine OSCoroutine;

struct OSCoroutine
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
};
CHECK_OFFSET(OSCoroutine, 0x00, nia);
CHECK_OFFSET(OSCoroutine, 0x04, cr);
CHECK_OFFSET(OSCoroutine, 0x08, ugqr1);
CHECK_OFFSET(OSCoroutine, 0x0C, stack);
CHECK_OFFSET(OSCoroutine, 0x10, sda2Base);
CHECK_OFFSET(OSCoroutine, 0x14, sdaBase);
CHECK_OFFSET(OSCoroutine, 0x18, gpr);
CHECK_OFFSET(OSCoroutine, 0x60, fpr);
CHECK_OFFSET(OSCoroutine, 0xF0, psr);
CHECK_SIZE(OSCoroutine, 0x180);

void
OSInitCoroutine(OSCoroutine *coroutine,
                void *entry,
                void *stack);

uint32_t
OSLoadCoroutine(OSCoroutine *coroutine,
                uint32_t result);

uint32_t
OSSaveCoroutine(OSCoroutine *coroutine);

void
OSSwitchCoroutine(OSCoroutine *from,
                  OSCoroutine *to);

#ifdef __cplusplus
}
#endif

/** @} */
