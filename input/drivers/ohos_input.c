/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2019 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <unistd.h>


#include "../../frontend/drivers/platform_unix.h"
#include "../../retroarch.h"
#include "../input_keymaps.h"
#include "../input_driver.h"

#define MAX_TOUCH 10
#define MAX_NUM_KEYBOARDS 3
#define DEFAULT_ASENSOR_EVENT_RATE 60


/* Use this to enable/disable using the touch screen as mouse */
#define ENABLE_TOUCH_SCREEN_MOUSE 0
// TODO fix
#define KEYCODE_ASSIST 219
#define KEYCODE_BACK 220

#define LAST_KEYCODE KEYCODE_ASSIST

#define MAX_KEYS ((LAST_KEYCODE + 7) / 8)

/* First ports are used to keep track of gamepad states.
 * Last port is used for keyboard state */
static uint8_t ohos_key_state[DEFAULT_MAX_PADS + 1][MAX_KEYS];

#define OHOS_KEYBOARD_PORT_INPUT_PRESSED(binds, id) (BIT_GET(ohos_key_state[OHOS_KEYBOARD_PORT], rarch_keysym_lut[(binds)[(id)].key]))

#define OHOS_KEYBOARD_INPUT_PRESSED(key) (BIT_GET(ohos_key_state[0], (key)))

uint8_t *ohos_keyboard_state_get(unsigned port)
{
   return ohos_key_state[port];
}


typedef struct
{
   float x;
   float y;
   float z;
} sensor_t;

struct input_pointer
{
   int16_t x, y;
   int16_t confined_x, confined_y;
   int16_t full_x, full_y;
};

enum
{
   AXIS_X        = 0,
   AXIS_Y        = 1,
   AXIS_Z        = 11,
   AXIS_RZ       = 14,
   AXIS_HAT_X    = 15,
   AXIS_HAT_Y    = 16,
   AXIS_LTRIGGER = 17,
   AXIS_RTRIGGER = 18,
   AXIS_GAS      = 22,
   AXIS_BRAKE    = 23
};

typedef struct state_device
{
   int id;
   int port;
   char name[256];
} state_device_t;

typedef struct ohos_input
{
   int64_t downTime;
   int64_t quick_tap_time;
   state_device_t pad_states[MAX_USERS];        /* int alignment */
   int mouse_x, mouse_y;
   int16_t mouse_x_viewport_screen, mouse_y_viewport_screen;
   int16_t mouse_x_viewport, mouse_y_viewport;
   int mouse_x_delta, mouse_y_delta;
   int mouse_l, mouse_r, mouse_m, mouse_wu, mouse_wd;
   bool mouse_activated;
   unsigned pads_connected;
   unsigned pointer_count;
   sensor_t accelerometer_state;                /* float alignment */
   sensor_t gyroscope_state;                    /* float alignment */
   float mouse_x_prev, mouse_y_prev;
   struct input_pointer pointer[MAX_TOUCH];     /* int16_t alignment */
   char device_model[256];
} ohos_input_t;

static float TouchEvent_getX(TouchEvent event, int touchId){
    for(int i=0; i< event.pointerCount; i++){
        if(event.touchPoints[i].id == touchId)
            return event.touchPoints[i].x;
    }
    return 0;
}
static float TouchEvent_getY(TouchEvent event, int touchId){
    for(int i=0; i< event.pointerCount; i++){
        if(event.touchPoints[i].id == touchId)
            return event.touchPoints[i].y;
    }
    return 0;
}

