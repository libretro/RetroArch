/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <file/config_file.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <ctype.h>
#include "config.def.h"
#include <file/file_path.h>
#include "input/input_common.h"
#include "input/input_keymaps.h"
#include "input/input_remapping.h"
#include "configuration.h"
#include "general.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CONFIG_GET_BOOL_BASE(conf, base, var, key) do { \
   bool tmp = false; \
   if (config_get_bool(conf, key, &tmp)) \
      base->var = tmp; \
} while(0)

#define CONFIG_GET_INT_BASE(conf, base, var, key) do { \
   int tmp = 0; \
   if (config_get_int(conf, key, &tmp)) \
      base->var = tmp; \
} while(0)

#define CONFIG_GET_UINT64_BASE(conf, base, var, key) do { \
   uint64_t tmp = 0; \
   if (config_get_int(conf, key, &tmp)) \
      base->var = tmp; \
} while(0)

#define CONFIG_GET_HEX_BASE(conf, base, var, key) do { \
   unsigned tmp = 0; \
   if (config_get_hex(conf, key, &tmp)) \
      base->var = tmp; \
} while(0)

#define CONFIG_GET_FLOAT_BASE(conf, base, var, key) do { \
   float tmp = 0.0f; \
   if (config_get_float(conf, key, &tmp)) \
      base->var = tmp; \
} while(0)

#define CONFIG_GET_STRING_BASE(conf, base, var, key) \
   config_get_array(conf, key, base->var, sizeof(base->var))

#define CONFIG_GET_PATH_BASE(conf, base, var, key) \
   config_get_path(conf, key, base->var, sizeof(base->var))

#define CONFIG_GET_BOOL(var, key) CONFIG_GET_BOOL_BASE(conf, settings, var, key)
#define CONFIG_GET_INT(var, key) CONFIG_GET_INT_BASE(conf, settings, var, key)
#define CONFIG_GET_FLOAT(var, key) CONFIG_GET_FLOAT_BASE(conf, settings, var, key)
#define CONFIG_GET_STRING(var, key) CONFIG_GET_STRING_BASE(conf, settings, var, key)
#define CONFIG_GET_PATH(var, key) CONFIG_GET_PATH_BASE(conf, settings, var, key)

#define CONFIG_GET_BOOL_EXTERN(var, key) CONFIG_GET_BOOL_BASE(conf, global, var, key)
#define CONFIG_GET_INT_EXTERN(var, key) CONFIG_GET_INT_BASE(conf, global, var, key)
#define CONFIG_GET_FLOAT_EXTERN(var, key) CONFIG_GET_FLOAT_BASE(conf, global, var, key)
#define CONFIG_GET_STRING_EXTERN(var, key) CONFIG_GET_STRING_BASE(conf, global, var, key)
#define CONFIG_GET_PATH_EXTERN(var, key) CONFIG_GET_PATH_BASE(conf, global, var, key)

static settings_t *g_config;
struct defaults g_defaults;

/**
 * config_get_default_audio:
 *
 * Gets default audio driver.
 *
 * Returns: Default audio driver.
 **/
const char *config_get_default_audio(void)
{
   switch (AUDIO_DEFAULT_DRIVER)
   {
      case AUDIO_RSOUND:
         return "rsound";
      case AUDIO_OSS:
         return "oss";
      case AUDIO_ALSA:
         return "alsa";
      case AUDIO_ALSATHREAD:
         return "alsathread";
      case AUDIO_ROAR:
         return "roar";
      case AUDIO_COREAUDIO:
         return "coreaudio";
      case AUDIO_AL:
         return "openal";
      case AUDIO_SL:
         return "opensl";
      case AUDIO_SDL:
         return "sdl";
      case AUDIO_SDL2:
         return "sdl2";
      case AUDIO_DSOUND:
         return "dsound";
      case AUDIO_XAUDIO:
         return "xaudio";
      case AUDIO_PULSE:
         return "pulse";
      case AUDIO_EXT:
         return "ext";
      case AUDIO_XENON360:
         return "xenon360";
      case AUDIO_PS3:
         return "ps3";
      case AUDIO_WII:
         return "gx";
      case AUDIO_PSP1:
         return "psp1";
      case AUDIO_RWEBAUDIO:
         return "rwebaudio";
      default:
         break;
   }

   return "null";
}

/**
 * config_get_default_audio_resampler:
 *
 * Gets default audio resampler driver.
 *
 * Returns: Default audio resampler driver.
 **/
const char *config_get_default_audio_resampler(void)
{
   switch (AUDIO_DEFAULT_RESAMPLER_DRIVER)
   {
      case AUDIO_RESAMPLER_CC:
         return "cc";
      case AUDIO_RESAMPLER_SINC:
         return "sinc";
      case AUDIO_RESAMPLER_NEAREST:
         return "nearest";
      default:
         break;
   }

   return "null";
}

/**
 * config_get_default_video:
 *
 * Gets default video driver.
 *
 * Returns: Default video driver.
 **/
const char *config_get_default_video(void)
{
   switch (VIDEO_DEFAULT_DRIVER)
   {
      case VIDEO_GL:
         return "gl";
      case VIDEO_WII:
         return "gx";
      case VIDEO_XENON360:
         return "xenon360";
      case VIDEO_XDK_D3D:
      case VIDEO_D3D9:
         return "d3d";
      case VIDEO_PSP1:
         return "psp1";
      case VIDEO_VITA:
         return "vita";
      case VIDEO_XVIDEO:
         return "xvideo";
      case VIDEO_SDL:
         return "sdl";
      case VIDEO_SDL2:
         return "sdl2";
      case VIDEO_EXT:
         return "ext";
      case VIDEO_VG:
         return "vg";
      case VIDEO_OMAP:
         return "omap";
      case VIDEO_EXYNOS:
         return "exynos";
      case VIDEO_DISPMANX:
         return "dispmanx";
      case VIDEO_SUNXI:
         return "sunxi";
      default:
         break;
   }

   return "null";
}

/**
 * config_get_default_input:
 *
 * Gets default input driver.
 *
 * Returns: Default input driver.
 **/
const char *config_get_default_input(void)
{
   switch (INPUT_DEFAULT_DRIVER)
   {
      case INPUT_ANDROID:
         return "android_input";
      case INPUT_PS3:
         return "ps3";
      case INPUT_PSP:
         return "psp";
      case INPUT_SDL:
         return "sdl";
      case INPUT_SDL2:
         return "sdl2";
      case INPUT_DINPUT:
         return "dinput";
      case INPUT_X:
         return "x";
      case INPUT_WAYLAND:
         return "wayland";
      case INPUT_XENON360:
         return "xenon360";
      case INPUT_XINPUT:
         return "xinput";
      case INPUT_WII:
         return "gx";
      case INPUT_LINUXRAW:
         return "linuxraw";
      case INPUT_UDEV:
         return "udev";
      case INPUT_APPLE:
         return "apple_input";
      case INPUT_QNX:
      	 return "qnx_input";
      case INPUT_RWEBINPUT:
      	 return "rwebinput";
      default:
         break;
   }

   return "null";
}

/**
 * config_get_default_joypad:
 *
 * Gets default input joypad driver.
 *
 * Returns: Default input joypad driver.
 **/
const char *config_get_default_joypad(void)
{
   switch (JOYPAD_DEFAULT_DRIVER)
   {
      case JOYPAD_PS3:
         return "ps3";
      case JOYPAD_WINXINPUT:
         return "winxinput";
      case JOYPAD_GX:
         return "gx";
      case JOYPAD_XDK:
         return "xdk";
      case JOYPAD_PSP:
         return "psp";
      case JOYPAD_DINPUT:
         return "dinput";
      case JOYPAD_UDEV:
         return "udev";
      case JOYPAD_LINUXRAW:
         return "linuxraw";
      case JOYPAD_ANDROID:
         return "android";
      case JOYPAD_SDL:
#ifdef HAVE_SDL2
         return "sdl2";
#else
         return "sdl";
#endif
      case JOYPAD_APPLE_HID:
         return "apple_hid";
      case JOYPAD_APPLE_IOS:
         return "apple_ios";
      case JOYPAD_QNX:
         return "qnx";
      default:
         break;
   }

   return "null";
}

#ifdef HAVE_MENU
/**
 * config_get_default_menu:
 *
 * Gets default menu driver.
 *
 * Returns: Default menu driver.
 **/
const char *config_get_default_menu(void)
{
   switch (MENU_DEFAULT_DRIVER)
   {
      case MENU_RGUI:
         return "rgui";
      case MENU_RMENU:
         return "rmenu";
      case MENU_RMENU_XUI:
         return "rmenu_xui";
      case MENU_GLUI:
         return "glui";
      case MENU_IOS:
         return "ios";
      case MENU_XMB:
         return "xmb";
      default:
         break;
   }

   return "null";
}
#endif

/**
 * config_get_default_camera:
 *
 * Gets default camera driver.
 *
 * Returns: Default camera driver.
 **/
const char *config_get_default_camera(void)
{
   switch (CAMERA_DEFAULT_DRIVER)
   {
      case CAMERA_V4L2:
         return "video4linux2";
      case CAMERA_RWEBCAM:
         return "rwebcam";
      case CAMERA_ANDROID:
         return "android";
      case CAMERA_APPLE:
         return "apple";
      default:
         break;
   }

   return "null";
}

/**
 * config_get_default_location:
 *
 * Gets default location driver.
 *
 * Returns: Default location driver.
 **/
const char *config_get_default_location(void)
{
   switch (LOCATION_DEFAULT_DRIVER)
   {
      case LOCATION_ANDROID:
         return "android";
      case LOCATION_APPLE:
         return "apple";
      default:
         break;
   }

   return "null";
}

/**
 * config_set_defaults:
 *
 * Set 'default' configuration values.
 **/
