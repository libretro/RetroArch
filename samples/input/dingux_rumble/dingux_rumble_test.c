/* The Dingux joypad driver's vibration motor, written through evdev
 * from the output writer's thread.
 *
 * The real driver is included, with /dev/input stood in for: a few
 * event nodes, of which one plays a periodic sine, and the uploads,
 * plays and gains made to it noted. An upload can be made slow, as a
 * motor's driver can be, to show the frontend's call does not wait.
 * Built twice, with threads and without: without, the same writes are
 * made on the caller's thread. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#include <boolean.h>

/* ---- /dev/input, stood in for --------------------------------------- */

#define FAKE_FD 1000
#define NODES   4

/* what each node is: 0 nothing, 1 rumble only, 2 a periodic sine with
 * gain, 3 the same again; @motor_gain off makes 2 a motor without */
static int  node_kind[NODES] = { 0, 1, 2, 3 };
static int  motor_gain       = 1;
static int  opened[NODES];          /* open now */
static int  slow_ms;
/* written on the writer's thread, read on the test's: only through
 * the atomics below */
static int  uploads, plays, stops, gains, erases;
static int  last_period, last_magnitude, last_id_in, last_gain;
static int  next_id = 3;            /* the writer's thread's alone */

#define A_SET(v, x) __atomic_store_n(&(v), (x), __ATOMIC_SEQ_CST)
#define A_INC(v)    __atomic_fetch_add(&(v), 1, __ATOMIC_SEQ_CST)
#define N(v)        __atomic_load_n(&(v), __ATOMIC_SEQ_CST)

static int node_of(int fd)
{
   int n = fd - FAKE_FD;
   return (n >= 0 && n < NODES && N(opened[n])) ? n : -1;
}

static int test_open(const char *path, int flags, ...)
{
   unsigned n;
   (void)flags;
   if (sscanf(path, "/dev/input/event%u", &n) != 1 || n >= NODES)
   {
      errno = ENOENT;
      return -1;
   }
   A_SET(opened[n], 1);
   return FAKE_FD + (int)n;
}

static int test_close(int fd)
{
   int n = node_of(fd);
   if (n < 0)
      return close(fd);
   A_SET(opened[n], 0);
   return 0;
}

static void set_bit_in(unsigned long *bits, unsigned bit)
{
   bits[bit / (8 * sizeof(long))] |= 1UL << (bit % (8 * sizeof(long)));
}

static int test_ioctl(int fd, unsigned long req, uintptr_t arg)
{
   int n = node_of(fd);
   if (n < 0)
   {
      errno = EBADF;
      return -1;
   }
   if (req == EVIOCGBIT(EV_FF, sizeof(unsigned long) * ((FF_MAX + 8 * sizeof(long)) / (8 * sizeof(long)))))
   {
      unsigned long *bits = (unsigned long*)arg;
      memset(bits, 0, sizeof(unsigned long) * ((FF_MAX + 8 * sizeof(long)) / (8 * sizeof(long))));
      if (node_kind[n] >= 1)
         set_bit_in(bits, FF_RUMBLE);
      if (node_kind[n] >= 2)
      {
         set_bit_in(bits, FF_PERIODIC);
         set_bit_in(bits, FF_SINE);
         if (motor_gain)
            set_bit_in(bits, FF_GAIN);
      }
      return 0;
   }
   if (req == EVIOCGEFFECTS)
   {
      *(int*)arg = node_kind[n] ? 16 : 0;
      return 0;
   }
   if (req == EVIOCSFF)
   {
      struct ff_effect *e = (struct ff_effect*)arg;
      if (slow_ms)
      {
         struct timespec ts;
         ts.tv_sec  = slow_ms / 1000;
         ts.tv_nsec = (slow_ms % 1000) * 1000000L;
         nanosleep(&ts, NULL);
      }
      if (e->type != FF_PERIODIC || e->u.periodic.waveform != FF_SINE)
      {
         errno = EINVAL;
         return -1;
      }
      A_SET(last_id_in, e->id);
      if (e->id < 0)
         e->id = (short)next_id++;
      A_SET(last_period, e->u.periodic.period);
      A_SET(last_magnitude, e->u.periodic.magnitude);
      A_INC(uploads);
      return 0;
   }
   if (req == EVIOCRMFF)
   {
      A_INC(erases);
      return 0;
   }
   errno = ENOTTY;
   return -1;
}

