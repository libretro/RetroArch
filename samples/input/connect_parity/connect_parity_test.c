/* The pad handlers in input/connect decode a report where it lies
 * and keep only what they answer from, where they used to copy the
 * report and decode it at every read. This holds each converted
 * handler to the copying one: a reference built from the old code - the
 * report copied into the old struct and read through it - and the real
 * handler are given the same reports, and every button, the whole set
 * and every axis must agree. Random reports, and the edges (every hat
 * value, every bit alone, a report too short to be one). The DualSense
 * and Switch Pro handlers are new, so they are held to the reports'
 * layout instead, and the Switch Pro's to its greeting and commands.
 * A handler that keeps its state as a view is held, after every report,
 * to its view saying what its getters say; the DualShock 3's to the
 * copying handler's axes. The 8BitDo SN30 Pro's and the original Xbox
 * pad's are new too, and held to their reports' layout and their rumble
 * reports. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "input/connect/joypad_connection.h"
#include "input/include/gamepad.h"
#include "input/common/sony_pad_output.h"
#include "input/common/sony_pad_motion.h"
#include <retro_timers.h>
#include "input/drivers_hid/winusb_bind.h"

/* shorter than the Wii U Pro's buttons, which end at 0x0E */
#define WIIUPRO_SHORT 0x0E

static unsigned failures;
#define CHECK(c, ...) do { if (!(c)) { printf("[FAIL] "); printf(__VA_ARGS__); \
   printf("\n"); failures++; } } while (0)

static uint8_t  sent[128];
static size_t   sent_len;
static unsigned sent_count;

/* every report's first bytes, by when it went */
static uint8_t  sent_head[64][12];

static void stub_send_control(void *handle, uint8_t *s, size_t len)
{
   (void)handle;
   sent_len = len < sizeof(sent) ? len : sizeof(sent);
   memcpy(sent, s, sent_len);
   memset(sent_head[sent_count % 64], 0, sizeof(sent_head[0]));
   memcpy(sent_head[sent_count % 64], s,
         sent_len < sizeof(sent_head[0]) ? sent_len : sizeof(sent_head[0]));
   sent_count++;
}

static hid_driver_t stub_hid;

void RARCH_LOG(const char *f, ...) { (void)f; }
void RARCH_DBG(const char *f, ...) { (void)f; }
void RARCH_ERR(const char *f, ...) { (void)f; }

static int32_t stub_set_report(void *handle, uint8_t type, uint8_t id,
      uint8_t *data, size_t len)
{
   (void)handle; (void)type; (void)id; (void)data;
   return (int32_t)len;
}

static int32_t stub_ds3_get_report(void *handle, uint8_t type, uint8_t id,
      uint8_t *data, size_t len)
{
   (void)handle; (void)type; (void)id;
   memset(data, 0, len);
   return (int32_t)len;
}

/* ---- a handler's view against its getters -------------------- */

static unsigned view_reads;
static unsigned view_bad;

/* the view's hat bit for up, down, left and right, in HAT_*_MASK order */
static const unsigned view_hat_bit[4] = { 0, 1, 2, 3 };

static void view_check(const pad_connection_interface_t *ifc, void *dev)
{
   const input_pad_view_t *v;
   input_bits_t bits;
   unsigned k;

   if (!ifc->view)
      return;
   v = ifc->view(dev);
   view_reads++;
   ifc->get_buttons(dev, &bits);
   if (memcmp(&bits, &v->buttons, sizeof(bits)))
      view_bad++;
   for (k = 0; k < 32; k++)
      if (!!ifc->button(dev, (uint16_t)k) != !!BIT256_GET(v->buttons, k))
         view_bad++;
   for (k = 0; k < INPUT_PAD_VIEW_AXES; k++)
      if (ifc->get_axis(dev, k) != v->axes[k])
         view_bad++;
   /* a handler with no hat answers none */
   for (k = 0; k < 4; k++)
      if (!!ifc->button(dev, (uint16_t)HAT_MAP(0, HAT_UP_MASK >> k))
            != !!(v->hats & (1u << view_hat_bit[k])))
         view_bad++;
}

static void deliver(const pad_connection_interface_t *ifc, void *dev,
      uint8_t *p, uint16_t len)
{
   ifc->packet_handler(dev, p, len);
   view_check(ifc, dev);
}

/* ---- PS4: the old handler's report and reads ------------------ */

struct ref_ps4
{
   uint8_t hatvalue[4];
   /* the old struct ps4buttons, little-end first */
   unsigned dpad : 4, square : 1, cross : 1, circle : 1, triangle : 1;
   unsigned l1 : 1, r1 : 1, l2 : 1, r2 : 1, share : 1, options : 1, l3 : 1, r3 : 1;
   unsigned ps : 1, touchpad : 1, reportcounter : 6;
   uint8_t trigger[2]; /* L2 and R2's travel, new */
};

static void ref_ps4_take(struct ref_ps4 *r, const uint8_t *p)
{
   memcpy(r->hatvalue, p + 1, 4);
   r->dpad     =  p[5]       & 15;
   r->square   = (p[5] >> 4) & 1;
   r->cross    = (p[5] >> 5) & 1;
   r->circle   = (p[5] >> 6) & 1;
   r->triangle = (p[5] >> 7) & 1;
   r->l1       =  p[6]       & 1;
   r->r1       = (p[6] >> 1) & 1;
   r->l2       = (p[6] >> 2) & 1;
   r->r2       = (p[6] >> 3) & 1;
   r->share    = (p[6] >> 4) & 1;
   r->options  = (p[6] >> 5) & 1;
   r->l3       = (p[6] >> 6) & 1;
   r->r3       = (p[6] >> 7) & 1;
   r->ps       =  p[7]       & 1;
   r->trigger[0] = p[8];
   r->trigger[1] = p[9];
}

static int ref_ps4_dpad(const struct ref_ps4 *r, unsigned id)
{
   unsigned d = r->dpad;
   switch (id)
   {
      case RETRO_DEVICE_ID_JOYPAD_UP:    return d == 7 || d == 0 || d == 1;
      case RETRO_DEVICE_ID_JOYPAD_RIGHT: return d == 1 || d == 2 || d == 3;
      case RETRO_DEVICE_ID_JOYPAD_DOWN:  return d == 3 || d == 4 || d == 5;
      case RETRO_DEVICE_ID_JOYPAD_LEFT:  return d == 5 || d == 6 || d == 7;
   }
   return 0;
}

static int ref_ps4_button(const struct ref_ps4 *r, unsigned k)
{
   switch (k)
   {
      case RETRO_DEVICE_ID_JOYPAD_R3:     return r->r3;
      case RETRO_DEVICE_ID_JOYPAD_L3:     return r->l3;
      case RETRO_DEVICE_ID_JOYPAD_START:  return r->options;
      case RETRO_DEVICE_ID_JOYPAD_SELECT: return r->share;
      case RETRO_DEVICE_ID_JOYPAD_R2:     return r->r2;
      case RETRO_DEVICE_ID_JOYPAD_L2:     return r->l2;
      case RETRO_DEVICE_ID_JOYPAD_R:      return r->r1;
      case RETRO_DEVICE_ID_JOYPAD_L:      return r->l1;
      case RETRO_DEVICE_ID_JOYPAD_X:      return r->triangle;
      case RETRO_DEVICE_ID_JOYPAD_A:      return r->circle;
      case RETRO_DEVICE_ID_JOYPAD_B:      return r->cross;
      case RETRO_DEVICE_ID_JOYPAD_Y:      return r->square;
      case RETRO_DEVICE_ID_JOYPAD_LEFT:
      case RETRO_DEVICE_ID_JOYPAD_RIGHT:
      case RETRO_DEVICE_ID_JOYPAD_DOWN:
      case RETRO_DEVICE_ID_JOYPAD_UP:     return ref_ps4_dpad(r, k);
   }
   return 0;
}

static int16_t ref_ps4_axis(const struct ref_ps4 *r, unsigned axis)
{
   int val;
   if (axis == 4 || axis == 5)
      return (int16_t)((r->trigger[axis - 4] << 7) | (r->trigger[axis - 4] >> 1));
   if (axis >= 4)
      return 0;
   val = (r->hatvalue[axis] << 8) - 0x8000;
   return abs(val) > 0x1000 ? (int16_t)val : 0;
}

static unsigned ps4_mismatches(void *dev, const struct ref_ps4 *r)
{
   const pad_connection_interface_t *ifc = &pad_connection_ps4;
   input_bits_t bits;
   unsigned k, bad = 0;
   ifc->get_buttons(dev, &bits);
   for (k = 0; k < RARCH_FIRST_CUSTOM_BIND; k++)
   {
      int want = ref_ps4_button(r, k);
      if (!!ifc->button(dev, (uint16_t)k) != want
            || !!BIT256_GET(bits, k) != want)
         bad++;
   }
   if (!!BIT256_GET(bits, RARCH_MENU_TOGGLE) != r->ps)
      bad++;
   for (k = 0; k < 6; k++)
      if (ifc->get_axis(dev, k) != ref_ps4_axis(r, k))
         bad++;
   return bad;
}

static bool stub_is_bluetooth(void *handle) { (void)handle; return true; }

static void test_ps4(void)
{
   uint8_t p[64];
   struct ref_ps4 ref;
   unsigned i, bad = 0, had = failures;
   const pad_connection_interface_t *ifc = &pad_connection_ps4;
   int dummy_connection;
   void *dev = ifc->init(&dummy_connection, 0, &stub_hid);

   CHECK(dev != NULL, "ps4: no device");
   if (!dev)
      return;
   memset(&ref, 0, sizeof(ref));
   srand(1);
   /* every hat value and every bit alone, then random reports */
   for (i = 0; i < 16 + 24 + 20000; i++)
   {
      unsigned j;
      for (j = 0; j < sizeof(p); j++)
         p[j] = (uint8_t)rand();
      p[0] = 0x01;
      if (i < 16)
         p[5] = (uint8_t)i;
      else if (i < 40)
      {
         p[5] = 8;
         p[6] = p[7] = 0;
         p[5 + (i - 16) / 8] |= (uint8_t)(1u << ((i - 16) % 8));
      }
      if ((i & 3) == 3)
      {
         /* woken over Bluetooth: report 0x11, two bytes further on */
         uint8_t bt[66];
         bt[0] = 0x11; bt[1] = 0xc0; bt[2] = 0;
         memcpy(bt + 3, p + 1, 63);
         deliver(ifc, dev, bt, 12);
      }
      else
         deliver(ifc, dev, p, 10);
      ref_ps4_take(&ref, p);
      bad += ps4_mismatches(dev, &ref);
   }
   CHECK(!bad, "ps4: %u reads disagree with the copying handler", bad);
   /* a report too short to be one leaves the pad as it was */
   memset(p, 0xff, sizeof(p));
   deliver(ifc, dev, p, 9);
   CHECK(!ps4_mismatches(dev, &ref), "ps4: a short report was taken");
   p[0] = 0x05;
   deliver(ifc, dev, p, 64);
   CHECK(!ps4_mismatches(dev, &ref), "ps4: a report other than 0x01 or 0x11 was taken");
   ifc->deinit(dev);

   /* over USB: the light bar for its slot with the first report, then
    * rumble from the poll, as report 0x05 */
   {
      unsigned count;
      dev = ifc->init(&dummy_connection, 1, &stub_hid);
      memset(p, 0, sizeof(p));
      p[0] = 0x01; p[5] = 8;
      count = sent_count;
      deliver(ifc, dev, p, 64);
      CHECK(sent_count == count + 1 && sent[0] == 0x05 && sent_len == 32
            && (sent[1] & 0x02) && sent[6] == 0x40 && !sent[7] && !sent[8],
            "ps4: the light bar not lit red for the second slot");
      ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0xc000);
      ifc->set_rumble(dev, RETRO_RUMBLE_WEAK, 0x4000);
      CHECK(sent_count == count + 1, "ps4: rumble sent outside the poll");
      deliver(ifc, dev, p, 64);
      CHECK(sent_count == count + 2 && sent[0] == 0x05 && sent[4] == 0x40
            && sent[5] == 0xc0 && !(sent[1] & 0x02),
            "ps4: rumble not sent as report 0x05");
      deliver(ifc, dev, p, 64);
      CHECK(sent_count == count + 2, "ps4: rumble sent again unchanged");
      ifc->deinit(dev);

      /* over Bluetooth: report 0x11, with its CRC; behind a stack's
       * header, nothing */
      stub_hid.is_bluetooth = stub_is_bluetooth;
      dev = ifc->init(&dummy_connection, 0, &stub_hid);
      count = sent_count;
      deliver(ifc, dev, p, 10);
      CHECK(sent_count == count + 1 && sent[0] == 0x11 && sent_len == 78,
            "ps4: Bluetooth output not report 0x11");
      ifc->deinit(dev);
      stub_hid.is_bluetooth = NULL;
      dev = ifc->init(&dummy_connection, 0, &stub_hid);
      count = sent_count;
      memset(p, 0, sizeof(p));
      p[0] = 0xa1; p[1] = 0x01; p[6] = 8;
      deliver(ifc, dev, p, 11);
      CHECK(sent_count == count, "ps4: sent behind a stack's header");
      ifc->deinit(dev);
   }
   printf("[%s] ps4: every button, the set, the sticks and the triggers as the copying handler, from reports 0x01 and 0x11, on 20040 reports; the light bar and rumble as report 0x05 or 0x11\n",
         failures == had ? "pass" : "fail");
}

/* ---- Wii U GameCube adapter: the old handler's report and reads -- */

#define REF_GCA_LEN 37

struct ref_gca_pad
{
   uint32_t buttons;
   int16_t axes[6];  /* the sticks, then the triggers' travel */
   int lo[6], hi[6]; /* the reach seen */
   uint8_t connected;
};

/* SDL's reach: 128 +- 88 for a stick, from 40 for a trigger, widened
 * as the pad goes further, and reset when a pad is put in */
static int ref_gca_reach(struct ref_gca_pad *pad, unsigned i, int v)
{
   if (v < pad->lo[i]) pad->lo[i] = v;
   if (v > pad->hi[i]) pad->hi[i] = v;
   return (int)((long)(v - pad->lo[i]) * 65535 / (pad->hi[i] - pad->lo[i]));
}

/* the old update_button_state() and update_analog_state(), on the old
 * nine-byte copy */
static void ref_gca_take(struct ref_gca_pad *pads, const uint8_t *packet)
{
   static const uint32_t map[12] = {
      RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_Y,
      RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT,
      RETRO_DEVICE_ID_JOYPAD_DOWN, RETRO_DEVICE_ID_JOYPAD_UP,
      RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_SELECT,
      RETRO_DEVICE_ID_JOYPAD_R, RETRO_DEVICE_ID_JOYPAD_L };
   uint8_t copy[64];
   unsigned i;
   memcpy(copy, packet, REF_GCA_LEN);
   for (i = 1; i < 37; i += 9)
   {
      struct ref_gca_pad *pad = &pads[i / 9];
      uint8_t data[9];
      uint32_t keys, k;
      int a;
      if (copy[i] <= 0x04)
      {
         pad->connected = 0;
         continue;
      }
      if (!pad->connected)
         for (a = 0; a < 6; a++)
         {
            pad->lo[a] = 40;
            pad->hi[a] = 216;
         }
      pad->connected = copy[i];
      memcpy(data, &copy[i], 9);
      keys        = data[1] | (data[2] << 8);
      pad->buttons = 0;
      for (k = 0; k < 12; k++)
         pad->buttons |= (keys & (1 << k)) ? (1 << map[k]) : 0;
      for (a = 0; a < 4; a++)
      {
         long v = ref_gca_reach(pad, (unsigned)a, data[3 + a]) - 32768L;
         if (a & 1)
            v = v <= -32768L ? 32767L : -v; /* up negative */
         pad->axes[a] = (int16_t)v;
      }
      for (a = 4; a < 6; a++)
         pad->axes[a] = (int16_t)(ref_gca_reach(pad, (unsigned)a, data[3 + a]) / 2);
   }
}