static void config_set_defaults(void)
{
   unsigned i, j;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();
   const char *def_video = config_get_default_video();
   const char *def_audio = config_get_default_audio();
   const char *def_audio_resampler = config_get_default_audio_resampler();
   const char *def_input = config_get_default_input();
   const char *def_joypad = config_get_default_joypad();
#ifdef HAVE_MENU
   const char *def_menu  = config_get_default_menu();
#endif
   const char *def_camera = config_get_default_camera();
   const char *def_location = config_get_default_location();

   if (def_camera)
      strlcpy(settings->camera.driver,
            def_camera, sizeof(settings->camera.driver));
   if (def_location)
      strlcpy(settings->location.driver,
            def_location, sizeof(settings->location.driver));
   if (def_video)
      strlcpy(settings->video.driver,
            def_video, sizeof(settings->video.driver));
   if (def_audio)
      strlcpy(settings->audio.driver,
            def_audio, sizeof(settings->audio.driver));
   if (def_audio_resampler)
      strlcpy(settings->audio.resampler,
            def_audio_resampler, sizeof(settings->audio.resampler));
   if (def_input)
      strlcpy(settings->input.driver,
            def_input, sizeof(settings->input.driver));
   if (def_joypad)
      strlcpy(settings->input.joypad_driver,
            def_input, sizeof(settings->input.joypad_driver));
#ifdef HAVE_MENU
   if (def_menu)
      strlcpy(settings->menu.driver,
            def_menu,  sizeof(settings->menu.driver));
#endif

   settings->history_list_enable = def_history_list_enable;
   settings->load_dummy_on_core_shutdown = load_dummy_on_core_shutdown;

   settings->video.scale                 = scale;
   settings->video.fullscreen            = global->force_fullscreen ? true : fullscreen;
   settings->video.windowed_fullscreen   = windowed_fullscreen;
   settings->video.monitor_index         = monitor_index;
   settings->video.fullscreen_x          = fullscreen_x;
   settings->video.fullscreen_y          = fullscreen_y;
   settings->video.disable_composition   = disable_composition;
   settings->video.vsync                 = vsync;
   settings->video.hard_sync             = hard_sync;
   settings->video.hard_sync_frames      = hard_sync_frames;
   settings->video.frame_delay           = frame_delay;
   settings->video.black_frame_insertion = black_frame_insertion;
   settings->video.swap_interval         = swap_interval;
   settings->video.threaded              = video_threaded;

   if (g_defaults.settings.video_threaded_enable != video_threaded)
      settings->video.threaded = g_defaults.settings.video_threaded_enable;

#ifdef HAVE_THREADS
   settings->menu.threaded_data_runloop_enable = true;
#endif
   settings->video.shared_context = video_shared_context;
   settings->video.force_srgb_disable = false;
#ifdef GEKKO
   settings->video.viwidth = video_viwidth;
   settings->video.vfilter = video_vfilter;
#endif
   settings->video.smooth = video_smooth;
   settings->video.force_aspect = force_aspect;
   settings->video.scale_integer = scale_integer;
   settings->video.crop_overscan = crop_overscan;
   settings->video.aspect_ratio = aspect_ratio;
   settings->video.aspect_ratio_auto = aspect_ratio_auto; /* Let implementation decide if automatic, or 1:1 PAR. */
   settings->video.aspect_ratio_idx = aspect_ratio_idx;
   settings->video.shader_enable = shader_enable;
   settings->video.allow_rotate = allow_rotate;

   settings->video.font_enable = font_enable;
   settings->video.font_size = font_size;
   settings->video.msg_pos_x = message_pos_offset_x;
   settings->video.msg_pos_y = message_pos_offset_y;
   
   settings->video.msg_color_r = ((message_color >> 16) & 0xff) / 255.0f;
   settings->video.msg_color_g = ((message_color >>  8) & 0xff) / 255.0f;
   settings->video.msg_color_b = ((message_color >>  0) & 0xff) / 255.0f;

   settings->video.refresh_rate = refresh_rate;

   if (g_defaults.settings.video_refresh_rate > 0.0 &&
         g_defaults.settings.video_refresh_rate != refresh_rate)
      settings->video.refresh_rate = g_defaults.settings.video_refresh_rate;

   settings->video.post_filter_record = post_filter_record;
   settings->video.gpu_record = gpu_record;
   settings->video.gpu_screenshot = gpu_screenshot;
   settings->video.rotation = ORIENTATION_NORMAL;

   settings->audio.enable = audio_enable;
   settings->audio.mute_enable = false;
   settings->audio.out_rate = out_rate;
   settings->audio.block_frames = 0;
   if (audio_device)
      strlcpy(settings->audio.device,
            audio_device, sizeof(settings->audio.device));

   if (!g_defaults.settings.out_latency)
      g_defaults.settings.out_latency = out_latency;


   settings->audio.latency = g_defaults.settings.out_latency;
   settings->audio.sync = audio_sync;
   settings->audio.rate_control = rate_control;
   settings->audio.rate_control_delta = rate_control_delta;
   settings->audio.max_timing_skew = max_timing_skew;
   settings->audio.volume = audio_volume;
   global->audio_data.volume_gain = db_to_gain(settings->audio.volume);

   settings->rewind_enable = rewind_enable;
   settings->rewind_buffer_size = rewind_buffer_size;
   settings->rewind_granularity = rewind_granularity;
   settings->slowmotion_ratio = slowmotion_ratio;
   settings->fastforward_ratio = fastforward_ratio;
   settings->fastforward_ratio_throttle_enable = fastforward_ratio_throttle_enable;
   settings->pause_nonactive = pause_nonactive;
   settings->autosave_interval = autosave_interval;

   settings->block_sram_overwrite = block_sram_overwrite;
   settings->savestate_auto_index = savestate_auto_index;
   settings->savestate_auto_save  = savestate_auto_save;
   settings->savestate_auto_load  = savestate_auto_load;
   settings->network_cmd_enable   = network_cmd_enable;
   settings->network_cmd_port     = network_cmd_port;
   settings->stdin_cmd_enable     = stdin_cmd_enable;
   settings->content_history_size    = default_content_history_size;
   settings->libretro_log_level   = libretro_log_level;

#ifdef HAVE_MENU
   settings->menu_show_start_screen = menu_show_start_screen;
   settings->menu.pause_libretro = true;
   settings->menu.mouse.enable = false;
   settings->menu.timedate_enable = true;
   settings->menu.core_enable = true;
   *settings->menu.wallpaper = '\0';
   settings->menu.navigation.wraparound.horizontal_enable = true;
   settings->menu.navigation.wraparound.vertical_enable = true;
   settings->menu.navigation.browser.filter.supported_extensions_enable = true;
   settings->menu.collapse_subgroups_enable = collapse_subgroups_enable;
   settings->menu.show_advanced_settings = show_advanced_settings;
   settings->menu.entry_normal_color = menu_entry_normal_color;
   settings->menu.entry_hover_color  = menu_entry_hover_color;
   settings->menu.title_color        = menu_title_color;
#endif

   settings->ui.menubar_enable = true;
   settings->ui.suspend_screensaver_enable = true;

   settings->location.allow = false;
   settings->camera.allow = false;

   settings->input.autoconfig_descriptor_label_show = true;
   settings->input.input_descriptor_label_show = input_descriptor_label_show;
   settings->input.input_descriptor_hide_unbound = input_descriptor_hide_unbound;
   settings->input.remap_binds_enable = true;
   settings->input.max_users = MAX_USERS;

   rarch_assert(sizeof(settings->input.binds[0]) >= sizeof(retro_keybinds_1));
   rarch_assert(sizeof(settings->input.binds[1]) >= sizeof(retro_keybinds_rest));

   memcpy(settings->input.binds[0], retro_keybinds_1, sizeof(retro_keybinds_1));

   for (i = 1; i < MAX_USERS; i++)
      memcpy(settings->input.binds[i], retro_keybinds_rest,
            sizeof(retro_keybinds_rest));

   input_remapping_set_defaults();

   for (i = 0; i < MAX_USERS; i++)
   {
      for (j = 0; j < RARCH_BIND_LIST_END; j++)
      {
         settings->input.autoconf_binds[i][j].joykey = NO_BTN;
         settings->input.autoconf_binds[i][j].joyaxis = AXIS_NONE;
      }
   }
   memset(settings->input.autoconfigured, 0,
         sizeof(settings->input.autoconfigured));

   /* Verify that binds are in proper order. */
   for (i = 0; i < MAX_USERS; i++)
      for (j = 0; j < RARCH_BIND_LIST_END; j++)
      {
         if (settings->input.binds[i][j].valid)
            rarch_assert(j == settings->input.binds[i][j].id);
      }

   settings->input.axis_threshold = axis_threshold;
   settings->input.netplay_client_swap_input = netplay_client_swap_input;
   settings->input.turbo_period = turbo_period;
   settings->input.turbo_duty_cycle = turbo_duty_cycle;

   strlcpy(settings->network.buildbot_url, buildbot_server_url,
         sizeof(settings->network.buildbot_url));
   strlcpy(settings->network.buildbot_assets_url, buildbot_assets_server_url,
         sizeof(settings->network.buildbot_assets_url));
   settings->network.buildbot_auto_extract_archive = true;

   settings->input.overlay_enable = true;
   settings->input.overlay_opacity = 0.7f;
   settings->input.overlay_scale = 1.0f;
   settings->input.autodetect_enable = input_autodetect_enable;
   *settings->input.keyboard_layout = '\0';

   for (i = 0; i < MAX_USERS; i++)
   {
      settings->input.joypad_map[i] = i;
      settings->input.analog_dpad_mode[i] = ANALOG_DPAD_NONE;
      if (!global->has_set_libretro_device[i])
         settings->input.libretro_device[i] = RETRO_DEVICE_JOYPAD;
   }

   global->console.screen.viewports.custom_vp.width = 0;
   global->console.screen.viewports.custom_vp.height = 0;
   global->console.screen.viewports.custom_vp.x = 0;
   global->console.screen.viewports.custom_vp.y = 0;

   /* Make sure settings from other configs carry over into defaults 
    * for another config. */
   if (!global->has_set_save_path)
      *global->savefile_dir = '\0';
   if (!global->has_set_state_path)
      *global->savestate_dir = '\0';

   *settings->libretro_info_path = '\0';
   if (!global->has_set_libretro_directory)
      *settings->libretro_directory = '\0';

   if (!global->has_set_ups_pref)
      global->ups_pref = false;
   if (!global->has_set_bps_pref)
      global->bps_pref = false;
   if (!global->has_set_ips_pref)
      global->ips_pref = false;

   *settings->core_options_path = '\0';
   *settings->content_history_path = '\0';
   *settings->content_history_directory = '\0';
   *settings->content_database = '\0';
   *settings->cheat_database = '\0';
   *settings->cursor_directory = '\0';
   *settings->cheat_settings_path = '\0';
   *settings->resampler_directory = '\0';
   *settings->screenshot_directory = '\0';
   *settings->system_directory = '\0';
   *settings->extraction_directory = '\0';
   *settings->input_remapping_directory = '\0';
   *settings->input.autoconfig_dir = '\0';
   *settings->input.overlay = '\0';
   *settings->core_assets_directory = '\0';
   *settings->assets_directory = '\0';
   *settings->playlist_directory = '\0';
   *settings->video.shader_path = '\0';
   *settings->video.shader_dir = '\0';
   *settings->video.filter_dir = '\0';
   *settings->audio.filter_dir = '\0';
   *settings->video.softfilter_plugin = '\0';
   *settings->audio.dsp_plugin = '\0';
#ifdef HAVE_MENU
   *settings->menu_content_directory = '\0';
   *settings->menu_config_directory = '\0';
#endif
   settings->core_specific_config = default_core_specific_config;
   settings->user_language = 0;

   global->console.sound.system_bgm_enable = false;
#ifdef RARCH_CONSOLE
   global->console.screen.gamma_correction = DEFAULT_GAMMA;
   global->console.screen.resolutions.current.id = 0;
   global->console.sound.mode = SOUND_MODE_NORMAL;
#endif
   
   if (*g_defaults.extraction_dir)
      strlcpy(settings->extraction_directory,
            g_defaults.extraction_dir, sizeof(settings->extraction_directory));
   if (*g_defaults.audio_filter_dir)
      strlcpy(settings->audio.filter_dir,
            g_defaults.audio_filter_dir, sizeof(settings->audio.filter_dir));
   if (*g_defaults.video_filter_dir)
      strlcpy(settings->video.filter_dir,
            g_defaults.video_filter_dir, sizeof(settings->video.filter_dir));
   if (*g_defaults.assets_dir)
      strlcpy(settings->assets_directory,
            g_defaults.assets_dir, sizeof(settings->assets_directory));
   if (*g_defaults.playlist_dir)
      strlcpy(settings->playlist_directory,
            g_defaults.playlist_dir, sizeof(settings->playlist_directory));
   if (*g_defaults.core_dir)
      fill_pathname_expand_special(settings->libretro_directory,
            g_defaults.core_dir, sizeof(settings->libretro_directory));
   if (*g_defaults.core_path)
      strlcpy(settings->libretro, g_defaults.core_path,
            sizeof(settings->libretro));
   if (*g_defaults.database_dir)
      strlcpy(settings->content_database, g_defaults.database_dir,
            sizeof(settings->content_database));
   if (*g_defaults.cursor_dir)
      strlcpy(settings->cursor_directory, g_defaults.cursor_dir,
            sizeof(settings->cursor_directory));
   if (*g_defaults.cheats_dir)
      strlcpy(settings->cheat_database, g_defaults.cheats_dir,
            sizeof(settings->cheat_database));
   if (*g_defaults.core_info_dir)
      fill_pathname_expand_special(settings->libretro_info_path,
            g_defaults.core_info_dir, sizeof(settings->libretro_info_path));
#ifdef HAVE_OVERLAY
   if (*g_defaults.overlay_dir)
   {
      fill_pathname_expand_special(global->overlay_dir,
            g_defaults.overlay_dir, sizeof(global->overlay_dir));
#ifdef RARCH_MOBILE
      if (!*settings->input.overlay)
            fill_pathname_join(settings->input.overlay,
                  global->overlay_dir,
                  "gamepads/retropad/retropad.cfg",
                  sizeof(settings->input.overlay));
#endif
   }

   if (*g_defaults.osk_overlay_dir)
   {
      fill_pathname_expand_special(global->osk_overlay_dir,
            g_defaults.osk_overlay_dir, sizeof(global->osk_overlay_dir));
#ifdef RARCH_MOBILE
      if (!*settings->input.overlay)
            fill_pathname_join(settings->osk.overlay,
                  global->osk_overlay_dir,
                  "overlays/keyboards/US-101/US-101.cfg",
                  sizeof(settings->osk.overlay));
#endif
   }
   else
      strlcpy(global->osk_overlay_dir,
            global->overlay_dir, sizeof(global->osk_overlay_dir));
#endif
#ifdef HAVE_MENU
   if (*g_defaults.menu_config_dir)
      strlcpy(settings->menu_config_directory,
            g_defaults.menu_config_dir,
            sizeof(settings->menu_config_directory));
#endif
   if (*g_defaults.shader_dir)
      fill_pathname_expand_special(settings->video.shader_dir,
            g_defaults.shader_dir, sizeof(settings->video.shader_dir));
   if (*g_defaults.autoconfig_dir)
      strlcpy(settings->input.autoconfig_dir,
            g_defaults.autoconfig_dir,
            sizeof(settings->input.autoconfig_dir));

   if (!global->has_set_state_path && *g_defaults.savestate_dir)
      strlcpy(global->savestate_dir,
            g_defaults.savestate_dir, sizeof(global->savestate_dir));
   if (!global->has_set_save_path && *g_defaults.sram_dir)
      strlcpy(global->savefile_dir,
            g_defaults.sram_dir, sizeof(global->savefile_dir));
   if (*g_defaults.system_dir)
      strlcpy(settings->system_directory,
            g_defaults.system_dir, sizeof(settings->system_directory));
   if (*g_defaults.screenshot_dir)
      strlcpy(settings->screenshot_directory,
            g_defaults.screenshot_dir,
            sizeof(settings->screenshot_directory));
   if (*g_defaults.resampler_dir)
      strlcpy(settings->resampler_directory,
            g_defaults.resampler_dir,
            sizeof(settings->resampler_directory));
   if (*g_defaults.content_history_dir)
      strlcpy(settings->content_history_directory,
            g_defaults.content_history_dir,
            sizeof(settings->content_history_directory));

   if (*g_defaults.config_path)
      fill_pathname_expand_special(global->config_path,
            g_defaults.config_path, sizeof(global->config_path));
   
   settings->config_save_on_exit = config_save_on_exit;

   /* Avoid reloading config on every content load */
   global->block_config_read = default_block_config_read;
}

