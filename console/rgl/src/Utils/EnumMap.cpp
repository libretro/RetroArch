#include "../../include/export/RGL/rgl.h"
#include "../../include/RGL/private.h"
#include <string.h>

const char *rglMapLookupEnum(const void *data, unsigned int count, GLenum e )
{
   const RGLenumMap *map = (const RGLenumMap*)data;

   for (GLuint i = 0; i < count; ++i)
      if (map[i].e == e)
         return map[i].s;

   return NULL;
}

GLenum rglMapLookupString(const void *data, unsigned int count, const char *s )
{
   const RGLenumMap *map = (const RGLenumMap*)data;

   if (s != NULL)
      for (GLuint i = 0;i < count;++i)
         if ( strcmp( map[i].s, s) == 0)
            return map[i].e;

   return -1U;
}

