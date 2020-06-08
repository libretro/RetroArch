#include <vitasdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, const char *argv[]) {
	char core[256], rom[256];
	memset(core, 0, 256);
	memset(rom, 0, 256);
	FILE *f = fopen("app0:core.txt", "rb");
	FILE *f2 = fopen("app0:rom.txt", "rb");
	if (f && f2) {
		fread(core, 1, 256, f);
		fread(rom, 1, 256, f2);
		fclose(f);
		fclose(f2);
		char uri[512];
		sprintf(uri, "psgm:play?titleid=%s&param=%s&param2=%s", "RETROVITA", core, rom);

		sceAppMgrLaunchAppByUri(0xFFFFF, uri);
	}
	
	sceKernelExitProcess(0);
  
	return 0;
}
