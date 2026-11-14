#ifndef GFX_MENU_BLOOM_H
#define GFX_MENU_BLOOM_H

#include <stdint.h>
#include "../video_defines.h"

static unsigned menu_bloom_dims(unsigned dims)
{
   unsigned w = VIDEO_SCALE_W(dims);
   unsigned h = VIDEO_SCALE_H(dims);
   if (!w || !h)
      return 0;
   w = w > 1920 ? 960 : (w + 1) / 2;
   h = (unsigned)(((uint64_t)VIDEO_SCALE_H(dims) * w
            + VIDEO_SCALE_W(dims) - 1) / VIDEO_SCALE_W(dims));
   if (h > 540)
   {
      w = w * 540 / h;
      if (!w)
         w = 1;
      h = 540;
   }
   return VIDEO_SCALE_PACK(w, h);
}

#endif