void ohos_input_poll_touch_event(
   void  *ohos_input, TouchEvent event)
{
   ohos_input_t* ohos = (ohos_input_t*)ohos_input;
   int motion_ptr = event.id;
   int action        = event.type;
   bool keyup        = (
            action == EVENT_ACTION_UP
         || action == EVENT_ACTION_Cancel);
   if (keyup && motion_ptr < MAX_TOUCH)
   {
      if (event.type == 1 && ENABLE_TOUCH_SCREEN_MOUSE)
      {
         /* If touchscreen was pressed for less than 200ms
          * then register time stamp of a quick tap */
         if ((event.eventTime-ohos->downTime)/1000000 < 200)
         {
            /* Prevent the quick tap if a button on the overlay is down */
            input_driver_state_t *input_st = input_state_get_ptr();
            if (!(input_st->flags & INP_FLAG_BLOCK_POINTER_INPUT))
               ohos->quick_tap_time = event.pointerCount;
         }
         ohos->mouse_l = 0;
      }
      memmove(ohos->pointer + motion_ptr,
            ohos->pointer + motion_ptr + 1,
            (MAX_TOUCH - motion_ptr - 1) * sizeof(struct input_pointer));
      if (ohos->pointer_count > 0)
         ohos->pointer_count--;
   }
   else
   {
      int pointer_max  = MIN(event.pointerCount, MAX_TOUCH);

      if (action == EVENT_ACTION_DOWN && ENABLE_TOUCH_SCREEN_MOUSE)
      {
         /* When touch screen is pressed, set mouse
          * previous position to current position
          * before starting to calculate mouse movement deltas. */
         ohos->mouse_x_prev = TouchEvent_getX(event, motion_ptr);
         ohos->mouse_y_prev = TouchEvent_getY(event, motion_ptr);

         /* If another touch happened within 200ms after a quick tap
          * then cancel the quick tap and register left mouse button
          * as being held down */
         if ((event.eventTime - ohos->quick_tap_time)/1000000 < 200)
         {
            ohos->quick_tap_time = 0;
            ohos->mouse_l        = 1;
         }
      }

//      if ((  action == EVENT_ACTION_MOVE
//               || action == EVENT_ACTION_HOVER_MOVE)
//            && ENABLE_TOUCH_SCREEN_MOUSE)
        // android_mouse_calculate_deltas(ohos,event,motion_ptr,source);

      for (motion_ptr = 0; motion_ptr < pointer_max; motion_ptr++)
      {
         struct video_viewport vp = {0};
         float x = TouchEvent_getX(event, motion_ptr);
         float y = TouchEvent_getY(event, motion_ptr);;
      
        
         video_driver_translate_coord_viewport_confined_wrap(
               &vp,
               x, y,
               &ohos->pointer[motion_ptr].confined_x,
               &ohos->pointer[motion_ptr].confined_y,
               &ohos->pointer[motion_ptr].full_x,
               &ohos->pointer[motion_ptr].full_y);

         video_driver_translate_coord_viewport_wrap(
               &vp,
               x, y,
               &ohos->pointer[motion_ptr].x,
               &ohos->pointer[motion_ptr].y,
               &ohos->pointer[motion_ptr].full_x,
               &ohos->pointer[motion_ptr].full_y);

         ohos->pointer_count = MAX(
               ohos->pointer_count,
               motion_ptr + 1);
      }
   }

   /* If more than one pointer detected
    * then count it as a mouse right click */
   if (ENABLE_TOUCH_SCREEN_MOUSE)
      ohos->mouse_r = (ohos->pointer_count == 2);
}

