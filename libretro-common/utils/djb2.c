/* public domain */
/* gcc -O3 -o djb2 djb2.c */

#include <stdio.h>
#include <stdint.h>

static uint32_t djb2(const char* str)
{
   const unsigned char* aux = (const unsigned char*)str;
   uint32_t hash = 5381;

   while (*aux)
      hash = (hash << 5) + hash + *aux++;

   return hash;
}

int main(int argc, const char* argv[])
{
   int i;

   for (i = 1; i < argc; i++)
      printf( "0x%08xU: %s\n", djb2( argv[ i ] ), argv[ i ] );

   return 0;
}
