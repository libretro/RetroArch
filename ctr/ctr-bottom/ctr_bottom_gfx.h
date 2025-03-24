/*
 * ctr_bottom_gfx.h
 */

#ifndef CTR_BOTTOM_GFX_H_
#define CTR_BOTTOM_GFX_H_

void gfxFadeScreen(gfxScreen_t screen, gfx3dSide_t side, u32 f);

enum {
   CTR_BOTTOM_TEXTURE_GFX,
   CTR_BOTTOM_TEXTURE_THUMBNAIL,
} ctr_bottom_texture_enum;

struct ctr_bottom_gfx_t {
	char* path;
	unsigned int x;
	unsigned int y;
};

struct ctr_bottom_gfx_t ctr_bottom_gfx[] = {
   {"ctr_bottom_idle.png",       0,   0},
   {"ctr_bottom_mouse.png",      0,   0},
   {"ctr_bottom_savestate.png",  0,   0},
   {"ctr_bottom_todo.png",       0,   0},
   {"kbd/kbd_lower.png",         0,   0},
   {"kbd/kbd_upper.png",         0,   0},
   {"kbd/kbd_symbol.png",        0,   0},
   {"kbd/kbd_number.png",        0,   0},
};

enum {
   CTR_TEXTURE_IDLE,
   CTR_TEXTURE_MOUSE,
   CTR_TEXTURE_SAVESTATE,
   CTR_TEXTURE_TODO,
   CTR_TEXTURE_KBD_LOWER,
   CTR_TEXTURE_KBD_UPPER,
   CTR_TEXTURE_KBD_SYMBOL,
   CTR_TEXTURE_KBD_NUMBER
} ctr_bottom_gfx_enum;

struct ctr_bottom_gfx_t ctr_bottom_kbd_gfx[] = {
   {"kbd/key_key.png",      0,   0},
   {"kbd/key_fkey.png",     0,   0},
   {"kbd/key_alt.png",    233, 167},
   {"kbd/key_ctrl.png",   277, 167},
   {"kbd/key_enter.png",  278,  68},
   {"kbd/key_shift.png",  241, 134},
   {"kbd/key_caps.png",     2, 134},
   {"kbd/key_space.png",   90, 167},
   {"kbd/key_tab.png",      2, 101},
};

enum {
   CTR_TEXTURE_KBD_KEY_KEY,
   CTR_TEXTURE_KBD_KEY_FKEY,
   CTR_TEXTURE_KBD_KEY_ALT,
   CTR_TEXTURE_KBD_KEY_CTRL,
   CTR_TEXTURE_KBD_KEY_ENTER,
   CTR_TEXTURE_KBD_KEY_SHIFT,
   CTR_TEXTURE_KBD_KEY_CAPS,
   CTR_TEXTURE_KBD_KEY_SPACE,
   CTR_TEXTURE_KBD_KEY_TAB
} ctr_bottom_kbd_gfx_enum;

#endif /* CTR_BOTTOM_GFX_H_ */
