#pragma once
#include <wut.h>

/**
 * \defgroup coreinit_core Core Identification
 * \ingroup coreinit
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Returns the number of cores, should always be 3.
 */
uint32_t
OSGetCoreCount();


/**
 * Returns the ID of the core currently executing this thread.
 */
uint32_t
OSGetCoreId();


/**
 * Returns the ID of the main core.
 */
uint32_t
OSGetMainCoreId();


/**
 * Returns true if the current core is the main core.
 */
BOOL
OSIsMainCore();


#ifdef __cplusplus
}
#endif

/** @} */
