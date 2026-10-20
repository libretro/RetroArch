/* Regression test for input/common/hid_report_desc.h: a gamepad's
 * reports read from the layout its HID report descriptor gives.
 *
 *   dragonrise  -> a common USB pad's descriptor: X named four times
 *                  and Y once, so Y keeps its axis and the three more
 *                  Xs take the free ones after, as on the Mac; a hat
 *                  of eight ways and its rest; twelve buttons by
 *                  usage; the vendor byte and the output report left
 *                  alone
 *   ids         -> reports with IDs: a field read only from its own
 *                  report, a signed axis, a hat counted from 1, a
 *                  four-way hat
 *   not a pad   -> a keyboard's descriptor has nothing to read
 *   truncated   -> a descriptor cut short, a report too short: nothing
 *                  read past either */

#include <stdio.h>
#include <string.h>

#include "../../../input/common/hid_report_desc.h"

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL %s:%d: %s\n", \
   __FILE__, __LINE__, m); failures++; } } while (0)

/* DragonRise Inc. Generic USB Joystick (0079:0006) */
static const uint8_t dragonrise[] = {
   0x05, 0x01, 0x09, 0x04, 0xa1, 0x01, 0xa1, 0x02, 0x75, 0x08, 0x95, 0x05,
   0x15, 0x00, 0x26, 0xff, 0x00, 0x35, 0x00, 0x46, 0xff, 0x00, 0x09, 0x30,
   0x09, 0x30, 0x09, 0x30, 0x09, 0x30, 0x09, 0x31, 0x81, 0x02, 0x75, 0x04,
   0x95, 0x01, 0x25, 0x07, 0x46, 0x3b, 0x01, 0x65, 0x14, 0x09, 0x39, 0x81,
   0x42, 0x65, 0x00, 0x75, 0x01, 0x95, 0x0c, 0x25, 0x01, 0x45, 0x01, 0x05,
   0x09, 0x19, 0x01, 0x29, 0x0c, 0x81, 0x02, 0x06, 0x00, 0xff, 0x75, 0x01,
   0x95, 0x08, 0x25, 0x01, 0x45, 0x01, 0x09, 0x01, 0x81, 0x02, 0xc0, 0xa1,
   0x02, 0x75, 0x08, 0x95, 0x07, 0x46, 0xff, 0x00, 0x26, 0xff, 0x00, 0x09,
   0x02, 0x91, 0x02, 0xc0, 0xc0
};

/* A pad whose state is in two reports: report 1 with a signed X and a
 * hat counted from 1, report 2 with four buttons and a four-way hat
 * (which the first hat leaves unread). */
static const uint8_t two_reports[] = {
   0x05, 0x01, 0x09, 0x05, 0xa1, 0x01,
   0x85, 0x01,
   0x09, 0x30, 0x15, 0x81, 0x25, 0x7f, 0x75, 0x08, 0x95, 0x01, 0x81, 0x02,
   0x09, 0x39, 0x15, 0x01, 0x25, 0x08, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42,
   0x75, 0x04, 0x95, 0x01, 0x81, 0x03,
   0x85, 0x02,
   0x05, 0x09, 0x19, 0x01, 0x29, 0x04, 0x15, 0x00, 0x25, 0x01,
   0x75, 0x01, 0x95, 0x04, 0x81, 0x02,
   0x75, 0x04, 0x95, 0x01, 0x81, 0x03,
   0xc0
};

/* a keyboard */
static const uint8_t keyboard[] = {
   0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7,
   0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01,
   0x75, 0x08, 0x81, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
   0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xc0
};

