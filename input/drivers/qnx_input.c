/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2013-2014 - CatalystG
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

#include <boolean.h>
#include <string/stdstring.h>

#include <screen/screen.h>
#include <bps/event.h>
#include <bps/navigator.h>
#include <sys/keycodes.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../config.def.h"


#include "../../retroarch.h"
#include "../../tasks/tasks_internal.h"

#include "../../command.h"

#ifdef HAVE_BB10
#define MAX_TOUCH 16
#else
#define MAX_TOUCH 4
#endif

typedef struct
{
#ifdef HAVE_BB10
   screen_device_t handle;
#endif
   int type;
   int analogCount;
   int buttonCount;
   char id[64];
   char vid[64];
   char pid[64];

   int device;
   int port;
   int index;

   /* Current state. */
   int buttons;
   int analog0[3];
   int analog1[3];
} qnx_input_device_t;

struct input_pointer
{
   int16_t x, y;
   int16_t full_x, full_y;
   int contact_id;
   int map;
};

#define QNX_MAX_KEYS (65535 + 7) / 8
#define TRACKPAD_CPI 500
#define TRACKPAD_THRESHOLD TRACKPAD_CPI / 2

typedef struct qnx_input
{
   unsigned pads_connected;

   /*
    * The first pointer_count indices of touch_map will be a valid,
    * active index in pointer array.
    * Saves us from searching through pointer array when polling state.
    */
   struct input_pointer pointer[MAX_TOUCH];
   unsigned pointer_count;
   int touch_map[MAX_TOUCH];

   qnx_input_device_t devices[DEFAULT_MAX_PADS];
   const input_device_driver_t *joypad;

   uint8_t keyboard_state[QNX_MAX_KEYS];

   uint64_t pad_state[DEFAULT_MAX_PADS];

   int trackpad_acc[2];
} qnx_input_t;

extern screen_context_t screen_ctx;

static void qnx_init_controller(qnx_input_t *qnx, qnx_input_device_t* controller)
{
   if (!qnx)
      return;

   /* Initialize controller values. */
#ifdef HAVE_BB10
   controller->handle      = 0;
#endif
   controller->type        = 0;
   controller->analogCount = 0;
   controller->buttonCount = 0;
   controller->buttons     = 0;
   controller->analog0[0]  = 0;
   controller->analog0[1]  = 0;
   controller->analog0[2]  = 0;
   controller->analog1[0]  = 0;
   controller->analog1[1]  = 0;
   controller->analog1[2]  = 0;
   controller->port        = -1;
   controller->device      = -1;
   controller->index       = -1;

   memset(controller->id, 0, sizeof(controller->id));
}

#ifdef HAVE_BB10
bool prevMenu;
static void qnx_process_gamepad_event(
      qnx_input_t *qnx,
      screen_event_t screen_event, int type)
{
   int i;
   screen_device_t device;
   qnx_input_device_t* controller = NULL;
   uint64_t *state_cur            = NULL;

   (void)type;

   screen_get_event_property_pv(screen_event,
         SCREEN_PROPERTY_DEVICE, (void**)&device);

   for (i = 0; i < DEFAULT_MAX_PADS; ++i)
   {
      if (device == qnx->devices[i].handle)
      {
         controller = (qnx_input_device_t*)&qnx->devices[i];
         break;
      }
   }

   if (!controller)
      return;

   /* Store the controller's new state. */
   screen_get_event_property_iv(screen_event,
         SCREEN_PROPERTY_BUTTONS, &controller->buttons);

   if (controller->analogCount > 0)
   {
      screen_get_event_property_iv(screen_event,
            SCREEN_PROPERTY_ANALOG0, controller->analog0);

      controller->analog0[0] *= 256;
      controller->analog0[1] *= 256;

       if (controller->analogCount == 2)
       {
          screen_get_event_property_iv(screen_event,
                SCREEN_PROPERTY_ANALOG1, controller->analog1);

          controller->analog1[0] *= 256;
          controller->analog1[1] *= 256;
       }
   }
}

