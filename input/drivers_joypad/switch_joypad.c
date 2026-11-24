#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../config.def.h"

#include <switch.h>

#include "../../configuration.h"

#include "../../tasks/tasks_internal.h"
#include "../include/input_pad_view.h"

#include "../../retroarch.h"
#include "../../command.h"
/* TODO/FIXME - weird header include */
#include "string.h"

/* TODO/FIXME - static globals */
/* each pad as its poll left it, lent to the readers (input_pad_view.h):
 * libnx's sixteen buttons, axes 0-3 the sticks' x and y */
static input_pad_view_t switch_pads[DEFAULT_MAX_PADS];
static PadState pad_states[DEFAULT_MAX_PADS];
static HidVibrationDeviceHandle vibration_handles[DEFAULT_MAX_PADS][2];
static HidVibrationDeviceHandle vibration_handleheld[2];
static HidVibrationValue vibration_values[DEFAULT_MAX_PADS][2];
static HidVibrationValue vibration_stop;
/* What each pad's motors were last sent, on which handles, and what
 * the controller answered: a send is an IPC round trip, and the
 * frontend writes a motor every frame a core sets it, mostly to what
 * it already holds. */
static HidVibrationValue vibration_sent[DEFAULT_MAX_PADS][2];
static const HidVibrationDeviceHandle *vibration_sent_to[DEFAULT_MAX_PADS];
static bool vibration_sent_ok[DEFAULT_MAX_PADS];
static int previous_handheld                         = -1;
/* 1 = handheld, 0 = docked, -1 = first use */
static unsigned previous_split_joycon_setting[MAX_USERS] = { 0 };

static const char *switch_joypad_name(unsigned pad)
{
   return "Switch Controller";
}

static void switch_joypad_autodetect_add(unsigned autoconf_pad)
{
   input_autoconfigure_connect(
            switch_joypad_name(autoconf_pad), /* name */
            NULL, NULL,                       /* display names */
            switch_joypad.ident,              /* driver */
            autoconf_pad,                     /* idx */
            0,                                /* vid */
            0);                               /* pid */
}

static void *switch_joypad_init(void *data)
{
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
      if (i == 0)
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
   memset(vibration_sent_to, 0, sizeof(vibration_sent_to));

   return (void*)-1;
}

static int32_t switch_joypad_button(unsigned port_num, uint16_t joykey)
{
   if (port_num >= DEFAULT_MAX_PADS)
      return 0;
   return input_pad_view_button(&switch_pads[port_num], joykey);
}

static void switch_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
   if (port_num < DEFAULT_MAX_PADS)
      *state = switch_pads[port_num].buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t switch_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return input_pad_view_axis(&switch_pads[port], joyaxis);
}

static int16_t switch_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys,
      const uint32_t *joyaxes,
      unsigned port)
{
   uint16_t port_idx = joypad_info->joy_idx;
   if (port_idx >= DEFAULT_MAX_PADS)
      return 0;
   return input_pad_view_state(&switch_pads[port_idx], joypad_info,
         joykeys, joyaxes);
}

/* Read in the poll alone: the same until the next */
static const input_pad_view_t *switch_joypad_view(unsigned port)
{
   return (port < DEFAULT_MAX_PADS) ? &switch_pads[port] : NULL;
}

static bool switch_joypad_query_pad(unsigned pad)
{
   return pad < DEFAULT_MAX_PADS && padIsConnected(&pad_states[pad]);
}

static void switch_joypad_destroy(void)
{
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
   memset(vibration_sent_to, 0, sizeof(vibration_sent_to));
}

