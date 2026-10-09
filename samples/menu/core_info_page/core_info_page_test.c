/* The running core's information is there to show.
 *
 * Links the shipping RetroArch objects with only main() replaced and
 * boots the frontend with the null drivers on a core that has an info
 * file (both in a scratch directory, the core's directory and the info
 * directory). Once it runs, and again after the drivers are set up
 * again under it:
 *  - the current-core entry carries the info file (CORE_INFO_FLAG_HAS_INFO,
 *    its display name);
 *  - the Information menu lists Core Information first;
 *  - the Core Information page shows the info file, not "No Core
 *    Information Available".
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/core_info_page/build.sh
 *   samples/menu/core_info_page/core_info_page_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <lists/file_list.h>
#include <time/rtime.h>
#include <queues/task_queue.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../command.h"
#include "../../../core_info.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../frontend/frontend.h"
#include "../../../verbosity.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
#include "../../../menu/menu_displaylist.h"
#include "../../../msg_hash.h"

static unsigned failures = 0;

#define CHECK(cond, what) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, what); \
         failures++; \
      } \
   } while (0)

#define DISPLAY_NAME "Info Page Core (Harness)"

static void pump(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      runloop_iterate();
      task_queue_check();
   }
}

/* The menu list a displaylist type builds, as the menu builds it */
static file_list_t *build(enum menu_displaylist_ctl_state type,
      unsigned info_type, const char *label)
{
   menu_displaylist_info_t info;
   file_list_t *list = (file_list_t*)calloc(1, sizeof(*list));
   if (!list)
      return NULL;
   menu_displaylist_info_init(&info);
   info.list  = list;
   info.type  = info_type;
   info.label = strdup(label);
   info.path  = strdup("");
   menu_displaylist_ctl(type, &info, config_get_ptr());
   menu_displaylist_info_free(&info);
   return list;
}

/* "<when>: <what>", for a CHECK */
static const char *what(const char *when, const char *msg)
{
   static char buf[256];
   snprintf(buf, sizeof(buf), "%s: %s", when, msg);
   return buf;
}

/* The current-core entry, the Information menu and the Core Information
 * page all carry the running core's info file */
static void check_page(const char *when)
{
   unsigned had = failures;

   /* The current-core entry */
   {
      core_info_t *ci = NULL;
      core_info_get_current_core(&ci);
      CHECK(ci != NULL, what(when, "no current-core entry"));
      if (ci)
      {
         CHECK(ci->flags & CORE_INFO_FLAG_HAS_INFO,
               what(when, "the current-core entry has no info file"));
         CHECK(string_is_equal(ci->display_name, DISPLAY_NAME),
               what(when, "the current-core entry is not the info file's"));
      }
   }

   /* Information menu: Core Information first */
   {
      file_list_t *list = build(DISPLAYLIST_INFORMATION_LIST,
            0, msg_hash_to_str(MENU_ENUM_LABEL_INFORMATION_LIST));
      CHECK(list && list->size > 0 && list->list[0].label
            && string_is_equal(list->list[0].label,
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_INFORMATION)),
            what(when, "the Information menu does not list Core Information first"));
      file_list_free(list);
   }

   /* Core Information page */
   {
      size_t i;
      bool named = false;
      file_list_t *list = build(DISPLAYLIST_CORE_INFO,
            0, msg_hash_to_str(MENU_ENUM_LABEL_CORE_INFORMATION));
      CHECK(list && list->size > 1,
            what(when, "the Core Information page is empty"));
      for (i = 0; list && i < list->size; i++)
      {
         const char *path = list->list[i].path;
         CHECK(!list->list[i].label || !string_is_equal(list->list[i].label,
               msg_hash_to_str(MENU_ENUM_LABEL_NO_CORE_INFORMATION_AVAILABLE)),
               what(when, "the Core Information page says there is none"));
         if (path && strstr(path, DISPLAY_NAME))
            named = true;
      }
      CHECK(named, what(when, "the Core Information page does not name the core"));
      file_list_free(list);
   }

   if (failures == had)
      fprintf(stderr, "[pass] %s: the core's information is shown\n", when);
}

