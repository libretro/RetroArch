/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2020      - Google contributor
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

#ifdef __PSL1GHT__
#include <io/kb.h>
#else
#include <sdk_version.h>
#include <cell/keyboard.h>
#endif

#include <boolean.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <defines/ps3_defines.h>
#include <retro_inline.h>

#include "../input_driver.h"

#ifdef HAVE_MOUSE
#ifndef MAX_MICE
#define MAX_MICE 7
#endif
#endif

#include "../../config.def.h"

#include "../../tasks/tasks_internal.h"
#ifdef __PSL1GHT__
#include <spurs/spurs.h>
#endif

#ifdef HAVE_LIGHTGUN
#include <sys/spu.h>
#include <io/camera.h>
#include <io/move.h>
#include <vectormath/c/vectormath_aos.h>
#define SPURS_PREFIX_NAME "gemsample"
#endif

#ifdef HAVE_MOUSE
#define PS3_MAX_MICE 7
#endif

#define PS3_MAX_KB_PORT_NUM 7

/* TODO/FIXME -
 * fix game focus toggle */

typedef struct
{
   float x;
   float y;
   float z;
} sensor_t;

typedef struct ps3_input
{
#ifdef HAVE_MOUSE
   unsigned mice_connected;
#endif
   KbInfo kbinfo;
   KbData kbdata[PS3_MAX_KB_PORT_NUM];
#ifdef HAVE_LIGHTGUN
   cameraType type;
   cameraReadInfo camread;
   cameraInfoEx camInf;
   sys_mem_container_t container;
   gemAttribute gem_attr;
   gemInfo gem_info;
   gemVideoConvertAttribute gem_video_convert;
   gemState gem_state;
   gemInertialState gem_inertial_state;
   u8 *cam_buf;
   Spurs *spurs ATTRIBUTE_PRXPTR;
   sys_spu_thread_t *threads;
   void *gem_memory ATTRIBUTE_PRXPTR;
   void *buffer_mem ATTRIBUTE_PRXPTR;
   void *video_out ATTRIBUTE_PRXPTR;
   float adj_x;
   float adj_y;
   unsigned gem_connected;
   unsigned gem_init;
   int t_pressed;
   int start_pressed;
   int select_pressed;
   int m_pressed;
   int square_pressed;
   int circle_pressed;
   int cross_pressed;
   int triangle_pressed;
   u16 pos_x;
   u16 pos_y;
   u16 oldGemPad;
   u16 newGemPad;
   u16 newGemAnalogT;
   u8 video_frame[640*480*4];
#endif
   int connected[PS3_MAX_KB_PORT_NUM];
} ps3_input_t;

static int mod_table[] = {
    RETROK_RSUPER,
    RETROK_RALT,
    RETROK_RSHIFT,
    RETROK_RCTRL,
    RETROK_LSUPER,
    RETROK_LALT,
    RETROK_LSHIFT,
    RETROK_LCTRL
};

static void ps3_connect_keyboard(ps3_input_t *ps3, int port)
{
   ioKbSetCodeType(port, KB_CODETYPE_RAW);
   ioKbSetReadMode(port, KB_RMODE_INPUTCHAR);
   ps3->connected[port] = 1;
}

#ifdef HAVE_LIGHTGUN
static void ps3_end_camera(ps3_input_t *ps3)
{
   cameraStop(0);
   cameraClose(0);
   cameraEnd();
   sysMemContainerDestroy(ps3->container);
}

static int ps3_setup_camera(ps3_input_t *ps3)
{
   int error = 0;

   cameraGetType(0, &ps3->type);
   if (ps3->type == CAM_TYPE_PLAYSTATION_EYE)
   {
      ps3->camInf.format         = CAM_FORM_RAW8;
      ps3->camInf.framerate      = 60;
      ps3->camInf.resolution     = CAM_RESO_VGA;
      ps3->camInf.info_ver       = 0x0101;
      ps3->camInf.container      = ps3->container;

      switch (cameraOpenEx(0, &ps3->camInf))
      {
         case CAMERA_ERRO_DOUBLE_OPEN:
            cameraClose(0);
            error                = 1;
            break;
         case CAMERA_ERRO_NO_DEVICE_FOUND:
            error                = 1;
            break;
         case 0:
            ps3->camread.buffer  = ps3->camInf.buffer;
            ps3->camread.version = 0x0100;
            ps3->cam_buf         = (u8 *)(u64)ps3->camread.buffer;
            break;
         default:
            error                = 1;
            break;
      }
   }
   else
      error = 1;
   return error;
}

