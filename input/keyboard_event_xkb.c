#include <xkbcommon/xkbcommon.h>
#include "input_context.h"
#include "input_keymaps.h"
#include "keyboard_line.h"

#define MOD_MAP_SIZE 5

/* FIXME: Don't handle composed and dead-keys properly. 
 * Waiting for support in libxkbcommon ... */
void handle_xkb(
      struct xkb_state *xkb_state, 
      xkb_mod_index_t *mod_map_idx,
      uint16_t *mod_map_bit,
      int code, int value)
{
   unsigned i;
   const xkb_keysym_t *syms = NULL;
   unsigned num_syms = 0;
   uint16_t mod = 0;
   /* Convert Linux evdev to X11 (xkbcommon docs say so at least ...) */
   int xk_code = code + 8;

   if (!xkb_state)
      return;

   if (value == 2) /* Repeat, release first explicitly. */
      xkb_state_update_key(xkb_state, xk_code, XKB_KEY_UP);

   if (value)
      num_syms = xkb_state_key_get_syms(xkb_state, xk_code, &syms);

   xkb_state_update_key(xkb_state, xk_code, value ? XKB_KEY_DOWN : XKB_KEY_UP);

   /* Build mod state. */
   for (i = 0; i < MOD_MAP_SIZE; i++)
   {
      xkb_mod_index_t *map_idx = (xkb_mod_index_t*)&mod_map_idx[i];
      uint16_t        *map_bit = (uint16_t       *)&mod_map_bit[i];

      if (*map_idx != XKB_MOD_INVALID)
         mod |= xkb_state_mod_index_is_active(
               xkb_state,
               *map_idx,
               (enum xkb_state_component)((XKB_STATE_MODS_EFFECTIVE) > 0)) ? *map_bit : 0;
   }

   input_keyboard_event(value, input_translate_keysym_to_rk(code),
         num_syms ? xkb_keysym_to_utf32(syms[0]) : 0, mod);
   for (i = 1; i < num_syms; i++)
      input_keyboard_event(value, RETROK_UNKNOWN, xkb_keysym_to_utf32(syms[i]), mod);
}
