/* Regression test for input/drivers_hid/libusb_hid.c against a fake
 * libusb (fake_libusb.c).
 *
 * Each pad used to have a thread doing blocking interrupt transfers
 * with a one-second timeout: a command queued behind a read waited for
 * the pad to send a report or for the second to run out, and removing
 * a pad or freeing the driver joined that thread, which took up to a
 * second or two per pad. The driver now runs one asynchronous IN
 * transfer per pad and sends commands on one queued OUT transfer, all
 * completed on the poll thread; removal cancels and nothing waits.
 *
 * The contract this pins:
 *
 *   attach            -> one read in flight, reports reach the pad
 *   commands          -> sent at once, in order, one at a time, even
 *                        while the pad sends nothing
 *   unplug            -> transfers come back, the adapter is freed,
 *                        its slot released and the pad disconnected
 *   free with a pad   -> returns at once with a send still pending;
 *                        every transfer and handle released, none
 *                        freed while in flight
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include <boolean.h>
#include <retro_atomic.h>

#include "input/input_driver.h"
#include "input/connect/joypad_connection.h"
#include "fake_libusb.h"

extern hid_driver_t libusb_hid;

/* ---- the rest of RetroArch, stubbed ---- */

void RARCH_LOG(const char *f, ...)  { (void)f; }
void RARCH_WARN(const char *f, ...) { (void)f; }
void RARCH_ERR(const char *f, ...)  { (void)f; }


static retro_atomic_int_t packets     = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t last_byte   = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t disconnects = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t deinits     = RETRO_ATOMIC_INT_INITIALIZER(0);
static bool slot_used[16];
/* What the driver hands pad_connection_pad_init() as the device: the
 * adapter, which is what send_control() takes back. */
static void *last_device;

joypad_connection_t *pad_connection_init(unsigned pads)
{ return (joypad_connection_t*)calloc(pads, sizeof(joypad_connection_t)); }
void pad_connection_destroy(joypad_connection_t *c) { free(c); }
int32_t pad_connection_pad_init(joypad_connection_t *c, const char *name,
      uint16_t vid, uint16_t pid, void *data, hid_driver_t *driver)
{
   int i;
   for (i = 0; i < 16; i++)
      if (!slot_used[i]) { slot_used[i] = true; last_device = data; return i; }
   return -1;
}
void pad_connection_pad_deinit(joypad_connection_t *c, uint32_t idx)
{ slot_used[idx] = false; retro_atomic_fetch_add_int(&deinits, 1); }
void pad_connection_packet(joypad_connection_t *c, uint32_t idx,
      uint8_t *data, uint32_t length)
{
   retro_atomic_store_release_int(&last_byte, data[0]);
   retro_atomic_fetch_add_int(&packets, 1);
}
bool pad_connection_has_interface(joypad_connection_t *c, unsigned idx) { return true; }
void pad_connection_get_buttons(joypad_connection_t *c, unsigned idx, input_bits_t *s) { }
int16_t pad_connection_get_axis(joypad_connection_t *c, unsigned idx, unsigned i) { return 0; }
bool pad_connection_rumble(joypad_connection_t *c, unsigned pad,
      enum retro_rumble_effect e, uint16_t s) { return false; }
bool input_autoconfigure_connect(const char *a, const char *b, const char *c,
      const char *d, unsigned port, unsigned vid, unsigned pid) { return true; }
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{ retro_atomic_fetch_add_int(&disconnects, 1); return true; }

/* ---- helpers ---- */

static unsigned failures = 0;

static void check(bool cond, const char *what)
{
   printf("  [%s] %s\n", cond ? "pass" : "FAIL", what);
   if (!cond)
      failures++;
}

static double now_ms(void)
{
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

/* Polls a condition the poll thread will make true; 2 s cap. */
#define EVENTUALLY(expr) do { double t0_ = now_ms(); \
   while (!(expr) && now_ms() - t0_ < 2000) usleep(200); } while (0)

