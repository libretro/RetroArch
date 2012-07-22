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

#include "Global.h"
#include "Debug.h"

CDebug g_debug;

CDebug::CDebug(void)
{
	iLine = 1;
	m_bFileOpen = false;
}

CDebug::~CDebug(void)
{
}

void CDebug::Print(char *szMessage, ...)
{
	char	szMsg[512];
	va_list vaArgList;
   string szDebugFile ("D:\\debug.log");//(FixPath(PathRoot() + "debug.log"));
	
	va_start(vaArgList, szMessage);
	vsprintf(szMsg, szMessage, vaArgList);
	va_end(vaArgList);

	OutputDebugStringA(IntToString(iLine).c_str());
	OutputDebugStringA(": ");
	OutputDebugStringA(szMsg);
	OutputDebugStringA("\n\n");

#ifdef WIN32
	cout << IntToString(iLine)<< ": " << szMsg << endl << endl;
#endif

//#ifdef _LOG
	//Open the log file, create one if it does not exist, append the data
	if(!m_bFileOpen)
	{
		fp = fopen(szDebugFile.c_str(), "w+");
		m_bFileOpen = true;
	}
		else
	{
		fp = fopen(szDebugFile.c_str(), "a+");
	}
	
	//print the data to the log file
	fprintf(fp, IntToString(iLine).c_str());
	fprintf(fp, ": ");
	fprintf(fp, szMsg);
	fprintf(fp, "\r\n\r\n");
	
	//close the file
	fclose(fp);
//#endif //_LOG

	iLine ++;
}


string CDebug::IntToString(int value)
{
	stringstream ss;

	ss << value;

	return ss.str();
}
