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

/* Supports up to this many fingers at once */
#define MULTITOUCH_LIMIT 4
/* Abstraction of pointer coords */
#define TOUCH_AXIS_MAX 0x7fff
/* Size of rarch_key_map_switch */
#define SWITCH_NUM_SCANCODES 110
/* See https://switchbrew.github.io/libnx/hid_8h.html */
#define SWITCH_MAX_SCANCODE 0xfb
#define MOUSE_MAX_X 1920
#define MOUSE_MAX_Y 1080

/* Beginning of touch mouse defines and types */
#define TOUCH_MAX_X 1280
#define TOUCH_MAX_Y 720
#define TOUCH_MOUSE_BUTTON_LEFT 0
#define TOUCH_MOUSE_BUTTON_RIGHT 1
/* Finger ID setting if finger is not touching the screen */
#define NO_TOUCH -1 

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
   uint32_t mouse_previous_buttons;

   /* touch mouse */
   bool touch_mouse_indirect;
   float touch_mouse_speed_factor;
   int hires_dx; /* sub-pixel touch mouse precision */
   int hires_dy; /* sub-pixel touch mouse precision */
   Touch finger[MAX_NUM_FINGERS]; /* keep track of finger status for touch mouse */
   DraggingType multi_finger_dragging; /* keep track whether we are currently drag-and-dropping */
   int32_t simulated_click_start_time[2]; /* initiation time of last simulated left or right click (zero if no click) */

   /* sensor handles */
   HidSixAxisSensorHandle sixaxis_handles[DEFAULT_MAX_PADS][4];
   unsigned sixaxis_handles_count[DEFAULT_MAX_PADS];
#else
   void *empty;
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

#ifdef HAVE_LIBNX
static void switch_input_poll(void *data)
{
   HidTouchScreenState touch_screen_state;
   HidKeyboardState kbd_state;
   HidMouseState mouse_state;
   uint32_t touch_count;
   bool key_pressed;
   unsigned int i                = 0;
   int key_sym                   = 0;
   unsigned key_code             = 0;
   uint16_t mod                  = 0;
   switch_input_t *sw            = (switch_input_t*) data;

   hidGetTouchScreenStates(&touch_screen_state, 1);
   touch_count = MIN(MULTITOUCH_LIMIT, touch_screen_state.count);
   for (i = 0; i < MULTITOUCH_LIMIT; i++)
   {
      sw->previous_touch_state[i] = sw->touch_state[i];
      sw->touch_state[i]          = touch_count > i;

      if (sw->touch_state[i])
      {
         struct video_viewport vp;

         sw->touch_previous_x[i]     = sw->touch_x[i];
         sw->touch_previous_y[i]     = sw->touch_y[i];
         sw->touch_x[i]              = touch_screen_state.touches[i].x;
         sw->touch_y[i]              = touch_screen_state.touches[i].y;

         /* convert from event coordinates to core and screen coordinates */
         vp.x                        = 0;
         vp.y                        = 0;
         vp.width                    = 0;
         vp.height                   = 0;
         vp.full_width               = 0;
         vp.full_height              = 0;

         video_driver_translate_coord_viewport_wrap(
            &vp,
            touch_screen_state.touches[i].x,
            touch_screen_state.touches[i].y,
            &sw->touch_x_viewport[i],
            &sw->touch_y_viewport[i],
            &sw->touch_x_screen[i],
            &sw->touch_y_screen[i]);
      }
   }

   mod = 0;
   hidGetKeyboardStates(&kbd_state, 1);
   if (hidKeyboardStateGetKey(&kbd_state, HidKeyboardKey_LeftAlt) || hidKeyboardStateGetKey(&kbd_state, HidKeyboardKey_RightAlt))
      mod |= RETROKMOD_ALT;
   if (hidKeyboardStateGetKey(&kbd_state, HidKeyboardKey_LeftControl) || hidKeyboardStateGetKey(&kbd_state, HidKeyboardKey_RightControl))
      mod |= RETROKMOD_CTRL;
   if (hidKeyboardStateGetKey(&kbd_state, HidKeyboardKey_LeftShift) || hidKeyboardStateGetKey(&kbd_state, HidKeyboardKey_RightShift))
      mod |= RETROKMOD_SHIFT;

   for (i = 0; i < SWITCH_NUM_SCANCODES; i++)
   {
      key_sym = rarch_key_map_switch[i].sym;
      key_code = input_keymaps_translate_keysym_to_rk(key_sym);
      key_pressed = hidKeyboardStateGetKey(&kbd_state, key_sym);
      if (key_pressed && !(sw->keyboard_state[key_sym]))
      {
         sw->keyboard_state[key_sym] = true;
         input_keyboard_event(true, key_code, 0, mod, RETRO_DEVICE_KEYBOARD);
      }
      else if (!key_pressed && sw->keyboard_state[key_sym])
      {
         sw->keyboard_state[key_sym] = false;
         input_keyboard_event(false, key_code, 0, mod, RETRO_DEVICE_KEYBOARD);
      }
   }

   /* update physical mouse buttons only when they change
    * this allows the physical mouse and touch mouse to coexist */
   hidGetMouseStates(&mouse_state, 1);
   if ((mouse_state.buttons & HidMouseButton_Left)
         != (sw->mouse_previous_buttons & HidMouseButton_Left))
   {
      if (mouse_state.buttons & HidMouseButton_Left)
         sw->mouse_button_left = true;
      else
         sw->mouse_button_left = false;
   }

   if ((mouse_state.buttons & HidMouseButton_Right)
         != (sw->mouse_previous_buttons & HidMouseButton_Right))
   {
      if (mouse_state.buttons & HidMouseButton_Right)
         sw->mouse_button_right = true;
      else
         sw->mouse_button_right = false;
   }

   if ((mouse_state.buttons & HidMouseButton_Middle)
         != (sw->mouse_previous_buttons & HidMouseButton_Middle))
   {
      if (mouse_state.buttons & HidMouseButton_Middle)
         sw->mouse_button_middle = true;
      else
         sw->mouse_button_middle = false;
   }
   sw->mouse_previous_buttons = mouse_state.buttons;

   /* physical mouse position */
   sw->mouse_x_delta = mouse_state.delta_x;
   sw->mouse_y_delta = mouse_state.delta_y;

   sw->mouse_x = mouse_state.x;
   sw->mouse_y = mouse_state.y;

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

   sw->mouse_wheel = mouse_state.wheel_delta_y;
}
#endif

