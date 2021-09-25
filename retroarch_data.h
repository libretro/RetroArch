#define RARCH_TIMER_TICK(_timer, current_time) \
   _timer.current    = current_time; \
   _timer.timeout_us = (_timer.timeout_end - _timer.current) \

#define RARCH_TIMER_END(_timer) \
   _timer.timer_end   = true; \
   _timer.timer_begin = false; \
   _timer.timeout_end = 0

#define RARCH_TIMER_BEGIN_NEW_TIME_USEC(_timer, current_usec, timeout_usec) \
   _timer.timeout_us  = timeout_usec; \
   _timer.current     = current_usec; \
   _timer.timeout_end = _timer.current + _timer.timeout_us

#define RARCH_TIMER_GET_TIMEOUT(_timer) ((int)(_timer.timeout_us / 1000000))

#define RARCH_TIMER_HAS_EXPIRED(_timer) ((_timer.timeout_us <= 0) ? true : false)

#define DRIVERS_CMD_ALL \
      ( DRIVER_AUDIO_MASK \
      | DRIVER_VIDEO_MASK \
      | DRIVER_INPUT_MASK \
      | DRIVER_CAMERA_MASK \
      | DRIVER_LOCATION_MASK \
      | DRIVER_MENU_MASK \
      | DRIVERS_VIDEO_INPUT_MASK \
      | DRIVER_BLUETOOTH_MASK \
      | DRIVER_WIFI_MASK \
      | DRIVER_LED_MASK \
      | DRIVER_MIDI_MASK )

#define DRIVERS_CMD_ALL_BUT_MENU \
      ( DRIVER_AUDIO_MASK \
      | DRIVER_VIDEO_MASK \
      | DRIVER_INPUT_MASK \
      | DRIVER_CAMERA_MASK \
      | DRIVER_LOCATION_MASK \
      | DRIVERS_VIDEO_INPUT_MASK \
      | DRIVER_BLUETOOTH_MASK \
      | DRIVER_WIFI_MASK \
      | DRIVER_LED_MASK \
      | DRIVER_MIDI_MASK )


#define _PSUPP(var, name, desc) printf("  %s:\n\t\t%s: %s\n", name, desc, var ? "yes" : "no")

#define FAIL_CPU(p_rarch, simd_type) do { \
   RARCH_ERR(simd_type " code is compiled in, but CPU does not support this feature. Cannot continue.\n"); \
   retroarch_fail(p_rarch, 1, "validate_cpu_features()"); \
} while (0)

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "zip"
#else
#define DEFAULT_EXT ""
#endif

#define SHADER_FILE_WATCH_DELAY_MSEC 500

#define QUIT_DELAY_USEC 3 * 1000000 /* 3 seconds */

#define DEBUG_INFO_FILENAME "debug_info.txt"

#define MAGIC_INDEX        0
#define SERIALIZER_INDEX   1
#define CRC_INDEX          2
#define STATE_SIZE_INDEX   3

#ifdef HAVE_BSV_MOVIE
#define BSV_MAGIC          0x42535631

#define BSV_MOVIE_IS_PLAYBACK_ON() (p_rarch->bsv_movie_state_handle && p_rarch->bsv_movie_state.movie_playback)
#define BSV_MOVIE_IS_PLAYBACK_OFF() (p_rarch->bsv_movie_state_handle && !p_rarch->bsv_movie_state.movie_playback)
#endif

#define MEASURE_FRAME_TIME_SAMPLES_COUNT (2 * 1024)

#define TIME_TO_FPS(last_time, new_time, frames) ((1000000.0f * (frames)) / ((new_time) - (last_time)))

#define AUDIO_BUFFER_FREE_SAMPLES_COUNT (8 * 1024)

#define MENU_SOUND_FORMATS "ogg|mod|xm|s3m|mp3|flac|wav"

/**
 * db_to_gain:
 * @db          : Decibels.
 *
 * Converts decibels to voltage gain.
 *
 * Returns: voltage gain value.
 **/
#define DB_TO_GAIN(db) (powf(10.0f, (db) / 20.0f))

#define DEFAULT_NETWORK_GAMEPAD_PORT 55400
#define UDP_FRAME_PACKETS 16

#ifdef HAVE_THREADS
#define VIDEO_DRIVER_IS_THREADED_INTERNAL() ((!video_driver_is_hw_context() && p_rarch->video_driver_threaded) ? true : false)
#else
#define VIDEO_DRIVER_IS_THREADED_INTERNAL() (false)
#endif

#ifdef HAVE_THREADS
#define VIDEO_DRIVER_LOCK() \
   if (p_rarch->display_lock) \
      slock_lock(p_rarch->display_lock)

#define VIDEO_DRIVER_UNLOCK() \
   if (p_rarch->display_lock) \
      slock_unlock(p_rarch->display_lock)

#define VIDEO_DRIVER_CONTEXT_LOCK() \
   if (p_rarch->context_lock) \
      slock_lock(p_rarch->context_lock)

#define VIDEO_DRIVER_CONTEXT_UNLOCK() \
   if (p_rarch->context_lock) \
      slock_unlock(p_rarch->context_lock)

#define VIDEO_DRIVER_LOCK_FREE() \
   slock_free(p_rarch->display_lock); \
   slock_free(p_rarch->context_lock); \
   p_rarch->display_lock = NULL; \
   p_rarch->context_lock = NULL

#define VIDEO_DRIVER_THREADED_LOCK(is_threaded) \
   if (is_threaded) \
      VIDEO_DRIVER_LOCK()

#define VIDEO_DRIVER_THREADED_UNLOCK(is_threaded) \
   if (is_threaded) \
      VIDEO_DRIVER_UNLOCK()
#else
#define VIDEO_DRIVER_LOCK()            ((void)0)
#define VIDEO_DRIVER_UNLOCK()          ((void)0)
#define VIDEO_DRIVER_LOCK_FREE()       ((void)0)
#define VIDEO_DRIVER_THREADED_LOCK(is_threaded)   ((void)0)
#define VIDEO_DRIVER_THREADED_UNLOCK(is_threaded) ((void)0)
#define VIDEO_DRIVER_CONTEXT_LOCK()    ((void)0)
#define VIDEO_DRIVER_CONTEXT_UNLOCK()  ((void)0)
#endif

#ifdef HAVE_THREADS
#define VIDEO_DRIVER_GET_PTR_INTERNAL(p_rarch) ((VIDEO_DRIVER_IS_THREADED_INTERNAL()) ? video_thread_get_ptr(p_rarch) : p_rarch->video_driver_data)
#else
#define VIDEO_DRIVER_GET_PTR_INTERNAL(p_rarch) (p_rarch->video_driver_data)
#endif

