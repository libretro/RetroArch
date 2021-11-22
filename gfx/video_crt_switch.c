/* CRT SwitchRes Core
 * Copyright (C) 2018 Alphanu / Ben Templeman.
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

#include "../retroarch.h"
#include <retro_common_api.h>
#include "video_crt_switch.h"
#include "video_display_server.h"
#include "../core_info.h"
#include "../verbosity.h"
#include "../file_path_special.h"
#include "../paths.h"
#include "gfx_display.h"

#if !defined(HAVE_VIDEOCORE) 
#include "../deps/switchres/switchres_wrapper.h"
static sr_mode srm;
#endif

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#if defined(HAVE_VIDEOCORE) /* need to add video core to SR2 */
#include "include/userland/interface/vmcs_host/vc_vchi_gencmd.h"
static void crt_rpi_switch(videocrt_switch_t *p_switch,int width, int height, float hz, int xoffset, int native_width);
#endif

static void crt_check_hh_core(videocrt_switch_t *p_switch);
static void crt_adjust_sr_ini(videocrt_switch_t *p_switch);
static bool ini_overrides_loaded = false;
static char core_name[256]; /* same size as library_name on retroarch_data.h */
static char content_dir[PATH_MAX_LENGTH];

static bool crt_check_for_changes(videocrt_switch_t *p_switch)
{
   if ((p_switch->ra_tmp_height != p_switch->ra_core_height) ||
       (p_switch->ra_core_width != p_switch->ra_tmp_width) || 
       (p_switch->center_adjust != p_switch->tmp_center_adjust||
       p_switch->porch_adjust  !=  p_switch->tmp_porch_adjust ) ||
       (p_switch->ra_core_hz != p_switch->ra_tmp_core_hz))
      return true;
 

   return false;
}

static void crt_store_temp_changes(videocrt_switch_t *p_switch)
{
   p_switch->ra_tmp_height     = p_switch->ra_core_height;
   p_switch->ra_tmp_width      = p_switch->ra_core_width;
   p_switch->tmp_center_adjust = p_switch->center_adjust;
   p_switch->tmp_porch_adjust  = p_switch->porch_adjust;
   p_switch->ra_tmp_core_hz = p_switch->ra_core_hz;

}

static void switch_crt_hz(videocrt_switch_t *p_switch)
{
   video_monitor_set_refresh_rate(p_switch->sr_core_hz);
 
}


static void crt_aspect_ratio_switch(
      videocrt_switch_t *p_switch,
      unsigned width, unsigned height, float srm_width, float srm_height)
{
   /* send aspect float to video_driver */
   p_switch->fly_aspect = (float)width / (float)height;
   video_driver_set_aspect_ratio_value((float)p_switch->fly_aspect);
   RARCH_LOG("[CRT]: Setting Aspect Ratio: %f \n", (float)p_switch->fly_aspect);

   RARCH_LOG("[CRT]: Setting Video Screen Size to: %dx%d \n", width, height);
   video_driver_set_size(width , height); 
   video_driver_set_viewport(width , height,1,1);

   video_driver_apply_state_changes();
   
}

