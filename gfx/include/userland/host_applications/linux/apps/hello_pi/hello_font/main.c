/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Test app for VG font library.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "bcm_host.h"
#include "vgfont.h"

static const char *strnchr(const char *str, size_t len, char c)
{
   const char *e = str + len;
   do {
      if (*str == c) {
         return str;
      }
   } while (++str < e);
   return NULL;
}

int32_t render_subtitle(GRAPHICS_RESOURCE_HANDLE img, const char *text, const int skip, const uint32_t text_size, const uint32_t y_offset)
{
   uint32_t text_length = strlen(text)-skip;
   uint32_t width=0, height=0;
   const char *split = text;
   int32_t s=0;
   int len = 0; // length of pre-subtitle
   uint32_t img_w, img_h;

   graphics_get_resource_size(img, &img_w, &img_h);

   if (text_length==0)
      return 0;
   while (split[0]) {
      s = graphics_resource_text_dimensions_ext(img, split, text_length-(split-text), &width, &height, text_size);
      if (s != 0) return s;
      if (width > img_w) {
         const char *space = strnchr(split, text_length-(split-text), ' ');
         if (!space) {
            len = split+1-text;
            split = split+1;
         } else {
            len = space-text;
            split = space+1;
         }
      } else {
         break;
      }
   }
   // split now points to last line of text. split-text = length of initial text. text_length-(split-text) is length of last line
   if (width) {
      s = graphics_resource_render_text_ext(img, (img_w - width)>>1, y_offset-height,
                                     GRAPHICS_RESOURCE_WIDTH,
                                     GRAPHICS_RESOURCE_HEIGHT,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
                                     split, text_length-(split-text), text_size);
      if (s!=0) return s;
   }
   return render_subtitle(img, text, skip+text_length-len, text_size, y_offset - height);
}

int main(void)
{
   GRAPHICS_RESOURCE_HANDLE img;
   uint32_t width, height;
   int LAYER=1;
   bcm_host_init();
   int s;

   s = gx_graphics_init(".");
   assert(s == 0);

   s = graphics_get_display_size(0, &width, &height);
   assert(s == 0);

   s = gx_create_window(0, width, height, GRAPHICS_RESOURCE_RGBA32, &img);
   assert(s == 0);

   // transparent before display to avoid screen flash
   graphics_resource_fill(img, 0, 0, width, height, GRAPHICS_RGBA32(0,0,0,0x00));

   graphics_display_resource(img, 0, LAYER, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 1);

   uint32_t text_size = 10;
   while (1) {
      const char *text = "The quick brown fox jumps over the lazy dog";
      uint32_t y_offset = height-60+text_size/2;
      graphics_resource_fill(img, 0, 0, width, height, GRAPHICS_RGBA32(0,0,0,0x00));
      // blue, at the top (y=40)
      graphics_resource_fill(img, 0, 40, width, 1, GRAPHICS_RGBA32(0,0,0xff,0xff));

      // green, at the bottom (y=height-40)
      graphics_resource_fill(img, 0, height-40, width, 1, GRAPHICS_RGBA32(0,0xff,0,0xff));

      // draw the subtitle text
      render_subtitle(img, text, 0, text_size,  y_offset);
      graphics_update_displayed_resource(img, 0, 0, 0, 0);
      text_size += 1;
      if (text_size > 50)
         text_size = 10;
   }

   graphics_display_resource(img, 0, LAYER, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 0);
   graphics_delete_resource(img);

   return 0;
}

