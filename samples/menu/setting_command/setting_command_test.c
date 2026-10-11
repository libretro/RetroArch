/* One press, one command.
 *
 * Links the shipping RetroArch objects with only main() replaced (the
 * playlist_nav pattern) and boots the menu on the null video driver,
 * whose init is swapped for one that counts. Rows whose command
 * reinitialises the video driver are then pressed through
 * menu_action_handle_setting(), as the menu presses them.
 *
 * The claim: a press reinitialises the driver once - the first press
 * and every later one, with OK, right, left and Start, on a row that
 * applies its command by itself (SD_FLAG_CMD_APPLY_AUTO) and on one
 * that does not. The row's handler and the dispatcher both run the
 * change handler; when each of them fires the command, the driver
 * comes up twice for one press.
 *
 * And Start on the overlay preset rows restores what a fresh
 * configuration has - the bundled preset on mobile, none elsewhere -
 * not the directory their file browser opens in.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/setting_command/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <streams/file_stream.h>
#include <compat/strl.h>
#include <string/stdstring.h>

#include "../../../runloop.h"
#include "../../../record/record_driver.h"
#ifdef HAVE_MICROPHONE
#include "../../../audio/microphone_driver.h"
#endif
#include "../../../command.h"
#include "../../../core_info.h"
#include "../../../driver.h"
#include "../../../audio/audio_driver.h"
#include "../../../configuration.h"
#include "../../../file_path_special.h"
#include "../../../retroarch.h"
#include "../../../setting_list.h"
#include "../../../menu/menu_defines.h"
#include "../../../menu/menu_setting.h"
#include "../../../menu/menu_cbs.h"
#include "../../../menu/menu_driver.h"
#include "../../../msg_hash_lbl_str.h"
#include "../../../msg_hash.h"
#include "../../../gfx/video_driver.h"
#include "../../../input/input_driver.h"
#include "../../../frontend/frontend_driver.h"

static unsigned failures = 0;
static unsigned inits;
static void *(*null_init)(const video_info_t *video);

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

static void *harness_init(const video_info_t *video)
{
   void *ret = null_init(video);
   inits++;
   video_driver_modify_disp_flags(VIDEO_FLAG_HDR_SUPPORT, 0);
   return ret;
}

/* Presses a row with OK, right, left and Start; each press has to
 * move the setting and bring the video driver up once. */
static void lane(enum msg_hash_enums idx, const char *name, bool apply_auto)
{
   static const unsigned actions[4]    = {
      MENU_ACTION_OK, MENU_ACTION_RIGHT, MENU_ACTION_LEFT, MENU_ACTION_START
   };
   static const char *const pressed[4] = { "OK", "right", "left", "Start" };
   unsigned i;
   rarch_setting_t *s = menu_setting_find_enum(idx);

   CHECK(s != NULL, "fixture: %s is not a setting", name);
   if (!s)
      return;
   CHECK(s->type == ST_BOOL && s->cmd_trigger_idx == CMD_EVENT_REINIT,
         "fixture: %s is not a boolean that reinitialises", name);
   CHECK(!(s->flags & SD_FLAG_CMD_APPLY_AUTO) == !apply_auto,
         "fixture: %s %s its command by itself", name,
         apply_auto ? "no longer applies" : "now applies");
   /* Three flips leave it off its default, for Start to reset. */
   CHECK(*s->value.target.boolean == s->default_value.boolean,
         "fixture: %s does not start at its default", name);

   for (i = 0; i < 4; i++)
   {
      bool was        = *s->value.target.boolean;
      unsigned before = inits;

      menu_action_handle_setting(s, 0, actions[i], false);

      CHECK(*s->value.target.boolean != was,
            "%s, press %u (%s): the setting did not change",
            name, i + 1, pressed[i]);
      CHECK(inits - before == 1,
            "%s, press %u (%s): the video driver was initialised %u times,"
            " want 1", name, i + 1, pressed[i], inits - before);
   }
}

/* A row whose change handler picks the command itself, whatever the
 * row's flags say: the video scale, which reinitialises while not
 * fullscreen. Start resets it and runs the change handler, and
 * menu_setting_generic() runs it again. */
