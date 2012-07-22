/**
 * RetroLaunch 2012
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version. This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. To contact the
 * authors: Surreal64 CE Team (http://www.emuxtras.net)
 */

#ifdef _XBOX
#include "Input.h"

Input g_input;

Input::Input(void)
{
	m_pollingParameters.fAutoPoll = TRUE;
	m_pollingParameters.fInterruptOut = TRUE;
	m_pollingParameters.bInputInterval = 8;
	m_pollingParameters.bOutputInterval = 8;
	m_lastTick = GetTickCount();
	m_buttonDelay = XBINPUT_PRESS_BUTTON_DELAY;
	m_triggerDelay = XBINPUT_PRESS_TRIGGER_DELAY;
	m_buttonPressed = false;
}

Input::~Input(void)
{
}

bool Input::Create()
{
	XInitDevices(0, NULL);

	// get a mask of all currently available devices
	dword dwDeviceMask = XGetDevices(XDEVICE_TYPE_GAMEPAD);

	// open the devices
	for(dword i = 0; i < XGetPortCount(); i++)
	{
		ZeroMemory(&m_inputStates[i], sizeof(XINPUT_STATE));
		ZeroMemory(&m_gamepads[i], sizeof(XBGAMEPAD));

		if(dwDeviceMask & (1 << i))
		{
			// get a handle to the device
			m_gamepads[i].hDevice = XInputOpen(XDEVICE_TYPE_GAMEPAD, i,
			                                   XDEVICE_NO_SLOT, &m_pollingParameters);

			// store capabilities of the device
			XInputGetCapabilities(m_gamepads[i].hDevice, &m_gamepads[i].caps);

			// initialize last pressed buttons
			XInputGetState(m_gamepads[i].hDevice, &m_inputStates[i]);

			m_gamepads[i].wLastButtons = m_inputStates[i].Gamepad.wButtons;

			for(dword b = 0; b < 8; b++)
			{
				m_gamepads[i].bLastAnalogButtons[b] =
				        // Turn the 8-bit polled value into a boolean value
				        (m_inputStates[i].Gamepad.bAnalogButtons[b] > XINPUT_GAMEPAD_MAX_CROSSTALK);
			}
		}
	}

	return true;
}

void Input::RefreshDevices()
{
	dword dwInsertions, dwRemovals;

	XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, reinterpret_cast<PDWORD>(&dwInsertions), reinterpret_cast<PDWORD>(&dwRemovals));

	// loop through all gamepads
	for(dword i = 0; i < XGetPortCount(); i++)
	{
		// handle removed devices
		m_gamepads[i].bRemoved = (dwRemovals & (1 << i)) ? true : false;

		if(m_gamepads[i].bRemoved)
		{
			// if the controller was removed after XGetDeviceChanges but before
			// XInputOpen, the device handle will be NULL
			if(m_gamepads[i].hDevice)
				XInputClose(m_gamepads[i].hDevice);

			m_gamepads[i].hDevice = NULL;
			m_gamepads[i].Feedback.Rumble.wLeftMotorSpeed  = 0;
			m_gamepads[i].Feedback.Rumble.wRightMotorSpeed = 0;
		}

		// handle inserted devices
		m_gamepads[i].bInserted = (dwInsertions & (1 << i)) ? true : false;

		if(m_gamepads[i].bInserted)
		{
			m_gamepads[i].hDevice = XInputOpen(XDEVICE_TYPE_GAMEPAD, i,
			                                   XDEVICE_NO_SLOT, &m_pollingParameters );

			// if the controller is removed after XGetDeviceChanges but before
			// XInputOpen, the device handle will be NULL
			if(m_gamepads[i].hDevice)
			{
				XInputGetCapabilities(m_gamepads[i].hDevice, &m_gamepads[i].caps);

				// initialize last pressed buttons
				XInputGetState(m_gamepads[i].hDevice, &m_inputStates[i]);

				m_gamepads[i].wLastButtons = m_inputStates[i].Gamepad.wButtons;

				for(dword b = 0; b < 8; b++)
				{
					m_gamepads[i].bLastAnalogButtons[b] =
					        // Turn the 8-bit polled value into a boolean value
					        (m_inputStates[i].Gamepad.bAnalogButtons[b] > XINPUT_GAMEPAD_MAX_CROSSTALK);
				}
			}
		}
	}
}

