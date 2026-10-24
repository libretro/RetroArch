#ifndef MEDIA_WIN32_MODAL_TEST_H
#define MEDIA_WIN32_MODAL_TEST_H
#ifdef _WIN32
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <audio/audio_mixer.h>
#include "../../../audio/audio_driver.h"
#include "../../../runloop.h"
#include "../../../ui/ui_companion_driver.h"
#include "../../../gfx/video_thread_wrapper.h"

typedef struct media_modal_av_probe
{
   HWND hwnd;
   WPARAM command;
   DWORD owner;
   unsigned moves;
   retro_atomic_int_t active, verified, done;
   bool entered, exited;
} media_modal_av_probe_t;

static media_modal_av_probe_t media_modal_probe;

static LRESULT CALLBACK media_modal_message(int code, WPARAM wp, LPARAM lp)
{
   if (code >= 0)
   {
      const CWPSTRUCT *message = (const CWPSTRUCT*)lp;
      if (message->hwnd == media_modal_probe.hwnd)
      {
         if (message->message == WM_ENTERSIZEMOVE)
            retro_atomic_store_release_int(&media_modal_probe.active, 1);
         else if (message->message == WM_EXITSIZEMOVE)
            retro_atomic_store_release_int(&media_modal_probe.active, 0);
      }
   }
   return CallNextHookEx(NULL, code, wp, lp);
}

