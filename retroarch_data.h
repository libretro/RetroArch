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

#define FAIL_CPU(simd_type) do { \
   RARCH_ERR(simd_type " code is compiled in, but CPU does not support this feature. Cannot continue.\n"); \
   retroarch_fail(1, "validate_cpu_features()"); \
} while (0)

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "zip"
#else
#define DEFAULT_EXT ""
#endif

#ifdef HAVE_DYNAMIC
#define SYMBOL(x) do { \
   function_t func = dylib_proc(lib_handle_local, #x); \
   memcpy(&current_core->x, &func, sizeof(func)); \
   if (!current_core->x) { RARCH_ERR("Failed to load symbol: \"%s\"\n", #x); retroarch_fail(1, "init_libretro_symbols()"); } \
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

/* Griffin hack */
#ifdef HAVE_QT
#ifndef HAVE_MAIN
#define HAVE_MAIN
#endif
#endif

/* DRIVERS */
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
struct rarch_state
{
   struct global              g_extern;         /* retro_time_t alignment */
   struct retro_camera_callback camera_cb;    /* uint64_t alignment */

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
   char *connect_host; /* Netplay hostname passed from CLI */

   struct retro_perf_counter *perf_counters_rarch[MAX_COUNTERS];

   jmp_buf error_sjlj_context;              /* 4-byte alignment,
                                               put it right before long */
#ifdef HAVE_THREAD_STORAGE
   sthread_tls_t rarch_tls;               /* unsigned alignment */
#endif
   unsigned perf_ptr_rarch;

   char error_string[255];
   char launch_arguments[4096];
   char path_default_shader_preset[PATH_MAX_LENGTH];
   char path_content[PATH_MAX_LENGTH];
   char path_libretro[PATH_MAX_LENGTH];
   char path_config_file[PATH_MAX_LENGTH];
   char path_config_append_file[256];
   char path_core_options_file[PATH_MAX_LENGTH];
   char dir_system[PATH_MAX_LENGTH];
   char dir_savefile[PATH_MAX_LENGTH];
   char dir_savestate[PATH_MAX_LENGTH];
   bool has_set_username;
   bool rarch_error_on_init;
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
   bool rarch_ups_pref;
   bool rarch_bps_pref;
   bool rarch_ips_pref;

#ifdef HAVE_CONFIGFILE
   bool rarch_block_config_read;
#endif
   bool location_driver_active;
   bool bluetooth_driver_active;
   bool wifi_driver_active;
   bool camera_driver_active;

   bool streaming_enable;
   bool main_ui_companion_is_on_foreground;
};
