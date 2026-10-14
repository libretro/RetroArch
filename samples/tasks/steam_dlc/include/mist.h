/* Stand-in for mist's mist.h on a host without it: what
 * tasks/task_steam.c and steam/steam.h use, with the definitions mist
 * gives them.  The test supplies the call. */
#ifndef STEAM_DLC_TEST_MIST_H
#define STEAM_DLC_TEST_MIST_H

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t MistResult;
typedef uint32_t AppId;

#define MistResult_Success 0
#define MIST_ERROR(result) (result >> 16)
#define MIST_RESULT_CODE(result) (result & 0xFFFF)
#define MIST_IS_SUCCESS(result) (MIST_RESULT_CODE(result) == MistResult_Success)
#define MIST_IS_ERROR(result) (!MIST_IS_SUCCESS(result))

MistResult mist_steam_apps_get_dlc_download_progress(AppId app_id,
      bool *downloading, uint64_t *bytes_downloaded, uint64_t *bytes_total);

#endif
