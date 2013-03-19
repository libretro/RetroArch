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

#include "../../general.h"
#include "../../driver.h"
#include <screen/screen.h>

#define MAX_TOUCH 16

struct touches
{
   int16_t x, y;
   int16_t full_x, full_y;
   int contact_id;
};

static struct touches touch[MAX_TOUCH];
static unsigned touch_count;

//Internal helper functions
static void process_touch_event(screen_event_t event, int type){
   int contact_id;
   int pos[2];
   int i;

   screen_get_event_property_iv(event, SCREEN_PROPERTY_TOUCH_ID, (int*)&contact_id);
   screen_get_event_property_iv(event, SCREEN_PROPERTY_SOURCE_POSITION, pos);

   switch(type){
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
      for(i=0; i<touch_count; ++i){
         if(touch[i].contact_id == contact_id){
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

   switch(type){
   case SCREEN_EVENT_MTOUCH_TOUCH:
   case SCREEN_EVENT_MTOUCH_RELEASE:
   case SCREEN_EVENT_MTOUCH_MOVE:
      process_touch_event(screen_event, type);
      break;
   case SCREEN_EVENT_KEYBOARD:
      break;
#ifdef HAVE_BB10
   case SCREEN_EVENT_GAMEPAD:
   case SCREEN_EVENT_JOYSTICK:
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

      if (attached) {
         screen_get_device_property_iv(device,
            SCREEN_PROPERTY_TYPE, &type);
      }

      int i;
      if (attached && (type == SCREEN_EVENT_GAMEPAD ||
         type == SCREEN_EVENT_JOYSTICK)) {
          //Load controller
      } else {
         //Remove controller
      }
      break;
   }
#endif
   }
}

static void handle_navigator_event(bps_event_t *event) {
   navigator_window_state_t state;
   bps_event_t *event_pause = NULL;
   int rc;

   switch (bps_event_get_code(event)) {
   case NAVIGATOR_SWIPE_DOWN:
      break;
   case NAVIGATOR_EXIT:
      //Catch this in thumbnail loop
      break;
   case NAVIGATOR_WINDOW_STATE:
      state = navigator_event_get_window_state(event);

      switch(state){
      case NAVIGATOR_WINDOW_THUMBNAIL:
         for(;;){
            //Block until we get a resume or exit event
            rc = bps_get_event(&event_pause, -1);

            if(bps_event_get_code(event_pause) == NAVIGATOR_WINDOW_STATE){
                state = navigator_event_get_window_state(event_pause);
                if(state == NAVIGATOR_WINDOW_FULLSCREEN){
                   break;
                }
             } else if (bps_event_get_code(event_pause) == NAVIGATOR_EXIT){
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

   for(i=0; i<MAX_TOUCH; ++i){
      touch[i].contact_id = -1;
   }

   return (void*)-1;
}

static void qnx_input_poll(void *data)
{
   (void)data;
   //Request and process all available BPS events

   int rc, domain;

   while(true){
      bps_event_t *event = NULL;
      rc = bps_get_event(&event, 0);
      if(rc == BPS_SUCCESS)
      {
         if (event) {
            domain = bps_event_get_domain(event);
            if (domain == navigator_get_domain()) {
               handle_navigator_event(event);
            } else if (domain == screen_get_domain()) {
               handle_screen_event(event);
            }
         } else {
            break;
         }
      }
   }
}

static int16_t qnx_input_state(void *data, const struct retro_keybind **retro_keybinds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   (void)data;
   (void)retro_keybinds;
   (void)port;
   (void)device;
   (void)index;
   (void)id;

   switch (device)
   {
   case RETRO_DEVICE_JOYPAD:
      return false;
   case RARCH_DEVICE_POINTER_SCREEN:
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
   return ((g_extern.lifecycle_state | driver.overlay_state) & (1ULL << key));
}

static void qnx_input_free_input(void *data)
{
   (void)data;
}

static void qnx_set_keybinds(void *data, unsigned device,
      unsigned port, unsigned id, unsigned keybind_action)
{
   (void)data;
   (void)device;
   (void)port;
   (void)id;
   (void)keybind_action;
}

const input_driver_t input_qnx = {
	qnx_input_init,
	qnx_input_poll,
	qnx_input_state,
	qnx_input_key_pressed,
	qnx_input_free_input,
	qnx_set_keybinds,
    "qnx_input",
};

