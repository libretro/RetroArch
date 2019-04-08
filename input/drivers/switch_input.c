#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_LIBNX
#include <switch.h>

#define MULTITOUCH_LIMIT 4 /* supports up to this many fingers at once */
#define TOUCH_AXIS_MAX 0x7fff /* abstraction of pointer coords */
#define SWITCH_NUM_SCANCODES 114 /* size of rarch_key_map_switch */
#define SWITCH_MAX_SCANCODE 0xfb /* see https://switchbrew.github.io/libnx/hid_8h.html */
#define MOUSE_MAX_X 1920
#define MOUSE_MAX_Y 1080
#endif

#include "../input_driver.h"
#include "../input_keymaps.h"

#define MAX_PADS 10

/* TODO/FIXME -
 * fix game focus toggle */

typedef struct switch_input
{
   const input_device_driver_t *joypad;
   bool blocked;

#ifdef HAVE_LIBNX
   uint32_t touch_scale_x;
   uint32_t touch_scale_y;

   uint32_t touch_half_resolution_x;
   uint32_t touch_half_resolution_y;

   bool touch_state[MULTITOUCH_LIMIT];
   uint32_t touch_x[MULTITOUCH_LIMIT];
   uint32_t touch_y[MULTITOUCH_LIMIT];
   uint32_t touch_previous_x[MULTITOUCH_LIMIT];
   uint32_t touch_previous_y[MULTITOUCH_LIMIT];
   bool keyboard_state[SWITCH_MAX_SCANCODE + 1];

   int32_t mouse_x;
   int32_t mouse_y;
   int32_t mouse_x_delta;
   int32_t mouse_y_delta;
   int32_t mouse_wheel;
   bool mouse_button_left;
   bool mouse_button_right;
   bool mouse_button_middle;
#endif
} switch_input_t;

static void switch_input_poll(void *data)
{
   switch_input_t *sw = (switch_input_t*) data;

   if (sw->joypad)
      sw->joypad->poll();

#ifdef HAVE_LIBNX
   uint32_t touch_count = hidTouchCount();
   unsigned int i = 0;
   int keySym = 0;
   unsigned keyCode = 0;
   uint16_t mod = 0;
   MousePosition mouse_pos;
   bool previous_touch_state[MULTITOUCH_LIMIT];

   for (i = 0; i < MULTITOUCH_LIMIT; i++)
   {
      previous_touch_state[i] = sw->touch_state[i];
      sw->touch_state[i] = touch_count > i;

      if (sw->touch_state[i])
      {
         touchPosition touch_position;
         hidTouchRead(&touch_position, i);

         sw->touch_previous_x[i] = sw->touch_x[i];
         sw->touch_previous_y[i] = sw->touch_y[i];
         sw->touch_x[i] = touch_position.px;
         sw->touch_y[i] = touch_position.py;
         // prevent jumps in mouse pointer when putting down finger
         if (!previous_touch_state[i]) 
         {
            sw->touch_previous_x[i] = sw->touch_x[i];
            sw->touch_previous_y[i] = sw->touch_y[i];
         }
      }
   }

   mod = 0;
   if (hidKeyboardHeld(KBD_LEFTALT) || hidKeyboardHeld(KBD_RIGHTALT))
      mod |= RETROKMOD_ALT;
   if (hidKeyboardHeld(KBD_LEFTCTRL) || hidKeyboardHeld(KBD_RIGHTCTRL))
      mod |= RETROKMOD_CTRL;
   if (hidKeyboardHeld(KBD_LEFTSHIFT) || hidKeyboardHeld(KBD_RIGHTSHIFT))
      mod |= RETROKMOD_SHIFT;

   for (i = 0; i < SWITCH_NUM_SCANCODES; i++)
   {
      keySym = rarch_key_map_switch[i].sym;
      keyCode = input_keymaps_translate_keysym_to_rk(keySym);

      if (hidKeyboardHeld(keySym) && !(sw->keyboard_state[keySym]))
      {
         sw->keyboard_state[keySym] = true;
         input_keyboard_event(true, keyCode, 0, mod, RETRO_DEVICE_KEYBOARD);
      }
      else if (!hidKeyboardHeld(keySym) && sw->keyboard_state[keySym])
      {
         sw->keyboard_state[keySym] = false;
         input_keyboard_event(false, keyCode, 0, mod, RETRO_DEVICE_KEYBOARD);
      }
   }

   if (hidMouseButtonsHeld() & MOUSE_LEFT)
   {
      sw->mouse_button_left = true;
   }
   else
   {
      sw->mouse_button_left = false;
   }

   if (hidMouseButtonsHeld() & MOUSE_RIGHT)
   {
      sw->mouse_button_right = true;
   }
   else
   {
      sw->mouse_button_right = false;
   }

   if (hidMouseButtonsHeld() & MOUSE_MIDDLE)
   {
      sw->mouse_button_middle = true;
   } 
   else
   {
      sw->mouse_button_middle = false;
   }

   hidMouseRead(&mouse_pos);

   sw->mouse_x_delta = mouse_pos.velocityX;
   sw->mouse_y_delta = mouse_pos.velocityY;

   // allow finger to move mouse pointer like on a laptop touchpad
   for (i = 0; i < MULTITOUCH_LIMIT; i++)
   {
      if (sw->touch_state[i])
      {
         sw->mouse_x_delta += sw->touch_x[i] - sw->touch_previous_x[i];
         sw->mouse_y_delta += sw->touch_y[i] - sw->touch_previous_y[i];
      }
   }

   sw->mouse_x += sw->mouse_x_delta;
   sw->mouse_y += sw->mouse_y_delta;
   if (sw->mouse_x < 0)
   {
      sw->mouse_x = 0;
   }
   else if (sw->mouse_x > MOUSE_MAX_X)
   {
      sw->mouse_x = MOUSE_MAX_X;
   }

   if (sw->mouse_y < 0) 
   {
      sw->mouse_y = 0;
   }
   else if (sw->mouse_y > MOUSE_MAX_Y)
   {
      sw->mouse_y = MOUSE_MAX_Y;
   }

   sw->mouse_wheel = mouse_pos.scrollVelocityY;
#endif
}

