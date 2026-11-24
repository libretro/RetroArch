/* What qnx_input.c names of BPS's events, for qnx_checked_test alone
 * (see screen/screen.h beside it). */
#ifndef QNX_CHECKED_BPS_EVENT_H
#define QNX_CHECKED_BPS_EVENT_H
typedef struct bps_event bps_event_t;
#define BPS_SUCCESS 0
int bps_get_event(bps_event_t **event, int timeout_ms);
unsigned bps_event_get_code(bps_event_t *event);
int bps_event_get_domain(bps_event_t *event);
int screen_get_domain(void);
#include <screen/screen.h>
screen_event_t screen_event_get_event(bps_event_t *event);
#endif
