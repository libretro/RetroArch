/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "frontend_qnx.h"

#define MAX_TOUCH 16

struct touches
{
   int16_t x, y;
   int16_t full_x, full_y;
   int contact_id;
};

static struct touches touch[MAX_TOUCH];
static unsigned touch_count;

input_device_t devices[MAX_PADS];
input_device_t *port_device[MAX_PADS];

unsigned pads_connected;

static void qnx_input_autodetect_gamepad(input_device_t* controller);
static void initController(input_device_t* controller);

#ifdef HAVE_BB10
static void process_gamepad_event(screen_event_t screen_event, int type)
{
   screen_device_t device;
   screen_get_event_property_pv(screen_event, SCREEN_PROPERTY_DEVICE, (void**)&device);

   input_device_t* controller = NULL;
   int i;
   for (i = 0; i < MAX_PADS; ++i)
   {
      if (device == devices[i].handle)
      {
         controller = &devices[i];
         break;
      }
   }

   if (!controller)
      return;

   // Store the controller's new state.
   screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_BUTTONS, &controller->buttons);

   if (controller->analogCount > 0)
      screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_ANALOG0, controller->analog0);

   if (controller->analogCount == 2)
      screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_ANALOG1, controller->analog1);

   //Only player 1
   //TODO: Am I missing something? Is there a better way?
   if((controller->port == 0) && (controller->buttons & g_settings.input.binds[0][RARCH_MENU_TOGGLE].joykey))
      g_extern.lifecycle_state ^= (1ULL << RARCH_MENU_TOGGLE);
}

static void loadController(input_device_t* controller)
{
   int device;

   // Query libscreen for information about this device.
   screen_get_device_property_iv(controller->handle, SCREEN_PROPERTY_TYPE, &controller->type);
   screen_get_device_property_cv(controller->handle, SCREEN_PROPERTY_ID_STRING, sizeof(controller->id), controller->id);
   screen_get_device_property_cv(controller->handle, SCREEN_PROPERTY_VENDOR, sizeof(controller->id), controller->vendor);
   screen_get_device_property_cv(controller->handle, SCREEN_PROPERTY_PRODUCT, sizeof(controller->id), controller->product);

   if (controller->type == SCREEN_EVENT_GAMEPAD || controller->type == SCREEN_EVENT_JOYSTICK)
   {
      screen_get_device_property_iv(controller->handle, SCREEN_PROPERTY_BUTTON_COUNT, &controller->buttonCount);

      // Check for the existence of analog sticks.
      if (!screen_get_device_property_iv(controller->handle, SCREEN_PROPERTY_ANALOG0, controller->analog0))
         ++controller->analogCount;

      if (!screen_get_device_property_iv(controller->handle, SCREEN_PROPERTY_ANALOG1, controller->analog1))
         ++controller->analogCount;
   }

   //Screen service will map supported controllers, we still might need to adjust.
   qnx_input_autodetect_gamepad(controller);

   if (controller->type == SCREEN_EVENT_GAMEPAD)
      RARCH_LOG("Gamepad Device Connected:\n");
   else if (controller->type == SCREEN_EVENT_JOYSTICK)
      RARCH_LOG("Joystick Device Connected:\n");
   else if (controller->type == SCREEN_EVENT_KEYBOARD)
      RARCH_LOG("Keyboard Device Connected:\n");

   RARCH_LOG("\tID: %s\n", controller->id);
   RARCH_LOG("\tVendor: %s\n", controller->vendor);
   RARCH_LOG("\tProduct: %s\n", controller->product);
   RARCH_LOG("\tButton Count: %d\n", controller->buttonCount);
   RARCH_LOG("\tAnalog Count: %d\n", controller->analogCount);
}

