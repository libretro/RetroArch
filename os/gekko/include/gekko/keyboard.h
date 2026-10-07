/* Wii: USB keyboards (boot protocol HID).  Events queue up as the
 * keyboards report and are taken by polling. */

#ifndef GEKKO_KEYBOARD_H
#define GEKKO_KEYBOARD_H

#include <gekko/gekko.h>

enum gk_kbd_type
{
   GK_KBD_CONNECT = 0,
   GK_KBD_DISCONNECT,
   GK_KBD_KEYS
};

/* Modifier bits: left control, shift, alt, super, then the right
 * ones. */
#define GK_KBD_LCTRL  0x01
#define GK_KBD_LSHIFT 0x02
#define GK_KBD_LALT   0x04
#define GK_KBD_LSUPER 0x08
#define GK_KBD_RCTRL  0x10
#define GK_KBD_RSHIFT 0x20
#define GK_KBD_RALT   0x40
#define GK_KBD_RSUPER 0x80

typedef struct gk_kbd_event
{
   uint32_t id;          /* which keyboard */
   uint8_t  type;        /* enum gk_kbd_type */
   uint8_t  modifiers;   /* GK_KBD_* held */
   uint8_t  keys[6];     /* USB HID usages held, 0 for none */
} gk_kbd_event_t;

/* The next event: 1, or 0 when there is none.  The first call starts
 * looking for keyboards; those plugged in already arrive as
 * connections. */
int gk_kbd_read(gk_kbd_event_t *ev);

#endif
