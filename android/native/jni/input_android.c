/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012-2013 - Michael Lelli
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

#include <android/keycodes.h>
#include <unistd.h>
#include <dlfcn.h>
#include "input_autodetect.h"
#include "../../../frontend/frontend_android.h"
#include "../../../input/input_common.h"
#include "../../../performance.h"
#include "../../../general.h"
#include "../../../driver.h"

#define MAX_TOUCH 16

typedef struct
{
   int16_t lx, ly;
   int16_t rx, ry;
} analog_t;

#define PRESSED_UP(x, y)    ((y <= dzone_min))
#define PRESSED_DOWN(x, y)  ((y >= dzone_max))
#define PRESSED_LEFT(x, y)  ((x <= dzone_min))
#define PRESSED_RIGHT(x, y) ((x >= dzone_max))

static unsigned pads_connected;
static int state_device_ids[MAX_PADS];
static uint64_t state[MAX_PADS];
static uint64_t keycode_lut[LAST_KEYCODE];
analog_t analog_state[MAX_PADS];

struct input_pointer
{
   int16_t x, y;
   int16_t full_x, full_y;
};

static struct input_pointer pointer[MAX_TOUCH];
static unsigned pointer_count;

enum
{
   AXIS_X = 0,
   AXIS_Y = 1,
   AXIS_Z = 11,
   AXIS_RZ = 14,
   AXIS_HAT_X = 15,
   AXIS_HAT_Y = 16,
   AXIS_LTRIGGER = 17,
   AXIS_RTRIGGER = 18,
};

void (*engine_handle_dpad)(AInputEvent*, size_t, int, char*, size_t, int, bool, unsigned);

extern float AMotionEvent_getAxisValue(const AInputEvent* motion_event,
      int32_t axis, size_t pointer_index);

static typeof(AMotionEvent_getAxisValue) *p_AMotionEvent_getAxisValue;

#define AMotionEvent_getAxisValue (*p_AMotionEvent_getAxisValue)

/**
 * Process the next main command.
 */
void engine_handle_cmd(void)
{
   struct android_app *android_app = (struct android_app*)g_android;
   int8_t cmd;

   if (read(android_app->msgread, &cmd, sizeof(cmd)) != sizeof(cmd))
      cmd = -1;

   switch (cmd)
   {
      case APP_CMD_INPUT_CHANGED:
         pthread_mutex_lock(&android_app->mutex);

         if (android_app->inputQueue != NULL)
            AInputQueue_detachLooper(android_app->inputQueue);

         android_app->inputQueue = android_app->pendingInputQueue;

         if (android_app->inputQueue != NULL)
         {
            RARCH_LOG("Attaching input queue to looper");
            AInputQueue_attachLooper(android_app->inputQueue,
                  android_app->looper, LOOPER_ID_INPUT, NULL,
                  NULL);
         }

         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         
         break;

      case APP_CMD_INIT_WINDOW:
         pthread_mutex_lock(&android_app->mutex);
         android_app->window = android_app->pendingWindow;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);

         if (g_extern.lifecycle_state & (1ULL << RARCH_PAUSE_TOGGLE))
            init_drivers();
         break;

      case APP_CMD_RESUME:
         pthread_mutex_lock(&android_app->mutex);
         android_app->activityState = cmd;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;

      case APP_CMD_START:
         pthread_mutex_lock(&android_app->mutex);
         android_app->activityState = cmd;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;

      case APP_CMD_PAUSE:
         pthread_mutex_lock(&android_app->mutex);
         android_app->activityState = cmd;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);

         if (!(g_extern.lifecycle_state & (1ULL << RARCH_QUIT_KEY)))
         {
            RARCH_LOG("Pausing RetroArch.\n");
            g_extern.lifecycle_state |= (1ULL << RARCH_PAUSE_TOGGLE);
         }
         break;

      case APP_CMD_STOP:
         pthread_mutex_lock(&android_app->mutex);
         android_app->activityState = cmd;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;

      case APP_CMD_CONFIG_CHANGED:
         break;
      case APP_CMD_TERM_WINDOW:
         pthread_mutex_lock(&android_app->mutex);

         /* The window is being hidden or closed, clean it up. */
         /* terminate display/EGL context here */
         if (g_extern.lifecycle_state & (1ULL << RARCH_PAUSE_TOGGLE))
            uninit_drivers();
         else
            RARCH_WARN("Window is terminated outside PAUSED state.\n");

         android_app->window = NULL;
         pthread_cond_broadcast(&android_app->cond);
         pthread_mutex_unlock(&android_app->mutex);
         break;

      case APP_CMD_GAINED_FOCUS:
         g_extern.lifecycle_state &= ~(1ULL << RARCH_PAUSE_TOGGLE);
         break;

      case APP_CMD_LOST_FOCUS:
         break;

      case APP_CMD_DESTROY:
         g_extern.lifecycle_state |= (1ULL << RARCH_QUIT_KEY);
         break;
   }
}

static void engine_handle_dpad_default(AInputEvent *event,
      size_t motion_pointer, int state_id, char *msg, size_t msg_sizeof,
      int source, bool debug_enable, unsigned emulation)
{
   uint64_t *state_cur = &state[state_id];
   float dzone_min = -g_settings.input.axis_threshold;
   float dzone_max = g_settings.input.axis_threshold;
   float x = AMotionEvent_getX(event, motion_pointer);
   float y = AMotionEvent_getY(event, motion_pointer);

   *state_cur &= ~((1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) | 
         (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) | (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) |
         (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN));

   if (emulation == ANALOG_DPAD_LSTICK)
   {
      *state_cur |= PRESSED_LEFT(x, y)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)  : 0;
      *state_cur |= PRESSED_RIGHT(x, y) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
      *state_cur |= PRESSED_UP(x, y)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)    : 0;
      *state_cur |= PRESSED_DOWN(x, y)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)  : 0;
   }

   if (debug_enable)
      snprintf(msg, msg_sizeof, "Pad %d : x = %.2f, y = %.2f, src %d.\n",
            state_id, x, y, source);
}

