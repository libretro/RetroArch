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
   MSG messages[32], lifecycle[16], last_mouse;
   RECT first_sizing, last_sizing;
   HWND hwnd, exit_foreground, exit_capture;
   WPARAM command;
   DWORD owner, exit_pid;
   unsigned moves, capture_serial;
   unsigned sizing, size, canceled, capture_lost, message_count, lifecycle_count;
   unsigned mouse_moves;
   retro_atomic_int_t active, verified, done, capture, capture_ready, moved;
   bool entered, exited, continuous, uniform;
   char exit_class[64];
} media_modal_av_probe_t;

static media_modal_av_probe_t media_modal_probe;

static LRESULT CALLBACK media_modal_message(int code, WPARAM wp, LPARAM lp)
{
   if (code >= 0)
   {
      const CWPSTRUCT *message = (const CWPSTRUCT*)lp;
      if (message->hwnd == media_modal_probe.hwnd)
      {
         if ((retro_atomic_load_acquire_int(&media_modal_probe.active)
                  || message->message == WM_ENTERSIZEMOVE)
               && (message->message == WM_ACTIVATE
                  || message->message == WM_ACTIVATEAPP
                  || message->message == WM_SETFOCUS
                  || message->message == WM_KILLFOCUS
                  || message->message == WM_CANCELMODE
                  || message->message == WM_CAPTURECHANGED
                  || message->message == WM_ENTERSIZEMOVE
                  || message->message == WM_EXITSIZEMOVE
                  || message->message == WM_DESTROY))
         {
            MSG *event = &media_modal_probe.lifecycle[
               media_modal_probe.lifecycle_count++ % 16];
            event->message = message->message;
            event->wParam = message->wParam;
            event->lParam = message->lParam;
            event->time = GetTickCount();
         }
         if (message->message == WM_SIZING)
         {
            const RECT *rect = (const RECT*)message->lParam;
            if (!media_modal_probe.sizing)
               media_modal_probe.first_sizing = *rect;
            media_modal_probe.last_sizing = *rect;
            media_modal_probe.sizing++;
         }
         else if (message->message == WM_SIZE)
            media_modal_probe.size++;
         else if (message->message == WM_CANCELMODE)
            media_modal_probe.canceled++;
         else if (message->message == WM_CAPTURECHANGED)
            media_modal_probe.capture_lost++;
         if (message->message == WM_ENTERSIZEMOVE)
            retro_atomic_store_release_int(&media_modal_probe.active, 1);
         else if (message->message == WM_EXITSIZEMOVE)
         {
            media_modal_probe.exit_foreground = GetForegroundWindow();
            GetWindowThreadProcessId(media_modal_probe.exit_foreground,
                  &media_modal_probe.exit_pid);
            GetClassNameA(media_modal_probe.exit_foreground,
                  media_modal_probe.exit_class, sizeof(media_modal_probe.exit_class));
            media_modal_probe.exit_capture = GetCapture();
            retro_atomic_store_release_int(&media_modal_probe.active, 0);
         }
      }
   }
   return CallNextHookEx(NULL, code, wp, lp);
}