#if 0
/* TODO/FIXME - function never used? */
static int ps3_init_camera(ps3_input_t *ps3)
{
  int ret = sysMemContainerCreate(&ps3->container, 0x200000);
  ret     = cameraInit();
  if (ret == 0)
    return ps3_setup_camera(ps3);
  return ret;
}
#endif

static int ps3_read_camera(ps3_input_t *ps3)
{
   int ret = cameraReadEx(0, &ps3->camread);
   switch (ret)
   {
      case CAMERA_ERRO_NEED_START:
       cameraReset(0);
       ret = gemPrepareCamera(128, 0.5);
       ret = cameraStart(0);
       break;
    case 0:
       break;
    default:
       ret = 1;
       break;
  }
  if (ret == 0 && ps3->camread.readcount != 0)
     return ps3->camread.readcount;
  return 0;
}

static int ps3_process_gem(ps3_input_t *ps3, int t)
{
   switch (t)
   {
      case 0:
         return gemUpdateStart((void *)(u64)ps3->camread.buffer, ps3->camread.timestamp);
      case 1:
         return gemConvertVideoStart((void *)(u64)ps3->camread.buffer);
      case 2:
         return gemUpdateFinish();
      case 3:
         return gemConvertVideoFinish();
      default:
         break;
   }
   return -1;

}

#if 0
/* TODO/FIXME - not used for now */
static int ps3_process_move(ps3_input_t *ps3)
{
   const unsigned int hues[] = { 4 << 24, 4 << 24, 4 << 24, 4 << 24 };
   int ret = -1;

   if (ps3_read_camera(ps3) > 0)
   {
      ret = gemUpdateStart(ps3->camread.buffer, ps3->camread.timestamp);
      if (ret == 0)
      {
         ret = gemUpdateFinish();
         if (ret == 0)
         {
            ret = gemGetState(0, STATE_LATEST_IMAGE_TIME, 0, &ps3->gem_state);
            switch (ret)
            {
               case 2:
                 gemForceRGB(0, 0.5, 0.5, 0.5);
                 break;
               case 5:
                 gemTrackHues(hues, NULL);
                 break;
               default:
                 break;
            }
         }
      }
   }

   return ret;
}
#endif

static int ps3_init_spurs(ps3_input_t *ps3)
{
   int ppu_prio;
   sys_ppu_thread_t ppu_thread_id;
   unsigned int nthread;
   int ret      = sysSpuInitialize(6, 0);
   ret          = sysThreadGetId(&ppu_thread_id);
   ret          = sysThreadGetPriority(ppu_thread_id, &ppu_prio);

   /* initialize spurs */
   ps3->spurs   = (Spurs *)memalign(SPURS_ALIGN, sizeof(Spurs));
   SpursAttribute attributeSpurs;

   if ((ret     = spursAttributeInitialize(&attributeSpurs, 5, 250, ppu_prio - 1, true)))
      return ret;

   if ((ret     = spursAttributeSetNamePrefix(&attributeSpurs, SPURS_PREFIX_NAME, strlen(SPURS_PREFIX_NAME))))
      return ret;

   if ((ret     = spursInitializeWithAttribute(ps3->spurs, &attributeSpurs)))
      return ret;

   if ((ret     = spursGetNumSpuThread(ps3->spurs, &nthread)))
      return ret;

   ps3->threads = (sys_spu_thread_t *)malloc(sizeof(sys_spu_thread_t) * nthread);

   if ((ret = spursGetSpuThreadId(ps3->spurs, ps3->threads, &nthread)))
      return ret;

   SpursInfo info;
   ret = spursGetInfo(ps3->spurs, &info);

   return 0;
}

