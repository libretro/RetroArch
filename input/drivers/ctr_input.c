/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../configuration.h"
//#include "../../command.h"
//#include "../../driver.h"

//#include "../../retroarch.h"
//#include "../../verbosity.h"

#include "../input_driver.h"
#include "../input_keymaps.h"

#include "../../ctr/ctr-bottom/ctr_bottom.h"

#include "../../input/common/ctr_common.h"
#include "../../gfx/common/ctr_defines.h"

static bool ctr_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate);

#ifdef HAVE_QTM
	u32 qtm_pos;
	u32 qtm_x, qtm_y;
	Result ret;
	bool qtm_usable;
	QTM_HeadTrackingInfo qtminfo;
#endif

static bool keyboard_state[RETROK_LAST] = { 0 };

/*
static int16_t ctr_translate_coords(ctr_input_t *ctr,
      unsigned port, unsigned id)
{
   struct video_viewport vp;
   const int edge_detect = 32700;
   bool inside           = false;
   int16_t res_x         = 0;
   int16_t res_y         = 0;
   int16_t res_screen_x  = 0;
   int16_t res_screen_y  = 0;

   vp.x                  = 0;
   vp.y                  = 0;
   vp.width              = 0;
   vp.height             = 0;
   vp.full_width         = 0;
   vp.full_height        = 0;

   if (!(video_driver_translate_coord_viewport_wrap(
               &vp, mouse->x, mouse->y,
               &res_x, &res_y, &res_screen_x, &res_screen_y)))
      return 0;

   inside =    (res_x >= -edge_detect) 
            && (res_y >= -edge_detect)
            && (res_x <= edge_detect)
            && (res_y <= edge_detect);

   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
         if (inside)
            return res_x;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
         if (inside)
            return res_y;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
         return !inside;
      default:
         break;
   }

   return 0;
}
*/

static void ctr_input_poll_sensor(void *data)
{
   ctr_input_t *ctr     = (ctr_input_t*)data;

   u32 kDown = hidKeysHeld();

#ifdef HAVE_GYRO
   angularRate rate;
   hidGyroRead (&rate);

   ctr->gyroscope_state.x = rate.x;
   ctr->gyroscope_state.y = rate.y;
   ctr->gyroscope_state.z = rate.z;

// accelerometer only availableble on new3DS?
   accelVector vector;
   hidAccelRead (&vector);

   ctr->accelerometer_state.x = vector.x;
   ctr->accelerometer_state.y = vector.y;
   ctr->accelerometer_state.z = vector.z;

#endif


#ifdef HAVE_QTM
   if(qtm_usable)
   {
float range       = 65534.0f; //32767.0f;



   if (ctr->qtm_state.rel_x == 0)
   {
      ctr->qtm_state.rel_x      = 25000;
      ctr->qtm_state.rel_y      = 25000;
      ctr->qtm_state.multiplier = 1;
   }
   
      ret = QTM_GetHeadTrackingInfo(0, &qtminfo);
      if(ret==0)
      {
         if(qtmCheckHeadFullyDetected(&qtminfo))
         {
//					for(pos=0; pos<4; pos++)
//					{
//						ret = qtmConvertCoordToScreen(&qtminfo.coords0[pos], 400.0f, 320.0f, &x, &y);

//            ret = qtmConvertCoordToScreen(&qtminfo.coords0[0], NULL, NULL, &qtm_x, &qtm_y);
            ret = qtmConvertCoordToScreen(&qtminfo.coords0[0], &range, &range, &qtm_x, &qtm_y);

            if (kDown & KEY_ZL)
			{
               ctr->qtm_state.rel_x = qtm_x;
			   ctr->qtm_state.rel_y = qtm_y;
			}
            if (kDown & KEY_ZR)
			{
               ctr->qtm_state.multiplier += 1;
			   if (ctr->qtm_state.multiplier > 50)
                  ctr->qtm_state.multiplier = 1;
			}
/*
            if (kDown & KEY_ZR)
			{
               ctr->qtm_state.multiplier -= 1;
			   if (ctr->qtm_state.multiplier < 1)
                  ctr->qtm_state.multiplier = 1;
			}
*/
            ctr->qtm_state.x = (qtm_x - ctr->qtm_state.rel_x)*ctr->qtm_state.multiplier;
            if (ctr->qtm_state.x > 32767)
               ctr->qtm_state.x = 32767;
            if (ctr->qtm_state.x < -32767)
               ctr->qtm_state.x = -32767;



            ctr->qtm_state.y = -((qtm_y - ctr->qtm_state.rel_y)*ctr->qtm_state.multiplier);
            if (ctr->qtm_state.y > 32767)
               ctr->qtm_state.y = 32767;
            if (ctr->qtm_state.y < -32767)
               ctr->qtm_state.y = -32767;



						//if(ret==0)memcpy(&fb[(x*240 + y) * 3], &colors[pos], 3);
//					}
         }
      }

   }
#endif

   ctr->lightgun_state.trigger    = (kDown & KEY_R);
   ctr->lightgun_state.reload     = (kDown & KEY_L);
   ctr->lightgun_state.aux_a      = (kDown & KEY_A);
   ctr->lightgun_state.aux_b      = (kDown & KEY_B);
   ctr->lightgun_state.aux_c      = (kDown & KEY_Y);
   ctr->lightgun_state.start      = (kDown & KEY_START);
   ctr->lightgun_state.select     = (kDown & KEY_SELECT);
   ctr->lightgun_state.dpad_up    = (kDown & KEY_DUP);
   ctr->lightgun_state.dpad_down  = (kDown & KEY_DDOWN);
   ctr->lightgun_state.dpad_left  = (kDown & KEY_DLEFT);
   ctr->lightgun_state.dpad_right = (kDown & KEY_DRIGHT);

}





