#include <stdio.h>
#include "http_lib.h"

int main()
{
	char url[]="http://unicodesnowmanforyou.com/";
	char* urlfilename=NULL;
	http_parse_url(url, &urlfilename);
	char * out;
	http_retcode status=http_get(urlfilename, &out, NULL, NULL);
	if (status<0) printf("%i - failure...\n", status);
	else printf("%i - success - %s\n", status, out);
}
