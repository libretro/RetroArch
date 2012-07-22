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

#include "Rom.h"
//#include "BoxArtTable.h"

Rom::Rom()
{
	m_bLoaded = false;
}

Rom::~Rom(void)
{
}

bool Rom::Load(const string &szFilename)
{
	if (m_bLoaded)
		return true;

	m_szFilename = szFilename;

	// get the filename for the box art image
	//FIXME: Add BoxArtTable.cpp/h, open iso file, grab header, extract ID ie. T-6003G, use for boxartfilename
	{
		m_szBoxArtFilename = "D:\\boxart\\default.jpg"; //g_boxArtTable.GetBoxArtFilename(m_dwCrc1);
	}

	m_bLoaded = true;

	return true;
}

bool Rom::LoadFromCache(const string &szFilename, const string &szBoxArtFilename)
{
	m_szFilename = szFilename;
	m_szBoxArtFilename = szBoxArtFilename;

	m_bLoaded = true;

	return true;
}

string Rom::GetFileName()
{
	return m_szFilename;
}

string Rom::GetBoxArtFilename()
{
	return m_szBoxArtFilename;
}

string Rom::GetComments()
{
	//return string(m_iniEntry->szComments);
	return "blah";
}


CSurface &Rom::GetTexture()
{
	return m_texture;
}