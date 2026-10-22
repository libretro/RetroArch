/* Playlists-tab Scan harness.
 *
 * On the Playlists tab, Scan jumps to a random playlist other than
 * the selected one.  Each menu driver drew until it hit one, so with
 * no other playlist to land on - none at all, or only the selected
 * one - the draw never ended and the menu froze for good.
 *
 * This links the shipping RetroArch objects with only main()
 * replaced (see build.sh), so Scan goes through the menu driver's own
 * action handler.  A watchdog turns a freeze into a failure.
 *
 * Ozone also offers Scan in its sidebar, where each playlist is a tab,
 * and drew tabs the same way: one playlist tab, already selected, froze
 * it; with none, it jumped to a tab past the end.  Ozone builds those
 * tabs at start-up, so each count is its own run.
 *
 * Usage: playlists_scan_test [menu_driver]   (default rgui)
 *        playlists_scan_test ozone sidebar<N>   (N playlist tabs, 0-2)
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <lists/file_list.h>
#include <string/stdstring.h>

#include "../../../msg_hash_lbl_str.h"
#include "../../../menu/menu_defines.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
#include "../../../menu/menu_displaylist.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../frontend/frontend_driver.h"
#include <time/rtime.h>
#include <file/config_file.h>

/* Seconds one Scan press may take before it counts as a freeze. */
#define SCAN_WATCHDOG_S 5

static unsigned failures = 0;
static const char *current_lane = "start-up";

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

static char fixture_dir[512];

static void watchdog(int sig)
{
   static const char msg[] = "FAIL: Scan never returned (menu frozen) in lane ";
   (void)sig;
   if (write(2, msg, sizeof(msg) - 1) < 0) { }
   if (write(2, current_lane, strlen(current_lane)) < 0) { }
   if (write(2, "\n", 1) < 0) { }
   _exit(1);
}

static bool write_playlist(const char *dir, const char *name)
{
   char path[700];
   FILE *f;
   snprintf(path, sizeof(path), "%s/%s", dir, name);
   if (!(f = fopen(path, "wb")))
      return false;
   /* The game path carries the playlist's name, so a list shows
    * which playlist it came from. */
   fprintf(f, "{\n  \"version\": \"1.5\",\n  \"items\": [\n"
              "    { \"path\": \"/games/%s.bin\", \"label\": \"a\","
              " \"core_path\": \"DETECT\", \"core_name\": \"DETECT\","
              " \"crc32\": \"00000000|crc\", \"db_name\": \"t.lpl\" }\n"
              "  ]\n}\n", name);
   fclose(f);
   return true;
}

static file_list_t *selection_buf(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   return menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
}

/* Builds the Playlists tab from @dir and tells the driver it is on
 * it, which is what a push of that tab does. */
static size_t open_playlists_tab(const char *dir)
{
   menu_displaylist_info_t info;
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *buf           = selection_buf();
   file_list_t *menu_stack    = MENU_LIST_GET(menu_st->entries.list, 0);
   settings_t *settings       = config_get_ptr();

   if (!buf || !menu_stack)
      return 0;

   strlcpy(settings->paths.directory_playlist, dir,
         sizeof(settings->paths.directory_playlist));

   menu_entries_clear(buf);
   menu_entries_append(menu_stack, dir,
         MENU_ENUM_LABEL_PLAYLISTS_TAB_STR,
         MENU_ENUM_LABEL_PLAYLISTS_TAB,
         MENU_SETTING_ACTION, 0, 0, NULL);

   menu_displaylist_info_init(&info);
   info.list          = buf;
   info.path          = strdup(dir);
   info.label         = strdup(MENU_ENUM_LABEL_PLAYLISTS_TAB_STR);
   info.enum_idx      = MENU_ENUM_LABEL_PLAYLISTS_TAB;
   info.type          = MENU_SETTING_ACTION;
   info.directory_ptr = 0;

   menu_displaylist_ctl(DISPLAYLIST_DATABASE_PLAYLISTS, &info, settings);
   menu_displaylist_process(&info);
   menu_displaylist_info_free(&info);

   if (menu_st->driver_ctx->populate_entries)
      menu_st->driver_ctx->populate_entries(menu_st->userdata, dir,
            MENU_ENUM_LABEL_PLAYLISTS_TAB_STR, MENU_SETTING_ACTION);

   return buf->size;
}

