/* Monitor Index dropdown harness.
 *
 * Links the shipping RetroArch objects (see build.sh) with only main()
 * replaced, and video_display_server_list_outputs() wrapped at link
 * time so each lane scripts how many monitors the display server
 * lists. Settings > Video > Output is opened the way the menu opens
 * it, then the Monitor Index dropdown, and its rows are counted.
 *
 * The claim: the dropdown offers Auto plus one row per listed monitor;
 * an index chosen past the monitors there are stays listed; a display
 * server without an output list keeps the setting's full range.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/monitor_index/build.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <lists/file_list.h>
#include <string/stdstring.h>
#include <time/rtime.h>
#include <file/config_file.h>

#include "../../../msg_hash.h"
#include "../../../menu/menu_defines.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
#include "../../../menu/menu_displaylist.h"
#include "../../../menu/menu_setting.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../config.def.h"
#include "../../../gfx/video_display_server.h"
#include "../../../frontend/frontend_driver.h"

static unsigned failures;
static int      scripted_outputs;

#define CHECK(cond, what) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, what); \
   failures++; } } while (0)

/* -1 is a server without an output list */
int __wrap_video_display_server_list_outputs(video_output_info_t *out,
      int max)
{
   int i;
   if (scripted_outputs < 0)
      return -1;
   for (i = 0; i < scripted_outputs && i < max; i++)
   {
      memset(&out[i], 0, sizeof(out[i]));
      out[i].id      = i;
      out[i].dims    = VIDEO_SCALE_PACK(1920, 1080);
      out[i].primary = (i == 0);
      snprintf(out[i].name, sizeof(out[i].name), "OUT-%d", i);
   }
   return i;
}

static file_list_t *selection_buf(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   return menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
}

static void push_list(enum menu_displaylist_ctl_state type,
      const char *path, const char *label, enum msg_hash_enums enum_idx)
{
   menu_displaylist_info_t info;
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *buf           = selection_buf();
   file_list_t *menu_stack    = MENU_LIST_GET(menu_st->entries.list, 0);

   if (!buf || !menu_stack)
      return;
   menu_entries_clear(buf);
   menu_entries_append(menu_stack, path, label, enum_idx,
         MENU_SETTING_ACTION, 0, 0, NULL);
   menu_displaylist_info_init(&info);
   info.list          = buf;
   info.path          = strdup(path);
   info.label         = strdup(label);
   info.enum_idx      = enum_idx;
   info.type          = MENU_SETTING_ACTION;
   info.directory_ptr = 0;
   menu_displaylist_ctl(type, &info, config_get_ptr());
   menu_displaylist_process(&info);
   menu_displaylist_info_free(&info);
}

/* Opens Video > Output with @outputs monitors listed, then the Monitor
 * Index dropdown; returns its row count. */
static size_t dropdown_rows(int outputs, unsigned value)
{
   char enum_s[16];
   file_list_t *buf;
   scripted_outputs = outputs;
   configuration_set_uint(config_get_ptr(),
         config_get_ptr()->uints.video_monitor_index, value);
   push_list(DISPLAYLIST_VIDEO_OUTPUT_SETTINGS_LIST, "",
         msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_VIDEO_OUTPUT_SETTINGS_LIST),
         MENU_ENUM_LABEL_DEFERRED_VIDEO_OUTPUT_SETTINGS_LIST);
   snprintf(enum_s, sizeof(enum_s), "%d",
         (int)MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX);
   push_list(DISPLAYLIST_DROPDOWN_LIST, enum_s, "", MSG_UNKNOWN);
   buf = selection_buf();
   return buf ? buf->size : 0;
}

static bool row_listed(const char *prefix)
{
   file_list_t *buf = selection_buf();
   size_t i;
   for (i = 0; buf && i < buf->size; i++)
      if (string_starts_with(buf->list[i].path, prefix))
         return true;
   return false;
}

int main(int argc, char *argv[])
{
   static char cfg_path[640];
   char fixture_dir[128], cmd[300];
   char *rarch_argv[8];
   int rarch_argc = 0;
   size_t n;

   (void)argc; (void)argv;
   snprintf(fixture_dir, sizeof(fixture_dir), "/tmp/monitor_index_%ld",
         (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
      return 1;
   snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", fixture_dir);
   {
      FILE *cfg = fopen(cfg_path, "wb");
      if (!cfg)
         return 1;
      fprintf(cfg, "video_driver = \"null\"\naudio_driver = \"null\"\n"
            "input_driver = \"null\"\ninput_joypad_driver = \"null\"\n"
            "menu_driver = \"rgui\"\nvideo_threaded = \"false\"\n"
            "menu_show_advanced_settings = \"true\"\n");
      fclose(cfg);
   }
   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   if (!selection_buf())
   {
      fprintf(stderr, "FAIL: the menu came up without an entry list\n");
      return 1;
   }

   /* One monitor: Auto and 1 */
   n = dropdown_rows(1, 0);
   CHECK(n == 2, "one monitor: Auto and 1");
   CHECK(row_listed("1 (OUT-0"), "one monitor: row 1 names its output");

   /* Three monitors: Auto and 1..3 */
   n = dropdown_rows(3, 2);
   CHECK(n == 4, "three monitors: Auto and 1..3");

   /* Index 5 chosen with two monitors: it stays listed */
   n = dropdown_rows(2, 5);
   CHECK(n == 6, "chosen past the monitors: Auto and 1..5");
   CHECK(row_listed("5"), "chosen past the monitors: row 5 listed");

   /* No output list: the full range comes back */
   n = dropdown_rows(-1, 0);
   CHECK(n == 16, "no output list: the full range");

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { /* fixture dir left behind; harmless */ }

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("monitor_index: all lanes passed\n");
   return 0;
}
