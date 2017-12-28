#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include<libtransistor/nx.h>

#include "../input_driver.h"

#include "../../tasks/tasks_internal.h"

#include "../../retroarch.h"
#include "../../command.h"
#include "string.h"


#ifndef MAX_PADS
#define MAX_PADS 10
#endif

static uint16_t pad_state[MAX_PADS];
static int16_t analog_state[MAX_PADS][2][2];
extern uint64_t lifecycle_state;

static const char *switch_joypad_name(unsigned pad)
{
   return "Switch Controller";
}

static void switch_joypad_autodetect_add(unsigned autoconf_pad)
{
   if(!input_autoconfigure_connect(
            switch_joypad_name(autoconf_pad), /* name */
            NULL,                             /* display name */
            switch_joypad.ident,              /* driver */
            autoconf_pad,                     /* idx */
            0,                                /* vid */
            0))                               /* pid */
      input_config_set_device_name(autoconf_pad, switch_joypad_name(autoconf_pad));
}

static bool switch_joypad_init(void *data)
{
   hid_init();

   switch_joypad_autodetect_add(0);
   switch_joypad_autodetect_add(1);

   return true;
}

static bool switch_joypad_button(unsigned port_num, uint16_t key)
{
   if(port_num >= MAX_PADS)
      return false;

#if 0
   RARCH_LOG("button(%d, %d)\n", port_num, key);
#endif

   return (pad_state[port_num] & (1 << key));
}

static void switch_joypad_get_buttons(unsigned port_num, retro_bits_t *state)
{
   if(port_num < MAX_PADS)
   {
      BITS_COPY16_PTR(state, pad_state[port_num]);
   }
   else
   {
      BIT256_CLEAR_ALL_PTR(state);
   }
}

static int16_t switch_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   int val = 0;
   int axis = -1;
   bool is_neg = false;
   bool is_pos = false;

   if(joyaxis == AXIS_NONE || port_num >= MAX_PADS)
   {
      /* TODO/FIXME - implement */
   }

   if(AXIS_NEG_GET(joyaxis) < 4)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if(AXIS_POS_GET(joyaxis) < 4)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   switch(axis)
   {
      case 0:
         val = analog_state[port_num][0][0];
         break;
      case 1:
         val = analog_state[port_num][0][1];
         break;
      case 2:
         val = analog_state[port_num][1][0];
         break;
      case 3:
         val = analog_state[port_num][1][1];
         break;
   }

   if(is_neg && val > 0)
      val = 0;
   else if(is_pos && val < 0)
      val = 0;

   return val;
}

static bool switch_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PADS && pad_state[pad];
}

static void switch_joypad_destroy(void)
{
   hid_finalize();
}

static void switch_joypad_poll(void)
{
   hid_controller_t    *controllers = hid_get_shared_memory()->controllers;
   hid_controller_t           *cont = &controllers[0];
   hid_controller_state_entry_t ent = cont->main.entries[cont->main.latest_idx];
   pad_state[0]                     = 0;
   pad_state[0]                    |= (ent.button_state & 0x1000) ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
   pad_state[0]                    |= (ent.button_state & 0x8000) ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
   pad_state[0]                    |= (ent.button_state & 0x4000) ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
   pad_state[0]                    |= (ent.button_state & 0x2000) ? (1 << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
   pad_state[0]                    |= (ent.button_state & 0x0400) ? (1 << RETRO_DEVICE_ID_JOYPAD_START) : 0;
   pad_state[0]                    |= (ent.button_state & 0x0800) ? (1 << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
   pad_state[0]                    |= (ent.button_state & 0x0004) ? (1 << RETRO_DEVICE_ID_JOYPAD_X) : 0;
   pad_state[0]                    |= (ent.button_state & 0x0008) ? (1 << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
   pad_state[0]                    |= (ent.button_state & 0x0002) ? (1 << RETRO_DEVICE_ID_JOYPAD_B) : 0;
   pad_state[0]                    |= (ent.button_state & 0x0001) ? (1 << RETRO_DEVICE_ID_JOYPAD_A) : 0;
   pad_state[0]                    |= (ent.button_state & 0x0080) ? (1 << RETRO_DEVICE_ID_JOYPAD_R) : 0;
   pad_state[0]                    |= (ent.button_state & 0x0040) ? (1 << RETRO_DEVICE_ID_JOYPAD_L) : 0;
   pad_state[0]                    |= (ent.button_state & 0x0200) ? (1 << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
   pad_state[0]                    |= (ent.button_state & 0x0100) ? (1 << RETRO_DEVICE_ID_JOYPAD_L2) : 0;

   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X]  = ent.left_stick_x / 0x20000;
   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y]  = ent.left_stick_y / 0x20000;
   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = ent.left_stick_x / 0x20000;
   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = ent.left_stick_y / 0x20000;
}

input_device_driver_t switch_joypad = {
	switch_joypad_init,
	switch_joypad_query_pad,
	switch_joypad_destroy,
	switch_joypad_button,
	switch_joypad_get_buttons,
	switch_joypad_axis,
	switch_joypad_poll,
	NULL, /* set_rumble */
	switch_joypad_name,
	"switch"
};