static void engine_handle_dpad_getaxisvalue(AInputEvent *event,
      size_t motion_pointer, int state_id, char *msg, size_t msg_sizeof, int source,
      bool debug_enable, unsigned emulation)
{
   uint64_t *state_cur = &state[state_id];
   float dzone_min = -g_settings.input.axis_threshold;
   float dzone_max = g_settings.input.axis_threshold;
   float x = AMotionEvent_getAxisValue(event, AXIS_X, motion_pointer);
   float y = AMotionEvent_getAxisValue(event, AXIS_Y, motion_pointer);
   float z = AMotionEvent_getAxisValue(event, AXIS_Z, motion_pointer);
   float rz = AMotionEvent_getAxisValue(event, AXIS_RZ, motion_pointer);
   float hatx = AMotionEvent_getAxisValue(event, AXIS_HAT_X, motion_pointer);
   float haty = AMotionEvent_getAxisValue(event, AXIS_HAT_Y, motion_pointer);
   float ltrig = AMotionEvent_getAxisValue(event, AXIS_LTRIGGER, motion_pointer);
   float rtrig = AMotionEvent_getAxisValue(event, AXIS_RTRIGGER, motion_pointer);

   *state_cur &= ~((1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) | 
         (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) | (1ULL << RETRO_DEVICE_ID_JOYPAD_R2) |
         (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) | (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) |
         (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN));

   /* On some devices the dpad sends AXIS_HAT_X / AXIS_HAT_Y events, use those first if the returned values are nonzero */
   if (fabsf(hatx) > 0.0001f || fabsf(haty) > 0.0001f)
   {
      *state_cur |= PRESSED_LEFT(hatx, haty)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)  : 0;
      *state_cur |= PRESSED_RIGHT(hatx, haty) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
      *state_cur |= PRESSED_UP(hatx, haty)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)    : 0;
      *state_cur |= PRESSED_DOWN(hatx, haty)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)  : 0;
   }
   else if (emulation == ANALOG_DPAD_LSTICK)
   {
      *state_cur |= PRESSED_LEFT(x, y)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)  : 0;
      *state_cur |= PRESSED_RIGHT(x, y) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
      *state_cur |= PRESSED_UP(x, y)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)    : 0;
      *state_cur |= PRESSED_DOWN(x, y)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)  : 0;
   }

   if (ltrig > 0.5f)
      *state_cur |= 1ULL << RETRO_DEVICE_ID_JOYPAD_L2;
   if (rtrig > 0.5f)
      *state_cur |= 1ULL << RETRO_DEVICE_ID_JOYPAD_R2;

   if (emulation == ANALOG_DPAD_DUALANALOG)
   {
      analog_state[state_id].lx = x * 0x7fff;
      analog_state[state_id].ly = y * 0x7fff;
      analog_state[state_id].rx = z * 0x7fff;
      analog_state[state_id].ry = rz * 0x7fff;
   }

   if (debug_enable)
      snprintf(msg, msg_sizeof, "Pad %d : x %.2f, y %.2f, z %.2f, rz %.2f, src %d.\n", 
            state_id, x, y, z, rz, source);
}

static void *android_input_init(void)
{
   unsigned i, j, k;
   pads_connected = 0;

   for (j = 0; j < LAST_KEYCODE; j++)
      keycode_lut[j] = 0;

   if (!g_settings.input.autodetect_enable)
   {
      for (j = 0; j < MAX_PADS; j++)
      {
         uint8_t shift = 8 + (j * 8);
         for (k = 0; k < RARCH_FIRST_CUSTOM_BIND; k++)
         {
            if (g_settings.input.binds[j][k].valid && g_settings.input.binds[j][k].joykey && g_settings.input.binds[j][k].joykey < LAST_KEYCODE)
            {
               RARCH_LOG("binding %llu to %d (p%d)\n", g_settings.input.binds[j][k].joykey, k, j);
               keycode_lut[g_settings.input.binds[j][k].joykey] |= ((k + 1) << shift);
            }
         }
      }
   }

   for (i = 0; i < MAX_PADS; i++)
   {
      for (j = 0; j < RARCH_FIRST_META_KEY; j++)
      {
         g_settings.input.binds[i][j].id = i;
         g_settings.input.binds[i][j].joykey = 0;
      }

      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_B].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_B);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_Y].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_Y);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_SELECT].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_START].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_START);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_UP].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_UP);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_A].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_A);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_X].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_X);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_L].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_L);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_R].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_R);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_L2].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_L2);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_R2].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_R2);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_L3].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_L3);
      g_settings.input.binds[i][RETRO_DEVICE_ID_JOYPAD_R3].joykey = (1ULL << RETRO_DEVICE_ID_JOYPAD_R3);

      g_settings.input.dpad_emulation[i] = ANALOG_DPAD_LSTICK;
   }

   for (i = 0; i < MAX_PLAYERS; i++)
      strlcpy(g_settings.input.device_names[i], "Custom", sizeof(g_settings.input.device_names[i]));

   if ((dlopen("/system/lib/libandroid.so", RTLD_LOCAL | RTLD_LAZY)) == 0)
   {
      RARCH_WARN("Unable to open libandroid.so\n");
      return (void*)-1;
   }
   else
   {
      p_AMotionEvent_getAxisValue = dlsym(RTLD_DEFAULT, "AMotionEvent_getAxisValue");

      if (p_AMotionEvent_getAxisValue != NULL)
      {
         RARCH_LOG("Setting engine_handle_dpad to 'Get Axis Value' (for reading extra analog sticks)");
         engine_handle_dpad = engine_handle_dpad_getaxisvalue;
      }
      else
      {
         RARCH_LOG("Setting engine_handle_dpad to 'Default'");
         engine_handle_dpad = engine_handle_dpad_default;
      }
   }

   return (void*)-1;
}

int zeus_id = -1;
int zeus_second_id = -1;
unsigned zeus_port;