static void qnx_process_joystick_event(qnx_input_t *qnx, screen_event_t screen_event, int type)
{
    int displacement[2];
    screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_DISPLACEMENT, displacement);

    if(displacement != 0)
    {
        qnx->trackpad_acc[0] += displacement[0];
        if(abs(qnx->trackpad_acc[0]) > TRACKPAD_THRESHOLD)
        {
            if(qnx->trackpad_acc < 0)
            {
                input_keyboard_event(true, RETROK_LEFT, 0, 0, RETRO_DEVICE_KEYBOARD);
                input_keyboard_event(false, RETROK_LEFT, 0, 0, RETRO_DEVICE_KEYBOARD);
            }
            else if(qnx->trackpad_acc > 0)
            {
                input_keyboard_event(true, RETROK_RIGHT, 0, 0, RETRO_DEVICE_KEYBOARD);
                input_keyboard_event(false, RETROK_RIGHT, 0, 0, RETRO_DEVICE_KEYBOARD);
            }

            qnx->trackpad_acc[0] = 0;
        }

        qnx->trackpad_acc[1] += displacement[1];
        if(abs(qnx->trackpad_acc[1]) > TRACKPAD_THRESHOLD)
        {
            if(qnx->trackpad_acc < 0)
            {
                input_keyboard_event(true, RETROK_UP, 0, 0, RETRO_DEVICE_KEYBOARD);
                input_keyboard_event(false, RETROK_UP, 0, 0, RETRO_DEVICE_KEYBOARD);
            }
            else if(qnx->trackpad_acc > 0)
            {
                input_keyboard_event(true, RETROK_DOWN, 0, 0, RETRO_DEVICE_KEYBOARD);
                input_keyboard_event(false, RETROK_DOWN, 0, 0, RETRO_DEVICE_KEYBOARD);
            }

            qnx->trackpad_acc[1] = 0;
        }
    }

    int buttons = 0;
    screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_BUTTONS, &buttons);
    input_keyboard_event(buttons != 0, RETROK_RETURN, 0, 0, RETRO_DEVICE_KEYBOARD);
}

static void qnx_input_autodetect_gamepad(qnx_input_t *qnx,
      qnx_input_device_t* controller)
{
   char name_buf[256];

   if (!qnx)
      return;

   name_buf[0] = '\0';
   if(controller && controller->type == SCREEN_EVENT_GAMEPAD)
   {
       if(strstr(controller->id, "0-054C-05C4-1.0"))
           strlcpy(name_buf, "DS4 Controller", sizeof(name_buf));
       else
           strlcpy(name_buf, "QNX Gamepad", sizeof(name_buf));
   }

   if (!string_is_empty(name_buf))
   {
      controller->port = qnx->pads_connected;

      input_autoconfigure_connect(
            name_buf,
            NULL,
            qnx->joypad->ident,
            controller->port,
            *controller->vid,
            *controller->pid);

      qnx->pads_connected++;
   }
}

static void qnx_handle_device(qnx_input_t *qnx,
      qnx_input_device_t* controller)
{
   if (!qnx)
      return;

   /* Query libscreen for information about this device. */
   screen_get_device_property_iv(controller->handle,
         SCREEN_PROPERTY_TYPE, &controller->type);
   screen_get_device_property_cv(controller->handle,
         SCREEN_PROPERTY_ID_STRING, sizeof(controller->id), controller->id);
   screen_get_device_property_cv(controller->handle,
         SCREEN_PROPERTY_VENDOR, sizeof(controller->vid), controller->vid);
   screen_get_device_property_cv(controller->handle,
         SCREEN_PROPERTY_PRODUCT, sizeof(controller->pid), controller->pid);

   if (controller->type == SCREEN_EVENT_GAMEPAD)
   {
      screen_get_device_property_iv(controller->handle,
            SCREEN_PROPERTY_BUTTON_COUNT, &controller->buttonCount);

      /* Check for the existence of analog sticks. */
      if (!screen_get_device_property_iv(controller->handle,
               SCREEN_PROPERTY_ANALOG0, controller->analog0))
         ++controller->analogCount;

      if (!screen_get_device_property_iv(controller->handle,
               SCREEN_PROPERTY_ANALOG1, controller->analog1))
         ++controller->analogCount;
   }

   /* Screen service will map supported controllers,
    * we still might need to adjust. */
   qnx_input_autodetect_gamepad(qnx, controller);

   if (controller->type == SCREEN_EVENT_GAMEPAD)
      RARCH_LOG("Gamepad Device Connected:\n");
   else if (controller->type == SCREEN_EVENT_JOYSTICK)
      RARCH_LOG("Joystick Device Connected:\n");
   else if (controller->type == SCREEN_EVENT_KEYBOARD)
      RARCH_LOG("Keyboard Device Connected:\n");

   RARCH_LOG("\tID: %s\n", controller->id);
   RARCH_LOG("\tVendor  ID: %s\n", controller->vid);
   RARCH_LOG("\tProduct ID: %s\n", controller->pid);
   RARCH_LOG("\tButton Count: %d\n", controller->buttonCount);
   RARCH_LOG("\tAnalog Count: %d\n", controller->analogCount);
}

