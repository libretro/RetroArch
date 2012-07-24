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

#include "MenuMain.h"
#include "Font.h"
#include "RomList.h"
#include "Input.h"

#include "../../console/retroarch_console.h"
#include "../../general.h"

CMenuMain g_menuMain;

CMenuMain::CMenuMain()
{
	// we think that the rom list is unloaded until we know otherwise
	m_bRomListLoadedState = false;

	ifstream stateFile;
	stateFile.open("T:\\RomlistState.dat");

	if (stateFile.is_open())
	{
		int baseIndex;
		int romListMode;

		stateFile >> baseIndex;
		stateFile >> romListMode;
		stateFile >> m_displayMode;

		g_romList.SetRomListMode(romListMode);
		g_romList.m_iBaseIndex = baseIndex;

		stateFile.close();
	}
	else
	{
		m_displayMode = List;
	}
}

CMenuMain::~CMenuMain()
{
	ofstream stateFile;
	stateFile.open("T:\\RomlistState.dat");

	stateFile << g_romList.GetBaseIndex() << endl;
	stateFile << g_romList.GetRomListMode() << endl;
	stateFile << m_displayMode << endl;

	stateFile.close();
}

bool CMenuMain::Create()
{
	RARCH_LOG("CMenuMain::Create().");

	// Title coords with color
	m_menuMainTitle_x = 305;
	m_menuMainTitle_y = 30;
	m_menuMainTitle_c = 0xFFFFFFFF;

	// Load background image
	m_menuMainBG.Create("Media\\menuMainBG.png");
	m_menuMainBG_x = 0;
	m_menuMainBG_y = 0;
	m_menuMainBG_w = 640;
	m_menuMainBG_h = 480;

	// Init rom list coords
	m_menuMainRomListPos_x = 100;
	m_menuMainRomListPos_y = 100;
	m_menuMainRomListSpacing = 20;

	// Load rom selector panel
	m_menuMainRomSelectPanel.Create("Media\\menuMainRomSelectPanel.png");
	m_menuMainRomSelectPanel_x = m_menuMainRomListPos_x - 5;
	m_menuMainRomSelectPanel_y = m_menuMainRomListPos_y - 2;
	m_menuMainRomSelectPanel_w = 440;
	m_menuMainRomSelectPanel_h = 20;

	m_romListSelectedRom = 0;

	//The first element in the romlist to render
	m_romListBeginRender = 0;

	//The last element in the romlist to render
	m_romListEndRender = 18;

	//The offset in the romlist
	m_romListOffset = 0;

	if(m_romListEndRender > g_romList.GetRomListSize() - 1)
	{
		m_romListEndRender = g_romList.GetRomListSize() - 1;
	}

	//Generate the rom list textures only once
	vector<Rom *>::iterator i;
	dword y = 0;
	for (i = g_romList.m_romList.begin(); i != g_romList.m_romList.end(); i++)
	{
		Rom *rom = *i;
		g_font.RenderToTexture(rom->GetTexture(), rom->GetFileName(), 18, XFONT_BOLD, 0xff808080, -1, false);
	}

	return true;
}


void CMenuMain::Render()
{
	//CheckRomListState();

	//Render background image
	m_menuMainBG.Render(m_menuMainBG_x, m_menuMainBG_y);

	//Display some text
	g_font.Render("Press RSTICK THUMB to exit. Press START and/or A to launch a rom.", 65, 430, 16, XFONT_NORMAL, m_menuMainTitle_c);

	//Begin with the rom selector panel
	//FIXME: Width/Height needs to be current Rom texture width/height (or should we just leave it at a fixed size?)
	m_menuMainRomSelectPanel.Render(m_menuMainRomSelectPanel_x, m_menuMainRomSelectPanel_y, m_menuMainRomSelectPanel_w, m_menuMainRomSelectPanel_h);

	dword dwSpacing = 0;

	for (int i = m_romListBeginRender; i <= m_romListEndRender; i++)
	{
		g_romList.GetRomAt(i + m_romListOffset)->GetTexture().Render(m_menuMainRomListPos_x, m_menuMainRomListPos_y + dwSpacing);
		dwSpacing += m_menuMainRomListSpacing;
	}
}