static void ctr_input_poll(void *data)
{
	
//	return;
	
	
ctr_input_t *ctr     = (ctr_input_t*)data;

   settings_t *settings             = config_get_ptr();
   unsigned ctr_bottom_display_mode = settings->uints.ctr_bottom_display_mode;

   bool ctr_sensors_enabled         = settings->bools.input_ctr_sensors_enable;   
   unsigned ctr_mouse_mode          = settings->uints.input_ctr_mouse_mode;
   bool input_ctr_lightgun_abs      = settings->bools.input_ctr_lightgun_abs;


   int mouse_velocity_x = 0;
   int mouse_velocity_y = 0;

   uint16_t mod         = 0;
   unsigned code        = 0;

   if(ctr_bottom_display_mode == CTR_BOTTOM_MODE_DISABLED)
      return;

   if(ctr_sensors_enabled != ctr->sensors_enabled)
      ctr_input_set_sensor_state(ctr, 0, (ctr_sensors_enabled?
	        RETRO_SENSOR_GYROSCOPE_ENABLE:RETRO_SENSOR_GYROSCOPE_DISABLE), 0);

   if(ctr->sensors_enabled)
      ctr_input_poll_sensor(ctr); // ACCEL, GYRO, QTM

   if (!(ctr_bottom_display_mode == CTR_BOTTOM_MODE_CONTROL))
      {
      uint32_t state_tmp = hidKeysDown();
      BIT64_CLEAR(lifecycle_state, RARCH_MENU_TOGGLE);

      if (state_tmp & KEY_TOUCH)
         BIT64_SET(lifecycle_state, RARCH_MENU_TOGGLE);

      svcSleepThread(0);
      return;
   }

//   if ( !ctr_bottom_state_gfx.isInit )
//      return;


   touchPosition touch;
   hidTouchRead(&touch);

   KBState = ctr_bottom_frame(touch);



   if ( ctr_bottom_state.mode == MODE_KBD )
   {

      if (KBState > 0 && KBState < 79)
      {
         code = input_keymaps_translate_keysym_to_rk(KBState);

         if(ctr_bottom_state_kbd.isCaps)
		 {
            input_keyboard_event(true, RETROK_LSHIFT, RETROK_LSHIFT, mod,
            RETRO_DEVICE_KEYBOARD);
		 }
         if(ctr_bottom_state_kbd.isShift)
		 {
            input_keyboard_event(true, RETROK_LSHIFT, RETROK_LSHIFT, mod,
            RETRO_DEVICE_KEYBOARD);
		 }
         if(ctr_bottom_state_kbd.isAlt)
		 {
            input_keyboard_event(true, RETROK_LSHIFT, RETROK_LALT, mod,
            RETRO_DEVICE_KEYBOARD);
		 }
         if(ctr_bottom_state_kbd.isCtrl)
		 {
            input_keyboard_event(true, RETROK_LSHIFT, RETROK_LCTRL, mod,
            RETRO_DEVICE_KEYBOARD);
		 }
/*
		    mod |= RETROKMOD_CAPSLOCK;
            mod |= RETROKMOD_SHIFT;
            mod |= RETROKMOD_ALT;
            mod |= RETROKMOD_CTRL;
			mod |= RETROKMOD_META;
			mod |= RETROKMOD_NUMLOCK;
			mod |= RETROKMOD_CAPSLOCK;
			mod |= RETROKMOD_SCROLLOCK;
*/
         input_keyboard_event(true, code, code, mod,
            RETRO_DEVICE_KEYBOARD);
		 
         input_keyboard_event(false, code, code, mod,
            RETRO_DEVICE_KEYBOARD);
			

         if(ctr_bottom_state_kbd.isCaps)
		 {
            input_keyboard_event(false, RETROK_LSHIFT, RETROK_LSHIFT, mod,
            RETRO_DEVICE_KEYBOARD);
		 }
         if(ctr_bottom_state_kbd.isShift)
		 {
            input_keyboard_event(false, RETROK_LSHIFT, RETROK_LSHIFT, mod,
            RETRO_DEVICE_KEYBOARD);
		 }
         if(ctr_bottom_state_kbd.isAlt)
		 {
            input_keyboard_event(false, RETROK_LSHIFT, RETROK_LALT, mod,
            RETRO_DEVICE_KEYBOARD);
		 }
         if(ctr_bottom_state_kbd.isCtrl)
		 {
            input_keyboard_event(false, RETROK_LSHIFT, RETROK_LCTRL, mod,
            RETRO_DEVICE_KEYBOARD);
		 }

         if(ctr_bottom_state_kbd.isShift || ctr_bottom_state_kbd.isAlt || ctr_bottom_state_kbd.isCtrl)
		    ctr_bottom_kbd_rst_mod();

      }
   }
   else if ( ctr_bottom_state.mode == MODE_MOUSE )
   {

      mouse_velocity_x += ctr_bottom_state_mouse.mouse_x_rel; // get rid of this
      mouse_velocity_y += ctr_bottom_state_mouse.mouse_y_rel; // get rid of this
  
//      ctr_bottom_state_mouse.mouse_x_delta = mouse_velocity_x; // get rid of this
//      ctr_bottom_state_mouse.mouse_y_delta = mouse_velocity_y; // get rid of this


	  
//	  if((ctr->mouse_state.pos_x+mouse_velocity_x) > 0)
//	  {
//         if((ctr->mouse_state.pos_x+mouse_velocity_x) <= 320)
//         {
            ctr->mouse_state.delta_x = mouse_velocity_x;
//            ctr->mouse_state.pos_x += ctr->mouse_state.delta_x;
//         }
//	  }
//	  if ((ctr->mouse_state.pos_y+mouse_velocity_y) > 0)
//	  {
//         if((ctr->mouse_state.pos_y+mouse_velocity_y) <= 240)
//         {
            ctr->mouse_state.delta_y = mouse_velocity_y;
//            ctr->mouse_state.pos_y += ctr->mouse_state.delta_y;
//         }
//	  }


//   ctr->lightgun_state.trigger    = ctr_bottom_state_mouse.mouse_button_left;
//   ctr->lightgun_state.reload     = ctr_bottom_state_mouse.mouse_button_right;


   }

   if(ctr_sensors_enabled)
   {
      if(ctr_mouse_mode == CTR_INPUT_MOUSE_TOUCH)
      {
         if(input_ctr_lightgun_abs)
		 {
// touch coords --> mouse absolute coords in range -32767 > 32767
            if(touch.px != 0)
               ctr->mouse_state.abs_x = roundf((((float)touch.px / 320.0f) * 65534.0f) - 32767.0f);

            if(touch.py != 0)
               ctr->mouse_state.abs_y = roundf((((float)touch.py / 240.0f) * 65534.0f) - 32767.0f);
         }
		 else
		 {
            if(touch.px != 0)
               ctr->mouse_state.abs_x = touch.px;

            if(touch.py != 0)
               ctr->mouse_state.abs_y = touch.py;
         }
		 
      }
#ifdef HAVE_GYRO
      else if (ctr_mouse_mode == CTR_INPUT_MOUSE_GYRO)
      {
         // gyro simulate mouse ( are gyro and acceleromaeter giving correct values? libctru issue?)
         // use accelerometer values for now.
         if(input_ctr_lightgun_abs)
		 {
            ctr->mouse_state.abs_x = -(ctr->accelerometer_state.x*100);
            ctr->mouse_state.abs_y = -(ctr->accelerometer_state.z*100);
         }
         else
         {
			 // sane range -250 -> 250 ?? 
            ctr->mouse_state.abs_x = roundf((((float)ctr->accelerometer_state.x + 250.0f) / 500.0f) * 400.0f);
            ctr->mouse_state.abs_y = roundf((((float)ctr->accelerometer_state.y + 250.0f) / 500.0f) * 240.0f);
		 }
      }
#endif
#ifdef HAVE_QTM
      else if (ctr_mouse_mode == CTR_INPUT_MOUSE_QTM)
      {
// qtm simulate mouse
         ctr->mouse_state.abs_x = ctr->qtm_state.x;
         ctr->mouse_state.abs_y = ctr->qtm_state.y;
      }
#endif

   }
}


