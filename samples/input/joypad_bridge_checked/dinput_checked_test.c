/* The DirectInput joypad driver, held to what the snapshot bridge
 * assumes of a driver it serves (see bridge_view.h). The driver source
 * is included for its pad table: each pad's DIJOYSTATE2 is filled here
 * by hand and made the pad's view as the poll makes it, and the view is
 * held to what DirectInput's state says - a button down, a hat's
 * eight ways, an axis's value. No device is opened, so it runs under
 * Wine with none. */
#include "../../../input/drivers_joypad/dinput_joypad.c"

/* the frontend's; the user's list is the settings', here none */
bool input_device_ignored(uint16_t vid, uint16_t pid)
{ return input_ignore_devices_check(NULL, vid, pid); }

#include "bridge_view.h"

/* What a hat at @pov holds of direction @dir, as DirectInput puts it:
 * the four ways and the four between them, nothing else */
static int pov_holds(DWORD pov, uint16_t dir)
{
   switch (dir)
   {
      case HAT_UP_MASK:    return pov == 0     || pov == 4500  || pov == 31500;
      case HAT_RIGHT_MASK: return pov == 9000  || pov == 4500  || pov == 13500;
      case HAT_DOWN_MASK:  return pov == 18000 || pov == 13500 || pov == 22500;
      case HAT_LEFT_MASK:  return pov == 27000 || pov == 22500 || pov == 31500;
   }
   return 0;
}

int main(void)
{
   static const DWORD povs[] = {
      0xFFFFFFFF, 0, 4500, 9000, 13500, 18000, 22500, 27000, 31500,
      100, 36000 };
   unsigned n, i, bad = 0;

   for (n = 0; n < 4000; n++)
   {
      struct dinput_joypad_data *pad = &g_pads[n % 3];
      LONG *axes[8];

      pad->joypad = (LPDIRECTINPUTDEVICE8)(uintptr_t)1; /* never called through */
      for (i = 0; i < sizeof(pad->joy_state.rgbButtons); i++)
         pad->joy_state.rgbButtons[i] = (bv_rand() & 1) ? 0x80 : 0;
      for (i = 0; i < 4; i++)
         pad->joy_state.rgdwPOV[i] = povs[bv_rand() % (sizeof(povs) / sizeof(povs[0]))];
      axes[0] = &pad->joy_state.lX;   axes[1] = &pad->joy_state.lY;
      axes[2] = &pad->joy_state.lZ;   axes[3] = &pad->joy_state.lRx;
      axes[4] = &pad->joy_state.lRy;  axes[5] = &pad->joy_state.lRz;
      axes[6] = &pad->joy_state.rglSlider[0];
      axes[7] = &pad->joy_state.rglSlider[1];
      for (i = 0; i < 8; i++)
      {
         unsigned r = bv_rand() % 8;
         *axes[i]   = (r == 0) ? 0
                    : (r == 1) ? 0x7fff
                    : (r == 2) ? -0x7fff
                    : (r == 3) ? -0x8000
                    : (LONG)(int16_t)bv_rand();
      }
      /* as the poll makes it */
      dinput_pad_make_view(pad);
      for (i = 0; i < ARRAY_SIZE_RGB_BUTTONS; i++)
         if (     (dinput_joypad.button(n % 3, (uint16_t)i) != 0)
               != (pad->joy_state.rgbButtons[i] != 0))
            bad++;
      for (i = 0; i < 4; i++)
      {
         static const uint16_t dirs[4] = {
            HAT_UP_MASK, HAT_DOWN_MASK, HAT_LEFT_MASK, HAT_RIGHT_MASK };
         unsigned d;
         for (d = 0; d < 4; d++)
            if (     (dinput_joypad.button(n % 3, (uint16_t)HAT_MAP(i, dirs[d])) != 0)
                  != pov_holds(pad->joy_state.rgdwPOV[i], dirs[d]))
               bad++;
      }
      for (i = 0; i < 8; i++)
         if ((int16_t)(dinput_joypad.axis(n % 3, AXIS_POS(i))
                  + dinput_joypad.axis(n % 3, AXIS_NEG(i))) != (int16_t)*axes[i])
            bad++;
      bad += bv_compare(&dinput_joypad, n % 3);
      bad += bv_compare_view(&dinput_joypad, n % 3);
   }

   /* a port with no pad on it has no buttons, and no view */
   {
      input_bits_t bits;
      g_pads[5].joypad = NULL;
      g_pads[5].joy_state.rgbButtons[3] = 0x80;
      dinput_pad_make_view(&g_pads[5]);
      if (dinput_joypad.view(5))
         bad++;
      dinput_joypad_get_buttons(5, &bits);
      for (i = 0; i < 256; i++)
         if (BIT256_GET(bits, i))
            bad++;
   }

   /* a pad that does not read holds nothing, its hats centred */
   g_pads[0].joy_state.rgdwPOV[0] = 0;
   dinput_pad_release_state(&g_pads[0]);
   for (i = 0; i < 4; i++)
      if (dinput_joypad.button(0, (uint16_t)HAT_MAP(0, HAT_UP_MASK)))
         bad++;

   if (bad)
   {
      printf("FAIL dinput_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS dinput_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
