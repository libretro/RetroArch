/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#include <gccore.h>
#include <ogc/pad.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

#include "../../config.def.h"

#include "../../tasks/tasks_internal.h"

#ifdef GEKKO
#define WPADInit WPAD_Init
#define WPADDisconnect WPAD_Disconnect
#define WPADProbe WPAD_Probe
#define WPADSetDataFormat WPAD_SetDataFormat
#endif

#define WPAD_EXP_SICKSAXIS     252
#define WPAD_EXP_GAMECUBE      253
#define WPAD_EXP_NOCONTROLLER  254

#ifdef HW_RVL
#define NUM_DEVICES 4
#else
#define NUM_DEVICES 1
#endif

#ifdef HW_RVL
#define MAX_MOUSEBUTTONS 6
static const uint32_t gx_mousemask[MAX_MOUSEBUTTONS] = 
{
   WPAD_BUTTON_B,
   WPAD_BUTTON_A,
   WPAD_BUTTON_1,
   WPAD_BUTTON_2, 
   WPAD_BUTTON_PLUS,
   WPAD_BUTTON_MINUS
};

struct gx_mousedata
{
   int32_t x, y;
   uint32_t mouse_button;
   bool valid;
};

static struct gx_mousedata gx_mouse[2];
#endif

enum
{
   GX_GC_A                 = 0,
   GX_GC_B                 = 1,
   GX_GC_X                 = 2,
   GX_GC_Y                 = 3,
   GX_GC_START             = 4,
   GX_GC_HOME              = 5,/* needed on GameCube as "fake" menu button. */
   GX_GC_Z_TRIGGER         = 6,
   GX_GC_L_TRIGGER         = 7,
   GX_GC_R_TRIGGER         = 8,
   GX_GC_UP                = 9,
   GX_GC_DOWN              = 10,
   GX_GC_LEFT              = 11,
   GX_GC_RIGHT             = 12,
#ifdef HW_RVL
   GX_CLASSIC_A            = 13,
   GX_CLASSIC_B            = 14,
   GX_CLASSIC_X            = 15,
   GX_CLASSIC_Y            = 16,
   GX_CLASSIC_PLUS         = 17,
   GX_CLASSIC_MINUS        = 18,
   GX_CLASSIC_HOME         = 19,
   GX_CLASSIC_L_TRIGGER    = 20,
   GX_CLASSIC_R_TRIGGER    = 21,
   GX_CLASSIC_ZL_TRIGGER   = 22,
   GX_CLASSIC_ZR_TRIGGER   = 23,
   GX_CLASSIC_UP           = 24,
   GX_CLASSIC_DOWN         = 25,
   GX_CLASSIC_LEFT         = 26,
   GX_CLASSIC_RIGHT        = 27,
   GX_WIIMOTE_A            = 28,
   GX_WIIMOTE_B            = 29,
   GX_WIIMOTE_1            = 30,
   GX_WIIMOTE_2            = 31,
   GX_WIIMOTE_PLUS         = 32,
   GX_WIIMOTE_MINUS        = 33,
   GX_WIIMOTE_HOME         = 34,
   GX_WIIMOTE_UP           = 35,
   GX_WIIMOTE_DOWN         = 36,
   GX_WIIMOTE_LEFT         = 37,
   GX_WIIMOTE_RIGHT        = 38,
   GX_NUNCHUK_Z            = 39,
   GX_NUNCHUK_C            = 40,
   GX_NUNCHUK_UP           = 41,
   GX_NUNCHUK_DOWN         = 42,
   GX_NUNCHUK_LEFT         = 43,
   GX_NUNCHUK_RIGHT        = 44,
#endif
   GX_QUIT_KEY             = 62,
};

#define GC_JOYSTICK_THRESHOLD (48 * 256)
#define WII_JOYSTICK_THRESHOLD (40 * 256)

extern uint64_t lifecycle_state;
static uint64_t pad_state[DEFAULT_MAX_PADS];
static uint32_t pad_type[DEFAULT_MAX_PADS] = { WPAD_EXP_NOCONTROLLER, WPAD_EXP_NOCONTROLLER, WPAD_EXP_NOCONTROLLER, WPAD_EXP_NOCONTROLLER };
static int16_t analog_state[DEFAULT_MAX_PADS][2][2];
static bool g_menu = false;

static bool gx_joypad_query_pad(unsigned pad);