static size_t count_collections(file_list_t *buf, size_t *first)
{
   size_t i;
   size_t n = 0;
   for (i = 0; i < buf->size; i++)
   {
      if (buf->list[i].type != FILE_TYPE_PLAYLIST_COLLECTION)
         continue;
      if (!n && first)
         *first = i;
      n++;
   }
   return n;
}

/* Presses Scan on @selection through the driver's action handler,
 * under the watchdog. */
static void press_scan(size_t selection)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_entry_t entry;

   menu_st->selection_ptr = selection;
   MENU_ENTRY_INITIALIZE(entry);
   menu_entry_get(&entry, 0, selection, NULL, true);

   alarm(SCAN_WATCHDOG_S);
   menu_entry_action(&entry, selection, MENU_ACTION_SCAN);
   alarm(0);
}

static bool make_dir(const char *leaf, char *out, size_t len)
{
   char cmd[700];
   snprintf(out, len, "%s/%s", fixture_dir, leaf);
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", out);
   return system(cmd) == 0;
}

/* No playlists at all: Scan has nothing to land on and must return. */
static void lane_no_playlists(void)
{
   char dir[600];
   current_lane = "no-playlists";
   if (!make_dir("none", dir, sizeof(dir)))
   {
      CHECK(false, "could not create %s", dir);
      return;
   }
   open_playlists_tab(dir);
   CHECK(count_collections(selection_buf(), NULL) == 0,
         "the empty playlist folder produced a playlist entry");
   press_scan(0);
   CHECK(menu_state_get_ptr()->selection_ptr == 0,
         "Scan moved off the only row with no playlist to land on");
}

/* One playlist, already selected: no OTHER playlist exists, so Scan
 * must return and leave the selection where it is. */
static void lane_one_playlist_selected(void)
{
   char dir[600];
   size_t first = 0;
   current_lane = "one-playlist-selected";
   if (!make_dir("one", dir, sizeof(dir)) || !write_playlist(dir, "a.lpl"))
   {
      CHECK(false, "could not create the one-playlist fixture");
      return;
   }
   open_playlists_tab(dir);
   if (count_collections(selection_buf(), &first) != 1)
   {
      CHECK(false, "expected one playlist entry on the tab");
      return;
   }
   press_scan(first);
   CHECK(menu_state_get_ptr()->selection_ptr == first,
         "Scan moved off the only playlist (now %u)",
         (unsigned)menu_state_get_ptr()->selection_ptr);
}

/* Two playlists: every Scan must land on the other one. */
static void lane_two_playlists_alternate(void)
{
   char dir[600];
   size_t first = 0;
   size_t sel;
   unsigned k;
   current_lane = "two-playlists";
   if (   !make_dir("two", dir, sizeof(dir))
       || !write_playlist(dir, "a.lpl")
       || !write_playlist(dir, "b.lpl"))
   {
      CHECK(false, "could not create the two-playlist fixture");
      return;
   }
   open_playlists_tab(dir);
   if (count_collections(selection_buf(), &first) != 2)
   {
      CHECK(false, "expected two playlist entries on the tab");
      return;
   }
   sel = first;
   for (k = 0; k < 20; k++)
   {
      size_t now;
      press_scan(sel);
      now = menu_state_get_ptr()->selection_ptr;
      if (   now == sel
          || now >= selection_buf()->size
          || selection_buf()->list[now].type != FILE_TYPE_PLAYLIST_COLLECTION)
      {
         CHECK(false, "Scan %u went from %u to %u, not to the other playlist",
               k, (unsigned)sel, (unsigned)now);
         return;
      }
      sel = now;
   }
}

/* Names the open Ozone tab: the label on top of the menu stack, and
 * for a playlist tab the game path of its first row, which names the
 * playlist (see write_playlist). */
static void open_tab(char *s, size_t len)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *menu_stack    = MENU_LIST_GET(menu_st->entries.list, 0);
   file_list_t *buf           = selection_buf();
   const char *top            = "";
   const char *row            = "";

   if (menu_stack && menu_stack->size
         && menu_stack->list[menu_stack->size - 1].label)
      top = menu_stack->list[menu_stack->size - 1].label;
   if (buf && buf->size && buf->list[0].label)
      row = buf->list[0].label;
   snprintf(s, len, "%s %s", top, row);
}