static void lane_start_picks(enum msg_hash_enums idx, const char *name)
{
   unsigned before;
   rarch_setting_t *s = menu_setting_find_enum(idx);

   CHECK(s != NULL, "fixture: %s is not a setting", name);
   if (!s)
      return;
   CHECK(s->type == ST_UINT, "fixture: %s is not an unsigned setting", name);
   *s->value.target.unsigned_integer = s->default_value.unsigned_integer + 1;
   before                            = inits;

   menu_action_handle_setting(s, 0, MENU_ACTION_START, false);

   CHECK(*s->value.target.unsigned_integer == s->default_value.unsigned_integer,
         "%s, Start: the setting was not reset", name);
   CHECK(inits - before == 1,
         "%s, Start: the video driver was initialised %u times, want 1",
         name, inits - before);
}

#ifdef HAVE_OVERLAY
/* The bundled presets a mobile build resets to; file_path_special.h
 * names them on mobile only */
#ifdef RARCH_MOBILE
#define OVERLAY_DEFAULT     FILE_PATH_DEFAULT_OVERLAY
#define OSK_OVERLAY_DEFAULT FILE_PATH_DEFAULT_OSK_OVERLAY
#else
#define OVERLAY_DEFAULT     NULL
#define OSK_OVERLAY_DEFAULT NULL
#endif

/* Start on an overlay preset row restores what a fresh configuration
 * has: on mobile, where touch devices have no other controls, the
 * bundled preset under the overlay directory; elsewhere none. The
 * row's default string is the directory its file browser opens in,
 * which is no preset. A tree built with CFLAGS=-DRARCH_MOBILE runs
 * the mobile half. */
static void lane_overlay_preset_start(enum msg_hash_enums idx,
      const char *name, const char *mobile_default)
{
   char want[PATH_MAX_LENGTH];
   rarch_setting_t *s = menu_setting_find_enum(idx);

   want[0] = '\0';
#ifdef RARCH_MOBILE
   fill_pathname_join_special(want,
         config_get_ptr()->paths.directory_overlay, mobile_default,
         sizeof(want));
#else
   (void)mobile_default;
#endif

   CHECK(s != NULL, "fixture: %s is not a setting", name);
   if (!s)
      return;
   CHECK(s->default_value.string && *s->default_value.string,
         "fixture: %s has no browser directory", name);
   strlcpy(s->value.target.string, "/nowhere/preset.cfg", s->size);

   menu_action_handle_setting(s, 0, MENU_ACTION_START, false);

   CHECK(string_is_equal(s->value.target.string, want),
         "%s, Start: the preset is \"%s\", want \"%s\"", name,
         s->value.target.string, want);
}

/* The rows beside a preset keep their own reset */
static void lane_overlay_opacity_start(void)
{
   rarch_setting_t *s = menu_setting_find_enum(
         MENU_ENUM_LABEL_OSK_OVERLAY_OPACITY);

   CHECK(s != NULL && s->type == ST_FLOAT,
         "fixture: osk_overlay_opacity is not a float setting");
   if (!s || s->type != ST_FLOAT)
      return;
   *s->value.target.fraction = s->default_value.fraction / 2.0f;

   menu_action_handle_setting(s, 0, MENU_ACTION_START, false);

   CHECK(*s->value.target.fraction == s->default_value.fraction,
         "input_osk_overlay_opacity, Start: %f, want the default %f",
         *s->value.target.fraction, s->default_value.fraction);
}
#endif


extern audio_driver_t audio_null;
static unsigned audio_inits;
static unsigned requested_audio_latency;

static void *harness_audio_init(const char *device, unsigned rate,
      unsigned latency, unsigned *new_rate)
{
   audio_inits++;
   (void)device;
   (void)rate;
   requested_audio_latency = latency;
   (void)new_rate;
   return NULL;
}

static unsigned modes;
static unsigned requested_dims;
static bool requested_fullscreen;
static bool mode_success;
static uint32_t mode_flags;

static bool harness_windowed(void *data)
{
   (void)data;
   return true;
}

static uint32_t harness_flags(void *data)
{
   (void)data;
   return mode_flags;
}