#ifndef GLOBAL_CONFIG_DIR
#if defined(__HAIKU__)
#define GLOBAL_CONFIG_DIR "/system/settings"
#else
#define GLOBAL_CONFIG_DIR "/etc"
#endif
#endif

/**
 * open_default_config_file
 *
 * Open a default config file. Platform-specific.
 *
 * Returns: handle to config file if found, otherwise NULL.
 **/
static config_file_t *open_default_config_file(void)
{
   char conf_path[PATH_MAX_LENGTH], app_path[PATH_MAX_LENGTH];
   config_file_t *conf = NULL;
   bool saved = false;
   global_t *global = global_get_ptr();

   (void)conf_path;
   (void)app_path;
   (void)saved;

#if defined(_WIN32) && !defined(_XBOX)
   fill_pathname_application_path(app_path, sizeof(app_path));
   fill_pathname_resolve_relative(conf_path, app_path,
         "retroarch.cfg", sizeof(conf_path));

   conf = config_file_new(conf_path);

   if (!conf)
   {
      const char *appdata = getenv("APPDATA");

      if (appdata)
      {
         fill_pathname_join(conf_path, appdata,
               "retroarch.cfg", sizeof(conf_path));
         conf = config_file_new(conf_path);
      }
   }

   if (!conf)
   {
      /* Try to create a new config file. */
      conf = config_file_new(NULL);

      if (conf)
      {
         /* Since this is a clean config file, we can 
          * safely use config_save_on_exit. */
         fill_pathname_resolve_relative(conf_path, app_path,
               "retroarch.cfg", sizeof(conf_path));
         config_set_bool(conf, "config_save_on_exit", true);
         saved = config_file_write(conf, conf_path);
      }

      if (!saved)
      {
         /* WARN here to make sure user has a good chance of seeing it. */
         RARCH_ERR("Failed to create new config file in: \"%s\".\n",
               conf_path);
         config_file_free(conf);
         return NULL;
      }

      RARCH_WARN("Created new config file in: \"%s\".\n", conf_path);
   }
#elif defined(OSX)
   const char *home = getenv("HOME");

   if (!home)
      return NULL;

   fill_pathname_join(conf_path, home,
         "Library/Application Support/RetroArch", sizeof(conf_path));
   path_mkdir(conf_path);
      
   fill_pathname_join(conf_path, conf_path,
         "retroarch.cfg", sizeof(conf_path));
   conf = config_file_new(conf_path);

   if (!conf)
   {
      conf = config_file_new(NULL);

      if (conf)
      {
         config_set_bool(conf, "config_save_on_exit", true);
         saved = config_file_write(conf, conf_path);
      }

      if (!saved)
      {
         /* WARN here to make sure user has a good chance of seeing it. */
         RARCH_ERR("Failed to create new config file in: \"%s\".\n",
               conf_path);
         config_file_free(conf);

         return NULL;
      }

      RARCH_WARN("Created new config file in: \"%s\".\n", conf_path);
   }
#elif !defined(__CELLOS_LV2__) && !defined(_XBOX)
   const char *xdg  = getenv("XDG_CONFIG_HOME");
   const char *home = getenv("HOME");

   /* XDG_CONFIG_HOME falls back to $HOME/.config. */
   if (xdg)
      fill_pathname_join(conf_path, xdg,
            "retroarch/retroarch.cfg", sizeof(conf_path));
   else if (home)
#ifdef __HAIKU__
      fill_pathname_join(conf_path, home,
            "config/settings/retroarch/retroarch.cfg", sizeof(conf_path));
#else
      fill_pathname_join(conf_path, home,
            ".config/retroarch/retroarch.cfg", sizeof(conf_path));
#endif

   if (xdg || home)
   {
      RARCH_LOG("Looking for config in: \"%s\".\n", conf_path);
      conf = config_file_new(conf_path);
   }

   /* Fallback to $HOME/.retroarch.cfg. */
   if (!conf && home)
   {
      fill_pathname_join(conf_path, home,
            ".retroarch.cfg", sizeof(conf_path));
      RARCH_LOG("Looking for config in: \"%s\".\n", conf_path);
      conf = config_file_new(conf_path);
   }

   if (!conf)
   {
      if (home || xdg)
      {
         /* Try to create a new config file. */

         char basedir[PATH_MAX_LENGTH];

         /* XDG_CONFIG_HOME falls back to $HOME/.config. */
         if (xdg)
            fill_pathname_join(conf_path, xdg,
                  "retroarch/retroarch.cfg", sizeof(conf_path));
         else if (home)
#ifdef __HAIKU__
            fill_pathname_join(conf_path, home,
                  "config/settings/retroarch/retroarch.cfg", sizeof(conf_path));
#else
         fill_pathname_join(conf_path, home,
               ".config/retroarch/retroarch.cfg", sizeof(conf_path));
#endif

         fill_pathname_basedir(basedir, conf_path, sizeof(basedir));

         if (path_mkdir(basedir))
         {
            char skeleton_conf[PATH_MAX_LENGTH];
            fill_pathname_join(skeleton_conf, GLOBAL_CONFIG_DIR,
                  "retroarch.cfg", sizeof(skeleton_conf));
            conf = config_file_new(skeleton_conf);
            if (conf)
               RARCH_WARN("Using skeleton config \"%s\" as base for a new config file.\n", skeleton_conf);
            else
               conf = config_file_new(NULL);

            if (conf)
            {
               /* Since this is a clean config file, we can safely use config_save_on_exit. */
               config_set_bool(conf, "config_save_on_exit", true);
               saved = config_file_write(conf, conf_path);
            }

            if (!saved)
            {
               /* WARN here to make sure user has a good chance of seeing it. */
               RARCH_ERR("Failed to create new config file in: \"%s\".\n", conf_path);
               config_file_free(conf);

               return NULL;
            }

            RARCH_WARN("Created new config file in: \"%s\".\n", conf_path);
         }
      }
   }
#endif

   if (!conf)
      return NULL;

   strlcpy(global->config_path, conf_path,
         sizeof(global->config_path));
   
   return conf;
}