static void test_wiiugca(void)
{
   uint8_t p[64];
   struct ref_gca_pad ref[4];
   joypad_connection_t joy[4];
   void *pads[4];
   unsigned n, i, bad = 0, had = failures;
   const pad_connection_interface_t *ifc = &pad_connection_wiiugca;
   int dummy_handle;
   void *dev = ifc->init(&dummy_handle, 0, &stub_hid);
   static const uint32_t axes[8] = {
      AXIS_POS(0), AXIS_POS(1), AXIS_POS(2), AXIS_POS(3),
      AXIS_NEG(0), AXIS_NEG(1), AXIS_NEG(2), AXIS_NEG(3) };

   CHECK(dev != NULL, "wiiugca: no device");
   if (!dev)
      return;
   CHECK(sent_len == 1 && sent[0] == 0x13,
         "wiiugca: not started with report 0x13 alone");
   memset(ref, 0, sizeof(ref));
   memset(joy, 0, sizeof(joy));
   memset(pads, 0, sizeof(pads));
   srand(2);
   for (n = 0; n < 20000; n++)
   {
      for (i = 0; i < sizeof(p); i++)
         p[i] = (uint8_t)rand();
      /* ports come and go: powered only, connected, a WaveBird */
      for (i = 0; i < 4; i++)
      {
         static const uint8_t states[4] = { 0x04, 0x10, 0x14, 0x22 };
         p[1 + i * 9] = states[rand() & 3];
      }
      deliver(ifc, dev, p, REF_GCA_LEN);
      ref_gca_take(ref, p);
      for (i = 0; i < 4; i++)
      {
         unsigned k;
         input_bits_t bits;
         if (!pads[i] && ref[i].connected)
            pads[i] = ifc->pad_init(dev, (int)i, &joy[i]);
         if (!pads[i])
            continue;
         if (!!(ifc->status(dev, (int)i) & PAD_CONNECT_READY) != !!ref[i].connected)
            bad++;
         ifc->get_buttons(pads[i], &bits);
         for (k = 0; k < 16; k++)
            if (   !!ifc->button(pads[i], (uint16_t)k) != !!(ref[i].buttons & (1u << k))
                || !!BIT256_GET(bits, k) != !!(ref[i].buttons & (1u << k)))
               bad++;
         /* plain, and bound as wiiu_hid asks: the stick's whole value */
         for (k = 0; k < 6; k++)
            if (ifc->get_axis(pads[i], k) != ref[i].axes[k])
               bad++;
         for (k = 0; k < 8; k++)
            if (ifc->get_axis(pads[i], axes[k]) != ref[i].axes[k & 3])
               bad++;
      }
   }
   CHECK(!bad, "wiiugca: %u reads disagree with the copying handler", bad);
   /* a short report is not taken */
   {
      int16_t ax = pads[0] ? ifc->get_axis(pads[0], AXIS_POS(0)) : 0;
      memset(p, 0x14, sizeof(p));
      deliver(ifc, dev, p, REF_GCA_LEN - 1);
      CHECK(!pads[0] || ifc->get_axis(pads[0], AXIS_POS(0)) == ax,
            "wiiugca: a short report was taken");
   }
   /* rumble: report 0x11, a byte a port, only a port with the power
    * for it and not a WaveBird; sent once a change */
   if (pads[0] && pads[2])
   {
      unsigned count;
      memset(p, 0, sizeof(p));
      p[1] = 0x14; p[10] = 0x10; p[19] = 0x22; p[28] = 0x14;
      deliver(ifc, dev, p, REF_GCA_LEN);
      count = sent_count;
      ifc->set_rumble(pads[0], RETRO_RUMBLE_STRONG, 0x8000);
      ifc->set_rumble(pads[2], RETRO_RUMBLE_WEAK, 0x8000);
      CHECK(sent_count == count, "wiiugca: rumble sent outside the packet handler");
      deliver(ifc, dev, p, REF_GCA_LEN);
      CHECK(sent_count == count + 1 && sent_len == 5 && sent[0] == 0x11
            && sent[1] == 1 && !sent[2] && !sent[3] && !sent[4],
            "wiiugca: rumble report not 11 01 00 00 00");
      ifc->set_rumble(pads[0], RETRO_RUMBLE_STRONG, 0x8000);
      deliver(ifc, dev, p, REF_GCA_LEN);
      CHECK(sent_count == count + 1, "wiiugca: rumble sent again unchanged");
      ifc->set_rumble(pads[0], RETRO_RUMBLE_STRONG, 0);
      deliver(ifc, dev, p, REF_GCA_LEN);
      CHECK(sent_count == count + 2 && sent[1] == 0, "wiiugca: rumble not stopped");
   }
   for (i = 0; i < 4; i++)
      if (pads[i])
         ifc->pad_deinit(pads[i]);
   ifc->deinit(dev);
   printf("[%s] wiiugca: started with report 0x13; four ports' buttons, sets and state as the copying handler, sticks and triggers over SDL's widening reach, on 20000 reports; rumble report 0x11 for powered wired pads\n",
         failures == had ? "pass" : "fail");
}

/* ---- PS2 and PSX adapters: the old handlers' report and reads -- */

struct ref_adapter
{
   uint8_t data[64];
   uint32_t buttons;
};

static const uint32_t ref_adapter_map[16] =
{
   RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_ID_JOYPAD_R2,
   RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R,
   RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START,
   RETRO_DEVICE_ID_JOYPAD_L3, RETRO_DEVICE_ID_JOYPAD_R3,
   NO_BTN, NO_BTN, NO_BTN, NO_BTN,
   RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_A,
   RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_Y
};

static void ref_adapter_hat(struct ref_adapter *r, unsigned h)
{
   if (h == 5 || h == 6 || h == 7)
      r->buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
   if (h == 1 || h == 2 || h == 3)
      r->buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   if (h == 7 || h == 0 || h == 1)
      r->buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
   if (h == 3 || h == 4 || h == 5)
      r->buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
}

static void ref_adapter_keys(struct ref_adapter *r, unsigned keys)
{
   unsigned i;
   r->buttons = 0;
   for (i = 0; i < 16; i++)
      if (ref_adapter_map[i] != NO_BTN && (keys & (1u << i)))
         r->buttons |= 1u << ref_adapter_map[i];
}

static void ref_ps2_take(struct ref_adapter *r, const uint8_t *p, unsigned size)
{
   if (p[1] != 1)
      return;
   memcpy(r->data, p, size);
   ref_adapter_keys(r, r->data[7] | (r->data[6] << 8));
   ref_adapter_hat(r, r->data[6] & 15);
}

static void ref_psx_take(struct ref_adapter *r, const uint8_t *p, unsigned size)
{
   memcpy(r->data, p, size);
   ref_adapter_keys(r, r->data[6] | (r->data[5] << 8));
   if (r->data[2] == 0x7F)
   {
      if (r->data[3] == 0x00) r->buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
      if (r->data[3] == 0xFF) r->buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
      if (r->data[4] == 0x00) r->buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
      if (r->data[4] == 0xFF) r->buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
   }
   else
      ref_adapter_hat(r, r->data[5] & 15);
}

static int16_t ref_adapter_axis(const struct ref_adapter *r, unsigned axis,
      bool psx)
{
   static const unsigned at[4] = { 4, 5, 3, 2 };
   int val;
   if (axis >= 4 || (psx && r->data[2] == 0x7F))
      return 0;
   val = (r->data[at[axis]] << 8) - 0x8000;
   return abs(val) > 0x1000 ? (int16_t)val : 0;
}

static unsigned adapter_mismatches(const pad_connection_interface_t *ifc,
      void *dev, const struct ref_adapter *r, bool psx)
{
   unsigned k, bad = 0;
   input_bits_t bits;
   ifc->get_buttons(dev, &bits);
   for (k = 0; k < 32; k++)
   {
      int want = !!(r->buttons & (1u << k));
      if (!!ifc->button(dev, (uint16_t)k) != want
            || (k < 16 && !!BIT256_GET(bits, k) != want))
         bad++;
   }
   for (k = 0; k < 5; k++)
      if (ifc->get_axis(dev, k) != ref_adapter_axis(r, k, psx))
         bad++;
   return bad;
}

static void test_adapter(const pad_connection_interface_t *ifc,
      const char *name, bool psx)
{
   uint8_t p[64];
   struct ref_adapter ref;
   unsigned n, i, bad = 0, had = failures;
   unsigned min = psx ? 7 : 8;
   int dummy_connection;
   void *dev = ifc->init(&dummy_connection, 0, &stub_hid);

   CHECK(dev != NULL, "%s: no device", name);
   if (!dev)
      return;
   memset(&ref, 0, sizeof(ref));
   srand(psx ? 4 : 3);
   for (n = 0; n < 20000; n++)
   {
      unsigned size = min + (unsigned)rand() % (sizeof(p) + 1 - min);
      for (i = 0; i < sizeof(p); i++)
         p[i] = (uint8_t)rand();
      if (psx)
      {
         /* digital mode half the time, its d-pad at the ends */
         if (rand() & 1)
            p[2] = 0x7F;
         if (rand() & 1)
            p[3] = (rand() & 1) ? 0xFF : 0x00;
         if (rand() & 1)
            p[4] = (rand() & 1) ? 0xFF : 0x00;
      }
      else if (rand() & 3)
         p[1] = 1; /* the first controller; others are not taken */
      deliver(ifc, dev, p, (uint16_t)size);
      if (psx)
         ref_psx_take(&ref, p, size);
      else
         ref_ps2_take(&ref, p, size);
      bad += adapter_mismatches(ifc, dev, &ref, psx);
   }
   CHECK(!bad, "%s: %u reads disagree with the copying handler", name, bad);
   /* a report too short to be one leaves the pad as it was */
   memset(p, 0x01, sizeof(p));
   deliver(ifc, dev, p, (uint16_t)(min - 1));
   CHECK(!adapter_mismatches(ifc, dev, &ref, psx),
         "%s: a short report was taken", name);
   ifc->deinit(dev);
   printf("[%s] %s: every button, the set and the axes as the copying handler, on 20000 reports\n",
         failures == had ? "pass" : "fail", name);
}

/* ---- SNES and NES USB pads: the old handlers' report and reads -- */

static const uint32_t ref_snesusb_map[16] =
{
   RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R, NO_BTN, NO_BTN,
   RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START,
   NO_BTN, NO_BTN, NO_BTN, NO_BTN, NO_BTN, NO_BTN,
   RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_A,
   RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_Y
};

static const uint32_t ref_nesusb_map[16] =
{
   RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_ID_JOYPAD_R2,
   RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R,
   RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START,
   NO_BTN, NO_BTN, NO_BTN, NO_BTN, NO_BTN, NO_BTN,
   RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A,
   RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_X
};

static void ref_usbpad_take(struct ref_adapter *r, const uint32_t *map,
      const uint8_t *p, unsigned size)
{
   unsigned i, keys;
   memcpy(r->data, p, size);
   keys       = r->data[6] | (r->data[5] << 8);
   r->buttons = 0;
   for (i = 0; i < 16; i++)
      if (map[i] != NO_BTN && (keys & (1u << i)))
         r->buttons |= 1u << map[i];
}

static int16_t ref_usbpad_axis(const struct ref_adapter *r, unsigned at,
      unsigned axis)
{
   int val;
   if (axis >= 2)
      return 0;
   val = (r->data[at + axis] << 8) - 0x8000;
   return abs(val) > 0x1000 ? (int16_t)val : 0;
}

static unsigned usbpad_mismatches(const pad_connection_interface_t *ifc,
      void *dev, const struct ref_adapter *r, unsigned at)
{
   unsigned k, bad = 0;
   input_bits_t bits;
   ifc->get_buttons(dev, &bits);
   for (k = 0; k < 32; k++)
   {
      int want = !!(r->buttons & (1u << k));
      if (!!ifc->button(dev, (uint16_t)k) != want
            || (k < 16 && !!BIT256_GET(bits, k) != want))
         bad++;
   }
   for (k = 0; k < 3; k++)
      if (ifc->get_axis(dev, k) != ref_usbpad_axis(r, at, k))
         bad++;
   return bad;
}

static void test_usbpad(const pad_connection_interface_t *ifc,
      const char *name, const uint32_t *map, unsigned at, unsigned seed)
{
   uint8_t p[64];
   struct ref_adapter ref;
   unsigned n, i, bad = 0, had = failures;
   int dummy_connection;
   void *dev = ifc->init(&dummy_connection, 0, &stub_hid);

   CHECK(dev != NULL, "%s: no device", name);
   if (!dev)
      return;
   memset(&ref, 0, sizeof(ref));
   srand(seed);
   for (n = 0; n < 20000; n++)
   {
      unsigned size = 7 + (unsigned)rand() % (sizeof(p) - 6);
      for (i = 0; i < sizeof(p); i++)
         p[i] = (uint8_t)rand();
      deliver(ifc, dev, p, (uint16_t)size);
      ref_usbpad_take(&ref, map, p, size);
      bad += usbpad_mismatches(ifc, dev, &ref, at);
   }
   CHECK(!bad, "%s: %u reads disagree with the copying handler", name, bad);
   memset(p, 0xff, sizeof(p));
   deliver(ifc, dev, p, 6);
   CHECK(!usbpad_mismatches(ifc, dev, &ref, at), "%s: a short report was taken", name);
   ifc->deinit(dev);
   printf("[%s] %s: every button, the set and the axes as the copying handler, on 20000 reports\n",
         failures == had ? "pass" : "fail", name);
}

/* ---- DragonRise encoder and HORI mini: the old handlers' report and
 * reads ------------------------------------------------------------- */

static void ref_dragonrise_take(struct ref_adapter *r, const uint8_t *p,
      unsigned size)
{
   memcpy(r->data, p, size);
   ref_adapter_keys(r, r->data[6] | (r->data[5] << 8));
   if (r->data[3] != 0x7F)
      ref_adapter_hat(r, r->data[5] & 15);
}

static int16_t ref_dragonrise_axis(const struct ref_adapter *r, unsigned axis)
{
   int val;
   if (axis >= 2 || r->data[3] != 0x7F)
      return 0;
   val = (r->data[axis] << 8) - 0x8000;
   return abs(val) > 0x1000 ? (int16_t)val : 0;
}

static void ref_hori_take(struct ref_adapter *r, const uint8_t *p,
      unsigned size)
{
   static const uint32_t map[15] =
   {
      RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_ID_JOYPAD_R2,
      RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START,
      RETRO_DEVICE_ID_JOYPAD_L3, RETRO_DEVICE_ID_JOYPAD_R3, 16, 17, 18
   };
   static const uint8_t dpad[8] = { 1, 1|8, 8, 8|2, 2, 2|4, 4, 4|1 };
   unsigned i, keys;
   memcpy(r->data, p, size);
   r->buttons = 0;
   if ((r->data[5] & 15) < 8)
   {
      unsigned d = dpad[r->data[5] & 15];
      if (d & 1) r->buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
      if (d & 2) r->buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
      if (d & 4) r->buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
      if (d & 8) r->buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   }
   keys = ((r->data[5] & 0xF0) >> 4) | (r->data[6] << 4) | (r->data[7] << 12);
   for (i = 0; i < 15; i++)
      if (keys & (1u << i))
         r->buttons |= 1u << map[i];
}

static int16_t ref_hori_axis(const struct ref_adapter *r, unsigned axis)
{
   int val;
   if (axis >= 4)
      return 0;
   val = (r->data[1 + axis] << 8) - 0x8000;
   return abs(val) > 0x1000 ? (int16_t)val : 0;
}

static unsigned pad_mismatches(const pad_connection_interface_t *ifc,
      void *dev, const struct ref_adapter *r, unsigned set_bits,
      int16_t (*axis)(const struct ref_adapter*, unsigned))
{
   unsigned k, bad = 0;
   input_bits_t bits;
   ifc->get_buttons(dev, &bits);
   for (k = 0; k < 32; k++)
   {
      int want = !!(r->buttons & (1u << k));
      if (!!ifc->button(dev, (uint16_t)k) != want
            || (k < set_bits && !!BIT256_GET(bits, k) != want))
         bad++;
   }
   for (k = 0; k < 5; k++)
      if (ifc->get_axis(dev, k) != axis(r, k))
         bad++;
   return bad;
}

static void test_pad(const pad_connection_interface_t *ifc, const char *name,
      unsigned min, unsigned set_bits, unsigned seed,
      void (*take)(struct ref_adapter*, const uint8_t*, unsigned),
      int16_t (*axis)(const struct ref_adapter*, unsigned))
{
   uint8_t p[64];
   struct ref_adapter ref;
   unsigned n, i, bad = 0, had = failures;
   int dummy_connection;
   void *dev = ifc->init(&dummy_connection, 0, &stub_hid);

   CHECK(dev != NULL, "%s: no device", name);
   if (!dev)
      return;
   memset(&ref, 0, sizeof(ref));
   srand(seed);
   for (n = 0; n < 20000; n++)
   {
      unsigned size = min + (unsigned)rand() % (sizeof(p) + 1 - min);
      for (i = 0; i < sizeof(p); i++)
         p[i] = (uint8_t)rand();
      if (rand() & 1)
         p[3] = 0x7F; /* the DragonRise's analog mode */
      deliver(ifc, dev, p, (uint16_t)size);
      take(&ref, p, size);
      bad += pad_mismatches(ifc, dev, &ref, set_bits, axis);
   }
   CHECK(!bad, "%s: %u reads disagree with the copying handler", name, bad);
   memset(p, 0x7F, sizeof(p));
   deliver(ifc, dev, p, (uint16_t)(min - 1));
   CHECK(!pad_mismatches(ifc, dev, &ref, set_bits, axis),
         "%s: a short report was taken", name);
   ifc->deinit(dev);
   printf("[%s] %s: every button, the set and the axes as the copying handler, on 20000 reports\n",
         failures == had ? "pass" : "fail", name);
}

/* ---- Kade encoder and Retrode: the old handlers' report and reads -- */

static void ref_kade_take(struct ref_adapter *r, const uint8_t *p,
      unsigned size)
{
   static const uint32_t map[16] =
   {
      RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START,
      NO_BTN, NO_BTN, NO_BTN, NO_BTN, NO_BTN, NO_BTN,
      RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_ID_JOYPAD_R2
   };
   unsigned i, keys;
   memcpy(r->data, p, size);
   keys       = r->data[1] | (r->data[0] << 8);
   r->buttons = 0;
   for (i = 0; i < 16; i++)
      if (map[i] != NO_BTN && (keys & (1u << i)))
         r->buttons |= 1u << map[i];
   ref_adapter_hat(r, r->data[2] & 15);
}