static void harness_mode(void *data, unsigned dims, bool fullscreen)
{
   (void)data;
   requested_dims = dims;
   modes++;
   requested_fullscreen = fullscreen;
   if (!mode_success)
      video_driver_modify_disp_flags(VIDEO_FLAG_DRIVER_REINIT, 0);
}

static void lane_fullscreen(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   audio_driver_state_t *audio_st = audio_state_get_ptr();
   settings_t *settings = config_get_ptr();
   const video_poke_interface_t *saved_poke = video_st->poke;
   video_poke_interface_t poke = *saved_poke;
   bool (*saved_windowed)(void*) = video_null.has_windowed;
   rarch_setting_t *s = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_FULLSCREEN);
   core_info_t *before_core = NULL;
   core_info_t *after_core = NULL;
   void *before_audio = audio_st->context_audio_data;
   unsigned before = inits;
   unsigned before_audio_inits = audio_inits;
   unsigned i;

   CHECK(s && s->cmd_trigger_idx == CMD_EVENT_FULLSCREEN_APPLY,
         "fullscreen setting must apply its selected state");
   if (!s)
      return;
   poke.get_flags = harness_flags;
   poke.set_video_mode = harness_mode;
   video_null.has_windowed = harness_windowed;
   video_st->poke = &poke;
   mode_flags = 0;
   BIT32_SET(mode_flags, GFX_CTX_FLAGS_FULLSCREEN_IN_PLACE);
   BIT32_SET(mode_flags, GFX_CTX_FLAGS_FULLSCREEN_EXCLUSIVE_IN_PLACE);
   mode_success = true;
   modes = 0;
   video_driver_modify_disp_flags(VIDEO_FLAG_FORCE_FULLSCREEN, 0);
   settings->bools.audio_enable_menu = true;
   core_info_get_current_core(&before_core);
   if (!before_core)
   {
      core_info_init_current_core();
      core_info_get_current_core(&before_core);
   }

   for (i = 0; i < 2; i++)
   {
      bool was = settings->bools.video_fullscreen;
      menu_action_handle_setting(s, 0, MENU_ACTION_RIGHT, false);
      CHECK(settings->bools.video_fullscreen != was,
            "fullscreen menu press must change selected state");
      CHECK(requested_fullscreen == settings->bools.video_fullscreen,
            "fullscreen apply must not invert the selected state");
   }
   CHECK(!(video_driver_get_disp_flags() & VIDEO_FLAG_FORCE_FULLSCREEN),
         "menu fullscreen selection must replace forced CLI fullscreen");
   CHECK(modes == 2 && inits == before,
         "fullscreen menu presses must use in-place transitions");

   mode_success = false;
   AUDIO_FLAGS_SET(audio_st, AUDIO_FLAG_SUSPENDED);
   CHECK(command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL),
         "failed in-place transition must recover");
   CHECK(inits == before + 1, "fallback must rebuild video once");
   CHECK(audio_inits == before_audio_inits
         && audio_st->context_audio_data == before_audio,
         "fullscreen fallback must retain audio with menu sounds enabled");
   CHECK(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_SUSPENDED,
         "fullscreen fallback must preserve suspended audio");
   core_info_get_current_core(&after_core);
   CHECK(before_core && before_core == after_core,
         "video fallback must retain the current core metadata");
   video_st->poke = &poke;
   mode_flags = 0;
   CHECK(command_event(CMD_EVENT_FULLSCREEN_APPLY, NULL),
         "unsupported in-place transition must recover");
   CHECK(modes == 3 && inits == before + 2
         && audio_inits == before_audio_inits,
         "unsupported transition must rebuild only video once");
   AUDIO_FLAGS_CLEAR(audio_st, AUDIO_FLAG_SUSPENDED);
   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   core_info_get_current_core(&after_core);
   CHECK(before_core == after_core && inits == before + 2,
         "audio reset must retain core metadata and video");
   video_st->poke = saved_poke;
   video_null.has_windowed = saved_windowed;
   settings->bools.audio_enable_menu = false;
}


