/* Check that every desktop companion backend exposes the same features.
 *
 * The companion UI has three presentation backends over one shared C
 * core (ui/companion/companion_core.*): Qt, native Win32, native Cocoa.
 * The plan for the refactor requires them to be interchangeable - "same
 * actions produce the same results" - but nothing enforces it: a backend
 * can quietly omit a feature (no file browser, no scan, no core picker)
 * and still compile and run, and the omission only surfaces when a user
 * switches drivers and finds a menu entry gone. That is exactly the
 * drift this check exists to catch, the same way vfs_backend_parity
 * catches a VFS backend that skips a call.
 *
 * Each feature below is defined by the companion-core entry point a
 * backend must call to implement it. A backend "has" the feature when it
 * references that symbol (as a whole word). Qt's presentation is split
 * across two translation units (ui_qt.cpp + ui_qt_widgets.cpp), so they
 * are checked together as one backend.
 *
 * This is a source-level grep, so it costs nothing and runs on Linux. It
 * is deliberately loose - it proves a backend wired the feature's core
 * call in at all, not that the UI is pixel-identical; visual parity is a
 * manual pass. What it reliably catches is a whole feature missing from
 * one backend.
 *
 * A backend that genuinely should not carry a feature must be listed in
 * waivers[] with a reason, so dropping one is a conscious, reviewed
 * choice rather than an oversight.
 *
 * The platforms' real main loops have to call the companion's iterate
 * too. Every companion is asynchronous on the far side of
 * ui_companion_driver_wimp_iterate(): the playlist parse, the browser
 * listing, thumbnails, animation frames, status. A platform main loop
 * that does not call it shows "Loading playlist..." for ever. The
 * generic loop (retroarch.c) covers Win32 and Qt; the non-Qt Mac build's
 * main loop is the CFRunLoop observer in cocoa_common.m, which once
 * lacked the call (the Qt-era rarch_main had it, under #ifdef HAVE_QT).
 *
 * Usage, from the repository root:
 *   tools/check.sh companion_parity            check, exit 1 on any gap
 *   tools/check.sh companion_parity --list     print the feature matrix */

#include "check_common.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* backend name -> the translation unit(s) that make up its presentation */
static const char *const backends[] = { "qt", "win32", "cocoa" };
static const char *const backend_files[][2] = {
   { "ui/drivers/ui_qt.cpp", "ui/drivers/ui_qt_widgets.cpp" },
   { "ui/drivers/ui_win32_companion.c", NULL },
   { "ui/drivers/ui_cocoa_companion.m", NULL }
};

/* feature -> the companion_core_* symbol whose use implements it. One
 * representative call per feature: the one a backend cannot skip and
 * still offer it. */
static const char *const features[][2] = {
   { "lifecycle",          "companion_core_new" },
   { "iterate",            "companion_core_iterate" },
   { "playlist list",      "companion_core_playlist_count" },
   { "playlist entries",   "companion_core_entry_count" },
   { "select playlist",    "companion_core_select_playlist" },
   { "run entry",          "companion_core_request_load_entry" },
   { "load content",       "companion_core_request_load_content" },
   { "start core",         "companion_core_start_core" },
   { "load core",          "companion_core_load_core" },
   { "installed cores",    "companion_core_installed_core_count" },
   { "core picker filter", "companion_core_installed_cores_supporting" },
   { "core association",   "companion_core_playlist_set_default_core" },
   { "delete entry",       "companion_core_playlist_delete_entry" },
   { "core info panel",    "companion_core_core_info_rows" },
   { "directory scan",     "companion_core_request_scan" },
   { "thumbnails",         "companion_core_thumbnail_path" },
   /* Decoding goes through the shared engine (threads, cache, visible-
    * first requests); a backend decoding on its own UI thread regresses
    * the grid to the "shows on click" behaviour this replaced. */
   { "thumbnail engine",   "companion_thumbs_request" },
   { "thumbnail poll",     "companion_thumbs_poll" },
   { "two-pane browser",   "companion_core_browse_dir_count" },
   /* The listing is enumerated off the UI thread; a backend that asks
    * whether it is still busy is one that waits for the callback rather
    * than rebuilding straight after open() (or enumerating itself). */
   { "async browser",      "companion_core_browse_busy" },
   { "browser sorting",    "companion_core_browse_sort" },
   /* The selected file's animation (APNG / WEBP / WEBM / MP4) plays in
    * the pane on every backend, as in RetroArch's own File Browser. */
   { "animated preview",   "companion_thumbs_animate" },
   /* Qt's Stop / File > Unload Core, and File > Exit */
   { "unload core",        "companion_core_unload_core" },
   { "exit retroarch",     "CMD_EVENT_QUIT" },
   /* Qt's playlist rename, "Add Files" / file drop, thumbnail drop */
   { "rename playlist",    "companion_core_playlist_rename" },
   { "add files",          "companion_core_playlist_add_files" },
   { "thumbnail drop",     "companion_core_thumbnail_install" },
   /* Qt's playlist context menu: hide / unhide, new, delete; whether a
    * playlist is hidden, which is what lists the hidden ones */
   { "hide playlist",      "companion_core_playlist_set_hidden" },
   { "hidden playlists",   "companion_core_playlist_is_hidden" },
   { "new playlist",       "companion_core_playlist_new" },
   { "delete playlist",    "companion_core_playlist_delete" },
   /* Qt's View > Core Options, View > Shader Parameters, View > Options */
   { "core options",       "companion_core_option_count" },
   { "shader parameters",  "companion_core_shader_param_count" },
   { "options dialog",     "companion_core_setting_count" },
   { "file browser",       "companion_core_browse_open" },
   { "pick core on run",   "companion_core_entry_needs_core" },
   { "window hand-off",    "companion_core_prepare_show_window" }
};

