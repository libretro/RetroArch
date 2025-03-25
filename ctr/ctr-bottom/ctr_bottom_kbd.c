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

#include "ctr_bottom.h"
#include "ctr_bottom_kbd.h"
#include "ctr_bottom_kbd_keydef.h"

ctr_bottom_kbd_lut_t ctr_bottom_kbd_lut[] = {
   {CTR_KEY_NONE     , 0,  0,  0,  0,  0},

   {CTR_KEY_ESCAPE   , 0,  2,  2, 30, 33},
   {CTR_KEY_F1       , 1, 32,  2, 55, 33},
   {CTR_KEY_F2       , 1, 56,  2, 79, 33},
   {CTR_KEY_F3       , 1, 80,  2,103, 33},
   {CTR_KEY_F4       , 1,104,  2,127, 33},
   {CTR_KEY_F5       , 1,128,  2,151, 33},
   {CTR_KEY_F6       , 1,152,  2,175, 33},
   {CTR_KEY_F7       , 1,176,  2,199, 33},
   {CTR_KEY_F8       , 1,200,  2,223, 33},
   {CTR_KEY_F9       , 1,224,  2,247, 33},
   {CTR_KEY_F10      , 1,248,  2,271, 33},
   {CTR_KEY_F11      , 1,272,  2,295, 33},
   {CTR_KEY_F12      , 1,296,  2,319, 33},

   {CTR_KEY_1        , 0,  2, 35, 30, 66},
   {CTR_KEY_2        , 0, 31, 35, 59, 66},
   {CTR_KEY_3        , 0, 60, 35, 88, 66},
   {CTR_KEY_4        , 0, 89, 35,117, 66},
   {CTR_KEY_5        , 0,118, 35,146, 66},
   {CTR_KEY_6        , 0,147, 35,175, 66},
   {CTR_KEY_7        , 0,176, 35,204, 66},
   {CTR_KEY_8        , 0,205, 35,233, 66},
   {CTR_KEY_9        , 0,234, 35,262, 66},
   {CTR_KEY_0        , 0,263, 35,291, 66},
   {CTR_KEY_BACKSPACE, 0,292, 35,319, 66},

   {CTR_KEY_Q        , 0,  2, 68, 30, 99},
   {CTR_KEY_W        , 0, 31, 68, 59, 99},
   {CTR_KEY_E        , 0, 60, 68, 88, 99},
   {CTR_KEY_R        , 0, 89, 68,117, 99},
   {CTR_KEY_T        , 0,118, 68,146, 99},
   {CTR_KEY_Y        , 0,147, 68,175, 99},
   {CTR_KEY_U        , 0,176, 68,204, 99},
   {CTR_KEY_I        , 0,205, 68,233, 99},
   {CTR_KEY_O        , 0,234, 68,262, 99},
   {CTR_KEY_P        , 0,263, 68,291, 99},
   {CTR_KEY_RETURN   , 4,292, 68,319, 99},

   {CTR_KEY_TAB      , 8,  2,101, 16,132},
   {CTR_KEY_A        , 0, 17,101, 45,132},
   {CTR_KEY_S        , 0, 46,101, 74,132},
   {CTR_KEY_D        , 0, 75,101,103,132},
   {CTR_KEY_F        , 0,104,101,132,132},
   {CTR_KEY_G        , 0,133,101,161,132},
   {CTR_KEY_H        , 0,162,101,190,132},
   {CTR_KEY_J        , 0,191,101,219,132},
   {CTR_KEY_K        , 0,220,101,248,132},
   {CTR_KEY_L        , 0,249,101,277,132},
   {CTR_KEY_RETURN   , 4,278,101,319,132},

   {CTR_KEY_CAPS     , 6,  2,134, 37,165},
   {CTR_KEY_Z        , 0, 38,134, 66,165},
   {CTR_KEY_X        , 0, 67,134, 95,165},
   {CTR_KEY_C        , 0, 96,134,124,165},
   {CTR_KEY_V        , 0,125,134,153,165},
   {CTR_KEY_B        , 0,154,134,182,165},
   {CTR_KEY_N        , 0,183,134,211,165},
   {CTR_KEY_M        , 0,212,134,240,165},
   {CTR_KEY_SHIFT    , 5,241,134,319,165},

   {CTR_KEY_NUMBER   , 1,  2,166, 44,198},
   {CTR_KEY_SYMBOL   , 1, 44,166, 87,198},
   {CTR_KEY_SPACE    , 7, 88,166,232,198},
   {CTR_KEY_ALT      , 2,233,166,271,198},
   {CTR_KEY_CTRL     , 3,277,166,319,198}
};