#define VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(p_rarch) (&p_rarch->hw_render)

#ifdef HAVE_BSV_MOVIE
#define BSV_MOVIE_IS_EOF(p_rarch) || (p_rarch->bsv_movie_state.movie_end && p_rarch->bsv_movie_state.eof_exit)
#else
#define BSV_MOVIE_IS_EOF(p_rarch)
#endif

/* Depends on ASCII character values */
#define ISPRINT(c) (((int)(c) >= ' ' && (int)(c) <= '~') ? 1 : 0)

#define VIDEO_HAS_FOCUS(p_rarch) (p_rarch->current_video->focus ? (p_rarch->current_video->focus(p_rarch->video_driver_data)) : true)

#if HAVE_DYNAMIC
#define RUNAHEAD_RUN_SECONDARY(p_rarch) \
   if (!secondary_core_run_use_last_input(p_rarch)) \
      p_rarch->runahead_secondary_core_available = false
#endif

#define RUNAHEAD_RESUME_VIDEO(p_rarch) \
   if (p_rarch->runahead_video_driver_is_active) \
      p_rarch->video_driver_active = true; \
   else \
      p_rarch->video_driver_active = false

#define _PSUPP_BUF(buf, var, name, desc) \
   strlcat(buf, "  ", sizeof(buf)); \
   strlcat(buf, name, sizeof(buf)); \
   strlcat(buf, ":\n\t\t", sizeof(buf)); \
   strlcat(buf, desc, sizeof(buf)); \
   strlcat(buf, ": ", sizeof(buf)); \
   strlcat(buf, var ? "yes\n" : "no\n", sizeof(buf))

#define HOTKEY_CHECK(cmd1, cmd2, cond, cond2) \
   { \
      static bool old_pressed                   = false; \
      bool pressed                              = BIT256_GET(current_bits, cmd1); \
      if (pressed && !old_pressed) \
         if (cond) \
            command_event(cmd2, cond2); \
      old_pressed                               = pressed; \
   }

#define HOTKEY_CHECK3(cmd1, cmd2, cmd3, cmd4, cmd5, cmd6) \
   { \
      static bool old_pressed                   = false; \
      static bool old_pressed2                  = false; \
      static bool old_pressed3                  = false; \
      bool pressed                              = BIT256_GET(current_bits, cmd1); \
      bool pressed2                             = BIT256_GET(current_bits, cmd3); \
      bool pressed3                             = BIT256_GET(current_bits, cmd5); \
      if (pressed && !old_pressed) \
         command_event(cmd2, (void*)(intptr_t)0); \
      else if (pressed2 && !old_pressed2) \
         command_event(cmd4, (void*)(intptr_t)0); \
      else if (pressed3 && !old_pressed3) \
         command_event(cmd6, (void*)(intptr_t)0); \
      old_pressed                               = pressed; \
      old_pressed2                              = pressed2; \
      old_pressed3                              = pressed3; \
   }

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
#define INPUT_REMOTE_KEY_PRESSED(p_rarch, key, port) (p_rarch->remote_st_ptr.buttons[(port)] & (UINT64_C(1) << (key)))
#endif

/**
 * check_input_driver_block_hotkey:
 *
 * Checks if 'hotkey enable' key is pressed.
 *
 * If we haven't bound anything to this,
 * always allow hotkeys.

 * If we hold ENABLE_HOTKEY button, block all libretro input to allow
 * hotkeys to be bound to same keys as RetroPad.
 **/
#define CHECK_INPUT_DRIVER_BLOCK_HOTKEY(normal_bind, autoconf_bind) \
( \
         (((normal_bind)->key      != RETROK_UNKNOWN) \
      || ((normal_bind)->mbutton   != NO_BTN) \
      || ((normal_bind)->joykey    != NO_BTN) \
      || ((normal_bind)->joyaxis   != AXIS_NONE) \
      || ((autoconf_bind)->key     != RETROK_UNKNOWN) \
      || ((autoconf_bind)->joykey  != NO_BTN) \
      || ((autoconf_bind)->joyaxis != AXIS_NONE)) \
)

#define INHERIT_JOYAXIS(binds) (((binds)[x_plus].joyaxis == (binds)[x_minus].joyaxis) || (  (binds)[y_plus].joyaxis == (binds)[y_minus].joyaxis))

#define MAPPER_GET_KEY(state, key) (((state)->keys[(key) / 32] >> ((key) % 32)) & 1)
#define MAPPER_SET_KEY(state, key) (state)->keys[(key) / 32] |= 1 << ((key) % 32)
#define MAPPER_UNSET_KEY(state, key) (state)->keys[(key) / 32] &= ~(1 << ((key) % 32))

#define CDN_URL "https://cdn.discordapp.com/avatars"