static int16_t switch_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   switch_input_t *sw = (switch_input_t*) data;

   if (port > DEFAULT_MAX_PADS - 1)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
      case RETRO_DEVICE_ANALOG:
         break;
#ifdef HAVE_LIBNX
      case RETRO_DEVICE_KEYBOARD:
         return ((id < RETROK_LAST) && 
               sw->keyboard_state[rarch_keysym_lut[(enum retro_key)id]]);
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         {
            int16_t val = 0;
            bool screen = (device == RARCH_DEVICE_MOUSE_SCREEN);
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_LEFT:
                  return sw->mouse_button_left;
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
                  return sw->mouse_button_right;
               case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                  return sw->mouse_button_middle;
               case RETRO_DEVICE_ID_MOUSE_X:
                  if (screen)
                     return sw->mouse_x;

                  val = sw->mouse_x_delta;
                  sw->mouse_x_delta = 0;
                  /* flush delta after it has been read */
                  break;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  if (screen)
                     return sw->mouse_y;

                  val = sw->mouse_y_delta;
                  sw->mouse_y_delta = 0;
                  /* flush delta after it has been read */
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
         break;
      case RETRO_DEVICE_POINTER:
         if (idx < MULTITOUCH_LIMIT)
         {
            switch (id)
            {
               case RETRO_DEVICE_ID_POINTER_PRESSED:
                  return sw->touch_state[idx];
               case RETRO_DEVICE_ID_POINTER_X:
                  return sw->touch_x_viewport[idx];
               case RETRO_DEVICE_ID_POINTER_Y:
                  return sw->touch_y_viewport[idx];
            }
         }
         break;
      case RARCH_DEVICE_POINTER_SCREEN:
         if (idx < MULTITOUCH_LIMIT)
         {
            switch (id)
            {
               case RETRO_DEVICE_ID_POINTER_PRESSED:
                  return sw->touch_state[idx];
               case RETRO_DEVICE_ID_POINTER_X:
                  return sw->touch_x_screen[idx];
               case RETRO_DEVICE_ID_POINTER_Y:
                  return sw->touch_y_screen[idx];
            }
         }
         break;
#endif
   }

   return 0;
}

