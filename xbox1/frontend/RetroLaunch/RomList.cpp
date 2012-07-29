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

#include "RomList.h"

RomList g_romList;

bool RLessThan(Rom *elem1, Rom *elem2)
{
	return (elem1->GetFileName() < elem2->GetFileName());
}

RomList::RomList(void)
{
	m_iBaseIndex = 0;
	m_bLoaded = false;
	m_szRomPath = "D:\\";
}

RomList::~RomList(void)
{
	Destroy();
}

void RomList::Refresh()
{
	Destroy();
}

bool RomList::IsLoaded()
{
	return m_bLoaded;
}

void RomList::AddRomToList(Rom *rom, int mode)
{
	vector<Rom *> *pList;

   pList = &m_romList;

	// look to see if the rom is already in the list, we dont want duplicates
	for (int i = 0; i < static_cast<int>(pList->size()); i++)
	{
		if (rom == (*pList)[i])
			return;
	}

	pList->push_back(rom);
	sort(pList->begin(), pList->end(), RLessThan);
}

void RomList::RemoveRomFromList(Rom *rom, int mode)
{
	vector<Rom *> *pList;
   
   pList = &m_romList;

	vector<Rom *>::iterator i;

	// look to see if the rom is already in the list, we dont want duplicates
	for (i = pList->begin(); i != pList->end(); i++)
	{
		if (rom == *i)
		{
			pList->erase(i);
			return;
		}
	}
}

int RomList::GetBaseIndex()
{
	if (m_iBaseIndex > GetRomListSize() - 1)
		m_iBaseIndex = GetRomListSize() - 1;
	if (m_iBaseIndex < 0)
		m_iBaseIndex = 0;

	return m_iBaseIndex;
}

void RomList::SetBaseIndex(int index)
{
	if (index > GetRomListSize() - 1)
		index = GetRomListSize() - 1;
	if (index < 0)
		index = 0;

	m_iBaseIndex = index;
}

int RomList::GetRomListSize()
{
   return m_romList.size();
}

Rom *RomList::GetRomAt(int index)
{
   return m_romList[index];
}

void RomList::Build()
{
	WIN32_FIND_DATA fd;

	HANDLE hFF = FindFirstFile((m_szRomPath + "*.*").c_str(), &fd);

	do
	{
		char ext[_MAX_EXT];

		// get the filename extension
		_splitpath((m_szRomPath + fd.cFileName).c_str(),
		           NULL, NULL, NULL, ext);

		if (
		                stricmp(ext, ".bin") == 0
		        ||      stricmp(ext, ".cue") == 0
		        ||      stricmp(ext, ".iso") == 0
		        ||      stricmp(ext, ".mdf") == 0
              ||      stricmp(ext, ".gba") == 0
		        )
		{
			Rom *rom = new Rom();
			bool bSuccess = rom->Load((m_szRomPath + fd.cFileName).c_str());

			if (bSuccess)
				m_romList.push_back(rom);
			else
				delete rom;
		}
	} while (FindNextFile(hFF, &fd));

	sort(m_romList.begin(), m_romList.end(), RLessThan);

	m_bLoaded = true;
}

void RomList::Destroy()
{
	m_bLoaded = false;
	m_iBaseIndex = 0;

	vector<Rom *>::iterator i;

	for (i = m_romList.begin(); i != m_romList.end(); i++)
		delete *i;

	m_romList.clear();
}