static int ps3_end_spurs(ps3_input_t *ps3)
{
   spursFinalize(ps3->spurs);
   free(ps3->spurs);
   free(ps3->threads);
   return 0;
}

static int ps3_end_gem(ps3_input_t *ps3)
{
   ps3_end_spurs(ps3);
   gemEnd();
   free(ps3->gem_memory);
   return 0;
}

static inline void ps3_init_attribute_gem(
      gemAttribute * attribute,
      u32 max_connect, void *memory_ptr,
      Spurs *spurs, const u8 spu_priorities[8])
{
   int i;

   attribute->version              = 2;
   attribute->max                  = max_connect;
   attribute->spurs                = spurs;
   attribute->memory               = memory_ptr;
   for (i = 0; i < 8; ++i)
      attribute->spu_priorities[i] = spu_priorities[i];
}

static int ps3_init_gem_video_convert(ps3_input_t *ps3)
{
   ps3->gem_video_convert.version        = 2;
   ps3->gem_video_convert.format         = 2; /* GEM_RGBA_640x480; */
   ps3->gem_video_convert.conversion     = GEM_AUTO_WHITE_BALANCE 
                                         | GEM_COMBINE_PREVIOUS_INPUT_FRAME
                                         | GEM_FILTER_OUTLIER_PIXELS 
				                             | GEM_GAMMA_BOOST;
   ps3->gem_video_convert.gain           = 1.0f;
   ps3->gem_video_convert.red_gain       = 1.0f;
   ps3->gem_video_convert.green_gain     = 1.0f;
   ps3->gem_video_convert.blue_gain      = 1.0f;
   ps3->buffer_mem                       = (void *)memalign(128, 640*480);
   ps3->video_out                        = (void *)ps3->video_frame;
   ps3->gem_video_convert.buffer_memory  = ps3->buffer_mem;
   ps3->gem_video_convert.video_data_out = ps3->video_out;
   ps3->gem_video_convert.alpha          = 255;

   return gemPrepareVideoConvert(&ps3->gem_video_convert);
}

static int ps3_init_gem(ps3_input_t *ps3)
{
   gemAttribute gem_attr;
   u8 gem_spu_priorities[8] = { 1, 1, 1, 1, 1, 0, 0, 0 };	/* execute */
                /* libgem jobs */
                /* on 5 SPUs */
   if (ps3_init_spurs(ps3))
      return -1;

   if (!(ps3->gem_memory = (void *)malloc(gemGetMemorySize(1))))
      return -1;

   ps3_init_attribute_gem(&gem_attr, 1, ps3->gem_memory,
		   ps3->spurs, gem_spu_priorities);

   gemInit(&gem_attr);
   ps3_init_gem_video_convert(ps3);
   gemPrepareCamera (128, 0.5);
   gemReset(0);

   return 0;
}

static void ps3_read_gem_pad(ps3_input_t *ps3, int num_gem)
{
   unsigned int hues[] = { 4 << 24, 4 << 24, 4 << 24, 4 << 24 };
   int ret             = gemGetState(0, 0, -22000, &ps3->gem_state);

   ps3->newGemPad      = ps3->gem_state.paddata.buttons & (~ps3->oldGemPad);
   ps3->newGemAnalogT  = ps3->gem_state.paddata.ANA_T;
   ps3->oldGemPad      = ps3->gem_state.paddata.buttons;

   switch (ret)
   {
      case 2:
         gemForceRGB (num_gem, 0.5, 0.5, 0.5);
         break;
      case 5:
         gemTrackHues (hues, NULL);
         break;
      default:
         break;
   }
}

#if 0
/* TODO/FIXME - functions not used for now */
static void ps3_read_gem_acc_position(int num_gem)
{
   vec_float4 position;
   VmathVector4 v;
   gemGetAccelerometerPositionInDevice(num_gem, &position);

   v.vec128 = position;
}

