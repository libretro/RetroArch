#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void DCInvalidateRange(void *addr, uint32_t size);  /* Equivalent to dcbi instruction. */
void DCFlushRange(void *addr, uint32_t size);       /* Equivalent to dcbf, sync, eieio. */
void DCStoreRange(void *addr, uint32_t size);       /* Equivalent to dcbst, sync, eieio. */
void DCFlushRangeNoSync(void *addr, uint32_t size); /* Equivalent to dcbf. Does not perform sync, eieio like DCFlushRange. */
void DCStoreRangeNoSync(void *addr, uint32_t size); /* Equivalent to dcbst. Does not perform sync, eieio like DCStoreRange. */
void DCZeroRange(void *addr, uint32_t size);        /* Equivalent to dcbz instruction. */
void DCTouchRange(void *addr, uint32_t size);       /* Equivalent to dcbt instruction. */
void ICInvalidateRange(void *addr, uint32_t size);  /* Equivalent to icbi instruction. */

#ifdef __cplusplus
}
#endif