/* Find currently connected gamepads. */
static void qnx_discover_controllers(qnx_input_t *qnx)
{
   /* Get an array of all available devices. */
   int deviceCount;
   unsigned i;

   screen_get_context_property_iv(screen_ctx,
         SCREEN_PROPERTY_DEVICE_COUNT, &deviceCount);
   screen_device_t* devices_found = (screen_device_t*)
      calloc(deviceCount, sizeof(screen_device_t));
   screen_get_context_property_pv(screen_ctx,
         SCREEN_PROPERTY_DEVICES, (void**)devices_found);

   /* Scan the list for gamepad and joystick devices. */
   for(i = 0; i < qnx->pads_connected; ++i)
   {
      qnx_init_controller(qnx, &qnx->devices[i]);
   }

   qnx->pads_connected = 0;

   for (i = 0; i < deviceCount; i++)
   {
      int type;
      screen_get_device_property_iv(
            devices_found[i], SCREEN_PROPERTY_TYPE, &type);

      if (type == SCREEN_EVENT_GAMEPAD  ||
          type == SCREEN_EVENT_JOYSTICK ||
          type == SCREEN_EVENT_KEYBOARD)
      {
         qnx->devices[qnx->pads_connected].handle = devices_found[i];
         qnx->devices[qnx->pads_connected].index = qnx->pads_connected;
         qnx_handle_device(qnx, &qnx->devices[qnx->pads_connected]);

         if (qnx->pads_connected == DEFAULT_MAX_PADS)
            break;
      }
   }

   free(devices_found);
}
#endif

static void qnx_process_keyboard_event(
      qnx_input_t *qnx,
      screen_event_t event, int type)
{
    // Get key properties from screen event
    int flags = 0;
    screen_get_event_property_iv(event, SCREEN_PROPERTY_KEY_FLAGS, &flags);

    int cap = 0;
    screen_get_event_property_iv(event, SCREEN_PROPERTY_KEY_CAP, &cap);

    int mod = 0;
    screen_get_event_property_iv(event, SCREEN_PROPERTY_KEY_MODIFIERS, &mod);

    // Calculate state
    unsigned keycode = input_keymaps_translate_keysym_to_rk(cap);
    bool keydown = flags & KEY_DOWN;
    bool keyrepeat = flags & KEY_REPEAT;

    // Fire keyboard event
    if(!keyrepeat)
    {
        input_keyboard_event(keydown, keycode, 0, mod, RETRO_DEVICE_KEYBOARD);
    }

    // Apply keyboard state
    if(keydown && !keyrepeat)
    {
       BIT_SET(qnx->keyboard_state, cap);
    }
    else if(!keydown && !keyrepeat)
    {
       BIT_CLEAR(qnx->keyboard_state, cap);
    }
}

