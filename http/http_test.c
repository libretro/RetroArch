#include <stdio.h>
#include "http_parser.h"

int main(void)
{
   char *w;
	struct http* http1, *http2, *http3;
	size_t q, pos = 0; size_t tot = 0;

	http1 = http_new("http://buildbot.libretro.com/nightly/win-x86/latest/mednafen_psx_libretro.dll.zip");

	while (!http_update(http1, &pos, &tot))
		printf("%.9lu / %.9lu        \r",pos,tot);
	
	http3 = http_new("http://www.wikipedia.org/");
	while (!http_update(http3, NULL, NULL)) {}
	
	w     = (char*)http_data(http3, &q, false);

	printf("%.*s\n", (int)256, w);

#if 0
	struct http* http1=http_new("http://floating.muncher.se:22/");
	struct http* http2=http_new("http://floating.muncher.se/sepulcher/");
	struct http* http3=http_new("http://www.wikipedia.org/");
	while (!http_update(http3, NULL, NULL)) {}
	while (!http_update(http2, NULL, NULL)) {}
	while (!http_update(http1, NULL, NULL)) {}
	printf("%i %i %i %p %s %s\n",
	http_status(http1),http_status(http2),http_status(http3),
	(char*)http_data(http1, NULL, false),http_data(http2, NULL, true),http_data(http3, NULL, true));
#endif
	http_delete(http1);
	http_delete(http3);
}
