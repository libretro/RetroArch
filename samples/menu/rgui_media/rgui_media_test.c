/* Links the shipping frontend with this real RGUI translation unit.
 * Only main() and the RGUI object are replaced; no playback/UI stubs. */
#include "../../../menu/drivers/rgui.c"
#include "../../../retroarch.h"
#include "../../../tasks/task_content.h"
#include "../../../paths.h"
#include "../../../frontend/frontend.h"
#include "../../../gfx/gfx_surface_player.h"
#include "../../../driver.h"
#include <time/rtime.h>
#ifdef _WIN32
#include "../../../gfx/common/win32_common.h"
#endif
#include <retro_timers.h>
#ifdef MEDIA_TEST_COLD_CANCEL
#include <rthreads/rthreads.h>
#endif

static unsigned media_failures;
static bool native_gpu;
static bool gpu_surface;
#ifdef MEDIA_TEST_COLD_CANCEL
static retro_atomic_int_t cold_gate;
static retro_atomic_int_t cold_entered;
static retro_atomic_int_t cold_finished;
static uintptr_t cold_poll_owner;
static uintptr_t cold_open_owner;

bool __real_image_transfer_anim_stream_set_hw(void *stream,
      enum image_type_enum type, bool enable);

bool __wrap_image_transfer_anim_stream_set_hw(void *stream,
      enum image_type_enum type, bool enable)
{
   bool held = enable && retro_atomic_cas_int(&cold_gate, 1, 2);
   bool result;
   if (held)
   {
      unsigned ticks;
      cold_open_owner = sthread_get_current_thread_id();
      retro_atomic_store_release_int(&cold_entered, 1);
      for (ticks = 0; ticks < 5000
            && retro_atomic_load_acquire_int(&cold_gate) == 2; ticks++)
         retro_sleep(1);
   }
   result = __real_image_transfer_anim_stream_set_hw(stream, type, enable);
   if (held)
      retro_atomic_store_release_int(&cold_finished, 1);
   return result;
}
#ifdef MEDIA_TEST_COLD_RESET
static bool cold_free_unsafe;
void __real_gfx_thumbnail_anim_worker_deinit(void);
void __real_video_driver_free_internal(void);

void __wrap_gfx_thumbnail_anim_worker_deinit(void)
{
   retro_atomic_store_release_int(&cold_gate, 0);
   __real_gfx_thumbnail_anim_worker_deinit();
}

void __wrap_video_driver_free_internal(void)
{
   if (retro_atomic_load_acquire_int(&cold_entered)
         && !retro_atomic_load_acquire_int(&cold_finished))
   {
      cold_free_unsafe = true;
      /* Record the failure, then make baseline cleanup safe. */
      retro_atomic_store_release_int(&cold_gate, 0);
      __real_gfx_thumbnail_anim_worker_deinit();
   }
   __real_video_driver_free_internal();
}
#endif
#endif
#ifdef MEDIA_TEST_IMPORT_REFUSAL
static unsigned refused_imports;
static unsigned queued_refusals;

enum gfx_surface_submit_result __real_gfx_surface_submit_import(
      gfx_surface_t *surface, const video_frame_import_t *frame);