static void qnx_process_touch_event(
      qnx_input_t *qnx, screen_event_t event, int type)
{
   int contact_id, pos[2];
   unsigned i, j;

   screen_get_event_property_iv(event,
         SCREEN_PROPERTY_TOUCH_ID, (int*)&contact_id);
   screen_get_event_property_iv(event,
         SCREEN_PROPERTY_SOURCE_POSITION, pos);

   switch(type)
   {
      case SCREEN_EVENT_MTOUCH_TOUCH:
         /* Find a free touch struct. */
         for(i = 0; i < MAX_TOUCH; ++i)
         {
            if(qnx->pointer[i].contact_id == -1)
            {
               struct video_viewport vp;

               vp.x                        = 0;
               vp.y                        = 0;
               vp.width                    = 0;
               vp.height                   = 0;
               vp.full_width               = 0;
               vp.full_height              = 0;

               qnx->pointer[i].contact_id  = contact_id;

               video_driver_translate_coord_viewport_wrap(
                     &vp,
                     pos[0], pos[1],
                     &qnx->pointer[i].x, &qnx->pointer[i].y,
                     &qnx->pointer[i].full_x, &qnx->pointer[i].full_y);

               /* Add this pointer to the map to signal it's valid. */
               qnx->pointer[i].map = qnx->pointer_count;
               qnx->touch_map[qnx->pointer_count] = i;
               qnx->pointer_count++;
               break;
            }
         }
#if 0
         RARCH_LOG("New Touch: x:%d, y:%d, id:%d\n", pos[0], pos[1], contact_id);
         RARCH_LOG("Map: %d %d %d %d %d %d\n", qnx->touch_map[0], qnx->touch_map[1],
               qnx->touch_map[2], qnx->touch_map[3], qnx->touch_map[4],
               qnx->touch_map[5]);
#endif
         break;

      case SCREEN_EVENT_MTOUCH_RELEASE:
         for(i = 0; i < MAX_TOUCH; ++i)
         {
            if(qnx->pointer[i].contact_id == contact_id)
            {
               /* Invalidate the finger. */
               qnx->pointer[i].contact_id = -1;

               /* Remove pointer from map and shift
                * remaining valid ones to the front. */
               qnx->touch_map[qnx->pointer[i].map] = -1;
               for(j = qnx->pointer[i].map; j < qnx->pointer_count; ++j)
               {
                  qnx->touch_map[j] = qnx->touch_map[j+1];
                  qnx->pointer[qnx->touch_map[j+1]].map = j;
                  qnx->touch_map[j+1] = -1;
               }
               qnx->pointer_count--;
               break;
            }
         }
#if 0
         RARCH_LOG("Release: x:%d, y:%d, id:%d\n", pos[0], pos[1], contact_id);
         RARCH_LOG("Map: %d %d %d %d %d %d\n", qnx->touch_map[0], qnx->touch_map[1],
               qnx->touch_map[2], qnx->touch_map[3], qnx->touch_map[4],
               qnx->touch_map[5]);
#endif
         break;

      case SCREEN_EVENT_MTOUCH_MOVE:
         /* Find the finger we're tracking and update. */
         for(i = 0; i < qnx->pointer_count; ++i)
         {
            if(qnx->pointer[i].contact_id == contact_id)
            {
               struct video_viewport vp;

               vp.x                        = 0;
               vp.y                        = 0;
               vp.width                    = 0;
               vp.height                   = 0;
               vp.full_width               = 0;
               vp.full_height              = 0;

#if 0
               gl_t *gl = (gl_t*)video_driver_get_ptr(false);

               /*During a move, we can go ~30 pixel into the
                * bezel which gives negative numbers or
                * numbers larger than the screen resolution.
                *
                * Normalize. */
               if(pos[0] < 0)
                  pos[0] = 0;
               if(pos[0] > gl->full_x)
                  pos[0] = gl->full_x;

               if(pos[1] < 0)
                  pos[1] = 0;
               if(pos[1] > gl->full_y)
                  pos[1] = gl->full_y;
#endif

               video_driver_translate_coord_viewport_wrap(&vp,
                     pos[0], pos[1],
                     &qnx->pointer[i].x, &qnx->pointer[i].y,
                     &qnx->pointer[i].full_x, &qnx->pointer[i].full_y);
#if 0
               RARCH_LOG("Move: x:%d, y:%d, id:%d\n", pos[0], pos[1],
                     contact_id);
#endif
               break;
            }
         }
         break;
   }
}