extern screen_context_t screen_ctx;
void discoverControllers()
{
   // Get an array of all available devices.
   int deviceCount;
   screen_get_context_property_iv(screen_ctx, SCREEN_PROPERTY_DEVICE_COUNT, &deviceCount);
   screen_device_t* devices_found = (screen_device_t*)calloc(deviceCount, sizeof(screen_device_t));
   screen_get_context_property_pv(screen_ctx, SCREEN_PROPERTY_DEVICES, (void**)devices_found);

   // Scan the list for gamepad and joystick devices.
   int i;

   for(i=0;i<pads_connected;++i)
      initController(&devices[i]);

   pads_connected = 0;

   for (i = 0; i < deviceCount; i++)
   {
      int type;
      screen_get_device_property_iv(devices_found[i], SCREEN_PROPERTY_TYPE, &type);

      if (type == SCREEN_EVENT_GAMEPAD || type == SCREEN_EVENT_JOYSTICK || type == SCREEN_EVENT_KEYBOARD)
      {
         devices[pads_connected].handle = devices_found[i];
         loadController(&devices[pads_connected]);

         pads_connected++;
         if (pads_connected == MAX_PADS)
            break;
      }
   }

   free(devices_found);
}
#else
void init_playbook_keyboard()
{
   strlcpy(devices[0].id, "0A5C-8502", sizeof(devices[0].id));
   qnx_input_autodetect_gamepad(&devices[0]);
   pads_connected = 1;
}
#endif

static void initController(input_device_t* controller)
{
    // Initialize controller values.
#ifdef HAVE_BB10
    controller->handle = 0;
#endif
    controller->type = 0;
    controller->analogCount = 0;
    controller->buttonCount = 0;
    controller->buttons = 0;
    controller->analog0[0] = controller->analog0[1] = controller->analog0[2] = 0;
    controller->analog1[0] = controller->analog1[1] = controller->analog1[2] = 0;
    controller->port = -1;
    controller->device = -1;
    memset(controller->id, 0, sizeof(controller->id));
}

static void qnx_input_autodetect_gamepad(input_device_t* controller)
{
   //ID: A-BBBB-CCCC-D.D
   //A is the device's index in the array returned by screen_get_context_property_pv()
   //BBBB is the device's Vendor ID (in hexadecimal)
   //CCCC is the device's Product ID (also in hexadecimal)
   //D.D is the device's version number
   if (strstr(controller->id, "057E-0306"))
   {
      controller->device = DEVICE_WIIMOTE;
      strlcpy(controller->device_name, "Wiimote", sizeof(controller->device_name));
   }
   else if (strstr(controller->id, "0A5C-8502"))
   {
      controller->device = DEVICE_KEYBOARD;
      strlcpy(controller->device_name, "BlackBerry BT Keyboard", sizeof(controller->device_name));
   }
   else if (strstr(controller->id, "qwerty:bb35"))
   {
      controller->device = DEVICE_KEYPAD;
      strlcpy(controller->device_name, "BlackBerry Q10 Keypad", sizeof(controller->device_name));
   }
   else if (strstr(controller->id, "BB-VKB"))
   {
      controller->device = DEVICE_NONE;
      strlcpy(controller->device_name, "None", sizeof(controller->device_name));
   }
   else if (controller->id[0])
   {
      controller->device = DEVICE_UNKNOWN;
      strlcpy(controller->device_name, "Unknown", sizeof(controller->device_name));
   }
   else
   {
      controller->device = DEVICE_NONE;
      strlcpy(controller->device_name, "None", sizeof(controller->device_name));
   }

   if (input_qnx.set_keybinds)
      input_qnx.set_keybinds((void*)controller, controller->device, pads_connected, 0,
            (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS));
}