static int16_t ctr_input_state(
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
   ctr_input_t *ctr = (ctr_input_t*)data;

   if (!(port < DEFAULT_MAX_PADS) || !binds || !binds[port])
      return 0;
	  
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
      case RETRO_DEVICE_ANALOG:
         break;
      case RETRO_DEVICE_KEYBOARD:
         if (id < RETROK_LAST && keyboard_state[id])
            return 1;
         break;
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         {
            bool screen = device == RARCH_DEVICE_MOUSE_SCREEN;
            int val     = 0;
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_LEFT:
//			      if(ctr->lightgun_state.trigger)
                  return ctr->lightgun_state.trigger || ctr_bottom_state_mouse.mouse_button_left;                  // WORKING: pcsx-rearmed
//                  else
//                     val = ctr_bottom_state_mouse.mouse_button_left;
//                  return ctr_bottom_state_mouse.mouse_button_left;
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
			      return ctr->lightgun_state.aux_a || ctr_bottom_state_mouse.mouse_button_right;                    // WORKING: pcsx-rearmed
//                  return ctr_bottom_state_mouse.mouse_button_right;
//                  if(ctr->lightgun_state.aux_a)
 //                    val = ctr->lightgun_state.aux_a;
//                  else
//                     val = ctr_bottom_state_mouse.mouse_button_right; 
               case RETRO_DEVICE_ID_MOUSE_MIDDLE:
			      return ctr->lightgun_state.aux_b;                    // WORKING: pcsx-rearmed
               case RETRO_DEVICE_ID_MOUSE_X:
//			      return ctr->mouse_state.delta_x;                     //WORKING: fceumm (range 0->320/240) 
				  val = ctr->mouse_state.delta_x;
                  ctr->mouse_state.pos_x += ctr->mouse_state.delta_x;
				  ctr->mouse_state.delta_x = 0;
			      return val;
               case RETRO_DEVICE_ID_MOUSE_Y:
//			      return ctr->mouse_state.delta_y;                     //WORKING: fceumm
                  val = ctr->mouse_state.delta_y;
                  ctr->mouse_state.pos_y += ctr->mouse_state.delta_y;
                  ctr->mouse_state.delta_y = 0;
			      return val;
            }
            return val;
         }
         break;

      case RETRO_DEVICE_LIGHTGUN:
         {
            settings_t *settings             = config_get_ptr();
            bool input_ctr_lightgun_abs = settings->bools.input_ctr_lightgun_abs;
			 
            int val = 0;

            if(input_ctr_lightgun_abs)
            {
               switch (id)
               {
//#ifndef HAVE_QTM // en de rest..
                  case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
                     return ctr->mouse_state.abs_x;                     //WORKING: fceumm
//                  val = ctr->qtm_state.x;
//                  break;
                  case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
                     return ctr->mouse_state.abs_y;                     //WORKING: fceumm
//                  val = ctr->qtm_state.y;
//                  break;
               }
            }
            else
            {
//#else
/* deprecated */
            switch (id)
            {
               case RETRO_DEVICE_ID_LIGHTGUN_X:
				  val = ctr->mouse_state.delta_x;
                  ctr->mouse_state.pos_x += ctr->mouse_state.delta_x;
				  ctr->mouse_state.delta_x = 0;
			      return val;                  //NOT WORKING: SNES ??range?? 0->400 or 0->32700nogwat?
               case RETRO_DEVICE_ID_LIGHTGUN_Y:
                  val = ctr->mouse_state.delta_y;
                  ctr->mouse_state.pos_y += ctr->mouse_state.delta_y;
                  ctr->mouse_state.delta_y = 0;
			      return val;                  //NOT WORKING: SNES
            }
         }
//#endif

            switch (id)
            {
               case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:                //NOT USED BY: SNES
                  return ctr->lightgun_state.trigger;
               case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
                  return ctr->lightgun_state.reload;
               case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
                  return ctr->lightgun_state.aux_a;
               case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
                  return ctr->lightgun_state.aux_b;
               case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
                  return ctr->lightgun_state.aux_c;
               case RETRO_DEVICE_ID_LIGHTGUN_START:
                  return ctr->lightgun_state.start;
               case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
                  return ctr->lightgun_state.select;
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
                  return ctr->lightgun_state.dpad_up;
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
                  return ctr->lightgun_state.dpad_down;
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
                  return ctr->lightgun_state.dpad_left;
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
                  return ctr->lightgun_state.dpad_right;
               case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
                  return ctr->lightgun_state.pause;


               return val;
            }
         }
         break;

      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
	  {
// SHOULD BE ABS range X0 -> 400 / Y0 -> 240
//         if (idx == 0)
//         {
				   if(input_ctr_lightgun_abs)
			   {
            struct video_viewport vp;
            bool screen                 = device == 
               RARCH_DEVICE_POINTER_SCREEN;
            const int edge_detect       = 32700;
            bool inside                 = false;
            int16_t res_x               = 0;
            int16_t res_y               = 0;
            int16_t res_screen_x        = 0;
            int16_t res_screen_y        = 0;

            vp.x                        = 0;
            vp.y                        = 0;
            vp.width                    = 0;
            vp.height                   = 0;
            vp.full_width               = 0;
            vp.full_height              = 0;

            if (video_driver_translate_coord_viewport_wrap(
                        &vp, ctr->mouse_state.abs_x, ctr->mouse_state.abs_y,
                        &res_x, &res_y, &res_screen_x, &res_screen_y))
            {
               if (screen)
               {
                  res_x = res_screen_x;
                  res_y = res_screen_y;
               }

               inside =    (res_x >= -edge_detect) 
                  && (res_y >= -edge_detect)
                  && (res_x <= edge_detect)
                  && (res_y <= edge_detect);

// DEBUG


      ctr->debug_state.inside         = inside;
      ctr->debug_state.res_x          = res_x;
      ctr->debug_state.res_y          = res_y;
      ctr->debug_state.res_screen_x   = res_screen_x;
      ctr->debug_state.res_screen_y   = res_screen_y;

      ctr->debug_state.vp_x           = vp.x;
      ctr->debug_state.vp_y           = vp.y;
      ctr->debug_state.vp_width       = vp.width;
      ctr->debug_state.vp_height      = vp.height;
      ctr->debug_state.vp_full_width  = vp.full_width;
      ctr->debug_state.vp_full_height = vp.full_height;

			}
// DEBUG
               switch (id)
               {
                  case RETRO_DEVICE_ID_POINTER_X:
                     return input_ctr_lightgun_abs? res_x:ctr->mouse_state.abs_x;       // WORKING: fceumm  USED BY: PCSX_REARMED
                  case RETRO_DEVICE_ID_POINTER_Y:
                     return input_ctr_lightgun_abs? res_y:ctr->mouse_state.abs_y;       // WORKING: fceumm  USED BY: PCSX_REARMED
                  case RETRO_DEVICE_ID_POINTER_PRESSED:
                     return ctr->lightgun_state.trigger;                                // USED BY: fceumm
                  case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
                     return !inside;
               }
            }
         }
         break;

	}
    return 0;
}

