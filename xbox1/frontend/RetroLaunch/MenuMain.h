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
#include "Surface.h"

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
// Rom selector panel with coords
CSurface m_menuMainRomSelectPanel;

// Rom list coords
int m_menuMainRomListPos_x;
int m_menuMainRomListPos_y;

// Backbuffer width, height
int width; 
int height;
wchar_t m_title[128];
};

extern CMenuMain g_menuMain;