static int16_t ref_no_axis(const struct ref_adapter *r, unsigned axis)
{
   (void)r;
   (void)axis;
   return 0;
}

static void ref_retrode_take(struct ref_adapter *r, const uint8_t *p,
      unsigned size)
{
   static const uint32_t map[8] =
   {
      RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_Y,
      RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START,
      RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R
   };
   unsigned i;
   memcpy(r->data, p, size);
   r->buttons = 0;
   for (i = 0; i < 8; i++)
      if (r->data[3] & (1u << i))
         r->buttons |= 1u << map[i];
}

static int16_t ref_retrode_axis(const struct ref_adapter *r, unsigned axis)
{
   int val;
   if (axis >= 2)
      return 0;
   val = r->data[1 + axis];
   if (val == 0x9C)
      val = 0x00;
   else if (val == 0x64)
      val = 0xFF;
   else
      val = 0x7F;
   val = (val << 8) - 0x8000;
   return abs(val) > 0x1000 ? (int16_t)val : 0;
}

static void test_retrode(void)
{
   uint8_t p[64];
   struct ref_adapter ref[4];
   joypad_connection_t joy[4];
   void *pads[4];
   unsigned n, i, bad = 0, had = failures;
   const pad_connection_interface_t *ifc = &pad_connection_retrode;
   int dummy_handle;
   void *dev = ifc->init(&dummy_handle, 0, &stub_hid);

   CHECK(dev != NULL, "retrode: no device");
   if (!dev)
      return;
   memset(ref, 0, sizeof(ref));
   memset(joy, 0, sizeof(joy));
   for (i = 0; i < 4; i++)
      pads[i] = ifc->pad_init(dev, (int)i, &joy[i]);
   srand(9);
   for (n = 0; n < 20000; n++)
   {
      unsigned size = 4 + (unsigned)rand() % (sizeof(p) - 3);
      for (i = 0; i < sizeof(p); i++)
         p[i] = (uint8_t)rand();
      p[0] = (uint8_t)(rand() % 6); /* ports 1-4, and two that are none */
      for (i = 1; i < 3; i++)
      {
         static const uint8_t dir[3] = { 0x9C, 0x64, 0x00 };
         if (rand() & 1)
            p[i] = dir[rand() % 3];
      }
      deliver(ifc, dev, p, (uint16_t)size);
      if (p[0] >= 1 && p[0] <= 4)
         ref_retrode_take(&ref[p[0] - 1], p, size);
      for (i = 0; i < 4; i++)
         if (pads[i])
            bad += pad_mismatches(ifc, pads[i], &ref[i], 16, ref_retrode_axis);
   }
   CHECK(!bad, "retrode: %u reads disagree with the copying handler", bad);
   memset(p, 0xff, sizeof(p));
   p[0] = 1;
   deliver(ifc, dev, p, 3);
   CHECK(!pads[0] || !pad_mismatches(ifc, pads[0], &ref[0], 16, ref_retrode_axis),
         "retrode: a short report was taken");
   for (i = 0; i < 4; i++)
      if (pads[i])
         ifc->pad_deinit(pads[i]);
   ifc->deinit(dev);
   printf("[%s] retrode: four ports' buttons, sets and axes as the copying handler, on 20000 reports\n",
         failures == had ? "pass" : "fail");
}

/* ---- DualSense: a new handler, so held to the report's layout ------ */

/* The pad's state as the three report shapes carry it. */
struct ps5_state
{
   uint8_t sticks[4];
   uint8_t triggers[2];
   uint8_t b[3];
};

static unsigned ps5_report(uint8_t *p, const struct ps5_state *st, int shape)
{
   unsigned i;
   for (i = 0; i < 80; i++)
      p[i] = (uint8_t)rand(); /* what is not read is noise */
   switch (shape)
   {
      case 0: /* USB */
         p[0] = 0x01;
         memcpy(p + 1, st->sticks, 4);
         memcpy(p + 5, st->triggers, 2);
         memcpy(p + 8, st->b, 3);
         return 64;
      case 1: /* Bluetooth, before any output */
         p[0] = 0x01;
         memcpy(p + 1, st->sticks, 4);
         memcpy(p + 5, st->b, 3);
         memcpy(p + 8, st->triggers, 2);
         return 10;
      case 3: /* Bluetooth's full report, as the system gives it */
         p[0] = 0x31;
         memcpy(p + 2, st->sticks, 4);
         memcpy(p + 6, st->triggers, 2);
         memcpy(p + 9, st->b, 3);
         return 78;
      default: /* Bluetooth's full report, from a stack with its header */
         p[0] = 0xa1;
         p[1] = 0x31;
         memcpy(p + 3, st->sticks, 4);
         memcpy(p + 7, st->triggers, 2);
         memcpy(p + 10, st->b, 3);
         return 78;
   }
}

static uint32_t ps5_want(const struct ps5_state *st)
{
   static const uint8_t hat[16] = { 1, 9, 8, 10, 2, 6, 4, 5 };
   static const unsigned ids[16] = {
      0, 0, 0, 0,
      RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_ID_JOYPAD_R2,
      RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START,
      RETRO_DEVICE_ID_JOYPAD_L3, RETRO_DEVICE_ID_JOYPAD_R3 };
   uint32_t r = 0;
   unsigned i, h = hat[st->b[0] & 15], word = st->b[0] | (st->b[1] << 8);
   if (h & 1) r |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
   if (h & 2) r |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
   if (h & 4) r |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
   if (h & 8) r |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   for (i = 4; i < 16; i++)
      if (word & (1u << i))
         r |= 1u << ids[i];
   for (i = 0; i < 3; i++)
      if (st->b[2] & (1u << i))
         r |= 1u << (16 + i);
   for (i = 4; i < 8; i++) /* an Edge's Fn and back buttons */
      if (st->b[2] & (1u << i))
         r |= 1u << (15 + i);
   return r;
}

static unsigned ps5_mismatches(const pad_connection_interface_t *ifc,
      void *dev, const struct ps5_state *st)
{
   unsigned k, bad = 0;
   uint32_t want = ps5_want(st);
   input_bits_t bits;
   ifc->get_buttons(dev, &bits);
   for (k = 0; k < 32; k++)
      if (   !!ifc->button(dev, (uint16_t)k) != !!(want & (1u << k))
          || !!BIT256_GET(bits, k) != !!(want & (1u << k)))
         bad++;
   if (!!BIT256_GET(bits, RARCH_MENU_TOGGLE) != !!(st->b[2] & 1))
      bad++;
   for (k = 0; k < 4; k++)
      if (ifc->get_axis(dev, k) != (int16_t)((st->sticks[k] << 8) - 0x8000))
         bad++;
   for (k = 0; k < 2; k++)
      if (ifc->get_axis(dev, 4 + k)
            != (int16_t)((st->triggers[k] << 7) | (st->triggers[k] >> 1)))
         bad++;
   if (ifc->get_axis(dev, 6))
      bad++;
   return bad;
}

/* a DualSense's feature report 0x20, its firmware at 44 */
static unsigned feature_firmware;

static int32_t stub_get_report(void *handle, uint8_t type, uint8_t id,
      uint8_t *s, size_t len)
{
   (void)handle;
   if (!feature_firmware || type != HID_REPORT_FEATURE || id != 0x20 || len < 64)
      return -1;
   memset(s, 0, len);
   s[0]  = 0x20;
   s[44] = (uint8_t)feature_firmware;
   s[45] = (uint8_t)(feature_firmware >> 8);
   return 64;
}


/* the last report sent is the shared builder's for this state */
static bool ps5_sent_is(bool bluetooth, bool v2, uint8_t strong,
      uint8_t weak, int player)
{
   uint8_t want[SONY_PAD_REPORT_MAX];
   size_t len = sony_pad_output_report(want, sizeof(want),
         SONY_PAD_DUALSENSE, bluetooth, v2, strong, weak, player);
   return len && sent_len == len && !memcmp(sent, want, len);
}

static void ps5_random_state(struct ps5_state *st)
{
   unsigned i;
   for (i = 0; i < 4; i++) st->sticks[i]   = (uint8_t)rand();
   for (i = 0; i < 2; i++) st->triggers[i] = (uint8_t)rand();
   for (i = 0; i < 3; i++) st->b[i]        = (uint8_t)rand();
   st->b[2] &= 0xf7;
}

static void test_ps5(void)
{
   uint8_t p[81]; /* one more: a report can follow a stack's header */
   struct ps5_state st;
   unsigned n, bad = 0, had = failures;
   const pad_connection_interface_t *ifc = &pad_connection_ps5;
   int dummy_connection;
   void *dev;

   /* behind a Bluetooth stack's header: read in both shapes, nothing
    * sent, the stack frames output its own way */
   sent_count = 0;
   dev = ifc->init(&dummy_connection, 1, &stub_hid);
   CHECK(dev != NULL, "ps5: no device");
   if (!dev)
      return;
   srand(11);
   for (n = 0; n < 20000; n++)
   {
      unsigned len;
      ps5_random_state(&st);
      if (n & 1)
         len = ps5_report(p, &st, 2);
      else
      {
         /* the short report, with the stack's header */
         len = ps5_report(p + 1, &st, 1) + 1;
         p[0] = 0xa1;
      }
      deliver(ifc, dev, p, (uint16_t)len);
      bad += ps5_mismatches(ifc, dev, &st);
   }
   CHECK(!bad, "ps5: %u reads over Bluetooth disagree with the report", bad);
   CHECK(sent_count == 0, "ps5: output went out behind a Bluetooth stack's header");

   /* reports that are not the pad's state, or too short to be */
   memset(p, 0xff, sizeof(p));
   p[0] = 0x01;
   deliver(ifc, dev, p, 9);
   p[0] = 0x31;
   deliver(ifc, dev, p, 11);
   p[0] = 0x05;
   deliver(ifc, dev, p, 64);
   CHECK(!ps5_mismatches(ifc, dev, &st), "ps5: a short or foreign report was taken");
   ifc->deinit(dev);

   /* Bluetooth through the system (hidraw, Windows): the lights, then
    * rumble, as the shared builder makes them, with their checksum */
   sent_count = 0;
   dev = ifc->init(&dummy_connection, 1, &stub_hid);
   ps5_random_state(&st);
   deliver(ifc, dev, p, (uint16_t)ps5_report(p, &st, 1));
   CHECK(sent_count == 1 && ps5_sent_is(true, false, 0, 0, 2),
         "ps5: the first Bluetooth report did not light player 2");
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0x8000);
   deliver(ifc, dev, p, (uint16_t)ps5_report(p, &st, 3));
   CHECK(sent_count == 2 && ps5_sent_is(true, false, 0x80, 0, 0),
         "ps5: Bluetooth rumble is not the shared builder's report");
   CHECK(!ps5_mismatches(ifc, dev, &st), "ps5: Bluetooth's full report misread");
   ifc->deinit(dev);

   /* Bluetooth through Windows: every report at the longest one's
    * length, so the driver says it is Bluetooth */
   sent_count            = 0;
   stub_hid.is_bluetooth = stub_is_bluetooth;
   dev = ifc->init(&dummy_connection, 1, &stub_hid);
   ps5_random_state(&st);
   ps5_report(p, &st, 1);
   deliver(ifc, dev, p, 78);
   CHECK(!ps5_mismatches(ifc, dev, &st), "ps5: Windows' padded Bluetooth report was read as USB's");
   CHECK(sent_count == 1 && ps5_sent_is(true, false, 0, 0, 2),
         "ps5: output to a pad the driver says is on Bluetooth is not Bluetooth's");
   ifc->deinit(dev);
   stub_hid.is_bluetooth = NULL;

   /* USB: read, and the first report lights player 2 */
   sent_count = 0;
   dev = ifc->init(&dummy_connection, 1, &stub_hid);
   bad = 0;
   for (n = 0; n < 20000; n++)
   {
      ps5_random_state(&st);
      deliver(ifc, dev, p, (uint16_t)ps5_report(p, &st, 0));
      bad += ps5_mismatches(ifc, dev, &st);
      if (n == 0)
         CHECK(sent_count == 1 && ps5_sent_is(false, false, 0, 0, 2),
               "ps5: the first USB report did not light player 2");
   }
   CHECK(!bad, "ps5: %u reads over USB disagree with the report", bad);
   CHECK(sent_count == 1, "ps5: output went with no change to send");

   /* rumble: one report at the next poll, however often it is asked */
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0x8000);
   ifc->set_rumble(dev, RETRO_RUMBLE_WEAK, 0xff00);
   ifc->set_rumble(dev, RETRO_RUMBLE_WEAK, 0xff00);
   CHECK(sent_count == 1, "ps5: rumble was sent outside the poll");
   deliver(ifc, dev, p, (uint16_t)ps5_report(p, &st, 0));
   CHECK(sent_count == 2 && ps5_sent_is(false, false, 0x80, 0xff, 0),
         "ps5: the rumble report is not the motors asked for");
   deliver(ifc, dev, p, (uint16_t)ps5_report(p, &st, 0));
   CHECK(sent_count == 2, "ps5: rumble was sent again unchanged");

   /* told its port: relit at the next report, then put out on none */
   ifc->set_player(dev, 3);
   deliver(ifc, dev, p, (uint16_t)ps5_report(p, &st, 0));
   CHECK(ps5_sent_is(false, false, 0x80, 0xff, 3), "ps5: not relit for port 3");
   ifc->set_player(dev, 0);
   deliver(ifc, dev, p, (uint16_t)ps5_report(p, &st, 0));
   CHECK(ps5_sent_is(false, false, 0x80, 0xff, -1), "ps5: lit on no port");
   ifc->deinit(dev);
   /* told before its first report: that port lit, not its slot's */
   sent_count = 0;
   dev = ifc->init(&dummy_connection, 1, &stub_hid);
   ifc->set_player(dev, 4);
   deliver(ifc, dev, p, (uint16_t)ps5_report(p, &st, 0));
   CHECK(sent_count == 1 && ps5_sent_is(false, false, 0, 0, 4),
         "ps5: a pad told its port before its first report lit its slot");
   ifc->deinit(dev);

   /* the newer motor control: an Edge always, a DualSense from
    * firmware 2.21 */
   sent_count = 0;
   dev = pad_connection_ps5_edge.init(&dummy_connection, 0, &stub_hid);
   deliver(ifc, dev, p, (uint16_t)ps5_report(p, &st, 0));
   CHECK(ps5_sent_is(false, true, 0, 0, 1), "ps5: an Edge's motors are not the newer way");
   CHECK(!strcmp(ifc->get_name(dev), "DualSense Edge Wireless Controller"),
         "ps5: an Edge is not named one");
   ifc->deinit(dev);
   stub_hid.get_report = stub_get_report;
   feature_firmware    = 0x0221;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   deliver(ifc, dev, p, (uint16_t)ps5_report(p, &st, 0));
   CHECK(ps5_sent_is(false, true, 0, 0, 1), "ps5: firmware 2.21's motors are not the newer way");
   ifc->deinit(dev);
   feature_firmware    = 0x0215;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   deliver(ifc, dev, p, (uint16_t)ps5_report(p, &st, 0));
   CHECK(ps5_sent_is(false, false, 0, 0, 1), "ps5: older firmware's motors are the newer way");
   ifc->deinit(dev);
   stub_hid.get_report = NULL;
   feature_firmware    = 0;

   printf("[%s] ps5: USB and both Bluetooth shapes read as laid out, on 40000 reports;"
         " lights once and rumble at the poll as the shared builder makes them,"
         " the Edge and firmware 2.21 the newer way, nothing behind a stack's header\n",
         failures == had ? "pass" : "fail");
}

/* ---- Switch Pro Controller: its layout, greeting and commands - */

struct sp_state
{
   uint8_t b[3];      /* bytes 3 to 5 of the full report */
   int stick[4];      /* 12 bits */
};

static uint32_t sp_want(const struct sp_state *st)
{
   static const struct { unsigned byte, bit, id; } map[] = {
      { 0, 0x01, RETRO_DEVICE_ID_JOYPAD_Y },
      { 0, 0x02, RETRO_DEVICE_ID_JOYPAD_X },
      { 0, 0x04, RETRO_DEVICE_ID_JOYPAD_B },
      { 0, 0x08, RETRO_DEVICE_ID_JOYPAD_A },
      { 0, 0x40, RETRO_DEVICE_ID_JOYPAD_R },
      { 0, 0x80, RETRO_DEVICE_ID_JOYPAD_R2 },
      { 1, 0x01, RETRO_DEVICE_ID_JOYPAD_SELECT },
      { 1, 0x02, RETRO_DEVICE_ID_JOYPAD_START },
      { 1, 0x04, RETRO_DEVICE_ID_JOYPAD_R3 },
      { 1, 0x08, RETRO_DEVICE_ID_JOYPAD_L3 },
      { 1, 0x10, 16 },
      { 1, 0x20, 17 },
      { 2, 0x01, RETRO_DEVICE_ID_JOYPAD_DOWN },
      { 2, 0x02, RETRO_DEVICE_ID_JOYPAD_UP },
      { 2, 0x04, RETRO_DEVICE_ID_JOYPAD_RIGHT },
      { 2, 0x08, RETRO_DEVICE_ID_JOYPAD_LEFT },
      { 2, 0x40, RETRO_DEVICE_ID_JOYPAD_L },
      { 2, 0x80, RETRO_DEVICE_ID_JOYPAD_L2 }
   };
   uint32_t r = 0;
   unsigned i;
   for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
      if (st->b[map[i].byte] & map[i].bit)
         r |= 1u << map[i].id;
   return r;
}