#ifdef HAVE_DYNAMIC
#define SYMBOL(x) do { \
   function_t func = dylib_proc(lib_handle_local, #x); \
   memcpy(&current_core->x, &func, sizeof(func)); \
   if (!current_core->x) { RARCH_ERR("Failed to load symbol: \"%s\"\n", #x); retroarch_fail(p_rarch, 1, "init_libretro_symbols()"); } \
} while (0)
#else
#define SYMBOL(x) current_core->x = x
#endif

#define SYMBOL_DUMMY(x) current_core->x = libretro_dummy_##x

#ifdef HAVE_FFMPEG
#define SYMBOL_FFMPEG(x) current_core->x = libretro_ffmpeg_##x
#endif

#ifdef HAVE_MPV
#define SYMBOL_MPV(x) current_core->x = libretro_mpv_##x
#endif

#ifdef HAVE_IMAGEVIEWER
#define SYMBOL_IMAGEVIEWER(x) current_core->x = libretro_imageviewer_##x
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
#define SYMBOL_NETRETROPAD(x) current_core->x = libretro_netretropad_##x
#endif

#if defined(HAVE_VIDEOPROCESSOR)
#define SYMBOL_VIDEOPROCESSOR(x) current_core->x = libretro_videoprocessor_##x
#endif

#ifdef HAVE_GONG
#define SYMBOL_GONG(x) current_core->x = libretro_gong_##x
#endif

#define CORE_SYMBOLS(x) \
            x(retro_init); \
            x(retro_deinit); \
            x(retro_api_version); \
            x(retro_get_system_info); \
            x(retro_get_system_av_info); \
            x(retro_set_environment); \
            x(retro_set_video_refresh); \
            x(retro_set_audio_sample); \
            x(retro_set_audio_sample_batch); \
            x(retro_set_input_poll); \
            x(retro_set_input_state); \
            x(retro_set_controller_port_device); \
            x(retro_reset); \
            x(retro_run); \
            x(retro_serialize_size); \
            x(retro_serialize); \
            x(retro_unserialize); \
            x(retro_cheat_reset); \
            x(retro_cheat_set); \
            x(retro_load_game); \
            x(retro_load_game_special); \
            x(retro_unload_game); \
            x(retro_get_region); \
            x(retro_get_memory_data); \
            x(retro_get_memory_size);

#define FFMPEG_RECORD_ARG "r:"

#ifdef HAVE_DYNAMIC
#define DYNAMIC_ARG "L:"
#else
#define DYNAMIC_ARG
#endif

#ifdef HAVE_NETWORKING
#define NETPLAY_ARG "HC:F:"
#else
#define NETPLAY_ARG
#endif

#ifdef HAVE_CONFIGFILE
#define CONFIG_FILE_ARG "c:"
#else
#define CONFIG_FILE_ARG
#endif

#ifdef HAVE_BSV_MOVIE
#define BSV_MOVIE_ARG "P:R:M:"
#else
#define BSV_MOVIE_ARG
#endif

#ifdef HAVE_LIBNX
#define LIBNX_SWKBD_LIMIT 500 /* enforced by HOS */
#endif

/* Griffin hack */
#ifdef HAVE_QT
#ifndef HAVE_MAIN
#define HAVE_MAIN
#endif
#endif

#ifdef _WIN32
#define PERF_LOG_FMT "[PERF]: Avg (%s): %I64u ticks, %I64u runs.\n"
#else
#define PERF_LOG_FMT "[PERF]: Avg (%s): %llu ticks, %llu runs.\n"
#endif

/* DRIVERS */

audio_driver_t audio_null = {
   NULL, /* init */
   NULL, /* write */
   NULL, /* stop */
   NULL, /* start */
   NULL, /* alive */
   NULL, /* set_nonblock_state */
   NULL, /* free */
   NULL, /* use_float */
   "null",
   NULL,
   NULL,
   NULL, /* write_avail */
   NULL
};

static const audio_driver_t *audio_drivers[] = {
#ifdef HAVE_ALSA
   &audio_alsa,
#if !defined(__QNX__) && defined(HAVE_THREADS)
   &audio_alsathread,
#endif
#endif
#ifdef HAVE_TINYALSA
	&audio_tinyalsa,
#endif
#if defined(HAVE_AUDIOIO)
   &audio_audioio,
#endif
#if defined(HAVE_OSS) || defined(HAVE_OSS_BSD)
   &audio_oss,
#endif
#ifdef HAVE_RSOUND
   &audio_rsound,
#endif
#ifdef HAVE_COREAUDIO
   &audio_coreaudio,
#endif
#ifdef HAVE_COREAUDIO3
   &audio_coreaudio3,
#endif
#ifdef HAVE_AL
   &audio_openal,
#endif
#ifdef HAVE_SL
   &audio_opensl,
#endif
#ifdef HAVE_ROAR
   &audio_roar,
#endif
#ifdef HAVE_JACK
   &audio_jack,
#endif
#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   &audio_sdl,
#endif
#ifdef HAVE_XAUDIO
   &audio_xa,
#endif
#ifdef HAVE_DSOUND
   &audio_dsound,
#endif
#ifdef HAVE_WASAPI
   &audio_wasapi,
#endif
#ifdef HAVE_PULSE
   &audio_pulse,
#endif
#if defined(__PSL1GHT__) || defined(__PS3__)
   &audio_ps3,
#endif
#ifdef XENON
   &audio_xenon360,
#endif
#ifdef GEKKO
   &audio_gx,
#endif
#ifdef WIIU
   &audio_ax,
#endif
#ifdef EMSCRIPTEN
   &audio_rwebaudio,
#endif
#if defined(PSP) || defined(VITA) || defined(ORBIS)
  &audio_psp,
#endif
#if defined(PS2)
  &audio_ps2,
#endif
#ifdef _3DS
   &audio_ctr_csnd,
   &audio_ctr_dsp,
#ifdef HAVE_THREADS
   &audio_ctr_dsp_thread,
#endif
#endif
#ifdef SWITCH
   &audio_switch,
   &audio_switch_thread,
#ifdef HAVE_LIBNX
   &audio_switch_libnx_audren,
   &audio_switch_libnx_audren_thread,
#endif
#endif
   &audio_null,
   NULL,
};

static const video_display_server_t dispserv_null = {
   NULL, /* init */
   NULL, /* destroy */
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
   NULL, /* set_resolution */
   NULL, /* get_resolution_list */
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   NULL, /* get_flags */
   "null"
};

static void *video_null_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   *input       = NULL;
   *input_data = NULL;

   frontend_driver_install_signal_handler();

   return (void*)-1;
}

static bool video_null_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   return true;
}

static void video_null_free(void *data) { }
static void video_null_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }
static bool video_null_alive(void *data) { return frontend_driver_get_signal_handler_state() != 1; }
static bool video_null_focus(void *data) { return true; }
static bool video_null_has_windowed(void *data) { return true; }
static bool video_null_suppress_screensaver(void *data, bool enable) { return false; }
static bool video_null_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

static video_driver_t video_null = {
   video_null_init,
   video_null_frame,
   video_null_set_nonblock_state,
   video_null_alive,
   video_null_focus,
   video_null_suppress_screensaver,
   video_null_has_windowed,
   video_null_set_shader,
   video_null_free,
   "null",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   NULL, /* viewport_info */
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
   NULL,
#endif
  NULL, /* get_poke_interface */
};

