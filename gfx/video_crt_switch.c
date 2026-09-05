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

/* The CRT switching policy: the first consumer of the video modeline
 * engine in gfx/modeline/. It maps the crt_switch_* settings to a
 * monitor preset and a super width, loads the switchres.ini overlays,
 * asks the engine for a mode on every geometry change and hands the
 * result to the display server's modeline_* ops. Anything that is
 * about 15 kHz, arcade names or geometry sliders lives here; the
 * engine itself is display-agnostic. */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libretro.h>
#include <math.h>

#include <retro_common_api.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

#include "gfx_display.h"
#include "video_crt_switch.h"
#include "video_display_server.h"
#include "modeline/modeline_list.h"
#include "modeline/modeline_ini.h"
#include "modeline/modeline_edid.h"
#include "../command.h"
#include "../core_info.h"
#include "../verbosity.h"
#include "../file_path_special.h"
#include "../paths.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

/* Forward declarations */
static void crt_adjust_ini(videocrt_switch_t *p_switch);

/* Global local variables */
static bool ini_overrides_loaded = false;
static char core_name[NAME_MAX_LENGTH]; /* Same size as library_name on retroarch_data.h */
static char content_dir[DIR_MAX_LENGTH];
static char current_content_name[256];
static char content_name[256];

#if defined(HAVE_VIDEOCORE) /* Pi VIDEOCORE keeps its own tvservice path */
#include <interface/vmcs_host/vc_vchi_gencmd.h>
static void crt_rpi_switch(videocrt_switch_t *p_switch,int width, int height, float hz, int xoffset, int native_width);
#endif

static bool crt_check_for_changes(videocrt_switch_t *p_switch)
{
   if (   (p_switch->ra_core_height != p_switch->ra_tmp_height)
       || (p_switch->ra_core_width  != p_switch->ra_tmp_width)
       || (p_switch->center_adjust  != p_switch->tmp_center_adjust)
       || (p_switch->porch_adjust   != p_switch->tmp_porch_adjust)
       || (p_switch->vert_adjust   != p_switch->tmp_vert_adjust)
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
   p_switch->tmp_vert_adjust   = p_switch->vert_adjust;
}

static void crt_aspect_ratio_switch(
      videocrt_switch_t *p_switch,
      unsigned width, unsigned height,
      float srm_width, float srm_height,
      unsigned video_aspect_ratio_idx)
{
   float fly_aspect               = (float)width / (float)height;
   video_driver_state_t *video_st = video_state_get_ptr();
   p_switch->fly_aspect           = fly_aspect;

   /* We only force aspect ratio for the core provided setting */
   if (video_aspect_ratio_idx != ASPECT_RATIO_CORE)
   {
      RARCH_LOG("[CRT] Aspect ratio forced by user: %f.\n", video_st->aspect_ratio);
      return;
   }

   /* Send aspect float to video_driver */
   video_st->aspect_ratio         = fly_aspect;
   RARCH_LOG("[CRT] Setting aspect ratio: %f.\n", fly_aspect);
   RARCH_LOG("[CRT] Setting screen size: %dx%d.\n",
         width, height);
   video_driver_set_output_size(width, height);
   if (video_st->current_video && video_st->current_video->set_viewport)
      video_st->current_video->set_viewport(
            video_st->data, width, height, true, true);

   command_event(CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, NULL);
}

static void crt_switch_set_aspect(
      videocrt_switch_t *p_switch,
      unsigned int width, unsigned int height,
      unsigned int srm_width, unsigned srm_height,
      float srm_xscale, float srm_yscale,
      bool srm_isstretched )
{
   unsigned int patched_width  = 0;
   unsigned int patched_height = 0;
   int scaled_width            = 0;
   int scaled_height           = 0;

   /* used to fix aspect should the engine not find a resolution */
   if (srm_width == 0)
   {
      video_driver_get_output_size(&patched_width, &patched_height);
      srm_xscale               = 1;
      srm_yscale               = 1;
   }
   else
   {
      /* use native values as we will be multiplying by the mode scale later. */
      patched_width            = width;
      patched_height           = height;
   }

#if !defined(HAVE_VIDEOCORE)
   if (p_switch->gen)
   {
      if ((int)srm_width >= p_switch->gen->super_width && !srm_isstretched)
         RARCH_LOG("[CRT] Super resolution detected. Fractal scaling @ X:%f Y:%f.\n", srm_xscale, srm_yscale);
      else if (srm_isstretched && srm_width > 0 )
         RARCH_LOG("[CRT] Resolution is stretched. Fractal scaling @ X:%f Y:%f.\n", srm_xscale, srm_yscale);
   }
#endif

   scaled_width  = (int)floor(patched_width  * srm_xscale + 0.5f);
   scaled_height = (int)floor(patched_height * srm_yscale + 0.5f);

   crt_aspect_ratio_switch(p_switch, scaled_width, scaled_height,
         srm_width, srm_height,
         config_get_ptr()->uints.video_aspect_ratio_idx);
}

#if !defined(HAVE_VIDEOCORE)
/* After a mode is on the wire the runloop observes the new timing:
 * the field rate, and the scanline / auto frame delay calibrations
 * that depended on the previous vtotal. */
static void crt_publish_timing(videocrt_switch_t *p_switch, double vfreq)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   p_switch->sr_core_hz           = (float)vfreq;
   video_monitor_set_refresh_rate((float)vfreq);
   video_driver_scanline_init();
   if (config_get_ptr()->bools.video_frame_delay_auto)
      video_st->frame_delay_target = 0;
}

