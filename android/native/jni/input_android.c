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
#include "../../../thread.h"

#define MAX_TOUCH 16

enum input_msg
{
   INPUT_MSG_UPDATE_INPUT_QUEUE = 0,
   INPUT_MSG_DIE,
};

static void event_handling_thread(void *data);

typedef struct
{
   float dzone_min;
   float dzone_max;
} dpad_values_t;

struct input_pointer
{
   int16_t x, y;
   int16_t full_x, full_y;
};

struct android_input_data
{
   struct input_pointer pointer[MAX_TOUCH];
   unsigned pointer_count;
   uint64_t state[MAX_PADS];
   unsigned pads_connected;
   uint64_t lifecycle_state;
};

struct android_input_state
{
   int msgpipe[2];

   sthread_t *input_thread;
   slock_t *lock;

   struct android_input_data thread, copy;
   char msg[512];
   unsigned msg_time;

   int state_device_ids[MAX_PADS];
   uint64_t keycode_lut[LAST_KEYCODE];
   dpad_values_t dpad_state[MAX_PADS]; 
};

enum
{
   AXIS_X = 0,
   AXIS_Y = 1,
   AXIS_Z = 11,
   AXIS_RZ = 14
};

#define PRESSED_UP(x, y)    ((y <= dzone_min))
#define PRESSED_DOWN(x, y)  ((y >= dzone_max))
#define PRESSED_LEFT(x, y)  ((x <= dzone_min))
#define PRESSED_RIGHT(x, y) ((x >= dzone_max))

static void (*engine_handle_dpad)(struct android_input_state *,
      AInputEvent*, size_t, int, char*, size_t, int, bool);
static float (*p_AMotionEvent_getAxisValue)(const AInputEvent* motion_event,
      int32_t axis, size_t pointer_index);
#define AMotionEvent_getAxisValue (*p_AMotionEvent_getAxisValue)

static void set_input_queue(ALooper *looper, AInputQueue *pending)
{
   struct android_app *android_app = (struct android_app*)g_android;
   pthread_mutex_lock(&android_app->mutex);

   if (android_app->inputQueue != NULL)
      AInputQueue_detachLooper(android_app->inputQueue);

   android_app->inputQueue = pending;

   if (android_app->inputQueue != NULL && looper)
   {
      RARCH_LOG("Attaching input queue to looper");
      AInputQueue_attachLooper(android_app->inputQueue,
            looper, LOOPER_ID_INPUT, NULL,
            NULL);
   }

   pthread_cond_broadcast(&android_app->cond);
   pthread_mutex_unlock(&android_app->mutex);
}

/**
 * Process the next main command.
 */
void engine_handle_cmd(void *data)
{
   struct android_input_state *thr = (struct android_input_state*)data;
   struct android_app *android_app = (struct android_app*)g_android;
   int8_t cmd;

   if (read(android_app->msgread, &cmd, sizeof(cmd)) != sizeof(cmd))
      cmd = -1;

   switch (cmd)
   {
      case APP_CMD_INPUT_CHANGED:
      {
         int msg = INPUT_MSG_UPDATE_INPUT_QUEUE;
         if (thr)
            write(thr->msgpipe[1], &msg, sizeof(msg));
         else
            set_input_queue(NULL, android_app->pendingInputQueue);
         break;
      }

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

static void engine_handle_dpad_default(struct android_input_state *thr, AInputEvent *event,
      size_t motion_pointer, int state_id, char *msg, size_t msg_sizeof,
      int source, bool debug_enable)
{
   uint64_t *state_cur = &thr->thread.state[state_id];
   float dzone_min = thr->dpad_state[state_id].dzone_min;
   float dzone_max = thr->dpad_state[state_id].dzone_max;
   float x = AMotionEvent_getX(event, motion_pointer);
   float y = AMotionEvent_getY(event, motion_pointer);

   *state_cur &= ~((1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) | 
         (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) | (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) |
         (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN));
   *state_cur |= PRESSED_LEFT(x, y)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)  : 0;
   *state_cur |= PRESSED_RIGHT(x, y) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
   *state_cur |= PRESSED_UP(x, y)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)    : 0;
   *state_cur |= PRESSED_DOWN(x, y)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)  : 0;

   if (debug_enable)
      snprintf(msg, msg_sizeof, "Pad %d : x = %.2f, y = %.2f, src %d.\n",
            state_id, x, y, source);
}

static void engine_handle_dpad_getaxisvalue(struct android_input_state *thr, AInputEvent *event,
      size_t motion_pointer, int state_id, char *msg, size_t msg_sizeof, int source,
      bool debug_enable)
{
   uint64_t *state_cur = &thr->thread.state[state_id];
   float dzone_min = thr->dpad_state[state_id].dzone_min;
   float dzone_max = thr->dpad_state[state_id].dzone_max;
   float x = AMotionEvent_getAxisValue(event, AXIS_X, motion_pointer);
   float y = AMotionEvent_getAxisValue(event, AXIS_Y, motion_pointer);
   float z = AMotionEvent_getAxisValue(event, AXIS_Z, motion_pointer);
   float rz = AMotionEvent_getAxisValue(event, AXIS_RZ, motion_pointer);

   *state_cur &= ~((1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) | 
         (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) | (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) |
         (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN));
   *state_cur |= PRESSED_LEFT(x, y)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)  : 0;
   *state_cur |= PRESSED_RIGHT(x, y) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
   *state_cur |= PRESSED_UP(x, y)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)    : 0;
   *state_cur |= PRESSED_DOWN(x, y)  ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)  : 0;

   if (debug_enable)
      snprintf(msg, msg_sizeof, "Pad %d : x %.2f, y %.2f, z %.2f, rz %.2f, src %d.\n", 
            state_id, x, y, z, rz, source);
}

