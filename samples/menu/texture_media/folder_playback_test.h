/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIA_FOLDER_PLAYBACK_TEST_H
#define MEDIA_FOLDER_PLAYBACK_TEST_H
#include "../../../menu/menu_cbs.h"


static bool media_folder_install(struct menu_state *menu_st, file_list_t *list,
      const char *directory, const char *fixture)
{
   static const unsigned episodes[] = {10, 2, 1, 11};
   const char *extension = path_get_extension(fixture);
   char name[64];
   unsigned i;
   unsigned count = getenv("MEDIA_TEST_CONTINUOUS_ERROR") ? 4 : 3;
   for (i = 0; i < count; i++)
   {
      snprintf(name, sizeof(name), "episode%u.%s", episodes[i], extension);
      if (!file_list_append(list, name, "", FILE_TYPE_MOVIE, 0, 0))
         return false;
      if (menu_st->driver_ctx->list_insert)
         menu_st->driver_ctx->list_insert(menu_st->userdata, list, name,
               directory, "", i, FILE_TYPE_MOVIE);
   }
   return true;
}

static bool media_folder_drive(struct menu_state *menu_st,
      menu_media_controls_t *media, void (*draw)(struct menu_state *),
      int (*stage)(void *), bool (*shown)(void *), void *viewer)
{
   struct menu_media_queue *identity = media->queue;
   bool next_loaded = false, ended = false;
   unsigned tick;
   if (!identity || menu_st->selection_ptr != 1)
   {
      printf("folder setup: queue=%u selection=%lu enabled=%u\n",
            identity != NULL, (unsigned long)menu_st->selection_ptr,
            config_get_ptr()->bools.media_player_continuous);
      return false;
   }
   if (getenv("MEDIA_TEST_CONTINUOUS_ERROR"))
   {
      unsigned settled = 0;
      for (tick = 0; tick < 16000; tick++)
      {
         draw(menu_st);
         if (!shown(viewer) || media->queue != identity)
            return false;
         if (menu_st->selection_ptr != 1 && menu_st->selection_ptr != 0)
            return false;
         if (menu_st->selection_ptr == 0 && !media->controls.target.user)
         {
            if (++settled == 1000)
            {
               puts("folder error: invalid next movie does not skip to episode11");
               return true;
            }
         }
         else
            settled = 0;
         retro_sleep(1);
      }
      return false;
   }
   for (tick = 0; tick < 16000; tick++)
   {
      int status;
      draw(menu_st);
      if (!shown(viewer) || media->queue != identity)
         break;
      if (menu_st->selection_ptr != 1 && menu_st->selection_ptr != 0)
         break;
      status = stage(viewer);
      if (menu_st->selection_ptr == 0 && status == 1)
         next_loaded = true;
      if (next_loaded && status == 2)
      {
         ended = true;
         break;
      }
      retro_sleep(1);
   }
   printf("folder playback: ticks=%u selection=%lu next=%u ended=%u "
          "fullscreen=%u stable=%u\n",
         tick, (unsigned long)menu_st->selection_ptr, next_loaded, ended,
         shown(viewer), media->queue == identity);
   if (!ended || !shown(viewer) || media->queue != identity
         || menu_st->selection_ptr != 0)
      return false;
   {
      int64_t until = cpu_features_get_time_usec() + 500000;
      for (tick = 0; cpu_features_get_time_usec() < until; tick++)
      {
         draw(menu_st);
         if (menu_st->selection_ptr != 0 || stage(viewer) != 2)
            return false;
         retro_sleep(1);
      }
   }

   if (!media->controls.target.user || !media->controls.playback.paused
         || media->controls.layout.button_count
            != 7u + (media->controls.target.subtitle_tracks ? 1u : 0u)
         || media->controls.target.directions != 1)
      return false;
   if (!gfx_media_controls_action(&media->controls, GFX_MEDIA_PREVIOUS_FILE,
             menu_st->current_time_us))
      return false;
   for (tick = 0; tick < 2000 && stage(viewer) != 3; tick++)
   {
      draw(menu_st);
      retro_sleep(1);
   }
   if (stage(viewer) != 3 || menu_st->selection_ptr != 1 || !shown(viewer))
      return false;
   if (!gfx_media_controls_action(
             &media->controls, GFX_MEDIA_STOP, menu_st->current_time_us))
      return false;
   for (tick = 0; tick < 2000; tick++)
   {
      draw(menu_st);
      if (!media->controls.playback.seeking && media->controls.playback.paused
            && !media->controls.playback.seek_failed
            && !media->controls.playback.position_us)
         break;
      retro_sleep(1);
   }
   if (tick == 2000)
      return false;
   {
      int64_t until = cpu_features_get_time_usec() + 4000000;
      for (tick = 0; cpu_features_get_time_usec() < until; tick++)
      {
         draw(menu_st);
         if (menu_st->selection_ptr != 1 || !media->controls.playback.paused)
            return false;
         retro_sleep(1);
      }
   }
   puts("folder controls: previous works after EOF; Stop does not advance");
   if (getenv("MEDIA_TEST_SETTINGS"))
   {
      file_list_t *stack = MENU_LIST_GET(menu_st->entries.list, 0);
      size_t depth = stack->size;
      size_t selected = menu_st->selection_ptr;
      file_list_t *entries;
      size_t row;
      bool found = false;
      if (!gfx_media_controls_action(&media->controls, GFX_MEDIA_SETTINGS,
               menu_st->current_time_us))
         return false;
      menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
            config_get_ptr(), MENU_ACTION_NOOP, menu_st->current_time_us);
      draw(menu_st);
      stack = MENU_LIST_GET(menu_st->entries.list, 0);
      if (!stack || !stack->size || !stack->list[stack->size - 1].label
            || strcmp(stack->list[stack->size - 1].label,
                  MENU_ENUM_LABEL_DEFERRED_MEDIA_PLAYER_SETTINGS_LIST_STR))
         return false;
      entries = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
      for (row = 0; entries && row < entries->size; row++)
         if (entries->list[row].label
               && !strcmp(entries->list[row].label, "media_player_continuous"))
            found = true;
      if (!found) return false;
      printf("fullscreen settings: showing=%u\n", shown(viewer));
      if (shown(viewer))
         return false;

      if (action_cancel_pop_default(NULL, NULL, 0, 0) != 0)
         return false;
      draw(menu_st);
      stack = MENU_LIST_GET(menu_st->entries.list, 0);
      if (stack->size != depth || menu_st->selection_ptr != selected)
         return false;
      puts("fullscreen settings: Back restores browser depth and selection");
   }
   return true;
}
#endif