static void android_input_set_keybinds(void *data, unsigned device,
      unsigned port, unsigned id, unsigned keybind_action)
{
   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS))
   {
      /* eight 8-bit values are packed into one uint64_t
       * one for each of the 8 pads */
      unsigned shift = 8 + (port * 8);

      g_settings.input.dpad_emulation[port] = ANALOG_DPAD_LSTICK;

      // NOTE - we have to add '1' to the bit mask because
      // RETRO_DEVICE_ID_JOYPAD_B is 0

      RARCH_LOG("Detecting keybinds. Device %u port %u id %u keybind_action %u\n", device, port, id, keybind_action);

      switch (device)
      {
         case DEVICE_LOGITECH_RUMBLEPAD2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Logitech Rumblepad 2",
                  sizeof(g_settings.input.device_names[port]));

            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_DUALANALOG;
            keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_9]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)  << shift);
            keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)       << shift);
            break;
         case DEVICE_LOGITECH_DUAL_ACTION:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Logitech Dual Action",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_A]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_X]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_B]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_L1]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R1]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L2]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_R2]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBL]  |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBR]  |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_SELECT]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)  << shift);
            keycode_lut[AKEYCODE_BACK]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)           << shift);
            keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_LOGITECH_PRECISION_GAMEPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Logitech Precision",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);

            keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)       << shift);

            keycode_lut[AKEYCODE_BUTTON_9]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)           << shift);
            keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_GAMEMID:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_DUALANALOG;
            strlcpy(g_settings.input.device_names[port], "GameMID",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)        << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)  << shift);
            keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)  << shift);
            keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)  << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)  << shift);
            keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)  << shift);
            keycode_lut[AKEYCODE_BUTTON_START] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)   << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBR] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBL] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)       << shift);

            /* left analog stick - TODO */
            /* right analog stick - TODO */
            /* menu button? */
            break;
         case DEVICE_ICONTROLPAD_HID_JOYSTICK:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "iControlPad HID Joystick profile",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_11] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)   << shift);
            keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)        << shift);
            keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
            break;
         case DEVICE_ICONTROLPAD_BLUEZ_IME:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "iControlPad SPP profile (using Bluez IME)",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)        << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)  << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)        << shift);
            keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)        << shift);
            keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)        << shift);
            keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)        << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
            keycode_lut[AKEYCODE_BUTTON_START] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)   << shift);

            /* left analog stick */
            keycode_lut[AKEYCODE_W] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)              << shift);
            keycode_lut[AKEYCODE_S] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)            << shift);
            keycode_lut[AKEYCODE_A] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)            << shift);
            keycode_lut[AKEYCODE_D] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)           << shift);

            /* right analog stick - TODO */
            // Left - 11
            // Right - 13
            // Down - 12
            // Up - 15
            break;
         case DEVICE_TTT_THT_ARCADE:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "TTT THT Arcade",
                  sizeof(g_settings.input.device_names[port]));

            /* same as Rumblepad 2 - merge? */
            //keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);

            keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_9] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);

            /* TODO - unsure about pad 2 still */
#if 0
            keycode_lut[AKEYCODE_BUTTON_Z]  |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]  |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_C]  |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_X]  |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

            keycode_lut[AKEYCODE_BUTTON_11]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_12]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_13]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_14]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_15]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_16]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_A]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_B]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_C]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