static void *android_input_init(void)
{
   struct android_input_state *thr = (struct android_input_state*)calloc(1, sizeof(*thr));
   if (!thr)
      return NULL;

   for (unsigned j = 0; j < LAST_KEYCODE; j++)
      thr->keycode_lut[j] = 0;

   if (!g_settings.input.autodetect_enable)
   {
      for (unsigned j = 0; j < MAX_PADS; j++)
      {
         uint8_t shift = 8 + (j * 8);
         for (unsigned k = 0; k < RARCH_FIRST_CUSTOM_BIND; k++)
         {
            if (g_settings.input.binds[j][k].valid &&
                  g_settings.input.binds[j][k].joykey &&
                  g_settings.input.binds[j][k].joykey < LAST_KEYCODE)
            {
               RARCH_LOG("binding %llu to %d (p%d)\n", g_settings.input.binds[j][k].joykey, k, j);
               thr->keycode_lut[g_settings.input.binds[j][k].joykey] |= ((k + 1) << shift);
            }
         }
      }
   }

   for (unsigned i = 0; i < MAX_PADS; i++)
   {
      for (unsigned j = 0; j < RARCH_FIRST_META_KEY; j++)
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

      thr->dpad_state[i].dzone_min = -0.99f;
      thr->dpad_state[i].dzone_max = 0.99f;
      g_settings.input.dpad_emulation[i] = ANALOG_DPAD_LSTICK;
   }

   for (unsigned i = 0; i < MAX_PLAYERS; i++)
      strlcpy(g_settings.input.device_names[i], "Custom", sizeof(g_settings.input.device_names[i]));

   if ((dlopen("/system/lib/libandroid.so", RTLD_LOCAL | RTLD_LAZY)) == 0)
   {
      RARCH_WARN("Unable to open libandroid.so\n");
      engine_handle_dpad = engine_handle_dpad_default;
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

   pipe(thr->msgpipe);
   thr->lock = slock_new();
   thr->input_thread = sthread_create(event_handling_thread, thr);

   return thr;
}

int zeus_id = -1;
int zeus_second_id = -1;
static unsigned zeus_port;

static void android_input_set_keybinds(void *data, unsigned device,
      unsigned port, unsigned id, unsigned keybind_action)
{
   struct android_input_state *thr = (struct android_input_state*)data;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS))
   {
      /* eight 8-bit values are packed into one uint64_t
       * one for each of the 8 pads */
      unsigned shift = 8 + (port * 8);

      g_settings.input.dpad_emulation[port] = ANALOG_DPAD_LSTICK;

      // NOTE - we have to add '1' to the bit mask because
      // RETRO_DEVICE_ID_JOYPAD_B is 0

      switch (device)
      {
         case DEVICE_LOGITECH_RUMBLEPAD2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Logitech Rumblepad 2",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_9]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)  << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)       << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)       << shift);
            break;
         case DEVICE_LOGITECH_DUAL_ACTION:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Logitech Dual Action",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_A]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L2]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R2]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_THUMBL]  |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_THUMBR]  |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_SELECT]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)  << shift);
            thr->keycode_lut[AKEYCODE_BACK]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)           << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_ICONTROLPAD_BLUEZ_IME:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "iControlpad Bluez IME",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)        << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)  << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)        << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)        << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)        << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)        << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_START] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)   << shift);

            /* left analog stick */
            thr->keycode_lut[AKEYCODE_W] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)              << shift);
            thr->keycode_lut[AKEYCODE_S] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)            << shift);
            thr->keycode_lut[AKEYCODE_A] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)            << shift);
            thr->keycode_lut[AKEYCODE_D] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)           << shift);

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
            thr->dpad_state[id].dzone_min = -2.00f;
            thr->dpad_state[id].dzone_max = 1.00f;


            /* same as Rumblepad 2 - merge? */
            //thr->keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);

            thr->keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_9] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);

            /* TODO - unsure about pad 2 still */
#if 0
            thr->keycode_lut[AKEYCODE_BUTTON_Z]  |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y]  |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_C]  |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X]  |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

            thr->keycode_lut[AKEYCODE_BUTTON_11]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_12]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_13]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_14]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_15]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_16]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_C]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
