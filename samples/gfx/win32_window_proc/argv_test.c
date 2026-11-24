#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <shellapi.h>
#include <encodings/utf.h>
#include "argv_impl.inc"

static int argv_test_entry(int argc, char *argv[], void *data)
{
   int i;
   (void)data;
   if (argv[argc] != NULL)
      return 2;
   for (i = 1; i < argc; i++)
   {
      const unsigned char *text = (const unsigned char*)argv[i];
      while (*text)
         printf("%02x", (unsigned)*text++);
      putchar('\n');
   }
   return 23;
}

int main(int argc, char *argv[])
{
   return frontend_win32_main(argc, argv, argv_test_entry);
}