static void test_dragonrise(void)
{
   hid_desc_layout_t l;
   hid_desc_state_t st;
   uint8_t r[8];
   int had = failures;

   CHECK(hid_desc_parse(&l, dragonrise, sizeof(dragonrise)),
         "dragonrise: no pad found");
   CHECK(!l.ids, "dragonrise: reports said to have IDs");
   memset(&st, 0, sizeof(st));
   /* X1..X4, Y; hat right, button 1 and 4; button 12; vendor */
   r[0] = 0x00; r[1] = 0x40; r[2] = 0x80; r[3] = 0xc0; r[4] = 0xff;
   r[5] = 0x02 | (0x01 << 4) | (0x08 << 4);
   r[6] = 0x80;
   r[7] = 0xff;
   hid_desc_decode(&l, r, sizeof(r), &st);
   CHECK(st.axes[0] == -32767 && st.axes[1] == 32767,
         "dragonrise: X and Y are not axes 0 and 1");
   CHECK(st.axes[2] < -16000 && st.axes[2] > -16600
         && st.axes[3] > -200 && st.axes[3] < 200
         && st.axes[4] > 16000,
         "dragonrise: the Xs named again are not axes 2, 3 and 4 in order");
   CHECK(st.hat_x == 1 && st.hat_y == 0, "dragonrise: the hat is not right");
   CHECK(st.buttons == ((1u << 0) | (1u << 3) | (1u << 11)),
         "dragonrise: the buttons are not numbered by usage");
   /* the hat at rest: past its range */
   r[5] = 0x0f;
   r[6] = 0x00;
   hid_desc_decode(&l, r, sizeof(r), &st);
   CHECK(st.hat_x == 0 && st.hat_y == 0 && st.buttons == 0,
         "dragonrise: the hat at rest or a button let go is still read");
   if (failures == had)
      printf("   ok   dragonrise: axes as on the Mac, hat, buttons by usage\n");
}

static void test_ids(void)
{
   hid_desc_layout_t l;
   hid_desc_state_t st;
   uint8_t r1[3], r2[2];
   int had = failures;

   CHECK(hid_desc_parse(&l, two_reports, sizeof(two_reports)),
         "ids: no pad found");
   CHECK(l.ids, "ids: reports not said to have IDs");
   memset(&st, 0, sizeof(st));
   r1[0] = 0x01; r1[1] = 0x81; r1[2] = 0x03; /* X -127, hat 3: right */
   hid_desc_decode(&l, r1, sizeof(r1), &st);
   CHECK(st.axes[0] == -32767, "ids: a signed axis is misread");
   CHECK(st.hat_x == 1 && st.hat_y == 0, "ids: a hat counted from 1 is misread");
   r2[0] = 0x02; r2[1] = 0x05; /* buttons 1 and 3 */
   hid_desc_decode(&l, r2, sizeof(r2), &st);
   CHECK(st.buttons == 0x05 && st.axes[0] == -32767 && st.hat_x == 1,
         "ids: a report changed fields of another");
   r1[1] = 0x7f; r1[2] = 0x00; /* X +127, hat at rest (0, below 1) */
   hid_desc_decode(&l, r1, sizeof(r1), &st);
   CHECK(st.axes[0] == 32767 && st.hat_x == 0 && st.buttons == 0x05,
         "ids: report 1 again is not read, or touched report 2's");
   if (failures == had)
      printf("   ok   ids: each report its own fields, signed, a hat from 1\n");
}

static void test_edges(void)
{
   hid_desc_layout_t l;
   hid_desc_state_t st;
   uint8_t r[8];
   size_t n;
   int had = failures;

   CHECK(!hid_desc_parse(&l, keyboard, sizeof(keyboard)),
         "a keyboard was taken for a pad");
   /* every cut of the descriptor parses without reading past it */
   for (n = 0; n < sizeof(dragonrise); n++)
      hid_desc_parse(&l, dragonrise, n);
   /* a short report leaves what it does not reach */
   hid_desc_parse(&l, dragonrise, sizeof(dragonrise));
   memset(&st, 0, sizeof(st));
   st.buttons = 0x800;
   memset(r, 0, sizeof(r));
   r[0] = 0xff;
   hid_desc_decode(&l, r, 2, &st);
   CHECK(st.axes[0] == 32767 && st.buttons == 0x800,
         "a short report was read past its end");
   if (failures == had)
      printf("   ok   a keyboard is no pad; nothing read past a cut descriptor or a short report\n");
}

int main(void)
{
   test_dragonrise();
   test_ids();
   test_edges();
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("[pass] hid_report_desc_test\n");
   return 0;
}