#endif
            break;
         case DEVICE_TOMMO_NEOGEOX_ARCADE:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "TOMMO Neogeo X Arcade",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_DPAD_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1) << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1) << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1) << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_C] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L2] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            break;
         case DEVICE_MADCATZ_PC_USB_STICK:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Madcatz PC USB Stick",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_A] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_C] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Z] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift); /* request hack */
            thr->keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift); /* request hack */
            thr->keycode_lut[AKEYCODE_BUTTON_L2] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_MODE] |= ((RARCH_QUIT_KEY+1) << shift);
            break;
         case DEVICE_LOGICOOL_RUMBLEPAD2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Logicool Rumblepad 2",
                  sizeof(g_settings.input.device_names[port]));
            
            // Rumblepad 2 DInput */
            /* TODO: Add L3/R3 */
            thr->keycode_lut[AKEYCODE_BUTTON_B]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_C]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);

            thr->keycode_lut[AKEYCODE_BUTTON_X]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);

            thr->keycode_lut[AKEYCODE_BUTTON_L2]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);

            thr->keycode_lut[AKEYCODE_BUTTON_Y]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Z]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            break;
         case DEVICE_IDROID_X360:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "iDroid x360",
                  sizeof(g_settings.input.device_names[port]));

            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            thr->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            thr->keycode_lut[AKEYCODE_INSERT] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_MINUS] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_SLASH] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_MOVE_END] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_LEFT_BRACKET] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_FORWARD_DEL] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_MEDIA_PLAY] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            thr->keycode_lut[AKEYCODE_EQUALS] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_MENU] |=  ((RARCH_MENU_TOGGLE+1)      << shift);
            // Left Analog Up: 152
            // Left Analog Down: 146
            // Left Analog Right: 150
            // Left Analog Left: 148
            // Right Analog Up: 92 (AKEYCODE_PAGE_UP)
            // Right Analog Down: 93 (AKEYCODE_PAGE_DOWN)
            // Right Analog Right: 113 (AKEYCODE_CTRL_LEFT)
            // Right Analog Left: 72 (AKEYCODE_RIGHT_BRACKET)
            thr->keycode_lut[AKEYCODE_NUMPAD_8] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_NUMPAD_2] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_NUMPAD_6] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            thr->keycode_lut[AKEYCODE_NUMPAD_4] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            break;
         case DEVICE_ZEEMOTE_STEELSERIES:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Zeemote Steelseries",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_A]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_MODE]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            break;
         case DEVICE_HUIJIA_USB_SNES:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Huijia USB SNES",
                  sizeof(g_settings.input.device_names[port]));
            thr->dpad_state[id].dzone_min = -1.00f;
            thr->dpad_state[id].dzone_max = 1.00f;

            thr->keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_9] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            break;
         case DEVICE_SUPER_SMARTJOY:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Super Smartjoy",
                  sizeof(g_settings.input.device_names[port]));
            thr->dpad_state[id].dzone_min = -1.00f;
            thr->dpad_state[id].dzone_max = 1.00f;

            thr->keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            break;
         case DEVICE_SAITEK_RUMBLE_P480:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Saitek Rumble P480",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_9]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_MS_SIDEWINDER_DUAL_STRIKE:
            /* TODO - unfinished */
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "MS Sidewinder Dual Strike",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_4] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_2] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_3] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1] |= ((RETRO_DEVICE_ID_JOYPAD_X) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7] |= ((RETRO_DEVICE_ID_JOYPAD_L) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_R) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_9] |= ((RETRO_DEVICE_ID_JOYPAD_L2) << shift);
            break;
         case DEVICE_MS_SIDEWINDER:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "MS Sidewinder",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_R2] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L2] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_11] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_12] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            break;
         case DEVICE_MS_XBOX:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Xbox",
                  sizeof(g_settings.input.device_names[port]));

            /* TODO: left and right triggers for Xbox 1*/
            thr->keycode_lut[AKEYCODE_BUTTON_SELECT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_THUMBL]  |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_THUMBR]  |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            break;
         case DEVICE_WISEGROUP_PLAYSTATION2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "WiseGroup PlayStation2",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_16] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_11] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_JCPS102_PLAYSTATION2:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "JCPS102 PlayStation2",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_16] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_11] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_GENERIC_PLAYSTATION2_CONVERTER:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Generic PlayStation2",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_16] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_11] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_KEYBOARD_RETROPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Generic Keyboard",
                  sizeof(g_settings.input.device_names[port]));

            // Keyboard
            // TODO: Map L2/R2/L3/R3
            thr->keycode_lut[AKEYCODE_Z] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_A] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_SHIFT_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_ENTER] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            thr->keycode_lut[AKEYCODE_DPAD_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1) << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1) << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1) << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1) << shift);
            thr->keycode_lut[AKEYCODE_X] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_S] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_Q] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_W] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);

            /* Misc control scheme */
            thr->keycode_lut[AKEYCODE_F1] |= ((RARCH_MENU_TOGGLE + 1) << shift);
            thr->keycode_lut[AKEYCODE_F2] |= ((RARCH_SAVE_STATE_KEY+1) << shift);
            thr->keycode_lut[AKEYCODE_F4] |= ((RARCH_LOAD_STATE_KEY+1) << shift);
            thr->keycode_lut[AKEYCODE_F7] |= ((RARCH_STATE_SLOT_PLUS+1) << shift);
            thr->keycode_lut[AKEYCODE_F6] |= ((RARCH_STATE_SLOT_MINUS+1) << shift);
            thr->keycode_lut[AKEYCODE_SPACE] |= ((RARCH_FAST_FORWARD_KEY+1) << shift);
            thr->keycode_lut[AKEYCODE_L] |= ((RARCH_FAST_FORWARD_HOLD_KEY+1) << shift);
            thr->keycode_lut[AKEYCODE_BREAK] |= ((RARCH_PAUSE_TOGGLE+1) << shift);
            thr->keycode_lut[AKEYCODE_K] |= ((RARCH_FRAMEADVANCE+1) << shift);
            thr->keycode_lut[AKEYCODE_H] |= ((RARCH_RESET+1) << shift);
            thr->keycode_lut[AKEYCODE_R] |= ((RARCH_REWIND+1) << shift);
            thr->keycode_lut[AKEYCODE_F9] |= ((RARCH_MUTE+1) << shift);

            thr->keycode_lut[AKEYCODE_ESCAPE] |= ((RARCH_QUIT_KEY+1) << shift);
            break;
         case DEVICE_PLAYSTATION3_VERSION1:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "PlayStation3 Ver.1",
                  sizeof(g_settings.input.device_names[port]));

            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            thr->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_THUMBL] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_THUMBR] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1] |= ((RARCH_MENU_TOGGLE+1)     << shift);
            break;
         case DEVICE_PLAYSTATION3_VERSION2:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "PlayStation3 Ver.2",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1] |= ((RARCH_MENU_TOGGLE+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_THUMBL] |= ((RETRO_DEVICE_ID_JOYPAD_L3+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_THUMBR] |= ((RETRO_DEVICE_ID_JOYPAD_R3+1)     << shift);
            break;
         case DEVICE_MOGA:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "MOGA",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_SELECT] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            break;
         case DEVICE_PSMOVE_NAVI:
            /* TODO - unfinished */
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "PS Move Navi",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);

            thr->keycode_lut[AKEYCODE_BUTTON_11]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_9]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_15]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_14]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            thr->keycode_lut[AKEYCODE_UNKNOWN]   |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_JXD_S7300B:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "JXD S7300B",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

            thr->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);

            thr->keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L2]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R2]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);

            thr->keycode_lut[AKEYCODE_ENTER]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            thr->keycode_lut[AKEYCODE_SPACE]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);

            //thr->keycode_lut[AKEYCODE_VOLUME_UP]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            //thr->keycode_lut[AKEYCODE_VOLUME_DOWN]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            break;
         case DEVICE_IDROID_CON:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "i.droid",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_9]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_10]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_11]  |= ((RETRO_DEVICE_ID_JOYPAD_L3+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_12]  |= ((RETRO_DEVICE_ID_JOYPAD_R3+1) << shift);
            break;
         case DEVICE_NYKO_PLAYPAD_PRO:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Nyko Playpad Pro",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_START] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            thr->keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_THUMBL] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_THUMBR] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
            break;
         case DEVICE_GENIUS_MAXFIRE_G08XU:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Genius Maxfire G08XU",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_USB_2_AXIS_8_BUTTON_GAMEPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "USB 2 Axis 8 button",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_BUFFALO_BGC_FC801:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Buffalo BGC FC801",
                  sizeof(g_settings.input.device_names[port]));
            thr->dpad_state[id].dzone_min = -1.00f;
            thr->dpad_state[id].dzone_max = 1.00f;

            thr->keycode_lut[AKEYCODE_BUTTON_1]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_2]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_3]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_RETROUSB_RETROPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "RetroUSB NES",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_RETROUSB_SNES_RETROPORT:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "RetroUSB SNES",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_CYPRESS_USB:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Cypress USB",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_A]  |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B]  |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_C]  |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X]  |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y]  |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Z]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1]  |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1]  |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L2]  |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_MAYFLASH_WII_CLASSIC:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "Mayflash Wii Classic",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_12] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

            thr->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1] |= ((RETRO_DEVICE_ID_JOYPAD_A+1)  << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            break;
         case DEVICE_SZMY_POWER_DUAL_BOX_WII:
            g_settings.input.device[port] = device;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            strlcpy(g_settings.input.device_names[port], "SZMy Power Dual Box Wii",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_13] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_15] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_16] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_14] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);

            thr->keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1] |= ((RETRO_DEVICE_ID_JOYPAD_X+1)  << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            break;
         case DEVICE_TOODLES_2008_CHIMP:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Toodles 2008 Chimp",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Z] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_C] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            break;
         case DEVICE_ARCHOS_GAMEPAD:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Archos Gamepad",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L2] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R2] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_SELECT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_JXD_S5110:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "JXD S5110",
                  sizeof(g_settings.input.device_names[port]));

            thr->keycode_lut[AKEYCODE_DPAD_UP] |=  ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |=  ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |=  ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT] |=  ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_SPACE] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_ENTER] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)  << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            break;
         case DEVICE_XPERIA_PLAY:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Xperia Play",
                  sizeof(g_settings.input.device_names[port]));

            if (zeus_id < 0)
            {
               RARCH_LOG("zeus_pad 1 detected: %d\n", id);
               zeus_id = id;
               zeus_port = port;
            }
            else
            {
               RARCH_LOG("zeus_pad 2 detected: %d\n", id);
               zeus_second_id = id;
               port = zeus_port;
               shift = 8 + (port * 8);
            }

            g_extern.lifecycle_mode_state |= (1ULL << MODE_INPUT_XPERIA_PLAY_HACK);

            thr->keycode_lut[AKEYCODE_DPAD_CENTER] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_DPAD_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1) << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1) << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1) << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            thr->keycode_lut[AKEYCODE_MENU] |=  ((RARCH_MENU_TOGGLE+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_SELECT] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_START] |= ((RETRO_DEVICE_ID_JOYPAD_START+1) << shift);
            break;
         case DEVICE_BROADCOM_BLUETOOTH_HID:
            if ((g_settings.input.icade_count +1) < 4)
            {
               g_settings.input.device[port] = device;
               strlcpy(g_settings.input.device_names[port], "Broadcom Bluetooth HID",
                     sizeof(g_settings.input.device_names[port]));
               g_settings.input.icade_count++;

               switch(g_settings.input.icade_profile[g_settings.input.icade_count])
               {
                  case ICADE_PROFILE_RED_SAMURAI:
                     /* TODO: unsure about Select button here */
                     /* TODO: hookup right stick 
                      * RStick Up: 37
                      * RStick Down: 39
                      * RStick Left:38
                      * RStick Right: 40 */

                     /* Red Samurai */
                     thr->keycode_lut[AKEYCODE_W]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
                     thr->keycode_lut[AKEYCODE_S] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
                     thr->keycode_lut[AKEYCODE_A] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
                     thr->keycode_lut[AKEYCODE_D]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
                     thr->keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
                     thr->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
                     thr->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
                     thr->keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
                     thr->keycode_lut[AKEYCODE_BACK] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
                     thr->keycode_lut[AKEYCODE_ENTER] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
                     thr->keycode_lut[AKEYCODE_9] |=  ((RETRO_DEVICE_ID_JOYPAD_L3+1)      << shift);
                     thr->keycode_lut[AKEYCODE_0] |=  ((RETRO_DEVICE_ID_JOYPAD_R3+1)      << shift);
                     thr->keycode_lut[AKEYCODE_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
                     thr->keycode_lut[AKEYCODE_6] |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
                     thr->keycode_lut[AKEYCODE_7] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
                     thr->keycode_lut[AKEYCODE_8] |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);

                     /* unsure if the person meant the SNES-mapping here or whether it's the pad */ 
                     thr->keycode_lut[AKEYCODE_1] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
                     thr->keycode_lut[AKEYCODE_2] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
                     thr->keycode_lut[AKEYCODE_3] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
                     thr->keycode_lut[AKEYCODE_4] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
                     break;
                  case ICADE_PROFILE_IPEGA_PG9017:
                     /* Todo: diagonals - patchy? */
                     thr->keycode_lut[AKEYCODE_BUTTON_1] |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
                     thr->keycode_lut[AKEYCODE_BUTTON_2] |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
                     thr->keycode_lut[AKEYCODE_BUTTON_3] |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
                     thr->keycode_lut[AKEYCODE_BUTTON_4] |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
                     thr->keycode_lut[AKEYCODE_BUTTON_5] |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
                     thr->keycode_lut[AKEYCODE_BUTTON_6] |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
                     thr->keycode_lut[AKEYCODE_BUTTON_9] |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
                     thr->keycode_lut[AKEYCODE_BUTTON_10] |=  ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
                     break;
               }
            }
            break;
         case DEVICE_THRUST_PREDATOR:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "Thrust Predator",
                  sizeof(g_settings.input.device_names[port]));
            /* TODO: L3/R3 */

            thr->keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_R2+1)     << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_9]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_10] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            break;
         case DEVICE_DRAGONRISE:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "DragonRise",
                  sizeof(g_settings.input.device_names[port]));
            /* TODO: L3/R3 */

            thr->keycode_lut[AKEYCODE_BUTTON_2]  |=  ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_3]  |=  ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_4]  |=  ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_1]  |=  ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_5]  |=  ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_6]  |=  ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_7]  |=  ((RETRO_DEVICE_ID_JOYPAD_SELECT+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_8] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
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
            thr->keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_START]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_SELECT]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X]|= ((RETRO_DEVICE_ID_JOYPAD_Y+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y]|= ((RETRO_DEVICE_ID_JOYPAD_X+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1]|= ((RETRO_DEVICE_ID_JOYPAD_L+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1]|= ((RETRO_DEVICE_ID_JOYPAD_R+1)    << shift);
            break;
         case DEVICE_CCPCREATIONS_WIIUSE_IME:
            g_settings.input.device[port] = device;
            strlcpy(g_settings.input.device_names[port], "ccpCreations WiiUse IME",
                  sizeof(g_settings.input.device_names[port]));
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;

            /* Player 1 */
            thr->keycode_lut[AKEYCODE_DPAD_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            thr->keycode_lut[AKEYCODE_DPAD_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            thr->keycode_lut[AKEYCODE_DPAD_LEFT] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            thr->keycode_lut[AKEYCODE_DPAD_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            thr->keycode_lut[AKEYCODE_1]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_2]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_3]         |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_4]         |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            thr->keycode_lut[AKEYCODE_5]         |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            thr->keycode_lut[AKEYCODE_6]         |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)   << shift);
            thr->keycode_lut[AKEYCODE_M]         |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)  << shift);
            thr->keycode_lut[AKEYCODE_P]         |= ((RETRO_DEVICE_ID_JOYPAD_START+1)   << shift);
            thr->keycode_lut[AKEYCODE_E]         |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)       << shift);
            thr->keycode_lut[AKEYCODE_B]         |= ((RETRO_DEVICE_ID_JOYPAD_X+1)       << shift);
            thr->keycode_lut[AKEYCODE_F]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)       << shift);
            thr->keycode_lut[AKEYCODE_G]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)       << shift);
            thr->keycode_lut[AKEYCODE_C]         |= ((RETRO_DEVICE_ID_JOYPAD_L+1)       << shift);
            thr->keycode_lut[AKEYCODE_LEFT_BRACKET]  |= ((RETRO_DEVICE_ID_JOYPAD_L2+1)       << shift);
            thr->keycode_lut[AKEYCODE_RIGHT_BRACKET] |= ((RETRO_DEVICE_ID_JOYPAD_R2+1)       << shift);
            thr->keycode_lut[AKEYCODE_Z]         |= ((RETRO_DEVICE_ID_JOYPAD_R+1)       << shift);
            thr->keycode_lut[AKEYCODE_H]         |= ((RARCH_RESET+1)                    << shift);
            thr->keycode_lut[AKEYCODE_W]         |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_S]         |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            thr->keycode_lut[AKEYCODE_A]         |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            thr->keycode_lut[AKEYCODE_D]         |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            thr->keycode_lut[AKEYCODE_C]         |= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            thr->keycode_lut[AKEYCODE_Z]         |= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);

            /* Player 2 */
            shift += 8;
            thr->keycode_lut[AKEYCODE_I]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            thr->keycode_lut[AKEYCODE_K] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            thr->keycode_lut[AKEYCODE_J] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            thr->keycode_lut[AKEYCODE_O]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            thr->keycode_lut[AKEYCODE_COMMA] |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_PERIOD] |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_VOLUME_UP] |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)      << shift);
            thr->keycode_lut[AKEYCODE_VOLUME_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)      << shift);
            thr->keycode_lut[AKEYCODE_MEDIA_PREVIOUS] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)      << shift);
            thr->keycode_lut[AKEYCODE_MEDIA_NEXT] |= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)      << shift);
            thr->keycode_lut[AKEYCODE_MEDIA_PLAY] |= ((RETRO_DEVICE_ID_JOYPAD_X+1)      << shift);
            thr->keycode_lut[AKEYCODE_MEDIA_STOP] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1)      << shift);
            thr->keycode_lut[AKEYCODE_ENDCALL] |= ((RETRO_DEVICE_ID_JOYPAD_A+1)      << shift);
            thr->keycode_lut[AKEYCODE_CALL] |= ((RETRO_DEVICE_ID_JOYPAD_B+1)      << shift);
            thr->keycode_lut[AKEYCODE_PLUS] |= ((RETRO_DEVICE_ID_JOYPAD_START+1)      << shift);
            thr->keycode_lut[AKEYCODE_MINUS] |= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)      << shift);
            thr->keycode_lut[AKEYCODE_BACKSLASH] |= ((RARCH_RESET+1)      << shift);
            thr->keycode_lut[AKEYCODE_L] |= ((RETRO_DEVICE_ID_JOYPAD_L+1)      << shift);
            thr->keycode_lut[AKEYCODE_R] |= ((RETRO_DEVICE_ID_JOYPAD_R+1)      << shift);
            thr->keycode_lut[AKEYCODE_SEARCH] |= ((RETRO_DEVICE_ID_JOYPAD_L2+1)      << shift);
            thr->keycode_lut[AKEYCODE_TAB] |= ((RETRO_DEVICE_ID_JOYPAD_R2+1)      << shift);

            /* Player 3 */
            shift += 8;
            thr->keycode_lut[AKEYCODE_PAGE_UP]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            thr->keycode_lut[AKEYCODE_PAGE_DOWN] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            thr->keycode_lut[AKEYCODE_MEDIA_REWIND] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            thr->keycode_lut[AKEYCODE_MEDIA_FAST_FORWARD]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            thr->keycode_lut[AKEYCODE_SOFT_LEFT]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            thr->keycode_lut[AKEYCODE_SOFT_RIGHT]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_THUMBR]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_THUMBL]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            thr->keycode_lut[AKEYCODE_SPACE]|= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            thr->keycode_lut[AKEYCODE_SYM]|= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            thr->keycode_lut[AKEYCODE_EXPLORER]|= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            thr->keycode_lut[AKEYCODE_ENVELOPE]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_X]|= ((RETRO_DEVICE_ID_JOYPAD_X+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Y]|= ((RETRO_DEVICE_ID_JOYPAD_Y+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_A]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_B]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L1]|= ((RETRO_DEVICE_ID_JOYPAD_L+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R1]|= ((RETRO_DEVICE_ID_JOYPAD_R+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_L2]|= ((RETRO_DEVICE_ID_JOYPAD_L2+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_R2]|= ((RETRO_DEVICE_ID_JOYPAD_R2+1)    << shift);

            /* Player 4 */
            shift += 8;
            thr->keycode_lut[AKEYCODE_N]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            thr->keycode_lut[AKEYCODE_Q] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            thr->keycode_lut[AKEYCODE_T] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift);
            thr->keycode_lut[AKEYCODE_APOSTROPHE]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift);
            thr->keycode_lut[AKEYCODE_NOTIFICATION]|= ((RETRO_DEVICE_ID_JOYPAD_B+1)    << shift);
            thr->keycode_lut[AKEYCODE_MUTE]|= ((RETRO_DEVICE_ID_JOYPAD_A+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_START]|= ((RETRO_DEVICE_ID_JOYPAD_START+1)    << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_SELECT]|= ((RETRO_DEVICE_ID_JOYPAD_SELECT+1)    << shift);
            thr->keycode_lut[AKEYCODE_CLEAR]|= ((RARCH_RESET+1)    << shift);
            thr->keycode_lut[AKEYCODE_CAPS_LOCK]   |= ((RETRO_DEVICE_ID_JOYPAD_UP+1)    << shift);
            thr->keycode_lut[AKEYCODE_SCROLL_LOCK] |= ((RETRO_DEVICE_ID_JOYPAD_DOWN+1)    << shift);
            //thr->keycode_lut[AKEYCODE_T] |= ((RETRO_DEVICE_ID_JOYPAD_LEFT+1)    << shift); -- Left meta
            //thr->keycode_lut[AKEYCODE_APOSTROPHE]|= ((RETRO_DEVICE_ID_JOYPAD_RIGHT+1)    << shift); -- right meta
            thr->keycode_lut[AKEYCODE_META_FUNCTION_ON] |= ((RETRO_DEVICE_ID_JOYPAD_X+1) << shift);
            thr->keycode_lut[AKEYCODE_SYSRQ] |= ((RETRO_DEVICE_ID_JOYPAD_Y+1) << shift);
            thr->keycode_lut[AKEYCODE_BREAK] |= ((RETRO_DEVICE_ID_JOYPAD_A+1) << shift);
            thr->keycode_lut[AKEYCODE_MOVE_HOME] |= ((RETRO_DEVICE_ID_JOYPAD_B+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_C] |= ((RETRO_DEVICE_ID_JOYPAD_L+1) << shift);
            thr->keycode_lut[AKEYCODE_BUTTON_Z] |= ((RETRO_DEVICE_ID_JOYPAD_R+1) << shift);
            thr->keycode_lut[AKEYCODE_GRAVE] |= ((RETRO_DEVICE_ID_JOYPAD_L2+1) << shift);
            thr->keycode_lut[AKEYCODE_MEDIA_PAUSE] |= ((RETRO_DEVICE_ID_JOYPAD_R2+1) << shift);
            break;
         case DEVICE_NONE:
         default:
            g_settings.input.device[port] = 0;
            strlcpy(g_settings.input.device_names[port], "Unknown",
                  sizeof(g_settings.input.device_names[port]));
            break;
      }

      thr->keycode_lut[AKEYCODE_MENU] |= ((RARCH_MENU_TOGGLE + 1) << shift);
   }
}

