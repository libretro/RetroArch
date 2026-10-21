/* Links the shipping frontend with this real RGUI translation unit.
 * Only main() and the RGUI object are replaced; no playback/UI stubs. */
#include "../../../menu/drivers/rgui.c"
#include "../../../retroarch.h"
#include "../../../frontend/frontend.h"
#include "../../../gfx/gfx_surface_player.h"
#include <time/rtime.h>
#include <retro_timers.h>

static unsigned media_failures;
static bool native_gpu;
static bool gpu_surface;
static uint32_t native_background[320*240];
#define MEDIA_CHECK(message, cond) do { \
   bool passed = (cond) != 0; \
   printf("%s: %s\n", passed ? "ok" : "FAIL", message); \
   if (!passed) media_failures++; \
} while (0)

static bool media_picture_ready(rgui_t *rgui)
{
   if (gpu_surface)
      return !rgui->media_player && rgui->media_gpu.texture
         && rgui->media_gpu.anim_player;
   return rgui->media_player && rgui->fs_thumbnail.is_valid
      && rgui->mini_thumbnail.is_valid;
}

static void media_render(struct menu_state *menu_st)
{
   if(native_gpu)menu_st->current_time_us=cpu_features_get_time_usec();
   else menu_st->current_time_us += 1000;
   menu_st->driver_ctx->render(menu_st->userdata, VIDEO_SCALE_PACK(320, 240), false);
   if(native_gpu)
   {
      video_driver_state_t *video=video_state_get_ptr();
      if(menu_st->driver_ctx->set_texture)
         menu_st->driver_ctx->set_texture(menu_st->userdata);
      if(video->poke&&video->poke->set_texture_enable)
         video->poke->set_texture_enable(video->data,true,false);
      video_driver_frame(native_background,320,240,320*sizeof(uint32_t));
   }
}

#include "../texture_media/native_media_capture.h"

static bool media_capture_settled(struct menu_state *menu_st, int ordinal)
{
   int64_t until = cpu_features_get_time_usec() + 50000;
   uint32_t hash;
   do
   {
      media_render(menu_st);
      retro_sleep(1);
   } while (cpu_features_get_time_usec() < until);
   return media_native_capture(&hash, ordinal);
}

