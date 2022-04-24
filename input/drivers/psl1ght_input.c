/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2020  Google
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <defines/ps3_defines.h>

#include "../input_driver.h"

#include <retro_inline.h>

#include "../../config.def.h"

#include "../../tasks/tasks_internal.h"

#ifdef HAVE_LIGHTGUN
#include <sys/spu.h>
#include <io/camera.h>
#include <io/move.h>
#include <vectormath/c/vectormath_aos.h>
#define SPURS_PREFIX_NAME "gemsample"
#endif

#ifdef HAVE_MOUSE
#define MAX_MICE 7
#endif

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
   int connected[MAX_KB_PORT_NUM];
#ifdef HAVE_MOUSE
   unsigned mice_connected;
#endif
   KbInfo kbinfo;
   KbData kbdata[MAX_KB_PORT_NUM];
#ifdef HAVE_LIGHTGUN
   unsigned gem_connected, gem_init;
   cameraType type;
   cameraReadInfo camread;
   cameraInfoEx camInf;
   sys_mem_container_t container;
   u8 *cam_buf;
   gemAttribute gem_attr;
   gemInfo gem_info;
   gemVideoConvertAttribute gem_video_convert;
   gemState gem_state;
   gemInertialState gem_inertial_state;
   Spurs *spurs ATTRIBUTE_PRXPTR;
   sys_spu_thread_t *threads;
   void *gem_memory ATTRIBUTE_PRXPTR;
   void *buffer_mem ATTRIBUTE_PRXPTR;
   void *video_out ATTRIBUTE_PRXPTR;
   u8 video_frame[640*480*4];
   u16 pos_x;
   u16 pos_y;
   float adj_x;
   float adj_y;
   u16 oldGemPad;
   u16 newGemPad;
   u16 newGemAnalogT;
   int t_pressed;
   int start_pressed;
   int select_pressed;
   int m_pressed;
   int square_pressed;
   int circle_pressed;
   int cross_pressed;
   int triangle_pressed;
#endif
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

static void ps3_disconnect_keyboard(ps3_input_t *ps3, int port)
{
   ps3->connected[port] = 0;
}

#ifdef HAVE_LIGHTGUN
void endCamera(ps3_input_t *ps3)
{
   cameraStop(0);
   cameraClose(0);
   cameraEnd();
   sysMemContainerDestroy(ps3->container);
}

int setupCamera(ps3_input_t *ps3)
{
   int ret;
   int error = 0;

   cameraGetType(0, &ps3->type);
   if (ps3->type == CAM_TYPE_PLAYSTATION_EYE) {
      ps3->camInf.format = CAM_FORM_RAW8;
      ps3->camInf.framerate = 60;
      ps3->camInf.resolution = CAM_RESO_VGA;
      ps3->camInf.info_ver = 0x0101;
      ps3->camInf.container = ps3->container;

      ret = cameraOpenEx(0, &ps3->camInf);
      switch (ret)
      {
         case CAMERA_ERRO_DOUBLE_OPEN:
            cameraClose(0);
            error = 1;
            break;
         case CAMERA_ERRO_NO_DEVICE_FOUND:
            error = 1;
            break;
         case 0:
            ps3->camread.buffer = ps3->camInf.buffer;
            ps3->camread.version = 0x0100;
            ps3->cam_buf = (u8 *)(u64)ps3->camread.buffer;
            ps3->camread.buffer);
            break;
         default:
            error = 1;
      }
   }
   else
   {
      error = 1;
   }
   return error;
}

int initCamera(ps3_input_t *ps3)
{
  int ret;

  ret = sysMemContainerCreate(&ps3->container, 0x200000);
  ret = cameraInit();
  if (ret == 0)
  {
    ret = setupCamera(ps3);
  }
  return ret;

}

int readCamera(ps3_input_t *ps3)
{
   int ret;

   ret = cameraReadEx(0, &ps3->camread);
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
  {
     return ps3->camread.readcount;
  }
  else
  {
     return 0;
  }
}

int proccessGem(ps3_input_t *ps3, int t)
{
   int ret;
   switch (t) {
      case 0:
         ret = gemUpdateStart(ps3->camread.buffer, ps3->camread.timestamp);
         break;
      case 1:
         ret = gemConvertVideoStart(ps3->camread.buffer);
         break;
      case 2:
         ret = gemUpdateFinish();
         break;
      case 3:
         ret = gemConvertVideoFinish();
         break;
      default:
         ret = -1;
         break;
  }
  return ret;

}

