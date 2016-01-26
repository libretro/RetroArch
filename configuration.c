/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <ctype.h>

#include <file/config_file.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_stat.h>
#include <string/stdstring.h>

#include "audio/audio_driver.h"
#include "configuration.h"
#include "config.def.h"
#include "input/input_config.h"
#include "input/input_keymaps.h"
#include "input/input_remapping.h"
#include "defaults.h"
#include "general.h"
#include "retroarch.h"
#include "system.h"
#include "verbosity.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

struct defaults g_defaults;

static settings_t **config_get_ptr_double(void)
{
   static settings_t *g_config;
   return &g_config;
}

static void config_free_ptr(void)
{
   settings_t **settings = config_get_ptr_double();
   *settings = NULL;
}

settings_t *config_get_ptr(void)
{
   settings_t **settings = config_get_ptr_double();
   return *settings;
}

void config_free(void)
{
   settings_t *settings = config_get_ptr();
   if (!settings)
      return;

   free(settings);
   config_free_ptr();
}

static bool config_init(void)
{
   settings_t **settings = config_get_ptr_double();
   settings_t    *config = (settings_t*)calloc(1, sizeof(settings_t));

   if (!config)
      return false;

   *settings = config;

   return true;
}

bool config_realloc(void)
{
   config_free();
   return config_init();
}


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
      case AUDIO_PSP:
#ifdef VITA
         return "vita";
#else
         return "psp";
#endif
      case AUDIO_CTR:
         return "csnd";
      case AUDIO_RWEBAUDIO:
         return "rwebaudio";
      default:
         break;
   }

   return "null";
}

