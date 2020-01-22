#pragma once

#define DISCORD_EXPORT

#ifdef __cplusplus
extern "C" {
#endif

DISCORD_EXPORT void Discord_Register(const char* applicationId, const char* command);
DISCORD_EXPORT void Discord_RegisterSteamGame(const char* applicationId, const char* steamId);

#ifdef __cplusplus
}
#endif