static void sp_put_pair(uint8_t *p, int a, int b)
{
   p[0] = (uint8_t)a;
   p[1] = (uint8_t)(((a >> 8) & 0x0f) | ((b & 0x0f) << 4));
   p[2] = (uint8_t)(b >> 4);
}

static unsigned sp_report(uint8_t *p, uint8_t id, const struct sp_state *st)
{
   memset(p, 0, 64);
   p[0] = id;
   memcpy(p + 3, st->b, 3);
   sp_put_pair(p + 6, st->stick[0], st->stick[1]);
   sp_put_pair(p + 9, st->stick[2], st->stick[3]);
   return 64;
}

/* a command's answer, @data at 15 */
static unsigned sp_reply(uint8_t *p, const struct sp_state *st, uint8_t cmd,
      const uint8_t *data, unsigned len)
{
   sp_report(p, 0x21, st);
   p[13] = 0x80;
   p[14] = cmd;
   if (data)
      memcpy(p + 15, data, len);
   return 64;
}

static void sp_random(struct sp_state *st)
{
   unsigned i;
   for (i = 0; i < 3; i++) st->b[i]     = (uint8_t)rand();
   for (i = 0; i < 4; i++) st->stick[i] = rand() & 0xfff;
}

static unsigned sp_mismatches(void *dev, const struct sp_state *st)
{
   const pad_connection_interface_t *ifc = &pad_connection_switch_pro;
   input_bits_t bits;
   uint32_t want = sp_want(st);
   unsigned k, bad = 0;
   ifc->get_buttons(dev, &bits);
   for (k = 0; k < 18; k++)
   {
      if ((unsigned)ifc->button(dev, k) != ((want >> k) & 1))
         bad++;
      if ((unsigned)BIT256_GET(bits, k) != ((want >> k) & 1))
         bad++;
   }
   if ((unsigned)BIT256_GET(bits, RARCH_MENU_TOGGLE) != ((want >> 16) & 1))
      bad++;
   return bad;
}

/* the report sent @back reports ago starts so */
static bool sp_sent(unsigned back, const uint8_t *head, unsigned len)
{
   if (sent_count < back + 1)
      return false;
   return !memcmp(sent_head[(sent_count - 1 - back) % 64], head, len);
}

static bool sp_sent_command(unsigned back, uint8_t cmd, uint8_t arg)
{
   const uint8_t *h;
   if (sent_count < back + 1)
      return false;
   h = sent_head[(sent_count - 1 - back) % 64];
   return h[0] == 0x01 && h[10] == cmd && h[11] == arg;
}

#define SWITCH_PRO_TEST_TRIES 4 /* the handler's greetings before it gives up */

static void test_switch_pro(void)
{
   static const uint8_t hello[2]  = { 0x80, 0x02 };
   static const uint8_t baud[2]   = { 0x80, 0x03 };
   static const uint8_t start[2]  = { 0x80, 0x04 };
   static const uint8_t neutral[8] = { 0x00, 0x01, 0x40, 0x40,
      0x00, 0x01, 0x40, 0x40 };
   const pad_connection_interface_t *ifc = &pad_connection_switch_pro;
   uint8_t p[65], flash[5 + 24];
   struct sp_state st;
   unsigned n, bad = 0, had = failures, count;
   int dummy_connection;
   void *dev;

   memset(&st, 0, sizeof(st));
   for (n = 0; n < 4; n++)
      st.stick[n] = 0x800;

   /* USB: the greeting, each step on its answer */
   sent_count = 0;
   dev = ifc->init(&dummy_connection, 2, &stub_hid);
   CHECK(dev != NULL, "switch_pro: no device");
   if (!dev)
      return;
   CHECK(sent_count == 1 && sp_sent(0, hello, 2), "switch_pro: USB not greeted");
   p[0] = 0x81; p[1] = 0x02;
   deliver(ifc, dev, p, 64);
   CHECK(sent_count == 2 && sp_sent(0, baud, 2), "switch_pro: no faster link after the greeting");
   p[1] = 0x02;
   deliver(ifc, dev, p, 64);
   CHECK(sent_count == 2, "switch_pro: a greeting's answer out of turn was taken");
   p[1] = 0x03;
   deliver(ifc, dev, p, 64);
   CHECK(sent_count == 3 && sp_sent(0, hello, 2), "switch_pro: not greeted again");
   p[1] = 0x02;
   deliver(ifc, dev, p, 64);
   CHECK(sent_count == 5 && sp_sent(1, start, 2) && sp_sent_command(0, 0x03, 0x30),
         "switch_pro: the reports not started, or the full report not asked for");
   CHECK(!memcmp(sent + 2, neutral, 8), "switch_pro: a command's rumble is not at rest");

   /* the commands, each on its answer: rumble on, lights for player 3,
    * the flash */
   deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x03, NULL, 0));
   CHECK(sp_sent_command(0, 0x48, 0x01), "switch_pro: rumble not turned on");
   deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x48, NULL, 0));
   CHECK(sp_sent_command(0, 0x30, 0x07), "switch_pro: player 3's lights not set");
   deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x30, NULL, 0));
   CHECK(sp_sent_command(0, 0x10, 0x3d) && sent[12] == 0x60 && sent[15] == 18,
         "switch_pro: the factory calibration not read");

   /* left: reach up 0x500/0x600, centre 0x7f0/0x810, reach down
    * 0x400/0x480; right: centre 0x800/0x800, unwritten reach */
   memset(flash, 0, sizeof(flash));
   flash[0] = 0x3d; flash[1] = 0x60; flash[4] = 18;
   sp_put_pair(flash + 5,      0x500, 0x600);
   sp_put_pair(flash + 5 + 3,  0x7f0, 0x810);
   sp_put_pair(flash + 5 + 6,  0x400, 0x480);
   sp_put_pair(flash + 5 + 9,  0x800, 0x800);
   sp_put_pair(flash + 5 + 12, 0xfff, 0xfff);
   sp_put_pair(flash + 5 + 15, 0xfff, 0xfff);
   deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x10, flash, 5 + 18));
   CHECK(sp_sent_command(0, 0x10, 0x10) && sent[12] == 0x80 && sent[15] == 22,
         "switch_pro: the user's calibration not read");

   /* the user calibrated only the right stick: centre 0x900 */
   memset(flash, 0xff, sizeof(flash));
   flash[0] = 0x10; flash[1] = 0x80; flash[2] = 0; flash[3] = 0; flash[4] = 22;
   flash[5 + 11] = 0xb2; flash[5 + 12] = 0xa1;
   sp_put_pair(flash + 5 + 13, 0x900, 0x900);
   sp_put_pair(flash + 5 + 16, 0x300, 0x300);
   sp_put_pair(flash + 5 + 19, 0x300, 0x300);
   deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x10, flash, 5 + 22));
   CHECK(sp_sent_command(0, 0x10, 0x20) && sent[12] == 0x60 && sent[15] == 24,
         "switch_pro: the motion sensors' calibration not read");

   /* the factory's: accelerometer origin 0 and sensitivity 16384 - a g
    * in 4096 - and gyroscope origins 10, -20 and 30, sensitivity 13371
    * past them */
   {
      static const int16_t cal[12] = { 0, 0, 0, 16384, 16384, 16384,
         10, -20, 30, 10 + 13371, -20 + 13371, 30 + 13371 };
      unsigned k;
      memset(flash, 0, sizeof(flash));
      flash[0] = 0x20; flash[1] = 0x60; flash[4] = 24;
      for (k = 0; k < 12; k++)
      {
         flash[5 + 2 * k]     = (uint8_t)(cal[k] & 0xff);
         flash[5 + 2 * k + 1] = (uint8_t)((uint16_t)cal[k] >> 8);
      }
      deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x10, flash, 5 + 24));
   }
   CHECK(sp_sent_command(0, 0x10, 0x26) && sent[12] == 0x80 && sent[15] == 20,
         "switch_pro: the user's motion calibration not read");
   /* not calibrated by the user */
   memset(flash, 0xff, sizeof(flash));
   flash[0] = 0x26; flash[1] = 0x80; flash[2] = 0; flash[3] = 0; flash[4] = 20;
   count = sent_count;
   deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x10, flash, 5 + 20));
   CHECK(sent_count == count, "switch_pro: something sent after the last command");

   /* sticks: up positive becomes down positive; the factory's reach is
    * taken at seven tenths */
   st.stick[0] = 0x7f0 + 0x500 * 7 / 10;
   st.stick[1] = 0x810 - 0x480 * 7 / 10;
   st.stick[2] = 0x900;
   st.stick[3] = 0x900 + 0x300 * 7 / 10;
   deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   CHECK(ifc->get_axis(dev, 0) == 32767, "switch_pro: left x at its reach is %d", ifc->get_axis(dev, 0));
   CHECK(ifc->get_axis(dev, 1) == 32767, "switch_pro: left y down is %d", ifc->get_axis(dev, 1));
   CHECK(ifc->get_axis(dev, 2) == 0, "switch_pro: the user's centre not taken (%d)", ifc->get_axis(dev, 2));
   CHECK(ifc->get_axis(dev, 3) == -32767, "switch_pro: right y up is %d", ifc->get_axis(dev, 3));
   st.stick[0] = 0xfff;
   deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   CHECK(ifc->get_axis(dev, 0) == 32767, "switch_pro: a stick past its reach is %d", ifc->get_axis(dev, 0));
   st.stick[0] = 0x7f0 + 0x500 * 7 / 10;
   deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   CHECK(ifc->get_axis(dev, 0) == 0x500 * 7 / 10 * 32767 / (0xfff - 0x7f0),
         "switch_pro: the reach did not widen (%d)", ifc->get_axis(dev, 0));
   CHECK(ifc->get_axis(dev, 4) == 0, "switch_pro: an axis past the sticks");

   /* buttons, from every report that carries them */
   srand(23);
   for (n = 0; n < 20000; n++)
   {
      static const uint8_t ids[5] = { 0x30, 0x31, 0x32, 0x33, 0x21 };
      sp_random(&st);
      deliver(ifc, dev, p, (uint16_t)sp_report(p, ids[n % 5], &st));
      bad += sp_mismatches(dev, &st);
   }
   CHECK(!bad, "switch_pro: %u reads disagree with the full report", bad);
   p[0] = 0x30;
   memset(p + 1, 0xff, 63);
   deliver(ifc, dev, p, 11);
   p[0] = 0x3e;
   deliver(ifc, dev, p, 64);
   CHECK(!sp_mismatches(dev, &st), "switch_pro: a short or foreign report was taken");

   /* rumble: from the poll, the strong motor the left hand's, held
    * while it lasts, at rest once */
   deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   count = sent_count;
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0xffff);
   ifc->set_rumble(dev, RETRO_RUMBLE_WEAK, 1);
   CHECK(sent_count == count, "switch_pro: rumble sent outside the poll");
   deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   CHECK(sent_count == count + 1 && sent[0] == 0x10 && sent_len == 10
         && sent[2] == 0x74 && sent[3] == 0xc8 && sent[4] == 0x3d && sent[5] == 0x72
         && sent[6] == 0x74 && sent[7] == 0x02 && sent[8] == 0xbd && sent[9] == 0x40,
         "switch_pro: rumble is not the motors asked for");
   for (n = 0; n < 12; n++)
      deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   CHECK(sent_count == count + 3, "switch_pro: held rumble not kept up (%u)", sent_count - count);
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0);
   ifc->set_rumble(dev, RETRO_RUMBLE_WEAK, 0);
   for (n = 0; n < 12; n++)
      deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   CHECK(sent_count == count + 4 && !memcmp(sent + 2, neutral, 8),
         "switch_pro: rumble not put at rest once");
   /* told its port once ready: the lights as a command of their own,
    * port 5 as the console lights it; on no port, out */
   ifc->set_player(dev, 5);
   for (n = 0; n < 2; n++)
      deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   CHECK(sp_sent_command(0, 0x30, 0x09), "switch_pro: not relit for port 5");
   count = sent_count;
   for (n = 0; n < 4; n++)
      deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   CHECK(sent_count == count, "switch_pro: lit again unchanged");
   ifc->set_player(dev, 0);
   for (n = 0; n < 2; n++)
      deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   CHECK(sp_sent_command(0, 0x30, 0x00), "switch_pro: lit on no port");
   CHECK(!strcmp(ifc->get_name(dev), "Pro Controller"), "switch_pro: misnamed");

   /* motion: nothing until a core asks; then the sensors turned on,
    * read from the newest sample with the flash's calibration, on the
    * DualShock 4's axes; turned off when it is done */
   {
      float v[6];
      const float g  = 936.0f / 13371.0f * 3.14159265358979323846f / 180.0f;
      static const int16_t imu[6] = { 4096, 0, -8192, 10 + 1000, -20, 30 - 500 };
      unsigned k, ok = 1;
      CHECK(!ifc->sensor(dev, RETRO_SENSOR_GYROSCOPE_X, &v[0]),
            "switch_pro: motion given before a core asked");
      CHECK(ifc->set_sensor(dev, RETRO_SENSOR_ACCELEROMETER_ENABLE)
            && ifc->set_sensor(dev, RETRO_SENSOR_GYROSCOPE_ENABLE),
            "switch_pro: the sensors not taken");
      for (n = 0; n < 2; n++)
         deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
      CHECK(sp_sent_command(0, 0x40, 0x01), "switch_pro: the sensors not turned on");
      sp_report(p, 0x30, &st);
      for (k = 0; k < 6; k++)
      {
         p[13 + 2 * k]     = (uint8_t)(imu[k] & 0xff);
         p[13 + 2 * k + 1] = (uint8_t)((uint16_t)imu[k] >> 8);
      }
      deliver(ifc, dev, p, 64);
      for (k = 0; k < 6; k++)
         ok &= ifc->sensor(dev, RETRO_SENSOR_ACCELEROMETER_X + k, &v[k]);
      CHECK(ok && fabsf(v[0]) < 1e-5f && fabsf(v[1] + 1.0f) < 1e-5f
            && fabsf(v[2] + 2.0f) < 1e-5f && fabsf(v[3]) < 1e-5f
            && fabsf(v[4] - 1000.0f * g) < 1e-4f
            && fabsf(v[5] + 500.0f * g) < 1e-4f,
            "switch_pro: motion read %f %f %f / %f %f %f",
            v[0], v[1], v[2], v[3], v[4], v[5]);
      ifc->set_sensor(dev, RETRO_SENSOR_ACCELEROMETER_DISABLE);
      ifc->set_sensor(dev, RETRO_SENSOR_GYROSCOPE_DISABLE);
      for (n = 0; n < 2; n++)
         deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
      CHECK(sp_sent_command(0, 0x40, 0x00), "switch_pro: the sensors not turned off");
      CHECK(!ifc->sensor(dev, RETRO_SENSOR_ACCELEROMETER_X, &v[0]),
            "switch_pro: motion given after the core was done");
   }
   ifc->deinit(dev);

   /* told its port before its lights went: that port, not its slot */
   sent_count = 0;
   stub_hid.is_bluetooth = stub_is_bluetooth;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   ifc->set_player(dev, 2);
   deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x03, NULL, 0));
   deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x48, NULL, 0));
   CHECK(sp_sent_command(0, 0x30, 0x03), "switch_pro: setup lit its slot, not its port");
   ifc->deinit(dev);
   stub_hid.is_bluetooth = NULL;

   /* a command not answered goes again, then the next */
   sent_count = 0;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   p[0] = 0x81; p[1] = 0x02; deliver(ifc, dev, p, 64);
   p[1] = 0x03;               deliver(ifc, dev, p, 64);
   p[1] = 0x02;               deliver(ifc, dev, p, 64);
   count = sent_count;
   for (n = 0; n < 32 * 4; n++)
      deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   CHECK(sent_count == count + 4 && sp_sent_command(0, 0x48, 0x01)
         && sp_sent_command(1, 0x03, 0x30),
         "switch_pro: an unanswered command not sent again, then given up");
   ifc->deinit(dev);

   /* a pad already sending, its greeting unanswered: on to the commands */
   sent_count = 0;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   for (n = 0; n < 32 * 4; n++)
      deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   CHECK(sp_sent_command(0, 0x03, 0x30), "switch_pro: stuck on an unanswered greeting");
   ifc->deinit(dev);

   /* a pad that passes for one, as 8BitDo's do: the faster link not
    * answered and nothing sent, so the polls without a report go on by
    * the clock - on to the second greeting, then the reports; once
    * ready, reports that stop are started again */
   sent_count = 0;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   p[0] = 0x81; p[1] = 0x02; deliver(ifc, dev, p, 64);
   CHECK(sp_sent(0, baud, 2), "switch_pro: no faster link asked for");
   count = sent_count;
   ifc->idle(dev);
   ifc->idle(dev);
   CHECK(sent_count == count, "switch_pro: went on before the pad had a while");
   retro_sleep(110);
   ifc->idle(dev);
   CHECK(sent_count == count + 1 && sp_sent(0, hello, 2),
         "switch_pro: an unanswered faster link not gone past");
   p[1] = 0x02; deliver(ifc, dev, p, 64);
   CHECK(sp_sent(1, start, 2) && sp_sent_command(0, 0x03, 0x30),
         "switch_pro: the reports not started after the second greeting");
   deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x03, NULL, 0));
   deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x48, NULL, 0));
   deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x30, NULL, 0));
   for (n = 0; n < 4; n++)
      deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x10, NULL, 0));
   deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   count = sent_count;
   ifc->idle(dev);
   retro_sleep(110);
   ifc->idle(dev);
   CHECK(sent_count == count + 1 && sp_sent(0, start, 2),
         "switch_pro: stopped reports not started again");
   deliver(ifc, dev, p, (uint16_t)sp_report(p, 0x30, &st));
   ifc->idle(dev);
   CHECK(sent_count == count + 1, "switch_pro: started again with reports coming");
   ifc->deinit(dev);

   /* a pad that answers nothing at all: greeted a few times, then its
    * reports started anyway */
   sent_count = 0;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   ifc->idle(dev);
   for (n = 0; n < SWITCH_PRO_TEST_TRIES; n++)
   {
      retro_sleep(110);
      ifc->idle(dev);
   }
   CHECK(sp_sent(1, start, 2) && sp_sent_command(0, 0x03, 0x30),
         "switch_pro: a silent pad's reports never started");
   ifc->deinit(dev);

   /* Bluetooth the driver says: full reports asked for first */
   sent_count            = 0;
   stub_hid.is_bluetooth = stub_is_bluetooth;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   CHECK(sent_count == 1 && sp_sent_command(0, 0x03, 0x30),
         "switch_pro: Bluetooth not asked for full reports");
   deliver(ifc, dev, p, (uint16_t)sp_reply(p, &st, 0x03, NULL, 0));
   CHECK(sp_sent_command(0, 0x48, 0x01), "switch_pro: Bluetooth's commands stopped");
   ifc->deinit(dev);
   stub_hid.is_bluetooth = NULL;

   /* Bluetooth the driver cannot say: the short report tells, and is read */
   sent_count = 0;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   memset(p, 0, sizeof(p));
   p[0] = 0x3f;
   p[1] = 0x01 | 0x40;           /* B, ZL */
   p[2] = 0x10;                  /* Home */
   p[3] = 0x02;                  /* right */
   p[4] = 0xff; p[5] = 0xbf;     /* left x: half way right */
   p[6] = 0x00; p[7] = 0x80;
   p[8] = 0x00; p[9] = 0x00;     /* right x: all the way left */
   p[10] = 0x00; p[11] = 0x80;
   deliver(ifc, dev, p, 12);
   CHECK(sp_sent_command(0, 0x03, 0x30), "switch_pro: the short report did not tell Bluetooth");
   CHECK(ifc->button(dev, RETRO_DEVICE_ID_JOYPAD_B) && ifc->button(dev, RETRO_DEVICE_ID_JOYPAD_L2)
         && ifc->button(dev, RETRO_DEVICE_ID_JOYPAD_RIGHT) && ifc->button(dev, 16)
         && !ifc->button(dev, RETRO_DEVICE_ID_JOYPAD_A) && !ifc->button(dev, RETRO_DEVICE_ID_JOYPAD_UP),
         "switch_pro: the short report's buttons misread");
   CHECK(ifc->get_axis(dev, 0) > 32000 && ifc->get_axis(dev, 1) == 0
         && ifc->get_axis(dev, 2) == -32767,
         "switch_pro: the short report's sticks misread (%d %d %d)",
         ifc->get_axis(dev, 0), ifc->get_axis(dev, 1), ifc->get_axis(dev, 2));
   ifc->deinit(dev);

   /* behind a Bluetooth stack's header: read, nothing more sent */
   sent_count = 0;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   count = sent_count;
   for (n = 0; n < 200; n++)
   {
      sp_random(&st);
      sp_report(p + 1, 0x30, &st);
      p[0] = 0xa1;
      deliver(ifc, dev, p, 65);
      bad += sp_mismatches(dev, &st);
   }
   CHECK(!bad, "switch_pro: reports behind a stack's header misread");
   CHECK(sent_count == count, "switch_pro: output went behind a Bluetooth stack's header");
   ifc->deinit(dev);

   printf("[%s] switch_pro: greeted on USB step by step, commands one at a time"
         " and again when unanswered, the flash's calibration, full and short reports"
         " read as laid out, rumble from the poll and kept up\n",
         failures == had ? "pass" : "fail");
}