const char *config_get_default_record(void)
{
   switch (RECORD_DEFAULT_DRIVER)
   {
      case RECORD_FFMPEG:
         return "ffmpeg";
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
      case VIDEO_VITA2D:
         return "vita2d";
      case VIDEO_CTR:
         return "ctr";
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
         return "android";
      case INPUT_PS3:
         return "ps3";
      case INPUT_PSP:
#ifdef VITA
         return "vita";
#else
         return "psp";
#endif
      case INPUT_CTR:
         return "ctr";
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
      case INPUT_COCOA:
         return "cocoa";
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
      case JOYPAD_XINPUT:
         return "xinput";
      case JOYPAD_GX:
         return "gx";
      case JOYPAD_XDK:
         return "xdk";
      case JOYPAD_PSP:
#ifdef VITA
         return "vita";
#else
         return "psp";
#endif
      case JOYPAD_CTR:
         return "ctr";
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
      case JOYPAD_HID:
         return "hid";
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
      case MENU_XUI:
         return "xui";
      case MENU_MATERIALUI:
         return "glui";
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
      case CAMERA_AVFOUNDATION:
         return "avfoundation";
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
      case LOCATION_CORELOCATION:
         return "corelocation";
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
   settings_t *settings            = config_get_ptr();
   global_t   *global              = global_get_ptr();
   const char *def_video           = config_get_default_video();
   const char *def_audio           = config_get_default_audio();
   const char *def_audio_resampler = config_get_default_audio_resampler();
   const char *def_input           = config_get_default_input();
   const char *def_joypad          = config_get_default_joypad();
#ifdef HAVE_MENU
   const char *def_menu            = config_get_default_menu();
#endif
   const char *def_camera          = config_get_default_camera();
   const char *def_location        = config_get_default_location();
   const char *def_record          = config_get_default_record();
   static bool first_initialized   = true;

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
            def_joypad, sizeof(settings->input.joypad_driver));
   if (def_record)
      strlcpy(settings->record.driver,
            def_record, sizeof(settings->record.driver));
#ifdef HAVE_MENU
   if (def_menu)
      strlcpy(settings->menu.driver,
            def_menu,  sizeof(settings->menu.driver));
#endif

   settings->history_list_enable         = def_history_list_enable;
   settings->load_dummy_on_core_shutdown = load_dummy_on_core_shutdown;

#if TARGET_OS_IPHONE
   settings->input.small_keyboard_enable   = false;
#endif
   settings->input.keyboard_gamepad_enable       = true;
   settings->input.keyboard_gamepad_mapping_type = 1;
#ifdef HAVE_FFMPEG
   settings->multimedia.builtin_mediaplayer_enable  = true;
#else
   settings->multimedia.builtin_mediaplayer_enable  = false;
#endif
   settings->multimedia.builtin_imageviewer_enable = true;
   settings->video.scale                 = scale;
   settings->video.fullscreen            = rarch_ctl(RARCH_CTL_IS_FORCE_FULLSCREEN, NULL)  ? true : fullscreen;
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
   settings->bundle_assets_extract_enable = bundle_assets_extract_enable;

   if (g_defaults.settings.video_threaded_enable != video_threaded)
      settings->video.threaded           = g_defaults.settings.video_threaded_enable;

#ifdef HAVE_THREADS
   settings->threaded_data_runloop_enable = threaded_data_runloop_enable;
#endif
   settings->video.shared_context              = video_shared_context;
   settings->video.force_srgb_disable          = false;
#ifdef GEKKO
   settings->video.viwidth                     = video_viwidth;
   settings->video.vfilter                     = video_vfilter;
#endif
   settings->video.smooth                      = video_smooth;
   settings->video.force_aspect                = force_aspect;
   settings->video.scale_integer               = scale_integer;
   settings->video.crop_overscan               = crop_overscan;
   settings->video.aspect_ratio                = aspect_ratio;
   settings->video.aspect_ratio_auto           = aspect_ratio_auto; /* Let implementation decide if automatic, or 1:1 PAR. */
   settings->video.aspect_ratio_idx            = aspect_ratio_idx;
   settings->video.shader_enable               = shader_enable;
   settings->video.allow_rotate                = allow_rotate;

   settings->video.font_enable                 = font_enable;
   settings->video.font_size                   = font_size;
   settings->video.msg_pos_x                   = message_pos_offset_x;
   settings->video.msg_pos_y                   = message_pos_offset_y;

   settings->video.msg_color_r                 = ((message_color >> 16) & 0xff) / 255.0f;
   settings->video.msg_color_g                 = ((message_color >>  8) & 0xff) / 255.0f;
   settings->video.msg_color_b                 = ((message_color >>  0) & 0xff) / 255.0f;

   settings->video.refresh_rate                = refresh_rate;

   if (g_defaults.settings.video_refresh_rate > 0.0 &&
         g_defaults.settings.video_refresh_rate != refresh_rate)
      settings->video.refresh_rate             = g_defaults.settings.video_refresh_rate;

   settings->video.post_filter_record          = post_filter_record;
   settings->video.gpu_record                  = gpu_record;
   settings->video.gpu_screenshot              = gpu_screenshot;
   settings->video.rotation                    = ORIENTATION_NORMAL;

   settings->audio.enable                      = audio_enable;
   settings->audio.mute_enable                 = false;
   settings->audio.out_rate                    = out_rate;
   settings->audio.block_frames                = 0;
   if (audio_device)
      strlcpy(settings->audio.device,
            audio_device, sizeof(settings->audio.device));

   if (!g_defaults.settings.out_latency)
      g_defaults.settings.out_latency          = out_latency;

   settings->audio.latency                     = g_defaults.settings.out_latency;
   settings->audio.sync                        = audio_sync;
   settings->audio.rate_control                = rate_control;
   settings->audio.rate_control_delta          = rate_control_delta;
   settings->audio.max_timing_skew             = max_timing_skew;
   settings->audio.volume                      = audio_volume;

   audio_driver_set_volume_gain(db_to_gain(settings->audio.volume));

   settings->rewind_enable                     = rewind_enable;
   settings->rewind_buffer_size                = rewind_buffer_size;
   settings->rewind_granularity                = rewind_granularity;
   settings->slowmotion_ratio                  = slowmotion_ratio;
   settings->fastforward_ratio                 = fastforward_ratio;
   settings->pause_nonactive                   = pause_nonactive;
   settings->autosave_interval                 = autosave_interval;

   settings->block_sram_overwrite              = block_sram_overwrite;
   settings->savestate_auto_index              = savestate_auto_index;
   settings->savestate_auto_save               = savestate_auto_save;
   settings->savestate_auto_load               = savestate_auto_load;
   settings->network_cmd_enable                = network_cmd_enable;
   settings->network_cmd_port                  = network_cmd_port;
   settings->network_remote_base_port           = network_remote_base_port;
   settings->stdin_cmd_enable                  = stdin_cmd_enable;
   settings->content_history_size              = default_content_history_size;
   settings->libretro_log_level                = libretro_log_level;

#ifdef HAVE_MENU
   if (first_initialized)
      settings->menu_show_start_screen         = default_menu_show_start_screen;
   settings->menu.pause_libretro               = true;
   settings->menu.mouse.enable                 = false;
   settings->menu.pointer.enable               = pointer_enable;
   settings->menu.timedate_enable              = true;
   settings->menu.core_enable                  = true;
   settings->menu.dynamic_wallpaper_enable     = false;
   settings->menu.boxart_enable                = false;
   *settings->menu.wallpaper                   = '\0';
   settings->menu.show_advanced_settings       = show_advanced_settings;
   settings->menu.entry_normal_color           = menu_entry_normal_color;
   settings->menu.entry_hover_color            = menu_entry_hover_color;
   settings->menu.title_color                  = menu_title_color;

   settings->menu.dpi.override_enable          = menu_dpi_override_enable;
   settings->menu.dpi.override_value           = menu_dpi_override_value;

   settings->menu.navigation.wraparound.setting_enable                  = true;
   settings->menu.navigation.wraparound.enable                          = true;
   settings->menu.navigation.browser.filter.supported_extensions_enable = true;
#endif

   settings->ui.companion_start_on_boot             = ui_companion_start_on_boot;
   settings->ui.companion_enable                    = ui_companion_enable;
   settings->ui.menubar_enable                      = true;
   settings->ui.suspend_screensaver_enable          = true;

   settings->location.allow                         = false;
   settings->camera.allow                           = false;

#ifdef HAVE_CHEEVOS
   settings->cheevos.enable                         = false;
   settings->cheevos.test_unofficial                = false;
   *settings->cheevos.username                      = '\0';
   *settings->cheevos.password                      = '\0';
#endif

   settings->input.back_as_menu_toggle_enable       = true;
   settings->input.input_descriptor_label_show      = input_descriptor_label_show;
   settings->input.input_descriptor_hide_unbound    = input_descriptor_hide_unbound;
   settings->input.remap_binds_enable               = true;
   settings->input.max_users                        = input_max_users;
   settings->input.menu_toggle_gamepad_combo        = menu_toggle_gamepad_combo;

   retro_assert(sizeof(settings->input.binds[0]) >= sizeof(retro_keybinds_1));
   retro_assert(sizeof(settings->input.binds[1]) >= sizeof(retro_keybinds_rest));

   memcpy(settings->input.binds[0], retro_keybinds_1, sizeof(retro_keybinds_1));

   for (i = 1; i < MAX_USERS; i++)
      memcpy(settings->input.binds[i], retro_keybinds_rest,
            sizeof(retro_keybinds_rest));

   input_remapping_set_defaults();

   for (i = 0; i < MAX_USERS; i++)
   {
      for (j = 0; j < RARCH_BIND_LIST_END; j++)
      {
         settings->input.autoconf_binds[i][j].joykey  = NO_BTN;
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
            retro_assert(j == settings->input.binds[i][j].id);
      }

   settings->input.axis_threshold                  = axis_threshold;
   settings->input.netplay_client_swap_input       = netplay_client_swap_input;
   settings->input.turbo_period                    = turbo_period;
   settings->input.turbo_duty_cycle                = turbo_duty_cycle;

   strlcpy(settings->network.buildbot_url, buildbot_server_url,
         sizeof(settings->network.buildbot_url));
   strlcpy(settings->network.buildbot_assets_url, buildbot_assets_server_url,
         sizeof(settings->network.buildbot_assets_url));
   settings->network.buildbot_auto_extract_archive = true;

   settings->input.overlay_enable                  = true;
   settings->input.overlay_enable_autopreferred    = true;
   settings->input.overlay_hide_in_menu            = overlay_hide_in_menu;
   settings->input.overlay_opacity                 = 0.7f;
   settings->input.overlay_scale                   = 1.0f;
   settings->input.autodetect_enable               = input_autodetect_enable;
   *settings->input.keyboard_layout                = '\0';

   settings->osk.enable                            = true;

   for (i = 0; i < MAX_USERS; i++)
   {
      settings->input.joypad_map[i] = i;
      settings->input.analog_dpad_mode[i] = ANALOG_DPAD_NONE;
      if (!global->has_set.libretro_device[i])
         settings->input.libretro_device[i] = RETRO_DEVICE_JOYPAD;
   }

   settings->set_supports_no_game_enable        = true;

   video_driver_ctl(RARCH_DISPLAY_CTL_RESET_CUSTOM_VIEWPORT, NULL);

   /* Make sure settings from other configs carry over into defaults
    * for another config. */
   if (!global->has_set.save_path)
      *global->dir.savefile = '\0';
   if (!global->has_set.state_path)
      *global->dir.savestate = '\0';

   *settings->libretro_info_path = '\0';
   if (!global->has_set.libretro_directory)
      *settings->libretro_directory = '\0';

   if (!global->has_set.ups_pref)
      global->patch.ups_pref = false;
   if (!global->has_set.bps_pref)
      global->patch.bps_pref = false;
   if (!global->has_set.ips_pref)
      global->patch.ips_pref = false;

   *global->record.output_dir = '\0';
   *global->record.config_dir = '\0';

   settings->bundle_assets_extract_version_current = 0;
   settings->bundle_assets_extract_last_version    = 0;
#ifndef IOS
   *settings->bundle_assets_src_path = '\0';
   *settings->bundle_assets_dst_path = '\0';
   *settings->bundle_assets_dst_path_subdir = '\0';
#endif
   *settings->playlist_names = '\0';
   *settings->playlist_cores = '\0';
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
   *settings->cache_directory = '\0';
   *settings->input_remapping_directory = '\0';
   *settings->input.autoconfig_dir = '\0';
   *settings->input.overlay = '\0';
   *settings->core_assets_directory = '\0';
   *settings->assets_directory = '\0';
   *settings->dynamic_wallpapers_directory = '\0';
   *settings->boxarts_directory = '\0';
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
   settings->game_specific_options = default_game_specific_options;
   settings->auto_overrides_enable = default_auto_overrides_enable;
   settings->auto_remaps_enable = default_auto_remaps_enable;

   settings->sort_savefiles_enable = default_sort_savefiles_enable;
   settings->sort_savestates_enable = default_sort_savestates_enable;

   settings->menu_ok_btn          = default_menu_btn_ok;
   settings->menu_cancel_btn      = default_menu_btn_cancel;
   settings->menu_search_btn      = default_menu_btn_search;
   settings->menu_default_btn     = default_menu_btn_default;
   settings->menu_info_btn        = default_menu_btn_info;
   settings->menu_scroll_down_btn = default_menu_btn_scroll_down;
   settings->menu_scroll_up_btn   = default_menu_btn_scroll_up;

   settings->user_language = 0;

   global->console.sound.system_bgm_enable = false;

   video_driver_ctl(RARCH_DISPLAY_CTL_DEFAULT_SETTINGS, NULL);

   if (*g_defaults.path.buildbot_server_url)
      strlcpy(settings->network.buildbot_url,
            g_defaults.path.buildbot_server_url, sizeof(settings->network.buildbot_url));
   if (*g_defaults.dir.wallpapers)
      strlcpy(settings->dynamic_wallpapers_directory,
            g_defaults.dir.wallpapers, sizeof(settings->dynamic_wallpapers_directory));
   if (*g_defaults.dir.remap)
      strlcpy(settings->input_remapping_directory,
            g_defaults.dir.remap, sizeof(settings->input_remapping_directory));
   if (*g_defaults.dir.cache)
      strlcpy(settings->cache_directory,
            g_defaults.dir.cache, sizeof(settings->cache_directory));
   if (*g_defaults.dir.audio_filter)
      strlcpy(settings->audio.filter_dir,
            g_defaults.dir.audio_filter, sizeof(settings->audio.filter_dir));
   if (*g_defaults.dir.video_filter)
      strlcpy(settings->video.filter_dir,
            g_defaults.dir.video_filter, sizeof(settings->video.filter_dir));
   if (*g_defaults.dir.assets)
      strlcpy(settings->assets_directory,
            g_defaults.dir.assets, sizeof(settings->assets_directory));
   if (*g_defaults.dir.core_assets)
      strlcpy(settings->core_assets_directory,
            g_defaults.dir.core_assets, sizeof(settings->core_assets_directory));
   if (*g_defaults.dir.playlist)
      strlcpy(settings->playlist_directory,
            g_defaults.dir.playlist, sizeof(settings->playlist_directory));
   if (*g_defaults.dir.core)
      fill_pathname_expand_special(settings->libretro_directory,
            g_defaults.dir.core, sizeof(settings->libretro_directory));
   if (*g_defaults.path.core)
      runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, g_defaults.path.core);
   if (*g_defaults.dir.database)
      strlcpy(settings->content_database, g_defaults.dir.database,
            sizeof(settings->content_database));
   if (*g_defaults.dir.cursor)
      strlcpy(settings->cursor_directory, g_defaults.dir.cursor,
            sizeof(settings->cursor_directory));
   if (*g_defaults.dir.cheats)
      strlcpy(settings->cheat_database, g_defaults.dir.cheats,
            sizeof(settings->cheat_database));
   if (*g_defaults.dir.core_info)
      fill_pathname_expand_special(settings->libretro_info_path,
            g_defaults.dir.core_info, sizeof(settings->libretro_info_path));
#ifdef HAVE_OVERLAY
   if (*g_defaults.dir.overlay)
   {
      fill_pathname_expand_special(settings->overlay_directory,
            g_defaults.dir.overlay, sizeof(settings->overlay_directory));
#ifdef RARCH_MOBILE
      if (!*settings->input.overlay)
            fill_pathname_join(settings->input.overlay,
                  settings->overlay_directory,
                  "gamepads/retropad/retropad.cfg",
                  sizeof(settings->input.overlay));
#endif
   }

   if (*g_defaults.dir.osk_overlay)
   {
      fill_pathname_expand_special(global->dir.osk_overlay,
            g_defaults.dir.osk_overlay, sizeof(global->dir.osk_overlay));
#ifdef RARCH_MOBILE
      if (!*settings->osk.overlay)
            fill_pathname_join(settings->osk.overlay,
                  global->dir.osk_overlay,
                  "keyboards/modular-keyboard/opaque/big.cfg",
                  sizeof(settings->osk.overlay));
#endif
   }
   else
      strlcpy(global->dir.osk_overlay,
            settings->overlay_directory, sizeof(global->dir.osk_overlay));
#endif
#ifdef HAVE_MENU
   if (*g_defaults.dir.menu_config)
      strlcpy(settings->menu_config_directory,
            g_defaults.dir.menu_config,
            sizeof(settings->menu_config_directory));
#endif
   if (*g_defaults.dir.shader)
      fill_pathname_expand_special(settings->video.shader_dir,
            g_defaults.dir.shader, sizeof(settings->video.shader_dir));
   if (*g_defaults.dir.autoconfig)
      strlcpy(settings->input.autoconfig_dir,
            g_defaults.dir.autoconfig,
            sizeof(settings->input.autoconfig_dir));

   if (!global->has_set.state_path && *g_defaults.dir.savestate)
      strlcpy(global->dir.savestate,
            g_defaults.dir.savestate, sizeof(global->dir.savestate));
   if (!global->has_set.save_path && *g_defaults.dir.sram)
      strlcpy(global->dir.savefile,
            g_defaults.dir.sram, sizeof(global->dir.savefile));
   if (*g_defaults.dir.system)
      strlcpy(settings->system_directory,
            g_defaults.dir.system, sizeof(settings->system_directory));
   if (*g_defaults.dir.screenshot)
      strlcpy(settings->screenshot_directory,
            g_defaults.dir.screenshot,
            sizeof(settings->screenshot_directory));
   if (*g_defaults.dir.resampler)
      strlcpy(settings->resampler_directory,
            g_defaults.dir.resampler,
            sizeof(settings->resampler_directory));
   if (*g_defaults.dir.content_history)
      strlcpy(settings->content_history_directory,
            g_defaults.dir.content_history,
            sizeof(settings->content_history_directory));

   if (*g_defaults.path.config)
      fill_pathname_expand_special(global->path.config,
            g_defaults.path.config, sizeof(global->path.config));

   settings->config_save_on_exit = config_save_on_exit;

   /* Avoid reloading config on every content load */
   if (default_block_config_read)
      rarch_ctl(RARCH_CTL_SET_BLOCK_CONFIG_READ, NULL);
   else
      rarch_ctl(RARCH_CTL_UNSET_BLOCK_CONFIG_READ, NULL);
   
   first_initialized = false;
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
   char conf_path[PATH_MAX_LENGTH] = {0};
   char app_path[PATH_MAX_LENGTH]  = {0};
   const char *xdg                 = NULL;
   const char *home                = NULL;
   config_file_t *conf             = NULL;
   bool saved                      = false;
   global_t *global                = global_get_ptr();

   (void)conf_path;
   (void)app_path;
   (void)saved;
   (void)xdg;
   (void)home;

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
   home = getenv("HOME");

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
   xdg  = getenv("XDG_CONFIG_HOME");
   home = getenv("HOME");

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
         char basedir[PATH_MAX_LENGTH] = {0};

         /* Try to create a new config file. */

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
            char skeleton_conf[PATH_MAX_LENGTH] = {0};

            fill_pathname_join(skeleton_conf, GLOBAL_CONFIG_DIR,
                  "retroarch.cfg", sizeof(skeleton_conf));
            conf = config_file_new(skeleton_conf);
            if (conf)
               RARCH_WARN("Config: using skeleton config \"%s\" as base for a new config file.\n", skeleton_conf);
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

            RARCH_WARN("Config: Created new config file in: \"%s\".\n", conf_path);
         }
      }
   }