#ifdef HAVE_GYRO
static bool ctr_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate)
{
   ctr_input_t *ctr = (ctr_input_t*)data;

   if (port > 0)
      return false;

   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
	  case RETRO_SENSOR_GYROSCOPE_ENABLE:
	     if(!ctr->sensors_enabled)
		 {
            ctr->sensors_enabled = true;
            HIDUSER_EnableAccelerometer();
            HIDUSER_EnableGyroscope();
            qtmInit();
            qtm_usable = qtmCheckInitialized();
         }
         return true;

      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
	  case RETRO_SENSOR_GYROSCOPE_DISABLE:
	     if(ctr->sensors_enabled)
		 {
            ctr->sensors_enabled = false;
            HIDUSER_DisableAccelerometer();
            HIDUSER_DisableGyroscope();
			qtm_usable = false;
			qtmExit();
		 }
         return true;

      case RETRO_SENSOR_DUMMY:
      case RETRO_SENSOR_ILLUMINANCE_ENABLE:
         break;
   }
   return false;
}


// is there even a sensor enabled core available for 3ds?
static float ctr_input_get_sensor_input(void *data,
      unsigned port, unsigned id)
{
   ctr_input_t *ctr = (ctr_input_t*)data;

   if(!ctr || !ctr->sensors_enabled)
      return false;

   if(id >= RETRO_SENSOR_ACCELEROMETER_X && id <= RETRO_SENSOR_GYROSCOPE_Z)
   {
      switch (id)
      {
         case RETRO_SENSOR_ACCELEROMETER_X:
            return ctr->accelerometer_state.x;
         case RETRO_SENSOR_ACCELEROMETER_Y:
            return ctr->accelerometer_state.y;
         case RETRO_SENSOR_ACCELEROMETER_Z:
            return ctr->accelerometer_state.z;
         case RETRO_SENSOR_GYROSCOPE_X:
            return ctr->gyroscope_state.x;
         case RETRO_SENSOR_GYROSCOPE_Y:
            return ctr->gyroscope_state.y;
         case RETRO_SENSOR_GYROSCOPE_Z:
            return ctr->gyroscope_state.z;
      }
   }
   return 0.0f;
}
#endif

