/* The Remote RetroPad core sends a control's state when it changes,
 * by UDP, and UDP may lose a message: a lost release left the control
 * held at the receiver until it changed again. The core now sends a
 * change again 5, 15 and 30 frames after it.
 *
 * The real core is driven here by a frontend of a few callbacks, with
 * its messages read from a socket on the loopback address. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <libretro.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

void retro_init(void);
void retro_deinit(void);
void retro_run(void);
bool retro_load_game(const struct retro_game_info *info);
void retro_set_environment(retro_environment_t cb);
void retro_set_video_refresh(retro_video_refresh_t cb);
void retro_set_audio_sample(retro_audio_sample_t cb);
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);
void retro_set_input_poll(retro_input_poll_t cb);
void retro_set_input_state(retro_input_state_t cb);

/* the core's message, as it sends it */
struct message
{
   int port;
   int device;
   int index;
   int id;
   uint16_t state;
};

static unsigned failures;
#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

/* ---- the frontend -------------------------------------------------- */

static char port_text[16];
static int  pad_a;      /* RetroPad A, held or not */
static int  stick_x;    /* left stick X */

static void RETRO_CALLCONV log_none(enum retro_log_level level, const char *fmt, ...)
{ (void)level; (void)fmt; }

static bool RETRO_CALLCONV environment(unsigned cmd, void *data)
{
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
         ((struct retro_log_callback*)data)->log = log_none;
         return true;
      case RETRO_ENVIRONMENT_GET_VARIABLE:
         {
            struct retro_variable *var = (struct retro_variable*)data;
            var->value = NULL;
            if (!strcmp(var->key, "net_retropad_port"))
               var->value = port_text;
            else if (!strcmp(var->key, "net_retropad_ip_octet1"))
               var->value = "127";
            else if (   !strcmp(var->key, "net_retropad_ip_octet2")
                     || !strcmp(var->key, "net_retropad_ip_octet3"))
               var->value = "0";
            else if (!strcmp(var->key, "net_retropad_ip_octet4"))
               var->value = "1";
            else if (!strcmp(var->key, "net_retropad_screen"))
               var->value = "RetroPad";
            return var->value != NULL;
         }
      case RETRO_ENVIRONMENT_SET_VARIABLES:
      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
      case RETRO_ENVIRONMENT_SET_MESSAGE:
         return true;
   }
   return false;
}

