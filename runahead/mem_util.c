#include <stdlib.h>

#include "mem_util.h"

char *strcpy_alloc(const char *sourceStr)
{
   size_t   len = 0;
   char *result = NULL;

   if (sourceStr)
      len = strlen(sourceStr);

   if (len == 0)
      return NULL;

   result = (char*)malloc(len + 1);
   strcpy(result, sourceStr);
   return result;
}

char *strcpy_alloc_force(const char *sourceStr)
{
   char *result = strcpy_alloc(sourceStr);
   if (!result)
      result = (char*)calloc(1, 1);
   return result;
}

void strcat_alloc(char ** destStr_p, const char *appendStr)
{
   size_t len1, len2, newLen;
   char *destStr = *destStr_p;

   if (!destStr)
   {
      destStr    = strcpy_alloc_force(appendStr);
      *destStr_p = destStr;
      return;
   }

   if (!appendStr)
      return;

   len1       = strlen(destStr);
   len2       = strlen(appendStr);
   newLen     = len1 + len2 + 1;
   destStr    = (char*)realloc(destStr, newLen);
   *destStr_p = destStr;
   strcpy(destStr + len1, appendStr);
}
