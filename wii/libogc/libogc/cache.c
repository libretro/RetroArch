/*-------------------------------------------------------------

cache.c -- Cache interface

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
#include <processor.h>
#include "cache.h"

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

extern void __LCEnable();
extern void L2GlobalInvalidate();
extern void L2Enable();

void LCEnable()
{
	u32 level;

	_CPU_ISR_Disable(level);
	__LCEnable();
	_CPU_ISR_Restore(level);
}

u32 LCLoadData(void *dstAddr,void *srcAddr,u32 nCount)
{
	u32 cnt,blocks;

	if((s32)nCount<=0) return 0;

	cnt = (nCount+31)>>5;
	blocks = (cnt+127)>>7;
	while(cnt) {
		if(cnt<0x80) {
			LCLoadBlocks(dstAddr,srcAddr,cnt);
			cnt = 0;
			break;
		}
		LCLoadBlocks(dstAddr,srcAddr,0);
		cnt -= 128;
		dstAddr += 4096;
		srcAddr += 4096;
	}
	return blocks;
}

u32 LCStoreData(void *dstAddr,void *srcAddr,u32 nCount)
{
	u32 cnt,blocks;

	if((s32)nCount<=0) return 0;

	cnt = (nCount+31)>>5;
	blocks = (cnt+127)>>7;
	while(cnt) {
		if(cnt<0x80) {
			LCStoreBlocks(dstAddr,srcAddr,cnt);
			cnt = 0;
			break;
		}
		LCStoreBlocks(dstAddr,srcAddr,0);
		cnt -= 128;
		dstAddr += 4096;
		srcAddr += 4096;
	}
	return blocks;
}

u32 LCQueueLength()
{
	u32 hid2 = mfspr(920);
	return _SHIFTR(hid2,4,4);
}

u32 LCQueueWait(u32 len)
{
	len++;
	while(_SHIFTR(mfspr(920),4,4)>=len);
	return len;
}

void LCFlushQueue()
{
	mtspr(922,0);
	mtspr(923,1);
	ppcsync();
}

void LCAlloc(void *addr,u32 bytes)
{
	u32 level;
	u32 cnt = bytes>>5;
	u32 hid2 = mfspr(920);
	if(!(hid2&0x10000000)) {
		_CPU_ISR_Disable(level);
		__LCEnable();
		_CPU_ISR_Restore(level);
	}
	LCAllocTags(TRUE,addr,cnt);
}

void LCAllocNoInvalidate(void *addr,u32 bytes)
{
	u32 level;
	u32 cnt = bytes>>5;
	u32 hid2 = mfspr(920);
	if(!(hid2&0x10000000)) {
		_CPU_ISR_Disable(level);
		__LCEnable();
		_CPU_ISR_Restore(level);
	}
	LCAllocTags(FALSE,addr,cnt);
}
#ifdef HW_RVL
void L2Enhance()
{
	u32 level, hid4;
	u32 *stub = (u32*)0x80001800;
	_CPU_ISR_Disable(level);
	hid4 = mfspr(HID4);
	// make sure H4A is set before doing anything
	if (hid4 & 0x80000000) {
		// There's no easy way to flush only L2, so just flush everything
		// L2GlobalInvalidate will take care of syncing
		DCFlushRangeNoSync((void*)0x80000000, 0x01800000);
		DCFlushRangeNoSync((void*)0x90000000, 0x04000000);

		// Invalidate L2 (this will disable it first)
		L2GlobalInvalidate();
		// set bits: L2FM=01, BCO=1, L2MUM=1
		hid4 |= 0x24200000;
		mtspr(HID4, hid4);
		// Re-enable L2
		L2Enable();

		// look for HBC stub (STUBHAXX)
		if (stub[1]==0x53545542 && stub[2]==0x48415858) {
			// look for a HID4 write
			for (stub += 3; (u32)stub < 0x80003000; stub++) {
				if ((stub[0] & 0xFC1FFFFF)==0x7C13FBA6) {
					write32((u32)stub, 0x60000000);
					break;
				}
			}
		}
	}
	_CPU_ISR_Restore(level);
}
#endif
