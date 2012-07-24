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
#include "Rom.h"

class RomList
{
public:
	RomList(void);
	virtual ~RomList(void);

	void Load();
	void Save();
	void Refresh();

	bool IsLoaded();

	void AddRomToList(Rom *rom, int mode);
	void RemoveRomFromList(Rom *rom, int mode);

	int GetBaseIndex();
	void SetBaseIndex(int index);

	int GetRomListSize();

	Rom *GetRomAt(int index);
	int FindRom(Rom *rom, int mode);

	void CleanUpTextures();
	void DestroyAllTextures();

	int m_iBaseIndex;

	vector<Rom *> m_romList;

private:
	void Build();
	void Destroy();

private:
	bool m_bLoaded;
	string m_szRomPath;
};

extern RomList g_romList;