static void read_keybinds_keyboard(config_file_t *conf, unsigned user,
      unsigned idx, struct retro_keybind *bind)
{
   const char *prefix = NULL;

   if (!input_config_bind_map[idx].valid)
      return;

   if (!input_config_bind_map[idx].base)
      return;

   prefix = input_config_get_prefix(user, input_config_bind_map[idx].meta);

   if (prefix)
      input_config_parse_key(conf, prefix,
            input_config_bind_map[idx].base, bind);
}

static void read_keybinds_button(config_file_t *conf, unsigned user,
      unsigned idx, struct retro_keybind *bind)
{
   const char *prefix = NULL;

   if (!input_config_bind_map[idx].valid)
      return;
   if (!input_config_bind_map[idx].base)
      return;

   prefix = input_config_get_prefix(user,
         input_config_bind_map[idx].meta);

   if (prefix)
      input_config_parse_joy_button(conf, prefix,
            input_config_bind_map[idx].base, bind);
}

static void read_keybinds_axis(config_file_t *conf, unsigned user,
      unsigned idx, struct retro_keybind *bind)
{
   const char *prefix = NULL;

   if (!input_config_bind_map[idx].valid)
      return;
   if (!input_config_bind_map[idx].base)
      return;

   prefix = input_config_get_prefix(user,
         input_config_bind_map[idx].meta);

   if (prefix)
      input_config_parse_joy_axis(conf, prefix,
            input_config_bind_map[idx].base, bind);
}

static void read_keybinds_user(config_file_t *conf, unsigned user)
{
   unsigned i;
   settings_t *settings = config_get_ptr();

   for (i = 0; input_config_bind_map[i].valid; i++)
   {
      struct retro_keybind *bind = (struct retro_keybind*)
         &settings->input.binds[user][i];

      if (!bind->valid)
         continue;

      read_keybinds_keyboard(conf, user, i, bind);
      read_keybinds_button(conf, user, i, bind);
      read_keybinds_axis(conf, user, i, bind);
   }
}

static void config_read_keybinds_conf(config_file_t *conf)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
      read_keybinds_user(conf, i);
}

/* Also dumps inherited values, useful for logging. */

static void config_file_dump_all(config_file_t *conf)
{
   struct config_entry_list *list = NULL;
   struct config_include_list *includes = conf->includes;

   while (includes)
   {
      RARCH_LOG("#include \"%s\"\n", includes->path);
      includes = includes->next;
   }

   list = conf->entries;

   while (list)
   {
      RARCH_LOG("%s = \"%s\" %s\n", list->key,
            list->value, list->readonly ? "(included)" : "");
      list = list->next;
   }
}

/**
 * config_load:
 * @path                : path to be read from.
 * @set_defaults        : set default values first before
 *                        reading the values from the config file
 *
 * Loads a config file and reads all the values into memory.
 *
 */
