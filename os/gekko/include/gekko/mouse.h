/* Wii: USB mice (boot protocol HID), all of them together, as motion
 * since the last look. */

#ifndef GEKKO_MOUSE_H
#define GEKKO_MOUSE_H

#include <gekko/gekko.h>

#define GK_MOUSE_LEFT   0x01
#define GK_MOUSE_RIGHT  0x02
#define GK_MOUSE_MIDDLE 0x04
#define GK_MOUSE_4      0x08
#define GK_MOUSE_5      0x10

typedef struct gk_mouse
{
   int32_t dx;          /* right positive */
   int32_t dy;          /* down positive */
   int32_t wheel;       /* away from the user positive */
   uint8_t buttons;     /* GK_MOUSE_* held */
   uint8_t count;       /* mice plugged in */
} gk_mouse_t;

/* What the mice did since the last call.  The first call starts
 * looking for them. */
void gk_mouse_read(gk_mouse_t *m);

#endif
