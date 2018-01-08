/*-------------------------------------------------------------

Copyright (C) 2008
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
#include "gctypes.h"
#include <string.h>

extern u8 __Arena1Lo[];
extern char *__argvArena1Lo;
void build_argv (struct __argv* argstruct );

void __CheckARGV() {

	if ( __system_argv->argvMagic != ARGV_MAGIC ) {
		__system_argv->argc = 0;
		__system_argv->argv = NULL;
		return;
	}

	u8 *dest = (u8 *)( ((int)__Arena1Lo + 3) & ~3);

	memmove(dest, __system_argv->commandLine, __system_argv->length);
	__system_argv->commandLine = (char *)dest;
	build_argv(__system_argv);

	__argvArena1Lo = (char *)__system_argv->endARGV;

}
