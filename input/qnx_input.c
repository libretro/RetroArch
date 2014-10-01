/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "../general.h"
#include "../driver.h"
#include <screen/screen.h>
#include <bps/event.h>
#include <bps/navigator.h>
#include <sys/keycodes.h>

#define MAX_PADS 8

#ifdef HAVE_BB10
#define MAX_TOUCH 16
#else
#define MAX_TOUCH 4
#endif

typedef struct {
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

    // Current state.
    int buttons;
    int analog0[3];
    int analog1[3];
} input_device_t;

struct input_pointer
{
   int16_t x, y;
   int16_t full_x, full_y;
   int contact_id;
   int map;
};

typedef struct qnx_input
{
   unsigned pads_connected;
   struct input_pointer pointer[MAX_TOUCH];
   unsigned pointer_count;

   int touch_map[MAX_TOUCH];
   /*
    * The first pointer_count indices of touch_map will be a valid, 
    * active index in pointer array.
    * Saves us from searching through pointer array when polling state.
    */
   input_device_t *port_device[MAX_PADS];
   input_device_t devices[MAX_PADS];
   const rarch_joypad_driver_t *joypad;
   int16_t analog_state[MAX_PADS][2][2];
   uint64_t pad_state[MAX_PADS];
} qnx_input_t;

static void qnx_input_autodetect_gamepad(void *data,
      input_device_t* controller, int port);

static void handle_device(void *data,
      input_device_t* controller);

#ifdef HAVE_BB10
static void process_gamepad_event(void *data,
      screen_event_t screen_event, int type)
{
   int i;
   screen_device_t device;
   input_device_t* controller = NULL;

   qnx_input_t *qnx = (qnx_input_t*)data;
   (void)type;

   screen_get_event_property_pv(screen_event,
         SCREEN_PROPERTY_DEVICE, (void**)&device);

   for (i = 0; i < MAX_PADS; ++i)
   {
      if (device == qnx->devices[i].handle)
      {
         controller = (input_device_t*)&qnx->devices[i];
         break;
      }
   }

   if (!controller)
      return;

   /* Store the controller's new state. */
   screen_get_event_property_iv(screen_event,
         SCREEN_PROPERTY_BUTTONS, &controller->buttons);

   uint64_t *state_cur = (uint64_t*)&qnx->pad_state[controller->port];

   *state_cur = 0;
   for (i = 0; i < 20; i++)
      *state_cur |= (controller->buttons & (1 << i) ? (1 << i) : 0);

   if (controller->analogCount > 0)
      screen_get_event_property_iv(screen_event,
            SCREEN_PROPERTY_ANALOG0, controller->analog0);

   if (controller->analogCount == 2)
      screen_get_event_property_iv(screen_event,
            SCREEN_PROPERTY_ANALOG1, controller->analog1);

   /* Only player 1
    * TODO: Am I missing something? Is there a better way? */
   if((controller->port == 0) && 
         (controller->buttons & 
          g_settings.input.binds[0][RARCH_MENU_TOGGLE].joykey))
      g_extern.lifecycle_state ^= (1ULL << RARCH_MENU_TOGGLE);
}

