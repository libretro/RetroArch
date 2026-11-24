/* udev input: what one poll waits for.
 *
 * The hotplug monitor is in the driver's event set with the devices,
 * so a poll with nothing plugged in or out is the one wait. A hotplug
 * is handled before any device's events, and can take a device away:
 * what was waited for alongside it is waited for again, never read
 * from the device that went.
 *
 * The driver is included whole. Its device set is real; the monitor
 * and an input device are pipes, the monitor's messages read by a
 * stand-in for libudev that unplugs the device on 'r'.
 *
 * Checked here:
 *
 * - an idle poll is one epoll_wait and no poll();
 * - a device event is read in the one wait;
 * - a hotplug is handled, and a device event beside it still read;
 * - a hotplug that removes the device leaves no read of it. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>

#include <boolean.h>

struct udev_monitor;
struct udev_device;
static int polls, waits, receives;
static int test_poll(struct pollfd *fds, nfds_t n, int timeout);
static int test_epoll_wait(int epfd, struct epoll_event *ev, int max, int t);
static int test_monitor_fd(struct udev_monitor *m);
static struct udev_device *test_receive(struct udev_monitor *m);
#define poll(a, b, c)                  test_poll(a, b, c)
#define epoll_wait(a, b, c, d)         test_epoll_wait(a, b, c, d)
#define udev_monitor_get_fd(m)         test_monitor_fd(m)
#define udev_monitor_receive_device(m) test_receive(m)
#include "input/drivers/udev_input.c"
#undef poll
#undef epoll_wait
#undef udev_monitor_get_fd
#undef udev_monitor_receive_device

/* ---- The frontend, as far as the driver links against it ------------ */

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }

static settings_t stub_settings;
settings_t *config_get_ptr(void) { return &stub_settings; }
/* the settings a driver asks for by name, read from the ones above */
#include "../input_config_stubs.h"

struct retro_keybind *input_config_binds[MAX_USERS];
struct retro_keybind *input_autoconf_binds[MAX_USERS];
enum retro_key rarch_keysym_lut[RETROK_LAST];
const struct rarch_key_map rarch_key_map_linux[] = { { 0, RETROK_UNKNOWN } };
void input_keymaps_init_keyboard_lut(const struct rarch_key_map *map) { (void)map; }
enum retro_key input_keymaps_translate_keysym_to_rk(unsigned sym) { (void)sym; return RETROK_UNKNOWN; }
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device)
{ (void)down; (void)code; (void)character; (void)mod; (void)device; }
unsigned input_driver_lightgun_id_convert(unsigned id) { return id; }
bool input_driver_pointer_is_offscreen(int16_t x, int16_t y) { (void)x; (void)y; return false; }

bool linux_terminal_disable_input(void) { return true; }
bool linux_terminal_grab_stdin(void *data) { (void)data; return true; }
void linux_terminal_restore_input(void) { }
linux_illuminance_sensor_t *linux_open_illuminance_sensor(unsigned rate) { (void)rate; return NULL; }
void linux_close_illuminance_sensor(linux_illuminance_sensor_t *s) { (void)s; }
float linux_get_illuminance_reading(const linux_illuminance_sensor_t *s) { (void)s; return 0.0f; }
void linux_set_illuminance_sensor_rate(linux_illuminance_sensor_t *s, unsigned rate) { (void)s; (void)rate; }

enum rarch_display_type video_driver_display_type_get(void) { return RARCH_DISPLAY_NONE; }
bool video_driver_has_focus(void) { return true; }
bool video_driver_get_viewport_info(struct video_viewport *vp) { (void)vp; return false; }
bool video_driver_translate_coord_viewport(struct video_viewport *vp,
      int mouse_x, int mouse_y, uint32_t *res_pos, uint32_t *res_screen_pos,
      bool report_oob)
{
   (void)vp; (void)mouse_x; (void)mouse_y; (void)res_pos;
   (void)res_screen_pos; (void)report_oob;
   return false;
}

/* what the driver lists: recorded as the menu would be given it */
static char     kb_name[MAX_INPUT_DEVICES][64];
static unsigned kb_vid[MAX_INPUT_DEVICES], kb_pid[MAX_INPUT_DEVICES];
static char     ms_name[MAX_INPUT_DEVICES][64];
static bool     ms_hidden[MAX_INPUT_DEVICES];
static unsigned ms_vid[MAX_INPUT_DEVICES];

void input_config_clear_keyboard_display_names(void)
{
   memset(kb_name, 0, sizeof(kb_name));
   memset(kb_vid, 0, sizeof(kb_vid));
   memset(kb_pid, 0, sizeof(kb_pid));
}
void input_config_set_keyboard_display_name(unsigned idx, const char *name)
{
   if (idx < MAX_INPUT_DEVICES)
      strlcpy(kb_name[idx], name ? name : "", sizeof(kb_name[idx]));
}
void input_config_set_keyboard_ids(unsigned idx, uint16_t vid, uint16_t pid)
{
   if (idx < MAX_INPUT_DEVICES)
   {
      kb_vid[idx] = vid;
      kb_pid[idx] = pid;
   }
}
void input_config_set_mouse_display_name(unsigned port, const char *name)
{
   if (port < MAX_INPUT_DEVICES)
      strlcpy(ms_name[port], name ? name : "", sizeof(ms_name[port]));
}
void input_config_set_mouse_device(unsigned idx, const char *device,
      uint16_t vid, uint16_t pid, bool hidden)
{
   (void)device; (void)pid;
   if (idx < MAX_INPUT_DEVICES)
   {
      ms_vid[idx]    = vid;
      ms_hidden[idx] = hidden;
   }
}

