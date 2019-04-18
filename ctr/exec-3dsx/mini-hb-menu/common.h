#pragma once

// C stdlib includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

// 3DS includes
#include <3ds.h>

#define ENTRY_ARGBUFSIZE   0x400
#define NUM_SERVICESTHATMATTER 5

typedef enum
{
	StrId_Loading = 0,
	StrId_Directory,
	StrId_DefaultLongTitle,
	StrId_DefaultPublisher,
	StrId_IOError,
	StrId_CouldNotOpenFile,

	StrId_NoAppsFound_Title,
	StrId_NoAppsFound_Msg,

	StrId_Reboot,
	StrId_ReturnToHome,

	StrId_TitleSelector,
	StrId_ErrorReadingTitleMetadata,
	StrId_NoTitlesFound,
	StrId_SelectTitle,

	StrId_NoTargetTitleSupport,
	StrId_MissingTargetTitle,

	StrId_NetLoader,
	StrId_NetLoaderUnavailable,
	StrId_NetLoaderOffline,
	StrId_NetLoaderError,
	StrId_NetLoaderActive,
	StrId_NetLoaderTransferring,

	StrId_Max,
} StrId;

typedef struct
{
	char* dst;
	u32 buf[ENTRY_ARGBUFSIZE/sizeof(u32)];
} argData_s;

typedef struct
{
	bool scanned;
	u32 sectionSizes[3];
	bool servicesThatMatter[NUM_SERVICESTHATMATTER];
} executableMetadata_s;

typedef struct
{
	u32 num;
	u32 text_end;
	u32 data_address;
	u32 data_size;
	u32 processLinearOffset;
	u32 processHookAddress;
	u32 processAppCodeAddress;
	u32 processHookTidLow, processHookTidHigh;
	u32 mediatype;
	bool capabilities[0x10]; // {socuAccess, csndAccess, qtmAccess, nfcAccess, httpcAccess, reserved...}
} memmap_header_t;

typedef struct
{
	u32 src, dst, size;
} memmap_entry_t;

typedef struct
{
	memmap_header_t header;
	memmap_entry_t map[];
} memmap_t;

#define memmapSize(m) (sizeof(memmap_header_t) + sizeof(memmap_entry_t)*(m)->header.num)

#include "launch.h"