static const video_driver_t *video_drivers[] = {
#ifdef __PSL1GHT__
   &video_gcm,
#endif
#ifdef HAVE_VITA2D
   &video_vita2d,
#endif
#ifdef HAVE_OPENGL
   &video_gl2,
#endif
#if defined(HAVE_OPENGL_CORE)
   &video_gl_core,
#endif
#ifdef HAVE_OPENGL1
   &video_gl1,
#endif
#ifdef HAVE_VULKAN
   &video_vulkan,
#endif
#ifdef HAVE_METAL
   &video_metal,
#endif
#ifdef XENON
   &video_xenon360,
#endif
#if defined(HAVE_D3D12)
   &video_d3d12,
#endif
#if defined(HAVE_D3D11)
   &video_d3d11,
#endif
#if defined(HAVE_D3D10)
   &video_d3d10,
#endif
#if defined(HAVE_D3D9)
   &video_d3d9,
#endif
#if defined(HAVE_D3D8)
   &video_d3d8,
#endif
#ifdef PSP
   &video_psp1,
#endif
#ifdef PS2
   &video_ps2,
#endif
#ifdef _3DS
   &video_ctr,
#endif
#ifdef SWITCH
   &video_switch,
#endif
#ifdef HAVE_ODROIDGO2
   &video_oga,
#endif
#if defined(HAVE_SDL) && !defined(HAVE_SDL_DINGUX)
   &video_sdl,
#endif
#ifdef HAVE_SDL2
   &video_sdl2,
#endif
#ifdef HAVE_SDL_DINGUX
#if defined(RS90)
   &video_sdl_rs90,
#else
   &video_sdl_dingux,
#endif
#endif
#ifdef HAVE_XVIDEO
   &video_xvideo,
#endif
#ifdef GEKKO
   &video_gx,
#endif
#ifdef WIIU
   &video_wiiu,
#endif
#ifdef HAVE_VG
   &video_vg,
#endif
#ifdef HAVE_OMAP
   &video_omap,
#endif
#ifdef HAVE_EXYNOS
   &video_exynos,
#endif
#ifdef HAVE_DISPMANX
   &video_dispmanx,
#endif
#ifdef HAVE_SUNXI
   &video_sunxi,
#endif
#ifdef HAVE_PLAIN_DRM
   &video_drm,
#endif
#ifdef HAVE_XSHM
   &video_xshm,
#endif
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
   &video_gdi,
#endif
#endif
#ifdef DJGPP
   &video_vga,
#endif
#ifdef HAVE_FPGA
   &video_fpga,
#endif
#ifdef HAVE_SIXEL
   &video_sixel,
#endif
#ifdef HAVE_CACA
   &video_caca,
#endif
#ifdef HAVE_NETWORK_VIDEO
   &video_network,
#endif
   &video_null,
   NULL,
};

#ifdef HAVE_VULKAN
static const gfx_ctx_driver_t *gfx_ctx_vk_drivers[] = {
#if defined(__APPLE__)
   &gfx_ctx_cocoavk,
#endif
#if defined(_WIN32) && !defined(__WINRT__)
   &gfx_ctx_w_vk,
#endif
#if defined(ANDROID)
   &gfx_ctx_vk_android,
#endif
#if defined(HAVE_WAYLAND)
   &gfx_ctx_vk_wayland,
#endif
#if defined(HAVE_X11)
   &gfx_ctx_vk_x,
#endif
#if defined(HAVE_VULKAN_DISPLAY)
   &gfx_ctx_khr_display,
#endif
   &gfx_ctx_null,
   NULL
};
#endif

static const gfx_ctx_driver_t *gfx_ctx_gl_drivers[] = {
#if defined(ORBIS)
   &orbis_ctx,
#endif
#if defined(HAVE_VITAGL) | defined(HAVE_VITAGLES)
   &vita_ctx,
#endif
#if !defined(__PSL1GHT__) && defined(__PS3__)
   &gfx_ctx_ps3,
#endif
#if defined(HAVE_LIBNX) && defined(HAVE_OPENGL)
   &switch_ctx,
#endif
#if defined(HAVE_VIDEOCORE)
   &gfx_ctx_videocore,
#endif
#if defined(HAVE_MALI_FBDEV)
   &gfx_ctx_mali_fbdev,
#endif
#if defined(HAVE_VIVANTE_FBDEV)
   &gfx_ctx_vivante_fbdev,
#endif
#if defined(HAVE_OPENDINGUX_FBDEV)
   &gfx_ctx_opendingux_fbdev,
#endif
#if defined(_WIN32) && !defined(__WINRT__) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE))
   &gfx_ctx_wgl,
#endif
#if defined(__WINRT__) && defined(HAVE_OPENGLES)
   &gfx_ctx_uwp,
#endif
#if defined(HAVE_WAYLAND)
   &gfx_ctx_wayland,
#endif
#if defined(HAVE_X11) && !defined(HAVE_OPENGLES)
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
   &gfx_ctx_x,
#endif
#endif
#if defined(HAVE_X11) && defined(HAVE_OPENGL) && defined(HAVE_EGL)
   &gfx_ctx_x_egl,
#endif
#if defined(HAVE_KMS)
#if defined(HAVE_ODROIDGO2)
   &gfx_ctx_go2_drm,
#endif
   &gfx_ctx_drm,
#endif
#if defined(ANDROID)
   &gfx_ctx_android,
#endif
#if defined(__QNX__)
   &gfx_ctx_qnx,
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH) || defined(HAVE_COCOA_METAL)
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   &gfx_ctx_cocoagl,
#endif
#endif
#if (defined(HAVE_SDL) || defined(HAVE_SDL2)) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE))
   &gfx_ctx_sdl_gl,
#endif
#ifdef HAVE_OSMESA
   &gfx_ctx_osmesa,
#endif
#ifdef EMSCRIPTEN
   &gfx_ctx_emscripten,
#endif
   &gfx_ctx_null,
   NULL
};

static bluetooth_driver_t bluetooth_null = {
   NULL, /* init */
   NULL, /* free */
   NULL, /* scan */
   NULL, /* get_devices */
   NULL, /* device_is_connected */
   NULL, /* device_get_sublabel */
   NULL, /* connect_device */
   "null",
};

static const bluetooth_driver_t *bluetooth_drivers[] = {
#ifdef HAVE_BLUETOOTH
   &bluetooth_bluetoothctl,
#ifdef HAVE_DBUS
   &bluetooth_bluez,
#endif
#endif
   &bluetooth_null,
   NULL,
};

static wifi_driver_t wifi_null = {
   NULL, /* init */
   NULL, /* free */
   NULL, /* start */
   NULL, /* stop */
   NULL, /* enable */
   NULL, /* connection_info */
   NULL, /* scan */
   NULL, /* get_ssids */
   NULL, /* ssid_is_online */
   NULL, /* connect_ssid */
   NULL, /* disconnect_ssid */
   NULL, /* tether_start_stop */
   "null",
};

static const wifi_driver_t *wifi_drivers[] = {
#ifdef HAVE_LAKKA
   &wifi_connmanctl,
#endif
#ifdef HAVE_WIFI
   &wifi_nmcli,
#endif
   &wifi_null,
   NULL,
};

static ui_companion_driver_t ui_companion_null = {
   NULL, /* init */
   NULL, /* deinit */
   NULL, /* toggle */
   NULL, /* event_command */
   NULL, /* notify_content_loaded */
   NULL, /* notify_list_loaded */
   NULL, /* notify_refresh */
   NULL, /* msg_queue_push */
   NULL, /* render_messagebox */
   NULL, /* get_main_window */
   NULL, /* log_msg */
   NULL, /* is_active */
   NULL, /* browser_window */
   NULL, /* msg_window */
   NULL, /* window */
   NULL, /* application */
   "null", /* ident */
};

