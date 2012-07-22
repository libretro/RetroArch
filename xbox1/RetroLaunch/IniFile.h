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

struct IniFileEntry
{
	//debug output
	bool bShowDebugInfo;

	//automatic frame skip method
	bool bAutomaticFrameSkip;

	//manual frame skip method (throttlespeed)
	bool bManualFrameSkip;

	//number of frames to skip
	dword dwNumFrameSkips;

	//sync audio to video
	bool bSyncAudioToVideo;

	//vertical synchronization
	bool bVSync;

	//flicker filter
	dword dwFlickerFilter;

	//soft display filter
	bool bSoftDisplayFilter;

	//texture filter
	DWORD dwTextureFilter;

	//screen xpos
	dword dwXPOS;
	//screen ypos
	dword dwYPOS;
	//screen xscale
	dword dwXWIDTH;
	//screen yscale
	dword dwYHEIGHT;

	//hide normal scroll 0
	bool bHideNBG0;
	//hide normal scroll 1
	bool bHideNBG1;
	//hide normal scroll 2
	bool bHideNBG2;
	//hide normal scroll 3
	bool bHideNBG3;
	//hide rotation scroll 0
	bool bHideRBG0;
	//hide VDP1
	bool bHideVDP1;
};


class IniFile
{
public:
	IniFile();
	~IniFile();

	bool Save(const string &szIniFileName);
	bool SaveTempRomFileName(const string &szFileName);
	bool Load(const string &szIniFileName);
	bool CreateAndSaveDefaultIniEntry();
	bool CheckForIniEntry();

	IniFileEntry m_currentIniEntry;

private:
	IniFileEntry m_defaultIniEntry;

	string szRomFileName;
	
};

extern IniFile g_iniFile;