int main(int argc, char *argv[])
{
   char dir[400];
   char cfg_path[512];
   char core_src[512];
   char core_path[600];
   char info_path[600];
   char *rarch_argv[8] = {0};
   int rarch_argc      = 0;
   const char *slash;
   int dirlen;
   FILE *f;
   (void)argc;

   {
      const char *tmp = getenv("TMPDIR");
      if (!tmp || !*tmp)
         tmp = getenv("TEMP");
      if (!tmp || !*tmp)
         tmp = "/tmp";
      snprintf(dir, sizeof(dir), "%s/core_info_page_%ld", tmp, (long)getpid());
   }
   if (!path_mkdir(dir))
      return 1;

   /* The core built next to this binary, copied in with its info file,
    * both named as the core info list pairs them */
   slash  = strrchr(argv[0], '/');
   dirlen = slash ? (int)(slash - argv[0]) : 1;
   snprintf(core_src, sizeof(core_src), "%.*s/info_core.so",
         dirlen, slash ? argv[0] : ".");
   snprintf(core_path, sizeof(core_path), "%s/info_core_libretro.so", dir);
   {
      int64_t len = 0;
      void *buf   = NULL;
      if (!filestream_read_file(core_src, &buf, &len)
            || !filestream_write_file(core_path, buf, len))
      {
         fprintf(stderr, "FAIL: could not copy %s\n", core_src);
         return 1;
      }
      free(buf);
   }
   snprintf(info_path, sizeof(info_path), "%s/info_core_libretro.info", dir);
   if (!(f = fopen(info_path, "wb")))
      return 1;
   fprintf(f, "display_name = \"%s\"\n", DISPLAY_NAME);
   fprintf(f, "authors = \"Harness\"\n");
   fprintf(f, "supported_extensions = \"bin\"\n");
   fprintf(f, "corename = \"Info Page Core\"\n");
   fprintf(f, "license = \"MIT\"\n");
   fprintf(f, "permissions = \"\"\n");
   fprintf(f, "display_version = \"1\"\n");
   fprintf(f, "categories = \"Emulator\"\n");
   fprintf(f, "systemname = \"Harness\"\n");
   fprintf(f, "supports_no_game = \"true\"\n");
   fclose(f);

   snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", dir);
   if (!(f = fopen(cfg_path, "wb")))
      return 1;
   fprintf(f, "video_driver = \"null\"\n");
   fprintf(f, "audio_driver = \"null\"\n");
   fprintf(f, "input_driver = \"null\"\n");
   fprintf(f, "input_joypad_driver = \"null\"\n");
   fprintf(f, "menu_driver = \"rgui\"\n");
   fprintf(f, "video_threaded = \"false\"\n");
   fprintf(f, "menu_pause_libretro = \"true\"\n");
   fprintf(f, "config_save_on_exit = \"false\"\n");
   fprintf(f, "libretro_directory = \"%s\"\n", dir);
   fprintf(f, "libretro_info_path = \"%s\"\n", dir);
   fprintf(f, "core_info_cache_enable = \"false\"\n");
   fclose(f);

   if (getenv("HARNESS_VERBOSE"))
      verbosity_enable();

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   runloop_msg_queue_init();

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;
   rarch_argv[rarch_argc++] = (char*)"-L";
   rarch_argv[rarch_argc++] = core_path;
   if (getenv("HARNESS_VERBOSE"))
      rarch_argv[rarch_argc++] = (char*)"-v";

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   pump(5);
   CHECK(runloop_state_get_ptr()->current_core_type != CORE_TYPE_DUMMY,
         "the core did not start");
   if (!(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   pump(2);

   check_page("the core up");

   /* The drivers set up again under the running core */
   command_event(CMD_EVENT_REINIT, NULL);
   pump(2);
   check_page("after a driver reinit");

   main_exit(NULL);

   remove(cfg_path);
   remove(info_path);
   remove(core_path);
   path_rmdir(dir);

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "[pass] core_info_page\n");
   return 0;
}