static void handle_device(void *data, input_device_t* controller)
{
   int device;
   qnx_input_t *qnx = (qnx_input_t*)data;

   (void)device;

   if (!qnx)
      return;

   /* Query libscreen for information about this device. */
   screen_get_device_property_iv(controller->handle,
         SCREEN_PROPERTY_TYPE, &controller->type);
   screen_get_device_property_cv(controller->handle,
         SCREEN_PROPERTY_ID_STRING, sizeof(controller->id), controller->id);
   screen_get_device_property_cv(controller->handle,
         SCREEN_PROPERTY_VENDOR, sizeof(controller->id), controller->vid);
   screen_get_device_property_cv(controller->handle,
         SCREEN_PROPERTY_PRODUCT, sizeof(controller->id), controller->pid);

   if (controller->type == SCREEN_EVENT_GAMEPAD || 
         controller->type == SCREEN_EVENT_JOYSTICK)
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
   qnx_input_autodetect_gamepad(qnx, controller, controller->port);

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

extern screen_context_t screen_ctx;

static void discoverControllers(void *data)
{
   /* Get an array of all available devices. */
   int deviceCount;
   unsigned i;
   screen_event_t *event;
   qnx_input_t *qnx = (qnx_input_t*)data;

   (void)event;

   screen_get_context_property_iv(screen_ctx,
         SCREEN_PROPERTY_DEVICE_COUNT, &deviceCount);
   screen_device_t* devices_found = (screen_device_t*)
      calloc(deviceCount, sizeof(screen_device_t));
   screen_get_context_property_pv(screen_ctx,
         SCREEN_PROPERTY_DEVICES, (void**)devices_found);

   /* Scan the list for gamepad and joystick devices. */
   for(i = 0; i < qnx->pads_connected; ++i)
      initController(qnx, &qnx->devices[i]);

   qnx->pads_connected = 0;

   for (i = 0; i < deviceCount; i++)
   {
      int type;
      screen_get_device_property_iv(
            devices_found[i], SCREEN_PROPERTY_TYPE, &type);

      if (
            type == SCREEN_EVENT_GAMEPAD  || 
            type == SCREEN_EVENT_JOYSTICK ||
            type == SCREEN_EVENT_KEYBOARD)
      {
         qnx->devices[qnx->pads_connected].handle = devices_found[i];
         qnx->devices[qnx->pads_connected].index = qnx->pads_connected;
         handle_device(qnx, &qnx->devices[qnx->pads_connected]);

         if (qnx->pads_connected == MAX_PADS)
            break;
      }
   }

   free(devices_found);
}
#endif

static void initController(void *data, input_device_t* controller)
{
   qnx_input_t *qnx = (qnx_input_t*)data;

   if (qnx)
   {
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
}

static void qnx_input_autodetect_gamepad(void *data,
      input_device_t* controller, int port)
{
   char name_buf[256];
   qnx_input_t *qnx = (qnx_input_t*)data;

   if (!qnx)
      return;

   name_buf[0] = '\0';

   /* ID: A-BBBB-CCCC-D.D
    * A is the device's index in the array 
    * returned by screen_get_context_property_pv()
    * BBBB is the device's Vendor ID (in hexadecimal)
    * CCCC is the device's Product ID (also in hexadecimal)
    * D.D is the device's version number
    */
   if (controller)
   {
#ifdef HAVE_BB10
      if (strstr(controller->id, "057E-0306"))
         strlcpy(name_buf, "Wiimote", sizeof(name_buf));
      else
#endif
         if (strstr(controller->id, "0A5C-8502"))
            strlcpy(name_buf, "BlackBerry BT Keyboard", sizeof(name_buf));
#ifdef HAVE_BB10
         else if (strstr(controller->id, "qwerty:bb35"))
            strlcpy(name_buf, "BlackBerry Q10 Keypad", sizeof(name_buf));
#endif
   }

   if (name_buf[0] != '\0')
   {
      strlcpy(g_settings.input.device_names[port],
            name_buf, sizeof(g_settings.input.device_names[port]));

      input_config_autoconfigure_joypad(port, name_buf,
            controller->vid, controller->vid,
            qnx->joypad);

      controller->port = port;
      qnx->port_device[port] = controller;
      qnx->pads_connected++;
   }
}

static void process_keyboard_event(void *data,
      screen_event_t event, int type)
{
   input_device_t* controller = NULL;
   int i, b, sym, modifiers, flags, scan, cap;
   qnx_input_t *qnx = (qnx_input_t*)data;

   (void)type;

   i = 0;
   sym = 0;
   modifiers = 0;
   flags = 0;
   scan = 0;
   cap = 0;

   /* Get Keyboard state. */
   screen_get_event_property_iv(event,
         SCREEN_PROPERTY_KEY_SYM, &sym);
   screen_get_event_property_iv(event,
         SCREEN_PROPERTY_KEY_MODIFIERS, &modifiers);
   screen_get_event_property_iv(event,
         SCREEN_PROPERTY_KEY_FLAGS, &flags);
   screen_get_event_property_iv(event,
         SCREEN_PROPERTY_KEY_SCAN, &scan);
   screen_get_event_property_iv(event,
         SCREEN_PROPERTY_KEY_CAP, &cap);

#ifdef HAVE_BB10
   /* Find device that pressed the key. */
   screen_device_t device;

   screen_get_event_property_pv(event,
         SCREEN_PROPERTY_DEVICE, (void**)&device);

   for (i = 0; i < MAX_PADS; ++i)
   {
      if (device == qnx->devices[i].handle)
      {
         controller = (input_device_t*)&qnx->devices[i];
         break;
      }
   }

   if (!controller)
      return;
#else
   controller = (input_device_t*)&qnx->devices[0];
#endif

   if(controller->port == -1)
      return;

   uint64_t *state_cur = &qnx->pad_state[controller->port];
   *state_cur = 0;

   for (b = 0; b < RARCH_FIRST_CUSTOM_BIND; ++b)
   {
      if ((unsigned int)
            g_settings.input.binds[controller->port][b].joykey 
            == (unsigned int)(sym & 0xFF))
      {
         if (flags & KEY_DOWN)
         {
            controller->buttons |= 1 << b;
            *state_cur |= 1 << b;
         }
         else
            controller->buttons &= ~(1<<b);
      }
   }

   /* TODO: Am I missing something? Is there a better way? */
   if((controller->port == 0) && ((unsigned int)
            g_settings.input.binds[0][RARCH_MENU_TOGGLE].joykey 
            == (unsigned int)(sym&0xFF)))
   {
      if (flags & KEY_DOWN)
         g_extern.lifecycle_state ^= (1ULL << RARCH_MENU_TOGGLE);
   }
}

static void process_touch_event(void *data, screen_event_t event, int type)
{
   int contact_id, pos[2];
   unsigned i, j;
   qnx_input_t *qnx = (qnx_input_t*)data;

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
               qnx->pointer[i].contact_id = contact_id;
               input_translate_coord_viewport(pos[0], pos[1],
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
         printf("New Touch: x:%d, y:%d, id:%d\n", pos[0], pos[1], contact_id);
         fflush(stdout);
         printf("Map: %d %d %d %d %d %d\n", qnx->touch_map[0], qnx->touch_map[1], 
               qnx->touch_map[2], qnx->touch_map[3], qnx->touch_map[4], 
               qnx->touch_map[5]);
         fflush(stdout);
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
         printf("Release: x:%d, y:%d, id:%d\n", pos[0], pos[1], contact_id);
         fflush(stdout);
         printf("Map: %d %d %d %d %d %d\n", qnx->touch_map[0], qnx->touch_map[1], 
               qnx->touch_map[2], qnx->touch_map[3], qnx->touch_map[4], 
               qnx->touch_map[5]);
         fflush(stdout);
#endif
         break;

      case SCREEN_EVENT_MTOUCH_MOVE:
         /* Find the finger we're tracking and update. */
         for(i = 0; i < qnx->pointer_count; ++i)
         {
            if(qnx->pointer[i].contact_id == contact_id)
            {
               gl_t *gl = (gl_t*)driver.video_data;

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

               input_translate_coord_viewport(pos[0], pos[1],
                     &qnx->pointer[i].x, &qnx->pointer[i].y,
                     &qnx->pointer[i].full_x, &qnx->pointer[i].full_y);
#if 0
               printf("Move: x:%d, y:%d, id:%d\n", pos[0], pos[1], 
                     contact_id);
               fflush(stdout);
#endif
               break;
            }
         }
         break;
   }
}

static void handle_screen_event(void *data, bps_event_t *event)
{
   int type;
   qnx_input_t *qnx = (qnx_input_t*)data;

   screen_event_t screen_event = screen_event_get_event(event);
   screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &type);

   switch(type)
   {
      case SCREEN_EVENT_MTOUCH_TOUCH:
      case SCREEN_EVENT_MTOUCH_RELEASE:
      case SCREEN_EVENT_MTOUCH_MOVE:
         process_touch_event(data, screen_event, type);
         break;
      case SCREEN_EVENT_KEYBOARD:
         process_keyboard_event(data, screen_event, type);
         break;
#ifdef HAVE_BB10
      case SCREEN_EVENT_GAMEPAD:
      case SCREEN_EVENT_JOYSTICK:
         process_gamepad_event(data, screen_event, type);
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

            if (attached && (
                     type == SCREEN_EVENT_GAMEPAD ||
                     type == SCREEN_EVENT_JOYSTICK ||
                     type == SCREEN_EVENT_KEYBOARD)
               )
            {
               for (i = 0; i < MAX_PADS; ++i)
               {
                  if (!qnx->devices[i].handle)
                  {
                     qnx->devices[i].handle = device;
                     handle_device(data, &qnx->devices[i]);
                     break;
                  }
               }
            }
            else
            {
               for (i = 0; i < MAX_PADS; ++i)
               {
                  if (device == qnx->devices[i].handle)
                  {
                     RARCH_LOG("Device %s: Disconnected.\n",
                           qnx->devices[i].id);
                     initController(data, &qnx->devices[i]);
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

static void handle_navigator_event(void *data, bps_event_t *event)
{
   navigator_window_state_t state;
   bps_event_t *event_pause = NULL;
   int rc;

   (void)data;
   (void)rc;

   switch (bps_event_get_code(event))
   {
      case NAVIGATOR_SWIPE_DOWN:
    	  g_extern.lifecycle_state ^= (1ULL << RARCH_MENU_TOGGLE);
         break;
      case NAVIGATOR_EXIT:
         /* Catch this in thumbnail loop. */
         break;
      case NAVIGATOR_WINDOW_STATE:
         state = navigator_event_get_window_state(event);

         switch(state)
         {
            case NAVIGATOR_WINDOW_THUMBNAIL:
               for(;;)
               {
                  /* Block until we get a resume or exit event. */
                  rc = bps_get_event(&event_pause, -1);

                  if(bps_event_get_code(event_pause) == NAVIGATOR_WINDOW_STATE)
                  {
                     state = navigator_event_get_window_state(event_pause);
                     if(state == NAVIGATOR_WINDOW_FULLSCREEN)
                        break;
                  }
                  else if (bps_event_get_code(event_pause) == NAVIGATOR_EXIT)
                  {
                     g_extern.system.shutdown = true;
                     break;
                  }
               }
               break;
            case NAVIGATOR_WINDOW_FULLSCREEN:
               break;
            case NAVIGATOR_WINDOW_INVISIBLE:
               break;
         }
         break;
      default:
         break;
   }
}

static void *qnx_input_init(void)
{
   int i;
   qnx_input_t *qnx = (qnx_input_t*)calloc(1, sizeof(*qnx));
   if (!qnx)
      return NULL;

   for (i = 0; i < MAX_TOUCH; ++i)
   {
      qnx->pointer[i].contact_id = -1;
      qnx->touch_map[i] = -1;
   }

   qnx->joypad = input_joypad_init_driver(g_settings.input.joypad_driver);

   for (i = 0; i < MAX_PADS; ++i)
   {
      initController(qnx, &qnx->devices[i]);
      qnx->port_device[i] = 0;
   }

#ifdef HAVE_BB10
   /* Find currently connected gamepads. */
   discoverControllers(qnx);
#else
   /* Initialize Playbook keyboard. */
   strlcpy(qnx->devices[0].id, "0A5C-8502",
         sizeof(qnx->devices[0].id));
   qnx_input_autodetect_gamepad(qnx, &qnx->devices[0], 0);
   qnx->pads_connected = 1;
#endif

   return qnx;
}

static void qnx_input_poll(void *data)
{
   (void)data;
   /* Request and process all available BPS events. */

   int rc, domain;

   g_extern.lifecycle_state &= ~(1ULL << RARCH_MENU_TOGGLE);

   while(true)
   {
      bps_event_t *event = NULL;
      rc = bps_get_event(&event, 0);
      if(rc == BPS_SUCCESS)
      {
         if (event)
         {
            domain = bps_event_get_domain(event);
            if (domain == navigator_get_domain())
               handle_navigator_event(data, event);
            else if (domain == screen_get_domain())
               handle_screen_event(data, event);
         }
         else
            break;
      }
   }
}

/* Need to return [-0x8000, 0x7fff].
 * Gamepad API gives us [-128, 127] with (0,0) center
 * Untested
 */

static int16_t qnx_analog_input_state(qnx_input_t *qnx,
      unsigned port, unsigned index, unsigned id)
{
#ifdef HAVE_BB10
   if(qnx->port_device[port])
   {
      switch ((index << 1) | id)
      {
         case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_X:
            return qnx->port_device[port]->analog0[0] * 256;
         case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_Y:
            return qnx->port_device[port]->analog0[1] * 256;
         case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_X:
            return qnx->port_device[port]->analog1[0] * 256;
         case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_Y:
            return qnx->port_device[port]->analog1[1] * 256;
      }
   }
#endif

   return 0;
}

static int16_t qnx_pointer_screen_input_state(qnx_input_t *qnx,
      unsigned index, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_X:
         return qnx->pointer[qnx->touch_map[index]].full_x;
      case RETRO_DEVICE_ID_POINTER_Y:
         return qnx->pointer[qnx->touch_map[index]].full_y;
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return (
               index < qnx->pointer_count)
            && (qnx->pointer[index].full_x != -0x8000) 
            && (qnx->pointer[index].full_y != -0x8000);
   }

   return 0;
}

static int16_t qnx_pointer_input_state(qnx_input_t *qnx,
      unsigned index, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_X:
         return qnx->pointer[qnx->touch_map[index]].x;
      case RETRO_DEVICE_ID_POINTER_Y:
         return qnx->pointer[qnx->touch_map[index]].y;
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return (
               index < qnx->pointer_count)
            && (qnx->pointer[index].x != -0x8000)
            && (qnx->pointer[index].y != -0x8000);
   }

   return 0;
}

static int16_t qnx_input_state(void *data,
      const struct retro_keybind **retro_keybinds,
      unsigned port, unsigned device, unsigned index, unsigned id)
{
   qnx_input_t *qnx = (qnx_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return input_joypad_pressed(qnx->joypad, port,
               (unsigned int)g_settings.input.binds[port], id);
      case RETRO_DEVICE_ANALOG:
         return qnx_analog_input_state(qnx, port, index, id);
      case RARCH_DEVICE_POINTER_SCREEN:
         return qnx_pointer_screen_input_state(qnx, index, id);
      case RETRO_DEVICE_POINTER:
         return qnx_pointer_input_state(qnx, index, id);
   }

   return 0;
}

static bool qnx_input_key_pressed(void *data, int key)
{
   qnx_input_t *qnx = (qnx_input_t*)data;
   return ((g_extern.lifecycle_state | driver.overlay_state.buttons ) & (1ULL << key) ||
         input_joypad_pressed(qnx->joypad, 0, g_settings.input.binds[0], key));
}

static void qnx_input_free_input(void *data)
{
   free(data);
}

static uint64_t qnx_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   (void)data;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_POINTER);
#ifdef HAVE_BB10
   caps |= (1 << RETRO_DEVICE_ANALOG);
#endif

   return caps;
}

static const rarch_joypad_driver_t *qnx_input_get_joypad_driver(void *data)
{
   qnx_input_t *qnx = (qnx_input_t*)data;
   return qnx->joypad;
}

input_driver_t input_qnx = {
   qnx_input_init,
   qnx_input_poll,
   qnx_input_state,
   qnx_input_key_pressed,
   qnx_input_free_input,
   NULL,
   NULL,
   qnx_input_get_capabilities,
   "qnx_input",
   NULL,
   NULL,
   qnx_input_get_joypad_driver,
};
