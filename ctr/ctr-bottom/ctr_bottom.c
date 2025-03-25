/*  RetroArch - A frontend for libretro.
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

#include <3ds.h>

#include "ctr_bottom.h"
#include "ctr_bottom_kbd.h"
#include "ctr_bottom_states.h"

#include "../../paths.h"
#include "../../command.h"
#include "../../gfx/common/ctr_defines.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

LightLock lock;
bool isInit = false;
unsigned CurrentKey;
int KBState;
u64 elapsed_tick;
u8 PreviousKey;
bool KeyHeld, HasDrawn = false;
int fadeCount = 256;

ctr_bottom_state_t            ctr_bottom_state            = { MODE_IDLE, MODE_TODO, 0, 0, 0, 0, false, false, false, false };
ctr_bottom_state_gfx_t        ctr_bottom_state_gfx        = { false, false, false, false, 0, false, false, NULL, NULL, true };
ctr_bottom_state_kbd_t        ctr_bottom_state_kbd        = { KBD_LOWER, 0, false, false, false, false };
ctr_bottom_state_mouse_t      ctr_bottom_state_mouse      = { 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, false, false, false };
ctr_bottom_state_savestates_t ctr_bottom_state_savestates = { false, false, "00/00/0000" , -2 };


void ctr_bottom_init()
{
   LightLock_Init(&lock);
   isInit = true;

   return;
}

bool ctr_refresh_bottom(bool refresh)
{
   bool oldval;

   while (LightLock_TryLock(&lock) != 0)
   {
      oldval                                   = ctr_bottom_state_gfx.refresh_bottom_menu;
      ctr_bottom_state_gfx.refresh_bottom_menu = refresh;

      LightLock_Unlock(&lock);
   }

   return oldval;
}

void ctr_set_bottom_mode(unsigned id)
{
   ctr_bottom_state.previous_mode = ctr_bottom_state.mode;

   switch (id)
   {
      case MODE_IDLE:
         ctr_bottom_state_gfx.gfx_id = 0;
         break;
      case MODE_MOUSE:
         ctr_bottom_state_gfx.gfx_id = 1;
         break;
      case MODE_SAVESTATE:
         ctr_bottom_state_gfx.gfx_id = 2;
         break;
      case MODE_TODO:
         ctr_bottom_state_gfx.gfx_id = 3;
         break;
      case MODE_KBD:
         switch (ctr_bottom_state_kbd.kbd_mode)
         {
            case KBD_LOWER:
               ctr_bottom_state_gfx.gfx_id = 4;
               break;
            case KBD_UPPER:
               ctr_bottom_state_gfx.gfx_id = 5;
               break;
            case KBD_SYMBOL:
               ctr_bottom_state_gfx.gfx_id = 6;
               break;
            case KBD_NUMBER:
               ctr_bottom_state_gfx.gfx_id = 7;
               break;
         }
         break;
   }
   ctr_bottom_state.mode = id;
   ctr_refresh_bottom(true);
}

u8 ctr_bottom_frame(touchPosition TouchScreenPos)
{
   BIT64_CLEAR(lifecycle_state, RARCH_MENU_TOGGLE);
   BIT64_CLEAR(lifecycle_state, RARCH_RESET);
   BIT64_CLEAR(lifecycle_state, RARCH_STATE_SLOT_PLUS);
   BIT64_CLEAR(lifecycle_state, RARCH_STATE_SLOT_MINUS);

   s16 T_X = TouchScreenPos.px;
   s16 T_Y = TouchScreenPos.py;

   ctr_bottom_state.touch_x = TouchScreenPos.px;
   ctr_bottom_state.touch_y = TouchScreenPos.py;
uint32_t flags                 = runloop_get_flags();
/*
 *  Bottom Disabled / idle
 */
   if (!ctr_bottom_state_gfx.bottom_enabled)
   {
      if ( ctr_bottom_state.idle_timestamp == 0 )
      {
         ctr_bottom_state.idle_timestamp  = svcGetSystemTick();
         ctr_bottom_state.shouldCheckIdle = true;
         return 0;
      }

      if ( ctr_bottom_state.shouldCheckIdle && !KeyHeld && !ctr_bottom_state_gfx.bottom_idle )
      {
         elapsed_tick = ( svcGetSystemTick() - ctr_bottom_state.idle_timestamp );
         if ( elapsed_tick > 2000000000 )
         {
            ctr_bottom_state_gfx.bottom_idle = true;
            ctr_bottom_state.shouldCheckIdle = false;

            gspLcdInit();
            GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTTOM);
            gspLcdExit();

            return 0;
         }
      }

      if ( ctr_bottom_state.shouldCheckMenu && KeyHeld )
      {
         elapsed_tick = ( svcGetSystemTick() - ctr_bottom_state.touch_timestamp );
         if ( elapsed_tick > 200000000)
         {
            ctr_bottom_state.shouldCheckMenu   = false;
            ctr_bottom_state_gfx.bottom_enabled = true;
            ctr_set_bottom_mode(ctr_bottom_state.previous_mode);
            ctr_refresh_bottom(true);
         }
      }
      else if ( ctr_bottom_state.shouldCheckMenu && !KeyHeld )
      {
         if ( elapsed_tick < 200000000)
         {
            BIT64_SET(lifecycle_state, RARCH_MENU_TOGGLE);
         }
         ctr_bottom_state.shouldCheckMenu   = false;
      }

      if (T_X == 0 && T_Y == 0)
      {
         if (KeyHeld)
         {
            KeyHeld = false;
            ctr_bottom_state.idle_timestamp  = svcGetSystemTick();
			   
            if ( !ctr_bottom_state_gfx.bottom_idle )
            {
               ctr_bottom_state.shouldCheckIdle = true;
            }
         }
		 return 0;
      }
      else if (T_Y > 200 && T_X > 128 && T_X < 191)
      {
         if (!KeyHeld)
         {
            KeyHeld = true;
            ctr_bottom_state.touch_timestamp = svcGetSystemTick();

            ctr_bottom_state.shouldCheckMenu = true;
            ctr_bottom_state.shouldCheckIdle = false;
            fadeCount = 256;
         }
      }

      else
      {
         ctr_bottom_state.shouldCheckMenu = false;
         ctr_bottom_state.shouldCheckIdle = false;
         fadeCount = 256;
         KeyHeld = true;
      }

	  if ( ctr_bottom_state_gfx.bottom_idle )
	  {
         gspLcdInit();
         GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTTOM);
         gspLcdExit();
		 
		 ctr_bottom_state_gfx.bottom_idle = false;
	  }
   }
   else
   {

/*
 *  Bottom Enabled
 */
      if (T_X == 0 && T_Y == 0)
      {
         PreviousKey = 0;
         CurrentKey  = 0;
		 ctr_bottom_state_kbd.isPressed = 0;
         ctr_bottom_state_mouse.mouse_x_rel = 0;
         ctr_bottom_state_mouse.mouse_y_rel = 0;

         if(KeyHeld)
         {
            ctr_bottom_state_mouse.mouse_button_left = false;
            ctr_bottom_state_mouse.mouse_button_right = false;
         }

         if ( ctr_bottom_state.shouldCheckMenu && KeyHeld)
         {
            elapsed_tick = ( svcGetSystemTick() - ctr_bottom_state.touch_timestamp );
            if ( elapsed_tick < 200000000)
            {
               BIT64_SET(lifecycle_state, RARCH_MENU_TOGGLE);
            }
            ctr_bottom_state.shouldCheckMenu   = false;
         }

         if (KeyHeld)
            ctr_refresh_bottom(true);

         KeyHeld = false;
      }

      if (T_Y > 200 && !KeyHeld)
      {
         KeyHeld = true;

         if (ctr_bottom_state.touch_y > 200)
         {
            if (ctr_bottom_state.touch_x < 63)
            {
               ctr_set_bottom_mode( MODE_KBD );
            }
            else if (ctr_bottom_state.touch_x > 64 && ctr_bottom_state.touch_x < 127)
            {
               ctr_set_bottom_mode( MODE_MOUSE );
            }
            else if (ctr_bottom_state.touch_x > 128 && ctr_bottom_state.touch_x < 191)
            {
               ctr_bottom_state.touch_timestamp = svcGetSystemTick();
               ctr_bottom_state.shouldCheckMenu = true;
               return 0;
            }
            else if (ctr_bottom_state.touch_x > 192 && ctr_bottom_state.touch_x < 255)
            {
               ctr_set_bottom_mode( MODE_SAVESTATE );
            }
            else if (ctr_bottom_state.touch_x > 256 && ctr_bottom_state.touch_x < 320)
            {
               ctr_set_bottom_mode( MODE_TODO );
            }
         }
         return 0;
      }

      else if (T_Y > 200 && KeyHeld)
      {
         if ( ctr_bottom_state.shouldCheckMenu )
         {
            elapsed_tick = ( svcGetSystemTick() - ctr_bottom_state.touch_timestamp );
            if ( elapsed_tick > 200000000)
            {
               ctr_bottom_state_gfx.bottom_enabled = false;
               ctr_bottom_state.shouldCheckMenu   = false;
               ctr_set_bottom_mode( MODE_IDLE );
               return 0;
            }
         }
         else if ( ctr_bottom_state.mode == MODE_MOUSE )
         {
            ctr_bottom_state_mouse.mouse_x_rel = 0;
            ctr_bottom_state_mouse.mouse_y_rel = 0;
         }
         return 0;
      }

/*
 *  MOUSE INPUT
 */
      else if ( ctr_bottom_state.mode == MODE_MOUSE )
      {

         if (T_X == 0 && T_Y == 0)
         {
            if ( ctr_bottom_state_mouse.ShouldCheck && elapsed_tick < 300000000 )
            {
               ctr_bottom_state_mouse.mouse_button_left = true;
               ctr_bottom_state_mouse.ShouldCheck = false;
			   KeyHeld = true;
            }
         }

         if (T_X > 0 || T_Y > 0)
         {
            elapsed_tick = ( svcGetSystemTick() - ctr_bottom_state_mouse.touch_timestamp );

            if (!KeyHeld)
            {
               if (T_X > 294 && T_X < 320)
               {
                  if (T_Y > 158 && T_Y < 176)
                  {
                     if (ctr_bottom_state_mouse.mouse_multiplier != 9)
                     {
                        ctr_bottom_state_mouse.mouse_multiplier++;
                        KeyHeld = true;
                        return 0;
                     }
                  }
                  else if (T_Y > 175 && T_Y < 193)
                  {
                     if (ctr_bottom_state_mouse.mouse_multiplier != 1)
                     {
                        ctr_bottom_state_mouse.mouse_multiplier--;
                        KeyHeld = true;
                        return 0;
                     }
                  }
               }

               ctr_bottom_state_mouse.mouse_x_origin = T_X;
               ctr_bottom_state_mouse.mouse_y_origin = T_Y;

               KeyHeld = true;

               if ( elapsed_tick < 100000000)
               {
                  if(!ctr_bottom_state_mouse.ShouldCheck)
                  {
                     ctr_bottom_state_mouse.mouse_button_x_origin = ctr_bottom_state_mouse.mouse_x_origin;
                     ctr_bottom_state_mouse.mouse_button_y_origin = ctr_bottom_state_mouse.mouse_y_origin;
                  }
                  ctr_bottom_state_mouse.ShouldCheck = true;
               }
               ctr_bottom_state_mouse.touch_timestamp = svcGetSystemTick();
            }
            else if( ctr_bottom_state_mouse.ShouldCheck && elapsed_tick > 300000000 )
            {
               ctr_bottom_state_mouse.mouse_button_right = true;
               ctr_bottom_state_mouse.ShouldCheck = false;
            }
            else
            {
               if ( ctr_bottom_state_mouse.ShouldCheck && elapsed_tick < 300000000 )
               {
				  int pos_x;
				  int pos_y;
				  if (ctr_bottom_state_mouse.mouse_button_x_origin < T_X)
				  {
                     pos_x = T_X - ctr_bottom_state_mouse.mouse_button_x_origin;
				  }
				  else
				  {
					  pos_x = ctr_bottom_state_mouse.mouse_button_x_origin - T_X;
				  }
                  if (ctr_bottom_state_mouse.mouse_button_y_origin < T_Y)
				  {
                     pos_y = T_Y - ctr_bottom_state_mouse.mouse_button_y_origin;
				  }
				  else
				  {
					  pos_y = ctr_bottom_state_mouse.mouse_button_y_origin - T_Y;
				  }

                  if (pos_x > 1 || pos_y > 1)
                  {
			         ctr_bottom_state_mouse.mouse_button_left = true;
			         ctr_bottom_state_mouse.ShouldCheck = false;
                  }
               }
               if ( T_Y < ctr_bottom_state_mouse.mouse_y_origin )
               {
                  ctr_bottom_state_mouse.mouse_y_rel = -(( ctr_bottom_state_mouse.mouse_y_origin -  T_Y ) * ctr_bottom_state_mouse.mouse_multiplier );
               }
               else
               {
                  ctr_bottom_state_mouse.mouse_y_rel = (( T_Y - ctr_bottom_state_mouse.mouse_y_origin ) * ctr_bottom_state_mouse.mouse_multiplier );
               }
               if ( T_X < ctr_bottom_state_mouse.mouse_x_origin )
               {
                  ctr_bottom_state_mouse.mouse_x_rel = -(( ctr_bottom_state_mouse.mouse_x_origin -  T_X ) * ctr_bottom_state_mouse.mouse_multiplier );
               }
               else
               {
                  ctr_bottom_state_mouse.mouse_x_rel = (( T_X - ctr_bottom_state_mouse.mouse_x_origin ) * ctr_bottom_state_mouse.mouse_multiplier );
               }
               ctr_bottom_state_mouse.mouse_x_origin = T_X;
               ctr_bottom_state_mouse.mouse_y_origin = T_Y;
            }
            return 0;
         }
      }

/*
 *  KBD INPUT
 */
      else if ( ctr_bottom_state.mode == MODE_KBD )
      {
         CurrentKey = ctr_bottom_kbd_get_key(T_X, T_Y);

         if ((CurrentKey != PreviousKey) && !KeyHeld)
         {
            unsigned ret;

            ret = ctr_bottom_kbd_lut[CurrentKey].key;

            ctr_bottom_kbd_set_mod(ret);

            ctr_bottom_state_kbd.isPressed = CurrentKey;

            if(ctr_bottom_state_gfx.gfx_id == 6 && ctr_bottom_kbd_lut[CurrentKey].gfx == 0) /* KBD_SYMBOL */
               if(ret <= 26)
                  ret += 48;

            PreviousKey = CurrentKey;
            KeyHeld = true;
            ctr_refresh_bottom(true);

            return ret;
         }
      }

/*
 *  SAVE STATES
 */
      else if ( ctr_bottom_state.mode == MODE_SAVESTATE )
      {
         if (!(flags & RUNLOOP_FLAG_CORE_RUNNING))
            return 0;

         settings_t *settings = config_get_ptr();
         bool save_to_ram     = settings->bools.ctr_save_state_to_ram;
         int config_slot      = settings->ints.state_slot;

         if (ctr_bottom_state_savestates.state_slot != config_slot)
         {
            if ( ctr_bottom_state_savestates.state_data_on_ram = true )
            {
               save_state_to_file();
               ctr_bottom_state_savestates.state_data_on_ram = false;
            }
            ctr_bottom_state_gfx.reload_texture = true;
            ctr_bottom_state_savestates.state_slot = config_slot;
         }

         if (T_X == 0 && T_Y == 0)
         {
            KeyHeld = false;
         }

         if (T_X > 8 && T_X < 107 && T_Y > 8 && T_Y < 86 && !KeyHeld )
         {
            BIT64_SET(lifecycle_state, RARCH_RESET);
            KeyHeld = true;
         }

         if (T_X > 117 && T_X < 165 && T_Y > 8 && T_Y < 41 && !KeyHeld )
         {
            BIT64_SET(lifecycle_state, RARCH_STATE_SLOT_PLUS); /* not working while in menu */
            KeyHeld = true;
         }

         if (T_X > 117 && T_X < 165 && T_Y > 55 && T_Y < 88 && !KeyHeld )
         {
            BIT64_SET(lifecycle_state, RARCH_STATE_SLOT_MINUS); /* not working while in menu */
            KeyHeld = true;
         }

         else if (T_X > 8 &&
                  T_X < 164 &&
                  T_Y > 99 &&
                  T_Y < 200 && !KeyHeld )
         {
            KeyHeld = true;
//command_event(CMD_EVENT_PAUSE, NULL);
            if (save_to_ram)
            {
               ctr_bottom_state_savestates.state_data_on_ram = true;

               char _msg[20];
               sprintf(_msg, "Saving state to RAM");

               runloop_msg_queue_push(_msg, strlen(_msg), 1, 256, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO); /* todo: add MSG_HASH */

               command_event(CMD_EVENT_SAVE_STATE_TO_RAM, NULL);
            }
            else
            {
               command_event(CMD_EVENT_SAVE_STATE, NULL);
            }
            ctr_bottom_state.task_save = true;


         }

         else if (T_X > 176 &&
                  T_X < 311 &&
                  T_Y > 9   &&
                  T_Y < 200 && !KeyHeld )
         {
            KeyHeld = true;

            if (ctr_bottom_state_savestates.state_data_on_ram)
			{
               char _msg[20];
               sprintf(_msg, "Loading state from RAM");

               runloop_msg_queue_push(_msg, strlen(_msg), 1, 256, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO); /* todo: add MSG_HASH */
               command_event(CMD_EVENT_LOAD_STATE_FROM_RAM, NULL);
			}
            else
            {
               command_event(CMD_EVENT_LOAD_STATE, NULL);
			}
         }
         return 0;
      }
   }
   return 0;
}

void ctr_bottom_deinit()
{
   return;
}
