/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Alfred Agrell
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include "http_lib.h"

int main(void)
{
	char url[]="http://buildbot.libretro.com/nightly/android/latest/armeabi-v7a/2048_libretro.so.zip";
	char* urlfilename=NULL;
	int len;
	char * out;
	FILE * f;
	http_parse_url(url, &urlfilename);
	http_retcode status=http_get(urlfilename, &out, &len, NULL);
	if (status<0) printf("%i - failure...\n", status);
	else printf("%i - success\n", status);
	f=fopen("2048_libretro.so.zip", "wb");
	fwrite(out, 1,len, f);
	fclose(f);
}
