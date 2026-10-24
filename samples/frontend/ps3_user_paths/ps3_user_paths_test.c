/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <streams/file_stream.h>

#include "defaults.h"
#include "frontend/drivers/platform_ps3_paths.h"

struct defaults g_defaults;

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { \
   fprintf(stderr, "FAIL: " __VA_ARGS__); \
   fprintf(stderr, "\n"); failures++; } } while (0)

#define ROOT "ps3-user-paths-test-data"

static void join(char *s, const char *a, const char *b)
{
   fill_pathname_join(s, a, b, PATH_MAX_LENGTH);
}

static bool write_text(const char *path, const char *text)
{
   return filestream_write_file(path, text, (int64_t)strlen(text));
}

static bool file_is(const char *path, const char *text)
{
   void *buf   = NULL;
   int64_t len = 0;
   bool ret    = filestream_read_file(path, &buf, &len)
      && len == (int64_t)strlen(text) && !memcmp(buf, text, (size_t)len);
   free(buf);
   return ret;
}

static bool config_value_is(const char *path, const char *key,
      const char *value)
{
   char found[PATH_MAX_LENGTH];
   config_file_t *config = config_file_new_from_path_to_string(path);
   bool ret = config
      && config_get_path(config, key, found, sizeof(found))
      && !strcmp(found, value);
   config_file_free(config);
   return ret;
}

static void remove_tree(const char *dir)
{
   size_t i;
   struct string_list *list = dir_list_new(dir, NULL, true, true,
         false, false);
   if (list)
   {
      for (i = 0; i < list->size; i++)
      {
         if (list->elems[i].attr.i == RARCH_DIRECTORY)
            remove_tree(list->elems[i].data);
         else
            filestream_delete(list->elems[i].data);
      }
      string_list_free(list);
   }
   path_rmdir(dir);
}

/* The user's directories, below user */
static void user_dirs_at(const char *user)
{
   static const struct { enum default_dirs dir; const char *name; } dirs[] = {
      { DEFAULT_DIR_MENU_CONFIG, "config" },
      { DEFAULT_DIR_REMAP,       "remaps" },
      { DEFAULT_DIR_SRAM,        "savefiles" },
      { DEFAULT_DIR_SAVESTATE,   "savestates" },
      { DEFAULT_DIR_PLAYLIST,    "playlists" },
      { DEFAULT_DIR_AUTOCONFIG,  "autoconfig" }
   };
   size_t i;
   for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
      fill_pathname_join(g_defaults.dirs[dirs[i].dir], user, dirs[i].name,
            sizeof(g_defaults.dirs[0]));
   fill_pathname_join(g_defaults.path_config, user, "retroarch.cfg",
         sizeof(g_defaults.path_config));
}

static void check_paths(void)
{
   char config[PATH_MAX_LENGTH];
   char saves[DIR_MAX_LENGTH];

   strlcpy(g_defaults.dirs[DEFAULT_DIR_CORE], "/installation/cores",
         sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_SYSTEM], "/installation/system",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   CHECK(ps3_user_paths_init(1), "paths: user 1");
   CHECK(!strcmp(g_defaults.path_config,
            "/dev_hdd0/home/00000001/retroarch/retroarch.cfg"),
         "paths: user 1 configuration is %s", g_defaults.path_config);
   strlcpy(config, g_defaults.path_config, sizeof(config));
   strlcpy(saves, g_defaults.dirs[DEFAULT_DIR_SRAM], sizeof(saves));
   CHECK(ps3_user_paths_init(42), "paths: user 42");
   CHECK(strcmp(config, g_defaults.path_config),
         "paths: two users share a configuration");
   CHECK(strcmp(saves, g_defaults.dirs[DEFAULT_DIR_SRAM]),
         "paths: two users share save files");
   CHECK(!strcmp(g_defaults.dirs[DEFAULT_DIR_CORE], "/installation/cores")
      && !strcmp(g_defaults.dirs[DEFAULT_DIR_SYSTEM], "/installation/system"),
         "paths: installation directories moved");
   strlcpy(config, g_defaults.path_config, sizeof(config));
   CHECK(!ps3_user_paths_init(0), "paths: user 0 accepted");
   CHECK(!ps3_user_paths_init(100000000), "paths: user 100000000 accepted");
   CHECK(!strcmp(config, g_defaults.path_config),
         "paths: a refused user changed the paths");
}

