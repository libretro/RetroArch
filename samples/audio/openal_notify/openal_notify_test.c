/* The OpenAL driver against a scripted library that offers Apple's
 * source notification (AL_EXT_SOURCE_NOTIFICATIONS) and no
 * AL_SOFT_events: a blocking write short of buffers waits on the
 * notification OpenAL's own thread raises as a buffer is processed,
 * and never sleeps; the notification is removed before the source
 * goes, and one already on its way when it is removed - the library's
 * thread calls it outside its own lock - touches nothing the driver
 * freed (TSan and ASan are the check on that). Against a library with
 * neither, the driver still plays, on its one sleeping fallback. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/* The driver's sleeps - retro_sleep_us() is nanosleep() here - counted */
static unsigned sleeps; /* the driver's thread only */
static int test_nanosleep(const struct timespec *req, struct timespec *rem);
#define nanosleep test_nanosleep
#include "../../../audio/drivers/openal.c"
#undef nanosleep
static int test_nanosleep(const struct timespec *req, struct timespec *rem)
{
   sleeps++;
   return nanosleep(req, rem);
}

static unsigned failures;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* ---- the scripted library ---------------------------------------- */

#define MAX_BUFS 64
static pthread_mutex_t lib_lock = PTHREAD_MUTEX_INITIALIZER;
static ALuint queued[MAX_BUFS];
static int    queued_n, processed_n, playing, offer_notify;
static ALuint next_name = 1;
static al_source_notification_proc_t notify_proc;
static ALvoid *notify_user;
static ALuint notify_sid;
static unsigned notifies, removes, notifies_after_remove;
static int source_deleted;
static retro_atomic_int_t mixer_run;
static pthread_t mixer;

/* OpenAL's thread: plays the oldest queued buffer every 2 ms and,
 * where it was asked to, says so */
static void *mixer_thread(void *arg)
{
   (void)arg;
   while (retro_atomic_load_acquire_int(&mixer_run))
   {
      usleep(2000);
      pthread_mutex_lock(&lib_lock);
      if (playing && processed_n < queued_n)
      {
         processed_n++;
         if (notify_proc)
         {
            al_source_notification_proc_t proc = notify_proc;
            ALvoid *user = notify_user;
            ALuint sid   = notify_sid;
            notifies++;
            if (removes)
               notifies_after_remove++;
            pthread_mutex_unlock(&lib_lock);
            proc(sid, AL_BUFFERS_PROCESSED, user);
            continue;
         }
      }
      pthread_mutex_unlock(&lib_lock);
   }
   return NULL;
}

static ALenum fake_add(ALuint sid, ALuint id, al_source_notification_proc_t proc, ALvoid *user)
{
   pthread_mutex_lock(&lib_lock);
   if (id == AL_BUFFERS_PROCESSED)
   {
      notify_proc = proc;
      notify_user = user;
      notify_sid  = sid;
   }
   pthread_mutex_unlock(&lib_lock);
   return AL_NO_ERROR;
}
static ALvoid fake_remove(ALuint sid, ALuint id, al_source_notification_proc_t proc, ALvoid *user)
{
   (void)sid; (void)id; (void)user;
   pthread_mutex_lock(&lib_lock);
   if (proc == notify_proc)
      notify_proc = NULL;
   removes++;
   CHECK(!source_deleted, "the notification was removed after the source went");
   pthread_mutex_unlock(&lib_lock);
}

