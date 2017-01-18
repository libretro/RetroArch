#pragma once
#include <wut.h>

/**
 * \defgroup coreinit_exit Exit
 * \ingroup coreinit
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

void
exit(int code);

void
_Exit();

#ifdef __cplusplus
}
#endif

/** @} */
