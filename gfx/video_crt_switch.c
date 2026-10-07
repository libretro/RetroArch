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
static char *get_game_name(char *full_path);

/* Global local variables */
static bool ini_overrides_loaded = false;
static char core_name[NAME_MAX_LENGTH]; /* Same size as library_name on retroarch_data.h */
static char content_dir[DIR_MAX_LENGTH];
static char current_content_name[256];
static char content_name[256];

static bool crt_check_for_changes(videocrt_switch_t *p_switch)
{
   if (   (p_switch->ra_core_dims != p_switch->ra_tmp_dims)
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
   p_switch->ra_tmp_dims       = p_switch->ra_core_dims;
   p_switch->tmp_center_adjust = p_switch->center_adjust;
   p_switch->tmp_porch_adjust  = p_switch->porch_adjust;
   p_switch->ra_tmp_core_hz    = p_switch->ra_core_hz;
   p_switch->tmp_rotated       = p_switch->rotated;
   p_switch->tmp_vert_adjust   = p_switch->vert_adjust;
}

static void crt_aspect_ratio_switch(
      videocrt_switch_t *p_switch,
      unsigned dims, unsigned video_aspect_ratio_idx)
{
   float fly_aspect               = (float)VIDEO_SCALE_W(dims)
                                  / (float)VIDEO_SCALE_H(dims);
   video_driver_state_t *video_st = video_state_get_ptr();
   p_switch->fly_aspect           = fly_aspect;

   /* We only force aspect ratio for the core provided setting */
   if (video_aspect_ratio_idx != ASPECT_RATIO_CORE)
   {
      RARCH_LOG("[CRT] Aspect ratio forced by user: %f.\n", VIDEO_DRIVER_ASPECT_RATIO(video_st));
      return;
   }

   /* Send aspect float to video_driver */
   video_driver_aspect_ratio_put(&video_st->aspect_ratio_bits, fly_aspect);
   RARCH_LOG("[CRT] Setting aspect ratio: %f.\n", fly_aspect);
   RARCH_LOG("[CRT] Setting screen size: %ux%u.\n",
         VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
   video_driver_set_output_dims(dims);
   if (video_st->current_video && video_st->current_video->set_viewport)
      video_st->current_video->set_viewport(
            video_st->data, dims, true, true);

   command_event(CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, NULL);
}

static void crt_switch_set_aspect(
      videocrt_switch_t *p_switch,
      unsigned dims, unsigned srm_width,
      float srm_xscale, float srm_yscale,
      bool srm_isstretched )
{
   unsigned patched_dims       = dims;
   int scaled_width            = 0;
   int scaled_height           = 0;

   /* used to fix aspect should the engine not find a resolution */
   if (srm_width == 0)
   {
      patched_dims             = video_driver_get_output_dims();
      srm_xscale               = 1;
      srm_yscale               = 1;
   }
   /* Otherwise the native size, multiplied by the mode scale below. */

   if (p_switch->gen)
   {
      if ((int)srm_width >= p_switch->gen->super_width && !srm_isstretched)
         RARCH_LOG("[CRT] Super resolution detected. Fractal scaling @ X:%f Y:%f.\n", srm_xscale, srm_yscale);
      else if (srm_isstretched && srm_width > 0 )
         RARCH_LOG("[CRT] Resolution is stretched. Fractal scaling @ X:%f Y:%f.\n", srm_xscale, srm_yscale);
   }

   scaled_width  = (int)floor(VIDEO_SCALE_W(patched_dims) * srm_xscale + 0.5f);
   scaled_height = (int)floor(VIDEO_SCALE_H(patched_dims) * srm_yscale + 0.5f);

   crt_aspect_ratio_switch(p_switch,
         VIDEO_SCALE_PACK(scaled_width, scaled_height),
         config_get_ptr()->uints.video_aspect_ratio_idx);
}

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
      case 5:
         /* The range limits the display reports; the block itself is
          * read once the display server is bound */
         modeline_set_monitor(p_switch->gen, "edid");
         RARCH_LOG("[CRT] CRT mode: %d - edid.\n", crt_mode);
         break;
      case 6:
         /* The panel's own line count is kept and only the rate
          * moves; the band it may move within is seeded from the
          * EDID once the display server is bound */
         modeline_set_monitor(p_switch->gen, "lcd");
         RARCH_LOG("[CRT] CRT mode: %d - lcd.\n", crt_mode);
         break;
      default:
         break;
   }

   if (super_width > 2)
   {
      modeline_set_user_mode(p_switch->gen, VIDEO_SCALE_PACK(super_width, 0), 0);
      p_switch->gen->super_width = super_width;
   }
}

