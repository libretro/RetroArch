#pragma once
#include <wut.h>

#ifdef __cplusplus
extern "C" {
#endif

//TODO
typedef void sysapp_input_struct;

void SYSSwitchToSyncControllerOnHBM();
void SYSSwitchToEManual();
void SYSSwitchToEShop();
void _SYSSwitchToMainApp();
void SYSSwitchToBrowserForViewer(sysapp_input_struct *);

#ifdef __cplusplus
}
#endif