static void crt_apply_menu_preset(videocrt_switch_t *p_switch,
      unsigned crt_mode, unsigned super_width)
{
   switch (crt_mode)
   {
      case 1:
         modeline_set_monitor(p_switch->gen, "arcade_15");
         RARCH_LOG("[CRT] CRT mode: %d - arcade_15.\n", crt_mode);
         break;
      case 2:
         modeline_set_monitor(p_switch->gen, "arcade_31");
         RARCH_LOG("[CRT] CRT mode: %d - arcade_31.\n", crt_mode);
         break;
      case 3:
         modeline_set_monitor(p_switch->gen, "pc_31_120");
         RARCH_LOG("[CRT] CRT mode: %d - pc_31_120.\n", crt_mode);
         break;
      case 4:
         RARCH_LOG("[CRT] CRT mode: %d - Selected from ini.\n", crt_mode);
         break;
      default:
         break;
   }

   if (super_width > 2)
   {
      modeline_set_user_mode(p_switch->gen, super_width, 0, 0);
      p_switch->gen->super_width = super_width;
   }
}

/* With the SDL display server set to Always the user has chosen
 * listed-mode switching over the native server, and SDL can neither
 * add nor rewrite a timing: the listed modes are all there is, so the
 * engine's default lock on modes without a known timing would leave
 * it nothing but the desktop. That choice unlocks them; every other
 * path keeps the ini-controlled default, which protects a 15 kHz CRT
 * from a stock driver's VESA timings. */
static void crt_apply_server_policy(videocrt_switch_t *p_switch)
{
   settings_t *settings = config_get_ptr();
   if (!p_switch->gen)
      return;
   if (string_is_equal(p_switch->ops.name, "sdl")
         && settings->uints.video_sdl_display_server == VIDEO_SDL_DISPLAY_SERVER_ALWAYS
         && p_switch->gen->lock_system_modes)
   {
      RARCH_LOG("[CRT] SDL display server: listed modes without known timings are selectable.\n");
      p_switch->gen->lock_system_modes = false;
      modeline_parse_options(p_switch->gen);
   }
}

/* The base ini next to retroarch.cfg, the one the overlays sit on */
static bool crt_load_config_ini(videocrt_switch_t *p_switch)
{
   char ra_config_path[DIR_MAX_LENGTH];
   char ini_file[PATH_MAX_LENGTH];

   fill_pathname_application_data(ra_config_path, sizeof(ra_config_path));
   fill_pathname_join(ini_file, ra_config_path, "switchres.ini", sizeof(ini_file));
   if (!path_is_valid(ini_file))
      return false;
   RARCH_LOG("[CRT] Loading switchres.ini override file from \"%s\".\n", ini_file);
   modeline_ini_load(p_switch->gen, ini_file);
   modeline_parse_options(p_switch->gen);
   return true;
}

