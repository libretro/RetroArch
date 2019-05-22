#include <stdlib.h>

#include "mem_util.h"

char *strcpy_alloc(const char *src)
{
   char *result = NULL;
   size_t   len = src ? strlen(src) : 0;

   if (len == 0)
      return NULL;

   result = (char*)malloc(len + 1);
   strcpy(result, src);
   return result;
}

char *strcpy_alloc_force(const char *src)
{
   char *result = strcpy_alloc(src);
   if (!result)
      return (char*)calloc(1, 1);
   return result;
}

void strcat_alloc(char **dst, const char *s)
{
   size_t len1;
   char *src  = *dst;

   if (!src)
   {
      src     = strcpy_alloc_force(s);
      *dst    = src;
      return;
   }

   if (!s)
      return;

   len1       = strlen(src);
   src        = (char*)realloc(src, len1 + strlen(s) + 1);

   if (!src)
      return;

   *dst       = src;
   strcpy(src + len1, s);
}