#endif
            break;
         case DEVICE_TOMMO_NEOGEOX_ARCADE:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "TOMMO Neogeo X Arcade",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_DPAD_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1) << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1) << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1) << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_A] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_B] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_C] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L2] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            break;
         case DEVICE_MADCATZ_PC_USB_STICK:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Madcatz PC USB Stick",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_A] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_B] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_C] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Y] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Z] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift); /* request hack */
            keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift); /* request hack */
            keycode_lut[AKEYCODE_BUTTON_L2] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_MODE] |= ((RARCH_QUIT_KEY+1) << shift);
            break;
         case DEVICE_LOGICOOL_RUMBLEPAD2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Logicool Rumblepad 2",
                  sizeof(g_settings.input.device_names[port]));
            
            // Rumblepad 2 DInput */
            /* TODO: Add L3/R3 */
            keycode_lut[AKEYCODE_BUTTON_B]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_C]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);

            keycode_lut[AKEYCODE_BUTTON_X]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_A]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);

            keycode_lut[AKEYCODE_BUTTON_L2]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);

            keycode_lut[AKEYCODE_BUTTON_Y]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_Z]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L1]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_R1]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            break;
         case DEVICE_IDROID_X360:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "iDroid x360",
                  sizeof(g_settings.input.device_names[port]));

            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            keycode_lut[AKEYCODE_INSERT] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_MINUS] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_SLASH] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_MOVE_END] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_LEFT_BRACKET] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_FORWARD_DEL] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_MEDIA_PLAY] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            keycode_lut[AKEYCODE_EQUALS] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_MENU] |=  ((RARCH_MENU_TOGGLE+1)      << shift);
            // Left Analog Up: 152
            // Left Analog Down: 146
            // Left Analog Right: 150
            // Left Analog Left: 148
            // Right Analog Up: 92 (AKEYCODE_PAGE_UP)
            // Right Analog Down: 93 (AKEYCODE_PAGE_DOWN)
            // Right Analog Right: 113 (AKEYCODE_CTRL_LEFT)
            // Right Analog Left: 72 (AKEYCODE_RIGHT_BRACKET)
            keycode_lut[AKEYCODE_NUMPAD_8] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_NUMPAD_2] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_NUMPAD_6] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            keycode_lut[AKEYCODE_NUMPAD_4] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            break;
         case DEVICE_ZEEMOTE_STEELSERIES:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Zeemote Steelseries",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_A]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_B]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_X]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_MODE]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            keycode_lut[AKEYCODE_BUTTON_L1]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R1]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            break;
         case DEVICE_HUIJIA_USB_SNES:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Huijia USB SNES",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_9] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_7] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            break;
         case DEVICE_SUPER_SMARTJOY:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Super Smartjoy",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_5] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_6] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_7] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            break;
         case DEVICE_SAITEK_RUMBLE_P480:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Saitek Rumble P480",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_9]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_MS_SIDEWINDER_DUAL_STRIKE:
            /* TODO - unfinished */
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "MS Sidewinder Dual Strike",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_4] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_2] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_6] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_5] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_3] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_1] |= ((RETRO_DEVICE_ID_JOYPAD_X) << shift);
            keycode_lut[AKEYCODE_BUTTON_7] |= ((RETRO_DEVICE_ID_JOYPAD_L) << shift);
            keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_R) << shift);
            keycode_lut[AKEYCODE_BUTTON_9] |= ((RETRO_DEVICE_ID_JOYPAD_L2) << shift);
            break;
         case DEVICE_MS_SIDEWINDER:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "MS Sidewinder",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            break;
         case DEVICE_MS_XBOX:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Xbox",
                  sizeof(g_settings.input.device_names[port]));

            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_DUALANALOG;
            keycode_lut[AKEYCODE_BUTTON_MODE] |= ((RARCH_MENU_TOGGLE + 1) << shift);
            keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_SELECT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBL]  |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBR]  |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            break;
         case DEVICE_WISEGROUP_PLAYSTATION2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "WiseGroup PlayStation2",
                  sizeof(g_settings.input.device_names[port]));

            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_DUALANALOG;
            keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_16] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_11] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_JCPS102_PLAYSTATION2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "JCPS102 PlayStation2",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_16] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_11] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_GENERIC_PLAYSTATION2_CONVERTER:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Generic PlayStation2",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_16] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_11] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_KEYBOARD_RETROPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Generic Keyboard",
                  sizeof(g_settings.input.device_names[port]));

            // Keyboard
            // TODO: Map L2/R2/L3/R3
            keycode_lut[AKEYCODE_Z] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_A] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_SHIFT_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_ENTER] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            keycode_lut[AKEYCODE_DPAD_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1) << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1) << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1) << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1) << shift);
            keycode_lut[AKEYCODE_X] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_S] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_Q] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_W] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);

            /* Misc control scheme */
            keycode_lut[AKEYCODE_F1] |= ((RARCH_MENU_TOGGLE + 1) << shift);
            keycode_lut[AKEYCODE_F2] |= ((RARCH_SAVE_STATE_KEY+1) << shift);
            keycode_lut[AKEYCODE_F4] |= ((RARCH_LOAD_STATE_KEY+1) << shift);
            keycode_lut[AKEYCODE_F7] |= ((RARCH_STATE_SLOT_PLUS+1) << shift);
            keycode_lut[AKEYCODE_F6] |= ((RARCH_STATE_SLOT_MINUS+1) << shift);
            keycode_lut[AKEYCODE_SPACE] |= ((RARCH_FAST_FORWARD_KEY+1) << shift);
            keycode_lut[AKEYCODE_L] |= ((RARCH_FAST_FORWARD_HOLD_KEY+1) << shift);
            keycode_lut[AKEYCODE_BREAK] |= ((RARCH_PAUSE_TOGGLE+1) << shift);
            keycode_lut[AKEYCODE_K] |= ((RARCH_FRAMEADVANCE+1) << shift);
            keycode_lut[AKEYCODE_H] |= ((RARCH_RESET+1) << shift);
            keycode_lut[AKEYCODE_R] |= ((RARCH_REWIND+1) << shift);
            keycode_lut[AKEYCODE_F9] |= ((RARCH_MUTE+1) << shift);

            keycode_lut[AKEYCODE_ESCAPE] |= ((RARCH_QUIT_KEY+1) << shift);
            break;
         case DEVICE_PLAYSTATION3_VERSION1:
         case DEVICE_PLAYSTATION3_VERSION2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "PlayStation3",
                  sizeof(g_settings.input.device_names[port]));

            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
            keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBL] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBR] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_1] |= ((RARCH_MENU_TOGGLE+1)     << shift);
            break;
         case DEVICE_MOGA:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "MOGA",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);

            keycode_lut[AKEYCODE_BUTTON_START] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_SELECT] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);

            keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            break;
         case DEVICE_SEGA_VIRTUA_STICK_HIGH_GRADE:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Sega Virtua Stick",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_C] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_Z] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_MODE] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RARCH_MENU_TOGGLE+1)      << shift);
            break;
         case DEVICE_PSMOVE_NAVI:
            /* TODO - unfinished */
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "PS Move Navi",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);

            keycode_lut[AKEYCODE_BUTTON_11]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_9]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_15]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_14]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            keycode_lut[AKEYCODE_UNKNOWN]   |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_JXD_S7300B:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "JXD S7300B",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

            keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);

            keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L2]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R2]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);

            keycode_lut[AKEYCODE_ENTER]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            keycode_lut[AKEYCODE_SPACE]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);

            //keycode_lut[AKEYCODE_VOLUME_UP]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            //keycode_lut[AKEYCODE_VOLUME_DOWN]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            break;
         case DEVICE_IDROID_CON:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "i.droid",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_9]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_10]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_11]  |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_12]  |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);
            break;
         case DEVICE_NYKO_PLAYPAD_PRO:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Nyko Playpad Pro",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_START] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBL] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBR] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_ONLIVE_WIRELESS_CONTROLLER:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Onlive Wireless",
                  sizeof(g_settings.input.device_names[port]));

            /* TODO - Does D-pad work as D-pad? */

            keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);

            keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);

            /* TODO - 
             * LT - find out value
             * RT - find out value
             */
            break;
         case DEVICE_GENIUS_MAXFIRE_G08XU:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Genius Maxfire G08XU",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_USB_2_AXIS_8_BUTTON_GAMEPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "USB 2 Axis 8 button",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_BUFFALO_BGC_FC801:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Buffalo BGC FC801",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_RETROUSB_RETROPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "RetroUSB NES",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_RETROUSB_SNES_RETROPORT:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "RetroUSB SNES",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_CYPRESS_USB:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Cypress USB",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L2]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_MAYFLASH_WII_CLASSIC:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "Mayflash Wii Classic",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

            keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_1] |= ((RETRO_DEVICE_ID_JOYPAD_A+1)  << shift);
            keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            break;
         case DEVICE_SZMY_POWER_DUAL_BOX_WII:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "SZMy Power Dual Box Wii",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_16] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

            keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_1] |= ((RETRO_DEVICE_ID_JOYPAD_X+1)  << shift);
            keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            break;
         case DEVICE_TOODLES_2008_CHIMP:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Toodles 2008 Chimp",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_Z] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_C] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            break;
         case DEVICE_ARCHOS_GAMEPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Archos Gamepad",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_SELECT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_JXD_S5110:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "JXD S5110",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_SPACE] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_ENTER] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
            keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            break;
         case DEVICE_OUYA:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "OUYA",
                  sizeof(g_settings.input.device_names[port]));

            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_DUALANALOG;
            keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBL] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBR] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            keycode_lut[AKEYCODE_MENU] |=  ((RARCH_MENU_TOGGLE+1)      << shift);
            break;
         case DEVICE_HOLTEK_JC_U912F:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Elecom JC-U912F",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_11] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            break;
         case DEVICE_XPERIA_PLAY:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Xperia Play",
                  sizeof(g_settings.input.device_names[port]));

            if ((zeus_second_id != -1 && (zeus_second_id == id)))
            {
               port = zeus_port;
               shift = 8 + (port * 8);
            }

            g_extern.lifecycle_mode_state |= (1ULL << MODE_INPUT_XPERIA_PLAY_HACK);

            keycode_lut[AKEYCODE_DPAD_CENTER] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_DPAD_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1) << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1) << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1) << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            keycode_lut[AKEYCODE_MENU] |=  ((RARCH_MENU_TOGGLE+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_SELECT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_BROADCOM_BLUETOOTH_HID:
            RARCH_LOG("Bluetooth HID\n");
            if ((g_settings.input.icade_count +1) < 4)
            {
               g_settings.input.device[port] = device;
               g_settings.input.icade_count++;
               RARCH_LOG("Using icade profile %u\n", g_settings.input.icade_count - 1);

               switch(g_settings.input.icade_profile[g_settings.input.icade_count - 1])  /* was just incremented... */
               {
                  case ICADE_PROFILE_RED_SAMURAI:
                     /* TODO: unsure about Select button here */
                     /* TODO: hookup right stick 
                      * RStick Up: 37
                      * RStick Down: 39
                      * RStick Left:38
                      * RStick Right: 40 */

                     /* Red Samurai */
                     strlcpy(g_settings.input.device_names[port], "Red Samurai",
                        sizeof(g_settings.input.device_names[port]));
                     keycode_lut[AKEYCODE_W]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
                     keycode_lut[AKEYCODE_S] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
                     keycode_lut[AKEYCODE_A] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
                     keycode_lut[AKEYCODE_D]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
                     keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
                     keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
                     keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
                     keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
                     keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
                     keycode_lut[AKEYCODE_ENTER] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
                     keycode_lut[AKEYCODE_9] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
                     keycode_lut[AKEYCODE_0] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
                     keycode_lut[AKEYCODE_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
                     keycode_lut[AKEYCODE_6] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
                     keycode_lut[AKEYCODE_7] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
                     keycode_lut[AKEYCODE_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);

                     /* unsure if the person meant the SNES-mapping here or whether it's the pad */ 
                     keycode_lut[AKEYCODE_1] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
                     keycode_lut[AKEYCODE_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
                     keycode_lut[AKEYCODE_3] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
                     keycode_lut[AKEYCODE_4] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
                     break;
                  case ICADE_PROFILE_IPEGA_PG9017:
                     strlcpy(g_settings.input.device_names[port], "iPega PG-9017",
                        sizeof(g_settings.input.device_names[port]));
                     /* This maps to SNES layout, not button labels on gamepad -- SNES layout has A to the right of B */
                     keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift); /* Button labeled X on gamepad */
                     keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift); /* Button labeled A on gamepad */
                     keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift); /* Button labeled B on gamepad */
                     keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift); /* Button labeled Y on gamepad */
                     keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
                     keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
                     keycode_lut[AKEYCODE_BUTTON_9]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
                     keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
                     /* These don't work, in gamepad mode the dpad sends AXIS_HAT_X and AXIS_HAT_Y motion events
                        instead of button events, so they get processed by engine_handle_dpad_getaxisvalue()  */
                     keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)     << shift);
                     keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)   << shift);
                     keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)   << shift);
                     keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)  << shift);
                     break;
                  case ICADE_PROFILE_IPEGA_PG9017_MODE2:
                     strlcpy(g_settings.input.device_names[port], "iPega PG-9017 (Mode2)",
                        sizeof(g_settings.input.device_names[port]));
                     keycode_lut[AKEYCODE_M]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
                     keycode_lut[AKEYCODE_J]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
                     keycode_lut[AKEYCODE_K]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
                     keycode_lut[AKEYCODE_I]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
                     keycode_lut[AKEYCODE_Q]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
                     keycode_lut[AKEYCODE_P]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
                     keycode_lut[AKEYCODE_R]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
                     keycode_lut[AKEYCODE_Y] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
                     keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)     << shift);
                     keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)   << shift);
                     keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)   << shift);
                     keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)  << shift);
                     break;
                  case ICADE_PROFILE_GAMESTOP_WIRELESS:
                     strlcpy(g_settings.input.device_names[port], "Gamestop Wireless",
                        sizeof(g_settings.input.device_names[port]));
                     keycode_lut[AKEYCODE_W] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
                     keycode_lut[AKEYCODE_S] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
                     keycode_lut[AKEYCODE_A] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
                     keycode_lut[AKEYCODE_D] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
                     keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
                     keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
                     keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
                     keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
                     keycode_lut[AKEYCODE_3] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
                     keycode_lut[AKEYCODE_4] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
                     keycode_lut[AKEYCODE_1] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
                     keycode_lut[AKEYCODE_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
                     keycode_lut[AKEYCODE_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
                     keycode_lut[AKEYCODE_7] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
                     keycode_lut[AKEYCODE_ESCAPE] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
                     keycode_lut[AKEYCODE_ENTER] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
                     keycode_lut[AKEYCODE_6] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
                     keycode_lut[AKEYCODE_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
                     keycode_lut[AKEYCODE_9] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
                     keycode_lut[AKEYCODE_0] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
                     break;
               }
            }
            break;
         case DEVICE_THRUST_PREDATOR:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Thrust Predator",
                  sizeof(g_settings.input.device_names[port]));
            /* TODO: L3/R3 */

            keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            keycode_lut[AKEYCODE_BUTTON_9]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            break;
         case DEVICE_DRAGONRISE:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "DragonRise",
                  sizeof(g_settings.input.device_names[port]));
            /* TODO: L3/R3 */

            keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            break;
         case DEVICE_TOMEE_NES_USB:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Tomee NES USB",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_R2]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_L2]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_A]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_B]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            break;
         case DEVICE_THRUSTMASTER_T_MINI:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Thrustmaster T Mini",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_R2]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_L2]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_A]|= ((RETRO_DEVICE_ID_JOYPAD_Y+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_X]|= ((RETRO_DEVICE_ID_JOYPAD_X+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_B]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_C]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]|= ((RETRO_DEVICE_ID_JOYPAD_L+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_Z]|= ((RETRO_DEVICE_ID_JOYPAD_L+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_L2]|= ((RETRO_DEVICE_ID_JOYPAD_L2+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_R1]|= ((RETRO_DEVICE_ID_JOYPAD_R2+1)    << shift);
            break;
         case DEVICE_DEFENDER_GAME_RACER_CLASSIC:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Defender Game Racer Classic",
                  sizeof(g_settings.input.device_names[port]));

            keycode_lut[AKEYCODE_BUTTON_10]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_1]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_2]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_3]|= ((RETRO_DEVICE_ID_JOYPAD_R2+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_4]|= ((RETRO_DEVICE_ID_JOYPAD_Y+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_5]|= ((RETRO_DEVICE_ID_JOYPAD_X+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_6]|= ((RETRO_DEVICE_ID_JOYPAD_L2+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_7]|= ((RETRO_DEVICE_ID_JOYPAD_L+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_8]|= ((RETRO_DEVICE_ID_JOYPAD_R+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_9]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            break;
         case DEVICE_MOGA_IME:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Moga IME",
                  sizeof(g_settings.input.device_names[port]));
            /* TODO:
             * right stick up: 188
             * right stick down: 189 
             * right stick left: 190
             * right stick right: 191
             */
            keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_START]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_SELECT]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_A]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_B]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_X]|= ((RETRO_DEVICE_ID_JOYPAD_Y+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]|= ((RETRO_DEVICE_ID_JOYPAD_X+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_L1]|= ((RETRO_DEVICE_ID_JOYPAD_L+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_R1]|= ((RETRO_DEVICE_ID_JOYPAD_R+1)    << shift);
            break;
         case DEVICE_NVIDIA_SHIELD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "NVIDIA Shield",
                  sizeof(g_settings.input.device_names[port]));
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_DUALANALOG;
            keycode_lut[AKEYCODE_BUTTON_B] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_A] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Y] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_X] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_START] |= ((RARCH_MENU_TOGGLE+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBL] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBR] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_CCPCREATIONS_WIIUSE_IME:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "ccpCreations WiiUse IME",
                  sizeof(g_settings.input.device_names[port]));
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;

            /* Player 1 */
            keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            keycode_lut[AKEYCODE_1]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_2]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_3]         |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_4]         |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            keycode_lut[AKEYCODE_5]         |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            keycode_lut[AKEYCODE_6]         |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)   << shift);
            keycode_lut[AKEYCODE_M]         |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)  << shift);
            keycode_lut[AKEYCODE_P]         |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            keycode_lut[AKEYCODE_E]         |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
            keycode_lut[AKEYCODE_B]         |= ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);
            keycode_lut[AKEYCODE_F]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
            keycode_lut[AKEYCODE_G]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
            keycode_lut[AKEYCODE_C]         |= ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            keycode_lut[AKEYCODE_LEFT_BRACKET]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1)       << shift);
            keycode_lut[AKEYCODE_RIGHT_BRACKET] |= ((RETRO_DEVICE_ID_JOYPAD_R2+1)       << shift);
            keycode_lut[AKEYCODE_Z]         |= ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
            keycode_lut[AKEYCODE_H]         |= ((RARCH_RESET+1)                    << shift);
            keycode_lut[AKEYCODE_W]         |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_S]         |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            keycode_lut[AKEYCODE_A]         |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            keycode_lut[AKEYCODE_D]         |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            keycode_lut[AKEYCODE_C]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            keycode_lut[AKEYCODE_Z]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);

            /* Player 2 */
            shift += 8;
            keycode_lut[AKEYCODE_I]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            keycode_lut[AKEYCODE_K] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            keycode_lut[AKEYCODE_J] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            keycode_lut[AKEYCODE_O]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            keycode_lut[AKEYCODE_COMMA] |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_PERIOD] |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_VOLUME_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            keycode_lut[AKEYCODE_VOLUME_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            keycode_lut[AKEYCODE_MEDIA_PREVIOUS] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            keycode_lut[AKEYCODE_MEDIA_NEXT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            keycode_lut[AKEYCODE_MEDIA_PLAY] |= ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            keycode_lut[AKEYCODE_MEDIA_STOP] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            keycode_lut[AKEYCODE_ENDCALL] |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            keycode_lut[AKEYCODE_CALL] |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            keycode_lut[AKEYCODE_PLUS] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            keycode_lut[AKEYCODE_MINUS] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            keycode_lut[AKEYCODE_BACKSLASH] |= ((RARCH_RESET+1)      << shift);
            keycode_lut[AKEYCODE_L] |= ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            keycode_lut[AKEYCODE_R] |= ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            keycode_lut[AKEYCODE_SEARCH] |= ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            keycode_lut[AKEYCODE_TAB] |= ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);

            /* Player 3 */
            shift += 8;
            keycode_lut[AKEYCODE_PAGE_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            keycode_lut[AKEYCODE_PAGE_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            keycode_lut[AKEYCODE_MEDIA_REWIND] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            keycode_lut[AKEYCODE_MEDIA_FAST_FORWARD]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            keycode_lut[AKEYCODE_SOFT_LEFT]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            keycode_lut[AKEYCODE_SOFT_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBR]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_THUMBL]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            keycode_lut[AKEYCODE_SPACE]|= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            keycode_lut[AKEYCODE_SYM]|= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            keycode_lut[AKEYCODE_EXPLORER]|= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            keycode_lut[AKEYCODE_ENVELOPE]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_X]|= ((RETRO_DEVICE_ID_JOYPAD_X+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_Y]|= ((RETRO_DEVICE_ID_JOYPAD_Y+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_A]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_B]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_L1]|= ((RETRO_DEVICE_ID_JOYPAD_L+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_R1]|= ((RETRO_DEVICE_ID_JOYPAD_R+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_L2]|= ((RETRO_DEVICE_ID_JOYPAD_L2+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_R2]|= ((RETRO_DEVICE_ID_JOYPAD_R2+1)    << shift);

            /* Player 4 */
            shift += 8;
            keycode_lut[AKEYCODE_N]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            keycode_lut[AKEYCODE_Q] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            keycode_lut[AKEYCODE_T] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            keycode_lut[AKEYCODE_APOSTROPHE]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            keycode_lut[AKEYCODE_NOTIFICATION]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            keycode_lut[AKEYCODE_MUTE]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_START]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            keycode_lut[AKEYCODE_BUTTON_SELECT]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            keycode_lut[AKEYCODE_CLEAR]|= ((RARCH_RESET+1)    << shift);
            keycode_lut[AKEYCODE_CAPS_LOCK]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            keycode_lut[AKEYCODE_SCROLL_LOCK] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            //keycode_lut[AKEYCODE_T] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift); -- Left meta
            //keycode_lut[AKEYCODE_APOSTROPHE]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift); -- right meta
            keycode_lut[AKEYCODE_META_FUNCTION_ON] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            keycode_lut[AKEYCODE_SYSRQ] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            keycode_lut[AKEYCODE_BREAK] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            keycode_lut[AKEYCODE_MOVE_HOME] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_C] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            keycode_lut[AKEYCODE_BUTTON_Z] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            keycode_lut[AKEYCODE_GRAVE] |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            keycode_lut[AKEYCODE_MEDIA_PAUSE] |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            break;
         case DEVICE_NONE:
         default:
            g_settings.input.device[port] = 0;
            strlcpy(g_settings.input.device_names[port], "Unknown",
                  sizeof(g_settings.input.device_names[port]));
            break;
      }

      keycode_lut[AKEYCODE_MENU] |= ((RARCH_MENU_TOGGLE + 1) << shift);
   }
}

