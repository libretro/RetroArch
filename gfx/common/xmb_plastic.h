#ifndef GFX_XMB_PLASTIC_H
#define GFX_XMB_PLASTIC_H

#include <string.h>
#include <boolean.h>
#include <retro_inline.h>
#include "../drivers/xmb_plastic.h"

#define XMB_STRING_IMPL(x) #x
#define XMB_STRING(x) XMB_STRING_IMPL(x)

static INLINE size_t xmb_plastic_source(char *dst, size_t capacity,
      const char *prefix, const char *suffix, bool font)
{
   static const char *const body[2][3] = {
      {
         XMB_STRING(XMB_PLASTIC_SAMPLE),
         XMB_STRING(XMB_PLASTIC_LIGHT),
         XMB_STRING(XMB_PLASTIC_COLOR)
      },
      {
         XMB_STRING(XMB_FONT_SAMPLE),
         XMB_STRING(XMB_FONT_LIGHT),
         XMB_STRING(XMB_FONT_COLOR)
      }
   };
   size_t used = 0;
   unsigned i;

   for (i = 0; i < 5; i++)
   {
      const char *part = i == 0 ? prefix : (i == 4 ? suffix : body[font ? 1 : 0][i - 1]);
      size_t len = strlen(part);
      if (len >= capacity - used)
         return 0;
      memcpy(dst + used, part, len);
      used += len;
   }
   dst[used] = '\0';
   return used;
}

#undef XMB_STRING
#undef XMB_STRING_IMPL

#endif
