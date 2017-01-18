#pragma once
#include <wut.h>
#include "enum.h"

/**
 * \defgroup gx2_state State
 * \ingroup gx2
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

void
GX2Init(uint32_t *attributes);

void
GX2Shutdown();

void
GX2Flush();

#ifdef __cplusplus
}
#endif

/** @} */
