/* A stand-in nvapi64.dll beside the test on Windows: nvapi_QueryInterface
 * answers NvAPI_Initialize and one made-up id, so gfx/nvidia/
 * nvapi_loader.c's probe and query are pinned under Wine. */
#include <stddef.h>
#include <windows.h>

#define ID_INITIALIZE 0x0150e828
#define ID_SAMPLE     0x12345678

static int __cdecl fake_initialize(void) { return 0; }
static int __cdecl fake_sample(int v) { return v * 2; }

__declspec(dllexport) void *__cdecl nvapi_QueryInterface(unsigned id)
{
   if (id == ID_INITIALIZE)
      return (void*)fake_initialize;
   if (id == ID_SAMPLE)
      return (void*)fake_sample;
   return NULL;
}
