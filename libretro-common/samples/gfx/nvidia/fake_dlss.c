/* A stand-in DLSS feature library (nvngx_dlss.dll on Windows,
 * libnvidia-ngx-dlss.so.* on Linux). Its file version (fake_dlss.rc;
 * on Linux the file name's) says 310.2.1; the snippet word says
 * 310.2.2, packed as the real libraries pack it, so a test sees which
 * was read. */
#include <stdint.h>

#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#define CONV __cdecl
#else
#define EXPORT __attribute__((visibility("default")))
#define CONV
#endif

EXPORT uint32_t CONV NVSDK_NGX_GetSnippetVersion(void)
{
   return (310u << 16) | (2u << 8) | 2u;
}
