#include <vitasdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, const char *argv[])
{
   FILE *f, *f2;
   char core[256], rom[256];

   memset(core, 0, 256);
   memset(rom, 0, 256);

   f  = fopen("app0:core.txt", "rb");
   f2 = fopen("app0:rom.txt",  "rb");

   if (f && f2)
   {
      char uri[512];
      fread(core, 1, 256, f);
      fread(rom, 1, 256, f2);
      fclose(f);
      fclose(f2);
      snprintf(uri, sizeof(uri),
            "psgm:play?titleid=%s&param=%s&param2=%s",
            "RETROVITA", core, rom);
      sceAppMgrLaunchAppByUri(0xFFFFF, uri);
      sceKernelDelayThread(1000);
   }

   sceKernelExitProcess(0);

   return 0;
}
