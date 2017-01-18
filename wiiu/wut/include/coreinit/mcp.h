#pragma once
#include <wut.h>

/**
 * \defgroup coreinit_mcp MCP IOS Calls
 * \ingroup coreinit
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MCPInstallProgress MCPInstallProgress;
typedef struct MCPInstallInfo MCPInstallInfo;
typedef struct MCPInstallTitleInfo MCPInstallTitleInfo;
typedef struct MCPDevice MCPDevice;
typedef struct MCPDeviceList MCPDeviceList;

typedef enum MCPInstallTarget
{
   MCP_INSTALL_TARGET_MLC  = 0,
   MCP_INSTALL_TARGET_USB  = 1,
} MCPInstallTarget;

struct __attribute__((__packed__)) MCPInstallProgress
{
   uint32_t inProgress;
   uint64_t tid;
   uint64_t sizeTotal;
   uint64_t sizeProgress;
   uint32_t contentsTotal;
   uint32_t contentsProgress;
};
CHECK_OFFSET(MCPInstallProgress, 0x00, inProgress);
CHECK_OFFSET(MCPInstallProgress, 0x04, tid);
CHECK_OFFSET(MCPInstallProgress, 0x0C, sizeTotal);
CHECK_OFFSET(MCPInstallProgress, 0x14, sizeProgress);
CHECK_OFFSET(MCPInstallProgress, 0x1C, contentsTotal);
CHECK_OFFSET(MCPInstallProgress, 0x20, contentsProgress);
CHECK_SIZE(MCPInstallProgress, 0x24);

struct MCPInstallInfo
{
   UNKNOWN(0x27F);
};
CHECK_SIZE(MCPInstallInfo, 0x27F);

struct MCPInstallTitleInfo
{
   UNKNOWN(0x27F);
};
CHECK_SIZE(MCPInstallTitleInfo, 0x27F);

struct MCPDevice
{
   char name[0x31B];
};
CHECK_SIZE(MCPDevice, 0x31B);

struct MCPDeviceList
{
   MCPDevice devices[32];
};
CHECK_SIZE(MCPDeviceList, 0x31B*32);

int
MCP_Open();

int
MCP_Close(int handle);

int
MCP_InstallSetTargetDevice(int handle,
                           MCPInstallTarget device);

int
MCP_InstallGetTargetDevice(int handle,
                           MCPInstallTarget *deviceOut);

int
MCP_InstallSetTargetUsb(int handle,
                        int usb);

int
MCP_InstallGetInfo(int handle,
                   char *path,
                   MCPInstallInfo *out);

int
MCP_InstallTitleAsync(int handle,
                      char *path,
                      MCPInstallTitleInfo *out);

int
MCP_InstallGetProgress(int handle,
                       MCPInstallProgress *installProgressOut);

int
MCP_InstallTitleAbort(int handle);

int
MCP_UninstallTitleAsync(int handle,
                        char *path,
                        MCPInstallTitleInfo *out);

int
MCP_DeviceList(int handle,
               int *numDevices,
               MCPDeviceList *outDevices,
               uint32_t outBufferSize);

int
MCP_FullDeviceList(int handle,
                   int *numDevices,
                   MCPDeviceList *outDevices,
                   uint32_t outBufferSize);

#ifdef __cplusplus
}
#endif

/** @} */
