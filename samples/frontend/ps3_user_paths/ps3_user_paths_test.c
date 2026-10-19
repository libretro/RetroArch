/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

#include "defaults.h"
#include "frontend/drivers/platform_ps3_paths.h"

struct defaults g_defaults;

static void check_paths(void)
{
   char config[PATH_MAX_LENGTH];
   char saves[DIR_MAX_LENGTH];

   strlcpy(g_defaults.dirs[DEFAULT_DIR_CORE], "/installation/cores",
         sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_SYSTEM], "/installation/system",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   assert(ps3_user_paths_init(1));
   assert(!strcmp(g_defaults.path_config,
            "/dev_hdd0/home/00000001/retroarch/retroarch.cfg"));
   strlcpy(config, g_defaults.path_config, sizeof(config));
   strlcpy(saves, g_defaults.dirs[DEFAULT_DIR_SRAM], sizeof(saves));
   assert(ps3_user_paths_init(42));
   assert(strcmp(config, g_defaults.path_config));
   assert(strcmp(saves, g_defaults.dirs[DEFAULT_DIR_SRAM]));
   assert(!strcmp(g_defaults.dirs[DEFAULT_DIR_CORE], "/installation/cores"));
   assert(!strcmp(g_defaults.dirs[DEFAULT_DIR_SYSTEM], "/installation/system"));
   strlcpy(config, g_defaults.path_config, sizeof(config));
   assert(!ps3_user_paths_init(0));
   assert(!ps3_user_paths_init(100000000));
   assert(!strcmp(config, g_defaults.path_config));
}

static void check_import(void)
{
   struct import_paths
   {
      char source[PATH_MAX_LENGTH];
      char include[PATH_MAX_LENGTH];
      char first[PATH_MAX_LENGTH];
      char second[PATH_MAX_LENGTH];
      char third[PATH_MAX_LENGTH];
      char value[PATH_MAX_LENGTH];
   } *paths = (struct import_paths*)calloc(1, sizeof(*paths));
   char root[DIR_MAX_LENGTH] = "ps3-user-paths-test-data";
   config_file_t *config;
   const char text[] =
      "#include \"included.cfg\"\n"
      "savefile_directory = \"/another-user/saves\"\n"
      "runtime_log_directory = \"/another-user/logs\"\n"
      "core_options_path = \"/another-user/core.opt\"\n";
   const char included[] = "video_vsync = \"false\"\n";
   bool vsync = true;

   assert(paths);
   assert(path_mkdir(root));
   fill_pathname_join(paths->source, root, "source.cfg", sizeof(paths->source));
   fill_pathname_join(paths->include, root, "included.cfg", sizeof(paths->include));
   assert(filestream_write_file(paths->source, text, sizeof(text) - 1));
   assert(filestream_write_file(paths->include, included, sizeof(included) - 1));
   fill_pathname_join(g_defaults.path_config, root, "user/retroarch.cfg",
         sizeof(g_defaults.path_config));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], root,
         "user/config", sizeof(g_defaults.dirs[0]));

   assert(ps3_config_import(paths->source, paths->first, sizeof(paths->first)));
   assert(ps3_config_import(paths->source, paths->second, sizeof(paths->second)));
   assert(strcmp(paths->first, paths->second));
   assert(filestream_delete(paths->include) == 0);
   config = config_file_new_from_path_to_string(paths->first);
   assert(config);
   assert(config_get_bool(config, "video_vsync", &vsync) && !vsync);
   assert(config_get_path(config, "savefile_directory", paths->value, sizeof(paths->value)));
   assert(!strcmp(paths->value, g_defaults.dirs[DEFAULT_DIR_SRAM]));
   assert(config_get_path(config, "core_options_path", paths->value, sizeof(paths->value)));
   assert(!*paths->value);
   assert(config_get_path(config, "runtime_log_directory", paths->value, sizeof(paths->value)));
   assert(!*paths->value);
   config_file_free(config);
   assert(ps3_config_import(paths->first, paths->value, sizeof(paths->value)));
   assert(!strcmp(paths->first, paths->value));
   assert(!ps3_config_import(paths->source, paths->value, 4));
   fill_pathname_join(paths->value, root, "user/../source.cfg", sizeof(paths->value));
   assert(ps3_config_import(paths->value, paths->third, sizeof(paths->third)));
   assert(!strncmp(paths->third, g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
            strlen(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG])));

   config = config_file_new_from_path_to_string(paths->source);
   assert(config);
   assert(config_get_path(config, "savefile_directory", paths->value, sizeof(paths->value)));
   assert(!strcmp(paths->value, "/another-user/saves"));
   config_file_free(config);

   assert(filestream_delete(paths->first) == 0);
   assert(filestream_delete(paths->second) == 0);
   assert(filestream_delete(paths->third) == 0);
   assert(filestream_delete(paths->source) == 0);
   assert(path_rmdir(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(paths->value, root, "user", sizeof(paths->value));
   assert(path_rmdir(paths->value));
   assert(path_rmdir(root));
   free(paths);
}

int main(void)
{
   check_paths();
   check_import();
   puts("PS3 user paths: passed");
   return 0;
}
