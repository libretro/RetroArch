#include "../common.h"

typedef struct
{
	s32 processId;
	bool capabilities[0x10];
} processEntry_s;

typedef void (*callBootloader_2x_fn)(Handle file, u32* argbuf, u32 arglength);
typedef void (*callBootloaderNewProcess_2x_fn)(s32 processId, u32* argbuf, u32 arglength);
typedef void (*callBootloaderRunTitle_2x_fn)(u8 mediatype, u32* argbuf, u32 argbuflength, u32 tid_low, u32 tid_high);
typedef void (*callBootloaderRunTitleCustom_2x_fn)(u8 mediatype, u32* argbuf, u32 argbuflength, u32 tid_low, u32 tid_high, memmap_t* mmap);
typedef void (*getBestProcess_2x_fn)(u32 sectionSizes[3], bool* requirements, int num_requirements, processEntry_s* out, int out_size, int* out_len);

#define callBootloader_2x ((callBootloader_2x_fn)0x00100000)
#define callBootloaderNewProcess_2x ((callBootloaderNewProcess_2x_fn)0x00100008)
#define callBootloaderRunTitle_2x ((callBootloaderRunTitle_2x_fn)0x00100010)
#define callBootloaderRunTitleCustom_2x ((callBootloaderRunTitleCustom_2x_fn)0x00100014)
#define getBestProcess_2x ((getBestProcess_2x_fn)0x0010000C)

static s32 targetProcess = -1;
static u64 targetTid;
static u8 targetMediatype;
static Handle fileHandle;
static u32 argBuf[ENTRY_ARGBUFSIZE/sizeof(u32)];
static u32 argBufLen;
static u32 memMapBuf[0x40];
static bool useMemMap;

static bool init(void)
{
	return R_SUCCEEDED(amInit());
}

static void deinit(void)
{
	amExit();
}

static void bootloaderJump(void)
{
	if (targetProcess == -1)
		callBootloader_2x(fileHandle, argBuf, argBufLen);
	else if (targetProcess == -2)
	{
		if (useMemMap)
			callBootloaderRunTitleCustom_2x(targetMediatype, argBuf, argBufLen, (u32)targetTid, (u32)(targetTid>>32), (memmap_t*)memMapBuf);
		else
			callBootloaderRunTitle_2x(targetMediatype, argBuf, argBufLen, (u32)targetTid, (u32)(targetTid>>32));
	}
	else
		callBootloaderNewProcess_2x(targetProcess, argBuf, argBufLen);
}

static void launchFile(const char* path, argData_s* args, executableMetadata_s* em)
{
	fileHandle = launchOpenFile(path);
	if (fileHandle==0)
		return;
	argBufLen = args->dst - (char*)args->buf;
	memcpy(argBuf, args->buf, argBufLen);
	__system_retAddr = bootloaderJump;
}

const loaderFuncs_s loader_Ninjhax2 =
{
	.name = "hax 2.x",
	.flags = LOADER_SHOW_REBOOT | LOADER_NEED_SCAN,
	.init = init,
	.deinit = deinit,
	.launchFile = launchFile,
	//.useTitle = useTitle,
};
