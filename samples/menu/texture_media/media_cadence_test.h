/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIA_CADENCE_TEST_H
#define MEDIA_CADENCE_TEST_H

static bool media_cadence_check(struct menu_state *menu_st,
      menu_media_controls_t *media, void (*draw)(struct menu_state *))
{
   const char *expected = getenv("MEDIA_TEST_CADENCE_HZ");
   double hz;
   unsigned tick;
   if (!expected)
      return true;
   hz = strtod(expected, NULL);
   if (!(hz >= 5.0 && hz <= 240.0))
      return false;
   for (tick = 0; tick < 3000; tick++)
   {
      const gfx_media_playback_state_t *state;
      double rate;
      draw(menu_st);
      state = &media->controls.playback;
      if (state->cadence_num && state->cadence_den)
      {
         rate = (double)state->cadence_num / state->cadence_den;
         printf("movie cadence: %u/%u expected=%.6f dims=%u\n",
               state->cadence_num, state->cadence_den, hz, state->dims);
         return state->dims && rate >= hz * 0.999 && rate <= hz * 1.001;
      }
      retro_sleep(1);
   }
   return false;
}

#endif
