/* The pad handlers in input/connect decode a report where it lies
 * and keep only what they answer from, where they used to copy the
 * report and decode it at every read. This holds each converted
 * handler to the copying one: a reference built from the old code - the
 * report copied into the old struct and read through it - and the real
 * handler are given the same reports, and every button, the whole set
 * and every axis must agree. Random reports, and the edges (every hat
 * value, every bit alone, a report too short to be one). The DualSense
 * handler is new, so it is held to the reports' layout instead. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "input/connect/joypad_connection.h"
#include "input/include/gamepad.h"
#include "input/common/sony_pad_output.h"

static unsigned failures;
#define CHECK(c, ...) do { if (!(c)) { printf("[FAIL] "); printf(__VA_ARGS__); \
   printf("\n"); failures++; } } while (0)

static uint8_t  sent[128];
static size_t   sent_len;
static unsigned sent_count;

static void stub_send_control(void *handle, uint8_t *s, size_t len)
{
   (void)handle;
   sent_len = len < sizeof(sent) ? len : sizeof(sent);
   memcpy(sent, s, sent_len);
   sent_count++;
}

static hid_driver_t stub_hid;

/* ---- PS4: the old handler's report and reads ------------------ */

struct ref_ps4
{
   uint8_t hatvalue[4];
   /* the old struct ps4buttons, little-end first */
   unsigned dpad : 4, square : 1, cross : 1, circle : 1, triangle : 1;
   unsigned l1 : 1, r1 : 1, l2 : 1, r2 : 1, share : 1, options : 1, l3 : 1, r3 : 1;
   unsigned ps : 1, touchpad : 1, reportcounter : 6;
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
      if (i < 16)
         p[5] = (uint8_t)i;
      else if (i < 40)
      {
         p[5] = 8;
         p[6] = p[7] = 0;
         p[5 + (i - 16) / 8] |= (uint8_t)(1u << ((i - 16) % 8));
      }
      ifc->packet_handler(dev, p, 10);
      ref_ps4_take(&ref, p);
      bad += ps4_mismatches(dev, &ref);
   }
   CHECK(!bad, "ps4: %u reads disagree with the copying handler", bad);
   /* a report too short to be one leaves the pad as it was */
   memset(p, 0xff, sizeof(p));
   ifc->packet_handler(dev, p, 9);
   CHECK(!ps4_mismatches(dev, &ref), "ps4: a short report was taken");
   ifc->deinit(dev);
   printf("[%s] ps4: every button, the set and the axes as the copying handler, on 20040 reports\n",
         failures == had ? "pass" : "fail");
}

/* ---- Wii U GameCube adapter: the old handler's report and reads -- */

#define REF_GCA_LEN 37

struct ref_gca_pad
{
   uint32_t buttons;
   int16_t analog[3][2];
   uint8_t connected;
};

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
      pad->connected = copy[i];
      memcpy(data, &copy[i], 9);
      keys        = data[1] | (data[2] << 8);
      pad->buttons = 0;
      for (k = 0; k < 12; k++)
         pad->buttons |= (keys & (1 << k)) ? (1 << map[k]) : 0;
      for (a = 0; a < 4; a++)
      {
         unsigned axis = (a % 2) ? 0 : 1;
         int16_t v     = data[3 + a];
         v = axis ? ((v - 128) * 256) : ((v - 128) * -256);
         pad->analog[a / 2][axis] = v;
      }
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
      ifc->packet_handler(dev, p, REF_GCA_LEN);
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
         for (k = 0; k < 8; k++)
         {
            axis_data ad;
            gamepad_read_axis_data(axes[k], &ad);
            if (ifc->get_axis(pads[i], axes[k])
                  != gamepad_get_axis_value_raw(ref[i].analog, &ad, false))
               bad++;
         }
      }
   }
   CHECK(!bad, "wiiugca: %u reads disagree with the copying handler", bad);
   /* a short report is not taken */
   {
      int16_t ax = pads[0] ? ifc->get_axis(pads[0], AXIS_POS(0)) : 0;
      memset(p, 0x14, sizeof(p));
      ifc->packet_handler(dev, p, REF_GCA_LEN - 1);
      CHECK(!pads[0] || ifc->get_axis(pads[0], AXIS_POS(0)) == ax,
            "wiiugca: a short report was taken");
   }
   for (i = 0; i < 4; i++)
      if (pads[i])
         ifc->pad_deinit(pads[i]);
   ifc->deinit(dev);
   printf("[%s] wiiugca: four ports' buttons, sets, axes and state as the copying handler, on 20000 reports\n",
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
      ifc->packet_handler(dev, p, (uint16_t)size);
      if (psx)
         ref_psx_take(&ref, p, size);
      else
         ref_ps2_take(&ref, p, size);
      bad += adapter_mismatches(ifc, dev, &ref, psx);
   }
   CHECK(!bad, "%s: %u reads disagree with the copying handler", name, bad);
   /* a report too short to be one leaves the pad as it was */
   memset(p, 0x01, sizeof(p));
   ifc->packet_handler(dev, p, (uint16_t)(min - 1));
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
      ifc->packet_handler(dev, p, (uint16_t)size);
      ref_usbpad_take(&ref, map, p, size);
      bad += usbpad_mismatches(ifc, dev, &ref, at);
   }
   CHECK(!bad, "%s: %u reads disagree with the copying handler", name, bad);
   memset(p, 0xff, sizeof(p));
   ifc->packet_handler(dev, p, 6);
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
      ifc->packet_handler(dev, p, (uint16_t)size);
      take(&ref, p, size);
      bad += pad_mismatches(ifc, dev, &ref, set_bits, axis);
   }
   CHECK(!bad, "%s: %u reads disagree with the copying handler", name, bad);
   memset(p, 0x7F, sizeof(p));
   ifc->packet_handler(dev, p, (uint16_t)(min - 1));
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
      ifc->packet_handler(dev, p, (uint16_t)size);
      if (p[0] >= 1 && p[0] <= 4)
         ref_retrode_take(&ref[p[0] - 1], p, size);
      for (i = 0; i < 4; i++)
         if (pads[i])
            bad += pad_mismatches(ifc, pads[i], &ref[i], 16, ref_retrode_axis);
   }
   CHECK(!bad, "retrode: %u reads disagree with the copying handler", bad);
   memset(p, 0xff, sizeof(p));
   p[0] = 1;
   ifc->packet_handler(dev, p, 3);
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