static void set_aspect(videocrt_switch_t *p_switch, unsigned int width, 
      unsigned int height, unsigned int srm_width, unsigned srm_height,
      float srm_xscale, float srm_yscale, bool srm_isstretched )
{
   unsigned int patched_width = 0;
   unsigned int patched_height = 0;
   int scaled_width = 0;
   int scaled_height = 0;

   /* used to fix aspect should SR not find a resolution */
   if (srm_width == 0)
   {
      video_driver_get_size(&patched_width, &patched_height);
      srm_xscale = 1;
      srm_yscale = 1;
   }else{
      /* use native values as we will be multiplying by srm scale later. */
      patched_width = width;
      patched_height = height;
   }
   if (srm_width >= 1920)
   {
      srm_xscale = (float)srm_width/width;
      RARCH_LOG("[CRT]: Super resolution detected. Fractal scaling @ X:%f Y:%d \n", srm_xscale, (int)srm_yscale);
   }
   else if (srm_isstretched && srm_width > 0 ){
      srm_xscale = (float)srm_width/width;
      srm_yscale = (float)srm_height/height;
      RARCH_LOG("[CRT]: Resolution is stretched. Fractal scaling @ X:%f Y:%f \n", srm_xscale, srm_yscale);
   }
   else
      RARCH_LOG("[CRT]: SR integer scaled  X:%d Y:%d \n",srm.x_scale, srm.y_scale);

   scaled_width = roundf(patched_width*srm_xscale);
   scaled_height = roundf(patched_height*srm_yscale);

   crt_aspect_ratio_switch(p_switch, scaled_width, scaled_height, srm_width, srm_height);
}
#if !defined(HAVE_VIDEOCORE) 
static bool crt_sr2_init(videocrt_switch_t *p_switch, int monitor_index, unsigned int crt_mode, unsigned int super_width)
{
   const char* err_msg;
   char* mode;
   char index[10];
  

   if (monitor_index+1 >= 0 && monitor_index+1 < 10)
      sprintf(index, "%d", monitor_index);
   else
      sprintf(index, "%s", "0");

   if (!p_switch->sr2_active)
   {

      RARCH_LOG("[CRT]: SR init \n");

      sr_init();
      #if (__STDC_VERSION__ >= 199409L) /* no logs for C98 or less */
       sr_set_log_callback_info(RARCH_LOG); 
      sr_set_log_callback_debug(RARCH_DBG); 
      sr_set_log_callback_error(RARCH_ERR); 
      #endif
   
      if (crt_mode == 1)
      { 
         sr_set_monitor("arcade_15");
         RARCH_LOG("[CRT]: CRT Mode: %d - arcade_15 \n", crt_mode) ;
      }else if (crt_mode == 2)
      {
         sr_set_monitor("arcade_31");
         RARCH_LOG("[CRT]: CRT Mode: %d - arcade_31 \n", crt_mode) ;
      }else if (crt_mode == 3)
      {
         sr_set_monitor("pc_31_120");
         RARCH_LOG("[CRT]: CRT Mode: %d - pc_31_120 \n", crt_mode) ;
      }else if (crt_mode == 4)
      {
         RARCH_LOG("[CRT]: CRT Mode: %d - Selected from ini \n", crt_mode) ;
      }

      if (super_width >2 )
         sr_set_user_mode(super_width, 0, 0);
      
      RARCH_LOG("[CRT]: SR init_disp \n");
      if (monitor_index+1 > 0)
      {
         RARCH_LOG("[CRT]: RA Monitor Index Manual: %s\n", &index[0]);
         p_switch->rtn = sr_init_disp(index); 
         RARCH_LOG("[CRT]: SR Disp Monitor Index Manual: %s  \n", &index[0]);
      }

      if (monitor_index == -1)
      {
         RARCH_LOG("[CRT]: RA Monitor Index Auto: %s\n","auto");
         p_switch->rtn = sr_init_disp("auto");
         RARCH_LOG("[CRT]: SR Disp Monitor Index Auto: Auto  \n");
      }

      RARCH_LOG("[CRT]: SR rtn %d \n", p_switch->rtn);

      if(p_switch->rtn == 1)
      {
         core_name[0] = '\0';
         content_dir[0] = '\0';
      }
   }
   
   if (p_switch->rtn == 1)
   {
      p_switch->sr2_active = true;
     return true;
   }else{
      RARCH_LOG("[CRT]: SR failed to init \n");
      sr_deinit();
      p_switch->sr2_active = false;
   } 

   return false;
}