static const ui_companion_driver_t *ui_companion_drivers[] = {
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   &ui_companion_win32,
#endif
#if defined(OSX)
   &ui_companion_cocoa,
#endif
   &ui_companion_null,
   NULL
};

static const record_driver_t record_null = {
   NULL, /* new */
   NULL, /* free */
   NULL, /* push_video */
   NULL, /* push_audio */
   NULL, /* finalize */
   "null",
};

static const record_driver_t *record_drivers[] = {
#ifdef HAVE_FFMPEG
   &record_ffmpeg,
#endif
   &record_null,
   NULL,
};

static void *nullcamera_init(const char *device, uint64_t caps,
      unsigned width, unsigned height) { return (void*)-1; }
static void nullcamera_free(void *data) { }
static void nullcamera_stop(void *data) { }
static bool nullcamera_start(void *data) { return true; }
static bool nullcamera_poll(void *a,
      retro_camera_frame_raw_framebuffer_t b,
      retro_camera_frame_opengl_texture_t c) { return true; }

static camera_driver_t camera_null = {
   nullcamera_init,
   nullcamera_free,
   nullcamera_start,
   nullcamera_stop,
   nullcamera_poll,
   "null",
};

static const camera_driver_t *camera_drivers[] = {
#ifdef HAVE_V4L2
   &camera_v4l2,
#endif
#ifdef EMSCRIPTEN
   &camera_rwebcam,
#endif
#ifdef ANDROID
   &camera_android,
#endif
   &camera_null,
   NULL,
};

/* MAIN GLOBAL VARIABLES */

/* Descriptive names for options without short variant.
 *
 * Please keep the name in sync with the option name.
 * Order does not matter. */
enum
{
   RA_OPT_MENU = 256, /* must be outside the range of a char */
   RA_OPT_STATELESS,
   RA_OPT_CHECK_FRAMES,
   RA_OPT_PORT,
   RA_OPT_SPECTATE,
   RA_OPT_NICK,
   RA_OPT_COMMAND,
   RA_OPT_APPENDCONFIG,
   RA_OPT_BPS,
   RA_OPT_IPS,
   RA_OPT_NO_PATCH,
   RA_OPT_RECORDCONFIG,
   RA_OPT_SUBSYSTEM,
   RA_OPT_SIZE,
   RA_OPT_FEATURES,
   RA_OPT_VERSION,
   RA_OPT_EOF_EXIT,
   RA_OPT_LOG_FILE,
   RA_OPT_MAX_FRAMES,
   RA_OPT_MAX_FRAMES_SCREENSHOT,
   RA_OPT_MAX_FRAMES_SCREENSHOT_PATH,
   RA_OPT_SET_SHADER,
   RA_OPT_ACCESSIBILITY,
   RA_OPT_LOAD_MENU_ON_ERROR
};

enum rarch_movie_type
{
   RARCH_MOVIE_PLAYBACK = 0,
   RARCH_MOVIE_RECORD
};

enum poll_type_override_t
{
   POLL_TYPE_OVERRIDE_DONTCARE = 0,
   POLL_TYPE_OVERRIDE_EARLY,
   POLL_TYPE_OVERRIDE_NORMAL,
   POLL_TYPE_OVERRIDE_LATE
};

enum input_game_focus_cmd_type
{
   GAME_FOCUS_CMD_OFF = 0,
   GAME_FOCUS_CMD_ON,
   GAME_FOCUS_CMD_TOGGLE,
   GAME_FOCUS_CMD_REAPPLY
};

typedef struct video_pixel_scaler
{
   struct scaler_ctx *scaler;
   void *scaler_out;
} video_pixel_scaler_t;

typedef struct
{
   struct string_list *list;
   enum gfx_ctx_api api;
} gfx_api_gpu_map;

typedef void *(*constructor_t)(void);
typedef void  (*destructor_t )(void*);

typedef struct my_list_t
{
   void **data;
   constructor_t constructor;
   destructor_t destructor;
   int capacity;
   int size;
} my_list;

typedef struct turbo_buttons turbo_buttons_t;

/* Turbo support. */
struct turbo_buttons
{
   int32_t turbo_pressed[MAX_USERS];
   unsigned count;
   uint16_t enable[MAX_USERS];
   bool frame_enable[MAX_USERS];
   bool mode1_enable[MAX_USERS];
};

typedef struct input_game_focus_state
{
   bool enabled;
   bool core_requested;
} input_game_focus_state_t;

#ifdef HAVE_RUNAHEAD
typedef bool(*runahead_load_state_function)(const void*, size_t);
#endif

#ifdef HAVE_DISCORD
/* The Discord API specifies these variables:
- userId --------- char[24]   - the userId of the player asking to join
- username ------- char[344]  - the username of the player asking to join
- discriminator -- char[8]    - the discriminator of the player asking to join
- spectateSecret - char[128] - secret used for spectatin matches
- joinSecret     - char[128] - secret used to join matches
- partyId        - char[128] - the party you would be joining
*/

struct discord_state
{
   int64_t start_time;
   int64_t pause_time;
   int64_t elapsed_time;

   DiscordRichPresence presence;       /* int64_t alignment */

   unsigned status;

   char self_party_id[128];
   char peer_party_id[128];
   char user_name[344];
   char user_avatar[344];

   bool ready;
   bool avatar_ready;
   bool connecting;
};

typedef struct discord_state discord_state_t;
#endif

struct rarch_state
{
   input_driver_state_t input_driver_state;

   double audio_source_ratio_original;
   double audio_source_ratio_current;
   struct retro_system_av_info video_driver_av_info; /* double alignment */
#ifdef HAVE_CRTSWITCHRES
   videocrt_switch_t crt_switch_st;                  /* double alignment */
#endif
   retro_time_t frame_limit_minimum_time;
   retro_time_t frame_limit_last_time;
   retro_time_t libretro_core_runtime_last;
   retro_time_t libretro_core_runtime_usec;
   retro_time_t video_driver_frame_time_samples[
      MEASURE_FRAME_TIME_SAMPLES_COUNT];
   struct global              g_extern;         /* retro_time_t alignment */
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   rarch_timer_t shader_delay_timer;            /* int64_t alignment */
#endif
#ifdef HAVE_DISCORD
   discord_state_t discord_st;                  /* int64_t alignment */
#endif
#ifdef HAVE_GFX_WIDGETS
   dispgfx_widget_t dispwidget_st;              /* uint64_t alignment */
#endif
   struct retro_core_t        current_core;     /* uint64_t alignment */
#if defined(HAVE_RUNAHEAD)
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   struct retro_core_t secondary_core;          /* uint64_t alignment */
#endif
#endif