// Handle all events. If our activity is in pause state, block until we're unpaused.

static void android_input_poll(void *data)
{
   int ident;
   uint64_t lifecycle_mask = (1ULL << RARCH_RESET) | (1ULL << RARCH_REWIND) | (1ULL << RARCH_FAST_FORWARD_KEY) | (1ULL << RARCH_FAST_FORWARD_HOLD_KEY) | (1ULL << RARCH_MUTE) | (1ULL << RARCH_SAVE_STATE_KEY) | (1ULL << RARCH_LOAD_STATE_KEY) | (1ULL << RARCH_STATE_SLOT_PLUS) | (1ULL << RARCH_STATE_SLOT_MINUS) | (1ULL << RARCH_QUIT_KEY) | (1ULL << RARCH_MENU_TOGGLE);
   uint64_t *lifecycle_state = &g_extern.lifecycle_state;
   *lifecycle_state &= ~lifecycle_mask;

   while ((ident = ALooper_pollAll((input_key_pressed_func(RARCH_PAUSE_TOGGLE)) ? -1 : 0,
               NULL, NULL, NULL)) >= 0)
   {
      if (ident == LOOPER_ID_INPUT)
      {
         bool debug_enable = g_settings.input.debug_enable;
         struct android_app *android_app = (struct android_app*)g_android;
         AInputEvent* event = NULL;

         // Read all pending events.
         while (AInputQueue_hasEvents(android_app->inputQueue))
         {
            int processed = 0;
            while (AInputQueue_getEvent(android_app->inputQueue, &event) >= 0)
            {
               bool long_msg_enable = false;
               int32_t handled = 1;
               int action = 0;
               char msg[128];
               int source, id, keycode, type_event, state_id;
               int predispatched;

               msg[0] = 0;
               predispatched = AInputQueue_preDispatchEvent(android_app->inputQueue,event);

               if (predispatched)
                  continue;

               source = AInputEvent_getSource(event);
               id = AInputEvent_getDeviceId(event);
               if (id == zeus_second_id)
                  id = zeus_id;
               keycode = AKeyEvent_getKeyCode(event);

               type_event = AInputEvent_getType(event);
               state_id = -1;

               if (source & (AINPUT_SOURCE_TOUCHSCREEN | AINPUT_SOURCE_MOUSE | AINPUT_SOURCE_TOUCHPAD))
                  state_id = 0; // touch overlay is always player 1
               else
               {
                  for (unsigned i = 0; i < pads_connected; i++)
                     if (state_device_ids[i] == id)
                        state_id = i;
               }

               if (state_id < 0)
               {
                  state_id = pads_connected;
                  if (g_settings.input.autodetect_enable)
                  {
                     bool primary = false;
                     input_autodetect_setup(android_app, msg, sizeof(msg), state_id, id, source, &primary);

                     if (primary)
                     {
                        RARCH_LOG("Found primary input device.\n");
                        memmove(state_device_ids + 1, state_device_ids, pads_connected * sizeof(state_device_ids[0]));
                        state_id = 0;
                        state_device_ids[0] = id;
                        pads_connected++;
                     }
                     else
                        state_device_ids[pads_connected++] = id;
                  }
                  else
                     state_device_ids[pads_connected++] = id;

                  long_msg_enable = true;
               }

               if (keycode == AKEYCODE_BACK)
               {
                  uint8_t unpacked = (keycode_lut[AKEYCODE_BACK] >> ((state_id+1) << 3)) - 1;
                  uint64_t input_state = (1ULL << unpacked);

                  if (g_extern.lifecycle_mode_state & (1ULL << MODE_INPUT_XPERIA_PLAY_HACK))
                  {
                     int meta = AKeyEvent_getMetaState(event);
                     if (!(meta & AMETA_ALT_ON))
                     {
                        *lifecycle_state |= (1ULL << RARCH_QUIT_KEY);
                        AInputQueue_finishEvent(android_app->inputQueue, event, handled);
                        break;
                     }
                  }
                  else if (type_event == AINPUT_EVENT_TYPE_KEY && input_state < (1ULL << RARCH_FIRST_META_KEY)
                        && input_state > 0)
                  {
                  }
                  else if (g_settings.input.back_behavior == BACK_BUTTON_MENU_TOGGLE)
                  {
                     int action = AKeyEvent_getAction(event);
                     if (action == AKEY_EVENT_ACTION_DOWN)
                        *lifecycle_state |= (1ULL << RARCH_MENU_TOGGLE);
                     else if (action == AKEY_EVENT_ACTION_UP)
                        *lifecycle_state &= ~(1ULL << RARCH_MENU_TOGGLE);
                     AInputQueue_finishEvent(android_app->inputQueue, event, handled);
                     break;
                  }
                  else
                  {
                     // exits the app, so no need to check for up/down action
                     *lifecycle_state |= (1ULL << RARCH_QUIT_KEY);
                     AInputQueue_finishEvent(android_app->inputQueue, event, handled);
                     break;
                  }
               }

               if (type_event == AINPUT_EVENT_TYPE_MOTION)
               {
                  action = AMotionEvent_getAction(event);
                  size_t motion_pointer = action >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
                  action &= AMOTION_EVENT_ACTION_MASK;

                  if (source & ~(AINPUT_SOURCE_TOUCHSCREEN | AINPUT_SOURCE_MOUSE))
                  {
                     if (g_settings.input.dpad_emulation[state_id] != ANALOG_DPAD_NONE)
                        engine_handle_dpad(event, motion_pointer, state_id, msg, sizeof(msg), source, debug_enable,
                              g_settings.input.dpad_emulation[state_id]);
                  }
                  else
                  {
                     float x = 0.0f;
                     float y = 0.0f;
                     bool keyup = (action == AMOTION_EVENT_ACTION_UP ||
                           action == AMOTION_EVENT_ACTION_CANCEL || action == AMOTION_EVENT_ACTION_POINTER_UP) ||
                        (source == AINPUT_SOURCE_MOUSE && action != AMOTION_EVENT_ACTION_DOWN);

                     if (keyup && motion_pointer < MAX_TOUCH)
                     {
                        memmove(pointer + motion_pointer, pointer + motion_pointer + 1, (MAX_TOUCH - motion_pointer - 1) * sizeof(struct input_pointer));
                        if (pointer_count > 0)
                           pointer_count--;
                     }
                     else
                     {
                        int pointer_max = min(AMotionEvent_getPointerCount(event), MAX_TOUCH);
                        for (motion_pointer = 0; motion_pointer < pointer_max; motion_pointer++)
                        {
                           x = AMotionEvent_getX(event, motion_pointer);
                           y = AMotionEvent_getY(event, motion_pointer);

                           input_translate_coord_viewport(x, y,
                                 &pointer[motion_pointer].x, &pointer[motion_pointer].y,
                                 &pointer[motion_pointer].full_x, &pointer[motion_pointer].full_y);

                           pointer_count = max(pointer_count, motion_pointer + 1);
                        }
                     }
                     if (debug_enable)
                        snprintf(msg, sizeof(msg), "Pad %d : x = %.2f, y = %.2f, src %d.\n", state_id, x, y, source);
                  }

               }
               else if (type_event == AINPUT_EVENT_TYPE_KEY)
               {
                  /* Hack - we have to decrease the unpacked value by 1
                   * because we 'added' 1 to each entry in the LUT -
                   * RETRO_DEVICE_ID_JOYPAD_B is 0
                   */
                  uint8_t unpacked = (keycode_lut[keycode] >> ((state_id+1) << 3)) - 1;
                  uint64_t input_state = (1ULL << unpacked);
                  int action  = AKeyEvent_getAction(event);
                  uint64_t *key = NULL;

                  if (debug_enable)
                     snprintf(msg, sizeof(msg), "Pad %d : %d, ac = %d, src = %d.\n", state_id, keycode, action, source);

                  if (input_state < (1ULL << RARCH_FIRST_META_KEY))
                     key = &state[state_id];
                  else if (input_state/* && action == AKEY_EVENT_ACTION_DOWN*/)
                     key = &g_extern.lifecycle_state;

                  if (key != NULL)
                  {
                     // some controllers send both the up and down events at once when the button is released for "special" buttons, like menu buttons
                     // work around that by only using down events for meta keys (which get cleared every poll anyway)
                     if (action == AKEY_EVENT_ACTION_UP && !(input_state & lifecycle_mask))
                        *key &= ~(input_state);
                     else if (action == AKEY_EVENT_ACTION_DOWN)
                        *key |= input_state;
                  }

                  if ((keycode == AKEYCODE_VOLUME_UP || keycode == AKEYCODE_VOLUME_DOWN) && keycode_lut[keycode] == 0)
                     handled = 0;
               }

               if (msg[0] != 0)
               {
                  msg_queue_clear(g_extern.msg_queue);
                  msg_queue_push(g_extern.msg_queue, msg, 0, long_msg_enable ? 180 : 30);
                  RARCH_LOG("Input debug: %s\n", msg);
               }

               AInputQueue_finishEvent(android_app->inputQueue, event, handled);
               processed = 1;
            }
            if (processed == 0)
               RARCH_WARN("Failure reading next input event: %s\n", strerror(errno));
         }
      }
      else if (ident == LOOPER_ID_MAIN)
         engine_handle_cmd();
   }
}