static bool crt_engine_init(videocrt_switch_t *p_switch,
      int monitor_index, unsigned int crt_mode, unsigned int super_width)
{
   char index[10];
   gfx_ctx_ident_t gfxctx;

   if (monitor_index+1 >= 0 && monitor_index+1 < 10)
      snprintf(index, sizeof(index), "%d", monitor_index);
   else
      strlcpy_lit(index, "0", sizeof(index));

   video_context_driver_get_ident(&gfxctx);

   p_switch->kms_ctx = (gfxctx.ident && strncmp(gfxctx.ident, "kms", 3) == 0);
   p_switch->khr_ctx = (gfxctx.ident && strncmp(gfxctx.ident, "khr_display", 11) == 0);

   RARCH_LOG("[CRT] Video context is: %s.\n", gfxctx.ident);

   if (!p_switch->active)
   {
      video_modeline_gen_t *gen = modeline_gen_new();
      if (!gen)
         return false;
      p_switch->gen = gen;

      /* switchres.ini from the working directory search paths */
      gen->has_ini = modeline_ini_load(gen, "switchres.ini");

      crt_apply_menu_preset(p_switch, crt_mode, super_width);

      /* The screen the display server binds: KMS has no list to
       * pick from, elsewhere the monitor index or "auto" */
      if (p_switch->kms_ctx)
         strlcpy(gen->disp.screen, "dummy", sizeof(gen->disp.screen));
      else if (monitor_index + 1 > 0)
      {
         RARCH_LOG("[CRT] Monitor index manual: %s.\n", &index[0]);
         strlcpy(gen->disp.screen, index, sizeof(gen->disp.screen));
      }
      else
      {
         RARCH_LOG("[CRT] Monitor index auto: %s.\n", "auto");
         strlcpy(gen->disp.screen, "auto", sizeof(gen->disp.screen));
      }

      /* Display-specific ini, then the display server */
      modeline_ini_load(gen, "display0.ini");
      modeline_parse_options(gen);

      memset(&p_switch->ops, 0, sizeof(p_switch->ops));
      p_switch->ops_valid = false;
      if (!p_switch->khr_ctx && video_display_server_get_modeline_ops(&p_switch->ops))
      {
         if (p_switch->ops.open && !p_switch->ops.open(p_switch->ops.data, &gen->disp))
         {
            RARCH_ERR("[CRT] Display server could not open the modeline path, generating only.\n");
            memset(&p_switch->ops, 0, sizeof(p_switch->ops));
         }
         else
            p_switch->ops_valid = true;
      }
      p_switch->ops.name = p_switch->ops_valid ? video_display_server_get_ident() : "dummy";

      p_switch->rtn = modeline_list_init(gen, &p_switch->ops) ? 0 : -1;
      RARCH_LOG("[CRT] Engine rtn %d.\n", p_switch->rtn);

      if (p_switch->rtn >= 0)
      {
         core_name[0]   = '\0';
         content_dir[0] = '\0';
         /* For Lakka, check a switchres.ini next to user's retroarch.cfg */
         crt_load_config_ini(p_switch);
         crt_apply_server_policy(p_switch);
      }
   }

   if (p_switch->rtn >= 0)
   {
      p_switch->active = true;
      if (p_switch->kms_ctx)
         RARCH_LOG("[CRT] KMS context detected, keeping the engine alive.\n");
      else if (p_switch->khr_ctx)
         RARCH_LOG("[CRT] Vulkan context detected, keeping the engine alive.\n");
      return true;
   }

   RARCH_ERR("[CRT] Error at init, CRT modeswitching disabled.\n");
   crt_destroy_modes(p_switch);

   return false;
}

