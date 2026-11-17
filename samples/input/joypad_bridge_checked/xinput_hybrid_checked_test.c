/* The Windows joypad driver with XInput and DirectInput pads side by
 * side (xinput_hybrid_joypad.c), held to what the snapshot bridge
 * assumes of a driver it serves (see bridge_view.h). The driver source
 * is included for its tables; DirectInput's pad table and its views
 * come from dinput_joypad.c, linked beside it as in the build. Ports 0
 * and 1 are XInput pads, 2 and 3 DirectInput ones: each pad's state is
 * filled here by hand and made its view as the driver's poll makes it.
 * Neither API is called, so it runs under Wine with no controller. */
#include "../../../input/drivers_joypad/xinput_hybrid_joypad.c"

#include "bridge_view.h"

/* the frontend's; the user's list is the settings', here none */
bool input_device_ignored(uint16_t vid, uint16_t pid)
{ return input_ignore_devices_check(NULL, vid, pid); }

int main(void)
{
   static const DWORD povs[] = {
      0xFFFFFFFF, 0, 4500, 9000, 13500, 18000, 22500, 27000, 31500,
      100, 36000 };
   unsigned n, i, bad = 0;

   g_xinput_num_buttons   = 11;
   g_xinput_pad_indexes[0] = 0;
   g_xinput_pad_indexes[1] = 1;
   for (i = 2; i < MAX_USERS; i++)
      g_xinput_pad_indexes[i] = -1;

   for (n = 0; n < 4000; n++)
   {
      unsigned port = n % 4;

      if (port < 2)
      {
         XINPUT_GAMEPAD *pad = &g_xinput_states[port].xstate.Gamepad;
         g_xinput_states[port].connected = true;
         pad->wButtons      = (WORD)bv_rand();
         pad->bLeftTrigger  = (BYTE)bv_rand();
         pad->bRightTrigger = (BYTE)bv_rand();
         pad->sThumbLX      = (SHORT)bv_rand_axis();
         pad->sThumbLY      = (SHORT)bv_rand_axis();
         pad->sThumbRX      = (SHORT)bv_rand_axis();
         pad->sThumbRY      = (SHORT)bv_rand_axis();
         xinput_joypad_make_view(&g_xinput_states[port]);
         for (i = 0; i < g_xinput_num_buttons; i++)
            if (     (xinput_joypad.button(port, (uint16_t)i) != 0)
                  != ((pad->wButtons & button_index_to_bitmap_code[i]) != 0))
               bad++;
         if (     (xinput_joypad.button(port, HAT_MAP(0, HAT_LEFT_MASK)) != 0)
               != ((pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0))
            bad++;
         if (xinput_joypad.axis(port, AXIS_POS(5)) != pad->bRightTrigger * 32767 / 255)
            bad++;
      }
      else
      {
         struct dinput_joypad_data *pad = &g_pads[port];
         pad->joypad = (LPDIRECTINPUTDEVICE8)(uintptr_t)1; /* never called through */
         for (i = 0; i < ARRAY_SIZE_RGB_BUTTONS; i++)
            pad->joy_state.rgbButtons[i] = (bv_rand() & 1) ? 0x80 : 0;
         for (i = 0; i < 4; i++)
            pad->joy_state.rgdwPOV[i] = povs[bv_rand() % (sizeof(povs) / sizeof(povs[0]))];
         pad->joy_state.lX  = bv_rand_axis();
         pad->joy_state.lRz = bv_rand_axis();
         dinput_pad_make_view(pad);
         for (i = 0; i < ARRAY_SIZE_RGB_BUTTONS; i++)
            if (     (xinput_joypad.button(port, (uint16_t)i) != 0)
                  != (pad->joy_state.rgbButtons[i] != 0))
               bad++;
         if (     (xinput_joypad.button(port, HAT_MAP(1, HAT_UP_MASK)) != 0)
               != (   pad->joy_state.rgdwPOV[1] == 0
                   || pad->joy_state.rgdwPOV[1] == 4500
                   || pad->joy_state.rgdwPOV[1] == 31500))
            bad++;
         if ((int16_t)(xinput_joypad.axis(port, AXIS_POS(5))
                  + xinput_joypad.axis(port, AXIS_NEG(5)))
               != (int16_t)pad->joy_state.lRz)
            bad++;
      }
      bad += bv_compare(&xinput_joypad, port);
      bad += bv_compare_view(&xinput_joypad, port);
   }

   /* a DirectInput pad that does not read holds nothing, its hats
    * centred rather than up */
   g_pads[2].joy_state.rgdwPOV[0] = 0;
   dinput_pad_release_state(&g_pads[2]);
   if (xinput_joypad.button(2, HAT_MAP(0, HAT_UP_MASK)))
      bad++;
   /* ports with nothing on them, and past the table */
   g_xinput_states[1].connected = false;
   g_pads[5].joypad             = NULL;
   if (     xinput_joypad.view(1) || xinput_joypad.view(5)
         || xinput_joypad.view(MAX_USERS) || xinput_joypad.button(MAX_USERS, 0))
      bad++;

   if (bad)
   {
      printf("FAIL xinput_hybrid_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS xinput_hybrid_checked_test: 4000 random pads; what the bridge"
         " would read is what the driver says\n");
   return 0;
}