   uint64_t audio_driver_free_samples_count;

#ifdef HAVE_RUNAHEAD
   uint64_t runahead_last_frame_count;
#endif

   uint64_t video_driver_frame_time_count;
   uint64_t video_driver_frame_count;
   struct retro_camera_callback camera_cb;    /* uint64_t alignment */
   gfx_thumbnail_state_t gfx_thumb_state;     /* uint64_t alignment */
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   input_remote_state_t remote_st_ptr;        /* uint64_t alignment */
#endif

   struct string_list *subsystem_fullpaths;
   struct string_list *audio_driver_devices_list;

   uint8_t *video_driver_record_gpu_buffer;
   bool    *load_no_content_hook;
   float   *audio_driver_output_samples_buf;
   char    *osk_grid[45];
#if defined(HAVE_RUNAHEAD)
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   char    *secondary_library_path;
#endif
   retro_ctx_load_content_info_t *load_content_info;
#endif

   const record_driver_t *recording_driver;
   void *recording_data;

#ifdef HAVE_THREADS
   slock_t *display_lock;
   slock_t *context_lock;
#endif

   const camera_driver_t *camera_driver;
   void *camera_data;

   const ui_companion_driver_t *ui_companion;
   void *ui_companion_data;

#ifdef HAVE_QT
   void *ui_companion_qt_data;
#endif


   const bluetooth_driver_t *bluetooth_driver;
   void *bluetooth_data;

   const wifi_driver_t *wifi_driver;
   void *wifi_data;

   void *current_display_server_data;

#ifdef HAVE_VIDEO_FILTER
   rarch_softfilter_t *video_driver_state_filter;
   void               *video_driver_state_buffer;
#endif

   const void *frame_cache_data;

   void *video_driver_data;
   video_driver_t *current_video;

   /* Interface for "poking". */
   const video_poke_interface_t *video_driver_poke;

   /* Used for 15-bit -> 16-bit conversions that take place before
    * being passed to video driver. */
   video_pixel_scaler_t *video_driver_scaler_ptr;

   const struct
      retro_hw_render_context_negotiation_interface *
      hw_render_context_negotiation;

   void *video_context_data;

#ifdef HAVE_REWIND
   int16_t *audio_driver_rewind_buf;
#endif
   int16_t *audio_driver_output_samples_conv_buf;

#ifdef HAVE_DSP_FILTER
   retro_dsp_filter_t *audio_driver_dsp;
#endif
   const retro_resampler_t *audio_driver_resampler;

   void *audio_driver_resampler_data;
   const audio_driver_t *current_audio;
   void *audio_driver_context_audio_data;
#ifdef HAVE_OVERLAY
   input_overlay_t *overlay_ptr;
#endif

   pad_connection_listener_t *pad_connection_listener;

   void *keyboard_press_data;

#ifdef HAVE_COMMAND
   command_t *input_driver_command[MAX_CMD_DRIVERS];
#endif
#ifdef HAVE_NETWORKGAMEPAD
   input_remote_t *input_driver_remote;
#endif

#ifdef HAVE_HID
   const void *hid_data;
#endif
   settings_t *configuration_settings;
#ifdef HAVE_NETWORKING
   /* Used while Netplay is running */
   netplay_t *netplay_data;
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   struct video_shader *menu_driver_shader;
#endif
   frontend_ctx_driver_t *current_frontend_ctx;
#ifdef HAVE_RUNAHEAD
   my_list *runahead_save_state_list;
   my_list *input_state_list;
#endif

   struct retro_perf_counter *perf_counters_rarch[MAX_COUNTERS];
   struct retro_perf_counter *perf_counters_libretro[MAX_COUNTERS];

   const struct retro_keybind *libretro_input_binds[MAX_USERS];

#ifdef HAVE_REWIND
   struct state_manager_rewind_state rewind_st;
#endif
   input_keyboard_line_t keyboard_line; /* ptr alignment */
   struct retro_subsystem_rom_info
      subsystem_data_roms[SUBSYSTEM_MAX_SUBSYSTEMS]
      [SUBSYSTEM_MAX_SUBSYSTEM_ROMS];                    /* ptr alignment */

   gfx_ctx_driver_t current_video_context;               /* ptr alignment */
   content_state_t            content_st;                /* ptr alignment */
   struct retro_hw_render_callback hw_render;            /* ptr alignment */
#ifdef HAVE_BSV_MOVIE
   bsv_movie_t     *bsv_movie_state_handle;              /* ptr alignment */
#endif
   gfx_display_t              dispgfx;                   /* ptr alignment */
   input_keyboard_press_t keyboard_press_cb;             /* ptr alignment */
   retro_input_state_t input_state_callback_original;    /* ptr alignment */
   struct retro_audio_callback audio_callback;           /* ptr alignment */
   video_driver_frame_t frame_bak;                       /* ptr alignment */
   struct rarch_dir_shader_list dir_shader_list;         /* ptr alignment */
#ifdef HAVE_RUNAHEAD
   function_t retro_reset_callback_original;             /* ptr alignment */
   function_t original_retro_deinit;                     /* ptr alignment */
   function_t original_retro_unload;                     /* ptr alignment */
   runahead_load_state_function
      retro_unserialize_callback_original;               /* ptr alignment */
#endif
   struct retro_callbacks     retro_ctx;                 /* ptr alignment */
#if defined(HAVE_RUNAHEAD)
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   struct retro_callbacks secondary_callbacks;           /* ptr alignment */
#endif
#endif
#ifdef HAVE_AUDIOMIXER
   struct audio_mixer_stream
      audio_mixer_streams[AUDIO_MIXER_MAX_SYSTEM_STREAMS];
                                                         /* ptr alignment */
#endif
#ifdef HAVE_NETWORKING
   struct netplay_room netplay_host_room;                /* ptr alignment */
#endif
#ifdef HAVE_DYNAMIC
   dylib_t lib_handle;                                   /* ptr alignment */
#endif
#if defined(HAVE_RUNAHEAD)
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   dylib_t secondary_lib_handle;                         /* ptr alignment */
#endif
#endif

   /* Opaque handles to currently running window.
    * Used by e.g. input drivers which bind to a window.
    * Drivers are responsible for setting these if an input driver
    * could potentially make use of this. */
   uintptr_t video_driver_display_userdata;
   uintptr_t video_driver_display;
   uintptr_t video_driver_window;

   size_t recording_gpu_width;
   size_t recording_gpu_height;

   size_t frame_cache_pitch;