/* ---- Switch 2 Pro Controller: its layout and rumble ----------- */

static void test_switch2_pro(void)
{
   static const struct { unsigned byte, bit, id; } map[] = {
      { 5, 0x01, RETRO_DEVICE_ID_JOYPAD_Y },
      { 5, 0x02, RETRO_DEVICE_ID_JOYPAD_X },
      { 5, 0x04, RETRO_DEVICE_ID_JOYPAD_B },
      { 5, 0x08, RETRO_DEVICE_ID_JOYPAD_A },
      { 5, 0x40, RETRO_DEVICE_ID_JOYPAD_R },
      { 5, 0x80, RETRO_DEVICE_ID_JOYPAD_R2 },
      { 6, 0x01, RETRO_DEVICE_ID_JOYPAD_SELECT },
      { 6, 0x02, RETRO_DEVICE_ID_JOYPAD_START },
      { 6, 0x04, RETRO_DEVICE_ID_JOYPAD_R3 },
      { 6, 0x08, RETRO_DEVICE_ID_JOYPAD_L3 },
      { 6, 0x10, 16 },
      { 6, 0x20, 17 },
      { 6, 0x40, 18 },
      { 7, 0x01, RETRO_DEVICE_ID_JOYPAD_DOWN },
      { 7, 0x02, RETRO_DEVICE_ID_JOYPAD_UP },
      { 7, 0x04, RETRO_DEVICE_ID_JOYPAD_RIGHT },
      { 7, 0x08, RETRO_DEVICE_ID_JOYPAD_LEFT },
      { 7, 0x40, RETRO_DEVICE_ID_JOYPAD_L },
      { 7, 0x80, RETRO_DEVICE_ID_JOYPAD_L2 },
      { 8, 0x01, 19 },
      { 8, 0x02, 20 }
   };
   const pad_connection_interface_t *ifc = &pad_connection_switch2_pro;
   uint8_t p[64];
   unsigned n, k, bad = 0, had = failures, count;
   int dummy_connection;
   void *dev;

   sent_count = 0;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   CHECK(dev != NULL, "switch2_pro: no device");
   if (!dev)
      return;
   CHECK(sent_count == 0, "switch2_pro: something sent at start");

   /* buttons */
   srand(31);
   for (n = 0; n < 20000; n++)
   {
      input_bits_t bits;
      uint32_t want = 0;
      for (k = 0; k < 64; k++)
         p[k] = (uint8_t)rand();
      for (k = 0; k < sizeof(map) / sizeof(map[0]); k++)
         if (p[map[k].byte] & map[k].bit)
            want |= 1u << map[k].id;
      deliver(ifc, dev, p, 64);
      ifc->get_buttons(dev, &bits);
      for (k = 0; k < 21; k++)
         if (     (unsigned)ifc->button(dev, k) != ((want >> k) & 1)
               || (unsigned)BIT256_GET(bits, k) != ((want >> k) & 1))
            bad++;
      if ((unsigned)BIT256_GET(bits, RARCH_MENU_TOGGLE) != ((want >> 16) & 1))
         bad++;
   }
   CHECK(!bad, "switch2_pro: %u reads disagree with the report", bad);
   ifc->deinit(dev);

   /* sticks: centred, full right, full up; down positive out */
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   memset(p, 0, sizeof(p));
   sp_put_pair(p + 11, 2048, 2048);
   sp_put_pair(p + 14, 2048 + 1200, 2048 + 1200);
   deliver(ifc, dev, p, 64);
   CHECK(ifc->get_axis(dev, 0) == 0 && ifc->get_axis(dev, 1) == 0,
         "switch2_pro: a centred stick is not at rest");
   CHECK(ifc->get_axis(dev, 2) == 32767 && ifc->get_axis(dev, 3) == -32767,
         "switch2_pro: the right stick right and up is %d %d",
         ifc->get_axis(dev, 2), ifc->get_axis(dev, 3));
   sp_put_pair(p + 11, 2048 - 1800, 2048);
   deliver(ifc, dev, p, 64);
   sp_put_pair(p + 11, 2048 - 1200, 2048);
   deliver(ifc, dev, p, 64);
   CHECK(ifc->get_axis(dev, 0) == -1200 * 32767 / 1800,
         "switch2_pro: the reach did not widen (%d)", ifc->get_axis(dev, 0));
   CHECK(ifc->get_axis(dev, 4) == 0, "switch2_pro: an axis past the sticks");

   /* a short report is not one */
   memset(p, 0xff, sizeof(p));
   deliver(ifc, dev, p, 63);
   CHECK(!ifc->button(dev, RETRO_DEVICE_ID_JOYPAD_A), "switch2_pro: a short report was taken");

   /* rumble: from the poll, both hands alike, kept up while held, at
    * rest once */
   memset(p, 0, sizeof(p));
   for (n = 0; n < 4; n++)
      deliver(ifc, dev, p, 64);
   count = sent_count;
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0xffff);
   ifc->set_rumble(dev, RETRO_RUMBLE_WEAK, 0x8000);
   CHECK(sent_count == count, "switch2_pro: rumble sent outside the poll");
   deliver(ifc, dev, p, 64);
   {
      /* weak 0x8000 -> 14500 (0x38a4), strong 29000 (0x7148) */
      static const uint8_t want[7] = { 0x02, 0x50, 0x87, 0x89, 0x23, 0x51, 0x71 };
      CHECK(sent_count == count + 1 && sent_len == 64 && !memcmp(sent, want, 7)
            && !memcmp(sent + 0x11, sent + 1, 6),
            "switch2_pro: rumble is not the motors asked for"
            " (%02x %02x %02x %02x %02x %02x %02x)",
            sent[0], sent[1], sent[2], sent[3], sent[4], sent[5], sent[6]);
   }
   for (n = 0; n < 9; n++)
      deliver(ifc, dev, p, 64);
   CHECK(sent_count == count + 4 && (sent[1] & 0x0f) == 3,
         "switch2_pro: held rumble not kept up, numbered (%u)", sent_count - count);
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0);
   ifc->set_rumble(dev, RETRO_RUMBLE_WEAK, 0);
   for (n = 0; n < 12; n++)
      deliver(ifc, dev, p, 64);
   CHECK(sent_count == count + 5 && sent[3] == 0x01 && sent[4] == 0x20
         && sent[5] == 0x11 && sent[6] == 0x00,
         "switch2_pro: rumble not put at rest once");
   CHECK(!strcmp(ifc->get_name(dev), "Nintendo Switch 2 Pro Controller"),
         "switch2_pro: misnamed");
   ifc->deinit(dev);

   printf("[%s] switch2_pro: buttons and grips read as laid out on 20000 reports,"
         " sticks centred and widened, rumble from the poll on both hands and kept up\n",
         failures == had ? "pass" : "fail");
}

/* ---- Wii U Pro: the old handler's decode ----------------------- */

/* the old packet handler's buttons, from a copy of the report it
 * inverted in place, and the old button()'s answer for each */
static int ref_wiiupro_button(const uint8_t *p, unsigned key)
{
   uint8_t c = (uint8_t)(p[0x0C] ^ 0xFF);
   uint8_t d = (uint8_t)(p[0x0D] ^ 0xFF);
   uint8_t e = (uint8_t)(p[0x0E] ^ 0xFF);
   switch (key)
   {
      case RETRO_DEVICE_ID_JOYPAD_R3:     return (e & 0x01) != 0;
      case RETRO_DEVICE_ID_JOYPAD_L3:     return (e & 0x02) != 0;
      case RETRO_DEVICE_ID_JOYPAD_START:  return (c & 0x04) != 0;
      case RETRO_DEVICE_ID_JOYPAD_SELECT: return (c & 0x10) != 0;
      case RETRO_DEVICE_ID_JOYPAD_R2:     return (d & 0x04) != 0;
      case RETRO_DEVICE_ID_JOYPAD_L2:     return (d & 0x80) != 0;
      case RETRO_DEVICE_ID_JOYPAD_R:      return (c & 0x02) != 0;
      case RETRO_DEVICE_ID_JOYPAD_L:      return (c & 0x20) != 0;
      case RETRO_DEVICE_ID_JOYPAD_X:      return (d & 0x08) != 0;
      case RETRO_DEVICE_ID_JOYPAD_A:      return (d & 0x10) != 0;
      case RETRO_DEVICE_ID_JOYPAD_B:      return (d & 0x40) != 0;
      case RETRO_DEVICE_ID_JOYPAD_Y:      return (d & 0x20) != 0;
      case RETRO_DEVICE_ID_JOYPAD_LEFT:   return (d & 0x02) != 0;
      case RETRO_DEVICE_ID_JOYPAD_RIGHT:  return (c & 0x80) != 0;
      case RETRO_DEVICE_ID_JOYPAD_DOWN:   return (c & 0x40) != 0;
      case RETRO_DEVICE_ID_JOYPAD_UP:     return (d & 0x01) != 0;
      case RARCH_MENU_TOGGLE:             return (c & 0x08) != 0;
   }
   return 0;
}

static void test_wiiupro(void)
{
   uint8_t p[64], before[64];
   unsigned i, k, bad = 0, touched = 0, had = failures;
   const pad_connection_interface_t *ifc = &pad_connection_wiiupro;
   int dummy_connection;
   void *dev = ifc->init(&dummy_connection, 0, &stub_hid);

   CHECK(dev != NULL, "wiiupro: no device");
   if (!dev)
      return;
   srand(5);
   for (i = 0; i < 20000; i++)
   {
      input_bits_t bits;
      for (k = 0; k < sizeof(p); k++)
         p[k] = (uint8_t)rand();
      /* report 0x34, as a Bluetooth stack gives it with its header,
       * or with its ID first as the other drivers do */
      p[0] = 0xa1;
      p[1] = 0x34;
      memcpy(before, p, sizeof(p));
      if (i & 1)
         deliver(ifc, dev, p, 22);
      else
         deliver(ifc, dev, p + 1, 21);
      if (memcmp(before, p, sizeof(p)))
         touched++;
      ifc->get_buttons(dev, &bits);
      for (k = 0; k < RARCH_BIND_LIST_END; k++)
      {
         int want = ref_wiiupro_button(p, k);
         if (     !!ifc->button(dev, (uint16_t)k) != want
               || !!BIT256_GET(bits, k) != want)
            bad++;
      }
   }
   CHECK(!bad, "wiiupro: %u reads disagree with the old handler", bad);
   CHECK(!touched, "wiiupro: the report was changed in %u of the handler's reads", touched);
   /* a report too short to hold the buttons is not read, nor another
    * report */
   memset(p, 0, sizeof(p));
   p[0] = 0xa1;
   p[1] = 0x34;
   deliver(ifc, dev, p, WIIUPRO_SHORT);
   CHECK(ifc->button(dev, RETRO_DEVICE_ID_JOYPAD_UP) == ref_wiiupro_button(before, RETRO_DEVICE_ID_JOYPAD_UP),
         "wiiupro: a short report was read");
   p[1] = 0x30;
   deliver(ifc, dev, p, 22);
   CHECK(ifc->button(dev, RETRO_DEVICE_ID_JOYPAD_UP) == ref_wiiupro_button(before, RETRO_DEVICE_ID_JOYPAD_UP),
         "wiiupro: a report other than 0x34 was read");
   ifc->deinit(dev);

   /* its mode asked for with the HIDP header for a Bluetooth stack, and
    * as report 0x12 alone for a driver that takes the ID first */
   sent_len = 0;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   CHECK(sent_len == 4 && sent[0] == 0xa2 && sent[1] == 0x12 && sent[3] == 0x34,
         "wiiupro: mode not asked for after the header");
   ifc->deinit(dev);
   stub_hid.ident = "hidraw";
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   CHECK(sent_len == 3 && sent[0] == 0x12 && sent[2] == 0x34,
         "wiiupro: mode not asked for as report 0x12 alone");
   ifc->deinit(dev);
   stub_hid.ident = NULL;
   printf("[%s] wiiupro: every button and the set as the old handler, on 20000 reports with and without a stack's header, the report left as it came; a short one or another report not read; its mode asked for as the driver frames reports\n",
         failures == had ? "pass" : "fail");
}

/* ---- DualShock 3: the copying handler's axes ------------------ */

static const uint8_t ref_ds3_pressure_byte[12] = {
   18, 19, 24, 23, 25, 22, 20, 21, 14, 16, 17, 15
};

