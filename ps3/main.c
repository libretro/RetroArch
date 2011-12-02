#include <sys/process.h>
int ssnes_main(int argc, char *argv[]);
#include <stddef.h>
SYS_PROCESS_PARAM(1001, 0x100000)

#undef main
// Temporary, a more sane implementation should go here.
int main(int argc, char *argv[])
{
   char arg1[] = "ssnes";
   char arg2[] = "/dev_hdd0/game/SNES90000/USRDIR/main.sfc";
   char arg3[] = "-v";
   char *argv_[] = { arg1, arg2, arg3, NULL };
   return ssnes_main(3, argv_);
}

