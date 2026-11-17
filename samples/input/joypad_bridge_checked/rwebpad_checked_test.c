/* The Emscripten (web) joypad driver, held to what the snapshot bridge
 * assumes of a driver it serves (see bridge_view.h). Built with emcc
 * and run under node: the driver source is included for its pad table,
 * which is filled here by hand as its poll fills it from the browser's
 * gamepad events. */
#include "../../../input/drivers_joypad/rwebpad_joypad.c"

#include "bridge_view.h"

/* the frontend platform's; the poll is never run here */
void platform_emscripten_run_on_browser_thread_sync(void (*func)(void*),
      void *data)
{ func(data); }

int main(void)
{
   static rwebpad_joypad_data_t data;
   static const double values[] = { 0.0, 1.0, -1.0, 0.5, -0.5, 2.0, -2.0,
      0.000015, -0.000015, 0.99999 };
   unsigned n, i, bad = 0;

   rwebpad_joypad_data = &data;
   for (n = 0; n < 4000; n++)
   {
      unsigned pad                 = n % 3;
      EmscriptenGamepadEvent *ev   = &data.pads[pad];

      data.live_pads[pad] = true;
      /* as many as a pad may say it has, and past what the event holds */
      ev->numButtons      = (int)(bv_rand() % 80);
      ev->numAxes         = (int)(bv_rand() % 80);
      for (i = 0; i < 64; i++)
      {
         ev->digitalButton[i] = (bv_rand() & 1) ? EM_TRUE : EM_FALSE;
         ev->axis[i]          = (bv_rand() & 1)
            ? values[bv_rand() % (sizeof(values) / sizeof(values[0]))]
            : ((double)(int)(bv_rand() % 20001) - 10000.0) / 10000.0;
      }
      bad += bv_compare(&rwebpad_joypad, pad);
   }

   /* a pad gone, and a port past the pads the driver keeps, have no
    * buttons, whatever the copy handed in held */
   {
      input_bits_t bits;
      data.live_pads[1] = false;
      memset(&bits, 0xff, sizeof(bits));
      rwebpad_joypad_get_buttons(1, &bits);
      for (i = 0; i < 256; i++)
         if (BIT256_GET(bits, i))
            bad++;
      memset(&bits, 0xff, sizeof(bits));
      rwebpad_joypad_get_buttons(DEFAULT_MAX_PADS, &bits);
      for (i = 0; i < 256; i++)
         if (BIT256_GET(bits, i))
            bad++;
   }

   if (bad)
   {
      printf("FAIL rwebpad_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS rwebpad_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
