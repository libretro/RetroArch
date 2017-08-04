/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2017 Ash Logan (QuarkTheAwesome)
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

//TODO: Program exceptions don't seem to work. Good thing they almost never happen.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiu/os.h>
#include "wiiu_dbg.h"
#include "exception_handler.h"

/*	Settings */
#define NUM_STACK_TRACE_LINES 5

/*	Externals
	From the linker scripts.
*/
extern unsigned int __code_start;
#define TEXT_START (unsigned int)&__code_start
extern unsigned int __code_end;
#define TEXT_END (unsigned int)&__code_end

void test_os_exceptions(void);
void exception_print_symbol(unsigned int addr);

typedef struct _framerec
{
   struct _framerec* up;
   void* lr;
} frame_rec, *frame_rec_t;

/*	Fill in a few gaps in thread.h
	Dimok calls these exception_specific0 and 1;
	though we may as well name them by their function.
*/
#define dsisr __unknown[0]
#define dar __unknown[1]

/*	Some bitmasks for determining DSI causes.
	Taken from the PowerPC Programming Environments Manual (32-bit).
*/
//Set if the EA is unmapped.
#define DSISR_TRANSLATION_MISS 0x40000000
//Set if the memory accessed is protected.
#define DSISR_TRANSLATION_PROT 0x8000000
//Set if certain instructions are used on uncached memory (see manual)
#define DSISR_BAD_CACHING 0x4000000
//Set if the offending operation is a write, clear for a read.
#define DSISR_WRITE_ATTEMPTED 0x2000000
//Set if the memory accessed is a DABR match
#define DSISR_DABR_MATCH 0x400000
/*	ISI cause bitmasks, same source */
#define SRR1_ISI_TRANSLATION_MISS 0x40000000
#define SRR1_ISI_TRANSLATION_PROT 0x8000000
/*	PROG cause bitmasks, guess where from */
//Set on floating-point exceptions
#define SRR1_PROG_IEEE_FLOAT 0x100000
//Set on an malformed instruction (can't decode)
#define SRR1_PROG_BAD_INSTR 0x80000
//Set on a privileged instruction executing in userspace
#define SRR1_PROG_PRIV_INSTR 0x40000
//Set on a trap instruction
#define SRR1_PROG_TRAP 0x20000
//Clear if srr0 points to the address that caused the exception (yes, really)
#define SRR1_PROG_SRR0_INACCURATE 0x10000

#define buf_add(...) pos += sprintf(exception_msgbuf + pos, __VA_ARGS__)
size_t pos = 0;
char* exception_msgbuf;

void __attribute__((__noreturn__)) exception_cb(OSContext* ctx, OSExceptionType type) {
	if (!exception_msgbuf || !OSEffectiveToPhysical(exception_msgbuf)) {
	/*	No message buffer available, fall back onto MEM1 */
		exception_msgbuf = (char*)0xF4000000;
	}

/*	First up, the pretty header that tells you wtf just happened */
	if (type == OS_EXCEPTION_TYPE_DSI) {
	/*	Exception type and offending instruction location
		Also initializes exception_msgbuf, use buf_add from now on */
		buf_add("DSI: Instr at %08X", ctx->srr0);
	/*	Was this a read or a write? */
		if (ctx->dsisr & DSISR_WRITE_ATTEMPTED) {
			buf_add(" bad write to");
		} else {
			buf_add(" bad read from");
		}
	/*	So why was it bad?
		Other causes (DABR) don't have a message to go with them. */
		if (ctx->dsisr & DSISR_TRANSLATION_MISS) {
			buf_add(" unmapped memory at");
		} else if (ctx->dsisr & DSISR_TRANSLATION_PROT) {
			buf_add(" protected memory at");
		} else if (ctx->dsisr & DSISR_BAD_CACHING) {
			buf_add(" uncached memory at");
		}
		buf_add(" %08X\n", ctx->dar);
	} else if (type == OS_EXCEPTION_TYPE_ISI) {
		buf_add("ISI: Bad execute of");
		if (ctx->srr1 & SRR1_ISI_TRANSLATION_PROT) {
			buf_add(" protected memory at");
		} else if (ctx->srr1 & SRR1_ISI_TRANSLATION_MISS) {
			buf_add(" unmapped memory at");
		}
		buf_add(" %08X\n", ctx->srr0);
	} else if (type == OS_EXCEPTION_TYPE_PROGRAM) {
		buf_add("PROG:");
		if (ctx->srr1 & SRR1_PROG_BAD_INSTR) {
			buf_add(" Malformed instruction at");
		} else if (ctx->srr1 & SRR1_PROG_PRIV_INSTR) {
			buf_add(" Privileged instruction in userspace at");
		} else if (ctx->srr1 & SRR1_PROG_IEEE_FLOAT) {
			buf_add(" Floating-point exception at");
		} else if (ctx->srr1 & SRR1_PROG_TRAP) {
			buf_add(" Trap conditions met at");
		} else {
			buf_add(" Out-of-spec error (!) at");
		}
		if (ctx->srr1 & SRR1_PROG_SRR0_INACCURATE) {
			buf_add("%08X-ish\n", ctx->srr0);
		} else {
			buf_add("%08X\n", ctx->srr0);
		}
	}

/*	Add register dump
	There's space for two more regs at the end of the last line...
	Any ideas for what to put there? */
	buf_add( \
		"r0  %08X r1  %08X r2  %08X r3  %08X r4  %08X\n" \
		"r5  %08X r6  %08X r7  %08X r8  %08X r9  %08X\n" \
		"r10 %08X r11 %08X r12 %08X r13 %08X r14 %08X\n" \
		"r15 %08X r16 %08X r17 %08X r18 %08X r19 %08X\n" \
		"r20 %08X r21 %08X r22 %08X r23 %08X r24 %08X\n" \
		"r25 %08X r26 %08X r27 %08X r28 %08X r29 %08X\n" \
		"r30 %08X r31 %08X lr  %08X sr1 %08X dsi %08X\n" \
		"ctr %08X cr  %08X xer %08X\n",\
		ctx->gpr[0],  ctx->gpr[1],  ctx->gpr[2],  ctx->gpr[3],  ctx->gpr[4],  \
		ctx->gpr[5],  ctx->gpr[6],  ctx->gpr[7],  ctx->gpr[8],  ctx->gpr[9],  \
		ctx->gpr[10], ctx->gpr[11], ctx->gpr[12], ctx->gpr[13], ctx->gpr[14], \
		ctx->gpr[15], ctx->gpr[16], ctx->gpr[17], ctx->gpr[18], ctx->gpr[19], \
		ctx->gpr[20], ctx->gpr[21], ctx->gpr[22], ctx->gpr[23], ctx->gpr[24], \
		ctx->gpr[25], ctx->gpr[26], ctx->gpr[27], ctx->gpr[28], ctx->gpr[29], \
		ctx->gpr[30], ctx->gpr[31], ctx->lr,      ctx->srr1,    ctx->dsisr,   \
		ctx->ctr,     ctx->cr,      ctx->xer                                  \
	);

/*	Stack trace!
	First, let's print the PC... */
	exception_print_symbol(ctx->srr0);

	if (ctx->gpr[1]) {
	/*	Then the addresses off the stack.
		Code borrowed from Dimok's exception handler. */
		frame_rec_t p = (frame_rec_t)ctx->gpr[1];
		if ((unsigned int)p->lr != ctx->lr) {
			exception_print_symbol(ctx->lr);
		}
		for (int i = 0; i < NUM_STACK_TRACE_LINES && p->up; p = p->up, i++) {
			exception_print_symbol((unsigned int)p->lr);
		}
	} else {
		buf_add("Stack pointer invalid. Could not trace further.\n");
	}

	OSFatal(exception_msgbuf);
	for (;;) {}
}