static void switch_joypad_poll(void)
{
   int i, handheld;

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
               input_config_get_split_joycon(i);

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
         previous_split_joycon_setting[i] = input_config_get_split_joycon(i);
   }

   if (!handheld && previous_handheld)
   {
      /* switching out of handheld, so make sure
       * joycons are correctly split. */
      for (i = 0; i < MAX_USERS; i += 2)
      {
         unsigned input_split_joycon = input_config_get_split_joycon(i);

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
         unsigned input_split_joycon = input_config_get_split_joycon(i);
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
      previous_split_joycon_setting[i] = input_config_get_split_joycon(i);

   previous_handheld = handheld;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      HidAnalogStickState stick_left_state  = padGetStickPos(&pad_states[i], 0);
      HidAnalogStickState stick_right_state = padGetStickPos(&pad_states[i], 1);
      unsigned input_split_joycon = input_config_get_split_joycon(i);
      int pad_button              = padGetButtons(&pad_states[i]);
      uint16_t bits               = 0;
      input_pad_view_t *v         = &switch_pads[i];
      if (input_split_joycon && !handheld)
      {
         if (hidGetNpadDeviceType((HidNpadIdType)i) & HidDeviceTypeBits_JoyLeft)
         {
            if (pad_button & HidNpadButton_Left)
               bits |= (uint16_t)HidNpadButton_B;
            if (pad_button & HidNpadButton_Up)
               bits |= (uint16_t)HidNpadButton_Y;
            if (pad_button & HidNpadButton_Right)
               bits |= (uint16_t)HidNpadButton_X;
            if (pad_button & HidNpadButton_Down)
               bits |= (uint16_t)HidNpadButton_A;
            if (pad_button & HidNpadButton_LeftSL)
               bits |= (uint16_t)HidNpadButton_L;
            if (pad_button & HidNpadButton_LeftSR)
               bits |= (uint16_t)HidNpadButton_R;
            if (pad_button & HidNpadButton_StickL)
               bits |= (uint16_t)HidNpadButton_StickL;
            if (pad_button & HidNpadButton_Minus)
               bits |= (uint16_t)HidNpadButton_Plus;

            v->axes[0] = -stick_left_state.y;
            v->axes[1] = -stick_left_state.x;
            /* A lone Joy-Con has one stick. */
            v->axes[2] = 0;
            v->axes[3] = 0;
         }
         else if (hidGetNpadDeviceType((HidNpadIdType)i) & HidDeviceTypeBits_JoyRight)
         {
            if (pad_button & HidNpadButton_A)
               bits |= (uint16_t)HidNpadButton_B;
            if (pad_button & HidNpadButton_B)
               bits |= (uint16_t)HidNpadButton_Y;
            if (pad_button & HidNpadButton_X)
               bits |= (uint16_t)HidNpadButton_A;
            if (pad_button & HidNpadButton_Y)
               bits |= (uint16_t)HidNpadButton_X;
            if (pad_button & HidNpadButton_RightSL)
               bits |= (uint16_t)HidNpadButton_L;
            if (pad_button & HidNpadButton_RightSR)
               bits |= (uint16_t)HidNpadButton_R;
            if (pad_button & HidNpadButton_StickR)
               bits |= (uint16_t)HidNpadButton_StickL;
            if (pad_button & HidNpadButton_Plus)
               bits |= (uint16_t)HidNpadButton_Plus;

            /* Throw JoyRight state into retro left analog */
            v->axes[0] = stick_right_state.y;
            v->axes[1] = stick_right_state.x;
            /* A lone Joy-Con has one stick. */
            v->axes[2] = 0;
            v->axes[3] = 0;
         }
         else /* Handle other types via Default Input Handling */
            goto lblDefaultInputHandling;
      }
      else
      {
         /* Default Input Handling */
lblDefaultInputHandling:
         bits = (uint16_t)pad_button;
         v->axes[0] = stick_left_state.x;
         v->axes[1] = -stick_left_state.y;

         v->axes[2] = stick_right_state.x;
         v->axes[3] = -stick_right_state.y;
      }
      INPUT_PAD_VIEW_SET_BITS64(v, bits);
   }
}

bool switch_joypad_set_rumble(unsigned pad,
      enum retro_rumble_effect type, uint16_t strength)
{
   HidVibrationDeviceHandle* handle;
   float amp;

   if (pad >= DEFAULT_MAX_PADS)
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
   if (     vibration_sent_to[pad] == handle
         && !memcmp(vibration_sent[pad], vibration_values[pad],
               sizeof(vibration_sent[pad])))
      return vibration_sent_ok[pad];
   vibration_sent_ok[pad] = R_SUCCEEDED(
         hidSendVibrationValues(handle, vibration_values[pad], 2));
   vibration_sent_to[pad] = handle;
   memcpy(vibration_sent[pad], vibration_values[pad],
         sizeof(vibration_sent[pad]));
   return vibration_sent_ok[pad];
}

input_device_driver_t switch_joypad = {
   switch_joypad_init,
   switch_joypad_query_pad,
   switch_joypad_destroy,
   switch_joypad_button,
   switch_joypad_state,
   switch_joypad_get_buttons,
   switch_joypad_axis,
   switch_joypad_poll,
   switch_joypad_set_rumble,
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   switch_joypad_name,
   "switch",
   NULL, /* get_bt_host */
   NULL, /* set_bt_host */
   NULL, /* survives_video */
   NULL, /* set_player */
   switch_joypad_view
};