static bool config_load_file(const char *path, bool set_defaults)
{
   unsigned i;
   char *save, tmp_str[PATH_MAX_LENGTH];
   char tmp_append_path[PATH_MAX_LENGTH]; /* Don't destroy append_config_path. */
   const char *extra_path;
   unsigned msg_color = 0;
   config_file_t *conf = NULL;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (path)
   {
      conf = config_file_new(path);
      if (!conf)
         return false;
   }
   else
      conf = open_default_config_file();

   if (!conf)
      return true;

   if (set_defaults)
      config_set_defaults();

   strlcpy(tmp_append_path, global->append_config_path,
         sizeof(tmp_append_path));
   extra_path = strtok_r(tmp_append_path, ",", &save);

   while (extra_path)
   {
      bool ret = false;
      RARCH_LOG("Appending config \"%s\"\n", extra_path);
      ret = config_append_file(conf, extra_path);
      if (!ret)
         RARCH_ERR("Failed to append config \"%s\"\n", extra_path);
      extra_path = strtok_r(NULL, ",", &save);
   }

   if (global->verbosity)
   {
      RARCH_LOG_OUTPUT("=== Config ===\n");
      config_file_dump_all(conf);
      RARCH_LOG_OUTPUT("=== Config end ===\n");
   }


   CONFIG_GET_FLOAT(video.scale, "video_scale");
   CONFIG_GET_INT(video.fullscreen_x, "video_fullscreen_x");
   CONFIG_GET_INT(video.fullscreen_y, "video_fullscreen_y");

   if (!global->force_fullscreen)
      CONFIG_GET_BOOL(video.fullscreen, "video_fullscreen");

   CONFIG_GET_BOOL(video.windowed_fullscreen, "video_windowed_fullscreen");
   CONFIG_GET_INT(video.monitor_index, "video_monitor_index");
   CONFIG_GET_BOOL(video.disable_composition, "video_disable_composition");
   CONFIG_GET_BOOL(video.vsync, "video_vsync");
   CONFIG_GET_BOOL(video.hard_sync, "video_hard_sync");

#ifdef HAVE_MENU
#ifdef HAVE_THREADS
   CONFIG_GET_BOOL(menu.threaded_data_runloop_enable, "threaded_data_runloop_enable");
#endif
   CONFIG_GET_BOOL(menu.pause_libretro, "menu_pause_libretro");
   CONFIG_GET_BOOL(menu.mouse.enable,   "menu_mouse_enable");
   CONFIG_GET_BOOL(menu.timedate_enable,   "menu_timedate_enable");
   CONFIG_GET_BOOL(menu.core_enable,   "menu_core_enable");
   CONFIG_GET_BOOL(menu.navigation.wraparound.horizontal_enable, "menu_navigation_wraparound_horizontal_enable");
   CONFIG_GET_BOOL(menu.navigation.wraparound.vertical_enable,   "menu_navigation_wraparound_vertical_enable");
   CONFIG_GET_BOOL(menu.navigation.browser.filter.supported_extensions_enable,   "menu_navigation_browser_filter_supported_extensions_enable");
   CONFIG_GET_BOOL(menu.collapse_subgroups_enable,   "menu_collapse_subgroups_enable");
   CONFIG_GET_BOOL(menu.show_advanced_settings,   "menu_show_advanced_settings");
   CONFIG_GET_HEX_BASE(conf, settings, menu.entry_normal_color,   "menu_entry_normal_color");
   CONFIG_GET_HEX_BASE(conf, settings, menu.entry_hover_color,   "menu_entry_hover_color");
   CONFIG_GET_HEX_BASE(conf, settings, menu.title_color,   "menu_title_color");
   config_get_path(conf, "menu_wallpaper", settings->menu.wallpaper, sizeof(settings->menu.wallpaper));
   if (!strcmp(settings->menu.wallpaper, "default"))
      *settings->menu.wallpaper = '\0';
#endif

   CONFIG_GET_INT(video.hard_sync_frames, "video_hard_sync_frames");
   if (settings->video.hard_sync_frames > 3)
      settings->video.hard_sync_frames = 3;

   CONFIG_GET_INT(video.frame_delay, "video_frame_delay");
   if (settings->video.frame_delay > 15)
      settings->video.frame_delay = 15;

   CONFIG_GET_BOOL(video.black_frame_insertion, "video_black_frame_insertion");
   CONFIG_GET_INT(video.swap_interval, "video_swap_interval");
   settings->video.swap_interval = max(settings->video.swap_interval, 1);
   settings->video.swap_interval = min(settings->video.swap_interval, 4);
   CONFIG_GET_BOOL(video.threaded, "video_threaded");
   CONFIG_GET_BOOL(video.shared_context, "video_shared_context");
#ifdef GEKKO
   CONFIG_GET_INT(video.viwidth, "video_viwidth");
   CONFIG_GET_BOOL(video.vfilter, "video_vfilter");
#endif
   CONFIG_GET_BOOL(video.smooth, "video_smooth");
   CONFIG_GET_BOOL(video.force_aspect, "video_force_aspect");
   CONFIG_GET_BOOL(video.scale_integer, "video_scale_integer");
   CONFIG_GET_BOOL(video.crop_overscan, "video_crop_overscan");
   CONFIG_GET_FLOAT(video.aspect_ratio, "video_aspect_ratio");
   CONFIG_GET_INT(video.aspect_ratio_idx, "aspect_ratio_index");
   CONFIG_GET_BOOL(video.aspect_ratio_auto, "video_aspect_ratio_auto");
   CONFIG_GET_FLOAT(video.refresh_rate, "video_refresh_rate");

   config_get_path(conf, "video_shader", settings->video.shader_path, sizeof(settings->video.shader_path));
   CONFIG_GET_BOOL(video.shader_enable, "video_shader_enable");

   CONFIG_GET_BOOL(video.allow_rotate, "video_allow_rotate");

   config_get_path(conf, "video_font_path", settings->video.font_path, sizeof(settings->video.font_path));
   CONFIG_GET_FLOAT(video.font_size, "video_font_size");
   CONFIG_GET_BOOL(video.font_enable, "video_font_enable");
   CONFIG_GET_FLOAT(video.msg_pos_x, "video_message_pos_x");
   CONFIG_GET_FLOAT(video.msg_pos_y, "video_message_pos_y");
   CONFIG_GET_INT(video.rotation, "video_rotation");

   CONFIG_GET_BOOL(video.force_srgb_disable, "video_force_srgb_disable");

#ifdef RARCH_CONSOLE
   /* TODO - will be refactored later to make it more clean - it's more 
    * important that it works for consoles right now */

   CONFIG_GET_BOOL_EXTERN(console.screen.gamma_correction, "gamma_correction");

   config_get_bool(conf, "custom_bgm_enable",
         &global->console.sound.system_bgm_enable);
   config_get_bool(conf, "flicker_filter_enable",
         &global->console.flickerfilter_enable);
   config_get_bool(conf, "soft_filter_enable",
         &global->console.softfilter_enable);

   CONFIG_GET_INT_EXTERN(console.screen.flicker_filter_index,
         "flicker_filter_index");
   CONFIG_GET_INT_EXTERN(console.screen.soft_filter_index,
         "soft_filter_index");
   CONFIG_GET_INT_EXTERN(console.screen.resolutions.current.id,
         "current_resolution_id");
   CONFIG_GET_INT_EXTERN(console.sound.mode, "sound_mode");
#endif
   CONFIG_GET_INT(state_slot, "state_slot");

   CONFIG_GET_INT_EXTERN(console.screen.viewports.custom_vp.x,
         "custom_viewport_x");
   CONFIG_GET_INT_EXTERN(console.screen.viewports.custom_vp.y,
         "custom_viewport_y");
   CONFIG_GET_INT_EXTERN(console.screen.viewports.custom_vp.width,
         "custom_viewport_width");
   CONFIG_GET_INT_EXTERN(console.screen.viewports.custom_vp.height,
         "custom_viewport_height");

   if (config_get_hex(conf, "video_message_color", &msg_color))
   {
      settings->video.msg_color_r = ((msg_color >> 16) & 0xff) / 255.0f;
      settings->video.msg_color_g = ((msg_color >>  8) & 0xff) / 255.0f;
      settings->video.msg_color_b = ((msg_color >>  0) & 0xff) / 255.0f;
   }

   CONFIG_GET_BOOL(video.post_filter_record, "video_post_filter_record");
   CONFIG_GET_BOOL(video.gpu_record, "video_gpu_record");
   CONFIG_GET_BOOL(video.gpu_screenshot, "video_gpu_screenshot");

   config_get_path(conf, "video_shader_dir", settings->video.shader_dir, sizeof(settings->video.shader_dir));
   if (!strcmp(settings->video.shader_dir, "default"))
      *settings->video.shader_dir = '\0';

   config_get_path(conf, "video_filter_dir", settings->video.filter_dir, sizeof(settings->video.filter_dir));
   if (!strcmp(settings->video.filter_dir, "default"))
      *settings->video.filter_dir = '\0';

   config_get_path(conf, "audio_filter_dir", settings->audio.filter_dir, sizeof(settings->audio.filter_dir));
   if (!strcmp(settings->audio.filter_dir, "default"))
      *settings->audio.filter_dir = '\0';

   CONFIG_GET_BOOL(input.remap_binds_enable,
         "input_remap_binds_enable");
   CONFIG_GET_FLOAT(input.axis_threshold, "input_axis_threshold");
   CONFIG_GET_BOOL(input.netplay_client_swap_input,
         "netplay_client_swap_input");
   CONFIG_GET_INT(input.max_users, "input_max_users");
   CONFIG_GET_BOOL(input.input_descriptor_label_show,
         "input_descriptor_label_show");
   CONFIG_GET_BOOL(input.input_descriptor_hide_unbound,
         "input_descriptor_hide_unbound");
   CONFIG_GET_BOOL(input.autoconfig_descriptor_label_show,
         "autoconfig_descriptor_label_show");

   config_get_path(conf, "core_updater_buildbot_url",
         settings->network.buildbot_url, sizeof(settings->network.buildbot_url));
   config_get_path(conf, "core_updater_buildbot_assets_url",
         settings->network.buildbot_assets_url, sizeof(settings->network.buildbot_assets_url));
   CONFIG_GET_BOOL(network.buildbot_auto_extract_archive,
         "core_updater_auto_extract_archive");

   for (i = 0; i < MAX_USERS; i++)
   {
      char buf[64];
      snprintf(buf, sizeof(buf), "input_player%u_joypad_index", i + 1);
      CONFIG_GET_INT(input.joypad_map[i], buf);

      snprintf(buf, sizeof(buf), "input_player%u_analog_dpad_mode", i + 1);
      CONFIG_GET_INT(input.analog_dpad_mode[i], buf);

      if (!global->has_set_libretro_device[i])
      {
         snprintf(buf, sizeof(buf), "input_libretro_device_p%u", i + 1);
         CONFIG_GET_INT(input.libretro_device[i], buf);
      }
   }

   if (!global->has_set_ups_pref)
   {
      CONFIG_GET_BOOL_EXTERN(ups_pref, "ups_pref");
   }
   if (!global->has_set_bps_pref)
   {
      CONFIG_GET_BOOL_EXTERN(bps_pref, "bps_pref");
   }
   if (!global->has_set_ips_pref)
   {
      CONFIG_GET_BOOL_EXTERN(ips_pref, "ips_pref");
   }

   /* Audio settings. */
   CONFIG_GET_BOOL(audio.enable, "audio_enable");
   CONFIG_GET_BOOL(audio.mute_enable, "audio_mute_enable");
   CONFIG_GET_INT(audio.out_rate, "audio_out_rate");
   CONFIG_GET_INT(audio.block_frames, "audio_block_frames");
   CONFIG_GET_STRING(audio.device, "audio_device");
   CONFIG_GET_INT(audio.latency, "audio_latency");
   CONFIG_GET_BOOL(audio.sync, "audio_sync");
   CONFIG_GET_BOOL(audio.rate_control, "audio_rate_control");
   CONFIG_GET_FLOAT(audio.rate_control_delta, "audio_rate_control_delta");
   CONFIG_GET_FLOAT(audio.max_timing_skew, "audio_max_timing_skew");
   CONFIG_GET_FLOAT(audio.volume, "audio_volume");
   CONFIG_GET_STRING(audio.resampler, "audio_resampler");
   global->audio_data.volume_gain = db_to_gain(settings->audio.volume);

   CONFIG_GET_STRING(camera.device, "camera_device");
   CONFIG_GET_BOOL(camera.allow, "camera_allow");

   CONFIG_GET_BOOL(location.allow, "location_allow");
   CONFIG_GET_STRING(video.driver, "video_driver");
#ifdef HAVE_MENU
   CONFIG_GET_STRING(menu.driver, "menu_driver");
#endif
   CONFIG_GET_STRING(video.context_driver, "video_context_driver");
   CONFIG_GET_STRING(audio.driver, "audio_driver");
   config_get_path(conf, "video_filter", settings->video.softfilter_plugin, sizeof(settings->video.softfilter_plugin));
   config_get_path(conf, "audio_dsp_plugin", settings->audio.dsp_plugin, sizeof(settings->audio.dsp_plugin));
   CONFIG_GET_STRING(input.driver, "input_driver");
   CONFIG_GET_STRING(input.joypad_driver, "input_joypad_driver");
   CONFIG_GET_STRING(input.keyboard_layout, "input_keyboard_layout");

   if (!global->has_set_libretro)
      config_get_path(conf, "libretro_path", settings->libretro, sizeof(settings->libretro));
   if (!global->has_set_libretro_directory)
      config_get_path(conf, "libretro_directory", settings->libretro_directory, sizeof(settings->libretro_directory));

   /* Safe-guard against older behavior. */
   if (path_is_directory(settings->libretro))
   {
      RARCH_WARN("\"libretro_path\" is a directory, using this for \"libretro_directory\" instead.\n");
      strlcpy(settings->libretro_directory, settings->libretro,
            sizeof(settings->libretro_directory));
      *settings->libretro = '\0';
   }

   CONFIG_GET_BOOL(ui.menubar_enable, "ui_menubar_enable");
   CONFIG_GET_BOOL(ui.suspend_screensaver_enable, "suspend_screensaver_enable");
   CONFIG_GET_BOOL(fps_show, "fps_show");
   CONFIG_GET_BOOL(load_dummy_on_core_shutdown, "load_dummy_on_core_shutdown");

   config_get_path(conf, "libretro_info_path", settings->libretro_info_path, sizeof(settings->libretro_info_path));

   config_get_path(conf, "core_options_path", settings->core_options_path, sizeof(settings->core_options_path));
   config_get_path(conf, "screenshot_directory", settings->screenshot_directory, sizeof(settings->screenshot_directory));
   if (*settings->screenshot_directory)
   {
      if (!strcmp(settings->screenshot_directory, "default"))
         *settings->screenshot_directory = '\0';
      else if (!path_is_directory(settings->screenshot_directory))
      {
         RARCH_WARN("screenshot_directory is not an existing directory, ignoring ...\n");
         *settings->screenshot_directory = '\0';
      }
   }

   config_get_path(conf, "input_remapping_path", settings->input.remapping_path, sizeof(settings->input.remapping_path));

   config_get_path(conf, "resampler_directory", settings->resampler_directory, sizeof(settings->resampler_directory));
   config_get_path(conf, "extraction_directory", settings->extraction_directory, sizeof(settings->extraction_directory));
   config_get_path(conf, "input_remapping_directory", settings->input_remapping_directory, sizeof(settings->input_remapping_directory));
   config_get_path(conf, "core_assets_directory", settings->core_assets_directory, sizeof(settings->core_assets_directory));
   config_get_path(conf, "assets_directory", settings->assets_directory, sizeof(settings->assets_directory));
   config_get_path(conf, "playlist_directory", settings->playlist_directory, sizeof(settings->playlist_directory));
   if (!strcmp(settings->core_assets_directory, "default"))
      *settings->core_assets_directory = '\0';
   if (!strcmp(settings->assets_directory, "default"))
      *settings->assets_directory = '\0';
   if (!strcmp(settings->playlist_directory, "default"))
      *settings->playlist_directory = '\0';
#ifdef HAVE_MENU
   config_get_path(conf, "rgui_browser_directory", settings->menu_content_directory, sizeof(settings->menu_content_directory));
   if (!strcmp(settings->menu_content_directory, "default"))
      *settings->menu_content_directory = '\0';
   config_get_path(conf, "rgui_config_directory", settings->menu_config_directory, sizeof(settings->menu_config_directory));
   if (!strcmp(settings->menu_config_directory, "default"))
      *settings->menu_config_directory = '\0';
   CONFIG_GET_BOOL(menu_show_start_screen, "rgui_show_start_screen");
#endif
   CONFIG_GET_INT(libretro_log_level, "libretro_log_level");

   if (!global->has_set_verbosity)
      CONFIG_GET_BOOL_EXTERN(verbosity, "log_verbosity");

   CONFIG_GET_BOOL_EXTERN(perfcnt_enable, "perfcnt_enable");

   CONFIG_GET_INT(archive.mode, "archive_mode");

#ifdef HAVE_OVERLAY
   config_get_path(conf, "overlay_directory", global->overlay_dir, sizeof(global->overlay_dir));
   if (!strcmp(global->overlay_dir, "default"))
      *global->overlay_dir = '\0';

   config_get_path(conf, "input_overlay", settings->input.overlay, sizeof(settings->input.overlay));
   CONFIG_GET_BOOL(input.overlay_enable, "input_overlay_enable");
   CONFIG_GET_FLOAT(input.overlay_opacity, "input_overlay_opacity");
   CONFIG_GET_FLOAT(input.overlay_scale, "input_overlay_scale");

   config_get_path(conf, "osk_overlay_directory", global->osk_overlay_dir, sizeof(global->osk_overlay_dir));
   if (!strcmp(global->osk_overlay_dir, "default"))
      *global->osk_overlay_dir = '\0';

   config_get_path(conf, "input_osk_overlay", settings->osk.overlay, sizeof(settings->osk.overlay));
   CONFIG_GET_BOOL(osk.enable, "input_osk_overlay_enable");
#endif

   CONFIG_GET_BOOL(rewind_enable, "rewind_enable");

   int buffer_size = 0;
   if (config_get_int(conf, "rewind_buffer_size", &buffer_size))
      settings->rewind_buffer_size = buffer_size * UINT64_C(1000000);

   CONFIG_GET_INT(rewind_granularity, "rewind_granularity");
   CONFIG_GET_FLOAT(slowmotion_ratio, "slowmotion_ratio");
   if (settings->slowmotion_ratio < 1.0f)
      settings->slowmotion_ratio = 1.0f;

   CONFIG_GET_FLOAT(fastforward_ratio, "fastforward_ratio");

   /* Sanitize fastforward_ratio value - previously range was -1
    * and up (with 0 being skipped) */
   if (settings->fastforward_ratio <= 0.0f)
      settings->fastforward_ratio = 1.0f;

   CONFIG_GET_BOOL(fastforward_ratio_throttle_enable, "fastforward_ratio_throttle_enable");

   CONFIG_GET_BOOL(pause_nonactive, "pause_nonactive");
   CONFIG_GET_INT(autosave_interval, "autosave_interval");

   CONFIG_GET_PATH(content_database, "content_database_path");
   CONFIG_GET_PATH(cheat_database, "cheat_database_path");
   CONFIG_GET_PATH(cursor_directory, "cursor_directory");
   CONFIG_GET_PATH(cheat_settings_path, "cheat_settings_path");

   CONFIG_GET_BOOL(block_sram_overwrite, "block_sram_overwrite");
   CONFIG_GET_BOOL(savestate_auto_index, "savestate_auto_index");
   CONFIG_GET_BOOL(savestate_auto_save, "savestate_auto_save");
   CONFIG_GET_BOOL(savestate_auto_load, "savestate_auto_load");

   CONFIG_GET_BOOL(network_cmd_enable, "network_cmd_enable");
   CONFIG_GET_INT(network_cmd_port, "network_cmd_port");
   CONFIG_GET_BOOL(stdin_cmd_enable, "stdin_cmd_enable");

   CONFIG_GET_PATH(content_history_directory, "content_history_dir");

   CONFIG_GET_BOOL(history_list_enable, "history_list_enable");

   CONFIG_GET_PATH(content_history_path, "game_history_path");
   CONFIG_GET_INT(content_history_size, "game_history_size");

   CONFIG_GET_INT(input.turbo_period, "input_turbo_period");
   CONFIG_GET_INT(input.turbo_duty_cycle, "input_duty_cycle");

   CONFIG_GET_BOOL(input.autodetect_enable, "input_autodetect_enable");
   CONFIG_GET_PATH(input.autoconfig_dir, "joypad_autoconfig_dir");

   if (!global->has_set_username)
      CONFIG_GET_PATH(username, "netplay_nickname");
   CONFIG_GET_INT(user_language, "user_language");
#ifdef HAVE_NETPLAY
   if (!global->has_set_netplay_mode)
      CONFIG_GET_BOOL_EXTERN(netplay_is_spectate,
            "netplay_spectator_mode_enable");
   if (!global->has_set_netplay_mode)
      CONFIG_GET_BOOL_EXTERN(netplay_is_client, "netplay_mode");
   if (!global->has_set_netplay_ip_address)
      CONFIG_GET_PATH_EXTERN(netplay_server, "netplay_ip_address");
   if (!global->has_set_netplay_delay_frames)
      CONFIG_GET_INT_EXTERN(netplay_sync_frames, "netplay_delay_frames");
   if (!global->has_set_netplay_ip_port)
      CONFIG_GET_INT_EXTERN(netplay_port, "netplay_ip_port");
#endif

   CONFIG_GET_BOOL(config_save_on_exit, "config_save_on_exit");

   if (!global->has_set_save_path &&
         config_get_path(conf, "savefile_directory", tmp_str, sizeof(tmp_str)))
   {
      if (!strcmp(tmp_str, "default"))
         strlcpy(global->savefile_dir, g_defaults.sram_dir,
               sizeof(global->savefile_dir));
      else if (path_is_directory(tmp_str))
      {
         strlcpy(global->savefile_dir, tmp_str,
               sizeof(global->savefile_dir));
         strlcpy(global->savefile_name, tmp_str,
               sizeof(global->savefile_name));
         fill_pathname_dir(global->savefile_name, global->basename,
               ".srm", sizeof(global->savefile_name));
      }
      else
         RARCH_WARN("savefile_directory is not a directory, ignoring ...\n");
   }

   if (!global->has_set_state_path &&
         config_get_path(conf, "savestate_directory", tmp_str, sizeof(tmp_str)))
   {
      if (!strcmp(tmp_str, "default"))
         strlcpy(global->savestate_dir, g_defaults.savestate_dir,
               sizeof(global->savestate_dir));
      else if (path_is_directory(tmp_str))
      {
         strlcpy(global->savestate_dir, tmp_str,
               sizeof(global->savestate_dir));
         strlcpy(global->savestate_name, tmp_str,
               sizeof(global->savestate_name));
         fill_pathname_dir(global->savestate_name, global->basename,
               ".state", sizeof(global->savestate_name));
      }
      else
         RARCH_WARN("savestate_directory is not a directory, ignoring ...\n");
   }

   if (settings->content_history_path[0] == '\0')
   {
      if (settings->content_history_directory[0] != '\0')
      {
         fill_pathname_join(settings->content_history_path,
               settings->content_history_directory,
               "retroarch-content-history.txt",
               sizeof(settings->content_history_path));
      }
      else
      {
         fill_pathname_resolve_relative(settings->content_history_path,
               global->config_path, "retroarch-content-history.txt",
               sizeof(settings->content_history_path));
      }
   }

   if (!config_get_path(conf, "system_directory",
            settings->system_directory, sizeof(settings->system_directory)))
   {
      RARCH_WARN("system_directory is not set in config. Assuming system directory is same folder as game: \"%s\".\n",
            settings->system_directory);
   }

   if (!strcmp(settings->system_directory, "default"))
      *settings->system_directory = '\0';

   config_read_keybinds_conf(conf);

   CONFIG_GET_BOOL(core_specific_config, "core_specific_config");

   config_file_free(conf);
   return true;
}

