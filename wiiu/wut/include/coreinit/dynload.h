#pragma once
#include <wut.h>
#include "thread.h"
#include "time.h"

/**
 * \defgroup coreinit_dynload Dynamic Loading
 * \ingroup coreinit
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef void *OSDynLoadModule;

typedef int (*OSDynLoadAllocFn)(int size, int align, void **outAddr);
typedef void (*OSDynLoadFreeFn)(void *addr);


/**
 * Set the allocator function to use for dynamic loading.
 */
int32_t
OSDynLoad_SetAllocator(OSDynLoadAllocFn allocFn,
                       OSDynLoadFreeFn freeFn);


/**
 * Get the allocator function used for dynamic loading.
 */
int32_t
OSDynLoad_GetAllocator(OSDynLoadAllocFn *outAllocFn,
                       OSDynLoadFreeFn *outFreeFn);


/**
 * Load a module.
 *
 * If the module is already loaded, increase reference count.
 * Similar to LoadLibrary on Windows.
 */
int32_t
OSDynLoad_Acquire(char const *name,
                  OSDynLoadModule *outModule);


/**
 * Retrieve the address of a function or data export from a module.
 *
 * Similar to GetProcAddress on Windows.
 */
int32_t
OSDynLoad_FindExport(OSDynLoadModule module,
                     int32_t isData,
                     char const *name,
                     void **outAddr);


/**
 * Free a module handle returned from OSDynLoad_Acquire.
 *
 * Will decrease reference count and only unload the module if count reaches 0.
 * Similar to FreeLibrary on Windows.
 */
void
OSDynLoad_Release(OSDynLoadModule module);

#ifdef __cplusplus
}
#endif

/** @} */