static void qnx_handle_screen_event(qnx_input_t *qnx, bps_event_t *event)
{
   int type;
   screen_event_t screen_event = screen_event_get_event(event);

   screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &type);

   switch(type)
   {
      case SCREEN_EVENT_MTOUCH_TOUCH:
      case SCREEN_EVENT_MTOUCH_RELEASE:
      case SCREEN_EVENT_MTOUCH_MOVE:
         qnx_process_touch_event(qnx, screen_event, type);
         break;
      case SCREEN_EVENT_KEYBOARD:
         qnx_process_keyboard_event(qnx, screen_event, type);
         break;
#ifdef HAVE_BB10
      case SCREEN_EVENT_GAMEPAD:
         qnx_process_gamepad_event(qnx, screen_event, type);
          break;
      case SCREEN_EVENT_JOYSTICK:
         qnx_process_joystick_event(qnx, screen_event, type);
         break;
      case SCREEN_EVENT_DEVICE:
         {
            /* A device was attached or removed. */
            screen_device_t device;
            int attached, type, i;

            screen_get_event_property_pv(screen_event,
                  SCREEN_PROPERTY_DEVICE, (void**)&device);
            screen_get_event_property_iv(screen_event,
                  SCREEN_PROPERTY_ATTACHED, &attached);

            if (attached)
               screen_get_device_property_iv(device,
                     SCREEN_PROPERTY_TYPE, &type);

            if (attached &&
                  (
                   type == SCREEN_EVENT_GAMEPAD ||
                   type == SCREEN_EVENT_JOYSTICK ||
                   type == SCREEN_EVENT_KEYBOARD)
               )
            {
               for (i = 0; i < DEFAULT_MAX_PADS; ++i)
               {
                  if (!qnx->devices[i].handle)
                  {
                     qnx->devices[i].handle = device;
                     qnx_handle_device(qnx, &qnx->devices[i]);
                     break;
                  }
               }
            }
            else
            {
               for (i = 0; i < DEFAULT_MAX_PADS; ++i)
               {
                  if (device == qnx->devices[i].handle)
                  {
                     RARCH_LOG("Device %s: Disconnected.\n",
                           qnx->devices[i].id);
                     qnx_init_controller(qnx, &qnx->devices[i]);
                     break;
                  }
               }
            }
         }
         break;
#endif
      default:
         break;
   }
}

static void qnx_handle_navigator_event(
      qnx_input_t *qnx, bps_event_t *event)
{
   navigator_window_state_t state;
   bps_event_t *event_pause = NULL;

   switch (bps_event_get_code(event))
   {
      case NAVIGATOR_SYSKEY_PRESS:
         switch(navigator_event_get_syskey_key(event))
         {
            case NAVIGATOR_SYSKEY_BACK:
               input_keyboard_event(true, RETROK_BACKSPACE, 0, 0, RETRO_DEVICE_KEYBOARD);
               input_keyboard_event(false, RETROK_BACKSPACE, 0, 0, RETRO_DEVICE_KEYBOARD);
               break;
            case NAVIGATOR_SYSKEY_SEND:
            case NAVIGATOR_SYSKEY_END:
               break;
            default:
               break;
         }
         break;
      case NAVIGATOR_SWIPE_DOWN:
         command_event(CMD_EVENT_MENU_TOGGLE, NULL);
         break;
      case NAVIGATOR_WINDOW_STATE:
         switch(navigator_event_get_window_state(event))
         {
            case NAVIGATOR_WINDOW_THUMBNAIL:
            case NAVIGATOR_WINDOW_INVISIBLE:
               while(true)
               {
                  unsigned event_code;

                  /* Block until we get a resume or exit event. */
                  bps_get_event(&event_pause, -1);
                  event_code = bps_event_get_code(event_pause);

                  if(event_code == NAVIGATOR_WINDOW_STATE)
                  {
                     if(navigator_event_get_window_state(event_pause) == NAVIGATOR_WINDOW_FULLSCREEN)
                        break;
                  }
                  else if(event_code == NAVIGATOR_EXIT)
                     goto shutdown;
               }
               break;
            case NAVIGATOR_WINDOW_FULLSCREEN:
               break;
         }
         break;
     case NAVIGATOR_EXIT:
        goto shutdown;
      default:
         break;
   }

   return;

   togglemenu:
       command_event(CMD_EVENT_MENU_TOGGLE, NULL);
       return;
   shutdown:
       rarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
       return;
}

static void *qnx_input_init(const char *joypad_driver)
{
   int i;
   qnx_input_t *qnx     = (qnx_input_t*)calloc(1, sizeof(*qnx));

   if (!qnx)
      return NULL;

   input_keymaps_init_keyboard_lut(rarch_key_map_qnx);

   for (i = 0; i < MAX_TOUCH; ++i)
   {
      qnx->pointer[i].contact_id = -1;
      qnx->touch_map[i] = -1;
   }

   qnx->joypad = input_joypad_init_driver(joypad_driver, qnx);

   for (i = 0; i < DEFAULT_MAX_PADS; ++i)
      qnx_init_controller(qnx, &qnx->devices[i]);

#ifdef HAVE_BB10
   qnx_discover_controllers(qnx);
#else
   /* Initialize Playbook keyboard. */
   strlcpy(qnx->devices[0].id, "0A5C-8502",
         sizeof(qnx->devices[0].id));
   qnx_input_autodetect_gamepad(qnx, &qnx->devices[0]);
   qnx->pads_connected = 1;
#endif

   return qnx;
}

