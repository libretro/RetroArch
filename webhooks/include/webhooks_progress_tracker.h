#ifndef __WEBHOOKS_PROGRESS_TRACKER_H
#define __WEBHOOKS_PROGRESS_TRACKER_H

#include "../deps/rcheevos/include/rc_runtime.h"

RETRO_BEGIN_DECLS

enum {
    PROGRESS_UNCHANGED = 0,
    PROGRESS_UPDATED = 1
};

const char* wpt_get_last_progress();

void wpt_clear_progress();

int wpt_process_frame
(
    rc_runtime_t* runtime
);

RETRO_END_DECLS

#endif /* __WEBHOOKS_PROGRESS_TRACKER_H */