/* (backend, feature) pairs that are intentionally absent, with a
 * reason. */
static const char *const waivers[][3] = {
   /* Qt's "Add Files" is its own dialog (name, extension filter, core
    * choice, archive filter, progress) over
    * companion_core_playlist_push; the natives' plain add-files call is
    * the drop semantics only. */
   { "qt", "add files",
      "Qt: PlaylistEntryDialog over companion_core_playlist_push" },
   /* Qt's thumbnail drop receives image data (a QImage from any drag
    * source), not a file path, and saves through QImage; the core call
    * takes a path. */
   { "qt", "thumbnail drop", "Qt: QImage drop saved via changeThumbnail" },
   /* Qt hides rows in its own list widget (it asks the core whether a
    * playlist is hidden, and keeps the row) rather than reading the
    * filtered listing; new / delete are its own dialogs over the same
    * files. */
   { "qt", "hidden playlists",
      "Qt: hides rows in its list widget, is_hidden per row" },
   { "qt", "new playlist", "Qt: PlaylistEntryDialog writes the file itself" },
   { "qt", "delete playlist", "Qt: confirm dialog + QFile::remove" },
   /* The hidden-playlist list is in the core (Qt reads and writes it
    * there), but only Qt has the UI for it - a context-menu Hide and a
    * submenu of hidden playlists to bring back. The natives need a row
    * map in their playlist lists before they can hide rows; until then
    * they show every playlist, which is the pre-existing behaviour. */
   { "win32", "hidden playlists", "no hide UI yet; core call is Qt-side only" },
   { "cocoa", "hidden playlists", "no hide UI yet; core call is Qt-side only" },
   /* Qt's three dialogs predate the core and read RetroArch directly
    * (CoreOptionsDialog, ShaderParamsDialog, ViewOptionsWidget). */
   { "qt", "core options",
      "Qt: CoreOptionsDialog reads runloop core_options directly" },
   { "qt", "shader parameters",
      "Qt: ShaderParamsDialog reads menu_shader_get directly" },
   { "qt", "options dialog", "Qt: ViewOptionsWidget edits settings directly" }
};

/* A feature Qt implements a different but equivalent way, and the
 * symbol that shows it; "" for one implemented a toolkit-native way. */
static const char *const qt_equivalent[][2] = {
   /* Qt lists playlist files through its own QDir model
    * (getPlaylistFiles) and selects by path, not by the core's
    * playlist-file index. */
   { "playlist list",    "companion_core_select_playlist_path" },
   { "select playlist",  "companion_core_select_playlist_path" },
   /* Qt's "Run" and its core picker go through the launch-with combo:
    * it resolves the core (companion_core_launch_options) and loads the
    * content directly, rather than the natives' request_load_entry /
    * entry_needs_core path. */
   { "run entry",        "companion_core_launch_options" },
   { "pick core on run", "companion_core_launch_options" },
   /* Qt runs the file browser through QFileSystemModel; it has no
    * in-core browse listing, and so no folder / file split either. */
   { "file browser",     "" },
   { "two-pane browser", "BrowseTableModel" }
};

static char *texts[3][2];

