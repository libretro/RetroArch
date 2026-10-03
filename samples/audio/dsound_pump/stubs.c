/* The parts of RetroArch audio/drivers/dsound.c reaches for. */
#include <stdarg.h>
#include <string.h>
#include <windows.h>
#include "configuration.h"
#include "audio/audio_driver.h"
#include "audio/audio_upmix.h"

void RARCH_LOG(const char *f, ...)  { (void)f; }
void RARCH_WARN(const char *f, ...) { (void)f; }
void RARCH_ERR(const char *f, ...)  { (void)f; }
void RARCH_DBG(const char *f, ...)  { (void)f; }

static settings_t settings;
settings_t *config_get_ptr(void) { return &settings; }
uint32_t audio_driver_requested_layout(void) { return AUDIO_LAYOUT_STEREO; }
unsigned audio_layout_channels(uint32_t layout) { return 2; }

DWORD IMMNotificationThreadId;
#ifdef HAVE_THREADS
void mmdevice_thread(void *data) { }
#else
DWORD CALLBACK mmdevice_thread(PVOID data) { return 0; }
#endif
