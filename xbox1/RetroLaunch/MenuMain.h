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
#include "Surface.h"

enum DisplayMode
{
	Box,
	List
};

class CMenuMain
{
public:
CMenuMain();
~CMenuMain();

bool Create();

void Render();

void ProcessInput();

private:
/*
Texture,
_x = xpos,
_y = ypos,
_w = width,
_h = height,
_c = color,
*/

// Background image with coords
CSurface m_menuMainBG;
int m_menuMainBG_x;
int m_menuMainBG_y;
dword m_menuMainBG_w;
dword m_menuMainBG_h;

// Rom selector panel with coords
CSurface m_menuMainRomSelectPanel;
int m_menuMainRomSelectPanel_x;
int m_menuMainRomSelectPanel_y;
dword m_menuMainRomSelectPanel_w;
dword m_menuMainRomSelectPanel_h;

// Title coords with color
int m_menuMainTitle_x;
int m_menuMainTitle_y;
dword m_menuMainTitle_c;

// Rom list coords
int m_menuMainRomListPos_x;
int m_menuMainRomListPos_y;
int m_menuMainRomListSpacing;




/**
* The Rom List menu buttons. The size can be variable so we use a list
*/
//list<MenuButton *> m_romListButtons;//list<Texture *>
//no menu buttons, we will use plain textures
list<CSurface *> m_romListButtons;

/** 
* The current mode the rom list is in
*/
int m_displayMode;

/**
* The current loaded state the rom list is in
*/
bool m_bRomListLoadedState;

int m_romListBeginRender;
int m_romListEndRender;
int m_romListSelectedRom;
int m_romListOffset;

};

extern CMenuMain g_menuMain;
