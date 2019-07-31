#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../config.def.h"

#ifdef HAVE_LIBNX
#include <switch.h>
#else
#include <libtransistor/nx.h>
#endif

#include "../configuration.h"

#include "../../tasks/tasks_internal.h"

#include "../../retroarch.h"
#include "../../command.h"
#include "string.h"

static uint16_t pad_state[DEFAULT_MAX_PADS];
static int16_t analog_state[DEFAULT_MAX_PADS][2][2];
extern uint64_t lifecycle_state;

#ifdef HAVE_LIBNX
static u32 vibration_handles[DEFAULT_MAX_PADS][2];
static u32 vibration_handleheld[2];
static HidVibrationValue vibration_values[DEFAULT_MAX_PADS][2];
static HidVibrationValue vibration_stop;
static int previous_handheld                         = -1; 
/* 1 = handheld, 0 = docked, -1 = first use */
static uint previous_split_joycon_setting[MAX_USERS] = { 0 };
#endif

static const char *switch_joypad_name(unsigned pad)
{
   return "Switch Controller";
}

static void switch_joypad_autodetect_add(unsigned autoconf_pad)
{
   input_autoconfigure_connect(
            switch_joypad_name(autoconf_pad), /* name */
            NULL,                             /* display name */
            switch_joypad.ident,              /* driver */
            autoconf_pad,                     /* idx */
            0,                                /* vid */
            0);                               /* pid */
}

static bool switch_joypad_init(void *data)
{
#ifdef HAVE_LIBNX
   unsigned i;
   hidScanInput();

   /* Switch like stop behavior with muted band channels 
    * and frequencies set to default. */
   vibration_stop.amp_low   = 0.0f;
   vibration_stop.freq_low  = 160.0f;
   vibration_stop.amp_high  = 0.0f;
   vibration_stop.freq_high = 320.0f;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      switch_joypad_autodetect_add(i);
      hidInitializeVibrationDevices(
            vibration_handles[i], 2, i,
            TYPE_HANDHELD | TYPE_JOYCON_PAIR);
      memcpy(&vibration_values[i][0],
            &vibration_stop, sizeof(HidVibrationValue));
      memcpy(&vibration_values[i][1],
            &vibration_stop, sizeof(HidVibrationValue));
   }
   hidInitializeVibrationDevices(vibration_handleheld,
         2, CONTROLLER_HANDHELD, TYPE_HANDHELD | TYPE_JOYCON_PAIR);
#else
   hid_init();
   switch_joypad_autodetect_add(0);
   switch_joypad_autodetect_add(1);
#endif

   return true;
}

static bool switch_joypad_button(unsigned port_num, uint16_t key)
{
   if (port_num >= DEFAULT_MAX_PADS)
      return false;

#if 0
   RARCH_LOG("button(%d, %d)\n", port_num, key);
#endif

   return (pad_state[port_num] & (1 << key));
}

static void switch_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
   if (port_num < DEFAULT_MAX_PADS)
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
   int val     = 0;
   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

#if 0
   /* TODO/FIXME - implement */
   if (joyaxis == AXIS_NONE || port_num >= DEFAULT_MAX_PADS) { }
#endif

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
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

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static bool switch_joypad_query_pad(unsigned pad)
{
   return pad < DEFAULT_MAX_PADS && pad_state[pad];
}

static void switch_joypad_destroy(void)
{
#ifdef HAVE_LIBNX
   unsigned i;

   previous_handheld = -1;

   for (i = 0; i < MAX_USERS; i++)
      previous_split_joycon_setting[i] = 0;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      memcpy(&vibration_values[i][0],
            &vibration_stop, sizeof(HidVibrationValue));
      memcpy(&vibration_values[i][1],
            &vibration_stop, sizeof(HidVibrationValue));
      hidSendVibrationValues(vibration_handles[i], vibration_values[i], 2);
   }
   hidSendVibrationValues(vibration_handleheld, vibration_values[0], 2);
#else
   hid_finalize();
#endif
}

#ifdef HAVE_LIBNX

