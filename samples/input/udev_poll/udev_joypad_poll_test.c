/* udev joypad: what one poll reads.
 *
 * The pads' event nodes and the hotplug monitor are one event set, so
 * a poll reads only the pads with something to read: with four pads
 * plugged in and none moved, it is the one wait and no read. A pad
 * unplugged leaves the set before its node is closed, though the
 * rumble writer's copy of the node lives on.
 *
 * The driver is included whole; its pads are pipes, and the monitor a
 * pipe read by a stand-in for libudev.
 *
 * Checked here:
 *
 * - an idle poll with four pads is one epoll_wait, no read, no poll();
 * - an event on one pad reads that pad alone;
 * - a pad unplugged is out of the set, even with its node still open
 *   elsewhere, and is not read;
 * - a hotplug is noticed through the set. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <linux/input.h>

#include <boolean.h>

struct udev_monitor;
struct udev_device;
static int polls, waits, reads[64], receives;
static int pad_pipe[4][2], monitor_pipe[2];
static int test_poll(struct pollfd *fds, nfds_t n, int timeout);
static int test_epoll_wait(int epfd, struct epoll_event *ev, int max, int t);
static ssize_t test_read(int fd, void *buf, size_t count);
static int test_monitor_fd(struct udev_monitor *m);
static struct udev_device *test_receive(struct udev_monitor *m);
#define poll(a, b, c)                  test_poll(a, b, c)
#define epoll_wait(a, b, c, d)         test_epoll_wait(a, b, c, d)
#define read(a, b, c)                  test_read(a, b, c)
#define udev_monitor_get_fd(m)         test_monitor_fd(m)
#define udev_monitor_receive_device(m) test_receive(m)
#include "input/drivers_joypad/udev_joypad.c"
#undef poll
#undef epoll_wait
#undef read
#undef udev_monitor_get_fd
#undef udev_monitor_receive_device

/* ---- what the driver links against --------------------------------- */

static settings_t stub_settings;
settings_t *config_get_ptr(void) { return &stub_settings; }
#include "../input_config_stubs.h"
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
bool input_autoconfigure_connect_uniq(const char *name,
      const char *display_name, const char *phys, const char *uniq,
      const char *driver, unsigned port, unsigned vid, unsigned pid)
{ (void)name; (void)display_name; (void)phys; (void)uniq; (void)driver; (void)port; (void)vid; (void)pid; return true; }
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{ (void)port; (void)name; return true; }
const char *input_config_get_device_name(unsigned port) { (void)port; return NULL; }

/* ---- the pads and the monitor --------------------------------------- */

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
static ssize_t test_read(int fd, void *buf, size_t count)
{
   int p;
   for (p = 0; p < 4; p++)
      if (fd == pad_pipe[p][0])
         break;
   reads[p < 4 ? p : MAX_USERS]++;
   return read(fd, buf, count);
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
   if (read(monitor_pipe[0], &message, 1) == 1)
      receives++;
   return NULL;
}

static unsigned failures;
#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
   else { printf("   ok   "); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

static int total_reads(void)
{
   int p, n = 0;
   for (p = 0; p <= MAX_USERS; p++)
      n += reads[p];
   return n;
}
static void frame(void)
{
   polls = waits = receives = 0;
   memset(reads, 0, sizeof(reads));
   udev_joypad_poll();
}
static void send_event(int p)
{
   struct input_event e;
   memset(&e, 0, sizeof(e));
   e.type  = EV_KEY;
   e.code  = BTN_SOUTH;
   e.value = 1;
   if (write(pad_pipe[p][1], &e, sizeof(e)) != (ssize_t)sizeof(e))
      abort();
}

int main(void)
{
   struct epoll_event event;
   int p, kept;
   char message = 'a';

   for (p = 0; p < MAX_USERS; p++)
   {
      udev_pad_set_fd(p, -1);
      udev_pads[p].sensor_fd = -1;
      udev_pads[p].touch_fd  = -1;
   }
   if (pipe2(monitor_pipe, O_NONBLOCK))
      return 1;
   udev_joypad_ep          = epoll_create(MAX_USERS + 1);
   udev_joypad_mon         = (struct udev_monitor*)&monitor_pipe;
   event.events            = EPOLLIN;
   event.data.u64          = 0;
   event.data.u32          = MAX_USERS;
   udev_joypad_mon_watched = epoll_ctl(udev_joypad_ep, EPOLL_CTL_ADD,
         monitor_pipe[0], &event) == 0;
   for (p = 0; p < 4; p++)
   {
      if (pipe2(pad_pipe[p], O_NONBLOCK))
         return 1;
      udev_pads[p].sensor_fd = -1;
      udev_pads[p].touch_fd  = -1;
      udev_pad_set_fd(p, pad_pipe[p][0]);
   }

   frame();
   CHECK(waits == 1 && !polls && !total_reads(),
         "an idle poll with four pads is one wait (%d epoll_wait, %d poll, %d read)",
         waits, polls, total_reads());
   send_event(2);
   frame();
   CHECK(reads[2] == 2 && total_reads() == 2,
         "an event on one pad reads that pad alone (%d reads)", total_reads());

   /* the node stays open elsewhere, as the rumble writer's copy keeps it */
   kept = dup(pad_pipe[1][0]);
   udev_free_pad(1);
   send_event(1);
   frame();
   CHECK(waits == 1 && !total_reads(),
         "a pad unplugged is out of the set and not read (%d reads)", total_reads());
   close(kept);

   if (write(monitor_pipe[1], &message, 1) != 1)
      abort();
   frame();
   CHECK(receives == 1,
         "a hotplug is noticed through the set");
   frame();
   CHECK(!receives && !polls, "the poll after it is idle again");

   for (p = 0; p < 4; p++)
   {
      if (p != 1)
         udev_free_pad(p);
      close(pad_pipe[p][1]);
   }
   close(monitor_pipe[0]);
   close(monitor_pipe[1]);
   close(udev_joypad_ep);
   if (failures)
   {
      printf("FAIL udev_joypad_poll_test: %u\n", failures);
      return 1;
   }
   printf("PASS udev_joypad_poll_test\n");
   return 0;
}
