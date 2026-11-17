/* The Wii U joypad driver, held to what the snapshot bridge assumes of
 * a driver it serves (see bridge_view.h). Built on the host against the
 * tree's own Wii U headers (os/wiiu/include): the dispatcher in
 * wiiu_joypad.c and the three drivers it hands a slot to - the GamePad's
 * (wpad), the Wiimotes' (kpad) and USB HID's (hidpad) - with their pad
 * state filled by hand as their polls fill it. The HID driver behind
 * hidpad is one of this test's, answering as a pad connection does.
 * The GamePad and the Wiimotes lend their views; HID's pads are not. */
#include "../../../input/drivers_joypad/wiiu_joypad.c"
#include "../../../input/drivers_joypad/wiiu/wpad_driver.c"
#include "../../../input/drivers_joypad/wiiu/kpad_driver.c"
#include "../../../input/drivers_joypad/wiiu/hidpad_driver.c"

#include "bridge_view.h"

/* The slots: the GamePad, a Wiimote, a HID pad, and one with nothing */
#define SLOT_WPAD 0
#define SLOT_KPAD 1
#define SLOT_HID  2
#define SLOT_NONE 3

/* What the HID pad holds */
static uint64_t hid_buttons;
static int16_t  hid_axes[4];

static void *fake_hid_init(void) { return (void *)-1; }
static bool fake_hid_query(void *data, unsigned slot)
{ (void)data; return slot < MAX_USERS; }
static void fake_hid_free(const void *data) { (void)data; }
static void fake_hid_poll(void *data) { (void)data; }
static const char *fake_hid_name(void *data, unsigned slot)
{ (void)data; (void)slot; return "hid"; }
static bool fake_hid_rumble(void *data, unsigned slot,
      enum retro_rumble_effect effect, uint16_t strength)
{ (void)data; (void)slot; (void)effect; (void)strength; return false; }

/* As wiiu_hid's do, through a pad connection's button(), get_buttons()
 * and get_axis() */
static int16_t fake_hid_button(void *data, unsigned slot, uint16_t joykey)
{
   (void)data;
   if (slot != SLOT_HID || joykey >= 64)
      return 0;
   return (int16_t)((hid_buttons >> joykey) & 1);
}

static void fake_hid_get_buttons(void *data, unsigned slot,
      input_bits_t *state)
{
   (void)data;
   if (slot != SLOT_HID)
      return;
   BITS_COPY64_PTR(state, hid_buttons);
}

static int16_t fake_hid_axis(void *data, unsigned slot, uint32_t joyaxis)
{
   (void)data;
   if (slot != SLOT_HID)
      return 0;
   if (AXIS_NEG_GET(joyaxis) < 4)
      return (hid_axes[AXIS_NEG_GET(joyaxis)] < 0)
         ? hid_axes[AXIS_NEG_GET(joyaxis)] : 0;
   if (AXIS_POS_GET(joyaxis) < 4)
      return (hid_axes[AXIS_POS_GET(joyaxis)] > 0)
         ? hid_axes[AXIS_POS_GET(joyaxis)] : 0;
   return 0;
}

/* Filled in main(), its other members left NULL */
hid_driver_t wiiu_hid;

/* The frontend's and the system's, as the drivers' init, poll and
 * rumble reach for them; never run */
void *hid_driver_get_data(void) { return (void *)-1; }
const hid_driver_t *input_hid_init_first(void) { return NULL; }
void input_pad_connect(unsigned port, input_device_driver_t *driver)
{ (void)port; (void)driver; }
void input_driver_platform_request(enum input_platform_request req)
{ (void)req; }
bool video_driver_get_viewport_info(struct video_viewport *viewport)
{ (void)viewport; return false; }
int pad_connection_find_vacant_pad(joypad_connection_t *joyconn)
{ (void)joyconn; return -1; }
void pad_connection_release_slot(joypad_connection_t *joyconn)
{ (void)joyconn; }
void pad_connection_mark_end(joypad_connection_t *joyconn) { (void)joyconn; }
int32_t VPADRead(uint32_t chan, VPADStatus *buffers, uint32_t count,
      VPADReadError *error)
{ (void)chan; (void)buffers; (void)count; *error = VPAD_READ_INVALID_CONTROLLER; return 0; }
void VPADGetTPCalibratedPoint(uint32_t chan, VPADTouchData *calibratedData,
      VPADTouchData *uncalibratedData)
{ (void)chan; (void)calibratedData; (void)uncalibratedData; }
s32 KPADRead(s32 chan, void *data, u32 size)
{ (void)chan; (void)data; (void)size; return 0; }


