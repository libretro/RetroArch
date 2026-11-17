/* What qnx_input.c names of libscreen, for qnx_checked_test alone: the
 * test runs the joypad driver's reads over the input driver's devices,
 * never these calls, so only the names are kept, not the SDK's values. */
#ifndef QNX_CHECKED_SCREEN_H
#define QNX_CHECKED_SCREEN_H
/* the SDK's headers bring errno in, which qnx_input.c uses */
#include <errno.h>

typedef void *screen_context_t;
typedef void *screen_device_t;
typedef void *screen_event_t;
enum
{
   SCREEN_EVENT_DEVICE = 1, SCREEN_EVENT_GAMEPAD, SCREEN_EVENT_JOYSTICK,
   SCREEN_EVENT_KEYBOARD, SCREEN_EVENT_MTOUCH_MOVE,
   SCREEN_EVENT_MTOUCH_RELEASE, SCREEN_EVENT_MTOUCH_TOUCH
};
enum
{
   SCREEN_PROPERTY_ANALOG = 1, SCREEN_PROPERTY_ANALOG0,
   SCREEN_PROPERTY_ANALOG1, SCREEN_PROPERTY_ATTACHED,
   SCREEN_PROPERTY_BUTTONS, SCREEN_PROPERTY_BUTTON_COUNT,
   SCREEN_PROPERTY_DEVICE, SCREEN_PROPERTY_DEVICES,
   SCREEN_PROPERTY_DEVICE_COUNT, SCREEN_PROPERTY_DISPLACEMENT,
   SCREEN_PROPERTY_ID_STRING, SCREEN_PROPERTY_KEY_CAP,
   SCREEN_PROPERTY_KEY_FLAGS, SCREEN_PROPERTY_KEY_MODIFIERS,
   SCREEN_PROPERTY_PRODUCT, SCREEN_PROPERTY_SOURCE_POSITION,
   SCREEN_PROPERTY_TOUCH_ID, SCREEN_PROPERTY_TYPE, SCREEN_PROPERTY_VENDOR
};
int screen_get_event_property_iv(screen_event_t ev, int prop, int *v);
int screen_get_event_property_pv(screen_event_t ev, int prop, void **v);
int screen_get_device_property_iv(screen_device_t d, int prop, int *v);
int screen_get_device_property_cv(screen_device_t d, int prop, int len, char *v);
int screen_get_context_property_iv(screen_context_t c, int prop, int *v);
int screen_get_context_property_pv(screen_context_t c, int prop, void **v);
#endif
