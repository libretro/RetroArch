#ifndef _RETROARCH_FWD_DECLS_H
#define _RETROARCH_FWD_DECLS_H

#ifdef HAVE_DISCORD
#if defined(__cplusplus) && !defined(CXX_BUILD)
extern "C"
{
#endif
   void Discord_Register(const char *a, const char *b);
#if defined(__cplusplus) && !defined(CXX_BUILD)
}
#endif
#endif

static void retroarch_fail(struct rarch_state *p_rarch,
      int error_code, const char *error);
static void ui_companion_driver_toggle(
      struct rarch_state *p_rarch,
      bool desktop_menu_enable,
      bool ui_companion_toggle,
      bool force);

#ifdef HAVE_LIBNX
void libnx_apply_overclock(void);
#endif
#ifdef HAVE_ACCESSIBILITY
#ifdef HAVE_TRANSLATE
static bool is_narrator_running(struct rarch_state *p_rarch, bool accessibility_enable);
#endif
#endif

#ifdef HAVE_NETWORKING
static void deinit_netplay(struct rarch_state *p_rarch);
#endif

static void retroarch_deinit_drivers(struct rarch_state *p_rarch,
      struct retro_callbacks *cbs);

static void retroarch_deinit_core_options(
      bool game_options_active,
      const char *path_core_options,
      core_option_manager_t *core_options);
static core_option_manager_t *retroarch_init_core_variables(
      settings_t *settings,
      const struct retro_variable *vars);
static core_option_manager_t *rarch_init_core_options(
      settings_t *settings,
      const struct retro_core_options_v2 *options_v2);
#ifdef HAVE_RUNAHEAD
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
static bool secondary_core_create(struct rarch_state *p_rarch,
      settings_t *settings);
static void secondary_core_destroy(struct rarch_state *p_rarch);
static bool secondary_core_ensure_exists(struct rarch_state *p_rarch,
      settings_t *settings);
#endif
static int16_t input_state_get_last(unsigned port,
      unsigned device, unsigned index, unsigned id);
#endif
static int16_t input_state(unsigned port, unsigned device,
      unsigned idx, unsigned id);
static void video_driver_frame(const void *data, unsigned width,
      unsigned height, size_t pitch);
static void retro_frame_null(const void *data, unsigned width,
      unsigned height, size_t pitch);
static void retro_run_null(void);
static void retro_input_poll_null(void);
static void runloop_apply_fastmotion_override(
      struct rarch_state *p_rarch, runloop_state_t *p_runloop,
      settings_t *settings);

static uint64_t input_driver_get_capabilities(void);

static void uninit_libretro_symbols(
      struct rarch_state *p_rarch,
      struct retro_core_t *current_core);
static bool init_libretro_symbols(
      struct rarch_state *p_rarch,
      enum rarch_core_type type,
      struct retro_core_t *current_core);

static void ui_companion_driver_deinit(struct rarch_state *p_rarch);
static void ui_companion_driver_init_first(
      settings_t *settings,
      struct rarch_state *p_rarch);

static bool audio_driver_stop(struct rarch_state *p_rarch);
static bool audio_driver_start(struct rarch_state *p_rarch,
      bool is_shutdown);

static bool recording_init(settings_t *settings,
      struct rarch_state *p_rarch);
static bool recording_deinit(struct rarch_state *p_rarch);

#ifdef HAVE_OVERLAY
static void retroarch_overlay_init(struct rarch_state *p_rarch);
static void retroarch_overlay_deinit(struct rarch_state *p_rarch);
#endif

#ifdef HAVE_AUDIOMIXER
static void audio_mixer_play_stop_sequential_cb(
      audio_mixer_sound_t *sound, unsigned reason);
static void audio_mixer_play_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason);
static void audio_mixer_menu_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason);
#endif

static void video_driver_gpu_record_deinit(struct rarch_state *p_rarch);
static retro_proc_address_t video_driver_get_proc_address(const char *sym);
static uintptr_t video_driver_get_current_framebuffer(void);
static bool video_driver_find_driver(
      struct rarch_state *p_rarch,
      settings_t *settings,
      const char *prefix, bool verbosity_enabled);

#ifdef HAVE_BSV_MOVIE
static void bsv_movie_deinit(struct rarch_state *p_rarch);
static bool bsv_movie_init(struct rarch_state *p_rarch);
static bool bsv_movie_check(struct rarch_state *p_rarch,
      settings_t *settings);
#endif

static void driver_uninit(struct rarch_state *p_rarch, int flags);
static void drivers_init(struct rarch_state *p_rarch,
      settings_t *settings,
      int flags,
      bool verbosity_enabled);

static bool core_load(struct rarch_state *p_rarch,
      unsigned poll_type_behavior);
static bool core_unload_game(struct rarch_state *p_rarch);

static bool rarch_environment_cb(unsigned cmd, void *data);

static void driver_camera_stop(void);
static bool driver_camera_start(void);

#ifdef HAVE_ACCESSIBILITY
static bool is_accessibility_enabled(bool accessibility_enable,
      bool accessibility_enabled);
static bool accessibility_speak_priority(
      struct rarch_state *p_rarch,
      bool accessibility_enable,
      unsigned accessibility_narrator_speech_speed,
      const char* speak_text, int priority);
#endif

#ifdef HAVE_MENU
static int menu_input_post_iterate(
      struct rarch_state *p_rarch,
      gfx_display_t *p_disp,
      struct menu_state *menu_st,
      unsigned action,
      retro_time_t current_time);
#endif

static bool retroarch_apply_shader(
      struct rarch_state *p_rarch,
      settings_t *settings,
      enum rarch_shader_type type, const char *preset_path,
      bool message);

static void video_driver_restore_cached(struct rarch_state *p_rarch,
      settings_t *settings);

static const void *find_driver_nonempty(
      const char *label, int i,
      char *s, size_t len);

static bool core_set_default_callbacks(struct retro_callbacks *cbs);

#endif