static void process_keyboard_event(screen_event_t event, int type)
{
   input_device_t* controller = NULL;
   int i = 0;

   //Get Keyboard state
   int sym = 0;
   screen_get_event_property_iv(event, SCREEN_PROPERTY_KEY_SYM, &sym);
   int modifiers = 0;
   screen_get_event_property_iv(event, SCREEN_PROPERTY_KEY_MODIFIERS, &modifiers);
   int flags = 0;
   screen_get_event_property_iv(event, SCREEN_PROPERTY_KEY_FLAGS, &flags);
   int scan = 0;
   screen_get_event_property_iv(event, SCREEN_PROPERTY_KEY_SCAN, &scan);
   int cap = 0;
   screen_get_event_property_iv(event, SCREEN_PROPERTY_KEY_CAP, &cap);

#ifdef HAVE_BB10
   //Find device that pressed the key
   screen_device_t device;
   screen_get_event_property_pv(event, SCREEN_PROPERTY_DEVICE, (void**)&device);

   for (i = 0; i < MAX_PADS; ++i)
   {
      if (device == devices[i].handle)
      {
         controller = &devices[i];
         break;
      }
   }

   if (!controller)
      return;
#else
   controller = &devices[0];
#endif

   if(controller->port == -1)
      return;

   int b;
   for (b = 0; b < RARCH_FIRST_CUSTOM_BIND; ++b)
   {
      if ((unsigned int)g_settings.input.binds[controller->port][b].joykey == (unsigned int)(sym&0xFF))
      {
         if (flags & KEY_DOWN)
            controller->buttons |= 1 << b;
         else
            controller->buttons &= ~(1<<b);
      }

   }

   //TODO: Am I missing something? Is there a better way?
   if((controller->port == 0) && ((unsigned int)g_settings.input.binds[0][RARCH_MENU_TOGGLE].joykey == (unsigned int)(sym&0xFF)))
      if (flags & KEY_DOWN)
         g_extern.lifecycle_state ^= (1ULL << RARCH_MENU_TOGGLE);
}

static void process_touch_event(screen_event_t event, int type)
{
   int contact_id;
   int pos[2];
   int i;

   screen_get_event_property_iv(event, SCREEN_PROPERTY_TOUCH_ID, (int*)&contact_id);
   screen_get_event_property_iv(event, SCREEN_PROPERTY_SOURCE_POSITION, pos);

   switch(type)
   {
      case SCREEN_EVENT_MTOUCH_TOUCH:
         touch[touch_count].contact_id = contact_id;
         input_translate_coord_viewport(pos[0], pos[1],
               &touch[touch_count].x, &touch[touch_count].y,
               &touch[touch_count].full_x, &touch[touch_count].full_y);
         touch_count++;
         //printf("New Touch: x:%d, y:%d, id:%d\n", pos[0], pos[1], contact_id);fflush(stdout);
         break;
      case SCREEN_EVENT_MTOUCH_RELEASE:
         //Invalidate the finger
         touch_count--;
         touch[touch_count].contact_id = -1;
         input_translate_coord_viewport(pos[0], pos[1],
               &touch[touch_count].x, &touch[touch_count].y,
               &touch[touch_count].full_x, &touch[touch_count].full_y);
         //printf("Release: x:%d, y:%d, id:%d\n", pos[0], pos[1], contact_id);fflush(stdout);
         break;
      case SCREEN_EVENT_MTOUCH_MOVE:
         //Find the finger we're tracking and update
         for(i=0; i<touch_count; ++i)
         {
            if(touch[i].contact_id == contact_id)
            {
               input_translate_coord_viewport(pos[0], pos[1],
                     &touch[i].x, &touch[i].y,
                     &touch[i].full_x, &touch[i].full_y);
               //printf("Move: x:%d, y:%d, id:%d\n", pos[0], pos[1], contact_id);fflush(stdout);
            }
         }
         break;
   }
}