enum gfx_surface_submit_result __wrap_gfx_surface_submit_import(
      gfx_surface_t *surface, const video_frame_import_t *frame)
{
   video_frame_import_t refused = *frame;
   enum gfx_surface_submit_result result;
   if (frame->image)
      refused_imports++;
   /* Keep the native lease intact; the renderer rejects this output format. */
   refused.output_pixfmt = IMAGE_PIXFMT_NV12;
   result = __real_gfx_surface_submit_import(surface, &refused);
   if (result == GFX_SURFACE_SUBMIT_QUEUED)
      queued_refusals++;
   return result;
}
#endif
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
#include "../texture_media/win32_modal_test.h"
static bool media_test_subtitle_glyph(void)
{
   rgui_t *rgui = (rgui_t*)menu_state_get_ptr()->userdata;
   unsigned x, y, i, j;
   unsigned width = rgui->frame_buf.width;
   unsigned height = rgui->frame_buf.height;
   int top = rgui->media.view.visible ? rgui->media.view.layout.panel.y - 2
      : (int)height - (int)rgui->font_height_stride;
   if (!rgui->frame_buf.data || !rgui->font_width_stride
         || !rgui->font_height_stride || top < (int)rgui->font_height_stride)
      return false;
   x = (width - 7 * rgui->font_width_stride) / 2 + 6 * rgui->font_width_stride;
   y = top - rgui->font_height_stride;
   if (x + FONT_WIDTH > width || y + FONT_HEIGHT > height)
      return false;
   for (j = 0; j < FONT_HEIGHT; j++)
      for (i = 0; i < FONT_WIDTH; i++)
         if (rgui->frame_buf.data[(y + j) * width + x + i]
               == rgui->colors.normal_color)
            return true;
   return false;
}

#define MEDIA_SUBTITLE_GLYPH_CHECK() media_test_subtitle_glyph()
#include "../texture_media/subtitle_media_test.h"

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

#include "../texture_media/folder_playback_test.h"
#include "../texture_media/media_cadence_test.h"
#ifdef _WIN32
static bool media_current_visible(struct menu_state *menu_st)
{
   rgui_t *rgui = (rgui_t*)menu_st->userdata;
   return (rgui->flags & RGUI_FLAG_SHOW_FULLSCREEN_THUMBNAIL) && rgui->fs_thumbnail.is_valid;
}
#endif

static bool media_current_target(struct menu_state *menu_st,
      gfx_media_target_t *target)
{
   rgui_t *rgui = (rgui_t *)menu_st->userdata;
   *target = rgui->media.controls.target;
   return target->user && target->state;
}

