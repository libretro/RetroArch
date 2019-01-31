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

// Font handling for graphicsx

#ifndef VGFT_H
#define VGFT_H

#include "interface/vcos/vcos.h"
#include <VG/openvg.h>
#include <ft2build.h>

typedef int VGFT_BOOL;
#define VGFT_FALSE 0
#define VGFT_TRUE (!VGFT_FALSE)

#include FT_FREETYPE_H

/* Returns 0 on success */
extern int vgft_init(void);
extern void vgft_term(void);

typedef struct {
   VGFont vg_font;
   FT_Face ft_face;
} VGFT_FONT_T;

/** Initialise a FT->VG font */
VCOS_STATUS_T vgft_font_init(VGFT_FONT_T *font);

/** Load a font file from memory */
VCOS_STATUS_T vgft_font_load_mem(VGFT_FONT_T *font, void *mem, size_t len);

/** Convert a font into VG glyphs */
VCOS_STATUS_T vgft_font_convert_glyphs(VGFT_FONT_T *font, unsigned int char_height, unsigned int dpi_x, unsigned int dpi_y);

/** Release a font. */
void vgft_font_term(VGFT_FONT_T *font);

void vgft_font_draw(VGFT_FONT_T *font, VGfloat x, VGfloat y, const char *text, unsigned text_length, VGbitfield paint_modes);

void vgft_get_text_extents(VGFT_FONT_T *font, const char *text, unsigned text_length, VGfloat start_x, VGfloat start_y, VGfloat *w, VGfloat *h);

VGfloat vgft_first_line_y_offset(VGFT_FONT_T *font);

#endif