static void switch_res_crt(
      videocrt_switch_t *p_switch,
      unsigned width, unsigned height,
      unsigned crt_mode, unsigned native_width,
      int monitor_index, int super_width)
{
   int w                   = native_width;
   int h                   = height;

   /* Check if the engine is loaded, if not, load it */
   if (crt_engine_init(p_switch, monitor_index, crt_mode, super_width))
   {
      video_modeline_t *mode;
      video_modeline_gen_t *gen = p_switch->gen;
      int flags               = 0;
      char current_core_name[NAME_MAX_LENGTH];
      char current_content_dir[DIR_MAX_LENGTH];
      double rr               = p_switch->ra_core_hz;
      const char *_core_name  = (const char*)runloop_state_get_ptr()->system.info.library_name;

      if (p_switch->rotated)
         flags |= MODELINE_REQ_ROTATED;

      /* Check for core and content changes in case we need
         to make any adjustments */
      if (!_core_name || !*_core_name)
         current_core_name[0] = '\0';
      else
         strlcpy(current_core_name, _core_name, sizeof(current_core_name));

      fill_pathname_parent_dir_name(current_content_dir,
            path_get(RARCH_PATH_CONTENT),
            sizeof(current_content_dir));

      if (     !string_is_equal(core_name,   current_core_name)
            || !string_is_equal(content_dir, current_content_dir)
            || !string_is_equal(current_content_name ,content_name))
      {
         /* A core or content change was detected,
            we update the current values and make adjustments */
         strlcpy(core_name,   current_core_name,   sizeof(core_name));
         strlcpy(content_dir, current_content_dir, sizeof(content_dir));
         strlcpy(content_name, current_content_name, sizeof(current_content_name));
         RARCH_LOG("[CRT] Current running core: %s.\n", core_name);
         crt_adjust_ini(p_switch);
         p_switch->hh_core = false;
      }

#if defined(_WIN32)
      /* ADL takes porch edits only through a real mode set, so a
       * throwaway mode goes first whenever a geometry slider moved */
      if (p_switch->center_adjust  != p_switch->tmp_center_adjust ||
         p_switch->vert_adjust   != p_switch->tmp_vert_adjust)
      {
         int temph = 640;
         int tempw = 480;

         if (w > 320 || h > 240)
         {
            temph = 240;
            tempw = 320;
            RARCH_LOG("[CRT] Temporary mode for windows geometry adjustment (320x240).\n");
         }
         else
            RARCH_LOG("[CRT] Temporary mode for windows geometry adjustment (640x400).\n");

         mode = modeline_get(gen, &p_switch->ops, tempw, temph, rr, flags);
         if (!mode)
            RARCH_ERR("[CRT] Failed to add temporary mode for windows geometry adjustment.\n");
         else
         {
            modeline_flush(gen, &p_switch->ops);
            modeline_set(gen, &p_switch->ops, mode);
            RARCH_LOG("[CRT] Added temporary mode for windows geometry adjustment.\n");
         }
      }
#endif

      /* Geometry sliders straight onto the generator policy */
      gen->h_size  = 1 + ((float)p_switch->porch_adjust / 100.0);
      gen->h_shift = p_switch->center_adjust;
      gen->v_shift = p_switch->vert_adjust;

      RARCH_DBG("[CRT] %dx%d rotation: %d rotated: %d core rotation:%d\n", w, h, p_switch->rotated, flags & MODELINE_REQ_ROTATED, retroarch_get_rotation());
      mode = modeline_get(gen, &p_switch->ops, w, h, rr, flags);
      if (!mode)
      {
         RARCH_ERR("[CRT] Engine failed to add mode.\n");
         crt_switch_set_aspect(p_switch,
               p_switch->rotated ? h : w,
               p_switch->rotated ? w : h,
               0, 0, 1.0f, 1.0f, false);
         return;
      }
      modeline_flush(gen, &p_switch->ops);

      if (p_switch->khr_ctx)
         RARCH_WARN("[CRT] Vulkan -> Can't modeswitch for now.\n");
      else if (!modeline_set(gen, &p_switch->ops, mode))
         RARCH_ERR("[CRT] Engine failed to switch mode.\n");

      crt_publish_timing(p_switch, mode->vfreq);

      crt_switch_set_aspect(p_switch,
            p_switch->rotated ? h : w,
            p_switch->rotated ? w : h,
            mode->hactive, mode->vactive,
            (float)mode->result.x_scale,
            (float)mode->result.y_scale,
            (mode->result.weight & MODELINE_R_RES_STRETCH) ? true : false);
   }
   else
   {
      crt_switch_set_aspect(p_switch,
            width, height,
            width, height,
            1.0f,
            1.0f,
            false);
      video_driver_set_output_size(width , height);
      command_event(CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, NULL);
   }
}
#endif