static void config_load_core_specific(void)
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   *global->core_specific_config_path = '\0';

   if (!*settings->libretro
#ifdef HAVE_DYNAMIC
      || global->libretro_dummy
#endif
      )
      return;

#ifdef HAVE_MENU
   if (*settings->menu_config_directory)
   {
      path_resolve_realpath(settings->menu_config_directory,
            sizeof(settings->menu_config_directory));
      strlcpy(global->core_specific_config_path,
            settings->menu_config_directory,
            sizeof(global->core_specific_config_path));
   }
   else
#endif
   {
      /* Use original config file's directory as a fallback. */
      fill_pathname_basedir(global->core_specific_config_path,
            global->config_path, sizeof(global->core_specific_config_path));
   }

   fill_pathname_dir(global->core_specific_config_path, settings->libretro,
         ".cfg", sizeof(global->core_specific_config_path));

   if (settings->core_specific_config)
   {
      char tmp[PATH_MAX_LENGTH];
      strlcpy(tmp, settings->libretro, sizeof(tmp));
      RARCH_LOG("Loading core-specific config from: %s.\n",
            global->core_specific_config_path);

      if (!config_load_file(global->core_specific_config_path, true))
         RARCH_WARN("Core-specific config not found, reusing last config.\n");

      /* Force some parameters which are implied when using core specific configs.
       * Don't have the core config file overwrite the libretro path. */
      strlcpy(settings->libretro, tmp, sizeof(settings->libretro));

      /* This must be true for core specific configs. */
      settings->core_specific_config = true;
   }
}

static void parse_config_file(void)
{
   global_t *global = global_get_ptr();
   bool ret = config_load_file((*global->config_path) 
         ? global->config_path : NULL, false);

   if (*global->config_path)
   {
      RARCH_LOG("Loading config from: %s.\n", global->config_path);
   }
   else
   {
      RARCH_LOG("Loading default config.\n");
      if (*global->config_path)
         RARCH_LOG("Found default config: %s.\n", global->config_path);
   }

   if (ret)
      return;

   RARCH_ERR("Couldn't find config at path: \"%s\"\n",
         global->config_path);
}


#if 0
static bool config_read_keybinds(const char *path)
{
   config_file_t *conf = (config_file_t*)config_file_new(path);

   if (!conf)
      return false;

   config_read_keybinds_conf(conf);
   config_file_free(conf);

   return true;
}
#endif

static void save_keybind_key(config_file_t *conf, const char *prefix,
      const char *base, const struct retro_keybind *bind)
{
   char key[64], btn[64];

   snprintf(key, sizeof(key), "%s_%s", prefix, base);
   input_keymaps_translate_rk_to_str(bind->key, btn, sizeof(btn));
   config_set_string(conf, key, btn);
}

