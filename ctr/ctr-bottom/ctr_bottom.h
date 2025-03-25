/*
 * ctr_bottom.h
 */

#ifndef CTR_BOTTOM_H_
#define CTR_BOTTOM_H_

#include <3ds.h>
#include <string.h>

#include "../../retroarch.h"

#define CTR_STATE_DATE_SIZE            11

extern uint64_t lifecycle_state;

typedef struct
{
   enum { MODE_IDLE, MODE_MOUSE, MODE_SAVESTATE, MODE_KBD, MODE_TODO } mode;
   unsigned previous_mode;
   int32_t touch_x;
   int32_t touch_y;
   u64  idle_timestamp;
   u64  touch_timestamp;
   bool shouldCheckIdle;
   bool shouldCheckMenu;
   bool task_save;
   bool task_load;
} ctr_bottom_state_t;

extern ctr_bottom_state_t ctr_bottom_state;

typedef struct
{
   bool isInit;
   bool bottom_enabled;
   bool bottom_idle;
   bool gfx_drawn; // no use
   unsigned gfx_id;
   bool refresh_bottom_menu;
   bool reload_texture;
   const char *texture_name;
   const char *texture_path;
   bool render_state_from_png_file;
} ctr_bottom_state_gfx_t;

extern ctr_bottom_state_gfx_t ctr_bottom_state_gfx;

typedef struct
{
   enum { KBD_LOWER, KBD_UPPER, KBD_SYMBOL, KBD_NUMBER } kbd_mode;
   unsigned isPressed;
   bool isShift;
   bool isCaps;
   bool isAlt;
   bool isCtrl;
} ctr_bottom_state_kbd_t;

extern ctr_bottom_state_kbd_t ctr_bottom_state_kbd;

typedef struct
{
   int32_t mouse_x;
   int32_t mouse_y;
   int32_t mouse_x_delta;
   int32_t mouse_y_delta;
   int32_t mouse_x_rel;
   int32_t mouse_y_rel;
   int32_t mouse_x_origin;
   int32_t mouse_y_origin;
   int mouse_multiplier;
   u64  touch_timestamp;
   bool mouse_button_left;
   bool mouse_button_right;
   int32_t mouse_button_x_origin;
   int32_t mouse_button_y_origin;
   bool ShouldCheck;
} ctr_bottom_state_mouse_t;

extern ctr_bottom_state_mouse_t ctr_bottom_state_mouse;

typedef struct
{
   bool state_data_on_ram;
   bool state_data_exist;
   char state_date[CTR_STATE_DATE_SIZE];
   int state_slot;
} ctr_bottom_state_savestates_t;

extern ctr_bottom_state_savestates_t ctr_bottom_state_savestates;

extern unsigned CurrentKey;
extern int KBState;
enum keys;

void ctr_bottom_init();
void ctr_bottom_deinit();
u8 ctr_bottom_frame(touchPosition TouchScreenPos);
void ctr_set_bottom_mode(unsigned id);
extern bool ctr_refresh_bottom(bool refresh);

#endif /* CTR_BOTTOM_H_ */
