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
 * its own. librsound is included for its statics.
 *
 * Audio leaves through the driver's own callbacks: the worker borrows
 * the fifo's frames where they lie and sends them from there, so what
 * the server receives must be the writer's bytes exactly, across the
 * ring's wrap on a plain and on a mirrored ring, with any shortfall
 * filled by whole frames of silence.
 *
 * The control channel's replies carry a server-chosen body length; a
 * body longer than the protocol limit, or a negative length, is
 * refused before anything is received into the reply buffer. */

#include <stdio.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <retro_atomic.h>

#include "../../../audio/librsound.c"
#include "../../../audio/drivers/rsound.c"

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
static uint8_t            feed[MAX_CHUNK_SIZE];

/* A feed that never runs dry, lending one static block. */
static ssize_t borrow_cb(const void **first, size_t *first_bytes,
      const void **second, size_t bytes, void *userdata)
{
   int n = retro_atomic_fetch_add_int(&cb_calls, 1) + 1;
   int f = retro_atomic_load_acquire_int(&fail_after);
   (void)userdata;
   if (f && n >= f)
      return -1;
   if (bytes > sizeof(feed))
      bytes = sizeof(feed);
   *first       = feed;
   *first_bytes = bytes;
   *second      = NULL;
   return (ssize_t)bytes;
}

static void release_cb(size_t bytes, void *userdata)
{
   (void)bytes;
   (void)userdata;
}