/* With the SDL display server set to Always the user has chosen
 * listed-mode switching over the native server, and SDL can neither
 * add nor rewrite a timing: the listed modes are all there is, so the
 * engine's default lock on modes without a known timing would leave
 * it nothing but the desktop. That choice unlocks them; every other
 * path keeps the ini-controlled default, which protects a 15 kHz CRT
 * from a stock driver's VESA timings.
 *
 * The VideoCore firmware takes its timing as hdmi_timings, which has
 * no doublescan, and drives an HDMI link whose pixel clock cannot go
 * as low as a native 15 kHz width needs; the mode is widened to a
 * 1920 super resolution unless the user picked a width of their own,
 * as that path always did. */
static void crt_apply_server_policy(videocrt_switch_t *p_switch)
{
   settings_t *settings = config_get_ptr();
   if (!p_switch->gen)
      return;
   if (string_is_equal(p_switch->ops.name, "videocore"))
   {
      p_switch->gen->doublescan = 0;
      if (!VIDEO_SCALE_W(p_switch->gen->user_mode.dims))
      {
         RARCH_LOG("[CRT] VideoCore: 1920 super resolution.\n");
         modeline_set_user_mode(p_switch->gen, VIDEO_SCALE_PACK(1920, 0), 0);
         p_switch->gen->super_width = 1920;
      }
   }
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

/* Point the ops table at the display server instance that is up
 * right now and open its modeline path. The instance is torn down
 * and rebuilt under a full video init (content load and close go
 * through the main deinit), so this runs at engine init and again
 * after crt_switch_display_server_lost() dropped the old table. */
static void crt_bind_display_server(videocrt_switch_t *p_switch)
{
   video_modeline_gen_t *gen = p_switch->gen;

   memset(&p_switch->ops, 0, sizeof(p_switch->ops));
   p_switch->ops_valid = false;
   p_switch->ops_lost  = false;
   if (p_switch->khr_ctx)
      RARCH_WARN("[CRT] Vulkan direct-to-display cannot modeswitch; modes are generated but not applied.\n");
   else if (video_display_server_get_modeline_ops(&p_switch->ops))
   {
      if (p_switch->ops.open && !p_switch->ops.open(p_switch->ops.data, &gen->disp))
      {
         RARCH_ERR("[CRT] Display server could not open the modeline path, generating only.\n");
         memset(&p_switch->ops, 0, sizeof(p_switch->ops));
      }
      else
         p_switch->ops_valid = true;
   }
   else
      RARCH_WARN("[CRT] Display server \"%s\" has no modeline path; modes are generated but not applied.\n",
            video_display_server_get_ident());
   p_switch->ops.name = p_switch->ops_valid ? video_display_server_get_ident() : "dummy";
}

void crt_switch_display_server_lost(videocrt_switch_t *p_switch, void *data)
{
   if (!p_switch->gen || !p_switch->ops_valid || p_switch->ops.data != data)
      return;

   /* The server closes its modeline path as it goes down, which
    * puts the desktop timing back on the wire, and the ops table
    * points into memory that is about to be freed. Drop the table,
    * forget what was current, and make the next frame's request
    * look new so the mode is applied again through the rebound
    * server. */
   memset(&p_switch->ops, 0, sizeof(p_switch->ops));
   p_switch->ops.name       = "dummy";
   p_switch->ops_valid      = false;
   p_switch->ops_lost       = true;
   p_switch->gen->current   = NULL;
   p_switch->ra_tmp_dims    = 0;
   p_switch->ra_tmp_core_hz = 0.0f;
   RARCH_LOG("[CRT] Display server going down, rebinding on the next switch.\n");
}

static bool crt_engine_init(videocrt_switch_t *p_switch,
      int monitor_index, unsigned int crt_mode, unsigned int super_width)
{
   char index[10];
   gfx_ctx_ident_t gfxctx;
   bool starting = !p_switch->active;

   if (monitor_index+1 >= 0 && monitor_index+1 < 10)
      snprintf(index, sizeof(index), "%d", monitor_index);
   else
      strlcpy_lit(index, "0", sizeof(index));

   video_context_driver_get_ident(&gfxctx);

   p_switch->kms_ctx = (gfxctx.ident && strncmp(gfxctx.ident, "kms", 3) == 0);
   p_switch->khr_ctx = (gfxctx.ident && strncmp(gfxctx.ident, "khr_display", 11) == 0);

   if (starting)
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

      crt_bind_display_server(p_switch);

      /* The display's EDID, now that a server is up to read it; the
       * "edid" preset (menu mode or an ini's monitor line) takes its
       * ranges from it */
      {
         int n = video_display_server_get_edid(gen->edid, sizeof(gen->edid));
         gen->edid_len = n > 0 ? (size_t)n : 0;
         if (!strcmp(gen->monitor, "edid"))
         {
            if (gen->edid_len)
            {
               int i;
               double hmax = 0.0;
               int want    = gen->super_width;
               int fit;
               modeline_set_monitor(gen, "edid");
               /* The super resolution the block's maximum pixel clock
                * can carry at the top of the display's horizontal
                * band; a wider one the user chose is stepped down
                * rather than handed to the display as a mode it will
                * reject */
               for (i = 0; i < MODELINE_MAX_RANGES; i++)
                  if (gen->range[i].hfreq_max > hmax)
                     hmax = gen->range[i].hfreq_max;
               fit = modeline_edid_super_width(gen->edid, gen->edid_len, hmax, want);
               if (want > 2 && fit != want)
               {
                  RARCH_LOG("[CRT] Super width %d exceeds the display's stated pixel clock at %.1f kHz; using %d.\n",
                        want, hmax / 1000.0, fit);
                  modeline_set_user_mode(gen, VIDEO_SCALE_PACK(fit, 0), 0);
                  gen->super_width = fit;
               }
            }
            else
               RARCH_WARN("[CRT] The display server could not read the display's EDID; the edid preset falls back to generic_15.\n");
         }
         else if (crt_mode == CRT_SWITCH_LCD
               && !strcmp(gen->lcd_range, "auto"))
         {
            /* Without a band the lcd preset takes the desktop rate
             * plus or minus one, which switches nothing. The band the
             * display states is the one it will accept. */
            video_edid_info_t *info = gen->edid_len
               ? (video_edid_info_t*)calloc(1, sizeof(*info)) : NULL;
            bool seeded = false;
            if (info)
            {
               if (     modeline_edid_parse(gen->edid, gen->edid_len, info)
                     && info->has_range
                     && info->vfreq_max > info->vfreq_min)
               {
                  snprintf(gen->lcd_range, sizeof(gen->lcd_range), "%u-%u",
                        info->vfreq_min, info->vfreq_max);
                  /* The preset filled its range from the old band
                   * when the monitor was set; it has to be filled
                   * again from this one */
                  modeline_set_monitor(gen, "lcd");
                  RARCH_LOG("[CRT] Refresh band %s Hz, from the display's EDID.\n",
                        gen->lcd_range);
                  seeded = true;
               }
               free(info);
            }
            if (!seeded)
               RARCH_WARN("[CRT] The display states no refresh band; the lcd preset holds the desktop rate. Set lcd_range in switchres.ini to widen it.\n");
         }
      }

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

   else if (p_switch->ops_lost && p_switch->rtn >= 0)
   {
      /* Engine alive, display server rebuilt underneath it */
      crt_bind_display_server(p_switch);
      if (p_switch->ops_valid)
         RARCH_LOG("[CRT] Rebound to display server \"%s\".\n", p_switch->ops.name);
   }

   if (p_switch->rtn >= 0)
   {
      p_switch->active = true;
      if (!starting)
         return true;
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
      unsigned dims, unsigned crt_mode, unsigned native_width,
      int monitor_index, int super_width)
{
   int w                   = native_width;
   int h                   = VIDEO_SCALE_H(dims);

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

      /* The name crt_adjust_ini() records, so a content that has not
       * changed is not taken for a new one on every mode change */
      strlcpy(current_content_name,
            get_game_name((char*)path_get(RARCH_PATH_BASENAME)),
            sizeof(current_content_name));

      if (     !string_is_equal(core_name,   current_core_name)
            || !string_is_equal(content_dir, current_content_dir)
            || !string_is_equal(current_content_name ,content_name))
      {
         /* A core or content change was detected,
            we update the current values and make adjustments */
         strlcpy(core_name,   current_core_name,   sizeof(core_name));
         strlcpy(content_dir, current_content_dir, sizeof(content_dir));
         strlcpy(content_name, current_content_name, sizeof(content_name));
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

         mode = modeline_get(gen, &p_switch->ops, VIDEO_SCALE_PACK(tempw, temph), rr, flags);
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

      /* Geometry onto the generator policy: a value the
       * core/directory/game .switchres.ini set holds until its own
       * slider is moved, after which the slider wins. Written on
       * every switch because the generator clamps them in place. */
      if (p_switch->porch_adjust != p_switch->tmp_porch_adjust)
         p_switch->ini_geom &= ~CRT_INI_GEOM_H_SIZE;
      if (p_switch->center_adjust != p_switch->tmp_center_adjust)
         p_switch->ini_geom &= ~CRT_INI_GEOM_H_SHIFT;
      if (p_switch->vert_adjust != p_switch->tmp_vert_adjust)
         p_switch->ini_geom &= ~CRT_INI_GEOM_V_SHIFT;

      gen->h_size  = (p_switch->ini_geom & CRT_INI_GEOM_H_SIZE)
                   ? p_switch->ini_h_size
                   : 1 + ((float)p_switch->porch_adjust / 100.0);
      gen->h_shift = (p_switch->ini_geom & CRT_INI_GEOM_H_SHIFT)
                   ? p_switch->ini_h_shift
                   : p_switch->center_adjust;
      gen->v_shift = (p_switch->ini_geom & CRT_INI_GEOM_V_SHIFT)
                   ? p_switch->ini_v_shift
                   : p_switch->vert_adjust;

      RARCH_DBG("[CRT] %dx%d rotation: %d rotated: %d core rotation:%d\n", w, h, p_switch->rotated, flags & MODELINE_REQ_ROTATED, retroarch_get_rotation());
      mode = modeline_get(gen, &p_switch->ops, VIDEO_SCALE_PACK(w, h), rr, flags);
      if (!mode)
      {
         RARCH_ERR("[CRT] Engine failed to add mode.\n");
         crt_switch_set_aspect(p_switch,
               p_switch->rotated ? VIDEO_SCALE_PACK(h, w)
                                 : VIDEO_SCALE_PACK(w, h),
               0, 1.0f, 1.0f, false);
         return;
      }
      modeline_flush(gen, &p_switch->ops);

      if (p_switch->ops_valid && !modeline_set(gen, &p_switch->ops, mode))
         RARCH_ERR("[CRT] Engine failed to switch mode.\n");

      crt_publish_timing(p_switch, mode->vfreq);

      crt_switch_set_aspect(p_switch,
            p_switch->rotated ? VIDEO_SCALE_PACK(h, w)
                              : VIDEO_SCALE_PACK(w, h),
            mode->hactive,
            (float)mode->result.x_scale,
            (float)mode->result.y_scale,
            (mode->result.weight & MODELINE_R_RES_STRETCH) ? true : false);
   }
   else
   {
      crt_switch_set_aspect(p_switch, dims, VIDEO_SCALE_W(dims),
            1.0f, 1.0f, false);
      video_driver_set_output_dims(dims);
      command_event(CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, NULL);
   }
}

bool crt_switch_write_edid(char *s, size_t len)
{
   uint8_t block[MODELINE_EDID_SIZE];
   char dir[DIR_MAX_LENGTH];
   video_output_info_t outputs[8];
   const char *conn          = NULL;
   unsigned idx;
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
      /* The head the monitor index lands on, named the way the
       * setting names it; "auto" is only unambiguous on one head */
      idx  = settings->uints.video_monitor_index;
      nout = video_display_server_list_outputs(outputs,
            (int)(sizeof(outputs) / sizeof(outputs[0])));
      if (nout > 0)
      {
         if (idx >= 1 && (int)idx <= nout && outputs[idx - 1].name[0])
            conn = outputs[idx - 1].name;
         else if (!idx && nout == 1 && outputs[0].name[0])
            conn = outputs[0].name;
      }
      if (!conn)
         conn = "<connector>";

      RARCH_LOG("[CRT] Linux: copy it to /lib/firmware/edid/ and boot with drm.edid_firmware=%s:edid/%s.bin\n",
            conn, gen->monitor);
      RARCH_LOG("[CRT] Linux: to try it without rebooting, write it as root to /sys/kernel/debug/dri/<card>/%s/edid_override, then 1 to trigger_hotplug beside it; not every driver has trigger_hotplug.\n",
            conn);
      RARCH_LOG("[CRT] Windows: load it as an EDID override for the CRT's monitor entry (CRU or a monitor INF), then restart the display driver.\n");
   }
   else
      RARCH_ERR("[CRT] Could not write an EDID for preset %s.\n", gen->monitor);

   modeline_gen_free(gen);
   return ok;
}

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
   p_switch->ops_lost  = false;
}