static uint16_t old_input_state = 0;

void CMenuMain::ProcessInput()
{
   uint16_t input_state = 0;
   input_xinput.poll(NULL);

   for (unsigned i = 0; i < RARCH_FIRST_META_KEY; i++)
   {
      input_state |= input_xinput.input_state(NULL, NULL, false,
         RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
   }

   uint16_t trigger_state = input_state & ~old_input_state;

	if(trigger_state & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
	{
		if(m_romListSelectedRom < g_romList.GetRomListSize())
		{

			if(m_menuMainRomSelectPanel_y < (m_menuMainRomListPos_y + (m_menuMainRomListSpacing * m_romListEndRender)))
			{
				m_menuMainRomSelectPanel_y += m_menuMainRomListSpacing;
				m_romListSelectedRom++;
				RARCH_LOG("SELECTED ROM: %d.\n", m_romListSelectedRom);
			}

			if(m_menuMainRomSelectPanel_y > (m_menuMainRomListPos_y + (m_menuMainRomListSpacing * (m_romListEndRender))))
			{
				m_menuMainRomSelectPanel_y -= m_menuMainRomListSpacing;
				m_romListSelectedRom++;
				if(m_romListSelectedRom > g_romList.GetRomListSize() - 1)
				{
					m_romListSelectedRom = g_romList.GetRomListSize() - 1;
				}


				RARCH_LOG("SELECTED ROM AFTER CORRECTION: %d.\n", m_romListSelectedRom);

				if(m_romListSelectedRom < g_romList.GetRomListSize() - 1 && m_romListOffset < g_romList.GetRomListSize() - 1 - m_romListEndRender - 1) {
					m_romListOffset++;
					RARCH_LOG("OFFSET: %d.\n", m_romListOffset);
				}
			}
		}
	}

	if(trigger_state & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
	{
		if(m_romListSelectedRom > -1)
		{
			if(m_menuMainRomSelectPanel_y > (m_menuMainRomListPos_y - m_menuMainRomListSpacing))
			{
				m_menuMainRomSelectPanel_y -= m_menuMainRomListSpacing;
				m_romListSelectedRom--;
				RARCH_LOG("SELECTED ROM: %d.\n", m_romListSelectedRom);
			}

			if(m_menuMainRomSelectPanel_y < (m_menuMainRomListPos_y - m_menuMainRomListSpacing))
			{
				m_menuMainRomSelectPanel_y += m_menuMainRomListSpacing;
				m_romListSelectedRom--;
				if(m_romListSelectedRom < 0)
				{
					m_romListSelectedRom = 0;
				}


				RARCH_LOG("SELECTED ROM AFTER CORRECTION: %d.\n", m_romListSelectedRom);

				if(m_romListSelectedRom > 0 && m_romListOffset > 0) {
					m_romListOffset--;
					RARCH_LOG("OFFSET: %d.\n", m_romListOffset);
				}
			}
		}
	}

	// Press A to launch, selected rom filename is saved into T:\\tmp.retro
	if(trigger_state & (1 << RETRO_DEVICE_ID_JOYPAD_B) || trigger_state & (1 << RETRO_DEVICE_ID_JOYPAD_START))
	{
      rarch_console_load_game(g_romList.GetRomAt(m_romListSelectedRom)->GetFileName().c_str());
	}

	if (trigger_state & (1 << RETRO_DEVICE_ID_JOYPAD_R3))
	{
		LD_LAUNCH_DASHBOARD LaunchData = { XLD_LAUNCH_DASHBOARD_MAIN_MENU };
		XLaunchNewImage( NULL, (LAUNCH_DATA*)&LaunchData );
	}
}

