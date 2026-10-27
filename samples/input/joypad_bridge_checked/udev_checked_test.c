/* The udev joypad driver, held to what the snapshot bridge assumes of
 * a driver it serves (see bridge_view.h). The driver source is
 * included for its pad table, which is filled here by hand: no
 * device is opened. */
#include "../../../input/drivers_joypad/udev_joypad.c"

/* the frontend's; the user's list is the settings', here none */
bool input_device_ignored(uint16_t vid, uint16_t pid)
{ return input_ignore_devices_check(NULL, vid, pid); }

#include "bridge_view.h"

int main(void)
{
   unsigned n, i, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      struct udev_joypad *pad = &udev_pads[n % 3];

      pad->fd      = 1; /* as a connected pad has one */
      memset(&pad->view.buttons, 0, sizeof(pad->view.buttons));
      pad->view.buttons.data[0] = bv_rand();
      pad->view.buttons.data[1] = bv_rand();
      for (i = 0; i < NUM_HATS; i++)
      {
         pad->hats[i][0] = (int8_t)((int)(bv_rand() % 3) - 1);
         pad->hats[i][1] = (int8_t)((int)(bv_rand() % 3) - 1);
         /* as an event does */
         udev_joypad_view_hat(pad, i);
      }
      for (i = 0; i < NUM_AXES; i++)
      {
         unsigned r = bv_rand() % 8;
         pad->axes[i]        = (r == 0) ? 0
                             : (r == 1) ? 0x7fff
                             : (r == 2) ? -0x7fff
                             : (r == 3) ? (int16_t)-0x8000
                             : (int16_t)bv_rand();
         pad->neg_trigger[i] = (bv_rand() & 1) != 0;
         udev_joypad_view_axis(pad, i);
      }
      bad += bv_compare(&udev_joypad, n % 3);
      bad += bv_compare_view(&udev_joypad, n % 3);
   }

   /* A touchpad node's events, as a DualSense's sends them: 0..1919
    * across, 0..1079 down; two slots; the click a button */
   {
      struct udev_joypad *pad = &udev_pads[0];
      const input_pad_view_t *view;
      unsigned before = bad;
      pad->touch_min[0]  = 0;  pad->touch_span[0] = 1919;
      pad->touch_min[1]  = 0;  pad->touch_span[1] = 1079;
      pad->touch_slot    = 0;
      pad->view.touch_down = 0;
      pad->view.touchpad = true;
      udev_touch_event(pad, EV_ABS, ABS_MT_SLOT, 1);
      udev_touch_event(pad, EV_ABS, ABS_MT_TRACKING_ID, 7);
      udev_touch_event(pad, EV_ABS, ABS_MT_POSITION_X, 1919);
      udev_touch_event(pad, EV_ABS, ABS_MT_POSITION_Y, 0);
      udev_touch_event(pad, EV_SYN, SYN_REPORT, 0);
      view = udev_joypad.view(0);
      if (!view || !view->touchpad || view->touch_down != 2
            || (int16_t)(view->touch[1] & 0xffff) != 0x7fff
            || (int16_t)(view->touch[1] >> 16) != -0x7fff)
         bad++;
      udev_touch_event(pad, EV_ABS, ABS_MT_SLOT, 0);
      udev_touch_event(pad, EV_ABS, ABS_MT_TRACKING_ID, 8);
      udev_touch_event(pad, EV_ABS, ABS_MT_POSITION_X, 959);
      udev_touch_event(pad, EV_ABS, ABS_MT_POSITION_Y, 539);
      udev_touch_event(pad, EV_KEY, BTN_LEFT, 1);
      if (view->touch_down != (3 | INPUT_PAD_TOUCH_PRESS)
            || (int16_t)(view->touch[0] & 0xffff) < -40
            || (int16_t)(view->touch[0] & 0xffff) > 40
            || (int16_t)(view->touch[0] >> 16) < -40
            || (int16_t)(view->touch[0] >> 16) > 40)
         bad++;
      /* a slot past the second is not one of ours; lifting works */
      udev_touch_event(pad, EV_ABS, ABS_MT_SLOT, 5);
      udev_touch_event(pad, EV_ABS, ABS_MT_TRACKING_ID, 9);
      udev_touch_event(pad, EV_ABS, ABS_MT_SLOT, 1);
      udev_touch_event(pad, EV_ABS, ABS_MT_TRACKING_ID, -1);
      udev_touch_event(pad, EV_KEY, BTN_LEFT, 0);
      if (view->touch_down != 1)
         bad++;
      if (bad != before)
         printf("FAIL udev_checked_test: the touchpad's events did not reach the view as sent\n");
   }

   if (bad)
   {
      printf("FAIL udev_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS udev_checked_test: 4000 random pads; what the bridge would"
         " read, copied or lent, is what the driver says; a touchpad node's"
         " slots and click reach the pad's view\n");
   return 0;
}
