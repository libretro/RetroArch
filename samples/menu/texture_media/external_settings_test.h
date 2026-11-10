/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MEDIA_EXTERNAL_SETTINGS_TEST_H
#define MEDIA_EXTERNAL_SETTINGS_TEST_H

static bool media_external_settings_check(struct menu_state *menu_st,
      test_viewer_t *viewer)
{
   gfx_thumbnail_t *thumbnail = test_thumbnail(viewer);
   menu_media_controls_t *media = &viewer->media;
   file_list_t *stack = MENU_LIST_GET(menu_st->entries.list, 0);
   size_t depth = stack->size;
   size_t selected = menu_st->selection_ptr;
   void *session = thumbnail->anim_sess;
   void *player = thumbnail->anim_player;
   gfx_media_playback_state_t state;
   int64_t position;
   unsigned tick;
   unsigned x, y;
   uint32_t picture;
   if (!session || !player || !test_showing(viewer)
         || !gfx_thumbnail_get_playback_state(thumbnail, menu_st->current_time_us, &state))
      return false;
   position = state.position_us;
   menu_st->input_state.pointer.type = MENU_POINTER_MOUSE;
   menu_st->input_state.pointer.pos = VIDEO_POS_PACK(0, 0);
   menu_st->input_state.pointer.flags = 0;
   render(menu_st);
   if (media->controls.layout.button_count < 7)
      return false;
   x = (unsigned)(media->controls.layout.buttons[6].x
         + media->controls.layout.buttons[6].w / 2);
   y = (unsigned)(media->controls.layout.buttons[6].y
         + media->controls.layout.buttons[6].h / 2);
   menu_st->input_state.pointer.type = MENU_POINTER_MOUSE;
   menu_st->input_state.pointer.pos = VIDEO_POS_PACK(x, y);
   menu_st->input_state.pointer.flags = MENU_INP_PTR_FLG_PRESSED;
   render(menu_st);
   test_release(viewer, x, y);
   menu_st->input_state.pointer.flags = 0;
   for (tick = 0; tick < 1000; tick++)
   {
      task_queue_check();
      menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
            config_get_ptr(), MENU_ACTION_NOOP, menu_st->current_time_us);
      render(menu_st);
      if (thumbnail->anim_sess != session || thumbnail->anim_player != player)
      {
         printf("external settings lost preview: selection=%lu depth=%lu tick=%u\n",
               (unsigned long)selected, (unsigned long)depth, tick);
         return false;
      }
      if (!media->settings_open || test_showing(viewer)
            || !gfx_thumbnail_get_playback_state(thumbnail, menu_st->current_time_us, &state)
            || state.paused)
         return false;
      if (tick >= 100 && state.position_us > position + 100000)
         break;
      retro_sleep(1);
   }
   if (tick == 1000)
      return false;
#if defined(TEST_OZONE)
   {
      uint32_t file_list = viewer->flags & OZONE_FLAG_IS_FILE_LIST;
      /* Navigation may precede population of the new page's flags. */
      viewer->flags |= OZONE_FLAG_IS_FILE_LIST;
      viewer->selection = (size_t)-1;
      menu_st->driver_ctx->navigation_set(menu_st->userdata, false);
      viewer->flags = (viewer->flags & ~OZONE_FLAG_IS_FILE_LIST) | file_list;
      if (thumbnail->anim_sess != session || thumbnail->anim_player != player)
         return false;
   }
#endif
   /* A refresh must not replace the settings page's borrowed player. */
   if (menu_st->driver_ctx->refresh_thumbnail_image)
      menu_st->driver_ctx->refresh_thumbnail_image(menu_st->userdata,
            menu_st->selection_ptr);
   for (tick = 0; tick < 6; tick++)
   {
      menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
            config_get_ptr(), MENU_ACTION_DOWN, menu_st->current_time_us);
      render(menu_st);
      if (thumbnail->anim_sess != session || thumbnail->anim_player != player)
         return false;
   }
   if (native_gpu && !media_native_capture(&picture, 50))
      return false;
   if (action_cancel_pop_default(NULL, NULL, 0, 0) != 0)
      return false;
   for (tick = 0; tick < 1000; tick++)
   {
      task_queue_check();
      menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
            config_get_ptr(), MENU_ACTION_NOOP, menu_st->current_time_us);
      render(menu_st);
      if (!media->settings_open && test_showing(viewer))
         break;
      retro_sleep(1);
   }
   stack = MENU_LIST_GET(menu_st->entries.list, 0);
   return tick < 1000 && stack->size == depth
      && menu_st->selection_ptr == selected
      && thumbnail->anim_sess == session && thumbnail->anim_player == player;
}
#endif
