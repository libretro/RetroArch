/* librsound's playback thread against the frontend that starts and
 * stops it.
 *
 * The thread ends on its own when the audio callback fails or the
 * server hangs up, and the frontend stops it whenever it likes - an
 * rsd_stop() can land at any point of the thread's own exit. Every
 * ending must run the error callback once (for the thread's own), join
 * the thread once, and close the connection once, from the frontend.
 *
 * The server is the other end of a socketpair, drained by a thread of
 * its own. librsound is included for its statics. */

#include <stdio.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <retro_atomic.h>

#include "../../../audio/librsound.c"

static unsigned failures;
#define CHECK(cond, ...) \
   do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); \
        printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static void watchdog(int sig)
{
   (void)sig;
   printf("FAIL: hung\n");
   fflush(stdout);
   _exit(1);
}

/* ---- the frontend's audio callbacks ---- */

static retro_atomic_int_t cb_calls, fail_after, errors;

static ssize_t audio_cb(void *data, size_t bytes, void *userdata)
{
   int n = retro_atomic_fetch_add_int(&cb_calls, 1) + 1;
   int f = retro_atomic_load_acquire_int(&fail_after);
   (void)userdata;
   if (f && n >= f)
      return -1;
   memset(data, 0x11, bytes);
   return (ssize_t)bytes;
}

static void error_cb(void *userdata)
{
   (void)userdata;
   retro_atomic_fetch_add_int(&errors, 1);
}

/* ---- the server ---- */

static void drain(void *data)
{
   int  fd = *(int*)data;
   char buf[4096];
   while (read(fd, buf, sizeof(buf)) > 0) { }
}

enum ending { END_CALLBACK, END_HANGUP, END_STOP };

/* One stream: start, let it end the given way, stop. @race: stop
 * without waiting for the thread's own ending. */
static void stream(rsound_t *rd, enum ending how, bool race, unsigned salt)
{
   int        sv[2];
   int        server_fd;
   sthread_t *server;
   int        expect_errors;

   if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
   {
      printf("FAIL: socketpair\n");
      failures++;
      return;
   }
   server_fd          = sv[1];
   rd->conn.socket    = sv[0];
   rd->conn.ctl_socket = -1;
   server             = sthread_create(drain, &server_fd);

   retro_atomic_store_release_int(&cb_calls, 0);
   retro_atomic_store_release_int(&errors, 0);
   retro_atomic_store_release_int(&fail_after,
         how == END_CALLBACK ? (int)(1 + salt % 7) : 0);

   CHECK(rsnd_start_thread(rd) == 0, "the playback thread did not start");

   if (how == END_HANGUP)
   {
      /* the server goes away under the stream */
      if (salt & 1)
         usleep(200 + (salt % 5) * 100);
      shutdown(server_fd, SHUT_RDWR);
   }

   if (!race)
   {
      int i;
      if (how != END_STOP)
         for (i = 0; i < 5000 && !retro_atomic_load_acquire_int(&errors); i++)
            usleep(200);
      else
         usleep(300);
   }
   else if (salt & 2)
      usleep(salt % 300);

   rsd_stop(rd);

   expect_errors = how == END_STOP ? 0 : 1;
   if (race && how != END_STOP)
      /* stopped before the thread got to its own ending, or after */
      CHECK(retro_atomic_load_acquire_int(&errors) <= 1,
            "%d error callbacks for one ending",
            retro_atomic_load_acquire_int(&errors));
   else
      CHECK(retro_atomic_load_acquire_int(&errors) == expect_errors,
            "%d error callbacks, expected %d (ending %d)",
            retro_atomic_load_acquire_int(&errors), expect_errors, how);
   CHECK(rd->conn.socket == -1, "the connection was left open");
   CHECK(!rd->thread.thread, "the thread was left unjoined");

   shutdown(server_fd, SHUT_RDWR);
   sthread_join(server);
   close(server_fd);
}

#define ROUNDS 300

int main(void)
{
   rsound_t *rd;
   unsigned  i;

   signal(SIGALRM, watchdog);
   signal(SIGPIPE, SIG_IGN);
   alarm(120);

   if (rsd_init(&rd) < 0)
   {
      printf("FAIL: rsd_init\n");
      return 1;
   }
   rd->rate                   = 48000;
   rd->channels               = 2;
   rd->samplesize             = 2;
   rd->backend_info.chunk_size = 256;
   rd->max_latency            = 0;
   rd->conn_type              = 0;
   rsd_set_callback(rd, audio_cb, error_cb, 64, NULL);

   for (i = 0; i < ROUNDS; i++)
   {
      stream(rd, END_CALLBACK, false, i);
      stream(rd, END_HANGUP,   false, i);
      stream(rd, END_STOP,     false, i);
      stream(rd, END_CALLBACK, true,  i);
      stream(rd, END_HANGUP,   true,  i);
   }

   rsd_free(rd);
   alarm(0);

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] rsound_thread_test: %u streams ended every way\n",
         ROUNDS * 5);
   return 0;
}