void crt_switch_res_core(
      videocrt_switch_t *p_switch,
      unsigned native_width, unsigned dims,
      float hz, bool rotated, unsigned crt_mode,
      int crt_switch_center_adjust,
      int crt_switch_porch_adjust,
      int monitor_index, bool dynamic,
      int super_width, bool hires_menu,
      unsigned video_aspect_ratio_idx,
      int crt_switch_vert_adjust)
{
   unsigned height    = VIDEO_SCALE_H(dims);
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
      dims            = VIDEO_SCALE_PACK(native_width, height);
   }

   if (height != 4 )
   {
      p_switch->menu_active           = false;
      p_switch->porch_adjust          = crt_switch_porch_adjust;
      p_switch->vert_adjust           = crt_switch_vert_adjust;
      p_switch->ra_core_dims          = dims;
      p_switch->ra_core_hz            = hz;

      p_switch->center_adjust         = crt_switch_center_adjust;
      p_switch->index                 = monitor_index;
      p_switch->rotated               = rotated;

      /* Detect resolution change and switch */
      if (crt_check_for_changes(p_switch))
      {
         RARCH_LOG("[CRT] Requested resolution: %dx%d@%f, orientation: %s.\n",
                  native_width, height, hz, rotated? "rotated" : "normal");
         if (p_switch->hh_core)
         {
            int corrected_width  = 320;
            int corrected_height = 240;
            switch_res_crt(p_switch,
                  VIDEO_SCALE_PACK(corrected_width, corrected_height),
                  crt_mode, corrected_width, monitor_index-1, super_width);
            crt_switch_set_aspect(p_switch,
                  VIDEO_SCALE_PACK(native_width, height), native_width,
                  1.0f, 1.0f, false);
            video_driver_set_output_dims(VIDEO_SCALE_PACK(native_width, height));
         }
         else
            switch_res_crt(p_switch, p_switch->ra_core_dims, crt_mode,
                  native_width, monitor_index-1, super_width);
         crt_store_temp_changes(p_switch);
      }

      if (  (video_aspect_ratio_idx == ASPECT_RATIO_CORE)
         &&  video_driver_get_aspect_ratio() != p_switch->fly_aspect)
      {
         video_driver_state_t *video_st = video_state_get_ptr();
         float fly_aspect               = (float)p_switch->fly_aspect;
         RARCH_LOG("[CRT] Restoring aspect ratio: %f.\n", fly_aspect);
         video_driver_aspect_ratio_put(&video_st->aspect_ratio_bits, fly_aspect);
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

static void crt_adjust_ini(videocrt_switch_t *p_switch)
{
   char* rom_filename = get_game_name((char*) path_get(RARCH_PATH_BASENAME));

   strlcpy(content_name, rom_filename, sizeof(current_content_name));

   RARCH_LOG("[CRT] Game info \"%s\".\n", rom_filename);

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

   /* The RetroArch geometry sliders are the base the override files
    * refine, so they go onto the generator first; whatever the files
    * change is recorded and outlives the per-switch rewrite in
    * switch_res_crt() until that slider is moved. The slider state
    * is taken as seen, so a value carried in from the config does
    * not count as a move on the first switch. */
   p_switch->ini_geom          = 0;
   p_switch->tmp_porch_adjust  = p_switch->porch_adjust;
   p_switch->tmp_center_adjust = p_switch->center_adjust;
   p_switch->tmp_vert_adjust   = p_switch->vert_adjust;
   p_switch->gen->h_size       = 1 + ((float)p_switch->porch_adjust / 100.0);
   p_switch->gen->h_shift      = p_switch->center_adjust;
   p_switch->gen->v_shift      = p_switch->vert_adjust;

   if (core_name[0] != '\0')
   {
      video_modeline_gen_t *gen = p_switch->gen;
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

      if (gen->h_size != 1 + ((float)p_switch->porch_adjust / 100.0))
      {
         p_switch->ini_h_size  = gen->h_size;
         p_switch->ini_geom   |= CRT_INI_GEOM_H_SIZE;
      }
      if (gen->h_shift != p_switch->center_adjust)
      {
         p_switch->ini_h_shift = gen->h_shift;
         p_switch->ini_geom   |= CRT_INI_GEOM_H_SHIFT;
      }
      if (gen->v_shift != p_switch->vert_adjust)
      {
         p_switch->ini_v_shift = gen->v_shift;
         p_switch->ini_geom   |= CRT_INI_GEOM_V_SHIFT;
      }
      if (p_switch->ini_geom)
         RARCH_LOG("[CRT] Geometry from switchres.ini overrides: h_size %.3f h_shift %d v_shift %d.\n",
               gen->h_size, gen->h_shift, gen->v_shift);
   }
}
