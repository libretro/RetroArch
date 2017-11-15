// 10 april 2015
#import "uipriv_darwin.h"

char *uiDarwinNSStringToText(NSString *s)
{
	char *out;

	out = strdup([s UTF8String]);
	if (out == NULL) {
		fprintf(stderr, "memory exhausted in uiDarwinNSStringToText()\n");
		abort();
	}
	return out;
}

void uiFreeText(char *s)
{
	free(s);
}
