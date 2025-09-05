#pragma once

#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _KEYState
{
    KBD_WIIU_DOWN                = 0x01,
    KBD_WIIU_REPEAT              = 0x02,
    KBD_WIIU_NULL                = 0x00
} KEYState;

typedef enum _KBDModifier
{
    KBD_WIIU_CTRL                = 0x0001,
    KBD_WIIU_SHIFT               = 0x0002,
    KBD_WIIU_ALT                 = 0x0004,
    KBD_WIIU_NUM_LOCK            = 0x0100,
    KBD_WIIU_CAPS_LOCK           = 0x0200,
    KBD_WIIU_SCROLL_LOCK         = 0x0400,
} KBDModifier;

typedef struct _KBDKeyEvent
{
    unsigned char  channel;
    unsigned char  scancode;    /* scancode */
    KEYState  state;            /* when held, value is 0x03, which is KBD_DOWN & KBD_REPEAT */
    KBDModifier modifier;       /* modifier state */
    unsigned short  UTF16;	    /* unicode, if any */
} KBDKeyEvent;

char KBDSetup(void *connection_callback, void *disconnection_callback, void *key_callback);
char KBDTeardown();

#ifdef __cplusplus
}
#endif