/* The layout before per-user directories, exactly */
static void check_legacy(void)
{
   CHECK(ps3_user_paths_init(7), "legacy: user 7");
   ps3_legacy_paths_init("/usrdir");
   CHECK(!strcmp(g_defaults.path_config, "/usrdir/retroarch.cfg"),
         "legacy: configuration is %s", g_defaults.path_config);
   CHECK(!strcmp(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], "/usrdir/config"),
         "legacy: config directory");
   CHECK(!strcmp(g_defaults.dirs[DEFAULT_DIR_REMAP], "/usrdir/remaps"),
         "legacy: remap directory");
   CHECK(!strcmp(g_defaults.dirs[DEFAULT_DIR_SRAM], "/usrdir/savefiles"),
         "legacy: save file directory");
   CHECK(!strcmp(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], "/usrdir/savestates"),
         "legacy: savestate directory");
   CHECK(!strcmp(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], "/usrdir/playlists"),
         "legacy: playlist directory");
   CHECK(!strcmp(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY], "/usrdir"),
         "legacy: history directory");
   CHECK(!*g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]
      && !*g_defaults.dirs[DEFAULT_DIR_CACHE]
      && !*g_defaults.dirs[DEFAULT_DIR_CONTENT_FAVORITES],
         "legacy: a user directory left set");
}

static void check_import(void)
{
   char *p      = (char*)calloc(6, PATH_MAX_LENGTH);
   char *source = p;
   char *other  = p + PATH_MAX_LENGTH;
   char *first  = p + 2 * PATH_MAX_LENGTH;
   char *second = p + 3 * PATH_MAX_LENGTH;
   char *value  = p + 4 * PATH_MAX_LENGTH;
   char *user   = p + 5 * PATH_MAX_LENGTH;
   config_file_t *config;
   bool vsync = true;

   if (!p || !path_mkdir(ROOT))
   {
      fprintf(stderr, "FAIL: import: setup\n");
      exit(1);
   }
   join(source, ROOT, "source.cfg");
   join(other, ROOT, "included.cfg");
   CHECK(write_text(source,
            "#include \"included.cfg\"\n"
            "savefile_directory = \"/another-user/saves\"\n"
            "runtime_log_directory = \"/another-user/logs\"\n"
            "core_options_path = \"/another-user/core.opt\"\n")
      && write_text(other, "video_vsync = \"false\"\n"), "import: setup");
   join(user, ROOT, "user");
   user_dirs_at(user);

   CHECK(ps3_config_import(source, first, PATH_MAX_LENGTH), "import: first");
   CHECK(!strncmp(first, g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
            strlen(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG])),
         "import: copied outside the user's config directory");
   filestream_delete(other);
   config = config_file_new_from_path_to_string(first);
   CHECK(config && config_get_bool(config, "video_vsync", &vsync) && !vsync,
         "import: included settings not flattened");
   config_file_free(config);
   CHECK(config_value_is(first, "savefile_directory",
            g_defaults.dirs[DEFAULT_DIR_SRAM]),
         "import: save files left in another user's directory");
   CHECK(config_value_is(first, "core_options_path", "")
      && config_value_is(first, "runtime_log_directory", ""),
         "import: personal files left in another user's directory");

   /* The same source again is the same copy, as the user left it */
   config = config_file_new_from_path_to_string(first);
   config_set_string(config, "video_vsync", "true");
   CHECK(config_file_write(config, first, true), "import: edit the copy");
   config_file_free(config);
   CHECK(ps3_config_import(source, second, PATH_MAX_LENGTH)
      && !strcmp(first, second), "import: a second copy of one source");
   CHECK(config_value_is(second, "video_vsync", "true"),
         "import: re-import discarded the user's changes");

   /* Another source with the same name is another copy */
   join(value, ROOT, "elsewhere");
   CHECK(path_mkdir(value), "import: setup");
   join(other, value, "source.cfg");
   CHECK(write_text(other, "video_vsync = \"false\"\n"), "import: setup");
   CHECK(ps3_config_import(other, second, PATH_MAX_LENGTH)
      && strcmp(first, second),
         "import: two sources share one copy");
   filestream_delete(second);
   filestream_delete(other);
   path_rmdir(value);

   CHECK(ps3_config_import(first, value, PATH_MAX_LENGTH)
      && !strcmp(first, value), "import: the user's own file copied");
   CHECK(!ps3_config_import(source, value, 4), "import: too long accepted");
   join(value, ROOT, "user/../source.cfg");
   CHECK(ps3_config_import(value, second, PATH_MAX_LENGTH)
      && strcmp(value, second), "import: '..' taken as the user's own");
   filestream_delete(second);

   CHECK(config_value_is(source, "savefile_directory", "/another-user/saves"),
         "import: the source was changed");

   remove_tree(ROOT);
   free(p);
}