int processMove(ps3_input_t *ps3)
{
   const unsigned int hues[] = { 4 << 24, 4 << 24, 4 << 24, 4 << 24 };
   int ret = -1;

   if (readCamera(ps3) > 0)
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

int initSpurs(ps3_input_t *ps3)
{
   int ret;
   int i;
   sys_ppu_thread_t ppu_thread_id;
   int ppu_prio;
   unsigned int nthread;

   ret = sysSpuInitialize(6, 0);
   ret = sysThreadGetId(&ppu_thread_id);
   ret = sysThreadGetPriority(ppu_thread_id, &ppu_prio);

   /* initialize spurs */
   ps3->spurs = (Spurs *)memalign(SPURS_ALIGN, sizeof(Spurs));
   SpursAttribute attributeSpurs;

   ret = spursAttributeInitialize(&attributeSpurs, 5, 250, ppu_prio - 1, true);
   if (ret)
   {
      return (ret);
   }

   ret = spursAttributeSetNamePrefix(&attributeSpurs, SPURS_PREFIX_NAME, strlen(SPURS_PREFIX_NAME));
   if (ret)
   {
      return (ret);
   }

   ret = spursInitializeWithAttribute(ps3->spurs, &attributeSpurs);
   if (ret)
   {
      return (ret);
   }

   ret = spursGetNumSpuThread(ps3->spurs, &nthread);
   if (ret)
   {
      return (ret);
   }

   ps3->threads = (sys_spu_thread_t *)malloc(sizeof(sys_spu_thread_t) * nthread);

   ret = spursGetSpuThreadId(ps3->spurs, ps3->threads, &nthread);
   if (ret)
   {
      return (ret);
   }

   SpursInfo info;
   ret = spursGetInfo(ps3->spurs, &info);
   return 0;
}

int endSpurs(ps3_input_t *ps3)
{
   spursFinalize(ps3->spurs);
   free(ps3->spurs);
   free(ps3->threads);
   return 0;
}

int endGem(ps3_input_t *ps3)
{
   endSpurs(ps3);
   gemEnd();
   free(ps3->gem_memory);
   return 0;
}

static inline void initAttributeGem(gemAttribute * attribute,
   u32 max_connect, void *memory_ptr,
   Spurs *spurs, const u8 spu_priorities[8])
{
   int i;

   attribute->version = 2;
   attribute->max = max_connect;
   attribute->spurs = spurs;
   attribute->memory = memory_ptr;
   for (i = 0; i < 8; ++i)
   {
    attribute->spu_priorities[i] = spu_priorities[i];
   }
}

int initGemVideoConvert(ps3_input_t *ps3)
{
   int ret;

   ps3->gem_video_convert.version = 2;
   ps3->gem_video_convert.format = 2; //GEM_RGBA_640x480;
   ps3->gem_video_convert.conversion= GEM_AUTO_WHITE_BALANCE | GEM_COMBINE_PREVIOUS_INPUT_FRAME |
                                      GEM_FILTER_OUTLIER_PIXELS | GEM_GAMMA_BOOST;
   ps3->gem_video_convert.gain = 1.0f;
   ps3->gem_video_convert.red_gain = 1.0f;
   ps3->gem_video_convert.green_gain = 1.0f;
   ps3->gem_video_convert.blue_gain = 1.0f;
   ps3->buffer_mem = (void *)memalign(128, 640*480);
   ps3->video_out = (void *)ps3->video_frame;
   ps3->gem_video_convert.buffer_memory = ps3->buffer_mem;
   ps3->gem_video_convert.video_data_out = ps3->video_out;
   ps3->gem_video_convert.alpha = 255;
   ret = gemPrepareVideoConvert(&ps3->gem_video_convert);
   return ret;
}

int initGem(ps3_input_t *ps3)
{
   int ret;
   int i;

   ret = initSpurs(ps3);
   if (ret)
   {
      return -1;
   }

   ret = gemGetMemorySize(1);
   ps3->gem_memory = (void *)malloc(ret);
   if (!ps3->gem_memory)
      return -1;

   u8 gem_spu_priorities[8] = { 1, 1, 1, 1, 1, 0, 0, 0 };	// execute
                // libgem jobs
                // on 5 spu
   gemAttribute gem_attr;

   initAttributeGem(&gem_attr, 1, ps3->gem_memory, ps3->spurs, gem_spu_priorities);

   ret = gemInit (&gem_attr);
   ret= initGemVideoConvert(ps3);
   ret = gemPrepareCamera (128, 0.5);
   ret = gemReset(0);
   return 0;
}

void readGemPad(ps3_input_t *ps3, int num_gem)
{
   int ret;
   unsigned int hues[] = { 4 << 24, 4 << 24, 4 << 24, 4 << 24 };
   ret = gemGetState (0, 0, -22000, &ps3->gem_state);

   ps3->newGemPad = ps3->gem_state.paddata.buttons & (~ps3->oldGemPad);
   ps3->newGemAnalogT = ps3->gem_state.paddata.ANA_T;
   ps3->oldGemPad = ps3->gem_state.paddata.buttons;

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

void readGemAccPosition(int num_gem)
{
   vec_float4 position;
   VmathVector4 v;
   gemGetAccelerometerPositionInDevice(num_gem, &position);

   v.vec128 = position;
}

void readGemInertial(ps3_input_t *ps3, int num_gem)
{
   int ret;
   VmathVector4 v;

   ret = gemGetInertialState(num_gem, 0, -22000, &ps3->gem_inertial_state);
   v.vec128 = ps3->gem_inertial_state.accelerometer;
   v.vec128 = ps3->gem_inertial_state.accelerometer_bias;
   v.vec128 = ps3->gem_inertial_state.gyro;
   v.vec128 = ps3->gem_inertial_state.gyro_bias;
}

void readGem(ps3_input_t *ps3)
{
   proccessGem(ps3, 0);
   proccessGem(ps3, 1);
   proccessGem(ps3, 2);
   proccessGem(ps3, 3);
   readGemPad(ps3, 0);		// This will read buttons from Move
   VmathVector4 v;
   v.vec128 = ps3->gem_state.pos;
   switch (ps3->newGemPad) {
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
         //readGemAccPosition(0);
         break;
      case 128:
         ps3->square_pressed++;
         //readGemInertial(ps3, 0);
         break;
      default:
         break;
   }
}
#endif // HAVE_LIGHTGUN

static void ps3_input_poll(void *data)
{
   unsigned i, j;
   ps3_input_t *ps3 = (ps3_input_t*)data;
   KbData last_kbdata[MAX_KB_PORT_NUM];

   ioKbGetInfo(&ps3->kbinfo);

   for (i = 0; i < MAX_KB_PORT_NUM; i++)
   {
      if (ps3->kbinfo.status[i] && !ps3->connected[i])
         ps3_connect_keyboard(ps3, i);
#if 0
      if (!ps3->kbinfo.status[i] && ps3->connected[i])
         ps3_disconnect_keyboard(ps3, i);
#endif
   }

   memcpy(last_kbdata, ps3->kbdata, sizeof(last_kbdata));
   for (i = 0; i < MAX_KB_PORT_NUM; i++)
   {
      if (ps3->kbinfo.status[i])
         ioKbRead(i, &ps3->kbdata[i]);
   }

   for (i = 0; i < MAX_KB_PORT_NUM; i++)
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

      /* TODO: windows keys.  */

      for (j = 0; j < last_kbdata[i].nb_keycode; j++)
      {
         unsigned k;
         int code            = last_kbdata[i].keycode[j];
         int newly_depressed = 1;

         for (k = 0; k < MAX_KB_PORT_NUM; i++)
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

         for (k = 0; k < MAX_KB_PORT_NUM; i++)
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
   mouseInfo mouse_info;
   ioMouseGetInfo(&mouse_info);
   ps3->mice_connected = mouse_info.connected;
#endif
#ifdef HAVE_LIGHTGUN
   gemInfo gem_info;
   gemGetInfo(&gem_info);
   ps3->gem_connected = gem_info.connected;
#endif
}

static bool psl1ght_keyboard_port_input_pressed(
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
         for (j = 0; j < MAX_KB_PORT_NUM; j++)
         {
            if (ps3->kbinfo.status[j] 
                  && (ps3->kbdata[j].mkey._KbMkeyU.mkeys & (1 << i)))
               return true;
         }
         return false;
      }
   }

   code = rarch_keysym_lut[id];
   if (code == 0)
      return false;
   for (i = 0; i < MAX_KB_PORT_NUM; i++)
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

   return false;
}

#ifdef HAVE_MOUSE
static int16_t ps3_mouse_device_state(ps3_input_t *ps3,
      unsigned user, unsigned id)
{
   if (!ps3->mice_connected)
      return 0;

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
   if (!ps3->gem_connected || !ps3->gem_init)
      return 0;

   readCamera(ps3);
   readGem(ps3);
   struct video_viewport vp;
   const int edge_detect       = 32700;
   bool inside                 = false;
   int16_t res_x               = 0;
   int16_t res_y               = 0;
   int16_t res_screen_x        = 0;
   int16_t res_screen_y        = 0;
   float center_x;
   float center_y;
   float pointer_x;
   float pointer_y;
   float sensitivity = 1.0f;

   videoState state;
   videoConfiguration vconfig;
   videoResolution res;
   videoGetState(0, 0, &state);
   videoGetResolution(state.displayMode.resolution, &res);

   if (res.height == 720)
   {
      // 720p offset adjustments
      center_x = 645.0f;
      center_y = 375.0f;
   }
   else if (res.height == 1080)
   {
      // 1080p offset adjustments
      center_x = 960.0f;
      center_y = 565.0f;
   }

   vp.x                        = 0;
   vp.y                        = 0;
   vp.width                    = 0;
   vp.height                   = 0;
   vp.full_width               = 0;
   vp.full_height              = 0;

#if 1
   // tracking mode 1: laser pointer mode (this is closest to actual lightgun behavior)
   VmathVector4 ray_start;
   ray_start.vec128 = ps3->gem_state.pos;
   VmathVector4 ray_tmp = {.vec128 = {0.0f,0.0f,-1.0f,0.0f}};
   const VmathQuat *quat = &ps3->gem_state.quat;
   VmathVector4 ray_dir;
   vmathQRotate(&ray_dir, quat, &ray_tmp);
   float t = -ray_start.vec128[2] / ray_dir.vec128[2];
   pointer_x = ray_start.vec128[0] + ray_dir.vec128[0]*t;
   pointer_y = ray_start.vec128[1] + ray_dir.vec128[1]*t;
#endif

#if 0
   // tracking mode 2: 3D coordinate system (move pointer position by moving the whole controller)
   VmathVector4 v;
   v.vec128 = ps3->gem_state.pos;
   pointer_x = v.vec128[0];
   pointer_y = v.vec128[1];
#endif

   if (video_driver_translate_coord_viewport_wrap(&vp,
           center_x + ((pointer_x - ps3->adj_x)*sensitivity), center_y + ((pointer_y - ps3->adj_y)*sensitivity),
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
            {
               return (res_x);
            }
            break;
         case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
            if (inside)
            {
               return (~res_y);
            }
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

   if (!ps3)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;

            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               if (binds[port][i].valid)
               {
                  if (psl1ght_keyboard_port_input_pressed(
                           ps3, binds[port][i].key))
                     ret |= (1 << i);
               }
            }

            return ret;
         }

         if (binds[port][id].valid)
         {
            if (psl1ght_keyboard_port_input_pressed(
                     ps3, binds[port][id].key))
               return 1;
         }
         break;
      case RETRO_DEVICE_ANALOG:
         break;
      case RETRO_DEVICE_KEYBOARD:
         return psl1ght_keyboard_port_input_pressed(ps3, id);
#ifdef HAVE_MOUSE
      case RETRO_DEVICE_MOUSE:
         return ps3_mouse_device_state(ps3, port, id);
#endif
#ifdef HAVE_LIGHTGUN
      case RETRO_DEVICE_LIGHTGUN:
         return ps3_lightgun_device_state(ps3, port, id);
#endif
   }

   return 0;
}