static void media_modal_av_worker(void *data)
{
   media_modal_av_probe_t *probe = (media_modal_av_probe_t*)data;
   int64_t deadline = cpu_features_get_time_usec() + 4000000;
   bool stopping = false;
   int64_t entered_at = 0;
   POINT origin;
   origin.x = origin.y = 0;
   PostMessage(probe->hwnd, WM_SYSCOMMAND, probe->command, 0);
   while (cpu_features_get_time_usec() < deadline)
   {
      if (retro_atomic_load_acquire_int(&probe->active))
      {
         if (!probe->entered)
         {
            GetCursorPos(&origin);
            entered_at = cpu_features_get_time_usec();
            probe->entered = true;
         }
         if (probe->moves < 2 && cpu_features_get_time_usec()
               >= entered_at + (int64_t)(probe->moves + 1) * 25000)
         {
            probe->moves++;
            if (probe->command == SC_MOVE)
            {
               PostMessage(probe->hwnd, WM_KEYDOWN, VK_RIGHT, 0);
               PostMessage(probe->hwnd, WM_KEYDOWN, VK_DOWN, 0);
            }
            else
               SetCursorPos(origin.x + (int)probe->moves * 32,
                     origin.y + (int)probe->moves * 24);
         }
         if (!stopping && retro_atomic_load_acquire_int(&probe->verified))
         {
            PostMessage(probe->hwnd, WM_KEYDOWN, VK_RETURN, 0);
            stopping = true;
         }
      }
      else if (probe->entered)
      {
         probe->exited = true;
         break;
      }
      retro_sleep(1);
   }
   if (!probe->exited)
   {
      int64_t cleanup = cpu_features_get_time_usec() + 1000000;
      for (;;)
      {
         PostMessage(probe->hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
         PostMessage(probe->hwnd, WM_CANCELMODE, 0, 0);
         if (!retro_atomic_load_acquire_int(&probe->active))
            break;
         if (cpu_features_get_time_usec() >= cleanup)
         {
            fprintf(stderr, "FAIL: modal A/V probe could not retire its window loop\n");
            ExitProcess(1);
         }
         retro_sleep(1);
      }
   }
   retro_atomic_store_release_int(&probe->active, 0);
   retro_atomic_store_release_int(&probe->done, 1);
}

static audio_mixer_voice_t *media_modal_audio_voice(void)
{
   unsigned i;
   for (i = 0; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
   {
      const char *name = audio_driver_mixer_get_stream_name(i);
      if (name && !strcmp(name, "__gfx_thumb_preview"))
      {
         audio_mixer_stream_t *stream = audio_driver_mixer_get_stream(i);
         return stream ? stream->voice : NULL;
      }
   }
   return NULL;
}

static uint64_t media_modal_present_count(void)
{
#ifdef HAVE_THREADS
   return video_thread_swap_count();
#else
   return 0;
#endif
}

static bool media_modal_normal_tick(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   bool blocked = (menu_st->flags & MENU_ST_FLAG_BLOCK_ALL_INPUT) != 0;
   int result;
   /* Physical controllers must not steer the fixture. */
   menu_st->flags |= MENU_ST_FLAG_BLOCK_ALL_INPUT;
   ui_companion_driver_wimp_iterate();
   result = runloop_iterate();
   task_queue_check();
   if (!blocked)
      menu_st->flags &= ~MENU_ST_FLAG_BLOCK_ALL_INPUT;
   return result != -1;
}

static bool media_native_modal_playback(struct menu_state *menu_st,
      bool (*get_target)(struct menu_state*, gfx_media_target_t*))
{
   media_modal_av_probe_t *probe = &media_modal_probe;
   HHOOK hook;
   gfx_media_target_t target;
   gfx_media_playback_state_t initial, current;
   sthread_t *thread;
   POINT cursor;
   RECT rect, final_rect, sampled_rect, live_rect;
   audio_mixer_voice_t *voice;
   size_t first_frames = 0, frames = 0, first_serial = 0, serial = 0;
   unsigned rate = 0, warmup, live = 0, stable_geometry = 0;
   uint32_t first_hash = 0, last_hash = 0;
   uint64_t first_present = 0, presents = 0;
   int64_t deadline;
   bool sampled = false, changed = false, geometry = false, valid = true;
   const char *mode = getenv("MEDIA_TEST_MODAL_MODE");
   memset(probe, 0, sizeof(*probe));
   memset(&initial, 0, sizeof(initial));
   ui_companion_driver_init_first(false, false, false);
   if (!get_target(menu_st, &target)
         || !target.seek(target.user, 0, cpu_features_get_time_usec())
         || !target.pause(target.user, false, cpu_features_get_time_usec()))
      return false;
   for (warmup = 0; warmup < 32; warmup++)
      if (!media_modal_normal_tick())
         return false;
   if (!get_target(menu_st, &target)
         || !target.state(target.user, cpu_features_get_time_usec(), &current))
      return false;
   printf("modal start: position=%ld duration=%ld seeking=%u paused=%u\n",
         (long)(current.position_us / 1000), (long)(current.duration_us / 1000),
         current.seeking, current.paused);
   voice = media_modal_audio_voice();
   if (!voice || !audio_mixer_voice_clock(voice, &frames, &rate, &serial)
         || !(AUDIO_FLAGS_GET(audio_state_get_ptr()) & AUDIO_FLAG_ACTIVE))
   {
      fprintf(stderr, "FAIL: modal A/V probe needs a live preview audio voice (%s, voice=%p flags=%x enabled=%u)\n",
            audio_driver_get_ident(), (void*)voice, AUDIO_FLAGS_GET(audio_state_get_ptr()),
            config_get_ptr()->bools.menu_thumbnail_preview_audio);
      return false;
   }
   probe->hwnd = win32_get_window();
   probe->owner = GetWindowThreadProcessId(probe->hwnd, NULL);
   printf("modal window: hwnd=%p owner=%lu current=%lu style=%lx visible=%u fullscreen=%u ui=%s\n",
         (void*)probe->hwnd, (unsigned long)probe->owner,
         (unsigned long)GetCurrentThreadId(),
         (unsigned long)GetWindowLongPtr(probe->hwnd, GWL_STYLE),
         IsWindowVisible(probe->hwnd), config_get_ptr()->bools.video_fullscreen,
         uico_state_get_ptr()->drv ? uico_state_get_ptr()->drv->ident : "none");
   probe->command = mode && !strcmp(mode, "move")
      ? SC_MOVE : (SC_SIZE | WMSZ_BOTTOMRIGHT);
   retro_atomic_int_init(&probe->active, 0);
   retro_atomic_int_init(&probe->verified, 0);
   retro_atomic_int_init(&probe->done, 0);
   hook = SetWindowsHookEx(WH_CALLWNDPROC, media_modal_message, NULL, probe->owner);
   if (!hook)
      return false;
   GetCursorPos(&cursor);
   GetWindowRect(probe->hwnd, &rect);
   sampled_rect = rect;
   if (mode && !strcmp(mode, "move"))
      SetCursorPos(rect.left + 100, rect.top + 20);
   else
      SetCursorPos(rect.right - 4, rect.bottom - 4);
   thread = sthread_create(media_modal_av_worker, probe);
   if (!thread)
   {
      SetCursorPos(cursor.x, cursor.y);
      UnhookWindowsHookEx(hook);
      return false;
   }
   deadline = cpu_features_get_time_usec() + 6000000;
   while (!retro_atomic_load_acquire_int(&probe->done)
         && cpu_features_get_time_usec() < deadline)
   {
      if (!media_modal_normal_tick())
      {
         valid = false;
         break;
      }
      if (!retro_atomic_load_acquire_int(&probe->active))
         continue;
      live++;
      if (!get_target(menu_st, &target)
            || !target.state(target.user, cpu_features_get_time_usec(), &current)
            || media_modal_audio_voice() != voice
            || !audio_mixer_voice_clock(voice, &frames, &rate, &serial)
            || !(AUDIO_FLAGS_GET(audio_state_get_ptr()) & AUDIO_FLAG_ACTIVE))
      {
         valid = false;
         retro_atomic_store_release_int(&probe->verified, 1);
         continue;
      }
      presents = media_modal_present_count();
      if (!GetWindowRect(probe->hwnd, &live_rect))
      {
         valid = false;
         retro_atomic_store_release_int(&probe->verified, 1);
         continue;
      }
      if (!EqualRect(&live_rect, &sampled_rect))
      {
         sampled_rect = live_rect;
         stable_geometry = 0;
         sampled = changed = false;
      }
      if (stable_geometry++ < 5)
         continue;
      if (!sampled)
      {
         initial = current;
         first_frames = frames;
         first_serial = serial;
         first_present = presents;
         sampled = media_native_capture_region(&first_hash, -1, true);
      }
      else if (serial == first_serial && frames - first_frames >= 4096
            && current.position_us >= initial.position_us + 100000)
      {
         changed = media_native_capture_region(&last_hash, -1, true)
            && last_hash != first_hash;
         if (changed)
            retro_atomic_store_release_int(&probe->verified, 1);
      }
   }
   sthread_join(thread);
   UnhookWindowsHookEx(hook);
   GetWindowRect(probe->hwnd, &final_rect);
   geometry = mode && !strcmp(mode, "move")
      ? final_rect.left != rect.left || final_rect.top != rect.top
      : final_rect.right - final_rect.left != rect.right - rect.left
         || final_rect.bottom - final_rect.top != rect.bottom - rect.top;
   SetWindowPos(probe->hwnd, NULL, rect.left, rect.top,
         rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
   SetCursorPos(cursor.x, cursor.y);
   for (warmup = 0; warmup < 4; warmup++)
      if (!media_modal_normal_tick()) valid = false;
   if (!get_target(menu_st, &target) || media_modal_audio_voice() != voice
         || !audio_mixer_voice_clock(voice, NULL, NULL, &serial)
         || serial != first_serial) valid = false;
   printf("modal geometry: commands=%u changed=%u before=%ld,%ld,%ld,%ld after=%ld,%ld,%ld,%ld\n",
         probe->moves, geometry, rect.left, rect.top, rect.right, rect.bottom,
         final_rect.left, final_rect.top, final_rect.right, final_rect.bottom);
   printf("modal A/V: mode=%s driver=%s entered=%u exited=%u ticks=%u pcm=%lu rate=%u presents=%lu pixels=%08x/%08x valid=%u\n",
         mode ? mode : "resize", audio_driver_get_ident(), probe->entered,
         probe->exited, live, (unsigned long)(frames - first_frames), rate,
         (unsigned long)(presents - first_present), first_hash, last_hash, valid);
   return valid && geometry && probe->moves == 2 && probe->entered
      && probe->exited && live >= 2 && changed
      && serial == first_serial && frames - first_frames >= 4096;
}

static bool media_native_modal_image(struct menu_state *menu_st,
      bool (*visible)(struct menu_state*))
{
   media_modal_av_probe_t *probe = &media_modal_probe;
   HHOOK hook;
   sthread_t *thread;
   POINT cursor;
   RECT rect, final_rect, live_rect, client;
   uint32_t before = 0, during = 0, after = 0;
   uint64_t first_present = 0, presents = 0;
   unsigned live = 0, stable = 0;
   int64_t deadline;
   bool valid = true, sampled = false, geometry = false, restored = false;
   memset(probe, 0, sizeof(*probe));
   ui_companion_driver_init_first(false, false, false);
   deadline = cpu_features_get_time_usec() + 500000;
   do
      if (!media_modal_normal_tick() || !visible(menu_st))
         return false;
   while (cpu_features_get_time_usec() < deadline);
   if (!media_native_capture_region(&before, 200, true))
      return false;
   probe->hwnd = win32_get_window();
   probe->owner = GetWindowThreadProcessId(probe->hwnd, NULL);
   probe->command = SC_SIZE | WMSZ_BOTTOMRIGHT;
   retro_atomic_int_init(&probe->active, 0);
   retro_atomic_int_init(&probe->verified, 0);
   retro_atomic_int_init(&probe->done, 0);
   hook = SetWindowsHookEx(WH_CALLWNDPROC, media_modal_message, NULL, probe->owner);
   if (!hook)
      return false;
   GetCursorPos(&cursor);
   GetWindowRect(probe->hwnd, &rect);
   live_rect = rect;
   SetCursorPos(rect.right - 4, rect.bottom - 4);
   thread = sthread_create(media_modal_av_worker, probe);
   if (!thread)
   {
      SetCursorPos(cursor.x, cursor.y);
      UnhookWindowsHookEx(hook);
      return false;
   }
   deadline = cpu_features_get_time_usec() + 6000000;
   while (!retro_atomic_load_acquire_int(&probe->done)
         && cpu_features_get_time_usec() < deadline)
   {
      unsigned dims;
      if (!media_modal_normal_tick() || !visible(menu_st))
      {
         valid = false;
         retro_atomic_store_release_int(&probe->verified, 1);
         continue;
      }
      if (!retro_atomic_load_acquire_int(&probe->active))
         continue;
      live++;
      if (!GetWindowRect(probe->hwnd, &final_rect))
      {
         valid = false;
         retro_atomic_store_release_int(&probe->verified, 1);
         continue;
      }
      if (!EqualRect(&final_rect, &live_rect))
      {
         live_rect = final_rect;
         stable = 0;
         sampled = false;
      }
      dims = video_driver_get_output_dims();
      if (!GetClientRect(probe->hwnd, &client)
            || VIDEO_SCALE_W(dims) != (unsigned)client.right
            || VIDEO_SCALE_H(dims) != (unsigned)client.bottom
            || stable++ < 5 || EqualRect(&rect, &live_rect))
         continue;
      presents = media_modal_present_count();
      if (!sampled)
      {
         first_present = presents;
         sampled = media_native_capture_region(&during, 201, true);
      }
      else if (presents - first_present >= 8)
         retro_atomic_store_release_int(&probe->verified, 1);
   }
   sthread_join(thread);
   UnhookWindowsHookEx(hook);
   GetWindowRect(probe->hwnd, &final_rect);
   geometry = final_rect.right - final_rect.left != rect.right - rect.left
      || final_rect.bottom - final_rect.top != rect.bottom - rect.top;
   SetWindowPos(probe->hwnd, NULL, rect.left, rect.top,
         rect.right - rect.left, rect.bottom - rect.top,
         SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
   SetCursorPos(cursor.x, cursor.y);
   deadline = cpu_features_get_time_usec() + 2000000;
   stable = 0;
   do
   {
      unsigned dims;
      if (!media_modal_normal_tick() || !visible(menu_st))
         valid = false;
      GetWindowRect(probe->hwnd, &live_rect);
      GetClientRect(probe->hwnd, &client);
      dims = video_driver_get_output_dims();
      if (EqualRect(&live_rect, &rect)
            && VIDEO_SCALE_W(dims) == (unsigned)client.right
            && VIDEO_SCALE_H(dims) == (unsigned)client.bottom)
      {
         if (++stable >= 8)
         {
            restored = media_native_capture_region(&after, 202, true)
               && before == after;
            if (restored)
               break;
         }
      }
      else
         stable = 0;
   } while (cpu_features_get_time_usec() < deadline);
   printf("modal image: entered=%u exited=%u ticks=%u commands=%u geometry=%u presents=%lu pixels=%08x/%08x/%08x restored=%u visible=%u valid=%u\n",
         probe->entered, probe->exited, live, probe->moves, geometry,
         (unsigned long)(presents - first_present), before, during, after,
         restored, visible(menu_st), valid);
   return valid && geometry && restored && sampled && probe->moves == 2
      && probe->entered && probe->exited && presents - first_present >= 8;
}

#endif
#endif
