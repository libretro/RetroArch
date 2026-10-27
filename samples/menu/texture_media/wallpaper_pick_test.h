/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIA_WALLPAPER_PICK_TEST_H
#define MEDIA_WALLPAPER_PICK_TEST_H
#include "../../../menu/menu_cbs.h"
#include "../../../menu/menu_dirwalk.h"

/* The menu Background setting opens a browser whose image rows are
 * picks: OK on one must store it as the wallpaper, never open the
 * fullscreen viewer. Both presses go through menu_driver_iterate, so
 * the driver's entry-action filter and the bound OK callbacks run. */

#define WALLPAPER_CHECK(message, cond) do { bool passed = (cond) != 0; \
   printf("%s: %s\n", passed ? "ok" : "FAIL", message); \
   if (!passed) ok = false; } while (0)

static int media_wallpaper_row(struct menu_state *menu_st,
      const char *label, const char *path)
{
   file_list_t *list = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
   size_t i;
   if (!list || menu_dirwalk_pending() || MENU_ENTRIES_NEEDS_REFRESH(menu_st))
      return -1;
   for (i = 0; i < list->size; i++)
      if (     (label && string_is_equal(list->list[i].label, label))
            || (path && string_is_equal(list->list[i].path, path)))
         return (int)i;
   return -1;
}

static int media_wallpaper_pump(struct menu_state *menu_st,
      void (*draw)(struct menu_state *), enum menu_action action,
      const char *label, const char *path)
{
   int tick, row = -1;
   for (tick = 0; tick < 2000; tick++)
   {
      task_queue_check();
      menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
            config_get_ptr(), action, cpu_features_get_time_usec());
      action = MENU_ACTION_NOOP;
      draw(menu_st);
      if ((label || path)
            && (row = media_wallpaper_row(menu_st, label, path)) >= 0)
         break;
      if (!label && !path && tick >= 8)
         break;
      retro_sleep(1);
   }
   return row;
}

static void media_wallpaper_select(struct menu_state *menu_st, int row)
{
   menu_st->selection_ptr = (size_t)row;
   if (menu_st->driver_ctx->navigation_set)
      menu_st->driver_ctx->navigation_set(menu_st->userdata, true);
}

static bool media_wallpaper_pick_check(struct menu_state *menu_st,
      const char *fixture, bool (*showing)(void *),
      void (*draw)(struct menu_state *))
{
   static char directory[PATH_MAX_LENGTH];
   static char expected[PATH_MAX_LENGTH];
   settings_t *settings = config_get_ptr();
   const char *name     = path_basename(fixture);
   file_list_t *list;
   bool ok              = true;
   int row;

   fill_pathname_basedir(directory, fixture, sizeof(directory));
   fill_pathname_join_special(expected, directory, name, sizeof(expected));
   /* An unset wallpaper browses the default directory; a stale entry
    * in the fixture's directory opens it there without matching it. */
   fill_pathname_join_special(settings->paths.path_menu_wallpaper,
         directory, "missing-wallpaper.png",
         sizeof(settings->paths.path_menu_wallpaper));

   WALLPAPER_CHECK("menu settings page pushed",
         generic_action_ok_displaylist_push(
            MENU_ENUM_LABEL_DEFERRED_MENU_SETTINGS_LIST_STR, NULL,
            MENU_ENUM_LABEL_DEFERRED_MENU_SETTINGS_LIST_STR, 0, 0, 0,
            ACTION_OK_DL_PUSH_DEFAULT) == 0);
   row = media_wallpaper_pump(menu_st, draw, MENU_ACTION_NOOP,
         MENU_ENUM_LABEL_MENU_WALLPAPER_STR, NULL);
   WALLPAPER_CHECK("Background setting listed", row >= 0);
   if (!ok)
      return false;

   media_wallpaper_select(menu_st, row);
   row = media_wallpaper_pump(menu_st, draw, MENU_ACTION_OK, NULL, name);
   WALLPAPER_CHECK("Background browser lists the fixture", row >= 0);
   if (!ok)
      return false;
   list = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
   WALLPAPER_CHECK("fixture row is an image pick",
         list->list[row].type == FILE_TYPE_IMAGE);

   media_wallpaper_select(menu_st, row);
   media_wallpaper_pump(menu_st, draw, MENU_ACTION_OK, NULL, NULL);
   WALLPAPER_CHECK("picking does not open the fullscreen viewer",
         !showing(menu_st->userdata));
   WALLPAPER_CHECK("picking stores the image as the wallpaper",
         string_is_equal(settings->paths.path_menu_wallpaper, expected));
   if (!string_is_equal(settings->paths.path_menu_wallpaper, expected))
      printf("menu_wallpaper = %s\n", settings->paths.path_menu_wallpaper);
   return ok;
}

#undef WALLPAPER_CHECK
#endif