static void ps3_read_gem_inertial(ps3_input_t *ps3, int num_gem)
{
   VmathVector4 v;
   gemGetInertialState(num_gem, 0, -22000, &ps3->gem_inertial_state);
   v.vec128 = ps3->gem_inertial_state.accelerometer;
   v.vec128 = ps3->gem_inertial_state.accelerometer_bias;
   v.vec128 = ps3->gem_inertial_state.gyro;
   v.vec128 = ps3->gem_inertial_state.gyro_bias;
}
#endif

static void ps3_read_gem(ps3_input_t *ps3)
{
   VmathVector4 v;

   ps3_process_gem(ps3, 0);
   ps3_process_gem(ps3, 1);
   ps3_process_gem(ps3, 2);
   ps3_process_gem(ps3, 3);
   ps3_read_gem_pad(ps3, 0); /* This will read buttons from Move */
   v.vec128 = ps3->gem_state.pos;
   switch (ps3->newGemPad)
   {
      case 1:
         ps3->select_pressed++;
         break;
      case 2:
         ps3->t_pressed++;
         break;
      case 4:
         ps3->m_pressed++;
         gemCalibrate(0);
         ps3->adj_x = v.vec128[0];
         ps3->adj_y = v.vec128[1];
         break;
      case 8:
         ps3->start_pressed++;
         break;
      case 16:
         ps3->triangle_pressed++;
         break;
      case 32:
         ps3->circle_pressed++;
         break;
      case 64:
         ps3->cross_pressed++;
#if 0
         ps3_read_gem_acc_position(0);
#endif
         break;
      case 128:
         ps3->square_pressed++;
#if 0
         ps3_read_gem_inertial(ps3, 0);
#endif
         break;
      default:
         break;
   }
}
#endif /* HAVE_LIGHTGUN */

static void ps3_input_poll(void *data)
{
   unsigned i, j;
   KbData last_kbdata[PS3_MAX_KB_PORT_NUM];
#ifdef HAVE_MOUSE
   mouseInfo mouse_info;
#endif
#ifdef HAVE_LIGHTGUN
   gemInfo gem_info;
#endif
   ps3_input_t *ps3 = (ps3_input_t*)data;

   ioKbGetInfo(&ps3->kbinfo);

   for (i = 0; i < PS3_MAX_KB_PORT_NUM; i++)
   {
      if (ps3->kbinfo.status[i] && !ps3->connected[i])
         ps3_connect_keyboard(ps3, i);
   }

   memcpy(last_kbdata, ps3->kbdata, sizeof(last_kbdata));
   for (i = 0; i < PS3_MAX_KB_PORT_NUM; i++)
   {
      if (ps3->kbinfo.status[i])
         ioKbRead(i, &ps3->kbdata[i]);
   }

   for (i = 0; i < PS3_MAX_KB_PORT_NUM; i++)
   {
      /* Set keyboard modifier based on shift,ctrl and alt state */
      uint16_t mod          = 0;

      if (     ps3->kbdata[i].mkey._KbMkeyU._KbMkeyS.l_alt 
            || ps3->kbdata[i].mkey._KbMkeyU._KbMkeyS.r_alt)
         mod |= RETROKMOD_ALT;
      if (     ps3->kbdata[i].mkey._KbMkeyU._KbMkeyS.l_ctrl 
            || ps3->kbdata[i].mkey._KbMkeyU._KbMkeyS.r_ctrl)
         mod |= RETROKMOD_CTRL;
      if (     ps3->kbdata[i].mkey._KbMkeyU._KbMkeyS.l_shift
            || ps3->kbdata[i].mkey._KbMkeyU._KbMkeyS.r_shift)
         mod |= RETROKMOD_SHIFT;

      /* TODO/FIXME: Windows keys.  */

      for (j = 0; j < last_kbdata[i].nb_keycode; j++)
      {
         unsigned k;
         int code            = last_kbdata[i].keycode[j];
         int newly_depressed = 1;

         for (k = 0; k < PS3_MAX_KB_PORT_NUM; i++)
         {
            if (ps3->kbdata[i].keycode[k] == code)
            {
               newly_depressed = 0;
               break;
            }
         }

         if (newly_depressed)
         {
            unsigned keyboardcode = input_keymaps_translate_keysym_to_rk(code);
            input_keyboard_event(false, keyboardcode, keyboardcode, mod, RETRO_DEVICE_KEYBOARD);
         }
      }

      for (j = 0; j < ps3->kbdata[i].nb_keycode; j++)
      {
         unsigned k;
         int code          = ps3->kbdata[i].keycode[j];
         int newly_pressed = 1;

         for (k = 0; k < PS3_MAX_KB_PORT_NUM; i++)
         {
            if (last_kbdata[i].keycode[k] == code)
            {
               newly_pressed = 0;
               break;
            }
         }

         if (newly_pressed)
         {
            unsigned keyboardcode = input_keymaps_translate_keysym_to_rk(code);
            input_keyboard_event(true, keyboardcode, keyboardcode, mod, RETRO_DEVICE_KEYBOARD);
         }
      }
   }

#ifdef HAVE_MOUSE
   ioMouseGetInfo(&mouse_info);
#ifdef __PSL1GHT__
   ps3->mice_connected = mouse_info.connected;
#else
   ps3->mice_connected = mouse_info.now_connect;
#endif
#endif
#ifdef HAVE_LIGHTGUN
   gemGetInfo(&gem_info);
   ps3->gem_connected = gem_info.connected;
#endif
}

