/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

static uint64_t pad_state[MAX_PADS];
static int16_t analog_state[MAX_PADS][2][2];
#ifdef _XBOX1
static HANDLE gamepads[MAX_PADS];
static DWORD dwDeviceMask;
static bool bInserted[MAX_PADS];
static bool bRemoved[MAX_PADS];
#endif

static const char* const XBOX_CONTROLLER_NAMES[4] =
{
   "XInput Controller (Player 1)",
   "XInput Controller (Player 2)",
   "XInput Controller (Player 3)",
   "XInput Controller (Player 4)"
};

static const char *xdk_joypad_name(unsigned pad)
{
   return g_settings.input.device_names[pad];
}

static bool xdk_joypad_init(void)
{
   unsigned autoconf_pad;

#ifdef _XBOX1
   XInitDevices(0, NULL);

   dwDeviceMask = XGetDevices(XDEVICE_TYPE_GAMEPAD);

   /* Check the device status. */
   switch(XGetDeviceEnumerationStatus())
   {
      case XDEVICE_ENUMERATION_IDLE:
         RARCH_LOG("Input state status: XDEVICE_ENUMERATION_IDLE\n");
         break;
      case XDEVICE_ENUMERATION_BUSY:
         RARCH_LOG("Input state status: XDEVICE_ENUMERATION_BUSY\n");
         break;
   }

   while(XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_BUSY) {}
#endif

   for (autoconf_pad = 0; autoconf_pad < MAX_PLAYERS; autoconf_pad++)
   {
      strlcpy(g_settings.input.device_names[autoconf_pad],
            "XInput Controller",
            sizeof(g_settings.input.device_names[autoconf_pad]));
      
      /* TODO - implement VID/PID? */
      input_config_autoconfigure_joypad(autoconf_pad,
            xdk_joypad_name(autoconf_pad), 
            0, 0,
            xdk_joypad.ident);
   }

   return true;
}

static bool xdk_joypad_button(unsigned port, uint16_t joykey)
{
   xdk_input_t *xdk = (xdk_input_t*)driver.input_data;

   if (port >= MAX_PADS)
      return false;

   return xdk->pad_state[port] & (1ULL << joykey);
}

static int16_t xdk_joypad_axis(unsigned port, uint32_t joyaxis)
{
   int val = 0, axis = -1;
   bool is_neg = false, is_pos = false;
   if (joyaxis == AXIS_NONE || port >= MAX_PADS)
      return 0;

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

   switch (axis)
   {
      case 0:
         val = analog_state[port][0][0];
         break;
      case 1:
         val = analog_state[port][0][1];
         break;
      case 2:
         val = analog_state[port][1][0];
         break;
      case 3:
         val = analog_state[port][1][1];
         break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}


static void xdk_joypad_poll(void)
{
   uint64_t *state_p1, *lifecycle_state;
#if defined(_XBOX1)
   unsigned int dwInsertions, dwRemovals, port;

   XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD,
         reinterpret_cast<PDWORD>(&dwInsertions),
         reinterpret_cast<PDWORD>(&dwRemovals));
#endif

   for (port = 0; port < MAX_PADS; port++)
   {
#ifdef _XBOX1
      XINPUT_CAPABILITIES caps[MAX_PADS];
      (void)caps;

      /* handle removed devices. */
      bRemoved[port] = (dwRemovals & (1 << port)) ? true : false;

      if(bRemoved[port])
      {
         /* if the controller was removed after 
          * XGetDeviceChanges but before
          * XInputOpen, the device handle will be NULL. */
         if(gamepads[port])
            XInputClose(gamepads[port]);

         gamepads[port] = 0;
         pad_state[port] = 0;
      }

      /* handle inserted devices. */
      bInserted[port] = (dwInsertions & (1 << port)) ? true : false;

      if(bInserted[port])
      {
         XINPUT_POLLING_PARAMETERS m_pollingParameters;
         m_pollingParameters.fAutoPoll = FALSE;
         m_pollingParameters.fInterruptOut = TRUE;
         m_pollingParameters.bInputInterval = 8;
         m_pollingParameters.bOutputInterval = 8;
         gamepads[port] = XInputOpen(XDEVICE_TYPE_GAMEPAD, port,
               XDEVICE_NO_SLOT, NULL);
      }

      if (!gamepads[port])
         continue;

      /* if the controller is removed after 
       * XGetDeviceChanges but before XInputOpen,
       * the device handle will be NULL. */
#endif

      XINPUT_STATE state_tmp;

#if defined(_XBOX1)
      if (XInputPoll(gamepads[port]) != ERROR_SUCCESS)
         continue;

      if (XInputGetState(gamepads[port], &state_tmp) != ERROR_SUCCESS)
         continue;
#elif defined(_XBOX360)
      if (XInputGetState(port, &state_tmp) == ERROR_DEVICE_NOT_CONNECTED)
         continue;
#endif

      uint64_t *state_cur = &pad_state[port];

      *state_cur = 0;
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_START) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0);

#if defined(_XBOX1)
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_B]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_A]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_Y]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_X]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_WHITE]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_BLACK]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R2) : 0);
#elif defined(_XBOX360)
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_B) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_A) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_Y) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_X) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0);
      *state_cur |= ((state_tmp.Gamepad.bLeftTrigger > 128) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0);
      *state_cur |= ((state_tmp.Gamepad.bRightTrigger > 128) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R2) : 0);
#endif
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L3) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R3) : 0);

      analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] = state_tmp.Gamepad.sThumbLX;
      analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] = state_tmp.Gamepad.sThumbLY;
      analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = state_tmp.Gamepad.sThumbRX;
      analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = state_tmp.Gamepad.sThumbRY;

      for (int i = 0; i < 2; i++)
         for (int j = 0; j < 2; j++)
            if (analog_state[port][i][j] == -0x8000)
               analog_state[port][i][j] = -0x7fff;
   }

   state_p1 = &pad_state[0];
   lifecycle_state = &g_extern.lifecycle_state;

   *lifecycle_state &= ~((1ULL << RARCH_MENU_TOGGLE));

   if((*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_L3)) && (*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_R3)))
      *lifecycle_state |= (1ULL << RARCH_MENU_TOGGLE);
}

static bool xdk_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS && pad_state[pad];
}

static void xdk_joypad_destroy(void)
{
}

rarch_joypad_driver_t xdk_joypad = {
   xdk_joypad_init,
   xdk_joypad_query_pad,
   xdk_joypad_destroy,
   xdk_joypad_button,
   xdk_joypad_axis,
   xdk_joypad_poll,
   NULL,
   xdk_joypad_name,
   "xdk",
};