static int16_t android_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return ((state[port] & binds[port][id].joykey) && (port < pads_connected));
      case RETRO_DEVICE_ANALOG:
         if (port >= pads_connected)
            return 0;
         switch ((index << 1) | id)
         {
            case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_X:
               return analog_state[port].lx;
            case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_Y:
               return analog_state[port].ly;
            case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_X:
               return analog_state[port].rx;
            case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_Y:
               return analog_state[port].ry;
         }
      case RETRO_DEVICE_POINTER:
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return pointer[index].x;
            case RETRO_DEVICE_ID_POINTER_Y:
               return pointer[index].y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return (index < pointer_count) && (pointer[index].x != -0x8000) && (pointer[index].y != -0x8000);
            default:
               return 0;
         }
      case RARCH_DEVICE_POINTER_SCREEN:
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return pointer[index].full_x;
            case RETRO_DEVICE_ID_POINTER_Y:
               return pointer[index].full_y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return (index < pointer_count) && (pointer[index].full_x != -0x8000) && (pointer[index].full_y != -0x8000);
            default:
               return 0;
         }
      default:
         return 0;
   }
}

static bool android_input_key_pressed(void *data, int key)
{
   return ((g_extern.lifecycle_state | driver.overlay_state) & (1ULL << key));
}

static void android_input_free_input(void *data)
{
   (void)data;
}

const input_driver_t input_android = {
   android_input_init,
   android_input_poll,
   android_input_state,
   android_input_key_pressed,
   android_input_free_input,
   android_input_set_keybinds,
   "android_input",
};
