#include "../common.h"

static Handle hbldrHandle;

static bool init(void)
{
	return R_SUCCEEDED(svcConnectToPort(&hbldrHandle, "hb:ldr"));
}

static Result HBLDR_SetTarget(const char* path)
{
	u32 pathLen = strlen(path) + 1;
	u32* cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(2, 0, 2); //0x20002
	cmdbuf[1] = IPC_Desc_StaticBuffer(pathLen, 0);
	cmdbuf[2] = (u32)path;

	Result rc = svcSendSyncRequest(hbldrHandle);
	if (R_SUCCEEDED(rc)) rc = cmdbuf[1];
	return rc;
}

static Result HBLDR_SetArgv(const void* buffer, u32 size)
{
	u32* cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(3, 0, 2); //0x30002
	cmdbuf[1] = IPC_Desc_StaticBuffer(size, 1);
	cmdbuf[2] = (u32)buffer;

	Result rc = svcSendSyncRequest(hbldrHandle);
	if (R_SUCCEEDED(rc)) rc = cmdbuf[1];
	return rc;
}

static void deinit(void)
{
	svcCloseHandle(hbldrHandle);
}

static void launchFile(const char* path, argData_s* args, executableMetadata_s* em)
{
	if (strncmp(path, "sdmc:/",6) == 0)
		path += 5;
	HBLDR_SetTarget(path);
	HBLDR_SetArgv(args->buf, sizeof(args->buf));
}

const loaderFuncs_s loader_Rosalina =
{
	.name = "Rosalina",
	.init = init,
	.deinit = deinit,
	.launchFile = launchFile,
};
