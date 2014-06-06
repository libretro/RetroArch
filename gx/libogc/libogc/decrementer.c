/*-------------------------------------------------------------

decrementer.c -- PPC decrementer exception support

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.


-------------------------------------------------------------*/


#include <gctypes.h>
#include <stdlib.h>
#include "asm.h"
#include "processor.h"
#include "lwp_threads.h"
#include "lwp_watchdog.h"
#include "context.h"

//#define _DECEX_DEBUG

#ifdef _DECEX_DEBUG
extern int printk(const char *fmt,...);
#endif

void __decrementer_init()
{
#ifdef _DECEX_DEBUG
	printf("__decrementer_init()\n\n");
#endif
}

void c_decrementer_handler(frame_context *ctx)
{
#ifdef _DECEX_DEBUG
	printk("c_decrementer_handler(%d)\n",_wd_ticks_since_boot);
#endif
	__lwp_wd_tickle_ticks();
}