BOOL __attribute__((__noreturn__)) exception_dsi_cb(OSContext* ctx) {
	exception_cb(ctx, OS_EXCEPTION_TYPE_DSI);
}
BOOL __attribute__((__noreturn__)) exception_isi_cb(OSContext* ctx) {
	exception_cb(ctx, OS_EXCEPTION_TYPE_ISI);
}
BOOL __attribute__((__noreturn__)) exception_prog_cb(OSContext* ctx) {
	exception_cb(ctx, OS_EXCEPTION_TYPE_PROGRAM);
}

void exception_print_symbol(unsigned int addr) {
/*	Check if addr is within this RPX's .text */
	if (addr >= TEXT_START && addr < TEXT_END) {
		char symbolName[64];
		OSGetSymbolName(addr, symbolName, 63);

		buf_add("%08X(%08X):%s\n", addr, addr - TEXT_START, symbolName);
	}
/*	Check if addr is within the system library area... */
	else if ((addr >= 0x01000000 && addr < 0x01800000) ||
/*	Or the rest of the app executable area.
	I would have used whatever method JGeckoU uses to determine
	the real lowest address, but *someone* didn't make it open-source :/ */
	(addr >= 0x01800000 && addr < 0x1000000)) {
		char symbolName[64];
		OSGetSymbolName(addr, symbolName, 63);
	/*	Extract RPL name and try and find its base address */
		char* seperator = strchr(symbolName, '|');
		if (seperator) {
		/*	Isolate library name; should end with .rpl
			(our main RPX was caught by another test case above) */
			*seperator = '\0';
		/*	Try for a base address */
			void* libAddr;
			OSDynLoad_Acquire(symbolName, &libAddr);
			*seperator = '|';
		/*	We got one! */
			if (libAddr) {
				buf_add("%08X(%08X):%s\n", addr, addr - (unsigned int)libAddr, symbolName);
				OSDynLoad_Release(libAddr);
				return;
			}
		}
	/*	Ah well. We can still print the basics. */
		buf_add("%08X(        ):%s\n", addr, symbolName);
	}
/*	Check if addr is in the HBL range
	TODO there's no real reason we couldn't find the symbol here,
	it's just laziness and arguably uneccesary bloat */
	else if (addr >= 0x00800000 && addr < 0x01000000) {
		buf_add("%08X(%08X):<unknown:HBL>\n", addr, addr - 0x00800000);
	}
/*	If all else fails, just say "unknown" */
	else {
		buf_add("%08X(        ):<unknown>\n", addr);
	}
}

/*	void setup_os_exceptions(void)
	Install and initialize the exception handler.
*/
void setup_os_exceptions(void) {
	exception_msgbuf = malloc(4096);
	OSSetExceptionCallback(OS_EXCEPTION_TYPE_DSI, exception_dsi_cb);
	OSSetExceptionCallback(OS_EXCEPTION_TYPE_ISI, exception_isi_cb);
	OSSetExceptionCallback(OS_EXCEPTION_TYPE_PROGRAM, exception_prog_cb);
	test_os_exceptions();
}

/*	void test_os_exceptions(void)
	Used for debugging. Insert code here to induce a crash.
*/
void test_os_exceptions(void) {
	//Write to 0x00000000; causes DSI
	/*__asm__ volatile (
		"li %r3, 0 \n" \
		"stw %r3, 0(%r3) \n"
	);
	DCFlushRange((void*)0, 4);*/
	//Malformed instruction, causes PROG. Doesn't seem to work.
	/*__asm__ volatile (
		".int 0xDEADC0DE"
	);*/
	//Jump to 0; causes ISI
	/*void (*testFunc)() = (void(*)())0;
	testFunc();*/
}
