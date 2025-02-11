/* CRT SwitchRes Core
 *  Copyright (C) 2018 Alphanu / Ben Templeman.
 *
 * RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libretro.h>
#include <math.h>

#include <retro_common_api.h>
#include "video_crt_switch.h"
#include "video_display_server.h"
#include "../core_info.h"
#include "../verbosity.h"
#include "../file_path_special.h"
#include "../paths.h"
#include "gfx_display.h"

#include "../deps/switchres/switchres_wrapper.h"
static sr_mode srm;

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

/* Forward declarations */
static void crt_adjust_sr_ini(videocrt_switch_t *p_switch);

/* Global local variables */
static bool ini_overrides_loaded = false;
static char core_name[NAME_MAX_LENGTH]; /* Same size as library_name on retroarch_data.h */
static char content_dir[DIR_MAX_LENGTH];

#if defined(HAVE_VIDEOCORE) /* Need to add video core to SR2 */
#include "include/userland/interface/vmcs_host/vc_vchi_gencmd.h"
static void crt_rpi_switch(videocrt_switch_t *p_switch,int width, int height, float hz, int xoffset, int native_width);
#endif

static bool crt_check_for_changes(videocrt_switch_t *p_switch)
{
   if (   (p_switch->ra_core_height != p_switch->ra_tmp_height)
       || (p_switch->ra_core_width  != p_switch->ra_tmp_width)
       || (p_switch->center_adjust  != p_switch->tmp_center_adjust
       ||  p_switch->porch_adjust   != p_switch->tmp_porch_adjust)
       || (p_switch->ra_core_hz     != p_switch->ra_tmp_core_hz)
       || (p_switch->rotated        != p_switch->tmp_rotated))
      return true;
   return false;
}

static void crt_store_temp_changes(videocrt_switch_t *p_switch)
{
   p_switch->ra_tmp_height     = p_switch->ra_core_height;
   p_switch->ra_tmp_width      = p_switch->ra_core_width;
   p_switch->tmp_center_adjust = p_switch->center_adjust;
   p_switch->tmp_porch_adjust  = p_switch->porch_adjust;
   p_switch->ra_tmp_core_hz    = p_switch->ra_core_hz;
   p_switch->tmp_rotated       = p_switch->rotated;
}

static void crt_aspect_ratio_switch(
      videocrt_switch_t *p_switch,
      unsigned width, unsigned height,
      float srm_width, float srm_height,
      unsigned video_aspect_ratio_idx)
{
   float fly_aspect               = (float)width / (float)height;
   p_switch->fly_aspect           = fly_aspect;
   video_driver_state_t *video_st = video_state_get_ptr();

   /* We only force aspect ratio for the core provided setting */
   if (video_aspect_ratio_idx != ASPECT_RATIO_CORE)
   {
      RARCH_LOG("[CRT]: Aspect ratio forced by user: %f\n", video_st->aspect_ratio);
      return;
   }

   /* Send aspect float to video_driver */
   video_st->aspect_ratio         = fly_aspect;
   RARCH_LOG("[CRT]: Setting Aspect Ratio: %f \n", fly_aspect);
   RARCH_LOG("[CRT]: Setting Video Screen Size to: %dx%d \n",
         width, height);
   video_driver_set_size(width, height);
   if (video_st->current_video && video_st->current_video->set_viewport)
      video_st->current_video->set_viewport(
            video_st->data, width, height, true, true);

   video_driver_apply_state_changes();

}

static void crt_switch_set_aspect(
      videocrt_switch_t *p_switch,
      unsigned int width, unsigned int height,
      unsigned int srm_width, unsigned srm_height,
      float srm_xscale, float srm_yscale,
      bool srm_isstretched )
{
   sr_state state;
   unsigned int patched_width  = 0;
   unsigned int patched_height = 0;
   int scaled_width            = 0;
   int scaled_height           = 0;

   /* used to fix aspect should SR not find a resolution */
   if (srm_width == 0)
   {
      video_driver_get_size(&patched_width, &patched_height);
      srm_xscale               = 1;
      srm_yscale               = 1;
   }
   else
   {
      /* use native values as we will be multiplying by srm scale later. */
      patched_width            = width;
      patched_height           = height;
   }

#if !defined(HAVE_VIDEOCORE)
   sr_get_state(&state);

   if ((int)srm_width >= state.super_width && !srm_isstretched)
      RARCH_LOG("[CRT]: Super resolution detected. Fractal scaling @ X:%f Y:%f \n", srm_xscale, srm_yscale);
   else if (srm_isstretched && srm_width > 0 )
      RARCH_LOG("[CRT]: Resolution is stretched. Fractal scaling @ X:%f Y:%f \n", srm_xscale, srm_yscale);
#endif

   scaled_width  = roundf(patched_width  * srm_xscale);
   scaled_height = roundf(patched_height * srm_yscale);

   crt_aspect_ratio_switch(p_switch, scaled_width, scaled_height,
         srm_width, srm_height,
         config_get_ptr()->uints.video_aspect_ratio_idx);
}

