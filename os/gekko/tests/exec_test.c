/* gk_exec, run in Dolphin by run-dolphin.sh with a copy of this
 * program attached, as an ELF or a DOL:
 *   ARGS='sd:/exec_test.elf|first' ATTACH=exec_test_rvl.elf \
 *      ./run-dolphin.sh exec_test_rvl.elf
 * The first run checks what gk_exec refuses, then runs the copy, which
 * checks its arguments and that it starts from a fresh image. */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <gekko/exec.h>

static unsigned failures;
static int      in_data = 42;
static int      in_bss;

#define CHECK(cond, what) \
   do { \
      if (cond) \
         gk_debug_printf("ok   %s", what); \
      else \
      { \
         gk_debug_printf("FAIL %s", what); \
         failures++; \
      } \
   } while (0)

static int first(int argc, char **argv)
{
   static char long_arg[5000];
   static const uint8_t junk[256] = { 1, 2, 3 };
   const char *args[3];
   const char *big[2];
   const void *image;
   size_t      len;
   int         ret;

   if (argc < 4)
      return 0;
   image = (const void*)strtoul(argv[2], NULL, 16);
   len   = strtoul(argv[3], NULL, 16);
   in_data++;
   in_bss++;

   CHECK(gk_exec(junk, sizeof(junk), 0, NULL) == -ENOEXEC,
         "exec: refuses what is not a program");
   CHECK(gk_exec(image, 200, 0, NULL) == -ENOEXEC,
         "exec: refuses a cut-off image");
   memset(long_arg, 'x', sizeof(long_arg) - 1);
   big[0] = argv[0];
   big[1] = long_arg;
   CHECK(gk_exec(image, len, 2, big) == -E2BIG,
         "exec: refuses arguments too long");
   if (failures)
      return 0;

   args[0] = argv[0];
   args[1] = "second";
   args[2] = "with spaces";
   gk_debug_printf("     running the copy");
   ret = gk_exec(image, len, 3, args);
   CHECK(0, "exec: returned");
   gk_debug_printf("     %d", ret);
   return 0;
}

int main(int argc, char **argv)
{
   if (argc >= 2 && !strcmp(argv[1], "second"))
   {
      CHECK(argc == 3 && !strcmp(argv[0], "sd:/exec_test.elf")
            && !strcmp(argv[2], "with spaces"),
            "exec: the copy has its arguments");
      CHECK(in_data == 42 && in_bss == 0,
            "exec: the copy starts from its image");
   }
   else if (argc >= 2 && !strcmp(argv[1], "first"))
      first(argc, argv);
   else
      CHECK(0, "exec: run with ARGS and ATTACH");
   gk_debug_printf("%s (%u failure(s))", failures ? "FAILED" : "PASSED",
         failures);
   return 0;
}
