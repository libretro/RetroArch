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

#pragma once
#ifdef _XBOX
#include "Global.h"

#define XBINPUT_DEADZONE 0.24f
#define XBINPUT_PRESS_BUTTON_DELAY 200
#define XBINPUT_PRESS_TRIGGER_DELAY 100

struct XBGAMEPAD : public XINPUT_GAMEPAD
{
    // thumb stick values converted to range [-1,+1]
    float fX1;
    float fY1;
    float fX2;
    float fY2;
    
    // state of buttons tracked since last poll
    word wLastButtons;
    bool bLastAnalogButtons[8];
    word wPressedButtons;
    bool bPressedAnalogButtons[8];

    // rumble properties
    XINPUT_RUMBLE Rumble;
    XINPUT_FEEDBACK Feedback;

    // device properties
    XINPUT_CAPABILITIES caps;
    HANDLE hDevice;

    // flags for whether game pad was just inserted or removed
    bool bInserted;
    bool bRemoved;
};

#define XBOX_BUTTON_COUNT 23

enum XboxButton
{
	XboxLeftThumbLeft,
	XboxLeftThumbRight,		
	XboxLeftThumbUp,		
	XboxLeftThumbDown,		

	XboxRightThumbLeft,		
	XboxRightThumbRight,		
	XboxRightThumbUp,		
	XboxRightThumbDown,		

	XboxDPadLeft,			
	XboxDPadRight,			
	XboxDPadUp,				
	XboxDPadDown,			

	XboxStart,				
	XboxBack,				

	XboxLeftThumbButton,		
	XboxRightThumbButton,	

	XboxA,					
	XboxB,					
	XboxX,					
	XboxY,					

	XboxBlack,				
	XboxWhite,				

	XboxLeftTrigger,			
	XboxRightTrigger,		
};

enum N64Button
{
	N64ThumbLeft,			
	N64ThumbRight,			
	N64ThumbUp,				
	N64ThumbDown,			

	N64DPadLeft,				
	N64DPadRight,			
	N64DPadUp,				
	N64DPadDown,				

	N64CButtonLeft,			
	N64CButtonRight,		
	N64CButtonUp,			
	N64CButtonDown,			

	N64Start,				

	N64A,					
	N64B,					

	N64ZTrigger,				
	N64LeftTrigger,		
	N64RightTrigger			
};


enum SATURNButton
{
	SATURNDPadLeft,
	SATURNDPadRight,
	SATURNDPadUp,
	SATURNDPadDown,

	SATURNA, 
	SATURNB,
	SATURNX,
	SATURNY,
	SATURNC,
	SATURNZ,

	SATURNStart,

	SATURNRightTrigger, 
	SATURNLeftTrigger
};

class Input
{
public:
	Input(void);
	~Input(void);

	bool Create();
	void GetInput();

	bool IsButtonPressed(XboxButton button);
	
	byte IsLTriggerPressed();
	byte IsRTriggerPressed();

	DWORD GetButtonDelay();
	void SetButtonDelay(DWORD milliseconds);

	DWORD GetTriggerDelay();
	void SetTriggerDelay(DWORD milliseconds);

private:
	void RefreshDevices();

private:
	XINPUT_POLLING_PARAMETERS m_pollingParameters;
	XINPUT_STATE m_inputStates[4];
	XBGAMEPAD m_gamepads[4];

	bool m_buttonPressed;
	DWORD m_buttonDelay;
	DWORD m_triggerDelay;
	DWORD m_lastTick;
};

extern Input g_input;
#endif