static int media_folder_stage(void *data)
{
   rgui_t *rgui = (rgui_t*)data;
   if (gfx_media_cpu_completed(rgui->media_player)
         || (rgui->media_gpu.flags & GFX_THUMB_FLAG_MEDIA_ENDED)) return 2;
   if (media_picture_ready(rgui))
   {
      if (!strncmp(path_basename(rgui->media_path), "episode10.", 10)) return 1;
      if (!strncmp(path_basename(rgui->media_path), "episode2.", 9)) return 3;
   }
   return rgui->media_player || rgui->media_gpu.anim_player ? 0 : 2;
}
static bool media_folder_shown(void *data)
{
   return (((rgui_t*)data)->flags & RGUI_FLAG_SHOW_FULLSCREEN_THUMBNAIL) != 0;
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
#ifdef MEDIA_TEST_COLD_CANCEL
   bool cold_canceled = false;
#endif
   bool folder = getenv("MEDIA_TEST_CONTINUOUS") != NULL;
   const char *first_video = getenv("MEDIA_TEST_FIRST_VIDEO_US");
   int64_t first_video_us = first_video ? strtol(first_video, NULL, 10) : 0;
#ifdef MEDIA_TEST_COLD_CANCEL
   retro_atomic_int_init(&cold_gate, 1);
   retro_atomic_int_init(&cold_entered, 0);
   retro_atomic_int_init(&cold_finished, 0);
   cold_poll_owner = sthread_get_current_thread_id();
#endif
   setvbuf(stdout, NULL, _IONBF, 0);
   if (argc != 3) return 2;
   native_gpu=getenv("MEDIA_TEST_GPU")!=NULL;
   gpu_surface=getenv("MEDIA_TEST_GPU_SURFACE")!=NULL;
   if (gpu_surface && !native_gpu) return 2;
   config_file_set_io_default(config_file_io_filestream());
   rtime_init(); retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   rarch_argv[0] = (char*)"retroarch"; rarch_argv[1] = getenv("MEDIA_TEST_EXTERNAL") ? argv[1] : (char*)"--menu";
   rarch_argv[2] = (char*)"--config"; rarch_argv[3] = argv[2];
   if (!retroarch_main_init(4, rarch_argv)) return 1;
   if (!media_native_video_driver_ready())
   {
      main_exit(NULL);
      return 1;
   }
#ifdef _WIN32
   if (!media_native_modal_audio_ready())
   {
      main_exit(NULL);
      return 1;
   }
#endif
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
   settings->bools.menu_thumbnail_preview_audio = getenv("MEDIA_TEST_MODAL_MODE") != NULL;
   if (getenv("MEDIA_TEST_EXTERNAL") || getenv("MEDIA_TEST_BROWSER_IMAGE"))
   {
      bool image = image_texture_get_type(argv[1]) != IMAGE_TYPE_MP4
         && image_texture_get_type(argv[1]) != IMAGE_TYPE_WEBM;
      bool first_frame = getenv("MEDIA_TEST_FIRST_FRAME") != NULL;
      bool startup_core_untouched = true;
      bool startup_readback = true;
      unsigned startup_frames = 0, startup_nonblack = 0;
      int startup_token = menu_driver_media_startup_token();
      if (getenv("MEDIA_TEST_STARTUP_FAILURE"))
      {
         MEDIA_CHECK("failed media releases startup presentation",
               media_startup_failure_check(menu_st, media_render));
         goto cleanup;
      }
      if (!getenv("MEDIA_TEST_EXTERNAL"))
         MEDIA_CHECK("image browser request accepted", menu_driver_media_open(argv[1]));
      if (first_frame)
      {
         /* Establish the first readable loading presentation before
          * pumping tasks that may publish the ready viewer at once. */
         for (tick = 0; tick < 32; tick++)
         {
            media_render(menu_st);
            if (media_native_startup_black(&startup_nonblack)) break;
            retro_sleep(1);
         }
         startup_readback = tick < 32;
         startup_frames++;
      }
      for (tick = 0; tick < 4000; tick++)
      {
         task_queue_check();
         rgui = (rgui_t *)menu_st->userdata;
         menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
               settings, MENU_ACTION_NOOP, cpu_features_get_time_usec());
         startup_core_untouched = !task_content_load_pending()
            && runloop_state_get_ptr()->current_core_type == CORE_TYPE_DUMMY
            && startup_core_untouched;
         media_render(menu_st);
         if (media_folder_shown(rgui) && (rgui->fs_thumbnail.is_valid
                  || (!image && rgui->media_gpu.texture
                     && rgui->media_gpu.anim_player))) break;
         if (first_frame)
         {
            startup_readback = media_native_startup_black(&startup_nonblack)
               && startup_readback;
            startup_frames++;
         }
         retro_sleep(1);
      }
      MEDIA_CHECK("opened file reaches internal fullscreen view", tick < 4000);
      {
         const char *title = NULL;
         MEDIA_CHECK("fullscreen viewer title is the active filename",
               gfx_thumbnail_get_label(menu_st->thumbnail_path_data, &title)
               && string_is_equal(title, path_basename(argv[1])));
      }
      if (first_frame)
      {
         printf("startup presentation: frames=%u nonblack=%u readback=%u\n",
               startup_frames, startup_nonblack, startup_readback);
         MEDIA_CHECK("every loading frame can be captured", startup_readback);
         MEDIA_CHECK("loading frames do not expose the browser", !startup_nonblack);
      }
      MEDIA_CHECK("external viewer never queues a content-core load", startup_core_untouched);
      MEDIA_CHECK("opened file does not launch a content core",
            runloop_state_get_ptr()->current_core_type == CORE_TYPE_DUMMY
            && !*path_get(RARCH_PATH_CONTENT));
#ifdef _WIN32
      if (image && getenv("MEDIA_TEST_MODAL_MODE"))
      {
         MEDIA_CHECK("image remains visible through native resize and restore",
               native_gpu && media_native_modal_image(menu_st, media_current_visible));
         goto cleanup;
      }
      if (image && native_gpu)
      {
         unsigned resize_tick;
         ui_companion_driver_init_first(false, false, false);
         ShowWindowAsync(win32_get_window(), SW_MAXIMIZE);
         for (resize_tick = 0; resize_tick < 30; resize_tick++)
         {
            media_modal_normal_tick();
            retro_sleep(1);
         }
         MEDIA_CHECK("maximizing retains image viewer",
               IsZoomed(win32_get_window()) && ((rgui->flags & RGUI_FLAG_SHOW_FULLSCREEN_THUMBNAIL) != 0));
         ShowWindowAsync(win32_get_window(), SW_RESTORE);
         for (resize_tick = 0; resize_tick < 30; resize_tick++)
         {
            media_modal_normal_tick();
            retro_sleep(1);
         }
         MEDIA_CHECK("restoring retains image viewer",
               !IsZoomed(win32_get_window()) && ((rgui->flags & RGUI_FLAG_SHOW_FULLSCREEN_THUMBNAIL) != 0));
      }
#endif
      if (image)
      {
         menu_ctx_pointer_t point;
         memset(&point, 0, sizeof(point));
         point.x = 100;
         point.y = -20;
         point.gesture = MENU_INPUT_GESTURE_TAP;
         menu_driver_ctl(RARCH_MENU_CTL_POINTER_UP, &point);
         MEDIA_CHECK("non-client release retains image viewer",
               (rgui->flags & RGUI_FLAG_SHOW_FULLSCREEN_THUMBNAIL) != 0);
      }
      if (image)
      {
         menu_ctx_pointer_t point;
         unsigned dims = menu_st->driver_ctx->set_texture
            ? disp_get_ptr()->framebuf_dims : video_driver_get_ui_dims();
         memset(&point, 0, sizeof(point));
         point.x = VIDEO_SCALE_W(dims) + 4;
         point.y = VIDEO_SCALE_H(dims) / 2;
         menu_driver_ctl(RARCH_MENU_CTL_POINTER_DOWN, &point);
         point.x = VIDEO_SCALE_W(dims) / 2;
         point.gesture = MENU_INPUT_GESTURE_TAP;
         menu_driver_ctl(RARCH_MENU_CTL_POINTER_UP, &point);
         MEDIA_CHECK("non-client resize press cannot become an image-dismiss tap", ((rgui->flags & RGUI_FLAG_SHOW_FULLSCREEN_THUMBNAIL) != 0));
      }
      if (image)
         MEDIA_CHECK("image confirmation remains in internal view",
               rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_OK)
                  == MENU_ACTION_NOOP && media_folder_shown(rgui));
