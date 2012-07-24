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

#include "MenuManager.h"
#include "MenuMain.h"

#include "../../general.h"


CMenuManager g_menuManager;

CMenuManager::CMenuManager()
{
}

CMenuManager::~CMenuManager()
{
}

bool CMenuManager::Create()
{
	//Create the MenuManager, set to Main Menu
	RARCH_LOG("Create MenuManager, set state to MENU_MAIN.\n");
	SetMenuState(MENU_MAIN);

	return true;
}

bool CMenuManager::SetMenuState(int nMenuID)
{
	m_pMenuID = nMenuID;

	switch (m_pMenuID) {
	case MENU_MAIN:
		//Create the Main Menu
		g_menuMain.Create();
		break;
	}
	return true;
}

void CMenuManager::Update()
{
	//Process overall input
	ProcessInput();

	switch (m_pMenuID) {
	case MENU_MAIN:

		// Process menu specific input
		g_menuMain.ProcessInput();

		// Render the Main Menu
		g_menuMain.Render();
		break;
	}

}


void CMenuManager::ProcessInput()
{
}



int CMenuManager::GetMenuState()
{
	return m_pMenuID;
}