/* the old ds3_update_analog_state() and ds3_get_axis(), on the old
 * analog_state and pressures */
static int16_t ref_ds3_axis(const uint8_t *report, uint32_t axis)
{
   union { uint32_t encoded; int16_t axis[2]; } joyaxis;
   int16_t analog_state[3][2];
   axis_data ad = {0};
   int p;

   memset(analog_state, 0, sizeof(analog_state));
   for (p = 0; p < 4; p++)
      analog_state[p / 2][(p % 2) ? 0 : 1] =
         (int16_t)(report[6 + p] * 257 - 32768);
   joyaxis.encoded = axis;
   if (axis >= 4 && axis < 16)
   {
      unsigned b = report[ref_ds3_pressure_byte[axis - 4]];
      return (int16_t)((b << 7) | (b >> 1));
   }
   gamepad_read_axis_data(axis, &ad);
   if (ad.axis >= 4)
      return 0;
   if (joyaxis.axis[0] < 0 || joyaxis.axis[1] < 0)
      return gamepad_get_axis_value(analog_state, &ad);
   return gamepad_get_axis_value_raw(analog_state, &ad, false);
}

static unsigned set_reports;
static int32_t stub_count_set_report(void *handle, uint8_t type, uint8_t id,
      uint8_t *data, size_t len)
{
   (void)handle; (void)id; (void)data;
   if (type == HID_REPORT_OUTPUT)
      set_reports++;
   return (int32_t)len;
}

static void test_ds3(void)
{
   static const unsigned map[17] = {
      RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_L3,
      RETRO_DEVICE_ID_JOYPAD_R3,     RETRO_DEVICE_ID_JOYPAD_START,
      RETRO_DEVICE_ID_JOYPAD_UP,     RETRO_DEVICE_ID_JOYPAD_RIGHT,
      RETRO_DEVICE_ID_JOYPAD_DOWN,   RETRO_DEVICE_ID_JOYPAD_LEFT,
      RETRO_DEVICE_ID_JOYPAD_L2,     RETRO_DEVICE_ID_JOYPAD_R2,
      RETRO_DEVICE_ID_JOYPAD_L,      RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_X,      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_B,      RETRO_DEVICE_ID_JOYPAD_Y,
      16
   };
   uint8_t p[64];
   unsigned i, k, bad = 0, had = failures;
   const pad_connection_interface_t *ifc = &pad_connection_ps3;
   int dummy_connection;
   void *dev;

   stub_hid.get_report = stub_ds3_get_report;
   dev                 = ifc->init(&dummy_connection, 0, &stub_hid);
   stub_hid.get_report = NULL;
   CHECK(dev != NULL, "ds3: no device");
   if (!dev)
      return;
   srand(3);
   for (i = 0; i < 20000; i++)
   {
      uint32_t keys;
      for (k = 0; k < sizeof(p); k++)
         p[k] = (uint8_t)rand();
      p[0] = 0x01;
      if (p[1] == 0xff) /* not a state, as SDL found */
         p[1] = 0;
      deliver(ifc, dev, p, 49);
      keys = p[2] | (p[3] << 8) | ((p[4] & 0x01) << 16);
      for (k = 0; k < 17; k++)
         if (!!ifc->button(dev, (uint16_t)map[k]) != !!(keys & (1u << k)))
            bad++;
      for (k = 0; k < 16; k++)
         if (ifc->get_axis(dev, k) != ref_ds3_axis(p, k))
            bad++;
      /* a stick's direction asked as bound, as wiiu_hid's state() asks */
      for (k = 0; k < 4; k++)
         if (     ifc->get_axis(dev, AXIS_NEG(k)) != ref_ds3_axis(p, AXIS_NEG(k))
               || ifc->get_axis(dev, AXIS_POS(k)) != ref_ds3_axis(p, AXIS_POS(k)))
            bad++;
   }
   CHECK(!bad, "ds3: %u reads disagree with the copying handler", bad);
   ifc->deinit(dev);
   /* ShanWan's: no output report at init; the converter's 7-byte
    * report read as SDL reads it; a state of 0xFF not taken */
   {
      int32_t (*set)(void*, uint8_t, uint8_t, uint8_t*, size_t) = stub_hid.set_report;
      stub_hid.get_report = stub_ds3_get_report;
      stub_hid.set_report = stub_count_set_report;
      set_reports = 0;
      dev = pad_connection_ps3_shanwan.init(&dummy_connection, 0, &stub_hid);
      stub_hid.get_report = NULL;
      CHECK(dev && !set_reports, "ds3: an output report sent to a ShanWan pad at init");
      memset(p, 0, sizeof(p));
      p[0] = 0x00; p[1] = 0xff; p[2] = 0x80; p[3] = 0x7f;
      p[4] = 0x02 | 0x40;  /* right, cross */
      p[5] = 0x04 | 0x20;  /* L2, start */
      deliver(&pad_connection_ps3_shanwan, dev, p, 7);
      CHECK(dev
            && pad_connection_ps3_shanwan.button(dev, RETRO_DEVICE_ID_JOYPAD_RIGHT)
            && pad_connection_ps3_shanwan.button(dev, RETRO_DEVICE_ID_JOYPAD_B)
            && pad_connection_ps3_shanwan.button(dev, RETRO_DEVICE_ID_JOYPAD_L2)
            && pad_connection_ps3_shanwan.button(dev, RETRO_DEVICE_ID_JOYPAD_START)
            && !pad_connection_ps3_shanwan.button(dev, RETRO_DEVICE_ID_JOYPAD_A)
            && pad_connection_ps3_shanwan.get_axis(dev, 0) == 128 * 257 - 32768
            && pad_connection_ps3_shanwan.get_axis(dev, 2) == -32768
            && pad_connection_ps3_shanwan.get_axis(dev, 3) == 32767
            && pad_connection_ps3_shanwan.get_axis(dev, 4) == 0x7fff
            && pad_connection_ps3_shanwan.get_axis(dev, 6) == 0x7fff
            && pad_connection_ps3_shanwan.get_axis(dev, 7) == 0,
            "ds3: the converter's short report misread");
      memset(p, 0, sizeof(p));
      p[0] = 0x01; p[1] = 0xff; p[3] = 0xff;
      deliver(&pad_connection_ps3_shanwan, dev, p, 49);
      CHECK(dev && pad_connection_ps3_shanwan.button(dev, RETRO_DEVICE_ID_JOYPAD_B),
            "ds3: a state of 0xFF taken");
      if (dev)
         pad_connection_ps3_shanwan.deinit(dev);
      stub_hid.set_report = set;
   }
   printf("[%s] ds3: every button and axis, plain and bound, as the copying handler, on 20000 reports; ShanWan's pads and converter\n",
         failures == had ? "pass" : "fail");
}

/* ---- 8BitDo SN30 Pro: its reports' layout ---------------------- */

static int32_t stub_8bitdo_feature(void *handle, uint8_t type, uint8_t id,
      uint8_t *data, size_t len)
{
   (void)handle;
   if (type != HID_REPORT_FEATURE || id != 0x06)
      return -1;
   memset(data, 0, len);
   data[0] = 0x06;
   return 16;
}

static int16_t ref_8bitdo_stick(uint8_t b)
{
   int v = (int)b - 0x7f;
   if (!v)
      return 0;
   return (int16_t)(v < 0 ? v * 32768 / 127 : v * 32767 / 128);
}

static unsigned ref_8bitdo_hat(uint8_t h)
{
   /* up 1, down 2, left 4, right 8; 0 is up, clockwise */
   static const uint8_t way[8] = { 1, 9, 8, 10, 2, 6, 4, 5 };
   return h < 8 ? way[h] : 0;
}

static unsigned mismatches_8bitdo(const pad_connection_interface_t *ifc,
      void *dev, const uint8_t *r, bool old)
{
   const uint8_t *b = old ? r : r + 8;
   uint32_t want    = (uint32_t)b[0] | ((uint32_t)b[1] << 8);
   unsigned hat     = ref_8bitdo_hat(old ? r[2] : r[1]);
   const uint8_t *s = old ? r + 3 : r + 2;
   int16_t axes[6];
   input_bits_t bits;
   unsigned k, bad = 0;

   axes[0] = ref_8bitdo_stick(s[0]);
   axes[1] = ref_8bitdo_stick(s[1]);
   axes[4] = ref_8bitdo_stick(s[2]);
   axes[5] = ref_8bitdo_stick(s[3]);
   if (old)
   {
      axes[2] = (r[1] & 1) ? 0x7fff : 0;
      axes[3] = (r[1] & 2) ? 0x7fff : 0;
   }
   else
   {
      axes[2] = (int16_t)((r[7] << 7) | (r[7] >> 1));
      axes[3] = (int16_t)((r[6] << 7) | (r[6] >> 1));
   }
   ifc->get_buttons(dev, &bits);
   for (k = 0; k < 32; k++)
      if (     !!ifc->button(dev, (uint16_t)k) != !!(want & (1u << k))
            || !!BIT256_GET(bits, k) != !!(want & (1u << k)))
         bad++;
   if (!!BIT256_GET(bits, RARCH_MENU_TOGGLE) != !!(b[1] & 0x10))
      bad++;
   for (k = 0; k < 4; k++)
      if (!!ifc->button(dev, (uint16_t)HAT_MAP(0, HAT_UP_MASK >> k))
            != !!(hat & (1u << k)))
         bad++;
   /* hat 1 is not there */
   if (ifc->button(dev, (uint16_t)HAT_MAP(1, HAT_UP_MASK)))
      bad++;
   for (k = 0; k < 6; k++)
      if (ifc->get_axis(dev, k) != axes[k])
         bad++;
   return bad;
}

static void test_8bitdo(void)
{
   uint8_t p[65], last[65];
   unsigned n, i, bad = 0, had = failures, count;
   bool last_old = false, have = false;
   const pad_connection_interface_t *ifc = &pad_connection_8bitdo_sn30_pro;
   int dummy_connection;
   void *dev;

   stub_hid.get_report = stub_8bitdo_feature;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   CHECK(dev != NULL, "8bitdo: no device");
   if (!dev)
      return;
   CHECK(!strcmp(ifc->get_name(dev), "8Bitdo SN30 Pro"),
         "8bitdo: named '%s', not as its hid profiles", ifc->get_name(dev));

   srand(11);
   for (n = 0; n < 40000; n++)
   {
      static const uint8_t ids[5] = { 0x01, 0x03, 0x04, 0x02, 0x05 };
      unsigned size;
      bool old = (rand() % 4) == 0;
      for (i = 0; i < sizeof(p); i++)
         p[i] = (uint8_t)rand();
      if (rand() & 1)
         p[old ? 2 : 1] = (uint8_t)(rand() % 10); /* the hat's ways, rest */
      for (i = 0; i < 4; i++)
         if (!(rand() % 4))
            p[(old ? 3 : 2) + i] = 0x7f;
      if (old)
         size = 9;
      else
      {
         p[0] = ids[rand() % 5];
         size = 10 + (unsigned)rand() % 55;
      }
      /* now and then a report too short to be one */
      if (!(rand() % 16))
         size = (unsigned)rand() % 9;
      deliver(ifc, dev, p, (uint16_t)size);
      if (     size == 9
            || (size >= 10 && (p[0] == 1 || p[0] == 3 || p[0] == 4)))
      {
         memcpy(last, p, sizeof(last));
         last_old = size == 9;
         have     = true;
      }
      if (have)
         bad += mismatches_8bitdo(ifc, dev, last, last_old);
   }
   CHECK(!bad, "8bitdo: %u reads disagree with the reports' layout", bad);

   /* the ends of a stick */
   memset(p, 0, sizeof(p));
   p[0] = 0x04; p[1] = 8; p[2] = 0x00; p[3] = 0xff;
   deliver(ifc, dev, p, 15);
   CHECK(ifc->get_axis(dev, 0) == -32768 && ifc->get_axis(dev, 1) == 32767,
         "8bitdo: a stick's ends read %d and %d",
         ifc->get_axis(dev, 0), ifc->get_axis(dev, 1));

   /* rumble: from the poll, once for each change */
   count = sent_count;
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0xC000);
   ifc->set_rumble(dev, RETRO_RUMBLE_WEAK, 0x4000);
   CHECK(sent_count == count, "8bitdo: rumble was sent outside the poll");
   deliver(ifc, dev, p, 15);
   CHECK(sent_count == count + 1 && sent_len == 5 && sent[0] == 0x05
         && sent[1] == 0xC0 && sent[2] == 0x40 && !sent[3] && !sent[4],
         "8bitdo: rumble report not 05 C0 40 00 00");
   deliver(ifc, dev, p, 15);
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0xC000);
   deliver(ifc, dev, p, 15);
   CHECK(sent_count == count + 1, "8bitdo: rumble sent again unchanged");
   ifc->deinit(dev);

   /* firmware with no mode for rumble: nothing sent */
   stub_hid.get_report = NULL;
   dev   = ifc->init(&dummy_connection, 0, &stub_hid);
   count = sent_count;
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0xffff);
   deliver(ifc, dev, p, 15);
   CHECK(sent_count == count, "8bitdo: rumble sent with no mode for it");
   ifc->deinit(dev);

   /* behind a Bluetooth stack's header: read, nothing sent */
   stub_hid.get_report = stub_8bitdo_feature;
   dev   = ifc->init(&dummy_connection, 0, &stub_hid);
   count = sent_count;
   memset(p, 0, sizeof(p));
   p[0] = 0xa1; p[1] = 0x01; p[2] = 8; p[9] = 0x01; p[10] = 0x10;
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0xffff);
   deliver(ifc, dev, p, 16);
   CHECK(ifc->button(dev, 0) && !ifc->button(dev, 1)
         && ifc->view(dev)->buttons.data[0] & (1u << 12),
         "8bitdo: a report behind a stack's header was not read");
   CHECK(sent_count == count, "8bitdo: rumble sent behind a stack's header");
   ifc->deinit(dev);
   stub_hid.get_report = NULL;

   /* the SF30 Pro: the same reports, its own name */
   dev = pad_connection_8bitdo_sf30_pro.init(&dummy_connection, 0, &stub_hid);
   CHECK(dev && !strcmp(pad_connection_8bitdo_sf30_pro.get_name(dev),
            "8Bitdo SF30 Pro"), "8bitdo: the SF30 Pro not named as its hid profile");
   memset(p, 0, sizeof(p));
   p[0] = 0x03; p[1] = 2; p[2] = 0xff; p[8] = 0x02;
   deliver(&pad_connection_8bitdo_sf30_pro, dev, p, 15);
   CHECK(dev && !mismatches_8bitdo(&pad_connection_8bitdo_sf30_pro, dev, p, false),
         "8bitdo: the SF30 Pro's report read otherwise");
   if (dev)
      pad_connection_8bitdo_sf30_pro.deinit(dev);

   printf("[%s] 8bitdo: SN30 Pro and SF30 Pro, buttons, home, hat, sticks and triggers as both firmwares' reports lay them, on 40000 reports; rumble report 0x05 from the poll only where its mode is on\n",
         failures == had ? "pass" : "fail");
}

/* ---- motion: DualShock 4, DualSense, DualShock 3 ---------------- */

/* the sensors from @ifc's @dev against @want: nothing before a core
 * asks; then each of the six as wanted, within @tol */
static unsigned motion_mismatches(const pad_connection_interface_t *ifc,
      void *dev, const float *want, bool gyro)
{
   unsigned k, bad = 0;
   for (k = 0; k < 6; k++)
   {
      float v = 0;
      bool got = ifc->sensor(dev, RETRO_SENSOR_ACCELEROMETER_X + k, &v);
      if (k >= 3 && !gyro)
      {
         if (got)
            bad++;
         continue;
      }
      if (!got || fabsf(v - want[k]) > 1e-4f)
         bad++;
   }
   return bad;
}

static void put_le16(uint8_t *p, int16_t v)
{
   p[0] = (uint8_t)(v & 0xff);
   p[1] = (uint8_t)((uint16_t)v >> 8);
}

