/* The rumble reports of the DualShock 4 and the DualSense:
 * input/common/sony_pad_output.h, on its own.
 *
 * No pad is written to. What is held is what can be held without one:
 * that each report has the id, the length and the layout its pad
 * expects, with the motors where they belong and no flag set but the
 * ones for rumble, and that a Bluetooth report ends in the CRC the pad
 * checks. */
#include <stdio.h>
#include <string.h>

#include "../../../input/common/sony_pad_output.h"

static unsigned failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

/* every byte but the ones named is zero */
static unsigned others_set(const uint8_t *r, size_t len, const unsigned *named, unsigned n)
{
   size_t i;
   unsigned k, set = 0;
   for (i = 0; i < len; i++)
   {
      for (k = 0; k < n; k++)
         if (named[k] == i)
            break;
      if (k == n && r[i])
         set++;
   }
   return set;
}

int main(void)
{
   uint8_t r[128];
   size_t len;

   /* ---- which pad ------------------------------------------------- */
   CHECK(sony_pad_model(0x054C, 0x0DF2) == SONY_PAD_DUALSENSE
         && sony_pad_model(0x054C, 0x0CE6) == SONY_PAD_DUALSENSE
         && sony_pad_model(0x054C, 0x09CC) == SONY_PAD_DS4
         && sony_pad_model(0x054C, 0x05C4) == SONY_PAD_DS4
         && sony_pad_model(0x054C, 0x0268) == SONY_PAD_NONE
         && sony_pad_model(0x045E, 0x0DF2) == SONY_PAD_NONE,
         "the pads are not told apart by their ids");
   CHECK(sony_pad_dualsense_is_edge(0x0DF2) && !sony_pad_dualsense_is_edge(0x0CE6),
         "the Edge is not told from the DualSense");
   printf("   ok   the pads are known by their ids, and nothing else is taken for one\n");

   /* ---- the CRC --------------------------------------------------- */
   /* CRC-32 of "123456789" is 0xCBF43926; with the unsent 0xA2 in
    * front it is the CRC-32 of those ten bytes, which a table-free
    * run of the same polynomial gives here */
   {
      static const uint8_t msg[] = { 0xA2, '1', '2', '3', '4', '5', '6', '7', '8', '9' };
      uint32_t ref = 0xFFFFFFFFu;
      unsigned i, b;
      for (i = 0; i < sizeof(msg); i++)
      {
         ref ^= msg[i];
         for (b = 0; b < 8; b++)
            ref = (ref & 1) ? (ref >> 1) ^ 0xEDB88320u : ref >> 1;
      }
      ref = ~ref;
      CHECK(sony_pad_crc32(msg + 1, 9) == ref,
            "the CRC with its unsent first byte: %08x, want %08x",
            (unsigned)sony_pad_crc32(msg + 1, 9), (unsigned)ref);
      /* and the polynomial is the one everyone means by CRC-32 */
      ref = 0xFFFFFFFFu;
      for (i = 1; i < sizeof(msg); i++)
      {
         ref ^= msg[i];
         for (b = 0; b < 8; b++)
            ref = (ref & 1) ? (ref >> 1) ^ 0xEDB88320u : ref >> 1;
      }
      CHECK(~ref == 0xCBF43926u, "CRC-32 of \"123456789\" is %08x", (unsigned)~ref);
   }
   printf("   ok   the Bluetooth CRC is CRC-32 over an unsent 0xA2 and the report\n");

   /* ---- DualSense, USB -------------------------------------------- */
   {
      static const unsigned v2[] = { 0, 1, 3, 4, 39 };
      static const unsigned v1[] = { 0, 1, 3, 4 };
      len = sony_pad_rumble_report(r, sizeof(r), SONY_PAD_DUALSENSE, false, true, 200, 100);
      CHECK(len == 48 && r[0] == 0x02 && r[1] == 0x02 && r[2] == 0
            && r[3] == 100 && r[4] == 200 && r[39] == 0x04,
            "DualSense, USB, newer way: length %u, id %02x, flags %02x %02x .. %02x, motors %u %u",
            (unsigned)len, r[0], r[1], r[2], r[39], r[3], r[4]);
      CHECK(!others_set(r, len, v2, 5), "DualSense, USB: a byte is set that asks for something else");
      len = sony_pad_rumble_report(r, sizeof(r), SONY_PAD_DUALSENSE, false, false, 200, 100);
      CHECK(len == 48 && r[1] == 0x03 && r[39] == 0 && r[3] == 100 && r[4] == 200,
            "DualSense, USB, older way: flags %02x .. %02x", r[1], r[39]);
      CHECK(!others_set(r, len, v1, 4), "DualSense, USB, older way: a byte is set that asks for something else");
   }
   /* ---- DualSense, Bluetooth -------------------------------------- */
   {
      static const unsigned named[] = { 0, 1, 2, 4, 5, 40, 74, 75, 76, 77 };
      uint32_t crc;
      len = sony_pad_rumble_report(r, sizeof(r), SONY_PAD_DUALSENSE, true, true, 200, 100);
      crc = sony_pad_crc32(r, 74);
      CHECK(len == 78 && r[0] == 0x31 && r[1] == 0x02 && r[2] == 0x02
            && r[4] == 100 && r[5] == 200 && r[40] == 0x04,
            "DualSense, Bluetooth: length %u, id %02x %02x, flags %02x .. %02x, motors %u %u",
            (unsigned)len, r[0], r[1], r[2], r[40], r[4], r[5]);
      CHECK(r[74] == (crc & 0xFF) && r[75] == ((crc >> 8) & 0xFF)
            && r[76] == ((crc >> 16) & 0xFF) && r[77] == (crc >> 24),
            "DualSense, Bluetooth: the report does not end in its CRC, low byte first");
      CHECK(!others_set(r, len, named, 10), "DualSense, Bluetooth: a byte is set that asks for something else");
   }
   printf("   ok   DualSense: USB and Bluetooth, the newer way and the older; only the motors are asked for\n");

   /* ---- DualShock 4 ----------------------------------------------- */
   {
      static const unsigned usb[] = { 0, 1, 4, 5 };
      static const unsigned bt[]  = { 0, 1, 3, 6, 7, 74, 75, 76, 77 };
      uint32_t crc;
      len = sony_pad_rumble_report(r, sizeof(r), SONY_PAD_DS4, false, false, 200, 100);
      CHECK(len == 32 && r[0] == 0x05 && r[1] == 0x01 && r[4] == 100 && r[5] == 200,
            "DualShock 4, USB: length %u, id %02x, flags %02x, motors %u %u",
            (unsigned)len, r[0], r[1], r[4], r[5]);
      CHECK(!others_set(r, len, usb, 4), "DualShock 4, USB: a byte is set that asks for something else");
      len = sony_pad_rumble_report(r, sizeof(r), SONY_PAD_DS4, true, false, 200, 100);
      crc = sony_pad_crc32(r, 74);
      CHECK(len == 78 && r[0] == 0x11 && r[1] == 0xC0 && r[3] == 0x01
            && r[6] == 100 && r[7] == 200,
            "DualShock 4, Bluetooth: length %u, id %02x %02x, flags %02x, motors %u %u",
            (unsigned)len, r[0], r[1], r[3], r[6], r[7]);
      CHECK(r[74] == (crc & 0xFF) && r[77] == (crc >> 24),
            "DualShock 4, Bluetooth: the report does not end in its CRC");
      CHECK(!others_set(r, len, bt, 9), "DualShock 4, Bluetooth: a byte is set that asks for something else");
   }
   printf("   ok   DualShock 4: USB and Bluetooth; only the motors are asked for\n");

   /* ---- what there is no report for ------------------------------- */
   /* the lights, when a player is given: beside the motors, and only
    * then */
   len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DUALSENSE, false, true, 200, 100, 1);
   CHECK(len == 48 && (r[2] & 0x10) && r[44] == 0x04 && r[3] == 100 && r[4] == 200,
         "DualSense, USB, player 1: flags %02x lights %02x", r[2], r[44]);
   len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DUALSENSE, false, true, 0, 0, 3);
   CHECK(r[44] == 0x15, "DualSense, USB, player 3: lights %02x, want 15", r[44]);
   len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DUALSENSE, false, true, 0, 0, 5);
   CHECK(r[44] == 0x1F, "DualSense, USB, player 5: lights %02x, want 1f", r[44]);
   len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DUALSENSE, true, true, 0, 0, 2);
   {
      uint32_t crc = sony_pad_crc32(r, 74);
      CHECK(len == 78 && (r[3] & 0x10) && r[45] == 0x0A && r[74] == (crc & 0xFF) && r[77] == (crc >> 24),
            "DualSense, Bluetooth, player 2: flags %02x lights %02x, or the CRC", r[3], r[45]);
   }
   len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DS4, false, false, 200, 100, 2);
   CHECK(len == 32 && r[1] == 0x03 && r[6] == 0x40 && r[7] == 0 && r[8] == 0 && r[4] == 100 && r[5] == 200,
         "DualShock 4, USB, player 2: flags %02x colour %02x %02x %02x", r[1], r[6], r[7], r[8]);
   len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DS4, true, false, 0, 0, 1);
   CHECK(len == 78 && r[3] == 0x03 && r[8] == 0 && r[9] == 0 && r[10] == 0x40,
         "DualShock 4, Bluetooth, player 1: flags %02x colour %02x %02x %02x", r[3], r[8], r[9], r[10]);
   len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DS4, false, false, 0, 0, 4);
   CHECK(r[6] == 0x20 && r[7] == 0 && r[8] == 0x20, "DualShock 4, player 4 is not pink");
   len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DUALSENSE, false, true, 200, 100, 0);
   CHECK(!(r[2] & 0x10) && r[44] == 0, "with no player given the lights are asked for");
   /* put out: asked for, with nothing lit; the motors still beside */
   len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DUALSENSE, false, true, 200, 100, -1);
   CHECK((r[2] & 0x10) && r[44] == 0 && r[3] == 100 && r[4] == 200,
         "DualSense, lights put out: flags %02x lights %02x", r[2], r[44]);
   len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DUALSENSE, true, true, 0, 0, -1);
   CHECK((r[3] & 0x10) && r[45] == 0, "DualSense, Bluetooth, lights put out");
   len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DS4, false, false, 0, 0, -1);
   CHECK(r[1] == 0x03 && !r[6] && !r[7] && !r[8], "DualShock 4, light bar put out");
   len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DS4, true, false, 0, 0, -1);
   CHECK(r[3] == 0x03 && !r[8] && !r[9] && !r[10], "DualShock 4, Bluetooth, light bar put out");
   printf("   ok   a player given: the DualSense's player lights and the DualShock 4's light bar, beside the motors; none given, the lights are left alone; less than none, they are put out\n");

   CHECK(sony_pad_rumble_report(r, sizeof(r), SONY_PAD_NONE, false, false, 1, 1) == 0,
         "a report was built for no pad");
   CHECK(sony_pad_rumble_report(r, 40, SONY_PAD_DUALSENSE, false, true, 1, 1) == 0,
         "a report was built into a buffer too small for any");
   /* motors off is a report too: it is what stops them */
   len = sony_pad_rumble_report(r, sizeof(r), SONY_PAD_DUALSENSE, false, true, 0, 0);
   CHECK(len == 48 && r[3] == 0 && r[4] == 0 && r[1] == 0x02 && r[39] == 0x04,
         "motors off is not a report with the rumble flags set and both at zero");
   printf("   ok   no report for no pad or too small a buffer; motors off is a report\n");

   if (failures)
   {
      printf("FAIL sony_pad_output_test: %u\n", failures);
      return 1;
   }
   printf("PASS sony_pad_output_test\n");
   return 0;
}
