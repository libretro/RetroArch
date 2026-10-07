/* What the snapshot bridge (input_driver.c) makes of a joypad driver's
 * answers, written out without the copy: the buttons as get_buttons()
 * hands them over, a hat as its four directions through button(), an
 * axis as its two directions through axis() put back together, and the
 * RetroPad mask as the common loop over the binds.
 *
 * A driver is put on the bridge when this is what its own button(),
 * axis() and state() say. The tests here hold a driver to it over
 * random pads, binds and thresholds. */
#ifndef BRIDGE_VIEW_H
#define BRIDGE_VIEW_H

#define BV_AXES 16 /* INPUT_SNAPSHOT_AXES */
#define BV_HATS 4  /* INPUT_SNAPSHOT_HATS */

static int32_t bv_button(const input_device_driver_t *drv,
      unsigned pad, uint16_t joykey)
{
   uint16_t dir = GET_HAT_DIR(joykey);
   input_bits_t bits;

   if (dir)
   {
      unsigned h = GET_HAT(joykey);
      if (     h >= BV_HATS
            || (   dir != HAT_UP_MASK   && dir != HAT_DOWN_MASK
                && dir != HAT_LEFT_MASK && dir != HAT_RIGHT_MASK))
         return drv->button(pad, joykey);
      return drv->button(pad, (uint16_t)HAT_MAP(h, dir)) ? 1 : 0;
   }
   if (joykey >= 256)
      return drv->button(pad, joykey);
   drv->get_buttons(pad, &bits);
   return BIT256_GET(bits, joykey) ? 1 : 0;
}

static int16_t bv_axis(const input_device_driver_t *drv,
      unsigned pad, uint32_t joyaxis)
{
   bool negative = AXIS_NEG_GET(joyaxis) < BV_AXES;
   unsigned i    = negative ? AXIS_NEG_GET(joyaxis) : AXIS_POS_GET(joyaxis);
   int16_t value;

   if (i >= BV_AXES)
      return drv->axis(pad, joyaxis);
   value = (int16_t)(drv->axis(pad, AXIS_POS(i)) + drv->axis(pad, AXIS_NEG(i)));
   if (negative)
      return (value < 0) ? value : 0;
   return (value > 0) ? value : 0;
}

static int16_t bv_state(const input_device_driver_t *drv,
      rarch_joypad_info_t *info, const struct retro_keybind *binds)
{
   unsigned i;
   int16_t ret  = 0;
   uint16_t pad = info->joy_idx;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : info->auto_binds[i].joyaxis;
      if (     (uint16_t)joykey != NO_BTN
            && bv_button(drv, pad, (uint16_t)joykey))
         ret |= (1 << i);
      else if (joyaxis != AXIS_NONE
            && ((float)abs(bv_axis(drv, pad, joyaxis)) / 0x8000)
               > info->axis_threshold)
         ret |= (1 << i);
   }
   return ret;
}

/* ---- the comparison, the same for every driver --------------------- */

static unsigned bv_seed = 0x2545F491u;
static unsigned bv_rand(void)
{
   bv_seed ^= bv_seed << 13;
   bv_seed ^= bv_seed >> 17;
   bv_seed ^= bv_seed << 5;
   return bv_seed;
}

static void bv_random_binds(struct retro_keybind *b)
{
   static const uint16_t dirs[4] = {
      HAT_UP_MASK, HAT_DOWN_MASK, HAT_LEFT_MASK, HAT_RIGHT_MASK };
   unsigned i;
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      unsigned k = bv_rand() % 10;
      unsigned a = bv_rand() % 10;
      if (k < 3)
         b[i].joykey  = NO_BTN;
      else if (k < 8)
         b[i].joykey  = bv_rand() % 140; /* past what any pad has, too */
      else
         b[i].joykey  = HAT_MAP(bv_rand() % 5, dirs[bv_rand() % 4]);
      if (a < 4)
         b[i].joyaxis = AXIS_NONE;
      else if (a < 7)
         b[i].joyaxis = AXIS_POS(bv_rand() % 20);
      else
         b[i].joyaxis = AXIS_NEG(bv_rand() % 20);
   }
}

/* The driver's pad @pad has just been filled with something random.
 * Returns the number of disagreements. */
static unsigned bv_compare(const input_device_driver_t *drv, unsigned pad)
{
   static const float thresholds[] = { 0.0f, 0.01f, 0.25f, 0.5f, 0.75f, 0.999f, 1.0f };
   static const uint16_t dirs[4] = {
      HAT_UP_MASK, HAT_DOWN_MASK, HAT_LEFT_MASK, HAT_RIGHT_MASK };
   struct retro_keybind binds[RARCH_BIND_LIST_END];
   struct retro_keybind autos[RARCH_BIND_LIST_END];
   rarch_joypad_info_t info;
   unsigned bad = 0, i, t;

   /* every button, hat direction and axis direction on its own */
   for (i = 0; i < 300; i++)
      if ((bv_button(drv, pad, (uint16_t)i) != 0) != (drv->button(pad, (uint16_t)i) != 0))
         bad++;
   for (i = 0; i < 5; i++)
      for (t = 0; t < 4; t++)
      {
         uint16_t key = (uint16_t)HAT_MAP(i, dirs[t]);
         if ((bv_button(drv, pad, key) != 0) != (drv->button(pad, key) != 0))
            bad++;
      }
   for (i = 0; i < 24; i++)
   {
      if (bv_axis(drv, pad, AXIS_POS(i)) != drv->axis(pad, AXIS_POS(i)))
         bad++;
      if (bv_axis(drv, pad, AXIS_NEG(i)) != drv->axis(pad, AXIS_NEG(i)))
         bad++;
   }

   /* and the RetroPad mask, over binds and thresholds */
   memset(binds, 0, sizeof(binds));
   memset(autos, 0, sizeof(autos));
   info.auto_binds = autos;
   info.joy_idx    = (uint16_t)pad;
   for (i = 0; i < 6; i++)
   {
      bv_random_binds(binds);
      bv_random_binds(autos);
      for (t = 0; t < sizeof(thresholds) / sizeof(thresholds[0]); t++)
      {
         info.axis_threshold = thresholds[t];
         if (drv->state(&info, binds, pad) != bv_state(drv, &info, binds))
            bad++;
      }
   }
   return bad;
}

#endif