static void thread_handle_input(struct android_input_state *thr)
{
   bool debug_enable = g_settings.input.debug_enable;
   struct android_app *android_app = (struct android_app*)g_android;
   AInputEvent* event = NULL;

   uint64_t *lifecycle_state = &thr->thread.lifecycle_state;

   // Read all pending events.
   while (AInputQueue_hasEvents(android_app->inputQueue))
   {
      if (AInputQueue_getEvent(android_app->inputQueue, &event) >= 0)
      {
         bool long_msg_enable = false;
         int32_t handled = 1;
         int action = 0;
         char msg[128];
         int source, id, keycode, type_event, state_id;
         //int predispatched;

         msg[0] = 0;
         //predispatched =AInputQueue_preDispatchEvent(android_app->inputQueue,event);

         //if (predispatched)
         //continue;

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
            for (unsigned i = 0; i < thr->thread.pads_connected; i++)
               if (thr->state_device_ids[i] == id)
                  state_id = i;
         }

         if (state_id < 0)
         {
            state_id = thr->thread.pads_connected;
            thr->state_device_ids[thr->thread.pads_connected++] = id;

            input_autodetect_setup(android_app, msg, sizeof(msg), state_id, id, source);
            long_msg_enable = true;
         }

         if (keycode == AKEYCODE_BACK)
         {
            uint8_t unpacked = (thr->keycode_lut[AKEYCODE_BACK] >> ((state_id+1) << 3)) - 1;
            uint64_t input_state = (1ULL << unpacked);

            if (g_extern.lifecycle_mode_state & (1ULL << MODE_INPUT_XPERIA_PLAY_HACK))
            {
               int meta = AKeyEvent_getMetaState(event);
               if (!(meta & AMETA_ALT_ON))
               {
                  *lifecycle_state |= (1ULL << RARCH_QUIT_KEY);
                  AInputQueue_finishEvent(android_app->inputQueue, event, handled);
                  continue;
               }
            }
            else if (type_event == AINPUT_EVENT_TYPE_KEY &&
                  input_state < (1ULL << RARCH_FIRST_META_KEY) &&
                  input_state > 0)
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
               continue;
            }
            else
            {
               // exits the app, so no need to check for up/down action
               *lifecycle_state |= (1ULL << RARCH_QUIT_KEY);
               AInputQueue_finishEvent(android_app->inputQueue, event, handled);
               continue;
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
                  engine_handle_dpad(thr, event, motion_pointer, state_id, msg, sizeof(msg), source, debug_enable);
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
                  memmove(thr->thread.pointer + motion_pointer, thr->thread.pointer + motion_pointer + 1,
                        (MAX_TOUCH - motion_pointer - 1) * sizeof(struct input_pointer));
                  if (thr->thread.pointer_count > 0)
                     thr->thread.pointer_count--;
               }
               else
               {
                  thr->thread.pointer_count = AMotionEvent_getPointerCount(event);
                  int pointer_max = min(thr->thread.pointer_count, MAX_TOUCH);
                  for (motion_pointer = 0; motion_pointer < pointer_max; motion_pointer++)
                  {
                     x = AMotionEvent_getX(event, motion_pointer);
                     y = AMotionEvent_getY(event, motion_pointer);

                     input_translate_coord_viewport(x, y,
                           &thr->thread.pointer[motion_pointer].x,
                           &thr->thread.pointer[motion_pointer].y,
                           &thr->thread.pointer[motion_pointer].full_x,
                           &thr->thread.pointer[motion_pointer].full_y);
                  }
               }

               if (debug_enable)
                  snprintf(msg, sizeof(msg), "Pad %d : x = %.2f, y = %.2f, src %d.\n", state_id, x, y, source);
            }

         }
         else if (type_event == AINPUT_EVENT_TYPE_KEY)
         {
            if (debug_enable)
               snprintf(msg, sizeof(msg), "Pad %d : %d, ac = %d, src = %d.\n", state_id, keycode, action, source);

            /* Hack - we have to decrease the unpacked value by 1
             * because we 'added' 1 to each entry in the LUT -
             * RETRO_DEVICE_ID_JOYPAD_B is 0
             */
            uint8_t unpacked = (thr->keycode_lut[keycode] >> ((state_id+1) << 3)) - 1;
            uint64_t input_state = (1ULL << unpacked);
            int action  = AKeyEvent_getAction(event);
            uint64_t *key = NULL;

            if (input_state < (1ULL << RARCH_FIRST_META_KEY))
               key = &thr->thread.state[state_id];
            else if (input_state)
               key = &thr->thread.lifecycle_state;

            if (key != NULL)
            {
               if (action == AKEY_EVENT_ACTION_UP)
                  *key &= ~(input_state);
               else if (action == AKEY_EVENT_ACTION_DOWN)
                  *key |= input_state;
            }

            if ((keycode == AKEYCODE_VOLUME_UP ||
                     keycode == AKEYCODE_VOLUME_DOWN) && thr->keycode_lut[keycode] == 0)
               handled = 0;
         }

         if (*msg)
         {
            strlcpy(thr->msg, msg, sizeof(thr->msg));
            thr->msg_time = long_msg_enable ? 180 : 30;
         }

         AInputQueue_finishEvent(android_app->inputQueue, event, handled);
      }
   }
}