#ifdef _WIN32
      if (getenv("MEDIA_TEST_MODAL_MODE"))
      {
         MEDIA_CHECK(getenv("MEDIA_TEST_MODAL_AUDIO_ONLY")
            ? "movie audio continues inside the native modal loop without capture"
            : "movie pixels and audio continue inside the native modal loop",
               native_gpu && media_native_modal_playback(menu_st, media_current_target));
         goto cleanup;
      }
#endif
      rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_CANCEL);
      MEDIA_CHECK("Back closes internal view", !media_folder_shown(rgui));
      MEDIA_CHECK("cancellation and stale startup acknowledgements stay isolated",
            media_startup_cancellation_check(menu_st, argv[1], startup_token));
      goto cleanup;
   }
   list = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
   stack = MENU_LIST_GET(menu_st->entries.list, 0);
   fill_pathname_basedir(directory, argv[1], sizeof(directory));
   file_list_clear(list); file_list_clear(stack);
   MEDIA_CHECK("file-browser stack installed", menu_entries_append(stack, directory,
         MENU_ENUM_LABEL_FAVORITES_STR, MENU_ENUM_LABEL_FAVORITES,
         FILE_TYPE_DIRECTORY, 0, 0, NULL));
   if (folder)
      MEDIA_CHECK("folder movies installed", media_folder_install(menu_st, list, directory, argv[1]));
   else
      MEDIA_CHECK("movie entry installed", file_list_append(list, path_basename(argv[1]),
            "", FILE_TYPE_MOVIE, 0, 0));
   menu_st->selection_ptr = folder ? 1 : 0; menu_st->current_time_us = native_gpu?cpu_features_get_time_usec():1000000;
   rgui->flags &= ~(RGUI_FLAG_IS_PLAYLIST | RGUI_FLAG_IS_EXPLORE_LIST | RGUI_FLAG_IS_QUICK_MENU);
   rgui_scan_selected_entry_thumbnail(rgui, true);
   MEDIA_CHECK("file-browser movie resolves without a playlist", *rgui->media_requested_path);
   MEDIA_CHECK("movie activation consumed before core launch",
         rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_OK) == MENU_ACTION_NOOP
         && (rgui->flags & RGUI_FLAG_SHOW_FULLSCREEN_THUMBNAIL));
   for (tick = 0; tick < 2000; tick++)
   {
      media_render(menu_st);
#ifdef MEDIA_TEST_COLD_CANCEL
      if (!cold_canceled && retro_atomic_load_acquire_int(&cold_entered))
      {
         MEDIA_CHECK("cold hardware admission runs off the polling owner",
               cold_open_owner != cold_poll_owner);
#ifdef MEDIA_TEST_COLD_RESET
         driver_uninit(DRIVER_VIDEO_MASK, DRIVER_LIFETIME_RESET);
         MEDIA_CHECK("video-only teardown drains admission before device free",
               !cold_free_unsafe
               && retro_atomic_load_acquire_int(&cold_finished));
         drivers_init(settings, DRIVER_VIDEO_MASK, DRIVER_LIFETIME_RESET, false);
         MEDIA_CHECK("video-only reset retains the requested renderer",
               media_native_video_driver_ready());
#else
         rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_CANCEL);
         media_render(menu_st);
         MEDIA_CHECK("closing held admission detaches the player",
               !rgui->media_gpu.anim_player && !rgui->media_gpu.anim_sess);
         rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_OK);
         media_render(menu_st);