static void ohos_input_poll_main_cmd(void)
{
   int8_t cmd;
   struct ohos_app *ohos_app = (struct ohos_app*)g_ohos;

   if (read(ohos_app->msgread, &cmd, sizeof(cmd)) != sizeof(cmd))
      cmd = -1;

   switch (cmd)
   {
      case APP_CMD_REINIT_DONE:
         slock_lock(ohos_app->mutex);

         ohos_app->reinitRequested = 0;

         scond_broadcast(ohos_app->cond);
         slock_unlock(ohos_app->mutex);
         break;

      case APP_CMD_INPUT_CHANGED:
         slock_lock(ohos_app->mutex);

//         if (ohos_app->inputQueue)
//            AInputQueue_detachLooper(ohos_app->inputQueue);
//
//         ohos_app->inputQueue = ohos_app->pendingInputQueue;
//
//         if (ohos_app->inputQueue)
//            AInputQueue_attachLooper(ohos_app->inputQueue,
//                  ohos_app->looper, LOOPER_ID_INPUT, NULL,
//                  NULL);

         scond_broadcast(ohos_app->cond);
         slock_unlock(ohos_app->mutex);

         break;

      case APP_CMD_INIT_WINDOW:
         slock_lock(ohos_app->mutex);
         ohos_app->window = ohos_app->pendingWindow;
         ohos_app->reinitRequested = 1;
         scond_broadcast(ohos_app->cond);
         slock_unlock(ohos_app->mutex);

         break;

      case APP_CMD_SAVE_STATE:
         slock_lock(ohos_app->mutex);
         ohos_app->stateSaved = 1;
         scond_broadcast(ohos_app->cond);
         slock_unlock(ohos_app->mutex);
         break;

      case APP_CMD_RESUME:
      case APP_CMD_START:
      case APP_CMD_PAUSE:
      case APP_CMD_STOP:
         slock_lock(ohos_app->mutex);
         ohos_app->activityState = cmd;
         scond_broadcast(ohos_app->cond);
         slock_unlock(ohos_app->mutex);
         break;
   
      case APP_CMD_TERM_WINDOW:
         slock_lock(ohos_app->mutex);

         /* The window is being hidden or closed, clean it up. */
         /* terminate display/EGL context here */
//         {
//            video_driver_state_t *state = video_state_get_ptr();
//            if (state->current_video_context.destroy_surface != NULL)
//               state->current_video_context.destroy_surface(state->context_data);
//         }

         ohos_app->window = NULL;
         scond_broadcast(ohos_app->cond);
         slock_unlock(ohos_app->mutex);
         break;

      case APP_CMD_GAINED_FOCUS:
//         {
//            runloop_state_t *runloop_st = runloop_state_get_ptr();
//            settings_t *settings         = config_get_ptr();
//            bool sensors_allowed         = !settings || settings->bools.input_sensors_enable;
//            /* Re-enable sensors that were disabled on focus loss */
//            bool enable_accelerometer   = (ohos_app->sensor_state_mask &
//                  (UINT64_C(1) << RETRO_SENSOR_ACCELEROMETER_DISABLE));
//            bool enable_gyroscope       = (ohos_app->sensor_state_mask &
//                  (UINT64_C(1) << RETRO_SENSOR_GYROSCOPE_DISABLE));
//
//            /* On first focus (no sensor state yet), enable if setting is on.
//             * This handles shader sensor access without going through cores.
//             * Default to enabling if settings aren't loaded yet (first launch). */
//            if (!enable_accelerometer &&
//                !(ohos_app->sensor_state_mask & (UINT64_C(1) << RETRO_SENSOR_ACCELEROMETER_ENABLE)))
//            {
//               if (sensors_allowed)
//                  enable_accelerometer = true;
//            }
//            if (!enable_gyroscope &&
//                !(ohos_app->sensor_state_mask & (UINT64_C(1) << RETRO_SENSOR_GYROSCOPE_ENABLE)))
//            {
//               if (sensors_allowed)
//                  enable_gyroscope = true;
//            }
//
//            if (!sensors_allowed)
//            {
//               enable_accelerometer = false;
//               enable_gyroscope     = false;
//            }
//
//            runloop_st->flags &= ~(RUNLOOP_FLAG_PAUSED
//                                 | RUNLOOP_FLAG_IDLE);
//            video_driver_unset_stub_frame();
//
//            /* Try to enable sensors via input driver. If that fails before the
//             * input driver has initialized, enable directly via sensor API. */
//            if (enable_accelerometer)
//            {
//               if (!input_set_sensor_state(0,
//                     RETRO_SENSOR_ACCELEROMETER_ENABLE,
//                     ohos_app->accelerometer_event_rate) &&
//                   !ohos_app->input_alive)
//               {
//                  /* Input driver not ready — enable sensor directly */
//                  unsigned rate = ohos_app->accelerometer_event_rate;
//                  if (rate == 0)
//                     rate = DEFAULT_ASENSOR_EVENT_RATE;
//                  ohos_input_enable_sensor_manager(ohos_app);
//                  if (ohos_enable_sensor(ohos_app,
//                        ohos_app->accelerometerSensor, rate,
//                        RETRO_SENSOR_ACCELEROMETER_ENABLE,
//                        RETRO_SENSOR_ACCELEROMETER_DISABLE))
//                     ohos_app->accelerometer_event_rate = rate;
//               }
//            }
//
//            if (enable_gyroscope)
//            {
//               if (!input_set_sensor_state(0,
//                     RETRO_SENSOR_GYROSCOPE_ENABLE,
//                     ohos_app->gyroscope_event_rate) &&
//                   !ohos_app->input_alive)
//               {
//                  /* Input driver not ready — enable sensor directly */
//                  unsigned rate = ohos_app->gyroscope_event_rate;
//                  if (rate == 0)
//                     rate = DEFAULT_ASENSOR_EVENT_RATE;
//                  ohos_input_enable_sensor_manager(ohos_app);
//                  if (ohos_enable_sensor(ohos_app,
//                        ohos_app->gyroscopeSensor, rate,
//                        RETRO_SENSOR_GYROSCOPE_ENABLE,
//                        RETRO_SENSOR_GYROSCOPE_DISABLE))
//                     ohos_app->gyroscope_event_rate = rate;
//               }
//            }
//
//            /* Start gravity-based orientation detection (once per app launch only).
//             * Samples accelerometer data over ~30 frames to determine which axis
//             * gravity is on, detecting both display rotation and sensor IC
//             * misalignment automatically. */
//            if (!ohos_app->gravity_calibrated)
//            {
//               ohos_app->gravity_accum_x      = 0.0f;
//               ohos_app->gravity_accum_y      = 0.0f;
//               ohos_app->gravity_sample_count = 0;
//            }
//            else
//            {
//               /* Gravity already calibrated (not first launch),
//                * start rest position capture immediately */
//               input_sensor_start_rest_capture();
//            }
//         }
//         slock_lock(ohos_app->mutex);
//         ohos_app->unfocused = false;
//         scond_broadcast(ohos_app->cond);
//         slock_unlock(ohos_app->mutex);
         break;
      case APP_CMD_LOST_FOCUS:
//         {
//            runloop_state_t *runloop_st = runloop_state_get_ptr();
//            bool disable_accelerometer  = (ohos_app->sensor_state_mask &
//                  (UINT64_C(1) << RETRO_SENSOR_ACCELEROMETER_ENABLE)) &&
//                        ohos_app->accelerometerSensor;
//            bool disable_gyroscope      = (ohos_app->sensor_state_mask &
//                  (UINT64_C(1) << RETRO_SENSOR_GYROSCOPE_ENABLE)) &&
//                        ohos_app->gyroscopeSensor;
//
//            runloop_st->flags |=  (RUNLOOP_FLAG_PAUSED
//                                 | RUNLOOP_FLAG_IDLE);
//            video_driver_set_stub_frame();
//
//            /* Avoid draining battery while app is not being used. */
//            if (disable_accelerometer)
//               input_set_sensor_state(0,
//                     RETRO_SENSOR_ACCELEROMETER_DISABLE,
//                     ohos_app->accelerometer_event_rate);
//
//            if (disable_gyroscope)
//               input_set_sensor_state(0,
//                     RETRO_SENSOR_GYROSCOPE_DISABLE,
//                     ohos_app->gyroscope_event_rate);
//         }
//         slock_lock(ohos_app->mutex);
//         ohos_app->unfocused = true;
//         scond_broadcast(ohos_app->cond);
//         slock_unlock(ohos_app->mutex);
         break;

      case APP_CMD_DESTROY:
         ohos_app->destroyRequested = 1;
         break;
   }
}
bool ohos_run_events(void *data)
{
   struct ohos_app *ohos_app = (struct ohos_app*)g_ohos;
   ohos_input_poll_main_cmd();
   return true;
}
    
    
//static void ohos_input_reinit(void)
//{
//   struct android_app *android_app = (struct android_app*)g_android;
//   uint32_t runloop_flags = runloop_get_flags();
//
//   if (runloop_flags & RUNLOOP_FLAG_PAUSED)
//   {
//      /* When using OpenGL, pausing the app (e.g. by opening the app switcher)
//       * will result in the EGL window surface being destroyed, but the actual
//       * OpenGL context will be preserved on most devices, so we may be able to
//       * get away with reinitializing only the window surface without having to
//       * do a full video driver reinitialization. */
//      video_driver_state_t *state = video_state_get_ptr();
//      if (state->current_video_context.create_surface == NULL || !state->current_video_context.create_surface(state->context_data))
//         command_event(CMD_EVENT_REINIT, NULL);
//   }
//
//   ohos_app_write_cmd(android_app, APP_CMD_REINIT_DONE);
//}
static int ohos_check_quick_tap(ohos_input_t *ohos)
{
   /* Check if the touch screen has been been quick tapped
    * and then not touched again for 200ms
    * If so then return true and deactivate quick tap timer */
   retro_time_t now = cpu_features_get_time_usec();
   if (ohos->quick_tap_time &&
         (now / 1000 - ohos->quick_tap_time / 1000000) >= 200)
   {
      ohos->quick_tap_time = 0;
      return 1;
   }

   return 0;
}


