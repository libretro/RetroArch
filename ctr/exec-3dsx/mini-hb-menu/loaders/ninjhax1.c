#include "../common.h"

static void (*callBootloader_1x)(Handle hb, Handle file);
static void (*setArgs_1x)(u32* src, u32 length);
static Handle fileHandle;

static bool init(void)
{
	Result res = hbInit();
	if (R_FAILED(res))
		return false;

	HB_GetBootloaderAddresses((void**)&callBootloader_1x, (void**)&setArgs_1x);
	return true;
}

static void deinit(void)
{
	hbExit();
}

static void bootloaderJump(void)
{
	callBootloader_1x(0x00000000, fileHandle);
}

static void launchFile(const char* path, argData_s* args, executableMetadata_s* em)
{
	fileHandle = launchOpenFile(path);
	if (fileHandle==0)
		return;
	setArgs_1x(args->buf, sizeof(args->buf));
	__system_retAddr = bootloaderJump;
}

const loaderFuncs_s loader_Ninjhax1 =
{
	.name = "ninjhax 1.x",
	.flags = LOADER_SHOW_REBOOT,
	.init = init,
	.deinit = deinit,
	.launchFile = launchFile,
};