#ifdef HW_RVL
static bool g_quit = false;

static void power_callback(void)
{
   g_quit = true;
}
#endif

static void reset_cb(void)
{
   g_menu = true;
}

#ifdef HW_RVL
static inline void gx_mouse_info(uint32_t joybutton, unsigned port)
{
   uint8_t i;
   ir_t ir;

   /* Get the IR data from the wiimote */
   WPAD_IR(port, &ir);
   if (ir.valid)
   {
      gx_mouse[port].valid = true;
      gx_mouse[port].x = ir.x;
      gx_mouse[port].y = ir.y;
   }
   else
   {
      gx_mouse[port].valid = false;
   }

   /* reset button state */
   gx_mouse[port].mouse_button = 0; 
   for (i = 0; i < MAX_MOUSEBUTTONS; i++)
      gx_mouse[port].mouse_button |= (joybutton & gx_mousemask[i]) ? (1 << i) : 0;

   /* Small adjustment to match the RA buttons */
   gx_mouse[port].mouse_button = gx_mouse[port].mouse_button << 2;
}

bool gxpad_mousevalid(unsigned port)
{
   return gx_mouse[port].valid;
}

void gx_joypad_read_mouse(unsigned port, int *irx, int *iry, uint32_t *button)
{
   *irx    = gx_mouse[port].x;
   *iry    = gx_mouse[port].y;
   *button = gx_mouse[port].mouse_button;
}
#endif

static const char *gx_joypad_name(unsigned pad)
{
   switch (pad_type[pad])
   {
#ifdef HW_RVL
      case WPAD_EXP_NONE:
         return "Wiimote Controller";
      case WPAD_EXP_NUNCHUK:
         return "Nunchuk Controller";
      case WPAD_EXP_CLASSIC:
         return "Classic Controller";
#endif
      case WPAD_EXP_GAMECUBE:
         return "GameCube Controller";
   }

   return NULL;
}

static void handle_hotplug(unsigned port, uint32_t ptype)
{
   pad_type[port] = ptype;

   if (ptype != WPAD_EXP_NOCONTROLLER)
      input_autoconfigure_connect(
            gx_joypad_name(port),
            NULL,
            gx_joypad.ident,
            port,
            0,
            0
            );
}

static void check_port0_active(uint8_t pad_count)
{
   settings_t *settings = config_get_ptr();
   int idx = settings->uints.input_joypad_map[0];

   if(pad_count < 2 && idx != 0)
   {
#ifdef HW_RVL
      pad_type[0] = WPAD_EXP_NONE;
#else
      pad_type[0] = WPAD_EXP_GAMECUBE;
#endif
      settings->uints.input_joypad_map[0] = 0;
               
      input_autoconfigure_connect(
            gx_joypad_name(0),
            NULL,
            gx_joypad.ident,
            0, // port
            0,
            0
            );

      input_config_set_device_name(0, gx_joypad_name(0));
   }
}

static bool gx_joypad_button(unsigned port, uint16_t key)
{
   if (port >= DEFAULT_MAX_PADS)
      return false;
   return (pad_state[port] & (UINT64_C(1) << key));
}

static void gx_joypad_get_buttons(unsigned port, input_bits_t *state)
{
	if (port < DEFAULT_MAX_PADS)
   {
		BITS_COPY16_PTR( state, pad_state[port] );
	}
   else
		BIT256_CLEAR_ALL_PTR(state);
}