static void test_motion(void)
{
   static const int16_t imu[6] = { 100, -200, 300, 4096, -8192, 2048 };
   sony_pad_motion_t m;
   float want[6];
   uint8_t p[80];
   unsigned k, had = failures;
   int dummy_connection;
   void *dev;
   float v;

   for (k = 0; k < 3; k++)
   {
      m.gyro[k]  = imu[k];
      m.accel[k] = imu[3 + k];
   }
   for (k = 0; k < 6; k++)
      sony_pad_motion_value(&m, RETRO_SENSOR_ACCELEROMETER_X + k, &want[k]);

   /* DualShock 4: report 0x01 over USB from 13, report 0x11 two on */
   dev = pad_connection_ps4.init(&dummy_connection, 0, &stub_hid);
   memset(p, 0, sizeof(p));
   p[0] = 0x01; p[5] = 8;
   for (k = 0; k < 6; k++)
      put_le16(p + 13 + 2 * k, imu[k]);
   deliver(&pad_connection_ps4, dev, p, 64);
   CHECK(!pad_connection_ps4.sensor(dev, RETRO_SENSOR_GYROSCOPE_X, &v),
         "motion: the DualShock 4's given before a core asked");
   pad_connection_ps4.set_sensor(dev, RETRO_SENSOR_ACCELEROMETER_ENABLE);
   pad_connection_ps4.set_sensor(dev, RETRO_SENSOR_GYROSCOPE_ENABLE);
   deliver(&pad_connection_ps4, dev, p, 64);
   CHECK(!motion_mismatches(&pad_connection_ps4, dev, want, true),
         "motion: the DualShock 4's over USB misread");
   memmove(p + 3, p + 1, 70);
   p[0] = 0x11; p[1] = 0xc0; p[2] = 0;
   memset(p + 13 + 2, 0, 12);
   deliver(&pad_connection_ps4, dev, p, 78);
   for (k = 0; k < 6; k++)
      put_le16(p + 15 + 2 * k, imu[k]);
   deliver(&pad_connection_ps4, dev, p, 78);
   CHECK(!motion_mismatches(&pad_connection_ps4, dev, want, true),
         "motion: the DualShock 4's over Bluetooth misread");
   pad_connection_ps4.set_sensor(dev, RETRO_SENSOR_GYROSCOPE_DISABLE);
   CHECK(!pad_connection_ps4.sensor(dev, RETRO_SENSOR_GYROSCOPE_X, &v)
         && pad_connection_ps4.sensor(dev, RETRO_SENSOR_ACCELEROMETER_X, &v),
         "motion: the DualShock 4's gyroscope given after the core was done");
   pad_connection_ps4.deinit(dev);

   /* DualSense: from 16 over USB; report 0x31 one on */
   dev = pad_connection_ps5.init(&dummy_connection, 0, &stub_hid);
   pad_connection_ps5.set_sensor(dev, RETRO_SENSOR_ACCELEROMETER_ENABLE);
   pad_connection_ps5.set_sensor(dev, RETRO_SENSOR_GYROSCOPE_ENABLE);
   memset(p, 0, sizeof(p));
   p[0] = 0x01; p[8] = 8;
   for (k = 0; k < 6; k++)
      put_le16(p + 16 + 2 * k, imu[k]);
   deliver(&pad_connection_ps5, dev, p, 64);
   CHECK(!motion_mismatches(&pad_connection_ps5, dev, want, true),
         "motion: the DualSense's over USB misread");
   pad_connection_ps5.deinit(dev);
   stub_hid.is_bluetooth = stub_is_bluetooth;
   dev = pad_connection_ps5.init(&dummy_connection, 0, &stub_hid);
   stub_hid.is_bluetooth = NULL;
   pad_connection_ps5.set_sensor(dev, RETRO_SENSOR_ACCELEROMETER_ENABLE);
   pad_connection_ps5.set_sensor(dev, RETRO_SENSOR_GYROSCOPE_ENABLE);
   memset(p, 0, sizeof(p));
   p[0] = 0x31; p[9] = 8;
   for (k = 0; k < 6; k++)
      put_le16(p + 17 + 2 * k, imu[k]);
   deliver(&pad_connection_ps5, dev, p, 78);
   CHECK(!motion_mismatches(&pad_connection_ps5, dev, want, true),
         "motion: the DualSense's over Bluetooth misread");
   pad_connection_ps5.deinit(dev);

   /* Switch 2 Pro: from 0x31, once the sensor clock tells the
    * gyroscope's range: 34.8 rad/s on a microsecond clock, else 40 */
   for (k = 0; k < 2; k++)
   {
      const pad_connection_interface_t *ifc = &pad_connection_switch2_pro;
      float range = k ? 40.0f : 34.8f;
      uint32_t clock = 1000;
      unsigned n, i;

      dev = ifc->init(&dummy_connection, 0, &stub_hid);
      ifc->set_sensor(dev, RETRO_SENSOR_ACCELEROMETER_ENABLE);
      ifc->set_sensor(dev, RETRO_SENSOR_GYROSCOPE_ENABLE);
      memset(p, 0, sizeof(p));
      for (i = 0; i < 6; i++)
         put_le16(p + 0x31 + 2 * i, imu[i]);
      for (n = 0; n < 104; n++)
      {
         p[0x2b] = (uint8_t)clock;         p[0x2c] = (uint8_t)(clock >> 8);
         p[0x2d] = (uint8_t)(clock >> 16); p[0x2e] = (uint8_t)(clock >> 24);
         deliver(ifc, dev, p, 64);
         clock += k ? 5000 : 4000;
      }
      CHECK(!ifc->sensor(dev, RETRO_SENSOR_ACCELEROMETER_X, &v),
            "motion: the Switch 2 Pro's given before its clock was timed");
      deliver(ifc, dev, p, 64);
      deliver(ifc, dev, p, 64);
      want[0] =   imu[0] * 8.0f / 32767.0f;
      want[1] = -(imu[1] * 8.0f / 32767.0f);
      want[2] =   imu[2] * 8.0f / 32767.0f;
      want[3] =   imu[3] * range / 32767.0f;
      want[4] =   imu[4] * range / 32767.0f;
      want[5] =   imu[5] * range / 32767.0f;
      CHECK(!motion_mismatches(ifc, dev, want, true),
            "motion: the Switch 2 Pro's misread at %.1f rad/s", range);
      ifc->deinit(dev);
   }

   /* DualShock 3: the accelerometer alone, big-endian from 41, 113 a g
    * about 511, on SDL's axes */
   stub_hid.get_report = stub_ds3_get_report;
   dev = pad_connection_ps3.init(&dummy_connection, 0, &stub_hid);
   stub_hid.get_report = NULL;
   CHECK(!pad_connection_ps3.set_sensor(dev, RETRO_SENSOR_GYROSCOPE_ENABLE),
         "motion: a gyroscope taken for the DualShock 3");
   pad_connection_ps3.set_sensor(dev, RETRO_SENSOR_ACCELEROMETER_ENABLE);
   memset(p, 0, sizeof(p));
   p[0] = 0x01;
   p[41] = 0x02; p[42] = 0x70;  /* X 624: 1 g */
   p[43] = 0x01; p[44] = 0xff;  /* Y 511: none */
   p[45] = 0x01; p[46] = 0x8e;  /* Z 398: -1 g */
   deliver(&pad_connection_ps3, dev, p, 49);
   want[0] = 1.0f; want[1] = 0.0f; want[2] = 1.0f;
   CHECK(!motion_mismatches(&pad_connection_ps3, dev, want, false),
         "motion: the DualShock 3's misread");
   pad_connection_ps3.deinit(dev);

   printf("[%s] motion: DualShock 4 over USB and Bluetooth, DualSense over USB and Bluetooth, Switch 2 Pro on either clock, DualShock 3's accelerometer, only while asked for\n",
         failures == had ? "pass" : "fail");
}

/* ---- the pads SDL handles that were added with its ways --------- */

static int32_t stub_ds5_caps(void *handle, uint8_t type, uint8_t id,
      uint8_t *data, size_t len)
{
   (void)handle;
   if (type != HID_REPORT_FEATURE || id != 0x03 || len < 48)
      return -1;
   memset(data, 0, len);
   data[0] = 0x03;
   data[2] = 0x28;
   data[4] = 0x02 | 0x08;  /* sensors, rumble */
   return 48;
}

/* a pad that passes for a DualShock 4: sensors and rumble, its
 * gyroscope at 1/32 of a degree a second */
static int32_t stub_ds4_caps(void *handle, uint8_t type, uint8_t id,
      uint8_t *data, size_t len)
{
   (void)handle;
   if (type != HID_REPORT_FEATURE || id != 0x03 || len < 48)
      return -1;
   memset(data, 0, len);
   data[0]  = 0x03;
   data[2]  = 0x27;
   data[4]  = 0x02 | 0x08;
   data[10] = 1;
   data[12] = 32;
   return 48;
}

static void test_sdl_pads(void)
{
   uint8_t p[80];
   unsigned k, had = failures, count;
   int dummy_connection;
   void *dev;
   float v;

   /* 8BitDo Pro 2: L4 and R4 from byte 10; motion from 15 where its
    * mode with rumble is on, as SDL turns it */
   stub_hid.get_report = stub_8bitdo_feature;
   dev = pad_connection_8bitdo_pro_2.init(&dummy_connection, 0, &stub_hid);
   stub_hid.get_report = NULL;
   CHECK(dev && !strcmp(pad_connection_8bitdo_pro_2.get_name(dev), "8BitDo Pro 2"),
         "sdl pads: the Pro 2 misnamed");
   pad_connection_8bitdo_pro_2.set_sensor(dev, RETRO_SENSOR_ACCELEROMETER_ENABLE);
   pad_connection_8bitdo_pro_2.set_sensor(dev, RETRO_SENSOR_GYROSCOPE_ENABLE);
   memset(p, 0, sizeof(p));
   p[0] = 0x04; p[1] = 8; p[2] = p[3] = p[4] = p[5] = 0x7f;
   p[8]  = 0x04 | 0x20;   /* the back paddles */
   p[10] = 0x01 | 0x02;   /* L4, R4 */
   p[15] = 0x00; p[16] = 0x10;   /* accelerometer X 4096: a g */
   p[21] = 0xe8; p[22] = 0x03;   /* gyroscope X 1000 */
   deliver(&pad_connection_8bitdo_pro_2, dev, p, 33);
   CHECK(pad_connection_8bitdo_pro_2.button(dev, 2)
         && pad_connection_8bitdo_pro_2.button(dev, 5)
         && pad_connection_8bitdo_pro_2.button(dev, 16)
         && pad_connection_8bitdo_pro_2.button(dev, 17),
         "sdl pads: the Pro 2's paddles, L4 or R4 not read");
   CHECK(pad_connection_8bitdo_pro_2.sensor(dev, RETRO_SENSOR_ACCELEROMETER_Y, &v)
         && fabsf(v + 1.0f) < 1e-5f
         && pad_connection_8bitdo_pro_2.sensor(dev, RETRO_SENSOR_GYROSCOPE_Y, &v)
         && fabsf(v - 1000.0f * 2000.0f * 3.14159265f / 180.0f / 32767.0f) < 1e-4f,
         "sdl pads: the Pro 2's motion misread");
   pad_connection_8bitdo_pro_2.deinit(dev);

   /* Ultimate 2C: no rumble, no motion; Ultimate 2 Wireless: rumble
    * once its report is the long one */
   dev = pad_connection_8bitdo_ultimate_2c.init(&dummy_connection, 0, &stub_hid);
   pad_connection_8bitdo_ultimate_2c.set_rumble(dev, RETRO_RUMBLE_STRONG, 0xffff);
   count = sent_count;
   deliver(&pad_connection_8bitdo_ultimate_2c, dev, p, 11);
   CHECK(sent_count == count, "sdl pads: the Ultimate 2C rumbled");
   pad_connection_8bitdo_ultimate_2c.deinit(dev);
   dev = pad_connection_8bitdo_ultimate_2.init(&dummy_connection, 0, &stub_hid);
   pad_connection_8bitdo_ultimate_2.set_rumble(dev, RETRO_RUMBLE_STRONG, 0xffff);
   deliver(&pad_connection_8bitdo_ultimate_2, dev, p, 12);
   CHECK(sent_count == count, "sdl pads: the Ultimate 2 rumbled on its short report");
   deliver(&pad_connection_8bitdo_ultimate_2, dev, p, 34);
   CHECK(sent_count == count + 1 && sent[0] == 0x05,
         "sdl pads: the Ultimate 2 not rumbled on its long report");
   pad_connection_8bitdo_ultimate_2.deinit(dev);

   /* a pad that passes for a DualSense: nothing sent unless it says it
    * takes it; then rumble and motion as it says */
   dev = pad_connection_ps5_third_party.init(&dummy_connection, 0, &stub_hid);
   memset(p, 0, sizeof(p));
   p[0] = 0x01; p[8] = 8;
   count = sent_count;
   deliver(&pad_connection_ps5_third_party, dev, p, 64);
   CHECK(sent_count == count, "sdl pads: output sent to a pad that did not answer");
   pad_connection_ps5_third_party.deinit(dev);
   stub_hid.get_report = stub_ds5_caps;
   dev = pad_connection_ps5_third_party.init(&dummy_connection, 0, &stub_hid);
   stub_hid.get_report = NULL;
   pad_connection_ps5_third_party.set_sensor(dev, RETRO_SENSOR_GYROSCOPE_ENABLE);
   for (k = 0; k < 6; k++)
      p[16 + 2 * k] = 1;
   deliver(&pad_connection_ps5_third_party, dev, p, 64);
   CHECK(sent_count == count + 1 && sent[0] == 0x02,
         "sdl pads: output not sent to a pad that takes it");
   CHECK(pad_connection_ps5_third_party.sensor(dev, RETRO_SENSOR_GYROSCOPE_X, &v),
         "sdl pads: motion not read from a pad that has it");
   CHECK(!strcmp(pad_connection_ps5_third_party.get_name(dev),
            "DualSense Wireless Controller"),
         "sdl pads: a pad that passes for a DualSense misnamed");
   pad_connection_ps5_third_party.deinit(dev);

   /* pads that pass for a DualShock 4: nothing sent, and no motion,
    * unless one says it has them; a Razer's rumble even so; never
    * anything to a Victrix */
   memset(p, 0, sizeof(p));
   p[0] = 0x01; p[5] = 8;
   count = sent_count;
   dev = pad_connection_ps4_third_party.init(&dummy_connection, 0, &stub_hid);
   deliver(&pad_connection_ps4_third_party, dev, p, 64);
   CHECK(sent_count == count
         && !pad_connection_ps4_third_party.set_sensor(dev,
               RETRO_SENSOR_GYROSCOPE_ENABLE),
         "sdl pads: output or motion for a DualShock 4 pad that did not answer");
   pad_connection_ps4_third_party.deinit(dev);
   dev = pad_connection_ps4_razer.init(&dummy_connection, 0, &stub_hid);
   deliver(&pad_connection_ps4_razer, dev, p, 64);
   CHECK(sent_count == count + 1 && sent[0] == 0x05 && sent[1] == 0x07,
         "sdl pads: a Razer's rumble and light bar not sent together");
   pad_connection_ps4_razer.deinit(dev);
   stub_hid.get_report = stub_ds4_caps;
   dev = pad_connection_ps4_no_output.init(&dummy_connection, 0, &stub_hid);
   deliver(&pad_connection_ps4_no_output, dev, p, 64);
   CHECK(sent_count == count + 1, "sdl pads: output sent to a Victrix");
   pad_connection_ps4_no_output.deinit(dev);
   dev = pad_connection_ps4_third_party.init(&dummy_connection, 0, &stub_hid);
   stub_hid.get_report = NULL;
   CHECK(pad_connection_ps4_third_party.set_sensor(dev,
            RETRO_SENSOR_GYROSCOPE_ENABLE),
         "sdl pads: a DualShock 4 pad's motion not taken");
   put_le16(p + 13, 320);   /* 10 degrees a second at its scale */
   deliver(&pad_connection_ps4_third_party, dev, p, 64);
   CHECK(sent_count == count + 2 && sent[1] == 0x07,
         "sdl pads: output not sent to a DualShock 4 pad that takes it");
   v = 0;
   CHECK(pad_connection_ps4_third_party.sensor(dev, RETRO_SENSOR_GYROSCOPE_X, &v)
         && fabsf(v - 10.0f * 3.14159265f / 180.0f) < 1e-4f,
         "sdl pads: a DualShock 4 pad's gyroscope not at its own scale (%f)", v);
   pad_connection_ps4_third_party.set_rumble(dev, RETRO_RUMBLE_STRONG, 0xffff);
   deliver(&pad_connection_ps4_third_party, dev, p, 64);
   CHECK(sent_count == count + 3 && sent[1] == 0x07 && sent[5] == 0xff
         && (sent[6] | sent[7] | sent[8]),
         "sdl pads: a DualShock 4 pad's rumble sent without its light bar");
   pad_connection_ps4_third_party.deinit(dev);

   /* GameCube adapters in their PC mode: the older firmware's port
    * first, the first port with a pad read, its C stick the other way;
    * the newer's rumble, kept up */
   dev = pad_connection_gamecube_pc.init(&dummy_connection, 0, &stub_hid);
   memset(p, 0, sizeof(p));
   p[0] = 2;
   p[1] = 0x02 | 0x80;    /* A, Z */
   p[2] = 0x02;           /* Start */
   p[3] = 216; p[4] = 128; p[5] = 39; p[6] = 39;
   p[7] = 40; p[8] = 216;
   count = sent_count;
   deliver(&pad_connection_gamecube_pc, dev, p, 10);
   p[0] = 3; p[1] = 0x04; /* B on the next port */
   deliver(&pad_connection_gamecube_pc, dev, p, 10);
   CHECK(pad_connection_gamecube_pc.button(dev, RETRO_DEVICE_ID_JOYPAD_A)
         && pad_connection_gamecube_pc.button(dev, RETRO_DEVICE_ID_JOYPAD_SELECT)
         && pad_connection_gamecube_pc.button(dev, RETRO_DEVICE_ID_JOYPAD_START)
         && !pad_connection_gamecube_pc.button(dev, RETRO_DEVICE_ID_JOYPAD_B),
         "sdl pads: the GameCube adapter's first port not the one read");
   CHECK(pad_connection_gamecube_pc.get_axis(dev, 0) == 32767
         && abs(pad_connection_gamecube_pc.get_axis(dev, 1)) <= 1
         && pad_connection_gamecube_pc.get_axis(dev, 2) == 32767
         && pad_connection_gamecube_pc.get_axis(dev, 3) == -32767
         && pad_connection_gamecube_pc.get_axis(dev, 4) == 0
         && pad_connection_gamecube_pc.get_axis(dev, 5) == 32767,
         "sdl pads: the older GameCube adapter's sticks misread (%d %d %d %d %d %d)",
         pad_connection_gamecube_pc.get_axis(dev, 0),
         pad_connection_gamecube_pc.get_axis(dev, 1),
         pad_connection_gamecube_pc.get_axis(dev, 2),
         pad_connection_gamecube_pc.get_axis(dev, 3),
         pad_connection_gamecube_pc.get_axis(dev, 4),
         pad_connection_gamecube_pc.get_axis(dev, 5));
   pad_connection_gamecube_pc.set_rumble(dev, RETRO_RUMBLE_STRONG, 0xffff);
   deliver(&pad_connection_gamecube_pc, dev, p, 10);
   CHECK(sent_count == count, "sdl pads: the older GameCube adapter rumbled");
   pad_connection_gamecube_pc.deinit(dev);
   dev = pad_connection_gamecube_pc.init(&dummy_connection, 0, &stub_hid);
   memmove(p, p + 1, 9);
   deliver(&pad_connection_gamecube_pc, dev, p, 9);
   CHECK(pad_connection_gamecube_pc.get_axis(dev, 2) == -32768
         && pad_connection_gamecube_pc.get_axis(dev, 3) == 32767,
         "sdl pads: the newer GameCube adapter's C stick misread");
   pad_connection_gamecube_pc.set_rumble(dev, RETRO_RUMBLE_WEAK, 0x100);
   deliver(&pad_connection_gamecube_pc, dev, p, 9);
   deliver(&pad_connection_gamecube_pc, dev, p, 9);
   CHECK(sent_count == count + 1 && sent_len == 3 && sent[0] == 0
         && sent[1] == 0xff && sent[2] == 0xff,
         "sdl pads: the newer GameCube adapter not rumbled once (%u)",
         sent_count - count);
   pad_connection_gamecube_pc.set_rumble(dev, RETRO_RUMBLE_WEAK, 0);
   deliver(&pad_connection_gamecube_pc, dev, p, 9);
   deliver(&pad_connection_gamecube_pc, dev, p, 9);
   CHECK(sent_count == count + 2 && sent[1] == 0 && sent[2] == 0,
         "sdl pads: the newer GameCube adapter's rumble not stopped once");
   pad_connection_gamecube_pc.deinit(dev);

   /* the Switch Online N64 pad: at rest with no amplitude */
   sent_count = 0;
   stub_hid.is_bluetooth = stub_is_bluetooth;
   dev = pad_connection_switch_pro_n64.init(&dummy_connection, 0, &stub_hid);
   stub_hid.is_bluetooth = NULL;
   CHECK(sent_count == 1 && sent[0] == 0x01 && sent[2] == 0x00 && sent[3] == 0x00
         && sent[4] == 0x01 && sent[5] == 0x40,
         "sdl pads: the N64 pad's rest is not 00 00 01 40");
   pad_connection_switch_pro_n64.deinit(dev);

   printf("[%s] sdl pads: 8BitDo Pro 2's paddles and motion, the Ultimates' rumble, third-party DualSense and DualShock 4 pads by what they say, GameCube adapters in PC mode, the N64 pad's quiet rest\n",
         failures == had ? "pass" : "fail");
}