int main(void)
{
   struct libusb_device *pad = fake_new_device();
   void *hid;
   unsigned char rep[4] = { 0x42, 0, 0, 0 };
   uint8_t cmd[3][2]    = { { 0xA1, 1 }, { 0xB2, 2 }, { 0xC3, 3 } };
   double t0, took;

   printf("attach\n");
   fake_plug(pad);
   hid = libusb_hid.init();
   check(hid != NULL, "driver initialises");
   if (!hid)
      return 1;
   check(fake_snapshot().opens == 1, "the pad is opened");
   check(fake_in_flight(pad, 1) == 1, "one read is in flight");

   printf("reports\n");
   check(fake_report(pad, rep, 4), "the pad sends a report");
   EVENTUALLY(retro_atomic_load_acquire_int(&packets) == 1);
   check(retro_atomic_load_acquire_int(&packets) == 1
      && retro_atomic_load_acquire_int(&last_byte) == 0x42,
         "the report reaches the pad handler");
   EVENTUALLY(fake_in_flight(pad, 1) == 1);
   check(fake_in_flight(pad, 1) == 1, "the read is resubmitted");

   printf("commands\n");
   /* The pad sends nothing from here: a command must not wait on it. */
   libusb_hid.send_control(last_device, cmd[0], 2);
   check(fake_in_flight(pad, 0) == 1, "a command goes out at once, with no report pending");
   libusb_hid.send_control(last_device, cmd[1], 2);
   libusb_hid.send_control(last_device, cmd[2], 2);
   check(fake_in_flight(pad, 0) == 1, "further commands queue behind it");
   fake_ack_out(pad);
   EVENTUALLY(fake_snapshot().nsent == 2);
   fake_ack_out(pad);
   EVENTUALLY(fake_snapshot().nsent == 3);
   fake_ack_out(pad);
   EVENTUALLY(fake_in_flight(pad, 0) == 0);
   check(fake_snapshot().nsent == 3
      && fake_snapshot().sent[0][0] == 0xA1
      && fake_snapshot().sent[1][0] == 0xB2
      && fake_snapshot().sent[2][0] == 0xC3, "all three are sent, in order");
   check(fake_snapshot().max_outs_in_flight == 1, "one at a time");

   printf("unplug\n");
   fake_unplug(pad);
   EVENTUALLY(fake_snapshot().closes == 1 && fake_snapshot().frees == 2);
   check(fake_snapshot().closes == 1, "the handle is closed");
   check(fake_snapshot().frees == 2, "both transfers are freed");
   check(retro_atomic_load_acquire_int(&disconnects) == 1, "the pad is disconnected");
   check(retro_atomic_load_acquire_int(&deinits) == 1, "its slot is released");

   printf("free with a pad attached and a send pending\n");
   fake_plug(pad);
   EVENTUALLY(fake_snapshot().opens == 2 && fake_in_flight(pad, 1) == 1);
   check(fake_snapshot().opens == 2 && fake_in_flight(pad, 1) == 1, "the pad comes back");
   libusb_hid.send_control(last_device, cmd[0], 2);
   check(fake_in_flight(pad, 0) == 1, "a send is left pending");
   t0 = now_ms();
   libusb_hid.free(hid);
   took = now_ms() - t0;
   check(took < 100, "free returns at once");
   check(fake_snapshot().closes == 2, "the handle is closed");
   check(fake_snapshot().frees == fake_snapshot().allocs, "every transfer is freed");
   check(fake_snapshot().freed_in_flight == 0, "none while in flight");
   check(fake_snapshot().double_submits == 0, "no transfer submitted twice");
   check(fake_snapshot().exits == 1, "libusb is shut down");
   check(retro_atomic_load_acquire_int(&deinits) == 2, "the slot is released");

   if (failures)
   {
      printf("\n%u failure(s)\n", failures);
      return 1;
   }
   printf("\nall passed\n");
   return 0;
}
