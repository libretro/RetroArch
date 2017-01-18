#pragma once
#include <wut.h>

/**
 * \defgroup sysapp_launch SYSAPP Launch
 * \ingroup sysapp
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

void
SYSRelaunchTitle(uint32_t argc, 
                 char *pa_Argv[]);

void
SYSLaunchMenu();

void
SYSLaunchTitle(uint64_t TitleId);

void
_SYSLaunchMiiStudio();

void
_SYSLaunchSettings();

void
_SYSLaunchParental();

void
_SYSLaunchNotifications();

#ifdef __cplusplus
}
#endif

/** @} */
