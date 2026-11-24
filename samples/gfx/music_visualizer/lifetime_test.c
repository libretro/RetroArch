#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "../../../audio/audio_driver.h"
#include "../../../tasks/task_audio_mixer.h"
#include "../../../gfx/gfx_music_player.h"
#include "../../../gfx/gfx_music_visualizer.h"

static task_audio_mixer_viewer_t pending;
static pthread_barrier_t admission;
static pthread_t main_thread;
static retro_atomic_int_t removed;
static unsigned freed;
static bool late;


bool audio_driver_mixer_prepare_format(unsigned *rate, bool *s16)
{
   *rate = 48000;
   *s16 = false;
   return true;
}
bool task_push_audio_mixer_viewer(const char *path, const task_audio_mixer_viewer_t *viewer)
{
   (void)path;
   pending = *viewer;
   pending.observer.retain(pending.observer.user);
   return true;
}
void audio_driver_mixer_remove_stream_clocked(unsigned slot, audio_mixer_voice_t *voice, size_t serial)
{
   assert(slot == 3 && voice == (audio_mixer_voice_t*)(uintptr_t)1 && serial == 17);
   retro_atomic_fetch_add_int(&removed, 1);
}
void __real_gfx_music_visualizer_free(gfx_music_visualizer_t*);
void __wrap_gfx_music_visualizer_free(gfx_music_visualizer_t *v)
{
   assert(pthread_equal(pthread_self(), main_thread));
   freed++;
   __real_gfx_music_visualizer_free(v);
}

static void *load(void *unused)
{
   (void)unused;
   if (late)
   {
      /* Admission passed before Close, binding finishes after it. */
      assert(pending.active(pending.observer.user));
      pending.observer.retain(pending.observer.user);
      pthread_barrier_wait(&admission);
      pthread_barrier_wait(&admission);
      pending.started(pending.observer.user, 3, (audio_mixer_voice_t*)(uintptr_t)1, 17);
      pending.observer.release(pending.observer.user);
   }
   else
      assert(!pending.active(pending.observer.user));
   pending.observer.release(pending.observer.user);
   return NULL;
}

int main(void)
{
   unsigned i;
   main_thread = pthread_self();
   assert(!pthread_barrier_init(&admission, NULL, 2));
   for (i = 0; i < 128; i++)
   {
      gfx_music_player_t *player;
      pthread_t worker;
      late = (i & 1) != 0;
      retro_atomic_int_init(&removed, 0);
      player = gfx_music_player_new("pending.wav");
      assert(player);
      if (!late) gfx_music_player_close(player);
      assert(!pthread_create(&worker, NULL, load, NULL));
      if (late)
      {
         pthread_barrier_wait(&admission);
         gfx_music_player_close(player);
         pthread_barrier_wait(&admission);
      }
      assert(!pthread_join(worker, NULL));
      gfx_music_player_reap();
      assert(freed == i + 1);
      assert(retro_atomic_load_acquire_int(&removed) == (late ? 1 : 0));
   }
   assert(!pthread_barrier_destroy(&admission));
   puts("PASS: canceled/late mixer binding retires borrowed visualizer ownership only on the main thread");
   return 0;
}
