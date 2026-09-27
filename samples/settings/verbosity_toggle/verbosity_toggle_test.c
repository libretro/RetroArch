/* The menu's verbosity toggle transitions, it does not snap back.
 *
 * Links the shipping objects with only main() replaced, boots the
 * menu headless, finds the LOG_VERBOSITY setting, and drives its
 * own action handler - the exact path a person's right-press takes:
 * the framework writes the bound flag, then the change handler runs
 * the enable/disable transition with its side effects (console
 * attach, log-file init/deinit).
 *
 * The claims, from verbosity off: one press lands enabled - the
 * flag holds and, with log_to_file set, the log file is open; a
 * second press lands disabled with the file closed. A handler that
 * reads the post-write state as if it were the pre-press state
 * takes each transition backwards: the flag snaps back on every
 * press and the log file churns in the opposite direction of the
 * display.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/settings/verbosity_toggle/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <streams/file_stream.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../verbosity.h"
#include "../../../setting_list.h"
#include "../../../menu/menu_setting.h"
#include "../../../msg_hash.h"
#include "../../../frontend/frontend_driver.h"

static unsigned failures = 0;

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

static void press(rarch_setting_t *s)
{
   if (s->actions && s->actions->right)
      s->actions->right(s, 0, false);
}

int main(int argc, char *argv[])
{
   char fixture_dir[512];
   char cmd[700];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc = 0;
   rarch_setting_t *s;
   settings_t *settings;

   (void)argc;
   (void)argv;

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/verb_toggle_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
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
   {
      FILE *cfg;
      snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg",
            fixture_dir);
      if ((cfg = fopen(cfg_path, "wb")))
      {
         fprintf(cfg, "video_driver = \"null\"\n");
         fprintf(cfg, "audio_driver = \"null\"\n");
         fprintf(cfg, "input_driver = \"null\"\n");
         fprintf(cfg, "input_joypad_driver = \"null\"\n");
         fprintf(cfg, "menu_driver = \"rgui\"\n");
         /* Start silent, with the file sink configured so the
          * transition's file side is observable. */
         fprintf(cfg, "log_verbosity = \"false\"\n");
         fprintf(cfg, "log_to_file = \"true\"\n");
         fprintf(cfg, "log_to_file_timestamp = \"false\"\n");
         fprintf(cfg, "log_dir = \"%s\"\n", fixture_dir);
         fclose(cfg);
      }
   }

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   settings = config_get_ptr();
   retroarch_menu_running();

   /* The settings list is built when the menu is; the entry must be
    * findable and bool-bound to the verbosity flag. */
   s = menu_setting_find_enum(MENU_ENUM_LABEL_LOG_VERBOSITY);
   CHECK(s != NULL, "fixture: LOG_VERBOSITY setting not found");
   if (!s)
      goto done;
   CHECK(s->value.target.boolean == verbosity_get_ptr(),
         "fixture: setting is not bound to the verbosity flag");

   /* Baseline: off, no file. */
   CHECK(!verbosity_is_enabled(), "fixture: verbosity starts enabled");
   CHECK(!is_logging_to_file(), "fixture: file sink already open");

   /* Press 1: off -> on. The flag holds and the file opens. */
   press(s);
   CHECK(verbosity_is_enabled(),
         "press 1: verbosity snapped back to off");
   CHECK(is_logging_to_file(),
         "press 1: enabled but the log file did not open");

   /* Press 2: on -> off. */
   press(s);
   CHECK(!verbosity_is_enabled(),
         "press 2: verbosity snapped back to on");
   CHECK(!is_logging_to_file(),
         "press 2: disabled but the log file stayed open");

   /* Press 3: off -> on again - the transition is stable, not a
    * parity accident. */
   press(s);
   CHECK(verbosity_is_enabled(),
         "press 3: verbosity snapped back to off");

done:
   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { /* fixture dir left behind; harmless */ }

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("verbosity_toggle: all lanes passed\n");
   return 0;
}