#ifdef HAVE_LIBNX
void calc_touch_scaling(switch_input_t *sw, uint32_t x, uint32_t y, uint32_t axis_max)
{
   sw->touch_half_resolution_x = x/2;
   sw->touch_half_resolution_y = y/2;

   sw->touch_scale_x = axis_max / sw->touch_half_resolution_x;
   sw->touch_scale_y = axis_max / sw->touch_half_resolution_y;
}

static int16_t switch_pointer_device_state(switch_input_t *sw,
      unsigned id, unsigned idx)
{
   if (idx >= MULTITOUCH_LIMIT)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return sw->touch_state[idx];
      case RETRO_DEVICE_ID_POINTER_X:
         return ((sw->touch_x[idx] - sw->touch_half_resolution_x) * sw->touch_scale_x);
      case RETRO_DEVICE_ID_POINTER_Y:
         return ((sw->touch_y[idx] - sw->touch_half_resolution_y) * sw->touch_scale_y);
   }

   return 0;
}

static int16_t switch_input_mouse_state(switch_input_t *sw, unsigned id, bool screen)
{
   int val = 0;
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         val = sw->mouse_button_left;
         break;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         val = sw->mouse_button_right;
         break;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         val = sw->mouse_button_middle;
         break;
      case RETRO_DEVICE_ID_MOUSE_X:
         if (screen)
         {
            val = sw->mouse_x;
         }
         else
         {
            val = sw->mouse_x_delta;
            sw->mouse_x_delta = 0; /* flush delta after it has been read */
         }
         break;
      case RETRO_DEVICE_ID_MOUSE_Y:
         if (screen)
         {
            val = sw->mouse_y;
         }
         else
         {
            val = sw->mouse_y_delta;
            sw->mouse_y_delta = 0; /* flush delta after it has been read */
         }
         break;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         if (sw->mouse_wheel > 0)
         {
            val = sw->mouse_wheel;
            sw->mouse_wheel = 0;
         }
         break;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         if (sw->mouse_wheel < 0)
         {
            val = sw->mouse_wheel;
            sw->mouse_wheel = 0;
         }
         break;
   }

   return val;
}
#endif