/* what the driver says its keyboards and mice are known by, and what
 * the frontend then tells a port to read: set by the test */
static char     pin_kb[MAX_INPUT_DEVICES][64], pin_ms[MAX_INPUT_DEVICES][64];
static unsigned pin_kb_n, pin_ms_n;
static int      port_keyboard[MAX_USERS];
static unsigned port_mouse[MAX_USERS];

void input_keyboard_pins_set_devices(const char (*base)[64], unsigned n)
{
   memset(pin_kb, 0, sizeof(pin_kb));
   pin_kb_n = n;
   if (n)
      memcpy(pin_kb, base, n * sizeof(pin_kb[0]));
}
void input_mouse_pins_set_devices(const char (*base)[64], unsigned n)
{
   memset(pin_ms, 0, sizeof(pin_ms));
   pin_ms_n = n;
   if (n)
      memcpy(pin_ms, base, n * sizeof(pin_ms[0]));
}
int input_keyboard_port_choice(unsigned port) { return port_keyboard[port]; }
unsigned input_mouse_port_index(unsigned port) { return port_mouse[port]; }


/* ---- The monitor and the device -------------------------------------- */

static int monitor_pipe[2], device_pipe[2], events_read;
static udev_input_t *test_udev;

static int test_poll(struct pollfd *fds, nfds_t n, int timeout)
{
   polls++;
   return poll(fds, n, timeout);
}
static int test_epoll_wait(int epfd, struct epoll_event *ev, int max, int t)
{
   waits++;
   return epoll_wait(epfd, ev, max, t);
}
static int test_monitor_fd(struct udev_monitor *m)
{
   (void)m;
   return monitor_pipe[0];
}
static struct udev_device *test_receive(struct udev_monitor *m)
{
   char message;
   (void)m;
   if (read(monitor_pipe[0], &message, 1) != 1)
      return NULL;
   receives++;
   if (message == 'r')
      udev_input_remove_device(test_udev, "/dev/input/event9");
   return NULL;
}
static void count_event(void *data, const struct input_event *event,
      udev_input_device_t *dev)
{
   (void)data; (void)event; (void)dev;
   events_read++;
}

static unsigned failures;
#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
   else { printf("   ok   "); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

static void frame(void)
{
   polls = waits = receives = events_read = 0;
   udev_input_poll(test_udev);
}
static void send_event(void)
{
   struct input_event e;
   memset(&e, 0, sizeof(e));
   e.type = EV_KEY;
   e.code = KEY_A;
   e.value = 1;
   if (write(device_pipe[1], &e, sizeof(e)) != (ssize_t)sizeof(e))
      abort();
}
static void hotplug(char message)
{
   if (write(monitor_pipe[1], &message, 1) != 1)
      abort();
}

int main(void)
{
   struct epoll_event event;
   udev_input_device_t *dev;
   if (pipe2(monitor_pipe, O_NONBLOCK) || pipe2(device_pipe, O_NONBLOCK))
      return 1;
   test_udev          = (udev_input_t*)calloc(1, sizeof(*test_udev));
   test_udev->fd      = epoll_create(32);
   test_udev->monitor = (struct udev_monitor*)&monitor_pipe;
   udev_input_watch_monitor(test_udev);
   dev                = (udev_input_device_t*)calloc(1, sizeof(*dev));
   dev->fd            = device_pipe[0];
   dev->type          = UDEV_INPUT_KEYBOARD;
   dev->handle_cb     = count_event;
   strlcpy(dev->devnode, "/dev/input/event9", sizeof(dev->devnode));
   test_udev->devices = (udev_input_device_t**)malloc(sizeof(*test_udev->devices));
   test_udev->devices[test_udev->num_devices++] = dev;
   event.events       = EPOLLIN;
   event.data.ptr     = dev;
   if (epoll_ctl(test_udev->fd, EPOLL_CTL_ADD, dev->fd, &event))
      return 1;

   frame();
   CHECK(waits == 1 && !polls,
         "an idle poll is one wait (%d epoll_wait, %d poll)", waits, polls);
   send_event();
   frame();
   CHECK(waits == 1 && !polls && events_read == 1,
         "a device event is read in the one wait");
   hotplug('a');
   send_event();
   frame();
   CHECK(receives == 1 && events_read == 1,
         "a hotplug is handled and the device event beside it read");
   frame();
   CHECK(waits == 1 && !polls && !receives && !events_read,
         "the poll after it is idle again");
   hotplug('r');
   send_event();
   frame();
   CHECK(receives == 1 && !events_read && !test_udev->num_devices,
         "a hotplug that takes the device away leaves no read of it");

   close(monitor_pipe[0]);
   close(monitor_pipe[1]);
   close(device_pipe[1]);
   close(test_udev->fd);
   free(test_udev->devices);
   free(test_udev);
   if (failures)
   {
      printf("FAIL udev_poll_test: %u\n", failures);
      return 1;
   }
   printf("PASS udev_poll_test\n");
   return 0;
}