#ifdef HAVE_LIBNX
void handle_touch_mouse(switch_input_t *sw)
{
   unsigned int i;
   int finger_id         = 0;
   uint64_t current_time = svcGetSystemTick() * 1000 / 19200000;
   finish_simulated_mouse_clicks(sw, current_time);

   for (i = 0; i < MULTITOUCH_LIMIT; i++)
   {
      if (sw->touch_state[i])
      {
         float x   = 0;
         float y   = 0;
         normalize_touch_mouse_xy(&x, &y, sw->touch_x[i], sw->touch_y[i]);
         finger_id = i;

         /* Send an initial touch if finger hasn't been down */
         if (!sw->previous_touch_state[i])
         {
            TouchEvent ev;
            ev.type              = FINGERDOWN;
            ev.tfinger.timestamp = current_time;
            ev.tfinger.fingerId  = finger_id;
            ev.tfinger.x         = x;
            ev.tfinger.y         = y;
            process_touch_mouse_event(sw, &ev);
         }
         else
         {
            /* If finger moved, send motion event instead */
            if (sw->touch_x[i] != sw->touch_previous_x[i] ||
                sw->touch_y[i] != sw->touch_previous_y[i])
            {
               TouchEvent ev;
               float oldx = 0;
               float oldy = 0;
               normalize_touch_mouse_xy(&oldx, &oldy, sw->touch_previous_x[i], sw->touch_previous_y[i]);
               ev.type              = FINGERMOTION;
               ev.tfinger.timestamp = current_time;
               ev.tfinger.fingerId  = finger_id;
               ev.tfinger.x         = x;
               ev.tfinger.y         = y;
               ev.tfinger.dx        = x - oldx;
               ev.tfinger.dy        = y - oldy;
               process_touch_mouse_event(sw, &ev);
            }
         }
      }

      /* some fingers might have been let go */
      if (sw->previous_touch_state[i] && sw->touch_state[i] == false)
      {
         float x = 0;
         float y = 0;
         TouchEvent ev;
         normalize_touch_mouse_xy(&x, &y,
               sw->touch_previous_x[i], sw->touch_previous_y[i]);
         finger_id            = i;
         /* finger released from screen */
         ev.type              = FINGERUP;
         ev.tfinger.timestamp = current_time;
         ev.tfinger.fingerId  = finger_id;
         ev.tfinger.x         = x;
         ev.tfinger.y         = y;
         process_touch_mouse_event(sw, &ev);
      }
   }
}

void normalize_touch_mouse_xy(float *normalized_x,
      float *normalized_y, int reported_x, int reported_y)
{
   float x = (float) reported_x / TOUCH_MAX_X;
   float y = (float) reported_y / TOUCH_MAX_Y;

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
    * right mouse click = second finger short tap 
    * while first finger is still down
    * left click drag and drop = dual finger drag
    * right click drag and drop = triple finger drag */
   if (     event->type == FINGERDOWN 
         || event->type == FINGERUP
         || event->type == FINGERMOTION)
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
   unsigned int i;
   int id = event->tfinger.fingerId;

   /* make sure each finger is not reported down multiple times */
   for (i = 0; i < MAX_NUM_FINGERS; i++)
   {
      if (sw->finger[i].id == id)
         sw->finger[i].id = NO_TOUCH;
   }

   /* we need the timestamps to decide later if the 
    * user performed a short tap (click)
    * or a long tap (drag)
    * we also need the last coordinates for each finger 
    * to keep track of dragging */
   for (i = 0; i < MAX_NUM_FINGERS; i++)
   {
      if (sw->finger[i].id == NO_TOUCH)
      {
         sw->finger[i].id             = id;
         sw->finger[i].time_last_down = event->tfinger.timestamp;
         sw->finger[i].last_down_x    = event->tfinger.x;
         sw->finger[i].last_down_y    = event->tfinger.y;
         break;
      }
   }
}