#ifndef RARCH_MOBILE
static void lane_window_scale(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings = config_get_ptr();
   const video_poke_interface_t *saved_poke = video_st->poke;
   video_poke_interface_t poke = *saved_poke;
   struct retro_game_geometry saved_geometry = video_st->av_info.geometry;
   rarch_setting_t *s = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_SCALE);
   core_info_t *before_core = NULL;
   core_info_t *after_core = NULL;
   unsigned before = inits;
   unsigned before_audio_inits = audio_inits;
   unsigned before_modes;
   unsigned expected_dims;

   CHECK(s != NULL, "fixture: window scale setting exists");
   if (!s)
      return;
   settings->bools.video_fullscreen = false;
   settings->bools.video_force_aspect = false;
   settings->bools.video_window_save_positions = false;
   settings->bools.video_window_custom_size_enable = false;
   settings->bools.audio_enable_menu = true;
   settings->uints.window_auto_dims_max = VIDEO_SCALE_PACK(1280, 960);
   video_st->av_info.geometry.base_width = 320;
   video_st->av_info.geometry.base_height = 240;
   poke.get_flags = harness_flags;
   poke.set_video_mode = harness_mode;
   video_st->poke = &poke;
   mode_flags = 0;
   BIT32_SET(mode_flags, GFX_CTX_FLAGS_WINDOW_RESIZE_IN_PLACE);
   mode_success = true;
   modes = 0;
   video_driver_modify_disp_flags(0, VIDEO_FLAG_FORCE_FULLSCREEN);
   core_info_get_current_core(&before_core);

   CHECK(command_event_resize_windowed_scale(settings, 2),
         "window scale request succeeds");
   CHECK(modes == 1 && !requested_fullscreen
         && requested_dims == VIDEO_SCALE_PACK(640, 480) && inits == before,
         "window scale must resize the live window to the requested dimensions");

   menu_action_handle_setting(s, 0, MENU_ACTION_RIGHT, false);
   CHECK(modes == 2 && settings->uints.video_scale == 3
         && requested_dims == VIDEO_SCALE_PACK(960, 720) && inits == before,
         "menu scale must apply its current value once in place");
   expected_dims = VIDEO_SCALE_PACK(320 * s->default_value.unsigned_integer,
         240 * s->default_value.unsigned_integer);
   menu_action_handle_setting(s, 0, MENU_ACTION_START, false);
   CHECK(modes == 3 && requested_dims == expected_dims && inits == before,
         "Start must apply the default window scale once");

   before_modes = modes;
   CHECK(!command_event_resize_windowed_scale(settings, 0) && modes == before_modes,
         "zero scale must not resize or restart");
   settings->bools.video_fullscreen = true;
   CHECK(command_event_resize_windowed_scale(settings, 4) && modes == before_modes,
         "fullscreen scale changes must wait for windowed mode");
   settings->bools.video_fullscreen = false;
   video_driver_modify_disp_flags(VIDEO_FLAG_FORCE_FULLSCREEN, 0);
   CHECK(command_event_resize_windowed_scale(settings, 2) && modes == before_modes,
         "CLI-forced fullscreen must not become windowed on a scale change");
   video_driver_modify_disp_flags(0, VIDEO_FLAG_FORCE_FULLSCREEN);

   mode_success = false;
   CHECK(command_event_resize_windowed_scale(settings, 3),
         "failed live resize must recover");
   CHECK(modes == before_modes + 1 && inits == before + 1,
         "failed live resize must rebuild video once");
   video_st->poke = &poke;
   mode_flags = 0;
   BIT32_SET(mode_flags, GFX_CTX_FLAGS_FULLSCREEN_IN_PLACE);
   CHECK(command_event_resize_windowed_scale(settings, 2),
         "unsupported live resize must recover");
   CHECK(modes == before_modes + 1 && inits == before + 2,
         "fullscreen capability alone must not attempt a live resize");
   CHECK(audio_inits == before_audio_inits,
         "resize recovery must not reopen audio with menu sounds enabled");
   core_info_get_current_core(&after_core);
   CHECK(before_core && before_core == after_core,
         "resize recovery must retain core metadata");
   video_st->poke = saved_poke;
   video_st->av_info.geometry = saved_geometry;
   settings->bools.audio_enable_menu = false;
}
#endif