int main(int argc, char **argv)
{
   struct menu_state *menu_st;
   gfx_display_t *display;
   rgui_t *rgui;
   file_list_t *list, *stack;
   menu_entry_t *entry;
   settings_t *settings;
   char *rarch_argv[4];
   static char directory[PATH_MAX_LENGTH];
   unsigned i, selection, x, y;
   int tick;
   gfx_media_playback_state_t state;
   uint32_t pixels = 0;
   bool seeking;
   const char *first_video = getenv("MEDIA_TEST_FIRST_VIDEO_US");
   int64_t first_video_us = first_video ? strtol(first_video, NULL, 10) : 0;
   if (argc != 3) return 2;
   native_gpu=getenv("MEDIA_TEST_GPU")!=NULL;
   gpu_surface=getenv("MEDIA_TEST_GPU_SURFACE")!=NULL;
   if (gpu_surface && !native_gpu) return 2;
   config_file_set_io_default(config_file_io_filestream());
   rtime_init(); retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   rarch_argv[0] = (char*)"retroarch"; rarch_argv[1] = (char*)"--menu";
   rarch_argv[2] = (char*)"--config"; rarch_argv[3] = argv[2];
   if (!retroarch_main_init(4, rarch_argv)) return 1;
   retroarch_menu_running();
   menu_st = menu_state_get_ptr(); display = disp_get_ptr();
   MEDIA_CHECK("actual RGUI initialized", menu_st->driver_ctx
         && !strcmp(menu_st->driver_ctx->ident, "rgui"));
   if (media_failures) { main_exit(NULL); return 1; }
   rgui = (rgui_t*)menu_st->userdata;
   entry = (menu_entry_t*)calloc(1, sizeof(*entry));
   if (!entry) { main_exit(NULL); return 1; }
   MENU_ENTRY_INITIALIZE((*entry));
   settings = config_get_ptr();
   settings->bools.menu_rgui_inline_thumbnails = false;
   settings->bools.menu_thumbnail_preview_audio = false;
   list = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
   stack = MENU_LIST_GET(menu_st->entries.list, 0);
   fill_pathname_basedir(directory, argv[1], sizeof(directory));
   file_list_clear(list); file_list_clear(stack);
   MEDIA_CHECK("file-browser stack installed", file_list_append(stack, directory,
         "", FILE_TYPE_PLAIN, 0, 0));
   MEDIA_CHECK("movie entry installed", file_list_append(list, path_basename(argv[1]),
         "", FILE_TYPE_MOVIE, 0, 0));
   menu_st->selection_ptr = 0; menu_st->current_time_us = native_gpu?cpu_features_get_time_usec():1000000;
   rgui->flags &= ~(RGUI_FLAG_IS_PLAYLIST | RGUI_FLAG_IS_EXPLORE_LIST | RGUI_FLAG_IS_QUICK_MENU);
   rgui_scan_selected_entry_thumbnail(rgui, true);
   MEDIA_CHECK("file-browser movie resolves without a playlist", *rgui->media_requested_path);
   MEDIA_CHECK("movie activation consumed before core launch",
         rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_OK) == MENU_ACTION_NOOP
         && (rgui->flags & RGUI_FLAG_SHOW_FULLSCREEN_THUMBNAIL));
   for (tick = 0; tick < 2000; tick++)
   {
      media_render(menu_st);
      if (media_picture_ready(rgui)) break;
      retro_sleep(1);
   }
   MEDIA_CHECK("real decoder supplies the requested presentation path",
         tick < 2000 && media_picture_ready(rgui));
   if (media_failures) goto cleanup;
   if (getenv("MEDIA_TEST_HW_DECODE"))
      MEDIA_CHECK("movie uses the hardware decoder sink",
            gpu_surface && rgui->media_gpu.anim_player
            && ((gfx_surface_player_t*)rgui->media_gpu.anim_player)->hw_output_pixfmt
            && rgui->media_gpu.anim_surface
            && ((gfx_surface_t*)rgui->media_gpu.anim_surface)->imported);
   for (i = 0; i < VIDEO_SCALE_AREA(rgui->fs_thumbnail.dims); i++) pixels |= rgui->fs_thumbnail.data[i];
   MEDIA_CHECK("decoded movie picture is nonempty",
         gpu_surface ? rgui->media_gpu.texture != 0 : pixels != 0);
   MEDIA_CHECK("fullscreen shared controller owns the player", rgui->media.controls.target.user == rgui);
   MEDIA_CHECK("confirm pauses through the real menu adapter",
         rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_OK) == MENU_ACTION_NOOP
         && rgui_media_state(rgui, menu_st->current_time_us, &state) && state.paused);
   if(native_gpu)
   {
      gfx_media_target_t target=rgui->media.controls.target;
      uint32_t first=0,middle=0,again=0;
      unsigned burst;
      MEDIA_CHECK("paused movie picture reaches renderer",media_native_seek_capture(menu_st,media_render,&target,first_video_us,0,0,0,&first));
      MEDIA_CHECK("movie seek changes rendered picture",media_native_seek_capture(menu_st,media_render,&target,375000,first,1,1,&middle));
      MEDIA_CHECK("movie backward seek restores picture",media_native_seek_capture(menu_st,media_render,&target,first_video_us,first,-1,2,&again));
      for(burst=0;burst<16;burst++)
      {
         MEDIA_CHECK("movie burst seek accepted",target.seek(target.user,burst&1?375000:0,cpu_features_get_time_usec()));
         media_render(menu_st);retro_sleep(1);
      }
      MEDIA_CHECK("movie latest burst reaches renderer while paused",media_native_seek_capture(menu_st,media_render,&target,375000,first,1,3,&middle));
      printf("rendered media hashes: %08x %08x %08x\n",first,middle,again);
   }
   media_render(menu_st);
   MEDIA_CHECK("software controls reach the framebuffer", rgui->media.view.visible
         && display->framebuf_dims && rgui->frame_buf.data
         && rgui->frame_buf.data[(rgui->media.view.layout.panel.y
               + rgui->media.view.layout.panel.h - 2) * VIDEO_SCALE_W(display->framebuf_dims)
            + rgui->media.view.layout.panel.x + rgui->media.view.layout.panel.w - 2]
               == rgui->colors.shadow_color);
   selection = (unsigned)menu_st->selection_ptr;
   x = (unsigned)(rgui->media.controls.layout.timeline.x + rgui->media.controls.layout.timeline.w / 2);
   y = (unsigned)(rgui->media.controls.layout.timeline.y + 1);
   menu_st->input_state.pointer.type = MENU_POINTER_MOUSE;
   menu_st->input_state.pointer.pos = VIDEO_POS_PACK(0, 0);
   menu_st->input_state.pointer.flags = 0;
   media_render(menu_st);
   menu_st->input_state.pointer.pos = VIDEO_POS_PACK(x, y);
   menu_st->input_state.pointer.flags = MENU_INP_PTR_FLG_PRESSED;
   media_render(menu_st);
   MEDIA_CHECK("actual RGUI timeline captures press", rgui->media.controls.dragging);
   rgui_pointer_up(rgui, x, y, 0, MENU_INPUT_GESTURE_TAP, NULL, entry, 0);
   MEDIA_CHECK("release seeks without dismissing or activating the browser",
         (rgui->flags & RGUI_FLAG_SHOW_FULLSCREEN_THUMBNAIL)
         && menu_st->selection_ptr == selection
         && rgui_media_state(rgui, menu_st->current_time_us, &state) && state.seeking);
   menu_st->input_state.pointer.flags = 0;
   seeking = true;
   for (tick = 0; tick < 2000 && seeking; tick++)
   {
      media_render(menu_st);
      rgui_media_state(rgui, menu_st->current_time_us, &state);
      seeking = state.seeking;
      retro_sleep(1);
   }
   printf("seek result: ticks=%d pending=%d paused=%d position=%.6f duration=%.6f\n", tick, (int)state.seeking, (int)state.paused, (double)state.position_us/1000000.0, (double)state.duration_us/1000000.0);
   MEDIA_CHECK("real RGUI seek lands while pause is preserved", !seeking && state.paused && state.position_us > 0);
   MEDIA_CHECK("Stop is consumed by the RGUI transport",
         rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_START) == MENU_ACTION_NOOP);
   seeking = true;
   for (tick = 0; tick < 2000 && seeking; tick++)
   {
      media_render(menu_st);
      rgui_media_state(rgui, menu_st->current_time_us, &state);
      seeking = state.seeking;
      retro_sleep(1);
   }
   MEDIA_CHECK("RGUI Stop lands at the start and stays paused",
         !seeking && state.paused && state.position_us == 0);

   settings->bools.menu_rgui_inline_thumbnails = true;
   rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_CANCEL);
   media_render(menu_st);
   MEDIA_CHECK("paused inline picture survives leaving fullscreen",
         media_picture_ready(rgui));
   if (gpu_surface)
      MEDIA_CHECK("inline surface reaches the renderer",
            media_capture_settled(menu_st, 4));
   rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_OK);
   media_render(menu_st);
   MEDIA_CHECK("paused fullscreen picture survives reopening",
         media_picture_ready(rgui)
         && rgui_media_state(rgui, menu_st->current_time_us, &state) && state.paused);
   if (gpu_surface)
      MEDIA_CHECK("reopened surface reaches the renderer",
            media_capture_settled(menu_st, 5));
   settings->bools.menu_rgui_inline_thumbnails = false;
   rgui_media_seek(rgui, 1000000, menu_st->current_time_us);
   media_render(menu_st);
   MEDIA_CHECK("Back is consumed by viewer close",
         rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_CANCEL) == MENU_ACTION_NOOP);
   media_render(menu_st);
   MEDIA_CHECK("closing viewer retires player when inline previews are disabled",
         !rgui->media_player && !rgui->media_gpu.anim_sess && !rgui->media.controls.target.user);
   if (gpu_surface)
   {
      char broken_path[PATH_MAX_LENGTH];
      FILE *broken;
      for (i = 0; i < 8; i++)
      {
         rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_OK);
         for (tick = 0; tick < 2000; tick++)
         {
            media_render(menu_st);
            if (media_picture_ready(rgui)) break;
            retro_sleep(1);
         }
         MEDIA_CHECK("replacement viewer has a new playable session",
               tick < 2000 && media_picture_ready(rgui));
         if (media_failures) break;
         MEDIA_CHECK("replacement accepts seek before retirement",
               rgui_media_seek(rgui, 375000, menu_st->current_time_us));
         rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_CANCEL);
         media_render(menu_st);
         MEDIA_CHECK("retirement cancels pending transport ownership",
               !rgui->media_gpu.anim_sess && !rgui->media.controls.target.user);
      }
      MEDIA_CHECK("retired surface disappears from the renderer",
            media_capture_settled(menu_st, 6));
      strlcpy(rgui->media_requested_path, argv[1], sizeof(rgui->media_requested_path));
      strlcat(rgui->media_requested_path, ".missing.mp4", sizeof(rgui->media_requested_path));
      rgui->flags |= RGUI_FLAG_SHOW_FULLSCREEN_THUMBNAIL;
      media_render(menu_st);
      MEDIA_CHECK("missing replacement has no playable target",
            !rgui->media_gpu.anim_sess && !rgui->media.controls.target.user
            && retro_atomic_load_acquire_int(&rgui->media_gpu.status)
               == GFX_THUMBNAIL_STATUS_MISSING);
      snprintf(broken_path, sizeof(broken_path), "%s.broken.mp4", argv[2]);
      broken = fopen(broken_path, "wb");
      MEDIA_CHECK("broken movie fixture opens", broken != NULL);
      if (!broken) goto cleanup;
      fputs("not a video", broken);
      fclose(broken);
      strlcpy(rgui->media_requested_path, broken_path, sizeof(rgui->media_requested_path));
      media_render(menu_st);
      MEDIA_CHECK("failed video admission queues a still-image task",
            retro_atomic_load_acquire_int(&rgui->media_gpu.status)
               == GFX_THUMBNAIL_STATUS_PENDING);
      rgui_media_close(rgui);
      task_queue_check();
      MEDIA_CHECK("closing invalidates queued image delivery",
            !rgui->media_gpu.anim_sess && !rgui->media.controls.target.user
            && retro_atomic_load_acquire_int(&rgui->media_gpu.status)
               == GFX_THUMBNAIL_STATUS_UNKNOWN);
      remove(broken_path);
      strlcpy(rgui->media_requested_path, argv[1], sizeof(rgui->media_requested_path));
      for (tick = 0; tick < 2000; tick++)
      {
         media_render(menu_st);
         if (media_picture_ready(rgui)) break;
         retro_sleep(1);
      }
      MEDIA_CHECK("valid replacement recovers after failed admission",
            tick < 2000 && media_picture_ready(rgui));
   }
cleanup:
   free(entry);
   main_exit(NULL);
   printf("rgui media: %u failure(s)\n", media_failures);
   return media_failures ? 1 : 0;
}
