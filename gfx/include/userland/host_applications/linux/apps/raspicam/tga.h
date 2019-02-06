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

#ifndef TGA_H
#define TGA_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

typedef enum {
   tga_type_null = 0,
   tga_type_color_map = 1,
   tga_type_true_color = 2,
   tga_type_grayscale = 3,
   tga_type_rle_color_map = 9,
   tga_type_rle_true_color = 10,
   tga_type_rle_grayscale = 11,

} tga_image_type;

struct tga_colormap_info {
   unsigned short offset;
   unsigned short length;
   unsigned char bpp;
};

struct tga_image_info {
   unsigned short x_origin;
   unsigned short y_origin;
   unsigned short width;
   unsigned short height;
   unsigned char bpp;
   unsigned char descriptor;
};

struct tga_header {
   unsigned char id_length;
   unsigned char color_map_type;
   unsigned char image_type;
   struct tga_colormap_info colormap_info;
   struct tga_image_info image_info;
};

int write_tga(FILE* fp, int width, int height, uint8_t *buffer, size_t buffer_size);
unsigned char *load_tga(const char *filename, struct tga_header *header);

#endif /* TGA_H */