static unsigned latency_cursor_calls;
static unsigned latency_aspect_calls;
static unsigned latency_record_frees;

static void latency_show_mouse(void *data, bool visible)
{
   (void)data;
   (void)visible;
   latency_cursor_calls++;
}

static void latency_set_aspect(void *data, unsigned aspect)
{
   (void)data;
   (void)aspect;
   latency_aspect_calls++;
}

static void latency_record_free(void *data)
{
   (void)data;
   latency_record_frees++;
}

#ifdef HAVE_MICROPHONE
extern microphone_driver_t microphone_null;
static unsigned latency_mic_inits;

static void *latency_mic_init(void)
{
   latency_mic_inits++;
   return NULL;
}
#endif

static void lane_hdr_restart_scope(void)
{
   rarch_setting_t *s = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_HDR_ENABLE);
   core_info_t *before_core = NULL;
   core_info_t *after_core = NULL;
   unsigned before_video = inits;
   unsigned before_audio = audio_inits;

   config_get_ptr()->bools.audio_enable_menu = true;
   core_info_get_current_core(&before_core);
   CHECK(s != NULL, "HDR mode row must exist");
   if (!s)
      return;
   *s->value.target.unsigned_integer = 1;
   menu_setting_generic(s, 0, false);
   CHECK(inits == before_video + 1, "HDR mode must rebuild video/input once");
   CHECK(audio_inits == before_audio, "HDR mode must retain audio");
   core_info_get_current_core(&after_core);
   CHECK(before_core == after_core, "HDR mode must retain core metadata");
   before_video = inits;
   *s->value.target.unsigned_integer = 0;
   menu_setting_generic(s, 0, false);
   CHECK(inits == before_video + 1, "disabling HDR must rebuild video/input once");
   CHECK(audio_inits == before_audio, "disabling HDR must retain audio");
}

static void lane_input_restart_scope(void)
{
   rarch_setting_t *s = menu_setting_find_enum(
         MENU_ENUM_LABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME);
   menu_file_list_cbs_t cbs;
   char enum_s[16];
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *menu_stack = MENU_LIST_GET(menu_st->entries.list, 0);
   core_info_t *before_core = NULL;
   core_info_t *after_core = NULL;
   unsigned before_video = inits;
   unsigned before_audio = audio_inits;

   config_get_ptr()->bools.audio_enable_menu = true;
   core_info_get_current_core(&before_core);
   CHECK(s != NULL, "reserved device row must exist");
   if (!s)
      return;
   strlcpy(s->value.target.string, "1234:5678 Test controller", s->size);
   menu_action_handle_setting(s, 0, MENU_ACTION_START, false);
   CHECK(!s->value.target.string[0], "Start must clear the reserved device");
   CHECK(inits == before_video + 1, "reservation reset must rebuild video/input once");
   CHECK(audio_inits == before_audio, "reservation reset must retain audio");
   core_info_get_current_core(&after_core);
   CHECK(before_core == after_core, "reservation reset must retain core metadata");

   memset(&cbs, 0, sizeof(cbs));
   menu_cbs_init_bind_ok(&cbs, NULL, "", 0,
         MENU_SETTING_DROPDOWN_ITEM_INPUT_SELECT_RESERVED_DEVICE, 0, "", 0);
   CHECK(menu_stack && cbs.action_ok, "reserved device dropdown must bind OK");
   if (menu_stack && cbs.action_ok)
   {
      snprintf(enum_s, sizeof(enum_s), "%d",
            (int)MENU_ENUM_LABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME);
      menu_entries_append(menu_stack, enum_s, "", MSG_UNKNOWN,
            MENU_SETTING_ACTION, 0, 0, NULL);
      strlcpy(s->value.target.string, "1234:5678 Test controller", s->size);
      before_video = inits;
      cbs.action_ok(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), "", 0, 0, 0);
      CHECK(!s->value.target.string[0], "Off must clear the reserved device");
      CHECK(inits == before_video + 1, "reservation dropdown must rebuild video/input once");
      CHECK(audio_inits == before_audio, "reservation dropdown must retain audio");
      core_info_get_current_core(&after_core);
      CHECK(before_core == after_core, "reservation dropdown must retain core metadata");
   }

   before_video = inits;
   input_driver_platform_request(INPUT_PLATFORM_REINIT_VIDEO);
   CHECK(inits == before_video + 1, "surface recovery must rebuild video/input once");
   CHECK(audio_inits == before_audio, "surface recovery must retain audio");
   core_info_get_current_core(&after_core);
   CHECK(before_core == after_core, "surface recovery must retain core metadata");
}