void process_touch_mouse_finger_up(switch_input_t *sw, TouchEvent *event)
{
   unsigned int i;
   /* id (for multitouch) */
   int id = event->tfinger.fingerId;
   /* find out how many fingers were down before this event */
   int num_fingers_down = 0;

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

         if ((event->tfinger.timestamp - sw->finger[i].time_last_down) 
               > MAX_TAP_TIME)
            continue;

         /* short (<MAX_TAP_TIME ms) tap is interpreted as 
          * right/left mouse click depending on # fingers already down
          * but only if the finger hasn't moved since it was 
          * pressed down by more than MAX_TAP_MOTION_DISTANCE pixels */
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
   unsigned int i;
   unsigned int j;
   bool update_pointer;
   /* id (for multitouch) */
   int id = event->tfinger.fingerId;
   /* find out how many fingers were down before this event */
   int num_fingers_down = 0;

   for (i = 0; i < MAX_NUM_FINGERS; i++)
   {
      if (sw->finger[i].id >= 0)
         num_fingers_down++;
   }

   if (num_fingers_down == 0)
      return;

   /* If we are starting a multi-finger drag, 
    * start holding down the mouse button */
   if (num_fingers_down >= 2 && !sw->multi_finger_dragging)
   {
      /* only start a multi-finger drag if at least 
       * two fingers have been down long enough */
      int num_fingers_down_long = 0;
      for (i = 0; i < MAX_NUM_FINGERS; i++)
      {
         if (sw->finger[i].id == NO_TOUCH)
            continue;
         if (event->tfinger.timestamp - sw->finger[i].time_last_down 
               > MAX_TAP_TIME)
            num_fingers_down_long++;
      }
      if (num_fingers_down_long >= 2)
      {
         int simulated_button = 0;
         if (num_fingers_down_long == 2)
         {
            simulated_button          = TOUCH_MOUSE_BUTTON_LEFT;
            sw->multi_finger_dragging = DRAG_TWO_FINGER;
         }
         else
         {
            simulated_button          = TOUCH_MOUSE_BUTTON_RIGHT;
            sw->multi_finger_dragging = DRAG_THREE_FINGER;
         }

         if (simulated_button == TOUCH_MOUSE_BUTTON_LEFT)
            sw->mouse_button_left     = true;
         else if (simulated_button == TOUCH_MOUSE_BUTTON_RIGHT)
            sw->mouse_button_right    = true;
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
      sw->mouse_x       = x;
      sw->mouse_y       = y;
   }
   else
   {
      /* for relative mode, use the pointer speed setting */
      int dx        = event->tfinger.dx * TOUCH_MAX_X * 256 * 
         sw->touch_mouse_speed_factor;
      int dy        = event->tfinger.dy * TOUCH_MAX_Y * 256 * 
         sw->touch_mouse_speed_factor;
      sw->hires_dx += dx;
      sw->hires_dy += dy;
      int x_rel     = sw->hires_dx / 256;
      int y_rel     = sw->hires_dy / 256;
      if (x_rel || y_rel)
      {
         sw->mouse_x_delta  = x_rel;
         sw->mouse_y_delta  = y_rel;
         sw->mouse_x       += x_rel;
         sw->mouse_y       += y_rel;
      }
      sw->hires_dx         %= 256;
      sw->hires_dy         %= 256;
   }
}

static void normalized_to_screen_xy(
      int *screenX, int *screenY, float x, float y)
{
   /* map to display */
   *screenX = x * TOUCH_MAX_X;
   *screenY = y * TOUCH_MAX_Y;
}

static void finish_simulated_mouse_clicks(
      switch_input_t *sw, uint64_t currentTime)
{
   unsigned int i;
   for (i = 0; i < 2; i++)
   {
      if (sw->simulated_click_start_time[i] == 0)
         continue;

      if (currentTime - sw->simulated_click_start_time[i] 
            < SIMULATED_CLICK_DURATION)
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

   if (!sw)
      return;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
      if (sw->sixaxis_handles_count[i] > 0)
         for (j = 0; j < sw->sixaxis_handles_count[i]; j++)
            hidStopSixAxisSensor(sw->sixaxis_handles[i][j]);

   free(sw);
}

static void* switch_input_init(const char *joypad_driver)
{
#ifdef HAVE_LIBNX
   unsigned int i;
#endif
   switch_input_t *sw = (switch_input_t*) calloc(1, sizeof(*sw));
   if (!sw)
      return NULL;

#ifdef HAVE_LIBNX
    hidInitializeTouchScreen();
    hidInitializeMouse();
    hidInitializeKeyboard();

   /*
      Here we assume that the touch screen is always 1280x720
      Call me back when a Nintendo Switch XL is out
   */

   input_keymaps_init_keyboard_lut(rarch_key_map_switch);
   for (i = 0; i <= SWITCH_MAX_SCANCODE; i++)
      sw->keyboard_state[i]     = false;

   sw->mouse_x                  = 0;
   sw->mouse_y                  = 0;
   sw->mouse_previous_buttons   = 0;

   /* touch mouse init */
   sw->touch_mouse_indirect     = true;
   /* direct mode is not calibrated it seems */
   sw->touch_mouse_speed_factor = 1.0;
   for (i = 0; i < MAX_NUM_FINGERS; i++)
      sw->finger[i].id = NO_TOUCH;

   sw->multi_finger_dragging = DRAG_NONE;

   for (i = 0; i < 2; i++)
      sw->simulated_click_start_time[i] = 0;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
      sw->sixaxis_handles_count[i]      = 0;
#endif

   return sw;
}

static uint64_t switch_input_get_capabilities(void *data)
{
   return
#ifdef HAVE_LIBNX
           (1 << RETRO_DEVICE_POINTER) 
         | (1 << RETRO_DEVICE_KEYBOARD) 
         | (1 << RETRO_DEVICE_MOUSE) |
#endif
           (1 << RETRO_DEVICE_JOYPAD) 
         | (1 << RETRO_DEVICE_ANALOG);
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
            hidGetSixAxisSensorHandles(&sw->sixaxis_handles[port][0], 2, port, HidNpadStyleTag_NpadJoyDual);

            hidGetSixAxisSensorHandles(&sw->sixaxis_handles[port][2], 1, port, HidNpadStyleTag_NpadFullKey);

            if(port == 0)
            {
               hidGetSixAxisSensorHandles(&sw->sixaxis_handles[port][3], 1, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
               handles_count = 4;
            }
            else
               handles_count = 3;

            for (i = 0; i < handles_count; i++)
               hidStartSixAxisSensor(sw->sixaxis_handles[port][i]);

            sw->sixaxis_handles_count[port] = handles_count;
         }
         return true;
      case RETRO_SENSOR_DUMMY:
         break;
   }
#endif

   return false;
}

static float switch_input_get_sensor_input(void *data,
      unsigned port, unsigned id)
{
#ifdef HAVE_LIBNX
   float f;
   unsigned int i;
   HidSixAxisSensorState sixaxis;
   switch_input_t *sw = (switch_input_t*) data;

   if (id >= RETRO_SENSOR_ACCELEROMETER_X && id <= RETRO_SENSOR_GYROSCOPE_Z
      && port < DEFAULT_MAX_PADS)
   {
      for(i = 0; i < sw->sixaxis_handles_count[port]; i++)
      {
         hidGetSixAxisSensorStates(sw->sixaxis_handles[port][i], &sixaxis, 1);
         if(sixaxis.delta_time)
            break;
      }

      switch(id)
      {
         case RETRO_SENSOR_ACCELEROMETER_X:
            return sixaxis.acceleration.x;
         case RETRO_SENSOR_ACCELEROMETER_Y:
            return sixaxis.acceleration.y;
         case RETRO_SENSOR_ACCELEROMETER_Z:
            return sixaxis.acceleration.z;
         case RETRO_SENSOR_GYROSCOPE_X:
            return sixaxis.angular_velocity.x;
         case RETRO_SENSOR_GYROSCOPE_Y:
            return sixaxis.angular_velocity.y;
         case RETRO_SENSOR_GYROSCOPE_Z:
            return sixaxis.angular_velocity.z;
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
#ifdef HAVE_LIBNX
   switch_input_poll,
#else
   NULL,                            /* poll       */
#endif
   switch_input_state,
   switch_input_free_input,
   switch_input_set_sensor_state,
   switch_input_get_sensor_input,
   switch_input_get_capabilities,
   "switch",
   NULL,                            /* grab_mouse */
   NULL,
   NULL
};
