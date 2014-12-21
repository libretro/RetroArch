#include <stdio.h>
#include "http_lib.h"

int main()
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