#if !defined(HAVE_VIDEOCORE)
static bool crt_sr2_init(videocrt_switch_t *p_switch,
      int monitor_index, unsigned int crt_mode, unsigned int super_width)
{
   char *mode;
   const char *err_msg;
   char index[10];
   gfx_ctx_ident_t gfxctx;
   char ra_config_path[PATH_MAX_LENGTH];
   char sr_ini_file[PATH_MAX_LENGTH];

   if (monitor_index+1 >= 0 && monitor_index+1 < 10)
      snprintf(index, sizeof(index), "%d", monitor_index);
   else
      strlcpy(index, "0", sizeof(index));

   video_context_driver_get_ident(&gfxctx);

   p_switch->kms_ctx = (gfxctx.ident && strncmp(gfxctx.ident, "kms", 3) == 0);
   p_switch->khr_ctx = (gfxctx.ident && strncmp(gfxctx.ident, "khr_display", 11) == 0);

   RARCH_LOG("[CRT] Video context is: %s\n", gfxctx.ident);

   if (!p_switch->sr2_active)
   {
      sr_init();
#if (__STDC_VERSION__ >= 199409L) /* no logs for C98 or less */
      sr_set_log_callback_info(RARCH_LOG);
      sr_set_log_callback_debug(RARCH_DBG);
      sr_set_log_callback_error(RARCH_ERR);
#endif

      switch (crt_mode)
      {
         case 1:
            sr_set_monitor("arcade_15");
            RARCH_LOG("[CRT]: CRT Mode: %d - arcade_15 \n", crt_mode);
            break;
         case 2:
            sr_set_monitor("arcade_31");
            RARCH_LOG("[CRT]: CRT Mode: %d - arcade_31 \n", crt_mode);
            break;
         case 3:
            sr_set_monitor("pc_31_120");
            RARCH_LOG("[CRT]: CRT Mode: %d - pc_31_120 \n", crt_mode);
            break;
         case 4:
            RARCH_LOG("[CRT]: CRT Mode: %d - Selected from ini \n", crt_mode);
            break;
         default:
            break;
      }

      if (super_width > 2)
      {
         char sw[16];
         sr_set_user_mode(super_width, 0, 0);
         snprintf(sw, sizeof(sw), "%d", super_width);
         sr_set_option(SR_OPT_SUPER_WIDTH, sw);
      }

      if (p_switch->kms_ctx)
            p_switch->rtn = sr_init_disp("dummy", NULL);
      else if (monitor_index + 1 > 0)
      {
         RARCH_LOG("[CRT]: Monitor Index Manual: %s\n", &index[0]);
         p_switch->rtn = sr_init_disp(index, NULL);
      }
      else
      {
         RARCH_LOG("[CRT]: Monitor Index Auto: %s\n","auto");
         p_switch->rtn = sr_init_disp("auto", NULL);
      }

      RARCH_LOG("[CRT]: SR rtn %d \n", p_switch->rtn);

      if (p_switch->rtn >= 0)
      {
         core_name[0]   = '\0';
         content_dir[0] = '\0';
         /* For Lakka, check a switchres.ini next to user's retroarch.cfg */
         fill_pathname_application_data(ra_config_path, PATH_MAX_LENGTH);
         fill_pathname_join(sr_ini_file,
               ra_config_path, "switchres.ini", sizeof(sr_ini_file));
         if (path_is_valid(sr_ini_file))
         {
            RARCH_LOG("[CRT]: Loading switchres.ini override file from %s \n", sr_ini_file);
            sr_load_ini(sr_ini_file);
         }
      }
   }

   if (p_switch->rtn >= 0 && !p_switch->kms_ctx)
   {
      p_switch->sr2_active = true;
      return true;
   }
   else if (p_switch->rtn >= 0 && p_switch->kms_ctx)
   {
      p_switch->sr2_active = true;
      RARCH_LOG("[CRT]: KMS context detected, keeping SR alive\n");
      return true;
   }
   else if (p_switch->rtn >= 0 && p_switch->khr_ctx)
   {
      p_switch->sr2_active = true;
      RARCH_LOG("[CRT]: Vulkan context detected, keeping SR alive\n");
      return true;
   }

   RARCH_ERR("[CRT]: error at init, CRT modeswitching disabled\n");
   sr_deinit();
   p_switch->sr2_active = false;

   return false;
}