static void lane_font_fallback(void)
{
   rarch_setting_t *s = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_FONT_SIZE);
   menu_file_list_cbs_t cbs;
   core_info_t *before_core = NULL;
   core_info_t *after_core = NULL;
   unsigned before_video = inits;
   unsigned before_audio = audio_inits;

   config_get_ptr()->bools.audio_enable_menu = true;
   core_info_get_current_core(&before_core);
   CHECK(s != NULL, "font size row must exist");
   if (!s)
      return;
   menu_action_handle_setting(s, 0, MENU_ACTION_RIGHT, false);
   core_info_get_current_core(&after_core);
   CHECK(inits == before_video + 1, "private font fallback must rebuild video once");
   CHECK(audio_inits == before_audio, "private font fallback must retain audio");
   CHECK(before_core == after_core, "private font fallback must retain core metadata");
   s = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_FONT_PATH);
   CHECK(s != NULL, "font path row must exist");
   if (!s)
      return;
   before_video = inits;
   strlcpy(s->value.target.string, "null", s->size);
   menu_setting_generic(s, 0, false);
   CHECK(inits == before_video + 1, "font path fallback must rebuild video once");
   CHECK(audio_inits == before_audio, "font path fallback must retain audio");

   memset(&cbs, 0, sizeof(cbs));
   cbs.setting = s;
   menu_cbs_init_bind_scan(&cbs, NULL, MENU_ENUM_LABEL_VIDEO_FONT_PATH_STR, 0, 0);
   CHECK(cbs.action_scan != NULL, "font path reset must bind its scan callback");
   if (!cbs.action_scan)
      return;
   before_video = inits;
   cbs.action_scan(NULL, MENU_ENUM_LABEL_VIDEO_FONT_PATH_STR, 0, 0);
   core_info_get_current_core(&after_core);
   CHECK(inits == before_video + 1, "font path reset must rebuild video once");
   CHECK(audio_inits == before_audio, "font path reset must retain audio");
   CHECK(before_core == after_core, "font path reset must retain core metadata");
}

static void lane_minimum_audio_latency(void)
{
   settings_t *settings = config_get_ptr();
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   recording_state_t *rec_st = recording_state_get_ptr();
   const video_poke_interface_t *saved_poke = video_st->poke;
   video_poke_interface_t poke = *saved_poke;
   void *saved_record_data = rec_st->data;
   const record_driver_t *saved_record_driver = rec_st->driver;
   static record_driver_t record_driver;
   static unsigned record_cookie;
   core_info_t *before_core = NULL;
   core_info_t *after_core = NULL;
   unsigned before_video = inits;
   unsigned before_audio = audio_inits;
   unsigned request;
   void *saved_video_data;
#ifdef HAVE_MICROPHONE
   void *(*saved_mic_init)(void) = microphone_null.init;
   unsigned before_mic;
#endif

   settings->bools.audio_enable = true;
   settings->bools.audio_enable_menu = true;
   settings->bools.video_fullscreen = true;
   settings->uints.audio_latency = 64;
   settings->uints.audio_latency_floor = 1;
   runloop_st->audio_latency = 0;
   poke.show_mouse = latency_show_mouse;
   poke.set_aspect_ratio = latency_set_aspect;
   video_st->poke = &poke;
   record_driver.free = latency_record_free;
   rec_st->data = &record_cookie;
   rec_st->driver = &record_driver;
   core_info_get_current_core(&before_core);
#ifdef HAVE_MICROPHONE
   settings->bools.microphone_enable = true;
   settings->uints.microphone_latency = 512;
   strlcpy(settings->arrays.microphone_driver, "null",
         sizeof(settings->arrays.microphone_driver));
   microphone_null.init = latency_mic_init;
   command_event(CMD_EVENT_MICROPHONE_REINIT, NULL);
   before_mic = latency_mic_inits;
#endif

   request = 128;
   CHECK(runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request),
         "minimum audio latency is supported");
   CHECK(audio_inits == before_audio + 1 && requested_audio_latency == 128,
         "changed latency must reopen audio once, including with menu sounds");
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(audio_inits == before_audio + 1, "identical latency must retain audio");
   request = 32;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(audio_inits == before_audio + 2 && requested_audio_latency == 64,
         "latency below the frontend default must use that default");
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, NULL);
   CHECK(audio_inits == before_audio + 2 && runloop_st->audio_latency == 0,
         "clearing an already-default request must retain audio");
   request = 999;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(audio_inits == before_audio + 3 && requested_audio_latency == 512
         && runloop_st->audio_latency == 512, "latency is capped at 512 ms");
   request = 513;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(audio_inits == before_audio + 3, "the same capped latency must retain audio");
   request = 64;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(audio_inits == before_audio + 4 && requested_audio_latency == 64,
         "lowering latency to the default must reopen audio once");