   size_t audio_driver_chunk_size;
   size_t audio_driver_chunk_nonblock_size;
   size_t audio_driver_chunk_block_size;

#ifdef HAVE_REWIND
   size_t audio_driver_rewind_ptr;
   size_t audio_driver_rewind_size;
#endif
   size_t audio_driver_buffer_size;
   size_t audio_driver_data_ptr;

#ifdef HAVE_RUNAHEAD
   size_t runahead_save_state_size;
#endif

   jmp_buf error_sjlj_context;              /* 4-byte alignment,
                                               put it right before long */

   turbo_buttons_t input_driver_turbo_btns; /* int32_t alignment */
   int osk_ptr;
#if defined(HAVE_COMMAND)
#ifdef HAVE_NETWORK_CMD
   int lastcmd_net_fd;
#endif
#endif

#ifdef HAVE_TRANSLATE
   int ai_service_auto;
#endif

#if defined(HAVE_RUNAHEAD)
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   int port_map[MAX_USERS];
#endif
#endif

#if defined(HAVE_ACCESSIBILITY) && defined(HAVE_TRANSLATE)
   int ai_gamepad_state[MAX_USERS];
#endif
#ifdef HAVE_NETWORKING
   int reannounce;
#endif

   input_device_info_t input_device_info[MAX_INPUT_DEVICES];
   input_mouse_info_t input_mouse_info[MAX_INPUT_DEVICES];
                                          /* unsigned alignment */
#ifdef HAVE_THREAD_STORAGE
   sthread_tls_t rarch_tls;               /* unsigned alignment */
#endif
   unsigned fastforward_after_frames;

   unsigned recording_width;
   unsigned recording_height;

#ifdef HAVE_VIDEO_FILTER
   unsigned video_driver_state_scale;
   unsigned video_driver_state_out_bpp;
#endif
   unsigned frame_cache_width;
   unsigned frame_cache_height;
   unsigned video_driver_width;
   unsigned video_driver_height;
   unsigned osk_last_codepoint;
   unsigned osk_last_codepoint_len;
   unsigned input_hotkey_block_counter;
#ifdef HAVE_ACCESSIBILITY
   unsigned gamepad_input_override;
#endif
#ifdef HAVE_NETWORKING
   unsigned server_port_deferred;
#endif

   unsigned audio_driver_free_samples_buf[
      AUDIO_BUFFER_FREE_SAMPLES_COUNT];
   unsigned perf_ptr_rarch;
   unsigned perf_ptr_libretro;

   float *audio_driver_input_data;
   float video_driver_core_hz;
   float video_driver_aspect_ratio;
   float video_refresh_rate_original;

#ifdef HAVE_AUDIOMIXER
   float audio_driver_mixer_volume_gain;
#endif

   float audio_driver_rate_control_delta;
   float audio_driver_input;
   float audio_driver_volume_gain;

   enum osk_type osk_idx;
   enum rarch_core_type current_core_type;
   enum rarch_core_type explicit_current_core_type;
   enum rotation initial_screen_orientation;
   enum rotation current_screen_orientation;
   enum retro_pixel_format video_driver_pix_fmt;
#if defined(HAVE_COMMAND)
   enum cmd_source_t lastcmd_source;
#endif
#if defined(HAVE_RUNAHEAD)
   enum rarch_core_type last_core_type;
#endif
   enum rarch_display_type video_driver_display_type;
   enum poll_type_override_t core_poll_type_override;
#ifdef HAVE_OVERLAY
   enum overlay_visibility *overlay_visibility;
#endif
   enum resampler_quality audio_driver_resampler_quality;

   /**
    * dynamic.c:dynamic_request_hw_context will try to set flag data when the context
    * is in the middle of being rebuilt; in these cases we will save flag
    * data and set this to true.
    * When the context is reinit, it checks this, reads from
    * deferred_flag_data and cleans it.
    *
    * TODO - Dirty hack, fix it better
    */
   gfx_ctx_flags_t deferred_flag_data;          /* uint32_t alignment */
   retro_bits_t has_set_libretro_device;        /* uint32_t alignment */
   input_mapper_t input_driver_mapper;          /* uint32_t alignment */


#ifdef HAVE_BSV_MOVIE
   struct bsv_state bsv_movie_state;            /* char alignment */
#endif
   char cached_video_driver[32];
   char video_driver_title_buf[64];
   char video_driver_gpu_device_string[128];
   char video_driver_gpu_api_version_string[128];
   char error_string[255];
   char video_driver_window_title[512];
#ifdef HAVE_NETWORKING
   char server_address_deferred[512];
#endif
   char current_library_name[256];
   char current_library_version[256];
   char current_valid_extensions[256];
   char launch_arguments[4096];
   char path_main_basename[8192];
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   char cli_shader[PATH_MAX_LENGTH];
   char runtime_shader_preset[PATH_MAX_LENGTH];
#endif
   char runtime_content_path[PATH_MAX_LENGTH];
   char runtime_core_path[PATH_MAX_LENGTH];
   char subsystem_path[256];
   char path_default_shader_preset[PATH_MAX_LENGTH];
   char path_content[PATH_MAX_LENGTH];
   char path_libretro[PATH_MAX_LENGTH];
   char path_config_file[PATH_MAX_LENGTH];
   char path_config_append_file[256];
   char path_core_options_file[PATH_MAX_LENGTH];
   char dir_system[PATH_MAX_LENGTH];
   char dir_savefile[PATH_MAX_LENGTH];
   char current_savefile_dir[PATH_MAX_LENGTH];
   char current_savestate_dir[PATH_MAX_LENGTH];
   char dir_savestate[PATH_MAX_LENGTH];
   char audio_driver_resampler_ident[64];

#ifdef HAVE_GFX_WIDGETS
   bool widgets_active;
   bool widgets_persisting;
#endif
#ifdef HAVE_NETWORKING
/* Only used before init_netplay */
   bool netplay_enabled;
   bool netplay_is_client;
   /* Used to avoid recursive netplay calls */
   bool in_netplay;
   bool netplay_client_deferred;
   bool is_mitm;
#endif
   bool has_set_username;
   bool rarch_error_on_init;
   bool rarch_force_fullscreen;
   bool has_set_core;
   bool has_set_verbosity;
   bool has_set_libretro;
   bool has_set_libretro_directory;
   bool has_set_save_path;
   bool has_set_state_path;
#ifdef HAVE_PATCH
   bool has_set_ups_pref;
   bool has_set_bps_pref;
   bool has_set_ips_pref;
#endif
#ifdef HAVE_QT
   bool qt_is_inited;
#endif
   bool has_set_log_to_file;
   bool rarch_is_inited;
   bool rarch_is_switching_display_mode;
   bool rarch_is_sram_load_disabled;
   bool rarch_is_sram_save_disabled;
   bool rarch_use_sram;
   bool rarch_ups_pref;
   bool rarch_bps_pref;
   bool rarch_ips_pref;
#ifdef HAVE_PATCH
   bool rarch_patch_blocked;
#endif
   bool video_driver_window_title_update;

