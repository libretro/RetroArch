#include "hash.h"

uint32_t rcheevos_djb2(const char* str, size_t length)
{
   const unsigned char* aux = (const unsigned char*)str;
   uint32_t            hash = 5381;

   while (length--)
      hash = ( hash << 5 ) + hash + *aux++;

   return hash;
}
