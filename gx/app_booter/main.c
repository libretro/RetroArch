/* Copyright 2011 Dimok
   This code is licensed to you under the terms of the GNU GPL, version 2;
   see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt  */
#include <gccore.h>
#include <stdio.h>
#include <stdarg.h>
#include "string.h"

#include "dolloader.h"
#include "elfloader.h"
#include "sync.h"

#define EXECUTABLE_MEM_ADDR 0x91800000
#define SYSTEM_ARGV	((struct __argv *) 0x93200000)

// if we name this main, GCC inserts the __eabi symbol, even when we specify -mno-eabi
// what a lovely "feature"
void app_booter_main(void)
{
	void *exeBuffer = (void *) EXECUTABLE_MEM_ADDR;
	u32 exeEntryPointAddress = 0;
	entrypoint exeEntryPoint;

	if (valid_elf_image(exeBuffer) == 1)
		exeEntryPointAddress = load_elf_image(exeBuffer);
	else
		exeEntryPointAddress = load_dol_image(exeBuffer);

	exeEntryPoint = (entrypoint) exeEntryPointAddress;
	if (!exeEntryPoint)
		return;

	if (SYSTEM_ARGV->argvMagic == ARGV_MAGIC)
	{
		void *new_argv = (void *) (exeEntryPointAddress + 8);
		memcpy(new_argv, SYSTEM_ARGV, sizeof(struct __argv));
		sync_before_exec(new_argv, sizeof(struct __argv));
	}

	exeEntryPoint ();
}