#endif

   if (!conf)
      return NULL;

   strlcpy(global->path.config, conf_path,
         sizeof(global->path.config));

   return conf;
}

static void read_keybinds_keyboard(config_file_t *conf, unsigned user,
      unsigned idx, struct retro_keybind *bind)
{
   const char *prefix = NULL;

   if (!input_config_bind_map_get_valid(idx))
      return;

   if (!input_config_bind_map_get_base(idx))
      return;

   prefix = input_config_get_prefix(user, input_config_bind_map_get_meta(idx));

   if (prefix)
      input_config_parse_key(conf, prefix,
            input_config_bind_map_get_base(idx), bind);
}

static void read_keybinds_button(config_file_t *conf, unsigned user,
      unsigned idx, struct retro_keybind *bind)
{
   const char *prefix = NULL;

   if (!input_config_bind_map_get_valid(idx))
      return;
   if (!input_config_bind_map_get_base(idx))
      return;

   prefix = input_config_get_prefix(user,
         input_config_bind_map_get_meta(idx));

   if (prefix)
      input_config_parse_joy_button(conf, prefix,
            input_config_bind_map_get_base(idx), bind);
}

static void read_keybinds_axis(config_file_t *conf, unsigned user,
      unsigned idx, struct retro_keybind *bind)
{
   const char *prefix = NULL;

   if (!input_config_bind_map_get_valid(idx))
      return;
   if (!input_config_bind_map_get_base(idx))
      return;

   prefix = input_config_get_prefix(user,
         input_config_bind_map_get_meta(idx));

   if (prefix)
      input_config_parse_joy_axis(conf, prefix,
            input_config_bind_map_get_base(idx), bind);
}

static void read_keybinds_user(config_file_t *conf, unsigned user)
{
   unsigned i;
   settings_t *settings = config_get_ptr();

   for (i = 0; input_config_bind_map_get_valid(i); i++)
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
#if 0
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
      RARCH_LOG("%s = \"%s\"%s\n", list->key,
            list->value, list->readonly ? " (included)" : "");
      list = list->next;
   }
}
#endif

static void config_get_hex_base(config_file_t *conf, const char *key, unsigned *base)
{
   unsigned tmp = 0;
   if (!base)
      return;
   if (config_get_hex(conf, key, &tmp))
      *base = tmp;
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
   bool tmp_bool;
   char *save                            = NULL;
   const char *extra_path                = NULL;
   char tmp_str[PATH_MAX_LENGTH]         = {0};
   char tmp_append_path[PATH_MAX_LENGTH] = {0}; /* Don't destroy append_config_path. */
   unsigned msg_color                    = 0;
   config_file_t *conf                   = NULL;
   settings_t *settings                  = config_get_ptr();
   global_t   *global                    = global_get_ptr();
   bool *verbose                         = retro_main_verbosity();

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

   strlcpy(tmp_append_path, global->path.append_config,
         sizeof(tmp_append_path));
   extra_path = strtok_r(tmp_append_path, "|", &save);

   while (extra_path)
   {
      bool ret = config_append_file(conf, extra_path);

      RARCH_LOG("Config: appending config \"%s\"\n", extra_path);

      if (!ret)
         RARCH_ERR("Config: failed to append config \"%s\"\n", extra_path);
      extra_path = strtok_r(NULL, "|", &save);
   }
#if 0
   if (*verbose)
   {
      RARCH_LOG_OUTPUT("=== Config ===\n");
      config_file_dump_all(conf);
      RARCH_LOG_OUTPUT("=== Config end ===\n");
   }
#endif

   CONFIG_GET_FLOAT_BASE(conf, settings, video.scale, "video_scale");
   CONFIG_GET_INT_BASE  (conf, settings, video.fullscreen_x, "video_fullscreen_x");
   CONFIG_GET_INT_BASE  (conf, settings, video.fullscreen_y, "video_fullscreen_y");

   if (!rarch_ctl(RARCH_CTL_IS_FORCE_FULLSCREEN, NULL))
      CONFIG_GET_BOOL_BASE(conf, settings, video.fullscreen, "video_fullscreen");

   config_get_array(conf, "playlist_names", settings->playlist_names, sizeof(settings->playlist_names));
   config_get_array(conf, "playlist_cores", settings->playlist_cores, sizeof(settings->playlist_cores));

   CONFIG_GET_BOOL_BASE(conf, settings, video.windowed_fullscreen, "video_windowed_fullscreen");
   CONFIG_GET_INT_BASE (conf, settings, video.monitor_index, "video_monitor_index");
   CONFIG_GET_BOOL_BASE(conf, settings, video.disable_composition, "video_disable_composition");
   CONFIG_GET_BOOL_BASE(conf, settings, video.vsync, "video_vsync");
   CONFIG_GET_BOOL_BASE(conf, settings, video.hard_sync, "video_hard_sync");

#ifdef HAVE_MENU
#ifdef HAVE_THREADS
   CONFIG_GET_BOOL_BASE(conf, settings, threaded_data_runloop_enable,
         "threaded_data_runloop_enable");
#endif

   CONFIG_GET_BOOL_BASE(conf, settings, menu.dpi.override_enable,
         "dpi_override_enable");
   CONFIG_GET_INT_BASE (conf, settings, menu.dpi.override_value,
         "dpi_override_value");

   CONFIG_GET_BOOL_BASE(conf, settings, menu.pause_libretro,
         "menu_pause_libretro");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.mouse.enable,
         "menu_mouse_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.pointer.enable,
         "menu_pointer_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.timedate_enable,
         "menu_timedate_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.core_enable,
         "menu_core_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.dynamic_wallpaper_enable,
         "menu_dynamic_wallpaper_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.boxart_enable,
         "menu_boxart_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.navigation.wraparound.enable,
         "menu_navigation_wraparound_enable");
   CONFIG_GET_BOOL_BASE(conf, settings,
         menu.navigation.browser.filter.supported_extensions_enable,
         "menu_navigation_browser_filter_supported_extensions_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, menu.show_advanced_settings,
         "menu_show_advanced_settings");
   config_get_hex_base(conf, "menu_entry_normal_color",
         &settings->menu.entry_normal_color);
   config_get_hex_base(conf, "menu_entry_hover_color",
         &settings->menu.entry_hover_color);
   config_get_hex_base(conf, "menu_title_color",
         &settings->menu.title_color);
   config_get_path(conf, "menu_wallpaper",
         settings->menu.wallpaper, sizeof(settings->menu.wallpaper));
   if (string_is_equal(settings->menu.wallpaper, "default"))
      *settings->menu.wallpaper = '\0';
#endif

   CONFIG_GET_INT_BASE(conf, settings, video.hard_sync_frames, "video_hard_sync_frames");
   if (settings->video.hard_sync_frames > 3)
      settings->video.hard_sync_frames = 3;

   CONFIG_GET_INT_BASE(conf, settings, video.frame_delay, "video_frame_delay");
   if (settings->video.frame_delay > 15)
      settings->video.frame_delay = 15;

   CONFIG_GET_BOOL_BASE(conf, settings, video.black_frame_insertion, "video_black_frame_insertion");
   CONFIG_GET_INT_BASE(conf, settings, video.swap_interval, "video_swap_interval");
   settings->video.swap_interval = max(settings->video.swap_interval, 1);
   settings->video.swap_interval = min(settings->video.swap_interval, 4);
   CONFIG_GET_BOOL_BASE(conf, settings, video.threaded, "video_threaded");
   CONFIG_GET_BOOL_BASE(conf, settings, video.shared_context, "video_shared_context");
#ifdef GEKKO
   CONFIG_GET_INT_BASE(conf, settings, video.viwidth, "video_viwidth");
   CONFIG_GET_BOOL_BASE(conf, settings, video.vfilter, "video_vfilter");
#endif
   CONFIG_GET_BOOL_BASE(conf, settings, video.smooth, "video_smooth");
   CONFIG_GET_BOOL_BASE(conf, settings, video.force_aspect, "video_force_aspect");
   CONFIG_GET_BOOL_BASE(conf, settings, video.scale_integer, "video_scale_integer");
   CONFIG_GET_BOOL_BASE(conf, settings, video.crop_overscan, "video_crop_overscan");
   CONFIG_GET_FLOAT_BASE(conf, settings, video.aspect_ratio, "video_aspect_ratio");
   CONFIG_GET_INT_BASE(conf, settings, video.aspect_ratio_idx, "aspect_ratio_index");
   CONFIG_GET_BOOL_BASE(conf,  settings, video.aspect_ratio_auto, "video_aspect_ratio_auto");
   CONFIG_GET_FLOAT_BASE(conf, settings, video.refresh_rate, "video_refresh_rate");

   config_get_path(conf, "video_shader", settings->video.shader_path, sizeof(settings->video.shader_path));
   CONFIG_GET_BOOL_BASE(conf, settings, video.shader_enable, "video_shader_enable");

   CONFIG_GET_BOOL_BASE(conf, settings, video.allow_rotate, "video_allow_rotate");

   config_get_path(conf, "video_font_path", settings->video.font_path, sizeof(settings->video.font_path));
   CONFIG_GET_FLOAT_BASE(conf, settings, video.font_size, "video_font_size");
   CONFIG_GET_BOOL_BASE(conf, settings, video.font_enable, "video_font_enable");
   CONFIG_GET_FLOAT_BASE(conf, settings, video.msg_pos_x, "video_message_pos_x");
   CONFIG_GET_FLOAT_BASE(conf, settings, video.msg_pos_y, "video_message_pos_y");
   CONFIG_GET_INT_BASE(conf, settings, video.rotation, "video_rotation");

   CONFIG_GET_BOOL_BASE(conf, settings, video.force_srgb_disable, "video_force_srgb_disable");

   CONFIG_GET_BOOL_BASE(conf, settings, set_supports_no_game_enable, "core_set_supports_no_game_enable");

#ifdef RARCH_CONSOLE
   /* TODO - will be refactored later to make it more clean - it's more
    * important that it works for consoles right now */
   config_get_bool(conf, "custom_bgm_enable",
         &global->console.sound.system_bgm_enable);
   video_driver_ctl(RARCH_DISPLAY_CTL_LOAD_SETTINGS, conf);
