int ssnes_main(int argc, char *argv[]);
#include <stddef.h>

#undef main
// Temporary, a more sane implementation should go here.
int main(int argc, char *argv[])
{
   char arg1[] = "ssnes";
   char arg2[] = "path/to/your/testrom.sfc";
   char *argv_[] = { arg1, arg2, NULL };
   return ssnes_main(2, argv_);
}

