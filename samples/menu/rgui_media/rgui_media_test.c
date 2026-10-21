/* Links the shipping frontend with this real RGUI translation unit.
 * Only main() and the RGUI object are replaced; no playback/UI stubs. */
#include "../../../menu/drivers/rgui.c"
#include "../../../retroarch.h"
#include "../../../frontend/frontend.h"
#include <time/rtime.h>
#include <retro_timers.h>

static unsigned media_failures;
static bool native_gpu;
static uint32_t native_background[320*240];
#define MEDIA_CHECK(message, cond) do { \
   bool passed = (cond) != 0; \
   printf("%s: %s\n", passed ? "ok" : "FAIL", message); \
   if (!passed) media_failures++; \
} while (0)

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
   if (argc != 3) return 2;
   native_gpu=getenv("MEDIA_TEST_GPU")!=NULL;
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
      if (rgui->fs_thumbnail.is_valid && rgui->mini_thumbnail.is_valid) break;
      retro_sleep(1);
   }
   MEDIA_CHECK("real decoder filled fullscreen and mini CPU thumbnails",
         tick < 2000 && rgui->media_player && rgui->fs_thumbnail.dims && rgui->mini_thumbnail.dims);
   if (media_failures) goto cleanup;
   for (i = 0; i < VIDEO_SCALE_AREA(rgui->fs_thumbnail.dims); i++) pixels |= rgui->fs_thumbnail.data[i];
   MEDIA_CHECK("converted 16-bit movie picture is nonempty", pixels != 0);
   MEDIA_CHECK("fullscreen shared controller owns the CPU player", rgui->media.controls.target.user == rgui);
   MEDIA_CHECK("confirm pauses through the real menu adapter",
         rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_OK) == MENU_ACTION_NOOP
         && gfx_media_cpu_state(rgui->media_player, menu_st->current_time_us, &state) && state.paused);
   if(native_gpu)
   {
      gfx_media_target_t target=rgui->media.controls.target;
      uint32_t first=0,middle=0,again=0;
      unsigned burst;
      MEDIA_CHECK("paused CPU picture reaches renderer",media_native_seek_capture(menu_st,media_render,&target,0,0,0,0,&first));
      MEDIA_CHECK("CPU seek changes rendered picture",media_native_seek_capture(menu_st,media_render,&target,375000,first,1,1,&middle));
      MEDIA_CHECK("CPU backward seek restores picture",media_native_seek_capture(menu_st,media_render,&target,0,first,-1,2,&again));
      for(burst=0;burst<16;burst++)
      {
         MEDIA_CHECK("CPU burst seek accepted",target.seek(target.user,burst&1?375000:0,cpu_features_get_time_usec()));
         media_render(menu_st);retro_sleep(1);
      }
      MEDIA_CHECK("CPU latest burst reaches renderer while paused",media_native_seek_capture(menu_st,media_render,&target,375000,first,1,3,&middle));
      printf("rendered CPU media hashes: %08x %08x %08x\n",first,middle,again);
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
         && gfx_media_cpu_state(rgui->media_player, menu_st->current_time_us, &state) && state.seeking);
   menu_st->input_state.pointer.flags = 0;
   seeking = true;
   for (tick = 0; tick < 2000 && seeking; tick++)
   {
      media_render(menu_st);
      gfx_media_cpu_state(rgui->media_player, menu_st->current_time_us, &state);
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
      gfx_media_cpu_state(rgui->media_player, menu_st->current_time_us, &state);
      seeking = state.seeking;
      retro_sleep(1);
   }
   MEDIA_CHECK("RGUI Stop lands at the start and stays paused",
         !seeking && state.paused && state.position_us == 0);

   settings->bools.menu_rgui_inline_thumbnails = true;
   rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_CANCEL);
   media_render(menu_st);
   MEDIA_CHECK("paused inline picture survives leaving fullscreen",
         rgui->media_player && rgui->mini_thumbnail.is_valid);
   rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_OK);
   media_render(menu_st);
   MEDIA_CHECK("paused fullscreen picture survives reopening",
         rgui->fs_thumbnail.is_valid && rgui->mini_thumbnail.is_valid
         && gfx_media_cpu_state(rgui->media_player, menu_st->current_time_us, &state) && state.paused);
   settings->bools.menu_rgui_inline_thumbnails = false;
   rgui_media_seek(rgui, 1000000, menu_st->current_time_us);
   media_render(menu_st);
   MEDIA_CHECK("Back is consumed by viewer close",
         rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_CANCEL) == MENU_ACTION_NOOP);
   media_render(menu_st);
   MEDIA_CHECK("closing viewer retires player when inline previews are disabled",
         !rgui->media_player && !rgui->media.controls.target.user);
cleanup:
   free(entry);
   main_exit(NULL);
   printf("rgui media: %u failure(s)\n", media_failures);
   return media_failures ? 1 : 0;
}