static void switch_res_crt(
      videocrt_switch_t *p_switch,
      unsigned width, unsigned height, unsigned crt_mode, unsigned native_width, int monitor_index, int super_width)
{
   char current_core_name[sizeof(core_name)];
   char current_content_dir[sizeof(content_dir)];

   unsigned char interlace = 0,   ret;
   const char* err_msg;
   int w = native_width, h = height;
   double rr = p_switch->ra_core_hz;
   
   if (crt_sr2_init(p_switch, monitor_index, crt_mode, super_width)) /* Checked SR2 is loded if not Load it */
   {
      /* Check for core and content changes in case we need to make any adjustments */
      if(crt_switch_core_name())
         strlcpy(current_core_name, crt_switch_core_name(), sizeof(current_core_name));
      else
         current_core_name[0] = '\0';
      fill_pathname_parent_dir_name(current_content_dir, path_get(RARCH_PATH_CONTENT), sizeof(current_content_dir));
      if (!string_is_equal(core_name, current_core_name) || !string_is_equal(content_dir, current_content_dir))
      {
         /* A core or content change was detected, we update the current values and make adjustments */
         strlcpy(core_name, current_core_name, sizeof(core_name));
         strlcpy(content_dir, current_content_dir, sizeof(content_dir));
         RARCH_LOG("[CRT]: Current running core %s \n", core_name);
         crt_adjust_sr_ini(p_switch);
         crt_check_hh_core(p_switch);
      }
      ret =   sr_switch_to_mode(w, h, rr, interlace, &srm);
      if(!ret) 
      {
         RARCH_LOG("[CRT]: SR failed to switch mode");
         /*sr_deinit();*/
            
      }
      p_switch->sr_core_hz = srm.refresh;

      set_aspect(p_switch, w , h, srm.width, srm.height, (float)srm.x_scale, (float)srm.y_scale, srm.is_stretched);

   }else {
      set_aspect(p_switch, width , height, width, height ,(float)1,(float)1, false);
      video_driver_set_size(width , height); 
      video_driver_apply_state_changes();

   }
}
#endif
void crt_destroy_modes(videocrt_switch_t *p_switch)
{
 
   if (p_switch->sr2_active == true)
   {
      p_switch->sr2_active = false;
      sr_deinit();
      /*RARCH_LOG("[CRT]: SR Destroyed \n"); */
      
   }

}

static void crt_check_hh_core(videocrt_switch_t *p_switch)
{
   /*
   char* handheld[8] = {"mGBA","Gambatte","gpSP","Gearboy","VBA Next","VBA-M","SameBoy","TGB Dual"};
   int i = 0;
   for(i = 0; i < 7; i++)
   {
      if (strcmp(handheld[i],p_switch->core_name) == 0)
      {
         RARCH_LOG("[CRT]: Handheld core detected %s adjusting resolutions.\n", p_switch->core_name);   
         p_switch->hh_core = true;        
         break;
      }
      else
      {
         p_switch->hh_core = false;
      }               
 

   }
 
   */
   p_switch->hh_core = false;
 
} 
#if !defined(HAVE_VIDEOCORE) 
static void crt_fix_hh_res(videocrt_switch_t *p_switch, int native_width, int width,  
            int height, int crt_mode, int monitor_index, int super_width)
{
   int corrected_width = 320;
   int corrected_height = 240;

   switch_res_crt(p_switch, corrected_width, corrected_height , crt_mode, corrected_width, monitor_index-1, super_width);
   set_aspect(p_switch, native_width , height, native_width, height ,(float)1,(float)1, false);
   video_driver_set_size(native_width , height);
            

}
#endif
/*
static void crt_menu_restore(videocrt_switch_t *p_switch)
{

       video_driver_get_size(&p_switch->fb_width, &p_switch->fb_height); 
      RARCH_LOG("[CRT]: Menu Only Restoring Aspect: %dx%d \n", p_switch->fb_width, p_switch->fb_height);
      crt_aspect_ratio_switch(p_switch, p_switch->fb_width, p_switch->fb_height, p_switch->fb_width, p_switch->fb_height);

}

static bool crt_get_desktop_res(videocrt_switch_t *p_switch, unsigned width, unsigned height, float hz)
{
   if (p_switch->menu_active ==  false) 
   {
      if (p_switch->fb_width == 0)
         video_driver_get_size(&p_switch->fb_width, &p_switch->fb_height);

      p_switch->fb_ra_core_hz = 60.0;
      RARCH_LOG("[CRT]: Storing Desktop Resolution: %dx%d@%f \n", p_switch->fb_width, p_switch->fb_height, p_switch->fb_ra_core_hz);
      crt_menu_restore(p_switch);
      p_switch->menu_active = true;
      return true;
   }
   return false;
}
*/

