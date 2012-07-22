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

#include "SimpleIni.h"
#include "IniFile.h"


IniFile g_iniFile;

//FIXME: SCREEN XPOS, YPOS, XSCALE, YSCALE should be floats!
//FIXME: Path for WIN32

IniFile::IniFile(void)
{
}

IniFile::~IniFile(void)
{
}

bool IniFile::Save(const string &szIniFileName)
{
	CSimpleIniA ini;
	SI_Error rc;
	ini.SetUnicode(true);
	ini.SetMultiKey(true);
	ini.SetMultiLine(true);

	//GENERAL SETTINGS
	ini.SetBoolValue("GENERAL SETTINGS", "SHOW DEBUG INFO", m_currentIniEntry.bShowDebugInfo);

	//VIDEO SETTINGS
	ini.SetBoolValue("VIDEO SETTINGS", "AUTOMATIC FRAME SKIP", m_currentIniEntry.bAutomaticFrameSkip);

	rc = ini.SaveFile(szIniFileName.c_str());

	OutputDebugStringA(szIniFileName.c_str());

	if (rc < 0)
	{
		OutputDebugStringA(" failed to save!\n");
		return false;
	}

	OutputDebugStringA(" saved successfully!\n");
	return true;
}

bool IniFile::Load(const string &szIniFileName)
{
	CSimpleIniA ini;
	SI_Error rc;
	ini.SetUnicode(true);
	ini.SetMultiKey(true);
	ini.SetMultiLine(true);

	rc = ini.LoadFile(szIniFileName.c_str());

	if (rc < 0)
	{
		OutputDebugString("Failed to load ");
		OutputDebugString(szIniFileName.c_str());
		OutputDebugString("\n");
		return false;
	}

	OutputDebugStringA("Successfully loaded ");
	OutputDebugString(szIniFileName.c_str());
	OutputDebugString("\n");

	//GENERAL SETTINGS
	m_currentIniEntry.bShowDebugInfo = ini.GetBoolValue("GENERAL SETTINGS", "SHOW DEBUG INFO", NULL );

	//VIDEO SETTINGS
	m_currentIniEntry.bAutomaticFrameSkip = ini.GetBoolValue("VIDEO SETTINGS", "AUTOMATIC FRAME SKIP", NULL );

	return true;
}

bool IniFile::CreateAndSaveDefaultIniEntry()
{
	//GENERAL SETTINGS
	m_defaultIniEntry.bShowDebugInfo = false;

	//VIDEO SETTINGS
	m_defaultIniEntry.bAutomaticFrameSkip = true;

	// our current ini is now the default ini
	m_currentIniEntry = m_defaultIniEntry;

	// save the default ini
	// FIXME! -> CD/DVD -> utility drive X:
	Save("D:\\retrolaunch.ini");

	return true;
}


bool IniFile::CheckForIniEntry()
{
	// try to load our ini file
	if(!Load("D:\\retrolaunch.ini"))
	{
		// create a new one, if it doesn't exist
		CreateAndSaveDefaultIniEntry();
	}

	return true;
}


bool IniFile::SaveTempRomFileName(const string &szFileName)
{
	CSimpleIniA ini;
	SI_Error rc;
	ini.SetUnicode(true);
	ini.SetMultiKey(true);
	ini.SetMultiLine(true);

	ini.SetValue("LAUNCHER", "ROM", szFileName.c_str());

	DeleteFile("T:\\tmp.retro");
	rc = ini.SaveFile("T:\\tmp.retro");

	OutputDebugStringA("T:\\tmp.retro");

	if (rc < 0)
	{
		OutputDebugStringA(" failed to save!\n");
		return false;
	}

	OutputDebugStringA(" saved successfully!\n");
	return true;

}




