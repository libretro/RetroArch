#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IPC_CUSTOM_START_MCP_THREAD       0xFE
#define IPC_CUSTOM_MEN_RPX_HOOK_COMPLETED 0xFD
#define IPC_CUSTOM_LOAD_CUSTOM_RPX        0xFC
#define IPC_CUSTOM_START_USB_LOGGING      0xFA
#define IPC_CUSTOM_COPY_ENVIRONMENT_PATH  0xF9
#define IPC_CUSTOM_GET_MOCHA_API_VERSION  0xF8

typedef enum LoadRPXTargetEnum {
    LOAD_RPX_TARGET_SD_CARD              = 0,
    LOAD_RPX_TARGET_EXTRA_REVERT_PREPARE = 0x42424242,
} LoadRPXTargetEnum;

typedef struct __attribute((packed)) {
    LoadRPXTargetEnum target; // Target where the file will be loaded from.
    uint32_t filesize;        // Size of RPX inside given file. 0 for full filesize.
    uint32_t fileoffset;      // Offset of RPX inside given file.
    char path[256];           // Relative path on target device
} MochaRPXLoadInfo;

#ifdef __cplusplus
} // extern "C"
#endif