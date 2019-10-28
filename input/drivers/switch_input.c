#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../config.def.h"

#include "../../retroarch.h"

#ifdef HAVE_LIBNX
#include <switch.h>

#define MULTITOUCH_LIMIT 4 /* supports up to this many fingers at once */
#define TOUCH_AXIS_MAX 0x7fff /* abstraction of pointer coords */
#define SWITCH_NUM_SCANCODES 114 /* size of rarch_key_map_switch */
#define SWITCH_MAX_SCANCODE 0xfb /* see https://switchbrew.github.io/libnx/hid_8h.html */
#define MOUSE_MAX_X 1920
#define MOUSE_MAX_Y 1080

/* beginning of touch mouse defines and types */
#define TOUCH_MAX_X 1280
#define TOUCH_MAX_Y 720
#define TOUCH_MOUSE_BUTTON_LEFT 0
#define TOUCH_MOUSE_BUTTON_RIGHT 1
#define NO_TOUCH -1 /* finger id setting if finger is not touching the screen */

enum
{
   MAX_NUM_FINGERS = 3, /* number of fingers to track per panel for touch mouse */
   MAX_TAP_TIME = 250, /* taps longer than this will not result in mouse click events */
   MAX_TAP_MOTION_DISTANCE = 10, /* max distance finger motion in Vita screen pixels to be considered a tap */
   SIMULATED_CLICK_DURATION = 50, /* time in ms how long simulated mouse clicks should be */
}; /* track three fingers per panel */

typedef struct
{
   int id; /* -1: no touch */
   int time_last_down;
   float last_down_x;
   float last_down_y;
} Touch;

typedef enum DraggingType
{
   DRAG_NONE = 0,
   DRAG_TWO_FINGER,
   DRAG_THREE_FINGER,
} DraggingType;

typedef enum TouchEventType
{
   FINGERDOWN,
   FINGERUP,
   FINGERMOTION,
} TouchEventType;

typedef struct
{
   TouchEventType type;
   uint64_t timestamp;
   int touchId;
   int fingerId;
   float x;
   float y;
   float dx;
   float dy;
} FingerType;

typedef struct
{
   TouchEventType type;
   FingerType tfinger;
} TouchEvent;
/* end of touch mouse defines and types */
#endif

#include "../input_keymaps.h"

/* TODO/FIXME -
 * fix game focus toggle */

typedef struct switch_input
{
   const input_device_driver_t *joypad;

#ifdef HAVE_LIBNX
   /* pointer */
   bool touch_state[MULTITOUCH_LIMIT];
   bool previous_touch_state[MULTITOUCH_LIMIT];
   int16_t touch_x_viewport[MULTITOUCH_LIMIT]; /* used for POINTER device */
   int16_t touch_y_viewport[MULTITOUCH_LIMIT]; /* used for POINTER device */
   int16_t touch_x_screen[MULTITOUCH_LIMIT]; /* used for POINTER_SCREEN device */
   int16_t touch_y_screen[MULTITOUCH_LIMIT]; /* used for POINTER_SCREEN device */
   uint32_t touch_x[MULTITOUCH_LIMIT]; /* used for touch mouse */
   uint32_t touch_y[MULTITOUCH_LIMIT]; /* used for touch mouse */
   uint32_t touch_previous_x[MULTITOUCH_LIMIT];
   uint32_t touch_previous_y[MULTITOUCH_LIMIT];
   bool keyboard_state[SWITCH_MAX_SCANCODE + 1];

   /* physical mouse */
   int32_t mouse_x;
   int32_t mouse_y;
   int32_t mouse_x_delta;
   int32_t mouse_y_delta;
   int32_t mouse_wheel;
   bool mouse_button_left;
   bool mouse_button_right;
   bool mouse_button_middle;
   uint64_t mouse_previous_report;

   /* touch mouse */
   bool touch_mouse_indirect;
   float touch_mouse_speed_factor;
   int hires_dx; /* sub-pixel touch mouse precision */
   int hires_dy; /* sub-pixel touch mouse precision */
   Touch finger[MAX_NUM_FINGERS]; /* keep track of finger status for touch mouse */
   DraggingType multi_finger_dragging; /* keep track whether we are currently drag-and-dropping */
   int32_t simulated_click_start_time[2]; /* initiation time of last simulated left or right click (zero if no click) */

   /* sensor handles */
   uint32_t sixaxis_handles[DEFAULT_MAX_PADS][4];
   unsigned sixaxis_handles_count[DEFAULT_MAX_PADS];
#endif
} switch_input_t;

