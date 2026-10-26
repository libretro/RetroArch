#pragma once

#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MCPInstallTarget
{
   MCP_INSTALL_TARGET_MLC  = 0,
   MCP_INSTALL_TARGET_USB  = 1,
} MCPInstallTarget;

typedef struct __attribute__((__packed__))
{
   uint32_t inProgress;
   uint64_t tid;
   uint64_t sizeTotal;
   uint64_t sizeProgress;
   uint32_t contentsTotal;
   uint32_t contentsProgress;
}MCPInstallProgress;

typedef struct
{
   char buffer[0x27F];
} MCPInstallInfo;

typedef struct
{
   char buffer[0x27F];
}MCPInstallTitleInfo;

typedef struct
{
   char name[0x31B];
}MCPDevice;

typedef struct
{
   MCPDevice devices[32];
}MCPDeviceList;

int MCP_Open();
int MCP_Close(int handle);
int MCP_InstallSetTargetDevice(int handle, MCPInstallTarget device);
int MCP_InstallGetTargetDevice(int handle, MCPInstallTarget *deviceOut);
int MCP_InstallSetTargetUsb(int handle, int usb);
int MCP_InstallGetInfo(int handle, char *path, MCPInstallInfo *out);
int MCP_InstallTitleAsync(int handle, char *path, MCPInstallTitleInfo *out);
int MCP_InstallGetProgress(int handle, MCPInstallProgress *installProgressOut);
int MCP_InstallTitleAbort(int handle);
int MCP_UninstallTitleAsync(int handle, char *path, MCPInstallTitleInfo *out);
int MCP_DeviceList(int handle, int *numDevices, MCPDeviceList *outDevices, uint32_t outBufferSize);
int MCP_FullDeviceList(int handle, int *numDevices, MCPDeviceList *outDevices,
                       uint32_t outBufferSize);

#ifdef __cplusplus
}
#endif
