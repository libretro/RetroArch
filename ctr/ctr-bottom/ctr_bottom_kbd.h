/*
 * ctr_bottom_kbd.h
 */

#ifndef CTR_BOTTOM_KBD_H_
#define CTR_BOTTOM_KBD_H_

typedef struct {
	unsigned key;
	unsigned gfx;
	unsigned x0;
	unsigned y0;
	unsigned x1;
	unsigned y1;
} ctr_bottom_kbd_lut_t;
extern ctr_bottom_kbd_lut_t ctr_bottom_kbd_lut[];

unsigned ctr_bottom_kbd_get_key(s16 T_X, s16 T_Y);
void ctr_bottom_kbd_set_mod(int PressedKey);
void ctr_bottom_kbd_rst_mod();

#endif /* CTR_BOTTOM_KBD_H_ */