static void press(enum menu_action action)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   size_t selection           = menu_st->selection_ptr;
   menu_entry_t entry;

   MENU_ENTRY_INITIALIZE(entry);
   menu_entry_get(&entry, 0, selection, NULL, true);

   alarm(SCAN_WATCHDOG_S);
   menu_entry_action(&entry, selection, action);
   alarm(0);
}

/* Ozone's sidebar, with @tabs playlist tabs: Left enters it on the
 * first tab, and Up wraps to the last tab, which is a playlist tab when
 * there is one.  Scan must land on another playlist tab or do nothing.
 * With no playlist tabs the cursor stays on the first tab: on the last
 * system tab, a wrong pick can land back where it started. */
static void lane_ozone_sidebar(unsigned tabs)
{
   char before[700];
   char after[700];
   unsigned k;

   current_lane = "ozone-sidebar";
   press(MENU_ACTION_LEFT);
   if (tabs)
      press(MENU_ACTION_UP);
   open_tab(before, sizeof(before));

   for (k = 0; k < 10; k++)
   {
      press(MENU_ACTION_SCAN);
      open_tab(after, sizeof(after));
      if (tabs < 2)
         CHECK(string_is_equal(before, after),
               "Scan with %u playlist tab(s) moved from '%s' to '%s'",
               tabs, before, after);
      else
         CHECK(   !string_is_equal(before, after)
               && strstr(after, ".lpl.bin"),
               "Scan %u moved from '%s' to '%s', not to the other "
               "playlist tab", k, before, after);
      strlcpy(before, after, sizeof(before));
   }
}

int main(int argc, char *argv[])
{
   char cmd[700];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc         = 0;
   const char *menu_drv   = (argc > 1) ? argv[1] : "rgui";
   int sidebar_tabs       = -1;

   signal(SIGALRM, watchdog);

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/menu_scan_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
      return 1;

   if (argc > 2 && !strncmp(argv[2], "sidebar", 7))
   {
      sidebar_tabs = atoi(argv[2] + 7);
      if (   (sidebar_tabs > 0 && !write_playlist(fixture_dir, "a.lpl"))
          || (sidebar_tabs > 1 && !write_playlist(fixture_dir, "b.lpl")))
         return 1;
   }

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   /* The same prelude rarch_main() runs before main_init; see
    * samples/menu/playlist_nav for why none of it can be skipped. */
   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   {
      FILE *cfg;
      snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", fixture_dir);
      if ((cfg = fopen(cfg_path, "wb")))
      {
         fprintf(cfg, "video_driver = \"null\"\n");
         fprintf(cfg, "audio_driver = \"null\"\n");
         fprintf(cfg, "input_driver = \"null\"\n");
         fprintf(cfg, "input_joypad_driver = \"null\"\n");
         fprintf(cfg, "menu_driver = \"%s\"\n", menu_drv);
         fprintf(cfg, "video_threaded = \"false\"\n");
         fprintf(cfg, "history_list_enable = \"false\"\n");
         fprintf(cfg, "menu_navigation_wraparound_enable = \"true\"\n");
         fprintf(cfg, "playlist_directory = \"%s\"\n", fixture_dir);
         fprintf(cfg, "savefile_directory = \"%s\"\n", fixture_dir);
         fprintf(cfg, "savestate_directory = \"%s\"\n", fixture_dir);
         fclose(cfg);
      }
   }

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }

   if (!selection_buf() || !menu_state_get_ptr()->driver_ctx)
   {
      fprintf(stderr, "FAIL: the menu came up without an entry list\n");
      return 1;
   }
   if (!string_is_equal(menu_state_get_ptr()->driver_ctx->ident, menu_drv))
   {
      fprintf(stderr, "FAIL: asked for menu driver %s, got %s\n",
            menu_drv, menu_state_get_ptr()->driver_ctx->ident);
      return 1;
   }

   if (sidebar_tabs >= 0)
      lane_ozone_sidebar((unsigned)sidebar_tabs);
   else
   {
      lane_two_playlists_alternate();
      lane_one_playlist_selected();
      lane_no_playlists();
   }

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { }

   if (failures)
   {
      fprintf(stderr, "FAIL playlists_scan_test (%s): %u failures\n",
            menu_drv, failures);
      return 1;
   }
   fprintf(stderr, "PASS playlists_scan_test (%s)\n", menu_drv);
   return 0;
}
