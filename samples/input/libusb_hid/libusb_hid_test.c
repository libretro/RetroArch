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
 *   two senders       -> the frontend and a pad handler on the
 *                        event-handling thread send at once: every
 *                        command goes out, each sender's in its order,
 *                        one at a time
 *   unplug mid-send   -> an adapter is not freed while a sender is
 *                        still inside it, with its send in flight or
 *                        with none, and is freed once it leaves
 *
 * Nothing in the driver takes a lock: the senders hand the one OUT
 * transfer between them with an atomic claim. */

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
/* Set: a sender that finds the queue full stops in the warning until
 * it is cleared - a sender held inside the adapter with no send of
 * its own in flight. */
static retro_atomic_int_t block_full_warning = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t warning_blocked    = RETRO_ATOMIC_INT_INITIALIZER(0);
void RARCH_WARN(const char *f, ...)
{
   if (     strstr(f, "buffer is full")
         && retro_atomic_load_acquire_int(&block_full_warning))
   {
      retro_atomic_store_release_int(&warning_blocked, 1);
      while (retro_atomic_load_acquire_int(&block_full_warning))
         usleep(200);
   }
}
void RARCH_ERR(const char *f, ...)  { (void)f; }


static retro_atomic_int_t packets     = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t last_byte   = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t disconnects = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t deinits     = RETRO_ATOMIC_INT_INITIALIZER(0);
static bool slot_used[16];
/* What the driver hands pad_connection_pad_init() as the device: the
 * adapter, which is what send_control() takes back. */
static void *last_device;
/* Set: the pad handler answers each report with a command, from the
 * event-handling thread, as the PS4 handler does. */
static retro_atomic_int_t send_on_packet = RETRO_ATOMIC_INT_INITIALIZER(0);
static unsigned char packet_cmds;

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
   if (retro_atomic_load_acquire_int(&send_on_packet))
   {
      uint8_t cmd[2];
      cmd[0] = 0x20;
      cmd[1] = packet_cmds++;
      libusb_hid.send_control(last_device, cmd, 2);
   }
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

#define FRONTEND_CMDS 200
#define PACKET_CMDS   100

static struct libusb_device *the_pad;
static retro_atomic_int_t acking;

/* The pad taking each command as it comes. */
static void *acker(void *unused)
{
   (void)unused;
   while (retro_atomic_load_acquire_int(&acking))
   {
      fake_ack_out(the_pad);
      usleep(50);
   }
   return NULL;
}

/* The frontend: rumble and LEDs. */
static void *frontend_sender(void *unused)
{
   unsigned i;
   (void)unused;
   for (i = 0; i < FRONTEND_CMDS; i++)
   {
      uint8_t cmd[2];
      cmd[0] = 0x10;
      cmd[1] = (uint8_t)i;
      libusb_hid.send_control(last_device, cmd, 2);
      if (!(i & 7))
         usleep(100);
   }
   return NULL;
}

#define HANDOFF_ROUNDS 3000
static retro_atomic_int_t go;

static void *racing_sender(void *unused)
{
   uint8_t cmd[2] = { 0x60, 0 };
   (void)unused;
   while (!retro_atomic_load_acquire_int(&go));
   libusb_hid.send_control(last_device, cmd, 2);
   return NULL;
}

