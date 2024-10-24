#include <stdio.h>
#include <string.h>

#include <pspsdk.h>
#include <psploadexec_kernel.h>

PSP_MODULE_INFO("kernel_functions", PSP_MODULE_KERNEL, 0, 0);
PSP_MAIN_THREAD_ATTR(0);

void exitspawn_kernel(const char *fileName, SceSize args, void *argp)
{
   int k1;
   struct SceKernelLoadExecVSHParam game_param;

   memset(&game_param,0,sizeof(game_param));

   game_param.size              = sizeof(game_param);
   game_param.args              = args;
   game_param.argp              = argp;
   game_param.key               = "game";
   game_param.vshmain_args_size = 0;
   game_param.vshmain_args      = NULL;
   game_param.configfile        = 0;
   game_param.unk4              = 0;
   game_param.unk5              = 0x10000;

   k1                           = pspSdkSetK1(0);
   sceKernelLoadExecVSHMs2(fileName, &game_param);
   pspSdkSetK1(k1);
}

int module_start(SceSize args, void *argp)
{
   return 0;
}

int module_stop(void)
{
   return 0;
}