#ifdef HAVE_MICROPHONE
   CHECK(latency_mic_inits == before_mic,
         "a microphone with an unchanged latency floor must survive");
#endif

   runloop_st->content_switching = true;
   request = 256;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(audio_inits == before_audio + 4 && runloop_st->audio_latency == 256,
         "staged loading must store latency without restarting drivers");
   runloop_st->content_switching = false;
   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   CHECK(audio_inits == before_audio + 5 && requested_audio_latency == 256,
         "the deferred request must be used when audio starts");
   saved_video_data = video_st->data;
   video_st->data = NULL;
   request = 512;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(audio_inits == before_audio + 5 && runloop_st->audio_latency == 512,
         "a request before driver startup must not open audio early");
   video_st->data = saved_video_data;
   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   CHECK(audio_inits == before_audio + 6 && requested_audio_latency == 512,
         "audio startup must use the stored request");
   request = 64;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
#ifdef HAVE_MICROPHONE
   settings->uints.microphone_latency = 16;
   command_event(CMD_EVENT_MICROPHONE_REINIT, NULL);
   before_mic = latency_mic_inits;
   request = 128;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(latency_mic_inits == before_mic + 1,
         "a changed microphone latency floor must reopen the microphone");
   request = 32;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(latency_mic_inits == before_mic + 2 && requested_audio_latency == 64,
         "microphone and audio floors must follow their own limits");
   before_audio = audio_inits;
   request = 16;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(latency_mic_inits == before_mic + 3 && audio_inits == before_audio,
         "a microphone-only latency change must retain playback audio");
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, NULL);
   CHECK(latency_mic_inits == before_mic + 3 && audio_inits == before_audio,
         "unchanged playback and microphone floors must retain both drivers");
   runloop_st->content_switching = true;
   request = 32;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(latency_mic_inits == before_mic + 3 && audio_inits == before_audio
         && runloop_st->audio_latency == 32,
         "staged loading must defer a microphone-only latency change");
   runloop_st->content_switching = false;
   command_event(CMD_EVENT_MICROPHONE_REINIT, NULL);
   saved_video_data = video_st->data;
   video_st->data = NULL;
   request = 64;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(latency_mic_inits == before_mic + 4 && audio_inits == before_audio,
         "microphone-only latency requests must wait for driver startup");
   video_st->data = saved_video_data;
   command_event(CMD_EVENT_MICROPHONE_REINIT, NULL);
   settings->bools.microphone_enable = false;
   request = 128;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(latency_mic_inits == before_mic + 5,
         "a disabled microphone must not be restarted");
   microphone_driver_deinit(false);
   microphone_null.init = saved_mic_init;