static void get_modeline_for_kms(videocrt_switch_t *p_switch, sr_mode* srm)
{
   p_switch->clock       = srm->pclock / 1000;
   p_switch->hdisplay    = srm->width;
   p_switch->hsync_start = srm->hbegin;
   p_switch->hsync_end   = srm->hend;
   p_switch->htotal      = srm->htotal;
   p_switch->vdisplay    = srm->height;
   p_switch->vsync_start = srm->vbegin;
   p_switch->vsync_end   = srm->vend;
   p_switch->vtotal      = srm->vtotal;
   p_switch->vrefresh    = srm->refresh;
   p_switch->hskew       = 0;
   p_switch->vscan       = 0;
   p_switch->interlace   = srm->interlace;
   p_switch->doublescan  = srm->doublescan;
   p_switch->hsync       = srm->hsync;
   p_switch->vsync       = srm->vsync;
}

static void switch_res_crt(
      videocrt_switch_t *p_switch,
      unsigned width, unsigned height,
      unsigned crt_mode, unsigned native_width,
      int monitor_index, int super_width)
{
   int w                   = native_width;
   int h                   = height;

   /* Check if SR2 is loaded, if not, load it */
   if (crt_sr2_init(p_switch, monitor_index, crt_mode, super_width))
   {
      int ret;
      int flags = 0;
      char current_core_name[NAME_MAX_LENGTH];
      char current_content_dir[DIR_MAX_LENGTH];
      double rr              = p_switch->ra_core_hz;
      const char *_core_name = (const char*)runloop_state_get_ptr()->system.info.library_name;
      /* Check for core and content changes in case we need
         to make any adjustments */
      if (string_is_empty(_core_name))
         current_core_name[0] = '\0';
      else
         strlcpy(current_core_name, _core_name, sizeof(current_core_name));

      fill_pathname_parent_dir_name(current_content_dir,
            path_get(RARCH_PATH_CONTENT),
            sizeof(current_content_dir));

      if (     !string_is_equal(core_name,   current_core_name)
            || !string_is_equal(content_dir, current_content_dir))
      {
         /* A core or content change was detected,
            we update the current values and make adjustments */
         strlcpy(core_name,   current_core_name,   sizeof(core_name));
         strlcpy(content_dir, current_content_dir, sizeof(content_dir));
         RARCH_LOG("[CRT]: Current running core %s \n", core_name);
         crt_adjust_sr_ini(p_switch);
         p_switch->hh_core = false;
      }

      if (p_switch->rotated)
         flags |= SR_MODE_ROTATED;

      RARCH_DBG("%dx%d rotation: %d rotated: %d core rotation:%d\n", w, h, p_switch->rotated, flags & SR_MODE_ROTATED, retroarch_get_rotation());
      ret = sr_add_mode(w, h, rr, flags, &srm);
      if (!ret)
         RARCH_ERR("[CRT]: SR failed to add mode\n");
      if (p_switch->kms_ctx)
      {
         get_modeline_for_kms(p_switch, &srm);
         video_driver_set_video_mode(srm.width, srm.height, true);
      }
      else if (p_switch->khr_ctx)
         RARCH_WARN("[CRT]: Vulkan -> Can't modeswitch for now\n");
      else
         ret = sr_set_mode(srm.id);
      if (!p_switch->kms_ctx && !ret)
         RARCH_ERR("[CRT]: SR failed to switch mode\n");
      p_switch->sr_core_hz = (float)srm.vfreq;

      crt_switch_set_aspect(p_switch,
            p_switch->rotated ? h : w,
            p_switch->rotated ? w : h,
            srm.width, srm.height,
            (float)srm.x_scale,
            (float)srm.y_scale,
            srm.is_stretched);
   }
   else
   {
      crt_switch_set_aspect(p_switch,
            width, height,
            width, height,
            1.0f,
            1.0f,
            false);
      video_driver_set_size(width , height);
      video_driver_apply_state_changes();
   }
}
#endif

void crt_destroy_modes(videocrt_switch_t *p_switch)
{
   if (p_switch->sr2_active)
   {
      p_switch->sr2_active = false;
      sr_deinit();
   }
}