#endif
         cold_canceled = true;
         retro_atomic_store_release_int(&cold_gate, 0);
      }
#endif
      if (media_picture_ready(rgui)) break;
      retro_sleep(1);
   }
   MEDIA_CHECK("real decoder supplies the requested presentation path",
         tick < 2000 && media_picture_ready(rgui));
   if (media_failures) goto cleanup;
#ifdef MEDIA_TEST_COLD_CANCEL
   MEDIA_CHECK("replacement survives cancellation during native admission",
         cold_canceled && media_picture_ready(rgui));
   if (media_failures) goto cleanup;
#endif
#ifdef MEDIA_TEST_IMPORT_REFUSAL
   MEDIA_CHECK("native import refusal recovers through software decoding",
         gpu_surface && refused_imports && queued_refusals
         && rgui->media_gpu.anim_surface
         && !((gfx_surface_t*)rgui->media_gpu.anim_surface)->imported);
   if (media_failures) goto cleanup;
#endif
   if (getenv("MEDIA_TEST_HW_DECODE"))
      MEDIA_CHECK("movie uses the hardware decoder sink",
            gpu_surface && rgui->media_gpu.anim_player
            && ((gfx_surface_player_t*)rgui->media_gpu.anim_player)->hw_output_pixfmt
            && rgui->media_gpu.anim_surface
            && ((gfx_surface_t*)rgui->media_gpu.anim_surface)->imported);
   if (media_failures) goto cleanup;
   if (folder)
   {
      MEDIA_CHECK("natural folder playback reaches next episode and stops",
            media_folder_drive(menu_st, &rgui->media, media_render,
                  media_folder_stage, media_folder_shown, rgui));
      rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_CANCEL);
      media_render(menu_st);
      MEDIA_CHECK("Back cancels the folder queue", !media_folder_shown(rgui) && !rgui->media.queue);
      goto cleanup;
   }

   if (getenv("MEDIA_TEST_CADENCE_HZ"))
   {
      MEDIA_CHECK("movie timestamps supply stable source cadence",
            media_cadence_check(menu_st, &rgui->media, media_render));
   }
   if (media_failures) goto cleanup;
   for (i = 0; i < VIDEO_SCALE_AREA(rgui->fs_thumbnail.dims); i++) pixels |= rgui->fs_thumbnail.data[i];
   MEDIA_CHECK("decoded movie picture is nonempty",
         gpu_surface ? rgui->media_gpu.texture != 0 : pixels != 0);
