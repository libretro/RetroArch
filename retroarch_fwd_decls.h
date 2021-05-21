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

static bool midi_driver_read(uint8_t *byte);
static bool midi_driver_write(uint8_t byte, uint32_t delta_time);
static bool midi_driver_output_enabled(void);
static bool midi_driver_input_enabled(void);
static bool midi_driver_set_all_sounds_off(struct rarch_state *p_rarch);
static const void *midi_driver_find_handle(int index);
static bool midi_driver_flush(void);

static void retroarch_deinit_core_options(struct rarch_state *p_rarch,
      const char *p);
static void retroarch_init_core_variables(
      struct rarch_state *p_rarch,
      const struct retro_variable *vars);
static void rarch_init_core_options(
      struct rarch_state *p_rarch,
      const struct retro_core_option_definition *option_defs);
#ifdef HAVE_RUNAHEAD
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
static bool secondary_core_create(struct rarch_state *p_rarch,
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
static void input_overlay_set_alpha_mod(struct rarch_state *p_rarch,
      input_overlay_t *ol, float mod);
static void input_overlay_set_scale_factor(struct rarch_state *p_rarch,
      input_overlay_t *ol, const overlay_layout_desc_t *layout_desc);
static void input_overlay_load_active(
      struct rarch_state *p_rarch,
      input_overlay_t *ol, float opacity);
static void input_overlay_auto_rotate_(struct rarch_state *p_rarch,
      bool input_overlay_enable, input_overlay_t *ol);
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

#if defined(HAVE_RUNAHEAD)
static void core_free_retro_game_info(struct retro_game_info *dest);
#endif
static bool core_load(struct rarch_state *p_rarch,
      unsigned poll_type_behavior);
static bool core_unload_game(struct rarch_state *p_rarch);

static bool rarch_environment_cb(unsigned cmd, void *data);

static bool driver_location_get_position(double *lat, double *lon,
      double *horiz_accuracy, double *vert_accuracy);
static void driver_location_set_interval(unsigned interval_msecs,
      unsigned interval_distance);
static void driver_location_stop(void);
static bool driver_location_start(void);
static void driver_camera_stop(void);
static bool driver_camera_start(void);
static int16_t input_joypad_analog_button(
      float input_analog_deadzone,
      float input_analog_sensitivity,
      const input_device_driver_t *drv,
      rarch_joypad_info_t *joypad_info,
      unsigned ident,
      const struct retro_keybind *binds);
static int16_t input_joypad_analog_axis(
      unsigned input_analog_dpad_mode,
      float input_analog_deadzone,
      float input_analog_sensitivity,
      const input_device_driver_t *drv,
      rarch_joypad_info_t *joypad_info,
      unsigned idx,
      unsigned ident,
      const struct retro_keybind *binds);

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
static bool input_mouse_button_raw(
      struct rarch_state *p_rarch,
      input_driver_t *current_input,
      unsigned joy_idx,
      unsigned port, unsigned id);
static bool input_keyboard_line_append(
      struct input_keyboard_line *keyboard_line,
      const char *word);
static const char **input_keyboard_start_line(
      void *userdata,
      struct input_keyboard_line *keyboard_line,
      input_keyboard_line_complete_t cb);

static void menu_driver_list_free(
      const menu_ctx_driver_t *menu_driver_ctx,
      menu_ctx_list_t *list);
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