static void event_handling_thread(void *data)
{
   struct android_input_state *thr = (struct android_input_state*)data;

   struct android_app *android_app = (struct android_app*)g_android;
   ALooper *looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
   ALooper_addFd(looper, thr->msgpipe[0], LOOPER_ID_INPUT_MSG, ALOOPER_EVENT_INPUT, NULL, NULL);

   if (android_app->inputQueue)
   {
      AInputQueue_attachLooper(android_app->inputQueue,
            looper, LOOPER_ID_INPUT, NULL,
            NULL);
   }

   int ident;
   while ((ident = ALooper_pollAll(-1, NULL, NULL, NULL)) >= 0)
   {
      if (ident == LOOPER_ID_INPUT_MSG)
      {
         int msg = 0;
         if (read(thr->msgpipe[0], &msg, sizeof(msg)) != (ssize_t)sizeof(msg))
            break;

         switch (msg)
         {
            case INPUT_MSG_DIE:
               return;

            case INPUT_MSG_UPDATE_INPUT_QUEUE:
               set_input_queue(looper, android_app->pendingInputQueue);
               break;
         }
         break;
      }
      else if (ident == LOOPER_ID_INPUT)
      {
         slock_lock(thr->lock);
         thread_handle_input(thr);
         slock_unlock(thr->lock);
      }
   }
}

