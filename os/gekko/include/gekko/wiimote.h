/* Wii: Wii Remotes and their Nunchuk, Classic Controller and guitar
 * extensions, over Bluetooth.
 *
 * Remotes synced to the console connect when a button is pressed and
 * take the first free player slot; the slot's LED lights up.  Buttons
 * read 1 when pressed. */

#ifndef GEKKO_WIIMOTE_H
#define GEKKO_WIIMOTE_H

#include <gekko/gekko.h>

#define GK_WIIMOTE_SLOTS 4

/* The remote's buttons. */
#define GK_WM_LEFT   0x0100
#define GK_WM_RIGHT  0x0200
#define GK_WM_DOWN   0x0400
#define GK_WM_UP     0x0800
#define GK_WM_PLUS   0x1000
#define GK_WM_TWO    0x0001
#define GK_WM_ONE    0x0002
#define GK_WM_B      0x0004
#define GK_WM_A      0x0008
#define GK_WM_MINUS  0x0010
#define GK_WM_HOME   0x0080

/* The Nunchuk's, in ext_buttons. */
#define GK_NC_Z      0x0001
#define GK_NC_C      0x0002

/* The Classic Controller's, in ext_buttons. */
#define GK_CC_UP     0x0001
#define GK_CC_LEFT   0x0002
#define GK_CC_ZR     0x0004
#define GK_CC_X      0x0008
#define GK_CC_A      0x0010
#define GK_CC_Y      0x0020
#define GK_CC_B      0x0040
#define GK_CC_ZL     0x0080
#define GK_CC_R      0x0200
#define GK_CC_PLUS   0x0400
#define GK_CC_HOME   0x0800
#define GK_CC_MINUS  0x1000
#define GK_CC_L      0x2000
#define GK_CC_DOWN   0x4000
#define GK_CC_RIGHT  0x8000

/* The guitar's, in ext_buttons; strumming is up and down. */
#define GK_GH_GREEN  GK_CC_A
#define GK_GH_RED    GK_CC_B
#define GK_GH_YELLOW GK_CC_X
#define GK_GH_BLUE   GK_CC_Y
#define GK_GH_ORANGE GK_CC_ZL
#define GK_GH_UP     GK_CC_UP
#define GK_GH_DOWN   GK_CC_DOWN
#define GK_GH_PLUS   GK_CC_PLUS
#define GK_GH_MINUS  GK_CC_MINUS
#define GK_GH_ALL    0x54f9

enum gk_wiimote_ext
{
   GK_WM_EXT_NONE = 0,
   GK_WM_EXT_NUNCHUK,
   GK_WM_EXT_CLASSIC,
   GK_WM_EXT_GUITAR,
   GK_WM_EXT_OTHER
};

typedef struct gk_wiimote
{
   uint32_t reports;        /* input reports so far */
   uint16_t buttons;
   uint16_t ext_buttons;
   uint16_t accel[3];       /* 10 bits, about 512 at rest */
   int16_t  ir_x;           /* where it points: -32767..32767 left to */
   int16_t  ir_y;           /* right, top to bottom; held at the edge */
   int8_t   stick[2][2];    /* x, y: -128..127, up positive */
   uint8_t  trigger[2];     /* Classic L, R (or whammy), 0..255 */
   uint8_t  connected;
   uint8_t  ext;            /* enum gk_wiimote_ext */
   uint8_t  battery;        /* 0..255 */
   uint8_t  ir_dots;        /* sensor bar dots the camera sees, 0..4 */
   uint8_t  ir_valid;       /* the pointer is on the screen */
} gk_wiimote_t;

/* Start Bluetooth and listen for remotes; 0 or a negative errno. */
int  gk_wiimote_init(void);
/* The state of a player slot; 0 when connected. */
int  gk_wiimote_read(unsigned slot, gk_wiimote_t *out);
void gk_wiimote_rumble(unsigned slot, int on);
void gk_wiimote_disconnect(unsigned slot);

#endif
