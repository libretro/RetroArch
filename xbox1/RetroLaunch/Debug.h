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

#ifndef _DEBUG_H__DASH_
#define _DEBUG_H__DASH_

#if _MSC_VER > 1000
#pragma once
#endif //_MSC_VER > 1000

class CDebug
{
public:
	CDebug();
	~CDebug();

	void Print(char *szMessage, ...);
   string IntToString(int value);

private:
	int iLine;
	FILE *fp;
	bool m_bFileOpen;

};

extern CDebug g_debug;

#endif //_DEBUG_H__DASH_