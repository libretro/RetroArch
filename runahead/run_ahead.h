#ifndef __RUN_AHEAD_H__
#define __RUN_AHEAD_H__

#include "boolean.h"
#include "retro_common_api.h"

RETRO_BEGIN_DECLS

void runahead_destroy(void);
void run_ahead(int runAheadCount, bool useSecondary);

RETRO_END_DECLS

#endif