   /**
    * dynamic.c:dynamic_request_hw_context will try to set
    * flag data when the context
    * is in the middle of being rebuilt; in these cases we will save flag
    * data and set this to true.
    * When the context is reinit, it checks this, reads from
    * deferred_flag_data and cleans it.
    *
    * TODO - Dirty hack, fix it better
    */
   bool deferred_video_context_driver_set_flags;
   bool ignore_environment_cb;
   bool core_set_shared_context;

   /* Graphics driver requires RGBA byte order data (ABGR on little-endian)
    * for 32-bit.
    * This takes effect for overlay and shader cores that wants to load
    * data into graphics driver. Kinda hackish to place it here, it is only
    * used for GLES.
    * TODO: Refactor this better. */
   bool video_driver_use_rgba;

   /* Graphics driver supports HDR displays
    * Currently only D3D11/D3D12 supports HDR displays and 
    * whether we've enabled it */
   bool video_driver_hdr_support;

   /* If set during context deinit, the driver should keep
    * graphics context alive to avoid having to reset all
    * context state. */
   bool video_driver_cache_context;

   /* Set to true by driver if context caching succeeded. */
   bool video_driver_cache_context_ack;

#ifdef HAVE_GFX_WIDGETS
   bool gfx_widgets_paused;
   bool gfx_widgets_fast_forward;
   bool gfx_widgets_rewinding;
#endif
#ifdef HAVE_ACCESSIBILITY
   /* Is text-to-speech accessibility turned on? */
   bool accessibility_enabled;
#endif
#ifdef HAVE_CONFIGFILE
   bool rarch_block_config_read;
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   bool cli_shader_disable;
#endif

   bool location_driver_active;
   bool bluetooth_driver_active;
   bool wifi_driver_active;
   bool video_driver_active;
   bool audio_driver_active;
   bool camera_driver_active;
#ifdef HAVE_VIDEO_FILTER
   bool video_driver_state_out_rgb32;
#endif
   bool video_driver_crt_switching_active;
   bool video_driver_threaded;

   bool video_started_fullscreen;

   bool audio_driver_control;
   bool audio_driver_mute_enable;
   bool audio_driver_use_float;

   bool audio_suspended;

#ifdef HAVE_RUNAHEAD
   bool has_variable_update;
   bool runahead_save_state_size_known;
   bool request_fast_savestate;
   bool hard_disable_audio;

   bool input_is_dirty;
#endif

#if defined(HAVE_NETWORKING)
   bool has_set_netplay_mode;
   bool has_set_netplay_ip_address;
   bool has_set_netplay_ip_port;
   bool has_set_netplay_stateless_mode;
   bool has_set_netplay_check_frames;
#endif

   bool input_driver_keyboard_linefeed_enable;

   bool input_driver_block_hotkey;
   bool input_driver_block_libretro_input;
   bool input_driver_grab_mouse_state;
   bool input_driver_analog_requested[MAX_USERS];

   input_game_focus_state_t game_focus_state; /* bool alignment */

#ifdef HAVE_MENU
   /* Is the menu driver still running? */
   bool menu_driver_alive;
   /* Are we binding a button inside the menu? */
   bool menu_driver_is_binding;
#endif

   bool recording_enable;
   bool streaming_enable;


   bool main_ui_companion_is_on_foreground;
   bool keyboard_mapping_blocked;
   retro_bits_512_t keyboard_mapping_bits;

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   bool shader_presets_need_reload;
#endif
#ifdef HAVE_RUNAHEAD
   bool runahead_video_driver_is_active;
   bool runahead_available;
   bool runahead_secondary_core_available;
   bool runahead_force_input_dirty;
#endif

#ifdef HAVE_AUDIOMIXER
   bool audio_driver_mixer_mute_enable;
   bool audio_mixer_active;
#endif
};

static struct rarch_state         rarch_st;

#ifdef HAVE_THREAD_STORAGE
static const void *MAGIC_POINTER                                 = (void*)(uintptr_t)0x0DEFACED;
#endif

#ifdef HAVE_LIBNX
/* TODO/FIXME - public global variable */
extern u32 __nx_applet_type;
#endif

static const video_display_server_t *current_display_server      = &dispserv_null;

struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END] = {
   { 1.3333f,         "4:3"           },
   { 1.7778f,         "16:9"          },
   { 1.6f,            "16:10"         },
   { 16.0f / 15.0f,   "16:15"         },
   { 21.0f / 9.0f,    "21:9"          },
   { 1.0f,            "1:1"           },
   { 2.0f,            "2:1"           },
   { 1.5f,            "3:2"           },
   { 0.75f,           "3:4"           },
   { 4.0f,            "4:1"           },
   { 0.5625f,         "9:16"          },
   { 1.25f,           "5:4"           },
   { 1.2f,            "6:5"           },
   { 0.7777f,         "7:9"           },
   { 2.6666f,         "8:3"           },
   { 1.1428f,         "8:7"           },
   { 1.5833f,         "19:12"         },
   { 1.3571f,         "19:14"         },
   { 1.7647f,         "30:17"         },
   { 3.5555f,         "32:9"          },
   { 0.0f,            "Config"        },
   { 1.0f,            "Square pixel"  },
   { 1.0f,            "Core provided" },
   { 0.0f,            "Custom"        },
   { 1.3333f,         "Full" }
};

static gfx_api_gpu_map gpu_map[] = {
   { NULL,                   GFX_CTX_VULKAN_API     },
   { NULL,                   GFX_CTX_DIRECT3D10_API },
   { NULL,                   GFX_CTX_DIRECT3D11_API },
   { NULL,                   GFX_CTX_DIRECT3D12_API }
};

/* TODO/FIXME - turn these into static global variable */
#ifdef HAVE_DISCORD
bool discord_is_inited                                          = false;
#endif
uint64_t lifecycle_state                                        = 0;
unsigned subsystem_current_count                                = 0;
struct retro_keybind input_config_binds[MAX_USERS][RARCH_BIND_LIST_END];
struct retro_keybind input_autoconf_binds[MAX_USERS][RARCH_BIND_LIST_END];
struct retro_subsystem_info subsystem_data[SUBSYSTEM_MAX_SUBSYSTEMS];
