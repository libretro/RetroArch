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

#include "../../../command.h"
#include "../../../configuration.h"
#include "../../../file_path_special.h"
#include "../../../retroarch.h"
#include "../../../setting_list.h"
#include "../../../menu/menu_defines.h"
#include "../../../menu/menu_setting.h"
#include "../../../msg_hash.h"
#include "../../../gfx/video_driver.h"
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
   inits++;
   return null_init(video);
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
         "input_overlay", FILE_PATH_DEFAULT_OVERLAY);
   lane_overlay_preset_start(MENU_ENUM_LABEL_OSK_OVERLAY_PRESET,
         "input_osk_overlay", FILE_PATH_DEFAULT_OSK_OVERLAY);
   lane_overlay_opacity_start();
#endif

   scratch_remove(dir);

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("setting_command: all lanes passed\n");
   return 0;
}