static void qnx_input_poll(void *data)
{
   qnx_input_t *qnx = (qnx_input_t*)data;

   /* Request and process all available BPS events. */
   while(true)
   {
      bps_event_t *event = NULL;
      int rc = bps_get_event(&event, 0);

      if(rc == BPS_SUCCESS)
      {
         int domain;

         if (!event)
            break;

         domain = bps_event_get_domain(event);
         if (domain == navigator_get_domain())
            qnx_handle_navigator_event(qnx, event);
         else if (domain == screen_get_domain())
            qnx_handle_screen_event(qnx, event);
      }
   }
}

static bool qnx_keyboard_pressed(qnx_input_t *qnx, unsigned id)
{
    unsigned bit = rarch_keysym_lut[(enum retro_key)id];
    return id < RETROK_LAST && BIT_GET(qnx->keyboard_state, bit);
}

static bool qnx_is_pressed(qnx_input_t *qnx,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind *binds,
      unsigned port, unsigned id)
{
   const struct retro_keybind *bind = &binds[id];
   int key                          = bind->key;

   if (id >= RARCH_BIND_LIST_END)
      return false;

   if (qnx_keyboard_pressed(qnx, key))
      if ((id == RARCH_GAME_FOCUS_TOGGLE) || !input_qnx.keyboard_mapping_blocked)
         return true;

   if (binds && binds[id].valid)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[id].joykey != NO_BTN)
         ? binds[id].joykey : joypad_info.auto_binds[id].joykey;
      const uint32_t joyaxis = (binds[id].joyaxis != AXIS_NONE)
         ? binds[id].joyaxis : joypad_info.auto_binds[id].joyaxis;

      if ((uint16_t)joykey != NO_BTN && qnx->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
         return true;
      if (((float)abs(qnx->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
         return true;
   }

   return false;
}

static int16_t qnx_pointer_input_state(qnx_input_t *qnx,
      unsigned idx, unsigned id, bool screen)
{
   int16_t x;
   int16_t y;

   if(screen)
   {
       x = qnx->pointer[idx].full_x;
       y = qnx->pointer[idx].full_y;
   }
   else
   {
       x = qnx->pointer[idx].x;
       y = qnx->pointer[idx].y;
   }

   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_X:
         return x;
      case RETRO_DEVICE_ID_POINTER_Y:
         return y;
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return (idx < qnx->pointer_count)
                 && (x != -0x8000)
                 && (y != -0x8000);
   }

   return 0;
}

static int16_t qnx_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   qnx_input_t *qnx           = (qnx_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               if (qnx_is_pressed(
                        qnx, joypad_info, port, binds[port], i))
               {
                  ret |= (1 << i);
                  continue;
               }
            }

            return ret;
         }
         else
            if (qnx_is_pressed(qnx, joypad_info, port, binds[port], id))
               return true;
         break;
      case RETRO_DEVICE_KEYBOARD:
         return qnx_keyboard_pressed(qnx, id);
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         return qnx_pointer_input_state(qnx, idx, id, device == RARCH_DEVICE_POINTER_SCREEN);
      default:
          break;
   }

   return 0;
}

static void qnx_input_free_input(void *data)
{
   if (data)
      free(data);
}

static uint64_t qnx_input_get_capabilities(void *data)
{
    (void)data;

    return
        (1 << RETRO_DEVICE_JOYPAD)   |
        (1 << RETRO_DEVICE_POINTER)  |
#ifdef HAVE_BB10
        (1 << RETRO_DEVICE_ANALOG)   |
#endif
        (1 << RETRO_DEVICE_KEYBOARD);
}

static const input_device_driver_t *qnx_input_get_joypad_driver(void *data)
{
   qnx_input_t *qnx = (qnx_input_t*)data;
   return qnx->joypad;
}

input_driver_t input_qnx = {
   qnx_input_init,
   qnx_input_poll,
   qnx_input_state,
   qnx_input_free_input,
   NULL,
   NULL,
   qnx_input_get_capabilities,
   "qnx_input",
   NULL,
   NULL,
   NULL,
   qnx_input_get_joypad_driver,
   NULL,
   false
};