static bool ps3_keyboard_port_input_pressed(
      ps3_input_t *ps3, unsigned id)
{
   int code;
   unsigned i, j;

   if (id >= RETROK_LAST || id == 0)
      return false;

   for (i = 0; i < 8; i++)
   {
      if (id == mod_table[i])
      {
         for (j = 0; j < PS3_MAX_KB_PORT_NUM; j++)
         {
            if (ps3->kbinfo.status[j] 
                  && (ps3->kbdata[j].mkey._KbMkeyU.mkeys & (1 << i)))
               return true;
         }
         return false;
      }
   }

   if ((code = rarch_keysym_lut[id]) != 0)
   {
      for (i = 0; i < PS3_MAX_KB_PORT_NUM; i++)
      {
         if (ps3->kbinfo.status[i])
         {
            for (j = 0; j < ps3->kbdata[i].nb_keycode; j++)
            {
               if (ps3->kbdata[i].keycode[j] == code)
                  return true;
            }
         }
      }
   }

   return false;
}

#ifdef HAVE_MOUSE
static int16_t ps3_mouse_device_state(ps3_input_t *ps3,
      unsigned user, unsigned id)
{
   mouseData mouse_state;
   ioMouseGetData(id, &mouse_state);

   switch (id)
   {
      /* TODO: mouse wheel up/down */
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return (mouse_state.buttons & CELL_MOUSE_BUTTON_1);
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return (mouse_state.buttons & CELL_MOUSE_BUTTON_2);
      case RETRO_DEVICE_ID_MOUSE_X:
         return (mouse_state.x_axis);
      case RETRO_DEVICE_ID_MOUSE_Y:
         return (mouse_state.y_axis);
   }

   return 0;
}
#endif

