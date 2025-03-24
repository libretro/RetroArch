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
 
#include "ctr_bottom_debug.h"

#include "../../retroarch.h"
#include "../../gfx/font_driver.h"

#include "../../gfx/common/ctr_defines.h"
#include "../../input/common/ctr_common.h"

#include "../../ctr/ctr-bottom/ctr_bottom.h"
#include "../../ctr/ctr-bottom/ctr_bottom_kbd.h"

unsigned debug_id     = 0;
unsigned debug_id_max = 3;

static void font_driver_render_msg_bottom(ctr_video_t *ctr,
      const char *msg, const void *_params)
{
   ctr->render_font_bottom = true;
   font_driver_render_msg(ctr, msg, _params, NULL);
   ctr->render_font_bottom = false;
}

void ctr_bottom_render_screen_debug(void *data)
{
   struct font_params params         = { 0, };
   ctr_video_t *ctr                  = (ctr_video_t*)data;
   input_driver_state_t *ctr_input   = input_state_get_ptr();
   const ctr_input_t *ctr_input_data = ctr_input->current_data;

   if (!ctr)
      return;

   u32 kDown = hidKeysDown();
   if (kDown & KEY_TOUCH)
   {
      touchPosition touch;
      hidTouchRead(&touch);

      if (touch.px > 300 &&
          touch.px < 320 &&
          touch.py > 0   &&
          touch.py < 20)
      {
         debug_id++;
         if (debug_id >= debug_id_max)
            debug_id = 0;
      }
   }

   char str_tex_width[20];
   char str_tex_height[20];
	  
   char str_full_width[20];
   char str_full_height[20];
   char str_vp_x[20];
   char str_vp_y[20];
	  
   char str_width[20];
   char str_height[20];
   char str_x0[20];
   char str_y0[20];
   char str_x1[20];
   char str_y1[20];

   char str_x0_bottom[20];
   char str_y0_bottom[20];
   char str_x1_bottom[20];
   char str_y1_bottom[20];

   char str_u0[20];
   char str_v0[20];
   char str_u1[20];
   char str_v1[20];

   char str_u0_bottom[20];
   char str_v0_bottom[20];
   char str_u1_bottom[20];
   char str_v1_bottom[20];

   sprintf(str_full_width,  "full_width  : %d", ctr->vp.full_width);
   sprintf(str_full_height, "full_height : %d", ctr->vp.full_height);
   sprintf(str_vp_x,        "vp.x        : %d", ctr->vp.x);
   sprintf(str_vp_y,        "vp.y        : %d", ctr->vp.y);
   sprintf(str_width,       "vp.width    : %d", ctr->vp.width);
   sprintf(str_height,      "vp.height   : %d", ctr->vp.height);
   sprintf(str_tex_width,   "tex_width   : %d", ctr->texture_width);
   sprintf(str_tex_height,  "tex_height  : %d", ctr->texture_height);

   sprintf(str_x0,        "x0  : %d", ctr->frame_coords->x0);
   sprintf(str_y0,        "y0  : %d", ctr->frame_coords->y0);
   sprintf(str_x1,        "x1  : %d", ctr->frame_coords->x1);
   sprintf(str_y1,        "y1  : %d", ctr->frame_coords->y1);

   sprintf(str_x0_bottom, "x0  : %d", ctr->frame_coords_bottom->x0);
   sprintf(str_y0_bottom, "y0  : %d", ctr->frame_coords_bottom->y0);
   sprintf(str_x1_bottom, "x1  : %d", ctr->frame_coords_bottom->x1);
   sprintf(str_y1_bottom, "y1  : %d", ctr->frame_coords_bottom->y1);

//      sprintf(str_scale,     "scale: %d", (video_ctr_dual_offset_y * ((height / 2) / ctr->vp.y))));


   sprintf(str_u0,         "u0  : %d", ctr->frame_coords->u0);
   sprintf(str_v0,         "v0  : %d", ctr->frame_coords->v0);
   sprintf(str_u1,         "u1  : %d", ctr->frame_coords->u1);
   sprintf(str_v1,         "v1  : %d", ctr->frame_coords->v1);

   sprintf(str_u0_bottom,  "u0  : %d", ctr->frame_coords_bottom->u0);
   sprintf(str_v0_bottom,  "v0  : %d", ctr->frame_coords_bottom->v0);
   sprintf(str_u1_bottom,  "u1  : %d", ctr->frame_coords_bottom->u1);
   sprintf(str_v1_bottom,  "v1  : %d", ctr->frame_coords_bottom->v1);







   params.text_align = TEXT_ALIGN_LEFT;
   params.color = COLOR_ABGR(255, 255, 255, 255);
   params.scale = 1.0f;
//      params.x = 0.70f; // right side
   params.x = 0.07f; // left side
//      params.x = 0.40f; // middle
   params.y = 0.92f; // top side

   if (debug_id == 0)
   {
      font_driver_render_msg_bottom(ctr, str_full_width,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_full_height,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_width,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_height,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_vp_x,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_vp_y,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_tex_width,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_tex_height,
         &params);		 

// top screen coord
      params.y -= 0.06f;
      font_driver_render_msg_bottom(ctr, "Top coords:",
         &params);
      params.y -= 0.06f;
      font_driver_render_msg_bottom(ctr, str_x0,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_y0,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_x1,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_y1,
         &params);

// bottom screen coord
      params.y -= 0.06f;
      font_driver_render_msg_bottom(ctr, str_u0,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_v0,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_u1,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_v1,
         &params);

      params.x = 0.60f; // right side
	  params.y = 0.90f; // top side

//      params.y -= 0.20f;
//      font_driver_render_msg_bottom(ctr, str_scale,
//         &params);

      params.y -= 0.34f;
      font_driver_render_msg_bottom(ctr, "Bottom coords:",
         &params);

      params.y -= 0.06f;
      font_driver_render_msg_bottom(ctr, str_x0_bottom,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_y0_bottom,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_x1_bottom,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_y1_bottom,
         &params);

// bottom screen coord
      params.y -= 0.06f;
      font_driver_render_msg_bottom(ctr, str_u0_bottom,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_v0_bottom,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_u1_bottom,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_v1_bottom,
         &params);
   }
   else if (debug_id == 1)
   {

// QTM --------------------- and sensors

   char str_sensors[20];
   sprintf(str_sensors,   "Sensors : %s", ctr_input_data->sensors_enabled? "enabled":"disabled");

   font_driver_render_msg_bottom(ctr, str_sensors,
         &params);

#ifdef HAVE_QTM
// qtm screen coords
      char str_qtm_x[20];
      char str_qtm_y[20];
      char str_qtm_rel_x[20];
      char str_qtm_rel_y[20];
      char str_qtm_multiplier[20];

      sprintf(str_qtm_x,          "qtm_x     : %d", ctr_input_data->qtm_state.x);
      sprintf(str_qtm_y,          "qtm_y     : %d", ctr_input_data->qtm_state.y);
      sprintf(str_qtm_rel_x,      "qtm_rel_x : %d", ctr_input_data->qtm_state.rel_x);
      sprintf(str_qtm_rel_y,      "qtm_rel_y : %d", ctr_input_data->qtm_state.rel_y);
      sprintf(str_qtm_multiplier, "qtm_multi : %d", ctr_input_data->qtm_state.multiplier);

      params.y -= 0.06f;
      font_driver_render_msg_bottom(ctr, str_qtm_x,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_qtm_y,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_qtm_rel_x,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_qtm_rel_y,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_qtm_multiplier,
         &params);
#endif
#ifdef HAVE_GYRO
// gyro coords
      char str_accel_x[20];
      char str_accel_y[20];
      char str_accel_z[20];

      sprintf(str_accel_x, "accel_x : %d", ctr_input_data->accelerometer_state.x);
      sprintf(str_accel_y, "accel_y : %d", ctr_input_data->accelerometer_state.y);
      sprintf(str_accel_z, "accel_z : %d", ctr_input_data->accelerometer_state.z);
 
      char str_gyro_x[20];
      char str_gyro_y[20];
      char str_gyro_z[20];

      sprintf(str_gyro_x, "gyro_x  : %d", ctr_input_data->gyroscope_state.x);
      sprintf(str_gyro_y, "gyro_y  : %d", ctr_input_data->gyroscope_state.y);
      sprintf(str_gyro_z, "gyro_z  : %d", ctr_input_data->gyroscope_state.z);
      params.y -= 0.06f;
      font_driver_render_msg_bottom(ctr, str_gyro_x,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_gyro_y,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_gyro_z,
         &params);
//accel coords
      params.y -= 0.06f;
      font_driver_render_msg_bottom(ctr, str_accel_x,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_accel_y,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_accel_z,
         &params);
#endif

      char str_mouse_abs_x[30];
      char str_mouse_abs_y[30];
      char str_mouse_delta_x[30];
      char str_mouse_delta_y[30];

      sprintf(str_mouse_abs_x,   "mouse_abs_x   : %d", ctr_input_data->mouse_state.abs_x);
      sprintf(str_mouse_abs_y,   "mouse_abs_y   : %d", ctr_input_data->mouse_state.abs_y);
      sprintf(str_mouse_delta_x, "mouse_delta_x : %d", ctr_input_data->mouse_state.delta_x);
      sprintf(str_mouse_delta_y, "mouse_delta_y : %d", ctr_input_data->mouse_state.delta_y);

      params.y -= 0.06f;
      font_driver_render_msg_bottom(ctr, str_mouse_delta_x,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_mouse_delta_y,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_mouse_abs_x,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_mouse_abs_y,
         &params);

// scaling coords
      char str_inside[30];
      char str_res_x[30];
      char str_res_y[30];
      char str_res_screen_x[30];
      char str_res_screen_y[30];
      char str_vp_x[30];
      char str_vp_y[30];
      char str_vp_width[30];
      char str_vp_height[30];
      char str_vp_full_width[30];
      char str_vp_full_height[30];


      sprintf(str_inside,         "inside         : %d", ctr_input_data->debug_state.inside);
      sprintf(str_res_x,          "res_x          : %d", ctr_input_data->debug_state.res_x);
      sprintf(str_res_y,          "res_y          : %d", ctr_input_data->debug_state.res_y);
      sprintf(str_res_screen_x,   "res_screen_x   : %d", ctr_input_data->debug_state.res_screen_x);
      sprintf(str_res_screen_y,   "res_screen_y   : %d", ctr_input_data->debug_state.res_screen_y);
      sprintf(str_vp_x,           "vp_x           : %d", ctr_input_data->debug_state.vp_x);
      sprintf(str_vp_y,           "vp_y           : %d", ctr_input_data->debug_state.vp_y);
      sprintf(str_vp_width,       "vp_width       : %d", ctr_input_data->debug_state.vp_width);
      sprintf(str_vp_height,      "vp_height      : %d", ctr_input_data->debug_state.vp_height);
      sprintf(str_vp_full_width,  "vp_full_width  : %d", ctr_input_data->debug_state.vp_full_width);
      sprintf(str_vp_full_height, "vp_full_height : %d", ctr_input_data->debug_state.vp_full_height);

      params.x = 0.52f; // right side
	  params.y = 0.90f; // top side

      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_inside,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_res_x,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_res_y,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_res_screen_x,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_res_screen_y,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_vp_x,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_vp_y,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_vp_width,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_vp_height,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_vp_full_width,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_vp_full_height,
         &params);

      char str_mouse_pos_x[30];
      char str_mouse_pos_y[30];

      sprintf(str_mouse_pos_x,   "mouse_pos_x   : %d", ctr_input_data->mouse_state.pos_x);
      sprintf(str_mouse_pos_y,   "mouse_pos_y   : %d", ctr_input_data->mouse_state.pos_y);

      params.y -= 0.10f;
      font_driver_render_msg_bottom(ctr, str_mouse_pos_x,
         &params);
      params.y -= 0.04f;
      font_driver_render_msg_bottom(ctr, str_mouse_pos_y,
         &params);
   }
      else if (debug_id == 1)
   {
	      struct font_params params = { 0, };
   ctr_video_t *ctr          = (ctr_video_t*)data;
   settings_t *settings      = config_get_ptr();
	  

   char str_mode[20];
   char str_target[20];

   char str_isPressed[20];
   char str_key[20];
   char str_gfx[20];

   char str_x0[20];
   char str_y0[20];
   char str_x1[20];
   char str_y1[20];

   char str_texture_name[256];
   char str_texture_path[256];


   sprintf(str_target,     "target   : %d", settings->uints.video_ctr_render_target);
   sprintf(str_mode,       "mode     : %d", ctr_bottom_state.mode);

   sprintf(str_isPressed,  "isPressed: %d", ctr_bottom_state_kbd.isPressed);
   sprintf(str_key,        "key      : %d", ctr_bottom_kbd_lut[ctr_bottom_state_kbd.isPressed].key);
   sprintf(str_gfx,        "gfx      : %d", ctr_bottom_kbd_lut[ctr_bottom_state_kbd.isPressed].gfx);
   sprintf(str_x0,         "x0       : %d", ctr_bottom_kbd_lut[ctr_bottom_state_kbd.isPressed].x0);
   sprintf(str_y0,         "y0       : %d", ctr_bottom_kbd_lut[ctr_bottom_state_kbd.isPressed].y0);
   sprintf(str_x1,         "x1       : %d", ctr_bottom_kbd_lut[ctr_bottom_state_kbd.isPressed].x1);
   sprintf(str_y1,         "y1       : %d", ctr_bottom_kbd_lut[ctr_bottom_state_kbd.isPressed].y1);


   sprintf(str_texture_name, "1: %s", ctr_bottom_state_gfx.texture_name);
   sprintf(str_texture_path, "2: %s", ctr_bottom_state_gfx.texture_path);



   params.text_align = TEXT_ALIGN_LEFT;
   params.color = COLOR_ABGR(255, 255, 255, 255);
   params.scale = 1.0f;
   params.x = 0.70f;

   params.y = 0.93f;
   font_driver_render_msg_bottom(ctr, str_target,
         &params);

   params.y = 0.87f;
   font_driver_render_msg_bottom(ctr, str_mode,
         &params);

   params.y = 0.81f;
   font_driver_render_msg_bottom(ctr, str_isPressed,
         &params);

   params.y = 0.75f;
   font_driver_render_msg_bottom(ctr, str_key,
         &params);

   params.y = 0.69f;
   font_driver_render_msg_bottom(ctr, str_gfx,
         &params);

   params.y = 0.63f;
   font_driver_render_msg_bottom(ctr, str_x0,
         &params);

   params.y = 0.57f;
   font_driver_render_msg_bottom(ctr, str_y0,
         &params);

   params.y = 0.51f;
   font_driver_render_msg_bottom(ctr, str_x1,
         &params);

   params.y = 0.45f;
   font_driver_render_msg_bottom(ctr, str_y1,
         &params);


   params.x = 0.00f;
   params.y = 0.39f;
   font_driver_render_msg_bottom(ctr, str_texture_name,
         &params);

   params.y = 0.33f;
   font_driver_render_msg_bottom(ctr, str_texture_path,
         &params);
   }
}
