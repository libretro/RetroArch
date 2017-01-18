#pragma once
#include <wut.h>
#include "resource.h"

/**
 * \defgroup gx2r_displaylist Display List
 * \ingroup gx2r
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GX2RBuffer GX2RBuffer;

void
GX2RBeginDisplayListEx(GX2RBuffer *displayList,
                       uint32_t unknown,
                       GX2RResourceFlags flags);

uint32_t
GX2REndDisplayList(GX2RBuffer *displayList);

void
GX2RCallDisplayList(GX2RBuffer *displayList,
                    uint32_t size);

void
GX2RDirectCallDisplayList(GX2RBuffer *displayList,
                          uint32_t size);

#ifdef __cplusplus
}
#endif

/** @} */