static LRESULT CALLBACK media_modal_queue_message(int code, WPARAM wp, LPARAM lp)
{
   if (code >= 0 && wp == PM_REMOVE)
   {
      const MSG *message = (const MSG*)lp;
      if (retro_atomic_load_acquire_int(&media_modal_probe.active))
      {
         if (message->message == WM_MOUSEMOVE
               || message->message == WM_NCMOUSEMOVE)
         {
            media_modal_probe.mouse_moves++;
            media_modal_probe.last_mouse = *message;
         }
         else if ((message->message >= WM_KEYFIRST
                  && message->message <= WM_KEYLAST)
               || (message->message >= WM_MOUSEFIRST
                  && message->message <= WM_MOUSELAST)
               || (message->message >= WM_NCLBUTTONDOWN
                  && message->message <= WM_NCMBUTTONDBLCLK))
            media_modal_probe.messages[media_modal_probe.message_count++ % 32] = *message;
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
   RECT clip;
   int direction_x = 1, direction_y = 1, capture;
   origin.x = origin.y = 0;
   PostMessage(probe->hwnd, WM_SYSCOMMAND, probe->command, 0);
   while (cpu_features_get_time_usec() < deadline)
   {
      if (retro_atomic_load_acquire_int(&probe->active))
      {
         if (!probe->entered)
         {
            GetCursorPos(&origin);
            if (GetClipCursor(&clip))
            {
               if (origin.x + 64 >= clip.right)
                  direction_x = -1;
               if (origin.y + 48 >= clip.bottom)
                  direction_y = -1;
            }
            if (getenv("MEDIA_TEST_MODAL_SHRINK") && probe->command != SC_MOVE)
               direction_x = direction_y = -1;
            entered_at = cpu_features_get_time_usec();
            probe->entered = true;
         }
         capture = retro_atomic_load_acquire_int(&probe->capture);
         retro_atomic_store_release_int(&probe->capture_ready, capture);
         if (!stopping && !capture
               && (probe->continuous || probe->moves < 2)
               && cpu_features_get_time_usec() >= entered_at
                  + (int64_t)(probe->moves + 1) * (probe->continuous ? 4000 : 25000))
         {
            probe->moves++;
            if (probe->command == SC_MOVE)
            {
               PostMessage(probe->hwnd, WM_KEYDOWN, VK_RIGHT, 0);
               PostMessage(probe->hwnd, WM_KEYDOWN, VK_DOWN, 0);
            }
            else if (!probe->continuous && getenv("MEDIA_TEST_MODAL_KEYS"))
            {
               PostMessage(probe->hwnd, WM_KEYDOWN,
                     direction_x < 0 ? VK_LEFT : VK_RIGHT, 0);
               PostMessage(probe->hwnd, WM_KEYDOWN,
                     direction_y < 0 ? VK_UP : VK_DOWN, 0);
            }
            else
            {
               unsigned step = probe->continuous ? probe->moves % 8 : probe->moves;
               if (probe->continuous && step > 4)
                  step = 8 - step;
               SetCursorPos(origin.x + direction_x * (int)step * (probe->continuous ? 16 : 32),
                     origin.y + direction_y * (int)step * (probe->continuous ? 12 : 24));
            }
            retro_atomic_store_release_int(&probe->moved, (int)probe->moves);
         }
         if (!stopping && (!probe->continuous || probe->moves >= 120)
               && retro_atomic_load_acquire_int(&probe->verified))
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
   return video_state_get_ptr()->swap_count;
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

static bool media_modal_pixels_changed(uint32_t first, uint32_t last,
      bool uniform)
{
   unsigned shift;
   if (!uniform)
      return first != last;
   for (shift = 0; shift < 24; shift += 8)
   {
      int delta = (int)((last >> shift) & 255) - (int)((first >> shift) & 255);
      if (delta > 16 || delta < -16)
         return true;
   }
   return false;
}

/* A capture must keep the viewport size stable through its readback. */
static bool media_modal_capture(media_modal_av_probe_t *probe, uint32_t *hash,
      RECT *sample_rect, bool first)
{
   RECT previous, current;
   unsigned stable = 0;
   int ticket = (int)++probe->capture_serial;
   bool ok = false;
   int64_t deadline = cpu_features_get_time_usec() + 1000000;
   memset(&previous, 0, sizeof(previous));
   retro_atomic_store_release_int(&probe->capture, ticket);
   while (retro_atomic_load_acquire_int(&probe->active)
         && cpu_features_get_time_usec() < deadline)
   {
      if (!media_modal_normal_tick())
         break;
      if (retro_atomic_load_acquire_int(&probe->capture_ready) != ticket
            || !GetWindowRect(probe->hwnd, &current))
         continue;
      if (EqualRect(&previous, &current))
      {
         if (++stable >= 5)
         {
            if (first || probe->uniform || EqualRect(sample_rect, &current))
            {
               ok = media_native_capture_region(hash, -1, true);
               if (ok)
                  *sample_rect = current;
            }
            break;
         }
      }
      else
      {
         previous = current;
         stable = 0;
      }
   }
   retro_atomic_store_release_int(&probe->capture, 0);
   return ok;
}

typedef struct media_modal_placement
{
   HWND hwnd;
   RECT rect;
   UINT message;
   BOOL result;
   retro_atomic_int_t ready, done, busy;
} media_modal_placement_t;

static media_modal_placement_t media_placement;

static LRESULT CALLBACK media_modal_place_message(int code, WPARAM wp, LPARAM lp)
{
   if (code >= 0 && wp == PM_REMOVE)
   {
      const MSG *message = (const MSG*)lp;
      if (message->hwnd == media_placement.hwnd
            && message->message == media_placement.message
            && retro_atomic_load_acquire_int(&media_placement.ready))
      {
         media_placement.result = SetWindowPos(message->hwnd, NULL,
               media_placement.rect.left, media_placement.rect.top,
               media_placement.rect.right - media_placement.rect.left,
               media_placement.rect.bottom - media_placement.rect.top,
               SWP_NOZORDER | SWP_NOACTIVATE);
         retro_atomic_store_release_int(&media_placement.done, 1);
      }
   }
   return CallNextHookEx(NULL, code, wp, lp);
}

static bool media_modal_place(HWND hwnd, const RECT *rect)
{
   HHOOK hook;
   DWORD owner = GetWindowThreadProcessId(hwnd, NULL);
   int64_t deadline;
   bool applied;
   if (owner == GetCurrentThreadId())
      return SetWindowPos(hwnd, NULL, rect->left, rect->top,
            rect->right - rect->left, rect->bottom - rect->top,
            SWP_NOZORDER | SWP_NOACTIVATE) != 0;
   if (!retro_atomic_cas_int(&media_placement.busy, 0, 1))
      return false;
   media_placement.hwnd = hwnd;
   media_placement.rect = *rect;
   media_placement.result = FALSE;
   media_placement.message = RegisterWindowMessageA("RetroArch media placement test");
   retro_atomic_store_relaxed_int(&media_placement.done, 0);
   hook = SetWindowsHookEx(WH_GETMESSAGE, media_modal_place_message, NULL, owner);
   if (!hook || !media_placement.message)
   {
      if (hook) UnhookWindowsHookEx(hook);
      retro_atomic_store_release_int(&media_placement.busy, 0);
      return false;
   }
   retro_atomic_store_release_int(&media_placement.ready, 1);
   PostMessage(hwnd, media_placement.message, 0, 0);
   deadline = cpu_features_get_time_usec() + 2000000;
   while (!retro_atomic_load_acquire_int(&media_placement.done)
         && cpu_features_get_time_usec() < deadline)
      if (!media_modal_normal_tick()) break;
   applied = retro_atomic_load_acquire_int(&media_placement.done) != 0;
   retro_atomic_store_release_int(&media_placement.ready, 0);
   UnhookWindowsHookEx(hook);
   if (!applied) return false;
   retro_atomic_store_release_int(&media_placement.busy, 0);
   return media_placement.result != FALSE;
}

static bool media_modal_fit_window(void)
{
   RECT window, clip;
   HWND hwnd = win32_get_window();
   if (!GetWindowRect(hwnd, &window) || !GetClipCursor(&clip)
         || clip.right - clip.left <= 64 || clip.bottom - clip.top <= 64)
      return false;
   window.right = clip.left + 16
      + MIN(window.right - window.left, clip.right - clip.left - 32);
   window.bottom = clip.top + 16
      + MIN(window.bottom - window.top, clip.bottom - clip.top - 32);
   window.left = clip.left + 16;
   window.top = clip.top + 16;
   return media_modal_place(hwnd, &window);
}

static bool media_native_modal_audio_ready(void)
{
   const char *cold_audio = getenv("MEDIA_TEST_MODAL_AUDIO_COLD");
   settings_t *settings = config_get_ptr();
   if (!getenv("MEDIA_TEST_MODAL_MODE") || !strcmp(settings->arrays.audio_driver, "null"))
      return true;
   if (strcmp(settings->arrays.audio_driver, "wasapi"))
      return (AUDIO_FLAGS_GET(audio_state_get_ptr()) & AUDIO_FLAG_ACTIVE)
         && !strcmp(settings->arrays.audio_driver, audio_driver_get_ident());
   if (cold_audio)
   {
      settings->bools.audio_enable = false;
      command_event(CMD_EVENT_AUDIO_REINIT, NULL);
      settings->bools.audio_enable = strcmp(cold_audio, "unavailable") != 0;
   }
   if (!(AUDIO_FLAGS_GET(audio_state_get_ptr()) & AUDIO_FLAG_ACTIVE))
   {
      puts("modal startup: reinitializing inactive WASAPI endpoint once");
      command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   }
   if (!(AUDIO_FLAGS_GET(audio_state_get_ptr()) & AUDIO_FLAG_ACTIVE)
         || strcmp(audio_driver_get_ident(), "wasapi"))
   {
      fputs("FAIL: WASAPI modal fixture could not initialize its endpoint\n", stderr);
      return false;
   }
   return true;
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
   size_t first_underruns = 0, exit_underruns = 0, restored_underruns = 0, last_underruns = 0;
   unsigned rate = 0, warmup, live = 0, stable_geometry = 0;
   int64_t longest_tick = 0, longest_active_tick = 0;
   uint32_t first_hash = 0, last_hash = 0;
   uint64_t first_present = 0, presents = 0;
   int64_t deadline;
   bool sampled = false, changed = false, geometry = false, valid = true;
   bool audio_only = getenv("MEDIA_TEST_MODAL_AUDIO_ONLY") != NULL;
   const char *mode = getenv("MEDIA_TEST_MODAL_MODE");
   memset(probe, 0, sizeof(*probe));
   memset(&initial, 0, sizeof(initial));
   probe->continuous = getenv("MEDIA_TEST_MODAL_CONTINUOUS") != NULL;
   probe->uniform = getenv("MEDIA_TEST_MODAL_UNIFORM") != NULL;
   ui_companion_driver_init_first(false, false, false);
   if (!media_modal_fit_window())
      return false;
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
   first_underruns = audio_driver_get_underruns();
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
   retro_atomic_int_init(&probe->capture, 0);
   retro_atomic_int_init(&probe->capture_ready, 0);
   retro_atomic_int_init(&probe->moved, 0);
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
      bool was_active = retro_atomic_load_acquire_int(&probe->active) != 0;
      int64_t before_tick = cpu_features_get_time_usec();
      int64_t elapsed;
      if (!media_modal_normal_tick())
      {
         valid = false;
         break;
      }
      elapsed = cpu_features_get_time_usec() - before_tick;
      if (elapsed > longest_tick)
         longest_tick = elapsed;
      if (was_active && retro_atomic_load_acquire_int(&probe->active)
            && elapsed > longest_active_tick)
         longest_active_tick = elapsed;
      if (!retro_atomic_load_acquire_int(&probe->active))
         continue;
      if (!win32_message_modal_active())
         valid = false;
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
      if (probe->continuous && !sampled
            && retro_atomic_load_acquire_int(&probe->moved) < 16)
         continue;
      if (!probe->continuous && !EqualRect(&live_rect, &sampled_rect))
      {
         sampled_rect = live_rect;
         stable_geometry = 0;
         sampled = changed = false;
      }
      if (!probe->continuous && stable_geometry++ < 5)
         continue;
      if (probe->continuous && !probe->uniform && sampled
            && !EqualRect(&live_rect, &sampled_rect))
         continue;
      if (!sampled)
      {
         sampled_rect = live_rect;
         initial = current;
         first_frames = frames;
         first_serial = serial;
         first_present = presents;
         sampled = audio_only || media_modal_capture(probe, &first_hash, &sampled_rect, true);
      }
      else if (!changed && serial == first_serial && frames - first_frames >= 4096
            && current.position_us >= initial.position_us + 100000)
      {
         changed = audio_only || (media_modal_capture(probe, &last_hash, &sampled_rect, false)
            && media_modal_pixels_changed(first_hash, last_hash, probe->uniform));
         if (changed)
            retro_atomic_store_release_int(&probe->verified, 1);
      }
   }
   sthread_join(thread);
   exit_underruns = audio_driver_get_underruns();
   if (win32_message_modal_active())
      valid = false;
   UnhookWindowsHookEx(hook);
   GetWindowRect(probe->hwnd, &final_rect);
   geometry = mode && !strcmp(mode, "move")
      ? final_rect.left != rect.left || final_rect.top != rect.top
      : final_rect.right - final_rect.left != rect.right - rect.left
         || final_rect.bottom - final_rect.top != rect.bottom - rect.top;
   if (!media_modal_place(probe->hwnd, &rect))
      valid = false;
   restored_underruns = audio_driver_get_underruns();
   SetCursorPos(cursor.x, cursor.y);
   for (warmup = 0; warmup < 4; warmup++)
      if (!media_modal_normal_tick()) valid = false;
   if (!get_target(menu_st, &target) || media_modal_audio_voice() != voice
         || !audio_mixer_voice_clock(voice, NULL, NULL, &serial)
         || serial != first_serial) valid = false;
   printf("modal snapshot: value=%x menu=%x runloop=%x initialized=%u\n",
         retro_atomic_load_acquire_int(&audio_state_get_ptr()->runloop_snapshot),
         menu_st->flags, runloop_get_flags(), !!runloop_is_inited());
   printf("modal source: device=%u\n", !!(retro_atomic_load_acquire_int(
            &audio_state_get_ptr()->runloop_snapshot) & AUDIO_SNAP_MENU_SOURCE));
   if (getenv("MEDIA_TEST_MODAL_DEVICE_SOURCE") && !(retro_atomic_load_acquire_int(
            &audio_state_get_ptr()->runloop_snapshot) & AUDIO_SNAP_MENU_SOURCE)) valid = false;
   last_underruns = audio_driver_get_underruns();
   if (last_underruns != first_underruns) valid = false;
   printf("modal geometry: commands=%u changed=%u before=%ld,%ld,%ld,%ld after=%ld,%ld,%ld,%ld\n",
         probe->moves, geometry, rect.left, rect.top, rect.right, rect.bottom,
         final_rect.left, final_rect.top, final_rect.right, final_rect.bottom);
   printf("modal A/V: mode=%s driver=%s entered=%u exited=%u ticks=%u pcm=%lu rate=%u presents=%lu pixels=%08x/%08x valid=%u\n",
         mode ? mode : "resize", audio_driver_get_ident(), probe->entered,
         probe->exited, live, (unsigned long)(frames - first_frames), rate,
         (unsigned long)(presents - first_present), first_hash, last_hash, valid);
   printf("modal audio: capture_disabled=%u underruns=%lu/%lu/%lu/%lu\n",
         audio_only, (unsigned long)first_underruns, (unsigned long)exit_underruns,
         (unsigned long)restored_underruns, (unsigned long)last_underruns);
   printf("modal continuous: enabled=%u moves=%u longest_tick_us=%ld active_tick_us=%ld\n",
         probe->continuous, probe->moves, (long)longest_tick, (long)longest_active_tick);
   return valid && (probe->continuous || geometry)
      && (probe->continuous ? probe->moves >= 120 : probe->moves == 2) && probe->entered
      && probe->exited && live >= 2 && changed
      && serial == first_serial && frames - first_frames >= 4096;
}

static bool media_native_modal_image_once(struct menu_state *menu_st,
      bool (*visible)(struct menu_state*))
{
   media_modal_av_probe_t *probe = &media_modal_probe;
   HHOOK hook, queue_hook;
   sthread_t *thread;
   POINT cursor;
   RECT rect, final_rect, live_rect, client;
   uint32_t before = 0, during = 0, after = 0;
   uint64_t first_present = 0, presents = 0;
   unsigned live = 0, stable = 0;
   int64_t deadline;
   const char *mode = getenv("MEDIA_TEST_MODAL_MODE");
   bool valid = true, sampled = false, geometry = false, restored = false;
   memset(probe, 0, sizeof(*probe));
   ui_companion_driver_init_first(false, false, false);
   if (!media_modal_fit_window())
      return false;
   deadline = cpu_features_get_time_usec() + 500000;
   do
      if (!media_modal_normal_tick() || !visible(menu_st))
         return false;
   while (cpu_features_get_time_usec() < deadline);
   if (!media_native_capture_region(&before, 200, true))
      return false;
   probe->hwnd = win32_get_window();
   probe->owner = GetWindowThreadProcessId(probe->hwnd, NULL);
   probe->command = mode && !strcmp(mode, "move")
      ? SC_MOVE : (SC_SIZE | WMSZ_BOTTOMRIGHT);
   retro_atomic_int_init(&probe->active, 0);
   retro_atomic_int_init(&probe->verified, 0);
   retro_atomic_int_init(&probe->done, 0);
   retro_atomic_int_init(&probe->capture, 0);
   retro_atomic_int_init(&probe->capture_ready, 0);
   retro_atomic_int_init(&probe->moved, 0);
   hook = SetWindowsHookEx(WH_CALLWNDPROC, media_modal_message, NULL, probe->owner);
   if (!hook)
      return false;
   queue_hook = SetWindowsHookEx(WH_GETMESSAGE, media_modal_queue_message,
         NULL, probe->owner);
   if (!queue_hook)
   {
      UnhookWindowsHookEx(hook);
      return false;
   }
   GetCursorPos(&cursor);
   GetWindowRect(probe->hwnd, &rect);
   live_rect = rect;
   if (probe->command == SC_MOVE)
      SetCursorPos(rect.left + 100, rect.top + 20);
   else
      SetCursorPos(rect.right - 4, rect.bottom - 4);
   thread = sthread_create(media_modal_av_worker, probe);
   if (!thread)
   {
      SetCursorPos(cursor.x, cursor.y);
      UnhookWindowsHookEx(hook);
      UnhookWindowsHookEx(queue_hook);
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
      if (!win32_message_modal_active())
         valid = false;
      live++;
      if (retro_atomic_load_acquire_int(&probe->verified))
         continue;
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
   if (win32_message_modal_active())
      valid = false;
   UnhookWindowsHookEx(hook);
   UnhookWindowsHookEx(queue_hook);
   GetWindowRect(probe->hwnd, &final_rect);
   geometry = probe->command == SC_MOVE
      ? final_rect.left != rect.left || final_rect.top != rect.top
      : final_rect.right - final_rect.left != rect.right - rect.left
         || final_rect.bottom - final_rect.top != rect.bottom - rect.top;
   if (!media_modal_place(probe->hwnd, &rect))
      valid = false;
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
   printf("modal image messages: sizing=%u size=%u cancel=%u capture_lost=%u\n",
         probe->sizing, probe->size, probe->canceled, probe->capture_lost);
   printf("modal sizing rectangles: first=%ld,%ld,%ld,%ld last=%ld,%ld,%ld,%ld\n",
         probe->first_sizing.left, probe->first_sizing.top,
         probe->first_sizing.right, probe->first_sizing.bottom,
         probe->last_sizing.left, probe->last_sizing.top,
         probe->last_sizing.right, probe->last_sizing.bottom);
   printf("modal image geometry: mode=%s before=%ld,%ld,%ld,%ld after=%ld,%ld,%ld,%ld\n",
         mode ? mode : "resize", rect.left, rect.top, rect.right, rect.bottom,
         final_rect.left, final_rect.top, final_rect.right, final_rect.bottom);
   printf("modal image: entered=%u exited=%u ticks=%u commands=%u geometry=%u presents=%lu pixels=%08x/%08x/%08x restored=%u visible=%u valid=%u\n",
         probe->entered, probe->exited, live, probe->moves, geometry,
         (unsigned long)(presents - first_present), before, during, after,
         restored, visible(menu_st), valid);
   if (!geometry || !sampled)
   {
      unsigned i = probe->lifecycle_count > 16 ? probe->lifecycle_count - 16 : 0;
      printf("modal exit: hwnd=%p foreground=%p capture=%p pid=%lu class=%s\n",
            (void*)probe->hwnd, (void*)probe->exit_foreground,
            (void*)probe->exit_capture, (unsigned long)probe->exit_pid,
            probe->exit_class);
      for (; i < probe->lifecycle_count; i++)
      {
         const MSG *event = &probe->lifecycle[i % 16];
         printf("modal lifecycle: message=%x wparam=%lu lparam=%p time=%lu\n",
               event->message, (unsigned long)event->wParam,
               (void*)event->lParam, (unsigned long)event->time);
      }
      printf("modal mouse: count=%u hwnd=%p message=%x position=%ld time=%lu\n",
            probe->mouse_moves, (void*)probe->last_mouse.hwnd,
            probe->last_mouse.message, (long)probe->last_mouse.lParam,
            (unsigned long)probe->last_mouse.time);
      i = probe->message_count > 32 ? probe->message_count - 32 : 0;
      for (; i < probe->message_count; i++)
      {
         const MSG *message = &probe->messages[i % 32];
         printf("modal input: hwnd=%p message=%x key=%lu position=%ld time=%lu\n",
               (void*)message->hwnd, message->message, (unsigned long)message->wParam,
               (long)message->lParam, (unsigned long)message->time);
      }
   }
   return valid && geometry && restored && sampled && probe->moves == 2
      && probe->entered && probe->exited && presents - first_present >= 8;
}

static bool media_native_modal_image(struct menu_state *menu_st,
      bool (*visible)(struct menu_state*))
{
   const char *value = getenv("MEDIA_TEST_MODAL_REPEATS");
   unsigned long count = value ? strtoul(value, NULL, 10) : 1;
   unsigned long i;
   if (!count || count > 16)
      return false;
   for (i = 0; i < count; i++)
   {
      if (!media_native_modal_image_once(menu_st, visible))
         return false;
      printf("modal image repeat: %lu/%lu\n", i + 1, count);
   }
   return true;
}

#endif
#endif