static void check_migrate(void)
{
   char *p       = (char*)calloc(6, PATH_MAX_LENGTH);
   char *legacy  = p;
   char *user    = p + PATH_MAX_LENGTH;
   char *path    = p + 2 * PATH_MAX_LENGTH;
   char *value   = p + 3 * PATH_MAX_LENGTH;
   char *cores   = p + 4 * PATH_MAX_LENGTH;
   char *history = p + 5 * PATH_MAX_LENGTH;

   if (!p || !path_mkdir(ROOT))
   {
      fprintf(stderr, "FAIL: migrate: setup\n");
      exit(1);
   }
   join(legacy, ROOT, "USRDIR");
   join(user, ROOT, "user");
   user_dirs_at(user);

   /* No shared configuration: a user starts empty */
   CHECK(path_mkdir(legacy), "migrate: setup");
   CHECK(ps3_user_data_migrate(legacy)
      && !path_is_valid(g_defaults.path_config),
         "migrate: a configuration made from nothing");

   join(path, legacy, "savefiles/sub");
   CHECK(path_mkdir(path), "migrate: setup");
   join(path, legacy, "savestates/Core");
   CHECK(path_mkdir(path), "migrate: setup");
   join(path, legacy, "remaps/Core");
   CHECK(path_mkdir(path), "migrate: setup");
   join(path, legacy, "savefiles/game.srm");
   CHECK(write_text(path, "sram"), "migrate: setup");
   join(path, legacy, "savefiles/sub/deep.srm");
   CHECK(write_text(path, "deep"), "migrate: setup");
   join(path, legacy, "savefiles/mine.srm");
   CHECK(write_text(path, "shared"), "migrate: setup");
   join(path, legacy, "savestates/Core/game.state");
   CHECK(write_text(path, "state"), "migrate: setup");
   join(path, legacy, "remaps/Core/Core.rmp");
   CHECK(write_text(path, "remap"), "migrate: setup");
   join(path, legacy, "content_history.lpl");
   CHECK(write_text(path, "history"), "migrate: setup");
   join(path, legacy, "retroarch-core-options.cfg");
   CHECK(write_text(path, "options"), "migrate: setup");
   join(value, legacy, "savefiles");
   join(path, legacy, "retroarch.cfg");
   {
      config_file_t *config = config_file_new_alloc();
      join(cores, legacy, "cores");
      join(history, legacy, "content_history.lpl");
      config_set_path(config, "savefile_directory", value);
      config_set_string(config, "savestate_directory", "default");
      config_set_path(config, "screenshot_directory", "/usb/shots");
      config_set_path(config, "content_history_path", history);
      config_set_path(config, "libretro_directory", cores);
      CHECK(config_file_write(config, path, true), "migrate: setup");
      config_file_free(config);
   }

   /* What the user already has is kept */
   CHECK(path_mkdir(g_defaults.dirs[DEFAULT_DIR_SRAM]), "migrate: setup");
   join(path, g_defaults.dirs[DEFAULT_DIR_SRAM], "mine.srm");
   CHECK(write_text(path, "mine"), "migrate: setup");

   /* A copy that fails leaves the user without a configuration */
   join(path, g_defaults.dirs[DEFAULT_DIR_SRAM], "sub");
   CHECK(write_text(path, "a file where a directory goes"), "migrate: setup");
   CHECK(!ps3_user_data_migrate(legacy), "migrate: a failed copy succeeded");
   CHECK(!path_is_valid(g_defaults.path_config),
         "migrate: configuration written after a failed copy");
   filestream_delete(path);

   CHECK(ps3_user_data_migrate(legacy), "migrate");
   join(path, g_defaults.dirs[DEFAULT_DIR_SRAM], "game.srm");
   CHECK(file_is(path, "sram"), "migrate: save file");
   join(path, g_defaults.dirs[DEFAULT_DIR_SRAM], "sub/deep.srm");
   CHECK(file_is(path, "deep"), "migrate: nested save file");
   join(path, g_defaults.dirs[DEFAULT_DIR_SRAM], "mine.srm");
   CHECK(file_is(path, "mine"), "migrate: the user's own save overwritten");
   join(path, g_defaults.dirs[DEFAULT_DIR_SAVESTATE], "Core/game.state");
   CHECK(file_is(path, "state"), "migrate: savestate");
   join(path, g_defaults.dirs[DEFAULT_DIR_REMAP], "Core/Core.rmp");
   CHECK(file_is(path, "remap"), "migrate: remap");
   join(path, user, "content_history.lpl");
   CHECK(file_is(path, "history"), "migrate: history");
   join(path, user, "retroarch-core-options.cfg");
   CHECK(file_is(path, "options"), "migrate: core options");

   CHECK(config_value_is(g_defaults.path_config, "savefile_directory",
            g_defaults.dirs[DEFAULT_DIR_SRAM]),
         "migrate: save files left in the shared directory");
   CHECK(config_value_is(g_defaults.path_config, "savestate_directory",
            g_defaults.dirs[DEFAULT_DIR_SAVESTATE]),
         "migrate: default savestate directory not the user's");
   CHECK(config_value_is(g_defaults.path_config, "screenshot_directory",
            "/usb/shots"), "migrate: a chosen directory replaced");
   CHECK(config_value_is(g_defaults.path_config, "content_history_path", ""),
         "migrate: history left in the shared directory");
   join(value, legacy, "cores");
   CHECK(config_value_is(g_defaults.path_config, "libretro_directory", value),
         "migrate: an installation directory moved");

   join(path, legacy, "savefiles/game.srm");
   CHECK(file_is(path, "sram"), "migrate: the shared data was changed");

   /* Once the user has a configuration, it is theirs */
   {
      config_file_t *config =
         config_file_new_from_path_to_string(g_defaults.path_config);
      config_set_path(config, "screenshot_directory", "/usb/mine");
      CHECK(config_file_write(config, g_defaults.path_config, true),
            "migrate: setup");
      config_file_free(config);
   }
   CHECK(ps3_user_data_migrate(legacy)
      && config_value_is(g_defaults.path_config, "screenshot_directory",
            "/usb/mine"), "migrate: ran again over the user's configuration");

   remove_tree(ROOT);
   free(p);
}

int main(void)
{
   check_paths();
   check_legacy();
   check_import();
   check_migrate();
   if (failures)
   {
      fprintf(stderr, "PS3 user paths: %d failed\n", failures);
      return 1;
   }
   puts("PS3 user paths: passed");
   return 0;
}
