/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Text measurement, written once: the width gfx/font_layout.h gives a
 * right or centred line, for a caller that wants it without drawing.
 *
 * A statement block, like gfx/font_layout.h, included where it is used.
 * font_renderer_get_message_width() in font_driver.c is built from it
 * and serves every video driver whose atlas goes up apart from its
 * lookups; a driver that uploads an atlas cell as each glyph is looked
 * up includes the block itself with FONT_MEASURE_DIRTY, so its loop has
 * no call per glyph either.
 *
 * In scope where it is included:
 *
 *   msg, msg_len    the text; it ends at msg_len or at a NUL
 *   get_glyph       the font's get_glyph
 *   font_data       its handle
 *   glyph_q         the glyph standing in for a codepoint the font
 *                   cannot draw, or NULL to skip those
 *
 * Defined by the caller before including, undefined again after:
 *
 *   FONT_MEASURE_SUM
 *       An int lvalue each glyph's advance_x is added to.
 *   FONT_MEASURE_DIRTY(glyph)         optional
 *       Runs after every lookup.
 *
 * C89: the block declares its locals at its top. Its names begin fm_. */

#ifndef FONT_MEASURE_DIRTY
#define FONT_MEASURE_DIRTY(glyph)
#endif

{
   const char *fm_s   = (msg);
   const char *fm_end = fm_s + (msg_len);

   while (fm_s < fm_end && *fm_s)
   {
      const struct font_glyph *fm_g;
      uint32_t fm_code = utf8_walk(&fm_s);

      if (!(fm_g = get_glyph(font_data, fm_code)))
         if (!(fm_g = glyph_q))
            continue;
      FONT_MEASURE_DIRTY(fm_g);
      FONT_MEASURE_SUM += fm_g->advance_x;
   }
}

#undef FONT_MEASURE_SUM
#undef FONT_MEASURE_DIRTY
