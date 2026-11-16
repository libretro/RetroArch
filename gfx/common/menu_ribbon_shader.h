#ifndef GFX_MENU_RIBBON_SHADER_H
#define GFX_MENU_RIBBON_SHADER_H

#include <string.h>
#include <retro_inline.h>
#include "../drivers/menu_ribbon_shader.h"

#define RIBBON_STRING_(x) #x
#define RIBBON_STRING(x) RIBBON_STRING_(x)

static INLINE const char *menu_ribbon_shader(char *dst, size_t capacity,
      const char *prefix, const char *suffix, unsigned stages)
{
   static const char *const parts[] = {
      RIBBON_STRING(RIBBON_VERTEX_0),
      RIBBON_STRING(RIBBON_VERTEX_1),
      RIBBON_STRING(RIBBON_VERTEX_2),
      RIBBON_STRING(RIBBON_FRAGMENT_0),
   };
   unsigned i;
   size_t used = 0;
   for (i = 0; i < 6; i++)
   {
      const char *part;
      size_t len;
      if (i > 0 && i <= 3 && !(stages & 1))
         continue;
      if (i == 4 && !(stages & 2))
         continue;
      part = i == 0 ? prefix : (i == 5 ? suffix : parts[i - 1]);
      len = strlen(part);
      if (len >= capacity - used)
         return NULL;
      memcpy(dst + used, part, len);
      used += len;
   }
   dst[used] = '\0';
   return dst;
}

#undef RIBBON_STRING
#undef RIBBON_STRING_
#endif