static ssize_t test_write(int fd, const void *buf, size_t count)
{
   const struct input_event *ie = (const struct input_event*)buf;
   if (node_of(fd) < 0)
      return write(fd, buf, count);
   if (count != sizeof(*ie) || ie->type != EV_FF)
   {
      errno = EINVAL;
      return -1;
   }
   if (ie->code == FF_GAIN)
   {
      A_SET(last_gain, ie->value);
      A_INC(gains);
   }
   else if (ie->value)
      A_INC(plays);
   else
      A_INC(stops);
   return (ssize_t)count;
}

#define open(...)             test_open(__VA_ARGS__)
#define close(fd)             test_close((fd))
#define write(fd, buf, count) test_write((fd), (buf), (count))
#define ioctl(fd, req, arg)   test_ioctl((fd), (unsigned long)(req), (uintptr_t)(arg))

#include "input/drivers_joypad/sdl_dingux_joypad.c"
#undef open
#undef close
#undef write
#undef ioctl

/* ---- what the driver links against --------------------------------- */

static unsigned stub_gain = 50;
unsigned input_config_get_rumble_gain(void) { return stub_gain; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port, unsigned vid,
      unsigned pid)
{ (void)name; (void)display_name; (void)phys; (void)driver; (void)port; (void)vid; (void)pid; return true; }
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{ (void)port; (void)name; return true; }
void input_driver_set_platform_menu_button(bool v) { (void)v; }
void input_driver_set_platform_menu_pressed(bool v) { (void)v; }
bool input_driver_platform_menu_button(void) { return false; }

uint32_t SDL_WasInit(uint32_t flags) { (void)flags; return SDL_INIT_JOYSTICK; }
int SDL_Init(uint32_t flags) { (void)flags; return 0; }
int SDL_InitSubSystem(uint32_t flags) { (void)flags; return 0; }
int SDL_NumJoysticks(void) { return 0; }
SDL_Joystick *SDL_JoystickOpen(int index) { (void)index; return NULL; }
int SDL_JoystickNumAxes(SDL_Joystick *j) { (void)j; return 0; }
int16_t SDL_JoystickGetAxis(SDL_Joystick *j, int axis) { (void)j; (void)axis; return 0; }
void SDL_JoystickUpdate(void) { }
void SDL_JoystickClose(SDL_Joystick *j) { (void)j; }
int SDL_PollEvent(SDL_Event *event) { (void)event; return 0; }

/* ---- the test ------------------------------------------------------- */

static unsigned failures;
#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

/* waits for @v to reach @want: at once where there is no writer */
static void settle(int *v, int want)
{
   int i;
   for (i = 0; i < 600 && N(*v) < want; i++)
      usleep(5000);
}

/* something that must not come is given this long to */
static void quiet(void) { usleep(50000); }