/* \bSYMBOL\b in any of a backend's files */
static int uses(size_t backend, const char *sym)
{
   size_t n = strlen(sym);
   int f;
   for (f = 0; f < 2; f++)
   {
      const char *t = texts[backend][f];
      const char *p;
      if (!t)
         continue;
      for (p = t; (p = strstr(p, sym)); p++)
      {
         if ((p == t || !CK_IS_WORD(p[-1])) && !CK_IS_WORD(p[n]))
            return 1;
      }
   }
   return 0;
}

static int has_feature(size_t backend, size_t feature)
{
   size_t i;
   if (uses(backend, features[feature][1]))
      return 1;
   if (backend != 0)
      return 0;
   for (i = 0; i < COUNT(qt_equivalent); i++)
   {
      if (strcmp(qt_equivalent[i][0], features[feature][0]))
         continue;
      return !*qt_equivalent[i][1] || uses(0, qt_equivalent[i][1]);
   }
   return 0;
}

static int waived(size_t backend, size_t feature)
{
   size_t i;
   for (i = 0; i < COUNT(waivers); i++)
   {
      if (     !strcmp(waivers[i][0], backends[backend])
            && !strcmp(waivers[i][1], features[feature][0]))
         return 1;
   }
   return 0;
}

/* static void FUNC\b.*?\n\}\n  holds the iterate call */
static int loop_calls_iterate(const char *path, const char *func)
{
   char *src = ck_read_file(path, NULL);
   struct ck_sb head = {0};
   const char *p;
   int found = 0;

   if (!src)
   {
      fprintf(stderr, "companion_parity: cannot read %s\n", path);
      exit(1);
   }
   ck_sb_puts(&head, "static void ");
   ck_sb_puts(&head, func);
   for (p = src; (p = strstr(p, head.p)); p++)
   {
      const char *end;
      if (CK_IS_WORD(p[head.len]) && CK_IS_WORD(head.p[head.len - 1]))
         continue;
      if ((end = strstr(p + head.len, "\n}\n")))
      {
         const char *c = strstr(p, "ui_companion_driver_wimp_iterate()");
         found = c && c + 34 <= end + 3;
      }
      break;
   }
   ck_sb_free(&head);
   free(src);
   return found;
}

static int check_main_loops(void)
{
   if (!loop_calls_iterate("ui/drivers/cocoa/cocoa_common.m",
            "rarch_draw_observer"))
   {
      fprintf(stderr, "error: main loop(s) that never call"
            " ui_companion_driver_wimp_iterate(): rarch_draw_observer\n");
      exit(1);
   }
   printf("main loops iterate the companion: ok\n");
   return 0;
}

int main(int argc, char **argv)
{
   int want_list = 0, gaps = 0;
   size_t b, f;
   int a;

   for (a = 1; a < argc; a++)
   {
      if (!strcmp(argv[a], "--list"))
         want_list = 1;
      else
      {
         fprintf(stderr, "usage: companion_parity [--list]\n");
         return 2;
      }
   }
   for (b = 0; b < 3; b++)
   {
      for (f = 0; f < 2; f++)
         texts[b][f] = backend_files[b][f]
            ? ck_read_file(backend_files[b][f], NULL) : NULL;
   }
   if (want_list)
   {
      int w = 0;
      for (f = 0; f < COUNT(features); f++)
      {
         if ((int)strlen(features[f][0]) > w)
            w = (int)strlen(features[f][0]);
      }
      printf("%-*s  qt  win32  cocoa\n", w, "feature");
      for (f = 0; f < COUNT(features); f++)
      {
         printf("%-*s  ", w, features[f][0]);
         for (b = 0; b < 3; b++)
         {
            const char *c = waived(b, f) ? "waived"
               : has_feature(b, f) ? "yes" : "NO";
            printf("%s%-*s", b ? "  " : "", (int)strlen(backends[b]), c);
         }
         printf("\n");
      }
      return 0;
   }
   for (f = 0; f < COUNT(features); f++)
   {
      for (b = 0; b < 3; b++)
      {
         if (waived(b, f) || has_feature(b, f))
            continue;
         if (!gaps++)
            fprintf(stderr, "error: companion backends disagree on"
                  " features:\n");
         fprintf(stderr, "   %-6s is missing \"%s\" (expected a call to %s)\n",
               backends[b], features[f][0], features[f][1]);
      }
   }
   if (gaps)
   {
      fprintf(stderr, "\nEither wire the feature into that backend, or - if"
            " it genuinely should not carry it - add the (backend, feature)"
            " pair to waivers[] in tools/companion_parity.c with a reason.\n");
      return 1;
   }
   if (check_main_loops())
      return 1;
   printf("companion parity: %lu features, all 3 backends agree\n",
         (unsigned long)COUNT(features));
   return 0;
}
