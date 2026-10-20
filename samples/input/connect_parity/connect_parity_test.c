/* The pad handlers in input/connect decode a report where it lies
 * and keep only what they answer from, where they used to copy the
 * report and decode it at every read. This holds each converted
 * handler to the copying one: a reference built from the old code - the
 * report copied into the old struct and read through it - and the real
 * handler are given the same reports, and every button, the whole set
 * and every axis must agree. Random reports, and the edges (every hat
 * value, every bit alone, a report too short to be one). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "input/connect/joypad_connection.h"
#include "input/include/gamepad.h"

static unsigned failures;
#define CHECK(c, ...) do { if (!(c)) { printf("[FAIL] "); printf(__VA_ARGS__); \
   printf("\n"); failures++; } } while (0)

static void stub_send_control(void *handle, uint8_t *s, size_t len)
{ (void)handle; (void)s; (void)len; }

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

int main(void)
{
   stub_hid.send_control = stub_send_control;
   test_ps4();
   test_wiiugca();
   puts(failures ? "FAIL connect_parity_test" : "PASS connect_parity_test");
   return failures ? 1 : 0;
}
