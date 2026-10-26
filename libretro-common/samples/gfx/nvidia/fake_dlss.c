/* A stand-in DLSS feature library (nvngx_dlss.dll on Windows,
 * libnvidia-ngx-dlss.so.310.2.1 on Linux): exports the version query
 * gfx/nvidia/ngx_loader.c reads before NGX ever loads the file. */
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
   return (310u << 16) | (2u << 8) | 1u;
}
