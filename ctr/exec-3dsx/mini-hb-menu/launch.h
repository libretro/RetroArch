#pragma once

#include "common.h"

extern void (*__system_retAddr)(void);

enum
{
	LOADER_SHOW_REBOOT = 0x01,
	LOADER_NEED_SCAN   = 0x02
};

typedef struct
{
	// Mandatory fields
	const char* name;
	u32 flags;
	bool (* init)(void);
	void (* deinit)(void);
	void (* launchFile)(const char* path, argData_s* args, executableMetadata_s* em);

	// Optional fields
	void (* useTitle)(u64 tid, u8 mediatype);
} loaderFuncs_s;

Handle launchOpenFile(const char* path);