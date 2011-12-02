#include <sys/process.h>
#include <sysutil/sysutil_common.h>
#include <sys/spu_initialize.h>
#include <stddef.h>

int ssnes_main(int argc, char *argv[]);

SYS_PROCESS_PARAM(1001, 0x100000)

#undef main
// Temporary, a more sane implementation should go here.
int main(int argc, char *argv[])
{
   sys_spu_initialize(4, 3);
   char arg1[] = "ssnes";
   char arg2[] = "/dev_hdd0/game/SNES90000/USRDIR/main.sfc";
   char arg3[] = "-v";
   char arg4[] = "-c";
   char arg5[] = "/dev_hdd0/game/SSNE10000/USRDIR/ssnes.cfg";
   char *argv_[] = { arg1, arg2, arg3, arg4, arg5, NULL};
   return ssnes_main(5, argv_);
}