static void RETRO_CALLCONV video(const void *d, unsigned w, unsigned h, size_t p)
{ (void)d; (void)w; (void)h; (void)p; }
static void RETRO_CALLCONV audio(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t RETRO_CALLCONV audio_batch(const int16_t *d, size_t n) { (void)d; return n; }
static void RETRO_CALLCONV input_poll(void) { }

static int16_t RETRO_CALLCONV input_state(unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   if (port != 0)
      return 0;
   if (device == RETRO_DEVICE_JOYPAD && id == RETRO_DEVICE_ID_JOYPAD_A)
      return (int16_t)pad_a;
   if (     device == RETRO_DEVICE_ANALOG
         && index  == RETRO_DEVICE_INDEX_ANALOG_LEFT
         && id     == RETRO_DEVICE_ID_ANALOG_X)
      return (int16_t)stick_x;
   return 0;
}

/* ---- what arrives --------------------------------------------------- */

#define SEEN_MAX 256
static struct { unsigned frame; int device, id; int state; } seen[SEEN_MAX];
static unsigned seen_n, frame;
static int      fd = -1;

static void take(void)
{
   for (;;)
   {
      struct message m;
      int ret = (int)recv(fd, (char*)&m, sizeof(m), 0);
      if (ret != (int)sizeof(m))
         break;
      if (seen_n < SEEN_MAX)
      {
         seen[seen_n].frame  = frame;
         seen[seen_n].device = m.device;
         seen[seen_n].id     = m.id;
         seen[seen_n].state  = (int16_t)m.state;
         seen_n++;
      }
   }
}

static void run(unsigned frames)
{
   unsigned i;
   for (i = 0; i < frames; i++)
   {
      retro_run();
      take();
      frame++;
   }
   /* a message still on its way belongs to the frame before */
   retro_sleep(2);
   if (frame)
   {
      frame--;
      take();
      frame++;
   }
}

/* the frames at which (@device, @id) arrived with @state, since @from */
static unsigned frames_of(int device, int id, int state, unsigned from,
      unsigned *out, unsigned max)
{
   unsigned i, n = 0;
   for (i = 0; i < seen_n; i++)
      if (     seen[i].frame >= from && seen[i].device == device
            && seen[i].id == id && seen[i].state == state && n < max)
         out[n++] = seen[i].frame - from;
   return n;
}

int main(void)
{
   void *addr = NULL;
   unsigned at[16], n, f0;
   unsigned port = 56000 + (unsigned)(cpu_features_get_time_usec() % 4000);

   network_init();
   fd = socket_init(&addr, (uint16_t)port, "127.0.0.1", SOCKET_TYPE_DATAGRAM, AF_INET);
   if (fd < 0 || !socket_nonblock(fd) || !socket_bind(fd, addr))
   {
      printf("FAIL net_retropad_repeat_test: no socket on port %u\n", port);
      return 1;
   }
   snprintf(port_text, sizeof(port_text), "%u", port);

   retro_set_environment(environment);
   retro_set_video_refresh(video);
   retro_set_audio_sample(audio);
   retro_set_audio_sample_batch(audio_batch);
   retro_set_input_poll(input_poll);
   retro_set_input_state(input_state);
   retro_init();
   retro_load_game(NULL);

   /* nothing changes: nothing is sent */
   run(40);
   CHECK(seen_n == 0, "%u message(s) sent with nothing changed", seen_n);
   printf("   ok   with nothing changed nothing is sent\n");

   /* a press: sent, and again 5, 15 and 30 frames on; then no more */
   f0    = frame;
   pad_a = 1;
   run(70);
   n = frames_of(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_A, 1, f0, at, 16);
   CHECK(n == 4 && at[0] == 0 && at[1] == 5 && at[2] == 15 && at[3] == 30,
         "a press arrived %u time(s), at frames %u %u %u %u: want 4, at 0 5 15 30",
         n, at[0], n > 1 ? at[1] : 0, n > 2 ? at[2] : 0, n > 3 ? at[3] : 0);
   CHECK(seen_n == 4, "%u messages in all after one press, want 4", seen_n);
   printf("   ok   a press is sent, and again 5, 15 and 30 frames on, and then no more\n");

   /* a release, the same */
   f0    = frame;
   pad_a = 0;
   run(70);
   n = frames_of(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_A, 0, f0, at, 16);
   CHECK(n == 4 && at[0] == 0 && at[1] == 5 && at[2] == 15 && at[3] == 30,
         "a release arrived %u time(s), want 4 at frames 0 5 15 30", n);
   printf("   ok   a release is sent again the same way\n");

   /* pressed, and let go three frames on: the press is not sent again
    * after the release, only the release is */
   f0    = frame;
   pad_a = 1;
   run(3);
   pad_a = 0;
   run(70);
   n = frames_of(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_A, 1, f0, at, 16);
   CHECK(n == 1 && at[0] == 0, "a press let go at once was sent %u time(s), want once", n);
   n = frames_of(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_A, 0, f0, at, 16);
   CHECK(n == 4 && at[0] == 3 && at[1] == 8 && at[2] == 18 && at[3] == 33,
         "its release arrived %u time(s), want 4 at frames 3 8 18 33", n);
   printf("   ok   a press let go at once is not sent again after its release\n");

   /* a stick, the same */
   f0      = frame;
   stick_x = 12345;
   run(70);
   n = frames_of(RETRO_DEVICE_ANALOG, RETRO_DEVICE_ID_ANALOG_X, 12345, f0, at, 16);
   CHECK(n == 4 && at[0] == 0 && at[3] == 30,
         "a stick's move arrived %u time(s), want 4, the last 30 frames on", n);
   printf("   ok   a stick's move is sent again the same way\n");

   retro_deinit();
   socket_close(fd);
   if (failures)
   {
      printf("FAIL net_retropad_repeat_test: %u\n", failures);
      return 1;
   }
   printf("PASS net_retropad_repeat_test\n");
   return 0;
}
