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

#ifdef HAVE_RUNAHEAD
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
static bool secondary_core_create(struct rarch_state *p_rarch,
      runloop_state_t *runloop_st, settings_t *settings);
static void secondary_core_destroy(runloop_state_t *runloop_st);
static bool secondary_core_ensure_exists(struct rarch_state *p_rarch,
      runloop_state_t *runloop_st, settings_t *settings);
#endif
static int16_t input_state_get_last(unsigned port,
      unsigned device, unsigned index, unsigned id);
#endif
static void video_driver_frame(const void *data, unsigned width,
      unsigned height, size_t pitch);
static void retro_frame_null(const void *data, unsigned width,
      unsigned height, size_t pitch);
static void retro_run_null(void);
static void retro_input_poll_null(void);
static void runloop_apply_fastmotion_override(runloop_state_t *p_runloop, settings_t *settings);

static void uninit_libretro_symbols(
      struct rarch_state *p_rarch,
      struct retro_core_t *current_core);
static bool init_libretro_symbols(
      struct rarch_state *p_rarch,
      runloop_state_t *runloop_st,
      enum rarch_core_type type,
      struct retro_core_t *current_core);

static void ui_companion_driver_deinit(struct rarch_state *p_rarch);
static void ui_companion_driver_init_first(
      settings_t *settings,
      struct rarch_state *p_rarch);

static void driver_uninit(struct rarch_state *p_rarch, int flags);

static void drivers_init(struct rarch_state *p_rarch,
      settings_t *settings,
      int flags,
      bool verbosity_enabled);

static bool core_load(unsigned poll_type_behavior);
static bool core_unload_game(void);

static bool retroarch_environment_cb(unsigned cmd, void *data);

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

static const void *find_driver_nonempty(
      const char *label, int i,
      char *s, size_t len);

static bool core_set_default_callbacks(struct retro_callbacks *cbs);

#endif