#ifdef HAVE_LIBNX
/* beginning of touch mouse function declarations */
static void handle_touch_mouse(switch_input_t *sw);
static void normalize_touch_mouse_xy(float *normalized_x, float *normalized_y, int reported_x, int reported_y);
static void process_touch_mouse_event(switch_input_t *sw, TouchEvent *event);
static void process_touch_mouse_finger_down(switch_input_t * sw, TouchEvent *event);
static void process_touch_mouse_finger_up(switch_input_t *sw, TouchEvent *event);
static void process_touch_mouse_finger_motion(switch_input_t *sw, TouchEvent *event);
static void normalized_to_screen_xy(int *screenX, int *screenY, float x, float y);
static void finish_simulated_mouse_clicks(switch_input_t *sw, uint64_t currentTime);
/* end of touch mouse function declarations */
#endif

static void switch_input_poll(void *data)
{
   switch_input_t *sw = (switch_input_t*) data;
#ifdef HAVE_LIBNX
   uint32_t touch_count;
   unsigned int i = 0;
   int keySym = 0;
   unsigned keyCode = 0;
   uint16_t mod = 0;
   MousePosition mouse_pos;
   uint64_t mouse_current_report = 0;
#endif

   if (sw->joypad)
      sw->joypad->poll();

#ifdef HAVE_LIBNX
   touch_count = hidTouchCount();
   for (i = 0; i < MULTITOUCH_LIMIT; i++)
   {
      sw->previous_touch_state[i] = sw->touch_state[i];
      sw->touch_state[i] = touch_count > i;

      if (sw->touch_state[i])
      {
         struct video_viewport vp;
         touchPosition touch_position;
         hidTouchRead(&touch_position, i);

         sw->touch_previous_x[i] = sw->touch_x[i];
         sw->touch_previous_y[i] = sw->touch_y[i];
         sw->touch_x[i] = touch_position.px;
         sw->touch_y[i] = touch_position.py;

         /* convert from event coordinates to core and screen coordinates */
         vp.x                        = 0;
         vp.y                        = 0;
         vp.width                    = 0;
         vp.height                   = 0;
         vp.full_width               = 0;
         vp.full_height              = 0;

         video_driver_translate_coord_viewport_wrap(
            &vp,
            touch_position.px,
            touch_position.py,
            &sw->touch_x_viewport[i],
            &sw->touch_y_viewport[i],
            &sw->touch_x_screen[i],
            &sw->touch_y_screen[i]);
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

   /* update physical mouse buttons only when they change
    * this allows the physical mouse and touch mouse to coexist */
   mouse_current_report = hidMouseButtonsHeld();
   if ((mouse_current_report & MOUSE_LEFT) != (sw->mouse_previous_report & MOUSE_LEFT))
   {
      if (mouse_current_report & MOUSE_LEFT)
         sw->mouse_button_left = true;
      else
         sw->mouse_button_left = false;
   }

   if ((mouse_current_report & MOUSE_RIGHT) != (sw->mouse_previous_report & MOUSE_RIGHT))
   {
      if (mouse_current_report & MOUSE_RIGHT)
         sw->mouse_button_right = true;
      else
         sw->mouse_button_right = false;
   }

   if ((mouse_current_report & MOUSE_MIDDLE) != (sw->mouse_previous_report & MOUSE_MIDDLE))
   {
      if (mouse_current_report & MOUSE_MIDDLE)
         sw->mouse_button_middle = true;
      else
         sw->mouse_button_middle = false;
   }
   sw->mouse_previous_report = mouse_current_report;

   /* physical mouse position */
   hidMouseRead(&mouse_pos);

   sw->mouse_x_delta = mouse_pos.velocityX;
   sw->mouse_y_delta = mouse_pos.velocityY;

   sw->mouse_x += sw->mouse_x_delta;
   sw->mouse_y += sw->mouse_y_delta;

   /* touch mouse events
    * handle_touch_mouse will update sw->mouse_* variables
    * depending on touch input gestures
    * see first comment in process_touch_mouse_event() for a list of
    * supported touch gestures */
   handle_touch_mouse(sw);

   if (sw->mouse_x < 0)
      sw->mouse_x = 0;
   else if (sw->mouse_x > MOUSE_MAX_X)
      sw->mouse_x = MOUSE_MAX_X;

   if (sw->mouse_y < 0) 
      sw->mouse_y = 0;
   else if (sw->mouse_y > MOUSE_MAX_Y)
      sw->mouse_y = MOUSE_MAX_Y;

   sw->mouse_wheel = mouse_pos.scrollVelocityY;
#endif
}

#ifdef HAVE_LIBNX
static int16_t switch_pointer_screen_device_state(switch_input_t *sw,
      unsigned id, unsigned idx)
{
   if (idx >= MULTITOUCH_LIMIT)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return sw->touch_state[idx];
      case RETRO_DEVICE_ID_POINTER_X:
         return sw->touch_x_screen[idx];
      case RETRO_DEVICE_ID_POINTER_Y:
         return sw->touch_y_screen[idx];
   }

   return 0;
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
         return sw->touch_x_viewport[idx];
      case RETRO_DEVICE_ID_POINTER_Y:
         return sw->touch_y_viewport[idx];
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
            val = sw->mouse_x;
         else
         {
            val = sw->mouse_x_delta;
            sw->mouse_x_delta = 0; /* flush delta after it has been read */
         }
         break;
      case RETRO_DEVICE_ID_MOUSE_Y:
         if (screen)
            val = sw->mouse_y;
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
   switch_input_t *sw = (switch_input_t*) data;

   if (port > DEFAULT_MAX_PADS - 1)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               /* Auto-binds are per joypad, not per user. */
               const uint64_t joykey  = (binds[port][i].joykey != NO_BTN)
                  ? binds[port][i].joykey : joypad_info.auto_binds[i].joykey;
               const uint32_t joyaxis = (binds[port][i].joyaxis != AXIS_NONE)
                  ? binds[port][i].joyaxis : joypad_info.auto_binds[i].joyaxis;

               if ((uint16_t)joykey != NO_BTN && sw->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               {
                  ret |= (1 << i);
                  continue;
               }
               if (((float)abs(sw->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               {
                  ret |= (1 << i);
                  continue;
               }
            }

            return ret;
         }
         else
         {
            /* Auto-binds are per joypad, not per user. */
            const uint64_t joykey  = (binds[port][id].joykey != NO_BTN)
               ? binds[port][id].joykey : joypad_info.auto_binds[id].joykey;
            const uint32_t joyaxis = (binds[port][id].joyaxis != AXIS_NONE)
               ? binds[port][id].joyaxis : joypad_info.auto_binds[id].joyaxis;
            if ((uint16_t)joykey != NO_BTN && sw->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               return true;
            if (((float)abs(sw->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               return true;
         }
         break;
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
         return switch_pointer_device_state(sw, id, idx);
         break;
      case RARCH_DEVICE_POINTER_SCREEN:
         return switch_pointer_screen_device_state(sw, id, idx);
         break;
#endif
   }

   return 0;
}

#ifdef HAVE_LIBNX
void handle_touch_mouse(switch_input_t *sw)
{
   int finger_id = 0;
   uint64_t current_time = svcGetSystemTick() * 1000 / 19200000;
   unsigned int i;
   finish_simulated_mouse_clicks(sw, current_time);

   for (i = 0; i < MULTITOUCH_LIMIT; i++)
   {
      if (sw->touch_state[i])
      {
         float x = 0;
         float y = 0;
         normalize_touch_mouse_xy(&x, &y, sw->touch_x[i], sw->touch_y[i]);
         finger_id = i;

         /* Send an initial touch if finger hasn't been down */
         if (!sw->previous_touch_state[i])
         {
            TouchEvent ev;
            ev.type = FINGERDOWN;
            ev.tfinger.timestamp = current_time;
            ev.tfinger.fingerId = finger_id;
            ev.tfinger.x = x;
            ev.tfinger.y = y;
            process_touch_mouse_event(sw, &ev);
         }
         else
         {
            /* If finger moved, send motion event instead */
            if (sw->touch_x[i] != sw->touch_previous_x[i] ||
                sw->touch_y[i] != sw->touch_previous_y[i])
            {
               float oldx = 0;
               float oldy = 0;
               TouchEvent ev;
               normalize_touch_mouse_xy(&oldx, &oldy, sw->touch_previous_x[i], sw->touch_previous_y[i]);
               ev.type = FINGERMOTION;
               ev.tfinger.timestamp = current_time;
               ev.tfinger.fingerId = finger_id;
               ev.tfinger.x = x;
               ev.tfinger.y = y;
               ev.tfinger.dx = x - oldx;
               ev.tfinger.dy = y - oldy;
               process_touch_mouse_event(sw, &ev);
            }
         }
      }

      /* some fingers might have been let go */
      if (sw->previous_touch_state[i] == true && sw->touch_state[i] == false)
      {
         float x = 0;
         float y = 0;
         TouchEvent ev;
         normalize_touch_mouse_xy(&x, &y, sw->touch_previous_x[i], sw->touch_previous_y[i]);
         finger_id = i;
         /* finger released from screen */
         ev.type = FINGERUP;
         ev.tfinger.timestamp = current_time;
         ev.tfinger.fingerId = finger_id;
         ev.tfinger.x = x;
         ev.tfinger.y = y;
         process_touch_mouse_event(sw, &ev);
      }
   }
}

void normalize_touch_mouse_xy(float *normalized_x, float *normalized_y, int reported_x, int reported_y)
{
   float x = 0;
   float y = 0;
   x = (float) reported_x / TOUCH_MAX_X;
   y = (float) reported_y / TOUCH_MAX_Y;

   if (x < 0.0)
      x = 0.0;
   else if (x > 1.0)
      x = 1.0;

   if (y < 0.0)
      y = 0.0;
   else if (y > 1.0)
      y = 1.0;
   *normalized_x = x;
   *normalized_y = y;
}

void process_touch_mouse_event(switch_input_t *sw, TouchEvent *event)
{
   /* supported touch gestures:
    * pointer motion = single finger drag
    * left mouse click = single finger short tap
    * right mouse click = second finger short tap while first finger is still down
    * left click drag and drop = dual finger drag
    * right click drag and drop = triple finger drag */
   if (event->type == FINGERDOWN || event->type == FINGERUP || event->type == FINGERMOTION)
   {
      switch (event->type)
      {
         case FINGERDOWN:
            process_touch_mouse_finger_down(sw, event);
            break;
         case FINGERUP:
            process_touch_mouse_finger_up(sw, event);
            break;
         case FINGERMOTION:
            process_touch_mouse_finger_motion(sw, event);
            break;
      }
   }
}

void process_touch_mouse_finger_down(switch_input_t *sw, TouchEvent *event)
{
   /* id (for multitouch) */
   int id = event->tfinger.fingerId;
   unsigned int i;

   /* make sure each finger is not reported down multiple times */
   for (i = 0; i < MAX_NUM_FINGERS; i++)
   {
      if (sw->finger[i].id == id)
         sw->finger[i].id = NO_TOUCH;
   }

   /* we need the timestamps to decide later if the user performed a short tap (click)
    * or a long tap (drag)
    * we also need the last coordinates for each finger to keep track of dragging */
   for (i = 0; i < MAX_NUM_FINGERS; i++)
   {
      if (sw->finger[i].id == NO_TOUCH)
      {
         sw->finger[i].id = id;
         sw->finger[i].time_last_down = event->tfinger.timestamp;
         sw->finger[i].last_down_x = event->tfinger.x;
         sw->finger[i].last_down_y = event->tfinger.y;
         break;
      }
   }
}

void process_touch_mouse_finger_up(switch_input_t *sw, TouchEvent *event)
{
   /* id (for multitouch) */
   int id = event->tfinger.fingerId;
   int num_fingers_down;
   unsigned int i;

   /* find out how many fingers were down before this event */
   num_fingers_down = 0;
   for (i = 0; i < MAX_NUM_FINGERS; i++)
   {
      if (sw->finger[i].id >= 0)
         num_fingers_down++;
   }

   for (i = 0; i < MAX_NUM_FINGERS; i++)
   {
      if (sw->finger[i].id != id)
         continue;

      sw->finger[i].id = NO_TOUCH;
      if (!sw->multi_finger_dragging)
      {
         float xrel;
         float yrel;
         float max_r_squared;
         int simulated_button;

         if ((event->tfinger.timestamp - sw->finger[i].time_last_down) > MAX_TAP_TIME)
            continue;

         /* short (<MAX_TAP_TIME ms) tap is interpreted as right/left mouse click depending on # fingers already down
          * but only if the finger hasn't moved since it was pressed down by more than MAX_TAP_MOTION_DISTANCE pixels */
         xrel = ((event->tfinger.x * TOUCH_MAX_X) - (sw->finger[i].last_down_x * TOUCH_MAX_X));
         yrel = ((event->tfinger.y * TOUCH_MAX_Y) - (sw->finger[i].last_down_y * TOUCH_MAX_Y));
         max_r_squared = (float) (MAX_TAP_MOTION_DISTANCE * MAX_TAP_MOTION_DISTANCE);
         if ((xrel * xrel + yrel * yrel) >= max_r_squared)
            continue;
         if (num_fingers_down != 2 && num_fingers_down != 1)
            continue;
         simulated_button = 0;
         if (num_fingers_down == 2)
         {
            simulated_button = TOUCH_MOUSE_BUTTON_RIGHT;
            /* need to raise the button later */
            sw->simulated_click_start_time[TOUCH_MOUSE_BUTTON_RIGHT] = event->tfinger.timestamp;
         }
         else if (num_fingers_down == 1)
         {
            if (!sw->touch_mouse_indirect)
            {
               int x;
               int y;
               normalized_to_screen_xy(&x, &y, event->tfinger.x, event->tfinger.y);
               sw->mouse_x_delta = x - sw->mouse_x;
               sw->mouse_y_delta = y - sw->mouse_y;
               sw->mouse_x = x;
               sw->mouse_y = y;
            }
            simulated_button = TOUCH_MOUSE_BUTTON_LEFT;
            /* need to raise the button later */
            sw->simulated_click_start_time[TOUCH_MOUSE_BUTTON_LEFT] = event->tfinger.timestamp;
         }
         /* simulate mouse button down */
         if (simulated_button == TOUCH_MOUSE_BUTTON_LEFT)
            sw->mouse_button_left = true;
         else if (simulated_button == TOUCH_MOUSE_BUTTON_RIGHT)
            sw->mouse_button_right = true;
      }
      else if (num_fingers_down == 1)
      {
         /* when dragging, and the last finger is lifted, the drag is over
          * so simulate mouse button up */
         if (sw->multi_finger_dragging == DRAG_THREE_FINGER)
            sw->mouse_button_right = false;
         else
            sw->mouse_button_left = false;
         sw->multi_finger_dragging = DRAG_NONE;
      }
   }
}

void process_touch_mouse_finger_motion(switch_input_t *sw, TouchEvent *event)
{
   /* id (for multitouch) */
   int id = event->tfinger.fingerId;
   unsigned int i;
   unsigned int j;
   int num_fingers_down;
   bool update_pointer;

   /* find out how many fingers were down before this event */
   num_fingers_down = 0;
   for (i = 0; i < MAX_NUM_FINGERS; i++)
   {
      if (sw->finger[i].id >= 0)
         num_fingers_down++;
   }

   if (num_fingers_down == 0)
      return;

   /* If we are starting a multi-finger drag, start holding down the mouse button */
   if (num_fingers_down >= 2 && !sw->multi_finger_dragging)
   {
      /* only start a multi-finger drag if at least two fingers have been down long enough */
      int num_fingers_down_long = 0;
      for (i = 0; i < MAX_NUM_FINGERS; i++)
      {
         if (sw->finger[i].id == NO_TOUCH)
            continue;
         if (event->tfinger.timestamp - sw->finger[i].time_last_down > MAX_TAP_TIME)
            num_fingers_down_long++;
      }
      if (num_fingers_down_long >= 2)
      {
         int simulated_button = 0;
         if (num_fingers_down_long == 2)
         {
            simulated_button = TOUCH_MOUSE_BUTTON_LEFT;
            sw->multi_finger_dragging = DRAG_TWO_FINGER;
         }
         else
         {
            simulated_button = TOUCH_MOUSE_BUTTON_RIGHT;
            sw->multi_finger_dragging = DRAG_THREE_FINGER;
         }

         if (simulated_button == TOUCH_MOUSE_BUTTON_LEFT)
            sw->mouse_button_left = true;
         else if (simulated_button == TOUCH_MOUSE_BUTTON_RIGHT)
            sw->mouse_button_right = true;
      }
   }

   /* check if this is the "oldest" finger down (or the only finger down)
    * otherwise it will not affect mouse motion */
   update_pointer = true;
   if (num_fingers_down > 1)
   {
      for (i = 0; i < MAX_NUM_FINGERS; i++)
      {
         if (sw->finger[i].id != id)
            continue;
         for (j = 0; j < MAX_NUM_FINGERS; j++)
         {
            if (sw->finger[j].id == NO_TOUCH || (j == i))
               continue;
            if (sw->finger[j].time_last_down < sw->finger[i].time_last_down)
               update_pointer = false;
         }
      }
   }
   if (!update_pointer)
      return;

   if (!sw->touch_mouse_indirect)
   {
      int x;
      int y;
      normalized_to_screen_xy(&x, &y, event->tfinger.x, event->tfinger.y);
      sw->mouse_x_delta = x - sw->mouse_x;
      sw->mouse_y_delta = y - sw->mouse_y;
      sw->mouse_x = x;
      sw->mouse_y = y;
   }
   else
   {
      /* for relative mode, use the pointer speed setting */
      int dx = event->tfinger.dx * TOUCH_MAX_X * 256 * sw->touch_mouse_speed_factor;
      int dy = event->tfinger.dy * TOUCH_MAX_Y * 256 * sw->touch_mouse_speed_factor;
      sw->hires_dx += dx;
      sw->hires_dy += dy;
      int x_rel = sw->hires_dx / 256;
      int y_rel = sw->hires_dy / 256;
      if (x_rel || y_rel)
      {
         sw->mouse_x_delta = x_rel;
         sw->mouse_y_delta = y_rel;
         sw->mouse_x += x_rel;
         sw->mouse_y += y_rel;
      }
      sw->hires_dx %= 256;
      sw->hires_dy %= 256;
   }
}

void normalized_to_screen_xy(int *screenX, int *screenY, float x, float y)
{
   /* map to display */
   *screenX = x * TOUCH_MAX_X;
   *screenY = y * TOUCH_MAX_Y;
}

void finish_simulated_mouse_clicks(switch_input_t *sw, uint64_t currentTime)
{
   unsigned int i;
   for (i = 0; i < 2; i++)
   {
      if (sw->simulated_click_start_time[i] == 0)
         continue;

      if (currentTime - sw->simulated_click_start_time[i] < SIMULATED_CLICK_DURATION)
         continue;

      if (i == 0)
         sw->mouse_button_left = false;
      else
         sw->mouse_button_right = false;

      sw->simulated_click_start_time[i] = 0;
   }
}
#endif

static void switch_input_free_input(void *data)
{
   unsigned i,j;
   switch_input_t *sw = (switch_input_t*) data;

   if (sw)
   {
      if(sw->joypad)
         sw->joypad->destroy();

      for(i = 0; i < DEFAULT_MAX_PADS; i++)
         if(sw->sixaxis_handles_count[i] > 0)
            for(j = 0; j < sw->sixaxis_handles_count[i]; j++)
               hidStopSixAxisSensor(sw->sixaxis_handles[i][j]);

      free(sw);
   }

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

   input_keymaps_init_keyboard_lut(rarch_key_map_switch);
   unsigned int i;
   for (i = 0; i <= SWITCH_MAX_SCANCODE; i++)
      sw->keyboard_state[i] = false;

   sw->mouse_x = 0;
   sw->mouse_y = 0;
   sw->mouse_previous_report = 0;

   /* touch mouse init */
   sw->touch_mouse_indirect = true; /* direct mode is not calibrated it seems */
   sw->touch_mouse_speed_factor = 1.0;
   for (i = 0; i < MAX_NUM_FINGERS; i++)
      sw->finger[i].id = NO_TOUCH;

   sw->multi_finger_dragging = DRAG_NONE;

   for (i = 0; i < 2; i++)
      sw->simulated_click_start_time[i] = 0;

   for(i = 0; i < DEFAULT_MAX_PADS; i++)
      sw->sixaxis_handles_count[i] = 0;
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

static bool switch_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate)
{
#ifdef HAVE_LIBNX
   unsigned i, handles_count;
   bool available;
   switch_input_t *sw = (switch_input_t*) data;

   if(!sw)
      return false;

   switch(action)
   {
      case RETRO_SENSOR_ILLUMINANCE_ENABLE:
         available = false;
         appletIsIlluminanceAvailable(&available);
         return available;

      case RETRO_SENSOR_ILLUMINANCE_DISABLE:
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
         return true;

      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
      case RETRO_SENSOR_GYROSCOPE_ENABLE:
         if(port < DEFAULT_MAX_PADS && sw->sixaxis_handles_count[port] == 0)
         {
            hidGetSixAxisSensorHandles(&sw->sixaxis_handles[port][0], 2, port, TYPE_JOYCON_PAIR);

            hidGetSixAxisSensorHandles(&sw->sixaxis_handles[port][2], 1, port, TYPE_PROCONTROLLER);

            if(port == 0)
            {
               hidGetSixAxisSensorHandles(&sw->sixaxis_handles[port][3], 1, CONTROLLER_HANDHELD, TYPE_HANDHELD);
               handles_count = 4;
            }
            else
            {
               handles_count = 3;
            }

            for(i = 0; i < handles_count; i++) {
               hidStartSixAxisSensor(sw->sixaxis_handles[port][i]);
            }

            sw->sixaxis_handles_count[port] = handles_count;
         }
         return true;
   }
#endif

   return false;
}

static float switch_input_get_sensor_input(void *data,
      unsigned port, unsigned id)
{
#ifdef HAVE_LIBNX
   float f;
   SixAxisSensorValues sixaxis;

   if(id >= RETRO_SENSOR_ACCELEROMETER_X && id <= RETRO_SENSOR_GYROSCOPE_Z)
   {
      hidSixAxisSensorValuesRead(&sixaxis, port == 0 ? CONTROLLER_P1_AUTO : port, 1);

      switch(id)
      {
         case RETRO_SENSOR_ACCELEROMETER_X:
            return sixaxis.accelerometer.x;
         case RETRO_SENSOR_ACCELEROMETER_Y:
            return sixaxis.accelerometer.y;
         case RETRO_SENSOR_ACCELEROMETER_Z:
            return sixaxis.accelerometer.z;
         case RETRO_SENSOR_GYROSCOPE_X:
            return sixaxis.gyroscope.x;
         case RETRO_SENSOR_GYROSCOPE_Y:
            return sixaxis.gyroscope.y;
         case RETRO_SENSOR_GYROSCOPE_Z:
            return sixaxis.gyroscope.z;
      }

   }

   if(id == RETRO_SENSOR_ILLUMINANCE)
   {
      appletGetCurrentIlluminance(&f);
      return f;
   }
#endif

   return 0.0f;
}

input_driver_t input_switch = {
   switch_input_init,
   switch_input_poll,
   switch_input_state,
   switch_input_free_input,
   switch_input_set_sensor_state,
   switch_input_get_sensor_input,
   switch_input_get_capabilities,
   "switch",
   switch_input_grab_mouse,
   NULL,
   switch_input_set_rumble,
   switch_input_get_joypad_driver,
   NULL,
   false
};
