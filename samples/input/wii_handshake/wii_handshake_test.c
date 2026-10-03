/* Regression test for the Wiimote extension handshake in
 * input/connect/connect_wii.c.
 *
 * The extension-init sequence slept 100 ms after each of its two
 * register writes and before reading the calibration, on the thread
 * that delivered the packet: the main thread under iohidmanager, and
 * libusb's single event thread shared by every pad. It now sends each
 * write and goes on when the Wiimote acknowledges it (report 0x22),
 * with a fallback for a remote that never does.
 *
 * The contract this pins, by feeding reports and capturing what the
 * handshake sends:
 *
 *   status with an attachment -> first init write, and nothing else
 *   its acknowledgement       -> second init write
 *   that acknowledgement      -> the extension id read
 *   classic controller id     -> the calibration read, at once
 *   no acknowledgement        -> a later packet moves it on, but only
 *                                once the old 100 ms have passed
 *   and no call takes more than a few milliseconds
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "input/connect/connect_wii.c"

/* ---- capture what the handshake sends ---- */

static uint8_t sent[32][32];
static int     nsent;

static void capture(void *handle, uint8_t *s, size_t len)
{
   if (nsent < 32)
      memcpy(sent[nsent++], s, len < 32 ? len : 32);
}

static hid_driver_t fake_driver;

/* sent[i][1] is the output report; for 0x16/0x17 bytes 2-5 are the
 * big-endian address and, for writes, byte 7 the first data byte. */
static uint32_t sent_addr(int i)
{
   return ((uint32_t)sent[i][2] << 24) | ((uint32_t)sent[i][3] << 16)
        | ((uint32_t)sent[i][4] << 8)  |  (uint32_t)sent[i][5];
}

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

static double worst_ms;

static void feed(void *dev, uint8_t *pkt, uint16_t len)
{
   double t0 = now_ms(), took;
   pad_connection_wii.packet_handler(dev, pkt, len);
   took = now_ms() - t0;
   if (took > worst_ms)
      worst_ms = took;
}

static void *new_remote(int slot)
{
   /* The connection is only handed back to send_control() as a
    * handle, which the capture ignores. */
   static char conn[64];
   nsent = 0;
   return pad_connection_wii.init(conn, slot, &fake_driver);
}

int main(void)
{
   uint8_t status[8]  = { WM_RPT_CTRL_STATUS, 0, 0, WM_CTRL_STATUS_BYTE1_ATTACHMENT, 0, 0, 0, 0 };
   uint8_t ack[6]     = { WM_RPT_WRITE, 0, 0, WM_CMD_WRITE_DATA, 0, 0 };
   uint8_t btn[4]     = { WM_RPT_BTN, 0, 0, 0 };
   uint8_t id_cc[22]  = { WM_RPT_READ, 0, 0, 0x30, 0, 0xFA, 0xA4, 0x20, 0x01, 0x01 };
   void *wm;
   int base;

   fake_driver.send_control = capture;

   printf("acknowledged writes\n");
   wm = new_remote(0);
   check(wm != NULL, "remote initialises");
   base = nsent;
   feed(wm, status, sizeof(status));
   check(nsent == base + 1 && sent[base][1] == WM_CMD_WRITE_DATA
      && sent_addr(base) == 0x04A400F0 && sent[base][7] == 0x55,
         "status with an attachment sends the first init write only");
   feed(wm, ack, sizeof(ack));
   check(nsent == base + 2 && sent[base + 1][1] == WM_CMD_WRITE_DATA
      && sent_addr(base + 1) == 0x04A400FB && sent[base + 1][7] == 0x00,
         "its acknowledgement sends the second");
   feed(wm, ack, sizeof(ack));
   check(nsent == base + 3 && sent[base + 2][1] == WM_CMD_READ_DATA
      && sent_addr(base + 2) == WM_EXP_MEM_CALIBR + 220,
         "that acknowledgement reads the extension id");
   feed(wm, id_cc, sizeof(id_cc));
   check(nsent == base + 4 && sent[base + 3][1] == WM_CMD_READ_DATA
      && sent_addr(base + 3) == WM_EXP_MEM_CALIBR,
         "a classic controller id reads the calibration at once");
   pad_connection_wii.deinit(wm);

   printf("an acknowledgement that never comes\n");
   wm   = new_remote(1);
   base = nsent;
   feed(wm, status, sizeof(status));
   feed(wm, btn, sizeof(btn));
   check(nsent == base + 1, "a packet within 100 ms does not move it on");
   usleep(110000);
   feed(wm, btn, sizeof(btn));
   check(nsent == base + 2 && sent_addr(base + 1) == 0x04A400FB,
         "a packet after 100 ms sends the second write");
   usleep(110000);
   feed(wm, btn, sizeof(btn));
   check(nsent == base + 3 && sent[base + 2][1] == WM_CMD_READ_DATA,
         "and then the id read");
   pad_connection_wii.deinit(wm);

   printf("an acknowledgement of something else\n");
   wm   = new_remote(2);
   base = nsent;
   feed(wm, status, sizeof(status));
   ack[3] = WM_CMD_LED;
   feed(wm, ack, sizeof(ack));
   check(nsent == base + 1, "is not taken for the write's");
   pad_connection_wii.deinit(wm);

   check(worst_ms < 5.0, "no packet blocks the thread that delivers it");

   if (failures)
   {
      printf("\n%u failure(s)\n", failures);
      return 1;
   }
   printf("\nall passed\n");
   return 0;
}
