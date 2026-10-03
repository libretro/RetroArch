/* GameCube controllers on the serial interface, polled once a field. */

#ifndef GEKKO_PAD_H
#define GEKKO_PAD_H

#include <gekko/gekko.h>

#define GK_PAD_PORTS 4

#define GK_PAD_LEFT   0x0001u
#define GK_PAD_RIGHT  0x0002u
#define GK_PAD_DOWN   0x0004u
#define GK_PAD_UP     0x0008u
#define GK_PAD_Z      0x0010u
#define GK_PAD_R      0x0020u   /* digital click */
#define GK_PAD_L      0x0040u
#define GK_PAD_A      0x0100u
#define GK_PAD_B      0x0200u
#define GK_PAD_X      0x0400u
#define GK_PAD_Y      0x0800u
#define GK_PAD_START  0x1000u

typedef struct gk_pad
{
   uint32_t polls;              /* reports received since connecting */
   uint16_t buttons;
   int8_t   stick_x, stick_y;   /* from the origin the controller reports */
   int8_t   sub_x, sub_y;
   uint8_t  trigger_l, trigger_r;
   uint8_t  connected;
} gk_pad_t;

void gk_pad_init(void);
/* Latest state of each port; looks for newly plugged controllers now
 * and then. */
void gk_pad_read(gk_pad_t pads[GK_PAD_PORTS]);
void gk_pad_rumble(unsigned port, int on);

#endif
