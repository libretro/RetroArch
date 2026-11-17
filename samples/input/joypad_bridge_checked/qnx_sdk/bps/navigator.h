/* What qnx_input.c names of BPS's navigator, for qnx_checked_test alone
 * (see screen/screen.h beside it). */
#ifndef QNX_CHECKED_BPS_NAVIGATOR_H
#define QNX_CHECKED_BPS_NAVIGATOR_H
#include <bps/event.h>
enum
{
   NAVIGATOR_EXIT = 1, NAVIGATOR_SWIPE_DOWN, NAVIGATOR_SYSKEY_PRESS,
   NAVIGATOR_WINDOW_STATE
};
enum { NAVIGATOR_SYSKEY_BACK = 1, NAVIGATOR_SYSKEY_END, NAVIGATOR_SYSKEY_SEND };
enum
{
   NAVIGATOR_WINDOW_FULLSCREEN = 1, NAVIGATOR_WINDOW_INVISIBLE,
   NAVIGATOR_WINDOW_THUMBNAIL
};
int navigator_get_domain(void);
int navigator_event_get_syskey_key(bps_event_t *event);
int navigator_event_get_window_state(bps_event_t *event);
#endif