#if !defined(HAVE_VIDEOCORE)
bool crt_switch_write_edid(char *s, size_t len)
{
   uint8_t block[MODELINE_EDID_SIZE];
   char dir[DIR_MAX_LENGTH];
   video_output_info_t outputs[4];
   int nout;
   settings_t *settings      = config_get_ptr();
   video_modeline_gen_t *gen = modeline_gen_new();
   videocrt_switch_t tmp;
   bool ok;

   if (!gen)
      return false;

   /* The same ini and preset order the switching path uses */
   memset(&tmp, 0, sizeof(tmp));
   tmp.gen = gen;
   modeline_ini_load(gen, "switchres.ini");
   crt_apply_menu_preset(&tmp, settings->uints.crt_switch_resolution,
         settings->uints.crt_switch_resolution_super);
   modeline_ini_load(gen, "display0.ini");
   modeline_parse_options(gen);
   crt_load_config_ini(&tmp);

   ok = modeline_edid_for_gen(gen, block);
   if (ok)
   {
      fill_pathname_application_data(dir, sizeof(dir));
      fill_pathname_join(s, dir, "edid", len);
      path_mkdir(s);
      fill_pathname_join(dir, s, gen->monitor, sizeof(dir));
      strlcpy(s, dir, len);
      strlcat(s, ".bin", len);
      ok = filestream_write_file(s, block, MODELINE_EDID_SIZE);
   }

   if (ok)
   {
      RARCH_LOG("[CRT] EDID for preset %s (%u-%u kHz, %u-%u Hz) written to \"%s\".\n",
            gen->monitor, block[97], block[98], block[95], block[96], s);
      nout = video_display_server_list_outputs(outputs, 4);
      if (nout > 0 && outputs[0].name[0])
         RARCH_LOG("[CRT] Linux: copy it to /lib/firmware/edid/ and boot with drm.edid_firmware=%s:edid/%s.bin\n",
               outputs[0].name, gen->monitor);
      else
         RARCH_LOG("[CRT] Linux: copy it to /lib/firmware/edid/ and boot with drm.edid_firmware=<connector>:edid/%s.bin\n",
               gen->monitor);
      RARCH_LOG("[CRT] Windows: load it as an EDID override for the CRT's monitor entry (CRU or a monitor INF), then restart the display driver.\n");
   }
   else
      RARCH_ERR("[CRT] Could not write an EDID for preset %s.\n", gen->monitor);

   modeline_gen_free(gen);
   return ok;
}
#else
bool crt_switch_write_edid(char *s, size_t len)
{
   (void)s; (void)len;
   return false;
}
#endif

void crt_destroy_modes(videocrt_switch_t *p_switch)
{
   p_switch->active = false;
   if (p_switch->gen)
   {
      /* Added modes go, rewritten ones return, then the server
       * puts the desktop back */
      if (!p_switch->gen->disp.keep_changes)
         modeline_restore(p_switch->gen, &p_switch->ops);
      if (p_switch->ops_valid && p_switch->ops.close)
         p_switch->ops.close(p_switch->ops.data);
      modeline_gen_free(p_switch->gen);
      p_switch->gen = NULL;
   }
   memset(&p_switch->ops, 0, sizeof(p_switch->ops));
   p_switch->ops_valid = false;
}

void crt_switch_res_core(
      videocrt_switch_t *p_switch,
      unsigned native_width, unsigned width, unsigned height,
      float hz, bool rotated, unsigned crt_mode,
      int crt_switch_center_adjust,
      int crt_switch_porch_adjust,
      int monitor_index, bool dynamic,
      int super_width, bool hires_menu,
      unsigned video_aspect_ratio_idx,
      int crt_switch_vert_adjust)
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
      p_switch->vert_adjust           = crt_switch_vert_adjust;
      p_switch->ra_core_height        = height;
      p_switch->ra_core_hz            = hz;

      p_switch->ra_core_width         = width;

      p_switch->center_adjust         = crt_switch_center_adjust;
      p_switch->index                 = monitor_index;
      p_switch->rotated               = rotated;

      /* Detect resolution change and switch */
      if (crt_check_for_changes(p_switch))
      {
         RARCH_LOG("[CRT] Requested resolution: %dx%d@%f, orientation: %s.\n",
                  native_width, height, hz, rotated? "rotated" : "normal");
#if defined(HAVE_VIDEOCORE)
         crt_rpi_switch(p_switch, width, height, hz, 0, native_width);
         video_monitor_set_refresh_rate(p_switch->sr_core_hz);
#else
         if (p_switch->hh_core)
         {
            int corrected_width  = 320;
            int corrected_height = 240;
            switch_res_crt(p_switch, corrected_width, corrected_height,
                  crt_mode, corrected_width, monitor_index-1, super_width);
            crt_switch_set_aspect(p_switch, native_width, height, native_width,
                  height ,(float)1,(float)1, false);
            video_driver_set_output_size(native_width , height);
         }
         else
            switch_res_crt(p_switch, p_switch->ra_core_width,
                  p_switch->ra_core_height, crt_mode,
                  native_width, monitor_index-1, super_width);
#endif
         crt_store_temp_changes(p_switch);
      }

      if (  (video_aspect_ratio_idx == ASPECT_RATIO_CORE)
         &&  video_driver_get_aspect_ratio() != p_switch->fly_aspect)
      {
         video_driver_state_t *video_st = video_state_get_ptr();
         float fly_aspect               = (float)p_switch->fly_aspect;
         RARCH_LOG("[CRT] Restoring aspect ratio: %f.\n", fly_aspect);
         video_st->aspect_ratio         = fly_aspect;
         command_event(CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, NULL);
      }
   }
}

