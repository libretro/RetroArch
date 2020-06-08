#pragma once

#include <retro_common_api.h>

#if defined(__cplusplus) && !defined(CXX_BUILD)
extern "C" {
#endif

int get_process_id(void);
void Discord_Register(const char* applicationId, const char* command);
void Discord_RegisterSteamGame(const char* applicationId, const char* steamId);

#if defined(__cplusplus) && !defined(CXX_BUILD)
}
#endif
