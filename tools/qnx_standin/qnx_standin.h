/* Stand-ins for the BlackBerry 10 SDK's headers, for
 * tools/qnx_standin_check.sh: enough for the QNX input drivers to be
 * compiled where the SDK is not. Nothing here is the SDK's. Every
 * type is an opaque pointer, every constant a number of no meaning
 * (they only have to differ, for the switches), every call is declared
 * without its arguments. The headers beside this one - bps/, screen/,
 * sys/ - carry the SDK's names and include this.
 *
 * Add a line when a driver starts to use something more of the SDK. */
#ifndef QNX_STANDIN_H
#define QNX_STANDIN_H

typedef struct qnx_standin_bps_event_t *bps_event_t;
typedef struct qnx_standin_screen_context_t *screen_context_t;
typedef struct qnx_standin_screen_device_t *screen_device_t;
typedef struct qnx_standin_screen_event_t *screen_event_t;

#define BPS_SUCCESS 1
#define KEY_DOWN 2
#define KEY_REPEAT 3
#define NAVIGATOR_EXIT 4
#define NAVIGATOR_SWIPE_DOWN 5
#define NAVIGATOR_SYSKEY_BACK 6
#define NAVIGATOR_SYSKEY_END 7
#define NAVIGATOR_SYSKEY_PRESS 8
#define NAVIGATOR_SYSKEY_SEND 9
#define NAVIGATOR_WINDOW_FULLSCREEN 10
#define NAVIGATOR_WINDOW_INVISIBLE 11
#define NAVIGATOR_WINDOW_STATE 12
#define NAVIGATOR_WINDOW_THUMBNAIL 13
#define SCREEN_EVENT_DEVICE 14
#define SCREEN_EVENT_GAMEPAD 15
#define SCREEN_EVENT_JOYSTICK 16
#define SCREEN_EVENT_KEYBOARD 17
#define SCREEN_EVENT_MTOUCH_MOVE 18
#define SCREEN_EVENT_MTOUCH_RELEASE 19
#define SCREEN_EVENT_MTOUCH_TOUCH 20
#define SCREEN_PROPERTY_ANALOG0 21
#define SCREEN_PROPERTY_ANALOG1 22
#define SCREEN_PROPERTY_ATTACHED 23
#define SCREEN_PROPERTY_BUTTONS 24
#define SCREEN_PROPERTY_BUTTON_COUNT 25
#define SCREEN_PROPERTY_DEVICE 26
#define SCREEN_PROPERTY_DEVICES 27
#define SCREEN_PROPERTY_DEVICE_COUNT 28
#define SCREEN_PROPERTY_DISPLACEMENT 29
#define SCREEN_PROPERTY_ID_STRING 30
#define SCREEN_PROPERTY_KEY_CAP 31
#define SCREEN_PROPERTY_KEY_FLAGS 32
#define SCREEN_PROPERTY_KEY_MODIFIERS 33
#define SCREEN_PROPERTY_PRODUCT 34
#define SCREEN_PROPERTY_SOURCE_POSITION 35
#define SCREEN_PROPERTY_TOUCH_ID 36
#define SCREEN_PROPERTY_TYPE 37
#define SCREEN_PROPERTY_VENDOR 38

int bps_event_get_code();
int bps_event_get_domain();
int bps_get_event();
int navigator_event_get_syskey_key();
int navigator_event_get_window_state();
int navigator_get_domain();
int screen_event_get_event();
int screen_get_context_property_iv();
int screen_get_context_property_pv();
int screen_get_device_property_cv();
int screen_get_device_property_iv();
int screen_get_domain();
int screen_get_event_property_iv();
int screen_get_event_property_pv();
#endif