#endif
   CONFIG_GET_INT_BASE(conf, settings, state_slot, "state_slot");

   CONFIG_GET_INT_BASE(conf, settings, video_viewport_custom.width,  "custom_viewport_width");
   CONFIG_GET_INT_BASE(conf, settings, video_viewport_custom.height, "custom_viewport_height");
   CONFIG_GET_INT_BASE(conf, settings, video_viewport_custom.x,      "custom_viewport_x");
   CONFIG_GET_INT_BASE(conf, settings, video_viewport_custom.y,      "custom_viewport_y");

   if (config_get_hex(conf, "video_message_color", &msg_color))
   {
      settings->video.msg_color_r = ((msg_color >> 16) & 0xff) / 255.0f;
      settings->video.msg_color_g = ((msg_color >>  8) & 0xff) / 255.0f;
      settings->video.msg_color_b = ((msg_color >>  0) & 0xff) / 255.0f;
   }

   CONFIG_GET_BOOL_BASE(conf, settings, video.post_filter_record, "video_post_filter_record");
   CONFIG_GET_BOOL_BASE(conf, settings, video.gpu_record, "video_gpu_record");
   CONFIG_GET_BOOL_BASE(conf, settings, video.gpu_screenshot, "video_gpu_screenshot");

   config_get_path(conf, "video_shader_dir", settings->video.shader_dir, sizeof(settings->video.shader_dir));
   if (string_is_equal(settings->video.shader_dir, "default"))
      *settings->video.shader_dir = '\0';

   config_get_path(conf, "video_filter_dir", settings->video.filter_dir, sizeof(settings->video.filter_dir));
   if (string_is_equal(settings->video.filter_dir, "default"))
      *settings->video.filter_dir = '\0';

   config_get_path(conf, "audio_filter_dir", settings->audio.filter_dir, sizeof(settings->audio.filter_dir));
   if (string_is_equal(settings->audio.filter_dir, "default"))
      *settings->audio.filter_dir = '\0';

   CONFIG_GET_BOOL_BASE(conf, settings, input.back_as_menu_toggle_enable, "back_as_menu_toggle_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, input.remap_binds_enable, "input_remap_binds_enable");
   CONFIG_GET_FLOAT_BASE(conf, settings, input.axis_threshold, "input_axis_threshold");
   CONFIG_GET_BOOL_BASE(conf, settings, input.netplay_client_swap_input, "netplay_client_swap_input");
   CONFIG_GET_INT_BASE(conf, settings, input.max_users, "input_max_users");
   CONFIG_GET_INT_BASE(conf, settings, input.menu_toggle_gamepad_combo, "input_menu_toggle_gamepad_combo");
   CONFIG_GET_BOOL_BASE(conf, settings, input.input_descriptor_label_show, "input_descriptor_label_show");
   CONFIG_GET_BOOL_BASE(conf, settings, input.input_descriptor_hide_unbound, "input_descriptor_hide_unbound");

   CONFIG_GET_BOOL_BASE(conf, settings, ui.companion_start_on_boot, "ui_companion_start_on_boot");
   CONFIG_GET_BOOL_BASE(conf, settings, ui.companion_enable, "ui_companion_enable");

   config_get_path(conf, "core_updater_buildbot_url",
         settings->network.buildbot_url, sizeof(settings->network.buildbot_url));
   config_get_path(conf, "core_updater_buildbot_assets_url",
         settings->network.buildbot_assets_url, sizeof(settings->network.buildbot_assets_url));
   CONFIG_GET_BOOL_BASE(conf, settings, network.buildbot_auto_extract_archive, "core_updater_auto_extract_archive");

   for (i = 0; i < MAX_USERS; i++)
   {
      char buf[64] = {0};
      snprintf(buf, sizeof(buf), "input_player%u_joypad_index", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, input.joypad_map[i], buf);

      snprintf(buf, sizeof(buf), "input_player%u_analog_dpad_mode", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, input.analog_dpad_mode[i], buf);

      if (!global->has_set.libretro_device[i])
      {
         snprintf(buf, sizeof(buf), "input_libretro_device_p%u", i + 1);
         CONFIG_GET_INT_BASE(conf, settings, input.libretro_device[i], buf);
      }
   }

   if (!global->has_set.ups_pref)
   {
      CONFIG_GET_BOOL_BASE(conf, global, patch.ups_pref, "ups_pref");
   }
   if (!global->has_set.bps_pref)
   {
      CONFIG_GET_BOOL_BASE(conf, global, patch.bps_pref, "bps_pref");
   }
   if (!global->has_set.ips_pref)
   {
      CONFIG_GET_BOOL_BASE(conf, global, patch.ips_pref, "ips_pref");
   }

   /* Audio settings. */
   CONFIG_GET_BOOL_BASE(conf, settings, audio.enable, "audio_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, audio.mute_enable, "audio_mute_enable");
   CONFIG_GET_INT_BASE(conf, settings, audio.out_rate, "audio_out_rate");
   CONFIG_GET_INT_BASE(conf, settings, audio.block_frames, "audio_block_frames");

   config_get_array(conf, "audio_device", settings->audio.device, sizeof(settings->audio.device));

   CONFIG_GET_INT_BASE(conf, settings, audio.latency, "audio_latency");
   CONFIG_GET_BOOL_BASE(conf, settings, audio.sync, "audio_sync");
   CONFIG_GET_BOOL_BASE(conf, settings, audio.rate_control, "audio_rate_control");
   CONFIG_GET_FLOAT_BASE(conf, settings, audio.rate_control_delta, "audio_rate_control_delta");
   CONFIG_GET_FLOAT_BASE(conf, settings, audio.max_timing_skew, "audio_max_timing_skew");
   CONFIG_GET_FLOAT_BASE(conf, settings, audio.volume, "audio_volume");

   config_get_array(conf, "audio_resampler", settings->audio.resampler, sizeof(settings->audio.resampler));

   audio_driver_set_volume_gain(db_to_gain(settings->audio.volume));

   config_get_array(conf, "camera_device", settings->camera.device, sizeof(settings->camera.device));

   CONFIG_GET_BOOL_BASE(conf, settings, camera.allow, "camera_allow");

#ifdef HAVE_CHEEVOS
   CONFIG_GET_BOOL_BASE(conf, settings, cheevos.enable, "cheevos_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, cheevos.test_unofficial, "cheevos_test_unofficial");
   config_get_array(conf, "cheevos_username", settings->cheevos.username, sizeof(settings->cheevos.username));
   config_get_array(conf, "cheevos_password", settings->cheevos.password, sizeof(settings->cheevos.password));
#endif

   CONFIG_GET_BOOL_BASE(conf, settings, location.allow, "location_allow");
   config_get_array(conf, "video_driver",    settings->video.driver, sizeof(settings->video.driver));
   config_get_array(conf, "record_driver",   settings->record.driver, sizeof(settings->video.driver));
   config_get_array(conf, "camera_driver",   settings->camera.driver, sizeof(settings->camera.driver));
   config_get_array(conf, "location_driver", settings->location.driver, sizeof(settings->location.driver));
#ifdef HAVE_MENU
   config_get_array(conf, "menu_driver",     settings->menu.driver, sizeof(settings->menu.driver));
#endif
   config_get_array(conf, "video_context_driver", settings->video.context_driver, sizeof(settings->video.context_driver));
   config_get_array(conf, "audio_driver", settings->audio.driver, sizeof(settings->audio.driver));
   config_get_path(conf, "video_filter", settings->video.softfilter_plugin, sizeof(settings->video.softfilter_plugin));
   config_get_path(conf, "audio_dsp_plugin", settings->audio.dsp_plugin, sizeof(settings->audio.dsp_plugin));
   config_get_array(conf, "input_driver", settings->input.driver, sizeof(settings->input.driver));
   config_get_array(conf, "input_joypad_driver", settings->input.joypad_driver, sizeof(settings->input.joypad_driver));
   config_get_array(conf, "input_keyboard_layout", settings->input.keyboard_layout, sizeof(settings->input.keyboard_layout));

   if (!global->has_set.libretro)
      config_get_path(conf, "libretro_path", settings->libretro, sizeof(settings->libretro));
   if (!global->has_set.libretro_directory)
      config_get_path(conf, "libretro_directory", settings->libretro_directory, sizeof(settings->libretro_directory));

   /* Safe-guard against older behavior. */
   if (path_is_directory(settings->libretro))
   {
      RARCH_WARN("\"libretro_path\" is a directory, using this for \"libretro_directory\" instead.\n");
      strlcpy(settings->libretro_directory, settings->libretro,
            sizeof(settings->libretro_directory));
      *settings->libretro = '\0';
   }

   CONFIG_GET_BOOL_BASE(conf, settings, ui.menubar_enable, "ui_menubar_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, ui.suspend_screensaver_enable, "suspend_screensaver_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, fps_show, "fps_show");
   CONFIG_GET_BOOL_BASE(conf, settings, load_dummy_on_core_shutdown, "load_dummy_on_core_shutdown");
   CONFIG_GET_BOOL_BASE(conf, settings, multimedia.builtin_mediaplayer_enable, "builtin_mediaplayer_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, multimedia.builtin_imageviewer_enable, "builtin_imageviewer_enable");

   config_get_path(conf, "libretro_info_path", settings->libretro_info_path, sizeof(settings->libretro_info_path));

   config_get_path(conf, "core_options_path", settings->core_options_path, sizeof(settings->core_options_path));
   config_get_path(conf, "screenshot_directory", settings->screenshot_directory, sizeof(settings->screenshot_directory));
   if (*settings->screenshot_directory)
   {
      if (string_is_equal(settings->screenshot_directory, "default"))
         *settings->screenshot_directory = '\0';
      else if (!path_is_directory(settings->screenshot_directory))
      {
         RARCH_WARN("screenshot_directory is not an existing directory, ignoring ...\n");
         *settings->screenshot_directory = '\0';
      }
   }

   config_get_path(conf, "input_remapping_path", settings->input.remapping_path,
         sizeof(settings->input.remapping_path));
   config_get_path(conf, "resampler_directory", settings->resampler_directory,
         sizeof(settings->resampler_directory));
   config_get_path(conf, "cache_directory", settings->cache_directory,
         sizeof(settings->cache_directory));
   config_get_path(conf, "input_remapping_directory", settings->input_remapping_directory,
         sizeof(settings->input_remapping_directory));
   config_get_path(conf, "core_assets_directory", settings->core_assets_directory,
         sizeof(settings->core_assets_directory));
   config_get_path(conf, "assets_directory", settings->assets_directory,
         sizeof(settings->assets_directory));
   config_get_path(conf, "dynamic_wallpapers_directory", settings->dynamic_wallpapers_directory,
         sizeof(settings->dynamic_wallpapers_directory));
   config_get_path(conf, "boxarts_directory", settings->boxarts_directory,
         sizeof(settings->boxarts_directory));
   config_get_path(conf, "playlist_directory", settings->playlist_directory,
         sizeof(settings->playlist_directory));
   if (string_is_equal(settings->core_assets_directory, "default"))
      *settings->core_assets_directory = '\0';
   if (string_is_equal(settings->assets_directory, "default"))
      *settings->assets_directory = '\0';
   if (string_is_equal(settings->dynamic_wallpapers_directory, "default"))
      *settings->dynamic_wallpapers_directory = '\0';
   if (string_is_equal(settings->boxarts_directory, "default"))
      *settings->boxarts_directory = '\0';
   if (string_is_equal(settings->playlist_directory, "default"))
      *settings->playlist_directory = '\0';
#ifdef HAVE_MENU
   config_get_path(conf, "rgui_browser_directory", settings->menu_content_directory,
         sizeof(settings->menu_content_directory));
   if (string_is_equal(settings->menu_content_directory, "default"))
      *settings->menu_content_directory = '\0';
   config_get_path(conf, "rgui_config_directory", settings->menu_config_directory,
         sizeof(settings->menu_config_directory));
   if (string_is_equal(settings->menu_config_directory, "default"))
      *settings->menu_config_directory = '\0';
   CONFIG_GET_BOOL_BASE(conf, settings, menu_show_start_screen, "rgui_show_start_screen");
#endif
   CONFIG_GET_INT_BASE(conf, settings, libretro_log_level, "libretro_log_level");

   if (!global->has_set.verbosity)
   {
      if (config_get_bool(conf, "log_verbosity", &tmp_bool))
      {
         if (verbose)
            *verbose = tmp_bool;
      }
   }

   {
      bool tmp_bool;
      char tmp[64] = {0};
      strlcpy(tmp, "perfcnt_enable", sizeof(tmp));
      config_get_bool(conf, tmp, &tmp_bool);

      if (tmp_bool)
         runloop_ctl(RUNLOOP_CTL_SET_PERFCNT_ENABLE, NULL);
      else
         runloop_ctl(RUNLOOP_CTL_UNSET_PERFCNT_ENABLE, NULL);
   }

#if TARGET_OS_IPHONE
   CONFIG_GET_BOOL_BASE(conf, settings, input.small_keyboard_enable,   "small_keyboard_enable");
#endif
   CONFIG_GET_BOOL_BASE(conf, settings, input.keyboard_gamepad_enable, "keyboard_gamepad_enable");
   CONFIG_GET_INT_BASE(conf, settings, input.keyboard_gamepad_mapping_type, "keyboard_gamepad_mapping_type");

   config_get_path(conf, "recording_output_directory", global->record.output_dir,
         sizeof(global->record.output_dir));
   config_get_path(conf, "recording_config_directory", global->record.config_dir,
         sizeof(global->record.config_dir));

#ifdef HAVE_OVERLAY
   config_get_path(conf, "overlay_directory", settings->overlay_directory, sizeof(settings->overlay_directory));
   if (string_is_equal(settings->overlay_directory, "default"))
      *settings->overlay_directory = '\0';

   config_get_path(conf, "input_overlay", settings->input.overlay, sizeof(settings->input.overlay));
   CONFIG_GET_BOOL_BASE(conf, settings, input.overlay_enable, "input_overlay_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, input.overlay_enable_autopreferred, "input_overlay_enable_autopreferred");
   CONFIG_GET_BOOL_BASE(conf, settings, input.overlay_hide_in_menu, "input_overlay_hide_in_menu");
   CONFIG_GET_FLOAT_BASE(conf, settings, input.overlay_opacity, "input_overlay_opacity");
   CONFIG_GET_FLOAT_BASE(conf, settings, input.overlay_scale, "input_overlay_scale");

   config_get_path(conf, "osk_overlay_directory", global->dir.osk_overlay, sizeof(global->dir.osk_overlay));
   if (string_is_equal(global->dir.osk_overlay, "default"))
      *global->dir.osk_overlay = '\0';

   config_get_path(conf, "input_osk_overlay", settings->osk.overlay, sizeof(settings->osk.overlay));
   CONFIG_GET_BOOL_BASE(conf, settings, osk.enable, "input_osk_overlay_enable");
#endif

   CONFIG_GET_BOOL_BASE(conf, settings, rewind_enable, "rewind_enable");

   {
      /* ugly hack around C89 not allowing mixing declarations and code */
      int buffer_size = 0;
      if (config_get_int(conf, "rewind_buffer_size", &buffer_size))
         settings->rewind_buffer_size = buffer_size * UINT64_C(1000000);
   }

   CONFIG_GET_BOOL_BASE(conf, settings, bundle_assets_extract_enable, "bundle_assets_extract_enable");
   CONFIG_GET_INT_BASE(conf, settings, bundle_assets_extract_version_current, "bundle_assets_extract_version_current");
   CONFIG_GET_INT_BASE(conf, settings, bundle_assets_extract_last_version,    "bundle_assets_extract_last_version");
   config_get_array(conf, "bundle_assets_src_path", settings->bundle_assets_src_path, sizeof(settings->bundle_assets_src_path));
   config_get_array(conf, "bundle_assets_dst_path", settings->bundle_assets_dst_path, sizeof(settings->bundle_assets_dst_path));
   config_get_array(conf, "bundle_assets_dst_path_subdir", settings->bundle_assets_dst_path_subdir, sizeof(settings->bundle_assets_dst_path_subdir));

   CONFIG_GET_INT_BASE(conf, settings, rewind_granularity, "rewind_granularity");
   CONFIG_GET_FLOAT_BASE(conf, settings, slowmotion_ratio, "slowmotion_ratio");
   if (settings->slowmotion_ratio < 1.0f)
      settings->slowmotion_ratio = 1.0f;

   CONFIG_GET_FLOAT_BASE(conf, settings, fastforward_ratio, "fastforward_ratio");

   /* Sanitize fastforward_ratio value - previously range was -1
    * and up (with 0 being skipped) */
   if (settings->fastforward_ratio < 0.0f)
      settings->fastforward_ratio = 0.0f;

   CONFIG_GET_BOOL_BASE(conf, settings, pause_nonactive, "pause_nonactive");
   CONFIG_GET_INT_BASE(conf, settings, autosave_interval, "autosave_interval");

   config_get_path(conf, "content_database_path",
         settings->content_database, sizeof(settings->content_database));
   config_get_path(conf, "cheat_database_path",
         settings->cheat_database, sizeof(settings->cheat_database));
   config_get_path(conf, "cursor_directory",
         settings->cursor_directory, sizeof(settings->cursor_directory));
   config_get_path(conf, "cheat_settings_path",
         settings->cheat_settings_path, sizeof(settings->cheat_settings_path));

   CONFIG_GET_BOOL_BASE(conf, settings, block_sram_overwrite, "block_sram_overwrite");
   CONFIG_GET_BOOL_BASE(conf, settings, savestate_auto_index, "savestate_auto_index");
   CONFIG_GET_BOOL_BASE(conf, settings, savestate_auto_save, "savestate_auto_save");
   CONFIG_GET_BOOL_BASE(conf, settings, savestate_auto_load, "savestate_auto_load");

#ifdef HAVE_COMMAND
   CONFIG_GET_BOOL_BASE(conf, settings, network_cmd_enable, "network_cmd_enable");
   CONFIG_GET_INT_BASE(conf, settings, network_cmd_port, "network_cmd_port");
   CONFIG_GET_BOOL_BASE(conf, settings, stdin_cmd_enable, "stdin_cmd_enable");
#endif

#ifdef HAVE_NETWORK_GAMEPAD
   CONFIG_GET_BOOL_BASE(conf, settings, network_remote_enable, "network_remote_enable");
   for (i = 0; i < MAX_USERS; i++)
   {
      char tmp[64] = {0};
      snprintf(tmp, sizeof(tmp), "network_remote_enable_user_p%u", i + 1);
      config_get_bool(conf, tmp, &settings->network_remote_enable_user[i]);
   }
   CONFIG_GET_INT_BASE(conf, settings, network_remote_base_port, "network_remote_base_port");
   
#endif

   CONFIG_GET_BOOL_BASE(conf, settings, debug_panel_enable, "debug_panel_enable");

   config_get_path(conf, "content_history_dir", settings->content_history_directory,
         sizeof(settings->content_history_directory));

   CONFIG_GET_BOOL_BASE(conf, settings, history_list_enable, "history_list_enable");

   config_get_path(conf, "content_history_path", settings->content_history_path,
         sizeof(settings->content_history_path));
   CONFIG_GET_INT_BASE(conf, settings, content_history_size, "content_history_size");

   CONFIG_GET_INT_BASE(conf, settings, input.turbo_period, "input_turbo_period");
   CONFIG_GET_INT_BASE(conf, settings, input.turbo_duty_cycle, "input_duty_cycle");

   CONFIG_GET_BOOL_BASE(conf, settings, input.autodetect_enable, "input_autodetect_enable");
   config_get_path(conf, "joypad_autoconfig_dir",
         settings->input.autoconfig_dir, sizeof(settings->input.autoconfig_dir));

   if (!global->has_set.username)
      config_get_path(conf, "netplay_nickname",  settings->username, sizeof(settings->username));
   CONFIG_GET_INT_BASE(conf, settings, user_language, "user_language");
#ifdef HAVE_NETPLAY
   if (!global->has_set.netplay_mode)
      CONFIG_GET_BOOL_BASE(conf, global, netplay.is_spectate,
            "netplay_spectator_mode_enable");
   if (!global->has_set.netplay_mode)
      CONFIG_GET_BOOL_BASE(conf, global, netplay.is_client, "netplay_mode");
   if (!global->has_set.netplay_ip_address)
      config_get_path(conf, "netplay_ip_address", global->netplay.server, sizeof(global->netplay.server));
   if (!global->has_set.netplay_delay_frames)
      CONFIG_GET_INT_BASE(conf, global, netplay.sync_frames, "netplay_delay_frames");
   if (!global->has_set.netplay_ip_port)
      CONFIG_GET_INT_BASE(conf, global, netplay.port, "netplay_ip_port");
#endif

   CONFIG_GET_BOOL_BASE(conf, settings, config_save_on_exit, "config_save_on_exit");

   if (!global->has_set.save_path &&
         config_get_path(conf, "savefile_directory", tmp_str, sizeof(tmp_str)))
   {
      if (string_is_equal(tmp_str, "default"))
         strlcpy(global->dir.savefile, g_defaults.dir.sram,
               sizeof(global->dir.savefile));
      else if (path_is_directory(tmp_str))
      {
         strlcpy(global->dir.savefile, tmp_str,
               sizeof(global->dir.savefile));
         strlcpy(global->name.savefile, tmp_str,
               sizeof(global->name.savefile));
         fill_pathname_dir(global->name.savefile, global->name.base,
               ".srm", sizeof(global->name.savefile));
      }
      else
         RARCH_WARN("savefile_directory is not a directory, ignoring ...\n");
   }

   if (!global->has_set.state_path &&
         config_get_path(conf, "savestate_directory", tmp_str, sizeof(tmp_str)))
   {
      if (string_is_equal(tmp_str, "default"))
         strlcpy(global->dir.savestate, g_defaults.dir.savestate,
               sizeof(global->dir.savestate));
      else if (path_is_directory(tmp_str))
      {
         strlcpy(global->dir.savestate, tmp_str,
               sizeof(global->dir.savestate));
         strlcpy(global->name.savestate, tmp_str,
               sizeof(global->name.savestate));
         fill_pathname_dir(global->name.savestate, global->name.base,
               ".state", sizeof(global->name.savestate));
      }
      else
         RARCH_WARN("savestate_directory is not a directory, ignoring ...\n");
   }

   if (string_is_empty(settings->content_history_path))
   {
      if (string_is_empty(settings->content_history_directory))
      {
         fill_pathname_resolve_relative(settings->content_history_path,
               global->path.config, "content_history.lpl",
               sizeof(settings->content_history_path));
      }
      else
      {
         fill_pathname_join(settings->content_history_path,
               settings->content_history_directory,
               "content_history.lpl",
               sizeof(settings->content_history_path));
      }
   }

   if (!config_get_path(conf, "system_directory",
            settings->system_directory, sizeof(settings->system_directory)))
   {
      RARCH_WARN("SYSTEM DIR is empty, assume CONTENT DIR\n");
      *settings->system_directory = '\0';
   }

   if (string_is_equal(settings->system_directory, "default"))
   {
      RARCH_WARN("SYSTEM DIR is empty, assume CONTENT DIR\n");
      *settings->system_directory = '\0';
   }

   config_read_keybinds_conf(conf);

   CONFIG_GET_BOOL_BASE(conf, settings, core_specific_config, "core_specific_config");
   CONFIG_GET_BOOL_BASE(conf, settings, game_specific_options, "game_specific_options");
   CONFIG_GET_BOOL_BASE(conf, settings, auto_overrides_enable, "auto_overrides_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, auto_remaps_enable, "auto_remaps_enable");

   CONFIG_GET_BOOL_BASE(conf, settings, sort_savefiles_enable, "sort_savefiles_enable");
   CONFIG_GET_BOOL_BASE(conf, settings, sort_savestates_enable, "sort_savestates_enable");

   CONFIG_GET_INT_BASE(conf, settings, menu_ok_btn,          "menu_ok_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_cancel_btn,      "menu_cancel_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_search_btn,      "menu_search_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_info_btn,        "menu_info_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_default_btn,     "menu_default_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_cancel_btn,      "menu_cancel_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_scroll_down_btn, "menu_scroll_down_btn");
   CONFIG_GET_INT_BASE(conf, settings, menu_scroll_up_btn,   "menu_scroll_up_btn");

   config_file_free(conf);
   return true;
}

static void config_load_core_specific(void)
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   *global->path.core_specific_config = '\0';

   if (!*settings->libretro)
      return;
#ifdef HAVE_DYNAMIC
   if (rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
      return;
#endif

#ifdef HAVE_MENU
   if (*settings->menu_config_directory)
   {
      path_resolve_realpath(settings->menu_config_directory,
            sizeof(settings->menu_config_directory));
      strlcpy(global->path.core_specific_config,
            settings->menu_config_directory,
            sizeof(global->path.core_specific_config));
   }
   else
#endif
   {
      /* Use original config file's directory as a fallback. */
      fill_pathname_basedir(global->path.core_specific_config,
            global->path.config, sizeof(global->path.core_specific_config));
   }

   fill_pathname_dir(global->path.core_specific_config, settings->libretro,
         ".cfg", sizeof(global->path.core_specific_config));

   if (settings->core_specific_config)
   {
      char tmp[PATH_MAX_LENGTH] = {0};

      /* Toggle has_save_path to false so it resets */
      global->has_set.save_path = false;
      global->has_set.state_path = false;

      strlcpy(tmp, settings->libretro, sizeof(tmp));
      RARCH_LOG("Config: loading core-specific config from: %s.\n",
            global->path.core_specific_config);

      if (!config_load_file(global->path.core_specific_config, true))
         RARCH_WARN("Config: core-specific config not found, reusing last config.\n");

      /* Force some parameters which are implied when using core specific configs.
       * Don't have the core config file overwrite the libretro path. */
      strlcpy(settings->libretro, tmp, sizeof(settings->libretro));

      /* This must be true for core specific configs. */
      settings->core_specific_config = true;

      /* Reset save paths */
      global->has_set.save_path = true;
      global->has_set.state_path = true;
   }
}

/**
 * config_load_override:
 *
 * Tries to append game-specific and core-specific configuration.
 * These settings will always have precedence, thus this feature
 * can be used to enforce overrides.
 *
 * This function only has an effect if a game-specific or core-specific
 * configuration file exists at respective locations.
 *
 * core-specific: $CONFIG_DIR/$CORE_NAME/$CORE_NAME.cfg fallback: $CURRENT_CFG_LOCATION/$CORE_NAME/$CORE_NAME.cfg
 * game-specific: $CONFIG_DIR/$CORE_NAME/$ROM_NAME.cfg fallback: $CURRENT_CFG_LOCATION/$CORE_NAME/$GAME_NAME.cfg
 *
 * Returns: false if there was an error or no action was performed.
 *
 */
bool config_load_override(void)
{
   char buf[PATH_MAX_LENGTH];
   char config_directory[PATH_MAX_LENGTH];
   char core_path[PATH_MAX_LENGTH];
   char game_path[PATH_MAX_LENGTH];
   config_file_t *new_conf                = NULL;
   const char *core_name                  = NULL;
   const char *game_name                  = NULL;
   bool should_append                     = false;
   global_t *global                       = global_get_ptr();
   settings_t *settings                   = config_get_ptr();
   rarch_system_info_t *system            = NULL;
   
   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

   if (system)
      core_name = system->info.library_name;
   if (global)
      game_name = path_basename(global->name.base);

   if (string_is_empty(core_name) || string_is_empty(game_name))
      return false;

   /* Config directory: config_directory.
    * Try config directory setting first,
    * fallback to the location of the current configuration file. */
   if (!string_is_empty(settings->menu_config_directory))
      strlcpy(config_directory,
            settings->menu_config_directory, sizeof(config_directory));
   else if (!string_is_empty(global->path.config))
      fill_pathname_basedir(config_directory,
            global->path.config, sizeof(config_directory));
   else
      return false;

   /* Concatenate strings into full paths for core_path, game_path */
   fill_pathname_join(game_path,
         config_directory, core_name, sizeof(game_path));
   fill_string_join(game_path, game_name, sizeof(game_path));
   strlcat(game_path, ".cfg", sizeof(game_path));

   fill_pathname_join(core_path,
         config_directory, core_name, sizeof(core_path));
   fill_string_join(core_path, core_name, sizeof(core_path));
   strlcat(core_path, ".cfg", sizeof(core_path));


   /* Create a new config file from core_path */
   new_conf = config_file_new(core_path);

   /* If a core override exists, add its location to append_config_path */
   if (new_conf)
   {
      config_file_free(new_conf);

      if (settings->core_specific_config)
      {
         RARCH_LOG("Overrides: can't use overrides with with per-core configs, disabling overrides\n");
         return false;
      }
      RARCH_LOG("Overrides: core-specific overrides found at %s\n", core_path);
      strlcpy(global->path.append_config, core_path, sizeof(global->path.append_config));

      should_append = true;
   }
   else
      RARCH_LOG("Overrides: no core-specific overrides found at %s\n", core_path);

   /* Create a new config file from game_path */
   new_conf = config_file_new(game_path);

   /* If a game override exists, add it's location to append_config_path */
   if (new_conf)
   {
      config_file_free(new_conf);

      RARCH_LOG("Overrides: game-specific overrides found at %s\n", game_path);
      if (should_append)
      {
         strlcat(global->path.append_config, "|", sizeof(global->path.append_config));
         strlcat(global->path.append_config, game_path, sizeof(global->path.append_config));
      }
      else
         strlcpy(global->path.append_config, game_path, sizeof(global->path.append_config));

      should_append = true;
   }
   else
      RARCH_LOG("Overrides: no game-specific overrides found at %s\n", game_path);

   if (!should_append)
      return false;

   /* Re-load the configuration with any overrides that might have been found */

   if (settings->core_specific_config)
   {
      RARCH_LOG("Overrides: can't use overrides with with per-core configs, disabling overrides\n");
      return false;
   }

#ifdef HAVE_NETPLAY
   if (global->netplay.enable)
   {
      RARCH_WARN("Overrides: can't use overrides in conjunction with netplay, disabling overrides\n");
      return false;
   }
#endif

   /* Store the libretro_path we're using since it will be overwritten by the override when reloading */
   strlcpy(buf, settings->libretro, sizeof(buf));

   /* Toggle has_save_path to false so it resets */
   global->has_set.save_path  = false;
   global->has_set.state_path = false;

   if (!config_load_file(global->path.config, false))
      return false;

   /* Restore the libretro_path we're using
    * since it will be overwritten by the override when reloading. */
   strlcpy(settings->libretro, buf, sizeof(settings->libretro));
   runloop_msg_queue_push("Configuration override loaded", 1, 100, true);

   /* Reset save paths */
   global->has_set.save_path  = true;
   global->has_set.state_path = true;
   return true;
}

/**
 * config_unload_override:
 *
 * Unloads configuration overrides if overrides are active.
 *
 *
 * Returns: false if there was an error.
 */
 bool config_unload_override(void)
{
   global_t *global     = global_get_ptr();

   if (!global)
      return false;

   *global->path.append_config = '\0';

   /* Toggle has_save_path to false so it resets */
   global->has_set.save_path  = false;
   global->has_set.state_path = false;

   if (config_load_file(global->path.config, false))
   {
      RARCH_LOG("Overrides: configuration overrides unloaded, original configuration restored\n");

      /* Reset save paths */
      global->has_set.save_path  = true;
      global->has_set.state_path = true;

      return true;
   }

   return false;
}

/**
 * config_load_remap:
 *
 * Tries to append game-specific and core-specific remap files.
 *
 * This function only has an effect if a game-specific or core-specific
 * configuration file exists at respective locations.
 *
 * core-specific: $REMAP_DIR/$CORE_NAME/$CORE_NAME.cfg
 * game-specific: $REMAP_DIR/$CORE_NAME/$GAME_NAME.cfg
 *
 * Returns: false if there was an error or no action was performed.
 */
bool config_load_remap(void)
{
   config_file_t *new_conf                 = NULL;
   const char *core_name                   = NULL;
   const char *game_name                   = NULL;
   char remap_directory[PATH_MAX_LENGTH]   = {0};    /* path to the directory containing retroarch.cfg (prefix)    */
   char core_path[PATH_MAX_LENGTH]         = {0};    /* final path for core-specific configuration (prefix+suffix) */
   char game_path[PATH_MAX_LENGTH]         = {0};    /* final path for game-specific configuration (prefix+suffix) */
   global_t *global                        = global_get_ptr();
   settings_t *settings                    = config_get_ptr();
   rarch_system_info_t *system             = NULL;
   
   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

   /* Early return in case a library isn't loaded or remapping is disabled */
   if (!system->info.library_name || string_is_equal(system->info.library_name,"No Core"))
      return false;

   core_name = system ? system->info.library_name : NULL;
   game_name = global ? path_basename(global->name.base) : NULL;

   if (string_is_empty(core_name) || string_is_empty(game_name))
      return false;

   RARCH_LOG("Remaps: core name: %s\n", core_name);
   RARCH_LOG("Remaps: game name: %s\n", game_name);

   /* Remap directory: remap_directory.
    * Try remap directory setting, no fallbacks defined */
   if (string_is_empty(settings->input_remapping_directory))
   {
      RARCH_WARN("Remaps: no remap directory set.\n");
      return false;
   }

   strlcpy(remap_directory, settings->input_remapping_directory, sizeof(remap_directory));
   RARCH_LOG("Remaps: remap directory: %s\n", remap_directory);

   /* Concatenate strings into full paths for core_path, game_path */
   fill_pathname_join(core_path, remap_directory, core_name, sizeof(core_path));
   fill_pathname_join(core_path, core_path, core_name, sizeof(core_path));
   strlcat(core_path, ".rmp", sizeof(core_path));

   fill_pathname_join(game_path, remap_directory, core_name, sizeof(game_path));
   fill_pathname_join(game_path, game_path, game_name, sizeof(game_path));
   strlcat(game_path, ".rmp", sizeof(game_path));

   /* Create a new config file from game_path */
   new_conf = config_file_new(game_path);

   /* If a game remap file exists, load it. */
   if (new_conf)
   {
      RARCH_LOG("Remaps: game-specific remap found at %s\n", game_path);
      if (input_remapping_load_file(new_conf, game_path))
      {
         runloop_msg_queue_push("Game remap file loaded", 1, 100, true);
         return true;
      }
   }
   else
   {
      RARCH_LOG("Remaps: no game-specific remap found at %s\n", game_path);
      *settings->input.remapping_path= '\0';
      input_remapping_set_defaults();
   }

   /* Create a new config file from core_path */
   new_conf = config_file_new(core_path);

   /* If a core remap file exists, load it. */
   if (new_conf)
   {
      RARCH_LOG("Remaps: core-specific remap found at %s\n", core_path);
      if (input_remapping_load_file(new_conf, core_path))
      {
         runloop_msg_queue_push("Core remap file loaded", 1, 100, true);
         return true;
      }
   }
   else
   {
      RARCH_LOG("Remaps: no core-specific remap found at %s\n", core_path);
      *settings->input.remapping_path= '\0';
      input_remapping_set_defaults();
   }

   new_conf = NULL;

   return false;
}

static void parse_config_file(void)
{
   global_t *global = global_get_ptr();
   bool ret = config_load_file((*global->path.config)
         ? global->path.config : NULL, false);

   if (*global->path.config)
   {
      RARCH_LOG("Config: loading config from: %s.\n", global->path.config);
   }
   else
   {
      RARCH_LOG("Loading default config.\n");
      if (*global->path.config)
         RARCH_LOG("Config: found default config: %s.\n", global->path.config);
   }

   if (ret)
      return;

   RARCH_ERR("Config: couldn't find config at path: \"%s\"\n",
         global->path.config);
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
   char key[64] = {0};
   char btn[64] = {0};

   fill_pathname_join_delim(key, prefix, base, '_', sizeof(key));

   input_keymaps_translate_rk_to_str(bind->key, btn, sizeof(btn));
   config_set_string(conf, key, btn);
}

static void save_keybind_hat(config_file_t *conf, const char *key,
      const struct retro_keybind *bind)
{
   char config[16]  = {0};
   unsigned hat     = GET_HAT(bind->joykey);
   const char *dir  = NULL;

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
         retro_assert(0);
         break;
   }

   snprintf(config, sizeof(config), "h%u%s", hat, dir);
   config_set_string(conf, key, config);
}

static void save_keybind_joykey(config_file_t *conf, const char *prefix,
      const char *base, const struct retro_keybind *bind, bool save_empty)
{
   char key[64] = {0};

   fill_pathname_join_delim(key, prefix, base, '_', sizeof(key));
   strlcat(key, "_btn", sizeof(key));

   if (bind->joykey == NO_BTN)
   {
       if (save_empty)
         config_set_string(conf, key, "nul");
   }
   else if (GET_HAT_DIR(bind->joykey))
      save_keybind_hat(conf, key, bind);
   else
      config_set_uint64(conf, key, bind->joykey);
}

static void save_keybind_axis(config_file_t *conf, const char *prefix,
      const char *base, const struct retro_keybind *bind, bool save_empty)
{
   char key[64]    = {0};
   unsigned axis   = 0;
   char dir        = '\0';

   fill_pathname_join_delim(key, prefix, base, '_', sizeof(key));
   strlcat(key, "_axis", sizeof(key));

   if (bind->joyaxis == AXIS_NONE)
   {
      if (save_empty)
         config_set_string(conf, key, "nul");
   }
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
      char config[16];
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
 * @kb                 : save keyboard binds
 *
 * Save a key binding to the config file.
 */
static void save_keybind(config_file_t *conf, const char *prefix,
      const char *base, const struct retro_keybind *bind, bool save_kb, bool save_empty)
{
   if (!bind->valid)
      return;
   if (save_kb)
      save_keybind_key(conf, prefix, base, bind);
   save_keybind_joykey(conf, prefix, base, bind, save_empty);
   save_keybind_axis(conf, prefix, base, bind, save_empty);
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

   for (i = 0; input_config_bind_map_get_valid(i); i++)
   {
      const char *prefix = input_config_get_prefix(user,
            input_config_bind_map_get_meta(i));

      if (prefix)
         save_keybind(conf, prefix, input_config_bind_map_get_base(i),
               &settings->input.binds[user][i], true, true);
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
   if (*global->path.core_specific_config &&
         settings->config_save_on_exit && settings->core_specific_config)
      config_save_file(global->path.core_specific_config);

   /* Flush out some states that could have been set by core environment variables */
   global->has_set.input_descriptors = false;

   if (!rarch_ctl(RARCH_CTL_IS_BLOCK_CONFIG_READ, NULL))
   {
      config_set_defaults();
      parse_config_file();
   }

   /* Per-core config handling. */
   config_load_core_specific();
}

#if 0
/**
 * config_save_keybinds_file:
 * @path            : Path that shall be written to.
 *
 * Writes a keybinds config file to disk.
 *
 * Returns: true (1) on success, otherwise returns false (0).
 **/
static bool config_save_keybinds_file(const char *path)
{
   unsigned          i = 0;
   bool            ret = false;
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
#endif


/**
 * config_save_autoconf_profile:
 * @path            : Path that shall be written to.
 * @user              : Controller number to save
 * Writes a controller autoconf file to disk.
 **/
bool config_save_autoconf_profile(const char *path, unsigned user)
{
   unsigned i;
   int ret = false;
   char buf[PATH_MAX_LENGTH]            = {0};
   char autoconf_file[PATH_MAX_LENGTH]  = {0};
   config_file_t *conf                  = NULL;
   settings_t *settings                 = config_get_ptr();

   fill_pathname_join(buf, settings->input.autoconfig_dir,
         settings->input.joypad_driver, sizeof(buf));

   if(path_is_directory(buf))
   {
      fill_pathname_join(buf, buf,
            path, sizeof(buf));
      fill_pathname_noext(autoconf_file, buf, ".cfg", sizeof(autoconf_file));
   }
   else
   {
      fill_pathname_join(buf, settings->input.autoconfig_dir,
            path, sizeof(buf));
      fill_pathname_noext(autoconf_file, buf, ".cfg", sizeof(autoconf_file));
   }

   conf  = config_file_new(autoconf_file);

   if (!conf)
   {
      conf = config_file_new(NULL);
      if (!conf)
         return false;
   }

   config_set_string(conf, "input_driver", settings->input.joypad_driver);
   config_set_string(conf, "input_device", settings->input.device_names[user]);

   if(settings->input.vid[user] && settings->input.pid[user])
   {
      config_set_int(conf, "input_vendor_id", settings->input.vid[user]);
      config_set_int(conf, "input_product_id", settings->input.pid[user]);
   }

   for (i = 0; i < RARCH_FIRST_META_KEY; i++)
   {
      save_keybind(conf, "input", input_config_bind_map_get_base(i),
            &settings->input.binds[user][i], false, false);
   }
   ret = config_file_write(conf, autoconf_file);
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
   unsigned i           = 0;
   bool ret             = false;
   config_file_t *conf  = config_file_new(path);
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (!conf)
      conf = config_file_new(NULL);

   if (!conf || runloop_ctl(RUNLOOP_CTL_IS_OVERRIDES_ACTIVE, NULL))
      return false;

   RARCH_LOG("Saving config at path: \"%s\"\n", path);

   config_set_int(conf, "input_turbo_period", settings->input.turbo_period);
   config_set_int(conf, "input_duty_cycle", settings->input.turbo_duty_cycle);
   config_set_int(conf, "input_max_users", settings->input.max_users);
   config_set_int(conf, "input_menu_toggle_gamepad_combo", settings->input.menu_toggle_gamepad_combo);
   config_set_float(conf, "input_axis_threshold",
         settings->input.axis_threshold);
   config_set_bool(conf, "ui_companion_start_on_boot", settings->ui.companion_start_on_boot);
   config_set_bool(conf, "ui_companion_enable", settings->ui.companion_enable);
   config_set_bool(conf, "video_gpu_record", settings->video.gpu_record);
   config_set_bool(conf, "input_remap_binds_enable",
         settings->input.remap_binds_enable);
   config_set_bool(conf, "back_as_menu_toggle_enable",
         settings->input.back_as_menu_toggle_enable);
   config_set_bool(conf, "netplay_client_swap_input",
         settings->input.netplay_client_swap_input);
   config_set_bool(conf, "input_descriptor_label_show",
         settings->input.input_descriptor_label_show);
   config_set_bool(conf, "input_descriptor_hide_unbound",
         settings->input.input_descriptor_hide_unbound);
   config_set_bool(conf,  "load_dummy_on_core_shutdown",
         settings->load_dummy_on_core_shutdown);
   config_set_bool(conf,  "builtin_mediaplayer_enable",
         settings->multimedia.builtin_mediaplayer_enable);
   config_set_bool(conf,  "builtin_imageviewer_enable",
         settings->multimedia.builtin_imageviewer_enable);
   config_set_bool(conf,  "fps_show", settings->fps_show);
   config_set_bool(conf,  "ui_menubar_enable", settings->ui.menubar_enable);
   config_set_path(conf,  "libretro_path", settings->libretro);
   config_set_path(conf,  "core_options_path", settings->core_options_path);

   config_set_path(conf,  "recording_output_directory", global->record.output_dir);
   config_set_path(conf,  "recording_config_directory", global->record.config_dir);

   config_set_bool(conf,  "suspend_screensaver_enable", settings->ui.suspend_screensaver_enable);
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
   config_set_bool(conf,  "bundle_assets_extract_enable", settings->bundle_assets_extract_enable);
   config_set_int(conf,  "bundle_assets_extract_version_current", settings->bundle_assets_extract_version_current);
   config_set_int(conf,  "bundle_assets_extract_last_version", settings->bundle_assets_extract_last_version);
   config_set_string(conf,  "bundle_assets_src_path", settings->bundle_assets_src_path);
   config_set_string(conf,  "bundle_assets_dst_path", settings->bundle_assets_dst_path);
   config_set_string(conf,  "bundle_assets_dst_path_subdir", settings->bundle_assets_dst_path_subdir);
   config_set_string(conf,  "playlist_names", settings->playlist_names);
   config_set_string(conf,  "playlist_cores", settings->playlist_cores);
   config_set_float(conf, "video_refresh_rate", settings->video.refresh_rate);
   config_set_int(conf,   "video_monitor_index",
         settings->video.monitor_index);
   config_set_int(conf,    "video_fullscreen_x", settings->video.fullscreen_x);
   config_set_int(conf,    "video_fullscreen_y", settings->video.fullscreen_y);
   config_set_string(conf, "video_driver", settings->video.driver);
   config_set_string(conf, "record_driver", settings->record.driver);
   config_set_string(conf, "camera_driver", settings->camera.driver);
   config_set_string(conf, "location_driver", settings->location.driver);
#ifdef HAVE_MENU
#ifdef HAVE_THREADS
   config_set_bool(conf,"threaded_data_runloop_enable",
         settings->threaded_data_runloop_enable);
#endif

   config_set_bool(conf, "dpi_override_enable", settings->menu.dpi.override_enable);
   config_set_int (conf, "dpi_override_value", settings->menu.dpi.override_value);
   config_set_string(conf,"menu_driver", settings->menu.driver);
   config_set_bool(conf,"menu_pause_libretro", settings->menu.pause_libretro);
   config_set_bool(conf,"menu_mouse_enable", settings->menu.mouse.enable);
   config_set_bool(conf,"menu_pointer_enable", settings->menu.pointer.enable);
   config_set_bool(conf,"menu_timedate_enable", settings->menu.timedate_enable);
   config_set_bool(conf,"menu_core_enable", settings->menu.core_enable);
   config_set_bool(conf,"menu_dynamic_wallpaper_enable",
         settings->menu.dynamic_wallpaper_enable);
   config_set_bool(conf,"menu_boxart_enable", settings->menu.boxart_enable);
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
   config_set_string(conf, "core_updater_buildbot_url",
         settings->network.buildbot_url);
   config_set_string(conf, "core_updater_buildbot_assets_url",
         settings->network.buildbot_assets_url);
   config_set_bool(conf, "core_updater_auto_extract_archive",
         settings->network.buildbot_auto_extract_archive);
   config_set_string(conf, "camera_device", settings->camera.device);
   config_set_bool(conf, "camera_allow", settings->camera.allow);

#ifdef HAVE_CHEEVOS
   config_set_bool(conf, "cheevos_enable", settings->cheevos.enable);
   config_set_bool(conf, "cheevos_test_unofficial", settings->cheevos.test_unofficial);
   config_set_string(conf, "cheevos_username", settings->cheevos.username);
   config_set_string(conf, "cheevos_password", settings->cheevos.password);
#endif

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

   if (!global->has_set.ups_pref)
      config_set_bool(conf, "ups_pref", global->patch.ups_pref);
   if (!global->has_set.bps_pref)
      config_set_bool(conf, "bps_pref", global->patch.bps_pref);
   if (!global->has_set.ips_pref)
      config_set_bool(conf, "ips_pref", global->patch.ips_pref);

   config_set_path(conf, "system_directory",
         *settings->system_directory ?
         settings->system_directory : "default");
   config_set_path(conf, "cache_directory",
         settings->cache_directory);
   config_set_path(conf, "input_remapping_directory",
         settings->input_remapping_directory);
   config_set_path(conf, "input_remapping_path",
        settings->input.remapping_path);
   config_set_path(conf, "resampler_directory",
         settings->resampler_directory);
   config_set_string(conf, "audio_resampler", settings->audio.resampler);
   config_set_path(conf, "savefile_directory",
         *global->dir.savefile ? global->dir.savefile : "default");
   config_set_path(conf, "savestate_directory",
         *global->dir.savestate ? global->dir.savestate : "default");
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
   config_set_path(conf, "dynamic_wallpapers_directory",
         *settings->dynamic_wallpapers_directory ?
         settings->dynamic_wallpapers_directory : "default");
   config_set_path(conf, "boxarts_directory",
         *settings->boxarts_directory ?
         settings->boxarts_directory : "default");
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
   config_set_bool(conf, "menu_navigation_wraparound_enable",
         settings->menu.navigation.wraparound.enable);
   config_set_bool(conf,
         "menu_navigation_browser_filter_supported_extensions_enable",
         settings->menu.navigation.browser.filter.supported_extensions_enable);
   config_set_bool(conf, "menu_show_advanced_settings",
         settings->menu.show_advanced_settings);
   config_set_hex(conf, "menu_entry_normal_color",
         settings->menu.entry_normal_color);
   config_set_hex(conf, "menu_entry_hover_color",
         settings->menu.entry_hover_color);
   config_set_hex(conf, "menu_title_color",
         settings->menu.title_color);
#endif

   config_set_path(conf, "content_history_path", settings->content_history_path);
   config_set_int(conf, "content_history_size", settings->content_history_size);
   config_set_path(conf, "joypad_autoconfig_dir",
         settings->input.autoconfig_dir);
   config_set_bool(conf, "input_autodetect_enable",
         settings->input.autodetect_enable);

#ifdef HAVE_OVERLAY
   config_set_path(conf, "overlay_directory",
         *settings->overlay_directory ? settings->overlay_directory : "default");
   config_set_path(conf, "input_overlay", settings->input.overlay);
   config_set_bool(conf, "input_overlay_enable", settings->input.overlay_enable);
   config_set_bool(conf, "input_overlay_enable_autopreferred", settings->input.overlay_enable_autopreferred);
   config_set_bool(conf, "input_overlay_hide_in_menu", settings->input.overlay_hide_in_menu);
   config_set_float(conf, "input_overlay_opacity",
         settings->input.overlay_opacity);
   config_set_float(conf, "input_overlay_scale",
         settings->input.overlay_scale);

   config_set_path(conf, "osk_overlay_directory",
         *global->dir.osk_overlay ? global->dir.osk_overlay : "default");
   config_set_path(conf, "input_osk_overlay", settings->osk.overlay);
   config_set_bool(conf, "input_osk_overlay_enable", settings->osk.enable);
#endif

   config_set_path(conf, "video_font_path", settings->video.font_path);
   config_set_float(conf, "video_message_pos_x", settings->video.msg_pos_x);
   config_set_float(conf, "video_message_pos_y", settings->video.msg_pos_y);

   config_set_int(conf, "custom_viewport_width",
         settings->video_viewport_custom.width);
   config_set_int(conf, "custom_viewport_height",
         settings->video_viewport_custom.height);
   config_set_int(conf, "custom_viewport_x",
         settings->video_viewport_custom.x);
   config_set_int(conf, "custom_viewport_y",
         settings->video_viewport_custom.y);

   video_driver_ctl(RARCH_DISPLAY_CTL_SAVE_SETTINGS, conf);

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

#ifdef HAVE_COMMAND
   config_set_bool(conf, "network_cmd_enable",
         settings->network_cmd_enable);
   config_set_bool(conf, "stdin_cmd_enable",
         settings->stdin_cmd_enable);
   config_set_int(conf, "network_cmd_port",
         settings->network_cmd_port);
#endif

   config_set_float(conf, "fastforward_ratio", settings->fastforward_ratio);
   config_set_float(conf, "slowmotion_ratio", settings->slowmotion_ratio);

   config_set_bool(conf, "config_save_on_exit",
         settings->config_save_on_exit);
   config_set_int(conf, "state_slot", settings->state_slot);

#ifdef HAVE_NETPLAY
   config_set_bool(conf, "netplay_spectator_mode_enable",
         global->netplay.is_spectate);
   config_set_bool(conf, "netplay_mode", global->netplay.is_client);
   config_set_string(conf, "netplay_ip_address", global->netplay.server);
   config_set_int(conf, "netplay_ip_port", global->netplay.port);
   config_set_int(conf, "netplay_delay_frames", global->netplay.sync_frames);
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
      char cfg[64] = {0};

      snprintf(cfg, sizeof(cfg), "input_device_p%u", i + 1);
      config_set_int(conf, cfg, settings->input.device[i]);
      snprintf(cfg, sizeof(cfg), "input_player%u_joypad_index", i + 1);
      config_set_int(conf, cfg, settings->input.joypad_map[i]);
      snprintf(cfg, sizeof(cfg), "input_libretro_device_p%u", i + 1);
      config_set_int(conf, cfg, settings->input.libretro_device[i]);
      snprintf(cfg, sizeof(cfg), "input_player%u_analog_dpad_mode", i + 1);
      config_set_int(conf, cfg, settings->input.analog_dpad_mode[i]);
   }

#ifdef HAVE_NETWORK_GAMEPAD
   for (i = 0; i < MAX_USERS; i++)
   {
      char tmp[64] = {0};
      snprintf(tmp, sizeof(tmp), "network_remote_enable_user_p%u", i + 1);
      config_set_bool(conf, tmp, settings->network_remote_enable_user[i]);
   }
   config_set_bool(conf, "network_remote_enable", settings->network_remote_enable);
   config_set_int(conf, "network_remote_base_port", settings->network_remote_base_port);

#endif
   for (i = 0; i < MAX_USERS; i++)
      save_keybinds_user(conf, i);

   config_set_bool(conf, "core_specific_config",
         settings->core_specific_config);
   config_set_bool(conf, "game_specific_options",
         settings->game_specific_options);
   config_set_bool(conf, "auto_overrides_enable",
         settings->auto_overrides_enable);
   config_set_bool(conf, "auto_remaps_enable",
         settings->auto_remaps_enable);
   config_set_bool(conf, "sort_savefiles_enable",
         settings->sort_savefiles_enable);
   config_set_bool(conf, "sort_savestates_enable",
         settings->sort_savestates_enable);
   config_set_int(conf, "libretro_log_level", settings->libretro_log_level);
   config_set_bool(conf, "log_verbosity", *retro_main_verbosity());

   {
      bool perfcnt_enable = runloop_ctl(RUNLOOP_CTL_IS_PERFCNT_ENABLE, NULL);
      config_set_bool(conf, "perfcnt_enable", perfcnt_enable);
   }

#if TARGET_OS_IPHONE
   config_set_bool(conf, "small_keyboard_enable",   settings->input.small_keyboard_enable);
#endif
   config_set_bool(conf, "keyboard_gamepad_enable", settings->input.keyboard_gamepad_enable);
   config_set_int(conf, "keyboard_gamepad_mapping_type", settings->input.keyboard_gamepad_mapping_type);

   config_set_bool(conf, "core_set_supports_no_game_enable",
         settings->set_supports_no_game_enable);

   config_set_int(conf, "menu_ok_btn",          settings->menu_ok_btn);
   config_set_int(conf, "menu_cancel_btn",      settings->menu_cancel_btn);
   config_set_int(conf, "menu_search_btn",      settings->menu_search_btn);
   config_set_int(conf, "menu_info_btn",        settings->menu_info_btn);
   config_set_int(conf, "menu_default_btn",     settings->menu_default_btn);
   config_set_int(conf, "menu_scroll_down_btn", settings->menu_scroll_down_btn);
   config_set_int(conf, "menu_scroll_up_btn",   settings->menu_scroll_up_btn);

   ret = config_file_write(conf, path);
   config_file_free(conf);
   return ret;
}
