#include <stdio.h>
#include <stdlib.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <pspctrl.h>

PSP_MODULE_INFO("Pthread Test", 0, 1, 1);

extern void pte_test_main();

#ifdef JNS
#define printf pspDebugScreenPrintf
#endif

/* Exit callback */
int exit_callback(int arg1, int arg2, void *common)
{
   sceKernelExitGame();
   return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp)
{
   int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
   sceKernelRegisterExitCallback(cbid);

   sceKernelSleepThreadCB();

   return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void)
{
   int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
   if (thid >= 0)
      sceKernelStartThread(thid, 0, 0);

   return thid;
}

int main(void)
{
   SceCtrlData pad;

   pspDebugScreenInit();
   SetupCallbacks();

   pte_test_main();

   while (1)
   {
      sceCtrlReadBufferPositive(&pad, 1);
      if (pad.Buttons & PSP_CTRL_UP)
      {
         printf("Exiting...\n");
         return 0;
      }

   }
   return 0;
}