static bool stub_is_bluetooth(void *handle) { (void)handle; return true; }

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
   st->b[2] &= 0x07;
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
      ifc->packet_handler(dev, p, (uint16_t)len);
      bad += ps5_mismatches(ifc, dev, &st);
   }
   CHECK(!bad, "ps5: %u reads over Bluetooth disagree with the report", bad);
   CHECK(sent_count == 0, "ps5: output went out behind a Bluetooth stack's header");

   /* reports that are not the pad's state, or too short to be */
   memset(p, 0xff, sizeof(p));
   p[0] = 0x01;
   ifc->packet_handler(dev, p, 9);
   p[0] = 0x31;
   ifc->packet_handler(dev, p, 11);
   p[0] = 0x05;
   ifc->packet_handler(dev, p, 64);
   CHECK(!ps5_mismatches(ifc, dev, &st), "ps5: a short or foreign report was taken");
   ifc->deinit(dev);

   /* Bluetooth through the system (hidraw, Windows): the lights, then
    * rumble, as the shared builder makes them, with their checksum */
   sent_count = 0;
   dev = ifc->init(&dummy_connection, 1, &stub_hid);
   ps5_random_state(&st);
   ifc->packet_handler(dev, p, (uint16_t)ps5_report(p, &st, 1));
   CHECK(sent_count == 1 && ps5_sent_is(true, false, 0, 0, 2),
         "ps5: the first Bluetooth report did not light player 2");
   ifc->set_rumble(dev, RETRO_RUMBLE_STRONG, 0x8000);
   ifc->packet_handler(dev, p, (uint16_t)ps5_report(p, &st, 3));
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
   ifc->packet_handler(dev, p, 78);
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
      ifc->packet_handler(dev, p, (uint16_t)ps5_report(p, &st, 0));
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
   ifc->packet_handler(dev, p, (uint16_t)ps5_report(p, &st, 0));
   CHECK(sent_count == 2 && ps5_sent_is(false, false, 0x80, 0xff, 0),
         "ps5: the rumble report is not the motors asked for");
   ifc->packet_handler(dev, p, (uint16_t)ps5_report(p, &st, 0));
   CHECK(sent_count == 2, "ps5: rumble was sent again unchanged");
   ifc->deinit(dev);

   /* the newer motor control: an Edge always, a DualSense from
    * firmware 2.21 */
   sent_count = 0;
   dev = pad_connection_ps5_edge.init(&dummy_connection, 0, &stub_hid);
   ifc->packet_handler(dev, p, (uint16_t)ps5_report(p, &st, 0));
   CHECK(ps5_sent_is(false, true, 0, 0, 1), "ps5: an Edge's motors are not the newer way");
   CHECK(!strcmp(ifc->get_name(dev), "DualSense Edge Wireless Controller"),
         "ps5: an Edge is not named one");
   ifc->deinit(dev);
   stub_hid.get_report = stub_get_report;
   feature_firmware    = 0x0221;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   ifc->packet_handler(dev, p, (uint16_t)ps5_report(p, &st, 0));
   CHECK(ps5_sent_is(false, true, 0, 0, 1), "ps5: firmware 2.21's motors are not the newer way");
   ifc->deinit(dev);
   feature_firmware    = 0x0215;
   dev = ifc->init(&dummy_connection, 0, &stub_hid);
   ifc->packet_handler(dev, p, (uint16_t)ps5_report(p, &st, 0));
   CHECK(ps5_sent_is(false, false, 0, 0, 1), "ps5: older firmware's motors are the newer way");
   ifc->deinit(dev);
   stub_hid.get_report = NULL;
   feature_firmware    = 0;

   printf("[%s] ps5: USB and both Bluetooth shapes read as laid out, on 40000 reports;"
         " lights once and rumble at the poll as the shared builder makes them,"
         " the Edge and firmware 2.21 the newer way, nothing behind a stack's header\n",
         failures == had ? "pass" : "fail");
}

int main(void)
{
   stub_hid.send_control = stub_send_control;
   test_ps4();
   test_wiiugca();
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
   puts(failures ? "FAIL connect_parity_test" : "PASS connect_parity_test");
   return failures ? 1 : 0;
}
