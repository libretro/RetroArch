/*
Copyright (c) 2013, Broadcom Europe Ltd
Copyright (c) 2013, Tim Gover
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

#include "tga.h"
#include <string.h>

#define TGA_WRITE(FP, F) \
   if (fwrite((&F), sizeof(F), 1, (FP)) != 1) goto write_fail
int write_tga(FILE *fp, int width, int height,
      uint8_t *buffer, size_t buffer_size)
{
   struct tga_header header;
   memset(&header, 0, sizeof(header));
   header.image_type = tga_type_true_color;
   header.image_info.width = width;
   header.image_info.y_origin = height;
   header.image_info.height = height;
   header.image_info.bpp = 32;

   TGA_WRITE(fp, header.id_length);
   TGA_WRITE(fp, header.color_map_type);
   TGA_WRITE(fp, header.image_type);
   TGA_WRITE(fp, header.colormap_info.offset);
   TGA_WRITE(fp, header.colormap_info.length);
   TGA_WRITE(fp, header.colormap_info.bpp);
   TGA_WRITE(fp, header.image_info.x_origin);
   TGA_WRITE(fp, header.image_info.y_origin);
   TGA_WRITE(fp, header.image_info.width);
   TGA_WRITE(fp, header.image_info.height);
   TGA_WRITE(fp, header.image_info.bpp);
   TGA_WRITE(fp, header.image_info.descriptor);

   if (fwrite(buffer, 1, buffer_size, fp) != buffer_size)
      goto write_fail;

   return 0;
write_fail:
   return -1;
}

#define TGA_READ(FP, F) if (fread((&F), sizeof(F), 1, (FP)) != 1) goto read_fail

static int read_header(FILE *fp, struct tga_header *header) {
    TGA_READ(fp, header->id_length);
    TGA_READ(fp, header->color_map_type);
    TGA_READ(fp, header->image_type);
    TGA_READ(fp, header->colormap_info.offset);
    TGA_READ(fp, header->colormap_info.length);
    TGA_READ(fp, header->colormap_info.bpp);
    TGA_READ(fp, header->image_info.x_origin);
    TGA_READ(fp, header->image_info.y_origin);
    TGA_READ(fp, header->image_info.width);
    TGA_READ(fp, header->image_info.height);
    TGA_READ(fp, header->image_info.bpp);
    TGA_READ(fp, header->image_info.descriptor);

    return 0;

read_fail:
    return -1;
}

unsigned char *load_tga(const char *filename, struct tga_header *header) {
    unsigned char *image = NULL;
    FILE *fp = fopen(filename, "r");
    if (fp) {
        if(read_header(fp, header) == 0) {
            if (header->image_type == tga_type_true_color &&
                (header->image_info.bpp == 24 ||
                header->image_info.bpp == 32)) {
                int buflen = header->image_info.width *
                   header->image_info.height * (header->image_info.bpp / 8);
                image = malloc(buflen);
                if (image) {
                    if (header->id_length)
                        fseek(fp, SEEK_CUR, header->id_length);

                    if (fread(image, 1, buflen, fp) != buflen) {
                        free(image);
                        image = NULL;
                    }
                }
            }
        }
        fclose(fp);
    }
    return image;
}
