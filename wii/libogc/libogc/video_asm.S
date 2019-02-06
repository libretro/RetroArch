/*-------------------------------------------------------------

video_asm.S -- VIDEO subsystem assembler support

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

#include <asm.h>

	.globl __VIClearFramebuffer
	//r3 = dst, r4 = length(bytes),r5 = color
__VIClearFramebuffer:
	srwi.	r0,r4,8
	beq		2f
	mtctr	r0
	subi	r3,r3,4
1:	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)

	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	stwu	r5,4(r3)
	bdnz	1b
2:	blr