static char *get_game_name(char *full_path)
{
   unsigned i;
   size_t _len        = strlen(full_path);
   char* rom_filename = full_path + _len;
   char delim         = (char)  path_get(RARCH_PATH_BASENAME)[0];

   for (i = 0; i < _len; i++)
   {
      if (full_path[i] == '/' || full_path[i] =='\\')
      {
         delim = full_path[i];
         break;
      }
   }

   while (0 < _len && (full_path[--_len] != delim));
   if (full_path[_len] == delim)
      rom_filename = full_path + _len + 1;
   return rom_filename;
}

#if !defined(HAVE_VIDEOCORE)
static void crt_load_overlay(videocrt_switch_t *p_switch,
      const char *config_directory, const char *name, const char *what)
{
   char override_file[PATH_MAX_LENGTH];

   fill_pathname_join_special_ext(override_file,
         config_directory, core_name, name,
         ".switchres.ini", sizeof(override_file));

   if (!path_is_valid(override_file))
      return;

   RARCH_LOG("[CRT] Loading switchres.ini %s override file from \"%s\".\n",
         what, override_file);
   modeline_ini_load(p_switch->gen, override_file);
   modeline_parse_options(p_switch->gen);
   ini_overrides_loaded = true;
}
#endif

static void crt_adjust_ini(videocrt_switch_t *p_switch)
{
   char* rom_filename = get_game_name((char*) path_get(RARCH_PATH_BASENAME));

   strlcpy(content_name, rom_filename, sizeof(current_content_name));

   RARCH_LOG("[CRT] Game info \"%s\".\n", rom_filename);

#if !defined(HAVE_VIDEOCORE)
   if (!p_switch->active || !p_switch->gen)
      return;

   /* Overrides from another core go first: back to the base ini
    * set, in the same order it was loaded at init */
   if (ini_overrides_loaded)
   {
      settings_t *settings = config_get_ptr();
      RARCH_LOG("[CRT] Loading default switchres.ini...\n");
      modeline_ini_load(p_switch->gen, "switchres.ini");
      crt_apply_menu_preset(p_switch, settings->uints.crt_switch_resolution,
            settings->uints.crt_switch_resolution_super);
      modeline_ini_load(p_switch->gen, "display0.ini");
      modeline_parse_options(p_switch->gen);
      crt_load_config_ini(p_switch);
      crt_apply_server_policy(p_switch);
      ini_overrides_loaded = false;
   }

   if (core_name[0] != '\0')
   {
      char config_directory[DIR_MAX_LENGTH];
      /* config/Core Name/Core Name.switchres.ini, then the content
       * directory, then the game */
      config_directory[0] = '\0';
      fill_pathname_application_special(config_directory,
            sizeof(config_directory),
            APPLICATION_SPECIAL_DIRECTORY_CONFIG);

      crt_load_overlay(p_switch, config_directory, core_name, "core");
      crt_load_overlay(p_switch, config_directory, content_dir, "content directory");
      crt_load_overlay(p_switch, config_directory, content_name, "game");
      crt_apply_server_policy(p_switch);
   }
#endif
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
