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
/* TODO/FIXME - weird header include */
#include "string.h"

/* TODO/FIXME - global referenced outside */
extern uint64_t lifecycle_state;

/* TODO/FIXME - static globals */
static uint16_t button_state[DEFAULT_MAX_PADS];
static int16_t analog_state[DEFAULT_MAX_PADS][2][2];
#ifdef HAVE_LIBNX
static PadState pad_states[DEFAULT_MAX_PADS];
static HidVibrationDeviceHandle vibration_handles[DEFAULT_MAX_PADS][2];
static HidVibrationDeviceHandle vibration_handleheld[2];
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

static void *switch_joypad_init(void *data)
{
#ifdef HAVE_LIBNX
   unsigned i;
   hidSetNpadJoyHoldType(HidNpadJoyHoldType_Horizontal);
   padConfigureInput(DEFAULT_MAX_PADS, HidNpadStyleSet_NpadStandard);

   /* Switch like stop behavior with muted band channels 
    * and frequencies set to default. */
   vibration_stop.amp_low   = 0.0f;
   vibration_stop.freq_low  = 160.0f;
   vibration_stop.amp_high  = 0.0f;
   vibration_stop.freq_high = 320.0f;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      if(i == 0)
         padInitializeDefault(&pad_states[0]);
      else
         padInitialize(&pad_states[i], i);

      padUpdate(&pad_states[i]);
      switch_joypad_autodetect_add(i);
      hidInitializeVibrationDevices(
            vibration_handles[i], 2, i,
            HidNpadStyleTag_NpadHandheld | HidNpadStyleTag_NpadJoyDual);
      memcpy(&vibration_values[i][0],
            &vibration_stop, sizeof(HidVibrationValue));
      memcpy(&vibration_values[i][1],
            &vibration_stop, sizeof(HidVibrationValue));
   }
   hidInitializeVibrationDevices(vibration_handleheld,
         2, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld | HidNpadStyleTag_NpadJoyDual);
#else
   hid_init();
   switch_joypad_autodetect_add(0);
   switch_joypad_autodetect_add(1);
#endif

   return (void*)-1;
}

static int32_t switch_joypad_button(unsigned port_num, uint16_t joykey)
{
   if (port_num >= DEFAULT_MAX_PADS)
      return 0;
   return (button_state[port_num] & (1 << joykey));
}

static void switch_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
   if (port_num < DEFAULT_MAX_PADS)
   {
      BITS_COPY16_PTR(state, button_state[port_num]);
   }
   else
   {
      BIT256_CLEAR_ALL_PTR(state);
   }
}

static int16_t switch_joypad_axis_state(unsigned port, uint32_t joyaxis)
{
   int val     = 0;
   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      axis   = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      axis   = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   switch(axis)
   {
      case 0:
      case 1:
         val = analog_state[port][0][axis];
         break;
      case 2:
      case 3:
         val = analog_state[port][1][axis - 2];
         break;
   }

   if (is_neg && val > 0)
      return 0;
   else if (is_pos && val < 0)
      return 0;
   return val;
}

static int16_t switch_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return switch_joypad_axis_state(port, joyaxis);
}