static void wait_cb(int64_t timeout_us, void *userdata)
{
   (void)userdata;
   usleep((useconds_t)timeout_us);
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

/* ---- the driver's borrowed send path ---- */

#define STREAM_BYTES (128 * 320)

struct sink
{
   int     fd;
   uint8_t buf[STREAM_BYTES * 4];
   size_t  len;
};

static void sink_read(void *data)
{
   struct sink *k = (struct sink*)data;
   ssize_t      r;
   while (k->len < sizeof(k->buf)
         && (r = read(k->fd, k->buf + k->len, sizeof(k->buf) - k->len)) > 0)
      k->len += (size_t)r;
}

static retro_atomic_int_t split_loans;

/* The driver's own borrow, counting loans that span the wrap. */
static ssize_t counting_borrow_cb(const void **first, size_t *first_bytes,
      const void **second, size_t bytes, void *userdata)
{
   ssize_t got = rsound_borrow_cb(first, first_bytes, second, bytes,
         userdata);
   if (got > 0 && *second)
      retro_atomic_fetch_add_int(&split_loans, 1);
   return got;
}

static uint8_t pattern_byte(size_t i) { return (uint8_t)(1 + i % 251); }

/* @mirrored: the fifo's pages mapped twice, so no loan is split.
 * @starve: the writer leaves gaps, and the worker, running low, fills
 * them with silence. */
static void check_borrow_stream(bool mirrored, bool starve)
{
   static rsd_t       rsd;
   static struct sink k;
   static uint8_t     src[STREAM_BYTES];
   rsound_t          *r;
   sthread_t         *server;
   int                sv[2];
   size_t             i, off, n, written;
   unsigned           silences = 0;

   memset(&rsd, 0, sizeof(rsd));
   memset(&k, 0, sizeof(k));
   if (     !(mirrored ? retro_spsc_init_mirrored(&rsd.ring, 4096)
                       : retro_spsc_init(&rsd.ring, 4096))
         || !retro_eventcount_init(&rsd.park)
         || !retro_eventcount_init(&rsd.fill))
   {
      printf("FAIL: borrowed stream setup\n");
      failures++;
      return;
   }
   if (mirrored && !rsd.ring.mirror)
      printf("[note] no mirrored ring on this host; plain ring used\n");
   rsd.fifo_size = 4096;
   rsd.wait_us   = 1000;
   rsd.fill_init = true;
   retro_atomic_int_init(&rsd.has_error, 0);
   retro_atomic_store_release_int(&split_loans, 0);

   if (rsd_init(&r) < 0 || socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
   {
      printf("FAIL: borrowed stream setup\n");
      failures++;
      return;
   }
   r->rate                    = 48000;
   r->channels                = 2;
   r->samplesize              = 2;
   /* Not a divisor of the ring, so whole-chunk loans cross the wrap. */
   r->backend_info.chunk_size = 320;
   r->max_latency             = starve ? 1000 : 0;
   r->conn_type               = 0;
   r->conn.socket             = sv[0];
   r->conn.ctl_socket         = -1;
   rsd.rd                     = r;
   rsd_set_callback(r, counting_borrow_cb, rsound_release_cb,
         rsound_wait_cb, rsound_err_cb, &rsd);

   k.fd   = sv[1];
   server = sthread_create(sink_read, &k);

   for (i = 0; i < sizeof(src); i++)
      src[i] = pattern_byte(i);

   CHECK(rsnd_start_thread(r) == 0, "the playback thread did not start");

   /* Frame-multiple pieces through both write paths: blocking copies
    * and lent spans. */
   for (off = 0, n = 0; off < sizeof(src); n++)
   {
      size_t piece = 1236;
      if (piece > sizeof(src) - off)
         piece = sizeof(src) - off;
      if (n & 1)
      {
         void  *span;
         size_t got = audio_rsound.write_begin(&rsd, piece, &span);
         if (got)
            memcpy(span, src + off, got);
         written = (size_t)audio_rsound.write_end(&rsd, got);
         if (!written)
            usleep(200);
      }
      else
         written = (size_t)audio_rsound.write(&rsd, src + off, piece);
      off += written;
      if (starve && (n % 5) == 4)
         usleep(8000);
   }

   /* drained: every byte borrowed and sent */
   for (i = 0; i < 5000 && retro_spsc_read_avail(&rsd.ring); i++)
      usleep(200);
   usleep(starve ? 20000 : 5000);
   rsd_stop(r);
   sthread_join(server);
   close(sv[1]);

   /* the writer's bytes in order, silence only as whole frames */
   for (i = 0, off = 0; i < k.len; )
   {
      if (!k.buf[i])
      {
         size_t run = i;
         while (i < k.len && !k.buf[i])
            i++;
         CHECK(run % RS_FRAME_BYTES == 0 && (i - run) % RS_FRAME_BYTES == 0,
               "silence of %u bytes at %u splits a frame",
               (unsigned)(i - run), (unsigned)run);
         silences++;
         continue;
      }
      if (off >= sizeof(src) || k.buf[i] != src[off])
      {
         CHECK(0, "byte %u of the stream is %u, expected %u (writer byte %u)",
               (unsigned)i, k.buf[i],
               off < sizeof(src) ? src[off] : 0, (unsigned)off);
         break;
      }
      i++;
      off++;
   }
   CHECK(off == sizeof(src), "%u of %u written bytes reached the server",
         (unsigned)off, (unsigned)sizeof(src));
   if (starve)
      CHECK(silences && rs_underruns(&rsd) > 0,
            "a starved stream sent no silence (%u runs, %u counted)",
            silences, (unsigned)rs_underruns(&rsd));
   else
      CHECK(!silences && k.len == sizeof(src),
            "a fed stream carried %u silent runs, %u bytes for %u",
            silences, (unsigned)k.len, (unsigned)sizeof(src));
   if (!mirrored || !rsd.ring.mirror)
      CHECK(retro_atomic_load_acquire_int(&split_loans) > 0,
            "no loan crossed the plain ring's wrap");
   else
      CHECK(retro_atomic_load_acquire_int(&split_loans) == 0,
            "a mirrored ring lent a split span");

   rsd_free(r);
   retro_eventcount_free(&rsd.fill);
   retro_eventcount_free(&rsd.park);
   retro_spsc_free(&rsd.ring);
}

/* A span lent out of the ring: written where it lies, published by
 * write_end, partial frames not, nothing while the stream has failed. */
static void check_lend(void)
{
   static rsd_t rsd;
   int16_t      out[64];
   void        *span;
   size_t       got, i;

   memset(&rsd, 0, sizeof(rsd));
   if (!retro_spsc_init(&rsd.ring, 4096))
      return;
   rsd.fifo_size = 4096;
   got = audio_rsound.write_begin(&rsd, 64 * 2 + 3, &span);
   CHECK(span && got == 64 * 2, "the lend");
   for (i = 0; i < got / 2; i++)
      ((int16_t*)span)[i] = (int16_t)(i * 3);
   CHECK(audio_rsound.write_end(&rsd, got + 1) == (ssize_t)got, "the lend");
   CHECK(retro_spsc_read_avail(&rsd.ring) == got, "the lend");
   CHECK(retro_spsc_read(&rsd.ring, out, got) == got, "the lend");
   for (i = 0; i < got / 2; i++)
      CHECK(out[i] == (int16_t)(i * 3), "the lend");

   got = audio_rsound.write_begin(&rsd, 64, &span);
   CHECK(audio_rsound.write_end(&rsd, 0) == 0, "the lend");
   CHECK(retro_spsc_read_avail(&rsd.ring) == 0, "the lend");

   retro_atomic_store_release_int(&rsd.has_error, 1);
   CHECK(audio_rsound.write_begin(&rsd, 64, &span) == 0 && !span, "the lend");
   retro_spsc_free(&rsd.ring);
}

/* A non-blocking write reports what it put in the fifo, whole frames
 * only, so the pipeline neither sends the same bytes twice nor splits
 * a frame. */
static void check_nonblock_write(void)
{
   static rsd_t rsd;
   static char  src[6000];
   char         fill[4096];

   memset(&rsd, 0, sizeof(rsd));
   if (!retro_spsc_init(&rsd.ring, 4096))
      return;
   rsd.fifo_size = 4096;
   rsd.nonblock  = true;
   memset(src, 0x22, sizeof(src));

   CHECK(audio_rsound.write(&rsd, src, sizeof(src)) == 4096,
         "a non-blocking write into an empty fifo did not report 4096");
   CHECK(retro_spsc_read_avail(&rsd.ring) == 4096, "the fifo holds %u",
         (unsigned)retro_spsc_read_avail(&rsd.ring));
   CHECK(audio_rsound.write(&rsd, src, sizeof(src)) == 0,
         "a non-blocking write into a full fifo reported room");

   /* six bytes of room: one frame goes, the split one does not */
   retro_spsc_clear(&rsd.ring);
   memset(fill, 0, sizeof(fill));
   retro_spsc_write(&rsd.ring, fill, 4090);
   CHECK(audio_rsound.write(&rsd, src, 16) == 4,
         "a non-blocking write split a frame or reported the wrong count");
   CHECK(retro_spsc_read_avail(&rsd.ring) == 4094, "the fifo holds %u",
         (unsigned)retro_spsc_read_avail(&rsd.ring));
   retro_spsc_free(&rsd.ring);
}

/* One control reply: @hdr is the 8-byte header, followed by @body_len
 * bytes of filler. Returns rsnd_update_server_info()'s result. */
static int control_reply(rsound_t *rd, const char *hdr, size_t body_len)
{
   int    sv[2];
   int    ret;
   char   body[1024];
   int    saved = rd->conn.ctl_socket;

   if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
   {
      printf("FAIL: socketpair\n");
      failures++;
      return 0;
   }
   memset(body, 'A', sizeof(body));
   if (     write(sv[1], hdr, RSD_PROTO_CHUNKSIZE) != RSD_PROTO_CHUNKSIZE
         || write(sv[1], body, body_len) != (ssize_t)body_len)
   {
      printf("FAIL: control write\n");
      failures++;
   }
   rd->conn.ctl_socket = sv[0];
   ret                 = rsnd_update_server_info(rd);
   rd->conn.ctl_socket = saved;
   close(sv[0]);
   close(sv[1]);
   return ret;
}

/* Body lengths from the server are bounded by the reply buffer. */
static void check_control_length(rsound_t *rd)
{
   CHECK(control_reply(rd, "RSD  256", 256) == 0,
         "a body at the protocol limit was refused");
   CHECK(control_reply(rd, "RSD    0", 0) == 0,
         "an empty body was refused");
   CHECK(control_reply(rd, "RSD  257", 257) == -1,
         "a body past the protocol limit was accepted");
   CHECK(control_reply(rd, "RSD 1000", 1000) == -1,
         "an oversized body was accepted");
   CHECK(control_reply(rd, "RSD   -1", 600) == -1,
         "a negative body length was accepted");
}

int main(void)
{
   rsound_t *rd;
   unsigned  i;

   signal(SIGALRM, watchdog);
   signal(SIGPIPE, SIG_IGN);
   alarm(120);

   check_lend();
   check_nonblock_write();
   check_borrow_stream(false, false);
   check_borrow_stream(true,  false);
   check_borrow_stream(false, true);
   check_borrow_stream(true,  true);

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
   rsd_set_callback(rd, borrow_cb, release_cb, wait_cb, error_cb, NULL);

   check_control_length(rd);

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