/* ---- DualShock 4 and DualSense touchpads ---------------------- */

/* a finger's four bytes as the pads send them */
static void touch_put(uint8_t *t, bool down, unsigned x, unsigned y)
{
   t[0] = (uint8_t)(down ? 0x05 : 0x85);
   t[1] = (uint8_t)x;
   t[2] = (uint8_t)(((x >> 8) & 0x0f) | ((y & 0x0f) << 4));
   t[3] = (uint8_t)(y >> 4);
}

/* where a place on the pad is, as the pointer device has it */
static int16_t touch_want(unsigned v, unsigned reach)
{
   if (v >= reach)
      v = reach - 1;
   return (int16_t)((long)v * 0xfffe / (long)(reach - 1) - 0x7fff);
}

static bool touch_wrong(const input_pad_view_t *v, unsigned finger,
      unsigned x, unsigned y, unsigned h)
{
   return (int16_t)(v->touch[finger] & 0xffff) != touch_want(x, 1920)
       || (int16_t)(v->touch[finger] >> 16)    != touch_want(y, h);
}

/* @shape: 0 the DualShock 4 over USB, 1 woken over Bluetooth (0x11);
 * 2 the DualSense over USB, 3 over Bluetooth (0x31) */
static unsigned touch_reports(const pad_connection_interface_t *ifc,
      void *dev, int shape)
{
   uint8_t p[80];
   const input_pad_view_t *v = ifc->view(dev);
   unsigned i, bad = 0;
   unsigned h = shape < 2 ? 920 : 1070;
   for (i = 0; i < 4000; i++)
   {
      unsigned x[2], y[2], j, len, at, press_at;
      bool d[2], press;
      uint8_t want;
      for (j = 0; j < 2; j++)
      {
         x[j] = (unsigned)rand() % 2048;
         y[j] = (unsigned)rand() % 1280;
         d[j] = (rand() & 1) != 0;
      }
      press = (rand() & 1) != 0;
      for (j = 0; j < sizeof(p); j++)
         p[j] = (uint8_t)rand();
      switch (shape)
      {
         case 0:  p[0] = 0x01; at = 1 + 34; press_at = 1 + 6; len = 64; break;
         case 1:  p[0] = 0x11; at = 3 + 34; press_at = 3 + 6; len = 78; break;
         case 2:  p[0] = 0x01; at = 33;     press_at = 10;    len = 64; break;
         default: p[0] = 0x31; at = 34;     press_at = 11;    len = 78; break;
      }
      p[press_at] = (uint8_t)((p[press_at] & ~0x02) | (press ? 0x02 : 0));
      touch_put(p + at,     d[0], x[0], y[0]);
      touch_put(p + at + 4, d[1], x[1], y[1]);
      deliver(ifc, dev, p, (uint16_t)len);
      want = (uint8_t)((d[0] ? 1 : 0) | (d[1] ? 2 : 0)
            | (press ? INPUT_PAD_TOUCH_PRESS : 0));
      if (     v->touch_down != want
            || (d[0] && touch_wrong(v, 0, x[0], y[0], h))
            || (d[1] && touch_wrong(v, 1, x[1], y[1], h)))
         bad++;
   }
   return bad;
}

static void test_sony_touch(void)
{
   uint8_t p[80];
   unsigned had = failures, bad;
   int dummy_connection;
   void *dev;

   srand(7);
   dev = pad_connection_ps4.init(&dummy_connection, 0, &stub_hid);
   CHECK(pad_connection_ps4.view(dev)->touchpad, "touch: a DualShock 4 has no touchpad");
   bad  = touch_reports(&pad_connection_ps4, dev, 0);
   bad += touch_reports(&pad_connection_ps4, dev, 1);
   CHECK(!bad, "touch: %u DualShock 4 reports read wrong", bad);
   /* Bluetooth's short report has no fingers */
   memset(p, 0, sizeof(p));
   p[0] = 0x01; p[5] = 8;
   deliver(&pad_connection_ps4, dev, p, 10);
   CHECK(!pad_connection_ps4.view(dev)->touch_down,
         "touch: a DualShock 4's short report kept the fingers");
   pad_connection_ps4.deinit(dev);

   dev = pad_connection_ps5.init(&dummy_connection, 0, &stub_hid);
   CHECK(pad_connection_ps5.view(dev)->touchpad, "touch: a DualSense has no touchpad");
   bad  = touch_reports(&pad_connection_ps5, dev, 2);
   bad += touch_reports(&pad_connection_ps5, dev, 3);
   CHECK(!bad, "touch: %u DualSense reports read wrong", bad);
   memset(p, 0, sizeof(p));
   p[0] = 0x01; p[5] = 8;
   deliver(&pad_connection_ps5, dev, p, 10);
   CHECK(!pad_connection_ps5.view(dev)->touch_down,
         "touch: a DualSense's short report kept the fingers");
   pad_connection_ps5.deinit(dev);

   /* the others' are not known to be laid out the same */
   dev = pad_connection_ps4_third_party.init(&dummy_connection, 0, &stub_hid);
   CHECK(!pad_connection_ps4_third_party.view(dev)->touchpad,
         "touch: another make's DualShock 4 was taken to have a touchpad");
   pad_connection_ps4_third_party.deinit(dev);
   dev = pad_connection_ps5_third_party.init(&dummy_connection, 0, &stub_hid);
   CHECK(!pad_connection_ps5_third_party.view(dev)->touchpad,
         "touch: another make's DualSense was taken to have a touchpad");
   pad_connection_ps5_third_party.deinit(dev);

   printf("[%s] touch: both fingers and the press on the DualShock 4 and DualSense,"
         " over USB and Bluetooth, on 16000 reports; none in a short report or on another make's\n",
         failures == had ? "pass" : "fail");
}

/* ---- the original Xbox pad (XID): new, held to its report ---- */

static void test_xid(void)
{
   /* the report's pressure bytes 4 to 11 and byte 2's bits, from bit 0,
    * as the RetroPad's buttons */
   static const unsigned pressed[8] = {
      RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_R, RETRO_DEVICE_ID_JOYPAD_L,
      RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_ID_JOYPAD_R2 };
   static const unsigned bits[8] = {
      RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT,
      RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_SELECT,
      RETRO_DEVICE_ID_JOYPAD_L3, RETRO_DEVICE_ID_JOYPAD_R3 };
   const pad_connection_interface_t *ifc = &pad_connection_xid;
   uint8_t p[32];
   unsigned n, i, bad = 0, had = failures, count;
   int dummy_handle;
   void *dev = ifc->init(&dummy_handle, 0, &stub_hid);

   CHECK(dev != NULL, "xid: no device");
   if (!dev)
      return;
   srand(5);
   for (n = 0; n < 20000; n++)
   {
      uint32_t want = 0;
      input_bits_t got;
      for (i = 0; i < sizeof(p); i++)
         p[i] = (uint8_t)rand();
      p[0] = 0x00;
      p[1] = 20;
      /* the edges: a sticks' extremes, a pressure at the threshold */
      if (n < 16)
      {
         for (i = 12; i < 20; i += 2)
         {
            p[i]     = (n & 1) ? 0x00 : 0xff;
            p[i + 1] = (n & 1) ? 0x80 : 0x7f;
         }
         for (i = 4; i < 12; i++)
            p[i] = (uint8_t)(0x54 + (n & 3));
      }
      deliver(ifc, dev, p, 20);
      for (i = 0; i < 8; i++)
      {
         if (p[2] & (1u << i))
            want |= 1u << bits[i];
         if (p[4 + i] > 0x55)
            want |= 1u << pressed[i];
         /* a pressure 0..255 reaches 0..32767, monotonic */
         if (ifc->get_axis(dev, 4 + i) != (int16_t)((p[4 + i] * 32767 + 127) / 255)
               && abs(ifc->get_axis(dev, 4 + i) - (p[4 + i] * 32767) / 255) > 128)
            bad++;
      }
      ifc->get_buttons(dev, &got);
      for (i = 0; i < 32; i++)
         if (   !!BIT256_GET(got, i) != !!(want & (1u << i))
             || !!ifc->button(dev, (uint16_t)i) != !!(want & (1u << i)))
            bad++;
      /* X right, Y down, as the frontend's axes; the full reach, no
       * overflow at -32768 */
      for (i = 0; i < 4; i++)
      {
         int32_t raw = (int16_t)(uint16_t)(p[12 + i * 2] | (p[13 + i * 2] << 8));
         int32_t exp = (i & 1) ? -1 - raw : raw;
         if (ifc->get_axis(dev, i) != exp)
            bad++;
      }
   }
   CHECK(!bad, "xid: %u reads disagree with the report", bad);
   /* full pressure reads 32767, none 0 */
   memset(p, 0, sizeof(p));
   p[1] = 20;
   p[4] = 0xff;
   deliver(ifc, dev, p, 20);
   CHECK(ifc->get_axis(dev, 4) == 32767 && ifc->get_axis(dev, 5) == 0,
         "xid: pressure does not reach 0 to 32767");
   /* a short report, or another report, is not taken */
   {
      int16_t ax = ifc->get_axis(dev, 4);
      memset(p, 0, sizeof(p));
      p[1] = 20;
      deliver(ifc, dev, p, 19);
      p[1] = 26;   /* a Steel Battalion's */
      deliver(ifc, dev, p, 26);
      p[0] = 0x01; p[1] = 20;
      deliver(ifc, dev, p, 20);
      CHECK(ifc->get_axis(dev, 4) == ax, "xid: a report not the pad's was taken");
   }
   /* rumble: output report 0, the left motor at byte 3, the right at 5;
    * sent from the packet handler, once a change */
   memset(p, 0, sizeof(p));
   p[1] = 20;
   deliver(ifc, dev, p, 20);
   count = sent_count;
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0xffff);
   ifc->set_rumble(dev, RETRO_RUMBLE_WEAK, 0x4000);
   CHECK(sent_count == count, "xid: rumble sent outside the packet handler");
   deliver(ifc, dev, p, 20);
   CHECK(sent_count == count + 1 && sent_len == 6 && sent[0] == 0 && sent[1] == 6
         && sent[2] == 0 && sent[3] == 0xff && sent[4] == 0 && sent[5] == 0x40,
         "xid: rumble report not 00 06 00 ff 00 40");
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0xffff);
   deliver(ifc, dev, p, 20);
   CHECK(sent_count == count + 1, "xid: rumble sent again unchanged");
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0);
   ifc->set_rumble(dev, RETRO_RUMBLE_WEAK, 0);
   deliver(ifc, dev, p, 20);
   CHECK(sent_count == count + 2 && !sent[3] && !sent[5], "xid: rumble not stopped");
   ifc->deinit(dev);

   /* found by its interface's class, whoever made it; not the DVD
    * remote's receiver or Steel Battalion's controller */
   CHECK(winusb_is_xid("USB\\Class_58&SubClass_42&Prot_00\0USB\\Class_58&SubClass_42\0USB\\Class_58\0", 0x045e, 0x0202),
         "xid: Microsoft's pad not found by its class");
   CHECK(winusb_is_xid("USB\\CLASS_58&SUBCLASS_42\0", 0x0738, 0x4522),
         "xid: a third-party pad not found by its class");
   CHECK(!winusb_is_xid("USB\\Class_58&SubClass_42&Prot_00\0", 0x045e, 0x0284),
         "xid: the DVD remote's receiver taken for a pad");
   CHECK(!winusb_is_xid("USB\\Class_58&SubClass_42&Prot_00\0", 0x0a7b, 0xd000),
         "xid: Steel Battalion's controller taken for a pad");
   CHECK(!winusb_is_xid("USB\\Class_FF&SubClass_5D&Prot_01\0USB\\Class_FF\0", 0x045e, 0x028e),
         "xid: an Xbox 360 pad taken for an original Xbox pad");
   CHECK(!winusb_is_xid("\0", 0x045e, 0x0202), "xid: a device with no ids taken");

   printf("[%s] xid: buttons, pressures and sticks from the report on 20000 reports; other reports left; rumble from the packet handler, once a change; found by class, not the remote or Steel Battalion\n",
         failures == had ? "pass" : "fail");
}

int main(void)
{
   stub_hid.send_control = stub_send_control;
   stub_hid.set_report   = stub_set_report;
   test_ps4();
   test_wiiugca();
   test_xid();
   test_adapter(&pad_connection_ps2adapter, "ps2adapter", false);
   test_adapter(&pad_connection_psxadapter, "psxadapter", true);
   test_usbpad(&pad_connection_snesusb, "snesusb", ref_snesusb_map, 0, 5);
   test_usbpad(&pad_connection_nesusb, "nesusb", ref_nesusb_map, 3, 6);
   test_pad(&pad_connection_dragonrise, "dragonrise", 7, 16, 7,
         ref_dragonrise_take, ref_dragonrise_axis);
   test_pad(&pad_connection_ps4_hori_mini, "ps4_hori_mini", 8, 32, 8,
         ref_hori_take, ref_hori_axis);
   test_pad(&pad_connection_kade, "kade", 3, 16, 10,
         ref_kade_take, ref_no_axis);
   test_retrode();
   test_ps5();
   test_sony_touch();
   test_switch_pro();
   test_switch2_pro();
   test_ds3();
   test_wiiupro();
   test_8bitdo();
   test_motion();
   test_sdl_pads();
   CHECK(view_reads && !view_bad,
         "views: %u of %u reports' views disagree with the getters",
         view_bad, view_reads);
   printf("[%s] views: each kept as a view says what its getters say, after %u reports\n",
         view_bad || !view_reads ? "fail" : "pass", view_reads);
   puts(failures ? "FAIL connect_parity_test" : "PASS connect_parity_test");
   return failures ? 1 : 0;
}
