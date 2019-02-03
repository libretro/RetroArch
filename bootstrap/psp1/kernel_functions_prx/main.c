#include <stdio.h>
#include <string.h>

#include <pspdebug.h>
#include <pspsdk.h>
#include <pspctrl.h>
#include <psploadexec_kernel.h>
#include <pspthreadman_kernel.h>

PSP_MODULE_INFO("kernel_functions", PSP_MODULE_KERNEL, 0, 0);
PSP_MAIN_THREAD_ATTR(0);

static volatile int thread_active;
static unsigned int buttons;
static SceUID main_thread_id;

static int main_thread(SceSize args, void *argp)
{
   SceCtrlData paddata;

   thread_active = 1;

   while (thread_active)
   {
      sceCtrlPeekBufferPositive(&paddata, 1);
      buttons = paddata.Buttons;
      sceKernelDelayThread(1000000/60);
   }

   sceKernelExitThread(0);

   return 0;
}

unsigned int read_system_buttons(void)
{
   return buttons;
}

void exitspawn_kernel(const char *fileName, SceSize args, void *argp)
{
   struct SceKernelLoadExecVSHParam game_param;

   thread_active = 0;

   memset(&game_param,0,sizeof(game_param));

   game_param.size = sizeof(game_param);
   game_param.args = args;
   game_param.argp = argp;
   game_param.key  = "game";
   game_param.vshmain_args_size = 0;
   game_param.vshmain_args = NULL;
   game_param.configfile = 0;
   game_param.unk4 = 0;
   game_param.unk5 = 0x10000;

   int k1 = pspSdkSetK1(0);
   //sceKernelSuspendAllUserThreads();
   int ret = sceKernelLoadExecVSHMs2(fileName, &game_param);
   pspSdkSetK1(k1);
}

int module_start(SceSize args, void *argp)
{
   (void)args;
   (void)argp;

   buttons = 0;
   thread_active = 0;
   main_thread_id = sceKernelCreateThread("main Thread", main_thread, 0x11, 0x200, 0, NULL);

   if (main_thread >= 0)
      sceKernelStartThread(main_thread_id, 0, 0);

   return 0;
}

int module_stop(void)
{
   if (main_thread_id >= 0)
   {
      thread_active = 0;
      sceKernelWaitThreadEnd(main_thread_id, NULL);
   }
   return 0;
}