static void handle_screen_event(bps_event_t *event)
{
   int type;

   screen_event_t screen_event = screen_event_get_event(event);
   screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &type);

   switch(type)
   {
      case SCREEN_EVENT_MTOUCH_TOUCH:
      case SCREEN_EVENT_MTOUCH_RELEASE:
      case SCREEN_EVENT_MTOUCH_MOVE:
         process_touch_event(screen_event, type);
         break;
      case SCREEN_EVENT_KEYBOARD:
         process_keyboard_event(screen_event, type);
         break;
#ifdef HAVE_BB10
      case SCREEN_EVENT_GAMEPAD:
      case SCREEN_EVENT_JOYSTICK:
         process_gamepad_event(screen_event, type);
         break;
      case SCREEN_EVENT_DEVICE:
         {
            // A device was attached or removed.
            screen_device_t device;
            int attached;
            int type;

            screen_get_event_property_pv(screen_event,
                  SCREEN_PROPERTY_DEVICE, (void**)&device);
            screen_get_event_property_iv(screen_event,
                  SCREEN_PROPERTY_ATTACHED, &attached);

            if (attached)
               screen_get_device_property_iv(device,
                     SCREEN_PROPERTY_TYPE, &type);

            int i;

            if (attached && (type == SCREEN_EVENT_GAMEPAD || type == SCREEN_EVENT_JOYSTICK || type == SCREEN_EVENT_KEYBOARD))
            {
               for (i = 0; i < MAX_PADS; ++i)
               {
                  if (!devices[i].handle)
                  {
                     devices[i].handle = device;
                     loadController(&devices[i]);
                     break;
                  }
               }
            }
            else
            {
               for (i = 0; i < MAX_PADS; ++i)
               {
                  if (device == devices[i].handle)
                  {
                     RARCH_LOG("Device %s: Disconnected.\n", devices[i].id);
                     initController(&devices[i]);
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

static void handle_navigator_event(bps_event_t *event)
{
   navigator_window_state_t state;
   bps_event_t *event_pause = NULL;
   int rc;

   switch (bps_event_get_code(event))
   {
      case NAVIGATOR_SWIPE_DOWN:
    	  g_extern.lifecycle_state ^= (1ULL << RARCH_MENU_TOGGLE);
         break;
      case NAVIGATOR_EXIT:
         //Catch this in thumbnail loop
         break;
      case NAVIGATOR_WINDOW_STATE:
         state = navigator_event_get_window_state(event);

         switch(state)
         {
            case NAVIGATOR_WINDOW_THUMBNAIL:
               for(;;)
               {
                  //Block until we get a resume or exit event
                  rc = bps_get_event(&event_pause, -1);

                  if(bps_event_get_code(event_pause) == NAVIGATOR_WINDOW_STATE)
                  {
                     state = navigator_event_get_window_state(event_pause);
                     if(state == NAVIGATOR_WINDOW_FULLSCREEN)
                        break;
                  }
                  else if (bps_event_get_code(event_pause) == NAVIGATOR_EXIT)
                  {
                     g_extern.lifecycle_state |= (1ULL << RARCH_QUIT_KEY);
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

//External Functions
static void *qnx_input_init(void)
{
   int i;
   static int initialized = 0;

   if(initialized)
      return (void*)-1;

   for (i = 0; i < MAX_TOUCH; ++i)
      touch[i].contact_id = -1;

   for (i = 0; i < MAX_PADS; ++i)
   {
      initController(&devices[i]);
      port_device[i] = 0;
   }
#ifdef HAVE_BB10
   //Find currently connected gamepads
   discoverControllers();
#else
   init_playbook_keyboard();
#endif

   initialized = 1;

   return (void*)-1;
}

static void qnx_input_poll(void *data)
{
   (void)data;
   //Request and process all available BPS events

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
               handle_navigator_event(event);
            else if (domain == screen_get_domain())
               handle_screen_event(event);
         }
         else
            break;
      }
   }
}

static int16_t qnx_input_state(void *data, const struct retro_keybind **retro_keybinds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   (void)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if(port_device[port])
         {
            if (port_device[port]->device == DEVICE_KEYBOARD || port_device[port]->device == DEVICE_KEYPAD)
               return ((port_device[port]->buttons & (1 << id)) && (port < pads_connected) );
            else{
               return ((port_device[port]->buttons & retro_keybinds[port][id].joykey) && (port < pads_connected));
            }
         }
#ifdef HAVE_BB10
      case RETRO_DEVICE_ANALOG:
         //Need to return [-0x8000, 0x7fff]
         //Gamepad API gives us [-128, 127] with (0,0) center
         //Untested
         if(port_device[port])
         {
            switch ((index << 1) | id)
            {
               case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_X:
                  return port_device[port]->analog0[0] * 256;
               case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_Y:
                  return port_device[port]->analog0[1] * 256;
               case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_X:
                  return port_device[port]->analog1[0] * 256;
               case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_Y:
                  return port_device[port]->analog1[1] * 256;
               default:
                  break;
            }
         }
         break;
#endif
      case RARCH_DEVICE_POINTER_SCREEN:
      case RETRO_DEVICE_POINTER:
         {
            switch (id)
            {
               case RETRO_DEVICE_ID_POINTER_X: return touch[index].full_x;
               case RETRO_DEVICE_ID_POINTER_Y: return touch[index].full_y;
               case RETRO_DEVICE_ID_POINTER_PRESSED: return (touch[index].contact_id != -1);
            }

            return 0;
         }
      default:
         return 0;
   }

   return 0;
}

static bool qnx_input_key_pressed(void *data, int key)
{
   return ((g_extern.lifecycle_state | driver.overlay_state ) & (1ULL << key));
}

static void qnx_input_free_input(void *data)
{
   (void)data;
}

static void qnx_input_set_keybinds(void *data, unsigned device, unsigned port,
      unsigned id, unsigned keybind_action)
{
   input_device_t *controller = (input_device_t*)data;
#ifdef HAVE_BB10
   uint64_t *key = &g_settings.input.binds[port][id].joykey;
   uint64_t joykey = *key;
   size_t arr_size = sizeof(platform_keys) / sizeof(platform_keys[0]);

   (void)device;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_DECREMENT_BIND))
   {
      if (joykey == NO_BTN)
         *key = platform_keys[arr_size - 1].joykey;
      else if (platform_keys[0].joykey == joykey)
         *key = NO_BTN;
      else
      {
         *key = NO_BTN;
         for (size_t i = 1; i < arr_size; i++)
         {
            if (platform_keys[i].joykey == joykey)
            {
               *key = platform_keys[i - 1].joykey;
               break;
            }
         }
      }
   }

   if (keybind_action & (1ULL << KEYBINDS_ACTION_INCREMENT_BIND))
   {
      if (joykey == NO_BTN)
         *key = platform_keys[0].joykey;
      else if (platform_keys[arr_size - 1].joykey == joykey)
         *key = NO_BTN;
      else
      {
         *key = NO_BTN;
         for (size_t i = 0; i < arr_size - 1; i++)
         {
            if (platform_keys[i].joykey == joykey)
            {
               *key = platform_keys[i + 1].joykey;
               break;
            }
         }
      }
   }

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BIND))
      *key = g_settings.input.binds[port][id].def_joykey;
#endif
   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS))
   {
      switch (device)
      {
#ifdef HAVE_BB10
         case DEVICE_WIIMOTE:
            //TODO:Have enum lookup for string
            strlcpy(g_settings.input.device_names[port], "Wiimote",
               sizeof(g_settings.input.device_names[port]));
            g_settings.input.device[port] = device;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey      = SCREEN_X_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey      = SCREEN_B_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey = SCREEN_MENU1_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey  = SCREEN_MENU2_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey     = SCREEN_DPAD_UP_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey   = SCREEN_DPAD_DOWN_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey   = SCREEN_DPAD_LEFT_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey  = SCREEN_DPAD_RIGHT_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey      = SCREEN_Y_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey      = SCREEN_A_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey      = SCREEN_L1_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey      = SCREEN_R1_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey     = SCREEN_L2_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey     = SCREEN_R2_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey     = SCREEN_L3_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey     = SCREEN_R3_GAME_BUTTON;
            g_settings.input.binds[port][RARCH_MENU_TOGGLE].def_joykey             = SCREEN_MENU3_GAME_BUTTON;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            controller->port = port;
            port_device[port] = controller;
            break;
         case DEVICE_KEYPAD:
            strlcpy(g_settings.input.device_names[port], "BlackBerry Q10 Keypad",
               sizeof(g_settings.input.device_names[port]));
            g_settings.input.device[port] = device;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey      = KEYCODE_M & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey      = KEYCODE_J & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey = KEYCODE_RIGHT_SHIFT & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey  = KEYCODE_RETURN & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey     = KEYCODE_W & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey   = KEYCODE_S & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey   = KEYCODE_A & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey  = KEYCODE_D & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey      = KEYCODE_N & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey      = KEYCODE_K & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey      = KEYCODE_U & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey      = KEYCODE_I & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey     = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey     = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey     = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey     = 0;
            g_settings.input.binds[port][RARCH_MENU_TOGGLE].def_joykey             = KEYCODE_P & 0xFF;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            controller->port = port;
            port_device[port] = controller;
            break;
#endif
         case DEVICE_KEYBOARD:
            strlcpy(g_settings.input.device_names[port], "BlackBerry BT Keyboard",
               sizeof(g_settings.input.device_names[port]));
            g_settings.input.device[port] = device;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey      = KEYCODE_Z & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey      = KEYCODE_A & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey = KEYCODE_RIGHT_SHIFT & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey  = KEYCODE_RETURN & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey     = KEYCODE_UP & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey   = KEYCODE_DOWN & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey   = KEYCODE_LEFT & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey  = KEYCODE_RIGHT & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey      = KEYCODE_X & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey      = KEYCODE_S & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey      = KEYCODE_Q & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey      = KEYCODE_W & 0xFF;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey     = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey     = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey     = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey     = 0;
            g_settings.input.binds[port][RARCH_MENU_TOGGLE].def_joykey             = KEYCODE_TILDE;
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            controller->port = port;
            port_device[port] = controller;
            break;
#ifdef HAVE_BB10
         case DEVICE_UNKNOWN:
            strlcpy(g_settings.input.device_names[port], "Unknown",
               sizeof(g_settings.input.device_names[port]));
            g_settings.input.device[port] = device;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey      = SCREEN_B_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey      = SCREEN_Y_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey = SCREEN_MENU1_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey  = SCREEN_MENU2_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey     = SCREEN_DPAD_UP_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey   = SCREEN_DPAD_DOWN_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey   = SCREEN_DPAD_LEFT_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey  = SCREEN_DPAD_RIGHT_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey      = SCREEN_A_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey      = SCREEN_X_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey      = SCREEN_L1_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey      = SCREEN_R1_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey     = SCREEN_L2_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey     = SCREEN_R2_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey     = SCREEN_L3_GAME_BUTTON;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey     = SCREEN_R3_GAME_BUTTON;
            g_settings.input.binds[port][RARCH_MENU_TOGGLE].def_joykey             = 0; //TODO: Find a good mappnig
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_NONE;
            controller->port = port;
            port_device[port] = controller;
            break;
         case DEVICE_NONE:
         default:
            strlcpy(g_settings.input.device_names[port], "None",
               sizeof(g_settings.input.device_names[port]));
            g_settings.input.device[port] = device;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_B].def_joykey      = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_Y].def_joykey      = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_SELECT].def_joykey = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_START].def_joykey  = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_UP].def_joykey     = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_DOWN].def_joykey   = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_LEFT].def_joykey   = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_RIGHT].def_joykey  = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_A].def_joykey      = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_X].def_joykey      = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L].def_joykey      = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R].def_joykey      = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L2].def_joykey     = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R2].def_joykey     = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_L3].def_joykey     = 0;
            g_settings.input.binds[port][RETRO_DEVICE_ID_JOYPAD_R3].def_joykey     = 0;
            g_settings.input.binds[port][RARCH_MENU_TOGGLE].def_joykey     = 0;
            controller->port = -1;
            port_device[port] = 0;
            break;
