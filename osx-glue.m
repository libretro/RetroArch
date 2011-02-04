#import <Foundation/Foundation.h>
static NSAutoreleasePool * pool;
void init_ns_pool()
{
   pool = [[NSAutoreleasePool alloc] init];
}

void deinit_ns_pool()
{
   [pool release];
}

