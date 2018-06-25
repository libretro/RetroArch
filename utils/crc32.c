/* gcc -O3 -o crc32 crc32.c -lz */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <encodings/crc32.h>

int main(int argc, const char* argv[])
{
   if (argc != 2 )
   {
      fprintf( stderr, "Usage: crc32 <filename>\n" );
      return 1;
   }

   FILE *file = fopen(argv[1], "rb");

   if (file)
   {
      uint32_t crc = encoding_crc32(0L, NULL, 0 );

      for (;;)
      {
         uint8_t buffer[16384];

         int numread = fread((void*)buffer, 1, sizeof(buffer), file);

         if (numread > 0)
            crc = encoding_crc32( crc, buffer, numread );
         else
            break;
      }

      fclose(file);

      printf("%08x\n", crc);
      return 0;
   }

   fprintf(stderr, "Error opening input file: %s\n", strerror(errno));
   return 1;
}