void alBufferData(ALuint b, ALenum f, const ALvoid *d, ALsizei s, ALsizei r) { (void)b; (void)f; (void)d; (void)s; (void)r; }
void alDeleteBuffers(ALsizei n, const ALuint *b) { (void)n; (void)b; }
void alDeleteSources(ALsizei n, const ALuint *s) { (void)n; (void)s; source_deleted = 1; }
void alGenBuffers(ALsizei n, ALuint *b) { ALsizei i; for (i = 0; i < n; i++) b[i] = next_name++; }
void alGenSources(ALsizei n, ALuint *s) { ALsizei i; for (i = 0; i < n; i++) s[i] = next_name++; }
ALenum alGetEnumValue(const ALchar *name) { (void)name; return 0; }
ALenum alGetError(void) { return AL_NO_ERROR; }
void *alGetProcAddress(const ALchar *name)
{
   if (offer_notify && !strcmp(name, "alSourceAddNotification"))
      return (void*)(uintptr_t)fake_add;
   if (offer_notify && !strcmp(name, "alSourceRemoveNotification"))
      return (void*)(uintptr_t)fake_remove;
   return NULL;
}
void alGetSourcei(ALuint src, ALenum p, ALint *v)
{
   (void)src;
   pthread_mutex_lock(&lib_lock);
   if (p == AL_BUFFERS_PROCESSED)
      *v = processed_n;
   else if (p == AL_SOURCE_STATE)
      *v = playing ? AL_PLAYING : AL_STOPPED;
   else
      *v = 0;
   pthread_mutex_unlock(&lib_lock);
}
ALboolean alIsExtensionPresent(const ALchar *name) { (void)name; return AL_FALSE; }
void alSourcePlay(ALuint s) { (void)s; pthread_mutex_lock(&lib_lock); playing = 1; pthread_mutex_unlock(&lib_lock); }
void alSourceQueueBuffers(ALuint src, ALsizei n, const ALuint *b)
{
   ALsizei i;
   (void)src;
   pthread_mutex_lock(&lib_lock);
   for (i = 0; i < n && queued_n < MAX_BUFS; i++)
      queued[queued_n++] = b[i];
   pthread_mutex_unlock(&lib_lock);
}
void alSourceStop(ALuint s) { (void)s; pthread_mutex_lock(&lib_lock); playing = 0; pthread_mutex_unlock(&lib_lock); }
void alSourceUnqueueBuffers(ALuint src, ALsizei n, ALuint *b)
{
   ALsizei i;
   (void)src;
   pthread_mutex_lock(&lib_lock);
   if (n > processed_n)
      n = processed_n;
   for (i = 0; i < n; i++)
      b[i] = queued[i];
   memmove(queued, queued + n, (size_t)(queued_n - n) * sizeof(ALuint));
   queued_n    -= n;
   processed_n -= n;
   pthread_mutex_unlock(&lib_lock);
}
void alSourcei(ALuint s, ALenum p, ALint v) { (void)s; (void)p; (void)v; }

ALCdevice *alcOpenDevice(const ALCchar *n) { (void)n; return (ALCdevice*)(uintptr_t)1; }
ALCboolean alcCloseDevice(ALCdevice *d) { (void)d; return 1; }
ALCcontext *alcCreateContext(ALCdevice *d, const ALCint *a) { (void)d; (void)a; return (ALCcontext*)(uintptr_t)1; }
ALCboolean alcMakeContextCurrent(ALCcontext *c) { (void)c; return 1; }
void alcDestroyContext(ALCcontext *c) { (void)c; }
ALCboolean alcIsExtensionPresent(ALCdevice *d, const ALCchar *n) { (void)d; (void)n; return 0; }
const ALCchar *alcGetString(ALCdevice *d, ALCenum p) { (void)d; (void)p; return "\0"; }
void *alcGetProcAddress(ALCdevice *d, const ALCchar *n) { (void)d; (void)n; return NULL; }

/* ---- the run ------------------------------------------------------ */

static void reset_lib(int notify)
{
   queued_n = processed_n = playing = 0;
   notify_proc = NULL;
   notifies = removes = notifies_after_remove = 0;
   source_deleted = 0;
   offer_notify   = notify;
   sleeps         = 0;
}

/* Writes three times what the source holds, blocking: it plays,
 * buffer by buffer, as the mixer finishes them */
static void run(int notify)
{
   static uint8_t pcm[1 << 16];
   unsigned rate = 0;
   size_t total, written = 0;
   void *h;

   reset_lib(notify);
   retro_atomic_int_init(&mixer_run, 1);
   pthread_create(&mixer, NULL, mixer_thread, NULL);

   h = audio_openal.init(NULL, 48000, 64, &rate);
   CHECK(h != NULL, "init failed");
   if (h)
   {
      al_t *al = (al_t*)h;
      CHECK(al->events == !!notify, "events %d with notification %d",
            al->events, notify);
      audio_openal.set_nonblock_state(h, false);
      total = audio_openal.buffer_size(h) * 3;
      while (written < total)
      {
         ssize_t w = audio_openal.write(h, pcm,
               MIN(total - written, sizeof(pcm)));
         if (w <= 0)
            break;
         written += (size_t)w;
      }
      CHECK(written == total, "a blocking write took %u of %u bytes",
            (unsigned)written, (unsigned)total);
      if (notify)
      {
         CHECK(sleeps == 0, "the driver slept %u times with notifications",
               sleeps);
         CHECK(notifies > 0, "OpenAL never notified");
      }
      else
         CHECK(sleeps > 0, "without notifications the driver must wait on its fallback");
      audio_openal.free(h);
      if (notify)
         CHECK(removes == 1, "the notification was removed %u times", removes);
   }

   retro_atomic_store_release_int(&mixer_run, 0);
   pthread_join(mixer, NULL);
   CHECK(notifies_after_remove == 0, "%u notification(s) after removal",
         notifies_after_remove);
}

int main(void)
{
   printf("   Apple's source notification, no AL_SOFT_events\n");
   run(1);
   printf("   neither: the sleeping fallback\n");
   run(0);
   if (failures)
   {
      printf("[fail] openal_notify_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] openal_notify_test\n");
   return 0;
}