static int16_t gx_joypad_axis(unsigned port, uint32_t joyaxis)
{
   int val     = 0;
   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (joyaxis == AXIS_NONE || port >= DEFAULT_MAX_PADS)
      return 0;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   switch (axis)
   {
      case 0:
         val = analog_state[port][0][0];
         break;
      case 1:
         val = analog_state[port][0][1];
         break;
      case 2:
         val = analog_state[port][1][0];
         break;
      case 3:
         val = analog_state[port][1][1];
         break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

#ifdef HW_RVL

#ifndef PI
#define PI 3.14159265f
#endif

static int16_t WPAD_StickX(WPADData *data, u8 right)
{
  float mag = 0.0f;
  float ang = 0.0f;

  switch (data->exp.type)
  {
    case WPAD_EXP_NUNCHUK:
    case WPAD_EXP_GUITARHERO3:
      if (right == 0)
      {
        mag = data->exp.nunchuk.js.mag;
        ang = data->exp.nunchuk.js.ang;
      }
      break;

    case WPAD_EXP_CLASSIC:
      if (right == 0)
      {
        mag = data->exp.classic.ljs.mag;
        ang = data->exp.classic.ljs.ang;
      }
      else
      {
        mag = data->exp.classic.rjs.mag;
        ang = data->exp.classic.rjs.ang;
      }
      break;

    default:
      break;
  }

  /* calculate X value (angle need to be converted into radian) */
  if (mag > 1.0f)
     mag = 1.0f;
  else if (mag < -1.0f)
     mag = -1.0f;
  double val = mag * sin(PI * ang/180.0f);

  return (int16_t)(val * 32767.0f);
}

static int16_t WPAD_StickY(WPADData *data, u8 right)
{
   double val;
   float mag = 0.0f;
   float ang = 0.0f;

   switch (data->exp.type)
   {
      case WPAD_EXP_NUNCHUK:
      case WPAD_EXP_GUITARHERO3:
         if (right == 0)
         {
            mag = data->exp.nunchuk.js.mag;
            ang = data->exp.nunchuk.js.ang;
         }
         break;

      case WPAD_EXP_CLASSIC:
         if (right == 0)
         {
            mag = data->exp.classic.ljs.mag;
            ang = data->exp.classic.ljs.ang;
         }
         else
         {
            mag = data->exp.classic.rjs.mag;
            ang = data->exp.classic.rjs.ang;
         }
         break;

      default:
         break;
   }

   /* calculate Y value (angle need to be converted into radian) */
   if (mag > 1.0f)
      mag = 1.0f;
   else if (mag < -1.0f)
      mag = -1.0f;
   val    = -mag * cos(PI * ang/180.0f);

   return (int16_t)(val * 32767.0f);
}
#endif

static void gx_joypad_poll(void)
{
   unsigned i, j, port;
   uint8_t pad_count = 0;
   uint8_t gcpad = 0;
   uint64_t state_p1;
   uint64_t check_menu_toggle;

   pad_state[0] = 0;
   pad_state[1] = 0;
   pad_state[2] = 0;
   pad_state[3] = 0;

   gcpad = PAD_ScanPads();

#ifdef HW_RVL
   if (g_quit)
   {
      rarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
      g_quit = false;
      return;
   }

   WPAD_ReadPending(WPAD_CHAN_ALL, NULL);
#endif

   for (port = 0; port < DEFAULT_MAX_PADS; port++)
   {
      uint32_t down = 0, ptype = WPAD_EXP_NOCONTROLLER;
      uint64_t *state_cur = &pad_state[port];

      if (gcpad & (1 << port))
      {
         int16_t ls_x, ls_y, rs_x, rs_y;
         uint64_t menu_combo = 0;

         down = PAD_ButtonsHeld(port);

         *state_cur |= (down & PAD_BUTTON_A) ? (UINT64_C(1) << GX_GC_A) : 0;
         *state_cur |= (down & PAD_BUTTON_B) ? (UINT64_C(1) << GX_GC_B) : 0;
         *state_cur |= (down & PAD_BUTTON_X) ? (UINT64_C(1) << GX_GC_X) : 0;
         *state_cur |= (down & PAD_BUTTON_Y) ? (UINT64_C(1) << GX_GC_Y) : 0;
         *state_cur |= (down & PAD_BUTTON_UP) ? (UINT64_C(1) << GX_GC_UP) : 0;
         *state_cur |= (down & PAD_BUTTON_DOWN) ? (UINT64_C(1) << GX_GC_DOWN) : 0;
         *state_cur |= (down & PAD_BUTTON_LEFT) ? (UINT64_C(1) << GX_GC_LEFT) : 0;
         *state_cur |= (down & PAD_BUTTON_RIGHT) ? (UINT64_C(1) << GX_GC_RIGHT) : 0;
         *state_cur |= (down & PAD_BUTTON_START) ? (UINT64_C(1) << GX_GC_START) : 0;
         *state_cur |= (down & PAD_TRIGGER_Z) ? (UINT64_C(1) << GX_GC_Z_TRIGGER) : 0;
         *state_cur |= ((down & PAD_TRIGGER_L) || PAD_TriggerL(port) > 127) ? (UINT64_C(1) << GX_GC_L_TRIGGER) : 0;
         *state_cur |= ((down & PAD_TRIGGER_R) || PAD_TriggerR(port) > 127) ? (UINT64_C(1) << GX_GC_R_TRIGGER) : 0;

         ls_x = (int16_t)PAD_StickX(port) * 256;
         ls_y = (int16_t)PAD_StickY(port) * -256;
         rs_x = (int16_t)PAD_SubStickX(port) * 256;
         rs_y = (int16_t)PAD_SubStickY(port) * -256;

         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] = ls_x;
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] = ls_y;
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = rs_x;
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = rs_y;

         menu_combo = (UINT64_C(1) << GX_GC_START) | (UINT64_C(1) << GX_GC_Z_TRIGGER) |
                      (UINT64_C(1) << GX_GC_L_TRIGGER) | (UINT64_C(1) << GX_GC_R_TRIGGER);

         if ((*state_cur & menu_combo) == menu_combo)
            *state_cur |= (UINT64_C(1) << GX_GC_HOME);

         ptype = WPAD_EXP_GAMECUBE;
      }
#ifdef HW_RVL
      else if (WPADProbe(port, &ptype) == WPAD_ERR_NONE)
      {
         WPADData *wpaddata = (WPADData*)WPAD_Data(port);

         down = wpaddata->btns_h;

         /* Mouse & Lightgun: Retrieve IR data */
         if (ptype == WPAD_EXP_NONE)
            if (port == WPAD_CHAN_0 || port == WPAD_CHAN_1)
               gx_mouse_info(wpaddata->btns_h, port);

         *state_cur |= (down & WPAD_BUTTON_A) ? (UINT64_C(1) << GX_WIIMOTE_A) : 0;
         *state_cur |= (down & WPAD_BUTTON_B) ? (UINT64_C(1) << GX_WIIMOTE_B) : 0;
         *state_cur |= (down & WPAD_BUTTON_1) ? (UINT64_C(1) << GX_WIIMOTE_1) : 0;
         *state_cur |= (down & WPAD_BUTTON_2) ? (UINT64_C(1) << GX_WIIMOTE_2) : 0;
         *state_cur |= (down & WPAD_BUTTON_PLUS) ? (UINT64_C(1) << GX_WIIMOTE_PLUS) : 0;
         *state_cur |= (down & WPAD_BUTTON_MINUS) ? (UINT64_C(1) << GX_WIIMOTE_MINUS) : 0;
         *state_cur |= (down & WPAD_BUTTON_HOME) ? (UINT64_C(1) << GX_WIIMOTE_HOME) : 0;



         if (ptype != WPAD_EXP_NUNCHUK)
         {
            /* Rotated d-pad on Wiimote. */
            *state_cur |= (down & WPAD_BUTTON_UP) ? (UINT64_C(1) << GX_WIIMOTE_LEFT) : 0;
            *state_cur |= (down & WPAD_BUTTON_DOWN) ? (UINT64_C(1) << GX_WIIMOTE_RIGHT) : 0;
            *state_cur |= (down & WPAD_BUTTON_LEFT) ? (UINT64_C(1) << GX_WIIMOTE_DOWN) : 0;
            *state_cur |= (down & WPAD_BUTTON_RIGHT) ? (UINT64_C(1) << GX_WIIMOTE_UP) : 0;
         }

         if (ptype == WPAD_EXP_CLASSIC)
         {
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_A) ? (UINT64_C(1) << GX_CLASSIC_A) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_B) ? (UINT64_C(1) << GX_CLASSIC_B) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_X) ? (UINT64_C(1) << GX_CLASSIC_X) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_Y) ? (UINT64_C(1) << GX_CLASSIC_Y) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_UP) ? (UINT64_C(1) << GX_CLASSIC_UP) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_DOWN) ? (UINT64_C(1) << GX_CLASSIC_DOWN) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_LEFT) ? (UINT64_C(1) << GX_CLASSIC_LEFT) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_RIGHT) ? (UINT64_C(1) << GX_CLASSIC_RIGHT) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_PLUS) ? (UINT64_C(1) << GX_CLASSIC_PLUS) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_MINUS) ? (UINT64_C(1) << GX_CLASSIC_MINUS) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_HOME) ? (UINT64_C(1) << GX_CLASSIC_HOME) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_FULL_L) ? (UINT64_C(1) << GX_CLASSIC_L_TRIGGER) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_FULL_R) ? (UINT64_C(1) << GX_CLASSIC_R_TRIGGER) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_ZL) ? (UINT64_C(1) << GX_CLASSIC_ZL_TRIGGER) : 0;
            *state_cur |= (down & WPAD_CLASSIC_BUTTON_ZR) ? (UINT64_C(1) << GX_CLASSIC_ZR_TRIGGER) : 0;

            analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X]  = WPAD_StickX(wpaddata, 0);
            analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y]  = WPAD_StickY(wpaddata, 0);
            analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = WPAD_StickX(wpaddata, 1);
            analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = WPAD_StickY(wpaddata, 1);
         }
         else if (ptype == WPAD_EXP_NUNCHUK)
         {
            /* Wiimote is held upright with nunchuk,
             * do not change d-pad orientation. */
            *state_cur |= (down & WPAD_BUTTON_UP) ? (UINT64_C(1) << GX_WIIMOTE_UP) : 0;
            *state_cur |= (down & WPAD_BUTTON_DOWN) ? (UINT64_C(1) << GX_WIIMOTE_DOWN) : 0;
            *state_cur |= (down & WPAD_BUTTON_LEFT) ? (UINT64_C(1) << GX_WIIMOTE_LEFT) : 0;
            *state_cur |= (down & WPAD_BUTTON_RIGHT) ? (UINT64_C(1) << GX_WIIMOTE_RIGHT) : 0;

            *state_cur |= (down & WPAD_NUNCHUK_BUTTON_Z) ? (UINT64_C(1) << GX_NUNCHUK_Z) : 0;
            *state_cur |= (down & WPAD_NUNCHUK_BUTTON_C) ? (UINT64_C(1) << GX_NUNCHUK_C) : 0;

            analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] = WPAD_StickX(wpaddata, 0);
            analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] = WPAD_StickY(wpaddata, 0);
         }
      }
