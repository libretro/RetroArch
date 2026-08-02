// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright (C) 2014 Henner Zeller <h.zeller@acm.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>

#include "graphics.h"
#include "utf8-internal.h"

#include <stdlib.h>
#include <functional>
#include <algorithm>

namespace rgb_matrix {
bool SetImage(Canvas *c, int canvas_offset_x, int canvas_offset_y,
              const uint8_t *buffer, size_t size,
              const int width, const int height,
              bool is_bgr) {
  if (3 * width * height != (int)size)   // Sanity check
    return false;

  int image_display_w = width;
  int image_display_h = height;

  size_t skip_start_row = 0;   // Bytes to skip before each row
  if (canvas_offset_x < 0) {
    skip_start_row = -canvas_offset_x * 3;
    image_display_w += canvas_offset_x;
    if (image_display_w <= 0) return false;  // Done. outside canvas.
    canvas_offset_x = 0;
  }
  if (canvas_offset_y < 0) {
    // Skip buffer to the first row we'll be showing
    buffer += 3 * width * -canvas_offset_y;
    image_display_h += canvas_offset_y;
    if (image_display_h <= 0) return false;  // Done. outside canvas.
    canvas_offset_y = 0;
  }
  const int w = std::min(c->width(), canvas_offset_x + image_display_w);
  const int h = std::min(c->height(), canvas_offset_y + image_display_h);

  // Bytes to skip for wider than canvas image at the end of a row
  const size_t skip_end_row = (canvas_offset_x + image_display_w > w)
    ? (canvas_offset_x + image_display_w - w) * 3
    : 0;

  // Let's make this a combined skip per row and adjust where we start.
  const size_t next_row_skip = skip_start_row + skip_end_row;
  buffer += skip_start_row;

  if (is_bgr) {
    for (int y = canvas_offset_y; y < h; ++y) {
      for (int x = canvas_offset_x; x < w; ++x) {
        c->SetPixel(x, y, buffer[2], buffer[1], buffer[0]);
        buffer += 3;
      }
      buffer += next_row_skip;
    }
  } else {
    for (int y = canvas_offset_y; y < h; ++y) {
      for (int x = canvas_offset_x; x < w; ++x) {
        c->SetPixel(x, y, buffer[0], buffer[1], buffer[2]);
        buffer += 3;
      }
      buffer += next_row_skip;
    }
  }
  return true;
}

int DrawText(Canvas *c, const Font &font,
             int x, int y, const Color &color,
             const char *utf8_text) {
  return DrawText(c, font, x, y, color, NULL, utf8_text);
}

int DrawText(Canvas *c, const Font &font,
             int x, int y, const Color &color, const Color *background_color,
             const char *utf8_text, int extra_spacing) {
  const int start_x = x;
  while (*utf8_text) {
    const uint32_t cp = utf8_next_codepoint(utf8_text);
    x += font.DrawGlyph(c, x, y, color, background_color, cp);
    x += extra_spacing;
  }
  return x - start_x;
}

// There used to be a symbol without the optional extra_spacing parameter. Let's
// define this here so that people linking against an old library will still
// have their code usable. Now: 2017-06-04; can probably be removed in a couple
// of months.
int DrawText(Canvas *c, const Font &font,
             int x, int y, const Color &color, const Color *background_color,
             const char *utf8_text) {
  return DrawText(c, font, x, y, color, background_color, utf8_text, 0);
}

int VerticalDrawText(Canvas *c, const Font &font, int x, int y,
                     const Color &color, const Color *background_color,
                     const char *utf8_text, int extra_spacing) {
  const int start_y = y;
  while (*utf8_text) {
    const uint32_t cp = utf8_next_codepoint(utf8_text);
    font.DrawGlyph(c, x, y, color, background_color, cp);
    y += font.height() + extra_spacing;
  }
  return y - start_y;
}

int MeasureText(const Font &font, const char *utf8_text, int kerning_offset) {
  int result = 0;
  while (*utf8_text) {
    const uint32_t cp = utf8_next_codepoint(utf8_text);
    int width = font.CharacterWidth(cp);
    if (width > 0)
      result += width;
    if (*utf8_text)
      result += kerning_offset;
  }
  return result;
}

void DrawCircle(Canvas *c, int x0, int y0, int radius, const Color &color) {
  int x = radius, y = 0;
  int radiusError = 1 - x;

  while (y <= x) {
    c->SetPixel(x + x0, y + y0, color.r, color.g, color.b);
    c->SetPixel(y + x0, x + y0, color.r, color.g, color.b);
    c->SetPixel(-x + x0, y + y0, color.r, color.g, color.b);
    c->SetPixel(-y + x0, x + y0, color.r, color.g, color.b);
    c->SetPixel(-x + x0, -y + y0, color.r, color.g, color.b);
    c->SetPixel(-y + x0, -x + y0, color.r, color.g, color.b);
    c->SetPixel(x + x0, -y + y0, color.r, color.g, color.b);
    c->SetPixel(y + x0, -x + y0, color.r, color.g, color.b);
    y++;
    if (radiusError<0){
      radiusError += 2 * y + 1;
    } else {
      x--;
      radiusError+= 2 * (y - x + 1);
    }
  }
}

void DrawLine(Canvas *c, int x0, int y0, int x1, int y1, const Color &color) {
  int dy = y1 - y0, dx = x1 - x0, gradient, x, y, shift = 0x10;

  if (abs(dx) > abs(dy)) {
    // x variation is bigger than y variation
    if (x1 < x0) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }
    gradient = (dy << shift) / dx ;

    for (x = x0 , y = 0x8000 + (y0 << shift); x <= x1; ++x, y += gradient) {
      c->SetPixel(x, y >> shift, color.r, color.g, color.b);
    }
  } else if (dy != 0) {
    // y variation is bigger than x variation
    if (y1 < y0) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }
    gradient = (dx << shift) / dy;
    for (y = y0 , x = 0x8000 + (x0 << shift); y <= y1; ++y, x += gradient) {
      c->SetPixel(x >> shift, y, color.r, color.g, color.b);
    }
  } else {
    c->SetPixel(x0, y0, color.r, color.g, color.b);
  }
}

}//namespace