static void switch_joypad_poll(void)
{
   int i, handheld;
   settings_t *settings = config_get_ptr();

   hidScanInput();

   handheld = hidGetHandheldMode();
   
   if (previous_handheld == -1)
   {
      /* First call of this function, apply joycon settings 
       * according to preferences, init variables */
      if (!handheld)
      {
         for (i = 0; i < MAX_USERS; i += 2)
         {
            if (settings->uints.input_split_joycon[i])
            {
               hidSetNpadJoyAssignmentModeSingleByDefault(i);
               hidSetNpadJoyAssignmentModeSingleByDefault(i + 1);
               hidSetNpadJoyHoldType(HidJoyHoldType_Horizontal);
            } 
            else if (!settings->uints.input_split_joycon[i])
            {
               hidSetNpadJoyAssignmentModeDual(i);
               hidSetNpadJoyAssignmentModeDual(i + 1);
               hidMergeSingleJoyAsDualJoy(i, i + 1);
            }
         }
      }
      previous_handheld = handheld;
      for (i = 0; i < MAX_USERS; i += 2)
         previous_split_joycon_setting[i] = settings->uints.input_split_joycon[i];
   }

   if (!handheld && previous_handheld)
   {
      /* switching out of handheld, so make sure 
       * joycons are correctly split. */
      for (i = 0; i < MAX_USERS; i += 2)
      {
         /* CONTROLLER_PLAYER_X, X == i++ */
         if (settings->uints.input_split_joycon[i])
         {
            hidSetNpadJoyAssignmentModeSingleByDefault(i);
            hidSetNpadJoyAssignmentModeSingleByDefault(i + 1);
            hidSetNpadJoyHoldType(HidJoyHoldType_Horizontal);
         }
      }
   }
   else if (handheld && !previous_handheld)
   {
      /* switching into handheld, so make sure all split joycons are joined */
      for (i = 0; i < MAX_USERS; i += 2)
      {
         /* find all left/right single JoyCon pairs and join them together */
         int id, id_0, id_1;
         int last_right_id = MAX_USERS;
         for (id = 0; id < MAX_USERS; id++)
            hidSetNpadJoyAssignmentModeDual(id);

         for (id_0 = 0; id_0 < MAX_USERS; id_0++)
         {
            if (hidGetControllerType(id_0) & TYPE_JOYCON_LEFT)
            {
               for (id_1 = last_right_id - 1; id_1 >= 0; id_1--)
               {
                  if (hidGetControllerType(id_1) & TYPE_JOYCON_RIGHT)
                  {
                     /* prevent missing player numbers */
                     last_right_id = id_1;
                     if (id_0 < id_1)
                        hidMergeSingleJoyAsDualJoy(id_0, id_1);
                     else if (id_0 > id_1)
                        hidMergeSingleJoyAsDualJoy(id_1, id_0);
                     break;
                  }
               }
            }
         }
      }
   }
   else if (!handheld)
   {
      /* split or join joycons every time the user changes a setting */
      for (i = 0; i < MAX_USERS; i += 2)
      {
         if (      settings->uints.input_split_joycon[i] 
               && !previous_split_joycon_setting[i])
         {
            hidSetNpadJoyAssignmentModeSingleByDefault(i);
            hidSetNpadJoyAssignmentModeSingleByDefault(i + 1);
            hidSetNpadJoyHoldType(HidJoyHoldType_Horizontal);
         } 
         else if (!settings->uints.input_split_joycon[i] 
               && previous_split_joycon_setting[i])
         {
            hidSetNpadJoyAssignmentModeDual(i);
            hidSetNpadJoyAssignmentModeDual(i + 1);
            hidMergeSingleJoyAsDualJoy(i, i + 1);
         }
      }
   }

   for (i = 0; i < MAX_USERS; i += 2)
      previous_split_joycon_setting[i] = settings->uints.input_split_joycon[i];
   previous_handheld = handheld;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      JoystickPosition joy_position_left, joy_position_right;
      HidControllerID target        = (i == 0) ? CONTROLLER_P1_AUTO : i;
      pad_state[i]                  = hidKeysDown(target) | hidKeysHeld(target);

      hidJoystickRead(&joy_position_left, target, JOYSTICK_LEFT);
      hidJoystickRead(&joy_position_right, target, JOYSTICK_RIGHT);

      analog_state[i][RETRO_DEVICE_INDEX_ANALOG_LEFT]
         [RETRO_DEVICE_ID_ANALOG_X] = joy_position_left.dx;
      analog_state[i][RETRO_DEVICE_INDEX_ANALOG_LEFT]
         [RETRO_DEVICE_ID_ANALOG_Y] = -joy_position_left.dy;
      analog_state[i][RETRO_DEVICE_INDEX_ANALOG_RIGHT]
         [RETRO_DEVICE_ID_ANALOG_X] = joy_position_right.dx;
      analog_state[i][RETRO_DEVICE_INDEX_ANALOG_RIGHT]
         [RETRO_DEVICE_ID_ANALOG_Y] = -joy_position_right.dy;
   }
}
#else
static void switch_joypad_poll(void)
{
   int16_t lsx, lsy, rsx, rsy;
   hid_controller_t    *controllers  = hid_get_shared_memory()->controllers;
   hid_controller_t           *cont  = &controllers[0];
   hid_controller_state_entry_t ent  = cont->main.entries[cont->main.latest_idx];
   hid_controller_state_entry_t ent8 = (cont+8)->main.entries[(cont+8)->main.latest_idx];
   pad_state[0]                      = ent.button_state | ent8.button_state;

   lsx                               = ent.left_stick_x;
   lsy                               = ent.left_stick_y;
   rsx                               = ent.right_stick_x;
   rsy                               = ent.right_stick_y;

   if (ent8.left_stick_x != 0 || ent8.left_stick_y != 0)
   {
      /* handheld overrides player 1 */
	   lsx                            = ent8.left_stick_x;
	   lsy                            = ent8.left_stick_y;
   }

   if (ent8.right_stick_x != 0 || ent8.right_stick_y != 0)
   {
      /* handheld overrides player 1 */
	   rsx                            = ent8.right_stick_x;
	   rsy                            = ent8.right_stick_y;
   }

   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT]
      [RETRO_DEVICE_ID_ANALOG_X]     = lsx;
   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT]
      [RETRO_DEVICE_ID_ANALOG_Y]     = -lsy;
   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT]
      [RETRO_DEVICE_ID_ANALOG_X]     = rsx;
   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT]
      [RETRO_DEVICE_ID_ANALOG_Y]     = -rsy;
}
#endif