#endif


   runloop_st->audio_latency = 64;
   settings->uints.audio_latency_floor = 256;
   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   before_audio = audio_inits;
   request = 128;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   request = 256;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(audio_inits == before_audio && requested_audio_latency == 256,
         "requests satisfied by the configured floor must retain audio");
   request = 512;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(audio_inits == before_audio + 1 && requested_audio_latency == 512,
         "a request exceeding the configured floor must reopen audio once");
   request = 128;
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, &request);
   CHECK(audio_inits == before_audio + 2 && requested_audio_latency == 256,
         "lowering latency must stop at the configured floor");
   runloop_environment_cb(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY, NULL);
   CHECK(audio_inits == before_audio + 2,
         "clearing a request below the configured floor must retain audio");

   core_info_get_current_core(&after_core);
   CHECK(inits == before_video && before_core && before_core == after_core,
         "minimum latency changes must retain video and core metadata");
   CHECK(latency_cursor_calls == 0 && latency_aspect_calls == 0,
         "minimum latency changes must not change cursor or video aspect");
   CHECK(latency_record_frees == 0 && rec_st->data == &record_cookie
         && rec_st->driver == &record_driver,
         "minimum latency changes must keep the current recording");
   rec_st->data = saved_record_data;
   rec_st->driver = saved_record_driver;
   video_st->poke = saved_poke;
}

/* The frontend keeps more than the config beside it. */
static void scratch_remove(const char *dir)
{
   size_t i;
   struct string_list *files = dir_list_new(dir, NULL,
         false, true, false, false);

   for (i = 0; files && i < files->size; i++)
      filestream_delete(files->elems[i].data);
   if (files)
      dir_list_free(files);
   rmdir(dir);
}

int main(int argc, char *argv[])
{
   char dir[512];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc = 0;
   FILE *cfg;

   (void)argc;
   (void)argv;

   /* Every init, the reinit a press performs included, takes this. */
   audio_null.init = harness_audio_init;
   null_init       = video_null.init;
   video_null.init = harness_init;

   /* TMPDIR, then TEMP (Windows), then /tmp. */
   {
      const char *tmp = getenv("TMPDIR");
      if (!tmp || !*tmp)
         tmp = getenv("TEMP");
      if (!tmp || !*tmp)
         tmp = "/tmp";
      snprintf(dir, sizeof(dir), "%s/setting_command_%ld",
            tmp, (long)getpid());
   }
   if (!path_mkdir(dir))
      return 1;

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);

   snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", dir);
   if ((cfg = fopen(cfg_path, "wb")))
   {
      fprintf(cfg, "video_driver = \"null\"\n");
      fprintf(cfg, "audio_driver = \"null\"\n");
      fprintf(cfg, "input_driver = \"null\"\n");
      fprintf(cfg, "input_joypad_driver = \"null\"\n");
      fprintf(cfg, "menu_driver = \"rgui\"\n");
      fprintf(cfg, "video_threaded = \"false\"\n");
      fprintf(cfg, "video_fullscreen = \"false\"\n");
      fprintf(cfg, "input_overlay_enable = \"false\"\n");
      fprintf(cfg, "overlay_directory = \"%s\"\n", dir);
      fprintf(cfg, "osk_overlay_directory = \"%s\"\n", dir);
      fclose(cfg);
   }

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }

   retroarch_menu_running();
   CHECK(inits > 0, "fixture: the counting init never ran");

   lane(MENU_ENUM_LABEL_VIDEO_SMOOTH, "video_smooth", false);
   lane(MENU_ENUM_LABEL_MENU_TEXTURE_MIPMAPPING,
         "menu_texture_mipmapping", true);
#ifndef RARCH_MOBILE
   /* Mobile has no window scale row */
   lane_start_picks(MENU_ENUM_LABEL_VIDEO_SCALE, "video_scale");
#endif
#ifdef HAVE_OVERLAY
   lane_overlay_preset_start(MENU_ENUM_LABEL_OVERLAY_PRESET,
         "input_overlay", OVERLAY_DEFAULT);
   lane_overlay_preset_start(MENU_ENUM_LABEL_OSK_OVERLAY_PRESET,
         "input_osk_overlay", OSK_OVERLAY_DEFAULT);
   lane_overlay_opacity_start();
#endif

   lane_fullscreen();
#ifndef RARCH_MOBILE
   lane_window_scale();
#endif

   lane_hdr_restart_scope();
   lane_input_restart_scope();
   lane_font_fallback();
   lane_minimum_audio_latency();

   scratch_remove(dir);

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("setting_command: all lanes passed\n");
   return 0;
}