#ifdef _WIN32
   if (getenv("MEDIA_TEST_MODAL_MODE"))
   {
      MEDIA_CHECK(getenv("MEDIA_TEST_MODAL_AUDIO_ONLY")
            ? "movie audio continues inside the native modal loop without capture"
            : "movie pixels and audio continue inside the native modal loop",
            native_gpu && media_native_modal_playback(menu_st, media_current_target));
      goto cleanup;
   }
#endif
   if (getenv("MEDIA_TEST_WINDOW_TOGGLE"))
   {
      MEDIA_CHECK("window fullscreen toggles retain seeking and updating pixels",
            native_gpu && media_native_window_toggle(menu_st, media_render,
               media_current_target));
      goto cleanup;
   }
   MEDIA_CHECK("fullscreen shared controller owns the player", rgui->media.controls.target.user == rgui);
   MEDIA_CHECK("confirm pauses through the real menu adapter",
         rgui_parse_menu_entry_action(rgui, entry, MENU_ACTION_OK) == MENU_ACTION_NOOP
         && rgui_media_state(rgui, menu_st->current_time_us, &state) && state.paused);
   MEDIA_CHECK("sidecar subtitle owner, delay and disable apply",
         media_test_subtitles(&rgui->media, menu_st, media_render));
   if(native_gpu)
   {
      gfx_media_target_t target=rgui->media.controls.target;
      uint32_t first=0,middle=0,again=0;
      unsigned burst;
      MEDIA_CHECK("paused movie picture reaches renderer",media_native_seek_capture(menu_st,media_render,&target,first_video_us,0,0,0,&first));
      MEDIA_CHECK("movie seek changes rendered picture",media_native_seek_capture(menu_st,media_render,&target,first_video_us+375000,first,1,1,&middle));
      MEDIA_CHECK("movie backward seek restores picture",media_native_seek_capture(menu_st,media_render,&target,first_video_us,first,-1,2,&again));
      for(burst=0;burst<16;burst++)
      {
         MEDIA_CHECK("movie burst seek accepted",target.seek(target.user,burst&1?first_video_us+375000:first_video_us,cpu_features_get_time_usec()));
         media_render(menu_st);retro_sleep(1);
      }
      MEDIA_CHECK("movie latest burst reaches renderer while paused",media_native_seek_capture(menu_st,media_render,&target,first_video_us+375000,first,1,3,&middle));
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
   {
      int64_t position = state.position_us;
      bool restored = false;
      driver_uninit(DRIVER_VIDEO_MASK, DRIVER_LIFETIME_RESET);
      drivers_init(settings, DRIVER_VIDEO_MASK, DRIVER_LIFETIME_RESET, false);
      rgui = (rgui_t*)menu_st->userdata;
      for (tick = 0; tick < 2000 && !restored; tick++)
      {
         media_render(menu_st);
         restored = rgui_media_state(rgui, menu_st->current_time_us, &state)
               && !state.seeking && state.paused
               && state.position_us >= position - 100000
               && state.position_us <= position + 100000;
         if (!restored)
            retro_sleep(1);
      }
      MEDIA_CHECK("video reinit preserves the paused movie position", restored);
   }
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
      rgui_media_close(rgui, false);
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
#ifdef MEDIA_TEST_COLD_CANCEL
   retro_atomic_store_release_int(&cold_gate, 0);
#endif
   free(entry);
   main_exit(NULL);
   printf("rgui media: %u failure(s)\n", media_failures);
   return media_failures ? 1 : 0;
}