#ifdef HAVE_LIBNX
bool switch_joypad_set_rumble(unsigned pad,
      enum retro_rumble_effect type, uint16_t strength)
{
   u32* handle;
   float amp;

   if (pad >= DEFAULT_MAX_PADS || !vibration_handles[pad])
      return false;

   amp  = (float)strength / 65535.0f;
   amp *= 0.5f; /* Max strength is too strong */

   if (type == RETRO_RUMBLE_STRONG)
   {
      vibration_values[pad][0].amp_low = amp;
      vibration_values[pad][1].amp_low = amp;
   }
   else
   {
      vibration_values[pad][0].amp_high = amp;
      vibration_values[pad][1].amp_high = amp;
   }

   handle = (pad == 0 && hidGetHandheldMode()) 
      ? vibration_handleheld : vibration_handles[pad];
   return R_SUCCEEDED(hidSendVibrationValues(handle, vibration_values[pad], 2));
}
#endif

input_device_driver_t switch_joypad = {
	switch_joypad_init,
	switch_joypad_query_pad,
	switch_joypad_destroy,
	switch_joypad_button,
	switch_joypad_get_buttons,
	switch_joypad_axis,
	switch_joypad_poll,
#ifdef HAVE_LIBNX
   switch_joypad_set_rumble,
#else
	NULL, /* set_rumble */
#endif
	switch_joypad_name,
	"switch"
};
