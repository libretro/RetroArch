/**
 * Surreal 64 Launcher (C) 2003
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
 * authors: email: buttza@hotmail.com, lantus@lantus-x.com
 */

#pragma once

#include "Global.h"
#include "Surface.h"

class Rom
{
public:
	Rom();
	~Rom();

	bool Load(const char *szFilename);
	bool LoadFromCache(const string &szFilename);

	string GetFileName();
	string GetComments();
	CSurface &GetTexture();
private:
	string m_szFilename;
	bool m_bLoaded;
	CSurface m_texture;
};