#endif

      /* Count active controllers */
      if(gx_joypad_query_pad(port))
         pad_count++;

      /* Always enable 1 pad in port 0 if there's only 1 controller connected. 
       * This avoids being stuck in rgui input settings. */
      check_port0_active(pad_count);

      if (ptype != pad_type[port])
         handle_hotplug(port, ptype);

      for (i = 0; i < 2; i++)
         for (j = 0; j < 2; j++)
            if (analog_state[port][i][j] == -0x8000)
               analog_state[port][i][j] = -0x7fff;
   }

   state_p1 = pad_state[0];

   BIT64_CLEAR(lifecycle_state, RARCH_MENU_TOGGLE);
   if (g_menu)
   {
      state_p1 |= (UINT64_C(1) << GX_GC_HOME);
      g_menu = false;
   }

   check_menu_toggle = UINT64_C(1) << GX_GC_HOME;
#ifdef HW_RVL
   check_menu_toggle |= (UINT64_C(1) << GX_WIIMOTE_HOME) | (UINT64_C(1) << GX_CLASSIC_HOME);
#endif

   if (check_menu_toggle & state_p1)
      BIT64_SET(lifecycle_state, RARCH_MENU_TOGGLE);
}

static bool gx_joypad_init(void *data)
{
   int i;
   SYS_SetResetCallback(reset_cb);
#ifdef HW_RVL
   SYS_SetPowerCallback(power_callback);
#endif

   (void)data;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
      pad_type[i] = WPAD_EXP_NOCONTROLLER;

   PAD_Init();
#ifdef HW_RVL
   WPADInit();
   /* Set IR for all wiimotes */
   WPAD_SetVRes(WPAD_CHAN_ALL,640,480);
   WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
#endif

   gx_joypad_poll();

   return true;
}

static bool gx_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS && pad_type[pad] != WPAD_EXP_NOCONTROLLER;
}

static void gx_joypad_destroy(void)
{
#ifdef HW_RVL
#if 0
   int i;
   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      /* Commenting this out fixes the Wii
       * remote not reconnecting after
       * core load, exit, etc. */
      WPAD_Flush(i);
      WPADDisconnect(i);
   }
#endif
#endif
}

input_device_driver_t gx_joypad = {
   gx_joypad_init,
   gx_joypad_query_pad,
   gx_joypad_destroy,
   gx_joypad_button,
   gx_joypad_get_buttons,
   gx_joypad_axis,
   gx_joypad_poll,
   NULL,
   gx_joypad_name,
   "gx",
};