static int16_t rand_axis(void)
{
   unsigned r = bv_rand() % 8;
   return (r == 0) ? 0
      : (r == 1) ? 0x7fff
      : (r == 2) ? -0x7fff
      : (r == 3) ? -0x8000
      : (int16_t)bv_rand();
}

static uint64_t rand_mask(void)
{
   uint64_t m = ((uint64_t)bv_rand() << 32) | bv_rand();
   /* now and then only the low half, as most pads hold */
   if (bv_rand() & 1)
      m &= 0xffffffffu;
   return m;
}

int main(void)
{
   unsigned n, i, bad = 0;

   wiiu_hid.init        = fake_hid_init;
   wiiu_hid.query_pad   = fake_hid_query;
   wiiu_hid.free        = fake_hid_free;
   wiiu_hid.button      = fake_hid_button;
   wiiu_hid.get_buttons = fake_hid_get_buttons;
   wiiu_hid.axis        = fake_hid_axis;
   wiiu_hid.poll        = fake_hid_poll;
   wiiu_hid.set_rumble  = fake_hid_rumble;
   wiiu_hid.name        = fake_hid_name;
   wiiu_hid.ident       = "wiiu";

   memset(&joypad_state, 0, sizeof(joypad_state));
   joypad_state.max_slot = MAX_USERS;
   for (i = 0; i < WIIU_GAMEPAD_CHANNELS; i++)
      joypad_state.wpad.channel_slot_map[i] = -1;
   for (i = 0; i < WIIU_WIIMOTE_CHANNELS; i++)
      joypad_state.kpad.channel_slot_map[i] = -1;

   joypad_state.wpad.ready                = true;
   joypad_state.wpad.channel_slot_map[0]  = SLOT_WPAD;
   joypad_state.pads[SLOT_WPAD].connected = true;
   joypad_state.pads[SLOT_WPAD].input_driver = &wpad_driver;

   joypad_state.kpad.ready                = true;
   joypad_state.kpad.channel_slot_map[2]  = SLOT_KPAD;
   joypad_state.pads[SLOT_KPAD].connected = true;
   joypad_state.pads[SLOT_KPAD].input_driver = &kpad_driver;

   joypad_state.hid.ready                 = true;
   joypad_state.pads[SLOT_HID].connected  = true;
   joypad_state.pads[SLOT_HID].input_driver = &hidpad_driver;

   ready = true;

   for (n = 0; n < 4000; n++)
   {
      unsigned pad = n % 4;
      switch (pad)
      {
         case SLOT_WPAD:
            /* the sticks and where the screen is touched */
            bv_fill_view(&joypad_state.wpad.pads[0].view, 64,
                  WIIU_AXIS_TOUCH_Y + 1);
            break;
         case SLOT_KPAD:
            bv_fill_view(&joypad_state.kpad.wiimotes[2].view, 64, 4);
            break;
         case SLOT_HID:
            hid_buttons = rand_mask();
            for (i = 0; i < 4; i++)
               hid_axes[i] = rand_axis();
            break;
         default:
            break;
      }
      bad += bv_compare(&wiiu_joypad, pad);
      /* the GamePad and the Wiimote lend theirs; HID's pads are copied */
      if (pad == SLOT_WPAD || pad == SLOT_KPAD)
         bad += bv_compare_view(&wiiu_joypad, pad);
      else if (wiiu_joypad.view(pad))
         bad++;
   }

   if (bad)
   {
      printf("FAIL wiiu_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS wiiu_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
