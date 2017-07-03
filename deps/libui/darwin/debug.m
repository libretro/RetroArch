// 13 may 2016
#import "uipriv_darwin.h"

// LONGTERM don't halt on release builds

void realbug(const char *file, const char *line, const char *func, const char *prefix, const char *format, va_list ap)
{
	NSMutableString *str;
	NSString *formatted;

	str = [NSMutableString new];
	[str appendString:[NSString stringWithFormat:@"[libui] %s:%s:%s() %s", file, line, func, prefix]];
	formatted = [[NSString alloc] initWithFormat:[NSString stringWithUTF8String:format] arguments:ap];
	[str appendString:formatted];
	[formatted release];
	NSLog(@"%@", str);
	[str release];
	__builtin_trap();
}
