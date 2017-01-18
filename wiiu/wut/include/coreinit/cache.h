#pragma once
#include <wut.h>

/**
 * \defgroup coreinit_cache Cache
 * \ingroup coreinit
 *
 * Cache synchronisation functions.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Equivalent to dcbi instruction.
 */
void
DCInvalidateRange(void *addr,
                  uint32_t size);


/**
 * Equivalent to dcbf, sync, eieio.
 */
void
DCFlushRange(void *addr,
             uint32_t size);


/**
 * Equivalent to dcbst, sync, eieio.
 */
void
DCStoreRange(void *addr,
             uint32_t size);


/**
 * Equivalent to dcbf.
 *
 * Does not perform sync, eieio like DCFlushRange.
 */
void
DCFlushRangeNoSync(void *addr,
                   uint32_t size);


/**
 * Equivalent to dcbst.
 *
 * Does not perform sync, eieio like DCStoreRange.
 */
void
DCStoreRangeNoSync(void *addr,
                   uint32_t size);


/**
 * Equivalent to dcbz instruction.
 */
void
DCZeroRange(void *addr,
            uint32_t size);


/**
 * Equivalent to dcbt instruction.
 */
void
DCTouchRange(void *addr,
             uint32_t size);


/**
 * Equivalent to icbi instruction.
 */
void
ICInvalidateRange(void *addr,
                  uint32_t size);

#ifdef __cplusplus
}
#endif

/** @} */