static void *ohos_input_init(const char *joypad_driver)
{
   struct ohos_app *ohos_app = (struct ohos_app*)g_ohos;
   ohos_input_t *ohos  = NULL;
   if (!(ohos = (ohos_input_t*)calloc(1, sizeof(*ohos))))
      return NULL;
   ohos_app->ohos_input = ohos;
    
   ohos->mouse_activated = false;
   ohos->pads_connected = 0;
   ohos->quick_tap_time = 0;
   // TODO sensors
   {
      settings_t *settings = config_get_ptr();
      bool enable_sensors = !settings || settings->bools.input_sensors_enable;
      if (enable_sensors && !ohos_app->sensor_state_mask)
      {
         unsigned rate = g_ohos->accelerometer_event_rate;
         if (rate == 0)
            rate = DEFAULT_ASENSOR_EVENT_RATE;
         rate = g_ohos->gyroscope_event_rate;
         if (rate == 0)
            rate = DEFAULT_ASENSOR_EVENT_RATE;
      }
   }
   return ohos;
}

static int16_t ohos_input_state(
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
   ohos_input_t *ohos = (ohos_input_t*)data;

  switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;

            if (!keyboard_mapping_blocked)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (binds[port][i].valid)
                  {
                     if (     (binds[port][i].key && binds[port][i].key < RETROK_LAST)
                           && OHOS_KEYBOARD_PORT_INPUT_PRESSED(binds[port], i))
                        ret |= (1 << i);
                  }
               }
            }

            return ret;
         }
         if (id < RARCH_BIND_LIST_END)
         {
            if (binds[port][id].valid)
            {
               if (     (binds[port][id].key && binds[port][id].key < RETROK_LAST)
                     && OHOS_KEYBOARD_PORT_INPUT_PRESSED(binds[port], id)
                     && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked)
                     )
                  return 1;
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         break;
      case RETRO_DEVICE_KEYBOARD:
         return (id && id < RETROK_LAST) && BIT_GET(ohos_key_state[OHOS_KEYBOARD_PORT], rarch_keysym_lut[id]);
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         {
            /* Same mouse state is reported for all ports. */
            int val = 0;
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_LEFT:
                  return ohos->mouse_l || ohos_check_quick_tap(ohos);
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
                  return ohos->mouse_r;
               case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                  return ohos->mouse_m;
               case RETRO_DEVICE_ID_MOUSE_X:
                  if (device == RARCH_DEVICE_MOUSE_SCREEN)
                     return ohos->mouse_x_viewport_screen;

                  val = ohos->mouse_x_delta;
                  ohos->mouse_x_delta = 0;
                  /* flush delta after it has been read */
                  return val;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  if (device == RARCH_DEVICE_MOUSE_SCREEN)
                     return ohos->mouse_y_viewport_screen;

                  val = ohos->mouse_y_delta;
                  ohos->mouse_y_delta = 0;
                  /* flush delta after it has been read */
                  return val;
               case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                  val = ohos->mouse_wu;
                  ohos->mouse_wu = 0;
                  return val;
               case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                  val = ohos->mouse_wd;
                  ohos->mouse_wd = 0;
                  return val;
            }
         }
         break;
      case RETRO_DEVICE_LIGHTGUN:
         {
            /* Same lightgun state is reported for all ports. */
            int val = 0;
            switch (id)
            {
               /* Favor mouse for lightgun control. */
               case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
                  if (ohos->mouse_activated)
                     return ohos->mouse_x_viewport_screen;
                  else
                     return ohos->pointer[idx].x;
               case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
                  if (ohos->mouse_activated)
                     return ohos->mouse_y_viewport_screen;
                  else
                     return ohos->pointer[idx].y;
               /* Deprecated relative lightgun. */
               case RETRO_DEVICE_ID_LIGHTGUN_X:
                  val                    = ohos->mouse_x_delta;
                  ohos->mouse_x_delta = 0;
                  /* flush delta after it has been read */
                  return val;
               case RETRO_DEVICE_ID_LIGHTGUN_Y:
                  val                    = ohos->mouse_y_delta;
                  ohos->mouse_y_delta = 0;
                  /* flush delta after it has been read */
                  return val;
               case RETRO_DEVICE_ID_LIGHTGUN_CURSOR:
               case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
                  return ohos->mouse_m || ohos->pointer_count == 3;
               case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
                  return ohos->mouse_r && ohos->mouse_l;
               case RETRO_DEVICE_ID_LIGHTGUN_START:
               case RETRO_DEVICE_ID_LIGHTGUN_TURBO:
                  return ohos->mouse_r || ohos->pointer_count == 2;
               case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
                  return ohos->mouse_l || ohos_check_quick_tap(ohos) || ohos->pointer_count == 1;
               case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
                  return input_driver_pointer_is_offscreen(ohos->pointer[idx].x, ohos->pointer[idx].y);
            }
         }
         break;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         /* Same pointer state is reported for all ports. */
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               if (device == RARCH_DEVICE_POINTER_SCREEN)
                  return ohos->pointer[idx].full_x;
               return ohos->pointer[idx].confined_x;
            case RETRO_DEVICE_ID_POINTER_Y:
               if (device == RARCH_DEVICE_POINTER_SCREEN)
                  return ohos->pointer[idx].full_y;
               return ohos->pointer[idx].confined_y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               /* On mobile platforms, touches outside screen / core viewport are not reported. */
               if (device == RARCH_DEVICE_POINTER_SCREEN)
                  return (idx < ohos->pointer_count) &&
                     (ohos->pointer[idx].full_x != -0x8000) &&
                     (ohos->pointer[idx].full_y != -0x8000);
               return (idx < ohos->pointer_count) &&
                  (ohos->pointer[idx].x != -0x8000) &&
                  (ohos->pointer[idx].y != -0x8000);
            case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
               return input_driver_pointer_is_offscreen(ohos->pointer[idx].x, ohos->pointer[idx].y);
            case RETRO_DEVICE_ID_POINTER_COUNT:
               return ohos->pointer_count;
            case RARCH_DEVICE_ID_POINTER_BACK:
            {
               const struct retro_keybind *keyptr =
                  &input_autoconf_binds[0][RARCH_MENU_TOGGLE];
               if (keyptr->joykey == 0)
                  return OHOS_KEYBOARD_INPUT_PRESSED(KEYCODE_BACK);
            }
         }
         break;
   }

   return 0;
}

