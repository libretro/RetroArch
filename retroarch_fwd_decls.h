#ifndef _RETROARCH_FWD_DECLS_H
#define _RETROARCH_FWD_DECLS_H

static void retroarch_fail(int error_code, const char *error);
      
#ifdef HAVE_LIBNX
void libnx_apply_overclock(void);
#endif

static void retroarch_deinit_drivers(struct retro_callbacks *cbs);

#ifdef HAVE_RUNAHEAD
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
static bool secondary_core_create(runloop_state_t *runloop_st, settings_t *settings);
static void secondary_core_destroy(runloop_state_t *runloop_st);
static bool secondary_core_ensure_exists(
      runloop_state_t *runloop_st, settings_t *settings);
#endif
static int16_t input_state_get_last(unsigned port,
      unsigned device, unsigned index, unsigned id);
#endif
static void retro_frame_null(const void *data, unsigned width,
      unsigned height, size_t pitch);
static void retro_run_null(void);
static void retro_input_poll_null(void);
static void runloop_apply_fastmotion_override(runloop_state_t *p_runloop, settings_t *settings);

static void uninit_libretro_symbols(struct retro_core_t *current_core);
      
static bool init_libretro_symbols(
      runloop_state_t *runloop_st,
      enum rarch_core_type type,
      struct retro_core_t *current_core);

static void ui_companion_driver_toggle(
      struct rarch_state *p_rarch,
      bool desktop_menu_enable,
      bool ui_companion_toggle,
      bool force);

static void ui_companion_driver_deinit(struct rarch_state *p_rarch);
static void ui_companion_driver_init_first(struct rarch_state *p_rarch);

static bool core_load(unsigned poll_type_behavior);
static bool core_unload_game(void);

static void driver_camera_stop(void);
static bool driver_camera_start(void);

static const void *find_driver_nonempty(
      const char *label, int i,
      char *s, size_t len);

#endif
