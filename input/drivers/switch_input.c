#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../input_driver.h"

#define MAX_PADS 10

typedef struct switch_input
{
   const input_device_driver_t *joypad;
   bool blocked;
} switch_input_t;

static void switch_input_poll(void *data)
{
	switch_input_t *sw = (switch_input_t*) data;

	if (sw->joypad)
		sw->joypad->poll();
}

static int16_t switch_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   switch_input_t *sw = (switch_input_t*) data;

   if (port > MAX_PADS-1)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return input_joypad_pressed(sw->joypad,
               joypad_info, port, binds[port], id);
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return input_joypad_analog(sw->joypad,
                  joypad_info, port, idx, id, binds[port]);
         break;
   }

   return 0;
}

static void switch_input_free_input(void *data)
{
   switch_input_t *sw = (switch_input_t*) data;

   if (sw && sw->joypad)
      sw->joypad->destroy();

   free(sw);
}

static void* switch_input_init(const char *joypad_driver)
{
   switch_input_t *sw = (switch_input_t*) calloc(1, sizeof(*sw));
   if (!sw)
      return NULL;

   sw->joypad = input_joypad_init_driver(joypad_driver, sw);

   return sw;
}

static uint64_t switch_input_get_capabilities(void *data)
{
   (void) data;

   return (1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG);
}

static const input_device_driver_t *switch_input_get_joypad_driver(void *data)
{
   switch_input_t *sw = (switch_input_t*) data;
   if (sw)
      return sw->joypad;
   return NULL;
}

static void switch_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool switch_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
	(void)data;
	(void)port;
	(void)effect;
	(void)strength;
	
	return false;
}

static bool switch_input_keyboard_mapping_is_blocked(void *data)
{
   switch_input_t *sw = (switch_input_t*) data;
   if (!sw)
      return false;
   return sw->blocked;
}

static void switch_input_keyboard_mapping_set_block(void *data, bool value)
{
   switch_input_t *sw = (switch_input_t*) data;
   if (!sw)
      return;
   sw->blocked = value;
}

input_driver_t input_switch = {
	switch_input_init,
	switch_input_poll,
	switch_input_state,
	switch_input_free_input,
	NULL,
	NULL,
	switch_input_get_capabilities,
	"switch",
	switch_input_grab_mouse,
	NULL,
	switch_input_set_rumble,
	switch_input_get_joypad_driver,
	NULL,
	switch_input_keyboard_mapping_is_blocked,
	switch_input_keyboard_mapping_set_block,
};