static void *one_send(void *unused)
{
   uint8_t cmd[2] = { 0x30, 0 };
   (void)unused;
   libusb_hid.send_control(last_device, cmd, 2);
   return NULL;
}

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

   printf("two senders\n");
   {
      pthread_t ack_t, send_t;
      unsigned i, next_f = 0, next_p = 0, other = 0, out_of_order = 0;
      int base = fake_snapshot().nlog;
      struct fake_stats st;

      the_pad = pad;
      retro_atomic_store_release_int(&acking, 1);
      retro_atomic_store_release_int(&send_on_packet, 1);
      pthread_create(&ack_t, NULL, acker, NULL);
      pthread_create(&send_t, NULL, frontend_sender, NULL);
      for (i = 0; i < PACKET_CMDS; i++)
      {
         EVENTUALLY(fake_in_flight(pad, 1) == 1);
         fake_report(pad, rep, 4);
         EVENTUALLY(retro_atomic_load_acquire_int(&packets) == (int)(i + 2));
      }
      pthread_join(send_t, NULL);
      retro_atomic_store_release_int(&send_on_packet, 0);
      EVENTUALLY(fake_snapshot().nlog == base + FRONTEND_CMDS + PACKET_CMDS
            && fake_in_flight(pad, 0) == 0);
      retro_atomic_store_release_int(&acking, 0);
      pthread_join(ack_t, NULL);

      st = fake_snapshot();
      for (i = base; i < (unsigned)st.nlog; i++)
      {
         if (st.log[i][0] == 0x10)
         {
            if (st.log[i][1] != (unsigned char)next_f)
               out_of_order++;
            next_f = st.log[i][1] + 1;
         }
         else if (st.log[i][0] == 0x20)
         {
            if (st.log[i][1] != (unsigned char)next_p)
               out_of_order++;
            next_p = st.log[i][1] + 1;
         }
         else
            other++;
      }
      check(st.nlog - base == FRONTEND_CMDS + PACKET_CMDS,
            "every command from both senders is sent");
      check(!out_of_order && !other, "each sender's commands go out in its order");
      check(st.max_outs_in_flight == 1, "still one at a time");
      check(st.double_submits == 0, "no transfer submitted twice");
   }

   printf("hand-offs\n");
   {
      /* Two senders racing the holder's letting go of the transfer,
       * round after round: a command pushed as it lets go must still
       * go out, with no later one to carry it. */
      pthread_t ack_t, send_t;
      unsigned  round, stranded = 0;

      the_pad = pad;
      retro_atomic_store_release_int(&acking, 1);
      pthread_create(&ack_t, NULL, acker, NULL);
      for (round = 0; round < HANDOFF_ROUNDS && !stranded; round++)
      {
         int    want = fake_snapshot().nout + 2;
         double t0_;
         retro_atomic_store_release_int(&go, 0);
         pthread_create(&send_t, NULL, racing_sender, NULL);
         retro_atomic_store_release_int(&go, 1);
         {
            uint8_t cmd2[2] = { 0x50, 0 };
            libusb_hid.send_control(last_device, cmd2, 2);
         }
         pthread_join(send_t, NULL);
         t0_ = now_ms();
         while (fake_snapshot().nout < want && now_ms() - t0_ < 500)
            usleep(100);
         if (fake_snapshot().nout < want)
            stranded++;
      }
      EVENTUALLY(fake_in_flight(pad, 0) == 0);
      retro_atomic_store_release_int(&acking, 0);
      pthread_join(ack_t, NULL);
      check(!stranded, "no command is left behind by a hand-off");
      check(fake_in_flight(pad, 0) == 0, "the last one is taken");
   }

   printf("unplug with a sender inside\n");
   {
      pthread_t send_t;
      fake_hold_out_submits(1);
      pthread_create(&send_t, NULL, one_send, NULL);
      EVENTUALLY(fake_snapshot().gated == 1);
      check(fake_snapshot().gated == 1, "a sender is inside, submitting");
      fake_unplug(pad);
      EVENTUALLY(retro_atomic_load_acquire_int(&deinits) == 1);
      usleep(50000);
      check(fake_snapshot().closes == 0,
            "the adapter is not freed with the sender inside");
      fake_hold_out_submits(0);
      pthread_join(send_t, NULL);
   }

   printf("unplug\n");
   EVENTUALLY(fake_snapshot().closes == 1 && fake_snapshot().frees == 2);
   check(fake_snapshot().closes == 1, "the handle is closed");
   check(fake_snapshot().frees == 2, "both transfers are freed");
   check(retro_atomic_load_acquire_int(&disconnects) == 1, "the pad is disconnected");
   check(retro_atomic_load_acquire_int(&deinits) == 1, "its slot is released");

   printf("unplug with a sender inside and no send in flight\n");
   fake_plug(pad);
   EVENTUALLY(fake_snapshot().opens == 2 && fake_in_flight(pad, 1) == 1);
   {
      pthread_t a_t, b_t, ack_t;
      uint8_t   fill[2] = { 0x40, 0 };
      int       i, base = fake_snapshot().nlog;
      double    t0_;

      /* A takes the OUT transfer and is held submitting; the queue
       * behind it fills; B finds it full and is held there. */
      fake_hold_out_submits(1);
      pthread_create(&a_t, NULL, one_send, NULL);
      EVENTUALLY(fake_snapshot().gated == 1);
      for (i = 0; i < 2048; i++)
         libusb_hid.send_control(last_device, fill, 2);
      retro_atomic_store_release_int(&block_full_warning, 1);
      pthread_create(&b_t, NULL, one_send, NULL);
      EVENTUALLY(retro_atomic_load_acquire_int(&warning_blocked) == 1);
      check(retro_atomic_load_acquire_int(&warning_blocked) == 1,
            "a sender is inside, finding the queue full");

      /* Everything queued goes out; B is still inside. */
      fake_hold_out_submits(0);
      the_pad = pad;
      retro_atomic_store_release_int(&acking, 1);
      pthread_create(&ack_t, NULL, acker, NULL);
      t0_ = now_ms();
      while (!(fake_snapshot().nlog == base + 2049 && fake_in_flight(pad, 0) == 0)
            && now_ms() - t0_ < 20000)
         usleep(1000);
      retro_atomic_store_release_int(&acking, 0);
      pthread_join(ack_t, NULL);
      pthread_join(a_t, NULL);
      check(fake_snapshot().nlog == base + 2049, "the queue drains");

      fake_unplug(pad);
      EVENTUALLY(retro_atomic_load_acquire_int(&deinits) == 2);
      usleep(50000);
      check(fake_snapshot().closes == 1,
            "the adapter is not freed with a sender inside and nothing in flight");
      retro_atomic_store_release_int(&block_full_warning, 0);
      pthread_join(b_t, NULL);
      EVENTUALLY(fake_snapshot().closes == 2);
      check(fake_snapshot().closes == 2, "it is freed once the sender leaves");
   }

   printf("free with a pad attached and a send pending\n");
   fake_plug(pad);
   EVENTUALLY(fake_snapshot().opens == 3 && fake_in_flight(pad, 1) == 1);
   check(fake_snapshot().opens == 3 && fake_in_flight(pad, 1) == 1, "the pad comes back");
   libusb_hid.send_control(last_device, cmd[0], 2);
   check(fake_in_flight(pad, 0) == 1, "a send is left pending");
   t0 = now_ms();
   libusb_hid.free(hid);
   took = now_ms() - t0;
   check(took < 100, "free returns at once");
   check(fake_snapshot().closes == 3, "the handle is closed");
   check(fake_snapshot().frees == fake_snapshot().allocs, "every transfer is freed");
   check(fake_snapshot().freed_in_flight == 0, "none while in flight");
   check(fake_snapshot().double_submits == 0, "no transfer submitted twice");
   check(fake_snapshot().exits == 1, "libusb is shut down");
   check(retro_atomic_load_acquire_int(&deinits) == 3, "the slot is released");

   if (failures)
   {
      printf("\n%u failure(s)\n", failures);
      return 1;
   }
   printf("\nall passed\n");
   return 0;
}