unsigned ctr_bottom_kbd_get_key(s16 T_X, s16 T_Y)
{
   unsigned x0, y0, x1, y1;
   int i;

   for (i = 0; i < (61); i++)
   {
      if ((T_Y > ctr_bottom_kbd_lut[i].y0) && (T_Y < ctr_bottom_kbd_lut[i].y1))
      {
         if ((T_X > ctr_bottom_kbd_lut[i].x0) && (T_X < ctr_bottom_kbd_lut[i].x1))
         {
            return i;
         }
      }
   }
   return 0;
}

void ctr_bottom_kbd_set_mod(int PressedKey)
{
   if (PressedKey == CTR_KEY_SHIFT)
   {
      if (ctr_bottom_state_kbd.isShift)
         ctr_bottom_state_kbd.isShift = false;
      else
         ctr_bottom_state_kbd.isShift = true;
   }
   else if (PressedKey == CTR_KEY_CAPS)
   {
      if (ctr_bottom_state_kbd.isCaps)
         ctr_bottom_state_kbd.isCaps = false;
      else
         ctr_bottom_state_kbd.isCaps = true;
   }
   else if (PressedKey == CTR_KEY_ALT)
   {
      if (ctr_bottom_state_kbd.isAlt)
         ctr_bottom_state_kbd.isAlt = false;
      else
         ctr_bottom_state_kbd.isAlt = true;
   }
   else if (PressedKey == CTR_KEY_CTRL)
   {
      if (ctr_bottom_state_kbd.isCtrl)
         ctr_bottom_state_kbd.isCtrl = false;
      else
         ctr_bottom_state_kbd.isCtrl = true;
   }
   else if (PressedKey == CTR_KEY_SYMBOL)
   {
      if (ctr_bottom_state_kbd.kbd_mode == 2)
         ctr_bottom_state_kbd.kbd_mode = KBD_LOWER;
      else
         ctr_bottom_state_kbd.kbd_mode = KBD_SYMBOL;

      ctr_set_bottom_mode( MODE_KBD );
   }
   else if (PressedKey == CTR_KEY_NUMBER)
   {
      if (ctr_bottom_state_kbd.kbd_mode == KBD_NUMBER)
         ctr_bottom_state_kbd.kbd_mode = KBD_LOWER;
      else
         ctr_bottom_state_kbd.kbd_mode = KBD_NUMBER;

      ctr_set_bottom_mode( MODE_KBD );
   }

   if (ctr_bottom_state_kbd.kbd_mode == 0)
   {
      if (ctr_bottom_state_kbd.isShift || ctr_bottom_state_kbd.isCaps )
      {
         ctr_bottom_state_kbd.kbd_mode = 1;
      }
   }
   else if (ctr_bottom_state_kbd.kbd_mode == 1)
   {
      if (!ctr_bottom_state_kbd.isShift && !ctr_bottom_state_kbd.isCaps )
      {
         ctr_bottom_state_kbd.kbd_mode = 0;
      }
   }
   ctr_set_bottom_mode(MODE_KBD);
}

void ctr_bottom_kbd_rst_mod()
{
   ctr_bottom_state_kbd.isShift = false;
   ctr_bottom_state_kbd.isAlt   = false;
   ctr_bottom_state_kbd.isCtrl  = false;
   ctr_refresh_bottom(true);
}