static int16_t switch_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;

   if (port_idx >= DEFAULT_MAX_PADS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN 
            && (button_state[port_idx] & (1 << (uint16_t)joykey))
         )
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(switch_joypad_axis_state(port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static bool switch_joypad_query_pad(unsigned pad)
{
   return pad < DEFAULT_MAX_PADS && button_state[pad];
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

   for(i = 0; i < DEFAULT_MAX_PADS; i++)
      padUpdate(&pad_states[i]);

   handheld = padIsHandheld(&pad_states[0]);

   if (previous_handheld == -1)
   {
      /* First call of this function, apply joycon settings 
       * according to preferences, init variables */
      if (!handheld)
      {
         for (i = 0; i < MAX_USERS; i += 2)
         {
            unsigned input_split_joycon = 
               settings->uints.input_split_joycon[i];

            if (input_split_joycon)
            {
               hidSetNpadJoyAssignmentModeSingleByDefault(i);
               hidSetNpadJoyAssignmentModeSingleByDefault(i + 1);
            } 
            else if (!input_split_joycon)
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
         unsigned input_split_joycon = settings->uints.input_split_joycon[i];

         /* CONTROLLER_PLAYER_X, X == i++ */
         if (input_split_joycon)
         {
            hidSetNpadJoyAssignmentModeSingleByDefault(i);
            hidSetNpadJoyAssignmentModeSingleByDefault(i + 1);
         }
         else
         {
            hidSetNpadJoyAssignmentModeDual(i);
            hidSetNpadJoyAssignmentModeDual(i + 1);
            hidMergeSingleJoyAsDualJoy(i, i + 1);
         }
      }
   }
   else if (!handheld)
   {
      /* split or join joycons every time the user changes a setting */
      for (i = 0; i < MAX_USERS; i += 2)
      {
         unsigned input_split_joycon = settings->uints.input_split_joycon[i];
         if (input_split_joycon
               && !previous_split_joycon_setting[i])
         {
            hidSetNpadJoyAssignmentModeSingleByDefault(i);
            hidSetNpadJoyAssignmentModeSingleByDefault(i + 1);
         } 
         else if (!input_split_joycon
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
      HidAnalogStickState stick_left_state = padGetStickPos(&pad_states[i], 0);
      HidAnalogStickState stick_right_state = padGetStickPos(&pad_states[i], 1);
      unsigned input_split_joycon = settings->uints.input_split_joycon[i];
      int pad_button = padGetButtons(&pad_states[i]);
      if (input_split_joycon && !handheld) {
         button_state[i] = 0;

         if (hidGetNpadDeviceType((HidNpadIdType)i) & HidDeviceTypeBits_JoyLeft) {
            if (pad_button & HidNpadButton_Left)
               button_state[i] |= (uint16_t)HidNpadButton_B;
            if (pad_button & HidNpadButton_Up)
               button_state[i] |= (uint16_t)HidNpadButton_Y;
            if (pad_button & HidNpadButton_Right)
               button_state[i] |= (uint16_t)HidNpadButton_X;
            if (pad_button & HidNpadButton_Down)
               button_state[i] |= (uint16_t)HidNpadButton_A;
            if (pad_button & HidNpadButton_LeftSL)
               button_state[i] |= (uint16_t)HidNpadButton_L;
            if (pad_button & HidNpadButton_LeftSR)
               button_state[i] |= (uint16_t)HidNpadButton_R;
            
            analog_state[i][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] = -stick_left_state.y;
            analog_state[i][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] = -stick_left_state.x;
         } else if (hidGetNpadDeviceType((HidNpadIdType)i) & HidDeviceTypeBits_JoyRight) {
            if (pad_button & HidNpadButton_A)
               button_state[i] |= (uint16_t)HidNpadButton_B;
            if (pad_button & HidNpadButton_B)
               button_state[i] |= (uint16_t)HidNpadButton_Y;
            if (pad_button & HidNpadButton_X)
               button_state[i] |= (uint16_t)HidNpadButton_A;
            if (pad_button & HidNpadButton_Y)
               button_state[i] |= (uint16_t)HidNpadButton_X;
            if (pad_button & HidNpadButton_RightSL)
               button_state[i] |= (uint16_t)HidNpadButton_L;
            if (pad_button & HidNpadButton_RightSR)
               button_state[i] |= (uint16_t)HidNpadButton_R;
            if (pad_button & HidNpadButton_StickR)
               button_state[i] |= (uint16_t)HidNpadButton_StickL;
            
            /* Throw JoyRight state into retro left analog */
            analog_state[i][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] = stick_right_state.y;
            analog_state[i][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] = stick_right_state.x;
         } else {
            /* Handle other types via Default Input Handling */
            goto lblDefaultInputHandling;
         }
      }
      else
      {
         /* Default Input Handling */
lblDefaultInputHandling:
         button_state[i] = pad_button;
         analog_state[i][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] = stick_left_state.x;
         analog_state[i][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] = -stick_left_state.y;
         
         analog_state[i][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = stick_right_state.x;
         analog_state[i][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = -stick_right_state.y;
      }
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
   button_state[0]                   = ent.button_state | ent8.button_state;

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
   HidVibrationDeviceHandle* handle;
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

   handle = (pad == 0 && !padIsNpadActive(&pad_states[0], HidNpadIdType_No1))
      ? vibration_handleheld : vibration_handles[pad];
   return R_SUCCEEDED(hidSendVibrationValues(handle, vibration_values[pad], 2));
}
#endif

input_device_driver_t switch_joypad = {
   switch_joypad_init,
   switch_joypad_query_pad,
   switch_joypad_destroy,
   switch_joypad_button,
   switch_joypad_state,
   switch_joypad_get_buttons,
   switch_joypad_axis,
   switch_joypad_poll,
#ifdef HAVE_LIBNX
   switch_joypad_set_rumble,
#else
   NULL, /* set_rumble */
#endif
   NULL,
   switch_joypad_name,
   "switch"
};
