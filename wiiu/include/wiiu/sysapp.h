#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sysapp_input_struct sysapp_input_struct;

void SYSSwitchToSyncControllerOnHBM();
void SYSSwitchToEManual();
void SYSSwitchToEShop();
void _SYSSwitchToMainApp();
void SYSSwitchToBrowserForViewer(sysapp_input_struct *);

void SYSRelaunchTitle(uint32_t argc, char *pa_Argv[]);
void SYSLaunchMenu();
void SYSLaunchTitle(uint64_t TitleId);
void _SYSLaunchMiiStudio();
void _SYSLaunchSettings();
void _SYSLaunchParental();
void _SYSLaunchNotifications();

#ifdef __cplusplus
}
#endif