static void ps3_input_free_input(void *data)
{
    ioPadEnd();
    ioKbEnd();
#ifdef HAVE_MOUSE
    ioMouseEnd();
#endif
#ifdef HAVE_LIGHTGUN
    endGem((ps3_input_t *)data);
    endCamera((ps3_input_t *)data);
#endif
}

static void* ps3_input_init(const char *joypad_driver)
{
   unsigned i;
   ps3_input_t *ps3 = (ps3_input_t*)calloc(1, sizeof(*ps3));
   if (!ps3)
      return NULL;

   /* Keyboard  */

   input_keymaps_init_keyboard_lut(rarch_key_map_psl1ght);

   ioKbInit(MAX_KB_PORT_NUM);
   ioKbGetInfo(&ps3->kbinfo);

   for (i = 0; i < MAX_KB_PORT_NUM; i++)
   {
      if (ps3->kbinfo.status[i])
         ps3_connect_keyboard(ps3, i);
   }

#ifdef HAVE_MOUSE
   ioMouseInit(MAX_MICE);
#endif
#ifdef HAVE_LIGHTGUN
   ps3->gem_init = 0;
   gemInfo gem_info;
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
               if (!setupCamera(ps3));
               {
                  if (!initGem(ps3))
                  {
                     ps3->gem_init = 1;
                  }
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
      (1 << RETRO_DEVICE_LIGHTGUN)  |
#endif
      (1 << RETRO_DEVICE_KEYBOARD)  |
      (1 << RETRO_DEVICE_JOYPAD) |
      (1 << RETRO_DEVICE_ANALOG);
}

static bool ps3_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate) { return false; }

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
   NULL
};
