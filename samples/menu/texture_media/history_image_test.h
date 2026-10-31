/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIA_HISTORY_IMAGE_TEST_H
#define MEDIA_HISTORY_IMAGE_TEST_H
#include "../../../menu/menu_cbs.h"
#include "../../../menu/menu_dirwalk.h"
#include "../../../command.h"
#include "../../../defaults.h"
#include "../../../playlist.h"
#include <streams/file_stream.h>

/* OK on an Images (image history) entry shows the image in the viewer
 * at once, with no Run step and no content load; Back closes it and
 * leaves the image history list as it was, the same entry selected.
 * Every press goes through menu_driver_iterate, so the driver's entry
 * action filter and the bound OK callbacks run. */

#define HISTORY_CHECK(message, cond) do { bool passed = (cond) != 0; \
   printf("%s: %s\n", passed ? "ok" : "FAIL", message); \
   if (!passed) ok = false; } while (0)

static void media_history_push(const char *path)
{
   struct playlist_entry entry = {0};
   entry.path      = (char*)path;
   entry.core_path = (char*)"builtin";
   entry.core_name = (char*)"imageviewer";
   command_playlist_push_write(g_defaults.image_history, &entry);
}

/* The history list's rows, once the list is up: -1 until then */
static int media_history_rows(struct menu_state *menu_st)
{
   file_list_t *list = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
   size_t i;
   if (!list || menu_dirwalk_pending() || MENU_ENTRIES_NEEDS_REFRESH(menu_st))
      return -1;
   for (i = 0; i < list->size; i++)
      if (list->list[i].type != FILE_TYPE_RPL_ENTRY)
         return -1;
   return (int)list->size;
}

static void media_history_pump(struct menu_state *menu_st,
      void (*draw)(struct menu_state *), enum menu_action action,
      unsigned frames)
{
   unsigned tick;
   for (tick = 0; tick < frames; tick++)
   {
      task_queue_check();
      menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
            config_get_ptr(), action, cpu_features_get_time_usec());
      action = MENU_ACTION_NOOP;
      draw(menu_st);
   }
}

/* @open_tab opens the driver's own Images tab and returns true; NULL or
 * false pushes the list the Images entry of the Playlists list pushes. */
static bool media_history_image_check(struct menu_state *menu_st,
      const char *fixture, const char *scratch,
      bool (*showing)(void *), bool (*ready)(void *),
      const char *(*image)(void *), bool (*open_tab)(void *),
      void (*draw)(struct menu_state *))
{
   static char second[PATH_MAX_LENGTH];
   static char first[PATH_MAX_LENGTH];
   void *png         = NULL;
   int64_t len       = 0;
   file_list_t *stack;
   size_t depth;
   bool ok           = true;
   bool tab          = false;
   int tick, rows    = -1;

   /* A --menu start runs no content load, which loads the histories */
   if (!g_defaults.image_history)
      command_event(CMD_EVENT_HISTORY_INIT, NULL);
   HISTORY_CHECK("image history is loaded", g_defaults.image_history);
   if (!ok)
      return false;
   /* Two entries, so that keeping the selection proves something */
   strlcpy(first, fixture, sizeof(first));
   path_resolve_realpath(first, sizeof(first), true);
   fill_pathname_join_special(second, scratch, "second.png", sizeof(second));
   HISTORY_CHECK("second image written",
         filestream_read_file(first, &png, &len) && len > 0
         && filestream_write_file(second, png, len));
   free(png);
   if (!ok)
      return false;
   media_history_push(first);
   media_history_push(second);
   HISTORY_CHECK("both images are in the history",
         playlist_size(g_defaults.image_history) == 2);

   if (open_tab)
      tab = open_tab(menu_st->userdata);
   if (!tab)
      HISTORY_CHECK("Images list pushed",
            generic_action_ok_displaylist_push(
               playlist_get_conf_path(g_defaults.image_history), NULL,
               MENU_ENUM_LABEL_GOTO_IMAGES_STR, MENU_SETTING_ACTION, 0, 0,
               ACTION_OK_DL_IMAGES_LIST) == 0);
   for (tick = 0; tick < 2000 && (rows = media_history_rows(menu_st)) < 0; tick++)
   {
      media_history_pump(menu_st, draw, MENU_ACTION_NOOP, 1);
      retro_sleep(1);
   }
   HISTORY_CHECK("Images list shows both entries", rows == 2);
   if (!ok)
      return false;
   /* From the sidebar into the list, as a user does */
   if (tab)
      media_history_pump(menu_st, draw, MENU_ACTION_RIGHT, 8);

   /* The older entry: the fixture */
   menu_st->selection_ptr = 1;
   if (menu_st->driver_ctx->navigation_set)
      menu_st->driver_ctx->navigation_set(menu_st->userdata, true);
   media_history_pump(menu_st, draw, MENU_ACTION_NOOP, 8);
   stack = MENU_LIST_GET(menu_st->entries.list, 0);
   depth = stack->size;

   media_history_pump(menu_st, draw, MENU_ACTION_OK, 1);
   for (tick = 0; tick < 4000 && !(showing(menu_st->userdata)
            && ready(menu_st->userdata)); tick++)
   {
      media_history_pump(menu_st, draw, MENU_ACTION_NOOP, 1);
      retro_sleep(1);
   }
   HISTORY_CHECK("OK on an Images entry shows it in the viewer", tick < 4000);
   HISTORY_CHECK("the viewer opens in place, nothing pushed",
         MENU_LIST_GET(menu_st->entries.list, 0)->size == depth);
   HISTORY_CHECK("the viewer shows the selected entry",
         string_is_equal(image(menu_st->userdata), first));
   HISTORY_CHECK("no content load is started",
         !task_content_load_pending()
         && runloop_state_get_ptr()->current_core_type == CORE_TYPE_DUMMY
         && !*path_get(RARCH_PATH_CONTENT));

   media_history_pump(menu_st, draw, MENU_ACTION_OK, 8);
   HISTORY_CHECK("OK in the viewer keeps it open",
         showing(menu_st->userdata)
         && MENU_LIST_GET(menu_st->entries.list, 0)->size == depth);

   media_history_pump(menu_st, draw, MENU_ACTION_CANCEL, 8);
   HISTORY_CHECK("Back closes the viewer", !showing(menu_st->userdata));
   HISTORY_CHECK("Back returns to the Images list",
         MENU_LIST_GET(menu_st->entries.list, 0)->size == depth
         && media_history_rows(menu_st) == 2);
   HISTORY_CHECK("Back keeps the entry selected", menu_st->selection_ptr == 1);
   HISTORY_CHECK("no content load is started on Back",
         !task_content_load_pending()
         && runloop_state_get_ptr()->current_core_type == CORE_TYPE_DUMMY);
   remove(second);
   return ok;
}

#undef HISTORY_CHECK
#endif
