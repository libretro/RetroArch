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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <direct.h>
#include <list>
#include <vector>
#include <algorithm>
#ifdef _XBOX
	#include <xtl.h>
	#include <xgraphics.h>
#else
	#pragma comment(lib,"d3d8.lib")
	#pragma comment(lib,"d3dx8.lib")
	#pragma comment(lib,"DxErr8.lib")
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <d3d8.h>
	#include <d3dx8.h>
	#include <dxerr8.h>
#endif


using namespace std;

#define XBUILD "Launcher CE"

typedef unsigned __int8		byte;
typedef unsigned __int16	word;
typedef unsigned __int32	dword;
typedef unsigned __int64	qword;