#ifdef HAVE_LIGHTGUN
static int16_t ps3_lightgun_device_state(ps3_input_t *ps3,
      unsigned user, unsigned id)
{
   float pointer_x;
   float pointer_y;
   videoState state;
   videoResolution res;
   VmathVector3 ray_start, ray_dir;
   struct video_viewport vp;
   float center_y              = 0.0f;
   float center_x              = 0.0f;
   const int edge_detect       = 32700;
   bool inside                 = false;
   int16_t res_x               = 0;
   int16_t res_y               = 0;
   int16_t res_screen_x        = 0;
   int16_t res_screen_y        = 0;
   float sensitivity           = 1.0f;
   if (!ps3->gem_connected || !ps3->gem_init)
      return 0;

   ps3_read_camera(ps3);
   ps3_read_gem(ps3);

   videoGetState(0, 0, &state);
   videoGetResolution(state.displayMode.resolution, &res);

   if (res.height == 720)
   {
      /* 720p offset adjustments */
      center_x                 = 645.0f;
      center_y                 = 375.0f;
   }
   else if (res.height == 1080)
   {
      /* 1080p offset adjustments */
      center_x                 = 960.0f;
      center_y                 = 565.0f;
   }

   vp.x                        = 0;
   vp.y                        = 0;
   vp.width                    = 0;
   vp.height                   = 0;
   vp.full_width               = 0;
   vp.full_height              = 0;

   /* tracking mode 1: laser pointer mode (this is closest 
      to actual lightgun behavior) */
   ray_start.vec128            = ps3->gem_state.pos;
   VmathVector3 ray_tmp        = {.vec128 = {0.0f,0.0f,-1.0f,0.0f}};
   const VmathQuat *quat       = (VmathQuat *)&ps3->gem_state.quat;
   vmathQRotate(&ray_dir, quat, &ray_tmp);
   float t                     = -ray_start.vec128[2] / ray_dir.vec128[2];
   pointer_x                   = ray_start.vec128[0] + ray_dir.vec128[0]*t;
   pointer_y                   = ray_start.vec128[1] + ray_dir.vec128[1]*t;

#if 0
   /* tracking mode 2: 3D coordinate system (move pointer position by moving the
 * whole controller) */
   VmathVector4 v;
   v.vec128                    = ps3->gem_state.pos;
   pointer_x                   = v.vec128[0];
   pointer_y                   = v.vec128[1];
#endif

   if (video_driver_translate_coord_viewport_wrap(&vp,
           center_x + ((pointer_x - ps3->adj_x) * sensitivity), center_y + ((pointer_y - ps3->adj_y) * sensitivity),
           &res_x, &res_y, &res_screen_x, &res_screen_y))
   {

      inside = (res_x >= -edge_detect)
            && (res_y >= -edge_detect)
            && (res_x <= edge_detect)
            && (res_y <= edge_detect);

      switch (id)
      {
         case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
         case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
         case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
#if 0
         case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
         case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
         case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
         case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
         case RETRO_DEVICE_ID_LIGHTGUN_PAUSE: /* deprecated */
#endif
            if (ps3->t_pressed > 0)
            {
               ps3->t_pressed = 0;
               return 1;
            }
            break;
         case RETRO_DEVICE_ID_LIGHTGUN_START:
            if (ps3->start_pressed > 0)
            {
               ps3->start_pressed = 0;
               return 1;
            }
            break;
         case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
            if (ps3->select_pressed > 0)
            {
               ps3->select_pressed = 0;
               return 1;
            }
            break;
         case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
            if (ps3->triangle_pressed > 0)
            {
               ps3->triangle_pressed = 0;
               return 1;
            }
            break;
         case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
            if (inside)
               return res_x;
            break;
         case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
            if (inside)
               return ~res_y;
            break;
         case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
            return !inside;
         default:
            break;
      }
   }

   return 0;
}
#endif

static int16_t ps3_input_state(
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
   ps3_input_t *ps3           = (ps3_input_t*)data;

   if (ps3)
   {
      switch (device)
      {
         case RETRO_DEVICE_JOYPAD:
            if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            {
               int i;
               int16_t ret = 0;

               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (binds[port][i].valid)
                  {
                     if (ps3_keyboard_port_input_pressed(
                              ps3, binds[port][i].key))
                        ret |= (1 << i);
                  }
               }

               return ret;
            }

            if (binds[port][id].valid)
            {
               if (ps3_keyboard_port_input_pressed(
                        ps3, binds[port][id].key))
                  return 1;
            }
	    break;
         case RETRO_DEVICE_ANALOG:
            break;
         case RETRO_DEVICE_KEYBOARD:
            return ps3_keyboard_port_input_pressed(ps3, id);
#ifdef HAVE_MOUSE
         case RETRO_DEVICE_MOUSE:
            if (ps3->mice_connected)
               return ps3_mouse_device_state(data, port, id);
            break;
#endif
#ifdef HAVE_LIGHTGUN
         case RETRO_DEVICE_LIGHTGUN:
            return ps3_lightgun_device_state(ps3, port, id);
#endif
#if 0
         case RETRO_DEVICE_SENSOR_ACCELEROMETER:
            switch (id)
            {
               /* Fixed range of 0x000 - 0x3ff */
               case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_X:
                  return ps3->accelerometer_state[port].x;
               case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_Y:
                  return ps3->accelerometer_state[port].y;
               case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_Z:
                  return ps3->accelerometer_state[port].z;
               default:
                  break;
            }
            break;
#endif
      }
   }

   return 0;
}