static void ohos_input_free(void *data)
{
   ohos_input_t *ohos = (ohos_input_t*)data;

   if (!ohos)
      return;
   free(data);
}

static bool ohos_input_set_sensor_state(void *data, unsigned port, enum retro_sensor_action action, unsigned rate)
{
   ohos_input_t *ohos = (ohos_input_t*)data;

   if (!ohos)
      return false;
   return false;
}

static float ohos_input_get_sensor_input(void *data, unsigned port, unsigned id)
{
   ohos_input_t *ohos = (ohos_input_t*)data;

   if (!ohos)
      return 0.0f;

   switch (id)
   {
//      case RETRO_SENSOR_ILLUMINANCE:
//         if (ohos->illuminance_sensor)
//            return linux_get_illuminance_reading(ohos->illuminance_sensor);
      default:
         break;
   }

   return 0.0f;
}

static void ohos_input_poll(void *data)
{
   uint8_t c;
   ohos_input_t *ohos = (ohos_input_t*)data;
  
}

static uint64_t ohos_get_capabilities(void *data)
{
   return (1 << RETRO_DEVICE_JOYPAD)
        | (1 << RETRO_DEVICE_ANALOG)
        | (1 << RETRO_DEVICE_KEYBOARD);
}
input_driver_t input_ohos = {
   ohos_input_init,
   ohos_input_poll,
   ohos_input_state,
   ohos_input_free,
   ohos_input_set_sensor_state,
   ohos_input_get_sensor_input,
   ohos_get_capabilities,
   "ohos",
   NULL,                         /* grab_mouse */
   NULL,
   NULL
};