void crt_switch_res_core(
      videocrt_switch_t *p_switch,
      unsigned native_width, unsigned width, unsigned height,
      float hz, unsigned crt_mode,
      int crt_switch_center_adjust,
      int crt_switch_porch_adjust,
      int monitor_index, bool dynamic,
      int super_width, bool hires_menu)
{
   if (height <= 4)
   {
      if (hires_menu == true)
      {
         native_width = 640;
         width = 640;
         height = 480;
         hz = 60;
      }else{
         native_width = 320;
         width = 320;
         height = 240;
         hz = 60;
      }
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

      /* Detect resolution change and switch */
      if (crt_check_for_changes(p_switch))
      {
         RARCH_LOG("[CRT]: Requested Resolution: %dx%d@%f \n", native_width, height, hz);
         #if defined(HAVE_VIDEOCORE)
         crt_rpi_switch(p_switch, width, height, hz, 0, native_width);
         #else

         if (p_switch->hh_core == false)
            switch_res_crt(p_switch, p_switch->ra_core_width, p_switch->ra_core_height , crt_mode, native_width, monitor_index-1, super_width);
         else
            crt_fix_hh_res(p_switch, native_width, width, height, crt_mode, monitor_index, super_width);

         #endif
         switch_crt_hz(p_switch);
         crt_store_temp_changes(p_switch);
      }

      if (video_driver_get_aspect_ratio() != p_switch->fly_aspect)
      {
         RARCH_LOG("[CRT]: Restoring Aspect Ratio: %f \n", (float)p_switch->fly_aspect);
         video_driver_set_aspect_ratio_value((float)p_switch->fly_aspect);
         video_driver_apply_state_changes();
      }

   }
   
}

void crt_adjust_sr_ini(videocrt_switch_t *p_switch)
{
   char config_directory[PATH_MAX_LENGTH];
   char switchres_ini_override_file[PATH_MAX_LENGTH];

   if(p_switch->sr2_active)
   {
      /* First we reload the base switchres.ini file to undo any overrides that might have been loaded for another core */
      if(ini_overrides_loaded)
      {
         RARCH_LOG("[CRT]: Loading default switchres.ini \n");
         sr_load_ini((char *)"switchres.ini");
         ini_overrides_loaded = false;
      }
      if(strlen(core_name) > 0) {
         /* Then we look for config/Core Name/Core Name.switchres.ini and load it, overriding any variables it specifies */
         config_directory[0] = '\0';
         fill_pathname_application_special(config_directory,
            sizeof(config_directory), APPLICATION_SPECIAL_DIRECTORY_CONFIG);
         fill_pathname_join_special_ext(switchres_ini_override_file,
            config_directory, core_name, core_name, ".switchres.ini", sizeof(switchres_ini_override_file));
         if(path_is_valid(switchres_ini_override_file))
         {
            RARCH_LOG("[CRT]: Loading switchres.ini core override file from %s \n", switchres_ini_override_file);
            sr_load_ini(switchres_ini_override_file);
            ini_overrides_loaded = true;
         }
         /* Next up we load directory overrides, if any */
         fill_pathname_join_special_ext(switchres_ini_override_file,
            config_directory, core_name, content_dir, ".switchres.ini", sizeof(switchres_ini_override_file));
         if(path_is_valid(switchres_ini_override_file))
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
static void crt_rpi_switch(videocrt_switch_t *p_switch, int width, int height, float hz, int xoffset, int native_width)
{
   char buffer[1024];
   VCHI_INSTANCE_T vchi_instance;
   VCHI_CONNECTION_T *vchi_connection  = NULL;
   static char output[250]             = {0};
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
      height = height/2;

   /* set core refresh from hz */
   video_monitor_set_refresh_rate(hz);

   set_aspect(p_switch, width, 
      height, width, height,
      (float)1, (float)1, false);
   int w = width;
   while (w < 1920) 
   {
      w = w+width;
   }
   
   if (w > 2000)
         w =w- width;
   
   width = w;

   crt_aspect_ratio_switch(p_switch, width,height,width,height);
   
   /* following code is the mode line generator */
   hfp      = ((width * 0.044) + (width / 112));
   hbp      = ((width * 0.172) + (width /64));


   hsp         = (width * 0.117);

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
   vbp  = (vmax-height)-vsp-vfp;
   hmax = width+hfp+hsp+hbp;

   if (height < 300)
   {
      pixel_clock = (hmax * vmax * hz) ;
      ip_flag     = 0;
   }

   if (height > 300)
   {
      pixel_clock = (hmax * vmax * (hz/2)) /2 ;
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
   snprintf(output2,  sizeof(output1),
         "fbset -g %d %d %d %d 24 > /dev/null",
         width, height, width, height);
   system(output2);

   crt_switch_driver_refresh();
}
#endif