static void save_keybind_hat(config_file_t *conf, const char *key,
      const struct retro_keybind *bind)
{
   char config[16];
   unsigned hat = GET_HAT(bind->joykey);
   const char *dir = NULL;

   switch (GET_HAT_DIR(bind->joykey))
   {
      case HAT_UP_MASK:
         dir = "up";
         break;

      case HAT_DOWN_MASK:
         dir = "down";
         break;

      case HAT_LEFT_MASK:
         dir = "left";
         break;

      case HAT_RIGHT_MASK:
         dir = "right";
         break;

      default:
         rarch_assert(0);
   }

   snprintf(config, sizeof(config), "h%u%s", hat, dir);
   config_set_string(conf, key, config);
}

static void save_keybind_joykey(config_file_t *conf, const char *prefix,
      const char *base, const struct retro_keybind *bind)
{
   char key[64];
   snprintf(key, sizeof(key), "%s_%s_btn", prefix, base);

   if (bind->joykey == NO_BTN)
      config_set_string(conf, key, "nul");
   else if (GET_HAT_DIR(bind->joykey))
      save_keybind_hat(conf, key, bind);
   else
      config_set_uint64(conf, key, bind->joykey);
}

static void save_keybind_axis(config_file_t *conf, const char *prefix,
      const char *base, const struct retro_keybind *bind)
{
   char key[64], config[16];
   unsigned axis = 0;
   char dir = '\0';

   snprintf(key, sizeof(key), "%s_%s_axis", prefix, base);

   if (bind->joyaxis == AXIS_NONE)
      config_set_string(conf, key, "nul");
   else if (AXIS_NEG_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '-';
      axis = AXIS_NEG_GET(bind->joyaxis);
   }
   else if (AXIS_POS_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '+';
      axis = AXIS_POS_GET(bind->joyaxis);
   }

   if (dir)
   {
      snprintf(config, sizeof(config), "%c%u", dir, axis);
      config_set_string(conf, key, config);
   }
}

/**
 * save_keybind:
 * @conf               : pointer to config file object
 * @prefix             : prefix name of keybind
 * @base               : base name   of keybind
 * @bind               : pointer to key binding object
 *
 * Save a key binding to the config file.
 */
static void save_keybind(config_file_t *conf, const char *prefix,
      const char *base, const struct retro_keybind *bind)
{
   if (!bind->valid)
      return;

   save_keybind_key(conf, prefix, base, bind);
   save_keybind_joykey(conf, prefix, base, bind);
   save_keybind_axis(conf, prefix, base, bind);
}



/**
 * save_keybinds_user:
 * @conf               : pointer to config file object
 * @user               : user number
 *
 * Save the current keybinds of a user (@user) to the config file (@conf).
 */
static void save_keybinds_user(config_file_t *conf, unsigned user)
{
   unsigned i = 0;
   settings_t *settings = config_get_ptr();

   for (i = 0; input_config_bind_map[i].valid; i++)
   {
      const char *prefix = input_config_get_prefix(user,
            input_config_bind_map[i].meta);

      if (prefix)
         save_keybind(conf, prefix, input_config_bind_map[i].base,
               &settings->input.binds[user][i]);
   }
}

/**
 * config_load:
 *
 * Loads a config file and reads all the values into memory.
 *
 */
void config_load(void)
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   /* Flush out per-core configs before loading a new config. */
   if (*global->core_specific_config_path &&
         settings->config_save_on_exit && settings->core_specific_config)
      config_save_file(global->core_specific_config_path);

   /* Flush out some states that could have been set by core environment variables */
   global->has_set_input_descriptors = false;

   if (!global->block_config_read)
   {
      config_set_defaults();
      parse_config_file();
   }

   /* Per-core config handling. */
   config_load_core_specific();
}

/**
 * config_save_keybinds_file:
 * @path            : Path that shall be written to.
 *
 * Writes a keybinds config file to disk.
 *
 * Returns: true (1) on success, otherwise returns false (0).
 **/
bool config_save_keybinds_file(const char *path)
{
   unsigned i = 0;
   bool ret = false;
   config_file_t *conf = config_file_new(path);

   if (!conf)
      conf = config_file_new(NULL);

   if (!conf)
      return false;

   RARCH_LOG("Saving keybinds config at path: \"%s\"\n", path);

   for (i = 0; i < MAX_USERS; i++)
      save_keybinds_user(conf, i);

   ret = config_file_write(conf, path);
   config_file_free(conf);
   return ret;
}

/**
 * config_save_file:
 * @path            : Path that shall be written to.
 *
 * Writes a config file to disk.
 *
 * Returns: true (1) on success, otherwise returns false (0).
 **/
