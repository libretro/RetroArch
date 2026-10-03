/* Wii: the remote's pointer from its IR camera's dots; portable, so
 * the host tests can check it. */

#ifndef GEKKO_RVL_IR_H
#define GEKKO_RVL_IR_H

#include <stdint.h>

/* The sensor bar as last seen, for when one dot drops out. */
typedef struct ir_track
{
   float   half[2];     /* from the bar's midpoint to its right dot */
   float   mid[2];
   uint8_t have_bar;    /* half and mid hold */
} ir_track;

/* basic: the camera's 10 bytes in the basic format.  x and y are
 * -32767..32767 across the screen, left to right and top to bottom,
 * held at the edge; valid says the remote points at the screen. */
void ir_pointer(ir_track *t, const uint8_t *basic, int bar_on_top,
      int16_t *x, int16_t *y, uint8_t *dots, uint8_t *valid);

#endif