void Input::GetInput()
{
	RefreshDevices();

	if (m_buttonPressed)
	{
		m_lastTick = GetTickCount();
		m_buttonPressed = false;
	}

	// loop through all gamepads
	for(dword i = 0; i < XGetPortCount(); i++)
	{
		// if we have a valid device, poll it's state and track button changes
		if(m_gamepads[i].hDevice)
		{
			// read the input state
			XInputGetState(m_gamepads[i].hDevice, &m_inputStates[i]);

			// copy gamepad to local structure
			memcpy(&m_gamepads[i], &m_inputStates[i].Gamepad, sizeof(XINPUT_GAMEPAD));

			// put xbox device input for the gamepad into our custom format
			float fX1 = (m_gamepads[i].sThumbLX + 0.5f) / 32767.5f;
			m_gamepads[i].fX1 = (fX1 >= 0.0f ? 1.0f : -1.0f) *
			                    max(0.0f, (fabsf(fX1) - XBINPUT_DEADZONE) / (1.0f - XBINPUT_DEADZONE));

			float fY1 = (m_gamepads[i].sThumbLY + 0.5f) / 32767.5f;
			m_gamepads[i].fY1 = (fY1 >= 0.0f ? 1.0f : -1.0f) *
			                    max(0.0f, (fabsf(fY1) - XBINPUT_DEADZONE) / (1.0f - XBINPUT_DEADZONE));

			float fX2 = (m_gamepads[i].sThumbRX + 0.5f) / 32767.5f;
			m_gamepads[i].fX2 = (fX2 >= 0.0f ? 1.0f : -1.0f) *
			                    max(0.0f, (fabsf(fX2) - XBINPUT_DEADZONE) / (1.0f - XBINPUT_DEADZONE));

			float fY2 = (m_gamepads[i].sThumbRY + 0.5f) / 32767.5f;
			m_gamepads[i].fY2 = (fY2 >= 0.0f ? 1.0f : -1.0f) *
			                    max(0.0f, (fabsf(fY2) - XBINPUT_DEADZONE) / (1.0f - XBINPUT_DEADZONE));

			// get the boolean buttons that have been pressed since the last
			// call. each button is represented by one bit.
			m_gamepads[i].wPressedButtons = (m_gamepads[i].wLastButtons ^ m_gamepads[i].wButtons) & m_gamepads[i].wButtons;
			m_gamepads[i].wLastButtons    = m_gamepads[i].wButtons;

			// get the analog buttons that have been pressed or released since
			// the last call.
			for(dword b = 0; b < 8; b++)
			{
				// turn the 8-bit polled value into a boolean value
				bool bPressed = (m_gamepads[i].bAnalogButtons[b] > XINPUT_GAMEPAD_MAX_CROSSTALK);

				if(bPressed)
					m_gamepads[i].bPressedAnalogButtons[b] = !m_gamepads[i].bLastAnalogButtons[b];
				else
					m_gamepads[i].bPressedAnalogButtons[b] = false;

				// store the current state for the next time
				m_gamepads[i].bLastAnalogButtons[b] = bPressed;
			}
		}
	}
}

bool Input::IsButtonPressed(XboxButton button)
{
	if (m_lastTick + m_buttonDelay > GetTickCount())
		return false;

	bool buttonDown = false;

	switch (button)
	{
	case XboxLeftThumbLeft:
		buttonDown = (m_gamepads[0].fX1 < -0.5);
		break;
	case XboxLeftThumbRight:
		buttonDown = (m_gamepads[0].fX1 > 0.5);
		break;
	case XboxLeftThumbUp:
		buttonDown = (m_gamepads[0].fY1 > 0.5);
		break;
	case XboxLeftThumbDown:
		buttonDown = (m_gamepads[0].fY1 < -0.5);
		break;
	case XboxRightThumbLeft:
		buttonDown = (m_gamepads[0].fX2 < -0.5);
		break;
	case XboxRightThumbRight:
		buttonDown = (m_gamepads[0].fX2 > 0.5);
		break;
	case XboxRightThumbUp:
		buttonDown = (m_gamepads[0].fY2 > 0.5);
		break;
	case XboxRightThumbDown:
		buttonDown = (m_gamepads[0].fY2 < -0.5);
		break;
	case XboxDPadLeft:
		buttonDown = ((m_gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0);
		break;
	case XboxDPadRight:
		buttonDown = ((m_gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0);
		break;
	case XboxDPadUp:
		buttonDown = ((m_gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0);
		break;
	case XboxDPadDown:
		buttonDown = ((m_gamepads[0].wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0);
		break;
	case XboxStart:
		buttonDown = ((m_gamepads[0].wButtons & XINPUT_GAMEPAD_START) != 0);
		break;
	case XboxBack:
		buttonDown = ((m_gamepads[0].wButtons & XINPUT_GAMEPAD_BACK) != 0);
		break;
	case XboxA:
		buttonDown = (m_gamepads[0].bAnalogButtons[XINPUT_GAMEPAD_A] > 30);
		break;
	case XboxB:
		buttonDown = (m_gamepads[0].bAnalogButtons[XINPUT_GAMEPAD_B] > 30);
		break;
	case XboxX:
		buttonDown = (m_gamepads[0].bAnalogButtons[XINPUT_GAMEPAD_X] > 30);
		break;
	case XboxY:
		buttonDown = (m_gamepads[0].bAnalogButtons[XINPUT_GAMEPAD_Y] > 30);
		break;
	case XboxWhite:
		buttonDown = (m_gamepads[0].bAnalogButtons[XINPUT_GAMEPAD_WHITE] > 30);
		break;
	case XboxBlack:
		buttonDown = (m_gamepads[0].bAnalogButtons[XINPUT_GAMEPAD_BLACK] > 30);
		break;
	case XboxLeftThumbButton:
		buttonDown = ((m_gamepads[0].wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0);
		break;
	case XboxRightThumbButton:
		buttonDown = ((m_gamepads[0].wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0);
		break;
	default:
		return false;
	}

	if (buttonDown)
	{
		m_buttonPressed = true;
		return true;
	}

	return false;
}

byte Input::IsLTriggerPressed()
{
	if (m_lastTick + m_triggerDelay > GetTickCount())
		return 0;

	if (m_gamepads[0].bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] < 30)
		return 0;
	else
	{
		m_buttonPressed = true;
		return m_gamepads[0].bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER];
	}
}

byte Input::IsRTriggerPressed()
{
	if (m_lastTick + m_triggerDelay > GetTickCount())
		return 0;

	if (m_gamepads[0].bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] < 30)
		return 0;
	else
	{
		m_buttonPressed = true;
		return m_gamepads[0].bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER];
	}
}

DWORD Input::GetButtonDelay()
{
	return m_buttonDelay;
}

void Input::SetButtonDelay(DWORD milliseconds)
{
	m_buttonDelay = milliseconds;
}

DWORD Input::GetTriggerDelay()
{
	return m_triggerDelay;
}

void Input::SetTriggerDelay(DWORD milliseconds)
{
	m_triggerDelay = milliseconds;
}
#endif