#endif
      }

      for (unsigned i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
      {
         g_settings.input.binds[port][i].id = i;
         g_settings.input.binds[port][i].joykey = g_settings.input.binds[port][i].def_joykey;
      }

      g_settings.input.binds[port][RARCH_MENU_TOGGLE].id = RARCH_MENU_TOGGLE;
      g_settings.input.binds[port][RARCH_MENU_TOGGLE].joykey = g_settings.input.binds[port][RARCH_MENU_TOGGLE].def_joykey;
   }

#ifdef HAVE_BB10
   //TODO: Handle keyboard mappings
   if (keybind_action & (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL))
   {
      struct platform_bind *ret = (struct platform_bind*)data;

      if (ret->joykey == NO_BTN)
         strlcpy(ret->desc, "No button", sizeof(ret->desc));
      else
      {
         for (size_t i = 0; i < arr_size; i++)
         {
            if (platform_keys[i].joykey == ret->joykey)
            {
               strlcpy(ret->desc, platform_keys[i].desc, sizeof(ret->desc));
               return;
            }
         }
         strlcpy(ret->desc, "Unknown", sizeof(ret->desc));
      }
   }
#endif
}

const input_driver_t input_qnx = {
   qnx_input_init,
   qnx_input_poll,
   qnx_input_state,
   qnx_input_key_pressed,
   qnx_input_free_input,
   qnx_input_set_keybinds,
   "qnx_input",
};

