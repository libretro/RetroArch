#include "../input_keymaps.h"
#include "../../driver.h"

#include "keyboard_event_udev.h"


#define UDEV_MAX_KEYS (KEY_MAX + 7) / 8

static uint8_t udev_key_state[UDEV_MAX_KEYS];

#ifdef HAVE_XKBCOMMON
void free_xkb(void);

void handle_xkb(int code, int value);
#endif

void udev_handle_keyboard(void *data,
      const struct input_event *event, udev_input_device_t *dev)
{
   switch (event->type)
   {
      case EV_KEY:
         if (event->value)
            BIT_SET(udev_key_state, event->code);
         else
            BIT_CLEAR(udev_key_state, event->code);

#ifdef HAVE_XKBCOMMON
         handle_xkb(event->code, event->value);
#endif
         break;

      default:
         break;
   }
}

bool udev_input_is_pressed(const struct retro_keybind *binds, unsigned id)
{
   if (id < RARCH_BIND_LIST_END)
   {
      const struct retro_keybind *bind = &binds[id];
      unsigned bit = input_keymaps_translate_rk_to_keysym(binds[id].key);
      return bind->valid && BIT_GET(udev_key_state, bit);
   }
   return false;
}

bool udev_input_state_kb(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   unsigned bit = input_keymaps_translate_rk_to_keysym((enum retro_key)id);
   return id < RETROK_LAST && BIT_GET(udev_key_state, bit);
}

void udev_input_kb_free(void)
{
   unsigned i;

#ifdef HAVE_XKBCOMMON
   free_xkb();
#endif

   for (i = 0; i < UDEV_MAX_KEYS; i++)
      udev_key_state[i] = 0;
}
