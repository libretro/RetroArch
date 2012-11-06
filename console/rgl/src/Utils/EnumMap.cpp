#include "../../include/export/RGL/rgl.h"
#include "../../include/RGL/private.h"
#include <string.h>

const char *rglMapLookupEnum( const RGLenumMap* map, unsigned int count, GLenum e )
{
   for (GLuint i = 0; i < count; ++i)
      if (map[i].e == e)
         return map[i].s;

   return NULL;
}

GLenum rglMapLookupString( const RGLenumMap* map, unsigned int count, const char *s )
{
   if (s != NULL)
      for (GLuint i = 0;i < count;++i)
         if ( strcmp( map[i].s, s) == 0)
            return map[i].e;

   return -1U;
}