static void ctr_input_free_input(void *data)
{
   ctr_input_t *ctr = (ctr_input_t*)data;
   
   if (!data)
      return;

   ctr_bottom_deinit();
#ifdef HAVE_QTM
   qtmExit();
#endif
#ifdef HAVE_GYRO
   if(ctr->sensors_enabled)
   {
      ctr->sensors_enabled = false;
      HIDUSER_DisableAccelerometer();
      HIDUSER_DisableGyroscope();
   }
#endif
   free(data);

   return;
}

static void* ctr_input_init(const char *joypad_driver)
{
   ctr_input_t     *ctr = (ctr_input_t*)calloc(1, sizeof(*ctr));
   if (!ctr)
      return NULL;

  
   ctr_bottom_init();
   input_keymaps_init_keyboard_lut(rarch_key_map_ctr);

#ifdef HAVE_QTM
   qtmInit();
   qtm_usable = qtmCheckInitialized();
   if(!qtm_usable)printf("QTM is not usable, only new3ds and cia? builds are supported.\n");
#endif

#ifdef HAVE_GYRO // start disabled? doesn't seem to be working when not enabing on startup
   ctr->sensors_enabled = true;
   HIDUSER_EnableAccelerometer();
   HIDUSER_EnableGyroscope();
#endif

//   return (void*)-1;
return ctr;
}

static uint64_t ctr_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_ANALOG);
   caps |= (1 << RETRO_DEVICE_KEYBOARD);
   caps |= (1 << RETRO_DEVICE_MOUSE);
   caps |= (1 << RETRO_DEVICE_LIGHTGUN);
   caps |= (1 << RETRO_DEVICE_POINTER);

   return caps;
}

input_driver_t input_ctr = {
   ctr_input_init,
   ctr_input_poll,
   ctr_input_state,
   ctr_input_free_input,
   #ifdef HAVE_GYRO
   ctr_input_set_sensor_state,
   ctr_input_get_sensor_input,
   #else
   NULL,                        /* set_sensor_state */
   NULL,                        /* get_sensor_input */
   #endif
   ctr_input_get_capabilities,
   "ctr",
   NULL,                        /* grab_mouse */
   NULL,
   NULL
};
