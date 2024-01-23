#ifndef __WEBHOOKS_PROGRESS_TRACKER_H
#define __WEBHOOKS_PROGRESS_TRACKER_H

#include "../deps/rcheevos/include/rc_runtime.h"

enum {
    PROGRESS_UNCHANGED = 0,
    PROGRESS_UPDATED = 1
};

const char* wpt_get_last_progress
(
  void
);

void wpt_clear_progress
(
  void
);

int wpt_process_frame
(
    rc_runtime_t* runtime
);

#endif /* __WEBHOOKS_PROGRESS_TRACKER_H */
