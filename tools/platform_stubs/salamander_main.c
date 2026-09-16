/* Host main for the salamander link lane: references what a launcher
 * uses from rtime.c and nothing else, so every other symbol rtime.c
 * drags in fails the link the way the real launchers do. */
#include <time.h>
#include <time/rtime.h>
int sceKernelDelayThread(unsigned int d) { (void)d; return 0; }
int main(void)
{
   struct tm out;
   time_t now = 0;
   rtime_init();
   rtime_localtime(&now, &out);
   rtime_deinit();
   return 0;
}