static int16_t switch_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   int16_t ret        = 0;
   switch_input_t *sw = (switch_input_t*) data;

   if (port > MAX_PADS-1)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               /* Auto-binds are per joypad, not per user. */
               const uint16_t joykey  = (binds[port][i].joykey != NO_BTN)
                  ? binds[port][i].joykey : joypad_info.auto_binds[i].joykey;
               const uint32_t joyaxis = (binds[port][i].joyaxis != AXIS_NONE)
                  ? binds[port][i].joyaxis : joypad_info.auto_binds[i].joyaxis;

               if (joykey != NO_BTN && sw->joypad->button(joypad_info.joy_idx, joykey))
                  ret |= (1 << i);
               else if (((float)abs(sw->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
                  ret |= (1 << 1);
            }
         }
         else
         {
            /* Auto-binds are per joypad, not per user. */
            const uint16_t joykey  = (binds[port][id].joykey != NO_BTN)
               ? binds[port][id].joykey : joypad_info.auto_binds[id].joykey;
            const uint32_t joyaxis = (binds[port][id].joyaxis != AXIS_NONE)
               ? binds[port][id].joyaxis : joypad_info.auto_binds[id].joyaxis;
            if (joykey != NO_BTN && sw->joypad->button(joypad_info.joy_idx, joykey))
               ret = 1;
            else if (((float)abs(sw->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               ret = 1;
         }
         return ret;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return input_joypad_analog(sw->joypad,
                  joypad_info, port, idx, id, binds[port]);
         break;
#ifdef HAVE_LIBNX
      case RETRO_DEVICE_KEYBOARD:
         return ((id < RETROK_LAST) && sw->keyboard_state[rarch_keysym_lut[(enum retro_key)id]]);
         break;
      case RETRO_DEVICE_MOUSE:
         return switch_input_mouse_state(sw, id, false);
         break;
      case RARCH_DEVICE_MOUSE_SCREEN:
         return switch_input_mouse_state(sw, id, true);
         break;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         return switch_pointer_device_state(sw, id, idx);
#endif
   }

   return 0;
}

static void switch_input_free_input(void *data)
{
   switch_input_t *sw = (switch_input_t*) data;

   if (sw && sw->joypad)
      sw->joypad->destroy();

   free(sw);

#ifdef HAVE_LIBNX
   hidExit();
#endif
}

static void* switch_input_init(const char *joypad_driver)
{
   switch_input_t *sw = (switch_input_t*) calloc(1, sizeof(*sw));
   if (!sw)
      return NULL;

#ifdef HAVE_LIBNX
   hidInitialize();
#endif

   sw->joypad = input_joypad_init_driver(joypad_driver, sw);

#ifdef HAVE_LIBNX
   /*
      Here we assume that the touch screen is always 1280x720
      Call me back when a Nintendo Switch XL is out
   */

   calc_touch_scaling(sw, 1280, 720, TOUCH_AXIS_MAX);

   input_keymaps_init_keyboard_lut(rarch_key_map_switch);
   unsigned int i;
   for (i = 0; i <= SWITCH_MAX_SCANCODE; i++) {
      sw->keyboard_state[i] = false;
   }
   sw->mouse_x = 0;
   sw->mouse_y = 0;
#endif

   return sw;
}

static uint64_t switch_input_get_capabilities(void *data)
{
   (void) data;

   uint64_t caps =  (1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG);

#ifdef HAVE_LIBNX
   caps |= (1 << RETRO_DEVICE_POINTER) | (1 << RETRO_DEVICE_KEYBOARD) | (1 << RETRO_DEVICE_MOUSE);
#endif

   return caps;
}

static const input_device_driver_t *switch_input_get_joypad_driver(void *data)
{
   switch_input_t *sw = (switch_input_t*) data;
   if (sw)
      return sw->joypad;
   return NULL;
}

static void switch_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool switch_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
#ifdef HAVE_LIBNX
   switch_input_t *sw = (switch_input_t*) data;
   if (!sw)
      return false;
   return input_joypad_set_rumble(sw->joypad, port, effect, strength);
#else
   return false;
#endif
}

static bool switch_input_keyboard_mapping_is_blocked(void *data)
{
   switch_input_t *sw = (switch_input_t*) data;
   if (!sw)
      return false;
   return sw->blocked;
}

static void switch_input_keyboard_mapping_set_block(void *data, bool value)
{
   switch_input_t *sw = (switch_input_t*) data;
   if (!sw)
      return;
   sw->blocked = value;
}

input_driver_t input_switch = {
	switch_input_init,
	switch_input_poll,
	switch_input_state,
	switch_input_free_input,
	NULL,
	NULL,
	switch_input_get_capabilities,
	"switch",
	switch_input_grab_mouse,
	NULL,
	switch_input_set_rumble,
	switch_input_get_joypad_driver,
	NULL,
	switch_input_keyboard_mapping_is_blocked,
	switch_input_keyboard_mapping_set_block,
};
