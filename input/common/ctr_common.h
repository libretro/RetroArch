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

#ifndef _CTR_INPUT_COMMON_H
#define _CTR_INPUT_COMMON_H

static const bool input_ctr_lightgun_abs;

typedef enum
{
   CTR_INPUT_MOUSE_TOUCH = 0,
   CTR_INPUT_MOUSE_GYRO,
   CTR_INPUT_MOUSE_QTM,
   CTR_INPUT_MOUSE_LAST,
} ctr_mouse_mode_enum;


typedef struct ctr_input
{
#ifdef HAVE_BOTTOM_SCREEN
   struct
   {
      int abs_x;
      int abs_y;
      int delta_x;
      int delta_y;
      int pos_x;
      int pos_y;
   }mouse_state;
#endif
#ifdef HAVE_GYRO
   struct
   {
      int x;
      int y;
      int z;
   }accelerometer_state;

   struct
   {
      int x;
      int y;
      int z;
   }gyroscope_state;
#endif
#ifdef HAVE_QTM
   struct
   {
      int x;
      int y;
      unsigned rel_x;
      unsigned rel_y;
      int multiplier;
   }qtm_state;
#endif
   struct
   {
      unsigned trigger;
      unsigned reload;
      unsigned aux_a;
      unsigned aux_b;
      unsigned aux_c;
      unsigned start;
      unsigned select;
      unsigned dpad_up;
      unsigned dpad_down;
      unsigned dpad_left;
      unsigned dpad_right;
      unsigned pause;
   }lightgun_state;

   bool sensors_enabled;

   struct
   {
      bool inside;
      int res_x;
      int res_y;
      int res_screen_x;
      int res_screen_y;

      int vp_x;
      int vp_y;
      int vp_width;
      int vp_height;
      int vp_full_width;
      int vp_full_height;

   }debug_state;
   
} ctr_input_t;

#endif