static void ps3_input_free_input(void *data)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;

   if (!ps3)
      return;

   ioPadEnd();
   ioKbEnd();
#ifdef HAVE_MOUSE
   ioMouseEnd();
#endif
#ifdef HAVE_LIGHTGUN
    ps3_end_gem((ps3_input_t *)data);
    ps3_end_camera((ps3_input_t *)data);
#endif
   free(ps3);
}

static void* ps3_input_init(const char *joypad_driver)
{
   int i;
#ifdef HAVE_LIGHTGUN
   gemInfo gem_info;
#endif
   ps3_input_t *ps3 = (ps3_input_t*)calloc(1, sizeof(*ps3));
   if (!ps3)
      return NULL;

   /* Keyboard  */

   input_keymaps_init_keyboard_lut(rarch_key_map_ps3);
   ioKbInit(PS3_MAX_KB_PORT_NUM);
   ioKbGetInfo(&ps3->kbinfo);

   for (i = 0; i < PS3_MAX_KB_PORT_NUM; i++)
   {
      if (ps3->kbinfo.status[i])
         ps3_connect_keyboard(ps3, i);
   }

#ifdef HAVE_MOUSE
   ioMouseInit(MAX_MICE);
#endif
#ifdef HAVE_LIGHTGUN
   ps3->gem_init      = 0;
   gemGetInfo(&gem_info);
   ps3->gem_connected = gem_info.connected;
   if (ps3->gem_connected)
   {
      if (!cameraInit())
      {
         cameraGetType(0, &ps3->type);
         if (ps3->type == CAM_TYPE_PLAYSTATION_EYE)
         {
            if (!sysMemContainerCreate(&ps3->container, 0x200000))
            {
               if (!ps3_setup_camera(ps3))
               {
                  if (!ps3_init_gem(ps3))
                     ps3->gem_init = 1;
               }
            }
         }
      }
   }
#endif

   return ps3;
}

static uint64_t ps3_input_get_capabilities(void *data)
{
   return
#ifdef HAVE_MOUSE
        (1 << RETRO_DEVICE_MOUSE)  |
#endif
#ifdef HAVE_LIGHTGUN
        (1 << RETRO_DEVICE_LIGHTGUN) |
#endif
        (1 << RETRO_DEVICE_KEYBOARD)
      | (1 << RETRO_DEVICE_JOYPAD)
      | (1 << RETRO_DEVICE_ANALOG);
}

static bool ps3_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate)
{
   padInfo2 pad_info;

   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         ioPadGetInfo2(&pad_info);
         if ((pad_info.device_capability[port]
                  & CELL_PAD_CAPABILITY_SENSOR_MODE)
               == CELL_PAD_CAPABILITY_SENSOR_MODE)
         {
            ioPadSetPortSetting(port, CELL_PAD_SETTING_SENSOR_ON);
            return true;
         }
         break;
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         ioPadSetPortSetting(port, 0);
         return true;
      default:
         break;
   }

   return false;
}

input_driver_t input_ps3 = {
   ps3_input_init,
   ps3_input_poll,
   ps3_input_state,
   ps3_input_free_input,
   ps3_input_set_sensor_state,
   NULL,
   ps3_input_get_capabilities,
   "ps3",

   NULL,                         /* grab_mouse */
   NULL,
   NULL
};