// Handle all events. If our activity is in pause state, block until we're unpaused.

static void android_input_poll(void *data)
{
   struct android_input_state *thr = (struct android_input_state*)data;

   int ident;
   while ((ident = ALooper_pollAll((input_key_pressed_func(RARCH_PAUSE_TOGGLE)) ? -1 : 0,
               NULL, NULL, NULL)) >= 0)
   {
      if (ident == LOOPER_ID_MAIN)
         engine_handle_cmd(thr);
   }

   slock_lock(thr->lock);
   memcpy(&thr->copy, &thr->thread, sizeof(thr->copy));
   if (*thr->msg)
   {
      msg_queue_push(g_extern.msg_queue, thr->msg, 0, thr->msg_time);
      *thr->msg = '\0';
   }
   slock_unlock(thr->lock);
}

static int16_t android_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   struct android_input_state *thr = (struct android_input_state*)data;
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return ((thr->copy.state[port] & binds[port][id].joykey) &&
               (port < thr->copy.pads_connected));
      case RETRO_DEVICE_POINTER:
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return thr->copy.pointer[index].x;
            case RETRO_DEVICE_ID_POINTER_Y:
               return thr->copy.pointer[index].y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return (index < thr->copy.pointer_count) &&
                  (thr->copy.pointer[index].x != -0x8000) &&
                  (thr->copy.pointer[index].y != -0x8000);
            default:
               return 0;
         }
      case RARCH_DEVICE_POINTER_SCREEN:
         switch (id)
         {
            case RETRO_DEVICE_ID_POINTER_X:
               return thr->copy.pointer[index].full_x;
            case RETRO_DEVICE_ID_POINTER_Y:
               return thr->copy.pointer[index].full_y;
            case RETRO_DEVICE_ID_POINTER_PRESSED:
               return (index < thr->copy.pointer_count) &&
                  (thr->copy.pointer[index].full_x != -0x8000) &&
                  (thr->copy.pointer[index].full_y != -0x8000);
            default:
               return 0;
         }
      default:
         return 0;
   }
}

static bool android_input_key_pressed(void *data, int key)
{
   struct android_input_state *thr = (struct android_input_state*)data;
   return ((g_extern.lifecycle_state | thr->copy.lifecycle_state) & (1ULL << key));
}

static void android_input_free_input(void *data)
{
   struct android_input_state *thr = (struct android_input_state*)data;

   if (!thr)
      return;

   int msg = INPUT_MSG_DIE;
   write(thr->msgpipe[1], &msg, sizeof(msg));
   sthread_join(thr->input_thread);
   slock_free(thr->lock);
   close(thr->msgpipe[0]);
   close(thr->msgpipe[1]);
   free(thr);
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