int main(void)
{
   int i, before, strong_id;

   /* the motor found: the first node that plays a sine, the others
    * looked at and let go; its gain written as it is found */
   sdl_dingux_joypad_init(NULL);
   CHECK(dingux_joypad.rumble.fd == FAKE_FD + 2,
         "the motor was not found on the first node that plays a sine (%d)",
         dingux_joypad.rumble.fd);
   CHECK(!N(opened[0]) && !N(opened[1]) && N(opened[2]) && !N(opened[3]),
         "the nodes looked at were not let go");
#ifdef HAVE_THREADS
   CHECK(dingux_joypad.rumble.writer != NULL, "the rumble writer did not start");
#endif
   settle(&gains, 1);
   CHECK(N(gains) == 1 && N(last_gain) == 0xFFFF * 50 / 100,
         "the gain was not written as the motor was found (%d, 0x%x)",
         N(gains), N(last_gain));
   printf("   ok   the motor is the first node that plays a sine, its gain written\n");

   /* strong: a 7 Hz sine at full strength, uploaded then played */
   CHECK(sdl_dingux_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0xFFFF),
         "rumble was refused");
   settle(&plays, 1);
   CHECK(N(uploads) == 1 && N(last_id_in) == -1 && N(last_period) == 142
         && N(last_magnitude) == 0x7FFF && N(plays) == 1,
         "the strong motor was not uploaded and played (%d %d %d 0x%x %d)",
         N(uploads), N(last_id_in), N(last_period), N(last_magnitude), N(plays));
   strong_id = dingux_joypad.rumble.motor[RETRO_RUMBLE_STRONG].id;

   /* weak: a 200 Hz sine at half */
   CHECK(sdl_dingux_joypad_set_rumble(0, RETRO_RUMBLE_WEAK, 0x8000),
         "rumble was refused");
   settle(&plays, 2);
   CHECK(N(uploads) == 2 && N(last_period) == 5
         && N(last_magnitude) == (0x7FFF * 0x8000) / 0xFFFF && N(plays) == 2,
         "the weak motor was not uploaded and played");
   printf("   ok   strong and weak: their sines uploaded at their strengths and played\n");

   /* the same again, as a core asks each frame: nothing written */
   before = N(uploads) + N(plays) + N(stops);
   for (i = 0; i < 100; i++)
   {
      sdl_dingux_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0xFFFF);
      sdl_dingux_joypad_set_rumble(0, RETRO_RUMBLE_WEAK, 0x8000);
   }
   quiet();
   CHECK(N(uploads) + N(plays) + N(stops) == before,
         "an unchanged strength was written again");

   /* a new strength while playing: uploaded over the same effect */
   sdl_dingux_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0x4000);
   settle(&uploads, 3);
   quiet();
   CHECK(N(uploads) == 3 && N(last_id_in) == strong_id && N(plays) == 2,
         "a new strength was not uploaded over the playing effect");

   /* stopped, then started at the strength it had: a play, no upload */
   sdl_dingux_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0);
   settle(&stops, 1);
   sdl_dingux_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0x4000);
   settle(&plays, 3);
   quiet();
   CHECK(N(stops) == 1 && N(plays) == 3 && N(uploads) == 3,
         "a stop and a restart at the same strength were not a stop and a play");
   printf("   ok   unchanged strengths cost nothing; a change, a stop and a restart are written as they should be\n");

   /* the gain changed */
   CHECK(sdl_dingux_joypad_set_rumble_gain(0, 100), "the gain was refused");
   settle(&gains, 2);
   CHECK(N(last_gain) == 0xFFFF, "the new gain was not written (0x%x)", N(last_gain));
   CHECK(!sdl_dingux_joypad_set_rumble(1, RETRO_RUMBLE_STRONG, 0xFFFF),
         "rumble was accepted for a second pad");

#ifdef HAVE_THREADS
   /* a motor slow to take an upload: the call returns before it has */
   slow_ms = 300;
   before  = N(uploads);
   sdl_dingux_joypad_set_rumble(0, RETRO_RUMBLE_WEAK, 0x1000);
   CHECK(N(uploads) == before, "the call waited for a motor that takes 300 ms");
   settle(&uploads, before + 1);
   CHECK(N(uploads) == before + 1 && N(last_magnitude) == (0x7FFF * 0x1000) / 0xFFFF,
         "the strength did not arrive");
   slow_ms = 0;
   printf("   ok   a motor 300 ms slow to take an upload: the call returns at once\n");
#endif

   /* the driver stopped: both stopped, given back, the node closed */
   sdl_dingux_joypad_destroy();
   CHECK(N(stops) == 3 && N(erases) == 2 && !N(opened[2]),
         "the motor was not stopped and let go (%d %d %d)",
         N(stops), N(erases), N(opened[2]));
   CHECK(dingux_joypad.rumble.fd == -1, "the driver kept the motor's descriptor");
   CHECK(!sdl_dingux_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0xFFFF),
         "rumble was accepted with the driver stopped");
   printf("   ok   stopping the driver stops both motors and lets the node go\n");

   /* a motor without gain: none written, and the gain call refused */
   motor_gain = 0;
   before     = N(gains);
   sdl_dingux_joypad_init(NULL);
   quiet();
   CHECK(dingux_joypad.rumble.fd == FAKE_FD + 2 && N(gains) == before,
         "a gain was written to a motor that has none");
   CHECK(!sdl_dingux_joypad_set_rumble_gain(0, 80),
         "a gain was accepted for a motor that has none");
   sdl_dingux_joypad_destroy();

   /* no motor at all */
   node_kind[2] = node_kind[3] = 1;
   sdl_dingux_joypad_init(NULL);
   CHECK(dingux_joypad.rumble.fd == -1, "a motor was found where none plays a sine");
   CHECK(!sdl_dingux_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0xFFFF),
         "rumble was accepted with no motor");
   for (i = 0; i < NODES; i++)
      CHECK(!N(opened[i]), "node %d was left open", i);
   sdl_dingux_joypad_destroy();
   printf("   ok   a motor without gain, and none at all\n");

   if (failures)
   {
      printf("FAIL dingux_rumble_test: %u\n", failures);
      return 1;
   }
   printf("PASS dingux_rumble_test\n");
   return 0;
}