void crt_switch_res_core(
      videocrt_switch_t *p_switch,
      unsigned native_width, unsigned width, unsigned height,
      float hz, bool rotated, unsigned crt_mode,
      int crt_switch_center_adjust,
      int crt_switch_porch_adjust,
      int monitor_index, bool dynamic,
      int super_width, bool hires_menu,
      unsigned video_aspect_ratio_idx)
{
   if (height <= 4)
   {
      hz              = 60;
      if (hires_menu)
      {
         native_width = 640;
         height       = 480;
      }
      else
      {
         native_width = 320;
         height       = 240;
      }
      width           = native_width;
   }

   if (height != 4 )
   {
      p_switch->menu_active           = false;
      p_switch->porch_adjust          = crt_switch_porch_adjust;
      p_switch->ra_core_height        = height;
      p_switch->ra_core_hz            = hz;

      p_switch->ra_core_width         = width;

      p_switch->center_adjust         = crt_switch_center_adjust;
      p_switch->index                 = monitor_index;
      p_switch->rotated               = rotated;

      /* Detect resolution change and switch */
      if (crt_check_for_changes(p_switch))
      {
         RARCH_LOG("[CRT]: Requested Resolution: %dx%d@%f orientation: %s\n",
                  native_width, height, hz, rotated? "rotated" : "normal");
#if defined(HAVE_VIDEOCORE)
         crt_rpi_switch(p_switch, width, height, hz, 0, native_width);
#else

         if (p_switch->hh_core)
         {
            int corrected_width  = 320;
            int corrected_height = 240;
            switch_res_crt(p_switch, corrected_width, corrected_height,
                  crt_mode, corrected_width, monitor_index-1, super_width);
            crt_switch_set_aspect(p_switch, native_width, height, native_width,
                  height ,(float)1,(float)1, false);
            video_driver_set_size(native_width , height);
         }
         else
            switch_res_crt(p_switch, p_switch->ra_core_width,
                  p_switch->ra_core_height, crt_mode,
                  native_width, monitor_index-1, super_width);
#endif
         video_monitor_set_refresh_rate(p_switch->sr_core_hz);
         crt_store_temp_changes(p_switch);
      }

      if (  (video_aspect_ratio_idx == ASPECT_RATIO_CORE)
         &&  video_driver_get_aspect_ratio() != p_switch->fly_aspect)
      {
         video_driver_state_t *video_st = video_state_get_ptr();
         float fly_aspect               = (float)p_switch->fly_aspect;
         RARCH_LOG("[CRT]: Restoring Aspect Ratio: %f \n", fly_aspect);
         video_st->aspect_ratio         = fly_aspect;
         video_driver_apply_state_changes();
      }
   }
}

void crt_adjust_sr_ini(videocrt_switch_t *p_switch)
{
   char config_directory[DIR_MAX_LENGTH];
   char switchres_ini_override_file[PATH_MAX_LENGTH];

   if (p_switch->sr2_active)
   {
      /* First we reload the base switchres.ini file
         to undo any overrides that might have been
         loaded for another core */
      if (ini_overrides_loaded)
      {
         RARCH_LOG("[CRT]: Loading default switchres.ini... \n");
         sr_load_ini((char *)"switchres.ini");
         ini_overrides_loaded = false;
      }

      if (core_name[0] != '\0')
      {
         /* Then we look for config/Core Name/Core Name.switchres.ini
            and load it, overriding any variables it specifies */
         config_directory[0] = '\0';
         fill_pathname_application_special(config_directory,
               sizeof(config_directory),
               APPLICATION_SPECIAL_DIRECTORY_CONFIG);
         fill_pathname_join_special_ext(switchres_ini_override_file,
               config_directory, core_name, core_name,
               ".switchres.ini", sizeof(switchres_ini_override_file));

         if (path_is_valid(switchres_ini_override_file))
         {
            RARCH_LOG("[CRT]: Loading switchres.ini core override file from %s \n", switchres_ini_override_file);
            sr_load_ini(switchres_ini_override_file);
            ini_overrides_loaded = true;
         }

         /* Next up we load directory overrides, if any */
         fill_pathname_join_special_ext(switchres_ini_override_file,
               config_directory, core_name, content_dir,
               ".switchres.ini", sizeof(switchres_ini_override_file));

         if (path_is_valid(switchres_ini_override_file))
         {
            RARCH_LOG("[CRT]: Loading switchres.ini content directory override file from %s \n", switchres_ini_override_file);
            sr_load_ini(switchres_ini_override_file);
            ini_overrides_loaded = true;
         }
      }
   }
}

