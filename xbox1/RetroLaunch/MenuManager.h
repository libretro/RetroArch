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

#include "Global.h"

enum eMenuState
{
	MENU_MAIN = 0,
	MENU_SETTINGS_SELECT,
	MENU_SETTINGS_EMU,
	MENU_SETTINGS_AUDIO,
	MENU_SETTINGS_SKIN,
	MENU_LAUNCHER
};


class CMenuManager
{
public:
CMenuManager();
~CMenuManager();

bool Create();
bool SetMenuState(int nMenuID);
int GetMenuState();
bool Destroy();
void Update();
void ProcessInput();


private:
int m_pMenuID;

};

extern CMenuManager g_menuManager;