bool config_save_file(const char *path)
{
   unsigned i = 0;
   bool ret = false;
   config_file_t *conf  = config_file_new(path);
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (!conf)
      conf = config_file_new(NULL);

   if (!conf)
      return false;

   RARCH_LOG("Saving config at path: \"%s\"\n", path);

   config_set_int(conf, "input_max_users", settings->input.max_users);
   config_set_float(conf, "input_axis_threshold",
         settings->input.axis_threshold);
   config_set_bool(conf, "video_gpu_record", settings->video.gpu_record);
   config_set_bool(conf, "input_remap_binds_enable",
         settings->input.remap_binds_enable);
   config_set_bool(conf, "netplay_client_swap_input",
         settings->input.netplay_client_swap_input);
   config_set_bool(conf, "input_descriptor_label_show",
         settings->input.input_descriptor_label_show);
   config_set_bool(conf, "autoconfig_descriptor_label_show",
         settings->input.autoconfig_descriptor_label_show);
   config_set_bool(conf, "input_descriptor_hide_unbound",
         settings->input.input_descriptor_hide_unbound);
   config_set_bool(conf,  "load_dummy_on_core_shutdown",
         settings->load_dummy_on_core_shutdown);
   config_set_bool(conf,  "fps_show", settings->fps_show);
   config_set_bool(conf,  "ui_menubar_enable", settings->ui.menubar_enable);
   config_set_bool(conf,  "suspend_screensaver_enable", settings->ui.suspend_screensaver_enable);
   config_set_path(conf,  "libretro_path", settings->libretro);
   config_set_path(conf,  "libretro_directory", settings->libretro_directory);
   config_set_path(conf,  "libretro_info_path", settings->libretro_info_path);
   config_set_path(conf,  "content_database_path", settings->content_database);
   config_set_path(conf,  "cheat_database_path", settings->cheat_database);
   config_set_path(conf,  "cursor_directory", settings->cursor_directory);
   config_set_path(conf,  "content_history_dir", settings->content_history_directory);
   config_set_bool(conf,  "rewind_enable", settings->rewind_enable);
   config_set_int(conf,   "audio_latency", settings->audio.latency);
   config_set_bool(conf,  "audio_sync",    settings->audio.sync);
   config_set_int(conf,   "audio_block_frames", settings->audio.block_frames);
   config_set_int(conf,   "rewind_granularity", settings->rewind_granularity);
   config_set_path(conf,  "video_shader", settings->video.shader_path);
   config_set_bool(conf,  "video_shader_enable",
         settings->video.shader_enable);
   config_set_float(conf, "video_aspect_ratio", settings->video.aspect_ratio);
   config_set_bool(conf,  "video_aspect_ratio_auto", settings->video.aspect_ratio_auto);
   config_set_bool(conf,  "video_windowed_fullscreen",
         settings->video.windowed_fullscreen);
   config_set_float(conf, "video_scale", settings->video.scale);
   config_set_int(conf,   "autosave_interval", settings->autosave_interval);
   config_set_bool(conf,  "video_crop_overscan", settings->video.crop_overscan);
   config_set_bool(conf,  "video_scale_integer", settings->video.scale_integer);
#ifdef GEKKO
   config_set_int(conf,   "video_viwidth", settings->video.viwidth);
   config_set_bool(conf,  "video_vfilter", settings->video.vfilter);
#endif
   config_set_bool(conf,  "video_smooth", settings->video.smooth);
   config_set_bool(conf,  "video_threaded", settings->video.threaded);
   config_set_bool(conf,  "video_shared_context",
         settings->video.shared_context);
   config_set_bool(conf,  "video_force_srgb_disable",
         settings->video.force_srgb_disable);
   config_set_bool(conf,  "video_fullscreen", settings->video.fullscreen);
   config_set_float(conf, "video_refresh_rate", settings->video.refresh_rate);
   config_set_int(conf,   "video_monitor_index",
         settings->video.monitor_index);
   config_set_int(conf,   "video_fullscreen_x", settings->video.fullscreen_x);
   config_set_int(conf,   "video_fullscreen_y", settings->video.fullscreen_y);
   config_set_string(conf,"video_driver", settings->video.driver);
#ifdef HAVE_MENU
#ifdef HAVE_THREADS
   config_set_bool(conf,"threaded_data_runloop_enable", settings->menu.threaded_data_runloop_enable);
#endif
   config_set_string(conf,"menu_driver", settings->menu.driver);
   config_set_bool(conf,"menu_pause_libretro", settings->menu.pause_libretro);
   config_set_bool(conf,"menu_mouse_enable", settings->menu.mouse.enable);
   config_set_bool(conf,"menu_timedate_enable", settings->menu.timedate_enable);
   config_set_bool(conf,"menu_core_enable", settings->menu.core_enable);
   config_set_path(conf, "menu_wallpaper", settings->menu.wallpaper);
#endif
   config_set_bool(conf,  "video_vsync", settings->video.vsync);
   config_set_bool(conf,  "video_hard_sync", settings->video.hard_sync);
   config_set_int(conf,   "video_hard_sync_frames",
         settings->video.hard_sync_frames);
   config_set_int(conf,   "video_frame_delay", settings->video.frame_delay);
   config_set_bool(conf,  "video_black_frame_insertion",
         settings->video.black_frame_insertion);
   config_set_bool(conf,  "video_disable_composition",
         settings->video.disable_composition);
   config_set_bool(conf,  "pause_nonactive", settings->pause_nonactive);
   config_set_int(conf, "video_swap_interval", settings->video.swap_interval);
   config_set_bool(conf, "video_gpu_screenshot", settings->video.gpu_screenshot);
   config_set_int(conf, "video_rotation", settings->video.rotation);
   config_set_path(conf, "screenshot_directory",
         *settings->screenshot_directory ?
         settings->screenshot_directory : "default");
   config_set_int(conf, "aspect_ratio_index", settings->video.aspect_ratio_idx);
   config_set_string(conf, "audio_device", settings->audio.device);
   config_set_string(conf, "video_filter", settings->video.softfilter_plugin);
   config_set_string(conf, "audio_dsp_plugin", settings->audio.dsp_plugin);
   config_set_string(conf, "core_updater_buildbot_url", settings->network.buildbot_url);
   config_set_string(conf, "core_updater_buildbot_assets_url", settings->network.buildbot_assets_url);
   config_set_bool(conf, "core_updater_auto_extract_archive", settings->network.buildbot_auto_extract_archive);
   config_set_string(conf, "camera_device", settings->camera.device);
   config_set_bool(conf, "camera_allow", settings->camera.allow);
   config_set_bool(conf, "audio_rate_control", settings->audio.rate_control);
   config_set_float(conf, "audio_rate_control_delta",
         settings->audio.rate_control_delta);
   config_set_float(conf, "audio_max_timing_skew",
         settings->audio.max_timing_skew);
   config_set_float(conf, "audio_volume", settings->audio.volume);
   config_set_string(conf, "video_context_driver", settings->video.context_driver);
   config_set_string(conf, "audio_driver", settings->audio.driver);
   config_set_bool(conf, "audio_enable", settings->audio.enable);
   config_set_bool(conf, "audio_mute_enable", settings->audio.mute_enable);
   config_set_int(conf, "audio_out_rate", settings->audio.out_rate);

   config_set_bool(conf, "location_allow", settings->location.allow);

   config_set_float(conf, "video_font_size", settings->video.font_size);
   config_set_bool(conf,  "video_font_enable", settings->video.font_enable);

   if (!global->has_set_ups_pref)
      config_set_bool(conf, "ups_pref", global->ups_pref);
   if (!global->has_set_bps_pref)
      config_set_bool(conf, "bps_pref", global->bps_pref);
   if (!global->has_set_ips_pref)
      config_set_bool(conf, "ips_pref", global->ips_pref);

   config_set_path(conf, "system_directory",
         *settings->system_directory ?
         settings->system_directory : "default");
   config_set_path(conf, "extraction_directory",
         settings->extraction_directory);
   config_set_path(conf, "input_remapping_directory",
         settings->input_remapping_directory);
   config_set_path(conf, "input_remapping_path",
        settings->input.remapping_path);
   config_set_path(conf, "resampler_directory",
         settings->resampler_directory);
   config_set_string(conf, "audio_resampler", settings->audio.resampler);
   config_set_path(conf, "savefile_directory",
         *global->savefile_dir ? global->savefile_dir : "default");
   config_set_path(conf, "savestate_directory",
         *global->savestate_dir ? global->savestate_dir : "default");
   config_set_path(conf, "video_shader_dir",
         *settings->video.shader_dir ?
         settings->video.shader_dir : "default");
   config_set_path(conf, "video_filter_dir",
         *settings->video.filter_dir ?
         settings->video.filter_dir : "default");
   config_set_path(conf, "audio_filter_dir",
         *settings->audio.filter_dir ?
         settings->audio.filter_dir : "default");

   config_set_path(conf, "core_assets_directory",
         *settings->core_assets_directory ?
         settings->core_assets_directory : "default");
   config_set_path(conf, "assets_directory",
         *settings->assets_directory ?
         settings->assets_directory : "default");
   config_set_path(conf, "playlist_directory",
         *settings->playlist_directory ?
         settings->playlist_directory : "default");
#ifdef HAVE_MENU
   config_set_path(conf, "rgui_browser_directory",
         *settings->menu_content_directory ?
         settings->menu_content_directory : "default");
   config_set_path(conf, "rgui_config_directory",
         *settings->menu_config_directory ?
         settings->menu_config_directory : "default");
   config_set_bool(conf, "rgui_show_start_screen",
         settings->menu_show_start_screen);
   config_set_bool(conf, "menu_navigation_wraparound_horizontal_enable",
         settings->menu.navigation.wraparound.horizontal_enable);
   config_set_bool(conf, "menu_navigation_wraparound_vertical_enable",
         settings->menu.navigation.wraparound.vertical_enable);
   config_set_bool(conf, "menu_navigation_browser_filter_supported_extensions_enable",
         settings->menu.navigation.browser.filter.supported_extensions_enable);
   config_set_bool(conf, "menu_collapse_subgroups_enable",
         settings->menu.collapse_subgroups_enable);
   config_set_bool(conf, "menu_show_advanced_settings",
         settings->menu.show_advanced_settings);
   config_set_hex(conf, "menu_entry_normal_color",
         settings->menu.entry_normal_color);
   config_set_hex(conf, "menu_entry_hover_color",
         settings->menu.entry_hover_color);
   config_set_hex(conf, "menu_title_color",
         settings->menu.title_color);
#endif

   config_set_path(conf, "game_history_path", settings->content_history_path);
   config_set_int(conf, "game_history_size", settings->content_history_size);
   config_set_path(conf, "joypad_autoconfig_dir",
         settings->input.autoconfig_dir);
   config_set_bool(conf, "input_autodetect_enable",
         settings->input.autodetect_enable);

#ifdef HAVE_OVERLAY
   config_set_path(conf, "overlay_directory",
         *global->overlay_dir ? global->overlay_dir : "default");
   config_set_path(conf, "input_overlay", settings->input.overlay);
   config_set_bool(conf, "input_overlay_enable", settings->input.overlay_enable);
   config_set_float(conf, "input_overlay_opacity",
         settings->input.overlay_opacity);
   config_set_float(conf, "input_overlay_scale",
         settings->input.overlay_scale);

   config_set_path(conf, "osk_overlay_directory",
         *global->osk_overlay_dir ? global->osk_overlay_dir : "default");
   config_set_path(conf, "input_osk_overlay", settings->osk.overlay);
   config_set_bool(conf, "input_osk_overlay_enable", settings->osk.enable);
#endif

   config_set_path(conf, "video_font_path", settings->video.font_path);
   config_set_float(conf, "video_message_pos_x", settings->video.msg_pos_x);
   config_set_float(conf, "video_message_pos_y", settings->video.msg_pos_y);

   config_set_bool(conf, "gamma_correction",
         global->console.screen.gamma_correction);
   config_set_bool(conf, "soft_filter_enable",
         global->console.softfilter_enable);
   config_set_bool(conf, "flicker_filter_enable",
         global->console.flickerfilter_enable);

   config_set_int(conf, "flicker_filter_index",
         global->console.screen.flicker_filter_index);
   config_set_int(conf, "soft_filter_index",
         global->console.screen.soft_filter_index);
   config_set_int(conf, "current_resolution_id",
         global->console.screen.resolutions.current.id);
   config_set_int(conf, "custom_viewport_width",
         global->console.screen.viewports.custom_vp.width);
   config_set_int(conf, "custom_viewport_height",
         global->console.screen.viewports.custom_vp.height);
   config_set_int(conf, "custom_viewport_x",
         global->console.screen.viewports.custom_vp.x);
   config_set_int(conf, "custom_viewport_y",
         global->console.screen.viewports.custom_vp.y);
   config_set_float(conf, "video_font_size", settings->video.font_size);

   config_set_bool(conf, "block_sram_overwrite",
         settings->block_sram_overwrite);
   config_set_bool(conf, "savestate_auto_index",
         settings->savestate_auto_index);
   config_set_bool(conf, "savestate_auto_save",
         settings->savestate_auto_save);
   config_set_bool(conf, "savestate_auto_load",
         settings->savestate_auto_load);
   config_set_bool(conf, "history_list_enable",
         settings->history_list_enable);

   config_set_float(conf, "fastforward_ratio", settings->fastforward_ratio);
   config_set_bool(conf, "fastforward_ratio_throttle_enable", settings->fastforward_ratio_throttle_enable);
   config_set_float(conf, "slowmotion_ratio", settings->slowmotion_ratio);

   config_set_bool(conf, "config_save_on_exit",
         settings->config_save_on_exit);
   config_set_int(conf, "sound_mode", global->console.sound.mode);
   config_set_int(conf, "state_slot", settings->state_slot);

#ifdef HAVE_NETPLAY
   config_set_bool(conf, "netplay_spectator_mode_enable",
         global->netplay_is_spectate);
   config_set_bool(conf, "netplay_mode", global->netplay_is_client);
   config_set_string(conf, "netplay_ip_address", global->netplay_server);
   config_set_int(conf, "netplay_ip_port", global->netplay_port);
   config_set_int(conf, "netplay_delay_frames", global->netplay_sync_frames);
#endif
   config_set_string(conf, "netplay_nickname", settings->username);
   config_set_int(conf, "user_language", settings->user_language);

   config_set_bool(conf, "custom_bgm_enable",
         global->console.sound.system_bgm_enable);

   config_set_string(conf, "input_driver", settings->input.driver);
   config_set_string(conf, "input_joypad_driver",
         settings->input.joypad_driver);
   config_set_string(conf, "input_keyboard_layout",
         settings->input.keyboard_layout);
   for (i = 0; i < MAX_USERS; i++)
   {
      char cfg[64];

      snprintf(cfg, sizeof(cfg), "input_device_p%u", i + 1);
      config_set_int(conf, cfg, settings->input.device[i]);
      snprintf(cfg, sizeof(cfg), "input_player%u_joypad_index", i + 1);
      config_set_int(conf, cfg, settings->input.joypad_map[i]);
      snprintf(cfg, sizeof(cfg), "input_libretro_device_p%u", i + 1);
      config_set_int(conf, cfg, settings->input.libretro_device[i]);
      snprintf(cfg, sizeof(cfg), "input_player%u_analog_dpad_mode", i + 1);
      config_set_int(conf, cfg, settings->input.analog_dpad_mode[i]);
   }

   for (i = 0; i < MAX_USERS; i++)
      save_keybinds_user(conf, i);

   config_set_bool(conf, "core_specific_config",
         settings->core_specific_config);
   config_set_int(conf, "libretro_log_level", settings->libretro_log_level);
   config_set_bool(conf, "log_verbosity", global->verbosity);
   config_set_bool(conf, "perfcnt_enable", global->perfcnt_enable);

   config_set_int(conf, "archive_mode", settings->archive.mode);

   ret = config_file_write(conf, path);
   config_file_free(conf);
   return ret;
}

settings_t *config_get_ptr(void)
{
   return g_config;
}

void config_free(void)
{
   if (!g_config)
      return;

   free(g_config);
   g_config = NULL;
}

settings_t *config_init(void)
{
   g_config = (settings_t*)calloc(1, sizeof(settings_t));

   if (!g_config)
      return NULL;

   return g_config;
}