/* only used for RPi3 */
#if defined(HAVE_VIDEOCORE)
static void crt_rpi_switch(videocrt_switch_t *p_switch,
      int width, int height, float hz,
      int xoffset, int native_width)
{
   int w;
   char buffer[1024];
   VCHI_INSTANCE_T vchi_instance;
   VCHI_CONNECTION_T *vchi_connection  = NULL;
   static char output1[250]            = {0};
   static char output2[250]            = {0};
   static char set_hdmi[250]           = {0};
   static char set_hdmi_timing[250]    = {0};
   int i                               = 0;
   int hfp                             = 0;
   int hsp                             = 0;
   int hbp                             = 0;
   int vfp                             = 0;
   int vsp                             = 0;
   int vbp                             = 0;
   int hmax                            = 0;
   int vmax                            = 0;
   int pdefault                        = 8;
   int pwidth                          = 0;
   int ip_flag                         = 0;
   float roundw                        = 0.0f;
   float roundh                        = 0.0f;
   float pixel_clock                   = 0.0f;
   int xscale                          = 1;
   int yscale                          = 1;

   if (height > 300)
      height /= 2;

   /* set core refresh from hz */
   video_monitor_set_refresh_rate(hz);

   crt_switch_set_aspect(p_switch, width,
      height, width, height,
      (float)1, (float)1, false);

   w = width;
   while (w < 1920)
      w = w+width;

   if (w > 2000)
      w = w - width;

   width = w;

   crt_aspect_ratio_switch(p_switch, width, height, width, height,
         config_get_ptr()->uints.video_aspect_ratio_idx);

   /* following code is the mode line generator */
   hfp      = ((width * 0.044f) + (width / 112));
   hbp      = ((width * 0.172f) + (width /64));

   hsp      = (width * 0.117f);

   if (height < 241)
      vmax = 261;
   if (height < 241 && hz > 56 && hz < 58)
      vmax = 280;
   if (height < 241 && hz < 55)
      vmax = 313;
   if (height > 250 && height < 260 && hz > 54)
      vmax = 296;
   if (height > 250 && height < 260 && hz > 52 && hz < 54)
      vmax = 285;
   if (height > 250 && height < 260 && hz < 52)
      vmax = 313;
   if (height > 260 && height < 300)
      vmax = 318;

   if (height > 400 && hz > 56)
      vmax = 533;
   if (height > 520 && hz < 57)
      vmax = 580;

   if (height > 300 && hz < 56)
      vmax = 615;
   if (height > 500 && hz < 56)
      vmax = 624;
   if (height > 300)
      pdefault = pdefault * 2;

   vfp = (height + ((vmax - height) / 2) - pdefault) - height;

   if (height < 300)
      vsp = vfp + 3; /* needs to be 3 for progressive */
   if (height > 300)
      vsp = vfp + 6; /* needs to be 6 for interlaced */

   vsp  = 3;
   vbp  = (vmax - height) - vsp - vfp;
   hmax = width + hfp + hsp + hbp;

   if (height < 300)
      pixel_clock = (hmax * vmax * hz);

   if (height > 300)
   {
      pixel_clock = (hmax * vmax * (hz/2)) / 2;
      ip_flag     = 1;
   }

   /* above code is the modeline generator */
   snprintf(set_hdmi_timing, sizeof(set_hdmi_timing),
         "hdmi_timings %d 1 %d %d %d %d 1 %d %d %d 0 0 0 %f %d %f 1 ",
         width, hfp, hsp, hbp, height, vfp,vsp, vbp,
         hz, ip_flag, pixel_clock);

   vcos_init();
   vchi_initialise(&vchi_instance);
   vchi_connect(NULL, 0, vchi_instance);
   vc_vchi_gencmd_init(vchi_instance, &vchi_connection, 1);
   vc_gencmd(buffer, sizeof(buffer), set_hdmi_timing);
   vc_gencmd_stop();
   vchi_disconnect(vchi_instance);
   snprintf(output1,  sizeof(output1),
         "tvservice -e \"DMT 87\" > /dev/null");
   system(output1);
   snprintf(output2,  sizeof(output2),
         "fbset -g %d %d %d %d 24 > /dev/null",
         width, height, width, height);
   system(output2);
   video_driver_reinit(DRIVER_VIDEO_MASK);
}
#endif
